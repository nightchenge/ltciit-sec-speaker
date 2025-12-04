/**
 * @file util_timer.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-08-03
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _UTIL_TIME_H_
#define _UTIL_TIME_H_
#include "ebs.h"
#if PLATFORM == PLATFORM_LINUX
#include <stdint.h>
#include <time.h>
#include <sys/time.h>
static inline void _get_time_of_day(struct timeval *val)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    val->tv_sec = ts.tv_sec;
    val->tv_usec = ts.tv_nsec / 1000;
}

static inline void timer_start(time_t *t)
{
    struct timeval tv;
    _get_time_of_day(&tv);
    *t = tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static inline uint8_t timer_expired(time_t *t, uint32_t dly)
{
    time_t now;
    struct timeval tv;
    _get_time_of_day(&tv);
    now = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return (now - *t) > dly;
}
#elif PLATFORM == PLATFORM_Ex800G
#include <stdint.h>
#include "ql_api_osi.h"

static inline void timer_start(time_t *t)
{
    int64_t now;
    now = ql_rtos_up_time_ms();
    *t = now;
}

static inline uint8_t timer_expired(time_t *t, uint32_t dly)
{
    int64_t now;
    now = ql_rtos_up_time_ms();
    return (now - *t) > dly;
}
#endif

#endif
