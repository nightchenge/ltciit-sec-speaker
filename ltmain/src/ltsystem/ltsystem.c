
/*
 * @Author: your name
 * @Date: 2023-09-23 15:36:46
 * @LastEditTime: 2025-08-18 16:17:51
 * @LastEditors: zhouhao
 * @Description: In User Settings Edit
 * @FilePath: /LTE01R02A02_C_SDK_G/components/ql-application/ltmain/src/ltsystem/ltsystem.c
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "osi_api.h"
#include "ql_api_osi.h"
#include "ql_log.h"
#include "ql_api_camera.h"
#include "ql_api_rtc.h"
#include "ql_i2c.h"
#include "led.h"
#include "ql_gpio.h"
#include "ql_api_camera.h"
#include "ql_fs.h"
#include "ql_sdmmc.h"
#include "ql_power.h"

#include "ltmqtt.h"
#include "ip_gb_comm.h"
#include "ql_api_sim.h"
#include "ql_api_nw.h"
#include "ebm_task_play.h"
#include "ltplay.h"
#include "ltsystem.h"
#include "ebm_task_play.h"
#include "ltsms_t.h"
#include "ltconf.h"
#include "ltsdmmc.h"
#include "ltuart2frx8016.h"

#define LT_SYS_LOG_LEVEL QL_LOG_LEVEL_INFO
#define LT_SYS_LOG(msg, ...) QL_LOG(LT_SYS_LOG_LEVEL, "lt_system", msg, ##__VA_ARGS__)
#define LT_SYS_LOG_PUSH(msg, ...) QL_LOG_PUSH("lt_system", msg, ##__VA_ARGS__)

#define GPIO_LED_POWER GPIO_38
#define GPIO_FM_POWER GPIO_17
#define GPIO_PA_POWER GPIO_0
#define GPIO_ES8311_POWER GPIO_1
#define GPIO_CI1302_POWER GPIO_3

ql_task_t lt_system_pool_task = NULL;
ql_timer_t lt_system_pool_timer = NULL;

static lt_lp_status_t lp_status = IDLE_MODE;

static uint32_t led_haf_br = 30;
static bool update_mode = FALSE;
static i2c_init_flag_t i2c_1_isInit = Uninitialized;

#define VCHG_USB_CHARGE 3800
bool lt_get_usb_charge_status()
{
    uint32_t vchg_vol = 0;
    ql_get_vchg_vol(&vchg_vol);
    if (vchg_vol > VCHG_USB_CHARGE)
    {
        return TRUE;
    }
    return FALSE;
}

/*
    FM/LED开电后必须调用该函数进行I2C初始化。
*/
void i2c_1_Initialize()
{
    if (i2c_1_isInit != Initialized)
    {
        ql_I2cInit(i2c_1, STANDARD_MODE);
        i2c_1_isInit = Initialized;
    }
}

/*
    进入低功耗时，释放掉I2C初始化。
*/
void i2c_1_I2cRelease()
{
    if (i2c_1_isInit == Initialized)
    {
        ql_I2cRelease(i2c_1);
        i2c_1_isInit = Uninitialized;
    }
}

bool ltget_update_mode()
{
    return update_mode;
}
void ltset_update_mode()
{
    update_mode = TRUE;
}

uint32_t ltget_lp_ledbr()
{
    if (1 == mqtt_param_ledEnable_get())
        return 65535;

    return led_haf_br;
}
void ltset_lp_ledbr(uint32_t brtime)
{
    led_haf_br = brtime;
}
lt_lp_status_t ltget_lp_status(void)
{
    return lp_status;
}

static lt_lp_callback_cfg_t _lt_lp_callback_cfg[] =
    {
        {SYS_PRJ, NULL, NULL},
        {UART_PRJ, NULL, NULL},
        {EBS_PRJ, NULL, NULL},

};

// 设置低功耗的状态，涉及到外围电路和一些低功耗回调的处理
int ltset_lp_status(lt_lp_status_t st)
{
    uint16_t num = 0;
    if (SLEEP_MODE == st)
    {
        // 进入低功耗后FM/LED芯片关闭电源，需要释放I2C，要不可能电流倒灌。by.zhaisky
        i2c_1_I2cRelease();

        if (2 != mqtt_param_ledEnable_get())
        {
            ql_gpio_set_level(GPIO_PA_POWER, LVL_LOW);
            ql_gpio_set_level(GPIO_ES8311_POWER, LVL_LOW);
            ql_gpio_set_level(GPIO_CI1302_POWER, LVL_LOW);

            for (num = 0; num < sizeof(_lt_lp_callback_cfg) / sizeof(_lt_lp_callback_cfg[0]); num++)
            {
                if (_lt_lp_callback_cfg[num].lt_lpdown_callback)
                    _lt_lp_callback_cfg[num].lt_lpdown_callback(NULL);
            }
        }
    }
    else if (IDLE_MODE == st)
    {
        // 从低功耗唤醒时控制单片机的wakeup引脚GPIO_36
        ql_gpio_set_level(GPIO_36, LVL_HIGH);
        ql_rtos_task_sleep_ms(200);
        ql_gpio_set_level(GPIO_36, LVL_LOW);

        ql_gpio_set_level(GPIO_PA_POWER, LVL_LOW);
        ql_gpio_set_level(GPIO_3, LVL_HIGH); // CI1302 power
        ql_rtos_task_sleep_ms(1000);         // 让1302启动起来后再打开功放电源,最好在locader中处理
        ql_gpio_set_level(GPIO_1, LVL_HIGH); // ES8311 power
        ql_rtos_task_sleep_ms(200);
        ql_gpio_set_level(GPIO_0, LVL_HIGH); // PA power

        for (num = 0; num < sizeof(_lt_lp_callback_cfg) / sizeof(_lt_lp_callback_cfg[0]); num++)
        {
            if (_lt_lp_callback_cfg[num].lt_lpup_callback)
                _lt_lp_callback_cfg[num].lt_lpup_callback(NULL);
        }
    }

    lp_status = st;
    return 0;
}
void lt_exit_lp()
{

    if (IDLE_MODE != ltget_lp_status()) // 如果按键的第一下屏幕是灭的话 直接先唤醒
    {
        char *len_on = "len_on";
        lt_panel_light_msg(0, len_on, strlen(len_on) + 1);
    }
}
// 更新信号灯
static void lt_nw_csq_pool(void)
{
    ql_sim_status_e card_status = 0;

    ql_sim_get_card_status(0, &card_status);

    if (card_status == QL_SIM_STATUS_NOSIM)
    {
        lt_set_led_colour(4, 0);
    }
    else
    {
        unsigned char ca = 0;
        if (!ql_nw_get_csq(0, &ca))
        {
            if (ca > 20 && ca <= 31)
            {
                lt_set_led_colour(4, 1);
            }
            else if (ca <= 20 && ca > 10)
            {
                lt_set_led_colour(4, 3);
            }
            else if (ca <= 10 && ca >= 1)
            {
                lt_set_led_colour(4, 2);
            }
            else
            {
                lt_set_led_colour(4, 0);
            }
        }
    }
}

// 获取连接状态,更新LED状态
static void lt_connect_status_pool(void)
{
    // state: OFF/GREEN/RED/ORANGE
    if (lt_mqtt_is_connected() == 1)
    {
        ebs_comm_reg_status_get() == 1 ? lt_set_led_colour(2, 3) : lt_set_led_colour(2, 2);
    }
    else
    {
        ebs_comm_reg_status_get() == 1 ? lt_set_led_colour(2, 1) : lt_set_led_colour(2, 0);
    }
}

int lt_connect_status_get(void)
{
    // state: OFF/GREEN/RED/ORANGE
    if (lt_mqtt_is_connected() == 1)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
/*
 * 更新EBS的状态,关闭其他通道。
 */
static void lt_ebs_status_pool(void)
{
    static uint8_t last_type = 0;
    task_source_t source;
    uint8_t type = ebs_player_task_ebm_type_query();
    if (last_type != type && last_type != 0 && type == 0)
    {
        LT_SYS_LOG("set_function_state = RT_TIME");
        if ((SND_TEL != ltplay_get_src()) && (SND_FM_URL != ltplay_get_src()) && (SND_MP3_URL != ltplay_get_src()) && (SND_FM != ltplay_get_src()) && (SND_MP3 != ltplay_get_src()) && (TRUE != ql_tts_check_sim_sms()))
        {

            LT_SYS_LOG("set_function_state = RT_TIME2");
            ltplay_check_play(SND_STOP);
            set_function_state(RT_TIME, 0);
        }
        // LT_SYS_LOG("set_function_state = RT_TIME2");
        // ltplay_check_play(SND_STOP);
        // set_function_state(RT_TIME, 0);
    }

    last_type = type;
    if (SND_TEL == ltplay_get_src())
    {
        return;
    }
    if (type == 1)
    {
        source = ebs_player_task_source_query();
        switch (source)
        {
        case TASK_SOURCE_EBS:
            ltplay_check_play(SND_EBS);
            set_function_state(EBSO, 0);
            break;
        case TASK_SOURCE_MQTT:
            set_function_state(YL, 1);
            break;
        case TASK_SOURCE_MQTT_2:
            ltplay_check_play(SND_EBS);
            set_function_state(YL, 2);
            break;
        case TASK_SOURCE_MQTT_3:
            ltplay_check_play(SND_EBS);
            set_function_state(YL, 3);
            break;
        case TASK_SOURCE_CLK:
            ltplay_check_play(SND_EBS);
            set_function_state(CLC, 10);
            break;
        case TASK_SOURCE_SHOUT:
            ltplay_check_play(SND_EBS);
            set_function_state(YL, 5);
            break;
        case TASK_SOURCE_AUDIOURL:
            // ltplay_check_play(SND_MP3_URL);
            // set_function_state(MP3, 0);
            // set_blink("mp3", 1);
            break;
        case TASK_SOURCE_FMURL:
            // ltplay_check_play(SND_FM_URL);
            //  set_function_state(FM, 1000);
            // set_blink("fm", 1);
            break;
        default:
            break;
        }
    }
    else if (type == 2)
    {
        source = ebs_player_task_source_query();
        switch (source)
        {
        case TASK_SOURCE_EBS:
            ltplay_check_play(SND_EBS);
            set_function_state(EBSE, 0);
            break;
        case TASK_SOURCE_MQTT:
            set_function_state(YL, 1);
            break;
        case TASK_SOURCE_MQTT_2:
            ltplay_check_play(SND_EBS);
            set_function_state(YL, 2);
            break;
        case TASK_SOURCE_MQTT_3:
            ltplay_check_play(SND_EBS);
            set_function_state(YL, 3);
            break;
        case TASK_SOURCE_CLK:
            ltplay_check_play(SND_EBS);
            set_function_state(CLC, 10);
            break;
        case TASK_SOURCE_SHOUT:
            ltplay_check_play(SND_EBS);
            set_function_state(YL, 5);
            break;
        case TASK_SOURCE_AUDIOURL:
            // ltplay_check_play(SND_MP3_URL);
            // set_function_state(MP3, 0);
            // set_blink("mp3", 1);
            break;
        case TASK_SOURCE_FMURL:
            // ltplay_check_play(SND_FM_URL);
            // set_function_state(FM, 1000);
            // set_blink("fm", 1);
            break;
        default:
            break;
        }
    }
}

static void lt_system_pool_thread(void *param)
{
    ql_event_t event = {0};
    while (1)
    {
        if (ql_event_wait(&event, QL_WAIT_FOREVER) != 0)
        {
            continue;
        }
        switch (event.id)
        {
        case QL_EVENT_APP_START:
            lt_nw_csq_pool();
            lt_connect_status_pool();
            lt_ebs_status_pool();
            break;
        case LT_SYS_LED_POWER_ON:
            if (i2c_1_isInit != Initialized)
            {
                ql_I2cInit(i2c_1, STANDARD_MODE);
                i2c_1_isInit = Initialized;
            }
            ql_gpio_set_level(GPIO_LED_POWER, LVL_HIGH);
            break;
        case LT_SYS_LED_POWER_OFF:
            ql_gpio_set_level(GPIO_LED_POWER, LVL_LOW);
            break;
        case LT_SYS_FM_POWER_ON:
            if (i2c_1_isInit != Initialized)
            {
                ql_I2cInit(i2c_1, STANDARD_MODE);
                i2c_1_isInit = Initialized;
            }
            ql_gpio_set_level(GPIO_FM_POWER, LVL_HIGH);
            break;
        case LT_SYS_FM_POWER_OFF:
            ql_gpio_set_level(GPIO_FM_POWER, LVL_LOW);
            break;
        case LT_SYS_EXTERNAL_POWER_ON:
            ltset_lp_status(IDLE_MODE);
            break;
        case LT_SYS_EXTERNAL_POWER_OFF:
            ltset_lp_status(SLEEP_MODE);
            break;
        default:
            break;
        }
    }
}

// 外部调用system 事件的接口
void lt_system_msg_handle(uint32 msg_id)
{
    ql_event_t event = {
        .id = msg_id,
    };
    ql_rtos_event_send(lt_system_pool_task, &event);
}

void lt_system_timer_callback(void *ctx)
{
    ql_event_t event = {
        .id = QL_EVENT_APP_START,
    };
    ql_rtos_event_send(lt_system_pool_task, &event);

    static int sd_state = 0;
    if (sd_state != lt_sdmmc_get_state())
    {
        api_mqtt_sd_state_Publish(); // 用来上报sd卡的状态
        sd_state = lt_sdmmc_get_state();
    }
}

void lt_lp_callback_register(lt_lp_callback_cfg_t *reg)
{
    uint16_t num = 0;
    for (num = 0; num < sizeof(_lt_lp_callback_cfg) / sizeof(_lt_lp_callback_cfg[0]); num++)
    {
        if (reg->lp_id == _lt_lp_callback_cfg[num].lp_id)
        {
            if (reg->lt_lpdown_callback)
                _lt_lp_callback_cfg[num].lt_lpdown_callback = reg->lt_lpdown_callback;
            if (reg->lt_lpup_callback)
                _lt_lp_callback_cfg[num].lt_lpup_callback = reg->lt_lpup_callback;
            break;
        }
    }
}

static void lt_sys_lpup_cb()
{
    if (lt_system_pool_timer != NULL)
    {
        ql_rtos_timer_start(lt_system_pool_timer, 1000, 1);
    }
}

static void lt_sys_lpdown_cb()
{
    if (lt_system_pool_timer != NULL)
    {
        ql_rtos_timer_stop(lt_system_pool_timer);
    }
}

static void lt_lp_system_callback_register()
{
    lt_lp_callback_cfg_t reg;
    reg.lp_id = SYS_PRJ;
    reg.lt_lpdown_callback = lt_sys_lpdown_cb;
    reg.lt_lpup_callback = lt_sys_lpup_cb;
    lt_lp_callback_register(&reg);
}

// 开启功放
void lt_audio_pa_enable_pw(void)
{
    ql_gpio_set_level(GPIO_PA_POWER, LVL_HIGH);
}
// 关掉功放
void lt_audio_pa_disable_pw(void)
{
    ql_gpio_set_level(GPIO_PA_POWER, LVL_LOW);
}

// 开启功放power
void lt_audio_pa_enable(void)
{
    ql_gpio_set_level(GPIO_35, LVL_HIGH);
}
// 关掉功放power
void lt_audio_pa_disable(void)
{
    ql_gpio_set_level(GPIO_35, LVL_LOW);
}

void lt_sys_pin_init(void)
{
    // 功放使能引脚,去使能
    ql_pin_set_func(50, 2);
    ql_gpio_set_direction(GPIO_35, GPIO_OUTPUT);
    ql_gpio_set_level(GPIO_35, LVL_LOW);

    // 初始化LED POWER引脚
    ql_pin_set_func(52, 2);
    ql_gpio_set_direction(GPIO_38, GPIO_OUTPUT);

    // 初始化I2C引脚 led fm 共用同一个i2c_1
    ql_pin_set_func(57, 1);
    ql_pin_set_func(58, 1);

    i2c_1_Initialize(); // 初始化I2C，在FM启动/LED启动时都需调用一下I2C初始化操作。

    // 初始化FM POWER引脚
    ql_pin_set_func(69, 0);
    ql_gpio_set_direction(GPIO_17, GPIO_OUTPUT);

    ql_pin_set_func(25, 0);
    ql_gpio_set_direction(GPIO_0, GPIO_OUTPUT);
    ql_pin_set_func(20, 0);
    ql_gpio_set_direction(GPIO_1, GPIO_OUTPUT);
    ql_pin_set_func(21, 0);
    ql_gpio_set_direction(GPIO_3, GPIO_OUTPUT);

    // GPIo36 单片机的wakeup引脚
    ql_pin_set_func(51, 2);
    ql_gpio_set_direction(GPIO_36, GPIO_OUTPUT);
    ql_gpio_set_level(GPIO_36, LVL_LOW);

    // 减少下面两个GPIO操作时延，减少功放启动时间。暂时测试无POP声音
    ql_gpio_set_level(GPIO_3, LVL_HIGH); // CI1302 power
    ql_rtos_task_sleep_ms(80);           // 让1302启动起来后再打开功放电源,最好在locader中处理
    ql_gpio_set_level(GPIO_1, LVL_HIGH); // ES8311 power
    ql_rtos_task_sleep_ms(50);
    ql_gpio_set_level(GPIO_0, LVL_HIGH); // PA power
}

static int ebs_play_callback(void)
{
    if (IDLE_MODE != ltget_lp_status()) // 直接先唤醒
    {
        char *len_on = "len_on";
        lt_panel_light_msg(0, len_on, strlen(len_on) + 1);
    }
    return 0;
}

int lt_get_freeblock()
{
    int file_free_size = ql_fs_free_size("UFS:");
    LT_SYS_LOG("file_free_size = %d", file_free_size);
    return file_free_size;
}

void lt_system_init(void)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    // 初始化一些pin引脚功能
    lt_sys_pin_init();
    // 注册low_power回调
    lt_lp_system_callback_register();
    eb_player_prepare_sucess_callback_set(ebs_play_callback);

    err = ql_rtos_task_create(&lt_system_pool_task, 2 * 1024, APP_PRIORITY_NORMAL, "lt_system_task", lt_system_pool_thread, NULL, 10);
    if (err != QL_OSI_SUCCESS)
    {
        LT_SYS_LOG("Create system task fail err = %d", err);
    }

    err = ql_rtos_timer_create(&lt_system_pool_timer, lt_system_pool_task, lt_system_timer_callback, NULL);
    if (err != QL_OSI_SUCCESS)
    {
        LT_SYS_LOG("Create system pool timer fail err = %d", err);
    }

    err = ql_rtos_timer_start(lt_system_pool_timer, 1000, 1);
    if (err != QL_OSI_SUCCESS)
    {
        LT_SYS_LOG("Start system pool timer fail err = %d", err);
    }
}