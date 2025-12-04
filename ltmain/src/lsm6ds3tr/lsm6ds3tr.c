#include "lsm6ds3tr.h"
#include "ql_api_osi.h"
#include "ql_log.h"
#include "ql_i2c.h"
#include "ql_gpio.h" // 引入 GPIO 和中断相关的头文件
#include <string.h> 
#include "lt_tts_demo.h"
// 引入官方驱动的C文件，以获取所有 lsm6ds3tr_c_... 函数的实现
#include "lsm6ds3tr_c_reg.c"
#include "ltmqtt.h"
#include "ltkey.h"
#include "ltplay.h"
#include "ltsystem.h"
#include "ltvoicecall.h"

ql_timer_t lt_lsm_temp_key_timer = NULL;     // 定义按键超时定时器
static uint8_t key_pressed = 0;              // 按键按下标志

// 在文件开头添加全局变量
uint8_t fall_call_test_flag = 0;
// static ql_timer_t lt_FALL_TEST = NULL;

uint8_t get_fall_call_flag()
{
    return fall_call_test_flag;
}

// ----------- 日志定义 -----------
#define QL_APP_LSM_LOG_LEVEL   QL_LOG_LEVEL_INFO
#define QL_APP_LSM_LOG(msg, ...) QL_LOG(QL_APP_LSM_LOG_LEVEL, "lsm6ds3tr", msg, ##__VA_ARGS__)

// ----------- 任务定义 -----------
#define QL_LSM_TASK_STACK_SIZE     (1024 * 4)
#define QL_LSM_TASK_PRIO           APP_PRIORITY_NORMAL
#define QL_LSM_TASK_EVENT_CNT      5

// ----------- I2C 定义 -----------
#define LSM_I2C_BUS i2c_2 

// INT2 中断引脚定义 (假设这是连接到 LSM6DS3TR INT2 的 GPIO)
#define LSM_INT_GPIO_NUM        GPIO_2
#define LSM_INT_PIN_NUM         16 // 对应引脚功能设置


// 假设的事件ID，用于通知主线程跌倒发生 (请在全局头文件中定义)
#define LT_SYS_FALL_DETECTED    (0x1000) 
#define LT_SYS_FALL_CHECK_KEY    (0x2000)    //按键判断
#define LT_SYS_FALL_CHECK_CALL    (0x3000)    //拨打电话判断

// 默认满量程 (用于原始数据转换)
#define DEFAULT_ACCEL_FS  LSM6DS3TR_C_2g
#define DEFAULT_GYRO_FS   LSM6DS3TR_C_250dps


// ====================================================================
// ===== 全局变量和接口封装                                       =====
// ====================================================================

// 主线程任务句柄 (用于接收中断消息)
ql_task_t lsm_task = NULL;
// EC800G I2C 写操作实现
static int32_t platform_write(void *handle, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t i2c_addr = (uintptr_t)handle; 
    return ql_I2cWrite(LSM_I2C_BUS, i2c_addr, reg, buf, len);
}

// EC800G I2C 读操作实现
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint8_t i2c_addr = (uintptr_t)handle;
    return ql_I2cRead(LSM_I2C_BUS, i2c_addr, reg, buf, len);
}

// 官方驱动所需的上下文结构体
static stmdev_ctx_t dev_ctx;


// ====================================================================
// ===== 中断回调函数 (Callback Function)                         =====
// ====================================================================

/**
 * @brief GPIO 中断回调函数 (在中断线程中执行)
 */
static void _gpioint_callback01(void *param)
{
    // 在中断发生时，发送事件位通知主线程 (lsm_task)
    if (lsm_task)
    {
        ql_event_t event = {.id = LT_SYS_FALL_DETECTED};
        ql_rtos_event_send(lsm_task, &event);
        
        // 禁用中断，等待主线程处理并清除传感器内部状态后再重新使能
        ql_int_disable(LSM_INT_GPIO_NUM);
        
        QL_APP_LSM_LOG("########FALL_DETECTED_IRQ_SENT##############");
    }
}


// 1) Dump 所有关键寄存器 (使用官方驱动函数)
void lsm6ds3tr_dump_all_regs_for_debug(void)
{
    uint8_t b;
    platform_read(dev_ctx.handle, LSM6DS3TR_C_CTRL1_XL, &b, 1); QL_APP_LSM_LOG("CTRL1_XL  =0x%02X", b);
    platform_read(dev_ctx.handle, LSM6DS3TR_C_CTRL2_G,  &b, 1); QL_APP_LSM_LOG("CTRL2_G   =0x%02X", b);
    platform_read(dev_ctx.handle, LSM6DS3TR_C_CTRL3_C, &b, 1); QL_APP_LSM_LOG("CTRL3_C   =0x%02X", b);
    platform_read(dev_ctx.handle, LSM6DS3TR_C_INT2_CTRL, &b, 1); QL_APP_LSM_LOG("INT2_CTRL =0x%02X", b);
    platform_read(dev_ctx.handle, LSM6DS3TR_C_MD2_CFG,  &b, 1); QL_APP_LSM_LOG("MD2_CFG   =0x%02X", b);
    platform_read(dev_ctx.handle, LSM6DS3TR_C_WAKE_UP_SRC, &b, 1); QL_APP_LSM_LOG("WAKE_SRC  =0x%02X", b);
    platform_read(dev_ctx.handle, LSM6DS3TR_C_FUNC_SRC1, &b, 1); QL_APP_LSM_LOG("FUNC_SRC1 =0x%02X", b);
}

// ... (lsm6ds3tr_enable_tilt_int_on_int2 函数保持不变) ...

/**
 * @brief 启用自由落体 (Free Fall, FF) 检测并将中断路由到 INT2 引脚
 */
int lsm6ds3tr_enable_fall_detection_on_int2(void)
{
    int32_t ret;
    lsm6ds3tr_c_int2_route_t int2_route;

    // --- 1. 启用嵌入式功能 ---
    ret = lsm6ds3tr_c_func_en_set(&dev_ctx, PROPERTY_ENABLE);
    if (ret != 0) {
        QL_APP_LSM_LOG("Failed to enable func_en. Ret: %d", ret);
        return -1;
    }

    // --- 2. 配置自由落体 (FF) 阈值和持续时间 ---
    // 设置 FF 阈值: 312mg (LSM6DS3TR_C_FF_TSH_312mg = 3)
    ret = lsm6ds3tr_c_ff_threshold_set(&dev_ctx, LSM6DS3TR_C_FF_TSH_312mg);
    if (ret != 0) {
        QL_APP_LSM_LOG("Failed to set FF threshold. Ret: %d", ret);
        return -1;
    }
    
    // 设置 FF 持续时间: 5 个 ODR 周期 
    ret = lsm6ds3tr_c_ff_dur_set(&dev_ctx, 5);
    if (ret != 0) {
        QL_APP_LSM_LOG("Failed to set FF duration. Ret: %d", ret);
        return -1;
    }
    
    // --- 3. 路由 FF 中断到 INT2 ---
    ret = lsm6ds3tr_c_pin_int2_route_get(&dev_ctx, &int2_route);
    if (ret != 0) {
        QL_APP_LSM_LOG("Failed to get INT2 route. Ret: %d", ret);
        return -1;
    }
    
    // 清除所有现有的 MD2_CFG 和 INT2_CTRL 路由设置 (只关注 FF)
    memset(&int2_route, 0, sizeof(lsm6ds3tr_c_int2_route_t));

    // 设置 int2_ff (自由落体中断, MD2_CFG寄存器)
    int2_route.int2_ff = PROPERTY_ENABLE; 

    // 设置 int2_drdy_xl (加速度计数据就绪, INT2_CTRL寄存器) - 作为引脚激活保障
    //int2_route.int2_drdy_xl = PROPERTY_ENABLE; 
    
    ret = lsm6ds3tr_c_pin_int2_route_set(&dev_ctx, int2_route);
    if (ret != 0) {
        QL_APP_LSM_LOG("Failed to set INT2 FF routing. Ret: %d", ret);
        return -1;
    }

    QL_APP_LSM_LOG("Free Fall Detection enabled and routed to INT2 (MD2_CFG=0x10, INT2_CTRL=0x01).");
    return 0;
}


int lsm6ds3tr_sensor_init(void)
{
    // ... (初始化代码保持不变) ...
    uint8_t who_am_i = 0;
    int32_t ret = 0;

    // 初始化上下文结构体
    dev_ctx.write_reg = platform_write;
    dev_ctx.read_reg = platform_read;
    dev_ctx.handle = (void *)(uintptr_t)LSM6DS3TR_I2C_ADDR; 
    
    // 1. 检查设备ID 
    ret = lsm6ds3tr_c_device_id_get(&dev_ctx, &who_am_i);
    if (ret != 0)
    {
        QL_APP_LSM_LOG("Failed to read WHO_AM_I register. Ret: %d", ret);
        return -1;
    }
    if (who_am_i != LSM6DS3TR_WHO_AM_I_VALUE)
    {
        QL_APP_LSM_LOG("WHO_AM_I check failed. Expected: 0x%X, Got: 0x%X", LSM6DS3TR_WHO_AM_I_VALUE, who_am_i);
        return -1;
    }
    
    // 2. 传感器配置: 软件复位
    ret = lsm6ds3tr_c_reset_set(&dev_ctx, PROPERTY_ENABLE);
    ql_rtos_task_sleep_ms(50); 
    
    // 3. 配置陀螺仪和加速度计 ODR/FS
    // 加速度计: ODR=104Hz, FS=±2g
    ret += lsm6ds3tr_c_xl_data_rate_set(&dev_ctx, LSM6DS3TR_C_XL_ODR_104Hz);
    ret += lsm6ds3tr_c_xl_full_scale_set(&dev_ctx, DEFAULT_ACCEL_FS);
    
    // 陀螺仪: ODR=104Hz, FS=±250dps
    ret += lsm6ds3tr_c_gy_data_rate_set(&dev_ctx, LSM6DS3TR_C_GY_ODR_104Hz);
    ret += lsm6ds3tr_c_gy_full_scale_set(&dev_ctx, DEFAULT_GYRO_FS);

    // 4. 配置 BDU (块数据更新)
    ret += lsm6ds3tr_c_block_data_update_set(&dev_ctx, PROPERTY_ENABLE);
    
    if (ret != 0)
    {
        QL_APP_LSM_LOG("Sensor configuration failed. Ret: %d", ret);
        return -1;
    }

    QL_APP_LSM_LOG("LSM6DS3TR sensor initialized successfully.");
    return 0;
}


int lsm6ds3tr_read_raw_data(lsm6ds3tr_data_t* sensor_data)
{
    // ... (读取原始数据函数保持不变) ...
    int16_t temperature_raw;
    int16_t gyro_raw[3];
    int16_t accel_raw[3];
    int32_t ret = 0;

    ret += lsm6ds3tr_c_temperature_raw_get(&dev_ctx, &temperature_raw);
    ret += lsm6ds3tr_c_angular_rate_raw_get(&dev_ctx, gyro_raw);
    ret += lsm6ds3tr_c_acceleration_raw_get(&dev_ctx, accel_raw);

    if (ret != 0) {
        QL_APP_LSM_LOG("Failed to read all raw data. Ret: %d", ret);
        return -1;
    }

    sensor_data->temperature = temperature_raw;
    sensor_data->gyro_x = gyro_raw[0];
    sensor_data->gyro_y = gyro_raw[1];
    sensor_data->gyro_z = gyro_raw[2];
    sensor_data->accel_x = accel_raw[0];
    sensor_data->accel_y = accel_raw[1];
    sensor_data->accel_z = accel_raw[2];

    return 0;
}

float lsm6ds3tr_convert_accel_value(int16_t raw_value)
{
    return lsm6ds3tr_c_from_fs2g_to_mg(raw_value);
}

float lsm6ds3tr_convert_gyro_value(int16_t raw_value)
{
    return lsm6ds3tr_c_from_fs250dps_to_mdps(raw_value);
}

int lsm6ds3tr_read_step_count(uint16_t *step_count)
{
    // ... (读取步数函数保持不变) ...
    uint8_t step_buffer[2];
    int32_t ret = platform_read(dev_ctx.handle, LSM6DS3TR_C_STEP_COUNTER_L, step_buffer, 2);

    if (ret != 0)
    {
        QL_APP_LSM_LOG("Failed to read step count registers. Ret: %d", ret);
        *step_count = 0;
        return -1;
    }

    *step_count = (uint16_t)((step_buffer[1] << 8) | step_buffer[0]);
    
    return 0;
}

// ====================================================================
// ===== 定时器回调函数                                       =====
// ====================================================================

/**
 * @brief 按键超时定时器回调函数
 */
static void lt_lsm_temp_key_timer_callback(void *param)
{
    // 定时器超时检测
    if (key_pressed == 0)
    {      
        // 清除临时按键回调
        terminate_all_temp_callbacks();

        // 超时未按下按键，拨打电话
        ql_event_t event = {.id = LT_SYS_FALL_CHECK_CALL};
        ql_rtos_event_send(lsm_task, &event);
    }
    else
    {
        // 按键已按下，定时器被提前停止
        api_mqtt_fall_publish(3);  // 上报已处理状态
        QL_APP_LSM_LOG("FALL_TEST: api_mqtt_fall_publish(3);");
    }
    
    // 停止定时器
    if (lt_lsm_temp_key_timer != NULL)
    {
        ql_rtos_timer_stop(lt_lsm_temp_key_timer);
    }
}

static void fall_key_click_db()
{
    // 标记按键已按下
    key_pressed = 1;
    
    // 停止定时器
    if (lt_lsm_temp_key_timer != NULL)
    {
        ql_rtos_timer_stop(lt_lsm_temp_key_timer);
        QL_APP_LSM_LOG("FALL_TEST: stop timer");
    }

    if (ql_tts_is_running())
    {
        ltapi_stop_tts_encoding();
        ql_rtos_task_sleep_ms(500); 
    }

    ltapi_play_tts(TTS_STR_FALL_CANCEL, strlen(TTS_STR_FALL_CANCEL));
    QL_APP_LSM_LOG("FALL_TEST: api_mqtt_fall_publish(3);");
    api_mqtt_fall_publish(3);  // 上报已处理状态
    
    // 清除临时按键回调
    terminate_all_temp_callbacks();
    
    // 重新使能硬件中断
    ql_int_enable(LSM_INT_GPIO_NUM);
}

void lt_fall_temp_temp_key_callback_register()
{
    KeyOriginalCallbacks reg;
    reg.lt_key_click_handle = fall_key_click_db;
    reg.lt_key_doubleclick_handle = fall_key_click_db;
    reg.lt_key_longclick_handle = fall_key_click_db;
    reg.lt_key_repeat_callback = NULL;
    set_all_temp_key_callbacks(&reg);
}


void lt_fall_call_pub(int state)
{
    api_mqtt_fall_publish(state);
    QL_APP_LSM_LOG("FALL_TEST: publish status %d", state);

    // 重新使能硬件中断
    ql_int_enable(LSM_INT_GPIO_NUM);

    fall_call_test_flag = 0;

}


// LSM6DS3TR 传感器主线程 (阻塞接收消息)
static void lsm6ds3tr_thread(void *param)
{
    ql_event_t event = {0};
    
    ql_rtos_task_sleep_ms(200); 

    if (lsm6ds3tr_sensor_init() != 0)
    {
        QL_APP_LSM_LOG("LSM6DS3TR initialization failed. Thread will exit.");
        ql_rtos_task_delete(NULL);
        return;
    }
    
    // **保存任务句柄**
    //lsm_main_task = ql_rtos_get_current_task_handle();
    
    // **GPIO 中断注册**
    ql_pin_set_func(LSM_INT_PIN_NUM, 0); // 确保引脚功能设置为 GPIO
    ql_gpio_init(LSM_INT_GPIO_NUM, GPIO_INPUT, PULL_NONE, 0); // 初始化 GPIO 为输入
    ql_int_register(LSM_INT_GPIO_NUM, EDGE_TRIGGER, DEBOUNCE_EN, EDGE_FALLING, PULL_NONE, _gpioint_callback01, NULL); // 注册中断
    ql_int_enable(LSM_INT_GPIO_NUM); // 使能中断


    // **启用跌倒检测中断**
    if (lsm6ds3tr_enable_fall_detection_on_int2() != 0)
    {
        QL_APP_LSM_LOG("Failed to enable Fall Detection Interrupt.");
    }
    
   // lsm6ds3tr_data_t sensor_data; 
    
    while (1)
    {
        // 阻塞等待事件/消息
        if (ql_event_wait(&event, QL_WAIT_FOREVER) != 0)
        {
             // 发生超时或错误，继续等待
            continue;
        }

        switch (event.id)
        {
            case LT_SYS_FALL_DETECTED:
            {       

                if(SND_TEL == ltplay_get_src() )
                {
                    //上报跌到已处理
                    lt_fall_call_pub(3);

                    break;
                }
                else 
                {
                    ltplay_stop();    
                }

                //注册临时按键
                lt_fall_temp_temp_key_callback_register();

                //播报tts
                QL_APP_LSM_LOG("Received LT_SYS_FALL_DETECTED event.");   
                ltapi_play_tts(TTS_STR_FALL_ALERT, strlen(TTS_STR_FALL_ALERT));

                //上报跌到预警信息
                api_mqtt_fall_publish(1);
                QL_APP_LSM_LOG("FALL_TEST: api_mqtt_fall_publish(1);");

                event.id = LT_SYS_FALL_CHECK_KEY;
                ql_rtos_event_send(lsm_task, &event);
                
                break;
            }
            case LT_SYS_FALL_CHECK_KEY:
            {
                QlOSStatus err = QL_OSI_SUCCESS;
                
                // 重置按键状态
                key_pressed = 0;
                
                // 创建按键定时器
                if (lt_lsm_temp_key_timer == NULL)
                {
                    err = ql_rtos_timer_create(&lt_lsm_temp_key_timer, lsm_task, lt_lsm_temp_key_timer_callback, NULL);
                    if (err != QL_OSI_SUCCESS)
                    {
                        QL_APP_LSM_LOG("Create lsm temp key timer fail err = %d", err);
                        break;
                    }
                }
                
                // 启动10秒定时器（单次触发）
                err = ql_rtos_timer_start(lt_lsm_temp_key_timer, 15000, 0);
                if (err != QL_OSI_SUCCESS)
                {
                    QL_APP_LSM_LOG("Start lsm temp key timer fail err = %d", err);
                }
                else
                {
                    QL_APP_LSM_LOG("wait 10s......");
                }
                
                break;
            }
            case LT_SYS_FALL_CHECK_CALL:
            {      
                // 拨打SOS电话
                fall_call_test_flag = 1;
                sosPhone_key_click_cb();
                break;   
            }
            default:
                // 可以添加其他周期性任务或系统事件处理
                lsm6ds3tr_dump_all_regs_for_debug();
                break;
        }
    }
}


void ql_lsm6ds3tr_init(void)
{
    QlOSStatus err = QL_OSI_SUCCESS;


    err = ql_rtos_task_create(&lsm_task, QL_LSM_TASK_STACK_SIZE, QL_LSM_TASK_PRIO, "LSM6DS3TR", lsm6ds3tr_thread, NULL, QL_LSM_TASK_EVENT_CNT);
    if (err != QL_OSI_SUCCESS)
    {
        QL_APP_LSM_LOG("LSM6DS3TR task create failed.");
    }
}