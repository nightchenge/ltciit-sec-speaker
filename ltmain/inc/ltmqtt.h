
#ifndef LTMQTT_DEMO_H
#define LTMQTT_DEMO_H

#ifdef __cplusplus
extern "C"
{
#endif

extern char cmdId[32];

    typedef struct mqtt_alarm_msg
    {
        int alarmType;
        int alarmId;
    } mqtt_alarm_msg_t; 

    typedef struct mqtt_call_msg
    {
        int callType;
        int callState;
        char callNumber[23];
        int callStartTime;
        int callEndTime;
        int callTime;
        int callId;
    } mqtt_call_msg_t;

    typedef struct mqtt_radio_msg
    {
        // 0 空闲 1 在播
        int status;
        int playTime;
        char radioID[31];
        char audioResult;
        char radioType;
        char url[128];
    } mqtt_radio_msg_t;

    typedef enum msg_type
    {
        RECV_MSG = 0,
        SEND_MSG = 1
    } msg_type_t;

    typedef struct lt_mqtt_msg
    {
        msg_type_t type;
        char topic[128];
        uint32_t datalen;
        uint8_t *data;
    } lt_mqtt_msg_t;

    int lt_mqtt_app_init(void);
    int lt_mqtt_is_connected(void);

    /*****************************************************************
     * Function: api_mqtt_Sos_Publish
     *
     * Description: 发布sos一键告警信息
     *
     * Parameters:
     *	alarmType        	[in]    0：正常  1：一键报警  2：低电量报警
     *
     *****************************************************************/
void api_mqtt_Sos_Publish(mqtt_alarm_msg_t *msg);

    /*****************************************************************
     * Function: api_mqtt_CallLog_Publish
     *
     * Description: 通话记录上报
     *
     * Parameters:
     *	msg        	[in]    通话详细信息
     *
     *****************************************************************/
    void api_mqtt_CallLog_Publish(mqtt_call_msg_t *msg);

    /*****************************************************************
     * Function: api_mqtt_Status_Publish
     *
     * Description: 设备信息上报,开机连上服务器后上报一包，往后每隔 24 小时上报一次
     *
     * Parameters:
     *
     *****************************************************************/
    void api_mqtt_Status_Publish();

    void api_mqtt_sosPhone_Publish();

    void api_mqtt_SosWiteList_Publish();
    // 0：亲情电话 1：一键通电话
    void api_yuanLiuMqtt_KeyPhone_Publish(char keyphoneType);

    void api_mqtt_fota_Publish(int stepval);

    void api_mqtt_sd_state_Publish();
    
void api_mqtt_recordData_Publish(unsigned int totalNum, unsigned int curNum, char *cmdID, char *recordData);
void api_mqtt_weather_Publish();

//跌到预警上报  fall_state : 0正常 1跌到预警 2跌到未处理 3跌到已处理
void api_mqtt_fall_publish(int fall_state);
#ifdef __cplusplus
}
#endif

#endif
