/**
 * @file util_msg.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-07-31
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _UTIL_MSG_H_
#define _UTIL_MSG_H_
#include "ebs.h"

#if PLATFORM == PLATFORM_LINUX
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define ebs_msg_malloc malloc
#define ebs_msg_free mqueue_msg_free

#elif PLATFORM == PLATFORM_Ex800G
#include "ql_api_osi.h"
typedef ql_queue_t ebs_queue_t;

typedef enum
{
    EBS_WAIT_FOREVER = 0xFFFFFFFFUL,
    EBS_NO_WAIT = 0,
} ebs_wait_e;

typedef struct ebs_msg
{
    uint32_t type;
    uint32_t size;
    uint8_t *data;
} ebs_msg_t;

int ebs_msg_queue_creat(ebs_queue_t *msg_queue, uint32_t max_size, uint32_t max_num);
int ebs_msg_queue_send(ebs_queue_t msg_queue, uint8_t *msg, uint32_t size, uint32_t timeout);
int ebs_msg_queue_wait(ebs_queue_t msg_queue, uint8_t *msg, uint32_t size, uint32_t timeout);
int ebs_msg_queue_get_cnt(ebs_queue_t msg_queue, uint32_t *cnt);
int ebs_msg_queue_delete(ebs_queue_t msg_queue);

#define ebs_msg_malloc malloc
#define ebs_msg_free(p) \
    do                  \
    {                   \
        if (p)          \
        {               \
            free(p);    \
            p = NULL;   \
        }               \
    } while (0)

#endif

#endif