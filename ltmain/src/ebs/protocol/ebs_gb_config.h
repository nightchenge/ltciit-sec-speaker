/**
 * @file ebs_gb_config.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-11-22
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _EBS_GB_CONFIG_H_
#define _EBS_GB_CONFIG_H_
#include "ebs.h"
#include <stdint.h>

#ifdef PROT_GUANGXI
#define RESOURCES_CODE_LEN 9
#else
#define RESOURCES_CODE_LEN 12
#endif

#define EBM_TYPE_LEN 1
#define EVENT_LEVEL_LEN 1
#define EVENT_TYPE_LEN 5
#define VOLUME_LEN 1
#define START_TIME_LEN 4
#define STOP_TIME_LEN 4
#define SUP_DATA_CNT_LEN 1
#define SUP_DATA_TYPE_LEN 1
#define SUP_DATA_LEN_LEN 2

#ifdef PROT_GUANGXI
#define EBM_ID_LEN 15
#else
#define EBM_ID_LEN 18
#endif

#if PLATFORM == PLATFORM_Ex800G
#define PHY_ADDR_LEN 8
#else
#define PHY_ADDR_LEN 6
#endif
// ERROR CODE
#define COMM_RESULT_OK 0 // 补充国标错误代码类型，自定义
#define COMM_HAS_DATA (-1)
#define COMM_RESULT_NULL (-2)
#define COMM_ERR_RESP (-3)

#define PROT_ERR_BASE 10
#define PROT_ERR_UNDEF PROT_ERR_BASE
#define PROT_ERR_TIMEOUT (PROT_ERR_BASE + 1)
#define PROT_ERR_VER (PROT_ERR_BASE + 2)
#define PROT_ERR_DATA (PROT_ERR_BASE + 3)
#define PROT_ERR_PARAM (PROT_ERR_BASE + 4)
#define PROT_ERR_CRC (PROT_ERR_BASE + 5)

#define SYS_ERR_BASE 30
#define SYS_ERR_UNDEF SYS_ERR_BASE
#define SYS_ERR_BUSY (SYS_ERR_BASE + 1)
#define SYS_ERR_STORE_CARD (SYS_ERR_BASE + 2)
#define SYS_ERR_READ_FILE (SYS_ERR_BASE + 3)
#define SYS_ERR_WRITE_FILE (SYS_ERR_BASE + 4)
#define SYS_ERR_UKEY_LACK (SYS_ERR_BASE + 5)
#define SYS_ERR_UKEY_ILLEGAL (SYS_ERR_BASE + 6)

#define VERF_ERR_BASE 50
#define VERF_ERR_PASSWD (VERF_ERR_BASE + 1)
#define VERF_ERR_CERT_ILLEGAL (VERF_ERR_BASE + 2)
#define VERF_ERR_INPUT_TIMEOUT (VERF_ERR_BASE + 3)
#define VERF_ERR_PARAM_ILLEGAL (VERF_ERR_BASE + 4)
#define VERF_ERR_FUNC (VERF_ERR_BASE + 5)
#define VERF_ERR_SMS_FORMAT (VERF_ERR_BASE + 6)
#define VERF_ERR_NUMBER_INVALD (VERF_ERR_BASE + 7)
#define VERF_ERR_CONTEND (VERF_ERR_BASE + 8)
#define VERF_ERR_RESOURCE_CODE (VERF_ERR_BASE + 9)

#define DEV_ERR_BASE 70
#define DEV_ERR_UNDEF (DEVICE_ERR_BASE + 1)
#define DEV_ERR_OFFLINE (DEVICE_ERR_BASE + 2)
#define DEV_ERR_BUSY (DEVICE_ERR_BASE + 3)
typedef int comm_result_t;

#define CMD_RESULT_OK 0
#define CMD_ERR_BASE 80
#define CMD_ERR_UNDEF (CMD_ERR_BASE + 1)
#define CMD_ERR_REGION (CMD_ERR_BASE + 2)
#define CMD_ERR_PARAM (CMD_ERR_BASE + 3)
#define CMD_ERR_HAS_DATA (CMD_ERR_BASE + 4)
#define CMD_ERR_TIME_OUT (PROT_ERR_TIMEOUT)
typedef int cmd_result_t;

typedef float volume_t;
typedef uint32_t freq_t;
typedef uint8_t direction_t;

#define CHN_NULL_MASK 0
#define CHN_IP_MASK 0x01
#define CHN_4G_MASK 0x02
#define CHN_FM_MASK 0x04

#define CHN_MAX_CNT 3

#endif
