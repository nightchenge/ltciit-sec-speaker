#ifndef __UTIL_TYPES_H__
#define __UTIL_TYPES_H__
#include <stdint.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef PARAM_IN
#define PARAM_IN
#endif

#ifndef PARAM_OUT
#define PARAM_OUT
#endif

#ifndef PARAM_INOUT
#define PARAM_INOUT
#endif

#define MAC_ADDR_LEN (6)

#if defined(__GNUC__)
#define PACK_START
#define PACK_END __attribute__((__packed__))
#elif defined(__CC_ARM)
#define PACK_START __packed
#define PACK_END
#else
#error not support compiler
#endif

typedef uint8_t bool_t;
typedef uint32_t port_bmp_t;
typedef uint32_t ipv4_addr_t;
typedef uint16_t ipv6_addr_t[8];
typedef uint8_t mac_addr_t[MAC_ADDR_LEN];

typedef int32_t (*util_print_t)(void *ctx, const char *fmt, ...);

#endif /*__OLT_TYPES_H__  */
