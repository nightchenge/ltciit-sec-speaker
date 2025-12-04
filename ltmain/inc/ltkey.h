/*
 * @Author: zhouhao 
 * @Date: 2023-09-06 16:55:45 
 * @Last Modified by:   zhouhao 
 * @Last Modified time: 2023-09-06 16:55:45 
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


#ifndef _KEYDEMO_H
#define _KEYDEMO_H

#include "ql_gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Macro Definition
 ===========================================================================*/

/*===========================================================================
 * Struct
 ===========================================================================*/
// KEY1 打电话
// KEY2 短消息
// KEY3 FM
// KEY4 Music
// KEY5 一键通
//new
// KEY1 FM
// KEY2 亲情
// KEY3 短信
// KEY4 sos
// KEY5 开机
// KEY6 MP3
#define KEY1 GPIO_8
#define KEY2 GPIO_15
#define KEY3 GPIO_16
#define KEY4 GPIO_10
#define KEY5 GPIO_14
#define KEY6 GPIO_31
#define KEY99 0xFF

#define KEY1_PIN 84
#define KEY2_PIN 22
#define KEY3_PIN 68
#define KEY4_PIN 85
#define KEY5_PIN 23
#define KEY6_PIN 75


#define KEY1_FUN 1
#define KEY2_FUN 4
#define KEY3_FUN 0
#define KEY4_FUN 1
#define KEY5_FUN 1
#define KEY6_FUN 1






typedef struct
{
    ql_GpioNum      gpio_num;
    int             pin_num;
    int             gpio_fun;
    ql_TriggerMode  gpio_TriggerMode;
    ql_DebounceMode gpio_DebounceMod;
    ql_EdgeMode     gpio_EdgeMode;
    ql_PullMode     gpio_PullMode;
    void (*ql_key_callback)(void *ctx);
    void (*lt_key_click_callback)(void *ctx);
    void (*lt_key_doubleclick_callback)(void *ctx);
    void (*lt_key_longclick_callback)(void *ctx);
    void (*lt_key_repeat_callback)(void *ctx);   
} ql_key_cfg;

// KEY1 打电话
// KEY2 短消息
// KEY3 FM
// KEY4 Music
// KEY5 一键通
typedef struct lt_key_callback_s
{
    ql_GpioNum   key_gpio;//KEY1 KEY2 KEY3 KEY4 KEY5
    void (*lt_key_click_handle)(void *);
    void (*lt_key_doubleclick_handle)(void *);
    void (*lt_key_longclick_handle)(void *);
    void (*lt_key_repeat_callback)(void *);     
} lt_key_callback_t;

  typedef struct
  {
    void (*lt_key_click_handle)(void *);
    void (*lt_key_doubleclick_handle)(void *);
    void (*lt_key_longclick_handle)(void *);
    void (*lt_key_repeat_callback)(void *);
  } KeyOriginalCallbacks;


// static void lt_audio_callback_register()
// {
//     lt_key_callback_t reg;
//     reg.key_gpio                    = KEY4;
//     reg.lt_key_click_handle         = ql_key_mp3_next;
//     reg.lt_key_doubleclick_handle   = ql_key_mp3_next;
//     reg.lt_key_longclick_handle     = ql_key_mp3_callback;
//     lt_key_callback_register(&reg);
// } //回调参考上溯例子
//void lt_key_callback_register(lt_key_callback_t *reg);
void lt_key_callback_register(char* name,lt_key_callback_t *reg);
/*===========================================================================
 * Functions declaration
 ===========================================================================*/
void ql_key_app_init(void);

void terminate_all_temp_callbacks();
void set_all_temp_key_callbacks(KeyOriginalCallbacks *temp);


#ifdef __cplusplus
} /*"C" */
#endif

#endif /* _GPIODEMO_H */


