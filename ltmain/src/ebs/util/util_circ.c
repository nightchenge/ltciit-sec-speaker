/**
 * @file util_circ.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-12-06
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "util_circ.h"

static uint32_t _buffer_len(circular_buffer_t *this)
{
    return (this->in - this->out);
}

static uint32_t _buffer_put(circular_buffer_t *this, void *buff, uint32_t len)
{
    assert(this || buff);
    uint32_t l;
    len = min(len, this->size - this->in + this->out);
    l = min(len, this->size - (this->in & (this->size - 1)));
    memcpy(this->buff + (this->in & (this->size - 1)), buff, l);
    memcpy(this->buff, buff + l, len - l);
    this->in += len;
    return len;
}

static uint32_t _buffer_get(circular_buffer_t *this, void *buff, uint32_t len)
{
    assert(this || buff);
    uint32_t l = 0;
    len = min(len, this->in - this->out);
    l = min(len, this->size - (this->out & (this->size - 1)));
    memcpy(buff, this->buff + (this->out & (this->size - 1)), l);
    memcpy(buff + l, this->buff, len - l);
    this->out += len;
    return len;
}

uint32_t circular_buffer_len(circular_buffer_t *this)
{
    uint32_t len = 0;
    if (this->mutex)
        ebs_mutex_lock(this->mutex);
    len = _buffer_len(this);
    if (this->mutex)
        ebs_mutex_unlock(this->mutex);
    return len;
}

uint32_t circular_buffer_put(circular_buffer_t *this, void *buff, uint32_t len)
{
    uint32_t ret;
    if (this->mutex)
        ebs_mutex_lock(this->mutex);
    ret = _buffer_put(this, buff, len);
    if (this->mutex)
        ebs_mutex_unlock(this->mutex);
    return ret;
}

uint32_t circular_buffer_get(circular_buffer_t *this, void *buff, uint32_t len)
{
    uint32_t ret;
    if (this->mutex)
        ebs_mutex_lock(this->mutex);
    ret = _buffer_get(this, buff, len);
    if (this->in == this->out)
        this->in = this->out = 0;
    if (this->mutex)
        ebs_mutex_unlock(this->mutex);
    return ret;
}
