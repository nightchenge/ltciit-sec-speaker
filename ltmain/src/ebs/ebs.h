/**
 * @file ebs.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-07-31
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _EBS_H_
#define _EBS_H_
#define PLATFORM_LINUX 0
#define PLATFORM_Ex800G 1
#define PLATFORM PLATFORM_Ex800G

#if PLATFORM == PLATFORM_LINUX

#define ebs_sleep_ms(ms) usleep((ms)*1000)
#define ebs_sleep(s) sleep((s))

#define ebs_time time

#elif PLATFORM == PLATFORM_Ex800G

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_log.h"
#include "ql_api_osi.h"
#include "ql_osi_def.h"

// #define LT_LOG(level, module_str, msg, ...) QL_LOG_PRINTF_TAG(level, LT_LOG_TAG_EBS, module_str ":%s %d " msg "", __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define ebs_log(msg, ...) QL_LOG(QL_LOG_LEVEL_INFO, "ebs", msg, ##__VA_ARGS__)

#define HTONS(val) ((uint16_t)(((val) >> 8) | ((val) << 8)))
#define HTONL(val) ((uint32_t)(((val) >> 24) | (((val) >> 8) & 0xFF00) | (((val) << 8) & 0xFF0000) | ((val) << 24)))
#define TOUPPER(x) ((x) >= 'a' && (x) <= 'z' ? (x)-32 : (x))

#define ebs_sleep_ms(ms) ql_rtos_task_sleep_ms(ms)
#define ebs_sleep(s) ql_rtos_task_sleep_s(s)

static inline time_t ebs_time(time_t *t)
{
    ql_timeval_t tv;
    ql_gettimeofday(&tv);
    if (t)
        *t = tv.sec;
    return tv.sec;
}

static inline int dec_str2bcd_arr(const char *str, uint8_t *arr, size_t arr_len)
{
    if (!str || !arr || arr_len == 0)
    {
        return -1;
    }
    size_t len = strlen(str);
    len += len % 2;
    if (len > arr_len * 2)
    {
        return -1;
    }

    size_t i = 0, j = 0;
    for (i = 0; i < len; i++)
    {
        if (str[i] == '\0')
            break;
        if (i % 2)
            arr[j++] += str[i] - '0';
        else
            arr[j] = (str[i] - '0') << 4;
    }

    return 0;
}

static inline int hex_str2bcd_arr(const char *str, uint8_t *arr, size_t arr_len)
{
    if (!str || !arr || arr_len == 0)
    {
        return -1;
    }
    size_t len = strlen(str);

    // Ensure that the length of the string is even
    if (len % 2 != 0)
    {
        return -1;
    }

    // Calculate the maximum BCD array length required
    size_t bcd_len = len / 2;

    if (bcd_len > arr_len)
    {
        return -1;
    }

    size_t i, j = 0;
    for (i = 0; i < len; i += 2)
    {
        char hex_byte[3] = {str[i], str[i + 1], '\0'};
        arr[j++] = (uint8_t)strtol(hex_byte, NULL, 16);
    }

    return 0;
}

#endif // PLATFORM

#endif // !_EBS_H_