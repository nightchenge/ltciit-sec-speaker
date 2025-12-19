#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "ltconf.h"
#include "cJSON.h"
#include "ql_log.h"
#include "ql_power.h"
#include "lt_tts_demo.h"
#include "ql_api_osi.h"
#include "ip_gb_comm.h"
#include "ltsystem.h"
#include "dynamic_config.h"

// 定义MQTT参数宏
#define DEFAULT_MQTT_PARAM_QIXIA \
"{\"mqttServer\":\"124.222.191.112\", \
\"mqttPort\":1883, \
\"mqttProductID\":\"\", \
\"mqttUserName\":\"\", \
\"mqttPassword\":\"\", \
\"sosPhone\":\"\", \
\"familyPhone\":\"\", \
\"whiteListDisableTime\":0, \
\"whiteListState\":1, \
\"ledEnable\":0, \
\"whiteList\":\"\", \
\"whiteListName\":\"\", \
\"returnInterval\":300}"

#define DEFAULT_MQTT_PARAM_GENERIC \
"{\"mqttServer\":\"218.90.141.146\", \
\"mqttPort\":11883, \
\"mqttProductID\":\"\", \
\"mqttUserName\":\"\", \
\"mqttPassword\":\"\", \
\"sosPhone\":\"\", \
\"familyPhone\":\"\", \
\"whiteListDisableTime\":0, \
\"whiteListState\":1, \
\"ledEnable\":0, \
\"whiteList\":\"\", \
\"whiteListName\":\"\", \
\"returnInterval\":300}"

#define DEFAULT_MQTT_PARAM_SICHUAN \
"{\"mqttServer\":\"8.137.99.195\", \
\"mqttPort\":1883, \
\"mqttProductID\":\"\", \
\"mqttUserName\":\"\", \
\"mqttPassword\":\"\", \
\"sosPhone\":\"\", \
\"familyPhone\":\"\", \
\"whiteListDisableTime\":0, \
\"whiteListState\":1, \
\"ledEnable\":0, \
\"whiteList\":\"\", \
\"whiteListName\":\"\", \
\"returnInterval\":300}"


// 定义工厂参数宏
#define DEFAULT_FACTORY_PARAM_SICHUAN "{\"tp\":\"ZR-ZHRHT\",\"sn\":\"\",\"deviceType\":0,\"ebs_enable\":1}"
#define DEFAULT_FACTORY_PARAM_OTHER "{\"tp\":\"LTCIIT-HHT\",\"sn\":\"\",\"deviceType\":0,\"ebs_enable\":1}"

// 优化后的函数实现
char* get_default_mqtt_param(void)
{
    int type_func = get_type_func();
    
    switch (type_func) {
        case TYPE_QIXIA:
            return DEFAULT_MQTT_PARAM_QIXIA;
        case TYPE_GENERIC:
            return DEFAULT_MQTT_PARAM_GENERIC;
        case TYPE_SICHUAN:
            return DEFAULT_MQTT_PARAM_SICHUAN;
        default:
            // 默认返回GENERIC配置
            return DEFAULT_MQTT_PARAM_GENERIC;
    }
}

char* get_default_factory_param(void)
{
    int type_func = get_type_func();
    
    if (type_func == TYPE_SICHUAN) {
        return DEFAULT_FACTORY_PARAM_SICHUAN;
    } else {
        return DEFAULT_FACTORY_PARAM_OTHER;
    }
}
#define QL_MQTT_LOG_LEVEL QL_LOG_LEVEL_INFO
#define QL_MQTT_LOG(msg, ...) QL_LOG(QL_MQTT_LOG_LEVEL, "lt_mqtt", msg, ##__VA_ARGS__)
#define LOCAL static
#define RETURNINTERVAL_DEFULAT 300

#define ROR(x, n) ((x << (32 - n)) | (x >> n))

unsigned int rInit(double data)
{
    double ip;
    unsigned long r = 0;
    data = modf(data, &ip);
    for (int i = 0; i < 8; i++)
    {
        data = data * 16;
        data = modf(data, &ip);
        r = (r << 4) + (int)ip;
    }
    return r;
}

void sha256(char *text, unsigned char *result)
{
    int reg[8];
    reg[0] = rInit(sqrt(2)),
    reg[1] = rInit(sqrt(3)),
    reg[2] = rInit(sqrt(5)),
    reg[3] = rInit(sqrt(7)),
    reg[4] = rInit(sqrt(11)),
    reg[5] = rInit(sqrt(13)),
    reg[6] = rInit(sqrt(17)),
    reg[7] = rInit(sqrt(19));

    int p[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53,
               59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131,
               137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223,
               227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311};

    unsigned int k[64];
    for (int i = 0; i < 64; i++)
    {
        k[i] = rInit(pow(p[i], 1.0 / 3));
    }

    long long textLen = strlen(text);
    long long allLen = textLen + (64 - (textLen % 64));
    unsigned char *newText = malloc(allLen);
    memset(newText, 0, allLen);
    strcpy((char *)newText, text);
    newText[textLen] = (char)0x80;

    long long bitLen = textLen * 8;

    newText[allLen - 8] = (char)(bitLen >> 56);
    newText[allLen - 7] = (char)(bitLen >> 48);
    newText[allLen - 6] = (char)(bitLen >> 40);
    newText[allLen - 5] = (char)(bitLen >> 32);
    newText[allLen - 4] = (char)(bitLen >> 24);
    newText[allLen - 3] = (char)(bitLen >> 16);
    newText[allLen - 2] = (char)(bitLen >> 8);
    newText[allLen - 1] = (char)(bitLen);

    for (int i = 0; i < allLen / 64; i++)
    {
        unsigned int a = reg[0], b = reg[1], c = reg[2], d = reg[3], e = reg[4], f = reg[5], g = reg[6], h = reg[7];

        unsigned int w[64];
        for (int j = 0, k = i * 64; j < 16; j++, k += 4)
        {
            w[j] = (newText[k] << 24) | (newText[k + 1] << 16) | (newText[k + 2] << 8) | (newText[k + 3]);
        }

        for (int j = 16; j < 64; j++)
        {
            w[j] = (ROR(w[j - 2], 17) ^ ROR(w[j - 2], 19) ^ (w[j - 2] >> 10)) + w[j - 7] + (ROR(w[j - 15], 7) ^ ROR(w[j - 15], 18) ^ (w[j - 15] >> 3)) + w[j - 16];
        }

        for (int j = 0; j < 64; j++)
        {
            unsigned int t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ ((~e) & g)) + k[j] + w[j];
            unsigned int t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));

            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        reg[0] = a + reg[0];
        reg[1] = b + reg[1];
        reg[2] = c + reg[2];
        reg[3] = d + reg[3];
        reg[4] = e + reg[4];
        reg[5] = f + reg[5];
        reg[6] = g + reg[6];
        reg[7] = h + reg[7];
    }

    for (int i = 0; i < 8; i++)
    {
        result[i * 4] = (char)(reg[i] >> 24);
        result[i * 4 + 1] = (char)(reg[i] >> 16);
        result[i * 4 + 2] = (char)(reg[i] >> 8);
        result[i * 4 + 3] = (char)reg[i];
    }
    if (NULL != newText)
    {
        free(newText);
    }
}

cJSON *config = NULL;
cJSON *factory_config = NULL;

LOCAL int valuestring_param_set(cJSON *conf, const char *name, char *new_data, int new_data_len)
{
    if (conf == NULL)
        return -1;

    cJSON *temp = cJSON_GetObjectItem(conf, name);
    if (temp != NULL)
    {
        cJSON *value = cJSON_CreateString(new_data);
        if (value != NULL)
        {
            cJSON_ReplaceItemInObject(conf, name, value);
            return 0;
        }
    }
    else
    {
        cJSON_AddStringToObject(config, name, new_data);
        return 0;
    }
    return -1;
}

LOCAL int valuestring_param_get(cJSON *conf, const char *name, char *data, int data_len)
{
    cJSON *temp = cJSON_GetObjectItem(conf, name);
    if (temp != NULL)
    {
        memset(data, 0x00, data_len);
        memcpy(data, temp->valuestring, strlen(temp->valuestring));
    }
    else
    {
        memset(data, 0x00, data_len);
    }
    return 0;
}

LOCAL int valueint_param_set(cJSON *conf, const char *name, const double new_data)
{
    if (conf == NULL)
        return -1;

    cJSON *temp = cJSON_GetObjectItem(conf, name);
    if (temp != NULL)
    {
        cJSON *value = cJSON_CreateNumber(new_data);
        if (value != NULL)
        {
            cJSON_ReplaceItemInObject(conf, name, value);
            return 0;
        }
    }
    else
    {
        cJSON_AddNumberToObject(config, name, new_data);
        return 0;
    }
    return -1;
}

LOCAL int valueint_param_get(cJSON *conf, const char *name)
{
    cJSON *temp = cJSON_GetObjectItem(conf, name);
    return temp != NULL ? temp->valueint : 0;
}

auth_status_t phone_number_auth_verif(char *phone_number, int phone_number_len)
{
    char phone[512] = {0};
    if (mqtt_param_whiteListState_get() == 0)
    {
        return AUTHED;
    }

    if (!mqtt_param_sosPhone_get(phone, sizeof(phone)))
    {
        if (strstr(phone, phone_number) != NULL)
        {
            return AUTHED;
        }
    }

    if (!mqtt_param_phone_get(phone, sizeof(phone), FAMILY_TYPE))
    {
        if (strstr(phone, phone_number) != NULL)
        {
            return AUTHED;
        }
    }

    if (!mqtt_param_whiteList_get(phone, sizeof(phone)))
    {
        if (strstr(phone, phone_number) != NULL)
        {
            return AUTHED;
        }
    }
    return NO_AUTH;
}

auth_status_t sos_number_auth_verif(char *phone_number, int phone_number_len)
{
    char phone[512] = {0};

    if (!mqtt_param_sosPhone_get(phone, sizeof(phone)))
    {
        if (strlen(phone) < phone_number_len)
            return NO_AUTH;
        if (strstr(phone, phone_number) != NULL)
        {
            return AUTHED;
        }
    }

    return NO_AUTH;
}
int mqtt_param_client_url_get(char *p_url, int url_len)
{
    cJSON *ser = cJSON_GetObjectItem(config, "mqttServer");
    cJSON *port = cJSON_GetObjectItem(config, "mqttPort");
    if (ser != NULL && port != NULL)
    {
        memset(p_url, 0x00, url_len);
        sprintf(p_url, "mqtt://%s:%d", ser->valuestring, port->valueint);
    }
    else
    {
        memset(p_url, 0x00, url_len);
    }
    return 0;
}

int mqtt_param_mqttServer_get(char *p_ser, int p_len)
{
    cJSON *temp = cJSON_GetObjectItem(config, "mqttServer");
    if (temp != NULL)
    {
        memset(p_ser, 0x00, p_len);
        memcpy(p_ser, temp->valuestring, strlen(temp->valuestring));
    }
    else
    {
        memset(p_ser, 0x00, p_len);
    }
    return 0;
}

int mqtt_param_mqttServer_set(char *p_ser, int p_len)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "mqttServer");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateString(p_ser);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "mqttServer", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddStringToObject(config, "mqttServer", p_ser);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

int mqtt_param_mqttPort_get()
{
    cJSON *temp = cJSON_GetObjectItem(config, "mqttPort");
    if (temp != NULL)
    {
        return temp->valueint;
    }
    return 0;
}

int mqtt_param_mqttPort_set(int port)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "mqttPort");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateNumber(port);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "mqttPort", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddNumberToObject(config, "mqttPort", port);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

int mqtt_param_client_identity_get(char *p_identity, int identity_len)
{
    cJSON *temp = cJSON_GetObjectItem(config, "mqttProductID");
    if (temp != NULL)
    {
        memset(p_identity, 0x00, identity_len);
        memcpy(p_identity, temp->valuestring, strlen(temp->valuestring));
    }
    else
    {
        memset(p_identity, 0x00, identity_len);
    }
    return 0;
}

int mqtt_param_client_identity_set(char *p_identity, int identity_len)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "mqttProductID");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateString(p_identity);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "mqttProductID", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddStringToObject(config, "mqttProductID", p_identity);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

int mqtt_param_client_user_get(char *p_user, int user_len)
{
    cJSON *temp = cJSON_GetObjectItem(config, "mqttUserName");
    if (temp != NULL)
    {
        memset(p_user, 0x00, user_len);
        memcpy(p_user, temp->valuestring, strlen(temp->valuestring));
    }
    else
    {
        memset(p_user, 0x00, user_len);
    }
    return 0;
}

int mqtt_param_client_user_set(char *p_user, int user_len)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "mqttUserName");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateString(p_user);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "mqttUserName", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddStringToObject(config, "mqttUserName", p_user);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

int mqtt_param_client_pass_get(char *p_pass, int pass_len)
{
    cJSON *temp = cJSON_GetObjectItem(config, "mqttPassword");
    if (temp != NULL)
    {
        memset(p_pass, 0x00, pass_len);
        memcpy(p_pass, temp->valuestring, strlen(temp->valuestring));
    }
    else
    {
        memset(p_pass, 0x00, pass_len);
    }
    return 0;
}

int mqtt_param_client_pass_set(char *p_pass, int pass_len)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "mqttPassword");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateString(p_pass);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "mqttPassword", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddStringToObject(config, "mqttPassword", p_pass);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

int mqtt_param_phone_get(char *phone, int phone_len, phone_type_t type)
{
    if (NULL == phone)
    {
        return -1;
    }

    memset(phone, 0x00, phone_len);
    if(FAMILY_NAME_TYPE == type)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "familyName");
        if (temp != NULL)
        {
            if (0 == strlen(temp->valuestring))
            {
                return -1;
            }

            memcpy(phone, temp->valuestring, strlen(temp->valuestring));
        }
        else
        {
            return -1;
        }
    }
    else if (FAMILY_TYPE == type)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "familyPhone");
        if (temp != NULL)
        {
            if (0 == strlen(temp->valuestring))
            {
                return -1;
            }

            memcpy(phone, temp->valuestring, strlen(temp->valuestring));
        }
        else
        {
            return -1;
        }
    }
    else if (ELECTRIC_SERVICE_TYPE == type)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "electricServicePhone");
        if (temp != NULL)
        {
            if (0 == strlen(temp->valuestring))
            {
                return -1;
            }

            memcpy(phone, temp->valuestring, strlen(temp->valuestring));
        }
        else
        {
            return -1;
        }
    }
    else if (WATER_SERVICE_TYPE == type)
    { 
        cJSON *temp = cJSON_GetObjectItem(config, "waterServicePhone");
        if (temp != NULL)
        {
            if (0 == strlen(temp->valuestring))
            {
                return -1;
            }

            memcpy(phone, temp->valuestring, strlen(temp->valuestring));
        }
        else
        {
            return -1;
        }
    }
    else
    {
        cJSON *temp = cJSON_GetObjectItem(config, "gasServicePhone");
        if (temp != NULL)
        {
            if (0 == strlen(temp->valuestring))
            {
                return -1;
            }

            memcpy(phone, temp->valuestring, strlen(temp->valuestring));
        }
        else
        {
            return -1;
        }
    }

    return 0;
}

int mqtt_param_phone_set(char *phone, int phone_len, phone_type_t type)
{
    if (NULL == phone)
    {
        return -1;
    }

    if (config != NULL)
    {
        if (FAMILY_NAME_TYPE == type)
        {
            cJSON *temp = cJSON_GetObjectItem(config, "familyName");
            if (temp != NULL)
            {
                cJSON *value = cJSON_CreateString(phone);
                if (value != NULL)
                {
                    cJSON_ReplaceItemInObject(config, "familyName", value);
                    mqtt_param_save();
                    return 0;
                }
            }
            else
            {
                cJSON_AddStringToObject(config, "familyName", phone);
                mqtt_param_save();
                return 0;
            }
        }
        else if (FAMILY_TYPE == type)
        {
            cJSON *temp = cJSON_GetObjectItem(config, "familyPhone");
            if (temp != NULL)
            {
                cJSON *value = cJSON_CreateString(phone);
                if (value != NULL)
                {
                    cJSON_ReplaceItemInObject(config, "familyPhone", value);
                    mqtt_param_save();
                    return 0;
                }
            }
            else
            {
                cJSON_AddStringToObject(config, "familyPhone", phone);
                mqtt_param_save();
                return 0;
            }
        }
        else if (ELECTRIC_SERVICE_TYPE == type)
        {
            cJSON *temp = cJSON_GetObjectItem(config, "electricServicePhone");
            if (temp != NULL)
            {
                cJSON *value = cJSON_CreateString(phone);
                if (value != NULL)
                {
                    cJSON_ReplaceItemInObject(config, "electricServicePhone", value);
                    mqtt_param_save();
                    return 0;
                }
            }
            else
            {
                cJSON_AddStringToObject(config, "electricServicePhone", phone);
                mqtt_param_save();
                return 0;
            }
        }
        else if (WATER_SERVICE_TYPE == type)
        {
            cJSON *temp = cJSON_GetObjectItem(config, "waterServicePhone");
            if (temp != NULL)
            {
                cJSON *value = cJSON_CreateString(phone);
                if (value != NULL)
                {
                    cJSON_ReplaceItemInObject(config, "waterServicePhone", value);
                    mqtt_param_save();
                    return 0;
                }
            }
            else
            {
                cJSON_AddStringToObject(config, "waterServicePhone", phone);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON *temp = cJSON_GetObjectItem(config, "gasServicePhone");
            if (temp != NULL)
            {
                cJSON *value = cJSON_CreateString(phone);
                if (value != NULL)
                {
                    cJSON_ReplaceItemInObject(config, "gasServicePhone", value);
                    mqtt_param_save();
                    return 0;
                }
            }
            else
            {
                cJSON_AddStringToObject(config, "gasServicePhone", phone);
                mqtt_param_save();
                return 0;
}
        }
    }
    return -1;
}

// 获取sos号码列表长度
int mqtt_param_sosPhonenum_get()
{
    cJSON *temp = cJSON_GetObjectItem(config, "sosPhone");
    if (temp != NULL)
    {
        int count = 0;
        char *copy = strdup(temp->valuestring); // 复制一份字符串以免修改原始字符串
        char *token = strtok(copy, ",");

// 通过循环统计逗号分隔的电话号码数量
        while (token != NULL)
        {
            // 检查每个子字符串是否为有效电话号码
            if (strlen(token) > 0)
            {
                count++;
            }
            token = strtok(NULL, ",");
        }

        // 如果 buf 本身就是一个号码而没有逗号分隔，需要单独处理
        if (strlen(temp->valuestring) > 0 && count == 0)
        {
            count = 1;
        }

        free(copy); // 释放复制的字符串内存
        return count;
    }
    else
    {
        return 0;
    }
    return 0;
}
// 获取号码列表中指定的号码
int mqtt_param_sosPhonelist_get(char *p_sosPhone, int sosPhone_len, int index)
{
    cJSON *temp = cJSON_GetObjectItem(config, "sosPhone");
    if (temp != NULL)
    {

        int count = 0;
        char *copy = strdup(temp->valuestring); // 复制一份字符串以免修改原始字符串
        char *token = strtok(copy, ",");

        // 通过循环遍历逗号分隔的电话号码
        while (token != NULL)
        {
            // 检查每个子字符串是否为有效电话号码
            if (strlen(token) > 0)
            {
                // 如果达到目标索引，则将该号码复制到 p_sosPhone 中
                if (count == index)
                {
                    strncpy(p_sosPhone, token, strlen(token));
                    free(copy); // 释放复制的字符串内存
                    return 0;   // 返回0表示成功找到并复制号码
                }
                count++;
            }
            token = strtok(NULL, ",");
        }

        // 如果 buf 本身就是一个号码而没有逗号分隔，需要单独处理
        if (strlen(temp->valuestring) > 0 && count == 0)
        {
            if (index == 0)
            {
                strncpy(p_sosPhone, temp->valuestring, strlen(temp->valuestring));
                free(copy); // 释放复制的字符串内存
                return 0;   // 返回0表示成功找到并复制号码
            }
        }

        free(copy); // 释放复制的字符串内存
        return -1;  // 返回-1表示未找到指定索引的号码
    }
    else
    {
        memset(p_sosPhone, 0x00, sosPhone_len);
        return -1;
    }
    return 0;
}

int mqtt_param_sosPhone_get(char *p_sosPhone, int sosPhone_len)
{
    cJSON *temp = cJSON_GetObjectItem(config, "sosPhone");
    if (temp != NULL)
    {
        memset(p_sosPhone, 0x00, sosPhone_len);
        memcpy(p_sosPhone, temp->valuestring, strlen(temp->valuestring));
        if (0 == strlen(temp->valuestring))
            return -1;
    }
    else
    {
        memset(p_sosPhone, 0x00, sosPhone_len);
        return -1;
    }
    return 0;
}

int mqtt_param_sosPhone_set(char *p_sosPhone, int sosPhone_len)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "sosPhone");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateString(p_sosPhone);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "sosPhone", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddStringToObject(config, "sosPhone", p_sosPhone);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

int mqtt_param_whiteList_get(char *p, int len)
{
    cJSON *temp = cJSON_GetObjectItem(config, "whiteList");
    if (temp != NULL)
    {
        memset(p, 0x00, len);
        memcpy(p, temp->valuestring, strlen(temp->valuestring));
    }
    else
    {
        memset(p, 0x00, len);
    }
    return 0;
}

int mqtt_param_whiteList_set(char *p, int len)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "whiteList");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateString(p);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "whiteList", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddStringToObject(config, "whiteList", p);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

int mqtt_param_whiteListName_get(char *p, int len)
{
    cJSON *temp = cJSON_GetObjectItem(config, "whiteListName");
    if (temp != NULL)
    {
        memset(p, 0x00, len);
        memcpy(p, temp->valuestring, strlen(temp->valuestring));
    }
    else
    {
        memset(p, 0x00, len);
    }
    return 0;
}

int mqtt_param_whiteListName_set(char *p, int len)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "whiteListName");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateString(p);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "whiteListName", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddStringToObject(config, "whiteListName", p);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

int mqtt_param_ledEnable_get()
{
    cJSON *temp = cJSON_GetObjectItem(config, "ledEnable");
    if (temp != NULL)
    {
        return temp->valueint;
    }
    return 0;
}

int mqtt_param_ledEnable_set(led_mode_t ledEnable)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "ledEnable");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateNumber(ledEnable);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "ledEnable", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddNumberToObject(config, "ledEnable", ledEnable);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

// ================= 新增参数实现 =================

// 获取 LED 亮度
int mqtt_param_ledBrightness_get()
{
    if (config == NULL) return 15; // 默认值 15
    cJSON *temp = cJSON_GetObjectItem(config, "ledBrightness");
    if (temp != NULL)
    {
        return temp->valueint;
    }
    return 15;
}

// 设置 LED 亮度
int mqtt_param_ledBrightness_set(int brightness)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "ledBrightness");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateNumber(brightness);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "ledBrightness", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddNumberToObject(config, "ledBrightness", brightness);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

// 获取 亮屏时间
int mqtt_param_ledScreenOnTime_get()
{
    if (config == NULL) return 60; // 默认值 60秒
    cJSON *temp = cJSON_GetObjectItem(config, "ledScreenOnTime");
    if (temp != NULL)
    {
        return temp->valueint;
    }
    return 60;
}

// 设置 亮屏时间
int mqtt_param_ledScreenOnTime_set(int time)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "ledScreenOnTime");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateNumber(time);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "ledScreenOnTime", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddNumberToObject(config, "ledScreenOnTime", time);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}



int mqtt_param_whiteListState_get()
{
    cJSON *temp = cJSON_GetObjectItem(config, "whiteListState");
    if (temp != NULL)
    {
        return temp->valueint;
    }
    return 0;
}

int mqtt_param_whiteListState_set(int whiteListState)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "whiteListState");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateNumber(whiteListState);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "whiteListState", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddNumberToObject(config, "whiteListState", whiteListState);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

int mqtt_param_whiteListDisableTime_get()
{
    cJSON *temp = cJSON_GetObjectItem(config, "whiteListDisableTime");
    if (temp != NULL)
    {
        return temp->valueint;
    }
    return 0;
}

int mqtt_param_whiteListDisableTime_set(int whiteListDisableTime)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "whiteListDisableTime");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateNumber(whiteListDisableTime);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "whiteListDisableTime", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddNumberToObject(config, "whiteListDisableTime", whiteListDisableTime);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

int mqtt_param_returninterval_get()
{
    cJSON *temp = cJSON_GetObjectItem(config, "returnInterval");
    if (temp != NULL)
    {
        return temp->valueint;
    }
    return RETURNINTERVAL_DEFULAT;
}

int mqtt_param_returninterval_set(int ri)
{
    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "returnInterval");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateNumber(ri);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "returnInterval", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddNumberToObject(config, "returnInterval", ri);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}

int mqtt_param_ebsinterval_get()
{ //! 默认单位为秒
    return comm_reg_period_get() / 1000;
}

int mqtt_param_ebsinterval_set(int ri)
{ //! 默认单位为秒
    comm_reg_period_set(ri * 1000);
    return 0;
}

// =========================================================
// 跌倒检测参数实现 (合并字段 "fallDetect": "enable,threshold,duration")
// =========================================================

int mqtt_param_fallDetect_get(int *enable, int *threshold, int *duration)
{
    if (enable == NULL || threshold == NULL || duration == NULL) {
        return -1;
    }

    cJSON *temp = cJSON_GetObjectItem(config, "fallDetect");
    if (temp != NULL && cJSON_IsString(temp))
    {
        // 解析字符串 "E,T,D"
        int count = sscanf(temp->valuestring, "%d,%d,%d", enable, threshold, duration);
        if (count == 3) {
            return 0;
        }
    }
    // 默认值：关闭, 阈值3, 持续时间5
    *enable = 0;
    *threshold = 3;
    *duration = 10;
    return 0;
}

int mqtt_param_fallDetect_set(int enable, int threshold, int duration)
{
    char buf[32] = {0};
    
    // 格式化为字符串
    snprintf(buf, sizeof(buf), "%d,%d,%d", enable, threshold, duration);

    if (config != NULL)
    {
        cJSON *temp = cJSON_GetObjectItem(config, "fallDetect");
        if (temp != NULL)
        {
            cJSON *value = cJSON_CreateString(buf);
            if (value != NULL)
            {
                cJSON_ReplaceItemInObject(config, "fallDetect", value);
                mqtt_param_save();
                return 0;
            }
        }
        else
        {
            cJSON_AddStringToObject(config, "fallDetect", buf);
            mqtt_param_save();
            return 0;
        }
    }
    return -1;
}





void print_json_tree(cJSON *root)
{
    // 确保传入的是对象或数组
    if (root->type != cJSON_Object && root->type != cJSON_Array)
    {
        return;
    }

    // 遍历JSON对象的所有成员
    cJSON *node = NULL;
    cJSON_ArrayForEach(node, root)
    {
        switch (node->type)
        {
        case cJSON_Object:
        case cJSON_Array:
            print_json_tree(node);
            break;
        default:
            QL_MQTT_LOG("Key: %s, Value: %s\n", node->string, cJSON_Print(node));
            break;
        }
    }
}

static void json_config_init()
{
    char buff[20480] ={0};
    QFILE fd = ql_fopen(MQ_PARAM_FILE, "rb+");
    ql_fread(buff, sizeof(buff), 1, fd);
    config = cJSON_Parse(buff);
    ql_fclose(fd);
}

static void default_config_init()
{
    char devinfo[16] = {0};
    memset(devinfo, 0x00, sizeof(devinfo));
    if (0 == ql_dev_get_imei(devinfo, 16, 0))
    {
        char identity[18] = {};
        unsigned char sha256buff[32] = {0};
        char psd[9] = {0};

        memset(identity, 0x00, sizeof(identity));
        memset(sha256buff, 0x00, sizeof(sha256buff));
        memset(psd, 0x00, sizeof(psd));

       // sprintf(identity, "LT%s", devinfo); // 通过IMEI合成用户名和ID
#if TYPE_FUNC == TYPE_SICHUAN
        sprintf(identity, "ZR%s", devinfo); // 通过IMEI合成用户名和ID
#else
        sprintf(identity, "LT%s", devinfo); // 通过IMEI合成用户名和ID
#endif
        sha256(identity, sha256buff);       // SHA256加密取前8位合成默认密码
        sprintf(psd, "%02x%02x%02x%02x", sha256buff[0], sha256buff[1], sha256buff[2], sha256buff[3]);

        mqtt_param_client_identity_set(identity, strlen(identity));
        mqtt_param_client_user_set(identity, strlen(identity));
        mqtt_param_client_pass_set(psd, strlen(psd));

        QL_MQTT_LOG("Crt id:%s,user:%s,psd:%s", identity, identity, psd);
    }
}
#define COPY_BUFFER_SIZE 4096 // 缓冲区大小 (4KB)

/**
 * @brief 使用 QLFS API 和标准 C 库内存管理函数复制文件
 * * @param src_path 源文件路径
 * @param dest_path 目标文件路径
 * @return int 成功返回 QL_FILE_OK (0)，失败返回 QL_FILE_XXX 错误码
 */
int ql_copy_file_std_mem(const char *src_path, const char *dest_path) {
    QFILE src_fd = -1;
    QFILE dest_fd = -1;
    void *buffer = NULL; 
    int ret = QL_FILE_ERROR_GENERAL; // 默认错误

    // 1. 内存分配
    buffer = malloc(COPY_BUFFER_SIZE);
    if (buffer == NULL) {
        // 内存分配失败，返回磁盘空间不足相关的错误码或通用错误
        return QL_FILE_NO_SPACE; 
    }

    // 2. 打开源文件 (只读模式 "rb")
    src_fd = ql_fopen(src_path, "rb");
    if (src_fd < 0) {
        ret = src_fd;
        goto cleanup;
    }

    // 3. 创建/打开目标文件 (写入/创建/覆盖模式 "wb")
    dest_fd = ql_fopen(dest_path, "wb");
    if (dest_fd < 0) {
        ret = dest_fd;
        goto cleanup;
    }

    // 4. 循环读写
    int bytes_read = 0;
    int bytes_written = 0;

    while (1) {
        // 从源文件读取数据块
        bytes_read = ql_fread(buffer, 1, COPY_BUFFER_SIZE, src_fd);
        
        if (bytes_read < 0) {
            // 读取失败
            ret = bytes_read;
            goto cleanup;
        }
        
        if (bytes_read == 0) {
            // 读取到文件末尾 (EOF)
            ret = QL_FILE_OK;
            break; 
        }

        // 将读取到的数据写入目标文件
        bytes_written = ql_fwrite(buffer, 1, bytes_read, dest_fd);

        if (bytes_written != bytes_read) {
            // 写入失败或部分写入
            ret = QL_FILE_FAILED_TO_WRITE_FILE; 
            goto cleanup;
        }
    }
    
cleanup:
    // 5. 资源清理
    // 关闭文件句柄
    if (src_fd >= 0) {
        ql_fclose(src_fd);
    }
    if (dest_fd >= 0) {
        ql_fclose(dest_fd);
    }
    
    // 释放内存
    if (buffer != NULL) {
        free(buffer);
    }

    return ret;
}

void mqtt_param_init()
{
    if (ql_file_exist(MQ_PARAM_DEFAULT_FILE) != QL_FILE_OK)
    {
        QFILE fd = ql_fopen(MQ_PARAM_DEFAULT_FILE, "wb+");
        const char *mqtt_param = get_default_mqtt_param();
        ql_fwrite((char *)mqtt_param, strlen(mqtt_param), 1, fd);
        ql_fclose(fd);
    }

    if (ql_file_exist(MQ_PARAM_FILE) != QL_FILE_OK)
    {
        ql_copy_file_std_mem(MQ_PARAM_DEFAULT_FILE, MQ_PARAM_FILE);
        json_config_init();
        default_config_init();
    }
    else
    {
        json_config_init();
    }

    // 输出配置
    if (config != NULL)
    {
        QL_MQTT_LOG("Nomal param init OK!");
        print_json_tree(config);
    }
}

void mqtt_param_reset()
{
    if (config != NULL)
    {
        cJSON_Delete(config);
        config = NULL;
    }

    if (ql_file_exist(MQ_PARAM_FILE) == QL_FILE_OK)
    {
        ql_remove(MQ_PARAM_FILE);
    }
}

void mqtt_param_save()
{
    if (config != NULL)
    {
        char *buf = cJSON_PrintUnformatted(config);
        if (buf != NULL)
        {
            QFILE fd = ql_fopen(MQ_PARAM_FILE, "wb+");
            ql_fwrite(buf, strlen(buf), 1, fd);
            ql_fclose(fd);
            cJSON_free(buf);
        }
    }
}

int factory_param_sn_get(char *p, int len)
{
    return valuestring_param_get(factory_config, "sn", p, len);
}

int factory_param_sn_set(char *p, int len)
{
    return valuestring_param_set(factory_config, "sn", p, len);
}

int factory_param_deviceType_set(int type)
{
    return valueint_param_set(factory_config, "deviceType", type) == 0 ? factory_param_save() : -1;
}

int factory_param_deviceType_get()
{
    return valueint_param_get(factory_config, "deviceType");
}

int factory_param_ebs_enable_get()
{
    return valueint_param_get(factory_config, "ebs_enable");
}

void factory_param_init()
{
    if (ql_file_exist(FACTORY_PARAM_FILE) != QL_FILE_OK)
    {
        QFILE fd = ql_fopen(FACTORY_PARAM_FILE, "wb+");
        const char* factory_param = get_default_factory_param();
        ql_fwrite((char*)factory_param, strlen(factory_param), 1, fd);
        ql_fclose(fd);
    }
    char buff[2048] = {0};
    QFILE fd = ql_fopen(FACTORY_PARAM_FILE, "rb+");
    ql_fread(buff, sizeof(buff), 1, fd);
    factory_config = cJSON_Parse(buff);
    if (factory_config != NULL)
    {
        QL_MQTT_LOG("Factory param init OK!");
        print_json_tree(factory_config);
    }
    ql_fclose(fd);
}

void factory_param_reset()
{
    if (factory_config != NULL)
    {
        cJSON_Delete(factory_config);
        factory_config = NULL;
    }

    if (ql_file_exist(FACTORY_PARAM_FILE) == QL_FILE_OK)
    {
        ql_remove(FACTORY_PARAM_FILE);
    }
}

int factory_param_save()
{
    if (factory_config != NULL)
    {
        char *buf = cJSON_PrintUnformatted(factory_config);
        if (buf != NULL)
        {
            QFILE fd = ql_fopen(FACTORY_PARAM_FILE, "wb+");
            ql_fwrite(buf, strlen(buf), 1, fd);
            ql_fclose(fd);
            cJSON_free(buf);
            return 0;
        }
    }
    return -1;
}

void *mqtt_config_get()
{
    return config;
}

void *factory_config_get()
{ 
    return factory_config;
}