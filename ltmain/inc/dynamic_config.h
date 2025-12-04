/*
 * @Description: 
 * @Author: 
 * @Date: 2025-11-26 21:32:43
 * @LastModifiedBy: 
 * @LastEditTime: 2025-12-01 19:00:04
 */
/*
 * dynamic_config.h
 * 动态配置加载模块头文件
 */

#ifndef _DYNAMIC_CONFIG_H_
#define _DYNAMIC_CONFIG_H_

#include <stdbool.h>


// 新增：版本信息
#define TYPE_SICHUAN 2
#define TYPE_QIXIA 1
#define TYPE_GENERIC 0

int get_type_func(void);

/**
 * @brief 初始化配置模块
 * 读取 UFS:define.json 并解析所有开关到内存链表中
 */
void DynConfig_Init(void);

/**
 * @brief 查询某个功能开关是否开启
 * * @param key_name 配置文件中的 Key 名称 (例如 "ENABLE_GPS")
 * @return true  - 开关存在且为 true/1
 * @return false - 开关不存在 或 为 false/0
 */
bool DynConfig_IsEnabled(const char *key_name);

/**
 * @brief (调试用) 打印当前所有已加载的配置项
 */
void DynConfig_Dump(void);
void ql_entry_dyn(void);
#endif