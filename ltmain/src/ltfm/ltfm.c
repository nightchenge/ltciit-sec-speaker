#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ql_i2c.h"
#include "ql_api_osi.h"
#include "ql_gpio.h"
#include "ql_log.h"

#include "ltkey.h"
#include "fm8035.h"
#include "ltfm.h"
#include "ltaudio.h"

#include "ltplay.h"
#include "led.h"
#include "ltuart2frx8016.h"
#include "ltsystem.h"
static fm_q_info_t ltfm_info = {0, 30, FALSE}; // 保存的当前fm_info 可扩展

ql_queue_t lt_fm_msg_queue;
typedef struct lt_fm_msg
{
    int type;
    uint32_t datalen;
    uint8_t *data;
} lt_fm_msg_t;

static lt_fm_status_t fm_status = FM_POWR_OFF;

void lt_fm_power_status_set(lt_fm_status_t st)
{
    //   st == FM_POWR_OFF ? ql_gpio_set_level(GPIO_17, LVL_LOW) : ql_gpio_set_level(GPIO_17, LVL_HIGH);
    fm_status = st;
}

lt_fm_status_t lt_fm_power_status_get()
{
    return fm_status;
}

static void ql_fm_play_next()
{
    QL_FM_LOG("fm next");
    lt_fm_setvol(ltfm_info.fm_vol);
    ltfm_info.fm_CH = TRUE; // 开始搜台

    if (ltfm_info.fm_vol != 0)
        QND_RXConfigAudio(QND_CONFIG_MUTE, 0);

    int seek_fre = 0;
    ltfm_info.fm_freq = (ltfm_info.fm_freq <= qnd_CH_START || ltfm_info.fm_freq >= qnd_CH_STOP) ? qnd_CH_START : (ltfm_info.fm_freq); // 循环用，如果当前freq超过STOP就从START重新开始
    seek_fre = QND_RXSeekCH(ltfm_info.fm_freq, qnd_CH_STOP, qnd_CH_STEP, 0, 0);
    ltfm_info.fm_CH = FALSE; // 结束搜台
    if (seek_fre != 0)
    {
        ltfm_info.fm_freq = seek_fre;
        if (SND_FM == ltplay_get_src())
        {
            set_function_state(FM, seek_fre);
        }
        QL_FM_LOG("fm_next seek_fre == %d", seek_fre);
    }
}

static void ql_fm_play_pre()
{

    QL_FM_LOG("fm pre");
    lt_fm_setvol(ltfm_info.fm_vol);
    ltfm_info.fm_CH = TRUE;
    if (ltfm_info.fm_vol != 0)
        QND_RXConfigAudio(QND_CONFIG_MUTE, 0);

    int seek_fre = 0;
    ltfm_info.fm_freq = (ltfm_info.fm_freq <= qnd_CH_START || ltfm_info.fm_freq > qnd_CH_STOP) ? qnd_CH_START : (ltfm_info.fm_freq); // 循环用，如果当前freq超过STOP就从START重新开始
    seek_fre = QND_RXSeekCH(ltfm_info.fm_freq, qnd_CH_START, qnd_CH_STEP, 0, 0);
    ltfm_info.fm_CH = FALSE;
    if (seek_fre != 0)
    {
        ltfm_info.fm_freq = seek_fre;
        if (SND_FM == ltplay_get_src())
        {
            set_function_state(FM, seek_fre);
        }
        QL_FM_LOG("fm_pre seek_fre == %d", seek_fre);
    }
}

void ltapi_play_fm(int type, char *data, int data_len)
{
	QlOSStatus err = QL_OSI_SUCCESS;
	lt_fm_msg_t msg;

	msg.type = type;
	msg.datalen = data_len;
	msg.data = malloc(msg.datalen);
    if(msg.datalen != 0)
	    memcpy(msg.data, data, msg.datalen);

	if ((err = ql_rtos_queue_release(lt_fm_msg_queue, sizeof(lt_fm_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
	{
		QL_FM_LOG("send msg to lt_fm_msg_t failed, err=%d", err);
	}else
	{
		QL_FM_LOG("send msg to lt_fm_msg_t success");
	}
}
static void ql_key_fm_next()
{
    if (ltplay_get_src() == SND_TEL || ltplay_get_src() == SND_EBS || ltplay_get_src() == SND_SMS)
        return;
    if (SND_FM != ltplay_get_src() && ltfm_info.fm_freq != 0)
    {
        // QND_TuneToCH(fre_s);
        ltfm_info.fm_freq = ltfm_info.fm_freq - 10;
    }
    ltplay_check_play(SND_FM);
    if (FM_POWR_OFF == lt_fm_power_status_get())
    {
        // i2c操作前开启电源。
        // lt_fm_power_on();
        lt_system_msg_handle(LT_SYS_FM_POWER_ON);
        lt_fm_power_status_set(FM_POWR_ON);

        ql_rtos_task_sleep_ms(100);
        // 初始化FM芯片
        i2c_1_Initialize();
        QND_Init();
    }

 //   ql_fm_play_next();
    char *fm_start = "fm_start";
    ltapi_play_fm(0,fm_start, strlen(fm_start) + 1);
}

static void ql_key_fm_pre()
{
    if (ltplay_get_src() == SND_TEL || ltplay_get_src() == SND_EBS)
        return;
    if (SND_FM != ltplay_get_src() && ltfm_info.fm_freq != 0)
    {
        // QND_TuneToCH(fre_s);
        ltfm_info.fm_freq = ltfm_info.fm_freq - 10;
    }
    ltplay_check_play(SND_FM);
    if (FM_POWR_OFF == lt_fm_power_status_get())
    {
        // i2c操作前开启电源。
        // lt_fm_power_on();
        lt_system_msg_handle(LT_SYS_FM_POWER_ON);
        lt_fm_power_status_set(FM_POWR_ON);

        ql_rtos_task_sleep_ms(100);
        // 初始化FM芯片
        i2c_1_Initialize();
        QND_Init();
    }

    ql_fm_play_pre();
    //     char *fm_pre = "fm_pre";
    //     ltapi_play_fm(1,fm_pre, strlen(fm_pre) + 1);
}
static void ql_key_fm_keep()
{
    if (!ltfm_info.fm_CH) // 如果当前在搜台则不处理通道切换
    {
         if( ltfm_info.fm_vol != 0) 
            QND_RXConfigAudio(QND_CONFIG_MUTE, 0);
    }
    //  QND_RXConfigAudio(QND_CONFIG_MUTE, 0);
}
static void ql_key_fm_stop()
{
    if (SND_FM != ltplay_get_src())
    {
        QL_FM_LOG("fm not in play;exit");
        return;
    }
    QL_FM_LOG("fm long_press");
    /// lt_audio_pa_disable();
    QND_RXConfigAudio(QND_CONFIG_MUTE, 1);
    ltplay_check_play(SND_STOP);
    set_function_state(RT_TIME, 0);
    set_blink("fm", 0);
    // 操作完成后关闭电源。
    // lt_fm_power_off();
    lt_system_msg_handle(LT_SYS_FM_POWER_OFF);
    // lt_fm_power_off();
    lt_fm_power_status_set(FM_POWR_OFF);
    
}

// static void ql_key_fm_click()
// {
//     if (ltplay_get_src() == SND_TEL || ltplay_get_src() == SND_EBS || ltplay_get_src() == SND_SMS)
//         return;
//     if(SND_FM == ltplay_get_src() )
//     {
//         ql_key_fm_stop();
//         return ;
//     }
//     if (SND_FM != ltplay_get_src() && ltfm_info.fm_freq != 0)
//     {
//         // QND_TuneToCH(fre_s);
//         if(ltfm_info.fm_freq != qnd_CH_STOP)
//         {
//             ltfm_info.fm_freq = ltfm_info.fm_freq - 10;
//         }

//     }
//     ltplay_check_play(SND_FM);
//     char *fm_start = "fm_start";
//     ltapi_play_fm(0,fm_start, strlen(fm_start) + 1);
// }
void lt_fm_key_callback_register()
{
    lt_key_callback_t reg;
    // #if PACK_TYPE == PACK_LOOTOM
    //     reg.key_gpio = KEY1;
    // #elif PACK_TYPE == PACK_NANJING
    //     reg.key_gpio = KEY4;
    // #else
    //     reg.key_gpio = KEY3;
    // #endif

    //  reg.lt_key_click_handle = ql_key_fm_click;
    reg.lt_key_click_handle = ql_key_fm_next;
    reg.lt_key_doubleclick_handle = ql_key_fm_stop; // ql_key_fm_set_vol;
    reg.lt_key_longclick_handle = ql_key_fm_stop;
    reg.lt_key_repeat_callback = NULL;
    lt_key_callback_register("key_fm", &reg);
} // 回调参考上溯例子

void lt_fm_play_callback_register()
{

    play_fsm_t fm_play_fsm = {
        .src = SND_FM,
        .enter_play_func = ql_key_fm_next,
        .enter_stop_func = ql_key_fm_stop,
        .enter_keep_func = ql_key_fm_keep,
    };
    ltplay_callback_register(&fm_play_fsm);
} // 回调参考上溯例子

void lt_fm_asr_callback_register()
{
    asr_type_state_t reg;

    reg.function = ASR_AUDIO;
    reg.state = ASR_FM;
    reg.start_function = ql_key_fm_next;
    reg.stop_function = ql_key_fm_stop;
    reg.next_function = ql_key_fm_next;
    reg.last_function = ql_key_fm_pre;

    ltasr_callback_register(&reg);
} // 回调参考上溯例子

void ql_i2c_fm_thread(void *param)
{
    ql_rtos_task_sleep_s(5);

    ql_rtos_queue_create(&lt_fm_msg_queue, sizeof(lt_fm_msg_t), 5);
    lt_fm_key_callback_register();  // 注册事件回调
    lt_fm_play_callback_register(); // 注册事件回调
    lt_fm_asr_callback_register();  // 注册事件回调

    while (1)
    {
       // ql_rtos_task_sleep_s(5);
        lt_fm_msg_t msg;
        QL_FM_LOG("fm ql_rtos_queue_wait");
        ql_rtos_queue_wait(lt_fm_msg_queue, (uint8 *)(&msg), sizeof(lt_fm_msg_t), QL_WAIT_FOREVER);
        QL_FM_LOG("fm receive msg.");

        switch (msg.type)
        {
        case 0:
            if(FM_POWR_ON == fm_status)
            {
                ql_fm_play_next();
            }
        	break;
        case 1:
            if(FM_POWR_ON == fm_status)
            {
                ql_fm_play_pre();
            }
            break;
        default:
        	break;
        }
        do
        {
        	if (msg.data)
        	{
        		free((void *)msg.data);
        		msg.data = NULL;
        	}
        } while (0);
    }
}
// 设置FM音量
void lt_fm_setvol(int vol)
{
    vol = (vol <= 0) ? 0 : (vol > 47) ? 47
                                      : vol;
                                      
    QND_RXConfigAudio(QND_CONFIG_VOLUME, vol);
    if (SND_FM == ltplay_get_src() && !ltfm_info.fm_CH )
    {
        if (vol == 0)
        {
            QND_RXConfigAudio(QND_CONFIG_MUTE, 1);
        }
        else
        {
            QND_RXConfigAudio(QND_CONFIG_MUTE, 0);
        }
    }

    ltfm_info.fm_vol = vol;
}

void lt_fm_app_init(void)
{
    QlI2CStatus err = QL_OSI_SUCCESS;
    ql_task_t fm_task = NULL;
    // lt_fm_power_off();
    lt_system_msg_handle(LT_SYS_FM_POWER_OFF);
    lt_fm_power_status_set(FM_POWR_OFF);

    err = ql_rtos_task_create(&fm_task, QL_FM_TASK_STACK_SIZE, QL_FM_TASK_PRIO, "lt_fm", ql_i2c_fm_thread, NULL, QL_FM_TASK_EVENT_CNT);
    if (err != QL_OSI_SUCCESS)
    {
        QL_FM_LOG("i2ctest1 fm task created failed");
    }
}