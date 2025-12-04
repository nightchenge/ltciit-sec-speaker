/**
 * @file util_msg.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-07-31
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */

#include "util_msg.h"

#if PLATFORM == PLATFORM_LINUX

#elif PLATFORM == PLATFORM_Ex800G
int ebs_msg_queue_creat(ebs_queue_t *msg_queue, uint32_t max_size, uint32_t max_num)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    if ((err = ql_rtos_queue_create(msg_queue, max_size, max_num)) != QL_OSI_SUCCESS)
    {
        ebs_log("create msg queue failed, err=%d", err);
        return -1;
    }
    return 0;
}

int ebs_msg_queue_send(ebs_queue_t msg_queue, uint8_t *msg, uint32_t size, uint32_t timeout)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    if ((err = ql_rtos_queue_release(msg_queue, size, msg, timeout)) != QL_OSI_SUCCESS)
    {
        ebs_log("send msg to queue failed, err=%d", err);
        return -1;
    }
    return 0;
}

int ebs_msg_queue_wait(ebs_queue_t msg_queue, uint8_t *msg, uint32_t size, uint32_t timeout)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    if ((err = ql_rtos_queue_wait(msg_queue, msg, size, timeout)) != QL_OSI_SUCCESS)
    {
        // ebs_log("wait msg from queue failed, err=%d", err);
        return -1;
    }
    return 0;
}

int ebs_msg_queue_get_cnt(ebs_queue_t msg_queue, uint32_t *cnt)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    if ((err = ql_rtos_queue_get_cnt(msg_queue, (uint32 *)cnt)) != QL_OSI_SUCCESS)
    {
        ebs_log("get msg queue count failed, err=%d", err);
        return -1;
    }
    return 0;
}

int ebs_msg_queue_delete(ebs_queue_t msg_queue)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    if ((err = ql_rtos_queue_delete(msg_queue)) != QL_OSI_SUCCESS)
    {
        ebs_log("delete msg queue failed, err=%d", err);
        return -1;
    }
    return 0;
}

#endif