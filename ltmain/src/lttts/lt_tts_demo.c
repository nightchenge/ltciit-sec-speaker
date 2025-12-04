/*================================================================
  Copyright (c) 2020 Quectel Wireless Solution, Co., Ltd.  All Rights Reserved.
  Quectel Wireless Solution Proprietary and Confidential.
=================================================================*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "ql_app_feature_config.h"
#include "lt_tts_demo.h"
#include "ql_api_osi.h"
#include "ql_api_dev.h"
#include "ql_log.h"
#include "ql_osi_def.h"
#ifdef QL_APP_FEATURE_AUDIO
#include "ql_audio.h"
#endif
#include "ql_fs.h"
#ifdef QL_APP_FEATURE_PWM_AUDIO
#include "ql_pwm_audio.h"
#include "ql_pin_cfg.h"
#endif

#include "ql_gpio.h"
#include "ql_api_rtc.h"

/*
	1. 不同的TTS资源文件，对应的播放效果不同。其中中文资源不能用来播英文单词，单词会以字母的方式播出; 英文资源也不能用来播中文。默认使用
	   中文16k TTS资源

	2. 若使用16k中文TTS资源，且TTS资源文件预置到内置flash中，则不需要修改json脚本(脚本默认已选择预置16k中文资源，且预置到内置flash)，只需要调用
	   ql_tts_engine_init函数即可完成初始化，不需要关注以下描述

	3. 所有的资源文件均在components\ql-config\download\prepack下，其中:
	   英文16k资源文件名为: "quectel_tts_resource_english_16k.bin"
	   中文8k资源文件为："quectel_tts_resource_chinese_8k.bin"
	   中文16k资源文件为："quectel_pcm_resource.bin"

	4. 预置文件时，请将json脚本中的"file"固定为"/qsfs/quectel_pcm_resource.bin"(预置资源文件到内置flash), 或
	   "/ext/qsfs/quectel_pcm_resource.bin"(预置到外置6线spi flash中), 并修改"local_file"来选择上传哪个资源文件，如下述示例.
	   若不使用中文16k资源，则需要使用"ql_tts_engine_init_ex"函数，将配置结构体中的"resource"变量设置为需要使用的资源;
	   若将资源文件预置到外置6线spi flash，需要将"position"变量设置为 POSIT_EFS

	   当TTS资源文件预置在内置Flash时，针对需要FOTA升级的情况，新版本SDK中默认将该文件进行拆分为多个子文件进行预置！
	   外置存储时可以不用拆分。

	5. 使用英文16k TTS资源播放时，需要1.45M的RAM空间，因此要注意RAM空间是否充足； 选择中文16k TTS资源文件时，需要620k的RAM空间； 选择中文
	   8k资源时，需要570kRAM空间


预置文件示例：
	1. 预置16k中文TTS资源文件到内部flash(默认):
		"files": [
			{
				"file": "/qsfs/quectel_pcm_resource.bin",
				"local_file": "quectel_pcm_resource.bin"
			}
		]
	2. 预置16k英文TTS资源文件到内部flash(以"/qsfs/quectel_pcm_resource.bin"为文件系统路径)
		"files": [
			{
				"file": "/qsfs/quectel_pcm_resource.bin",
				"local_file": "quectel_tts_resource_english_16k.bin"
			}
		]
	3. 预置8K中文TTS资源文件到内部flash(以"/qsfs/quectel_pcm_resource.bin"为文件系统路径)
		"files": [
			{
				"file": "/qsfs/quectel_pcm_resource.bin",
				"local_file": "quectel_tts_resource_chinese_8k.bin"
			}
		]
	4.  (1)预置16k英文TTS资源到外置6线spi flash(以/ext/qsfs/quectel_pcm_resource.bin"为文件系统路径)
		"files": [
			{
				"file": "/ext/qsfs/quectel_pcm_resource.bin",
				"local_file": "quectel_tts_resource_english_16k.bin"
			}
		]
		(2)需要把boot_fdl_dnld.c文件的bool fdlDnldStart(fdlEngine_t *fdl, unsigned devtype)，
			6线flash部分的#if 0打开为1（CONFIG_QUEC_PROJECT_FEATURE_SPI6_EXT_NOR_SFFS部分）;
		(3)在target.config中，CONFIG_QUEC_PROJECT_FEATURE_SPI6_EXT_NOR_SFFS打开，CONFIG_QUEC_PROJECT_FEATURE_SPI4_EXT_NOR关闭

	5.  (1)预置16k中文TTS资源文件到外部4线flash
		"files": [
			{
				"file": "/ext4n/qsfs/quectel_pcm_resource.bin",
				"local_file": "quectel_pcm_resource.bin"
			}
		]
		(2)需要把boot_fdl_dnld.c文件的bool fdlDnldStart(fdlEngine_t *fdl, unsigned devtype)，
			4线flash部分的#if 0打开为1（CONFIG_QUEC_PROJECT_FEATURE_SPI4_EXT_NOR_SFFS部分）;
		(3)在target.config中，CONFIG_QUEC_PROJECT_FEATURE_SPI4_EXT_NOR_SFFS打开，CONFIG_QUEC_PROJECT_FEATURE_SPI6_EXT_NOR关闭
*/
#define QL_TTS_LANGUAGE_ENGLISH 0

#ifdef QL_APP_FEATURE_PWM_AUDIO
#define QL_TTS_PWM_AUDIO 1
#else
#define QL_TTS_PWM_AUDIO 0
#endif

/*0:tts库在内置flash，1:tts库在六线flash，2：tts库在四线flash*/
#define QL_TTS_LOCATION 0

#define QL_TTS_LOG_LEVEL QL_LOG_LEVEL_INFO
#define QL_TTS_LOG(msg, ...) QL_LOG(QL_TTS_LOG_LEVEL, "ql_app_tts", msg, ##__VA_ARGS__)
#define QL_TTS_LOG_PUSH(msg, ...) QL_LOG_PUSH("ql_app_tts", msg, ##__VA_ARGS__)

#if !defined(tts_demo_no_err)
#define tts_demo_no_err(x, action, str) \
	do                                  \
	{                                   \
		if (x != 0)                     \
		{                               \
			QL_TTS_LOG(str);            \
			{                           \
				action;                 \
			}                           \
		}                               \
	} while (1 == 0)
#endif

/*===========================================================================
 * Variate
 ===========================================================================*/
#if QL_TTS_PWM_AUDIO == 0
PCM_HANDLE_T tts_player = NULL;
#else
PWM_AUDIO_HANDLE_T tts_pwm_player = NULL;
#endif
ql_task_t ql_tts_demo_task = NULL;
ql_task_t ql_tts_demo_task1 = NULL;

ql_queue_t lt_tts_msg_queue;

/*===========================================================================
 * Functions
 ===========================================================================*/

int userCallback(void *param, int param1, int param2, int param3, int data_len, const void *pcm_data)
{
	int err;
	err = ql_pcm_write(tts_player, (void *)pcm_data, data_len);
	if (err <= 0)
	{
		QL_TTS_LOG("write data to PCM player failed");
		return -1;
	}
	return 0;
}
void lt_tts_vol_before()
{
	ql_rtc_time_t tm;
	//  ql_rtc_set_timezone(32);    //UTC+32
	ql_rtc_get_localtime(&tm);
	// 获取当前时间
	int currentHour = tm.tm_hour; // 当前小时
	if ((currentHour > 21 && currentHour < 24) || (currentHour >= 0 && currentHour <= 6))
	{
	}
}

// 新增type 0--开播 1--停播

#define TTS_OPEN 0
#define TTS_CLOSE 1
lt_tts_callback_t tts_fsm = {NULL};
void ql_tts_play_lo(int type, char *data, int data_len, ql_tts_encoding_e encoding,
					lt_tts_callback before_start,
					lt_tts_callback after_stop)
{
	int err = 0;
	tts_param_t tts_param = {0};
	if (TTS_CLOSE == type)
	{
		goto exit;
	}
	QL_TTS_LOG("ql_tts_play_lo");
	if (tts_fsm.lt_tts_start_handle)
	{
		QL_TTS_LOG("lt_tts_start_handle");
		tts_fsm.lt_tts_start_handle(1);
	}

	if (NULL != before_start)
	{
		before_start(1);
	}

	if(  QL_AUDIO_STATUS_IDLE != ql_aud_get_play_state())
	{
		QL_TTS_LOG("audio is playing , wait to play tts");
		ql_aud_wait_play_finish(QL_WAIT_FOREVER);//等待audio播放完成
		QL_TTS_LOG("ql_aud_get_play_state: %d", ql_aud_get_play_state());
	}

		


	QL_PCM_CONFIG_T config = {1, 16000, 0};
	tts_player = ql_pcm_open(&config, QL_PCM_BLOCK_FLAG | QL_PCM_WRITE_FLAG);
	tts_demo_no_err(!tts_player, goto exit, "create pcm_player failed");

	ql_errcode_dev_e Temp;
	ql_memory_heap_state_t Stack_Test;
	Temp = ql_dev_memory_size_query(&Stack_Test);
	if (Temp == QL_DEV_SUCCESS)
	{
		QL_TTS_LOG(" Stack_Test.avail_size: %d", Stack_Test.avail_size);
		QL_TTS_LOG(" Stack_Test.total_size: %d", Stack_Test.total_size);
		QL_TTS_LOG(" Stack_Test.max_block_size: %d", Stack_Test.max_block_size);
	}
	if (!ql_tts_is_running())
	{

		tts_param.resource = TTS_RESOURCE_16K_CN;
		tts_param.position = POSIT_INTERNAL_FS;
		err = ql_tts_engine_init_ex(userCallback, &tts_param);
		tts_demo_no_err(err, goto exit, "tts session begain failed");
	}
	do
	{
		ql_tts_set_config_param(QL_TTS_CONFIG_ENCODING, encoding);
		ql_tts_set_config_param(QL_TTS_CONFIG_DGAIN, 0);
		err = ql_tts_start((const char *)data, data_len);
		tts_demo_no_err(err, goto exit, "tts start failed");
	} while (0);
	// 播放结束了回调
	// exit:
	ql_aud_data_done();
	ql_aud_wait_play_finish(QL_WAIT_FOREVER);
exit:
	// ql_tts_end();
	//  播放结束了回调
	if (tts_fsm.lt_tts_stop_handle)
	{
		QL_TTS_LOG("lt_tts_stop_handle");
		tts_fsm.lt_tts_stop_handle(0);
	}

	if (NULL != after_stop)
	{
		after_stop(0);
	}

	if (tts_player)
	{
		ql_pcm_close(tts_player);
		tts_player = NULL;
	}
	QL_TTS_LOG("tts done");
}

void ql_tts_thread(void *param)
{
	ql_rtc_time_t tm;
	ql_rtc_set_timezone(32); // UTC+32
	ql_rtc_get_localtime(&tm);
	// 获取当前时间
	int currentHour = tm.tm_hour; // 当前小时

	QL_TTS_LOG("tm.year: %d tm.mon: %d,tm.tm_mday: %d,tm.tm_hour: %d", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour);
	if ((currentHour >= 21 && currentHour < 24) || (currentHour >= 0 && currentHour <= 6))
	{
		QL_TTS_LOG("tts  time wrong!");
	}
	else
	{
		do
		{
			char *tts_str = "欢迎使用智慧小音箱.";
			int len = strlen(tts_str);
			ql_tts_play_lo(TTS_OPEN, tts_str, len, QL_TTS_UTF8, NULL, NULL);
		} while (0);
	}

	while (1)
	{
		lt_tts_msg_t msg;
		QL_TTS_LOG("tts ql_rtos_queue_wait");

		ql_rtos_queue_wait(lt_tts_msg_queue, (uint8 *)(&msg), sizeof(lt_tts_msg_t), 0xFFFFFFFF);
		QL_TTS_LOG("tts receive msg.");
		ql_rtos_task_sleep_s(1); //
		switch (msg.type)
		{
		case 0:
			if (msg.data != NULL)
			{
				ql_tts_play_lo(TTS_OPEN, (char *)msg.data, msg.datalen, msg.encoding, msg.before_start, msg.after_stop);
			}
			break;
		case 1:
			ql_tts_play_lo(TTS_CLOSE, (char *)msg.data, msg.datalen, msg.encoding, msg.before_start, msg.after_stop);
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

void formatNumbersForTTS(char *input, char *output) {
    int i = 0, j = 0;
    int inNumberSequence = 0;
    
    for (i = 0; input[i] != '\0'; i++) {
        if (isdigit((unsigned char)input[i])) {
            // 如果是连续数字，先加空格（第一个数字除外）
            if (inNumberSequence && j > 0) {
                output[j++] = ' ';
            }
            output[j++] = input[i];
            inNumberSequence = 1;
        } else {
            // 非数字字符，直接复制
            if (inNumberSequence) {
                inNumberSequence = 0;
            }
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
}

// tts 停播
void ltapi_stop_tts_encoding()
{
	ql_tts_play_lo(TTS_CLOSE, NULL, 0, QL_TTS_UTF8, NULL, NULL);
}
void lt_tts_callback_register(lt_tts_callback_t *reg)
{
	if (reg->lt_tts_start_handle)
	{
		tts_fsm.lt_tts_start_handle = reg->lt_tts_start_handle;
	}
	if (reg->lt_tts_stop_handle)
	{
		tts_fsm.lt_tts_stop_handle = reg->lt_tts_stop_handle;
	}
}

void ltapi_play_tts_encoding_withcallback(char *data, int data_len, ql_tts_encoding_e encoding, lt_tts_callback before_start, lt_tts_callback after_stop)
{
	QlOSStatus err = QL_OSI_SUCCESS;
	lt_tts_msg_t msg;
	msg.type = 0;
	msg.encoding = encoding;
	msg.datalen = data_len;
	msg.before_start = before_start;
	msg.after_stop = after_stop;
	msg.data = malloc(msg.datalen);
	memcpy(msg.data, data, msg.datalen);
	uint32 que_cnt = 0;
	ql_rtos_queue_get_cnt(lt_tts_msg_queue, &que_cnt);
	QL_TTS_LOG("ql_rtos_queue_get_cnt=%d", que_cnt);
	ql_rtos_queue_get_free_cnt(lt_tts_msg_queue, &que_cnt);
	QL_TTS_LOG("ql_rtos_queue_get_free_cnt=%d", que_cnt);

	if ((err = ql_rtos_queue_release(lt_tts_msg_queue, sizeof(lt_tts_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
	{
		QL_TTS_LOG("send msg to lt_tts_msg_queue failed, err=%d", err);
	}
	else
	{
		QL_TTS_LOG("send msg to lt_tts_msg_queue success");
	}
}

void ltapi_play_tts_encoding(char *data, int data_len, ql_tts_encoding_e encoding)
{
	ltapi_play_tts_encoding_withcallback(data, data_len, encoding, NULL, NULL);
}

void ltapi_play_tts_withcallback(char *data, int data_len, ql_tts_encoding_e encoding, lt_tts_callback before_start, lt_tts_callback after_stop)
{
	ltapi_play_tts_encoding_withcallback(data, data_len, encoding, before_start, after_stop);
}

void ltapi_play_tts(char *data, int data_len)
{
	ql_errcode_dev_e Temp;
	ql_memory_heap_state_t Stack_Test;
	Temp = ql_dev_memory_size_query(&Stack_Test);
	if (Temp == QL_DEV_SUCCESS)
	{
		QL_TTS_LOG(" Stack_Test.avail_size: %d", Stack_Test.avail_size);
		QL_TTS_LOG(" Stack_Test.total_size: %d", Stack_Test.total_size);
		QL_TTS_LOG(" Stack_Test.max_block_size: %d", Stack_Test.max_block_size);
	}
	ltapi_play_tts_encoding(data, data_len, QL_TTS_UTF8);
}

void lt_tts_demo_init(void)
{
	uint8_t err = QL_OSI_SUCCESS;

//	ql_set_volume(3); // 设置开机默认音量 tts语音用
	ql_rtos_queue_create(&lt_tts_msg_queue, sizeof(lt_tts_msg_t), 5);

	err = ql_rtos_task_create(&ql_tts_demo_task, QL_TTS_TASK_STACK, QL_TTS_TASK_PRIO, "ql_tts_task", ql_tts_thread, NULL, 10);

	if (err != QL_OSI_SUCCESS)
	{
		QL_TTS_LOG("TTS demo task created failed");
	}
}
