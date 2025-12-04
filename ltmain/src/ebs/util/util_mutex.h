/**
 * @file util_mutex.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-08-03
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef __UTIL_MUTEX_H__
#define __UTIL_MUTEX_H__
#include "ebs.h"

#if PLATFORM == PLATFORM_LINUX
#include <pthread.h>

typedef pthread_mutex_t ebs_mutex_t;
#define ebs_mutex_init(mutex) pthread_mutex_init(mutex, NULL)
#define ebs_mutex_trylock(mutex) pthread_mutex_trylock(mutex)
#define ebs_mutex_lock(mutex) pthread_mutex_lock(mutex)
#define ebs_mutex_unlock(mutex) pthread_mutex_unlock(mutex)
#define ebs_mutex_destroy(mutex) pthread_mutex_destroy(mutex)

#elif PLATFORM == PLATFORM_Ex800G

typedef ql_mutex_t ebs_mutex_t;
static inline int ebs_mutex_init(ebs_mutex_t *mutex)
{
    ebs_mutex_t _mutex = NULL;
    ql_rtos_mutex_create(&_mutex);
    *mutex = _mutex;
    return 0;
}

static inline int ebs_mutex_trylock(ebs_mutex_t *mutex)
{
    return ql_rtos_mutex_try_lock(*mutex);
}

static inline int ebs_mutex_lock(ebs_mutex_t *mutex)
{
    return ql_rtos_mutex_lock(*mutex, QL_WAIT_FOREVER);
}

static inline int ebs_mutex_unlock(ebs_mutex_t *mutex)
{
    return ql_rtos_mutex_unlock(*mutex);
}

static inline int ebs_mutex_destroy(ebs_mutex_t *mutex)
{
    return ql_rtos_mutex_delete(*mutex);
}

#endif

#endif // __UTIL_MUTEX_H__