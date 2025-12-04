/**
 * @file ip_gb_comm.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-11-22
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#include "ebs.h"
#include "ip_gb_comm.h"
#include "ip_gb_prot.h"
#include "ebs_param.h"
#if PLATFORM == PLATFORM_LINUX
#include "ebs_log.h"
#include "util_time.h"
#include "hwdrv.h"
#include "util_os_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <ifaddrs.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#elif PLATFORM_Ex800G
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "sockets.h"
#include "lwip/ip_addr.h"
#include "lwip/ip6_addr.h"

#include "lwip/netdb.h"
#include "lwip/netif.h"
#include "lwip/inet.h"
#include "lwip/tcp.h"
#include "util_mutex.h"
#include "util_timer.h"
#include "ql_api_osi.h"
#endif

#define DEBUG_LINE ebs_log("===func:%s line:%d===\n", __func__, __LINE__)

#define EBS_MALLOC(x, size)                              \
    do                                                   \
    {                                                    \
        if ((x = calloc(size, sizeof(uint8_t))) == NULL) \
            ebs_log("malloc fatal error!!!\n");          \
    } while (0)

#define EBS_FREE(x)  \
    do               \
    {                \
        if (x)       \
            free(x); \
    } while (0)

// pthread_mutex_t mutex_comm = PTHREAD_MUTEX_INITIALIZER;
ebs_mutex_t mutex_comm;
DECLARE_COMM(reg, TRANS_REG);
DECLARE_COMM(lbk, TRANS_LBK);
static ebs_heart_beat_t heart_beat = {
    .work_status = 0x01,
    .first_reg = 0x01,
    .phy_addr_len = PHY_ADDR_LEN,
    .phy_addr = {0},
#ifdef PROT_GUANGXI
    .ext_type = 0x02,
    .ext_len = 0,
#endif
};

static char net_name[][5] = {
    "eth0",
    "ppp0",
};

static int socket_unblock_init(uint32_t ip, uint16_t port, const char *inf_name)
{
    if (ip == 0 || port == 0 || inf_name == NULL || strlen(inf_name) > 15)
        return -1;
    int fd = -1;
    int flags;
    int ret;
    int error = 0;
    socklen_t length = sizeof(error);
    struct sockaddr_in server_addr;
    // struct ifreq ifr;

    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        // ebs_log(EBS_LOG_WARNING, "socket create failed.\n");
        ebs_log("socket create failed.\n");
        return -1;
    }

    if ((flags = fcntl(fd, F_GETFL, 0)) < 0)
    {
        // ebs_log(EBS_LOG_WARNING, "get socket fd attribute failed.\n");
        ebs_log("get socket fd attribute failed.\n");
        close(fd);
        return -1;
    }

#if PLATFORM == PLATFORM_LINUX
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        // ebs_log(EBS_LOG_WARNING, "set socket fd attribute failed.\n");
        ebs_log("set socket fd attribute failed.\n");
        close(fd);
        return -1;
    }

    strcpy(ifr.ifr_name, inf_name);
    if (setsockopt(fd, SOL_SOCKET, SO_BINDTODEVICE, (char *)&ifr, sizeof(ifr)) < 0)
    {
        // ebs_log(EBS_LOG_WARNING, "socket bind device failed.\n");
        ebs_log("socket bind device failed.\n");
        close(fd);
        return -1;
    }
#endif

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = HTONS(port);
    server_addr.sin_addr.s_addr = HTONL(ip);
    ret = connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret == 0)
    {
        fcntl(fd, F_SETFL, flags);
        return fd;
    }
    else if (errno != EINPROGRESS)
    {
        // ebs_log(EBS_LOG_WARNING, "socket connect error != EINPROGRESS\n");
        ebs_log("socket connect errno:%d, info:%s \n", errno, strerror(errno));
        close(fd);
        return -1;
    }

    fd_set readfds;
    fd_set writefds;
    struct timeval timeout;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_SET(fd, &readfds);
    FD_SET(fd, &writefds);
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    ret = select(fd + 1, &readfds, &writefds, NULL, &timeout);
    if (ret <= 0)
    {
        // ebs_log(EBS_LOG_WARNING, "socket connect timeout!\n");
        ebs_log("socket connect timeout!\n");
        close(fd);
        return -1;
    }

    if (FD_ISSET(fd, &writefds))
    {
        if (FD_ISSET(fd, &readfds))
        {
            if (connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
            {
                if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0)
                {
                    // ebs_log(EBS_LOG_WARNING, "get socket option failed!\n");
                    ebs_log("get socket option failed!\n");
                    close(fd);
                    return -1;
                }
                if (error != EISCONN)
                {
                    // ebs_log(EBS_LOG_WARNING, "connect error != EISCONN\n");
                    ebs_log("connect error != EISCONN\n");
                    close(fd);
                    return -1;
                }
            }
        }
        fcntl(fd, F_SETFL, flags);
        return fd;
    }
    else
    {
        // ebs_log(EBS_LOG_WARNING, "socket connect failed!\n");
        ebs_log("socket connect failed!\n");
        close(fd);
        return -1;
    }

    return -1;
}

static int ebs_tcp_init(comm_module_t *this)
{
    int fd = -1;
    uint8_t i = 0;
    char *inf_name = NULL;
    if (this->chn_mode == 0x03)
    {
        for (i = 1; i < 3; i++)
        {
            if (i == this->index && this->trans->fd > 0)
                continue;
            inf_name = net_name[i - 1];
            if ((fd = socket_unblock_init(this->ip[i - 1], this->port[i - 1], inf_name)) < 0)
            {
                continue;
            }
            else
            {
                if (this->trans->fd > 0)
                    close(this->trans->fd);
                this->trans->fd = fd;
                this->index = i;
                return 0;
            }
        }
        if (this->trans->fd < 0)
            this->index = 0;
    }
    else
    {
        this->chn_mode &= 0x03;
        inf_name = net_name[this->chn_mode - 1];
        if ((fd = socket_unblock_init(this->ip[this->chn_mode - 1], this->port[this->chn_mode - 1], inf_name)) > 0)
        {
            this->trans->fd = fd;
            this->index = this->chn_mode;
            return 0;
        }
        this->trans->fd = -1;
        this->index = 0;
    }
    return -1;
}

bool ebs_reg_msg_send(reg_msg_t *msg)
{
    ip_gb_trans_t *this = ebs_comm_reg.trans;
#if PLATFORM == PLATFORM_LINUX
    mqueue_msg_t *m = mqueue_msg_alloc(msg->type, msg->len);
    uint8_t *data = mqueue_msg_body(m);
    memcpy(data, msg->data, msg->len);

    if (mqueue_timed_put(this->queue_send, m, 200) != 0)
    {
        ebs_log("IP cmd queue send error!\n");
        return false;
    }
    return true;
#elif PLATFORM == PLATFORM_Ex800G
    if (this->fd < 0)
    {
        ebs_log("IP cmd queue send error!\n");
        return false;
    }
    ebs_msg_t m = {0};
    m.type = msg->type;
    m.size = msg->len;
    m.data = ebs_msg_malloc(msg->len * sizeof(uint8_t));
    memcpy(m.data, msg->data, msg->len);
    if (ebs_msg_queue_send(this->queue_send, (uint8_t *)&m, sizeof(ebs_msg_t), 200) != 0)
    {
        ebs_log("IP cmd queue send error!\n");
        ebs_msg_free(m.data);
        return false;
    }
    return true;
#endif
}

bool ebs_lbk_msg_send(lbk_msg_t *msg)
{
    ip_gb_trans_t *this = ebs_comm_lbk.trans;
#if PLATFORM == PLATFORM_LINUX
    mqueue_msg_t *m = mqueue_msg_alloc(msg->type, msg->len);
    uint8_t *data = mqueue_msg_body(m);
    memcpy(data, msg->data, msg->len);
    if (mqueue_timed_put(this->queue_send, m, 200) != 0)
    {
        ebs_log("lbk msg queue send error!\n");
        return false;
    }
    return true;
#elif PLATFORM == PLATFORM_Ex800G
    ebs_msg_t m = {0};
    m.type = msg->type;
    m.size = msg->len;
    m.data = ebs_msg_malloc(msg->len * sizeof(uint8_t));
    memcpy(m.data, msg->data, msg->len);
    if (ebs_msg_queue_send(this->queue_send, (uint8_t *)&m, sizeof(ebs_msg_t), 200) != 0)
    {
        ebs_log("lbk msg queue send error!\n");
        ebs_msg_free(m.data);
        return false;
    }
    return true;
#endif
}

static ql_timer_t lt_ebs_comm_timer = NULL;
static inline int ebs_reg_heart_beat_timer_create(void (*callback)(void *))
{
    if (lt_ebs_comm_timer)
    {
        ql_rtos_timer_delete(lt_ebs_comm_timer);
        lt_ebs_comm_timer = NULL;
    }
    if (ql_rtos_timer_create(&lt_ebs_comm_timer, QL_TIMER_IN_SERVICE, callback, &ebs_comm_reg) != QL_OSI_SUCCESS)
    {
        ebs_log("ebs_comm timer create failed\n");
        return -1;
    }
    return 0;
}

static inline void ebs_reg_heart_beat_timer_start(uint32_t period)
{
    if (ql_rtos_timer_start(lt_ebs_comm_timer, period, 1) != QL_OSI_SUCCESS)
    {
        ebs_log("ebs_comm timer start failed\n");
    }
}

static inline void ebs_reg_heart_beat_timer_stop(void)
{
    if (ql_rtos_timer_stop(lt_ebs_comm_timer) != QL_OSI_SUCCESS)
    {
        ebs_log("ebs_comm timer stop failed\n");
    }
}

static inline void ebs_reg_heart_beat_timer_delete(void)
{
    if (lt_ebs_comm_timer)
    {
        ql_rtos_timer_delete(lt_ebs_comm_timer);
        lt_ebs_comm_timer = NULL;
    }
}

static comm_result_t comm_get_data_callback(ip_gb_trans_t *this, uint8_t *data, int len)
{
    comm_result_t result = COMM_HAS_DATA;
    if (this->prot_parse)
    {
        result = this->prot_parse(this, data, len);
        if (result != COMM_RESULT_OK && result != COMM_HAS_DATA)
        {
            this->resp[0] = result;
            this->resp[1] = 0;
            this->resp[2] = 0;
            if (this->response)
                this->response(this);
            this->phase = PHASE_IDLE;
            this->length = 0;
            return result;
        }
        else if (result == COMM_HAS_DATA)
            return COMM_HAS_DATA;
    }
    if (this->dst_msg_head.pack_type == 0x02)
    {
        if (this->resp_parse)
            result = this->resp_parse(this);
    }
    else
    {
        if (this->cmd_parse)
            result = this->cmd_parse(this);
    }

    this->phase = PHASE_IDLE;
    this->length = 0;
    return result;
}

static void ebs_reg_comm_task(void *arg)
{
    comm_module_t *this = &ebs_comm_reg;
    int recv_len = 0;
    uint8_t recv_buf[1500] = {0};
    uint8_t buff[1] = {0};
    comm_result_t result = COMM_RESULT_NULL;
    // uint8_t connect_out = 0;
#if PLATFORM == PLATFORM_LINUX
    os_sem_post((os_sem_t *)arg);
    mqueue_msg_t *msg;
#elif PLATFORM == PLATFORM_Ex800G
    this->chn_mode = 0x02;
#endif

    while (1)
    {
#if PLATFORM == PLATFORM_LINUX
        if (ebs_tcp_init(this) < 0)
        {
            this->trans->fd = -1;
            ebs_sleep(3);
            continue;
        }
#elif PLATFORM == PLATFORM_Ex800G
        ebs_sem_wait(&this->trans->sem_sync, EBS_WAIT_FOREVER); //! 等待心跳线程唤醒
#endif
        ebs_log("Network interface:%s link success fd:%d, start communicate\n", &net_name[this->index - 1][0], this->trans->fd);
        while (1)
        {
            if (this->trans->fd > 0)
            {
                do
                {
                    recv_len = recv(this->trans->fd, recv_buf, sizeof(recv_buf) - 1, MSG_DONTWAIT);
                    if (recv_len > 0)
                        this->circ_buff->put(this->circ_buff, recv_buf, recv_len);

                } while (recv_len > 0);
                do
                {
                    recv_len = this->circ_buff->get(this->circ_buff, buff, 1);
                    if (recv_len > 0)
                        result = comm_get_data_callback(this->trans, buff, recv_len);
                } while (recv_len > 0);
                if (result == COMM_RESULT_OK)
                {
                    ebs_log("recv cmd success\n");
                    result = COMM_RESULT_NULL;
                }
                else if (result == COMM_HAS_DATA)
                {
                    continue;
                }
            }
            else
            {
                break;
            }
            ebs_sleep(1);
        }
    }
}

static void ebs_lbk_comm_task(void *arg)
{
    comm_module_t *this = &ebs_comm_lbk;
#if PLATFORM == PLATFORM_LINUX
    os_sem_post((os_sem_t *)arg);
    mqueue_msg_t *msg;
#elif PLATFORM == PLATFORM_Ex800G
    ebs_msg_t m = {0}, *msg = NULL;
    msg = &m;
    this->chn_mode = 0x02;
#endif
    uint8_t msg_type = 0;
    uint8_t *data = NULL;

    while (1)
    {
#if PLATFORM == PLATFORM_LINUX
        if (ebs_tcp_init(this) < 0)
        {
            ebs_sleep(3);
            continue;
        }
#endif

#if PLATFORM == PLATFORM_LINUX
        if ((msg = mqueue_timed_get(this->trans->queue_send, 200)) != NULL)
#elif PLATFORM == PLATFORM_Ex800G
        if (ebs_msg_queue_wait(this->trans->queue_send, (uint8_t *)msg, sizeof(ebs_msg_t), EBS_WAIT_FOREVER) == 0)
#endif
        {
#if PLATFORM == PLATFORM_LINUX
            msg_type = (uint8_t)mqueue_msg_type_get(msg);
            data = (uint8_t *)mqueue_msg_body(msg);
#elif PLATFORM == PLATFORM_Ex800G
            msg_type = (uint8_t)msg->type;
            data = (uint8_t *)msg->data;
            while (this->retry_times < IP_GB_MAX_RETRY_TIMES)
            {
                if (ebs_tcp_init(this) < 0)
                {
                    this->trans->fd = -1;
                    ebs_sleep(this->retry_interval);
                    this->retry_times++;
                    this->retry_interval = (this->retry_interval * 2) > IP_GB_MAX_RETRY_INTERVAL ? IP_GB_MAX_RETRY_INTERVAL : (this->retry_interval * 2);
                    if (this->retry_times >= IP_GB_MAX_RETRY_TIMES)
                    {
                        ebs_msg_free(msg->data);
                        continue;
                    }
                }
                else
                {
                    this->retry_times = 0;
                    this->retry_interval = 1;
                    break;
                }
            }
#endif
            switch (msg_type)
            {
            case LBK_HEART_BEAT:
                ebs_log("loopback send heart beat \n");
                this->trans->src_msg_packet.cmd_type = 0x10;
                break;
            case LBK_STATUS_QUERY:
                ebs_log("param query report\n");
                this->trans->src_msg_packet.cmd_type = 0x11;
                break;
            case LBK_DEVICE_MALFUNC:
                ebs_log("device malfunction report\n");
                this->trans->src_msg_packet.cmd_type = 0x13;
                break;
            case LBK_TASK_SWITCH:
                ebs_log("play task switch report\n");
                this->trans->src_msg_packet.cmd_type = 0x14;
                break;
            case LBK_PLAY_RESULT:
                ebs_log("play result report\n");
                this->trans->src_msg_packet.cmd_type = 0x15;
                break;
            default:
                ebs_log("loopback msg type error\n");
                ebs_sleep_ms(200);
                continue;
                break;
            }
            memcpy(this->trans->src_msg_packet.resources_code, ebs_param.resources_code, RESOURCES_CODE_LEN);
            this->trans->src_msg_packet.dst_obj_cnt = 1;

#if PLATFORM == PLATFORM_LINUX
            this->trans->src_msg_packet.cmd_len = mqueue_msg_size_get(msg);
            memcpy(this->trans->src_msg_packet.cmd_data, data, this->trans->src_msg_packet.cmd_len);
            ebs_msg_free(msg);
#elif PLATFORM == PLATFORM_Ex800G
            this->trans->src_msg_packet.cmd_len = msg->size;
            memcpy(this->trans->src_msg_packet.cmd_data, data, msg->size);
            ebs_msg_free(msg->data);
#endif
            this->trans->ssend(this->trans, NULL, 0, &this->trans->src_msg_packet);
            if (this->trans->fd > 0)
            {
                close(this->trans->fd);
                this->trans->fd = -1;
            }
        }
    }
}
#if 0
static void ebs_heart_beat_task(void *arg)
{
#if PLATFORM == PLATFORM_LINUX
    os_sem_post((os_sem_t *)arg);
    mqueue_msg_t *msg;
#elif PLATFORM == PLATFORM_Ex800G
    ebs_msg_t m = {0}, *msg = NULL;
    msg = &m;
#endif
    comm_module_t *this = &ebs_comm_reg;
    int i;
    time_t timer_heart = 0;
    time_t timer_lbk_heart = 0;
    reg_msg_t heart_msg;
    lbk_msg_t lbk_msg;
    heart_msg.type = 0x10;
    heart_msg.len = sizeof(heart_beat);
    heart_beat.work_status = 0x01;
    heart_beat.first_reg = 0x01;
    memcpy(heart_msg.data, &heart_beat, sizeof(heart_beat));

    while (1)
    {
        if (this->trans->fd < 0)
        {
#if PLATFORM == PLATFORM_LINUX
            ebs_sleep(1);
#elif PLATFORM == PLATFORM_Ex800G
            while (this->retry_times < IP_GB_MAX_RETRY_TIMES)
            {
                if (ebs_tcp_init(this) < 0)
                {
                    this->trans->fd = -1;
                    ebs_sleep(this->retry_interval);
                    this->retry_times++;
                    this->retry_interval = (this->retry_interval * 2) > IP_GB_MAX_RETRY_INTERVAL ? IP_GB_MAX_RETRY_INTERVAL : (this->retry_interval * 2);
                    ebs_log("Network interface:%s link failed, retry times:%d retry interval:%d \n", &net_name[this->index - 1][0], this->retry_times, this->retry_interval);
                    if (this->retry_times >= IP_GB_MAX_RETRY_TIMES)
                    {
                        ebs_log("Network interface:%s link failed, retry times:%d retry interval:%d \n", &net_name[this->index - 1][0], this->retry_times, this->retry_interval);
                        ebs_sem_wait(this->trans->sem_wakeup, EBS_WAIT_FOREVER);
                        this->retry_times = 0;
                        this->retry_interval = 1;
                    }
                }
                else
                {
                    this->retry_times = 0;
                    this->retry_interval = 1;
                    break;
                }
            }
            ebs_sem_post(&this->trans->sem_sync);
#endif
        }
        if (timer_expired(&timer_heart, this->period))
        {
            timer_start(&timer_heart);
            ebs_log("reg ip:%08X port:%d \n", this->ip[this->index - 1], this->port[this->index - 1]);
            ebs_log("send heart beat in fd:%d \n", this->trans->fd);
            heart_msg.type = 0x10;
            heart_msg.len = sizeof(heart_beat);
            memcpy(heart_msg.data, &heart_beat, sizeof(ebs_heart_beat_t));
#if PLATFORM == PLATFORM_LINUX
            if (!ebs_reg_msg_send(&heart_msg))
            {
                ebs_log("heart beat msg send failed.\n");
                continue;
            }
#elif PLATFORM == PLATFORM_Ex800G
            memcpy(this->trans->src_msg_packet.resources_code, ebs_param.resources_code, RESOURCES_CODE_LEN);
            this->trans->src_msg_packet.dst_obj_cnt = 1;
            this->trans->src_msg_packet.dst_obj_list = resources_code_remote;
            this->trans->src_msg_packet.cmd_type = heart_msg.type;
            this->trans->src_msg_packet.cmd_len = heart_msg.len;
            memcpy(this->trans->src_msg_packet.cmd_data, heart_msg.data, heart_msg.len);
            this->trans->ssend(this->trans, NULL, false, &this->trans->src_msg_packet);
#endif
            for (i = 0; i < 10; i++)
            {
#if PLATFORM == PLATFORM_LINUX
                if ((msg = mqueue_timed_get(this->trans->queue_resp, 1000)) != NULL)
                {
                    break;
                }
#elif PLATFORM == PLATFORM_Ex800G
                if (ebs_msg_queue_wait(this->trans->queue_resp, (uint8_t *)msg, sizeof(ebs_msg_t), 1000) == 0)
                {
                    break;
                }
#endif
                ebs_log("heart beat recv error!\n");
            }
            if (i >= 10)
            {
                ebs_log("heart beat response timeout!\n");
                heart_beat.first_reg = 0x01;
                comm_reg_reconnection();
#if PLATFORM == PLATFORM_LINUX
                hwdrv_led_status_set(LED_IP_CHANNEL, RED_LIGHT);
#elif PLATFORM == PLATFORM_Ex800G
                this->status = COMM_STATUS_DISCONNECT;
                ebs_msg_free(msg->data);
#endif
            }
            else
            {
#if PLATFORM == PLATFORM_LINUX
                uint8_t msg_type = (uint8_t)mqueue_msg_type_get(msg);
                uint8_t *data = (uint8_t *)mqueue_msg_body(msg);
#elif PLATFORM == PLATFORM_Ex800G
                uint8_t msg_type = (uint8_t)msg->type;
                uint8_t *data = (uint8_t *)msg->data;
#endif
                if (msg_type != 0x10)
                {
                    ebs_log("heart beat response cmd type error:%02X \n", msg_type);
                }
                else if (data[0] != 0)
                {
                    ebs_log("heart beat response error!\n");
                    if (ntohs(*(uint16_t *)(data + 1)) > 0)
                    {
                        ebs_log("haert beat error info:%s\n", (char *)(data + 3));
                    }
                }
                else
                {
                    ebs_log("heart beat recv success!\n");
                    if (heart_beat.first_reg == 0x01)
                    {
                        heart_beat.first_reg = 0x02;
#ifdef PROT_GUAGNXI
                        if (((data[1] << 8) | data[2]) == 4)
                        {
                            // TODO 设置系统时间
                        }
#endif
                    }
#if PLATFORM == PLATFORM_LINUX
                    hwdrv_led_status_set(LED_IP_CHANNEL, GREEN_LIGHT);
#elif PLATFORM == PLATFORM_Ex800G
                    this->status = COMM_STATUS_CONNECT;
#endif
                }
#if PLATFORM == PLATFORM_LINUX
                ebs_msg_free(msg);
#elif PLATFORM == PLATFORM_Ex800G
                ebs_msg_free(msg->data);
#endif
            }
        }
        if (this->status == COMM_STATUS_CONNECT)
        {
            this = &ebs_comm_lbk;
            if (timer_expired(&timer_lbk_heart, this->period))
            {
                timer_start(&timer_lbk_heart);
                lbk_msg.type = LBK_HEART_BEAT;
                lbk_msg.len = sizeof(heart_beat);
                memcpy(lbk_msg.data, &heart_beat, sizeof(heart_beat));
                ebs_lbk_msg_send(&lbk_msg);
            }
            this = &ebs_comm_reg;
        }
        if (!this->index)
        {
            if (this->trans->fd > 0)
            {
                close(this->trans->fd);
                this->trans->fd = -1;
            }
        }
        ebs_sleep(1);
    }
}
#else
static void ebs_comm_callback(void *arg)
{
    ebs_log("ebs_comm_callback\n");
    comm_module_t *this = (comm_module_t *)arg;
    ebs_sem_post(&this->trans->sem_heart_beat);
}

static void ebs_heart_beat_task(void *arg)
{
#if PLATFORM == PLATFORM_LINUX
    os_sem_post((os_sem_t *)arg);
    mqueue_msg_t *msg;
#elif PLATFORM == PLATFORM_Ex800G
    ebs_msg_t m = {0}, *msg = NULL;
    msg = &m;
#endif
    comm_module_t *this = &ebs_comm_reg;
    int i;
    reg_msg_t heart_msg;
    lbk_msg_t lbk_msg;
    heart_msg.type = 0x10;
    heart_msg.len = sizeof(heart_beat);
    heart_beat.work_status = 0x01;
    heart_beat.first_reg = 0x01;
    memcpy(heart_msg.data, &heart_beat, sizeof(heart_beat));
    while (1)
    {
        // ql_event_wait(&event, EBS_WAIT_FOREVER);
        ebs_sem_wait(this->trans->sem_heart_beat, EBS_WAIT_FOREVER);

        if (!this->index)
        {
            if (this->trans->fd > 0)
            {
                close(this->trans->fd);
                this->trans->fd = -1;
            }
        }

        if (this->trans->fd < 0)
        {
#if PLATFORM == PLATFORM_LINUX
            ebs_sleep(1);
#elif PLATFORM == PLATFORM_Ex800G
            while (this->retry_times < IP_GB_MAX_RETRY_TIMES)
            {
                if (ebs_tcp_init(this) < 0)
                {
                    this->trans->fd = -1;
                    ebs_sleep(this->retry_interval);
                    this->retry_times++;
                    this->retry_interval = (this->retry_interval * 2) > IP_GB_MAX_RETRY_INTERVAL ? IP_GB_MAX_RETRY_INTERVAL : (this->retry_interval * 2);
                    ebs_log("Network interface:%s link failed, retry times:%d retry interval:%d \n", this->chn_mode == 0x03 ? "eth0&ppp0" : net_name[this->chn_mode - 1], this->retry_times, this->retry_interval);
                    if (this->retry_times >= IP_GB_MAX_RETRY_TIMES)
                    {
                        ebs_log("Network interface:%s link failed, retry times:%d retry interval:%d \n", this->chn_mode == 0x03 ? "eth0&ppp0" : net_name[this->chn_mode - 1], this->retry_times, this->retry_interval);
                        ebs_reg_heart_beat_timer_stop(); // 停止定时器
                        ebs_sem_wait(this->trans->sem_wakeup, EBS_WAIT_FOREVER);
                        ebs_reg_heart_beat_timer_start(this->period); // 重新载入定时器
                        this->retry_times = 0;
                        this->retry_interval = 1;
                    }
                }
                else
                {
                    this->retry_times = 0;
                    this->retry_interval = 1;
                    break;
                }
            }
            ebs_sem_post(&this->trans->sem_sync);
#endif
        }
        ebs_log("reg ip:%08X port:%d \n", this->ip[this->index - 1], this->port[this->index - 1]);
        ebs_log("send heart beat in fd:%d \n", this->trans->fd);
        // lt_stack_size_print();
        heart_msg.type = 0x10;
        heart_msg.len = sizeof(heart_beat);
        memcpy(heart_msg.data, &heart_beat, sizeof(ebs_heart_beat_t));
#if PLATFORM == PLATFORM_LINUX
        if (!ebs_reg_msg_send(&heart_msg))
        {
            ebs_log("heart beat msg send failed.\n");
            continue;
        }
#elif PLATFORM == PLATFORM_Ex800G
        memcpy(this->trans->src_msg_packet.resources_code, ebs_param.resources_code, RESOURCES_CODE_LEN);
        this->trans->src_msg_packet.dst_obj_cnt = 1;
        this->trans->src_msg_packet.dst_obj_list = resources_code_remote;
        this->trans->src_msg_packet.cmd_type = heart_msg.type;
        this->trans->src_msg_packet.cmd_len = heart_msg.len;
        memcpy(this->trans->src_msg_packet.cmd_data, heart_msg.data, heart_msg.len);
        this->trans->ssend(this->trans, NULL, false, &this->trans->src_msg_packet);

#endif
        for (i = 0; i < 10; i++)
        {
#if PLATFORM == PLATFORM_LINUX
            if ((msg = mqueue_timed_get(this->trans->queue_resp, 1000)) != NULL)
            {
                break;
            }
#elif PLATFORM == PLATFORM_Ex800G
            if (ebs_msg_queue_wait(this->trans->queue_resp, (uint8_t *)msg, sizeof(ebs_msg_t), 2000) == 0)
            {
                break;
            }
#endif
            ebs_log("heart beat recv error!\n");
        }
        if (i >= 10)
        {
            ebs_log("heart beat response timeout!\n");
            heart_beat.first_reg = 0x01;
            comm_reg_reconnection();
#if PLATFORM == PLATFORM_LINUX
            hwdrv_led_status_set(LED_IP_CHANNEL, RED_LIGHT);
#elif PLATFORM == PLATFORM_Ex800G
            this->status = COMM_STATUS_DISCONNECT;
            ebs_msg_free(msg->data);
#endif
        }
        else
        {
#if PLATFORM == PLATFORM_LINUX
            uint8_t msg_type = (uint8_t)mqueue_msg_type_get(msg);
            uint8_t *data = (uint8_t *)mqueue_msg_body(msg);
#elif PLATFORM == PLATFORM_Ex800G
            uint8_t msg_type = (uint8_t)msg->type;
            uint8_t *data = (uint8_t *)msg->data;
#endif
            if (msg_type != 0x10)
            {
                ebs_log("heart beat response cmd type error:%02X \n", msg_type);
            }
            else if (data[0] != 0)
            {
                ebs_log("heart beat response error!\n");
                if (ntohs(*(uint16_t *)(data + 1)) > 0)
                {
                    ebs_log("haert beat error info:%s\n", (char *)(data + 3));
                }
            }
            else
            {
                ebs_log("heart beat recv success!\n");
                if (heart_beat.first_reg == 0x01)
                {
                    heart_beat.first_reg = 0x02;
#ifdef PROT_GUAGNXI
                    if (((data[1] << 8) | data[2]) == 4)
                    {
                        // TODO 设置系统时间
                    }
#endif
                }
#if PLATFORM == PLATFORM_LINUX
                hwdrv_led_status_set(LED_IP_CHANNEL, GREEN_LIGHT);
#elif PLATFORM == PLATFORM_Ex800G
                this->status = COMM_STATUS_CONNECT;
#endif
            }
#if PLATFORM == PLATFORM_LINUX
            ebs_msg_free(msg);
#elif PLATFORM == PLATFORM_Ex800G
            ebs_msg_free(msg->data);
#endif
        }
        if (this->status == COMM_STATUS_CONNECT)
        {
            this = &ebs_comm_lbk;
            lbk_msg.type = LBK_HEART_BEAT;
            lbk_msg.len = sizeof(heart_beat);
            memcpy(lbk_msg.data, &heart_beat, sizeof(heart_beat));
            ebs_lbk_msg_send(&lbk_msg);
            this = &ebs_comm_reg;
        }
    }
}
#endif
static int ebs_reg_communicate_init(uint32_t stage)
{
    memcpy(heart_beat.phy_addr, ebs_param.phy_addr, PHY_ADDR_LEN);

    //四川定制
    ebs_comm_reg.period = 20 * 1000;
    //ebs_comm_reg.period = ebs_param.heart_period * 1000;
    
    ebs_comm_reg.index = 0;
    ebs_comm_reg.chn_mode = 0x02;

    // ebs_comm_reg.ip[0] = 0xC0A863E9; //19216899233
    // ebs_comm_reg.port[0] = 8301;

    ebs_comm_reg.ip[0] = ebs_param.reg_ip[0]; // 19216899233
    ebs_comm_reg.port[0] = ebs_param.reg_port[0];

    ebs_comm_reg.ip[1] = ebs_param.reg_ip_4g[0]; // 19216899233
    ebs_comm_reg.port[1] = ebs_param.reg_port_4g[0];

    EBS_MALLOC(ebs_comm_reg.trans->data, 1024 * 10);
    EBS_MALLOC(ebs_comm_reg.trans->resp, 4096);
    EBS_MALLOC(ebs_comm_reg.trans->dst_msg_packet.dst_obj_list, 100 * RESOURCES_CODE_LEN);
    EBS_MALLOC(ebs_comm_reg.trans->dst_msg_packet.cmd_data, 4096);
    // EBS_MALLOC(ebs_comm_reg.trans->src_msg_packet.dst_obj_list, 4096);
    ebs_comm_reg.trans->src_msg_packet.dst_obj_list = resources_code_remote;
    EBS_MALLOC(ebs_comm_reg.trans->src_msg_packet.cmd_data, 4096);

    // ebs_comm_reg.trans->queue_resp = mqueue_new("reg_msg_resp", 1024);
    // ebs_comm_reg.trans->queue_send = mqueue_new("reg_msg_send", 1024);
    ebs_msg_queue_creat(&ebs_comm_reg.trans->queue_resp, sizeof(ebs_msg_t), 512);
    ebs_msg_queue_creat(&ebs_comm_reg.trans->queue_send, sizeof(ebs_msg_t), 512);
#if PLATFORM == PLATFORM_Ex800G
    ebs_sem_create(&ebs_comm_reg.trans->sem_sync, 0);
    ebs_sem_create(&ebs_comm_reg.trans->sem_wakeup, 0);
    ebs_sem_create(&ebs_comm_reg.trans->sem_heart_beat, 0);
#endif
    return 0;
}

static int ebs_reg_communicate_deinit(void)
{
    EBS_FREE(ebs_comm_reg.trans->data);
    EBS_FREE(ebs_comm_reg.trans->resp);
    EBS_FREE(ebs_comm_reg.trans->dst_msg_packet.dst_obj_list);
    EBS_FREE(ebs_comm_reg.trans->dst_msg_packet.cmd_data);
    EBS_FREE(ebs_comm_reg.trans->src_msg_packet.cmd_data);

    ebs_msg_queue_delete(&ebs_comm_reg.trans->queue_resp);
    ebs_msg_queue_delete(&ebs_comm_reg.trans->queue_send);
#if PLATFORM == PLATFORM_Ex800G
    ebs_sem_delete(&ebs_comm_reg.trans->sem_sync);
    ebs_sem_delete(&ebs_comm_reg.trans->sem_wakeup);
    ebs_sem_delete(&ebs_comm_reg.trans->sem_heart_beat);
#endif
    return 0;
}

static int ebs_lbk_communicate_init(uint32_t stage)
{
    ebs_comm_lbk.period = ebs_param.lbk_period * 1000;
    ebs_comm_lbk.chn_mode = 0x02;
    ebs_comm_lbk.index = 0;

    ebs_comm_lbk.ip[0] = ebs_param.lbk_ip[0];
    ebs_comm_lbk.port[0] = ebs_param.lbk_port[0];

    ebs_comm_lbk.ip[1] = ebs_param.lbk_ip_4g[0];
    ebs_comm_lbk.port[1] = ebs_param.lbk_port_4g[0];

    EBS_MALLOC(ebs_comm_lbk.trans->data, 1024 * 10);
    EBS_MALLOC(ebs_comm_lbk.trans->resp, 4096);
    EBS_MALLOC(ebs_comm_lbk.trans->dst_msg_packet.dst_obj_list, 8 * RESOURCES_CODE_LEN);
    EBS_MALLOC(ebs_comm_lbk.trans->dst_msg_packet.cmd_data, 4096);
    // EBS_MALLOC(ebs_comm_lbk.trans->src_msg_packet.dst_obj_list, 4096);
    ebs_comm_lbk.trans->src_msg_packet.dst_obj_list = resources_code_remote;
    EBS_MALLOC(ebs_comm_lbk.trans->src_msg_packet.cmd_data, 4096);

    // ebs_comm_lbk.trans->queue_resp = mqueue_new("lbk_msg_resp", 1024);
    // ebs_comm_lbk.trans->queue_send = mqueue_new("lbk_msg_send", 1024);
    ebs_msg_queue_creat(&ebs_comm_lbk.trans->queue_resp, sizeof(ebs_msg_t), 512);
    ebs_msg_queue_creat(&ebs_comm_lbk.trans->queue_send, sizeof(ebs_msg_t), 512);
    return 0;
}

static int ebs_lbk_communicate_deinit(void)
{
    EBS_FREE(ebs_comm_lbk.trans->data);
    EBS_FREE(ebs_comm_lbk.trans->resp);
    EBS_FREE(ebs_comm_lbk.trans->dst_msg_packet.dst_obj_list);
    EBS_FREE(ebs_comm_lbk.trans->dst_msg_packet.cmd_data);
    EBS_FREE(ebs_comm_lbk.trans->src_msg_packet.cmd_data);
    ebs_msg_queue_delete(&ebs_comm_lbk.trans->queue_resp);
    ebs_msg_queue_delete(&ebs_comm_lbk.trans->queue_send);
    return 0;
}

static ql_task_t lt_ebs_reg_task;
static ql_task_t lt_ebs_heart_beat_task;
static ql_task_t lt_ebs_lbk_task;

int ebs_comm_task_init(void)
{
    QlOSStatus err = QL_OSI_SUCCESS;

    ebs_reg_communicate_init(0);
    ebs_lbk_communicate_init(0);

    ebs_mutex_init(&mutex_comm);

    err = ql_rtos_task_create(&lt_ebs_reg_task, 1024 * 16, APP_PRIORITY_NORMAL, "ebs_comm", ebs_reg_comm_task, NULL, 0);
    if (err != QL_OSI_SUCCESS)
    {
        ebs_log("ebs_comm task create failed\n");
        return -1;
    }

    err = ql_rtos_task_create(&lt_ebs_lbk_task, 1024 * 16, APP_PRIORITY_NORMAL, "ebs_lbk", ebs_lbk_comm_task, NULL, 0);
    if (err != QL_OSI_SUCCESS)
    {
        ebs_log("ebs_lbk task create failed\n");
        return -1;
    }

    err = ql_rtos_task_create(&lt_ebs_heart_beat_task, 1024 * 16, APP_PRIORITY_NORMAL, "ebs_heart_beat", ebs_heart_beat_task, NULL, 0);
    if (err != QL_OSI_SUCCESS)
    {
        ebs_log("ebs_heart_beat task create failed\n");
        return -1;
    }

    ebs_reg_heart_beat_timer_create(ebs_comm_callback);
    ebs_reg_heart_beat_timer_start(ebs_comm_reg.period);

    return 0;
}

int ebs_comm_task_deinit(void)
{
    ebs_reg_communicate_deinit();
    ebs_lbk_communicate_deinit();
    ql_rtos_task_delete(lt_ebs_reg_task);
    lt_ebs_reg_task = NULL;
    ql_rtos_task_delete(lt_ebs_lbk_task);
    lt_ebs_lbk_task = NULL;
    ebs_reg_heart_beat_timer_delete();
    ql_rtos_task_delete(lt_ebs_heart_beat_task);
    lt_ebs_heart_beat_task = NULL;
    return 0;
}

uint8_t device_work_mode_status_get(void)
{
    uint8_t status = 0;
    ebs_mutex_lock(&mutex_comm);
    status = heart_beat.work_status;
    ebs_mutex_unlock(&mutex_comm);
    return status;
}

void device_work_mode_status_set(uint8_t status)
{
    ebs_mutex_lock(&mutex_comm);
    heart_beat.work_status = status & 0x03;
    ebs_mutex_unlock(&mutex_comm);
}

uint8_t ebs_comm_reg_status_get(void)
{
    uint8_t status = 0;
    ebs_mutex_lock(&mutex_comm);
    status = ebs_comm_reg.status;
    ebs_mutex_unlock(&mutex_comm);
    return status;
}

uint32_t comm_lbk_period_get(void)
{
    uint32_t period = 0;
    ebs_mutex_lock(&mutex_comm);
    period = ebs_comm_lbk.period;
    ebs_mutex_unlock(&mutex_comm);
    return period;
}

void comm_lbk_period_set(uint32_t period)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_lbk.period = period;
    ebs_mutex_unlock(&mutex_comm);
}

int comm_lbk_channel_get(void)
{
    int chn = 0;
    ebs_mutex_lock(&mutex_comm);
    chn = ebs_comm_lbk.index;
    ebs_mutex_unlock(&mutex_comm);
    return chn;
}

void comm_lbk_channel_mode_set(uint8_t mode)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_lbk.chn_mode = mode;
    ebs_mutex_unlock(&mutex_comm);
}

void comm_lbk_ip_addr_set(uint32_t ip)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_lbk.ip[0] = ip;
    ebs_mutex_unlock(&mutex_comm);
}

void comm_lbk_ip_port_set(uint16_t port)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_lbk.port[0] = port;
    ebs_mutex_unlock(&mutex_comm);
}

void comm_lbk_4g_addr_set(uint32_t ip)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_lbk.ip[1] = ip;
    ebs_mutex_unlock(&mutex_comm);
}

void comm_lbk_4g_port_set(uint16_t port)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_lbk.port[1] = port;
    ebs_mutex_unlock(&mutex_comm);
}

uint32_t comm_reg_period_get(void)
{
    uint32_t period = 0;
    ebs_mutex_lock(&mutex_comm);
    period = ebs_comm_reg.period;
    ebs_mutex_unlock(&mutex_comm);
    return period;
}

void comm_reg_period_set(uint32_t period)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_reg.period = period;
    ebs_mutex_unlock(&mutex_comm);
}

int comm_reg_channel_get(void)
{
    int chn = 0;
    ebs_mutex_lock(&mutex_comm);
    chn = ebs_comm_reg.index;
    ebs_mutex_unlock(&mutex_comm);
    return chn;
}

void comm_reg_channel_mode_set(uint8_t mode)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_reg.chn_mode = mode;
    ebs_mutex_unlock(&mutex_comm);
}

void comm_reg_ip_addr_set(uint32_t ip)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_reg.ip[0] = ip;
    ebs_mutex_unlock(&mutex_comm);
}

void comm_reg_ip_port_set(uint16_t port)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_reg.port[0] = port;
    ebs_mutex_unlock(&mutex_comm);
}

void comm_reg_4g_addr_set(uint32_t ip)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_reg.ip[1] = ip;
    ebs_mutex_unlock(&mutex_comm);
}

void comm_reg_4g_port_set(uint16_t port)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_reg.port[1] = port;
    ebs_mutex_unlock(&mutex_comm);
}

void comm_reg_reconnection(void)
{
    ebs_mutex_lock(&mutex_comm);
    ebs_comm_reg.index = 0;
#if PLATFORM == PLATFORM_Ex800G
    comm_reg_restart_connection();
#endif
    ebs_mutex_unlock(&mutex_comm);
}

#if PLATFORM == PLATFORM_Ex800G
void comm_reg_restart_connection(void)
{
    ebs_mutex_lock(&mutex_comm);
    if (ebs_comm_reg.trans->sem_wakeup)
        ebs_sem_post(&ebs_comm_reg.trans->sem_wakeup);
    ebs_mutex_unlock(&mutex_comm);
}
#endif
