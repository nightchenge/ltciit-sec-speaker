/*
 * @file ltmain.c
 * @brief 整合了 ltplcgw 框架与 ltciit-sec-speaker 业务逻辑
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ql_api_osi.h"
#include "ql_log.h"

// 引入框架头文件
#include "ltmain.h"

// 引入原有业务头文件
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

#define LOG_TAG "BOOT"

/* ==========================================================================
   模块注册表 (g_module_infos)
   --------------------------------------------------------------------------
   将原有 ltmain.c 中的初始化函数按 Stage 填入表中
   注意：stack_size 这里填的是参考值，实际任务堆栈大小仍由各模块内部 init 决定
   ========================================================================== */
static const app_mgmt_info_t g_module_infos[] = 
{
    /* --- STAGE 0: 基础系统 --- */
    // 动态配置必须最先加载
    {APP_STAGE_0, "dyn_config",   0,         (app_mgmt_init_t *)ql_entry_dyn,         NULL, NULL}, 
    {APP_STAGE_0, "system_core",  4 * 1024,  (app_mgmt_init_t *)lt_system_init,       NULL, NULL},
    {APP_STAGE_0, "power_mgr",    4 * 1024,  (app_mgmt_init_t *)lt_power_app_init,    NULL, NULL},
    {APP_STAGE_0, "i2c_led",      0,         (app_mgmt_init_t *)ql_i2c_led_init,      NULL, NULL},

    /* --- STAGE 1: 本地驱动 & 音频 (Audio Ready) --- */
    {APP_STAGE_1, "uart_frx",     4 * 1024,  (app_mgmt_init_t *)lt_uart2frx8016_init, NULL, NULL},
    {APP_STAGE_1, "volume_knob",  0,         (app_mgmt_init_t *)lt_volume_knob_init,  NULL, NULL},

    {APP_STAGE_1, "sd_card",      4 * 1024,  (app_mgmt_init_t *)lt_sdmmc_app_init,    NULL, NULL},
    
    // 音频子系统 (Audio 必须在 EBS 之前)
    {APP_STAGE_1, "audio_core",   8 * 1024,  (app_mgmt_init_t *)lt_audio_app_init,    NULL, NULL},
    {APP_STAGE_1, "audio_url",    8 * 1024,  (app_mgmt_init_t *)lt_audiourl_app_init, NULL, NULL},
    {APP_STAGE_1, "audio_play",   4 * 1024,  (app_mgmt_init_t *)ltplay_init,          NULL, NULL},
    {APP_STAGE_1, "record",       4 * 1024,  (app_mgmt_init_t *)lt_record_app_init,   NULL, NULL},
    {APP_STAGE_1, "fm_radio",     4 * 1024,  (app_mgmt_init_t *)lt_fm_app_init,       NULL, NULL},
    {APP_STAGE_1, "fm_url",       0,         (app_mgmt_init_t *)lt_fmurl_app_init,    NULL, NULL},

    /* --- STAGE 2: 网络服务 --- */
    // 拨号成功后才能跑 MQTT
    {APP_STAGE_2, "datacall",     10 * 1024, (app_mgmt_init_t *)lt_datacall_app_init, NULL, NULL},

    /* --- STAGE 3: 核心业务 --- */
    {APP_STAGE_3, "keypad",       4 * 1024,  (app_mgmt_init_t *)ql_key_app_init,      NULL, NULL},
    // 应急广播 (EBS)
    {APP_STAGE_3, "ebs_service",  20 * 1024, (app_mgmt_init_t *)lt_ebs_app_init,     NULL, NULL},
    // MQTT 云连接
    {APP_STAGE_3, "mqtt_client",  20 * 1024, (app_mgmt_init_t *)lt_mqtt_app_init,    NULL, NULL},
    // 短信 & 通话
    {APP_STAGE_3, "sms_service",  4 * 1024,  (app_mgmt_init_t *)lt_sms_init,          NULL, NULL},
    {APP_STAGE_3, "voice_call",   4 * 1024,  (app_mgmt_init_t *)lt_voice_call_init,   NULL, NULL},
    {APP_STAGE_3, "http_svc",     8 * 1024,  (app_mgmt_init_t *)lt_http_app_init,     NULL, NULL},

    /* --- STAGE 4: 应用/Demo --- */
    {APP_STAGE_4, "tts_demo",     4 * 1024,  (app_mgmt_init_t *)lt_tts_demo_init,     NULL, NULL},
    {APP_STAGE_4, "imu_sensor",   0,         (app_mgmt_init_t *)ql_lsm6ds3tr_init,    NULL, NULL},
};

#define MODULE_CNT (sizeof(g_module_infos) / sizeof(g_module_infos[0]))

/* ==========================================================================
   启动逻辑实现 (参考 ltplcgw_main.c)
   ========================================================================== */
int lt_main_app_init(void)
{
    int32_t i;
    int32_t ret = 0;
    app_mgmt_stage_t stage;
    const app_mgmt_info_t *info;

    QL_LOG(QL_LOG_LEVEL_INFO, LOG_TAG, "System Booting (Framework Mode)...");

    // 按阶段遍历：STAGE 0 -> STAGE MAX
    for (stage = APP_STAGE_MIN; stage < APP_STAGE_MAX; stage++)
    {
        QL_LOG(QL_LOG_LEVEL_INFO, LOG_TAG, ">>> Entering STAGE %d", stage);

        // 遍历模块表
        for (i = 0; i < MODULE_CNT; i++)
        {
            info = &g_module_infos[i];

            // 只执行当前 Stage 的模块
            if (info->stage == stage && info->init != NULL)
            {
                QL_LOG(QL_LOG_LEVEL_INFO, LOG_TAG, "Init Module: %s", info->name);
                
                // 执行初始化 (传入 stage 参数，虽然旧接口会忽略它)
                ret = info->init(stage);

                if (ret != 0)
                {
                    QL_LOG(QL_LOG_LEVEL_ERROR, LOG_TAG, "Module %s Init Failed: %d", info->name, ret);
                    // 严重错误可在此处 return -1 或重启
                }
            }
        }

        /* 阶段间隙处理：等待资源就绪 */
        if (stage == APP_STAGE_0)
        {
            // 基础系统初始化后稍作延时
            ql_rtos_task_sleep_ms(100); 
        }
        else if (stage == APP_STAGE_2)
        {
            // DataCall 启动后，建议等待网络协议栈稳定，再启动上层 MQTT/EBS
            // 避免 MQTT 立即连接导致失败
            QL_LOG(QL_LOG_LEVEL_INFO, LOG_TAG, "Waiting for Network Stack...");
            ql_rtos_task_sleep_ms(500); 
        }
        else if (stage == APP_STAGE_3)
        {
            // 业务启动后
            ql_rtos_task_sleep_ms(200);
        }
    }

    QL_LOG(QL_LOG_LEVEL_INFO, LOG_TAG, "System Boot Done.");
    return 0;
}