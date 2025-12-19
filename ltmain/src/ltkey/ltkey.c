/*
 * @Author: mikey.zhaopeng 
 * @Date: 2023-09-06 15:07:55 
 * @Last Modified by: zhouhao
 * @Last Modified time: 2023-09-06 17:21:12
 */

/*================================================================
  Copyright (c) 2021, Quectel Wireless Solutions Co., Ltd. All rights reserved.
  Quectel Wireless Solutions Proprietary and Confidential.
=================================================================*/

/*=================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

WHEN			  WHO		  WHAT, WHERE, WHY
------------	 -------	 -------------------------------------------------------------------------------

=================================================================*/

/*===========================================================================i
 * include files
 ===========================================================================*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_api_osi.h"
#include "ql_log.h"
#include "ql_pin_cfg.h"

#include "ql_fs.h"
#include "ltkey.h"
#include "ltkeydrv.h"
#include "lt_tts_demo.h"

#include "ql_api_rtc.h"
#include "led.h"
#include "ltsystem.h"
//extern void ltapi_play_tts(char *data, int data_len);

/*===========================================================================
 * Macro Definition
 ===========================================================================*/
#define QL_KEYDEMO_LOG_LEVEL QL_LOG_LEVEL_INFO
#define QL_keyDEMO_LOG(msg, ...) QL_LOG(QL_KEYDEMO_LOG_LEVEL, "ql_ltkey", msg, ##__VA_ARGS__)
#define QL_keyDEMO_LOG_PUSH(msg, ...) QL_LOG_PUSH("ql_ltkey", msg, ##__VA_ARGS__)

#if PACK_TYPE == PACK_OLD
#define KEY_FM KEY3
#define KEY_AUDIO KEY4
#define KEY_SMS KEY5
#define KEY_VOICECALL KEY1
#define KEY_SOSCALL KEY2
#define KEY_WATERCALL KEY99
#define KEY_GASCALL KEY99
#define KEY_ELECTRICCALL KEY99
#define KEY_ADC_VOLUP KEY99
#define KEY_ADC_VOLDOWN KEY99
#define KEY_SHUTDOWM  KEY5
#define KEY_RECORD  KEY99
#elif PACK_TYPE == PACK_LOOTOM
#define KEY_FM KEY1
#define KEY_AUDIO KEY6
#define KEY_SMS KEY3
#define KEY_VOICECALL KEY2
#define KEY_SOSCALL KEY4
#define KEY_WATERCALL KEY99
#define KEY_GASCALL KEY99
#define KEY_ELECTRICCALL KEY99
#define KEY_ADC_VOLUP KEY2
#define KEY_ADC_VOLDOWN KEY3
#define KEY_SHUTDOWM  KEY5
#define KEY_RECORD  KEY99

#elif PACK_TYPE == PACK_NANJING
#define KEY_FM KEY4
#define KEY_AUDIO KEY99
#define KEY_SMS KEY99
#define KEY_VOICECALL KEY99
#define KEY_SOSCALL KEY5
#define KEY_WATERCALL KEY1
#define KEY_GASCALL KEY2
#define KEY_ELECTRICCALL KEY3
#define KEY_ADC_VOLUP KEY99
#define KEY_ADC_VOLDOWN KEY99
#define KEY_SHUTDOWM  KEY5
#define KEY_RECORD  KEY99

#endif

void pt();



void traverse_path() {
	QDIR *dir = NULL;
	qdirent *fp = NULL;
	if(!(dir = ql_opendir("SD:"))){
        QL_keyDEMO_LOG("dir NULL;");
        return;
	}
	while(1){
        fp = ql_readdir(dir);
        if(fp==NULL)
        {
            QL_keyDEMO_LOG("file NULL;");
           break; 
        }
        QL_keyDEMO_LOG("file: type=%d,name=%s",fp->d_ino, fp->d_name);
	}
	ql_closedir(dir);
}


void ql_key_det_callback(void *ctx)
{
        char *tts_str1 = "播放音乐";
        char *tts_str2 = "播放短消息";
        char *tts_str3 = "正在拨打电话";
        char *tts_str4 = "播放收音机";
        char *tts_str5 = "正在一键求助";
    ql_key_cfg *cfg = (ql_key_cfg *)ctx;
    switch (cfg->pin_num)
    {
    case 84 /* constant-expression */:
        ltapi_play_tts(tts_str3, strlen(tts_str3));
        break;
    case 22 /* constant-expression */:
        ltapi_play_tts(tts_str2, strlen(tts_str2));
        break;
    case 68 /* constant-expression */:
        ltapi_play_tts(tts_str4, strlen(tts_str4));
        break;
    case 85 /* constant-expression */:
        ltapi_play_tts(tts_str1, strlen(tts_str1));
        break;
    case 23 /* constant-expression */:
        ltapi_play_tts(tts_str5, strlen(tts_str5));
        break;

    default:
        break;
    }
}


void ql_key_callback(void *ctx)
{
    pt();
  //  if (0 == lt_led_show_onoff()) // 如果按键的第一下屏幕是灭的话 直接先唤醒
   // {
       //led_on();
        char *len_on = "len_on";
        lt_panel_light_msg(0, len_on, strlen(len_on) + 1);
    //}
     QL_keyDEMO_LOG("ql_key_callback");

}
//static MyKeyHandle key1, key2, key3, key4, key5;//key 定义
static MyKeyHandle key_handle[6];

static ql_key_cfg _ql_key_cfg[] =
{

        /* gpio_num pin_num  ql_TriggerMode ql_DebounceMode ql_EdgeMode ql_PullMode  ql_key_callback  lt_key_click_handle_callback lt_key_longclick_handle_callback */

        {KEY1, KEY1_PIN, KEY1_FUN, EDGE_TRIGGER, DEBOUNCE_EN, EDGE_FALLING, PULL_DOWN, ql_key_callback, NULL, NULL, NULL,NULL},
        {KEY2, KEY2_PIN, KEY2_FUN, EDGE_TRIGGER, DEBOUNCE_EN, EDGE_FALLING, PULL_DOWN, ql_key_callback, NULL, NULL, NULL,NULL},
        {KEY3, KEY3_PIN, KEY3_FUN, EDGE_TRIGGER, DEBOUNCE_EN, EDGE_FALLING, PULL_DOWN, ql_key_callback, NULL, NULL, NULL,NULL},
        {KEY4, KEY4_PIN, KEY4_FUN, EDGE_TRIGGER, DEBOUNCE_EN, EDGE_FALLING, PULL_DOWN, ql_key_callback, NULL, NULL, NULL,NULL},
        {KEY5, KEY5_PIN, KEY5_FUN, EDGE_TRIGGER, DEBOUNCE_EN, EDGE_FALLING, PULL_DOWN, ql_key_callback, NULL, NULL, NULL,NULL},
#if PACK_TYPE == PACK_LOOTOM
        {KEY6, KEY6_PIN, KEY6_FUN, EDGE_TRIGGER, DEBOUNCE_EN, EDGE_FALLING, PULL_DOWN, ql_key_callback, NULL, NULL, NULL,NULL},
#endif

};

void pt()
{
    uint16_t num;
    ql_LvlMode gpio_lvl;
    for (num = 0; num < sizeof(_ql_key_cfg) / sizeof(_ql_key_cfg[0]); num++)
    {
        ql_gpio_get_level(_ql_key_cfg[num].gpio_num, &gpio_lvl);
        
    }
}

void lt_key_callback_register(char* name,lt_key_callback_t *reg)
{
    if (name == NULL)
    {
        return;
    }
    if (strlen(name) != 0)
    {
        if (strcmp(name, "key_fm") == 0)
        {
            reg->key_gpio = KEY_FM;
        }
        else if (strcmp(name, "key_audio") == 0)
        {
            reg->key_gpio = KEY_AUDIO;
        }
        else if (strcmp(name, "key_sms") == 0)
        {
            reg->key_gpio = KEY_SMS;
        }
        else if (strcmp(name, "key_voicecall") == 0)
        {
            reg->key_gpio = KEY_VOICECALL;
        }
        else if (strcmp(name, "key_soscall") == 0)
        {
            reg->key_gpio = KEY_SOSCALL;
        }
        else if (strcmp(name, "key_watercall") == 0)
        {
            reg->key_gpio = KEY_WATERCALL;
        }
        else if (strcmp(name, "key_gascall") == 0)
        {
            reg->key_gpio = KEY_GASCALL;
        }
        else if (strcmp(name, "key_electriccall") == 0)
        {
            reg->key_gpio = KEY_ELECTRICCALL;
        }
        else if (strcmp(name, "key_adc_volup") == 0)
        {
            reg->key_gpio = KEY_ADC_VOLUP;
        }
        else if (strcmp(name, "key_adc_voldown") == 0)
        {
            reg->key_gpio = KEY_ADC_VOLDOWN;
        }
        else if (strcmp(name, "key_shutdown") == 0)
        {
            reg->key_gpio = KEY_SHUTDOWM;
        }
        else if (strcmp(name, "key_record") == 0)
        {
            reg->key_gpio = KEY_RECORD;
        }        
        else
        {
            reg->key_gpio = KEY99; // 默认返回 KEY99 表示无效
        }
    }
    if(reg->key_gpio == KEY99 )
    {
       QL_keyDEMO_LOG(" %s key cant support",name); 
       return ;
    }

    uint16_t num=0;
    for (num = 0; num < sizeof(_ql_key_cfg) / sizeof(_ql_key_cfg[0]); num++)
    {
        if(reg->key_gpio == _ql_key_cfg[num].gpio_num)
        {
            if(reg->lt_key_click_handle)
                _ql_key_cfg[num].lt_key_click_callback          =   reg->lt_key_click_handle;
            if(reg->lt_key_doubleclick_handle)
                _ql_key_cfg[num].lt_key_doubleclick_callback    =   reg->lt_key_doubleclick_handle;
            if(reg->lt_key_longclick_handle)
                _ql_key_cfg[num].lt_key_longclick_callback      =   reg->lt_key_longclick_handle;     
            if(reg->lt_key_repeat_callback)
                _ql_key_cfg[num].lt_key_repeat_callback      =   reg->lt_key_repeat_callback;                     
            break;                           
        }
    }
}
/*===========================================================================
 * Functions
 ===========================================================================*/

//注册按键回调函数
int GetKeyStatus_key(int gpio_num)
{
    ql_LvlMode gpio_lvl;

    ql_gpio_get_level(gpio_num, &gpio_lvl);
    return !gpio_lvl;
}
time_t my_mktime(ql_rtc_time_t *timeinfo) {
    // 简化实现，不考虑时区和夏令时
    time_t timestamp = 0;

    // 计算年份对应的秒数
    for (int year = 1970; year < timeinfo->tm_year; year++) {
        timestamp += (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 31622400 : 31536000;
    }

    // 计算月份对应的秒数
    int days_in_month[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int month = 1; month < timeinfo->tm_mon + 1; month++) {
        timestamp += days_in_month[month] * 86400;
    }

    // 计算日期对应的秒数
    timestamp += (timeinfo->tm_mday - 1) * 86400;

    // 计算小时、分钟和秒对应的秒数
    timestamp += timeinfo->tm_hour * 3600 + timeinfo->tm_min * 60 + timeinfo->tm_sec;

    return timestamp;
}

double my_difftime(time_t time1, time_t time2) {
    // 简化实现，返回秒数差值
    return (double)(time1 - time2);
}


//判断时间，当前设置未2024年5月21日10点00

int check_time() {
    // 获取当前时间
    return 1;
    ql_rtc_time_t tm;
    ql_rtc_set_timezone(32);    //UTC+32
    ql_rtc_get_localtime(&tm);
   
       QL_keyDEMO_LOG("key_show is %d  %d  %d  %d:%d:%d", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec); 
   // ql_rtc_get_time(&tm);
    // 设置目标时间为2023年11月21号9点半
    ql_rtc_time_t targetTime ;
    targetTime.tm_year = 2024; // 年份从1900开始
    targetTime.tm_mon = 5 ;       // 月份从0开始
    targetTime.tm_mday = 21;
    targetTime.tm_hour = 10;
    targetTime.tm_min = 0;
    targetTime.tm_sec = 0;
    // 比较当前时间和目标时间
    if (my_difftime(my_mktime(&tm), my_mktime(&targetTime)) > 0) {
      //  printf("1\n");
        return 0;
    } else {
        return 1;
    }

    return 0;
}

// 定义临时回调结构体，去掉定时器部分
typedef struct
{
    KeyOriginalCallbacks original; // 原始回调
} TempKeyCallback;

// 定义临时回调数组
static TempKeyCallback temp_callbacks[sizeof(_ql_key_cfg) / sizeof(_ql_key_cfg[0])];

// 封装恢复原始回调的函数
static void restore_original_callbacks(uint16_t num)
{
    printf("restore_original_callbacksrestore_original_callbacks\n");
    _ql_key_cfg[num].lt_key_click_callback = temp_callbacks[num].original.lt_key_click_handle;
    _ql_key_cfg[num].lt_key_doubleclick_callback = temp_callbacks[num].original.lt_key_doubleclick_handle;
    _ql_key_cfg[num].lt_key_longclick_callback = temp_callbacks[num].original.lt_key_longclick_handle;
    _ql_key_cfg[num].lt_key_repeat_callback = temp_callbacks[num].original.lt_key_repeat_callback;
}

// 临时状态设置函数，去掉定时器相关操作
void set_temp_key_callback(int pin_num, KeyOriginalCallbacks *temp)
{
    uint16_t num = 0;
    for (num = 0; num < sizeof(_ql_key_cfg) / sizeof(_ql_key_cfg[0]); num++)
    {
        if (_ql_key_cfg[num].pin_num == pin_num)
        {
            // 保存原始回调
            temp_callbacks[num].original.lt_key_click_handle = _ql_key_cfg[num].lt_key_click_callback;
            temp_callbacks[num].original.lt_key_doubleclick_handle = _ql_key_cfg[num].lt_key_doubleclick_callback;
            temp_callbacks[num].original.lt_key_longclick_handle = _ql_key_cfg[num].lt_key_longclick_callback;
            temp_callbacks[num].original.lt_key_repeat_callback = _ql_key_cfg[num].lt_key_repeat_callback;
            // memcpy(temp_callbacks[num].original, &_ql_key_cfg[num], sizeof(lt_key_callback_t));

            // 设置临时回调
            if (temp->lt_key_click_handle)
            {
                printf("set_temp_key_callbackset_temp_key_callback!!!\n");
                _ql_key_cfg[num].lt_key_click_callback = temp->lt_key_click_handle;
            }

            if (temp->lt_key_doubleclick_handle)
                _ql_key_cfg[num].lt_key_doubleclick_callback = temp->lt_key_doubleclick_handle;
            if (temp->lt_key_longclick_handle)
                _ql_key_cfg[num].lt_key_longclick_callback = temp->lt_key_longclick_handle;
            break;
        }
    }
}

// 设置所有按键的临时状态，去掉定时器相关操作
void set_all_temp_key_callbacks(KeyOriginalCallbacks *temp)
{
    uint16_t num = 0;
    for (num = 0; num < sizeof(_ql_key_cfg) / sizeof(_ql_key_cfg[0]); num++)
    {
        set_temp_key_callback(_ql_key_cfg[num].pin_num, temp);
    }
}

// 提前终止并恢复回调，去掉定时器相关操作
void terminate_all_temp_callbacks()
{
    uint16_t num = 0;
    for (num = 0; num < sizeof(_ql_key_cfg) / sizeof(_ql_key_cfg[0]); num++)
    {
        restore_original_callbacks(num);
    }
}

// 按键事件处理函数
static void KeyProcess_handle(ql_GpioNum gpio_num, size_t KeyEvent, size_t KeyClickCount)
{
    int num = 0 ;
    if(!check_time())
    {
        QL_keyDEMO_LOG("time is over");
        return ;
    }

    for (num = 0; num < sizeof(_ql_key_cfg) / sizeof(_ql_key_cfg[0]); num++)
    {
        if (_ql_key_cfg[num].gpio_num == gpio_num)
        {
            switch (KeyEvent)
            {
            case MYKEY_EVENT_CLICK:
            {
                // 按键单击处理
                QL_keyDEMO_LOG("key%d click, count %d\r\n",num+1, KeyClickCount);
                if(_ql_key_cfg[num].lt_key_click_callback)
                {
                    _ql_key_cfg[num].lt_key_click_callback(NULL);
                }
            }
            break;
            case MYKEY_EVENT_DBLCLICK:
            {
                // 按键双击处理
                QL_keyDEMO_LOG("key%d double click, count %d\r\n",num+1, KeyClickCount);
                if(_ql_key_cfg[num].lt_key_doubleclick_callback)
                {
                    _ql_key_cfg[num].lt_key_doubleclick_callback(NULL);
                }
            }
            break;
            case MYKEY_EVENT_LONG_PRESS:
            {
                // 按键长按处理
                QL_keyDEMO_LOG("key%d long press, count %d\r\n",num+1, KeyClickCount);
                if(_ql_key_cfg[num].lt_key_longclick_callback)
                {
                    _ql_key_cfg[num].lt_key_longclick_callback(NULL);
                }
            }
            break;
            case MYKEY_EVENT_REPEAT:
            {
                // 按键连续触发处理
                QL_keyDEMO_LOG("key%d repeat, count %d\r\n",num+1, KeyClickCount);
                {
                if(_ql_key_cfg[num].lt_key_repeat_callback)
                    _ql_key_cfg[num].lt_key_repeat_callback(&KeyClickCount);
                }

            }
            break;
            case MYKEY_EVENT_RELASE:
            {
                QL_keyDEMO_LOG("key%d relase\r\n",num);
            }
            break;
            default:
                QL_keyDEMO_LOG("undefine key message\r\n");
                break;
            }
            break;
        }
    }
}

// 主循环调用，从按键消息队列中取出消息并进行处理
void KeyProcess(void)
{
    // printf("start Keyprocess!!\n");
    MyKeyHandle KeyID;
    unsigned char KeyEvent;
    unsigned char KeyCliclCount;
    while (MyKey_Read(&KeyID, &KeyEvent, &KeyCliclCount) == 0)
    {
        uint16_t num;
        for (num = 0; num < sizeof(_ql_key_cfg) / sizeof(_ql_key_cfg[0]); num++)
        {
            if (KeyID == key_handle[num])
            {
                // if(0 == lt_led_show_onoff())//如果按键的第一下屏幕是灭的话 直接先唤醒
                // {
                //     led_on();
                // }
                // else
                KeyProcess_handle(_ql_key_cfg[num].gpio_num,KeyEvent, KeyCliclCount);
                break;
            }
        }
    }
}

void lt_key_demo_init(void)
{
    uint16_t num;
    ql_GpioDir gpio_dir;
    for (num = 0; num < sizeof(_ql_key_cfg) / sizeof(_ql_key_cfg[0]); num++)
    {
        ql_pin_set_func(_ql_key_cfg[num].pin_num, _ql_key_cfg[num].gpio_fun); // Pin reuse
        gpio_dir = GPIO_INPUT;
        ql_gpio_get_direction(_ql_key_cfg[num].gpio_num, &gpio_dir);
        QL_keyDEMO_LOG("key %d dir %d", _ql_key_cfg[num].gpio_num, gpio_dir);
        ql_gpio_set_direction(_ql_key_cfg[num].gpio_num, GPIO_INPUT);

        if (QL_GPIO_SUCCESS != ql_int_register(_ql_key_cfg[num].gpio_num, _ql_key_cfg[num].gpio_TriggerMode, _ql_key_cfg[num].gpio_DebounceMod, _ql_key_cfg[num].gpio_EdgeMode, _ql_key_cfg[num].gpio_PullMode, _ql_key_cfg[num].ql_key_callback, &_ql_key_cfg[num]))
        {
            QL_keyDEMO_LOG("det init reg err");
        }
        else
        {
              QL_keyDEMO_LOG("key %d dir %d", _ql_key_cfg[num].gpio_num, gpio_dir);
            ql_int_enable(_ql_key_cfg[num].gpio_num);   
            ql_rtos_task_sleep_ms(100);
            //注册event
            if (MyKey_Register(&key_handle[num], _ql_key_cfg[num].gpio_num, GetKeyStatus_key, MYKEY_EVENT_CLICK | MYKEY_EVENT_DBLCLICK | MYKEY_EVENT_LONG_PRESS | MYKEY_EVENT_REPEAT, MYKEY_EVENT_REPEAT_TIME, MYKEY_EVENT_LONG_PRESS_TIME) == 0)
            {
                QL_keyDEMO_LOG("key%d register successed\r\n",num);
            }
            else
            {
                QL_keyDEMO_LOG("key%d register faild\r\n",num);  
            }
        }
    }
}


static void ql_key_scan_event_thread(void *param)
{
    QL_keyDEMO_LOG("ql_key_scan_event_thread enter, param 0x%x", param);
    while (1)
    {
 //       ql_rtos_task_sleep_ms(10000);
        if (IDLE_MODE == ltget_lp_status()) // 如果按键的第一下屏幕是灭的话 直接先唤醒
        {
            // QL_keyDEMO_LOG("ql_key_scan_event_thread enter, param 0x%x", param);
            MyKey_Scan(10);
            ql_rtos_task_sleep_ms(10);
        }else
        {
           ql_rtos_task_sleep_ms(2000); 
        }
        // MyKey_Scan(10);
        // ql_rtos_task_sleep_ms(10);
    }
      ql_rtos_task_delete(NULL);
}
//ql_queue_t lt_key_msg_queue;
static void ql_key_handel_event_thread(void *param)
{
    QL_keyDEMO_LOG("ql_key_handel_event_thread enter, param 0x%x", param);
    while(1)
    {
        if (IDLE_MODE == ltget_lp_status()) // 如果按键的第一下屏幕是灭的话 直接先唤醒
        {
            KeyProcess();
            ql_rtos_task_sleep_ms(1);
        }else
        {
           ql_rtos_task_sleep_ms(2000); 
        }

    }


    // ql_rtos_queue_create(&lt_key_msg_queue, sizeof(myKeyMsg_t), 10);

    // while (1)
    // {
    //     myKeyMsg_t msg;
    //     QL_keyDEMO_LOG("key ql_rtos_queue_wait");
    //     ql_rtos_queue_wait(lt_key_msg_queue, (uint8 *)(&msg), sizeof(myKeyMsg_t), 0xFFFFFFFF);
    //     QL_keyDEMO_LOG("key receive msg.");
    //     // unsigned char KeyEvent;
    //     // unsigned char KeyCliclCount;
    //     uint16_t num;
    //     for (num = 0; num < sizeof(_ql_key_cfg) / sizeof(_ql_key_cfg[0]); num++)
    //     {
    //         if (msg.KeyID == key_handle[num])
    //         {
    //             if (0 == lt_led_show_onoff()) // 如果按键的第一下屏幕是灭的话 直接先唤醒
    //             {
    //                 led_on();
    //             }
    //             else
    //                 KeyProcess_handle(_ql_key_cfg[num].gpio_num, msg.KeyEvent, msg.KeyCliclCount);
    //             break;
    //         }
    //     }

    // }

    ql_rtos_task_delete(NULL);
}
void ql_key_app_init(void)
{
    QlOSStatus err = QL_OSI_SUCCESS;

    MyKey_Init();//key event init
    lt_key_demo_init();

    ql_task_t key_scan_event_task = NULL;
    err = ql_rtos_task_create(&key_scan_event_task, 10240, APP_PRIORITY_NORMAL, "key_scan_event_task", ql_key_scan_event_thread, NULL, 1);
    if (err != QL_OSI_SUCCESS)
    {
        QL_keyDEMO_LOG("key_butt_tasktask created failed");
    }

    ql_task_t key_process_event_task = NULL;
    err = ql_rtos_task_create(&key_process_event_task, 10240, APP_PRIORITY_NORMAL, "key_process_event_task", ql_key_handel_event_thread, NULL, 1);
    if (err != QL_OSI_SUCCESS)
    {
        QL_keyDEMO_LOG("key_process_task task created failed");
    }

    
 //   key_button_init();

}
