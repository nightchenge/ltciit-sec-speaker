/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2024-01-02 13:04:28
 * @LastEditors: zhouhao
 * @LastEditTime: 2024-11-18 11:09:05
 */
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
#include "ql_api_rtc.h"
#include "ql_api_dev.h"
#include "ltrecord.h"
#include "ltmqtt.h"

ql_queue_t lt_record_msg_queue;
typedef struct lt_record_msg
{
    int type;
    uint32_t datalen;
    uint8_t *data;
} lt_record_msg_t;

static char base64_table[] = {
     'A','B','C','D','E','F','G','H','I','J',
     'K','L','M','N','O','P','Q','R','S','T',
     'U','V','W','X','Y','Z','a','b','c','d',
     'e','f','g','h','i','j','k','l','m','n',
     'o','p','q','r','s','t','u','v','w','x',
     'y','z','0','1','2','3','4','5','6','7',
     '8','9','+', '/', '\0'
};

#define ltrecord_log(msg, ...) QL_LOG(QL_LOG_LEVEL_INFO, "lt_record", msg, ##__VA_ARGS__)

static char record_name[1024] = {0};
/**
 * @name: 
 * @description: 当前需要录音的文件名字
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
int record_generate_file_name(char *tel_num)
{
    QDIR *dir = NULL;
    char *path = "SD:/record/";

    if (!(dir = ql_opendir(path)))
    {
        ltrecord_log("fail to open directory %s!\n", path);
        ql_mkdir(path, 0);
    }
    memset(record_name, 0, sizeof(record_name));

    ql_rtc_time_t tm;
    ql_rtc_get_localtime(&tm);
    sprintf(record_name, "%s%s-%4d%02d%02d%02d%02d%02d.wav", path,tel_num, tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    ltrecord_log("record_name == %s\n", record_name);
    return 0;
}
/**
 * @name: 
 * @description: 具体录音接口 可调
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
void lt_record_file(int type)
{
    ql_aud_config config = {0};
    config.samplerate = 8000;
    ql_aud_adc_cfg adc_cfg = {0};
    adc_cfg.adc_gain = QL_ADC_GAIN_LEVEL_12;
    ql_aud_set_adc_gain(&adc_cfg);

    if (RECORD_STOP != type)
    {
        if (ql_aud_record_file_start(record_name, &config, type, NULL) != QL_AUDIO_SUCCESS)
        {
            ltrecord_log("record failed");
        }
    }
    else
    {
        ql_aud_record_stop();
        ltrecord_log("record finish, start play");
    }
}

/**
 * @name: 
 * @description: 外部调用的录音接口
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
void ltapi_record(int type, char *data, int data_len)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    lt_record_msg_t msg;
    msg.type = type;
    msg.datalen = data_len;
    msg.data = malloc(msg.datalen);
    if (msg.datalen != 0)
        memcpy(msg.data, data, msg.datalen);

    if ((err = ql_rtos_queue_release(lt_record_msg_queue, sizeof(lt_record_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
    {
        ltrecord_log("send msg to ltrecord_log failed, err=%d", err);
    }
    else
    {
        ltrecord_log("send msg to ltrecord_log success");
    }
}

static char wav_file_name[128] = {0};
#define MAX_REPORT_RECODE_DATA      9216    /* 一次上报录音数据最大值, 9 * 1024 因为要进行base64编码，所以最大值要是3的倍数 */
static unsigned int cmd = 0;

/**
 * @name: 
 * @description: wav格式文件名
 * @param {*}
 * @return {*}
 * @author: jinyiyou
 */
static void record_wav_file_name()
{
    QDIR *dir = NULL;
    char *path = "SD:/record/";

    if (!(dir = ql_opendir(path)))
    {
        ltrecord_log("fail to open directory %s!\n", path);
        ql_mkdir(path, 0);
    }

    memset(wav_file_name, 0, sizeof(wav_file_name));

    ql_rtc_time_t tm;
    ql_rtc_get_localtime(&tm);
    sprintf(wav_file_name, "%s%4d%02d%02d%02d%02d%02d.wav", path, tm.tm_year, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    
    if (dir != NULL)
    {
        ql_closedir(dir);
    }

    ltrecord_log("wav_file_name == %s\n", wav_file_name);

    return;
}

static void lt_record_wav_file()
{
    ql_aud_config config = {0};
    config.samplerate = 16000;
    ql_aud_adc_cfg adc_cfg = {0};
    adc_cfg.adc_gain = QL_ADC_GAIN_LEVEL_12;
    ql_aud_set_adc_gain(&adc_cfg);

    if (ql_aud_record_file_start(wav_file_name, &config, RECORD_MIC, NULL) != QL_AUDIO_SUCCESS)
    {
        ltrecord_log("record failed");
    }

    return;
}

static void base64_map(uint8_t *in_block, int len) 
{
    if (NULL == in_block)
    {
        return;
    }

    for (int i = 0; i < len; ++i) 
    {
        in_block[i] = base64_table[in_block[i]];
    }
    if (len % 4 == 3)
    {
        in_block[len] = '=';
    }
    else if (len % 4 == 2)
    {
        in_block[len] = in_block[len+1] = '=';
    }

    return;
}

static void base64_encode(char *in, int inlen, uint8_t *out) 
{
    char *in_block;
    uint8_t *out_block;
    char temp[3];
 
    out_block = out;
    in_block = in;
 
    for (int i = 0; i < inlen; i += 3) 
    {
        memset(temp, 0, 3);
        memcpy(temp, in_block, i + 3 < inlen ? 3 : inlen - i);
        memset(out_block, 0, 4);
 
        out_block[0] = (temp[0] >> 2) & 0x3f;
        out_block[1] = ((temp[0] << 4) & 0x30) | ((temp[1] >> 4) & 0x0f);
        out_block[2] = ((temp[1] << 2) & 0x3c) | ((temp[2] >> 6) & 0x03);
        out_block[3] = (temp[2]) & 0x3f;
        out_block += 4;
        in_block += 3;
    }
 
    base64_map(out, ((inlen * 4) - 1) / 3 + 1);
}

static char* read_record_data_from_file(QFILE fp, int len, char *recordInfo)
{
    char *b64 = NULL;
    unsigned int b64_len = 0;
    int readLen = 0;

    readLen = ql_fread(recordInfo, 1, len, fp);
    if (readLen != len)
    {
        ltrecord_log("fread [%d] failed: %d", len, readLen);
        return NULL;
    }

    if (0 == (len % 3))
    {
        b64_len = len / 3 * 4;
    }
    else
    {
        b64_len = (len / 3 + 1) * 4;
    }

    b64 = (char *)malloc(b64_len + 1);
    if (NULL == b64)
    {   
        ltrecord_log("malloc [%d] failed", b64_len);
        return NULL;
    }

    memset(b64, 0, b64_len + 1);
    base64_encode(recordInfo, len, (uint8_t *)b64);

    return b64;
}

static void lt_record_data_report(const char *path)
{
    QFILE fp;
    char *b64 = NULL;
    char *recordInfo = NULL;
    int fileLen = 0;
    unsigned int totalCnt = 0;
    unsigned int cnt = 0;
    
    ql_errcode_dev_e Temp;
    ql_memory_heap_state_t Stack_Test;

    if (NULL == path)
    {
        return;
    }

    fp = ql_fopen(path, "rb");
    if (fp < 0)
    {
        ltrecord_log("open %s failed", path);
        return;
    }

    ql_fseek(fp, 0L, SEEK_END);
    fileLen = ql_ftell(fp);
    ql_fseek(fp, 0, SEEK_SET);
    
    sprintf(cmdId, "100%d", cmd);

    if (0 == fileLen % MAX_REPORT_RECODE_DATA)
    {
        totalCnt = fileLen / MAX_REPORT_RECODE_DATA;
    }
    else
    {
        totalCnt = fileLen / MAX_REPORT_RECODE_DATA + 1;
    }

    while (fileLen > 0)
    {
        if (fileLen > MAX_REPORT_RECODE_DATA)
        {
            recordInfo = (char *)malloc(MAX_REPORT_RECODE_DATA + 1);
            if (NULL == recordInfo)
            {
                ltrecord_log("malloc failed");
                break;
            }

            memset(recordInfo, 0, MAX_REPORT_RECODE_DATA + 1);
            b64 = read_record_data_from_file(fp, MAX_REPORT_RECODE_DATA, recordInfo);
        }
        else
        {
            recordInfo = (char *)malloc(fileLen + 1);
            if (NULL == recordInfo)
            {
                ltrecord_log("malloc [%d] failed", fileLen + 1);
                break;
            }

            memset(recordInfo, 0, fileLen + 1);
            b64 = read_record_data_from_file(fp, fileLen, recordInfo);
        }

        cnt++;
    
        /* 推送语音指令查询请求 */
        if (b64 != NULL)
        {
            if (cnt > 1)
            {
                ql_rtos_task_sleep_ms(50);
            }

            Temp = ql_dev_memory_size_query(&Stack_Test);
            if (Temp == QL_DEV_SUCCESS)
            {
                ltrecord_log(" Stack_Test.avail_size: %d", Stack_Test.avail_size);
                ltrecord_log(" Stack_Test.total_size: %d", Stack_Test.total_size);
            }

            api_mqtt_recordData_Publish(totalCnt, cnt, cmdId, b64);
            free(b64);
            b64 = NULL;
        }

        if (recordInfo != NULL)
        {
            free(recordInfo);
            recordInfo = NULL;
        }

        fileLen -= MAX_REPORT_RECODE_DATA;
    }

    ql_fclose(fp);
    return;
}

void lt_record_data_handle()
{
	record_wav_file_name();
	lt_record_wav_file();
    return;
}

void lt_record_data_stop_handle()
{
    ql_aud_record_stop();
    cmd++;
	lt_record_data_report(wav_file_name);
    return;
}

/**
 * @name: 
 * @description: 处理消息的任务
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
void lt_record_msghandle_thread(void *param)
{
    ql_rtos_queue_create(&lt_record_msg_queue, sizeof(lt_record_msg_t), 5);

    while (1)
    {
        lt_record_msg_t msg;
        ltrecord_log("ltrecord ql_rtos_queue_wait");
        ql_rtos_queue_wait(lt_record_msg_queue, (uint8 *)(&msg), sizeof(lt_record_msg_t), 0xFFFFFFFF);
        ltrecord_log("ltrecord receive msg.");
        //	ql_rtos_task_sleep_s(5);
        switch (msg.type)
        {
        case RECORD_IDEL:
            break;
        case RECORD_MIC:
            lt_record_file(RECORD_MIC);
            break;
        case RECORD_VOICE:
            lt_record_file(RECORD_VOICE);
            break;
        case RECORD_STOP:
            lt_record_file(RECORD_STOP);
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

/**
 * @name: 
 * @description: 初始化
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
void lt_record_app_init(void)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    ql_task_t ql_record_task = NULL;
    ltrecord_log("lt_record_app_init enter");

    // lt_audio_pa_init();//初始化功放引脚

    // lt_audio_key_callback_register();//注册key 回调
    // lt_audio_play_callback_register(); //注册play回调

    err = ql_rtos_task_create(&ql_record_task, 8*1024, APP_PRIORITY_NORMAL, "lt_record", lt_record_msghandle_thread, NULL, 5);
    if (err != QL_OSI_SUCCESS)
    {
        ltrecord_log("ql_record_task create failed");
    }
}
