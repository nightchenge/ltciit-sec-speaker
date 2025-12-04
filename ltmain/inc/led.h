#ifndef __LED_H__
#define __LED_H__

#ifdef __cplusplus
extern "C"
{
#endif

#define VK16K33_CMD_VALUE 0xA0      // 初始值
#define VK16K33_CMD_BLINK 0x80      // IIC闪烁地址
#define VK16K33_CMD_BRIGHTNESS 0xE0 // 亮度地址
#define LED_NUMBER 4
#define level_max 17

#define QL_APP_I2C_LOG_LEVEL QL_LOG_LEVEL_INFO
#define QL_APP_I2C_LOG(msg, ...) QL_LOG(QL_APP_I2C_LOG_LEVEL, "QL_APP_I2C", msg, ##__VA_ARGS__)
#define QL_APP_I2C_LOG_PUSH(msg, ...) QL_LOG_PUSH("QL_APP_I2C", msg, ##__VA_ARGS__)
#define QL_I2C_TASK_STACK_SIZE 1024 * 10
#define QL_I2C_TASK_PRIO APP_PRIORITY_NORMAL
#define QL_I2C_TASK_EVENT_CNT 5
#define LED_SalveAddr (0xe0 >> 1)
#define SalveAddr_r_8bit (0xe1 >> 1)
/*reserved*/
#define SalveAddr_w_16bit (0xff >> 1)
#define SalveAddr_r_16bit (0xff >> 1)
#define LED_I2C i2c_1

    typedef int QlI2CStatus;
    typedef void *ql_task_t;

    enum led_function
    {
        RT_TIME = 1,
        FM,
        FM_CHECK,
        EBSE,
        EBSO,
        MP3,
        YL,
        CLC,
        CALL,
        PAUS,
        NEW_INFO,
        VOLUME, // 调节音量时显示
    };
    enum led_colour
    {
        OFF,
        GREEN,
        RED,
        ORANGE
    };

    typedef enum lt_led_status
    {
        LED_POWER_OFF,
        LED_POWER_ON
    } lt_led_status_t;

    // enum led_key
    // {
    //     VOICE_LED,
    //     SMS_LED,
    //   //  SOS_LED,
    //     FM_LED,
    //     MP3_LED
    // };
    // enum led_key
    // {
    //     FM_LED=0,
    //     VOICE_LED,
    //     SMS_LED,
    //     SOS_LED,
    //     MP3_LED
    // };

    /**
     * @brief 打开/关闭LED电源控制开关
     * @param lt_led_status_t  : LED电源开关状态
     * @return
     *       void
     */
    void lt_led_power_status_set(lt_led_status_t st);

    /**
     * @brief LED电源状态获取
     * @param void ： 无
     * @return
     *       lt_led_status_t  : LED电源开关状态
     */
    lt_led_status_t lt_led_power_status_get(void);

    unsigned char function_state;                                    //  led显示功能
    unsigned int function_buf[1];                                    //  led传入数据（FM、MP3、LF存在）
    extern unsigned char blink[5][2];                                //  值1：按键灯（0：关闭 1：打开 2：闪烁）
                                                                     //   值2：闪烁的状态值，默认为0
    int set_blink(char *name, unsigned char state);                  // 设置按键灯状态 name :按键的功能名字 state：灯的状态(0不亮 1常量 2闪烁)
    int set_function_state(char state, int buf);                     // 设置数码管显示状态和内容
    int lt_led_show_function(unsigned char num1, unsigned int num2); // 设置
    void lt_set_led_colour(unsigned char num, unsigned char state);  // 控制面板灯 num:1-4 state：OFF/GREEN/RED/ORANGE
    void ql_i2c_led_init(void);
    void led_on();
    void led_off();
    void lt_panel_light_msg(int type, char *data, int data_len);
    int lt_led_show_onoff(void);
    void lt_vk16k33_set_brightness(unsigned char level);            //新增：小程序设置屏幕亮度
#ifdef __cplusplus
}
#endif
#endif