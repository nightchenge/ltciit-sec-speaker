/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2024-05-28 10:03:31
 * @LastEditors: ubuntu
 * @LastEditTime: 2025-11-20 15:31:51
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ql_i2c.h"
#include "ql_api_osi.h"
#include "ql_gpio.h"
#include "ql_log.h"

#include "ltkey.h"
#include "fm8035.h"
#include "ltfm.h"
#include "ltaudio.h"

#include "ltplay.h"
#include "led.h"
#include "ltuart2frx8016.h"
#include "ltsystem.h"
#include "ltweb_audio.h"


#include "ebm_task_schedule.h"
#include "ebm_task_play.h"
#include "ebm_task_type.h"
#include "ebs.h"
#include "lt_tts_demo.h"

#define FM_MODE_8035 0
#define FM_MODE_URL 1
#define FM_URL_MAX  50
typedef struct fm_info
{
    int changgemod_repat;
    int fm_mode;
    char audioUrl[255];
    char radioId[31];
    lt_audiourl_player_t fmurl_player;
    int (*play_get_fminfo)();
    void (*current_play)();
    void (*current_stop)();
    void (*play_next)();   
    uint32_t fm_freq;
    uint32_t fm_vol;
    bool    fm_CH; //FALSE 不在搜台，TRUE，在搜台
    uint32_t  fm_Tag[FM_URL_MAX];//保存url TAG 用来显示频点
    ql_task_t fmurl_task;
    ql_timer_t fmurl_start_timer;
} fm_info_t;

static fm_info_t ltfm_info={
    .changgemod_repat =FM_MODE_8035,
    .current_play=NULL,
    .current_stop=NULL,
    .fm_CH=FALSE,
    .fm_freq=0,
    .fm_mode=0,
    .fm_vol=0,
    .play_get_fminfo=NULL,
    .play_next=NULL,
    .fmurl_task=NULL,
    .fmurl_start_timer=NULL,
};//保存的当前fm_info 可扩展

typedef struct lt_fm_msg
{
    int type;
    uint32_t datalen;
    uint8_t *data;
} lt_fm_msg_t;


static void fm_url_msg_stop_cb()
{
     fm_info_t * this = &ltfm_info;

    char ebm_id[18] = {0};
    ebm_task_msg_t ebm_msg = {0};
    ebm_msg.type = TASK_DEL;
    dec_str2bcd_arr(this->radioId, (uint8_t *)&ebm_id, sizeof(ebm_id));
    memcpy(ebm_msg.buff, ebm_id, sizeof(ebm_id));
    if (eb_player_task_msg_send(&ebm_msg) == TASK_RESULT_OK)
    {
        QL_FM_LOG("eb_player_task_msg_send delete");
    }
}

static void fmurl_stop(void *data)
{
    fm_info_t *this = &ltfm_info;
    ql_rtos_timer_stop(this->fmurl_start_timer);
    QL_FM_LOG("fmurl_stop.\n");
    fm_url_msg_stop_cb();
}
static void fm_url_msg_start_cb()
{
    fm_info_t * this = &ltfm_info;
  //  AudioUrlItem *radio_msg = (AudioUrlItem *)data;
    ebm_task_t task = {0};
    ebm_task_msg_t ebm_msg = {0};
    task.status = TASK_STATUS_WAIT;
    task.chn_mask = CHN_IP_MASK;
    task.next = task.prev = NULL;
      QL_FM_LOG("fm_url_msg_start_cb11.\n");
    //dec_str2bcd_arr(radio_msg->radioId, task.ebm_id, sizeof(task.ebm_id));
        dec_str2bcd_arr(this->radioId, task.ebm_id, sizeof(task.ebm_id));
   // dec_str2bcd_arr(radioId, task.ebm_id, sizeof(task.ebm_id));
    task.ebm_type = 5;
    task.severity = 4;
    task.volume = 100;
    task.start_time = 0;
    task.stop_time = 0xFFFFFFFF;
    task.task_source = TASK_SOURCE_FMURL;
    // memcpy(task.url, radio_msg->audioUrl, strlen(radio_msg->audioUrl));
    // memcpy(task.url,(char *)data, strlen((char *)data));
    memcpy(task.url, this->audioUrl, strlen(this->audioUrl));
    // task.callback = ebm_task_stop_cb;
    task.callback = NULL;
    memcpy(ebm_msg.buff, (char *)&task, sizeof(ebm_task_t));
    ebm_msg.type = TASK_ADD;
    QL_FM_LOG("fm_url_msg_start_cb.\n");
    if (eb_player_task_msg_send(&ebm_msg) != TASK_RESULT_OK)
    {
        QL_FM_LOG("Lcoal task of U-disk add failed.\n");
    }

}


void fmurl_play()
{
    fm_info_t * this = &ltfm_info;
    ql_rtos_timer_start(this->fmurl_start_timer, 1000, 0);
    QL_FM_LOG("fmurl_play.\n");
    fm_url_msg_start_cb();
}
void ltfmurl_play_next()
{
    fm_info_t * this = &ltfm_info;
    this->fmurl_player.AudioUrlManager_next(&this->fmurl_player.manager);
}
 int ltfm_get_audioinfo()
 {
    fm_info_t *this = &ltfm_info;
    //return AudioUrlManager_cur_audioname(&this->fmurl_player.manager,data);

    memset( this->audioUrl,0,sizeof( this->audioUrl));
    memset( this->radioId,0,sizeof( this->radioId));   
    AudioUrlManager_cur_audioname(&this->fmurl_player.manager, this->audioUrl);
    AudioUrlManager_cur_radio_id(&this->fmurl_player.manager, this->radioId);
    return 0;



 }
 static void ql_key_fmurl_stop()
 {
     fm_info_t *this = &ltfm_info;
     this->changgemod_repat = 0;
     if (SND_FM_URL != ltplay_get_src())
     {
         QL_FM_LOG("fm not in play;exit");
         return;
     }
     lt_audio_pa_disable();

     QL_FM_LOG("start current stop");
     if (this->current_stop)
         this->current_stop();
     QL_FM_LOG("end current stop");
     ql_rtos_task_sleep_s(1);
     ltplay_check_play(SND_STOP);
     set_function_state(RT_TIME, 0);
     set_blink("fm", 0);
     lt_set_led_colour(3, OFF);
 }
#include <time.h>


#include <sys/time.h>

static const int MIN_INTERVAL_MS = 2000; // 最小间隔时间（毫秒）

static void ql_key_fmurl_next()
{
    static struct timeval last_call_time; // 记录上次调用时间
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    // 检查调用间隔
    unsigned long elapsed_time = (current_time.tv_sec - last_call_time.tv_sec) * 1000 + 
                        (current_time.tv_usec - last_call_time.tv_usec) / 1000;
    

    
    last_call_time = current_time; // 更新上次调用时间

    if (ltplay_get_src() == SND_TEL || ltplay_get_src() == SND_EBS || ltplay_get_src() == SND_SMS)
        return;
    
    fm_info_t *this = &ltfm_info;
    if (ltplay_get_src() == SND_FM_URL)
    {
        QL_FM_LOG("current_stop!!");
        if (this->current_stop)
        {
            QL_FM_LOG("this->current_sto !=NULL");
            this->current_stop(); // 先停止当前的，如果有的话
        }
    }

    if (this->fmurl_player.manager.size != 0)
    {
        ltplay_check_play(SND_FM_URL);
        lt_set_led_colour(3, ORANGE);
        if (this->fmurl_player.manager.currentIndex < FM_URL_MAX)
        {
            set_function_state(FM, this->fm_Tag[this->fmurl_player.manager.currentIndex] * 10);
        }
        else
        {
            set_function_state(FM, this->fmurl_player.manager.currentIndex * 10 + 1000);
        }


        this->play_get_fminfo = ltfm_get_audioinfo;
        this->play_get_fminfo();
        QL_FM_LOG("name_url ==%s\n", this->audioUrl);
        this->current_stop = fmurl_stop;
        this->current_play = fmurl_play;
        this->play_next = ltfmurl_play_next;
        if (elapsed_time > MIN_INTERVAL_MS)
        {

        }
        this->current_play();
        QL_FM_LOG("调用间隔过短，忽略本次调用。\n");
        this->play_next();
    }
    else
    {
        char *fm_mode = "网络FM 模式没有列表";
        ltapi_play_tts(fm_mode, strlen(fm_mode));
        ql_key_fmurl_stop();
    }
}

static void ql_key_fmurl_pre()
{

}
static void ql_key_fmurl_keep()
{
 lt_set_led_colour(3, ORANGE);
}




//通用play 的stop。防止直接退出ebs播放器导致的其他任务异常
// static void ql_play_fmurl_stop()
// {
//     fm_info_t *this = &ltfm_info;
//     this->changgemod_repat =0;
//     if(SND_FM_URL != ltplay_get_src())
//     {
//         QL_FM_LOG("fm not in play;exit");
//         return ;
//     }

//     ///lt_audio_pa_disable();
//     ltplay_check_play(SND_STOP);
//     set_function_state(RT_TIME, 0);
//     set_blink("fm", 0);

//     // 操作完成后关闭电源。
//     //lt_fm_power_off();

//     // lt_fm_power_off();
// }
void lt_fmurl_key_callback_register()
{
    lt_key_callback_t reg;
// #if PACK_TYPE == PACK_LOOTOM
//     reg.key_gpio = KEY1;
// #else
//     reg.key_gpio = KEY3;
// #endif

    //  reg.lt_key_click_handle = ql_key_fm_click;
    reg.lt_key_click_handle = ql_key_fmurl_next;
    reg.lt_key_doubleclick_handle = ql_key_fmurl_stop; // ql_key_fm_set_vol;
    reg.lt_key_longclick_handle = ql_key_fmurl_stop;
    reg.lt_key_repeat_callback = NULL;
    lt_key_callback_register("key_fm",&reg);
} // 回调参考上溯例子

void lt_fmurl_play_callback_register()
{

    play_fsm_t fm_url_play_fsm = {
        .src = SND_FM_URL,
        .enter_play_func = ql_key_fmurl_next,
        .enter_stop_func = ql_key_fmurl_stop,
        .enter_keep_func = ql_key_fmurl_keep,
    };
    ltplay_callback_register(&fm_url_play_fsm);


} // 回调参考上溯例子

static void lt_fmurl_asr_callback_register()
{
    asr_type_state_t reg;

    reg.function = ASR_AUDIO;
    reg.state = ASR_FM;
    reg.start_function = ql_key_fmurl_next;
    reg.stop_function = ql_key_fmurl_stop;
    reg.next_function = ql_key_fmurl_next;
    reg.last_function = ql_key_fmurl_pre;

    ltasr_callback_register(&reg);
} // 回调参考上溯例子

static void ql_key_fm_changemod(void *data)
{
    fm_info_t *this = &ltfm_info;
    int changgemod_repat = *((int*)data);
     QL_FM_LOG("changgemod_repat ==%d\n",changgemod_repat);
    if(SND_STOP != ltplay_get_src() ||  changgemod_repat !=8)
    {
        QL_FM_LOG("ql_key_fm_changemod fail,not in stop mode");
        return ;
    }
    
    if(this->fm_mode == FM_MODE_URL)
    {
         
        // if(this->current_stop)
        //     this->current_stop();
        this->fm_mode = FM_MODE_8035;
       // ql_rtos_task_sleep_s(2);
        char *fm_mode = "切换到调频FM 模式";
        QL_FM_LOG("切换到调频FM 模式");
        // ltplay_check_play(SND_SMS);
        ltapi_play_tts(fm_mode, strlen(fm_mode));

        lt_fm_key_callback_register();
        lt_fm_play_callback_register();
        lt_fm_asr_callback_register();
    }else
    {
        // QND_RXConfigAudio(QND_CONFIG_MUTE, 1);
        // lt_system_msg_handle(LT_SYS_FM_POWER_OFF);
        // lt_fm_power_status_set(FM_POWR_OFF);
         this->fm_mode = FM_MODE_URL;
        char *fm_mode = "切换到网络FM 模式";
          QL_FM_LOG("切换到网络FM 模式");
	    //ltplay_check_play(SND_SMS);
	    ltapi_play_tts(fm_mode, strlen(fm_mode));

        lt_fmurl_key_callback_register();
        lt_fmurl_play_callback_register();
        lt_fmurl_asr_callback_register();
    }

}

void lt_fmurl_keyrepeat_callback_register()
{
    lt_key_callback_t reg;
// #if PACK_TYPE == PACK_LOOTOM
//     reg.key_gpio = KEY1;
// #else
//     reg.key_gpio = KEY3;
// #endif

    //  reg.lt_key_click_handle = ql_key_fm_click;
    reg.lt_key_click_handle = NULL;
    reg.lt_key_doubleclick_handle = NULL; // ql_key_fm_set_vol;
    reg.lt_key_longclick_handle = NULL;
    reg.lt_key_repeat_callback = ql_key_fm_changemod;
    lt_key_callback_register("key_fm",&reg);
} // 回调参考上溯例子

void lt_fmurl_thread(void *param)
{
    fm_info_t *this = &ltfm_info;
    ql_rtos_task_sleep_s(5);
    lt_fmurl_keyrepeat_callback_register();
    audio_url_init(&this->fmurl_player);

    // lt_fm_key_callback_register(); // 注册事件回调
    // lt_fm_play_callback_register(); // 注册事件回调
    // lt_fm_asr_callback_register(); // 注册事件回调
    while(1)
    {
        ql_rtos_task_sleep_s(1); 
    }
    
}


void lt_fm_del_allurl()
{
       fm_info_t * this = &ltfm_info;
       this->fmurl_player.AudioUrlManager_free(&(this->fmurl_player.manager));
        QL_FM_LOG("fmurl_del_allurl succeed!");
}
void lt_fm_add_url(char * url_name)
{
        fm_info_t * this = &ltfm_info;
       this->fmurl_player.audiourlitme_add(&(this->fmurl_player.manager),url_name);
        QL_FM_LOG("audio_add_url succeed!");
}
void lt_fm_add_fmTag(int Tagnum, uint32_t Tag_val)
{
    QL_FM_LOG("Tag_val ==%d\n",Tag_val);
    fm_info_t *this = &ltfm_info;
    if (Tagnum < FM_URL_MAX)
    {
        this->fm_Tag[Tagnum] = Tag_val;
    }
}
void lt_fmurl_start_timer_callback(void *ctx)
{
        fm_url_msg_start_cb();
}
void lt_fmurl_app_init(void)
{
    QlI2CStatus err = QL_OSI_SUCCESS;
    //ql_task_t fmurl_task = NULL;

 
    err = ql_rtos_task_create(&ltfm_info.fmurl_task, QL_FM_TASK_STACK_SIZE, QL_FM_TASK_PRIO, "lt_fmurl", lt_fmurl_thread, NULL, QL_FM_TASK_EVENT_CNT);
    if (err != QL_OSI_SUCCESS)
    {
        QL_FM_LOG("i2ctest1 fm task created failed");
    }
    err = ql_rtos_timer_create(&ltfm_info.fmurl_start_timer, ltfm_info.fmurl_task, lt_fmurl_start_timer_callback, NULL);
    if (err != QL_OSI_SUCCESS)
    {
        QL_FM_LOG("fmurl_start_timer created failed");
    }
}