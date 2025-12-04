

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ql_api_osi.h"
#include "ql_log.h"
#include "ql_uart.h"
#include "ql_gpio.h"
#include "ql_usb.h"
#include "ltuart2frx8016.h"
#include "cJSON.h"
#include "ql_osi_def.h"
#include "ql_api_sim.h"
#include "ql_api_dev.h"
#include "ltconf.h"
#include "ebs_param.h"
#include "ebs.h"
#include "ql_api_nw.h"
#include "ql_api_datacall.h"
#include "ip_gb_comm.h"
#include "ql_power.h"
#include "lt_tts_demo.h"
#include "ltsystem.h"
#include "ql_api_sim.h"
#include "led.h"
#include "ql_audio.h"
//#include "audio_demo.h"
#include "ltadc.h"
#include "ltsystem.h"
#include "ltplay.h"
#include "ltmqtt.h"
#include "ec800g_ota_master.h"
#include "dynamic_config.h"
#define QL_UART_DEMO_LOG_LEVEL QL_LOG_LEVEL_INFO
#define QL_UART_DEMO_LOG(msg, ...) QL_LOG(QL_UART_DEMO_LOG_LEVEL, "ltuart2frx8016", msg, ##__VA_ARGS__)

#define QL_UART_TASK_STACK_SIZE 4096
#define QL_UART_TASK_PRIO APP_PRIORITY_NORMAL
#define QL_UART_TASK_EVENT_CNT 5

#define QL_UART_RX_BUFF_SIZE 2048
#define QL_UART_TX_BUFF_SIZE 2048

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define QL_USB_PRINTER_ENABLE 0

extern  struct lt_uart_port_state_t {
    lt_uart_mode_e mode;
    ql_task_t normal_target_task_ref;
    uint32_t normal_msg_id;
    bool initialized;
    ql_uart_port_number_e port_num;
    volatile uint8_t ota_state;
    ql_timer_t recv_timer_ref;       // OTA 帧接收超时定时器
    ql_timer_t response_timer_ref;   // OTA 命令响应超时定时器
    uint8_t* ota_recv_buffer;
    uint16_t ota_recv_idx;
    uint8_t* ota_send_buffer;
    int64_t ota_fw_handle;
    char* ota_fw_path_copy;
    uint32_t ota_fw_total_len;
    uint32_t ota_base_address;
    uint32_t ota_dev_version;
    uint16_t ota_earse_sector_index;
    uint32_t ota_send_data_offset;
    uint16_t ota_timeout_count;
} g_uart_state; // <-- 继承ota定义全局变量



#define FACTORY_FARMAT "{\"tp\":\"%s\",\"sn\":\"%s\",\"hw\":\"%s\",\"sw\":\"%s\",\"deviceType\":%d}"
#define SERVICE_FARMAT "{\"ser\":\"%s\",\"port\":%d,\"usr\":\"%s\",\"psd\":\"%s\",\"id\":\"%s\"}"
#define MODULE_FARMAT "{\"IMEI\":\"%s\",\"IMSI\":\"%s\",\"ICCID\":\"%s\"}"
#define STATUS_FARMAT "{\"sosPhone\":\"%s\",\"familyPhone\":\"%s\",\"ebsServer\":\"%08lx\",\"ebsServerPort\":\"%d\",\"ebsLbk\":\"%08lx\",\"ebsLbkPort\":\"%d\",\"ebsArea\":\"%s\"}"

typedef struct lt_conf_msg_dispose
{
    char flag[32];
    void (*funcDispose)(cJSON *data);
} lt_conf_msg_dispose_t;

static int battery = 0;

static ql_queue_t lt_conf_msg_queue;
static void l_act_dispose(cJSON *root);
static void l_famp_dispose(cJSON *json);
static void l_sos_dispose(cJSON *json);
static void l_ser_dispose(cJSON *json);
static void l_port_dispose(cJSON *json);
static void l_usr_dispose(cJSON *json);
static void l_psd_dispose(cJSON *json);
static void l_id_dispose(cJSON *json);
static void l_ebsServer_dispose(cJSON *json);
static void l_ebsServerPort_dispose(cJSON *json);
static void l_ebsLbk_dispose(cJSON *json);
static void l_ebsLbkPort_dispose(cJSON *json);
static void l_ebsArea_dispose(cJSON *json);
static void f_deviceType_dispose(cJSON *json);
static void f_sn_dispose(cJSON *json);
static void l_reboot_dispose(cJSON *json);
static void l_reset_dispose(cJSON *json);
static void l_battery_dispose(cJSON *json);
//static void l_ota_start_dispose(cJSON *json);


void lt_ota_start_public()
{
    char local_filename[] = "UFS:ble_ota.bin";
    int ret = ec_ota_start(g_uart_state.port_num, local_filename);
    if (ret == 0)
    {
        QL_UART_DEMO_LOG("OTA process started successfully.");
        // 成功启动，不需要发 JSON 响应，等待 OTA 流程
    }
    else
    {
        QL_UART_DEMO_LOG("OTA process start failed with ret=%d", ret);
    }
}

//设备关闭唯一接口

//发送给单片机执行关机动作
void  lt_uart_reboot_now(int type)
{
    ql_gpio_set_level(GPIO_35, LVL_LOW); // PA power
    ql_gpio_set_level(GPIO_3, LVL_LOW); // CI1302 power
       // 让1302启动起来后再打开功放电源,最好在locader中处理
    ql_gpio_set_level(GPIO_1, LVL_LOW); // ES8311 power
    ql_gpio_set_level(GPIO_0, LVL_LOW); // PA power
    QL_UART_DEMO_LOG("lt_uart_reboot_now");
    char buf[4]={0xa5,0xa5,0xff,0xff};
    lt_uart2frx8016_send(buf,4);
}

void lt_shutdown()
{
    char *sim_check = "设备即将关闭";
    ltapi_play_tts_withcallback(sim_check, strlen(sim_check),QL_TTS_UTF8,NULL,lt_uart_reboot_now);
}





static lt_conf_msg_dispose_t m_dispose[] = {
    {"act", l_act_dispose}};

static lt_conf_msg_dispose_t fpara_dispose[] = {
    {"deviceType", f_deviceType_dispose},
    {"sn", f_sn_dispose},
};

static lt_conf_msg_dispose_t npara_dispose[] = {
    {"familyPhone", l_famp_dispose},
    {"sosPhone", l_sos_dispose},
    {"ser", l_ser_dispose},
    {"port", l_port_dispose},
    {"usr", l_usr_dispose},
    {"psd", l_psd_dispose},
    {"id", l_id_dispose},
    {"ebsServer", l_ebsServer_dispose},
    {"ebsServerPort", l_ebsServerPort_dispose},
    {"ebsLbk", l_ebsLbk_dispose},
    {"ebsLbkPort", l_ebsLbkPort_dispose},
    {"ebsArea", l_ebsArea_dispose},
    {"reboot", l_reboot_dispose},
    {"reset", l_reset_dispose},
    {"battery", l_battery_dispose},
//  {"ota_start", l_ota_start_dispose},
    };// <--- 新增


void f_deviceType_dispose(cJSON *json)
{
    if (json != NULL)
    {
        factory_param_deviceType_set(json->valueint);
    }
}

void f_sn_dispose(cJSON *json)
{
    if (json != NULL)
    {
        factory_param_sn_set(json->valuestring, strlen(json->valuestring));
    }
}

void l_famp_dispose(cJSON *json)
{
    if (json != NULL)
    {
        mqtt_param_phone_set(json->valuestring, strlen(json->valuestring), FAMILY_TYPE);
    }
}

void l_sos_dispose(cJSON *json)
{
    if (json != NULL)
    {
        mqtt_param_sosPhone_set(json->valuestring, strlen(json->valuestring));
    }
}

void l_ser_dispose(cJSON *json)
{
    if (json != NULL)
    {
        mqtt_param_mqttServer_set(json->valuestring, strlen(json->valuestring));
    }
}

void l_port_dispose(cJSON *json)
{
    if (json != NULL)
    {
        mqtt_param_mqttPort_set(json->valueint);
    }
}

void l_usr_dispose(cJSON *json)
{
    if (json != NULL)
    {
        mqtt_param_client_user_set(json->valuestring, strlen(json->valuestring));
    }
}

void l_psd_dispose(cJSON *json)
{
    if (json != NULL)
    {
        mqtt_param_client_pass_set(json->valuestring, strlen(json->valuestring));
    }
}

void l_id_dispose(cJSON *json)
{
    if (json != NULL)
    {
        mqtt_param_client_identity_set(json->valuestring, strlen(json->valuestring));
    }
}

void l_ebsServer_dispose(cJSON *json)
{
    ip4_addr_t addr;
    if (json != NULL)
    {
        if (ip4addr_aton(json->valuestring, &addr))
        {
            ebs_param.reg_ip[0] = HTONL(addr.addr);
            ebs_param.reg_ip_4g[0] = HTONL(addr.addr);           
            comm_reg_ip_addr_set(ebs_param.reg_ip[0]);
            comm_reg_4g_addr_set(ebs_param.reg_ip[0]);
            ebs_param_save();
            comm_reg_reconnection();
        }
    }
}

void l_ebsServerPort_dispose(cJSON *json)
{
    if (json != NULL)
    {
        ebs_param.reg_port[0] = json->valueint;
        ebs_param.reg_port_4g[0] = json->valueint;       
        ebs_param_save();
        comm_reg_ip_port_set(ebs_param.reg_port[0]);
        comm_reg_4g_port_set(ebs_param.reg_port[0]);      
        comm_reg_reconnection();
    }
}

void l_ebsLbk_dispose(cJSON *json) 
{
    ip4_addr_t addr;
    if (json != NULL)
    {
        if (ip4addr_aton(json->valuestring, &addr))
        {
            ebs_param.lbk_ip[0] = HTONL(addr.addr);
            ebs_param.lbk_ip_4g[0] = HTONL(addr.addr);
            ebs_param_save();            
            comm_lbk_ip_addr_set(ebs_param.lbk_ip[0]);
            comm_lbk_4g_addr_set(ebs_param.lbk_ip[0]);
        }
    }

}

void l_ebsLbkPort_dispose(cJSON *json) 
{
    if (json != NULL)
    {
        ebs_param.lbk_port[0] = json->valueint;
        ebs_param.lbk_port_4g[0] = json->valueint;
        ebs_param_save();
        comm_lbk_ip_port_set(ebs_param.lbk_port[0]);
        comm_lbk_4g_port_set(ebs_param.lbk_port[0]);
    }
}

void l_ebsArea_dispose(cJSON *json)
{
    if (json != NULL)
    {
        if (strlen(json->valuestring) == 24)
        {
            hex_str2bcd_arr(json->valuestring, ebs_param.resources_code,
                            sizeof(ebs_param.resources_code));
            ebs_param_save();
        }
    }
}

void l_reboot_dispose(cJSON *json)
{
    if (json != NULL)
    {
        if (json->valueint == 1)
        {          
            char *data = "设备将在5秒后重新启动.";
            ltapi_play_tts(data, strlen(data));
            ql_rtos_task_sleep_s(5);
            ql_power_reset(RESET_NORMAL);
            return;
        }
    }

}

void l_reset_dispose(cJSON *json)
{
    if (json != NULL)
    {
        if (json->valueint == 1)
        {
            char *data = "设备重置成功,将在5秒后自动重启.";
            char json[32] = "{\"state\": 0}";
            lt_uart2frx8016_send(json, strlen(json));
            mqtt_param_reset();
            factory_param_reset();
            ebs_param_reset();
            ltapi_play_tts(data, strlen(data));
            ql_rtos_task_sleep_s(5);
            ql_power_reset(RESET_NORMAL);
            return;
        }
    }
}

/* 更新当前电量信息,由单片机设置 */
void l_battery_dispose(cJSON *json)
{
    if (json != NULL)
    {
        int temp = json->valueint;
        battery = temp < 0 ? 0 : (temp > 100 ? 100 : temp);
    }
}


/* * 启动 OTA 的 JSON 命令处理
 * 示例: {"act":"s", "npara":{"ota_start":"/data/firmware.bin"}}
 */
// void l_ota_start_dispose(cJSON *json)
// {
//     if (json == NULL || json->valuestring == NULL) return;

//     if (g_uart_state.mode == UART_MODE_OTA) {
//         QL_UART_DEMO_LOG("OTA already in progress");
//         // 可选：发送错误响应
//         return;
//     }

//     char* firmware_path = json->valuestring;
//     QL_UART_DEMO_LOG("Attempting to start OTA with file: %s", firmware_path);

//     // 调用 OTA 模块的启动函数
//     int ret = ec_ota_start(g_uart_state.port_num, firmware_path);
//     if (ret == 0) {
//         QL_UART_DEMO_LOG("OTA process started successfully.");
//         // 成功启动，不需要发 JSON 响应，等待 OTA 流程
//     } else {
//         QL_UART_DEMO_LOG("OTA start failed, ret=%d", ret);
//         // 发送失败响应
//         char json_err[64];
//         sprintf(json_err, "{\"ota_state\": \"Error %d\"}", ret);
//         lt_uart2frx8016_send(json_err, strlen(json_err));
//     }
// }

int lt_uart2frx8016_battery_get()
{
    return battery;
}

void l_act_dispose(cJSON *root)
{
    if (root == NULL)
    {
        return;
    }

    cJSON *act = cJSON_GetObjectItem(root, "act");
    if (act != NULL && memcmp(act->valuestring, "q", 1) == 0)
    {
        cJSON *para = cJSON_GetObjectItem(root, "para");
        if (para != NULL && memcmp(para->valuestring, "factory", 7) == 0)
        {
            char json[128] = {0};
            char sn[32] = {0};
            factory_param_sn_get(sn, sizeof(sn));
            // #if TYPE_FUNC == TYPE_SICHUAN
            if(get_type_func() == TYPE_SICHUAN)
            {
          //  sprintf(json, FACTORY_FARMAT, "LTCIIT-HHT", sn,HARDWARE_VERSION ,SOFTWARE_VERSION, factory_param_deviceType_get());
            sprintf(json, FACTORY_FARMAT, "ZR-ZHRHT", sn,HARDWARE_VERSION ,SOFTWARE_VERSION, factory_param_deviceType_get());
            // #else
            }
            else 
            {
            sprintf(json, FACTORY_FARMAT, "LTCIIT-HHT", sn,HARDWARE_VERSION ,SOFTWARE_VERSION, factory_param_deviceType_get());
            // #endif
            }
            lt_uart2frx8016_send(json, strlen(json));
        }
        else if (para != NULL && memcmp(para->valuestring, "service", 7) == 0)
        {
            char json[512] = {0};
            char service[64] = {0};
            char user[64] = {0};
            char psd[64] = {0};
            char id[64] = {0};
            int port = mqtt_param_mqttPort_get();
            mqtt_param_mqttServer_get(service, sizeof(service));
            mqtt_param_client_user_get(user, sizeof(user));
            mqtt_param_client_pass_get(psd, sizeof(psd));
            mqtt_param_client_identity_get(id, sizeof(id));
            sprintf(json, SERVICE_FARMAT, service, port, user, psd, id);
            lt_uart2frx8016_send(json, strlen(json));
        }
        else if (para != NULL && memcmp(para->valuestring, "module", 6) == 0)
        {
            char tmp[16] = {0};
            if (ql_dev_get_imei(tmp, sizeof(tmp), 0) == 0)
            {
                char json[128] = {0};
                sprintf(json, MODULE_FARMAT, tmp, "", "");
                lt_uart2frx8016_send(json, strlen(json));
            }
        }
        else if (para != NULL && memcmp(para->valuestring, "status", 6) == 0)
        {
            char json[200] = {0};
            char sosPhone[15] = {0};
            char familyPhone[15] = {0};
            char area[25] = {0};
            mqtt_param_phone_get(familyPhone, sizeof(familyPhone), FAMILY_TYPE);
            mqtt_param_sosPhone_get(sosPhone, sizeof(sosPhone));
            sprintf(area,"%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                    ebs_param.resources_code[0], ebs_param.resources_code[1],
                    ebs_param.resources_code[2], ebs_param.resources_code[3],
                    ebs_param.resources_code[4], ebs_param.resources_code[5],
                    ebs_param.resources_code[6], ebs_param.resources_code[7],
                    ebs_param.resources_code[8], ebs_param.resources_code[9],
                    ebs_param.resources_code[10], ebs_param.resources_code[11]);
            sprintf(json, STATUS_FARMAT, sosPhone, familyPhone,
                    ebs_param.reg_ip[0], ebs_param.reg_port[0], ebs_param.lbk_ip[0], ebs_param.lbk_port[0], area);
            lt_uart2frx8016_send(json, strlen(json));
        }

        cJSON *ble_version = cJSON_GetObjectItem(root, "ble_version");
        if (ble_version != NULL)
        {
            set_ble_version(ble_version->valuestring);
            QL_UART_DEMO_LOG("Test1:ble_version=%s",ble_version->valuestring);
        }
    }
    else if (act != NULL && memcmp(act->valuestring, "s", 1) == 0)
    {
        cJSON *fpara = cJSON_GetObjectItem(root, "fpara");
        cJSON *npara = cJSON_GetObjectItem(root, "npara");
        if (fpara != NULL)
        {
            for (int i = 0; i < sizeof(fpara_dispose) / sizeof(fpara_dispose[0]); i++)
            {
                cJSON *temp = cJSON_GetObjectItem(fpara, fpara_dispose[i].flag);
                if (temp != NULL)
                {
                    fpara_dispose[i].funcDispose(temp);
                }
            }
            char json[32] = "{\"state\": 0}";
            lt_uart2frx8016_send(json, strlen(json));
        }
        int cmd_fg = 0;
        if (npara != NULL)
        {
            for (int i = 0; i < sizeof(npara_dispose) / sizeof(npara_dispose[0]); i++)
            {
                cJSON *temp = cJSON_GetObjectItem(npara, npara_dispose[i].flag);
                if (temp != NULL)
                {
                    npara_dispose[i].funcDispose(temp);
                    if (memcmp(npara_dispose[i].flag, "battery", 7) != 0)
                    {
                        //
                    }
                    else
                    {
                        cmd_fg = 1;
                        QL_UART_DEMO_LOG("battery");
                    }
                }
            }
            if (cmd_fg == 0)
            {
                char json[32] = "{\"state\": 0}";
                lt_uart2frx8016_send(json, strlen(json));
            }
        }
    }
    else
    {
        // other
    }
}

static void lt_uart_notify_cb(unsigned int ind_type, ql_uart_port_number_e port, unsigned int size)
{
    unsigned char *recv_buff = calloc(1, QL_UART_RX_BUFF_SIZE + 1);
    unsigned int real_size = 0;
    int read_len = 0;
    QlOSStatus err = QL_OSI_SUCCESS;
    lt_conf_msg_t msg;

    QL_UART_DEMO_LOG("UART port %d receive ind type:0x%x, receive data size:%d", port, ind_type, size);
    switch (ind_type)
    {
    case QUEC_UART_RX_OVERFLOW_IND: // rx buffer overflow
    case QUEC_UART_RX_RECV_DATA_IND:
    {
        // ======== MODIFICATION START ========
        if (g_uart_state.mode == UART_MODE_OTA)
        {
            // OTA 模式：将数据读入 OTA 专用缓冲区
            if (!g_uart_state.ota_recv_buffer)
            {
                // 缓冲区未初始化，丢弃数据
                while (size > 0)
                {
                    read_len = ql_uart_read(port, recv_buff, MIN(size, QL_UART_RX_BUFF_SIZE));
                    if (read_len > 0)
                        size -= read_len;
                    else
                        break;
                }
                break;
            }

            while (size > 0)
            {
                // 计算 OTA 缓冲区剩余空间
                uint16_t space_left = UART_OTA_RECV_BUFF_LEN - g_uart_state.ota_recv_idx;
                if (space_left == 0)
                {
                    // OTA 缓冲区满了，丢弃数据
                    QL_UART_DEMO_LOG("OTA recv buffer full, dropping data");
                    while (size > 0)
                    {
                        read_len = ql_uart_read(port, recv_buff, MIN(size, QL_UART_RX_BUFF_SIZE));
                        if (read_len > 0)
                            size -= read_len;
                        else
                            break;
                    }
                    break;
                }

                real_size = MIN(size, space_left);
                read_len = ql_uart_read(port, g_uart_state.ota_recv_buffer + g_uart_state.ota_recv_idx, real_size);

                if (read_len > 0)
                {
                    g_uart_state.ota_recv_idx += read_len;
                    size -= read_len;

                    // (重新)启动帧接收超时定时器
                    ql_rtos_timer_stop(g_uart_state.recv_timer_ref);
                    ql_rtos_timer_start(g_uart_state.recv_timer_ref, UART_RECV_FRAME_TIMEOUT_MS, FALSE);
                }
                else
                {
                    break;
                }
            }
        }
        else // UART_MODE_NORMAL
        {
            // 正常模式：使用原逻辑，将数据发往队列
            while (size > 0)
            {
                memset(recv_buff, 0, QL_UART_RX_BUFF_SIZE + 1);
                real_size = MIN(size, QL_UART_RX_BUFF_SIZE);
                read_len = ql_uart_read(port, recv_buff, real_size);

                msg.type = MSG_RECV;
                msg.datalen = read_len;
                msg.data = malloc(msg.datalen);
                memcpy(msg.data, recv_buff, msg.datalen);

                if ((err = ql_rtos_queue_release(lt_conf_msg_queue, sizeof(lt_conf_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
                {
                    QL_UART_DEMO_LOG("send msg to lt_conf_msg_queue failed, err=%d", err);
                    free(msg.data); // 失败时释放
                }

                if ((read_len > 0) && (size >= read_len))
                {
                    size -= read_len;
                }
                else
                {
                    break;
                }
            }
        }
        // ======== MODIFICATION END ========
        break;
    }
    case QUEC_UART_TX_FIFO_COMPLETE_IND:
    {
        QL_UART_DEMO_LOG("tx fifo complete");
        break;
    }
    }
    free(recv_buff);
    recv_buff = NULL;
}
//ASR功能列表

#if 0
static asr_type_state_t asr_type_state[]=
{
    {ASR_AUDIO_FM,            NULL,  NULL,  NULL,  },
 	{ASR_AUDIO_MP3,           NULL,  NULL,  NULL,  },
    {ASR_AUDIO_SMS,           NULL,  NULL,  NULL,  },
    {ASR_CALL_FAMILY,   NULL,  NULL,  NULL,  },
    {ASR_CALL_SOS,      NULL,  NULL,  NULL,  },
    {ASR_STATE_VOLTAGE,     NULL,  NULL,  NULL,  },
    {ASR_STATE_WEATHER,     NULL,  NULL,  NULL,  },
    {ASR_STATE_SINGAL,     NULL,  NULL,  NULL,  },
    {ASR_STATE_MATHINE,     NULL,  NULL,  NULL,  }

};

#else
static asr_type_state_t asr_type_state[]=
{
    {ASR_AUDIO, ASR_FM,         NULL,  NULL,  NULL,  },
 	{ASR_AUDIO, ASR_MP3,        NULL,  NULL,  NULL,  },
    {ASR_AUDIO, ASR_SMS,        NULL,  NULL,  NULL,  },
    {ASR_CALL,  ASR_FAMILY,     NULL,  NULL,  NULL,  },
    {ASR_CALL,  ASR_SOS,        NULL,  NULL,  NULL,  }

};
#endif
//注册ASR回调函数
void ltasr_callback_register(asr_type_state_t *reg)
{
    uint16_t num=0;

    for (num = 0; num < sizeof(asr_type_state) / sizeof(asr_type_state[0]); num++)
    {
        if(reg->function == asr_type_state[num].function && reg->state == asr_type_state[num].state)
        {
            if(0 == lt_led_show_onoff())//如果按键的第一下屏幕是灭的话 直接先唤醒
            {
              //  led_on();
            }

            if(reg->start_function)
                asr_type_state[num].start_function = reg->start_function;
            if(reg->stop_function)
                asr_type_state[num].stop_function  = reg->stop_function;
            //if(reg->pause_function)
                //asr_type_state[num].pause_function  = reg->pause_function;
            if(reg->next_function)
                asr_type_state[num].next_function  = reg->next_function;
            if(reg->last_function)
                asr_type_state[num].last_function  = reg->last_function;
            break;                           
        }
    }

}

uint8_t asr_state=0;
uint8_t audio_flag = 0;

//回调函数
void test_asr_process(uint8_t function, uint8_t state)
{
    uint8_t num;
    for (num = 0; num < sizeof(asr_type_state) / sizeof(asr_type_state[0]); num++)
    {
        if(function == asr_type_state[num].function && state == asr_type_state[num].state)
        {
            ql_rtos_task_sleep_ms(1000);
            if (audio_flag == ASR_START && asr_type_state[num].start_function)
                asr_type_state[num].start_function(NULL);
            else if (audio_flag == ASR_STOP && asr_type_state[num].stop_function)
                asr_type_state[num].stop_function(NULL);
            else if (audio_flag == ASR_NEXT && asr_type_state[num].next_function)
                asr_type_state[num].next_function(NULL);
            else if (audio_flag == ASR_LAST && asr_type_state[num].last_function)
                asr_type_state[num].last_function(NULL);
            //else if (now_state ==0xff && asr_type_state[num].pause_function)
                //asr_type_state[num].pause_function(NULL);
        }
    }   
}

//ASR处理函数
//ASR_SYSTEM

void asr_system_handle(uint8_t data)
{
    if(data==0x02)
    {
        char *buf="设备将在5秒后重新启动.";
        ltapi_play_tts(buf, strlen(buf));
        ql_rtos_task_sleep_s(5);
        ql_power_reset(RESET_NORMAL);

    }else if(data==0x00)
    {
        lt_shutdown();
    }
}
//ASR_AUDIO
void asr_audio_handle(uint8_t data)
{
    uint8_t now_state = ltplay_get_src();
    if(now_state == SND_FM_URL )
    {
        now_state = ASR_FM;
    }
    if (now_state == SND_MP3_URL)
    {
        now_state = ASR_MP3;
    }
    switch(data)
    {
        case 0x00:  asr_state = now_state;
                    audio_flag = ASR_STOP; //停止播放
                    break;
        case 0x01:  asr_state = ASR_FM;
                    audio_flag = ASR_START;
                    break;
        case 0x02:  asr_state = ASR_MP3;
                    audio_flag = ASR_START;
                    break;
        case 0x03:  asr_state = ASR_SMS;
                    audio_flag = ASR_START;
                    break;
        case 0x04:  if(ltplay_get_src() == SND_MP3 || ltplay_get_src() == SND_FM)
                    {
                        asr_state = now_state;
                        audio_flag = ASR_LAST;
                    }       //上一曲
                    break;
        case 0x05:  if(ltplay_get_src() == SND_MP3 || ltplay_get_src() == SND_FM)
                    {
                        asr_state = now_state;
                        audio_flag = ASR_NEXT;
                    }       //下一曲

                    break;
        default:    
                    break;
    }

}
//ASR_CALL

uint8_t now_state = 0;
void asr_call_handle(uint8_t data)
{
    uint8_t now_state = ltplay_get_src();
    if(now_state == SND_FM_URL )
    {
        now_state = ASR_FM;
    }
    if (now_state == SND_MP3_URL)
    {
        now_state = ASR_MP3;
    }

    switch (data)
    {
    case 0x00:
        asr_state = now_state;
        audio_flag = ASR_STOP;
        now_state = 0;
        break;
    case 0x01:
        asr_state = ASR_FAMILY;
        now_state = asr_state;
        audio_flag = ASR_START;
        break;
    case 0x02:
        asr_state = ASR_SOS;
        now_state = asr_state;
        audio_flag = ASR_START;
        break;
    case 0x04:
        if (now_state == SND_MP3 || now_state == SND_FM)
        {
            asr_state = now_state;
            audio_flag = ASR_LAST;
        } // 上一曲
        break;
    case 0x05:
        if (now_state == SND_MP3 || now_state == SND_FM)
        {
            asr_state = now_state;
            audio_flag = ASR_NEXT;
        } // 下一曲
        break;
    default:
        break;
    }
}
//ASR_STATE

static void asr_state_singal(void)
{
    ql_sim_status_e card_status = 0;

    ql_sim_get_card_status(0, &card_status);

    if (card_status == QL_SIM_STATUS_NOSIM)
    {
        char *buf="未插入SIM卡.";
        ltapi_play_tts(buf, strlen(buf));
    }
    else
    {
        unsigned char ca = 0;
        if (!ql_nw_get_csq(0, &ca))
        {
            if (ca > 20 && ca <= 31)
            {
                char *buf="当前信号质量优.";
                ltapi_play_tts(buf, strlen(buf));
            }
            else if (ca <= 20 && ca > 10)
            {
                char *buf="当前信号质量一般.";
                ltapi_play_tts(buf, strlen(buf));
            }
            else if (ca <= 10 && ca >= 1)
            {
                char *buf="当前信号质量差.";
                ltapi_play_tts(buf, strlen(buf));
            }
            else
            {
                char *buf="当前信号未连接.";
                ltapi_play_tts(buf, strlen(buf));
            }
        }
    }
}
#include "ltvoicecall.h"
void asr_state_handle(uint8_t data)
{
    if(data==0x01)
    {
        //char buf[]="当前电量百分之";
        //char *buf2=(char *)battery;
        //strcat(buf, buf2);
        //ltapi_play_tts(buf, strlen(buf));
    }else if(data==0x02)
    {
        //char *buf="今天天气状态.";
        //ltapi_play_tts(buf, strlen(buf));
        if (1 != lt_connect_status_get())
        {
            char *buf = "当前网络未连接,不可用";
            ltapi_play_tts(buf, strlen(buf));
            //  return;
        }
        else
        {
            api_mqtt_weather_Publish();
        }
    }else if(data==0x03)
    {
        asr_state_singal();
    }else if(data==0x04)
    {
        //char *buf="";
        //ltapi_play_tts(buf, strlen(buf));
    }
}

uint8_t volume_temp;

typedef struct lt_asr
{
    int lt_asr_weakup;

} lt_asr_t;

static lt_asr_t ltasr_t = {
    .lt_asr_weakup = 0,
};

void asr_wakeup()
{
    char *len_on = "len_on";
    lt_panel_light_msg(0, len_on, strlen(len_on) + 1);
    ltasr_t.lt_asr_weakup = 1;
}
void asr_shutdown()
{
    if (ltasr_t.lt_asr_weakup == 1)
    {
        ltasr_t.lt_asr_weakup = 0;
        
    } 
}
void asr_ok()
{
    if (ltasr_t.lt_asr_weakup == 1)
    {
        ltasr_t.lt_asr_weakup = 0;
        
    } 
}

int lt_ltasr_wakeup()
{
    return   ltasr_t.lt_asr_weakup;
}
void test_asr_handle(uint8_t asr_function, uint8_t data)
{  
    //音量处理
    // if(asr_function ==0xff && data ==0xff)  //退出唤醒
    // {
    //     // lt_audio_pa_disable_pw();
    //     //lt_adc_set_volume_instantly();
    // }
    // else
    // {
    //     ql_set_volume(3);
    // }

    switch (asr_function)
    {
    case ASR_WAKEUP:
        asr_wakeup();
        break;
    case ASR_SHUTDOWN:
        asr_shutdown();                                                                                                                                    
        break;
    case ASR_SYSTEM:
        asr_ok();
        asr_system_handle(data);
        break;
    case ASR_AUDIO:
        asr_ok();
        asr_audio_handle(data);
        break;
    case ASR_CALL:
        asr_ok();
        asr_call_handle(data);
        break;
    case ASR_STATE:
        asr_ok();
        asr_state_handle(data);
        break;
    default:
        break;
    }
    if((audio_flag == ASR_NEXT || audio_flag == ASR_LAST) &&(asr_function == ASR_CALL)) 
    {
        asr_function = ASR_AUDIO;
    }
    QL_UART_DEMO_LOG("Test1:asr_function=%02x asr_state=%02x",asr_function,asr_state);
    test_asr_process(asr_function, asr_state);   
}

char asr_version[10];
char ble_version[10];

void set_asr_version(char *version)
{
    memcpy(asr_version, version, 10);
}
void set_ble_version(char *version)
{
    memcpy(ble_version, version, 10);
}

char *get_asr_version()
{
    return asr_version;
}

char *get_ble_version()
{
    return ble_version;
}

static void lt_uart2frx8016_thread(void *param)
{
    int write_len = 0;
    while (1)
    {
        lt_conf_msg_t msg;
        ql_rtos_queue_wait(lt_conf_msg_queue, (uint8 *)(&msg), sizeof(lt_conf_msg_t), 0xFFFFFFFF);
        QL_UART_DEMO_LOG("Test1:read_len=%d", msg.datalen);

        switch (msg.type)
        {
        case MSG_RECV:
            if (msg.data != NULL)
            {
                /*处理收到的json数据*/
                //QL_UART_DEMO_LOG("read_len=%d, recv_data=%s", msg.datalen, msg.data);
                #if 1
                if(msg.data[0]==0x49)
                {
                    for(int i=0; i<msg.datalen; i++)
                    {
                        QL_UART_DEMO_LOG("Test1:recv_data[%d]=%02x",i,msg.data[i] );
                    }
                    //QL_UART_DEMO_LOG("Test1:recv_data[5]=%02x recv_data[8]=%02x",msg.data[5],msg.data[8] );
                    if(msg.data[1] == 0xaa)
                    {
                        // 获取语音版本号
                        char asr_version_temp[10];
                        memcpy(asr_version_temp, msg.data+2, msg.datalen -2);
                        asr_version_temp[msg.datalen -2] = '\0';
                        QL_UART_DEMO_LOG("Test1:asr_version_temp=%s",asr_version_temp );
                        set_asr_version(asr_version_temp);
                        
                    }
                    else 
                    {
                        test_asr_handle(msg.data[5],msg.data[8]);
                    }
                }
              
                #endif
                cJSON *root = cJSON_Parse((const char *)msg.data);

                if (root != NULL)
                {
                    for (int i = 0; i < sizeof(m_dispose) / sizeof(m_dispose[0]); i++)
                    {
                        cJSON *temp = cJSON_GetObjectItem(root, m_dispose[i].flag);
                        if (temp != NULL)
                        {
                            m_dispose[i].funcDispose(root);
                            break;
                        }
                    }
                    cJSON_Delete(root);
                }
            }
            break;

        case MSG_SEND:
            write_len = ql_uart_write(QL_CUR_UART_PORT, msg.data, msg.datalen);
            QL_UART_DEMO_LOG("write_len:%d data:%s", write_len, msg.data);
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

void lt_uart2frx8016_send(char *data, int data_len)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    lt_conf_msg_t msg;
    msg.type = MSG_SEND;
    msg.datalen = data_len;
    msg.data = malloc(msg.datalen);
    memcpy(msg.data, data, msg.datalen);

    if ((err = ql_rtos_queue_release(lt_conf_msg_queue, sizeof(lt_conf_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
    {
        QL_UART_DEMO_LOG("send msg to lt_conf_msg_queue failed, err=%d", err);
    }
}

static void lt_uart_lpup_cb()
{
    char buf[4]={0xa4,0xa4,0x01,0x00};
    lt_uart2frx8016_send(buf,4);
}
static void lt_uart_lpdown_cb()
{
    char buf[4]={0xa4,0xa4,0x01,0x01};
    lt_uart2frx8016_send(buf,4);
}

static void lt_lp_uart_callback_register()
{
    lt_lp_callback_cfg_t reg;
    reg.lp_id = UART_PRJ;
    reg.lt_lpdown_callback = lt_uart_lpdown_cb;
    reg.lt_lpup_callback = lt_uart_lpup_cb;
    lt_lp_callback_register(&reg);
}


void lt_uart2frx8016_init(void)
{
    int ret = 0;
    QlOSStatus err = 0;
    ql_task_t uart_task = NULL;

    lt_uart_ota_init();//初始化单片机ota 

    ql_uart_config_s uart_cfg = {0};

    uart_cfg.baudrate = QL_UART_BAUD_115200;
    uart_cfg.flow_ctrl = QL_FC_NONE;
    uart_cfg.data_bit = QL_UART_DATABIT_8;
    uart_cfg.stop_bit = QL_UART_STOP_1;
    uart_cfg.parity_bit = QL_UART_PARITY_NONE;

    ret = ql_uart_set_dcbconfig(QL_CUR_UART_PORT, &uart_cfg);
    QL_UART_DEMO_LOG("ql_uart_set_dcbconfig ret: 0x%x", ret);

    if (QL_UART_SUCCESS != ret)
    {
        QL_UART_DEMO_LOG("ql_uart_set_dcbconfig ret: 0x%x", ret);
        return;
    }

    ret = ql_pin_set_func(QL_CUR_UART_TX_PIN, QL_CUR_UART_TX_FUNC);
    if (QL_GPIO_SUCCESS != ret)
    {
        QL_UART_DEMO_LOG("ql_pin_set_func ret: 0x%x", ret);
        return;
    }
    ret = ql_pin_set_func(QL_CUR_UART_RX_PIN, QL_CUR_UART_RX_FUNC);
    if (QL_GPIO_SUCCESS != ret)
    {
        QL_UART_DEMO_LOG("ql_pin_set_func ret: 0x%x", ret);
        return;
    }

    ret = ql_uart_open(QL_CUR_UART_PORT);
    QL_UART_DEMO_LOG("ql_uart_open ret: 0x%x", ret);

    if (QL_UART_SUCCESS == ret)
    {
        ret = ql_uart_register_cb(QL_CUR_UART_PORT, lt_uart_notify_cb);
        QL_UART_DEMO_LOG("ql_uart_register_cb ret: 0x%x", ret);

        /*开启一个uart信息处理的msg——queue*/
        ql_rtos_queue_create(&lt_conf_msg_queue, sizeof(lt_conf_msg_t), 5);

        /*开启串口信息处理线程*/
        err = ql_rtos_task_create(&uart_task, QL_UART_TASK_STACK_SIZE, QL_UART_TASK_PRIO,
                                  "ltuart2frx8016", lt_uart2frx8016_thread, NULL, QL_UART_TASK_EVENT_CNT);
        if (err != QL_OSI_SUCCESS)
        {
            QL_UART_DEMO_LOG("ltuart2frx8016 task created failed");
            return;
        }
    }
    //注册串口 low_power回调
    lt_lp_uart_callback_register();

    return;
}
