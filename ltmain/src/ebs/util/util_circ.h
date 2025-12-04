/**
 * @file util_circ.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-12-06
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _UTIL_CIRC_H_
#define _UTIL_CIRC_H_
#include "ebs.h"
#include "util_mutex.h"
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

#define is_power_of_2(x) ((x) != 0 && (((x) & ((x)-1)) == 0))
#define min(a, b) (((a) < (b)) ? (a) : (b))

typedef struct circular_buffer
{
    uint8_t *buff;
    uint32_t size;
    uint32_t in;
    uint32_t out;
    ebs_mutex_t *mutex;
    uint32_t (*get_len)(struct circular_buffer *buffer);
    uint32_t (*put)(struct circular_buffer *buffer, void *buff, uint32_t len);
    uint32_t (*get)(struct circular_buffer *buffer, void *buff, uint32_t len);
} circular_buffer_t;

uint32_t circular_buffer_len(circular_buffer_t *buffer);
uint32_t circular_buffer_put(circular_buffer_t *buffer, void *buff, uint32_t len);
uint32_t circular_buffer_get(circular_buffer_t *buffer, void *buff, uint32_t len);

#define DECLARE_CIRCULAR_BUFFER(name, size_max, mutex_ptr) \
    uint8_t buff_circ_##name[size_max] = {0};              \
    circular_buffer_t circ_buffer_##name = {               \
        .buff = buff_circ_##name,                          \
        .size = size_max,                                  \
        .in = 0,                                           \
        .out = 0,                                          \
        .mutex = mutex_ptr,                                \
        .get_len = circular_buffer_len,                    \
        .put = circular_buffer_put,                        \
        .get = circular_buffer_get,                        \
    }

#endif
