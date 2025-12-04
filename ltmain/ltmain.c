/*
 * @Description: 
 * @Author: 
 * @Date: 2025-11-26 21:51:27
 * @LastModifiedBy: 
 * @LastEditTime: 2025-12-01 19:01:17
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ql_api_osi.h"
#include "ltmain.h"
#include "ltkey.h"
#include "ltmp3.h"
#include "ltaudio.h"
#include "ltaudiourl.h"
#include "ltsdmmc.h"
#include "ltfm.h"
#include "ltfmurl.h"
#include "lt_ebs.h"
#include "lt_datacall.h"
#include "ltrecord.h"
#include "ltplay.h"
#include "ltpower.h"
#include "ltmqtt.h"
#include "led.h"
#include "ltadc.h"
#include "lt_http_demo.h"
#include "lt_tts_demo.h"
#include "ltsystem.h"
#include "ltsms.h"
#include "ltuart2frx8016.h"
#include "ltvoicecall.h"
#include "lsm6ds3tr.h"
#include "volume_knob.h"
#include "dynamic_config.h"

int lt_main_app_init(void)
{
    ql_entry_dyn();   //新增：解析配置文件
    
    lt_system_init();
    
    lt_uart2frx8016_init();
    ql_i2c_led_init();

    ql_key_app_init();
    lt_power_app_init();

    lt_mqtt_app_init();


    // lt_adc_init();
    lt_volume_knob_init();

    lt_sdmmc_app_init();
    lt_datacall_app_init();
    lt_ebs_app_init();


    lt_audio_app_init();
    lt_audiourl_app_init();

    lt_record_app_init();
    lt_fm_app_init();
    lt_fmurl_app_init();
    ltplay_init();

    lt_http_app_init();

    lt_sms_init();
    lt_voice_call_init();
    ql_rtos_task_sleep_ms(200);  
    lt_tts_demo_init();

    ql_lsm6ds3tr_init();

    

    //ql_rtos_sleep_ms(1000);


   // lt_tts_demo_init();
    return 0;
}