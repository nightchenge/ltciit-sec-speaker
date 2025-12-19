/*
 * @Author: your name
 * @Date: 2023-09-07 11:16:24
 * @LastEditTime: 2025-12-05 16:49:38
 * @LastEditors: ubuntu
 * @Description: In User Settings Edit
 * @FilePath: /LTE01R02A01_BETA0726_C_SDK_G/components/ql-application/ltmain/src/ltkey/ltadc.c
 */
#include "ltadc.h"

/*================================================================
  Copyright (c) 2021, Quectel Wireless Solutions Co., Ltd. All rights reserved.
  Quectel Wireless Solutions Proprietary and Confidential.
=================================================================*/

/*=================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

WHEN			  WHO		  WHAT, WHERE, WHY
------------	 -------	 -------------------------------------------------------------------------------

=================================================================*/

/*===========================================================================
 * include files
 ===========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "osi_api.h"
#include "ql_log.h"
#include "ql_adc.h"
#include "adc_demo.h"
#include "ql_api_osi.h"
#include "ql_audio.h"

#include "ql_api_nw.h"
//#include "audio_demo.h"
#include "ltfm.h"
#include "led.h"
#include "ltmqtt.h"
#include "ip_gb_comm.h"
#include "ql_api_sim.h"
#include "ltsystem.h"
#include "ltkey.h"
#include "ltuart2frx8016.h"
/*===========================================================================
 *Definition
 ===========================================================================*/
#define QL_ADCDEMO_LOG_LEVEL QL_LOG_LEVEL_INFO
#define QL_ADCDEMO_LOG(msg, ...) QL_LOG(QL_ADCDEMO_LOG_LEVEL, "lt_adc", msg, ##__VA_ARGS__)
#define QL_ADCDEMO_LOG_PUSH(msg, ...) QL_LOG_PUSH("lt_adc_demo", msg, ##__VA_ARGS__)

#define QL_ADC_TASK_STACK_SIZE 1024
#define QL_ADC_TASK_PRIO APP_PRIORITY_NORMAL
#define QL_ADC_TASK_EVENT_CNT 5

static int old_value = 0;

void lt_set_volume_up()
{
    int vol_value = ql_get_volume();
    if (vol_value >= 11)
        return;
    else
    {
        vol_value++;
        ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_VOICE, vol_value);
        ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_LOCAL, vol_value);
        int fm_vol = vol_value * 4;
        QL_ADCDEMO_LOG("ADC[1]: fm_vol=[%d]", fm_vol);
        lt_fm_setvol(fm_vol);
    }
    return;
}

void lt_set_volume_down()
{
    int vol_value = ql_get_volume();
    if (vol_value <= 0)
        return;
    else
    {
        vol_value--;
        ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_VOICE, vol_value);
        ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_LOCAL, vol_value);
        int fm_vol = vol_value * 4;
        QL_ADCDEMO_LOG("ADC[1]: fm_vol=[%d]", fm_vol);
        lt_fm_setvol(fm_vol);
    }
    return;
}


ql_errcode_adc_e lt_adc_test(void)
{
    int adc_value = 0;
    int vol_value = 0;
    int fm_vol = 0;
    static int asr_vol = 800;
#if PACK_TYPE == PACK_LOOTOM
    adc_value = 1328;
#else
    ql_adc_get_volt(QL_ADC1_CHANNEL, &adc_value); // 512-1328
#endif
    if (adc_value <= 0)
    {
        return 0;
    }
    if(0 != lt_ltasr_wakeup())
    {
        if (abs(asr_vol - old_value) >= 10)
        {
            float vl = 0.01195 * asr_vol - 6.2;
            vol_value = (vl < AUDIOHAL_SPK_VOL_1) ? AUDIOHAL_SPK_MUTE : (vl > AUDIOHAL_SPK_VOL_11 ? AUDIOHAL_SPK_VOL_11 : vl);

            if (ql_get_volume() != vol_value)
            {
                QL_ADCDEMO_LOG("ql_set_volume: vol_value=[%d];adc_value=%d;%f", vol_value, asr_vol, vl);
                // ql_set_volume(vol_value);
                ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_VOICE, vol_value);
                ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_LOCAL, vol_value);
            }

            fm_vol = 0.06 * asr_vol - 30;
            QL_ADCDEMO_LOG("ADC[1]: fm_vol=[%d]", fm_vol);
            lt_fm_setvol(fm_vol);

            old_value = asr_vol;
        }
    }
    else
    {
        if (abs(adc_value - old_value) >= 10)
        {
            float vl = 0.01195 * adc_value - 6.2;
            vol_value = (vl < AUDIOHAL_SPK_VOL_1) ? AUDIOHAL_SPK_MUTE : (vl > AUDIOHAL_SPK_VOL_11 ? AUDIOHAL_SPK_VOL_11 : vl);

            if (ql_get_volume() != vol_value)
            {
                QL_ADCDEMO_LOG("ql_set_volume: vol_value=[%d];adc_value=%d;%f", vol_value, adc_value, vl);
                // ql_set_volume(vol_value);
                ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_VOICE, vol_value);
                ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_LOCAL, vol_value);
            }

            fm_vol = 0.06 * adc_value - 30;
            QL_ADCDEMO_LOG("ADC[1]: fm_vol=[%d]", fm_vol);
            lt_fm_setvol(fm_vol);

            old_value = adc_value;
        }
    }


    return 0;
}
// static void lt_uart_send_adc(int value)
// {
//     char buf[8]={0xa9,0xa9,0x01,0x01};
//     memcpy(&buf[4],&value,4);

//     lt_uart2frx8016_send(buf,8);
// }
int lt_adc_battery_get(void)
{
    int adc_value = 0;
    //int battery_value = 0;
    int battery_per =100;
    static int battery_per_lp = 100;
    ql_adc_get_volt(QL_ADC0_CHANNEL, &adc_value); // 512-1328
    QL_ADCDEMO_LOG("battery_adcvalue=[%d]", adc_value); 
    if(adc_value >= 1883)
        battery_per = 100;
    else if (adc_value >= 1849)  
        battery_per = 95;
    else if (adc_value >= 1838)
        battery_per = 90;
    else if (adc_value >= 1822)
        battery_per = 80;
    else if (adc_value >= 1803)      
        battery_per = 70;
    else if (adc_value >= 1766)
        battery_per = 60;
    else if (adc_value >= 1740)
        battery_per = 50;
    else if (adc_value >= 1707)
        battery_per = 40;
    else if (adc_value >= 1656)
        battery_per = 30;
    else if (adc_value >= 1613)  
        battery_per = 20;
    else if (adc_value >= 1559)
        battery_per = 10;
    else if (adc_value >= 1535)
        battery_per = 5;
    else
        battery_per = 0;

    QL_ADCDEMO_LOG("battery_value=[%d]", battery_per);
    if(battery_per == 0)
        return battery_per_lp;
        
    battery_per_lp =  battery_per;
   
    return battery_per;
}

static void lt_repeat_volup_callback_register()
{
    lt_key_callback_t reg;

    reg.lt_key_click_handle = NULL;
    reg.lt_key_doubleclick_handle = NULL;
    reg.lt_key_longclick_handle = NULL;
    // reg.key_gpio = KEY2;
    reg.lt_key_repeat_callback = lt_set_volume_up;
    lt_key_callback_register("key_adc_volup",&reg);
} // 回调参考上溯例子

static void lt_repeat_voldown_callback_register()
{
    lt_key_callback_t reg;
    reg.lt_key_click_handle = NULL;
    reg.lt_key_doubleclick_handle = NULL; // ql_key_fm_set_vol;
    reg.lt_key_longclick_handle = NULL;
    // reg.key_gpio = KEY2;
    reg.key_gpio = KEY3;
    reg.lt_key_repeat_callback = lt_set_volume_down;
    lt_key_callback_register("key_adc_voldown", &reg);
} // 回调参考上溯例子

static void lt_adc_thread(void *param)
{
    ql_rtos_task_sleep_s(10);
    lt_repeat_volup_callback_register();//增加音量
    lt_repeat_voldown_callback_register();//减少音量
    while (1)
    {
        if (IDLE_MODE == ltget_lp_status()) // 如果按键的第一下屏幕是灭的话 直接先唤醒
        {
         //   lt_adc_battery_get();
            lt_adc_test();
            ql_rtos_task_sleep_ms(200);
        }
        else
        {
            ql_rtos_task_sleep_ms(2000);
        }
    }
}

void lt_adc_init(void)
{
    QlADCStatus err = QL_ADC_SUCCESS;
    ql_task_t adc_task = NULL;

    err = ql_rtos_task_create(&adc_task, QL_ADC_TASK_STACK_SIZE * 10, QL_ADC_TASK_PRIO, "lt_adc", lt_adc_thread, NULL, QL_ADC_TASK_EVENT_CNT);
    if (err != QL_ADC_SUCCESS)
    {
        QL_ADCDEMO_LOG("lt_adc task created failed");
    }
}
