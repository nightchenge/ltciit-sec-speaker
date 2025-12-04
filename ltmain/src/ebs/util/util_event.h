/**
 * @file util_event.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-09-03
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _UTIL_EVENT_H_
#define _UTIL_EVENT_H_
#include "ebs.h"

#if PLATFORM == PLATFORM_LINUX

#elif PLATFORM == PLATFORM_Ex800G
#include "ql_api_osi.h"

typedef ql_event_t ebs_event_t;

#endif
#endif // _UTIL_EVENT_H_