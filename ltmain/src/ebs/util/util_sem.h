/**
 * @file util_sem.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2024-02-28
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _UTIL_SEM_H_
#define _UTIL_SEM_H_
#include "ebs.h"
#if PLATFORM == PLATFORM_LINUX

#elif PLATFORM == PLATFORM_Ex800G
#include "ql_api_osi.h"
typedef ql_sem_t ebs_sem_t;

int ebs_sem_create(ebs_sem_t *sem, int init_cnt);
int ebs_sem_wait(ebs_sem_t *sem, uint32_t timeout);
int ebs_sem_post(ebs_sem_t *sem);
int ebs_sem_get_cnt(ebs_sem_t *sem, uint32_t *cnt);
int ebs_sem_delete(ebs_sem_t *sem);
#endif // PLATFORM

#endif
