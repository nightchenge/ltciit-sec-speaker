/**
 * @file ebs_cmd.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-12-07
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#include "ebs.h"

#if PLATFORM == PLATFORM_LINUX
#include "ebs_cmd.h"
#include "ebs_log.h"
#include "ebm_task_type.h"
#include "ip_gb_comm.h"
#include "ebm_task_play.h"
#include "ebs_param.h"
#include "ebs_startup.h"
#include "upgrade.h"
#include "version.h"
#include "hwdrv.h"
#include "ebs_nms.h"
#include "version.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <arpa/inet.h>
#include <stdlib.h>
#elif PLATFORM == PLATFORM_Ex800G
#include "ebs_cmd.h"
#include "ebm_task_type.h"
#include "ip_gb_comm.h"
#include "ebm_task_play.h"
#include "ebs_param.h"
#include "ql_power.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#endif

#define TIME_CONV(x) ((x)[0] << 24 | (x)[1] << 16 | (x)[2] << 8 | (x)[3])
#define VER_CONVERT(a, b) ((a - '0') << 4) | (b - '0')
#define FM_FREQ_GET(a, b)       \
    do                          \
    {                           \
        uint32_t t = 1;         \
        while (b)               \
        {                       \
            a += t * (b & 0xF); \
            t *= 10;            \
            b >>= 4;            \
        }                       \
    } while (0)

#define DEC_BCD(x, b)                 \
    do                                \
    {                                 \
        x = ((b) / 10) << 4 | b % 10; \
    } while (0)

#ifndef PROT_GUANGXI
cmd_result_t cmd_play_start(uint16_t cmd_len, uint8_t *cmd, uint16_t *resp_len, uint8_t *resp)
{
    uint16_t len = 0;
    if (cmd_len == 0)
    {
        ebs_log("recv play start cmd error,data len:%d \n", cmd_len);
        resp[0] = PROT_ERR_PARAM;
        sprintf((char *)(resp + 3), "play command param error!");
        len = strlen((char *)(resp + 3));
        *(uint16_t *)(resp + 1) = HTONS(len);
        *resp_len = len + 3;
        return CMD_ERR_UNDEF;
    }

    task_result_t result;
    ebm_task_msg_t msg = {0};
    ebm_task_t task;
    uint8_t sup_cnt;
    uint8_t sup_type;
    uint16_t sup_len;
    uint8_t i;
    freq_t freq;
    char ebm_id[64] = {0};
    ebs_log("======recv start play cmd====== \n");
    memset(&task, 0, sizeof(ebm_task_t));
    task.status = TASK_STATUS_WAIT;
    task.chn_mask = comm_reg_channel_get() == 0x01 ? CHN_IP_MASK : CHN_4G_MASK;
    task.prev = task.next = NULL;
    memcpy(task.ebm_id, cmd, EBM_ID_LEN);
    for (i = 0; i < EBM_ID_LEN; i++)
        sprintf(&ebm_id[i * 2], "%02X", task.ebm_id[i]);
    ebs_log("task ebm id:%s \n", ebm_id);
    len = EBM_ID_LEN;
    task.ebm_type = cmd[len++];
    task.severity = cmd[len++];
    memcpy(task.event_type, cmd + len, EVENT_TYPE_LEN);
    len += EVENT_TYPE_LEN;
    task.volume = cmd[len++];
    task.start_time = TIME_CONV(cmd + len);
    len += START_TIME_LEN;
    task.stop_time = TIME_CONV(cmd + len);
    len += STOP_TIME_LEN;

    sup_cnt = cmd[len++];
    for (i = 0; i < sup_cnt; i++)
    {
        sup_type = cmd[len++];
        sup_len = (cmd[len] << 8) | cmd[len + 1];
        len += SUP_DATA_LEN_LEN;
        if (sup_type == 0x3D)
        {
            memcpy(task.url, cmd + len, sup_len);
            if (strlen(task.url) < 8)
            {
                ebs_log("add task error:%s \n", task.url);
                resp[0] = CMD_ERR_UNDEF;
                sprintf((char *)(resp + 3), "play task add error!");
                len = strlen((char *)(resp + 3));
                *(uint16_t *)(resp + 1) = HTONS(len);
                *resp_len = len + 3;
                return CMD_ERR_UNDEF;
            }
            if (TOUPPER(task.url[0]) == 'R' && TOUPPER(task.url[1]) == 'T')
            {
                if (TOUPPER(task.url[2]) == 'P' && task.url[3] == ':')
                    task.audio_source = AUDIO_RTP;
                else
                    task.audio_source = AUDIO_RTSP;
            }
            else if (TOUPPER(task.url[0]) == 'U' && TOUPPER(task.url[1]) == 'D' && TOUPPER(task.url[2]) == 'P')
            {
                task.audio_source = AUDIO_UDP;
            }
            else if (TOUPPER(task.url[0]) == 'H' && TOUPPER(task.url[1]) == 'T' && TOUPPER(task.url[2]) == 'T' && TOUPPER(task.url[3]) == 'P')
            {
                task.audio_source = AUDIO_HTTP;
            }
            else if (TOUPPER(task.url[0]) == 'F' && TOUPPER(task.url[1]) == 'M')
            {
                freq = atoi(&task.url[6]);
                task.fm_freq = (freq > 10800) ? (freq / 10) : freq;
                task.audio_source = AUDIO_FM;
                task.is_ip_fm = true;
                task.chn_mask = CHN_FM_MASK;
            }
            else
            {
                ebs_log("add task error:%s \n", task.url);
                resp[0] = CMD_ERR_UNDEF;
                sprintf((char *)(resp + 3), "play task add error!");
                len = strlen((char *)(resp + 3));
                *(uint16_t *)(resp + 1) = HTONS(len);
                *resp_len = len + 3;
                return CMD_ERR_UNDEF;
            }
            task.url[sup_len + 1] = '\0';
            break;
        }
    }
    memcpy(msg.buff, &task, sizeof(ebm_task_t));
    msg.type = TASK_ADD;
    result = eb_player_task_msg_send(&msg);
    if (result == TASK_RESULT_OK)
    {
        memset(resp, 0, 3);
        *resp_len = 3;
        return CMD_RESULT_OK;
    }
    else
    {
        ebs_log("add task error:%d \n", result);
        resp[0] = CMD_ERR_UNDEF;
        sprintf((char *)(resp + 3), "play task add error!");
        len = strlen((char *)(resp + 3));
        *(uint16_t *)(resp + 1) = HTONS(len);
        *resp_len = len + 3;
        return CMD_ERR_UNDEF;
    }
}
#else

#endif

cmd_result_t cmd_play_stop(uint16_t cmd_len, uint8_t *cmd, uint16_t *resp_len, uint8_t *resp)
{
    uint16_t len = 0;
    if (cmd_len == 0 || strlen((char *)cmd) == 0)
    {
        ebs_log("task ebm id error! \n");
        resp[0] = PROT_ERR_PARAM;
        sprintf((char *)(resp + 3), "ebm id error!");
        len = strlen((char *)(resp + 3));
        *(uint16_t *)(resp + 1) = HTONS(len);
        *resp_len = len + 3;
        return CMD_ERR_UNDEF;
    }
    ebs_log("======recv stop play cmd====== \n");
    ebm_task_msg_t msg = {0};
    msg.type = TASK_DEL;
    memcpy(msg.buff, cmd, cmd_len);
    if (eb_player_task_msg_send(&msg) == TASK_RESULT_OK)
    {
        memset(resp, 0, 3);
        *resp_len = 3;
        return CMD_RESULT_OK;
    }
    else
    {
        resp[0] = SYS_ERR_BUSY;
        sprintf((char *)(resp + 3), "Device is busying");
        len = strlen((char *)(resp + 3));
        *(uint16_t *)(resp + 1) = HTONS(len);
        *resp_len = len + 3;
        return CMD_ERR_UNDEF;
    }
}

cmd_result_t cmd_param_query(uint16_t cmd_len, uint8_t *cmd, uint16_t *resp_len, uint8_t *resp)
{
    lbk_msg_t msg = {.type = 0, .len = 0, .data = {0}};
    if (cmd_len == 0 || strlen((char *)cmd) == 0)
    {
        uint16_t len = 0;
        ebs_log("param query error len is 0 \n");
        msg.data[0] = 13;
        sprintf((char *)&msg.data[3], "param(status) query error:param len is 0!");
        strcpy((char *)&resp[3], (char *)&msg.data[3]);
        len = strlen((char *)&msg.data[3]);
        *(uint16_t *)(msg.data + 1) = HTONS(len);
        *(uint16_t *)(resp + 1) = HTONS(len);
        len += 3;
        *resp_len = len;
        msg.type = LBK_STATUS_QUERY;
        msg.len = len;
        ebs_lbk_msg_send(&msg);
        return CMD_ERR_UNDEF;
    }
    ebs_log("==== param query cmd exec ====\n");
    uint8_t i;
    uint8_t index = 0;
    uint8_t cnt = cmd[index++];
    uint8_t type;

    msg.data[msg.len++] = 0;
    msg.data[msg.len++] = 0;
    msg.data[msg.len++] = 0;
    msg.data[msg.len++] = cnt;
    ebs_log("==== query paramater count:%d ====\n", cnt);
    for (i = 0; i < cnt; i++)
    {
        type = cmd[index++];
        ebs_log("query param type:%02X \n", type);
        switch (type)
        {
        case 0x01: //! 音量
        {
            msg.data[msg.len++] = 0x01;
            msg.data[msg.len++] = 1;
            msg.data[msg.len++] = ebs_param.volume;
        }
        break;
        case 0x02: //! 终端本地IP地址信息
        {
            msg.data[msg.len++] = 0x02;
            msg.data[msg.len++] = 12;
#if PLATFORM == PLATFORM_LINUX
            db_ebs_nms_t ebs_nms;
            ebs_nms.index = 0;
            db_ebs_nms_entry_get(&ebs_nms);
            *(uint32_t *)(&msg.data[msg.len]) = htonl(ebs_nms.ip);
            msg.len += 4;
            *(uint32_t *)(&msg.data[msg.len]) = htonl(ebs_nms.mask);
            msg.len += 4;
            *(uint32_t *)(&msg.data[msg.len]) = htonl(ebs_nms.gw);
            msg.len += 4;
#elif PLATFORM == PLATFORM_Ex800G
            *(uint32_t *)(&msg.data[msg.len]) = HTONL(ebs_nms.ip);
            msg.len += 4;
            *(uint32_t *)(&msg.data[msg.len]) = HTONL(ebs_nms.mask);
            msg.len += 4;
            *(uint32_t *)(&msg.data[msg.len]) = HTONL(ebs_nms.gw);
            msg.len += 4;
#endif
        }
        break;
        case 0x03: //! 回传地址
        {
            msg.data[msg.len++] = 0x03;
            msg.data[msg.len++] = 7;
            msg.data[msg.len++] = 0x01; // 0x01 IP:PORT 0x02 域名:PORT 0x03 SMS号码
            uint8_t k = comm_reg_channel_get() == 0x01 ? 0 : 1;
            *(uint32_t *)(&msg.data[msg.len]) = HTONL(ebs_param.lbk_ip[k]);
            msg.len += 4;
            *(uint16_t *)(&msg.data[msg.len]) = HTONS(ebs_param.lbk_port[k]);
            msg.len += 2;
        }
        break;
        case 0x04: //! 资源编码
        {
            msg.data[msg.len++] = 0x04;
            msg.data[msg.len++] = RESOURCES_CODE_LEN;
            memcpy(&msg.data[msg.len], ebs_param.resources_code, RESOURCES_CODE_LEN);
            msg.len += RESOURCES_CODE_LEN;
        }
        break;
        case 0x05: //! 物理地址
        {
            msg.data[msg.len++] = 0x05;
            msg.data[msg.len++] = PHY_ADDR_LEN + 1;
            msg.data[msg.len++] = PHY_ADDR_LEN;
            memcpy(&msg.data[msg.len], ebs_param.phy_addr, PHY_ADDR_LEN);
            msg.len += PHY_ADDR_LEN;
        }
        break;
        case 0x06: //! 工作状态
        {
            msg.data[msg.len++] = 0x06;
            msg.data[msg.len++] = 1;
            msg.data[msg.len++] = device_work_mode_status_get();
        }
        break;
        case 0x07: //! 故障代码
        {
            msg.data[msg.len++] = 0x07;
            msg.data[msg.len++] = 1;
            msg.data[msg.len++] = 0;
        }
        break;
        case 0x08: //! 设备类型
        {
            msg.data[msg.len++] = 0x08;
            msg.data[msg.len++] = 2;
            msg.data[msg.len++] = 0;
            msg.data[msg.len++] = 2;
        }
        break;
        case 0x09: //! 硬件版本号
        {
            msg.data[msg.len++] = 0x09;
            msg.data[msg.len++] = 4;
            // DEC_BCD(msg.data[msg.len++], (HW_VERSION_MAJOR));
            // DEC_BCD(msg.data[msg.len++], (HW_VERSION_MINOR));
            // msg.data[msg.len++] = 0;
            // DEC_BCD(msg.data[msg.len++], (HW_VERSION_PATCH));
        }
        break;
        case 0x0A: //! 软件版本号
        {
            msg.data[msg.len++] = 0x0A;
            msg.data[msg.len++] = 4;
            // char *ver_str = LT_SW_VERSION;
            // msg.data[msg.len++] = VER_CONVERT(ver_str[3], ver_str[4]);
            // msg.data[msg.len++] = VER_CONVERT(ver_str[6], ver_str[7]);
            // msg.data[msg.len++] = VER_CONVERT(ver_str[13], ver_str[14]);
            // msg.data[msg.len++] = VER_CONVERT(ver_str[15], ver_str[16]);
        }
        break;
        case 0x0B: //! 调频信号状态
        {
            // TODO 获取FM信号状态
            msg.data[msg.len++] = 0x0B;
            msg.data[msg.len++] = 0;
            // msg.data[msg.len++] = 2;
            // msg.data[msg.len++] = 0;
            // msg.data[msg.len++] = 0;
        }
        break;
        case 0x10:
        {
            msg.data[msg.len++] = 0x10;
            msg.data[msg.len++] = 0;
            // msg.data[msg.len++] = 0x6;
            // msg.data[msg.len++] = ebs_param.fm_list.fm_cnt;
            // uint8_t k;
            // for (k = 0; k < ebs_param.fm_list.fm_cnt; k++)
            // {
            //     msg.data[msg.len++] = ebs_param.fm_list.fm_param[k].fm_index;
            //     msg.data[msg.len++] = ebs_param.fm_list.fm_param[k].fm_priority;
            //     freq_t freq = ebs_param.fm_list.fm_param[k].fm_freq / 10;
            //     msg.data[msg.len++] = ((freq / 100000) << 4) | ((freq / 10000) % 10);
            //     msg.data[msg.len++] = (((freq / 1000) % 10) << 4) | ((freq / 100) % 10);
            //     msg.data[msg.len++] = (((freq / 10) % 10) << 4) | (freq % 10);
            // }
        }
        break;
        case 0x11: //! FM当前频点
        {
            // TODO 获取FM当前频点
            msg.data[msg.len++] = 0x11;
            msg.data[msg.len++] = 0;
        }
        break;
        case 0x12: //! FM维持指令模式
        {
            msg.data[msg.len++] = 0x12;
            msg.data[msg.len++] = 0;
            // msg.data[msg.len++] = 3;
            // msg.data[msg.len++] = ((ebs_param.fm_remain_period & 0xFF00) >> 8);
            // msg.data[msg.len++] = (ebs_param.fm_remain_period & 0xFF);
        }
        break;
        case 0xFD: //! 4G 信号强度
        {
            msg.data[msg.len++] = 0xFD;
            msg.data[msg.len++] = 0;
            // TODO
        }
        break;
        case 0xFA: //! 4G注册服务器
        {
            msg.data[msg.len++] = 0xFA;
            msg.data[msg.len++] = 0;
            // TODO
        }
        break;
        case 0xF9: //! 4G回传服务器
        {
            msg.data[msg.len++] = 0xF9;
            msg.data[msg.len++] = 0;
            // TODO
        }
        break;
        case 0xEF: //! 通道优先级
        {
            msg.data[msg.len++] = 0xEF;
            msg.data[msg.len++] = CHN_MAX_CNT;
            memcpy(&msg.data[msg.len], ebs_param.chn_priority, CHN_MAX_CNT);
            msg.len += CHN_MAX_CNT;
        }
        break;
        case 0xFE: //! 功放状态
        {
            msg.data[msg.len++] = 0xFE;
            msg.data[msg.len++] = 1;
            // msg.data[msg.len++] = hwdrv_power_amplifier_get() == MUTE_STATUS_ON ? 0x02 : 0x01;
        }
        break;
        default:
        {
            msg.data[msg.len++] = type;
            msg.data[msg.len++] = 0;
        }
        }
    }
    msg.type = LBK_STATUS_QUERY;
    ebs_lbk_msg_send(&msg);
    memset(resp, 0, 3);
    *resp_len = 3;
    return CMD_RESULT_OK;
}

cmd_result_t cmd_param_set(uint16_t cmd_len, uint8_t *cmd, uint16_t *resp_len, uint8_t *resp)
{
    uint16_t len = 0;
    // cmd_result_t result = CMD_RESULT_OK;
    if (cmd_len == 0 || strlen((char *)cmd) == 0)
    {
        ebs_log("set param error len is 0! \n");
        resp[0] = PROT_ERR_PARAM;
        sprintf((char *)(resp + 3), "param setting error:param len is 0!");
        len = strlen((char *)(resp + 3));
        *(uint16_t *)(resp + 1) = HTONS(len);
        *resp_len = len + 3;
        return CMD_ERR_UNDEF;
    }
    uint8_t *p = cmd;
    uint8_t i = 0;
    uint8_t param_num = *p++;
    uint8_t param_type = 0;
    uint8_t param_len = 0;
    uint8_t *param = NULL;
    for (i = 0; i < param_num; i++)
    {
        param_type = *p++;
        param_len = *p++;
        param = p;
        ebs_log("set param cnt:%d index:%d type:%d, len:%d \n", param_num, i, param_type, param_len);
        switch (param_type)
        {
        case 0x01: // 音量
        {
            ebs_log("set volume:%d \n", param[0]);
            if (param[0] >= 0 && param[0] <= 100)
            {
                ebs_param.volume = param[0];
            }
        }
        break;
        case 0x02: // 本机IP地址、掩码和网关
        {          //! EG800G模组不支持设置本地IP地址
#if PLATFORM == PLATFORM_LINUX
            db_ebs_nms_t ebs_nms;
            ebs_nms.index = 0;
            db_ebs_nms_entry_get(&ebs_nms);
            ebs_log("set ip:%d.%d.%d.%d \n", param[0], param[1], param[2], param[3]);
            ebs_nms.ip = param[0] << 24 | param[1] << 16 | param[2] << 8 | param[3];
            param += 4;
            ebs_log("set mask:%d.%d.%d.%d \n", param[0], param[1], param[2], param[3]);
            ebs_nms.mask = param[0] << 24 | param[1] << 16 | param[2] << 8 | param[3];
            param += 4;
            ebs_log("set gateway:%d.%d.%d.%d \n", param[0], param[1], param[2], param[3]);
            ebs_nms.gw = param[0] << 24 | param[1] << 16 | param[2] << 8 | param[3];
            db_ebs_nms_entry_modify(&ebs_nms); // TODO
            ebs_nms_restore(0);
#endif
        }
        case 0x03: // 回传地址
        {
            ebs_log("set callback ip:%d.%d.%d.%d \n", param[0], param[1], param[2], param[3]);
            ebs_param.lbk_ip[0] = param[0] << 24 | param[1] << 16 | param[2] << 8 | param[3];
            param += 4;
            ebs_log("set callback port:%d \n", param[0] << 8 | param[1]);
            ebs_param.lbk_port[0] = param[0] << 8 | param[1];
            comm_lbk_ip_addr_set(ebs_param.lbk_ip[0]);
            comm_lbk_ip_port_set(ebs_param.lbk_port[0]);
        }
        break;
        case 0x04: // 物理地址&资源编码
        {
            ebs_log("set resources code \n");
            uint8_t phy_index = param[0];
            uint8_t phy[PHY_ADDR_LEN] = {0};
#if PLATFORM == PLATFORM_LINUX
            startup_phy_addr_get(phy);
#elif PLATFORM == PLATFORM_Ex800G
            memcpy(phy, ebs_param.phy_addr, PHY_ADDR_LEN);
#endif
            if (phy_index == PHY_ADDR_LEN && memcmp(param + 1, phy, PHY_ADDR_LEN) == 0)
            {
                memcpy(ebs_param.resources_code, param + phy_index + 1, RESOURCES_CODE_LEN);
            }
        }
        break;
        case 0x05: // 功放开关
        {
            ebs_log("set amplifier switch:%d \n", param[0]);
            if (param[0] & 0x01)
            {
                // hwdrv_power_amplifier_set(MUTE_STATUS_OFF);
                // TODO
            }
            else
            {
                // hwdrv_power_amplifier_set(MUTE_STATUS_ON);
                // TODO
            }
        }
        break;
        case 0x06: // 校时
        {          //! EG800G模组不依赖平台时间
#if PLATFORM == PLATFORM_LINUX
            ebs_log("set time\n");
            time_t utc_time = 0;
            utc_time = param[0] << 24 | param[1] << 16 | param[2] << 8 | param[3];
            struct tm *tm = localtime(&utc_time);
            char cmd[128] = {0};
            sprintf(cmd, "date -s \"%d-%d-%d %d:%d:%d\"", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
            system(cmd);
#endif
        }
        break;
        case 0x07: // 回传周期
        {
            uint32_t period;
            period = param[0] << 24 | param[1] << 16 | param[2] << 8 | param[3];
            ebs_log("set callback period:%d \n", period);
            ebs_param.lbk_period = period;
        }
        break;
        case 0xFA: // 4G回传服务器设置
        {
        }
        break;
        case 0xFB: // 4G注册服务器设置
        {
        }
        break;
        case 0xFC: // 注册服务器设置
        {
            ebs_log("set reg server ip\n");
            if (param[0] == 0x01)
            {
                ebs_param.reg_ip[0] = param[1] << 24 | param[2] << 16 | param[3] << 8 | param[4];
                comm_reg_ip_addr_set(ebs_param.reg_ip[0]);
                comm_reg_reconnection();
            }
        }
        break;
        case 0xFE: // 重启
        {
            ebs_log("reboot\n");
#if PLATFORM == PLATFORM_LINUX
            system("reboot");
#elif PLATFORM == PLATFORM_Ex800G
            ql_power_reset(RESET_NORMAL);
#endif
        }
        break;
        case 0xFD: // 恢复出厂设置
        {
            ebs_param_reset();
#if PLATFORM == PLATFORM_LINUX
            system("reboot");
#elif PLATFORM == PLATFORM_Ex800G
            ql_power_reset(RESET_NORMAL);
#endif
        }
        break;
        case 0xEF: // 通道优先级
        {
            ebs_log("set channel priority\n");
            char prior[CHN_MAX_CNT + 1] = {0};
            memcpy(prior, param, CHN_MAX_CNT);
            if (eb_player_channel_priority_set(prior) == 0)
            {
                memcpy(ebs_param.chn_priority, prior, CHN_MAX_CNT);
            }
        }
        break;
        default:
            ebs_log("parameter type:%02X not support.\n", param_type);
            break;
        }
        p += 2;
        p += param_len;
    }
#if PLATFORM == PLATFORM_LINUX
    db_ebs_param_entry_modify(&ebs_param);
    db_save(EBS_DB_FILE);
#elif PLATFORM == PLATFORM_Ex800G
    ebs_param_save();
#endif
    memset(resp, 0, 3);
    *resp_len = 3;
    return CMD_RESULT_OK;
}

cmd_result_t cmd_cert_update(uint16_t cmd_len, uint8_t *cmd, uint16_t *resp_len, uint8_t *resp)
{
    uint16_t len = 0;
    if (cmd_len == 0 || strlen((char *)cmd) == 0)
    {
        ebs_log("recv cert cmd error len is 0! \n");
        resp[0] = PROT_ERR_PARAM;
        sprintf((char *)(resp + 3), "cert update error:len is 0!");
        len = strlen((char *)(resp + 3));
        *(uint16_t *)(resp + 1) = HTONS(len);
        *resp_len = len + 3;
        return CMD_ERR_UNDEF;
    }
    // TODO
    return CMD_RESULT_OK;
}

cmd_result_t cmd_upgrade(uint16_t cmd_len, uint8_t *cmd, uint16_t *resp_len, uint8_t *resp)
{
    uint16_t len = 0;
    if (cmd_len == 0 || strlen((char *)cmd) == 0)
    {
        ebs_log("recv cert cmd error len is 0! \n");
        resp[0] = PROT_ERR_PARAM;
        sprintf((char *)(resp + 3), "upgrade cmd error:len is 0!");
        len = strlen((char *)(resp + 3));
        *(uint16_t *)(resp + 1) = HTONS(len);
        *resp_len = len + 3;
        return CMD_ERR_UNDEF;
    }
    ebs_log("recv upgrade cmd! \n");
#if PLATFORM == PLATFORM_LINUX
    uint8_t index = 0;
    uint8_t *p = cmd;
    uint8_t param_num = *p++;
    uint8_t param_type;
    uint8_t param_len;
    uint16_t manufacturer_id;
    uint16_t device_type;
    uint32_t version;
    char md5[33] = {0};
    uint8_t url[256] = {0};
    uint8_t enforce_upgrade;
    for (index = 0; index < param_num; index++)
    {
        param_type = *p++;
        param_len = *p++;
        switch (param_type)
        {
        case 0x01:
            if (param_len == 2)
            {
                // manufacturer_id = HTONS(*(uint16_t *)p);
            }
            break;
        case 0x02:
            if (param_len == 2)
            {
                // device_type = HTONS(*(uint16_t *)p);
            }
            break;
        case 0x03:
            if (param_len == 4)
            {
                // version = HTONL(*(uint32_t *)p);
            }
            break;
        case 0x04:
            if (param_len == 16)
            {
                for (int i = 0; i < 16; i++)
                {
                    sprintf(md5 + 2 * i, "%02x", p[i]);
                }
            }
            break;
        case 0x05:
            if (param_len < 256)
            {
                memcpy(url, p, param_len);
                url[param_len] = '\0';
            }
            break;
        case 0x06:
            if (param_len == 1)
            {
                // enforce_upgrade = *p++;
            }
            break;
        default:
            break;
        }
        p += param_len;
    }
    char *ver_str = LT_SW_VERSION;
    uint8_t v[4] = {0};
    uint32_t ver = 0;
    v[3] = VER_CONVERT(ver_str[3], ver_str[4]);
    v[2] = VER_CONVERT(ver_str[6], ver_str[7]);
    v[1] = VER_CONVERT(ver_str[13], ver_str[14]);
    v[0] = VER_CONVERT(ver_str[15], ver_str[16]);
    ver = p[3] << 24 | p[2] << 16 | p[1] << 8 | p[0];
    if (ver < version || enforce_upgrade)
    {
        char cmd[512];
        sprintf(cmd, "wget -O /tmp/upgrade.bin %s", url);
        system(cmd);
        sprintf(cmd, "md5sum /tmp/upgrade.bin");
        FILE *fp = popen(cmd, "r");
        char md5sum[33] = {0};
        fread(md5sum, 1, 32, fp);
        pclose(fp);
        if (memcmp(md5sum, md5, 32) == 0)
        {
            lt_upgrade_unblock("/tmp/upgrade.bin");
        }
        else
        {
            ebs_log("upgrade file md5 error! \n");
            ebs_log("md5:%s \n", md5sum);
            ebs_log("cmd md5:%s \n", md5);
            system("rm /tmp/upgrade.bin");
        }
    }
#elif PLATFORM == PLATFORM_Ex800G

#endif
    memset(resp, 0, 3);
    *resp_len = 3;
    return CMD_RESULT_OK;
}

const ebs_ip_cmd_t cmd_list[] = {
    {0x01, cmd_play_start},
    {0x02, cmd_play_stop},
    {0x11, cmd_param_query},
    {0x12, cmd_param_set},
    {0x17, cmd_cert_update},
    {0xA1, cmd_upgrade},
};

static uint8_t cmd_cnt = sizeof(cmd_list) / sizeof(cmd_list[0]);

static uint8_t _find_cmd_in_list(uint8_t type)
{
    uint8_t i = 0;
    for (i = 0; i < cmd_cnt; i++)
    {
        if (cmd_list[i].type == type)
            return i;
    }
    return 0xFF;
}

cmd_result_t ebs_ip_cmd_run(uint8_t cmd_type, uint16_t cmd_len, uint8_t *cmd, uint16_t *resp_len, uint8_t *resp)
{
    uint8_t index = 0xFF;
    index = _find_cmd_in_list(cmd_type);
    if (index < cmd_cnt && cmd_list[index].cmd_callback)
        return cmd_list[index].cmd_callback(cmd_len, cmd, resp_len, resp);
    return CMD_ERR_UNDEF;
}

bool resources_code_match(uint8_t *resources_code)
{
    uint8_t i;
    uint16_t dst_town = 0, dst_village = 0;
    uint16_t local_town = 0, local_village = 0;

    for (i = 0; i < RESOURCES_CODE_LEN; i++)
    {
        if (i == 0)
        {
            if (!(resources_code[i] & 0x0F) || (resources_code[i] & 0x0F) <= (ebs_param.resources_code[i] & 0x0F)) //! 级别高于当前级别的资源码符合条件
                continue;
            else
                return false;
        }
        else if (i == 7 || i == 8 || i == 10)
        {
            if (!resources_code[i] || resources_code[i] == ebs_param.resources_code[i]) //! 资源子类型码也做通配
                continue;
            else
                return false;
        }
        else if (i >= 4 && i <= 6)
        {
            if (i == 4)
            {
                dst_town = ((resources_code[i] << 4) & 0x0FF0);
                local_town = ((ebs_param.resources_code[i] << 4) & 0x0FF0);
            }
            else if (i == 5)
            {
                dst_town |= resources_code[i] >> 4;
                local_town |= ebs_param.resources_code[i] >> 4;
                if (!dst_town || dst_town == local_town)
                {
                    dst_village = (resources_code[i] << 8) & 0x0F00;
                    local_village = (ebs_param.resources_code[i] << 8) & 0x0F00;
                }
                else
                    return false;
            }
            else
            {
                dst_village |= resources_code[i];
                local_village |= ebs_param.resources_code[i];
                if (!dst_village || dst_village == local_village)
                    continue;
                else
                    return false;
            }
        }
        else
        {
            if (!resources_code[i] || resources_code[i] == ebs_param.resources_code[i])
                continue;
            else
                return false;
        }
    }
    return true;
}