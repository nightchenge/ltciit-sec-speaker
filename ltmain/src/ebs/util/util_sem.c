/**
 * @file util_sem.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2024-02-28
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#include "util_sem.h"
#if PLATFORM == PLATFORM_LINUX

#elif PLATFORM == PLATFORM_Ex800G
int ebs_sem_create(ebs_sem_t *sem, int init_cnt)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    if ((err = ql_rtos_semaphore_create(sem, init_cnt)) != QL_OSI_SUCCESS)
    {
        ebs_log("create semaphore failed, err=%d", err);
        return -1;
    }
    return 0;
}

int ebs_sem_wait(ebs_sem_t *sem, uint32_t timeout)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    if ((err = ql_rtos_semaphore_wait(*sem, timeout)) != QL_OSI_SUCCESS)
    {
        ebs_log("wait semaphore failed, err=%d", err);
        return -1;
    }
    return 0;
}

int ebs_sem_post(ebs_sem_t *sem)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    if ((err = ql_rtos_semaphore_release(*sem)) != QL_OSI_SUCCESS)
    {
        ebs_log("post semaphore failed, err=%d", err);
        return -1;
    }
    return 0;
}

int ebs_sem_get_cnt(ebs_sem_t *sem, uint32_t *cnt)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    if ((err = ql_rtos_semaphore_get_cnt(*sem, (uint32 *)cnt)) != QL_OSI_SUCCESS)
    {
        ebs_log("get semaphore count failed, err=%d", err);
        return -1;
    }
    return 0;
}

int ebs_sem_delete(ebs_sem_t *sem)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    if ((err = ql_rtos_semaphore_delete(*sem)) != QL_OSI_SUCCESS)
    {
        ebs_log("delete semaphore failed, err=%d", err);
        return -1;
    }
    return 0;
}
#endif