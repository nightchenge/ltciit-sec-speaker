/**
 *@file lt_http.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-09-03
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#include "lt_http.h"

#define lt_http_log(msg, ...) QL_LOG(QL_LOG_LEVEL_INFO, "lt_http", msg, ##__VA_ARGS__)

typedef enum
{
    LT_HTTP_EVENT_RESPONSE = 1001,
    LT_HTTP_EVENT_END,  // HTTP 连接终止
    LT_HTTP_EVENT_STOP, // HTTP连接被停止
} lt_http_event_code_t;

typedef struct lt_http_trans
{
    ql_task_t task;
    bool is_running;
    http_client_t client;
    ebs_queue_t queue;

    ebs_mutex_t mutex;
    bool dl_block;
    int dl_high_line;
    int dl_total_len;
    int timeout;
    char *url;
    void (*stop_callback)(void *);
    void (*destroy)(struct lt_http_trans *this);
    ebm_task_t *ebm_task;
    struct
    {
        bool is_parse;
        int len;
    } id3v2;
} lt_http_trans_t;

#define HTTP_MAX_MSG_CNT 20
#define ID3V2_HEADER_SIZE 10
#define HTTP_RECV_TIMEOUT 100

static void lt_http_event_callback(http_client_t *client, int event, int event_code, void *arg)
{
    lt_http_trans_t *trans = (lt_http_trans_t *)arg;
    ebs_event_t event_send = {0};

    if (trans == NULL)
    {
        return;
    }

    if (*client != trans->client)
    {
        return;
    }

    switch (event)
    {
    case HTTP_EVENT_SESSION_ESTABLISH:
    {
        if (event_code != QL_HTTP_OK)
        {
            lt_http_log("HTTP session establish failed, event_code=%d", event_code);
            event_send.id = LT_HTTP_EVENT_END;
            event_send.param1 = (uint32_t)trans;
            ebs_msg_queue_send(trans->queue, (uint8_t *)&event_send, sizeof(ebs_event_t), EBS_WAIT_FOREVER);
        }
    }
    break;
    case HTTP_EVENT_RESPONE_STATE_LINE:
    {
        if (event_code == QL_HTTP_OK)
        {
            int resp_code = 0;
            int content_length = 0;
            int chunk_encode = 0;
            char *location = NULL;
            ql_httpc_getinfo(client, HTTP_INFO_RESPONSE_CODE, &resp_code);
            lt_http_log("HTTP response code=%d", resp_code);
            ql_httpc_getinfo(client, HTTP_INFO_CHUNK_ENCODE, &chunk_encode);
            lt_http_log("HTTP response chunk_encode=%d", chunk_encode);
            if (chunk_encode == 0)
            {
                ql_httpc_getinfo(client, HTTP_INFO_CONTENT_LEN, &content_length);
                lt_http_log("HTTP response content length=%d", content_length);
            }
            else
            {
                lt_http_log("HTTP response chunk encode");
            }
            if (resp_code >= 300 && resp_code < 400)
            {
                ql_httpc_getinfo(client, HTTP_INFO_LOCATION, &location);
                lt_http_log("HTTP response location=%s", location);
                free(location);
            }
        }
    }
    break;
    case HTTP_EVENT_SESSION_DISCONNECT:
    {
        if (event_code == QL_HTTP_OK)
        {
            lt_http_log("HTTP transfer end");
        }
        else
        {
            lt_http_log("HTTP transfer error, event_code=%d", event_code);
        }
        event_send.id = LT_HTTP_EVENT_END;
        ebs_msg_queue_send(trans->queue, (uint8_t *)&event_send, sizeof(ebs_event_t), EBS_WAIT_FOREVER);
    }
    break;
    }
}

static int lt_http_write_response_data(http_client_t *client, void *arg, char *data, int size, unsigned char end)
{
    int ret = size;
    uint32_t msg_cnt = 0;
    char *read_buff = NULL;
    lt_http_trans_t *trans = (lt_http_trans_t *)arg;
    ebs_event_t event_send = {0};

    if (trans == NULL)
        return 0;

    if (*client != trans->client)
        return 0;

    read_buff = (char *)malloc(size + 1);
    if (read_buff == NULL)
    {
        lt_http_log("malloc failed");
        return 0;
    }

    memcpy(read_buff, data, size);

    if (QL_OSI_SUCCESS != ebs_msg_queue_get_cnt(trans->queue, &msg_cnt))
    {
        free(read_buff);
        lt_http_log("ebs_msg_queue_get_cnt failed");
        return 0;
    }

    ebs_mutex_lock(&trans->mutex);
    if (msg_cnt >= (HTTP_MAX_MSG_CNT - 1) || (trans->dl_total_len + size) > trans->dl_high_line)
    {
        trans->dl_block = true;
        ret = QL_HTTP_ERROR_WONDBLOCK;
    }
    ebs_mutex_unlock(&trans->mutex);

    event_send.id = LT_HTTP_EVENT_RESPONSE;
    event_send.param1 = (uint32_t)trans;
    event_send.param2 = (uint32_t)read_buff;
    event_send.param3 = size;
    if (QL_OSI_SUCCESS != ebs_msg_queue_send(trans->queue, (uint8_t *)&event_send, sizeof(ebs_event_t), EBS_WAIT_FOREVER))
    {
        free(read_buff);
        lt_http_log("ebs_msg_queue_send failed");
        return 0;
    }

    ebs_mutex_lock(&trans->mutex);
    trans->dl_total_len += size;
    trans->timeout = 0;
    ebs_mutex_unlock(&trans->mutex);

    return ret;
}

static int id3v2_match(const uint8_t *buf, const char *magic)
{
    return buf[0] == magic[0] &&
           buf[1] == magic[1] &&
           buf[2] == magic[2] &&
           buf[3] != 0xff &&
           buf[4] != 0xff &&
           (buf[6] & 0x80) == 0 &&
           (buf[7] & 0x80) == 0 &&
           (buf[8] & 0x80) == 0 &&
           (buf[9] & 0x80) == 0;
}

static void lt_http_write_response_data_func(void *param)
{
    int err = 0;
    int size = 0;

    bool dload_block = false;
    char *read_buff = NULL;
    lt_http_trans_t *trans = NULL;
    ebs_event_t *event_recv = (ebs_event_t *)param;
    if (event_recv == NULL || event_recv->param1 == 0 || event_recv->param2 == 0 || event_recv->param3 == 0)
        return;
    trans = (lt_http_trans_t *)event_recv->param1;
    read_buff = (char *)event_recv->param2;
    size = (int)event_recv->param3;

    if (!trans->id3v2.is_parse)
    {
        if (size >= ID3V2_HEADER_SIZE)
        {
            char *buf = read_buff;
            do
            {
                if (id3v2_match((const uint8_t *)buf, "ID3"))
                {
                    trans->id3v2.len = ((buf[6] & 0x7f) << 21) |
                                       ((buf[7] & 0x7f) << 14) |
                                       ((buf[8] & 0x7f) << 7) |
                                       (buf[9] & 0x7f);
                    trans->id3v2.len += ID3V2_HEADER_SIZE;
                    break;
                }
                buf++;
            } while (buf - read_buff < size);
            trans->id3v2.is_parse = true;
        }
    }

    if (trans->id3v2.len)
    {
        if (size <= trans->id3v2.len)
        {
            trans->id3v2.len -= size;
            goto end;
        }
        else
        {
            read_buff += trans->id3v2.len;
            size -= trans->id3v2.len;
            trans->id3v2.len = 0;
        }
    }

    if (size > PACKET_WRITE_MAX_SIZE)
    {
        do
        {
            err = ql_aud_play_stream_start(QL_AUDIO_FORMAT_MP3, (const void *)read_buff, size > PACKET_WRITE_MAX_SIZE ? PACKET_WRITE_MAX_SIZE : size, QL_AUDIO_PLAY_TYPE_LOCAL, NULL);
            if (QL_AUDIO_SUCCESS != err)
            {
                lt_http_log("ql_aud_play_stream_start failed, err=%d", err);
            }
            size -= PACKET_WRITE_MAX_SIZE;
            read_buff += PACKET_WRITE_MAX_SIZE;
        } while (size > 0);
    }
    else
    {
        err = ql_aud_play_stream_start(QL_AUDIO_FORMAT_MP3, (const void *)read_buff, size, QL_AUDIO_PLAY_TYPE_LOCAL, NULL);
        if (QL_AUDIO_SUCCESS != err)
        {
            lt_http_log("ql_aud_play_stream_start failed, err=%d", err);
        }
    }

end:
    free((void *)event_recv->param2);

    ebs_mutex_lock(&trans->mutex);
    trans->dl_total_len -= (int)event_recv->param3;
    if (trans->dl_total_len < 0)
        trans->dl_total_len = 0;
    if (trans->dl_block == true && trans->dl_total_len < trans->dl_high_line)
    {
        dload_block = trans->dl_block;
        trans->dl_block = false;
    }
    ebs_mutex_unlock(&trans->mutex);

    if (dload_block == true)
    {
        ql_httpc_continue_dload(&trans->client);
    }
}

static void lt_http_app_thread(void *arg)
{
    int ret = 0;
    lt_http_trans_t *this = (lt_http_trans_t *)arg;
    ebs_event_t event_msg = {0};

    if (this->url == NULL)
    {
        lt_http_log("url is null");
        goto exit;
    }

    this->is_running = true;
    this->dl_block = false;
    this->dl_total_len = 0;
    this->id3v2.is_parse = false;
    this->id3v2.len = 0;
    this->timeout = 0;

    ret = ebs_mutex_init(&this->mutex);
    if (ret)
    {
        lt_http_log("ebs_mutex_init failed");
        goto exit;
    }

    ret = ebs_msg_queue_creat(&this->queue, sizeof(ebs_event_t), HTTP_MAX_MSG_CNT);
    if (ret)
    {
        lt_http_log("ebs_msg_queue_creat failed");
        goto exit;
    }

    if (ql_httpc_new(&this->client, lt_http_event_callback, (void *)this) != QL_HTTP_OK)
    {
        lt_http_log("ql_httpc_new failed");
        goto exit;
    }

    ql_httpc_setopt(&this->client, HTTP_CLIENT_OPT_SIM_ID, 0);
    ql_httpc_setopt(&this->client, HTTP_CLIENT_OPT_PDPCID, 1);
    ql_httpc_setopt(&this->client, HTTP_CLIENT_OPT_WRITE_FUNC, lt_http_write_response_data);
    ql_httpc_setopt(&this->client, HTTP_CLIENT_OPT_WRITE_DATA, (void *)this);
    ql_httpc_setopt(&this->client, HTTP_CLIENT_OPT_METHOD, HTTP_METHOD_GET);
    ql_httpc_setopt(&this->client, HTTP_CLIENT_OPT_URL, this->url);

    if (ql_httpc_perform(&this->client) == QL_HTTP_OK)
    {
        while (this->is_running)
        {
            memset(&event_msg, 0, sizeof(ebs_event_t));

            if (0 == ebs_msg_queue_wait(this->queue, (uint8_t *)&event_msg, sizeof(ebs_event_t), 200))
            {
                switch (event_msg.id)
                {
                case LT_HTTP_EVENT_RESPONSE:
                {
                    lt_http_write_response_data_func((void *)&event_msg);
                }
                break;
                case LT_HTTP_EVENT_END:
                {
                    ql_aud_data_done(); // 告诉内核数据已写完,所有数据解码完成后停止播放,这一步必须要有
                    ql_aud_wait_play_finish(QL_WAIT_FOREVER);
                    // ql_aud_data_done(); // 告诉内核数据已写完,所有数据解码完成后停止播放,这一步必须要有
                    // // ql_aud_wait_play_finish(QL_WAIT_FOREVER);
                    // ql_aud_player_stop();
                    // this->is_running = false;
                    // if (this->stop_callback)
                    //     this->stop_callback((void *)this->ebm_task);
                    goto exit;
                }
                break;
                case LT_HTTP_EVENT_STOP:
                {
                    ql_aud_data_done(); // 告诉内核数据已写完,所有数据解码完成后停止播放,这一步必须要有
                  //  ql_aud_wait_play_finish(QL_WAIT_FOREVER);
                    // lt_http_log("LT_HTTP_EVENT_STOP111");
                    // ql_aud_data_done(); // 告诉内核数据已写完,所有数据解码完成后停止播放,这一步必须要有
                    // // lt_http_log("LT_HTTP_EVENT_STOP222");
                    // // ql_aud_wait_play_finish(QL_WAIT_FOREVER);
                    // lt_http_log("LT_HTTP_EVENT_STOP333");
                    // ql_aud_player_stop();
                    // lt_http_log("LT_HTTP_EVENT_STOP444");
                    // this->is_running = false;
                    // if (this->stop_callback)
                    //     this->stop_callback((void *)this->ebm_task);
                    goto exit;
                }
                break;
                default:
                    break;
                }
            }
            ebs_mutex_lock(&this->mutex);
            this->timeout++;
            ebs_mutex_unlock(&this->mutex);
            if (this->timeout >= HTTP_RECV_TIMEOUT)
            {
                ql_aud_data_done(); // 告诉内核数据已写完,所有数据解码完成后停止播放,这一步必须要有
             //   ql_aud_wait_play_finish(QL_WAIT_FOREVER);
                // ql_aud_data_done(); // 告诉内核数据已写完,所有数据解码完成后停止播放,这一步必须要有
                // // ql_aud_wait_play_finish(QL_WAIT_FOREVER);
                // ql_aud_player_stop();
                // this->is_running = false;
                // if (this->stop_callback)
                //     this->stop_callback((void *)this->ebm_task);
                goto exit;
            }
        }
    }
    else
    {
        lt_http_log("ql_httpc_perform failed");
    }

exit:
    lt_http_log("lt_http_app_thread exit");
  //  ql_aud_data_done(); // 告诉内核数据已写完,所有数据解码完成后停止播放,这一步必须要有
  //  ql_aud_wait_play_finish(QL_WAIT_FOREVER);
    ql_aud_player_stop();
    lt_http_log("lt_http_app_thread exit22");
    if (this->stop_callback)
        this->stop_callback((void *)this->ebm_task);
    this->destroy((void *)this);
    return;
}

static void lt_http_play_destroy(lt_http_trans_t *this)
{
    if (this == NULL)
        return;
    ebs_event_t event = {0};

    if (this->client)
    {
        ql_httpc_release(&this->client);
        this->client = 0;
    }

    do
    {
        memset(&event, 0, sizeof(ebs_event_t));
        if (0 != ebs_msg_queue_wait(this->queue, (uint8_t *)&event, sizeof(ebs_event_t), 0))
            break;
        switch (event.id)
        {
        case LT_HTTP_EVENT_RESPONSE:
            if (event.param2)
                free((void *)event.param2);
            break;
        default:
            break;
        }
    } while (1);

    if (this->mutex)
    {
        ebs_mutex_destroy(&this->mutex);
        this->mutex = NULL;
    }
    if (this->queue)
    {
        ebs_msg_queue_delete(this->queue);
        this->queue = NULL;
    }
    if (this->is_running)
        this->is_running = false;
    if (this->stop_callback)
        this->stop_callback = NULL;
    if (this->url)
        this->url = NULL;
    if (this->ebm_task)
        this->ebm_task = NULL;
    ql_task_t task = this->task;
    this->task = NULL;
    ql_rtos_task_delete(task);
}

static lt_http_trans_t lt_http_trans = {
    .task = 0,
    .client = 0,
    .dl_block = false,
    .dl_high_line = 0x10000,
    .dl_total_len = 0,
    .timeout = 0,
    .is_running = false,
    .mutex = NULL,
    .queue = NULL,
    .url = NULL,
    .stop_callback = NULL,
    .destroy = lt_http_play_destroy,
    .id3v2.is_parse = false,
    .id3v2.len = 0,
    .ebm_task = NULL,
};

int lt_http_play_start(char *url)
{
    if (lt_http_trans.is_running)
        return 0;
    lt_http_log("lt_http_play_start");
    QlOSStatus err = QL_OSI_SUCCESS;

    lt_http_trans.url = url;
    err = ql_rtos_task_create(&lt_http_trans.task, 1024 * 100, APP_PRIORITY_REALTIME, "lt_http_app", lt_http_app_thread, (void *)&lt_http_trans, 5);
    if (err != QL_OSI_SUCCESS)
    {
        lt_http_log("lt_http_app_thread create failed");
        return -1;
    }
    lt_http_log("lt_http_app_thread create success");
    return 0;
}

int lt_http_play_start_ex(char *url, void (*callback)(void *))
{
    if (lt_http_trans.is_running)
        return 0;
    lt_http_trans.stop_callback = callback;
    if (lt_http_play_start(url) != 0)
    {
        lt_http_trans.stop_callback = NULL;
        return -1;
    }
    return 0;
}

int lt_http_play_start_ebm_task(ebm_task_t *task)
{
    if (lt_http_trans.is_running)
        return 0;
    lt_http_trans.stop_callback = task->callback;
    lt_http_trans.ebm_task = task;
    if (lt_http_play_start(task->url) != 0)
    {
        lt_http_trans.stop_callback = NULL;
        return -1;
    }
    return 0;
}

int lt_http_status_query(void)
{
    if (lt_http_trans.is_running)
        return 0;
    return -1;
}

int lt_http_play_stop(void)
{
    lt_http_log("lt_http_play_stop");

    if (!lt_http_trans.is_running)
        return 0;

    ebs_event_t event_send = {0};
    event_send.id = LT_HTTP_EVENT_STOP;
    if (lt_http_trans.queue)
    {
        ebs_msg_queue_send(lt_http_trans.queue, (uint8_t *)&event_send, sizeof(ebs_event_t), EBS_WAIT_FOREVER);
    }

    return 0;
}

int lt_http_play_stop_direct(void)
{
    lt_http_log("lt_http_play_stop_direct");

    if (!lt_http_trans.is_running)
        return 0;

    lt_http_trans.is_running = false;
    ql_task_status_t status;
    do
    {
        ebs_sleep_ms(50);
        if (lt_http_trans.task)
            ql_rtos_task_get_status(lt_http_trans.task, &status);
    } while (lt_http_trans.task && status.eCurrentState < Deleted);
    return 0;
}