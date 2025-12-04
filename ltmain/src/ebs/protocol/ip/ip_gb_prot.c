/**
 * @file ip_gb_prot.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-11-22
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#include "ebs.h"
#include "ip_gb_prot.h"
#include "ebs_param.h"
#if PLATFORM == PLATFORM_LINUX
#include "util_timer.h"
#include "util_msgq.h" //TODO 替换为别的消息队列
#include "ebs_log.h"
#include <time.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#elif PLATFORM == PLATFORM_Ex800G
#include <stdlib.h>
#include <string.h>
#include "util_msg.h"
#include "util_timer.h"
#include "sockets.h"
#endif

#include "util_crc.h"
#include "ebs_cmd.h"

#define _DEBUG_DATA 0

static void get_utc_time(uint8_t *utc)
{
    time_t t;
    t = ebs_time(NULL);
    utc[0] = (t & 0xFF000000) >> 24;
    utc[1] = (t & 0x00FF0000) >> 16;
    utc[2] = (t & 0x0000FF00) >> 8;
    utc[3] = (t & 0x000000FF);
}

static void start_time_out(ip_gb_trans_t *this)
{
    timer_start(&this->time_ticks);
}

int time_tick(ip_gb_trans_t *this, uint32_t ticks)
{
    if (this->time_ticks == 0)
        return 0;

    else if (timer_expired(&this->time_ticks, ticks))
    {
        timer_start(&this->time_ticks);
        return -1;
    }
    return 0;
}

void recv_reset(ip_gb_trans_t *this)
{
    this->phase = PHASE_IDLE;
    this->length = 0;
    this->len = 0;
    this->time_ticks = 0;
    memset(&this->dst_msg_head, 0, sizeof(this->dst_msg_head));
    this->dst_msg_packet.cmd_type = 0;
    this->dst_msg_packet.cmd_len = 0;
    this->dst_msg_packet.dst_obj_cnt = 0;
    memset(this->dst_msg_packet.dst_obj_list, 0, 4096);
    memset(this->dst_msg_packet.resources_code, 0, RESOURCES_CODE_LEN);
    memset(&this->sign_packet, 0, sizeof(this->sign_packet));
    memset(this->data, 0, 1024 * 64);
}

int reg_send(ip_gb_trans_t *this, uint8_t session[], bool need_sign, ip_gb_prot_msg_t *msg)
{
    int len = 0;
    uint32_t crc = 0;
    ip_gb_sign_packet_t sign;
    this->data[len++] = 0xFE;
    this->data[len++] = 0xFD;
#ifdef PROT_GUANGXI
    this->data[len++] = 0x02;
#else
    this->data[len++] = 0x01;
#endif
    this->data[len++] = 0x00;
    if (session != NULL)
    {
        memcpy(this->data + len, session, SESSION_LEN);
        len += SESSION_LEN;
        this->data[len++] = 2;
    }
    else
    {
        this->data[len++] = (this->session >> 24) & 0xFF;
        this->data[len++] = (this->session >> 16) & 0xFF;
        this->data[len++] = (this->session >> 8) & 0xFF;
        this->data[len++] = this->session & 0xFF;
        this->data[len++] = 1;
        this->session++; // 每发送一次，session单向递增
    }
    this->data[len++] = need_sign > 0;
    len += DATA_PACK_LEN_LEN;
    memcpy(this->data + len, msg->resources_code, RESOURCES_CODE_LEN);
    len += RESOURCES_CODE_LEN;
    this->data[len++] = msg->dst_obj_cnt >> 8;
    this->data[len++] = msg->dst_obj_cnt & 0x00FF;
    memcpy(this->data + len, msg->dst_obj_list, RESOURCES_CODE_LEN * msg->dst_obj_cnt);
    len += (RESOURCES_CODE_LEN * msg->dst_obj_cnt);
    this->data[len++] = msg->cmd_type;
    this->data[len++] = msg->cmd_len >> 8;
    this->data[len++] = msg->cmd_len & 0x00FF;
    memcpy(this->data + len, msg->cmd_data, msg->cmd_len);
    len += msg->cmd_len;
    this->data[len++] = 0x00;
    this->data[len++] = 0x4A; // 无论是否验签数字签名长度是固定的 4+6+64
    if (need_sign)
    {
        // lt_sm_query_sm(sign.cert);   //TODO 获取证书编号
        memset(&sign, 0x00, sizeof(ip_gb_sign_packet_t));
        get_utc_time(sign.time);
        // lt_sm_generate_signature(0, this->data, len, sign.time, sign.cert, sign.data); //TODO 证书签名
        memcpy(this->data + len, sign.time, SIGN_TIME_LEN);
        len += SIGN_TIME_LEN;
        memcpy(this->data + len, sign.cert, SIGN_CERT_LEN);
        len += SIGN_CERT_LEN;
        memcpy(this->data + len, sign.data, SIGN_DATA_LEN);
        len += SIGN_DATA_LEN;
    }
    else
    {
        memset(this->data + len, 0x00, 0x004A);
        len += 0x004A;
    }
    this->data[DATA_PACK_LEN_INDEX] = ((len + 4) >> 8) & 0x00FF;
    this->data[DATA_PACK_LEN_INDEX + 1] = (len + 4) & 0x00FF;
    crc = crc32_mpeg2(this->data, len);
    this->data[len++] = (crc & 0xFF000000) >> 24;
    this->data[len++] = (crc & 0x00FF0000) >> 16;
    this->data[len++] = (crc & 0x0000FF00) >> 8;
    this->data[len++] = crc & 0x000000FF;
#if _DEBUG_DATA
    ebs_log("-----------send 0x%04X data:\n", len);
    uint16_t i;
    for (i = 0; i < len; i++)
        ebs_log("%02X ", this->data[i]);
    ebs_log("\n");
#endif
    return send(this->fd, this->data, len, 0);
}

int lbk_send(ip_gb_trans_t *this, uint8_t session[], bool need_sign, ip_gb_prot_msg_t *msg)
{
    int len = 0;
    uint32_t crc = 0;
    this->data[len++] = 0xFE;
    this->data[len++] = 0xFD;
    this->data[len++] = 0x01;
    this->data[len++] = 0x00;
    if (session != NULL)
    {
        memcpy(this->data + len, session, SESSION_LEN);
        len += SESSION_LEN;
        this->data[len++] = 2; // 数据包类型，为应答数据包
    }
    else
    {
        this->data[len++] = (this->session >> 24) & 0xFF;
        this->data[len++] = (this->session >> 16) & 0xFF;
        this->data[len++] = (this->session >> 8) & 0xFF;
        this->data[len++] = this->session & 0xFF;
        this->data[len++] = 1;
        this->session++;
    }
    len += DATA_PACK_LEN_LEN;
    memcpy(this->data + len, msg->resources_code, RESOURCES_CODE_LEN);
    len += RESOURCES_CODE_LEN;
    this->data[len++] = msg->dst_obj_cnt >> 8;
    this->data[len++] = msg->dst_obj_cnt & 0x00FF;
    memcpy(this->data + len, msg->dst_obj_list, RESOURCES_CODE_LEN * msg->dst_obj_cnt);
    len += RESOURCES_CODE_LEN * msg->dst_obj_cnt;
    this->data[len++] = msg->cmd_type;
    this->data[len++] = msg->cmd_len >> 8;
    this->data[len++] = msg->cmd_len & 0x00FF;
    memcpy(this->data + len, msg->cmd_data, msg->cmd_len);
    len += msg->cmd_len;
    this->data[DATA_PACK_LEN_INDEX - SIGN_MARK_LEN] = ((len + 4) >> 8) & 0x00FF; // 回传数据没有签名数据
    this->data[DATA_PACK_LEN_INDEX] = (len + 4) & 0x00FF;
    crc = crc32_mpeg2(this->data, len);
    this->data[len++] = (crc & 0xFF000000) >> 24;
    this->data[len++] = (crc & 0x00FF0000) >> 16;
    this->data[len++] = (crc & 0x0000FF00) >> 8;
    this->data[len++] = crc & 0x000000FF;
#if _DEBUG_DATA
    ebs_log("-----------send 0x%04X data:\n", len);
    uint16_t i;
    for (i = 0; i < len; i++)
        ebs_log("%02X ", this->data[i]);
    ebs_log("\n");
#endif
    return send(this->fd, this->data, len, 0);
}

comm_result_t prot_parse(ip_gb_trans_t *this, uint8_t *data, int length)
{
    int i;
    uint32_t crc = 0;
    if (this->phase != PHASE_IDLE)
    {
        if (this->tick(this, 5000) != 0)
        {
            ebs_log("recv data parse timeout!\n");
            this->reset(this);
            return PROT_ERR_TIMEOUT;
        }
    }
    for (i = 0; i < length; i++)
    {
        switch (this->phase)
        {
        case PHASE_IDLE:
#if _DEBUG_DATA
            if (this->length == 0 && data[i] == 0xFE)
                ebs_log("----------Recv data:\n");
            ebs_log("%02X ", data[i]);
#endif
            if (data[i] == 0xFE)
            {
                this->length = 0;
                this->data[this->length++] = data[i];
            }
            else if (data[i] == 0xFD)
            {
                this->data[this->length++] = data[i];
                if (this->length == 2 && this->data[0] == 0xFE)
                {
                    this->len = 0;
                    this->phase = PHASE_HEAD;
                    start_time_out(this);
                }
                else
                {
                    this->reset(this);
                    return PROT_ERR_DATA;
                }
            }
            break;
        case PHASE_HEAD:
#if _DEBUG_DATA
            ebs_log("%02X ", data[i]);
#endif
            this->data[this->length++] = data[i];
            this->len++;
            if (this->len >= (PROT_VER_LEN + SESSION_LEN + DATA_PACK_TYPE_LEN + SIGN_MARK_LEN + DATA_PACK_LEN_LEN))
            {
                memcpy(&this->dst_msg_head, this->data, this->length);
                if (((this->dst_msg_head.ver[0] << 8) | this->dst_msg_head.ver[1]) != PROT_VER)
                {
                    this->reset(this);
                    ebs_log("protocol version error!\n");
                }
                this->dst_msg_head.pack_len = (this->data[this->length - 2] << 8) | (this->data[this->length - 1]);
                this->len = 0;
                this->phase = PHASE_MSG_START;
            }
            break;
        case PHASE_MSG_START:
#if _DEBUG_DATA
            ebs_log("%02X ", data[i]);
#endif
            this->data[this->length++] = data[i];
            this->len++;
            if (this->len >= (RESOURCES_CODE_LEN + DST_OBJ_CNT_LEN))
            {
                memcpy(this->dst_msg_packet.resources_code, this->data + this->length - RESOURCES_CODE_LEN - DST_OBJ_CNT_LEN, RESOURCES_CODE_LEN);
                this->dst_msg_packet.dst_obj_cnt = (this->data[DST_OBJ_COUNT_INDEX] << 8) | this->data[DST_OBJ_COUNT_INDEX + 1];
                this->len = 0;
                if (this->dst_msg_packet.dst_obj_cnt > 0)
                    this->phase = PHASE_MSG_DST_OBJ;
                else
                    this->phase = PHASE_MSG_CMD_HEAD;
            }
            break;
        case PHASE_MSG_DST_OBJ:
#if _DEBUG_DATA
            ebs_log("%02X ", data[i]);
#endif
            this->data[this->length++] = data[i];
            this->len++;
            if (this->len >= RESOURCES_CODE_LEN * this->dst_msg_packet.dst_obj_cnt)
            {
                memcpy(this->dst_msg_packet.dst_obj_list, this->data + (this->length - this->len), this->len);
                this->len = 0;
                this->phase = PHASE_MSG_CMD_HEAD;
            }
            break;
        case PHASE_MSG_CMD_HEAD:
#if _DEBUG_DATA
            ebs_log("%02X ", data[i]);
#endif
            this->data[this->length++] = data[i];
            this->len++;
            if (this->len >= (CMD_DATA_TYPE_LEN + CMD_DATA_LEN_LEN))
            {
                this->dst_msg_packet.cmd_type = this->data[this->length - this->len];
                this->dst_msg_packet.cmd_len = (this->data[this->length - 2] << 8) | this->data[this->length - 1];
                if (this->dst_msg_packet.cmd_len >= this->dst_msg_head.pack_len)
                {
                    ebs_log("message len error!\n");
                    this->reset(this);
                    return PROT_ERR_UNDEF;
                }
                this->len = 0;
                this->phase = PHASE_MSG_CMD_DATA;
            }
            break;
        case PHASE_MSG_CMD_DATA:
#if _DEBUG_DATA
            ebs_log("%02X ", data[i]);
#endif
            this->data[this->length++] = data[i];
            this->len++;
            if (this->len >= this->dst_msg_packet.cmd_len)
            {
                memcpy(this->dst_msg_packet.cmd_data, this->data + (this->length - this->len), this->len);
                this->len_tmp = this->length;
                this->len = 0;
#ifdef PROT_GUANGXI
                if (this->dst_msg_head.sign_mark)
                    this->phase = PHASE_VERF_START;
                else
                    this->phase = PHASE_CRC;
#else
                this->phase = PHASE_VERF_START;
#endif
            }
            break;
        case PHASE_VERF_START:
#if _DEBUG_DATA
            ebs_log("%02X ", data[i]);
#endif
            this->data[this->length++] = data[i];
            this->len++;
            if (this->dst_msg_head.sign_mark == 0)
            {
                if ((this->dst_msg_head.pack_len - this->len_tmp - CRC32_LEN) == SIGN_DATA_LEN_LEN) // 整包数据长度 - 已接受长度 -CRC长度 == 2
                {
                    if (this->len >= SIGN_DATA_LEN_LEN)
                    {
                        this->sign_packet.len = (this->data[this->length - this->len] << 8) | (this->data[this->length - this->len + 1]);
                        this->phase = PHASE_CRC;
                        this->len = 0;
                    }
                }
                else
                {
                    if (this->len >= (SIGN_DATA_LEN_LEN + SIGN_TIME_LEN + SIGN_CERT_LEN))
                    {
                        this->sign_packet.len = (this->data[this->length - this->len] << 8) | (this->data[this->length - this->len + 1]);
                        memcpy(this->sign_packet.time, this->data + (this->length - this->len + SIGN_DATA_LEN_LEN), SIGN_TIME_LEN);
                        memcpy(this->sign_packet.cert, this->data + (this->length - this->len + SIGN_DATA_LEN_LEN + SIGN_TIME_LEN), SIGN_CERT_LEN);
                        if ((this->dst_msg_head.pack_len - this->len_tmp - CRC32_LEN) == (SIGN_DATA_LEN_LEN + SIGN_TIME_LEN + SIGN_CERT_LEN))
                        {
                            this->phase = PHASE_CRC;
                        }
                        else
                        {
                            this->phase = PHASE_SIGN_INFO;
                        }
                        this->len = 0;
                    }
                }
            }
            else
            {
                if (this->len >= (SIGN_DATA_LEN_LEN + SIGN_TIME_LEN + SIGN_CERT_LEN))
                {
                    this->sign_packet.len = (this->data[this->length - this->len] << 8) | (this->data[this->length - this->len + 1]);
                    memcpy(this->sign_packet.time, this->data + (this->length - this->len + SIGN_DATA_LEN_LEN), SIGN_TIME_LEN);
                    memcpy(this->sign_packet.cert, this->data + (this->length - this->len + SIGN_DATA_LEN_LEN + SIGN_TIME_LEN), SIGN_CERT_LEN);
                    this->len = 0;
                    this->phase = PHASE_SIGN_INFO;
                }
            }
            break;
        case PHASE_SIGN_INFO:
#if _DEBUG_DATA
            ebs_log("%02X ", data[i]);
#endif
            this->data[this->length++] = data[i];
            this->len++;
            if (this->len >= (this->sign_packet.len - (SIGN_TIME_LEN + SIGN_CERT_LEN)))
            {
                memcpy(this->sign_packet.data, this->data + (this->length - this->len), this->len);
                this->len = 0;
                this->phase = PHASE_CRC;
            }
            break;
        case PHASE_CRC:
#if _DEBUG_DATA
            ebs_log("%02X ", data[i]);
#endif
            this->data[this->length++] = data[i];
            this->len++;
            if (this->len >= CRC32_LEN)
            {
#if _DEBUG_DATA
                ebs_log("\n");
#endif
                this->len = 0;
                crc = crc32_mpeg2(this->data, this->length - CRC32_LEN);
                if (crc == (this->data[this->length - 1] | (this->data[this->length - 2] << 8) | (this->data[this->length - 3] << 16) | (this->data[this->length - 4] << 24)))
                {
                    this->length -= CRC32_LEN;
#if _DEBUG_DATA
                    ebs_log("------------- CRC OK\n");
#endif
#ifdef PROT_GUANGXI
                    if ((this->dst_msg_packet.cmd_type != 0x10) && (this->dst_msg_packet.cmd_type != 0x11) && (this->dst_msg_packet.cmd_type != 0x17) && (this->dst_msg_packet.cmd_type != 0xFE) && (this->dst_msg_packet.cmd_type != 0x18)) //! 心跳、参数查询、证书配置和软模块扩展协议指令不需要验签
#else
                    if ((this->dst_msg_packet.cmd_type != 0x10) && (this->dst_msg_packet.cmd_type != 0x11) && (this->dst_msg_packet.cmd_type != 0x17) && (this->dst_msg_packet.cmd_type != 0xFE)) //! 心跳、参数查询、证书配置和软模块扩展协议指令不需要验签
#endif
                    {
                        // if (0 != lt_sm_verify_signature(this->data, this->length - (SIGN_DATA_LEN_LEN + this->sign_packet.len), this->sign_packet.time, this->sign_packet.cert, this->sign_packet.data))
                        // {
                        //     ebs_log( "IP channel cmd type:%02X verify signature error.\n", this->dst_msg_packet.cmd_type);
                        //     this->reset(this);
                        //     return PROT_ERR_VER;
                        // }    //TODO 签名
                    }
                    this->phase = PHASE_END;
                    return COMM_RESULT_OK;
                }
                else
                {
                    ebs_log("------------- CRC ERROR crc:%08X data:%08X \n", crc, (this->data[this->length - 1] | (this->data[this->length - 2] << 8) | (this->data[this->length - 3] << 16) | (this->data[this->length - 4] << 24)));
                    this->reset(this);
                    return PROT_ERR_CRC;
                }
            }
            break;
        default:
            this->reset(this);
            return PROT_ERR_DATA;
            break;
        }
    }
    return COMM_HAS_DATA;
}

comm_result_t cmd_parse(ip_gb_trans_t *this)
{
    int i = 0;
    cmd_result_t result;
    uint16_t resp_len;
    for (i = 0; i < this->dst_msg_packet.dst_obj_cnt; i++)
    {
        if (resources_code_match(this->dst_msg_packet.dst_obj_list + RESOURCES_CODE_LEN * i)) // TODO 资源编码匹配
            break;
    }

    if (i >= this->dst_msg_packet.dst_obj_cnt)
    {
        ebs_log("=========resources code is not match========\n");
        result = CMD_ERR_REGION;
        this->resp[0] = VERF_ERR_RESOURCE_CODE;
        this->resp[1] = 0;
        this->resp[2] = 0;
        resp_len = 3;
    }
    else
    {
        result = ebs_ip_cmd_run(this->dst_msg_packet.cmd_type, this->dst_msg_packet.cmd_len, this->dst_msg_packet.cmd_data, &resp_len, this->resp);
    }

    if (this->response)
        this->response(this);

    if (result != CMD_RESULT_OK)
    {
        ebs_log("=========cmd exec error:%d =========\n", result);
        return result == CMD_ERR_REGION ? VERF_ERR_RESOURCE_CODE : PROT_ERR_PARAM;
    }
    return COMM_RESULT_OK;
}

comm_result_t resp_parse(ip_gb_trans_t *this)
{
    int32_t ret;
    uint32_t session;
    memcpy(&session, this->dst_msg_head.session, SESSION_LEN);
    if (this->src_msg_packet.cmd_type != this->dst_msg_packet.cmd_type )//|| (this->session - 1) != HTONL(session))四川XYD应急广播对接
    {
        ebs_log("src cmd type:%d dst cmd type:%d src session:%04X dst session:%04X \n", this->src_msg_packet.cmd_type, this->dst_msg_packet.cmd_type, this->session - 1, ntohl(session));
        return COMM_ERR_RESP;
    }
#if PLATFORM == PLATFORM_LINUX
    mqueue_msg_t *msg;
    uint8_t *data;
    if ((msg = mqueue_msg_alloc(this->dst_msg_packet.cmd_type, this->dst_msg_packet.cmd_len)) == NULL)
    {
        ebs_log("msg queue alloc failed.\n");
        return SYS_ERR_WRITE_FILE;
    }
    data = (uint8_t *)mqueue_msg_body(msg);
    memcpy(data, this->dst_msg_packet.cmd_data, this->dst_msg_packet.cmd_len);

    ret = mqueue_put(this->queue_resp, msg);
#elif PLATFORM == PLATFORM_Ex800G
    ebs_msg_t msg = {0};
    msg.type = this->dst_msg_packet.cmd_type;
    msg.size = this->dst_msg_packet.cmd_len;
    msg.data = (uint8_t *)ebs_msg_malloc(msg.size);
    memcpy(msg.data, this->dst_msg_packet.cmd_data, msg.size);
    ret = ebs_msg_queue_send(this->queue_resp, (uint8_t *)&msg, sizeof(ebs_msg_t), QL_WAIT_FOREVER);
#endif
    if (ret < 0)
    {
        ebs_log("msg send error:%d \n", ret);
        ebs_msg_free(msg.data);
        return SYS_ERR_WRITE_FILE;
    }
    return COMM_RESULT_OK;
}

comm_result_t ip_gb_common_response(ip_gb_trans_t *this)
{
    uint16_t resp_len;
    resp_len = (this->resp[1] << 8) | this->resp[2];
    resp_len += 3;
    this->dst_msg_packet.dst_obj_cnt = 1;
    memcpy(this->dst_msg_packet.dst_obj_list, this->dst_msg_packet.resources_code, RESOURCES_CODE_LEN);
    memcpy(this->dst_msg_packet.resources_code, resources_code_remote, RESOURCES_CODE_LEN);
    this->dst_msg_packet.cmd_len = resp_len;
    memcpy(this->dst_msg_packet.cmd_data, this->resp, this->dst_msg_packet.cmd_len);
    this->ssend(this, this->dst_msg_head.session, 0, &this->dst_msg_packet);
    return COMM_RESULT_OK;
}
