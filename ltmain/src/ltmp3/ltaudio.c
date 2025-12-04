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
/*========================================================================
 *  Global Variable
 *========================================================================*/

//static bool ring_tone_start = 0;
//static ql_task_t ql_play_task = NULL;

#define LT_AUDIO_LOG_LEVEL	            QL_LOG_LEVEL_INFO
#define LT_AUDIO_LOG(msg, ...)			QL_LOG(LT_AUDIO_LOG_LEVEL, "lt_audio", msg, ##__VA_ARGS__)
#define LT_AUDIO_LOG_PUSH(msg, ...)		QL_LOG_PUSH("lt_AUDIO", msg, ##__VA_ARGS__)


int play_callback(char *p_data, int len, enum_aud_player_state state)
{
	if(state == AUD_PLAYER_START)
	{
		LT_AUDIO_LOG("player start run");
	}
	else if(state == AUD_PLAYER_FINISHED)
	{
		LT_AUDIO_LOG("player stop run");
	}
	else
	{
		LT_AUDIO_LOG("type is %d", state);
	}

	return QL_AUDIO_SUCCESS;
}


static void ltapi_pause_mp3()
{
 //   ql_aud_player_pause();
    if(ql_aud_player_pause())
	{
		LT_AUDIO_LOG("pause failed");
		return;
	}
}

static void ltapi_resume_mp3()
{
 //   ql_aud_player_pause();
    if(ql_aud_player_resume())
	{
		LT_AUDIO_LOG("resume failed");
		return;
	}
}
static void ql_key_mp3_stop()
{
    //set_function_state(RT_TIME);
    if(SND_MP3 != ltplay_get_src() )
    {
        LT_AUDIO_LOG("mp3 not in play;exit");
        return ;
    }
    lt_audio_pa_disable();
    ql_aud_player_stop();
    ltmp3_set_audio_sta(LTAPCU_STA_STOP);
    ltplay_check_play(SND_STOP);
    set_function_state(RT_TIME,0);
    ltmp3_play_cur();//停止的继续从上一曲开始播放
    set_blink("mp3",0);
    
}

static void ql_key_mp3_play()
{
    if (ltplay_get_src() == SND_TEL || ltplay_get_src() == SND_EBS || ltplay_get_src() == SND_SMS)
        return ;
    if(ltmp3_get_audio_cnt() == 0 )
    {
	    //ltplay_check_play(SND_SMS);
	    ltapi_play_tts(TTS_STR_MP3_NO_LIST, strlen(TTS_STR_MP3_NO_LIST));
        LT_AUDIO_LOG("MP3 have no  imessage!!");
        return;
    }
       
    if(ltplay_get_src() != SND_MP3 && LTAPCU_STA_STOP == ltmp3_get_audio_sta() )
    {
        set_function_state(MP3, ltmp3_get_cur_audio_index()+1);//提前显示MP3的面板
        ltplay_check_play(SND_MP3);
        ql_aud_player_stop();
        ltmp3_set_audio_sta(LTAPCU_STA_PLAY);
    }else{
        if(LTAPCU_STA_PLAY == ltmp3_get_audio_sta())
        {
            ltmp3_set_audio_sta(LTAPCU_STA_PAUSE);
            set_function_state(PAUS, 0);//paus
            ltapi_pause_mp3();
        }else if(LTAPCU_STA_PAUSE == ltmp3_get_audio_sta()){
            ltmp3_set_audio_sta(LTAPCU_STA_PLAY);
            set_function_state(MP3, ltmp3_get_cur_audio_index()+1);
            ltapi_resume_mp3();
        }
        
    }
}


uint8_t play_flag = 0;
static void ql_key_mp3_next()
{

    if (ltplay_get_src() == SND_TEL || ltplay_get_src() == SND_EBS || ltplay_get_src() == SND_SMS)
        return;

    if (ltplay_get_src() != SND_MP3)
    {
        int cnt = ltmp3_get_audio_cnt();
        int idx = ltmp3_get_cur_audio_index();
        ltmp3_init(NULL);
        if (ltmp3_get_audio_cnt() == 0)
        {
            lt_audio_pa_disable();
            // ltplay_check_play(SND_SMS);
            ltapi_play_tts(TTS_STR_MP3_NO_LIST, strlen(TTS_STR_MP3_NO_LIST));
            LT_AUDIO_LOG("MP3 have no  imessage!!");
            return;
        }
        if (ltmp3_get_audio_cnt() == cnt)
            ltmp3_set_cur_audio_index(idx);
        set_function_state(MP3, ltmp3_get_cur_audio_index() + 1); // 提前显示MP3的面板
    }
    ltplay_check_play(SND_MP3);
    ql_aud_player_stop();
    play_flag = 0;  //播放下一首
    ltmp3_set_audio_sta(LTAPCU_STA_PLAY);
}

static void ql_key_mp3_last()
{
    if (ltplay_get_src() == SND_TEL || ltplay_get_src() == SND_EBS || ltplay_get_src() == SND_SMS)
        return;
    if(ltmp3_get_audio_cnt() == 0 )
    {
	    //ltplay_check_play(SND_SMS);
	    ltapi_play_tts(TTS_STR_MP3_NO_LIST, strlen(TTS_STR_MP3_NO_LIST));
        LT_AUDIO_LOG("MP3 have no  imessage!!");
        return;
    }           
    if (ltplay_get_src() != SND_MP3)
    {
        set_function_state(MP3, ltmp3_get_cur_audio_index()+1); // 提前显示MP3的面板
    }
    ltplay_check_play(SND_MP3);
    ql_aud_player_stop();
    play_flag = 1;  //播放上一首
    ltmp3_set_audio_sta(LTAPCU_STA_PLAY);
}

void lt_audio_play_callback_register()
{
    
    play_fsm_t audio_play_fsm = {
        .src = SND_MP3,
        .enter_play_func = ql_key_mp3_play,
        .enter_stop_func = ql_key_mp3_stop,
    };
    ltplay_callback_register(&audio_play_fsm);
} // 回调参考上溯例子


void lt_audio_key_callback_register()
{
    lt_key_callback_t reg;
    reg.lt_key_click_handle = ql_key_mp3_next;
    reg.lt_key_doubleclick_handle = ql_key_mp3_stop;
    reg.lt_key_longclick_handle = ql_key_mp3_stop;
    reg.lt_key_repeat_callback = NULL;
    lt_key_callback_register("key_audio", &reg);
}

void play_file(char *file_name)
{
       
	if(ql_aud_play_file_start(file_name, QL_AUDIO_PLAY_TYPE_LOCAL, play_callback))
	{
		LT_AUDIO_LOG("play failed");
		return;
	}	
    lt_audio_pa_enable(); 	
	ql_aud_wait_play_finish(QL_WAIT_FOREVER);
    lt_audio_pa_disable();
	ql_aud_player_stop(); //播放结束，释放播放资源	
	LT_AUDIO_LOG("play file %s done", file_name);
}


static void lt_audio_thread(void *param)
{
    int n = 0;
    ql_rtos_task_sleep_s(5);
    lt_audio_pa_disable();
    // ql_set_volume(11);
    // ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_LOCAL, 11);
    while (1)
    {
        ql_rtos_task_sleep_s(1);
        // continue;

        if (LTAPCU_STA_PLAY == ltmp3_get_audio_sta())
        {
            char mp3_name[255] = {0};
            LT_AUDIO_LOG("enter audio demo,time:%d", n);

            if (-1 != ltmp3_check())
            {
                ltmp3_get_cur_audio_name(mp3_name);
                LT_AUDIO_LOG("mp3_name==%s\n", mp3_name);
                if (ql_file_exist(mp3_name) == QL_FILE_OK)
                {
                    set_function_state(MP3, ltmp3_get_cur_audio_index()+1);            
                    play_file(mp3_name); // 支持wav mp3 amr awb格式
                }
                else
                {
                    LT_AUDIO_LOG("file %s no exist!", mp3_name);
                }
                (play_flag ==0)?ltmp3_play_next():ltmp3_play_cur();
            }
            else
            {
                ltmp3_set_audio_sta(LTAPCU_STA_STOP);
                ql_key_mp3_stop();
                LT_AUDIO_LOG("mp3 check error");
            }
        }
        n++;
    }

    LT_AUDIO_LOG("test done, exit audio demo");
    ql_rtos_task_delete(NULL);
}



//初始化功放
static void lt_audio_pa_init(void)
{
	ql_pin_set_func(50, 2);
	ql_gpio_set_direction(GPIO_35, GPIO_OUTPUT);
}

void lt_mp3_asr_callback_register()
{
    asr_type_state_t reg;

    reg.function = ASR_AUDIO;
    reg.state = ASR_MP3;
    reg.start_function = ql_key_mp3_next;
    reg.stop_function = ql_key_mp3_stop;
    reg.next_function = ql_key_mp3_next;
    reg.last_function = ql_key_mp3_last;

    ltasr_callback_register(&reg);
} // 回调参考上溯例子

static bool ring_tone_start = 0;
ql_task_t ql_play_task = NULL;
static int ringtone_callback(bool start, void *ctx)
{
	if(start)
	{
		LT_AUDIO_LOG("ringtone start play");
        int n = 0;
        while (QL_AUDIO_STATUS_RUNNING == ql_aud_get_play_state() && n < 20)
        {
            LT_AUDIO_LOG("ql_aud_get_play_state() ==%d", n);
            ql_rtos_task_sleep_ms(500);
            n++;
        }
        
        
        if(ql_aud_get_play_state() != QL_AUDIO_STATUS_RUNNING)
		{
			ql_event_t event = {0};
			
			event.id = QL_AUDIO_RINGTONE_PLAY;
			ring_tone_start = (ql_rtos_event_send(ql_play_task, &event) ? FALSE:TRUE);
		}
	}
	else
	{
		LT_AUDIO_LOG("ringtone stop play");
		if(ring_tone_start){

			ring_tone_start = FALSE;
		}
	}
    return 0;
}

void test_ring_tone(void)
{
    ql_aud_set_ringtone_type(QL_AUD_RING_CUSTOMER_DEF);
    ql_bind_ring_tone_cb(ringtone_callback);
}

// void ql_test_ring()
// {
//     int err = 0;

//     err = ql_aud_play_file_start("UFS:ring.mp3", QL_AUDIO_PLAY_TYPE_LOCAL, NULL);
//     if (err)
//     {
//         QL_AUDIO_LOG("play mp3 end");
//     }
//     ql_aud_wait_play_finish(QL_WAIT_FOREVER);
//       ql_aud_player_stop(); //播放结束，释放播放资源	
// }
static void ql_audio_play_thread(void *ctx)
{
    //  ql_rtos_task_sleep_s(10);
    // ql_test_ring();


	int err = 0;
	ql_event_t event = {0};

	while(1)
	{
		err = ql_event_try_wait(&event);
        if(err)
        {
            LT_AUDIO_LOG("wait event failed");
            continue;
        }
		switch(event.id)
		{
		case QL_AUDIO_RINGTONE_PLAY:
			do
			{
                if (ltplay_get_src() == SND_TEL)
                {
                    err = ql_aud_play_file_start("UFS:ring.mp3", QL_AUDIO_PLAY_TYPE_LOCAL, NULL);
                    if (err)
                    {
                        ring_tone_start = FALSE;
                        break;
                    }
                    ql_aud_wait_play_finish(QL_WAIT_FOREVER);
                    ql_aud_player_stop();
                }else
                {
                    ql_rtos_task_sleep_ms(500);
                }
            }while(ring_tone_start);
		break;
		}          
	}	
    // int err = 0;
    // while (1)
    // {
    //     while (ring_tone_start)
    //     {
    //         err = ql_aud_play_file_start("UFS:ring.mp3", QL_AUDIO_PLAY_TYPE_LOCAL, NULL);
    //         if (err)
    //         {
    //             QL_AUDIO_LOG("play mp3 end");
    //             ring_tone_start = FALSE;
    //             break;
    //         }
    //         ql_aud_wait_play_finish(QL_WAIT_FOREVER);
    //         ql_aud_player_stop(); //播放结束，释放播放资源	
    //     }
    //     ql_rtos_task_sleep_s(1);
    // }
}
void lt_audio_app_init(void)
{
	QlOSStatus err = QL_OSI_SUCCESS;
	ql_task_t ql_audio_task = NULL;
    LT_AUDIO_LOG("audio demo enter");

    lt_audio_pa_init();//初始化功放引脚

    lt_audio_key_callback_register();//注册key 回调
    lt_audio_play_callback_register(); //注册play回调
    lt_mp3_asr_callback_register();
	
    err = ql_rtos_task_create(&ql_audio_task, 10*1024, APP_PRIORITY_NORMAL, "ql_audio", lt_audio_thread, NULL, 5);
	if(err != QL_OSI_SUCCESS)
    {
		LT_AUDIO_LOG("audio task create failed");
	}

	test_ring_tone();

    err = ql_rtos_task_create(&ql_play_task,  2*1024, APP_PRIORITY_HIGH, "ql_play_task", ql_audio_play_thread, NULL, 2);
    if (err != QL_OSI_SUCCESS)
    {
        LT_AUDIO_LOG("audio task create failed");
    }
}


