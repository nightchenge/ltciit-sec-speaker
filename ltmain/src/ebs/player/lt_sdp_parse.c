/**
 * @file lt_sdp_parse.c
 * @author lijintang (lijintang@xdebian.com)
 * @brief
 * @version 0.1
 * @date 2023-04-07
 *
 * @copyright Copyright (c) 2022-2050 lijintang. All rights reserved.
 *
 */
#include "lt_sdp_parse.h"
#include "ql_log.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#define SDP_MAX_SIZE 2048
#define SPACE_CHARS " \t\r\n"

#define LT_LOG_WARN(msg, ...) QL_LOG(QL_LOG_LEVEL_WARN, "sdp_parse", msg, ##__VA_ARGS__)
#define LT_LOG_ERR(msg, ...) QL_LOG(QL_LOG_LEVEL_ERROR, "sdp_parse", msg, ##__VA_ARGS__)
#define LT_LOG_DBG(msg, ...) QL_LOG(QL_LOG_LEVEL_DEBUG, "sdp_parse", msg, ##__VA_ARGS__)

static inline __attribute__((const)) int av_toupper(int c)
{
    if (c >= 'a' && c <= 'z')
        c ^= 0x20;
    return c;
}

static inline __attribute__((const)) int av_tolower(int c)
{
    if (c >= 'A' && c <= 'Z')
        c ^= 0x20;
    return c;
}

static int aac_hex_to_data(uint8_t *data, const char *p)
{
    int c, len, v;
    len = 0;
    v = 1;
    while (1)
    {
        p += strspn(p, SPACE_CHARS);
        if (*p == '\0')
        {
            break;
        }
        c = av_toupper((unsigned char)*p++);
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'A' && c <= 'F')
            c = c - 'A' + 10;
        else
            break;
        v = (v << 4) | c;
        if (v & 0x100)
        {
            if (data)
                data[len] = v;
            len++;
            v = 1;
        }
    }
    return len;
}

static int av_strcasecmp(const char *a, const char *b)
{
    uint8_t c1, c2;
    do
    {
        c1 = av_tolower(*a++);
        c2 = av_tolower(*b++);
    } while (c1 && c1 == c2);
    return c1 - c2;
}

static void get_word_until_chars(char *buf, int buf_size, const char *sep, const char **pp)
{
    const char *p;
    char *q;

    p = *pp;
    p += strspn(p, SPACE_CHARS);
    q = buf;
    while (!strchr(sep, *p) && *p != '\0')
    {
        if ((q - buf) < buf_size - 1)
            *q++ = *p;
        p++;
    }
    if (buf_size > 0)
        *q = '\0';
    *pp = p;
}

static void get_word_sep(char *buf, int buf_size, const char *sep, const char **pp)
{
    if (**pp == '/')
        (*pp)++;
    get_word_until_chars(buf, buf_size, sep, pp);
}

static void get_word(char *buf, int buf_size, const char **pp)
{
    get_word_until_chars(buf, buf_size, SPACE_CHARS, pp);
}
static int av_strstart(const char *str, const char *pfx, const char **ptr)
{
    while (*pfx && *pfx == *str)
    {
        pfx++;
        str++;
    }
    if (!*pfx && ptr)
        *ptr = str;
    return !*pfx;
}

static int rtsp_next_attr_and_value(const char **p, char *attr, int attr_size, char *value, int value_size)
{
    *p += strspn(*p, SPACE_CHARS);
    if (**p)
    {
        get_word_sep(attr, attr_size, "=", p);
        if (**p == '=')
            (*p)++;
        get_word_sep(value, value_size, ";", p);
        if (**p == ';')
            (*p)++;
        return 1;
    }
    return 0;
}

static int parse_fmtp(sdp_parse_result_t *s, const char *attr, const char *value)
{
    if (!av_strcasecmp(attr, "config"))
    {
        int len = aac_hex_to_data(NULL, value);
        s->config = (uint8_t *)malloc(len);
        if (!s->config)
        {
            return -1;
        }
        memset(s->config, 0, len);
        aac_hex_to_data(s->config, value);
    }
    else if (!av_strcasecmp(attr, "sizelength"))
    {
        s->sizelength = atoi(value);
    }
    else if (!av_strcasecmp(attr, "indexlength"))
    {
        s->indexlength = atoi(value);
    }
    else if (!av_strcasecmp(attr, "indexdeltalength"))
    {
        s->indexdeltalength = atoi(value);
    }
    else if (!av_strcasecmp(attr, "profile-level-id"))
    {
        s->profile_level_id = atoi(value);
    }
    else if (!av_strcasecmp(attr, "mode"))
    {
        strncpy(s->mode, value, strlen(value));
    }
    else if (!av_strcasecmp(attr, "streamtype"))
    {
    }
    return 0;
}

static int sdp_parse_fmtp(sdp_parse_result_t *s, const char *p)
{
    char attr[256];
    char *value;
    int value_size = strlen(p) + 1;
    if (!(value = malloc(value_size)))
    {
        LT_LOG_WARN("failed to malloc data for FMTP.\n");
        return -1;
    }

    while (*p && *p == ' ')
        p++; // strip spaces
    while (*p && *p != ' ')
        p++; // eat protocol identifier
    while (*p && *p == ' ')
        p++; // strip trailing spaces

    while (rtsp_next_attr_and_value(&p, attr, sizeof(attr), value, value_size))
    {
        parse_fmtp(s, attr, value);
    }
    free(value);
    return 0;
}

static void sdp_parse_line(sdp_parse_result_t *s, int letter, const char *buf)
{
    char buf1[64], st_type[64];
    const char *p;
    int payload_type;
    LT_LOG_DBG("sdp: %c=%s \n", letter, buf);
    p = buf;
    if (s->skip_media && letter != 'm')
        return;
    switch (letter)
    {
    case 'c': // connection information --not required if included in all media
        // get_word(buf1, sizeof(buf1), &p);
        // if (strcmp(buf1, "IN") != 0)
        //     return;
        // get_word(buf1, sizeof(buf1), &p);
        // if (strcmp(buf1, "IP4") && strcmp(buf1, "IP6"))
        //     return;
        // get_word_sep(buf, sizeof(buf1), '/', &p);
        // strncpy(s->default_ip, buf1, strlen(buf1) > sizeof(s->default_ip) ? sizeof(s->default_ip) : strlen(buf1));
        // if (*p == '/')
        // {
        //     p++;
        //     get_word_sep(buf, sizeof(buf1), '/', &p);
        //     s->default_ttl = atoi(buf1);
        // }
        break;
    case 's': // session name
        break;
    case 'v': // protocol version
        break;
    case 'i': // media title
        break;
    case 'o': // originator and session identifier
        break;
    case 'u': // URI of description
        break;
    case 'm': // media name and transport address
        s->skip_media = 0;
        s->seen_fmtp = 0;
        s->seen_rtpmap = 0;
        get_word(st_type, sizeof(st_type), &p);
        if (!strcmp(st_type, "audio")) // 音频
        {
            get_word(buf1, sizeof(buf1), &p); // port
            get_word(buf1, sizeof(buf1), &p); // protocol
            get_word(buf1, sizeof(buf1), &p); //! format list
            s->payload_type = atoi(buf1);
        }
        else if (!strcmp(st_type, "video"))
        {
            get_word(buf1, sizeof(buf1), &p); // port
            get_word(buf1, sizeof(buf1), &p); // protocol
            get_word(buf1, sizeof(buf1), &p); //! format list
            s->payload_type = atoi(buf1);
        }
        else
        {
            s->skip_media = 1;
            return;
        }
        break;
    case 'a': // zero or more media attribute lines
        if (av_strstart(p, "control:", &p))
        {
            int len = strlen(p) + 1;
            s->control = (char *)malloc(len);
            if (!s->control)
            {
                LT_LOG_ERR("sdp a:control malloc failed.\n");
                return;
            }
            strcpy(s->control, p);
            s->control[len] = '\0';
        }
        else if (av_strstart(p, "rtpmap:", &p))
        {
            get_word(buf1, sizeof(buf1), &p);
            payload_type = atoi(buf1);
            if (s->payload_type != payload_type)
            {
                LT_LOG_WARN("rtpmap: payload type:%d not equal m: payload type:%d \n", payload_type, s->payload_type);
                s->skip_media = 1;
                return;
            }
            get_word_sep(buf1, sizeof(buf1), "/", &p);
            if (payload_type >= 96 && (av_strcasecmp(buf1, "mpeg4-generic") == 0))
            {
                // TODO AAC音频编码
                get_word_sep(buf1, sizeof(buf1), "/", &p);
                s->sample_freq = atoi(buf1);
                get_word_sep(buf1, sizeof(buf1), "/", &p);
                s->channel = atoi(buf1);
            }

            s->seen_rtpmap = 1;
            if (s->seen_fmtp)
            {
                sdp_parse_fmtp(s, s->delayed_fmtp);
            }
        }
        else if (av_strstart(p, "fmtp:", &p))
        {
            get_word(buf1, sizeof(buf1), &p);
            payload_type = atoi(buf1);
            if (s->payload_type != payload_type)
            {
                LT_LOG_WARN("fmtp: payload type:%d not equal m: payload type:%d \n", payload_type, s->payload_type);
                s->skip_media = 1;
                return;
            }
            if (s->seen_rtpmap)
            {
                sdp_parse_fmtp(s, buf);
            }
            else
            {
                s->seen_fmtp = 1;
                strncpy(s->delayed_fmtp, buf, strlen(buf));
            }
        }
        break;
    case 'k': // encryption key
        break;
    case 'e': // email address
        break;
    case 'b': // zero or more bandwidth information lines
        break;
    case 'p': // phone number
        break;
    case 't': // time the session is active
        break;
    default:
        break;
    }
}

int parse_sdp(sdp_parse_result_t *s, const char *content)
{
    const char *p;
    int letter;
    char buf[SDP_MAX_SIZE] = {0}, *q;
    p = content;
    while (1)
    {
        p += strspn(p, SPACE_CHARS);
        letter = *p;
        if (letter == '\0')
            break;
        p++;
        if (*p != '=')
            goto next_line;
        p++;

        q = buf;
        while (*p != '\n' && *p != '\r' && *p != '\0')
        {
            if ((q - buf) < sizeof(buf) - 1)
                *q++ = *p;
            p++;
        }
        *q = '\0';
        sdp_parse_line(s, letter, buf);
    next_line:
        while (*p != '\n' && *p != '\0')
            p++;
        if (*p == '\n')
            p++;
    }
    return 0;
}