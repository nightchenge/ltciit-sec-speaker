/*
 * @Descripttion:
 * @version:
 * @Author: zhouhao
 * @Date: 2023-09-18 15:57:33
 * @LastEditors: ubuntu
 * @LastEditTime: 2025-12-04 15:45:50
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_api_common.h"
#include "ql_api_osi.h"
#include "ql_api_sms.h"
#include "ql_api_rtc.h"
#include "ql_log.h"
#include "ltkey.h"
#include "ql_api_voice_call.h"
#include "ltsms_t.h"
#include "ltkey.h"
#include "ltplay.h"
#include "lt_tts_demo.h"
#include "led.h"
#include "ebm_task_play.h"
#include "ltaudio.h"
#include "ltuart2frx8016.h"

#include "ltsystem.h"
#define TTS_MAX_INSTRUCTION_LEN 1024
#define TTS_MAX_INSTRUCTION_CNT 10
static bool sim_sms=FALSE;
//获取ms时间戳
uint64_t tts_instruct_gettime(void)
{
    ql_timeval_t time;

    memset(&time, 0, sizeof(ql_timeval_t));
    ql_gettimeofday(&time);
    return ((uint64_t)time.sec * 1000 + time.usec / 1000);
}

typedef struct tts_play_instruct_s
{
    int type;           // 0-sms 1-ebs
    uint64_t timestamp; // ms级别时间戳 唯一标识
    uint32_t datalen;
    uint8_t *data;
    void (*instruct_play_handle)(void *buf,int date_len);
    void (*instruct_stop_handle)(void *buf,int date_len);    
} tts_play_instruct_t;

typedef struct tts_play_lst_s
{
    int used;
    tts_play_instruct_t instruct;
} tts_play_lst_t;


static tts_play_lst_t g_tts_play_lst[TTS_MAX_INSTRUCTION_CNT] = {0}; //初始化tts播放队列默认大小10
static tts_play_instruct_t *play_instruct = NULL;  //当前在播的通道
/**
 * @name: 
 * @description:  添加tts播放队列
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
static int tts_instruct_add(tts_play_instruct_t *instruct)
{
    if (instruct)
    {

        int i = 0;
        for (i = 0; i < TTS_MAX_INSTRUCTION_CNT; i++)
        {
            if (g_tts_play_lst[i].instruct.timestamp == instruct->timestamp)
                break;
        }

        if (TTS_MAX_INSTRUCTION_CNT == i)
        { /* not match */
            for (i = 0; i < TTS_MAX_INSTRUCTION_CNT; i++)
            {
                if (!g_tts_play_lst[i].used)
                    break;
            }
        }

        if (TTS_MAX_INSTRUCTION_CNT == i)
        { /* full */
            QL_SMS_LOG("tts struction list is full!\n");
            return -1;
        }

        memcpy(&g_tts_play_lst[i].instruct, instruct, sizeof(tts_play_instruct_t));
        g_tts_play_lst[i].used = 1;
    }

    return 0;
}

/**
 * @name: 
 * @description: 查找队列中tts
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
// static tts_play_instruct_t *tts_instruct_find(uint64_t timestamp)
// {
//     int i = 0;
    
//     for (i = 0; i < TTS_MAX_INSTRUCTION_CNT; i++)
//     {
//         if (g_tts_play_lst[i].used && g_tts_play_lst[i].instruct.timestamp != timestamp)
//         {
//             return &g_tts_play_lst[i].instruct;
//         }
//     }

//     return NULL;
// }
/**
 * @name: 
 * @description: 删除队列中的tts
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
static int tts_instruct_del(tts_play_instruct_t *instruct)
{
    int i = 0;

    for (i = 0; i < TTS_MAX_INSTRUCTION_CNT; i++)
    {
        if (g_tts_play_lst[i].instruct.timestamp == instruct->timestamp)
        {
            g_tts_play_lst[i].used = 0;
            if(g_tts_play_lst[i].instruct.data)
            {
                free(g_tts_play_lst[i].instruct.data);
                QL_SMS_LOG("free instruct.data\n");
            }
            g_tts_play_lst[i].instruct.instruct_play_handle =NULL;
            g_tts_play_lst[i].instruct.instruct_stop_handle =NULL;
        }

    }
    return 0;
}

/**
 * @name: 
 * @description: 获取当前播放队列中优先级别最高的tts
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
static tts_play_instruct_t *tts_instruct_get()
{
    tts_play_instruct_t *instruct = NULL;
    int i = 0;

    for (i = 0; i < TTS_MAX_INSTRUCTION_CNT; i++)
    {
        if (!g_tts_play_lst[i].used)
        {
            continue;
        }
        else if (!instruct)
        {
            instruct = &g_tts_play_lst[i].instruct;
        }
        else if (g_tts_play_lst[i].instruct.timestamp < instruct->timestamp)
        { // 判断时间
            instruct = &g_tts_play_lst[i].instruct;
        }
        else if (g_tts_play_lst[i].instruct.timestamp == instruct->timestamp)
        { // 时间一致 判断类型 EBS高于SMS 可调整
            if (g_tts_play_lst[i].instruct.type > instruct->type)
            {
                instruct = &g_tts_play_lst[i].instruct;
            }
        }
    }
    return instruct;
}

/**
 * @name: 
 * @description: 播放下一个tts
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
static void ql_key_tts_next()
{  
    
    if (ltplay_get_src() == SND_TEL || ltplay_get_src() == SND_EBS)
        return;


    play_instruct = tts_instruct_get();
    if (play_instruct)
    {
         ltplay_check_play(SND_SMS);
        QL_SMS_LOG("sms have imessage!!");
        set_blink("sms",1);
        set_function_state(YL, 0);
        if( TTS_TYPE_SMS==play_instruct->type  )
        {
            sim_sms =  TRUE;
          //  ltapi_play_tts_encoding((char *)play_instruct->data, play_instruct->datalen, QL_TTS_GBK);
            ltapi_play_tts_withcallback((char *)play_instruct->data, play_instruct->datalen, QL_TTS_GBK, NULL, lt_tts_instruct_stop);
        }else
        {
            if(play_instruct->instruct_play_handle)
            {
                play_instruct->instruct_play_handle((char *)play_instruct->data,play_instruct->datalen);
            }
        }
    }
    else
    {
	    //ltplay_check_play(SND_SMS);
	    ltapi_play_tts(TTS_STR_SMS_NONE_UNREAD, strlen(TTS_STR_SMS_NONE_UNREAD));
        QL_SMS_LOG("sms have no  imessage!!");
    }
}
/**
 * @name: 
 * @description: stop
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
static void ql_key_tts_stop()
{
    sim_sms =  FALSE;
    if(ltplay_get_src() == SND_EBS)
    {
         lt_audio_pa_disable();
        QL_SMS_LOG("eb_player_current_task_delete!!!");
      //  eb_player_current_task_delete();
       eb_player_all_task_delete();
         ql_rtos_task_sleep_s(3);
         return ;
        //eb_player_all_task_delete();
    }  
    if (SND_SMS != ltplay_get_src())
    {
        QL_SMS_LOG("sms not in play;exit");
        return;
    }
  //  set_function_state(RT_TIME, 0);
    ltplay_check_play(SND_STOP);
    if(play_instruct)
    {
        if(TTS_TYPE_SMS ==play_instruct->type )
        {
            ltapi_stop_tts_encoding();
        }else
        {
            if(play_instruct->instruct_stop_handle)
            {
                lt_audio_pa_disable();
                play_instruct->instruct_stop_handle((char *)play_instruct->data,play_instruct->datalen);
                ql_rtos_task_sleep_s(3);
            }
        }
        tts_instruct_del(play_instruct);
        play_instruct=NULL;
    }
    
    QL_SMS_LOG("ql_key_tts_stop!!");
    set_blink("sms",0);
    set_function_state(RT_TIME, 0);
      
}

#if PACK_TYPE != PACK_NANJING
static void ql_key_tts_click()
{

    if (ltplay_get_src() == SND_TEL)
        return;
    if (ltplay_get_src() == SND_SMS || ltplay_get_src() == SND_EBS)
    {
        ql_key_tts_stop();
        return;
    }

    play_instruct = tts_instruct_get(); 
    if (play_instruct)
    {
        ltplay_check_play(SND_SMS);
   //     ql_rtos_task_sleep_ms(1500);
        QL_SMS_LOG("sms have imessage!!");
        set_blink("sms", 1);
        set_function_state(YL, 0);
        if (TTS_TYPE_SMS == play_instruct->type)
        {
            sim_sms = TRUE;
            // ltplay_check_play(SND_SMS);
            // set_function_state(YL, 0);
            //  ltapi_play_tts_encoding((char *)play_instruct->data, play_instruct->datalen, QL_TTS_GBK);
            ql_rtos_task_sleep_s(1);
            ltapi_play_tts_withcallback((char *)play_instruct->data, play_instruct->datalen, QL_TTS_GBK, NULL, lt_tts_instruct_stop);
        }
        else
        {
            if (ltplay_get_src() == SND_MP3_URL || ltplay_get_src() == SND_FM_URL)
            {
                eb_player_all_task_delete();
                // ql_rtos_task_sleep_s(3);
            }
            if (play_instruct->instruct_play_handle)
            {
                play_instruct->instruct_play_handle((char *)play_instruct->data, play_instruct->datalen);
            }
        }
    }
    else
    {
	    //ltplay_check_play(SND_SMS);
        if(ltplay_get_src() == SND_STOP)
        {
	        ltapi_play_tts(TTS_STR_SMS_NONE_UNREAD, strlen(TTS_STR_SMS_NONE_UNREAD));
        }
        QL_SMS_LOG("sms have no  imessage!!");
    }
}
#endif

/**
 * @name: 
 * @description: check_tts_state
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
static void ql_tts_ckeck()
{
    tts_play_instruct_t *instruct = NULL;
    instruct = tts_instruct_get();
    if (instruct)
    {
        QL_SMS_LOG("sms have imessage!!");
        if(SND_SMS != ltplay_get_src() )
        {
        #if PACK_TYPE != PACK_NANJING
            //led_on();
            set_blink("sms",2);
        #endif
        }
        QL_SMS_LOG("imessage blink!!");
       
    }
    else
    {
       // QL_SMS_LOG("sms have no  imessage!!");
        //TODO 执行灭灯操作
    }
}
int ql_tts_ckeck_reboot()
{
    tts_play_instruct_t *instruct = NULL;
    instruct = tts_instruct_get();
    if (instruct)
    {
        QL_SMS_LOG("sms haveasdasdasd imessage!!");
        return 1;
       
    }
    else
    {
        QL_SMS_LOG("sms haveasdasd no  imessage!!");
       return 0;
        //TODO 执行灭灯操作
    }
}
bool ql_tts_check_sim_sms()
{
    return sim_sms;
}

//外部任务自动停止后调用的接口
void lt_tts_instruct_stop(int type)
{
    if(play_instruct)
    {
        if(type == play_instruct->type)
        {
        
            ql_key_tts_stop();
        }
        else{
            QL_SMS_LOG("now tts play_type is different!!");
        }
    }
}

void lt_tts_new_info()
{
    QL_SMS_LOG("lt_tts_new_info");
    #if PACK_TYPE != PACK_NANJING
	ql_rtc_time_t tm;
	ql_rtc_get_localtime(&tm);
	// 获取当前时间
	int currentHour = tm.tm_hour; // 当前小时
	if ((currentHour >= 21 && currentHour < 24) || (currentHour >= 0 && currentHour <= 6))
	{
        QL_SMS_LOG("tts  time wrong!");
    }
    else
    {
        if (IDLE_MODE != ltget_lp_status()) // 如果按键的第一下屏幕是灭的话 直接先唤醒
        {
            ql_rtos_task_sleep_s(2);
        }
        ltapi_play_tts(TTS_STR_SMS_NEW_MSG, strlen(TTS_STR_SMS_NEW_MSG));
    }

#endif
	//ql_rtos_task_sleep_s(10);
}
//外部有任务时加入队列的接口
/**
 * @name: 
 * @description: 
 * @param {int type, char *data, int datelen, void (*start_handle)(char *buf, int date_len), void (*stop_handle)(char *buf, int date_len)}
 * @return {*}
 * @author: zhouhao
 */
int lt_tts_instruct_add(int type, void *data, int datelen, void (*start_handle)(void *buf, int date_len), void (*stop_handle)(void *buf, int date_len))
{
    #if PACK_TYPE == PACK_NANJING
        return 1;
    #endif
    tts_play_instruct_t instruct;
    instruct.type = type;
    instruct.timestamp = tts_instruct_gettime();
    instruct.datalen = datelen;
    instruct.data = malloc(instruct.datalen);
    memcpy(instruct.data, data, instruct.datalen);
    if (start_handle)
        instruct.instruct_play_handle = start_handle;
    if (stop_handle)
        instruct.instruct_stop_handle = stop_handle;
    if (-1 == tts_instruct_add(&instruct))
    {
        QL_SMS_LOG("lt_tts_instruct_add fail!!");
        if (instruct.data)
        {
            free(instruct.data);
        }
        return -1;
    }
    else
    {
        if(SND_STOP == ltplay_get_src())
        {
            set_function_state(NEW_INFO, 0);
            lt_tts_new_info();
        }
        QL_SMS_LOG("lt_tts_instruct_add success!!");
        return 0;
    }
}

#if PACK_TYPE != PACK_NANJING
static void lt_lf_key_callback_register()
{
    lt_key_callback_t reg;
    //reg.key_gpio = KEY2;
// #if PACK_TYPE == PACK_LOOTOM
//     reg.key_gpio = KEY3;
// #else
//     reg.key_gpio = KEY2;
// #endif    

    reg.lt_key_click_handle = ql_key_tts_click;
    reg.lt_key_doubleclick_handle = ql_key_tts_next;
    //reg.lt_key_longclick_handle = ql_key_tts_stop;
    reg.lt_key_longclick_handle = NULL;
    reg.lt_key_repeat_callback =NULL;
    lt_key_callback_register("key_sms",&reg);
} // 回调参考上溯例子
#endif

static void lt_lf_play_callback_register()
{ 

    play_fsm_t tts_play_fsm = {
        .src = SND_SMS,
        .enter_play_func = ql_key_tts_next,
        .enter_stop_func = ql_key_tts_stop,
    };
    ltplay_callback_register(&tts_play_fsm);
} // 回调参考上溯例子
static void lt_ebs_play_callback_register()
{

    play_fsm_t tts_play_fsm = {
        .src = SND_EBS,
        .enter_play_func = ql_key_tts_next,
        .enter_stop_func = ql_key_tts_stop,
    };
    ltplay_callback_register(&tts_play_fsm);
} // 回调参考上溯例子

static void lt_sms_asr_callback_register()
{
    asr_type_state_t reg;

    reg.function = ASR_AUDIO;
    reg.state = ASR_SMS;
    reg.start_function = ql_key_tts_next;
    reg.stop_function = ql_key_tts_stop;
    //reg.pause_function = NULL;
    reg.next_function = NULL;
    reg.last_function = NULL;

    ltasr_callback_register(&reg);
} // 回调参考上溯例子

void lt_sms_t_task(void *param)
{
#if PACK_TYPE != PACK_NANJING
    lt_lf_key_callback_register();
#endif
    lt_lf_play_callback_register();
    lt_ebs_play_callback_register();
    lt_sms_asr_callback_register();
    while (1)
    { 
        ql_tts_ckeck();
        ql_rtos_task_sleep_s(1);
    }
}
