/**
 * @file ebm_task_play.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-12-07
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#include "ebs.h"
#include "ltplay.h"
#include "ebm_task_play.h"
#include "ebm_task_schedule.h"
#include "ebs_param.h"
#include "ip_gb_comm.h"
#if PLATFORM == PLATFORM_LINUX
#include "ebs_log.h"
#include "util_time.h"
#include "hwdrv.h"
#include "lootomplayer.h"
#include "audiodecode.h"
#include "ebm_alsa_volume.h"
#include "util_os_api.h"

#include <pthread.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#elif PLATFORM == PLATFORM_Ex800G
#include "util_msg.h"
#include "util_timer.h"
#include "util_mutex.h"
#include "ctype.h"
#include "lt_http.h"
#include "lt_rtsp_play.h"
#include "led.h"
#endif
#include <math.h>
#include <stdbool.h>
#include <string.h>

// static pthread_mutex_t mutex_player = PTHREAD_MUTEX_INITIALIZER;
// static pthread_mutex_t mutex_mp3 = PTHREAD_MUTEX_INITIALIZER;

static ebs_mutex_t mutex_player;
static ebs_mutex_t mutex_mp3;

DECLARE_CIRCULAR_BUFFER(play, 0x8000, &mutex_mp3);

#define PLAYER_SCH_TIME 200
#define PLAYER_ERR_MAX_CNT 6

static void target_volume_update(lt_ebs_player_t *this)
{
    if (this->task != NULL)
    {
        if (this->task->ebm_type != 5)
        {
            this->vol_target = 100;
        }
        else
        {
            this->vol_target = (volume_t)(this->task->volume * ebs_param.volume * 0.01);
        }
        this->vol_target = this->vol_target < 0 ? 0 : this->vol_target;
        this->vol_target = this->vol_target > 100 ? 100 : this->vol_target;
        if (this->vol_target == 0 || fabs(this->vol_target - this->vol) <= 8)
        {
            if (this->vol_target != this->vol)
            {
                this->vol = this->vol_target;
                // TODO 设置音量
                // alsa_volume_set(this->vol);
            }
        }
        else
        {
            this->vol_step = this->vol_target > this->vol ? 2.0 : -2.0;
        }
    }
}

static int channel_priority_check(const char *priority)
{
    if (priority == NULL || strlen(priority) != CHN_MAX_CNT)
        return -1;
    uint8_t i, j = 0;
    char prior[CHN_MAX_CNT + 1] = {0};
    char temp = 0;
    memcpy(prior, priority, CHN_MAX_CNT);
    for (i = 0; i < CHN_MAX_CNT; i++)
    {
        for (j = 0; j < CHN_MAX_CNT; j++)
        {
            if (prior[j] > prior[j + 1])
            {
                temp = prior[j + 1];
                prior[j + 1] = prior[j];
                prior[j] = temp;
            }
        }
    }
    return memcmp(prior, "12345678", CHN_MAX_CNT);
}

static audio_source_t task_audio_source_get(const char *url)
{
    if (TOUPPER(url[0]) == 'R' && TOUPPER(url[1]) == 'T')
    {
        if (TOUPPER(url[2]) == 'P' && TOUPPER(url[3]) == ':')
            return AUDIO_RTP;
        else
            return AUDIO_RTSP;
    }
    else if (TOUPPER(url[0]) == 'U' && TOUPPER(url[1]) == 'D' && TOUPPER(url[2]) == 'P')
    {
        return AUDIO_UDP;
    }
    else if (TOUPPER(url[0]) == 'H' && TOUPPER(url[1]) == 'T' && TOUPPER(url[2]) == 'T' && TOUPPER(url[3]) == 'P')
    {
        return AUDIO_HTTP;
    }
    return AUDIO_NONE;
}

static volume_t play_volume_gradient(lt_ebs_player_t *this, volume_t volume)
{
    if (volume == 0)
    {
        this->vol = 0;
        return 0;
    }
    // else if (!audio_is_start())
    //     return 0;
    volume = volume < 0 ? 0 : volume;
    volume = volume > 100 ? 100 : volume;
    if (this->vol != volume)
    {
        if (fabs(this->vol - volume) < 2 * fabs(this->vol_step))
        {
            this->vol = volume;
        }
        else
        {
            this->vol += this->vol_step;
            this->vol = this->vol > 100 ? 100 : this->vol;
            this->vol = this->vol < 0 ? 0 : this->vol;
        }
        if (this->play_chn != CHN_NULL_MASK)
        {
            if (this->play_chn != CHN_FM_MASK)
            {
                // alsa_volume_set(this->vol);//TODO
                // ql_set_volume(AUDIOHAL_SPK_VOL_11);
            }
            else
            {
                // TODO FM设置音量
            }
        }
        if (this->vol == 0)
        {
            // TODO 关闭功放
            //  alsa_volume_set(0);
            //  hwdrv_power_amplifier_set(0);
            //  hwdrv_broadcast_mode_set(BC_DAILY);
            // ql_set_volume(AUDIOHAL_SPK_MUTE);
        }
    }
    return this->vol;
}

static int play_switch_report(ebm_task_t *task, uint8_t status)
{
    lbk_msg_t msg = {.type = 0, .len = 0, .data = {0}};
    time_t utc;
    lbk_task_switch_t *s = (lbk_task_switch_t *)msg.data;
    s->switch_mark = status & 0x03;
    s->task_type = (task->ebm_type == 5) + 1;
    memcpy(s->ebm_id, task->ebm_id, EBM_ID_LEN);
    utc = ebs_time(NULL); // TODO UTC时间戳
    // s->time[0] = (utc >> 24) & 0xFF;
    // s->time[1] = (utc >> 16) & 0xFF;
    // s->time[2] = (utc >> 8) & 0xFF;
    // s->time[3] = utc & 0xFF;
    *(uint32_t *)(s->time) = HTONL(utc);
    msg.len = sizeof(lbk_task_switch_t);
    msg.type = LBK_TASK_SWITCH;
    return ebs_lbk_msg_send(&msg);
}

static int play_prepare(lt_ebs_player_t *this, ebm_task_t *task)
{
    uint8_t i = 0;
    if (this->is_ipsc || task->ebm_type == 5)
    {
        if (task->volume != 0xFF)
            this->vol_target = (int32_t)(task->volume * ebs_param.volume * 0.01);
        else if (this->vol_target == 0)
            this->vol_target = ebs_param.volume * 1.0;
        this->vol_target = this->vol_target < 0 ? 0 : this->vol_target;
        this->vol_target = this->vol_target > 100 ? 100 : this->vol_target;
    }
    else
    {
        this->vol_target = 100;
    }
    // hwdrv_broadcast_mode_set(task->ebm_type);//TODO EBM类型
    this->vol = (this->vol == this->vol_target) ? (this->vol - 4) : this->vol;

    for (i = 0; i < CHN_MAX_CNT; i++)
    {
        if (task->chn_mask & this->chn_prio[i])
            break;
    }
    if (i >= CHN_MAX_CNT)
    {
        ebs_log("task channel error!\n");
        return -1;
    }
    if (this->chn_prio[i] & CHN_IP_MASK)
    {
        this->play_chn = CHN_IP_MASK;
        task->audio_source = task_audio_source_get(task->url);
    }
    else if (this->chn_prio[i] & CHN_4G_MASK)
    {
        this->play_chn = CHN_4G_MASK;
        task->audio_source = task_audio_source_get(task->url);
    }
    else if (this->chn_prio[i] & CHN_FM_MASK)
    {
        this->play_chn = CHN_FM_MASK;
        task->audio_source = AUDIO_FM;
    }

    //! 任务准备阶段判断本地FM开播状态，如在播停播本地FM
    // if (ltplay_get_src() == SND_FM) // fm 切换了声卡通道
    //     ltplay_stop();

    switch (task->audio_source)
    {
    case AUDIO_UDP:
    case AUDIO_RTP:
    {
        this->vol_step = this->vol_target > this->vol ? 2.0 : -2.0;
        // hwdrv_broadcast_channel_select(BC_IP); // TODO IP/4G区分
        // TODO 开播
    }
    break;
    case AUDIO_RTSP:
    {
        this->vol_step = this->vol_target > this->vol ? 2.0 : -2.0;
        // hwdrv_broadcast_channel_select(BC_IP); // TODO IP/4G区分
        // mp3play_stop();
        // if (mp3play_start(task->url) != 0)
        // {
        //     ebs_log("mp3play_start error!\n");
        //     return -1;
        // }
        if (0 != lt_rtsp_play_start(task->url))
        {
            return -1;
        }
        timer_start(&this->timer_delay);
    }
    break;
    case AUDIO_HTTP:
    {
        // hwdrv_broadcast_channel_select(BC_IP); // TODO IP/4G区分
        // TODO 开播
        // if (0 != lt_http_play_start(task->url))
        // if (0 != lt_http_play_start_ex(task->url, task->callback))
        if (0 != lt_http_play_start_ebm_task(task))
        {
            return -1;
        }
        timer_start(&this->timer_delay);
    }
    break;
    case AUDIO_FM:
    {
        this->vol_step = this->vol_target > this->vol ? 2.0 : -2.0;
        // hwdrv_broadcast_channel_select(BC_FM); // TODO IP/4G区分
        // TODO 开播
    }
    break;
    default:
        break;
    }

    task->status = TASK_STATUS_RUNNING;
    task->real_start_time = ebs_time(NULL);

    if (task->ebm_type <= 2)
    {
        // hwdrv_power_amplifier_set(0); //TODO
    }
    else
    {
        // hwdrv_power_amplifier_set(1);
    }
    // hwdrv_led_status_set(LED_PLAY_CHANNEL, GREEN_FLASHING_FAST);
    return 0;
}

static int play_running(lt_ebs_player_t *this, ebm_task_t *task)
{
    int result = 0;
    this->volume_gradient(this, this->vol_target);

    switch (task->audio_source)
    {
    case AUDIO_UDP:
    case AUDIO_RTP:
    {
        // TODO 获取播放状态
    }
    break;
    case AUDIO_RTSP:
    {
        // ! 等待4秒后再去获取播放器状态
        if (timer_expired(&this->timer_delay, 4000))
        {
            // if (!lootom_player_status())
            //     return -1;
            if (!lt_rtsp_play_status_query())
                return -1;
        }
    }
    break;
    case AUDIO_HTTP:
    {
        // TODO 获取播放状态
        if (timer_expired(&this->timer_delay, 4000))
        {
            if (lt_http_status_query() != 0)
            {
                return -1;
            }
        }
    }
    break;
    case AUDIO_FM:
    {
        if (this->is_fm_remain)
        {
            if (timer_expired(&this->timer_fm_remain, this->fm_remain_period) && !task->is_ip_fm)
            {
                task->status = TASK_STATUS_CMPLT;
                task->real_stop_time = ebs_time(NULL);
            }
        }
    }
    break;
    default:
        break;
    }
    return result;
}

static int play_finish(lt_ebs_player_t *this, ebm_task_t *task)
{
    uint8_t channel;
    this->prepare_times = 0;
    channel = task->chn_mask;
    switch (task->audio_source)
    {
    case AUDIO_UDP:
    case AUDIO_RTP:
    {
        task->chn_mask &= ~this->play_chn;
    }
    break;
    case AUDIO_RTSP:
    {
        task->chn_mask &= ~this->play_chn;
        // mp3play_stop();
        // lt_rtsp_play_stop();
        lt_rtsp_play_stop_direct();
        ebs_log("rtsp play stop!\n");
    }
    break;
    case AUDIO_HTTP:
    {
        task->chn_mask &= ~this->play_chn;
        lt_http_play_stop_direct();
        ebs_log("http play stop!\n");
    }
    break;
    case AUDIO_FM:
    {
        task->chn_mask &= ~this->play_chn;
    }
    break;
    default:
        break;
    }
    this->play_chn = CHN_NULL_MASK;
    switch (task->status)
    {
    case TASK_STATUS_INTRPT:
        task->chn_mask = channel;
        task->status = TASK_STATUS_READY;
        this->task = this->task_high;
        this->task_high = NULL;
        break;
    case TASK_STATUS_ERROR:
    case TASK_STATUS_CMPLT:
    case TASK_STATUS_STOP:
    {
        ebs_log("play task stop process ok, prepare delete task!\n");
        task->play_result = (task->status == TASK_STATUS_ERROR) ? PLAY_RESULT_ERR : PLAY_RESULT_OK;
        task->status = TASK_STATUS_EXIT;
        task->real_stop_time = ebs_time(NULL);
        this->vol_target = 0;
        // this->vol_step = (this->vol_target - this->vol) * this->sleep_time / VLOUME_SET_TIME;
        this->vol_step = 2.0;
        if (this->play_switch_report)
            this->play_switch_report(this->task, 2);
        this->task = NULL;
    }
    default:
        break;
    }
    device_work_mode_status_set(0x01);
    // hwdrv_led_status_set(LED_PLAY_CHANNEL, RED_LIGHT);
    // hwdrv_power_amplifier_set(0);
    return 0;
}

void ebs_task_play_thread(void *arg)
{
    lt_ebs_player_t *this = &eb_player;
    char ebm_id[64] = {0};
    time_t timer_run = 0;
    time_t timer_schedule = 0;
    task_result_t result = TASK_ERR_EMPTY;
#if PLATFORM == PLATFORM_LINUX
    os_sem_t *sem = (os_sem_t *)arg;
    os_sem_post(sem);
#endif
    this->is_fm_remain = ebs_param.is_fm_remain;
    this->is_chn_switch = ebs_param.is_chn_switch;
    this->is_ipsc = ebs_param.is_ipsc;
    this->fm_remain_period = ebs_param.fm_remain_period * 1000;
    if (channel_priority_check(ebs_param.chn_priority) != 0)
    {
        memcpy(ebs_param.chn_priority, "123", 3);
        // db_save(EBS_DB_FILE);
    }
    eb_player_channel_priority_set(ebs_param.chn_priority);
    // alsa_volume_set(0);

    while (1)
    {
        if (timer_expired(&timer_schedule, PLAYER_SCH_TIME) || result != TASK_RESULT_OK)
        {
            timer_start(&timer_schedule);
            if ((result = this->task_add_del_process(this)) != TASK_RESULT_OK)
            {
                if (result != TASK_ERR_EMPTY)
                {
                    ebs_log("task add or del result:%d.\n", result);
                }
            }
            if ((result = this->schedule(this)) == TASK_RESULT_OK)
            {
                if (timer_expired(&this->timer_log, 10000))
                {
                    timer_start(&this->timer_log);
                    ebs_log("=============================================\n");
                    ebs_log("player task list num:%d \n", this->task_list->task_num);
                    ebs_log("Current task play channel:%d \n", this->play_chn);
                    if (this->task)
                    {
                        uint8_t i = 0;
                        for (i = 0; i < EBM_ID_LEN; i++)
                            sprintf(&ebm_id[i * 2], "%02X", this->task->ebm_id[i]);
                        ebs_log("task ebm id:%s \n", ebm_id);
                        ebs_log("task channel mask:%02X \n", this->task->chn_mask);
                        ebs_log("task status:%d \n", this->task->status);
                        ebs_log("task ebm type:%d,level:%d,volume:%d \n", this->task->ebm_type, this->task->severity, this->task->volume);
                        ebs_log("task start time:%ld,stop time:%ld \n", this->task->start_time, this->task->stop_time);
                        if (this->task->chn_mask & CHN_IP_MASK)
                            ebs_log("task ip url:%s \n", this->task->url);
                        if (this->task->chn_mask & CHN_4G_MASK)
                            ebs_log("task 4g url:%s \n", this->task->url);
                        if (this->task->chn_mask & CHN_FM_MASK)
                            ebs_log("task mf freq:%d \n", this->task->fm_freq);
                    }
                    ebs_log("=============================================\n");
                }
            }
        }
        if (this->task == NULL || this->task_list->task_num == 0)
        {
            device_work_mode_status_set(0x01);
            if (this->vol != 0)
                this->volume_gradient(this, 0);
            continue;
        }
        else if (this->task->status != TASK_STATUS_RUNNING)
        {
            device_work_mode_status_set(0x01);
        }

        switch (this->task->status)
        {
        case TASK_STATUS_READY:
        {
            if (this->prepare(this, this->task) < 0)
            {
                this->prepare_times++;
                if (this->task->chn_mask & (this->task->chn_mask - 1))
                {
                    if (this->prepare_times >= 5)
                    {
                        this->prepare_times = 0;
                        ebs_log("play task prepare error,change channel!\n");
                        this->task->status = TASK_STATUS_READY;
                        this->finish(this, this->task);
                    }
                }
                else
                {
                    if (this->prepare_times >= PLAYER_ERR_MAX_CNT)
                    {
                        ebs_log("play task prepare error:%d \n", this->task->audio_source);
                        this->prepare_times = 0;
                        this->task->status = TASK_STATUS_ERROR;
                    }
                }
                continue;
            }
            else
            {
                ebs_log("task prepare success\n");
                this->prepare_times = 0;
                // this->task->run_err_times = 0;
                timer_start(&timer_run);
                if (this->play_switch_report)
                    this->play_switch_report(this->task, 1);
#if PLATFORM == PLATFORM_Ex800G
                if (this->prepare_sucess_callback)
                    this->prepare_sucess_callback();
#endif
            }
        }
        break;
        case TASK_STATUS_RUNNING:
        {
            if (this->play(this, this->task) < 0)
            {
                if (this->task->chn_mask & (this->task->chn_mask - 1))
                {
                    if (timer_expired(&timer_run, 2000))
                    {
                        ebs_log("get play data error,change channel!\n");
                        this->task->status = TASK_STATUS_READY;
                        this->finish(this, this->task);
                    }
                }
                else
                {
                    if (this->task->run_err_times >= PLAYER_ERR_MAX_CNT)
                    {
                        this->task->run_err_times = 0;
                        ebs_log("get play data error!!!\n");
                        this->task->play_result_info = (char *)malloc(256);
                        if (this->task->play_result_info)
                        {
                            memset(this->task->play_result_info, 0, 256);
                            strcpy(this->task->play_result_info, "Get audio data error!");
                            this->task->info_len = strlen(this->task->play_result_info);
                        }
                        this->task->status = TASK_STATUS_ERROR;
                    }
                    else if (timer_expired(&timer_run, 1000))
                    {
                        timer_start(&timer_run);
                        this->task->run_err_times++;
                        ebs_log("get play data error,start reprepare:%d\n", this->task->run_err_times);
                        this->prepare(this, this->task);
                    }
                }
            }
            else
            {
                device_work_mode_status_set(0x02);
                this->task->play_result = PLAY_RESULT_RUN;
                this->task->run_err_times = 0;
            }
        }
        break;
        case TASK_STATUS_CMPLT:
        {
            ebs_log("task play complete!\n");
            // this->task->play_result = PLAY_RESULT_OK;
            // if (this->play_switch_report)
            //     this->play_switch_report(this->task, 2);
            this->finish(this, this->task);
        }
        break;
        case TASK_STATUS_STOP:
        {
            ebs_log("task was killed!\n");
            // this->task->play_result = PLAY_RESULT_OK;
            // if (this->play_switch_report)
            //     this->play_switch_report(this->task, 2);
            this->finish(this, this->task);
        }
        break;
        case TASK_STATUS_INTRPT:
        {
            ebs_log("task wsi interrupted by a high priority task!\n");
            // this->task->play_result = PLAY_RESULT_OK;
            // if (this->play_switch_report)
            //     this->play_switch_report(this->task, 2);
            this->finish(this, this->task);
        }
        break;
        case TASK_STATUS_ERROR:
        {
            ebs_log("task play failed!\n");
            // this->task->play_result = PLAY_RESULT_ERR;
            // if (this->play_switch_report)
            //     this->play_switch_report(this->task, 2);
            this->finish(this, this->task);
        }
        break;
        default:
            break;
        }
        ebs_sleep_ms(200);
    }
}

task_result_t eb_player_task_msg_send(ebm_task_msg_t *msg)
{
#if PLATFORM == PLATFORM_LINUX
    mqueue_msg_t *m = mqueue_msg_alloc(msg->type, sizeof(msg->buff));
    uint8_t *data = mqueue_msg_body(m);

    memcpy(data, msg->buff, sizeof(msg->buff));
    if (mqueue_timed_put(eb_player.task_queue, m, 1000) != 0)
    {
        ebs_log("ebm task msg queue send failed!\n");
        return TASK_ERR_MSG_SND;
    }
    return TASK_RESULT_OK;
#elif PLATFORM == PLATFORM_Ex800G

    //! 任务准备阶段判断本地FM开播状态，如在播停播本地FM
    // if (ltplay_get_src() == SND_MP3_URL || ltplay_get_src() == SND_FM_URL || ltplay_get_src() == SND_FM) // fm 切换了声卡通道
    //     ltplay_stop();

    ebs_msg_t m = {0};
    m.type = msg->type;
    m.size = sizeof(msg->buff);
    m.data = ebs_msg_malloc(m.size);
    memcpy(m.data, msg->buff, m.size);
    if (ebs_msg_queue_send(eb_player.task_queue, (uint8_t *)&m, sizeof(ebs_msg_t), 1000) != 0)
    {
        ebs_log("ebm task msg queue send failed!\n");
        ebs_msg_free(m.data);
        return TASK_ERR_MSG_SND;
    }
    return TASK_RESULT_OK;
#endif
}

task_result_t eb_player_werbaudio_task_msg_send(ebm_task_msg_t *msg)
{
#if PLATFORM == PLATFORM_LINUX
    mqueue_msg_t *m = mqueue_msg_alloc(msg->type, sizeof(msg->buff));
    uint8_t *data = mqueue_msg_body(m);

    memcpy(data, msg->buff, sizeof(msg->buff));
    if (mqueue_timed_put(eb_player.task_queue, m, 1000) != 0)
    {
        ebs_log("ebm task msg queue send failed!\n");
        return TASK_ERR_MSG_SND;
    }
    return TASK_RESULT_OK;
#elif PLATFORM == PLATFORM_Ex800G

    //! 任务准备阶段判断本地FM开播状态，如在播停播本地FM
    // if (ltplay_get_src() == SND_MP3_URL || ltplay_get_src() != SND_FM_URL || ltplay_get_src() != SND_FM) // fm 切换了声卡通道
    //     ltplay_stop();

    ebs_msg_t m = {0};
    m.type = msg->type;
    m.size = sizeof(msg->buff);
    m.data = ebs_msg_malloc(m.size);
    memcpy(m.data, msg->buff, m.size);
    if (ebs_msg_queue_send(eb_player.task_queue, (uint8_t *)&m, sizeof(ebs_msg_t), 1000) != 0)
    {
        ebs_log("ebm task msg queue send failed!\n");
        ebs_msg_free(m.data);
        return TASK_ERR_MSG_SND;
    }
    return TASK_RESULT_OK;
#endif
}
task_result_t eb_player_current_task_delete(void)
{
    ebs_msg_t m = {0};
    m.type = TASK_CUR_DEL;
    m.size = 0;
    m.data = NULL;
    if (ebs_msg_queue_send(eb_player.task_queue, (uint8_t *)&m, sizeof(ebs_msg_t), 1000) != 0)
    {
        ebs_log("ebm task msg queue send failed!\n");
        return TASK_ERR_MSG_SND;
    }
    return TASK_RESULT_OK;
}

task_result_t eb_player_all_task_delete(void)
{
    ebs_msg_t m = {0};
    m.type = TASK_ALL_DEL;
    m.size = 0;
    m.data = NULL;
    if (ebs_msg_queue_send(eb_player.task_queue, (uint8_t *)&m, sizeof(ebs_msg_t), 1000) != 0)
    {
        ebs_log("ebm task msg queue send failed!\n");
        return TASK_ERR_MSG_SND;
    }
    return TASK_RESULT_OK;
}

int eb_player_channel_priority_set(char *priority)
{
    ebs_mutex_lock(&mutex_player);
    if (priority == NULL)
    {
        ebs_mutex_unlock(&mutex_player);
        return -1;
    }
    uint8_t i = 0;
    uint8_t check = 0;
    uint8_t mask = 0;
    uint8_t chn_num = CHN_MAX_CNT;
    for (i = 0; i < chn_num; i++)
    {
        check |= (1 << (priority[i] - '1'));
        mask |= (1 << i);
    }
    if (check != mask)
    {
        ebs_mutex_unlock(&mutex_player);
        return -1;
    }
    for (i = 0; i < chn_num; i++)
    {
        eb_player.chn_prio[i] = 1 << (priority[i] - '1');
    }
    ebs_mutex_unlock(&mutex_player);
    return 0;
}

void eb_player_channel_priority_get(uint8_t *priority)
{
    if (priority == NULL)
        return;
    ebs_mutex_lock(&mutex_player);
    uint8_t i = 0;
    for (i = 0; i < CHN_MAX_CNT; i++)
    {
        if (eb_player.chn_prio[i] == 1)
            priority[0] = i + 1;
        else if (eb_player.chn_prio[i] == 2)
            priority[1] = i + 1;
        else if (eb_player.chn_prio[i] == 4)
            priority[2] = i + 1;
    }
    ebs_mutex_unlock(&mutex_player);
}

/**
 * @brief
 *
 * @param is_upper_first 0:下级优先,其它:上级优先
 */
void eb_player_platform_priority_set(bool is_upper_first)
{
    ebs_mutex_lock(&mutex_player);
    eb_player.is_upper_first = is_upper_first;
    ebs_mutex_unlock(&mutex_player);
}

/**
 * @brief 同一任务不同通道直接是否根据通道优先级进行切换
 *
 * @param is_chn_switch
 */
void eb_player_channel_switch_set(bool is_chn_switch)
{
    ebs_mutex_lock(&mutex_player);
    eb_player.is_chn_switch = is_chn_switch;
    ebs_mutex_unlock(&mutex_player);
}

void eb_player_ipsc_mode_set(bool is_ipsc)
{
    ebs_mutex_lock(&mutex_player);
    eb_player.is_ipsc = is_ipsc;
    ebs_mutex_unlock(&mutex_player);
}

void eb_player_fm_remain_set(bool status, uint16_t remain_time)
{
    ebs_mutex_lock(&mutex_player);
    eb_player.is_fm_remain = status;
    eb_player.fm_remain_period = remain_time * 1000;
    ebs_mutex_unlock(&mutex_player);
}

bool eb_player_fm_remain_query(void)
{
    bool status;
    ebs_mutex_lock(&mutex_player);
    status = eb_player.is_fm_remain;
    ebs_mutex_unlock(&mutex_player);
    return status;
}

void eb_player_fm_remain_timer_reload(void)
{
    ebs_mutex_lock(&mutex_player);
    if (eb_player.task != NULL)
    {
        if (eb_player.task->audio_source == AUDIO_FM && eb_player.task->status == TASK_STATUS_RUNNING && eb_player.is_fm_remain)
        {
            timer_start(&(eb_player.timer_fm_remain));
            ebs_log("===FM remain timer reload===\n");
        }
    }
    ebs_mutex_unlock(&mutex_player);
}

int ebs_play_task_status_query(char *channel, uint8_t *ebm_type, uint8_t *event_level, uint8_t *volume)
{
    ebs_mutex_lock(&mutex_player);
    if (eb_player.task == NULL || eb_player.task_list->task_num == 0)
    {
        if (channel)
            strcpy(channel, "null");
        *ebm_type = 0;
        *event_level = 0;
        *volume = 0;
        ebs_mutex_unlock(&mutex_player);
        return 0;
    }
    switch (eb_player.play_chn)
    {
    case CHN_NULL_MASK:
        if (channel)
            strcpy(channel, "null");
        *ebm_type = 0;
        *event_level = 0;
        *volume = 0;
        break;
    case CHN_IP_MASK:
        if (channel)
            strcpy(channel, "IP");
        *ebm_type = eb_player.task->ebm_type;
        *event_level = eb_player.task->severity;
        *volume = eb_player.task->volume;
        break;
    case CHN_4G_MASK:
        if (channel)
            strcpy(channel, "IP");
        *ebm_type = eb_player.task->ebm_type;
        *event_level = eb_player.task->severity;
        *volume = eb_player.task->volume;
        break;
    case CHN_FM_MASK:
        if (channel)
            strcpy(channel, "FM");
        *ebm_type = eb_player.task->ebm_type;
        *event_level = eb_player.task->severity;
        *volume = eb_player.task->volume;
        break;
        break;
    default:
        break;
    }
    ebs_mutex_unlock(&mutex_player);
    return 0;
}

uint8_t ebs_player_task_ebm_type_query(void)
{
    ebs_mutex_lock(&mutex_player);
    uint8_t status = 0;
    if (eb_player.task == NULL)
        status = 0;
    else if (eb_player.task->ebm_type <= 2)
        status = 0;
    else if (eb_player.task->ebm_type == 5)
        status = 1;
    else
        status = 2;
    ebs_mutex_unlock(&mutex_player);
    return status;
}

task_source_t ebs_player_task_source_query(void)
{
    ebs_mutex_lock(&mutex_player);
    task_source_t source = TASK_SOURCE_EBS;
    if (eb_player.task == NULL)
        source = TASK_SOURCE_EBS;
    else
        source = eb_player.task->task_source;
    ebs_mutex_unlock(&mutex_player);
    return source;
}

int32_t eb_player_init(uint32_t stage)
{
    lt_ebs_player_t *this = &eb_player;
    this->is_fm_remain = ebs_param.is_fm_remain;
    this->is_ipsc = ebs_param.is_ipsc;
    this->is_upper_first = ebs_param.is_upper_first;
    this->is_chn_switch = ebs_param.is_chn_switch;
    this->fm_remain_period = ebs_param.fm_remain_period * 1000;
    memcpy(this->chn_prio, ebs_param.chn_priority, CHN_MAX_CNT);
    // this->task_queue = mqueue_new("eb_player_queue", 1024);
    ebs_msg_queue_creat(&this->task_queue, sizeof(ebs_msg_t), 256);

    // lootom_player_init();//TODO 播放器初始化

    return 0;
}

int32_t eb_player_deinit(void)
{
    lt_ebs_player_t *this = &eb_player;
    ebs_msg_queue_delete(&this->task_queue);
    return 0;
}

#if PLATFORM == PLATFORM_Ex800G
void eb_player_prepare_sucess_callback_set(int (*callback)(void))
{
    eb_player.prepare_sucess_callback = callback;
}
#endif

static ebm_task_list_t ebm_task_list = {
    .head = NULL,
    .tail = NULL,
    .task_num = 0,
};

lt_ebs_player_t eb_player = {
    .task = NULL,
    .task_high = NULL,
    .task_add_del_process = ebm_task_add_del_process,
    .schedule = ebm_task_schedule,
    .prepare = play_prepare,
#if PLATFORM == PLATFORM_Ex800G
    .prepare_sucess_callback = NULL,
#endif
    .play = play_running,
    .finish = play_finish,
    .play_switch_report = play_switch_report,
    .target_volume_update = target_volume_update,
    .volume_gradient = play_volume_gradient,
    .task_list = &ebm_task_list,
    .task_queue = NULL,
    .prepare_times = 0,
    // .run_err_times = 0,
    .is_fm_remain = true,
    .is_upper_first = true,
    .is_chn_switch = true,
    .is_ipsc = false,
    .fm_remain_period = 600000,
    .vol = 0,
    .vol_target = 0,
    .vol_step = 0,
    .chn_prio = {'1', '2', '3'},
};