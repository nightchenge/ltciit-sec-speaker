/*

 * @LastEditTime: 2025-12-18 08:57:10
 * @LastEditors: ubuntu
 *@Description: 系统状态定时检测
 * @FilePath: /LTE01R02A02_C_SDK_G/components/ql-application/ltmain/inc/ltsystem.h
 */

#ifndef LTSYSTEM_H
#define LTSYSTEM_H

#ifdef __cplusplus
extern "C"
{
#endif

#define PACK_OLD 0
#define PACK_LOOTOM 1
#define PACK_NANJING 2
#define PACK_TYPE PACK_OLD

// #define TYPE_SICHUAN 2
// #define TYPE_QIXIA 1
// #define TYPE_GENERIC 0
// #define TYPE_FUNC TYPE_GENERIC


#define SOFTWARE_VERSION "V3.00.25" //V2.00.xx  通用版，V2.01.xx 鼓楼定制 ,V2.02.xx 栖霞定制  V2.03.xx 四川定制（只有一版）

#if PACK_TYPE == PACK_OLD
#define HARDWARE_VERSION "V2.01.04" //V1.01.01 源流结构,低功耗板子 V1.01.02 按键改制 短信和电源按键替换 v1.01.03 电源模块整改，vbus电压从4200->3800 V2.01.04 新硬件支持编码器和陀螺仪
#elif PACK_TYPE == PACK_LOOTOM
#define HARDWARE_VERSION "V1.02.01" // 自研结构结构,低功耗板子
#else
#define HARDWARE_VERSION "V1.00.01" // 源流结构,老板子
#endif

#define VCHG_USB_CHARGE 3800
#define LED_BRTIME_MAX 65535 //在设备需求常亮状态下返回的值

    typedef enum
    {
        LT_SYS_LED_POWER_ON = QL_EVENT_APP_START + 1,
        LT_SYS_LED_POWER_OFF = QL_EVENT_APP_START + 2,
        LT_SYS_FM_POWER_ON = QL_EVENT_APP_START + 3,
        LT_SYS_FM_POWER_OFF = QL_EVENT_APP_START + 4,
        LT_SYS_EXTERNAL_POWER_ON = QL_EVENT_APP_START + 5,
        LT_SYS_EXTERNAL_POWER_OFF = QL_EVENT_APP_START + 6,
    } lt_api_event_id_e;

    typedef enum lt_lp_status
    {
        IDLE_MODE = 0,
        SLEEP_MODE,
        DEEP_SLEEP_MODE,
        SHUTDOWN_MODE
    } lt_lp_status_t;

    void lt_system_init(void);
    // 外部调用system 事件的接口
    void lt_system_msg_handle(uint32 msg_id);

    lt_lp_status_t ltget_lp_status(void);

   // int ltset_lp_status(lt_lp_status_t st);

    typedef enum lt_lp_prj
    {
        SYS_PRJ = 0,
        UART_PRJ,
        EBS_PRJ
    } lt_lp_prj_t;

    typedef enum i2c_init_flag
    {
        Uninitialized = 0,
        Initialized,
    } i2c_init_flag_t;

    typedef struct lt_lp_callback_cfg
    {
        int lp_id;
        void (*lt_lpdown_callback)();
        void (*lt_lpup_callback)();
    } lt_lp_callback_cfg_t;

    void lt_lp_callback_register(lt_lp_callback_cfg_t *reg);
    void lt_audio_pa_enable(void);
    void lt_audio_pa_disable(void);
    void lt_audio_pa_enable_pw(void);
    void lt_audio_pa_disable_pw(void);
    void ltset_lp_ledbr(uint32_t brtime);
    uint32_t ltget_lp_ledbr();
    void lt_exit_lp();

    bool ltget_update_mode();
    void ltset_update_mode();

    void i2c_1_Initialize(void);
    void i2c_1_I2cRelease(void);
    int lt_get_sd_state();

    int lt_get_freeblock();
    
    int lt_connect_status_get(void);

    bool lt_get_usb_charge_status();
#ifdef __cplusplus
} /*"C" */
#endif

#endif /* LTSYSTEM_H */
