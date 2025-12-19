
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_api_osi.h"
#include "ql_api_nw.h"

#include "ql_log.h"
#include "ql_api_datacall.h"
#include "ql_mqttclient.h"

#include "ql_ssl.h"

#include "ebm_task_schedule.h"
#include "ebm_task_play.h"
#include "ebm_task_type.h"
#include "cJSON.h"
#include "ql_osi_def.h"
#include "ql_api_sim.h"
#include "ql_api_dev.h"
#include "ebs.h"
#include "ltmqtt.h"
#include "ltconf.h"
#include "ql_api_tts.h"

#include "lt_http.h"
#include "ltsms_t.h"
#include "led.h"
#include "ql_power.h"
#include "lt_tts_demo.h"
#include "ltclk_t.h"
#include "ltsystem.h"
#include "ltuart2frx8016.h"
#include "ltplay.h"
#include "ltadc.h"

#include "lt_http_demo.h"
#include "lt_fota_http.h"
#include "ltaudiourl.h"
#include "ltfmurl.h"
#include "ltsdmmc.h"
#include "ltvoicecall.h"
#include "ltrecord.h"
#include "lsm6ds3tr.h"
typedef struct lt_mqtt
{
    ql_task_t mqtt_task;
    ql_task_t mqtt_msg_task;
    ql_sem_t mqtt_semp;
    /* mqtt_client_url, set to mqtt://www.ltebs.cn:1883 OR mqtt://220.180.239.212:1883 */
    char mqtt_client_url[64];
    char mqtt_client_identity[64];
    char mqtt_client_user[64];
    char mqtt_client_pass[64];
    mqtt_client_t *mqtt_client;
    mqtt_radio_msg_t msg;

    int mqtt_connected;
    ql_queue_t mqtt_msg_queue;

    int (*f_battery_get)(); // 获取电池电量
    int (*f_sd_state_get)(); // 获取电池电量
    int (*f_freeblock_get)(); // 获取电池电量    
} lt_mqtt_t;

typedef struct mqtt_msg_dispose
{
    char flag[32];
    void (*funcDispose)(cJSON *data);
} mqtt_msg_dispose_t;

static lt_mqtt_t lt_mqtt = {
    .mqtt_client = NULL,
    .mqtt_connected = 0,
    //.mqtt_client_url = "mqtt://218.90.141.146:11883",
    .mqtt_client_url = "mqtt://mqtt.ctwing.cn:1883",
    .mqtt_client_user = "Yuanliu",
    .mqtt_client_pass = "lfH3o5NafEzlZSaVnfv_bP7D5rnIWndA-jIQFdjojNo",
    .mqtt_client_identity = "15106288862822068508039",

    // .f_battery_get = lt_uart2frx8016_battery_get
    .f_battery_get = lt_adc_battery_get,
    .f_freeblock_get = lt_get_freeblock,
    .f_sd_state_get = lt_sdmmc_get_state};
static char subtopic[2][32] = {"device_control"};
static char pubtopic[32] = "smartpension";
static char pubtopicfota[32] = "smartpension";
static int n = 0;

static mqtt_client_t mqtt_cli = {0};
char cmdId[32] = {0};
CellInfo cell;
#define QL_MQTT_LOG_LEVEL QL_LOG_LEVEL_INFO
#define QL_MQTT_LOG(msg, ...) QL_LOG(QL_MQTT_LOG_LEVEL, "lt_mqtt", msg, ##__VA_ARGS__)
#define QL_MQTT_LOG_PUSH(msg, ...) QL_LOG_PUSH("lt_mqtt", msg, ##__VA_ARGS__)

/*
 * 添加当前时间到JSON结构体
 */
static void _cJSON_AddTimeNowToObject(cJSON *const object, const char *const name)
{
    time_t _time = ebs_time(NULL);
    cJSON_AddNumberToObject(object, name, _time);
}

static void _cJSON_AddIMEINowToObject(cJSON *const object, const char *const name)
{
    char temp[16] = {0};
    if (ql_dev_get_imei(temp, sizeof(temp), 0) == 0)
    {
        cJSON_AddStringToObject(object, name, temp);
    }
}

static void _cJSON_AddIMSINowToObject(cJSON *const object, const char *const name)
{
    char imsi[16] = {0};
    if (ql_sim_get_imsi(0, imsi, sizeof(imsi)) == 0)
    {
        cJSON_AddStringToObject(object, name, imsi);
    }
}

static void _cJSON_AddICCIDNowToObject(cJSON *const object, const char *const name)
{
    char iccid[21] = {0};
    if (ql_sim_get_iccid(0, iccid, sizeof(iccid)) == 0)
    {
        cJSON_AddStringToObject(object, name, iccid);
    }
}

/***********************************************************
 * funcname		:_Publish (私有函数)
 * description	:
 *	mqtt发布数据时调用
 *	topic		[in]	[const char *] 发布的topic
 *	object		[in]    [cJSON *const] json对象
 *   infomation
 *
 ************************************************************/
static void _Publish(const char *topic, cJSON *const object)
{
    if (topic == NULL || object == NULL)
    {
        return;
    }

    char *payload = cJSON_PrintUnformatted(object);
    if (payload != NULL)
    {
        QlOSStatus err = QL_OSI_SUCCESS;
        lt_mqtt_msg_t msg;
        msg.type = SEND_MSG;
        msg.datalen = strlen(payload);
        msg.data = malloc(msg.datalen);
        memset(msg.topic, 0x00, sizeof(msg.topic));
        memcpy(msg.topic, topic, strlen(topic));
        memcpy(msg.data, payload, msg.datalen);
        
        if ((err = ql_rtos_queue_release(lt_mqtt.mqtt_msg_queue, sizeof(lt_mqtt_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
        {
            QL_MQTT_LOG("send msg to queue failed, err=%d", err);
        }

        free(payload);
        payload = NULL;
    }
}

// 2.1 告警数据上报
void api_mqtt_Sos_Publish(mqtt_alarm_msg_t *msg)
{
    cJSON *root = cJSON_CreateObject();
    _cJSON_AddTimeNowToObject(root, "sendTime");
    cJSON_AddStringToObject(root, "deviceType", "Huhutong");
    cJSON_AddNumberToObject(root, "dataType", 2);
    cJSON_AddNumberToObject(root, "alarmType", msg->alarmType);
    cJSON_AddNumberToObject(root, "alarmId", msg->alarmId);

    _cJSON_AddIMEINowToObject(root, "IMEI");
    _cJSON_AddIMSINowToObject(root, "IMSI");
    _cJSON_AddICCIDNowToObject(root, "ICCID");

    if (lt_mqtt.f_battery_get != NULL)
    {
        cJSON_AddNumberToObject(root, "battery", lt_mqtt.f_battery_get());
    }
    else
    {
        cJSON_AddNumberToObject(root, "battery", 0);
    }



    ql_nw_signal_strength_info_s info;
    ql_nw_get_signal_strength(0, &info);
    cJSON_AddNumberToObject(root, "signal", info.rssi);

    _Publish(pubtopic, root);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}

// 2.2 通话记录上报
void api_mqtt_CallLog_Publish(mqtt_call_msg_t *msg)
{
    cJSON *root = cJSON_CreateObject();
    _cJSON_AddTimeNowToObject(root, "sendTime");
    _cJSON_AddIMEINowToObject(root, "IMEI");
    _cJSON_AddIMSINowToObject(root, "IMSI");
    _cJSON_AddICCIDNowToObject(root, "ICCID");
    cJSON_AddNumberToObject(root, "dataType", 3);

    cJSON_AddNumberToObject(root, "callType", msg->callType);
    cJSON_AddNumberToObject(root, "callState", msg->callState);
    cJSON_AddStringToObject(root, "callNumber", msg->callNumber);
    cJSON_AddNumberToObject(root, "callStartTime", msg->callStartTime);
    cJSON_AddNumberToObject(root, "callEndTime", msg->callEndTime);
    cJSON_AddNumberToObject(root, "callTime", msg->callTime);

    cJSON_AddNumberToObject(root, "callId", msg->callId);


    _Publish(pubtopic, root);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}

// 2.3 设备信息上报, 触发条件：开机连上服务器后上报一包，往后每隔 24 小时上报一次
void api_mqtt_Status_Publish()
{
    char temp[640] = {0};
    cJSON *root = cJSON_CreateObject();

    _cJSON_AddTimeNowToObject(root, "sendTime");
    cJSON_AddStringToObject(root, "deviceType", "Huhutong");
    cJSON_AddNumberToObject(root, "dataType", 0);
    _cJSON_AddIMEINowToObject(root, "IMEI");
    _cJSON_AddIMSINowToObject(root, "IMSI");
    _cJSON_AddICCIDNowToObject(root, "ICCID");

    memset(temp, 0x00, sizeof(temp));
    mqtt_param_sosPhone_get(temp, sizeof(temp));
    cJSON_AddStringToObject(root, "sosPhone", temp);

    memset(temp, 0x00, sizeof(temp));
    mqtt_param_phone_get(temp, sizeof(temp), FAMILY_TYPE);
    cJSON_AddStringToObject(root, "familyPhone", temp);

    memset(temp, 0x00, sizeof(temp));
    mqtt_param_whiteList_get(temp, sizeof(temp));
    cJSON_AddStringToObject(root, "whiteList", temp);

    cJSON_AddStringToObject(root, "asr_version", get_asr_version());
    cJSON_AddStringToObject(root, "ble_version", get_ble_version());

    if (lt_mqtt.f_battery_get != NULL)
    {
        cJSON_AddNumberToObject(root, "battery", lt_mqtt.f_battery_get());
    }
    else
    {
        cJSON_AddNumberToObject(root, "battery", 0);
    }

    if (lt_mqtt.f_freeblock_get != NULL)
    {
        cJSON_AddNumberToObject(root, "freeblock", lt_mqtt.f_freeblock_get());
    }
    else
    {
        cJSON_AddNumberToObject(root, "freeblock", 0);
    }

    if (lt_mqtt.f_sd_state_get != NULL)
    {
        cJSON_AddNumberToObject(root, "sd_state", lt_mqtt.f_sd_state_get());
    }
    else
    {
        cJSON_AddNumberToObject(root, "sd_state", 0);
    }

    //新增led开关上报
    cJSON_AddNumberToObject(root, "ledEnable", mqtt_param_ledEnable_get());
    // 新增亮度和亮屏时间上报
    cJSON_AddNumberToObject(root, "ledBrightness", mqtt_param_ledBrightness_get());
    cJSON_AddNumberToObject(root, "ledScreenOnTime", mqtt_param_ledScreenOnTime_get());

    unsigned char signal;
    ql_nw_get_csq(0, &signal);
    cJSON_AddNumberToObject(root, "signal", signal);

    cJSON_AddStringToObject(root, "hardVersion", HARDWARE_VERSION);
    cJSON_AddStringToObject(root, "moduleVersion", SOFTWARE_VERSION);
    cJSON_AddNumberToObject(root, "activationState", 1);

    cJSON_AddNumberToObject(root, "whiteListState", mqtt_param_whiteListState_get());
    cJSON_AddNumberToObject(root, "whiteListDisableTime", mqtt_param_whiteListDisableTime_get());

    // 新增跌倒检测参数上报
    int ff_en = 0,ff_th = 3,ff_dur = 10;
    mqtt_param_fallDetect_get(&ff_en, &ff_th, &ff_dur);
    memset(temp, 0x00, sizeof(temp));
    sprintf(temp, "%d,%d,%d", ff_en, ff_th, ff_dur);

    cJSON_AddStringToObject(root, "fallDetectionCfg", temp);

   // CellInfo cell;
    lt_get_gnsss_t(&cell);
    sprintf(temp, "http://api.cellocation.com:84/cell/?coord=bd09&output=json&mcc=%d&mnc=%d&lac=%d&ci=%d", cell.mcc, cell.mnc, cell.lac, cell.cellid);
    lt_http_get_api_request(temp);

    _Publish(pubtopic, root);

    if (root != NULL)
    {       
        cJSON_Delete(root);
    }
}
// 2.3.x 位置上报20251024

void api_mqtt_location_pulish(char *buf)
{
    cJSON *root = cJSON_CreateObject();

    _cJSON_AddTimeNowToObject(root, "sendTime");
    cJSON_AddStringToObject(root, "deviceType", "Huhutong");
    cJSON_AddNumberToObject(root, "dataType", 0);
    _cJSON_AddIMEINowToObject(root, "IMEI");
    _cJSON_AddIMSINowToObject(root, "IMSI");
    _cJSON_AddICCIDNowToObject(root, "ICCID");
    // 新增 cell 信息
    cJSON *cellObj = cJSON_CreateObject();
    // lt_get_gnsss_t(&cell);
    QL_MQTT_LOG("mcc == %d,mnc == %d,lac == %d,cellid == %d\n", cell.mcc, cell.mnc, cell.lac, cell.cellid);

    cJSON_AddNumberToObject(cellObj, "mcc", cell.mcc);
    cJSON_AddNumberToObject(cellObj, "mnc", cell.mnc);
    cJSON_AddNumberToObject(cellObj, "lac", cell.lac);
    cJSON_AddNumberToObject(cellObj, "cellid", cell.cellid);

    if (buf != NULL) {
        cJSON *locationObj = cJSON_Parse(buf);
        if (locationObj) {
            cJSON_AddItemToObject(root, "location", locationObj);
        } else {
            QL_MQTT_LOG("Failed to parse location JSON");
        }
    }

  //  cJSON_AddStringToObject(cellObj, "location", buf);
    cJSON_AddItemToObject(root, "cell", cellObj);

    _Publish(pubtopic, root);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}

// 2.x sd卡有变化就上报, 触发条件：sd卡有拔插动作时上报
void api_mqtt_sd_state_Publish()
{
    cJSON *root = cJSON_CreateObject();

    _cJSON_AddTimeNowToObject(root, "sendTime");
    cJSON_AddStringToObject(root, "deviceType", "Huhutong");
    cJSON_AddNumberToObject(root, "dataType", 0);
    _cJSON_AddIMEINowToObject(root, "IMEI");
    _cJSON_AddIMSINowToObject(root, "IMSI");
    _cJSON_AddICCIDNowToObject(root, "ICCID");

    if (lt_mqtt.f_sd_state_get != NULL)
    {
        cJSON_AddNumberToObject(root, "sd_state", lt_mqtt.f_sd_state_get());
    }
    else
    {
        cJSON_AddNumberToObject(root, "sd_state", 0);
    }
    _Publish(pubtopic, root);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}


// 2.4 获取 SOS 告警电话请求上报
void api_mqtt_sosPhone_Publish()
{
    cJSON *root = cJSON_CreateObject();

    _cJSON_AddTimeNowToObject(root, "sendTime");
    cJSON_AddNumberToObject(root, "dataType", 5);
    _cJSON_AddIMEINowToObject(root, "IMEI");
    _cJSON_AddIMSINowToObject(root, "IMSI");
    _cJSON_AddICCIDNowToObject(root, "ICCID");

    _Publish(pubtopic, root);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}

// 2.5 广播播放结果上报
void api_mqtt_BroadcastRasult_Publish(mqtt_radio_msg_t *msg)
{
    cJSON *root = cJSON_CreateObject();
    _cJSON_AddTimeNowToObject(root, "sendTime");
    cJSON_AddNumberToObject(root, "dataType", 6);
    _cJSON_AddIMEINowToObject(root, "IMEI");

    cJSON_AddNumberToObject(root, "playTime", msg->playTime);
    cJSON_AddStringToObject(root, "radioID", msg->radioID);
    cJSON_AddNumberToObject(root, "audioResult", msg->audioResult);

    _Publish(pubtopic, root);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}

// 2.6 指令下发应答上报
// { "sendTime":1620887971, “cmdID”:"1370281908836700162", "IMEI": "12345678912345", "dataType":7, "cmdResult":0}
void api_mqtt_CmdResult_Publish(const char *const cmdID, char cmdResult)
{
    cJSON *root = cJSON_CreateObject();
    _cJSON_AddTimeNowToObject(root, "sendTime");
    cJSON_AddStringToObject(root, "cmdID", cmdID);
    _cJSON_AddIMEINowToObject(root, "IMEI");
    cJSON_AddNumberToObject(root, "dataType", 7);
    cJSON_AddNumberToObject(root, "cmdResult", cmdResult);

    _Publish(pubtopic, root);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}

// 2.7 白名单配置信息上报（SOS 触发会上报/白名单配置更改后会上报）
//  { "sendTime":1620887971, "IMEI": "12345678912345", "dataType":7, "whiteListState":0, "whiteListDisableTime":86400}
void api_mqtt_SosWiteList_Publish()
{
    cJSON *root = NULL;
    root = cJSON_CreateObject();
    _cJSON_AddTimeNowToObject(root, "sendTime");
    _cJSON_AddIMEINowToObject(root, "IMEI");
    cJSON_AddNumberToObject(root, "dataType", 8);

    cJSON_AddNumberToObject(root, "whiteListState", 0);
    cJSON_AddNumberToObject(root, "whiteListDisableTime", 86400);

    _Publish(pubtopic, root);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}

// 2.8 按键电话事件通知
//{ "sendTime":1620887971, "IMEI": "12345678912345", "dataType":10, "keyphoneType":0}
void api_yuanLiuMqtt_KeyPhone_Publish(char keyphoneType)
{
    cJSON *root = cJSON_CreateObject();
    _cJSON_AddTimeNowToObject(root, "sendTime");
    _cJSON_AddIMEINowToObject(root, "IMEI");
    cJSON_AddNumberToObject(root, "dataType", 10);

    cJSON_AddNumberToObject(root, "keyphoneType", keyphoneType);

    _Publish(pubtopic, root);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}

void api_mqtt_fota_Publish(int stepval)
{
    cJSON *root = cJSON_CreateObject();
    // _cJSON_AddTimeNowToObject(root, "sendTime");
    cJSON_AddStringToObject(root, "dataType", "progress");
    cJSON_AddNumberToObject(root, "step", stepval);
    _cJSON_AddIMEINowToObject(root, "nodeId");
    _cJSON_AddTimeNowToObject(root, "upgradeTime");

    _Publish(pubtopicfota, root);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}
/* 2.10 天气查询请求 */
void api_mqtt_weather_Publish()
{
    cJSON *root = NULL;
    root = cJSON_CreateObject();
    _cJSON_AddTimeNowToObject(root, "sendTime");
    _cJSON_AddIMEINowToObject(root, "IMEI");
    cJSON_AddNumberToObject(root, "dataType", 11);
    _Publish(pubtopic, root);
    QL_MQTT_LOG("weather data publish\n");
    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}
/* 2.10 语音指令查询请求 */
void api_mqtt_recordData_Publish(unsigned int totalNum, unsigned int curNum, char *cmdID, char *recordData)
{
    cJSON *root = NULL;
    root = cJSON_CreateObject();
    _cJSON_AddTimeNowToObject(root, "sendTime");
    _cJSON_AddIMEINowToObject(root, "IMEI");
    cJSON_AddNumberToObject(root, "dataType", 12);
    cJSON_AddStringToObject(root, "recordData", recordData);
    cJSON_AddStringToObject(root, "cmdID", cmdID);
    cJSON_AddNumberToObject(root, "totalNum", totalNum);
    cJSON_AddNumberToObject(root, "curNum", curNum);

    _Publish(pubtopic, root);
    QL_MQTT_LOG("record data publish\n");
    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}

static void ebm_task_stop_cb(void *data)
{
    char ebm_id[64] = {0};
    ebm_task_t *task = (ebm_task_t *)data;
    ebm_task_msg_t ebm_msg = {0};

    if (data == NULL)
    {
        return;
    }

    QL_MQTT_LOG("ebm_task_stop_cb OK!");

    ebm_msg.type = TASK_DEL;
    memcpy(ebm_msg.buff, task->ebm_id, sizeof(task->ebm_id));
    if (eb_player_task_msg_send(&ebm_msg) == TASK_RESULT_OK)
    {
        QL_MQTT_LOG("eb_player_task_msg_send delete");
    }

    do
    {
        for (int i = 0; i < EBM_ID_LEN; i++)
        {
            sprintf(&ebm_id[i * 2], "%02X", task->ebm_id[i]);
        }

        mqtt_radio_msg_t r_msg = {
            .audioResult = 3};
        memcpy(r_msg.radioID, ebm_id, 19);
        QL_MQTT_LOG("delete r_msg.radioID =%s", r_msg.radioID);

        api_mqtt_BroadcastRasult_Publish(&r_msg);
    } while (0);
    // if (SND_TEL != ltplay_get_src())
    // {
    //     set_function_state(RT_TIME, 0);
    // }

    lt_tts_instruct_stop(TTS_TYPE_EBS);
}

//跌到预警上报  fall_state : 0正常 1跌到预警 2跌到未处理 3跌到已处理
void api_mqtt_fall_publish(int fall_state)
{
    cJSON *root = NULL;
    root = cJSON_CreateObject();
    _cJSON_AddTimeNowToObject(root, "sendTime");
    cJSON_AddStringToObject(root, "deviceType", "Huhutong");
    _cJSON_AddIMEINowToObject(root, "IMEI");
    cJSON_AddNumberToObject(root, "dataType", 14);

    cJSON *specialEvent = cJSON_CreateObject();
    cJSON_AddNumberToObject(specialEvent, "Fall", fall_state);

    cJSON_AddItemToObject(root, "specialEvent", specialEvent);

    _Publish(pubtopic, root);

    if (root != NULL)
    {
        cJSON_Delete(root);
    }
}

// clock
static void clock_msg_start_cb(void *id, void *data, int data_len)
{
    ltplay_check_play(SND_EBS);
    if (data == NULL)
    {
        return;
    }
    ebm_task_t task = {0};
    ebm_task_msg_t ebm_msg = {0};
    task.status = TASK_STATUS_WAIT;
    task.chn_mask = CHN_IP_MASK;
    task.next = task.prev = NULL;
    // dec_str2bcd_arr(id, task.ebm_id, sizeof(task.ebm_id));
    memset(task.ebm_id, 0xa0, sizeof(task.ebm_id));
    task.ebm_type = 4;
    task.severity = 2;
    task.volume = 100;
    task.start_time = 0;
    task.stop_time = 0xFFFFFFFF;
    task.task_source = TASK_SOURCE_CLK;
    memcpy(task.url, data, data_len);
    task.callback = ebm_task_stop_cb;

    memcpy(ebm_msg.buff, (char *)&task, sizeof(ebm_task_t));
    ebm_msg.type = TASK_ADD;
    if (eb_player_task_msg_send(&ebm_msg) != TASK_RESULT_OK)
    {
        QL_MQTT_LOG("Lcoal task of U-disk add failed.\n");
    }
}

static void clock_msg_stop_cb()
{
    char ebm_id[18] = {0};
    ebm_task_msg_t ebm_msg = {0};
    //   mqtt_radio_msg_t *radio_msg = (mqtt_radio_msg_t *)data;
    ebm_msg.type = TASK_DEL;
    //    dec_str2bcd_arr(radio_msg->radioID, (uint8_t *)&ebm_id, sizeof(ebm_id));
    memset(ebm_id, 0xa0, sizeof(ebm_id));
    memcpy(ebm_msg.buff, ebm_id, sizeof(ebm_id));
    if (eb_player_task_msg_send(&ebm_msg) == TASK_RESULT_OK)
    {
        QL_MQTT_LOG("eb_player_task_msg_send delete");
    }
}

//

static void nomal_msg_start_cb(void *data, int data_len)
{
    if (data == NULL)
    {
        return;
    }
    mqtt_radio_msg_t *radio_msg = (mqtt_radio_msg_t *)data;
    ebm_task_t task = {0};
    ebm_task_msg_t ebm_msg = {0};
    task.status = TASK_STATUS_WAIT;
    task.chn_mask = CHN_IP_MASK;
    task.next = task.prev = NULL;
    dec_str2bcd_arr(radio_msg->radioID, task.ebm_id, sizeof(task.ebm_id));
    task.ebm_type = 4;
    task.severity = 4;
    task.volume = 100;
    task.start_time = 0;
    task.stop_time = 0xFFFFFFFF;
    task.task_source = TASK_SOURCE_MQTT;
    memcpy(task.url, radio_msg->url, strlen(radio_msg->url));
    task.callback = ebm_task_stop_cb;

    memcpy(ebm_msg.buff, (char *)&task, sizeof(ebm_task_t));
    ebm_msg.type = TASK_ADD;
    if (eb_player_task_msg_send(&ebm_msg) != TASK_RESULT_OK)
    {
        QL_MQTT_LOG("Lcoal task of U-disk add failed.\n");
    }

    do
    {
        radio_msg->audioResult = 1;
        api_mqtt_BroadcastRasult_Publish(radio_msg);
    } while (0);
}

static void nomal_msg_stop_cb(void *data, int data_len)
{
    if (data == NULL)
    {
        return;
    }
    char ebm_id[18] = {0};
    ebm_task_msg_t ebm_msg = {0};
    mqtt_radio_msg_t *radio_msg = (mqtt_radio_msg_t *)data;
    ebm_msg.type = TASK_DEL;
    dec_str2bcd_arr(radio_msg->radioID, (uint8_t *)&ebm_id, sizeof(ebm_id));
    memcpy(ebm_msg.buff, ebm_id, sizeof(ebm_id));
    if (eb_player_task_msg_send(&ebm_msg) == TASK_RESULT_OK)
    {
        QL_MQTT_LOG("eb_player_task_msg_send delete");
    }

    do
    {
        radio_msg->audioResult = 3;
        api_mqtt_BroadcastRasult_Publish(radio_msg);
    } while (0);
}

//
// DATA "cmdID":"1370281908836700162", "sendTime": 1620887971, "radioType": 1, "radioID": "1370281908836700162", "url": "http: //120.24.242.14:8080/test"
//
static void func_radioType(cJSON *data)
{

    if (IDLE_MODE != ltget_lp_status()) // 如果按键的第一下屏幕是灭的话 直接先唤醒
    {
        char *len_on = "len_on";
        lt_panel_light_msg(0, len_on, strlen(len_on) + 1);
    }

    cJSON *tmp = cJSON_GetObjectItem(data, "radioType");
    cJSON *url = cJSON_GetObjectItem(data, "url");
    cJSON *radioID = cJSON_GetObjectItem(data, "radioID");
    if (tmp != NULL && tmp->valueint == 1) // 普通广播
    {
        if (url != NULL && radioID != NULL)
        {
            mqtt_radio_msg_t radio_msg = {
                .playTime = ebs_time(NULL),
                .audioResult = 3};
            memcpy(radio_msg.url, url->valuestring, strlen(url->valuestring));
            memcpy(radio_msg.radioID, radioID->valuestring, strlen(radioID->valuestring));

            // 给信息队列，按的时候再触发开播，播放的时候再按就停止。
            lt_tts_instruct_add(TTS_TYPE_EBS, &radio_msg, sizeof(mqtt_radio_msg_t), nomal_msg_start_cb, nomal_msg_stop_cb);
        }
    }
    else if (tmp != NULL && tmp->valueint == 2) // 紧急广播
    {
        if (url != NULL && radioID != NULL)
        {

            ebm_task_t task = {0};
            ebm_task_msg_t ebm_msg = {0};
            task.status = TASK_STATUS_WAIT;
            task.chn_mask = CHN_IP_MASK;
            task.next = task.prev = NULL;
            dec_str2bcd_arr(radioID->valuestring, task.ebm_id, sizeof(task.ebm_id));
            task.ebm_type = 4;
            task.severity = 4;
            task.volume = 100;
            task.start_time = 0;
            task.stop_time = 0xFFFFFFFF;
            task.task_source = TASK_SOURCE_MQTT_2;
            memcpy(task.url, url->valuestring, strlen(url->valuestring));
            task.callback = ebm_task_stop_cb;
QL_MQTT_LOG("url->valuestring ==%s\n",url->valuestring);
            memcpy(ebm_msg.buff, (char *)&task, sizeof(ebm_task_t));
            ebm_msg.type = TASK_ADD;
            if (SND_TEL != ltplay_get_src())
            {
                ltplay_check_play(SND_EBS);
                if (eb_player_task_msg_send(&ebm_msg) != TASK_RESULT_OK)
                {
                    QL_MQTT_LOG("Lcoal task of U-disk add failed.\n");
                }
            }
            // if (eb_player_task_msg_send(&ebm_msg) != TASK_RESULT_OK)
            // {
            //     QL_MQTT_LOG("Lcoal task of U-disk add failed.\n");
            // }

            // set_function_state(YL, 2);

            do
            {
                mqtt_radio_msg_t r_msg = {
                    .playTime = ebs_time(NULL),
                    .audioResult = 1,
                };
                memcpy(r_msg.radioID, radioID->valuestring, strlen(radioID->valuestring));
                QL_MQTT_LOG("delete r_msg.radioID =%s", r_msg.radioID);
                api_mqtt_BroadcastRasult_Publish(&r_msg);
            } while (0);
        }
    }
    else if (tmp != NULL && tmp->valueint == 3) // 养生广播
    {
        if (url != NULL && radioID != NULL)
        {

            ebm_task_t task = {0};
            ebm_task_msg_t ebm_msg = {0};
            task.status = TASK_STATUS_WAIT;
            task.chn_mask = CHN_IP_MASK;
            task.next = task.prev = NULL;
            dec_str2bcd_arr(radioID->valuestring, task.ebm_id, sizeof(task.ebm_id));
            task.ebm_type = 5;
            task.severity = 4;
            task.volume = 100;
            task.start_time = 0;
            task.stop_time = 0xFFFFFFFF;
            task.task_source = TASK_SOURCE_MQTT_3;
            memcpy(task.url, url->valuestring, strlen(url->valuestring));
            task.callback = ebm_task_stop_cb;

            memcpy(ebm_msg.buff, (char *)&task, sizeof(ebm_task_t));
            ebm_msg.type = TASK_ADD;

            if (SND_TEL != ltplay_get_src())
            {
                ltplay_check_play(SND_EBS);
                if (eb_player_task_msg_send(&ebm_msg) != TASK_RESULT_OK)
                {
                    QL_MQTT_LOG("Lcoal task of U-disk add failed.\n");
                }
            }

            // set_function_state(YL, 3);

            do
            {
                mqtt_radio_msg_t r_msg = {
                    .playTime = ebs_time(NULL),
                    .audioResult = 1,
                };
                memcpy(r_msg.radioID, radioID->valuestring, strlen(radioID->valuestring));
                api_mqtt_BroadcastRasult_Publish(&lt_mqtt.msg);
            } while (0);
        }
    }

    do
    {
        cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
        if (cmdid != NULL)
        {
            api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
        }
        else
        {
            api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
        }
    } while (0);
}

// 配置亲情号码{"cmdID": "1370281908836700162", "sendTime": 1620887971, "familyPhone": "19860841689"}
// 配置白名单{"cmdID": "1370281908836700162", "sendTime": 1620887971, "whiteList": "13476572345,10976542673", "whiteListName": "张三,李四"}
// 配置亲情号码和白名单{"cmdID": "1370281908836700162", "sendTime": 1620887971,"familyName": "张三", "familyPhone": "19860841689", "whiteList": "13476572345,10976542673", "whiteListName": "张三,李四"}



static void func_telephone(cJSON *data)
{   
    cJSON *obj = NULL;

    obj = cJSON_GetObjectItem(data, "sosPhone");
    if (obj != NULL)
    {
        mqtt_param_sosPhone_set(obj->valuestring, strlen(obj->valuestring));
    }

    obj = cJSON_GetObjectItem(data, "familyName");
    if (obj != NULL)
    {
        mqtt_param_phone_set(obj->valuestring, strlen(obj->valuestring), FAMILY_NAME_TYPE);
    }

    obj = cJSON_GetObjectItem(data, "familyPhone");
    if (obj != NULL)
    {
        mqtt_param_phone_set(obj->valuestring, strlen(obj->valuestring), FAMILY_TYPE);
    }

    obj = cJSON_GetObjectItem(data, "electricServicePhone");
    if (obj != NULL)
    {
        mqtt_param_phone_set(obj->valuestring, strlen(obj->valuestring), ELECTRIC_SERVICE_TYPE);
    }

    obj = cJSON_GetObjectItem(data, "waterServicePhone");
    if (obj != NULL)
    {
        mqtt_param_phone_set(obj->valuestring, strlen(obj->valuestring), WATER_SERVICE_TYPE);
    }

    obj = cJSON_GetObjectItem(data, "gasServicePhone");
    if (obj != NULL)
    {
        mqtt_param_phone_set(obj->valuestring, strlen(obj->valuestring), GAS_SERVICE_TYPE);
    }

    obj = cJSON_GetObjectItem(data, "whiteList");
    if (obj != NULL)
    {
        mqtt_param_whiteList_set(obj->valuestring, strlen(obj->valuestring));
    }

    obj = cJSON_GetObjectItem(data, "whiteListName");
    if (obj != NULL)
    {
        mqtt_param_whiteListName_set(obj->valuestring, strlen(obj->valuestring));
    }

    obj = cJSON_GetObjectItem(data, "cmdID");
    if (obj != NULL)
    {
        api_mqtt_CmdResult_Publish(obj->valuestring, 1);
    }
    else
    {
        api_mqtt_CmdResult_Publish(obj->valuestring, 0);
    }
}

// 3.12 删除 mqtt 配置信息
// { "cmdID": "1370281908836700162", "sendTime": 1620887978, "mqttCfgDelete": 1}
static void func_mqttCfgDelete(cJSON *data)
{
    QL_MQTT_LOG("func_mqttCfgDelete");

    mqtt_param_reset();

    cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");

    if (cmdid != NULL)
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
    }
    else
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
    }
}

// static void func_sosPhone(cJSON *data)
// {
//     QL_MQTT_LOG("func_sosPhone");

//     cJSON *obj = NULL;
//     obj = cJSON_GetObjectItem(data, "sosPhone");
//     if (obj != NULL)
//     {
//         mqtt_param_sosPhone_set(obj->valuestring, strlen(obj->valuestring));
//     }

//     cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
//     if (cmdid != NULL)
//     {
//         api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
//     }
//     else
//     {
//         api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
//     }
// }
// DATA "cmdID":"1370281908836700162", "sendTime": 1620887971, "clockID": "1370281908836700162", "clockAction": "1", "alarmClockSet" : ""10#30#1111111#1", "clockUrl": "https://test001.aliyuncs.com/2021/10/22/test.mp3 "
static void func_clockID(cJSON *data)
{
    QL_MQTT_LOG("func_clockID");

    // 提取字段数据
    // cJSON *sendTime = cJSON_GetObjectItem(data, "sendTime");
    cJSON *clockID = cJSON_GetObjectItem(data, "clockID");
    cJSON *clockAction = cJSON_GetObjectItem(data, "clockAction");
    cJSON *alarmClockSet = cJSON_GetObjectItem(data, "alarmClockSet");
    cJSON *clockUrl = cJSON_GetObjectItem(data, "clockUrl");

    // QL_MQTT_LOG("cmdID: %s\n", cmdID->valuestring);
    QL_MQTT_LOG("clockID: %s\n", clockID->valuestring);
    QL_MQTT_LOG("clockAction: %d\n", clockAction->valueint);
    if (alarmClockSet != NULL)
        QL_MQTT_LOG("alarmClockSet: %s\n", alarmClockSet->valuestring);
    if (clockUrl != NULL)
        QL_MQTT_LOG("clockUrl: %s\n", clockUrl->valuestring);

    // 闹钟开启
    //  if(clockAction != NULL && clockAction->valueint == 1)
    if (clockAction != NULL)
    {
        if (clockUrl != NULL && clockID != NULL && clockAction->valueint != 0 && clockAction->valueint != 3)
        {
            // int lt_clk_instruct_add(char *clockID, int  clockAction, char *  alarmClockSet, char * clockUrl,int url_len,void (*start_handle)(void *buf, int date_len), void (*stop_handle)(void *buf, int date_len))
            lt_clk_instruct_add(clockID->valuestring, clockAction->valueint, alarmClockSet->valuestring, clockUrl->valuestring, strlen((const char *)clockUrl->valuestring), clock_msg_start_cb, clock_msg_stop_cb);
        }
        if (clockAction->valueint == 0 || clockAction->valueint == 3)
        {
            lt_clk_instruct_del(clockID->valuestring);
        }
    }

    cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
    if (cmdid != NULL)
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
    }
    else
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
    }
}

static void func_whiteListState(cJSON *data)
{
    QL_MQTT_LOG("func_whiteListState");

    cJSON *obj = NULL;
    obj = cJSON_GetObjectItem(data, "whiteListState");
    if (obj != NULL)
    {
        mqtt_param_whiteListState_set(obj->valueint);
    }

    obj = cJSON_GetObjectItem(data, "whiteListDisableTime");
    if (obj != NULL)
    {
        mqtt_param_whiteListDisableTime_set(obj->valueint);
    }

    cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
    if (cmdid != NULL)
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
    }
    else
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
    }
}
static void mqtt_reboot()
{
    ltapi_play_tts(TTS_STR_SYS_REBOOT_5S, strlen(TTS_STR_SYS_REBOOT_5S));
    ql_rtos_task_sleep_s(5);
    ql_power_reset(RESET_NORMAL);
}
static void func_mqttProductID(cJSON *data)
{
    QL_MQTT_LOG("func_mqttProductID");

    cJSON *mqttProductID = NULL;
    cJSON *mqttUserName = NULL;
    cJSON *mqttPassword = NULL;
    cJSON *mqttServerIP = NULL;
    cJSON *mqttPort = NULL;

    mqttProductID = cJSON_GetObjectItem(data, "mqttProductID");

    mqttUserName = cJSON_GetObjectItem(data, "mqttUserName");
    mqttPassword = cJSON_GetObjectItem(data, "mqttPassword");
    mqttServerIP = cJSON_GetObjectItem(data, "mqttServerIP");
    mqttPort = cJSON_GetObjectItem(data, "mqttPort");
    if (NULL != mqttProductID && NULL != mqttUserName && NULL != mqttPassword && NULL != mqttServerIP && NULL != mqttPort)
    {
        if (0 != strlen(mqttProductID->valuestring) && 0 != strlen(mqttUserName->valuestring) && 0 != strlen(mqttPassword->valuestring) && 0 != strlen(mqttServerIP->valuestring) && 0 != mqttPort->valueint)
        {
            mqtt_param_client_identity_set(mqttProductID->valuestring, strlen(mqttProductID->valuestring));
            mqtt_param_client_user_set(mqttUserName->valuestring, strlen(mqttUserName->valuestring));
            mqtt_param_client_pass_set(mqttPassword->valuestring, strlen(mqttPassword->valuestring));
            mqtt_param_mqttServer_set(mqttServerIP->valuestring, strlen(mqttServerIP->valuestring));
            mqtt_param_mqttPort_set(mqttPort->valueint);
            mqtt_reboot();
        }
        else
        {
            QL_MQTT_LOG("func_mqttProductID have wrong data");
        }
    }
    else
    {
        QL_MQTT_LOG("func_mqttProductID have NULl data");
    }
    cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
    if (cmdid != NULL)
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
    }
    else
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
    }
}

static void func_cancleCardBinding(cJSON *data)
{
    QL_MQTT_LOG("func_cancleCardBinding");

    cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
    if (cmdid != NULL)
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
    }
    else
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
    }
}


static void func_clearSetting(cJSON *data)
{
    QL_MQTT_LOG("func_clearSetting");

    cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
    if (cmdid != NULL)
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
    }
    else
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
    }
}

static void func_ledEnable(cJSON *data)
{
    QL_MQTT_LOG("func_ledEnable");

    cJSON *obj = NULL;
    obj = cJSON_GetObjectItem(data, "ledEnable");
    if (obj != NULL)
    {
        mqtt_param_ledEnable_set(obj->valueint);
    }
    // 新增亮度和亮屏时间设置
    obj = cJSON_GetObjectItem(data, "ledBrightness");
    if (obj != NULL)
    {
        mqtt_param_ledBrightness_set(obj->valueint);
        lt_panel_light_msg(2, NULL, 0);//设置led亮度
    }
    obj = cJSON_GetObjectItem(data, "ledScreenOnTime");
    if (obj != NULL)
    {
        mqtt_param_ledScreenOnTime_set(obj->valueint);
        ltset_lp_ledbr(mqtt_param_ledScreenOnTime_get());//设置lp led亮屏时间
    }

    cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
    if (cmdid != NULL)
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
    }
    else
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
    }
}

static void func_ttsCmd(cJSON *data)
{

    cJSON *obj = NULL;
    obj = cJSON_GetObjectItem(data, "ttsCmd");
    if (obj != NULL)
    {
        ltapi_play_tts(obj->valuestring, strlen(obj->valuestring));
        //mqtt_param_ledEnable_set(obj->valueint);

    }

    cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
    if (cmdid != NULL)
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
    }
    else
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
    }
}

static void func_reboot(cJSON *data)
{
    cJSON *reboot = cJSON_GetObjectItem(data, "reboot");
    if (reboot != NULL && reboot->valueint == 1)
    {
        mqtt_reboot();
        // char *data = "设备将在5秒后重启.";
        // ltapi_play_tts(data, strlen(data));
        // ql_rtos_task_sleep_s(5);
        // ql_power_reset(RESET_NORMAL);
    }
}
static void func_fota(cJSON *data)
{
    QL_MQTT_LOG("func_fota");
    cJSON *payload = cJSON_GetObjectItem(data, "payload");
    if (payload != NULL)
    {
        cJSON *module = cJSON_GetObjectItem(payload, "module");
        if(module != NULL)
        {
            QL_MQTT_LOG("module->valuestring = %s",module->valuestring);
            if(strcmp(module->valuestring,"mcu") == 0)
            {
                // 主控升级
                cJSON *fotaurl = cJSON_GetObjectItem(payload, "url");
                if (fotaurl != NULL)
                {
                    ltset_update_mode();
                    set_function_state(NEW_INFO, 0);
                    ql_rtos_task_sleep_s(5);   
                    lt_ql_fota_start(fotaurl->valuestring);
                }
            }
            else if(strcmp(module->valuestring,"ble") == 0)
            {
                // 单片机升级
                cJSON *fotaurl = cJSON_GetObjectItem(payload, "url");
                cJSON *fotamd5 = cJSON_GetObjectItem(payload, "md5");
                char local_filename[] = "ble_ota.bin";
                if (fotaurl != NULL && fotamd5 != NULL)
                {
                    if(lt_http_dload_ble(local_filename, fotaurl->valuestring, fotamd5->valuestring) == 0)
                    {
                        QL_MQTT_LOG("MQTT_TEST:download ble_ota.bin success!");
                        
                        // 触发升级程序
                        char bufsss[4]={0xaa,0xaa,0xff,0xff};
                        lt_uart2frx8016_send(bufsss,4);

                        lt_ota_start_public();
                    }
                    else 
                    {
                        QL_MQTT_LOG("MQTT_TEST:download ble_ota.bin fail!");
                    }
                }

            }
        }
        
    }

#if 0
	cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
    if (cmdid != NULL)
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
    }
    else
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
    }
#endif
}
// 实时通话
//  DATA "cmdID":"1370281908836700162", "sendTime": 1620887971, "rtspAddr": "rtsp: //120.24.242.14、1.sdp", "radioID": "1370281908836700162"}
//  rtspAddr为空则是停播
//
static void func_rtShout(cJSON *data)
{
    cJSON *url = cJSON_GetObjectItem(data, "rtspAddr");
    cJSON *radioID = cJSON_GetObjectItem(data, "radioID");

    if (strlen(url->valuestring) != 0 && strlen(radioID->valuestring) != 0)
    {
        ltplay_check_play(SND_EBS);
        ebm_task_t task = {0};
        ebm_task_msg_t ebm_msg = {0};
        task.status = TASK_STATUS_WAIT;
        task.chn_mask = CHN_IP_MASK;
        task.next = task.prev = NULL;
        dec_str2bcd_arr(radioID->valuestring, task.ebm_id, sizeof(task.ebm_id));
        task.ebm_type = 4;
        task.severity = 4;
        task.volume = 100;
        task.start_time = 0;
        task.stop_time = 0xFFFFFFFF;
        task.task_source = TASK_SOURCE_SHOUT;
        memcpy(task.url, url->valuestring, strlen(url->valuestring));
        QL_MQTT_LOG("task.url =%s", task.url);
        task.callback = ebm_task_stop_cb;

        memcpy(ebm_msg.buff, (char *)&task, sizeof(ebm_task_t));
        ebm_msg.type = TASK_ADD;
        if (eb_player_task_msg_send(&ebm_msg) != TASK_RESULT_OK)
        {
            QL_MQTT_LOG("Lcoal task of U-disk add failed.\n");
        }
    }

    do
    {
        mqtt_radio_msg_t r_msg = {
            .playTime = ebs_time(NULL),
            .audioResult = 1,
        };
        memcpy(r_msg.radioID, radioID->valuestring, strlen(radioID->valuestring));
        api_mqtt_BroadcastRasult_Publish(&r_msg);
        if (strlen(url->valuestring) == 0 && strlen(radioID->valuestring) != 0)
        {
            QL_MQTT_LOG("delete r_msg.radioID =%s", r_msg.radioID);
            nomal_msg_stop_cb(&r_msg, 0); // 如果url为空，则为停播
        }
    } while (0);

    do
    {
        cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
        if (cmdid != NULL)
        {
            api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
        }
        else
        {
            api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
        }
    } while (0);
}

const char* extractFileName(const char *url) {
    const char *lastSlash = strrchr(url, '/');
    if (lastSlash != NULL) {
        return lastSlash + 1;
    }
    return url;
}
static void func_musicList(cJSON *data)
{
    static unsigned char guid[32] = {0};
    cJSON *musicList = cJSON_GetObjectItem(data, "musicList");
    if (!cJSON_IsArray(musicList))
    {
        QL_MQTT_LOG("musicList is not an array.\n");
        return;
    }
    // 遍历musicList数组并打印
    QL_MQTT_LOG("Music List:\n");

    QL_MQTT_LOG(" cJSON_GetArraySize(musicList):%d\n", cJSON_GetArraySize(musicList));
    //计算列表的sha256值 防止多次更新
    char *json_string = cJSON_Print(musicList);
    QL_MQTT_LOG("json_string:%s\n", json_string);
    unsigned char sha256buff[32] = {0};
    memset(sha256buff, 0x00, sizeof(sha256buff));
    sha256(json_string, sha256buff);
    QL_MQTT_LOG("%02x%02x%02x%02x", sha256buff[0], sha256buff[1], sha256buff[2], sha256buff[3]);

    if (memcmp(guid, sha256buff,sizeof(guid)) != 0)
    {
        QL_MQTT_LOG("func_musicList need update");
        memcpy(guid,sha256buff,sizeof(guid));
        lt_audio_del_allurl();
        int i;
        for (i = 0; i < cJSON_GetArraySize(musicList); i++)
        {
            cJSON *musicItem = cJSON_GetArrayItem(musicList, i);
            if (cJSON_IsString(musicItem))
            {
                QL_MQTT_LOG("%s\n", musicItem->valuestring);
                const char *file_name = extractFileName(musicItem->valuestring);
                QL_MQTT_LOG("%s\n", file_name);
                lt_http_dload_mp3(file_name, musicItem->valuestring);
                //      audiourl_play_add(musicItem->valuestring);
                lt_audio_add_url(musicItem->valuestring);
            }
        }
    }else
    {
        QL_MQTT_LOG("func_musicList is same");
    }
     cJSON_free(json_string);
    do
    {
        cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
        if (cmdid != NULL)
        {
            api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
        }
        else
        {
            api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
        }
    } while (0);
}

int stringToInteger(const char *str) {
    int result = 0;
    while (*str) {
        if (*str >= '0' && *str <= '9') {
            result = result * 10 + (*str - '0');
        }
        str++;
    }
    return result;
}

static void func_fmList(cJSON *data)
{
    
    cJSON *fmList = cJSON_GetObjectItem(data, "fmList");
    if (!cJSON_IsArray(fmList))
    {
        QL_MQTT_LOG("fmList is not an array.\n");
        return;
    }
    static unsigned char guid[32] = {0};

    QL_MQTT_LOG(" cJSON_GetArraySize(fmList):%d\n", cJSON_GetArraySize(fmList));
    // 计算列表的sha256值 防止多次更新
    char *json_string = cJSON_Print(fmList);
    QL_MQTT_LOG("json_string:%s\n", json_string);
    unsigned char sha256buff[32] = {0};
    memset(sha256buff, 0x00, sizeof(sha256buff));
    sha256(json_string, sha256buff);
    QL_MQTT_LOG("%02x%02x%02x%02x", sha256buff[0], sha256buff[1], sha256buff[2], sha256buff[3]);

    if (memcmp(guid, sha256buff, sizeof(guid)) != 0)
    {
        QL_MQTT_LOG("func_fmList need update");
        memcpy(guid, sha256buff, sizeof(guid));

        lt_fm_del_allurl();
        // fmList
        // QL_MQTT_LOG("fmList:\n");
        // QL_MQTT_LOG(" cJSON_GetArraySize(fmList):%d\n", cJSON_GetArraySize(fmList));
        int i=0;
        for (i = 0; i < cJSON_GetArraySize(fmList); i++)
        {
            cJSON *fmItem = cJSON_GetArrayItem(fmList, i);
            if (cJSON_IsString(fmItem))
            {
                QL_MQTT_LOG("%s\n", fmItem->valuestring);
                lt_fm_add_url(fmItem->valuestring);
            }
        }
    }
    else
    {
        QL_MQTT_LOG("func_fmList is same");
    }
       cJSON_free(json_string);
    cJSON *fmTag = cJSON_GetObjectItem(data, "fmTag");
    if (!cJSON_IsArray(fmTag))
    {
        QL_MQTT_LOG("musicList is not an array.\n");
    }
    else
    {
        QL_MQTT_LOG("fmTag:\n");
        QL_MQTT_LOG(" cJSON_GetArraySize(fmList):%d\n", cJSON_GetArraySize(fmTag));
        int i;
        for (i = 0; i < cJSON_GetArraySize(fmTag); i++)
        {
            cJSON *fmTagItem = cJSON_GetArrayItem(fmTag, i);
            if (cJSON_IsString(fmTagItem))
            {
                QL_MQTT_LOG("%s\n", fmTagItem->valuestring);
                // const char *file_name = extractFileName(fmItem->valuestring);
                // QL_MQTT_LOG("%s\n", file_name);
                lt_fm_add_fmTag(i,stringToInteger(fmTagItem->valuestring));
            }
        }
    }
    // fmList


    do
    {
        cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
        if (cmdid != NULL)
        {
            api_mqtt_CmdResult_Publish(cmdid->valuestring, 1);
        }
        else
        {
            api_mqtt_CmdResult_Publish(cmdid->valuestring, 0);
        }
    } while (0);
}

static void func_aiResponse(cJSON *data)
{
    QL_MQTT_LOG("func_aiResponse");

    if (NULL == data)
    {
        QL_MQTT_LOG("cjson para invalid");
        return;
    }

    cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
    if (NULL == cmdid)
    {
        QL_MQTT_LOG("cmdID para invalid");
    }

    QL_MQTT_LOG("cmdId is %s", cmdId);
    if (0 == strcmp(cmdId, cmdid->valuestring))
    {
        cJSON *response = cJSON_GetObjectItem(data, "aiResponse");
        if (response != NULL)
        {
            QL_MQTT_LOG("response data:%s", response->valuestring);
            ltapi_play_tts(response->valuestring, strlen(response->valuestring));
            QL_MQTT_LOG("play aiResponse tts success");
        }
    }

    return;
}

// 新增:调节亮度
static void func_led_control(cJSON *data)
{
    QL_MQTT_LOG("func_led_control");

    if (NULL == data)
    {
        QL_MQTT_LOG("cjson para invalid");
        return;
    }

    cJSON *led_level = cJSON_GetObjectItem(data, "led_control");
    if (led_level)
    {
        //加入值判断 5-15
        if(led_level->valueint < 5 || led_level->valueint > 15)
        {
            QL_MQTT_LOG("led_level para invalid");
            return;
        }
        lt_vk16k33_set_brightness((uint8_t)led_level->valueint);
        QL_MQTT_LOG("led_level is %d", led_level->valueint);
    }

    return;
}

// 新增：下载配置文件,具体协议待定
static void func_dl_config(cJSON *data)
{
    QL_MQTT_LOG("func_dl_config");

    if (NULL == data)
    {
        QL_MQTT_LOG("cjson para invalid");
        return;
    }

    cJSON *dl_config = cJSON_GetObjectItem(data, "dl_config");
    if (dl_config != NULL)
    {

        cJSON *url = cJSON_GetObjectItem(dl_config, "url");
        cJSON *md5 = cJSON_GetObjectItem(dl_config, "md5");
        char local_filename[] = "define.json";
        if (url != NULL && md5 != NULL)
        {
            if(lt_http_dload_config(local_filename, url->valuestring, md5->valuestring) == 0)
            {
                QL_MQTT_LOG("MQTT_TEST:download define.json success!");
            }
            else 
            {
                QL_MQTT_LOG("MQTT_TEST:download ble_ota.bin fail!");
            }
        }
    }

    return;
}

/**
 * @brief 3.x 跌倒检测参数配置下发 (带纠错和回退机制)
 * 协议示例: { "cmdID": "...", "fallDetectionCfg": "1,3,5" } 
 * 逻辑:
 * 1. enable(必需): 0=关, 1=开. 
 * 2. threshold(可选): 范围1-7. 若无效/缺失 -> 保持原配置.
 * 3. duration(可选): 范围1-31. 若无效/缺失 -> 保持原配置.
 */
static void func_fallDetectionParams(cJSON *data)
{
    cJSON *cmdid = cJSON_GetObjectItem(data, "cmdID");
    cJSON *fall_cfg_item = cJSON_GetObjectItem(data, "fallDetectionCfg");
    
    int cur_enable = 0;
    int cur_th = 3;   // 默认安全值
    int cur_dur = 5;  // 默认安全值
    
    mqtt_param_fallDetect_get(&cur_enable, &cur_th, &cur_dur);

    int new_enable = cur_enable;
    int new_th = cur_th;
    int new_dur = cur_dur;

    // 临时解析变量
    int p_enable = -1, p_th = -1, p_dur = -1;
    uint8_t result = 0;

    if (fall_cfg_item == NULL || !cJSON_IsString(fall_cfg_item))
    {
        QL_MQTT_LOG("FF Cfg Fail: Invalid format.");
        goto exit;
    }

    int count = sscanf(fall_cfg_item->valuestring, "%d,%d,%d", &p_enable, &p_th, &p_dur);

    // 3. 处理 Enable (必需)
    if (count >= 1)
    {
        if (p_enable == 0 || p_enable == 1) {
            new_enable = p_enable;
        } else {
            QL_MQTT_LOG("FF Cfg Fail: Enable must be 0 or 1.");
            goto exit;
        }
    }
    else
    {
        QL_MQTT_LOG("FF Cfg Fail: Empty string.");
        goto exit;
    }

    // 4. 处理 Threshold (可选，带纠错)
    if (count >= 2)
    {
        // 如果下发的数值在 1-7 之间，则更新；否则保持 cur_th (原值)
        if (p_th >= 1 && p_th <= 7) {
            new_th = p_th;
        } else {
            QL_MQTT_LOG("FF Cfg: Threshold %d out of range(1-7), keeping old value %d.", p_th, cur_th);
        }
    }

    // 5. 处理 Duration (可选，带纠错)
    if (count >= 3)
    {
        // 如果下发的数值在 1-31 之间，则更新；否则保持 cur_dur (原值)
        if (p_dur >= 1 && p_dur <= 31) {
            new_dur = p_dur;
        } else {
            QL_MQTT_LOG("FF Cfg: Duration %d out of range(1-31), keeping old value %d.", p_dur, cur_dur);
        }
    }

    // 6. 保存最终的有效配置
    mqtt_param_fallDetect_set(new_enable, new_th, new_dur);

    // 7. 应用配置到驱动
    if (new_enable == 0)
    {
        if (lsm6ds3tr_disable_fall_detection() == 0)
        {
            QL_MQTT_LOG("FF Cfg Success: Disabled.");
            result = 1;
        }
    }
    else
    {
        // 使用最终确定的 new_th 和 new_dur
        if (lsm6ds3tr_set_fall_detection_params((uint8_t)new_th, (uint8_t)new_dur) == 0)
        {
            QL_MQTT_LOG("FF Cfg Success: Enabled. Th=%d, Dur=%d", new_th, new_dur);
            result = 1;
        }
    }

exit:
    if (cmdid != NULL)
    {
        api_mqtt_CmdResult_Publish(cmdid->valuestring, result);
    }
}

mqtt_msg_dispose_t Disposes[] = {
    {"radioType", func_radioType},                 // 3.1
    {"familyName", func_telephone},             // 3.2
    {"familyPhone", func_telephone},             // 3.2
    {"whiteList", func_telephone},               // 3.2
    {"electricServicePhone", func_telephone},    // 3.2
    {"waterServicePhone", func_telephone},       // 3.2
    {"gasServicePhone", func_telephone},         // 3.2
    {"sosPhone", func_telephone},                   // 3.3
    {"clockID", func_clockID},                     // 3.4
    {"whiteListState", func_whiteListState},       // 3.5
    {"mqttProductID", func_mqttProductID},         // 3.6
    {"cancleCardBinding", func_cancleCardBinding}, // 3.7
    {"clearSetting", func_clearSetting},           // 3.8
    {"ledEnable", func_ledEnable},                 // 3.9
    {"ttsCmd", func_ttsCmd},                       // 3.10
    {"mqttCfgDelete", func_mqttCfgDelete},         // 3.11
    {"reboot", func_reboot},                       // 3.12
    {"payload", func_fota},                        // 4.1
    {"rtspAddr", func_rtShout},                    // 5.1   实时通话
    {"musicList", func_musicList},                    // 5.2   音乐列表
    {"fmList",func_fmList},
    {"aiResponse", func_aiResponse},                   //2.10 语音指令查询响应
    {"led_control", func_led_control},                   //新增：调节亮度响应
    {"dl_config", func_dl_config},                   //新增：下载配置文件
    {"fallDetectionCfg", func_fallDetectionParams},                   //新增：设置跌倒检测参数(预留调试接口)
    
};

static void mqtt_state_exception_cb(mqtt_client_t *client)
{
    int err = 0;
    QL_MQTT_LOG("mqtt session abnormal disconnect");
    err = ql_mqtt_client_deinit(lt_mqtt.mqtt_client);
    if (err != 0)
    {
        QL_MQTT_LOG("ql_mqtt_client_deinit fail , err == %d", err);
    }
    else
    {
        QL_MQTT_LOG("ql_mqtt_client_deinit success!");
    }

    lt_mqtt.mqtt_connected = 0;
}

static void mqtt_connect_result_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_e status)
{
    QL_MQTT_LOG("status: %d", status);
    if (status == 0)
    {
        lt_mqtt.mqtt_connected = 1;
        api_mqtt_Status_Publish();
    }
    ql_rtos_semaphore_release(lt_mqtt.mqtt_semp);
}

static void mqtt_requst_result_cb(mqtt_client_t *client, void *arg, int err)
{
    QL_MQTT_LOG("err: %d", err);
    ql_rtos_semaphore_release(lt_mqtt.mqtt_semp);
}

static void mqtt_inpub_data_cb(mqtt_client_t *client, void *arg, int pkt_id, const char *topic, const unsigned char *payload, unsigned short payload_len)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    lt_mqtt_msg_t msg;
    msg.type = RECV_MSG;
    msg.datalen = payload_len;
    memset(msg.topic, 0x00, sizeof(msg.topic));
    memcpy(msg.topic, topic, strlen(topic));
    msg.data = malloc(msg.datalen);
    memcpy(msg.data, payload, msg.datalen);

    if ((err = ql_rtos_queue_release(lt_mqtt.mqtt_msg_queue, sizeof(lt_mqtt_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
    {
        QL_MQTT_LOG("send msg to queue failed, err=%d", err);
    }
}

// 更新MQTT连接信息
static void mqtt_connect_msg_init()
{
    char temp[16] = {0};
    if (ql_dev_get_imei(temp, sizeof(temp), 0) == 0)
    {
        int type = factory_param_deviceType_get();
        switch (type)
        {
        case 0:
            memset(pubtopic, 0x00, sizeof(pubtopic));
            sprintf(pubtopic, "smartpension/%s", temp);

            memset(pubtopicfota, 0x00, sizeof(pubtopicfota));
            sprintf(pubtopicfota, "/ota/progress/%s", temp);

            memset(subtopic[0], 0x00, sizeof(subtopic[0]));
            sprintf(subtopic[0], "device_control/%s", temp);

            memset(subtopic[1], 0x00, sizeof(subtopic[1]));
            sprintf(subtopic[1], "/ota/upgrade/%s", temp);

            QL_MQTT_LOG("mqtt device type Lootom");
            break;
        case 1:
            memset(pubtopic, 0x00, sizeof(pubtopic));
            sprintf(pubtopic, "v1");

            memset(subtopic[0], 0x00, sizeof(subtopic[0]));
            sprintf(subtopic[0], "device_control");
            QL_MQTT_LOG("mqtt device type YuanLiu");
            break;
        default:
            break;
        }
    }

    mqtt_param_client_url_get(lt_mqtt.mqtt_client_url, sizeof(lt_mqtt.mqtt_client_url));
    mqtt_param_client_user_get(lt_mqtt.mqtt_client_user, sizeof(lt_mqtt.mqtt_client_user));
    mqtt_param_client_pass_get(lt_mqtt.mqtt_client_pass, sizeof(lt_mqtt.mqtt_client_pass));
    mqtt_param_client_identity_get(lt_mqtt.mqtt_client_identity, sizeof(lt_mqtt.mqtt_client_identity));

    QL_MQTT_LOG("mqtt_connect_msg_init ok!");
    QL_MQTT_LOG("     mqtt_client_url: %s", lt_mqtt.mqtt_client_url);
    QL_MQTT_LOG("    mqtt_client_user: %s", lt_mqtt.mqtt_client_user);
    QL_MQTT_LOG("    mqtt_client_pass: %s", lt_mqtt.mqtt_client_pass);
    QL_MQTT_LOG("mqtt_client_identity: %s", lt_mqtt.mqtt_client_identity);
}

static void mqtt_app_thread(void *arg)
{
#define RETRY_INTERVAL 10 * 1000 // 10s
    int profile_idx = 1;
    uint8_t nSim = 0;
    uint16_t sim_cid;
    struct mqtt_connect_client_info_t client_info = {0};
    lt_mqtt.mqtt_client = &mqtt_cli;
    ql_rtos_semaphore_create(&lt_mqtt.mqtt_semp, 0);

    // 启动线程延迟10s后开始MQTT连接尝试.
    ql_rtos_task_sleep_s(10);

    while (1)
    {
        int ret = MQTTCLIENT_SUCCESS;
        // 周期判断一次MQTT连接是否正常：DEVICE_INFORMATION_RETURN_INTERVAL
        if (ql_mqtt_client_is_connected(lt_mqtt.mqtt_client) == 1)
        {
            int ri = mqtt_param_returninterval_get();
            if (ri < RETRY_INTERVAL / 1000)
                ri = RETRY_INTERVAL / 1000;

            if (ri <= n * RETRY_INTERVAL / 1000)
            {
                api_mqtt_Status_Publish(); // 发布一次心跳信息
                QL_MQTT_LOG("mqtt status publish!");
                n = 0;
            }
            else
            {
                n++;
            }
            ql_rtos_task_sleep_ms(RETRY_INTERVAL);
            continue;
        }
        else
        {
            do
            {
                ql_data_call_info_s info = {0};
                ret = ql_get_data_call_info(nSim, profile_idx, &info);
                // 未获取到拨号信息, 5s后重试.
                if (ret != 0)
                {
                    ql_rtos_task_sleep_ms(RETRY_INTERVAL);
                    QL_MQTT_LOG("ql_get_data_call_info ret: %d", ret);
                }
            } while (ret != 0);

            mqtt_connect_msg_init();

            if (QL_DATACALL_SUCCESS != ql_bind_sim_and_profile(nSim, profile_idx, &sim_cid))
            {
                QL_MQTT_LOG("nSim or profile_idx is invalid!!!!");
                ql_rtos_task_sleep_ms(RETRY_INTERVAL);
                continue;
            }

            if (ql_mqtt_client_init(&mqtt_cli, sim_cid) != MQTTCLIENT_SUCCESS)
            {
                QL_MQTT_LOG("mqtt client init failed!!!!");
                ql_rtos_task_sleep_ms(RETRY_INTERVAL);
                continue;
            }

            client_info.keep_alive = 300;
            client_info.pkt_timeout = 5;
            client_info.retry_times = 3;
            client_info.clean_session = 1;
            client_info.will_qos = 0;
            client_info.will_retain = 0;
            client_info.will_topic = NULL;
            client_info.will_msg = NULL;
            client_info.client_id = lt_mqtt.mqtt_client_identity;
            client_info.client_user = lt_mqtt.mqtt_client_user;
            client_info.client_pass = lt_mqtt.mqtt_client_pass;

            client_info.ssl_cfg = NULL;
            ret = ql_mqtt_connect(&mqtt_cli, lt_mqtt.mqtt_client_url, mqtt_connect_result_cb, NULL,
                                  (const struct mqtt_connect_client_info_t *)&client_info, mqtt_state_exception_cb);

            if (ret == MQTTCLIENT_WOUNDBLOCK)
            {
                QL_MQTT_LOG("====wait connect result");
                ql_rtos_semaphore_wait(lt_mqtt.mqtt_semp, QL_WAIT_FOREVER);
                if (lt_mqtt.mqtt_connected == 0)
                {
                    ql_mqtt_client_deinit(&mqtt_cli);
                    // MQTT连接到服务器不成功, 5s后重试.
                    ql_rtos_task_sleep_ms(RETRY_INTERVAL);
                    continue;
                }
            }
            else
            {
                QL_MQTT_LOG("===mqtt connect failed ,ret = %d", ret);
                ql_mqtt_client_deinit(&mqtt_cli);
                ql_rtos_task_sleep_ms(RETRY_INTERVAL); // MQTT连接到服务器不成功, 5s后重试.
                continue;
            }

            // 设置MQTT收到信息后的回调函数
            ql_mqtt_set_inpub_callback(&mqtt_cli, mqtt_inpub_data_cb, NULL);

            // 连接成功后，重新订阅消息
            for (int i = 0; i < sizeof(subtopic) / sizeof(subtopic[0]); i++)
            {
                if (strlen(subtopic[i]) == 0) // 防止连接源流平台会掉线的问题
                    continue;
                if (ql_mqtt_sub_unsub(&mqtt_cli, subtopic[i], 1, mqtt_requst_result_cb, NULL, 1) == MQTTCLIENT_WOUNDBLOCK)
                {
                    QL_MQTT_LOG("======wait subscrible result");
                    ql_rtos_semaphore_wait(lt_mqtt.mqtt_semp, QL_WAIT_FOREVER);
                }
            }

            ql_rtos_task_sleep_ms(RETRY_INTERVAL);
        }
    }

    ql_mqtt_client_deinit(&mqtt_cli);
    lt_mqtt.mqtt_connected = 0;
    ql_rtos_semaphore_delete(lt_mqtt.mqtt_semp);
    ql_rtos_task_delete(lt_mqtt.mqtt_task);
}

static void mqtt_msg_thread(void *arg)
{
    while (1)
    {
        cJSON *root = NULL;
        lt_mqtt_msg_t msg;
        ql_rtos_queue_wait(lt_mqtt.mqtt_msg_queue, (uint8 *)(&msg), sizeof(lt_mqtt_msg_t), 0xFFFFFFFF);
        switch (msg.type)
        {
        case RECV_MSG:
            QL_MQTT_LOG("MSG topic:%s", msg.topic);
            QL_MQTT_LOG("MSG len:%d", msg.datalen);
            QL_MQTT_LOG("MSG data:%s", msg.data);

            if (msg.data != NULL)
            {
                root = cJSON_Parse((const char *)msg.data);

                if (root != NULL)
                {
                    for (int i = 0; i < sizeof(Disposes) / sizeof(Disposes[0]); i++)
                    {
                        cJSON *temp = cJSON_GetObjectItem(root, Disposes[i].flag);
                        if (temp != NULL)
                        {
                            Disposes[i].funcDispose(root);
                            break;
                        }
                    }
                }
            }

            break;
        case SEND_MSG:
            if (ql_mqtt_client_is_connected(lt_mqtt.mqtt_client) == 1)
            {
                if (ql_mqtt_publish(lt_mqtt.mqtt_client, msg.topic, msg.data, msg.datalen, 0, 0, mqtt_requst_result_cb, NULL) == MQTTCLIENT_WOUNDBLOCK)
                {
                    ql_rtos_semaphore_wait(lt_mqtt.mqtt_semp, QL_WAIT_FOREVER);
                }
            }
            break;
        default:
            break;
        }

        do
        {
            if (root != NULL)
            {
                cJSON_Delete(root);
            }
            if (msg.data)
            {
                free((void *)msg.data);
                msg.data = NULL;
            }
        } while (0);
    }
}

int lt_mqtt_is_connected(void)
{
    if (lt_mqtt.mqtt_client == NULL)
    {
        return 0;
    }
    return ql_mqtt_client_is_connected(lt_mqtt.mqtt_client);
}

int lt_mqtt_publish(char *topic, uint8_t *data, uint32_t datalen)
{
    QlOSStatus err = QL_OSI_SUCCESS;

    lt_mqtt_msg_t msg;
    msg.type = SEND_MSG;
    msg.datalen = datalen;
    msg.data = malloc(msg.datalen);
    memset(msg.topic, 0x00, sizeof(msg.topic));
    memcpy(msg.topic, topic, strlen(topic));
    memcpy(msg.data, data, msg.datalen);

    if ((err = ql_rtos_queue_release(lt_mqtt.mqtt_msg_queue, sizeof(lt_mqtt_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
    {
        QL_MQTT_LOG("send msg to queue failed, err=%d", err);
        return -1;
    }

    return 0;
}

int lt_mqtt_app_init(void)
{
    factory_param_init();
    mqtt_param_init();

    ql_rtos_queue_create(&lt_mqtt.mqtt_msg_queue, sizeof(lt_mqtt_msg_t), 40);

    QlOSStatus err = QL_OSI_SUCCESS;

    err = ql_rtos_task_create(&lt_mqtt.mqtt_task, 8 * 1024, APP_PRIORITY_ABOVE_NORMAL, "mqtt_task", mqtt_app_thread, NULL, 5);

    if (err != QL_OSI_SUCCESS)
    {
        QL_MQTT_LOG("mqtt_task init failed");
    }

    err = ql_rtos_task_create(&lt_mqtt.mqtt_msg_task, 20 * 1024, APP_PRIORITY_ABOVE_NORMAL, "mqtt_msg_task", mqtt_msg_thread, NULL, 5);

    if (err != QL_OSI_SUCCESS)
    {
        QL_MQTT_LOG("mqtt_msg_task init failed");
    }
    QL_MQTT_LOG("lt_mqtt_app_init init OK!");
    return 0;
}
