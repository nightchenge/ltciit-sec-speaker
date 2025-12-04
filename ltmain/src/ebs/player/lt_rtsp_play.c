/**
 * @file lt_rtsp_play.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-09-22
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */

#include "lt_rtsp_play.h"
#include "mp3frame.h"

#include "ql_audio.h"
#include "ql_api_common.h"
#include "ql_log.h"
#include "ql_api_osi.h"
#include "ql_osi_def.h"

#define rtsp_play_log(msg, ...) QL_LOG(QL_LOG_LEVEL_INFO, "rtsp_play", msg, ##__VA_ARGS__)

typedef struct lt_rtsp_trans
{
    ql_task_t task;
    bool is_running;
    bool is_recv_stop;
    int err_cnt;
    rtsp_client_t *client;
    const char *url;
    ebs_queue_t queue;
    ql_sem_t sem_play;
    uint8_t *mp3_buff;
    mp3_info_t mp3_info;
    void (*destroy)(struct lt_rtsp_trans *this);
    void (*stop_callback)(void *);
} lt_rtsp_trans_t;

#define LT_RTSP_MSG_MAX_CNT 10
#define ID3V2_HEADER_SIZE 10

static int rtp_data_callback(uint8_t *buf, int size, void *user_arg)
{
    lt_rtsp_trans_t *this = (lt_rtsp_trans_t *)user_arg;

    if (!this->is_running)
        return -1;

    int len = 0;
    // uint32_t ssrc;
    // int payload_type, seq, flags = 0;
    int payload_type, flags = 0;
    int ext, csrc;
    // uint32_t timestamp;
    uint16_t rtp_len = 0;
    // uint8_t rtp_type = 0;
    if (buf[0] == 0x24)
    {
        // rtp_type = buf[1];
        rtp_len = (buf[2] << 8) | buf[3];
        if (rtp_len != size - 4)
        {
            rtsp_play_log("rtp len error: %d", rtp_len);
            return -1;
        }
        buf += 4;
        len = size - 4;
    }
    else
        len = size;

    csrc = buf[0] & 0x0f;
    ext = buf[0] & 0x10;
    payload_type = buf[1] & 0x7f;
    if (buf[1] & 0x80)
        flags |= 0x2;
    // seq = HTONS(*(uint16_t *)(buf + 2));
    // timestamp = HTONL(*(uint32_t *)(buf + 4));
    // ssrc = HTONL(*(uint32_t *)(buf + 8));
    if (buf[0] & 0x20)
    {
        int padding = buf[len - 1];
        if (len >= 12 + padding)
            len -= padding;
    }

    len -= 12;
    buf += 12;

    len -= 4 * csrc;
    buf += 4 * csrc;

    if (payload_type != 0x21)
    {
        len -= 4;
        buf += 4;
    }

    if (len < 0)
    {
        rtsp_play_log("RTP data len error!(len < 4)\n");
        return -1;
    }
    if (ext)
    {
        if (len < 4)
        {
            rtsp_play_log("RTP data len error!(len < 4)\n");
            return -1;
        }

        ext = (HTONS(*(uint16_t *)(buf + 2)) + 1) << 2;

        if (len < ext)
        {
            rtsp_play_log("RTP data len error!(len < ext)\n");
            return -1;
        }

        len -= ext;
        buf += ext;
    }
    this->err_cnt = 0;
    memcpy(this->mp3_buff, buf, len);
    return lt_mp3_parse(this->mp3_buff, len, &this->mp3_info);
}

static int rtsp_command_callback(void *user_arg)
{
    if (user_arg == NULL)
        return -1;

    lt_rtsp_trans_t *this = (lt_rtsp_trans_t *)user_arg;
    ebs_event_t event = {0};
    if (this->queue)
    {
        if (0 == ebs_msg_queue_wait(this->queue, (uint8_t *)&event, sizeof(ebs_event_t), QL_NO_WAIT))
        {
            if (event.id == LT_RTSP_STOP)
            {
                rtsp_play_log("===recv rtsp play stop cmd===");
                // this->client->teardown(this->client);
                // this->is_running = false;
                this->is_recv_stop = true;
            }
        }
    }
    return 0;
}

static void lt_rtsp_play_thread(void *arg)
{
    lt_rtsp_trans_t *this = (lt_rtsp_trans_t *)arg;
    int ret = 0;
    lt_rtsp_state_t state = LT_RTSP_STATE_CONNECTION;
    if (this->url == NULL)
    {
        rtsp_play_log("rtsp play url is NULL");
        return;
    }

    if (this->client)
        lt_rtsp_client_destory(&this->client);

    this->client = lt_rtsp_client_init();
    if (this->client == NULL)
    {
        rtsp_play_log("rtsp client init failed");
        return;
    }

    ret = ebs_msg_queue_creat(&this->queue, sizeof(ebs_event_t), LT_RTSP_MSG_MAX_CNT);
    if (ret)
    {
        rtsp_play_log("rtsp msg queue creat failed");
        goto exit;
    }

    this->mp3_buff = (uint8_t *)malloc(1024 * 20);
    if (this->mp3_buff == NULL)
    {
        rtsp_play_log("rtsp mp3 buff malloc failed");
        goto exit;
    }
    memset(&this->mp3_info, 0, sizeof(mp3_info_t));
    this->client->user_arg = (void *)this;
    this->client->cmd_callback = rtsp_command_callback;
    this->client->rtp_callback = rtp_data_callback;
    this->err_cnt = 0;
    this->is_running = true;
    this->is_recv_stop = false;

    while (this->is_running)
    {
        switch (state)
        {
        case LT_RTSP_STATE_CONNECTION:
        {
            rtsp_play_log("rtsp client connect");
            if ((ret = this->client->cconnect(this->client, this->url)) != 0)
            {
                rtsp_play_log("rtsp client connect failed");
                this->err_cnt++;
                ebs_sleep(1);
            }
            else
            {
                this->err_cnt = 0;
                state = LT_RTSP_STATE_OPTIONS;
            }
        }
        break;
        case LT_RTSP_STATE_OPTIONS:
        {
            rtsp_play_log("rtsp client options");
            if ((ret = this->client->options(this->client)) != 0)
            {
                rtsp_play_log("rtsp client options failed");
                this->err_cnt++;
                state = LT_RTSP_STATE_CONNECTION;
                ebs_sleep(1);
            }
            else
            {
                this->err_cnt = 0;
                state = LT_RTSP_STATE_DESCRIBE;
            }
        }
        break;
        case LT_RTSP_STATE_DESCRIBE:
        {
            rtsp_play_log("rtsp client describe");
            if ((ret = this->client->describe(this->client)) != 0)
            {
                rtsp_play_log("rtsp client describe failed");
                if (ret == -2)
                {
                    // goto exit;
                    state = LT_RTSP_STATE_TEARDOWN;
                }
                else
                {
                    this->err_cnt++;
                    state = LT_RTSP_STATE_CONNECTION;
                    ebs_sleep(1);
                }
            }
            else
            {
                this->err_cnt = 0;
                state = LT_RTSP_STATE_SETUP;
            }
        }
        break;
        case LT_RTSP_STATE_SETUP:
        {
            rtsp_play_log("rtsp client setup");
            if ((ret = this->client->setup(this->client)) != 0)
            {
                rtsp_play_log("rtsp client setup failed");
                this->err_cnt++;
                state = LT_RTSP_STATE_CONNECTION;
                ebs_sleep(1);
            }
            else
            {
                this->err_cnt = 0;
                ql_rtos_semaphore_release(this->sem_play);
                state = LT_RTSP_STATE_PLAY;
            }
        }
        break;
        case LT_RTSP_STATE_PLAY:
        {
            if ((ret = this->client->play(this->client)) != 0)
            {
                rtsp_play_log("rtsp client play failed ret:%d ", ret);
                this->err_cnt++;
                ebs_sleep(1);
            }
            else
                ebs_sleep_ms(200);
        }
        break;
        case LT_RTSP_STATE_TEARDOWN:
        {
            rtsp_play_log("rtsp client teardown");
            goto exit;
        }
        break;
        }
        if (this->err_cnt >= 10)
        {
            rtsp_play_log("rtsp play failed");
            state = LT_RTSP_STATE_TEARDOWN;
        }
        if (this->is_recv_stop)
            state = LT_RTSP_STATE_TEARDOWN;
    }

exit:
    this->client->teardown(this->client);
    rtsp_play_log("rtsp play teardown");

    if (this->stop_callback)
        this->stop_callback(this);
    ql_aud_data_done(); // 告诉内核数据已写完,所有数据解码完成后停止播放,这一步必须要有
    // ql_aud_wait_play_finish(QL_WAIT_FOREVER);
    // rtsp_play_log("rtsp play wait finish");
    ql_aud_player_stop();
    rtsp_play_log("rtsp play exit");
    this->destroy(this);
}

static void lt_rtsp_play_destroy(lt_rtsp_trans_t *this)
{
    if (this == NULL)
        return;

    if (this->client)
        lt_rtsp_client_destory(&this->client);

    if (this->queue)
    {
        ebs_event_t event = {0};
        do
        {
            memset(&event, 0, sizeof(ebs_event_t));
            if (0 != ebs_msg_queue_wait(this->queue, (uint8_t *)&event, sizeof(ebs_event_t), 0))
                break;
        } while (1);
        ebs_msg_queue_delete(this->queue);
        this->queue = NULL;
    }

    if (this->mp3_buff)
    {
        free(this->mp3_buff);
        this->mp3_buff = NULL;
    }

    this->err_cnt = 0;
    this->is_recv_stop = false;
    this->stop_callback = NULL;
    this->url = NULL;
    this->is_running = false;
    ql_task_t task = this->task;
    this->task = NULL;
    ql_rtos_task_delete(task);
}

static lt_rtsp_trans_t lt_rtsp_trans = {
    .task = 0,
    .is_running = false,
    .is_recv_stop = false,
    .err_cnt = 0,
    .client = NULL,
    .url = NULL,
    .queue = NULL,
    .sem_play = NULL,
    .mp3_buff = NULL,
    .mp3_info = {.parsed = false, .id3v2_parsed = false, .id3v2_size = 0},
    .stop_callback = NULL,
    .destroy = lt_rtsp_play_destroy,
};

int lt_rtsp_play_start(const char *url)
{
    if (lt_rtsp_trans.is_running)
    {
        rtsp_play_log("rtsp play is running");
        return -1;
    }
    QlOSStatus err = QL_OSI_SUCCESS;
    if (lt_rtsp_trans.sem_play)
    {
        ql_rtos_semaphore_delete(lt_rtsp_trans.sem_play);
        lt_rtsp_trans.sem_play = NULL;
    }
    if ((err = ql_rtos_semaphore_create(&lt_rtsp_trans.sem_play, 0)) != QL_OSI_SUCCESS)
    {
        rtsp_play_log("rtsp play sem create failed");
        return -1;
    }
    lt_rtsp_trans.url = url;
    rtsp_play_log("rtsp play url: %s", url);
    err = ql_rtos_task_create(&lt_rtsp_trans.task, 1024 * 100, APP_PRIORITY_HIGH, "lt_rtsp_play", lt_rtsp_play_thread, &lt_rtsp_trans, 5);
    if (err != QL_OSI_SUCCESS)
    {
        rtsp_play_log("rtsp play task create failed");
        goto error;
    }
    if (lt_rtsp_trans.sem_play)
    {
        err = ql_rtos_semaphore_wait(lt_rtsp_trans.sem_play, 2000);
        if (err != QL_OSI_SUCCESS)
        {
            rtsp_play_log("rtsp play task timeout");
            goto error;
        }
        return 0;
    }

error:
    if (lt_rtsp_trans.sem_play)
    {
        ql_rtos_semaphore_delete(lt_rtsp_trans.sem_play);
        lt_rtsp_trans.sem_play = NULL;
    }
    lt_rtsp_trans.url = NULL;
    return -1;
}

int lt_rtsp_play_start_ex(const char *url, void (*callback)(void *))
{
    if (lt_rtsp_trans.is_running)
    {
        rtsp_play_log("rtsp play is running");
        return -1;
    }

    lt_rtsp_trans.stop_callback = callback;
    return lt_rtsp_play_start(url);
}

int lt_rtsp_play_stop(void)
{
    rtsp_play_log("rtsp play stop");
    if (!lt_rtsp_trans.is_running)
        return 0;

    lt_rtsp_trans_t *this = &lt_rtsp_trans;
    ebs_event_t event = {0};
    event.id = LT_RTSP_STOP;
    if (this->queue)
        ebs_msg_queue_send(this->queue, (uint8_t *)&event, sizeof(ebs_event_t), EBS_WAIT_FOREVER);
    while (lt_rtsp_trans.is_running)
    {
        ebs_sleep_ms(20);
    }
    return 0;
}

int lt_rtsp_play_stop_direct(void)
{
    rtsp_play_log("rtsp play stop direct");

    if (!lt_rtsp_trans.is_running)
        return 0;

    lt_rtsp_trans.is_running = false;
    ql_task_status_t status;
    do
    {
        if (lt_rtsp_trans.task)
            ql_rtos_task_get_status(lt_rtsp_trans.task, &status);
    } while (lt_rtsp_trans.task && status.eCurrentState < Deleted);
    return 0;
}

bool lt_rtsp_play_status_query(void)
{
    return lt_rtsp_trans.is_running;
}
