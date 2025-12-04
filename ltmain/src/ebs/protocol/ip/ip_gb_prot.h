/**
 * @file ip_gb_prot.h
 * @author lijintang (lijintang@lootom.com)
 * @brief 
 * @version 0.1
 * @date 2021-11-22
 * 
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 * 
 */
#ifndef _IP_GB_PROT_H_
#define _IP_GB_PROT_H_
#include <stdbool.h>
#include "ip_gb_config.h"
#include "ip_gb_comm_type.h"
#include "ip_gb_comm.h"

#define FIRST_REG_INDEX 42
#define PHASE_IDLE 0
#define PHASE_HEAD 1
#define PHASE_MSG_START 2
#define PHASE_MSG_DST_OBJ 3
#define PHASE_MSG_CMD_HEAD 4
#define PHASE_MSG_CMD_DATA 5
#define PHASE_VERF_START 6
#define PHASE_SIGN_INFO 7
#define PHASE_CRC 8
#define PHASE_END 9

#define DATA_MSB_CONV(x) ((x)[0] << 24 | (x)[1] << 16 | (x)[2] << 8 | (x)[3])

int time_tick(ip_gb_trans_t *this, uint32_t ticks);
void recv_reset(ip_gb_trans_t *trans);
int reg_send(ip_gb_trans_t *trans, uint8_t session[], bool need_sign, ip_gb_prot_msg_t *msg);
int lbk_send(ip_gb_trans_t *trans, uint8_t session[], bool need_sign, ip_gb_prot_msg_t *msg);
comm_result_t prot_parse(ip_gb_trans_t *trans, uint8_t *data, int length);
comm_result_t cmd_parse(ip_gb_trans_t *trans);
comm_result_t resp_parse(ip_gb_trans_t *trans);
comm_result_t ip_gb_common_response(ip_gb_trans_t *trans);

#endif
