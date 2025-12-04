/*
 * @Author: zhouhao
 * @Date: 2023-09-13 10:27:29
 * @Last Modified by: zhouhao
 * @Last Modified time: 2023-09-We 01:18:29
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ql_api_osi.h"
#include "ltplay.h"
#include "ql_log.h"
#include "ltsystem.h"
#include "ltadc.h"
#include "ql_api_osi.h"
#include "ql_api_dev.h"
#include "lt_tts_demo.h"
#include "ql_power.h"

#define QL_PLAY_LOG_LEVEL QL_LOG_LEVEL_INFO
#define QL_PLAY_LOG(msg, ...) QL_LOG(QL_PLAY_LOG_LEVEL, "ql_ltplay", msg, ##__VA_ARGS__)
#define QL_PLAY_LOG_PUSH(msg, ...) QL_LOG_PUSH("ql_ltplay", msg, ##__VA_ARGS__)

#define QL_LTPLAY_TASK_STACK_SIZE 4096
#define QL_LTPLAY_TASK_PRIO APP_PRIORITY_NORMAL
#define QL_LTPLAY_TASK_EVENT_CNT 5

static inline int play_mutex_lock(ql_mutex_t *mutex)
{
    return ql_rtos_mutex_lock(*mutex, QL_WAIT_FOREVER);
}

static inline int play_mutex_unlock(ql_mutex_t *mutex)
{
    return ql_rtos_mutex_unlock(*mutex);
}

play_state_t g_play_sta = {
    .src = SND_STOP,
};

int ltplay_get_src()
{
    return g_play_sta.src;
}
int ltplay_set_src(char src)
{
    g_play_sta.src = src;
    return 0;
}

static play_fsm_t g_play_fsm[] = {
    {SND_MP3, "mp3", NULL, NULL, NULL},
    {SND_FM, "fm", NULL, NULL, NULL},
    {SND_EBS, "ebs", NULL, NULL, NULL},
    {SND_SMS, "sms", NULL, NULL, NULL},
    {SND_TEL, "tel", NULL, NULL, NULL},
    {SND_CLK, "ebs", NULL, NULL, NULL},
    {SND_MP3_URL, "mp3_url", NULL, NULL, NULL},
    {SND_FM_URL, "fm_url", NULL, NULL, NULL},
};

// 注册play_callback
void ltplay_callback_register(play_fsm_t *reg)
{
    uint16_t num = 0;
    for (num = 0; num < sizeof(g_play_fsm) / sizeof(g_play_fsm[0]); num++)
    {
        if (reg->src == g_play_fsm[num].src)
        {
            if (reg->enter_play_func)
                g_play_fsm[num].enter_play_func = reg->enter_play_func;
            if (reg->enter_stop_func)
                g_play_fsm[num].enter_stop_func = reg->enter_stop_func;
            if (reg->enter_keep_func)
                g_play_fsm[num].enter_keep_func = reg->enter_keep_func;
            break;
        }
    }
}
// 停止当前在播的通道
void ltplay_stop()
{
    QL_PLAY_LOG("ltplay_stop!!");
    uint16_t num = 0;
    if (ltplay_get_src() == SND_STOP)
        return;
    for (num = 0; num < sizeof(g_play_fsm) / sizeof(g_play_fsm[0]); num++)
    {
        if (ltplay_get_src() == g_play_fsm[num].src)
        {

            if (g_play_fsm[num].enter_stop_func)
            {
                g_play_fsm[num].enter_stop_func();
            }

            break;
        }
    }
}
// 判断是否为切换通道

void ltplay_check_play(char src)
{
    play_mutex_lock(&g_play_sta.mutex);
    QL_PLAY_LOG("src==%02x", src);
    if (SND_STOP == src)
    {
    }
    else if (SND_STOP != ltplay_get_src() && src != ltplay_get_src())
    {
        ltplay_stop();
    }
    else
    {
    }
    ltplay_set_src(src);
    play_mutex_unlock(&g_play_sta.mutex);
}

// ltplay add keepplay
void ltplay_thread(void *param)
{
    int num = 0;
    static int padown_index = 0;
    while (1)
    {
        if (SND_STOP != ltplay_get_src())
        {
            padown_index = 0;
            for (num = 0; num < sizeof(g_play_fsm) / sizeof(g_play_fsm[0]); num++)
            {
                if (ltplay_get_src() == g_play_fsm[num].src)
                {
                    if (g_play_fsm[num].enter_keep_func)
                        g_play_fsm[num].enter_keep_func();

                    break;
                }
            }
            if (ltplay_get_src() == SND_FM_URL || ltplay_get_src() == SND_FM || ltplay_get_src() == SND_MP3 || ltplay_get_src() == SND_MP3_URL)
            {
                if (lt_adc_battery_get() < 30 && lt_get_usb_charge_status() == false)
                {
                    ltplay_stop();
                    QL_PLAY_LOG("battery low \n");
                    ltapi_play_tts(TTS_STR_BATTERY_LOW, strlen(TTS_STR_BATTERY_LOW));
                }
            }
        }
        else
        {
            padown_index++;
        }

        ql_rtos_task_sleep_ms(3000);

#if 0 // 打印内存堆栈信息，在需要的时候可放开测试
        ql_errcode_dev_e Temp;
        ql_memory_heap_state_t Stack_Test;
        Temp = ql_dev_memory_size_query(&Stack_Test);
        if (Temp == QL_DEV_SUCCESS)
        {
            QL_PLAY_LOG(" Stack_Test.avail_size: %d", Stack_Test.avail_size);
            QL_PLAY_LOG(" Stack_Test.total_size: %d", Stack_Test.total_size);
            QL_PLAY_LOG(" Stack_Test.max_block_size: %d", Stack_Test.max_block_size);
        }
#endif
    }
}

void ltplay_init(void)
{
    int err = QL_OSI_SUCCESS;
    ql_task_t ltplay_task = NULL;

    err = ql_rtos_task_create(&ltplay_task, QL_LTPLAY_TASK_STACK_SIZE, QL_LTPLAY_TASK_PRIO, "lt_play", ltplay_thread, NULL, QL_LTPLAY_TASK_EVENT_CNT);

    if (err != QL_OSI_SUCCESS)
    {
        QL_PLAY_LOG(" lt_play task created failed");
    }
}