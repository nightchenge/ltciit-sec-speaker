/**
 * @file ip_gb_config.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-11-17
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _IP_GB_CONFIG_H_
#define _IP_GB_CONFIG_H_
#include <stdint.h>
#include "ebs_gb_config.h"

#ifdef PROT_GUANGXI
#define PROT_VER 0x0200
#else
#define PROT_VER 0x0100
#endif

#define PACK_HEAD_LEN 2
#define PROT_VER_LEN 2
#define SESSION_LEN 4
#define DATA_PACK_TYPE_LEN 1
#define SIGN_MARK_LEN 1
#define DATA_PACK_LEN_LEN 2
#define DST_OBJ_CNT_LEN 2
#define CMD_DATA_TYPE_LEN 1
#define CMD_DATA_LEN_LEN 2
#define DATA_SIGN_LEN_LEN 2
#define DATA_SIGN_TIME_LEN 4
#define SIGN_CERT_LEN 6
#define SIGN_DATA_LEN_LEN 2
#define SIGN_TIME_LEN 4
#define SIGN_DATA_LEN 64
#define CRC32_LEN 4

#define PACK_HEAD_INDEX 0
#define PROT_VER_INDEX (PACK_HEAD_INDEX + PACK_HEAD_LEN)
#define SESSION_INDEX (PROT_VER_INDEX + PROT_VER_LEN)
#define DATA_PACK_TYPE_INDEX (SESSION_INDEX + SESSION_LEN)
#define SIGN_MARK_INDEX (DATA_PACK_TYPE_INDEX + DATA_PACK_TYPE_LEN)
#define DATA_PACK_LEN_INDEX (SIGN_MARK_INDEX + SIGN_MARK_LEN)
#define SRC_OBJ_CODE_INDEX (DATA_PACK_LEN_INDEX + DATA_PACK_LEN_LEN)
#define DST_OBJ_COUNT_INDEX (SRC_OBJ_CODE_INDEX + RESOURCES_CODE_LEN)
#define DST_OBJ_LIST_INDEX (DST_OBJ_COUNT_INDEX + DST_OBJ_CNT_LEN)

typedef enum msg_lbk_type
{
    LBK_UNDEF = 0,
    LBK_HEART_BEAT,
    LBK_STATUS_QUERY,
    LBK_DEVICE_MALFUNC,
    LBK_TASK_SWITCH,
    LBK_PLAY_RESULT,
} msg_lbk_type_t;

#pragma pack(1)
typedef struct ip_gb_prot_head
{
    uint8_t head[PACK_HEAD_LEN];
    uint8_t ver[PROT_VER_LEN];
    uint8_t session[SESSION_LEN];
    uint8_t pack_type;
    uint8_t sign_mark;
    uint16_t pack_len;
} ip_gb_prot_head_t;

typedef struct ip_gb_prot_msg
{
    uint8_t resources_code[RESOURCES_CODE_LEN];
    uint16_t dst_obj_cnt;
    uint8_t *dst_obj_list;
    uint8_t cmd_type;
    uint16_t cmd_len;
    uint8_t *cmd_data;
} ip_gb_prot_msg_t;

typedef struct reg_msg
{
    uint8_t type;
    uint16_t len;
    uint8_t data[253];
} reg_msg_t;

typedef struct ip_gb_sign_packet
{
    uint16_t len;
    uint8_t time[SIGN_TIME_LEN];
    uint8_t cert[SIGN_CERT_LEN];
    uint8_t data[SIGN_DATA_LEN];
} ip_gb_sign_packet_t;

typedef struct ebs_heart_beat
{
    uint8_t work_status;
    uint8_t first_reg;
    uint8_t phy_addr_len;
    uint8_t phy_addr[PHY_ADDR_LEN];
#ifdef PROT_GUANGXI
    uint8_t ext_type;
    uint8_t ext_len;
    uint8_t ext_data[0];
#endif
} ebs_heart_beat_t;

typedef struct lbk_status
{
    uint8_t result;
    uint16_t len;
    uint8_t info[512];
} lbk_status_t;

typedef struct lbk_malfunc
{
    uint8_t err_mark;
    uint8_t err_type;
    uint8_t info[256];
    uint8_t time[START_TIME_LEN];
} lbk_malfunc_t;

typedef struct lbk_task_switch
{
    uint8_t switch_mark;
    uint8_t task_type;
    uint8_t ebm_id[EBM_ID_LEN];
    uint8_t time[START_TIME_LEN];
} lbk_task_switch_t;

typedef struct lbk_play_result
{
    uint8_t ebm_id[EBM_ID_LEN];
    uint8_t result_code;
    uint8_t info_len[2];
    char info[0];
} lbk_play_result_t;

typedef struct play_result_time
{
    uint8_t start_time[START_TIME_LEN];
    uint8_t stop_time[STOP_TIME_LEN];
    uint8_t play_times;
    uint8_t report_time[START_TIME_LEN];
} play_result_time_t;

typedef struct lbk_play_result_info
{
    uint8_t ebm_id[EBM_ID_LEN];
    uint8_t play_result;
    uint16_t info_len;
    uint8_t *info;
    play_result_time_t time;
} lbk_play_result_info_t;

typedef union ebs_lbk_report_msg {
    lbk_malfunc_t malfunction;
    lbk_task_switch_t task_switch;
    lbk_play_result_t play_result;
} ebs_lbk_report_msg_t;

typedef struct lbk_msg
{
    uint8_t type;
    uint16_t len;
    uint8_t data[512];
} lbk_msg_t;
#pragma pack()

#endif
