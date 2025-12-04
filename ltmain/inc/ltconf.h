
#ifndef LTCONF_DEMO_H
#define LTCONF_DEMO_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "ql_fs.h"
#include "ql_api_dev.h"

#define MQ_PARAM_PATH "UFS:"
#define MQ_PARAM_FILE MQ_PARAM_PATH "mqtt_param.json"
#define FACTORY_PARAM_FILE MQ_PARAM_PATH "factory_param.json"

#define MQ_PARAM_DEFAULT_FILE MQ_PARAM_PATH "mqtt_param_default.json"

    typedef enum auth_status
    {
        NO_AUTH = 0,
        AUTHED = 1,
    } auth_status_t;

/* 电话类型 */
typedef enum phone_type
{
    FAMILY_NAME_TYPE,           /* 亲情电话姓名 */  
    FAMILY_TYPE,                /* 亲情电话 */
    ELECTRIC_SERVICE_TYPE,      /* 电力电话 */
    WATER_SERVICE_TYPE,         /* 水利电话 */
    GAS_SERVICE_TYPE,           /* 燃气电话 */
} phone_type_t;

    /**
     * \brief 验证号码白名单授权状态
     * \return  白名单授权状态(NO_AUTH：未授权,AUTHED：已授权;
     * 注：白名单开关未禁用时，直接返回已授权)
     */
    auth_status_t phone_number_auth_verif(char *phone_number, int phone_number_len);
    auth_status_t sos_number_auth_verif(char *phone_number, int phone_number_len);

    
    int mqtt_param_client_url_get(char *p_url, int url_len);

    int mqtt_param_mqttServer_get(char *p_ser, int p_len);
    int mqtt_param_mqttServer_set(char *p_ser, int p_len);

    int mqtt_param_mqttPort_get();
    int mqtt_param_mqttPort_set(int port);

    int mqtt_param_client_identity_get(char *p_identity, int identity_len);
    int mqtt_param_client_identity_set(char *p_identity, int identity_len);

    int mqtt_param_client_user_get(char *p_user, int user_len);
    int mqtt_param_client_user_set(char *p_user, int user_len);

    int mqtt_param_client_pass_get(char *p_pass, int pass_len);
    int mqtt_param_client_pass_set(char *p_pass, int pass_len);

    int mqtt_param_phone_get(char *phone, int phone_len, phone_type_t type);
    int mqtt_param_phone_set(char *phone, int phone_len, phone_type_t type);

    int mqtt_param_sosPhonenum_get();
    int mqtt_param_sosPhonelist_get(char *p_sosPhone, int sosPhone_len, int index);

    int mqtt_param_sosPhone_get(char *p_sosPhone, int sosPhone_len);
    int mqtt_param_sosPhone_set(char *p_sosPhone, int sosPhone_len);

    int mqtt_param_whiteList_get(char *p, int len);
    int mqtt_param_whiteList_set(char *p, int len);

    int mqtt_param_whiteListName_get(char *p, int len);

    int mqtt_param_whiteListName_set(char *p, int len);

    int mqtt_param_ledEnable_get();
    int mqtt_param_ledEnable_set(int ledEnable);

    int mqtt_param_whiteListState_get();
    int mqtt_param_whiteListState_set(int whiteListState);

    int mqtt_param_whiteListDisableTime_get();
    int mqtt_param_whiteListDisableTime_set(int whiteListDisableTime);
    int mqtt_param_returninterval_get();
    int mqtt_param_returninterval_set(int ri);

    int mqtt_param_ebsinterval_get();
    int mqtt_param_ebsinterval_set(int ri);

    void mqtt_param_init();
    void mqtt_param_reset();
    void mqtt_param_save();

    int factory_param_sn_set(char *p, int len);
    int factory_param_sn_get(char *p, int len);
    int factory_param_deviceType_set(int type);
    int factory_param_deviceType_get();
    int factory_param_ebs_enable_get();
    void factory_param_init();
    void factory_param_reset();
    int factory_param_save();

    void *mqtt_config_get();
    void *factory_config_get();
    void sha256(char *text, unsigned char *result);
#ifdef __cplusplus
}
#endif

#endif
