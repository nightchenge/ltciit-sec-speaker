#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ql_api_osi.h"
#include "ql_log.h"
#include "ql_api_camera.h"
#include "ql_api_rtc.h"
#include "ql_i2c.h"
#include "led.h"
#include "ql_gpio.h"
#include "ql_api_camera.h"
#include "ltplay.h"
#include "ltsystem.h"
#include "ltuart2frx8016.h"
#include "ltadc.h"
#include "ql_power.h"
#include "volume_knob.h"
#include "ec800g_ota_master.h"

#define QL_APP_LED_LOG_LEVEL QL_LOG_LEVEL_INFO
#define QL_APP_LED_LOG(msg, ...) QL_LOG(QL_APP_LED_LOG_LEVEL, "QL_APP_LED", msg, ##__VA_ARGS__)
#define QL_APP_LED_LOG_PUSH(msg, ...) QL_LOG_PUSH("QL_APP_LED", msg, ##__VA_ARGS__)

#define QL_I2C_TASK_PRIO APP_PRIORITY_NORMAL
#define QL_I2C_TASK_EVENT_CNT 5
#define level_max 17
#define LED_SalveAddr (0xe0 >> 1)
#define SalveAddr_r_8bit (0xe1 >> 1)
#define LED_I2C i2c_1

/*
SEG1  0x80
SEG2  0x40
SEG3  0x20
SEG4  0x10
SEG5  0x08
SEG6  0x04
SEG7  0x02
SEG8  0x01
*/
/*、
    |+++4+++|
    |+  +  +|
   1|+  +  +|6
    |+  +  +|
    |+++2+++|
    |+  +  +|
   3|+  +  +|5
    |+++0+++|
*/
// 数码管数据 0-9              0    1    2    3    4    5    6    7    8    9
unsigned char lt_table[] = {0xde, 0x06, 0xba, 0xae, 0x66, 0xec, 0xfc, 0x0e, 0xfe, 0xee, 0x00};
//                          A     b     C     E     L     T    F   O   P      S      y        u
unsigned char lt_words[] = {0x7e, 0xf4, 0xd8, 0xf8, 0xd0, 0x5e, 0x78, 0xde, 0x7a, 0xec, 0xe6, 0xd6};

// 数码管地址
unsigned char led[] = {0x00, 0x08, 0x06, 0x04, 0x0c};
unsigned char led1_sta[] = {0x00, 0x08, 0x40, 0x48};
unsigned char led2_sta[] = {0x00, 0x20, 0x10, 0x30};
unsigned char led3_sta[] = {0x00, 0x02, 0x01, 0x03};
unsigned char led4_sta[] = {0x00, 0x80, 0x04, 0x84};
unsigned char led_symbol[] = {0x00, 0x02, 0x80, 0x01};

static lt_led_status_t led_status = LED_POWER_OFF;

// 默认（-1）不显示volume，每次调用时显示3s，超时后显示当前状态
static struct
{
    int show_volume;
    int times;
} volume_ctrl = {.show_volume = -1, .times = 0};

void lt_led_power_status_set(lt_led_status_t st)
{
    led_status = st;
}

lt_led_status_t lt_led_power_status_get()
{
    return led_status;
}

int VK16K33_blink(unsigned char state)
{
    unsigned char buf[5] = {0x80, 0x81, 0x82, 0x84, 0x86};
    ql_I2cWrite(LED_I2C, LED_SalveAddr, buf[state], &buf[state], 1);
    return 0;
}

int VK16K33_brightness(unsigned char level)
{
    unsigned char buf[16] = {0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef};

    if (level < level_max)
    {
        ql_I2cWrite(LED_I2C, LED_SalveAddr, buf[level], &buf[level], 1);
    }
    else
        return -1;

    return 0;
}

uint8_t brightness_level = 15;  //默认值
// 新增：小程序设置屏幕亮度(16级)
void lt_vk16k33_set_brightness(unsigned char level)
{
    brightness_level = level;
    VK16K33_brightness(brightness_level);
}

int VK16K33_clean_data()
{
    unsigned char buf = 0x00;
    for (int i = 0x00; i < 0x0f; i++)
    {
        ql_I2cWrite(LED_I2C, LED_SalveAddr, i, &buf, 1);
    }

    return 0;
}

int VK16K33_init()
{
    unsigned char buf1 = 0x21;
    unsigned char buf2 = 0xa0;

    VK16K33_clean_data();
    ql_I2cWrite(LED_I2C, LED_SalveAddr, 0x21, &buf1, 1);
    ql_I2cWrite(LED_I2C, LED_SalveAddr, 0xa0, &buf2, 1);
    VK16K33_blink(1);
    // VK16K33_brightness(15);
    VK16K33_brightness(brightness_level);

    return 0;
}

unsigned char blink[5][2] = { // 0:灭  1：亮   2：闪烁
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0},
    {0, 0}};

// num:按键灯号
// state：灯的状态，0不亮 1常量 2闪烁
#if PACK_TYPE == PACK_LOOTOM
enum led_key
{
    FM_LED = 0,
    VOICE_LED,
    SMS_LED,
    SOS_LED,
    MP3_LED
};
#elif PACK_TYPE == PACK_NANJING
#define WATER_LED 0
#define GAS_LED 1
#define SOS_LED 0
#define ELEC_LED 2
#define FM_LED 3
#define VOICE_LED 0
#define SMS_LED 1
#define MP3_LED 3
#else
#define VOICE_LED 0
#define SMS_LED 1
#define SOS_LED 0
#define FM_LED 2
#define MP3_LED 3
#endif

int set_blink(char *name, unsigned char state)
{
    if (strcmp(name, "fm") == 0)
    {
        blink[FM_LED][0] = state;
    }
#if PACK_TYPE == PACK_NANJING
    else if (strcmp(name, "water") == 0)
    {
        blink[WATER_LED][0] = state;
    }
    else if (strcmp(name, "gas") == 0)
    {
        blink[GAS_LED][0] = state;
    }
    else if (strcmp(name, "elec") == 0)
    {
        blink[ELEC_LED][0] = state;
    }
#endif
    else if (strcmp(name, "voice") == 0)
    {
        blink[VOICE_LED][0] = state;
    }
    else if (strcmp(name, "sms") == 0)
    {
        blink[SMS_LED][0] = state;
    }
    else if (strcmp(name, "sos") == 0)
    {
        blink[SOS_LED][0] = state;
    }
    else if (strcmp(name, "mp3") == 0)
    {
        blink[MP3_LED][0] = state;
    }
    return 0;
}

unsigned char set_blink_all_off()
{
    for (int i = 0; i < 5; i++)
    {
        blink[i][0] = 0;
    }

    return 0;
}

int VK16K33_Digital_show(unsigned char num, unsigned char data)
{
    unsigned char w_date = 0;

    if (num < 5)
    {
        w_date = lt_table[data];
        if (blink[num][0] == 0) // 灭
        {
            ql_I2cWrite(LED_I2C, LED_SalveAddr, led[num], &w_date, 1);
        }
        else if (blink[num][0] == 1) // 亮
        {
            w_date |= 0x01;
            ql_I2cWrite(LED_I2C, LED_SalveAddr, led[num], &w_date, 1);
        }
        else if (blink[num][0] == 2) // 闪烁
        {
            if (blink[num][1] == 0)
            {
                ql_I2cWrite(LED_I2C, LED_SalveAddr, led[num], &w_date, 1);
            }
            else
            {
                w_date |= 0x01;
                ql_I2cWrite(LED_I2C, LED_SalveAddr, led[num], &w_date, 1);
            }
            blink[num][1] = !blink[num][1];
        }
    }
    else
        return -1;

    return 0;
}

int VK16K33_Words_show(unsigned char num, unsigned char data)
{
    unsigned char w_date = 0;

    if (num < 5)
    {
        w_date = lt_words[data];
        if (blink[num][0] == 0) // 灭
        {
            ql_I2cWrite(LED_I2C, LED_SalveAddr, led[num], &w_date, 1);
        }
        else if (blink[num][0] == 1) // 亮
        {
            w_date |= 0x01;
            ql_I2cWrite(LED_I2C, LED_SalveAddr, led[num], &w_date, 1);
        }
        else if (blink[num][0] == 2) // 闪烁
        {
            if (blink[num][1] == 0)
            {
                ql_I2cWrite(LED_I2C, LED_SalveAddr, led[num], &w_date, 1);
            }
            else
            {
                w_date |= 0x01;
                ql_I2cWrite(LED_I2C, LED_SalveAddr, led[num], &w_date, 1);
            }
            blink[num][1] = !blink[num][1];
        }
    }
    else
        return -1;

    return 0;
}

int set_led_off()
{
    unsigned char value;

    for (int i = 0; i < 5; i++)
    {
        ql_I2cRead(LED_I2C, LED_SalveAddr, led[i], &value, 1);
        value &= 0x01;
        ql_I2cWrite(LED_I2C, LED_SalveAddr, led[i], &value, 1);
    }

    return 0;
}

uint8_t resetBit(uint8_t data, int8_t n)
{
    return data &= ~(0x1 << n);
}

int VK16K33_Led_show(unsigned char num, unsigned char data)
{
    unsigned char led_addr[] = {0x0a, 0x02};
    uint8_t value = 0;

    if (num == 1)
    {
        ql_I2cRead(LED_I2C, LED_SalveAddr, led_addr[0], &value, 1);
        value = resetBit(value, 3);
        value = resetBit(value, 6);
        value |= led1_sta[data];
        ql_I2cWrite(LED_I2C, LED_SalveAddr, led_addr[0], &value, 1);
    }
    else if (num == 2)
    {
        ql_I2cRead(LED_I2C, LED_SalveAddr, led_addr[1], &value, 1);
        value = resetBit(value, 4);
        value = resetBit(value, 5);
        value |= led2_sta[data];
        ql_I2cWrite(LED_I2C, LED_SalveAddr, led_addr[1], &value, 1);
    }
    else if (num == 3)
    {
        ql_I2cRead(LED_I2C, LED_SalveAddr, led_addr[1], &value, 1);
        value = resetBit(value, 0);
        value = resetBit(value, 1);
        value |= led3_sta[data];
        ql_I2cWrite(LED_I2C, LED_SalveAddr, led_addr[1], &value, 1);
    }
    else if (num == 4)
    {
        ql_I2cRead(LED_I2C, LED_SalveAddr, led_addr[1], &value, 1);
        value = resetBit(value, 2);
        value = resetBit(value, 7);
        value |= led4_sta[data];
        ql_I2cWrite(LED_I2C, LED_SalveAddr, led_addr[1], &value, 1);
    }
    else if (num == 5)
    {
        ql_I2cRead(LED_I2C, LED_SalveAddr, led_addr[0], &value, 1);
        value = resetBit(value, 1);
        value = resetBit(value, 7);
        value |= led_symbol[data];
        ql_I2cWrite(LED_I2C, LED_SalveAddr, led_addr[0], &value, 1);
    }
    else
    {
        return -1;
    }

    return 0;
}

int VK16K33_clean_data2()
{
    unsigned char value;
    unsigned char w_data[1] = {0x00};
    for (int i = 0; i < 4; i++)
    {
        ql_I2cRead(LED_I2C, LED_SalveAddr, led[i], &value, 1);
        value = resetBit(value, 0);
        value |= w_data[0];
        ql_I2cWrite(LED_I2C, LED_SalveAddr, led[i], &value, 1);
    }

    return 0;
}
// num: 1、2、3、4
// state: OFF/GREEN/RED/ORANGE
void lt_set_led_colour(unsigned char num, unsigned char state)
{
    if (num < 5)
        VK16K33_Led_show(num, state);
}

unsigned char timer_count = 0; // 计时
unsigned char led_on_off = 0;
// 功能一：时间显示（00:00 - 23:59）
int lt_led_show_onoff(void)
{
    return led_on_off;
}
void lt_led_show_rt_time(void)
{
    ql_rtc_time_t tm;
    ql_rtc_set_timezone(32); // UTC+32
    ql_rtc_get_localtime(&tm);
    // ql_rtc_get_time(&tm);
    int min1, min2, hour1, hour2; // sec1,sec2;
    hour1 = tm.tm_hour % 10;
    hour2 = tm.tm_hour / 10;
    min1 = tm.tm_min % 10;
    min2 = tm.tm_min / 10;

    QL_APP_LED_LOG("led_show is %d  %d  %d  %d:%d:%d", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    QL_APP_LED_LOG("led_show is %d%d :%d%d", hour2, hour1, min2, min1);

    VK16K33_Led_show(5, 1); // 显示冒号
    VK16K33_Digital_show(0, hour2);
    VK16K33_Digital_show(1, hour1);
    VK16K33_Digital_show(2, min2);
    VK16K33_Digital_show(3, min1);
    VK16K33_Digital_show(4, 0);
    ql_rtos_task_sleep_ms(500);
    VK16K33_Led_show(5, 0);
    uint32_t vchg_vol = 0;
    ql_get_vchg_vol(&vchg_vol);
    QL_APP_LED_LOG("vchg_vol  ==%d\n", vchg_vol);

    //不进入低功耗的几个条件，设置的常量模式 或者接着电，处在升级模式，处于语音唤醒状态，处于OTA状态
    if ((65535 == ltget_lp_ledbr()) || (lt_get_usb_charge_status() == TRUE)  || (TRUE == ltget_update_mode()) || (0 != lt_ltasr_wakeup()) || (TRUE == is_ota_mode()))
    {
        //  led_on();
        if (timer_count > ltget_lp_ledbr())
        {
            // VK16K33_brightness(15);
            VK16K33_brightness(brightness_level);
        }
        timer_count = 0;
    }

    if (timer_count == ltget_lp_ledbr())
    {
        // VK16K33_brightness(5);
        VK16K33_brightness((uint8_t)(brightness_level/2));
    }
    else if (timer_count / 2 == ltget_lp_ledbr())
    {
        led_off();
    }
}

// 功能二：FM广播 87.0 - 108.0
int lt_led_show_fm(int data)
{
    unsigned char buf[4];
    int num;

    num = data / 10;

    buf[0] = num / 1000;
    buf[1] = (num - buf[0] * 1000) / 100;
    buf[2] = (num - buf[0] * 1000 - buf[1] * 100) / 10;
    buf[3] = num - buf[0] * 1000 - buf[1] * 100 - buf[2] * 10;

    set_blink("fm", 1);
    VK16K33_Led_show(5, 2); // 显示小数点
    VK16K33_Digital_show(0, buf[0]);
    VK16K33_Digital_show(1, buf[1]);
    VK16K33_Digital_show(2, buf[2]);
    VK16K33_Digital_show(3, buf[3]);
    VK16K33_Digital_show(4, 0);

    return 0;
}

// 功能三：应急广播EbSE
void lt_led_show_ebse(void)
{
    VK16K33_Led_show(5, 0);
    VK16K33_Words_show(0, 3);
    VK16K33_Words_show(1, 1);
    VK16K33_Words_show(2, 9);
    VK16K33_Words_show(3, 3);
    VK16K33_Digital_show(4, 0);
}

// 功能四：日常广播EbSO
void lt_led_show_ebso(void)
{
    VK16K33_Led_show(5, 0);
    VK16K33_Words_show(0, 3);
    VK16K33_Words_show(1, 1);
    VK16K33_Words_show(2, 9);
    VK16K33_Words_show(3, 7);
    VK16K33_Digital_show(4, 0);
}

// 功能五：音乐播放MP3
int lt_led_show_mp3(unsigned char data)
{
    unsigned char buf[2];

    buf[1] = data / 10 % 10;
    buf[0] = data % 10;
    set_blink("mp3", 1);
    VK16K33_Led_show(5, 0);
    VK16K33_Words_show(0, 0);
    VK16K33_Words_show(1, 6);
    VK16K33_Digital_show(2, buf[1]);
    VK16K33_Digital_show(3, buf[0]);
    VK16K33_Digital_show(4, 0);

    return 0;
}

// 功能六：助老广播 yL 1-3
void lt_led_show_yL(unsigned char data)
{
    VK16K33_Led_show(5, 0);
    VK16K33_Words_show(0, 10);
    VK16K33_Words_show(1, 4);
    VK16K33_Digital_show(2, 0);
    VK16K33_Digital_show(3, data);
    VK16K33_Digital_show(4, 0);
}
// 功能七：闹钟广播 CLC0
void lt_led_show_clc(unsigned char data)
{
    VK16K33_Led_show(5, 2);
    VK16K33_Words_show(0, 2);
    VK16K33_Words_show(1, 4);
    VK16K33_Words_show(2, 2);
    VK16K33_Digital_show(3, data);
    VK16K33_Digital_show(4, 0);
}
// 功能七：接打电话

void lt_led_show_call()
{

    VK16K33_Led_show(5, 0);
    VK16K33_Words_show(0, 2);
    VK16K33_Words_show(1, 0);
    VK16K33_Words_show(2, 4);
    VK16K33_Words_show(3, 4);
    VK16K33_Digital_show(4, 0);
}

void lt_mp3_show_paus()
{
    set_blink("mp3", 1);
    VK16K33_Led_show(5, 0);
    VK16K33_Words_show(0, 8);
    VK16K33_Words_show(1, 0);
    VK16K33_Words_show(2, 11);
    VK16K33_Words_show(3, 9);
    VK16K33_Digital_show(4, 0);
}

void lt_mp3_show_new_info()
{
    set_function_state(RT_TIME, 0);
}

void lt_led_show_volume(unsigned char data)
{
    VK16K33_Led_show(5, 1); // 显示冒号
    VK16K33_Words_show(0, 11);
    VK16K33_Words_show(1, 4);
    VK16K33_Digital_show(2, data >= 10 ? 1 : 0);
    VK16K33_Digital_show(3, data >= 10 ? data - 10 : data);
}

void ltapi_on_volume_changed(unsigned char value)
{
    volume_ctrl.times = 10;
    volume_ctrl.show_volume = value;
}

int lt_led_show_function(unsigned char num1, unsigned int num2)
{
    switch (num1)
    {
    case RT_TIME:
        lt_led_show_rt_time();
        break;
    case FM:
        lt_led_show_fm(num2);
        break;
    case FM_CHECK:
        lt_led_show_fm(num2);
        break;
    case EBSE:
        lt_led_show_ebse();
        break;
    case EBSO:
        lt_led_show_ebso();
        break;
    case MP3:
        lt_led_show_mp3(num2);
        break;
    case YL:
        lt_led_show_yL(num2);
        break;
    case CLC:
        lt_led_show_clc(num2);
        break;
    case CALL:
        lt_led_show_call();
        break;
    case PAUS:
        lt_mp3_show_paus();
        break;
    case NEW_INFO:
        lt_mp3_show_new_info();
        break;
    case VOLUME:
        lt_led_show_volume(num2);
        break;
    default:
        break;
    }

    return 0;
}

int get_function_state()
{
    return function_state;
}

int set_function_state(char state, int buf)
{
    function_state = state;
    function_buf[0] = buf;

    return 0;
}

int get_function_buf()
{
    return function_buf[0];
}

void led_on()
{
    unsigned char buf1 = 0x21;
    unsigned char buf2 = 0xa0;

    QL_APP_LED_LOG("led_on!!!");
    timer_count = 0;

    if (lt_led_power_status_get() == LED_POWER_OFF)
    {
        lt_system_msg_handle(LT_SYS_LED_POWER_ON);
        ql_rtos_task_sleep_ms(100);
        i2c_1_Initialize();
        VK16K33_clean_data();
        ql_I2cWrite(LED_I2C, LED_SalveAddr, 0x21, &buf1, 1);
        ql_I2cWrite(LED_I2C, LED_SalveAddr, 0xa0, &buf2, 1);
        VK16K33_blink(1);
        // VK16K33_brightness(15);
        VK16K33_brightness(brightness_level);
        lt_led_power_status_set(LED_POWER_ON);
        lt_system_msg_handle(LT_SYS_EXTERNAL_POWER_ON);
    }
    else
    {
        ql_I2cWrite(LED_I2C, LED_SalveAddr, 0x21, &buf1, 1);
        ql_I2cWrite(LED_I2C, LED_SalveAddr, 0xa0, &buf2, 1);
        VK16K33_blink(1);
        // VK16K33_brightness(15);
        VK16K33_brightness(brightness_level);
    }
}
void led_off()
{
    QL_APP_LED_LOG("led_off!!!");
    VK16K33_blink(0);
    QL_APP_LED_LOG("led_off after LED_POWER_OFF!!!");
    lt_system_msg_handle(LT_SYS_LED_POWER_OFF); // 熄屏的时候关闭电源
    ql_rtos_task_sleep_ms(100);
    lt_led_power_status_set(LED_POWER_OFF);
    lt_system_msg_handle(LT_SYS_EXTERNAL_POWER_OFF);
}

void ql_i2c_led_thread(void *param)
{
    lt_system_msg_handle(LT_SYS_LED_POWER_ON);
    lt_led_power_status_set(LED_POWER_ON);
    ql_rtos_task_sleep_ms(100);

    i2c_1_Initialize();
    VK16K33_init();

    while (1)
    {
        if (volume_ctrl.show_volume != -1)
        {
            volume_ctrl.times--;
            if (volume_ctrl.times <= 0)
            {
                volume_ctrl.show_volume = -1;
            }
            else
            {
                led_on();
                lt_led_show_function(VOLUME, volume_ctrl.show_volume);
            }
            ql_rtos_task_sleep_ms(300);
            continue;
        }

        if (get_function_state() == FM_CHECK)
        {
            led_on();
            lt_led_show_function(get_function_state(), get_function_buf());
            ql_rtos_task_sleep_ms(100);
        }
        else
        {
            if (get_function_state() != RT_TIME)
            {
                led_on();
            }
            if (get_function_state() == RT_TIME)
            {
                if (timer_count < LED_OFF_BR)
                {
                    timer_count++;
                    QL_APP_LED_LOG("ql_i2c_led_thread");
                }
                else
                {
                    ql_rtos_task_sleep_ms(1000); // 低功耗模式
                    continue;
                }
            }
            lt_led_show_function(get_function_state(), get_function_buf());
            ql_rtos_task_sleep_ms(500);
        }
    }
}
ql_queue_t lt_led_msg_queue;
typedef struct lt_fm_msg
{
    int type;
    uint32_t datalen;
    uint8_t *data;
} lt_led_msg_t;
// 目前只有key的按键时间回调会需要异步的亮灭屏
void lt_panel_light_msg(int type, char *data, int data_len)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    lt_led_msg_t msg;

    msg.type = type;
    msg.datalen = data_len;
    msg.data = malloc(msg.datalen);
    if (msg.datalen != 0)
        memcpy(msg.data, data, msg.datalen);

    if ((err = ql_rtos_queue_release(lt_led_msg_queue, sizeof(lt_led_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
    {
        QL_APP_LED_LOG("send msg to lt_fm_msg_t failed, err=%d", err);
    }
    else
    {
        QL_APP_LED_LOG("send msg to lt_fm_msg_t success");
    }
}
// 处理非阻塞的亮灭屏操作
void lt_led_msg_thread(void *param)
{
    ql_rtos_task_sleep_s(3);
    ql_rtos_queue_create(&lt_led_msg_queue, sizeof(lt_led_msg_t), 5);
    while (1)
    {
        lt_led_msg_t msg;
        QL_APP_LED_LOG("led ql_rtos_queue_wait");
        ql_rtos_queue_wait(lt_led_msg_queue, (uint8 *)(&msg), sizeof(lt_led_msg_t), QL_WAIT_FOREVER);
        QL_APP_LED_LOG("led receive msg.");
        switch (msg.type)
        {
        case 0: // led_on
            led_on();
            break;
        case 1: // led_off
            break;
        default:
            break;
        }
        do
        {
            if (msg.data)
            {
                free((void *)msg.data);
                msg.data = NULL;
            }
        } while (0);
    }
}

void ql_i2c_led_init(void)
{
    QlI2CStatus err = QL_OSI_SUCCESS;
    ql_task_t i2c_task = NULL;
    set_function_state(RT_TIME, 0);
    // 注册音量变化回调
    ltapi_set_on_volume_changed(ltapi_on_volume_changed);

    err = ql_rtos_task_create(&i2c_task, QL_I2C_TASK_STACK_SIZE, QL_I2C_TASK_PRIO, "I2C LED", ql_i2c_led_thread, NULL, QL_I2C_TASK_EVENT_CNT);
    if (err != QL_OSI_SUCCESS)
    {
        QL_APP_LED_LOG("i2ctest1 led task created failed");
    }

    ql_task_t led_msg_task = NULL;
    err = ql_rtos_task_create(&led_msg_task, 2 * 1024, QL_I2C_TASK_PRIO, "LED_MSG", lt_led_msg_thread, NULL, QL_I2C_TASK_EVENT_CNT);
    if (err != QL_OSI_SUCCESS)
    {
        QL_APP_LED_LOG("ql_led_on_thread led task created failed");
    }
}
