/**
 * @file ebs_cmd.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-12-07
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _EBS_CMD_H_
#define _EBS_CMD_H_
#include <stdint.h>
#include <stdbool.h>
#include "ebs_gb_config.h"

typedef cmd_result_t (*cmd_func)(uint16_t cmd_len, uint8_t *cmd, uint16_t *resp_len, uint8_t *resp);
typedef struct ebs_ip_cmd
{
    uint8_t type;
    cmd_func cmd_callback;
} ebs_ip_cmd_t;

cmd_result_t ebs_ip_cmd_run(uint8_t cmd_type, uint16_t cmd_len, uint8_t *cmd, uint16_t *resp_len, uint8_t *resp);

// FM
typedef cmd_result_t (*fm_cmd_func)(uint16_t cmd_len, uint8_t *cmd);
typedef struct ebs_fm_cmd
{
    uint8_t type;
    fm_cmd_func cmd_callback;
} ebs_fm_cmd_t;

cmd_result_t ebs_fm_cmd_run(uint8_t cmd_type, uint16_t cmd_len, uint8_t *cmd);

bool resources_code_match(uint8_t *resources_code);

#endif
