/*
 * @Description: 
 * @Author: 
 * @Date: 2025-11-18 00:46:31
 * @LastModifiedBy: 
 * @LastEditTime: 2025-11-20 00:18:05
 */
#ifndef __LSM6DS3TR_H__
#define __LSM6DS3TR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
// 引入官方驱动的头文件，提供所有寄存器地址宏和结构体
#include "lsm6ds3tr_c_reg.h" 

// LSM6DS3TR I2C 地址 (SA0接地: 0x6A, SA0接VDD: 0x6B)
#define LSM6DS3TR_I2C_ADDR (0x6A)
// WHO_AM_I 寄存器的期望值
#define LSM6DS3TR_WHO_AM_I_VALUE    LSM6DS3TR_C_ID

// 存储传感器原始数据的结构体 (保持不变)
typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temperature;
} lsm6ds3tr_data_t;


// ====================================================================
// ===== 外部函数声明 (基于官方驱动函数)                            =====
// ====================================================================

/**
 * @brief 初始化LSM6DS3TR传感器，配置I2C上下文和默认工作模式。
 * @return int 0: 成功, -1: 失败
 */
int lsm6ds3tr_sensor_init(void);

/**
 * @brief 读取传感器的原始数据 (温度、陀螺仪、加速度计)。
 * @param sensor_data 指向 lsm6ds3tr_data_t 结构体的指针。
 * @return int 0: 成功, -1: 失败
 */
int lsm6ds3tr_read_raw_data(lsm6ds3tr_data_t* sensor_data);

/**
 * @brief 将加速度计原始数据转换为实际值 (单位: mg)。
 * 注: 依赖初始化时设置的满量程。
 * @param raw_value 原始数据
 * @return float 转换后的加速度值
 */
float lsm6ds3tr_convert_accel_value(int16_t raw_value);

/**
 * @brief 将陀螺仪原始数据转换为实际值 (单位: mdps)。
 * 注: 依赖初始化时设置的满量程。
 * @param raw_value 原始数据
 * @return float 转换后的角速度值
 */
float lsm6ds3tr_convert_gyro_value(int16_t raw_value);

/**
 * @brief 启用倾斜检测并将中断路由到 INT2 引脚 (使用官方驱动函数)。
 * @param ctx 传感器上下文
 * @return int 0: 成功, -1: 失败
 */
int lsm6ds3tr_enable_tilt_int_on_int2(void);

/**
 * @brief 创建并启动LSM6DS3TR传感器读取线程
 */
void ql_lsm6ds3tr_init(void);

void lt_fall_call_pub(int state);
uint8_t get_fall_call_flag();

#ifdef __cplusplus
}
#endif

#endif // __LSM6DS3TR_H__