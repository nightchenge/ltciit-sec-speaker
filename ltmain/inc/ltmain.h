/*
 * @file ltmain.h
 * @brief 基于 ltplcgw 框架的重构头文件
 */
#ifndef LTMAIN_H
#define LTMAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ql_api_osi.h"

// 调试开关
#define LT_LVGL_DEMO_DEBUG 0 

// 1. 定义启动阶段 (Stage)
typedef enum 
{
    APP_STAGE_MIN, 
    APP_STAGE_0 = APP_STAGE_MIN,  // 基础系统/配置
    APP_STAGE_1,                  // 本地驱动/外设
    APP_STAGE_2,                  // 网络连接
    APP_STAGE_3,                  // 核心业务
    APP_STAGE_4,                  // 上层应用/Demo
    APP_STAGE_MAX, 
} app_mgmt_stage_t;

// 2. 定义标准接口
// 注意：原 ltplcgw 定义 init 函数需接受 stage 参数
typedef int32_t app_mgmt_init_t(uint32_t stage);
typedef void app_mgmt_func_t(void);
typedef void app_mgmt_thread_t(void * arg);

// 3. 模块信息结构体
typedef struct 
{
    uint8_t             stage;
    char                name[32];
    uint32_t            stack_size;  // 仅作记录或供 init 内部使用
    app_mgmt_init_t * init;        // 初始化函数
    app_mgmt_thread_t * entry;       // 任务入口 (部分模块可能不需要)
    app_mgmt_func_t * finish;      // 结束/清理函数
} app_mgmt_info_t;

// 主入口
int lt_main_app_init(void);

#ifdef __cplusplus
}
#endif

#endif /* LTMAIN_H */