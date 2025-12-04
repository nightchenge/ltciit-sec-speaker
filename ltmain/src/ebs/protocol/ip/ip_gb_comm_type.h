/**
 * @file ip_gb_comm_type.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-12-08
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _IP_GB_COMMM_TYPE_H_
#define _IP_GB_COMMM_TYPE_H_
#include "ebs.h"
#include "ip_gb_config.h"
#include "util_msg.h"
#include "util_sem.h"
#include "util_circ.h"
#include <stdbool.h>

#define IP_GB_MAX_RETRY_TIMES 6
#define IP_GB_MAX_RETRY_INTERVAL 64

enum comm_status
{
    COMM_STATUS_DISCONNECT = 0,
    COMM_STATUS_CONNECT = 1,
};

typedef enum trans_type
{
    TRANS_REG = 0,
    TRANS_LBK = !TRANS_REG
} trans_type_t;

typedef struct ip_gb_trans
{
    int fd;
    void (*reset)(struct ip_gb_trans *trans);
    int (*tick)(struct ip_gb_trans *trans, uint32_t ticks);
    int (*ssend)(struct ip_gb_trans *trans, uint8_t session[], bool is_sign, ip_gb_prot_msg_t *msg);
    comm_result_t (*prot_parse)(struct ip_gb_trans *trans, uint8_t *data, int length);
    comm_result_t (*cmd_parse)(struct ip_gb_trans *trans);
    comm_result_t (*resp_parse)(struct ip_gb_trans *trans);
    comm_result_t (*response)(struct ip_gb_trans *trans);
#if PLATFORM == PLATFORM_LINUX
    mqueue_t *queue_send;
    mqueue_t *queue_resp;
#elif PLATFORM == PLATFORM_Ex800G
    ebs_queue_t queue_send;
    ebs_queue_t queue_resp;
    ebs_sem_t sem_sync;       // 心跳线程和协议数据接收解析线程之间的同步信号量
    ebs_sem_t sem_wakeup;     // 超时后重新唤醒连接
    ebs_sem_t sem_heart_beat; // 心跳信号量
#endif
    uint8_t phase;
    time_t time_ticks;
    int length;
    int len;
    int len_tmp;
    ip_gb_prot_head_t dst_msg_head;
    ip_gb_prot_msg_t dst_msg_packet;
    ip_gb_sign_packet_t sign_packet;
    uint32_t session;
    ip_gb_prot_msg_t src_msg_packet;
    uint8_t *data;
    uint8_t *resp;
} ip_gb_trans_t;

typedef struct comm_module
{
    ip_gb_trans_t *trans;
    uint32_t period;
    uint8_t index;
    uint8_t chn_mode;
    uint8_t status; //! 用于标识和平台连接状态 0:未连接 1:已连接
    uint32_t retry_interval;
    uint32_t retry_times;
    uint32_t ip[2];
    uint16_t port[2];
    circular_buffer_t *circ_buff;
} comm_module_t;

#define DECLARE_TRANS(name, type) ip_gb_trans_t trans_##name = {                        \
                                      .fd = -1,                                         \
                                      .reset = recv_reset,                              \
                                      .tick = time_tick,                                \
                                      .ssend = !type ? reg_send : lbk_send,             \
                                      .prot_parse = !type ? prot_parse : NULL,          \
                                      .cmd_parse = !type ? cmd_parse : NULL,            \
                                      .resp_parse = !type ? resp_parse : NULL,          \
                                      .response = !type ? ip_gb_common_response : NULL, \
                                      .queue_send = NULL,                               \
                                      .queue_resp = NULL,                               \
                                      .time_ticks = 0,                                  \
                                      .session = 0,                                     \
}

#define DECLARE_COMM(name, type)                    \
    DECLARE_TRANS(name, type);                      \
    DECLARE_CIRCULAR_BUFFER(name, 10 * 1024, NULL); \
    comm_module_t ebs_comm_##name = {               \
        .trans = &trans_##name,                     \
        .period = 20000,                            \
        .index = 0,                                 \
        .chn_mode = 0x03,                           \
        .status = 0,                                \
        .retry_interval = 1,                        \
        .retry_times = 0,                           \
        .ip = {0},                                  \
        .port = {0},                                \
        .circ_buff = &circ_buffer_##name,           \
    }

#endif