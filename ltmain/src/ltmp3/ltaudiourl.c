/*================================================================
  Copyright (c) 2020 Quectel Wireless Solution, Co., Ltd.  All Rights Reserved.
  Quectel Wireless Solution Proprietary and Confidential.
=================================================================*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_api_osi.h"
#include "ql_log.h"
//#include "audio_demo.h"
#include "ql_osi_def.h"
#include "ql_audio.h"
#include "ql_fs.h"
#include "ql_i2c.h"
#include "quec_pin_index.h"
#include "ql_gpio.h"
#include "ltmp3.h"
#include "ltkey.h"

#include "lt_tts_demo.h"
#include "ltplay.h"
#include "led.h"
#include "ltaudio.h"
#include "ltuart2frx8016.h"
#include "ltsystem.h"

#include "ebm_task_schedule.h"
#include "ebm_task_play.h"
#include "ebm_task_type.h"
#include "ebs.h"
#include "ltweb_audio.h"

/*========================================================================
 *  Global Variable
 *========================================================================*/

//static bool ring_tone_start = 0;
//static ql_task_t ql_play_task = NULL;

#define LT_AUDIO_LOG_LEVEL	            QL_LOG_LEVEL_INFO
#define LT_AUDIO_LOG(msg, ...)			QL_LOG(LT_AUDIO_LOG_LEVEL, "lt_audio", msg, ##__VA_ARGS__)
#define LT_AUDIO_LOG_PUSH(msg, ...)		QL_LOG_PUSH("lt_AUDIO", msg, ##__VA_ARGS__)

typedef struct lt_audio_player
{
    bool is_sd_mount;
    int play_sta;       // LTAPCU_STA_STOP;LTAPCU_STA_PLAY;LTAPCU_STA_PAUSE
    ql_sem_t wait_semp; // 信号用来通知当前已播放结束
    char audioUrl[255];
    char radioId[31];
    //  ltmp3_sta_t s_ltau_sta;
    lt_audiourl_player_t audiourl_player;
    bool N_F;//上下曲的标志位
    int (*check_list)();
    int (*play_get_audioinfo)();
    void (*current_start)();
    void (*current_stop)();
    void (*wait_stop)();
    void (*play_next)();
    void (*play_last)();
    void (*play_pause)();
    void (*play_resume)();
    int (*play_get_idx)();
} lt_audio_player_t;

lt_audio_player_t audio_player = {
    .is_sd_mount = false,
    .play_sta = LTAPCU_STA_STOP,
    .check_list =NULL,
    .current_start = NULL,
    .current_stop = NULL,
    .play_next = NULL,
    .play_last = NULL,
    .play_pause = NULL,
    .play_resume = NULL,
    .play_get_idx=NULL,
};
//lt_audiourl_player_t audiourl_player;


static void ql_key_mp3url_stop()
{
    //set_function_state(RT_TIME);
    lt_audio_player_t * this = &audio_player;
    if(SND_MP3_URL != ltplay_get_src() )
    {
        LT_AUDIO_LOG("mp3 not in play;exit");
        return ;
    }
    lt_audio_pa_disable();
    if(this->current_stop)
    {
        this->current_stop();
       // audiourl_player.audiourl_play(0, &(audiourl_player.manager));
       // ql_aud_player_stop();
    }
    this->play_sta = LTAPCU_STA_STOP;
    //ltmp3_set_audio_sta(LTAPCU_STA_STOP);
    ltplay_check_play(SND_STOP);
    set_function_state(RT_TIME,0);
     lt_set_led_colour(3,OFF);  
    if(this->play_last)
    {
     //   this->play_last();//停止的继续从上一曲开始播放
      //  ltmp3_play_cur();//停止的继续从上一曲开始播放//TODO
    }
  
    set_blink("mp3",0);
    
}

static void ql_key_mp3url_play()
{

}
static void ql_key_mp3url_keep()
{
   lt_set_led_colour(3, ORANGE);
    //  QND_RXConfigAudio(QND_CONFIG_MUTE, 0);
}
void ql_key_mp3url_next()
{
    lt_audio_player_t * this = &audio_player;
    if (ltplay_get_src() == SND_TEL || ltplay_get_src() == SND_EBS || ltplay_get_src() == SND_SMS)
        return ;

    if (0 == this->check_list())
    {
        char *MP3List_check = "当前没有获取网络音乐列表";
        // ltplay_check_play(SND_SMS);
        ltapi_play_tts(MP3List_check, strlen(MP3List_check));
        LT_AUDIO_LOG("MP3 have no  imessage!!");
        return;
    }
  
    if (ltplay_get_src() != SND_MP3_URL )
    {

        //set_function_state(MP3, ltmp3_get_cur_audio_index() + 1); // 提前显示MP3的面板
        set_function_state(MP3, this->play_get_idx() + 1); // 提前显示MP3的面板
      //  ltplay_check_play(SND_MP3);
    }
    else
    {
        LT_AUDIO_LOG("current_stop!!");
        this->current_stop();//先停止当前的，如果有的话
        // audiourl_player.audiourl_play(0, &(audiourl_player.manager));
        // ql_aud_player_stop();
    }
    //play_flag = 0;  //播放下一首
    this->N_F = TRUE;
    this->play_sta= LTAPCU_STA_STOP;

    //   set_function_state(MP3, this->play_get_idx() + 1); // 提前显示MP3的面板
    ltplay_check_play(SND_MP3_URL);
    // set_function_state(MP3, this->play_get_idx() + 1); // 提前显示MP3的面板
    this->play_sta = LTAPCU_STA_PLAY;
    //ltmp3_set_audio_sta(LTAPCU_STA_PLAY);
}

static void ql_key_mp3url_last()
{
    // if (ltplay_get_src() == SND_TEL || ltplay_get_src() == SND_EBS || ltplay_get_src() == SND_SMS)
    //     return;
    // if(ltmp3_get_audio_cnt() == 0 )
    // {
    //     char *MP3List_check = "当前没有音乐列表";
	//     //ltplay_check_play(SND_SMS);
	//     ltapi_play_tts(MP3List_check, strlen(MP3List_check));
    //     LT_AUDIO_LOG("MP3 have no  imessage!!");
    //     return;
    // }           
    // if (ltplay_get_src() != SND_MP3)
    // {
    //     set_function_state(MP3, ltmp3_get_cur_audio_index()+1); // 提前显示MP3的面板
    // }
    // ltplay_check_play(SND_MP3);

    // ql_aud_player_stop();
    // play_flag = 1;  //播放上一首
    //ltmp3_set_audio_sta(LTAPCU_STA_PLAY);
}

static void lt_audiourl_play_callback_register()
{
    play_fsm_t audiourl_play_fsm = {
        .src = SND_MP3_URL,
        .enter_play_func = ql_key_mp3url_play,
        .enter_stop_func = ql_key_mp3url_stop,
         .enter_keep_func = ql_key_mp3url_keep,
    };
    ltplay_callback_register(&audiourl_play_fsm);

} // 回调参考上溯例子


static void lt_audiourl_key_callback_register()
{
    lt_key_callback_t reg;
// #if PACK_TYPE == PACK_LOOTOM
//     reg.key_gpio = KEY6;
// #else
//     reg.key_gpio = KEY4;
// #endif
    // reg.key_gpio                    = KEY4;
    // reg.lt_key_click_handle         = ql_key_mp3_click;
    reg.lt_key_click_handle = ql_key_mp3url_next;
    reg.lt_key_doubleclick_handle = ql_key_mp3url_stop;
    reg.lt_key_longclick_handle = ql_key_mp3url_stop;
    reg.lt_key_repeat_callback = NULL;
    lt_key_callback_register("key_audio", &reg);
}

static void lt_mp3url_asr_callback_register()
{
    asr_type_state_t reg;

    reg.function = ASR_AUDIO;
    reg.state = ASR_MP3;
    reg.start_function = ql_key_mp3url_next;
    reg.stop_function = ql_key_mp3url_stop;
    reg.next_function = ql_key_mp3url_next;
    reg.last_function = ql_key_mp3url_last;

    ltasr_callback_register(&reg);
} // 回调参考上溯例子


void lt_audio_del_allurl()
{
    lt_audio_player_t *this = &audio_player;
    this->audiourl_player.AudioUrlManager_free(&(this->audiourl_player.manager));
    LT_AUDIO_LOG("audio_del_url succeed!");
}
void lt_audio_set_issd(bool st)
{
    lt_audio_player_t *this = &audio_player;
    this->is_sd_mount = st;
}
void lt_audio_add_url(char *url_name)
{
    lt_audio_player_t *this = &audio_player;
    this->audiourl_player.audiourlitme_add(&(this->audiourl_player.manager), url_name);
    LT_AUDIO_LOG("audio_add_url succeed!");
}


static void audio_url_msg_stop_cb()
{
    lt_audio_player_t * this = &audio_player;

    char ebm_id[18] = {0};
    ebm_task_msg_t ebm_msg = {0};
    ebm_msg.type = TASK_DEL;
    dec_str2bcd_arr(this->radioId, (uint8_t *)&ebm_id, sizeof(ebm_id));
    memcpy(ebm_msg.buff, ebm_id, sizeof(ebm_id));
    if (eb_player_task_msg_send(&ebm_msg) == TASK_RESULT_OK)
    {
        LT_AUDIO_LOG("eb_player_task_msg_send delete");
    }
}

void lt_audio_url_stop_cb(void *data)
{
    lt_audio_player_t *this = &audio_player;
    LT_AUDIO_LOG("lt_audio_url_stop_cb succeed!");

    if (this->play_sta != LTAPCU_STA_STOP)
        audio_url_msg_stop_cb();

    ql_rtos_semaphore_release(this->wait_semp);
}

static void audio_url_msg_start_cb()
{
    lt_audio_player_t * this = &audio_player;


    //  AudioUrlItem *radio_msg = (AudioUrlItem *)data;
    ebm_task_t task = {0};
    ebm_task_msg_t ebm_msg = {0};
    task.status = TASK_STATUS_WAIT;
    task.chn_mask = CHN_IP_MASK;
    task.next = task.prev = NULL;
    // char radioId[31];
    // generateRandomRadioId(radioId);
    // dec_str2bcd_arr(radio_msg->radioId, task.ebm_id, sizeof(task.ebm_id));
    dec_str2bcd_arr(this->radioId, task.ebm_id, sizeof(task.ebm_id));
    task.ebm_type = 5;
    task.severity = 3;
    task.volume = 100;
    task.start_time = 0;
    task.stop_time = 0xFFFFFFFF;
    task.task_source = TASK_SOURCE_AUDIOURL;
    // memcpy(task.url, radio_msg->audioUrl, strlen(radio_msg->audioUrl));
    memcpy(task.url,this->audioUrl, strlen(this->audioUrl));
    // task.callback = ebm_task_stop_cb;
    task.callback = lt_audio_url_stop_cb;
    memcpy(ebm_msg.buff, (char *)&task, sizeof(ebm_task_t));
    ebm_msg.type = TASK_ADD;
    if (eb_player_task_msg_send(&ebm_msg) != TASK_RESULT_OK)
    {
        LT_AUDIO_LOG("Lcoal task of U-disk add failed.\n");
    }
}


static void audiourl_stop(void *data)
{
    LT_AUDIO_LOG("audiourl_stop.\n");
   // eb_player_current_task_delete();
   audio_url_msg_stop_cb();

}


void audiourl_play()
{

    audio_url_msg_start_cb();
}

void audio_waitstop()
{
    lt_audio_player_t * this = &audio_player;
    ql_rtos_semaphore_wait(this->wait_semp, QL_WAIT_FOREVER);
}

static void lt_audiourl_thread(void *param)
{
    ql_rtos_task_sleep_s(5);
     lt_audio_player_t * this = &audio_player;
    // ql_set_volume(11);
    // ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_LOCAL, 11);
    while (1)
    {
        ql_rtos_task_sleep_s(1);
        // continue;

        //if (LTAPCU_STA_PLAY == ltmp3_get_audio_sta())
        if (LTAPCU_STA_PLAY == this->play_sta) // net url
        {
            if (0 ==this->check_list())
            {
                LT_AUDIO_LOG("当前没有网络音乐列表");
                 ql_key_mp3url_stop();
            }
            else
            {
                //   audiourl_play(0);
                //   audiourl_next();
                this->play_get_audioinfo();
                LT_AUDIO_LOG("name_url==%s\n", this->audioUrl);

                set_function_state(MP3, this->play_get_idx() + 1);
                lt_set_led_colour(3, ORANGE);
                // audiourl_player.audiourl_play(1, &(audiourl_player.manager));
                // ql_rtos_semaphore_wait(audio_semp, QL_WAIT_FOREVER);
                this->current_stop = audiourl_stop;
                this->current_start = audiourl_play;
                this->wait_stop = audio_waitstop;
                this->current_start(); // 支持wav mp3 amr awb格式
                this->wait_stop();
                //  (play_flag ==0)?audiourl_player.AudioUrlManager_next(&(audiourl_player.manager)):NULL;
                if (this->N_F)
                {
                   // audiourl_player.AudioUrlManager_next(&(audiourl_player.manager));
                    this->play_next();
                    
                }else
                {
                    this->play_last();
                }
            }
        }
    }

    LT_AUDIO_LOG("test done, exit audio demo");
    ql_rtos_task_delete(NULL);
}


static int ltaudio_check(int type)
{
    lt_audio_player_t * this = &audio_player;

    return this->audiourl_player.manager.size;
}

static int ltaudio_get_audioinfo()
{
    lt_audio_player_t *this = &audio_player;
    memset( this->audioUrl,0,sizeof( this->audioUrl));
    memset( this->radioId,0,sizeof( this->radioId));   
    AudioUrlManager_cur_audioname(&this->audiourl_player.manager, this->audioUrl);
    AudioUrlManager_cur_radio_id(&this->audiourl_player.manager, this->radioId);
    return 0;
}
static void ltaudio_play_next()
{
    lt_audio_player_t *this = &audio_player;

    this->audiourl_player.AudioUrlManager_next(&this->audiourl_player.manager);
    //  return AudioUrlManager_cur_audioname(&this->audiourl_player.manager,data);
}
static void ltaudio_play_last()
{
    lt_audio_player_t *this = &audio_player;
    this->audiourl_player.AudioUrlManager_next(&this->audiourl_player.manager); // 音乐列表无上一首
    //  return AudioUrlManager_cur_audioname(&this->audiourl_player.manager,data);
}

static int ltaudio_play_get_idx()
{
    lt_audio_player_t * this = &audio_player;

    return this->audiourl_player.manager.currentIndex;
}
static void ql_key_audio_changemod(void *data)
{
   // fm_info_t *this = &ltfm_info;
    int changgemod_repat = *((int*)data);
    static int audio_mod=0;

    if(SND_STOP != ltplay_get_src() ||  changgemod_repat !=8)
    {
        LT_AUDIO_LOG("ql_key_fm_changemod fail,not in stop mode");
        return ;
    }

    if (audio_mod == 0)
    {

        // if(this->current_stop)
        //     this->current_stop();
        audio_mod = 1;
        // ql_rtos_task_sleep_s(2);
        char *fm_mode = "切换到网络音频 模式";
        LT_AUDIO_LOG("切换到网络音频 模式");
        // ltplay_check_play(SND_SMS);
        ltapi_play_tts(fm_mode, strlen(fm_mode));
        lt_audiourl_key_callback_register();  // 注册key 回调
        lt_audiourl_play_callback_register(); // 注册play回调
        lt_mp3url_asr_callback_register();
    }
    else
    {
        audio_mod = 0;
        // ql_rtos_task_sleep_s(2);
        char *fm_mode = "切换到本地音频 模式";
        LT_AUDIO_LOG("切换到本地音频 模式");
        // ltplay_check_play(SND_SMS);
        ltapi_play_tts(fm_mode, strlen(fm_mode));
        lt_audio_key_callback_register();  // 注册key 回调
        lt_audio_play_callback_register(); // 注册play回调
        lt_mp3_asr_callback_register();
        ltmp3_init(NULL); //切换模式会重重新读取列表
        
    }
}


void lt_audiourl_keyrepeat_callback_register()
{
    lt_key_callback_t reg;
// #if PACK_TYPE == PACK_LOOTOM
//     reg.key_gpio = KEY6;
// #else
//     reg.key_gpio = KEY4;
// #endif
    // reg.key_gpio                    = KEY4;
    // reg.lt_key_click_handle         = ql_key_mp3_click;
    reg.lt_key_click_handle = NULL;
    reg.lt_key_doubleclick_handle = NULL;
    reg.lt_key_longclick_handle = NULL;
    reg.lt_key_repeat_callback = ql_key_audio_changemod;
    lt_key_callback_register("key_audio", &reg);

}
void lt_audiourl_app_init(void)
{
	QlOSStatus err = QL_OSI_SUCCESS;
	ql_task_t ql_audiourl_task = NULL;
    LT_AUDIO_LOG("audio demo enter");
    lt_audio_player_t * this = &audio_player;
    ql_rtos_semaphore_create(&this->wait_semp, 0);//创建等待音频自动播完信号量
    audio_url_init(&this->audiourl_player);

    this->check_list = ltaudio_check;
    this->play_get_audioinfo= ltaudio_get_audioinfo;
    this->current_start=NULL;
    this->current_stop=NULL;
    this->wait_stop=NULL;
    this->play_next=ltaudio_play_next;
    this->play_last=ltaudio_play_last;
    this->play_pause=NULL;
    this->play_resume=NULL;
    this->play_get_idx= ltaudio_play_get_idx;

    lt_audiourl_keyrepeat_callback_register();

   // audiourl_player.audiourl_play = audiourl_play;

    err = ql_rtos_task_create(&ql_audiourl_task, 5*4096, APP_PRIORITY_NORMAL, "ql_webaudio", lt_audiourl_thread, NULL, 5);
	if(err != QL_OSI_SUCCESS)
    {
		LT_AUDIO_LOG("audio task create failed");
	}


}

