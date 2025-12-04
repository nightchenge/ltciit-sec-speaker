/**
 * @file lt_sdp_parse.h
 * @author lijintang (lijintang@xdebian.com)
 * @brief
 * @version 0.1
 * @date 2023-04-07
 *
 * @copyright Copyright (c) 2022-2050 lijintang. All rights reserved.
 *
 */
#ifndef __LT_SDP_PARSE_H__
#define __LT_SDP_PARSE_H__
#include <stdint.h>

typedef struct sdp_parse_result_t
{
    // char default_ip[16];     //! 暂时不关注sdp内给的ip
    // uint16_t default_port;
    // int default_ttl;
    uint8_t skip_media;
    uint8_t seen_rtpmap;
    uint8_t seen_fmtp;
    char delayed_fmtp[2048];

    int payload_type;
    int sample_freq;
    int channel;
    int sizelength;
    int indexlength;
    int indexdeltalength;
    int profile_level_id;
    uint8_t *config;
    char mode[12];
    char *control;
} sdp_parse_result_t;

int parse_sdp(sdp_parse_result_t *s, const char *content);
#endif /* __LT_SDP_PARSE_H__ */