/**
 * @file ip_gb_comm.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-11-22
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _IP_GB_COMM_H_
#define _IP_GB_COMM_H_
#include "ip_gb_comm_type.h"
#include "ip_gb_prot.h"
#include <stdint.h>

bool ebs_reg_msg_send(reg_msg_t *msg);
bool ebs_lbk_msg_send(lbk_msg_t *msg);

int ebs_comm_task_init(void);
int ebs_comm_task_deinit(void);

uint8_t device_work_mode_status_get(void);
void device_work_mode_status_set(uint8_t status);

uint8_t ebs_comm_reg_status_get(void);

uint32_t comm_lbk_period_get(void);
void comm_lbk_period_set(uint32_t period);
int comm_lbk_channel_get(void);
void comm_lbk_channel_mode_set(uint8_t mode);
void comm_lbk_ip_addr_set(uint32_t ip);
void comm_lbk_ip_port_set(uint16_t port);
void comm_lbk_4g_addr_set(uint32_t ip);
void comm_lbk_4g_port_set(uint16_t port);

uint32_t comm_reg_period_get(void);
void comm_reg_period_set(uint32_t period);
int comm_reg_channel_get(void);
void comm_reg_channel_mode_set(uint8_t mode);
void comm_reg_ip_addr_set(uint32_t ip);
void comm_reg_ip_port_set(uint16_t port);
void comm_reg_4g_addr_set(uint32_t ip);
void comm_reg_4g_port_set(uint16_t port);
void comm_reg_reconnection(void);

#if PLATFORM == PLATFORM_Ex800G
void comm_reg_restart_connection(void);
#endif

#endif
