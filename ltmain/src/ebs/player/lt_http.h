/**
 * @file lt_http.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-09-03
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _LT_HTTP_
#define _LT_HTTP_
#include "ebs.h"
#include "lt_ebs.h"
#include "ebm_task_type.h"
#include "util_msg.h"
#include "util_event.h"
#include "util_mutex.h"

#include "ql_http_client.h"
#include "ql_fs.h"
#include "ql_audio.h"
#include "ql_api_common.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

int lt_http_play_start(char *url);
int lt_http_play_start_ex(char *url, void (*callback)(void *));
int lt_http_play_start_ebm_task(ebm_task_t *task);
int lt_http_status_query(void);
int lt_http_play_stop(void);
int lt_http_play_stop_direct(void);

#endif
