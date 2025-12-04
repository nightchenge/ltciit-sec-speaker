/**
 * @file ebs_param.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-12-08
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _EBS_PARAM_H_
#define _EBS_PARAM_H_
#include "ebs.h"
#include <stdint.h>
#include "ip_gb_config.h"
#if PLATFORM == PLATFORM_LINUX
#include "ebs_db.h"
#elif PLATFORM == PLATFORM_Ex800G
#include "ql_fs.h"
#define PARAM_PATH "UFS:"
#define PARAM_FILE PARAM_PATH "ebs_param.bin"
typedef struct
{
    uint8_t fm_index;
    uint8_t fm_priority;
    uint32_t fm_freq;
} fm_param_t;
typedef struct
{
    uint8_t fm_cnt;
    fm_param_t fm_param[5];
} fm_list_t;

typedef struct ebs_param
{
    uint8_t resources_code[12];
    uint8_t phy_addr[PHY_ADDR_LEN];

    uint32_t reg_ip[2];
    uint16_t reg_port[2];
    uint32_t lbk_ip[2];
    uint16_t lbk_port[2];

    uint32_t reg_ip_4g[2];
    uint16_t reg_port_4g[2];
    uint32_t lbk_ip_4g[2];
    uint16_t lbk_port_4g[2];

    uint32_t heart_period;
    uint32_t lbk_period;
    char chn_priority[4];

    uint8_t is_ipsc;
    uint8_t is_upper_first;
    uint8_t is_chn_switch;

    fm_list_t fm_list;
    uint8_t is_frame_crc;
    uint8_t is_fm_remain;
    uint16_t fm_remain_period;

    uint8_t volume;
    int8_t volume_db_max;

    uint8_t net_mode;
    char band[8];
    char apn_name[32];
    char apn_user[32];
    char apn_passwd[32];

    uint32_t crc; // 用于校验
} db_ebs_param_t;

typedef struct ebs_nms
{
    uint32_t index;
    uint32_t ip;
    uint32_t mask;
    uint32_t gw;

    uint8_t mac[13];
    char name[64];
} db_ebs_nms_t;
#endif
extern uint8_t resources_code_remote[RESOURCES_CODE_LEN];
extern db_ebs_param_t ebs_param;
extern db_ebs_nms_t ebs_nms;

#define IP_DATA_WAIT_TIME 10000

int32_t ebs_param_init(uint32_t stage);

int ebs_param_save(void);
int ebs_param_reset(void);
#endif
