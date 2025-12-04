/**
 * @file lt_rtsp_play.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-09-22
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef __LT_RTSP_PLAY_H__
#define __LT_RTSP_PLAY_H__

#include "ebs.h"
#include "lt_ebs.h"
#include "ebm_task_type.h"
#include "util_msg.h"
#include "util_event.h"
#include "lt_rtsp.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

typedef enum
{
    LT_RTSP_STOP = 0,
    LT_RTSP_PAUSE,
} lt_rtsp_play_cmd_t;

typedef enum
{
    LT_RTSP_STATE_CONNECTION = 0,
    LT_RTSP_STATE_OPTIONS,
    LT_RTSP_STATE_DESCRIBE,
    LT_RTSP_STATE_SETUP,
    LT_RTSP_STATE_PLAY,
    LT_RTSP_STATE_TEARDOWN,
} lt_rtsp_state_t;

int lt_rtsp_play_start(const char *url);
int lt_rtsp_play_start_ex(const char *url, void (*callback)(void *));
int lt_rtsp_play_stop(void);
int lt_rtsp_play_stop_direct(void);
bool lt_rtsp_play_status_query(void);
#endif // __LT_RTSP_PLAY_H__