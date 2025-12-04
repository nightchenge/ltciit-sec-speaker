/**
 * @file lt_rtsp.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-09-07
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#include "lt_rtsp.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "ql_log.h"
#include "ql_api_osi.h"
#include "ql_osi_def.h"

#include "sockets.h"
#include "lwip/ip_addr.h"
#include "lwip/netdb.h"
#include "lwip/netif.h"
#include "lwip/inet.h"
#include "lwip/tcp.h"

#define rtsp_log(msg, ...) QL_LOG(QL_LOG_LEVEL_INFO, "rtsp_prot", msg, ##__VA_ARGS__)
#define SPACE_CHARS " \t\r\n"

static inline int64_t _gettime_relative()
{
    ql_timeval_t tv;
    ql_gettimeofday(&tv);
    return (int64_t)tv.sec * 1000000 + tv.usec + 42 * 60 * 60 * INT64_C(1000000);
}

static inline void get_word_until_chars(char *buf, int buf_size, const char *sep, const char **pp)
{
    const char *p;
    char *q;
    p = *pp;
    p += strspn(p, SPACE_CHARS);
    q = buf;
    while (!strchr(sep, *p) && *p != '\0')
    {
        if (q - buf < buf_size - 1)
            *q++ = *p;
        p++;
    }
    if (buf_size > 0)
        *q = '\0';
    *pp = p;
}

static inline void get_word_sep(char *buf, int buf_size, const char *sep, const char **pp)
{
    if (**pp == '/')
        (*pp)++;
    get_word_until_chars(buf, buf_size, sep, pp);
}

static inline void get_word(char *buf, int buf_size, const char **pp)
{
    get_word_until_chars(buf, buf_size, SPACE_CHARS, pp);
}

static size_t _strlcpy(char *dst, const char *src, size_t size)
{
    size_t len = 0;
    while (++len < size && *src)
        *dst++ = *src++;
    if (len <= size)
        *dst = 0;
    return len + strlen(src) - 1;
}

static size_t _strlcat(char *dst, const char *src, size_t size)
{
    size_t len = strlen(dst);
    if (size < len + 1)
        return len + strlen(src);
    return len + _strlcpy(dst + len, src, size - len);
}

static int _strncasecmp(const char *a, const char *b, size_t n)
{
    uint8_t c1, c2;
    if (n <= 0)
        return 0;
    do
    {
        c1 = tolower((int)*a++);
        c2 = tolower((int)*b++);
    } while (--n && c1 && c1 == c2);
    return c1 - c2;
}

static int _stristart(const char *str, const char *pfx, const char **ptr)
{
    while (*pfx && toupper((int)*pfx) == toupper((int)*str))
    {
        pfx++;
        str++;
    }
    if (!*pfx && ptr)
        *ptr = str;
    return !*pfx;
}

static void rtsp_url_split(char *proto, int proto_size,
                           char *authorization, int authorization_size,
                           char *hostname, int hostname_size,
                           int *port_ptr, char *path, int path_size, const char *url)
{
    const char *p = NULL, *ls = NULL, *at = NULL, *at2 = NULL, *col = NULL, *brk = NULL;

    if (port_ptr)
        *port_ptr = -1;
    if (proto_size > 0)
        proto[0] = 0;
    if (authorization_size > 0)
        authorization[0] = 0;
    if (hostname_size > 0)
        hostname[0] = 0;
    if (path_size > 0)
        path[0] = 0;

    // parse protocol
    if ((p = strchr(url, ':')))
    {
        _strlcpy(proto, url, proto_size > (p + 1 - url) ? (p + 1 - url) : proto_size);
        p++;
        if (*p == '/')
            p++;
        if (*p == '/')
            p++;
    }
    else
    {
        _strlcpy(path, url, path_size);
        return;
    }

    ls = p + strcspn(p, "/?#");
    _strlcpy(path, ls, path_size);

    if (ls != p)
    {
        at2 = p;
        while ((at = strchr(p, '@')) && at < ls)
        {
            _strlcpy(authorization, at2, authorization_size > (at - at2 + 1) ? (at - at2 + 1) : authorization_size);
            p = at + 1;
        }
        // rtsp://192.168.99.233:55400/45326250000000103010101202305300486.sdp
        if (*p == '[' && (brk = strchr(p, ']')) && brk < ls)
        {
            _strlcpy(hostname, p + 1, hostname_size > (brk - p) ? (brk - p) : hostname_size);
            if (brk[1] == ':' && port_ptr)
                *port_ptr = atoi(brk + 2);
        }
        else if ((col = strchr(p, ':')) && col < ls)
        {
            _strlcpy(hostname, p, (col + 1 - p) > hostname_size ? hostname_size : (col + 1 - p));
            if (port_ptr)
                *port_ptr = atoi(col + 1);
        }
        else
        {
            _strlcpy(hostname, p, (ls + 1 - p) > hostname_size ? hostname_size : (ls + 1 - p));
        }
    }
}

/**
 * @brief
 *
 * @param fd
 * @param buf
 * @param size
 * @param timeout
 * @return int -1:select error -2:slect timeout -3:server closed the connection -4:server reset the connection -5:recv error
 */
static inline int rtsp_select_recv(int fd, uint8_t *buf, int size, uint32_t timeout)
{
    if (fd <= 0)
        return -1;
    int ret = 0;
    fd_set rfds = {0};
    int max_fd = fd + 1;
    struct timeval tout = {0};
    tout.tv_sec = timeout / 1000;
    tout.tv_usec = (timeout % 1000) * 1000;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    ret = select(max_fd, &rfds, NULL, NULL, &tout);
    if (ret == -1)
    {
        rtsp_log("select error");
        return -1;
    }
    else if (ret == 0)
    {
        rtsp_log("select timeout");
        return 0;
    }
    else
    {
        if (FD_ISSET(fd, &rfds))
        {
            ret = recv(fd, buf, size, 0);
            if (ret <= 0)
            {
                if (ret == 0)
                { //! server closed the connection
                    rtsp_log("server closed the connection");
                }
                else if (errno == ECONNRESET)
                {
                    rtsp_log("server reset the connection");
                }
                else
                {
                    rtsp_log("recv error");
                }
                return -1;
            }
            else
            {
                return ret;
            }
        }
    }
    return -1;
}

static inline int rtsp_skip_packet(rtsp_client_t *this)
{
    int ret, len, len1;
    uint8_t buf[MAX_URL_SIZE] = {0};

    // ret = recv(this->rtsp_fd, buf, 3, 0);
    ret = rtsp_select_recv(this->rtsp_fd, buf, 3, 3000);
    if (ret != 3)
        return ret < 0 ? ret : -5;
    len = (int)(buf[1] << 8 | buf[2]);

    while (len > 0)
    {
        len1 = len;
        if (len1 > sizeof(buf))
            len1 = sizeof(buf);
        // ret = recv(this->rtsp_fd, buf, len1, 0);
        ret = rtsp_select_recv(this->rtsp_fd, buf, len1, 1000);
        if (ret != len1)
            return ret < 0 ? ret : -5;
        len -= len1;
    }

    return 0;
}

static inline void rtsp_parse_range(int *min_ptr, int *max_ptr, const char **pp)
{
    const char *q;
    char *p;
    int v;

    q = *pp;
    q += strspn(q, SPACE_CHARS);
    v = strtol(q, &p, 10);
    if (*p == '-')
    {
        p++;
        *min_ptr = v;
        v = strtol(p, &p, 10);
        *max_ptr = v;
    }
    else
    {
        *min_ptr = v;
        *max_ptr = v;
    }
    *pp = p;
}

static inline void rtsp_parse_transport(rtsp_client_t *this, rtsp_response_t *reply, const char *p)
{
    char transport_protocol[16];
    char profile[16];
    char lower_transport[16];
    char parameter[16];
    rtsp_transport_field *th;
    char buf[256];

    reply->nb_transports = 0;

    while (1)
    {
        p += strspn(p, SPACE_CHARS);
        if (*p == '\0')
            break;

        th = &reply->transports[reply->nb_transports];

        get_word_sep(transport_protocol, sizeof(transport_protocol), "/", &p);
        if (!_strncasecmp(transport_protocol, "rtp", 3))
        {
            get_word_sep(profile, sizeof(profile), "/;,", &p);
            lower_transport[0] = '\0';
            /* rtp/avp/<protocol> */
            if (*p == '/')
            {
                get_word_sep(lower_transport, sizeof(lower_transport), ";,", &p);
            }
            th->transport = RTSP_TRANSPORT_RTP;
        }
        else if (!_strncasecmp(transport_protocol, "x-pn-tng", 8) || !_strncasecmp(transport_protocol, "x-real-rdt", 10))
        {
            get_word_sep(profile, sizeof(profile), "/;,", &p);
            lower_transport[0] = '\0';
            /* x-pn-tng/<protocol> */
            if (*p == '/')
            {
                get_word_sep(lower_transport, sizeof(lower_transport), ";,", &p);
            }
            th->transport = RTSP_TRANSPORT_RDT;
        }
        else if (!_strncasecmp(transport_protocol, "raw", 3))
        {
            get_word_sep(profile, sizeof(profile), "/;,", &p);
            lower_transport[0] = '\0';
            /* raw/<protocol> */
            if (*p == '/')
            {
                get_word_sep(lower_transport, sizeof(lower_transport), ";,", &p);
            }
            th->transport = RTSP_TRANSPORT_RAW;
        }
        else
            break;
        if (!_strncasecmp(lower_transport, "TCP", 3))
            th->lower_transport = RTSP_LOWER_TRANSPORT_TCP;
        else
            th->lower_transport = RTSP_LOWER_TRANSPORT_UDP;

        if (*p == ';')
            p++;

        while (*p != '\0' && *p != ',')
        {
            get_word_sep(parameter, sizeof(parameter), "=;,", &p);
            if (!strcmp(parameter, "port"))
            {
                if (*p == '=')
                {
                    p++;
                    rtsp_parse_range(&th->port_min, &th->port_max, &p);
                }
            }
            else if (!strcmp(parameter, "client_port"))
            {
                if (*p == '=')
                {
                    p++;
                    rtsp_parse_range(&th->client_port_min, &th->client_port_max, &p);
                }
            }
            else if (!strcmp(parameter, "server_port"))
            {
                if (*p == '=')
                {
                    p++;
                    rtsp_parse_range(&th->server_port_min, &th->server_port_max, &p);
                }
            }
            else if (!strcmp(parameter, "interleaved"))
            {
                if (*p == '=')
                {
                    p++;
                    rtsp_parse_range(&th->interleaved_min, &th->interleaved_max, &p);
                }
            }
            else if (!strcmp(parameter, "multicast"))
            {
                if (th->lower_transport == RTSP_LOWER_TRANSPORT_UDP)
                    th->lower_transport = RTSP_LOWER_TRANSPORT_UDP_MULTICAST;
            }
            else if (!strcmp(parameter, "ttl"))
            {
                if (*p == '=')
                {
                    char *end;
                    p++;
                    th->ttl = strtol(p, &end, 10);
                    p = end;
                }
            }
            else if (!strcmp(parameter, "destination"))
            {
                if (*p == '=')
                {
                    p++;
                    get_word_sep(buf, sizeof(buf), ";,", &p);
                    // TODO get_sockaddr(this, buf, &th->destination);
                }
            }
            else if (!strcmp(parameter, "source"))
            {
                if (*p == '=')
                {
                    p++;
                    get_word_sep(buf, sizeof(buf), ";,", &p);
                    _strlcpy(th->source, buf, sizeof(th->source));
                }
            }
            else if (!strcmp(parameter, "mode"))
            {
                if (*p == '=')
                {
                    get_word_sep(buf, sizeof(buf), ";,", &p);
                    if (!strcmp(buf, "record") || !strcmp(buf, "receive"))
                        th->mode_record = 1;
                }
            }

            while (*p != ';' && *p != '\0' && *p != ',')
                p++;
            if (*p == ';')
                p++;
        }
        if (*p == ',')
            p++;
        reply->nb_transports++;
        if (reply->nb_transports >= RTSP_MAX_TRANSPORTS)
            break;
    }
}

static inline void rtsp_parse_line(rtsp_client_t *this, rtsp_response_t *reply, const char *buf, const char *method)
{
    const char *p;

    p = buf;
    if (_stristart(p, "Session:", &p))
    {
        int t;
        get_word_sep(reply->session_id, sizeof(reply->session_id), ";", &p);
        if (_stristart(p, ";timeout=", &p) && (t = strtol(p, NULL, 10)) > 0)
        {
            reply->timeout = t;
        }
    }
    else if (_stristart(p, "Content-Length:", &p))
    {
        reply->content_length = strtol(p, NULL, 10);
    }
    else if (_stristart(p, "Transport:", &p))
    {
        rtsp_parse_transport(this, reply, p);
    }
    else if (_stristart(p, "CSeq:", &p))
    {
        reply->seq = strtol(p, NULL, 10);
        // rtsp_log("reply->seq:%d ", reply->seq);
    }
    else if (_stristart(p, "Range:", &p))
    {
        // TODO rtsp_parse_range_npt(p,&reply->range_start,&reply->range_end);
    }
    else if (_stristart(p, "RealChallenge1:", &p))
    {
        p += strspn(p, SPACE_CHARS);
        _strlcpy(reply->real_challenge, p, sizeof(reply->real_challenge));
    }
    else if (_stristart(p, "Server:", &p))
    {
        p += strspn(p, SPACE_CHARS);
        _strlcpy(reply->server, p, sizeof(reply->server));
    }
    else if (_stristart(p, "Notice:", &p) || _stristart(p, "X-Notice:", &p))
    {
        reply->notice = strtol(p, NULL, 10);
    }
    else if (_stristart(p, "Location:", &p))
    {
        p += strspn(p, SPACE_CHARS);
        _strlcpy(reply->location, p, sizeof(reply->location));
    }
    else if (_stristart(p, "WWW-Authenticate:", &p))
    {
        // p += strspn(p, SPACE_CHARS);
    }
    else if (_stristart(p, "Authentication-Info:", &p))
    {
        // p + strspn(p, SPACE_CHARS);
    }
    else if (_stristart(p, "Content-Base:", &p))
    {
        p += strspn(p, SPACE_CHARS);
        if (method && !strcmp(method, "DESCRIBE"))
        {
            _strlcpy(this->control_uri, p, sizeof(this->control_uri));
        }
    }
    else if (_stristart(p, "RTP-Info:", &p))
    {
        p += strspn(p, SPACE_CHARS);
        if (method && !strcmp(method, "PLAY"))
        {
            // TODO rtsp_parse_rtp_info(this,p);
        }
    }
    else if (_stristart(p, "Public:", &p))
    {
        if (strstr(p, "GET_PARAMETER") && method && !strcmp(method, "OPTIONS"))
        {
            // this->get_parameter_supported = 1;
        }
    }
    else if (_stristart(p, "x-Accept-Dynamic-Rate:", &p))
    {
        p += strspn(p, SPACE_CHARS);
        // this->accept_dynamic_rate = atoi(p);
    }
    else if (_stristart(p, "Content-Type:", &p))
    {
        p += strspn(p, SPACE_CHARS);
        _strlcpy(reply->content_type, p, sizeof(reply->content_type));
    }
    else if (_stristart(p, "com.ses.streamID:", &p))
    {
        p += strspn(p, SPACE_CHARS);
        _strlcpy(reply->stream_id, p, sizeof(reply->stream_id));
    }
}

static int rtsp_read_reply(rtsp_client_t *this, rtsp_response_t *reply, unsigned char **content_ptr, int return_on_interleaved_data, const char *method)
{
    char buf[MAX_URL_SIZE], buf1[MAX_URL_SIZE], *q;
    unsigned char ch;
    const char *p;
    int ret = 0, content_length, line_count, request, timeout = 0;
    unsigned char *content;

    line_count = 0;
    request = 0;
    content = NULL;
    memset(reply, 0, sizeof(rtsp_response_t));

    // this->last_reply[0] = '\0';
    while (1)
    {
        q = buf;
        while (1)
        {
            // ret = recv(this->rtsp_fd, &ch, 1, 0);
            do
            {
                ret = rtsp_select_recv(this->rtsp_fd, &ch, 1, 10000);
            } while (ret == 0 && timeout++ < 2);
            if (ret != 1)
                return ret < 0 ? ret : -5;
            if (ch == '\n')
                break;
            if (ch == '$' && q == buf)
            {
                if (return_on_interleaved_data)
                {
                    return 1;
                }
                else
                {
                    ret = rtsp_skip_packet(this);
                    if (ret < 0)
                        return ret;
                }
            }
            else if (ch != '\r')
            {
                if ((q - buf) < sizeof(buf) - 1)
                    *q++ = ch;
            }
        }
        *q = '\0';

        // rtsp_log("line='%s' ", buf);

        if (buf[0] == '\0')
            break;
        p = buf;

        if (line_count == 0)
        {
            get_word(buf1, sizeof(buf1), &p);
            if (!strncmp(buf1, "RTSP/", 5))
            {
                get_word(buf1, sizeof(buf1), &p);
                reply->status_code = atoi(buf1);
                _strlcpy(reply->reason, p, sizeof(reply->reason));
            }
            else
            {
                _strlcpy(reply->reason, buf1, sizeof(reply->reason));
                get_word(buf1, sizeof(buf1), &p);
                request = 1; // RTSP server
            }
        }
        else
        {
            rtsp_parse_line(this, reply, p, method);
            // _strlcat(this->last_reply, p, sizeof(this->last_reply));
            // _strlcat(this->last_reply, "\n", sizeof(this->last_reply));
        }
        line_count++;
    }

    if (this->session_id[0] == '\0' && reply->session_id[0] != '\0' && !request)
    {
        _strlcpy(this->session_id, reply->session_id, sizeof(this->session_id));
    }

    content_length = reply->content_length;
    if (content_length > 0)
    {
        content = malloc(content_length + 1);
        if (!content)
        {
            rtsp_log("malloc failed");
            return -1;
        }
        // if ((ret = recv(this->rtsp_fd, content, content_length, 0)) != content_length)
        if ((ret = rtsp_select_recv(this->rtsp_fd, content, content_length, 5000)) != content_length)
        {
            free(content);
            content = NULL;
            rtsp_log("rtsp recv content length error,recv len:%d content_length:%d", ret, content_length);
            return -1;
        }
        content[content_length] = '\0';
    }

    if (content_ptr)
        *content_ptr = content;
    else
    {
        free(content);
        content = NULL;
    }

    if (request)
    {
        //! 作为RTSP服务器时需要
    }

    if (this->seq != reply->seq)
    {
        rtsp_log("CSeq %d expected, %d received.", this->seq, reply->seq);
    }

    if (reply->notice == 2101 /* End-of-Stream Reached */ ||
        reply->notice == 2104 /* Start-of-Stream Reached*/ ||
        reply->notice == 2306 /* Continuous Feed Terminated*/)
    {
        this->state = RTSP_STATE_IDLE;
    }
    else if (reply->notice >= 4400 && reply->notice < 5500)
    {
        rtsp_log("data or server error");
        return -1;
    }
    else if (reply->notice == 2401 /*Ticket Expired*/ ||
             (reply->notice >= 5500 && reply->notice < 5600) /*end of term*/)
    {
        return -1;
    }

    return 0;
}

static int rtsp_send_cmd_with_content(rtsp_client_t *this, const char *method, const char *url, const char *header, rtsp_response_t *reply, unsigned char **content_ptr, const unsigned char *send_content, int send_content_length)
{
    int ret = 0;
    if (this->rtsp_fd < 0)
        return -1;

    char buf[MAX_URL_SIZE];

    this->seq++;
    snprintf(buf, sizeof(buf), "%s %s RTSP/1.0\r\n", method, url);
    if (header)
        strcat(buf, header);
    snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "CSeq: %d\r\n", this->seq);
    // snprintf(buf, sizeof(buf), "%sUser-Agent: %s\r\n", buf, this->user_agent);//! 暂时不添加user agent
    if (this->session_id[0] != '\0' && (!header || !strstr(header, "\nIf-Match:")))
        snprintf(buf + strlen(buf), sizeof(buf), "Session: %s\r\n", this->session_id);

    // if(this->auth[0])
    // {
    // TODO
    // }

    if (send_content_length > 0 && send_content)
    {
        snprintf(buf + strlen(buf), sizeof(buf), "Content-Length: %d\r\n", send_content_length);
    }
    strcat(buf, "\r\n");

    send(this->rtsp_fd, buf, strlen(buf), 0);
    if (send_content_length > 0 && send_content)
    {
        if (this->control_transport == RTSP_MODE_TUNNEL)
        {
            rtsp_log("Tunneling of RTSP requests with content data");
            return -1;
        }
        send(this->rtsp_fd, send_content, send_content_length, 0);
    }
    this->last_cmd_time = _gettime_relative();

    //! read reply
    if ((ret = rtsp_read_reply(this, reply, content_ptr, 0, method)) < 0)
    {
        return ret;
    }

    if (reply->status_code > 400)
    {
        rtsp_log("method %s failed:%d%s", method, reply->status_code, reply->reason);
        // rtsp_log("last reply:%s", this->last_reply);
    }
    return 0;
}

static inline int rtsp_cmd_send(rtsp_client_t *this, const char *method, const char *url, const char *headers, rtsp_response_t *reply, unsigned char **content_ptr)
{
    return rtsp_send_cmd_with_content(this, method, url, headers, reply, content_ptr, NULL, 0);
}

static int rtsp_disconnect(rtsp_client_t *this)
{
    if (this->rtsp_fd > 0)
    {
        // int ret = 0;
        // uint8_t buf[8] = {0};
        // do
        // {
        //     ret = recv(this->rtsp_fd, buf, sizeof(buf), 0);
        // } while (ret > 0);
        close(this->rtsp_fd);
        this->rtsp_fd = -1;
    }
    return 0;
}

// #define IP_REGEX "^([0-9]{1,3}\\.){3}[0-9]{1,3}$"
static inline int is_ip_address(const char *host)
{
    // regex_t regex;
    // int ret;

    // ret = regcomp(&regex, IP_REGEX, REG_EXTENDED);
    // if (ret)
    // {
    //     rtsp_log("regcomp failed");
    //     return 0;
    // }
    // ret = regexec(&regex, host, 0, NULL, 0);
    // regfree(&regex);
    // return ret == 0;
    struct sockaddr_in sa;
    int ret = inet_pton(AF_INET, host, &(sa.sin_addr));
    return ret != 0;
}

static inline int resolve_hostname_to_ip(const char *hostname, struct sockaddr_in *addr)
{
    int ret = -1;
    struct addrinfo hints, *res, *p;
    int status;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0)
    {
        rtsp_log("getaddrinfo:%d", status);
        return -1;
    }

    for (p = res; p != NULL; p = p->ai_next)
    {
        if (p->ai_family == AF_INET) // IPV4
        {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            memcpy(&addr->sin_addr, &ipv4->sin_addr, sizeof(ipv4->sin_addr));
            ret = 0;
        }
        else
        {
            rtsp_log("IPV6 not supported yet");
            ret = -1;
        }
    }
    freeaddrinfo(res);
    return ret;
}

static int rtsp_connect(rtsp_client_t *this, const char *url)
{
    rtsp_log("rtsp connect, url:%s", url);
    if (url == NULL)
        return -1;
    char proto[128], auth[128], host[1024], path[1024];
    int port;
    int lower_transport_mask;

    // if (this->rtp_port_max < this->rtp_port_min)
    // {
    //     rtsp_log("Invalid UDP port range, max port %d less than min port %d ", this->rtp_port_max, this->rtp_port_min);
    //     return -1;
    // }

    this->url = (char *)realloc(this->url, strlen(url) + 1);
    if (this->url == NULL)
    {
        rtsp_log("remalloc failed");
        return -1;
    }
    strcpy(this->url, url);

    rtsp_disconnect(this);

    this->control_transport = RTSP_MODE_PLAIN;
    if (this->lower_transport_mask & ((1 << RTSP_LOWER_TRANSPORT_HTTP) | (1 << RTSP_LOWER_TRANSPORT_HTTPS)))
    {
        // https_tunnel = !!(this->lower_transport_mask & (1 << RTSP_LOWER_TRANSPORT_HTTPS));
        this->lower_transport_mask = 1 << RTSP_LOWER_TRANSPORT_TCP;
        this->control_transport = RTSP_MODE_TUNNEL;
    }
    this->lower_transport_mask &= (1 << RTSP_LOWER_TRANSPORT_NB) - 1;

    memset(this->response, 0, sizeof(rtsp_response_t));
    rtsp_url_split(proto, sizeof(proto), auth, sizeof(auth), host, sizeof(host), &port, path, sizeof(path), this->url);
    if (!strcmp(proto, "rtsps")) //! rtsp over ssl not support
    {
        rtsp_log("RTSP over SSL not supported");
        goto fail;
    }
    else if (!strcmp(proto, "satip"))
    {
        _strlcpy(proto, "rtsp", sizeof(proto));
        this->server_type = RTSP_SERVER_SATIP;
    }

    // if (*auth) //! not supported yet
    // {
    //     _strlcpy(this->auth, auth, sizeof(this->auth));
    // }

    if (port < 0)
        port = RTSP_DEFAULT_PORT;

    lower_transport_mask = this->lower_transport_mask;
    if (!lower_transport_mask)
        lower_transport_mask = (1 << RTSP_LOWER_TRANSPORT_NB) - 1;

    lower_transport_mask &= (1 << RTSP_LOWER_TRANSPORT_UDP) | (1 << RTSP_LOWER_TRANSPORT_TCP);
    if (!lower_transport_mask && this->control_transport == RTSP_MODE_TUNNEL)
    {
        rtsp_log("Unsupported lower transport method, only UDP and TCP are supported."); //! RTSP over UDP not supported yet
        goto fail;
    }

    sprintf(this->control_uri, "%s://%s:%d%s", proto, host, port, path);
    if (this->control_transport == RTSP_MODE_TUNNEL) //! RTSP over HTTP(HTTPS)
    {
        rtsp_log("RTSP over HTTP(HTTPS) not supported");
        goto fail;
    }
    else
    {
        this->rtsp_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (this->rtsp_fd < 0)
        {
            rtsp_log("RTSP socket create failed");
            goto fail;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (is_ip_address(host))
        {
            if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0)
            {
                rtsp_log("inet_pton error for %s", host);
                goto fail;
            }
        }
        else
        {
            if (resolve_hostname_to_ip(host, &addr) < 0)
            {
                rtsp_log("resolve hostname to ip failed");
                goto fail;
            }
            char ipstr[INET_ADDRSTRLEN];
            if (inet_ntop(addr.sin_family, &addr, ipstr, sizeof(ipstr)) < 0)
            {
                rtsp_log("inet_ntop err");
                goto fail;
            }
            sprintf(this->control_uri, "%s://%s:%d%s", proto, ipstr, port, path);
        }

        if (connect(this->rtsp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            rtsp_log("RTSP connect server failed");
            goto fail;
        }
    }
    this->seq = 0;
    this->state = RTSP_STATE_IDLE;
    return 0;
fail:
    rtsp_disconnect(this);
    return -1;
}

static int rtsp_options(rtsp_client_t *this)
{
    rtsp_log("rtsp options");
    char cmd[MAX_URL_SIZE];
    if (this->rtsp_fd < 0)
    {
        rtsp_log("RTSP socket not connected");
        return -1;
    }

    if (this->server_type != RTSP_SERVER_SATIP)
        this->server_type = RTSP_SERVER_RTP;

    while (1)
    {
        cmd[0] = 0;
        if (this->server_type == RTSP_SERVER_REAL)
        {
            strcat(cmd, "ClientChallenge: 9e26d33f2984236010ef6253fb1887f7\r\n"
                        "PlayerStarttime: [28/03/2003:22:50:23 00:00]\r\n"
                        "CompanyID: KnKV4M4I/B2FjJ1TToLycw==\r\n"
                        "GUID: 00000000-0000-0000-0000-000000000000\r\n");
        }

        rtsp_cmd_send(this, "OPTIONS", this->control_uri, cmd, this->response, NULL);
        if (this->response->status_code != RTSP_STATUS_OK)
        {
            rtsp_log("RTSP server response error");
            goto fail;
        }

        if (this->server_type != RTSP_SERVER_REAL && this->response->real_challenge[0])
        {
            this->server_type = RTSP_SERVER_REAL;
            continue;
        }
        else if (!_strncasecmp(this->response->server, "WMServer/", 9))
        {
            this->server_type = RTSP_SERVER_WMS;
        }
        else if (this->server_type == RTSP_SERVER_REAL)
        {
            strcpy(this->real_challenge, this->response->real_challenge);
        }
        break;
    }

    return 0;

fail:
    if (this->response->status_code >= 300 && this->response->status_code < 400)
    {
        this->url = (char *)realloc(this->url, strlen(this->response->location) + 1);
        if (!this->url)
        {
            rtsp_log("rtsp url malloc failed");
            return -1;
        }
        strcpy(this->url, this->response->location);
        this->session_id[0] = '\0';
        this->is_redirect = true;
    }
    return -1;
}

static int rtsp_describe(rtsp_client_t *this)
{
    rtsp_log("rtsp describe");
    char cmd[MAX_URL_SIZE];
    unsigned char *content = NULL;
    int ret;

    snprintf(cmd, sizeof(cmd), "Accept: application/sdp\r\n");
    if (this->server_type == RTSP_SERVER_REAL)
    {
        _strlcat(cmd, "Require: com.real.retain-entity-for-setup\r\n", sizeof(cmd));
    }

    rtsp_cmd_send(this, "DESCRIBE", this->control_uri, cmd, this->response, &content);
    if (this->response->status_code != RTSP_STATUS_OK)
    {
        if (content)
        {
            free(content);
            content = NULL;
        }
        rtsp_log("rtsp status code:%d", this->response->status_code);
        return this->response->status_code == 404 ? -2 : -1;
    }
    if (!content)
    {
        rtsp_log("rtsp content is null");
        return -1;
    }

    sdp_parse_result_t sdp_result;
    ret = parse_sdp(&sdp_result, (const char *)content);
    if (sdp_result.control)
    {
        // this->setup_uri = (char *)calloc(strlen(this->control_uri) + strlen(sdp_result.control) + 1, 1);
        // sprintf(this->setup_uri, "%s/%s", this->control_uri, sdp_result.control);
        _strlcpy(this->setup_uri, this->control_uri, sizeof(this->setup_uri));
        _strlcat(this->setup_uri, "/", sizeof(this->setup_uri));
        _strlcat(this->setup_uri, sdp_result.control, sizeof(this->setup_uri));
        free(sdp_result.control);
        sdp_result.control = NULL;
    }

    if (content)
    {
        free(content);
        content = NULL;
    }

    if (ret < 0)
    {
        rtsp_log("sdp parse failed");
        return -1;
    }

    return 0;
}

static int rtsp_setup(rtsp_client_t *this)
{
    rtsp_log("rtsp setup");
    int interleave = 0;
    char cmd[MAX_URL_SIZE];
    char transport[1024];
    const char *trans_pref;

    if (this->transport == RTSP_TRANSPORT_RDT)
        trans_pref = "x-pn-tng";
    else if (this->transport == RTSP_TRANSPORT_RAW)
        trans_pref = "RAW/RAW";
    else
        trans_pref = "RTP/AVP";

    this->timeout = 60;

    // if (this->rtp_port_max - this->rtp_port_min >= 4)
    // {
    //     port_off = rand() % ((this->rtp_port_max - this->rtp_port_min) / 2);
    //     port_off -= port_off % 0x01;
    // }

    // if (lower_transport == RTSP_LOWER_TRANSPORT_UDP)
    // {
    //     // TODO RTSP over UDP not supported yet
    // }
    // else if (lower_transport == RTSP_LOWER_TRANSPORT_TCP)
    {
        //! WMS not support
        snprintf(transport, sizeof(transport) - 1, "%s/TCP;", trans_pref);
        // if (this->transport != RTSP_TRANSPORT_RDT)
        //     _strlcat(transport, "unicast;", sizeof(transport));
        snprintf(transport + strlen(transport), sizeof(transport) - strlen(transport) - 1, "interleaved=%d-%d", interleave, interleave + 1);
        interleave += 2;
    }
    // else if (lower_transport == RTSP_LOWER_TRANSPORT_UDP_MULTICAST)
    // {
    //     // TODO RTSP over UDP not supported yet
    // }

    snprintf(cmd, sizeof(cmd), "Transport: %s\r\n", transport);

    rtsp_cmd_send(this, "SETUP", this->setup_uri, cmd, this->response, NULL);
    if (this->response->status_code != RTSP_STATUS_OK || this->response->nb_transports != 1)
    {
        rtsp_log("rtsp status code:%d", this->response->status_code);
        return -1;
    }

    // TODO 目前不对其它情况做处理
    this->lower_transport = this->response->transports[0].lower_transport;
    this->transport = this->response->transports[0].transport;

    switch (this->response->transports[0].transport)
    {
    case RTSP_LOWER_TRANSPORT_TCP:
        /** interleave ids, if TCP transport; each TCP/RTSP data packet starts
         * with a '$', stream length and stream ID. If the stream ID is within
         * the range of this interleaved_min-max, then the packet belongs to
         * this stream. */
        // stream.interleaved_min = this->response->transports[0].interleaved_min;
        // stream.interleaved_max = this->response->transports[0].interleaved_max;
        break;
    case RTSP_LOWER_TRANSPORT_UDP:
        // TODO RTSP over UDP not supported yet
        break;
    case RTSP_LOWER_TRANSPORT_UDP_MULTICAST:
        // TODO RTSP over UDP not supported yet
        break;
    default:
        break;
    }

    if (this->response->timeout > 0)
        this->timeout = this->response->timeout;

    // if(this->server_type==RTSP_SERVER_REAL)
    // {
    //     //TODO
    // }
    return 0;
}

static void rtsp_rtp_parse_thread(void *arg)
{
    rtsp_client_t *this = (rtsp_client_t *)arg;
    ebs_event_t event = {0};
    rtsp_log("rtsp rtp parse thread start");
    while (this->state == RTSP_STATE_STREAMING)
    {
        if (0 == ebs_msg_queue_wait(this->queue, (uint8_t *)&event, sizeof(ebs_event_t), 20))
        {
            switch (event.id)
            {
            case RTSP_EVENT_DATA:
            {
                if (event.param1 && event.param2)
                {
                    uint8_t *buf = (uint8_t *)event.param1;
                    int len = (int)(event.param2);
                    if (this->rtp_callback)
                        this->rtp_callback(buf, len, this->user_arg);
                    free((void *)event.param1);
                }
            }
            break;
            case RTSP_EVENT_END:
                goto exit;
                break;
            default:
                break;
            }
        }
        if (this->cmd_callback)
            this->cmd_callback(this->user_arg);
    }
exit:
    rtsp_log("rtsp rtp parse thread exit");
    if (this->rtp_parse_task)
    {
        ql_task_t task = this->rtp_parse_task;
        this->rtp_parse_task = NULL;
        ql_rtos_task_delete(task);
    }
}
#define RTSP_PLAY_TYPE 1
#if RTSP_PLAY_TYPE
static void rtsp_rtp_recv_thread(void *arg)
{
    rtsp_client_t *this = (rtsp_client_t *)arg;
    char cmd[MAX_URL_SIZE] = {0};
    int len = 0, ret = 0, timeout = 0, rtp_len = 0;
    uint8_t buf[4] = {0};
    ebs_event_t event = {0};
    rtsp_log("rtsp rtp recv thread start");

    if (this->lower_transport == RTSP_LOWER_TRANSPORT_UDP)
    {
        // TODO RTSP over UDP not supported yet
    }
    {
        if (this->state == RTSP_STATE_PAUSED)
        {
            cmd[0] = '\0';
        }
        else
        {
            snprintf(cmd, sizeof(cmd), "Range: 0.000-\r\n");
        }
        rtsp_cmd_send(this, "PLAY", this->control_uri, cmd, this->response, NULL);
        if (this->response->status_code != RTSP_STATUS_OK)
        {
            rtsp_log("rtsp status code:%d", this->response->status_code);
            goto end;
        }
        this->state = RTSP_STATE_STREAMING;
        QlOSStatus err = ql_rtos_task_create(&this->rtp_parse_task, 20 * 1024, APP_PRIORITY_HIGH, "rtsp rtp data parse", rtsp_rtp_parse_thread, (void *)this, 5);
        if (err != QL_OSI_SUCCESS)
        {
            rtsp_log("rtsp rtp parse task create failed:%d", err);
            goto end;
        }
        while (this->state == RTSP_STATE_STREAMING)
        {
            while (1)
            {
                rtsp_response_t reply = {0};
                ret = rtsp_read_reply(this, &reply, NULL, 1, NULL);
                if (ret < 0)
                    goto end;
                if (ret == 1)
                    break;
                if (this->state != RTSP_STATE_STREAMING)
                {
                    ret = 0;
                    goto end;
                }
            }

            ret = rtsp_select_recv(this->rtsp_fd, buf, 3, 3000);
            if (ret != 3)
            {
                rtsp_log("rtsp recv interleaved data error");
                ret = -1;
                goto end;
            }

            rtp_len = (int)(buf[1] << 8 | buf[2]);
            if (rtp_len > 81920 || rtp_len < 8)
                continue;

            this->recv_buf = NULL;
            this->recv_buf = (uint8_t *)malloc(rtp_len);
            if (this->recv_buf == NULL)
            {
                rtsp_log("rtsp recv buf realloc failed");
                ret = -1;
                goto end;
            }

            len = 0;
            timeout = 0;
            do
            {
                ret = rtsp_select_recv(this->rtsp_fd, this->recv_buf + len, rtp_len > len ? rtp_len - len : 0, 3000);
                if (ret < 0 || this->rtsp_fd < 0)
                {
                    rtsp_log("rtsp recv interleaved data error");
                    ret = -1;
                    goto end;
                }
                len += ret;
            } while (len != rtp_len && timeout++ < 2);

            if (rtp_len != len)
            {
                free(this->recv_buf);
                this->recv_buf = NULL;
                rtsp_log("rtsp recv len:%d wait len:%d ", rtp_len, len);
                ret = -1;
                goto end;
            }
            else if (this->queue)
            {
                ebs_mutex_lock(&this->mutex);
                event.id = RTSP_EVENT_DATA;
                event.param1 = (uint32_t)this->recv_buf;
                this->recv_buf = NULL;
                ebs_mutex_unlock(&this->mutex);
                event.param2 = (uint32_t)len;
                if (ebs_msg_queue_send(this->queue, (uint8_t *)&event, sizeof(ebs_event_t), 10) < 0)
                {
                    // rtsp_log("ebs_msg_queue_send failed");
                    // free(this->recv_buf);
                    free((void *)event.param1);
                    // this->recv_buf = NULL;
                }
            }
        }
    }

end:
    rtsp_log("rtsp rtp recv thread exit");
    this->state = RTSP_STATE_IDLE;
    if (this->rtp_recv_task)
    {
        ql_task_t task = this->rtp_recv_task;
        this->rtp_recv_task = NULL;
        ql_rtos_task_delete(task);
    }
}

static int rtsp_play(rtsp_client_t *this)
{
    ql_task_status_t status;
    if (this->rtp_recv_task)
    {
        ql_rtos_task_get_status(this->rtp_recv_task, &status);
        return status.eCurrentState > Blocked;
    }

    rtsp_log("rtsp rtp recv task create");
    ebs_mutex_init(&this->mutex);
    QlOSStatus err = QL_OSI_SUCCESS;
    err = ql_rtos_task_create(&this->rtp_recv_task, 40 * 1024, APP_PRIORITY_HIGH, "rtsp rtp recv thread", rtsp_rtp_recv_thread, (void *)this, 5);
    if (err != QL_OSI_SUCCESS)
    {
        rtsp_log("rtsp_play_recv_data_thread create failed");
        return -1;
    }
    // err = ql_rtos_task_create(&this->rtp_parse_task, 20 * 1024, APP_PRIORITY_HIGH, "rtsp_rtp_parse_thread", rtsp_rtp_parse_thread, (void *)this, 5);
    // if (err != QL_OSI_SUCCESS)
    // {
    //     rtsp_log("rtsp_rtp_parse_thread create failed");
    //     return -1;
    // }
    rtsp_log("=====rtsp play task success=====");
    return 0;
}
#else
static int rtsp_play(rtsp_client_t *this)
{
    rtsp_log("rtsp play");
    char cmd[MAX_URL_SIZE] = {0};
    int len = 0, ret = 0, timeout = 0, rtp_len = 0;
    uint8_t buf[4] = {0};
    ebs_event_t event = {0};
    QlOSStatus err = QL_OSI_SUCCESS;
    // this->nb_byes = 0;

    if (this->lower_transport == RTSP_LOWER_TRANSPORT_UDP)
    {
        // TODO RTSP over UDP not supported yet
    }
    // if(!(this->server_type==RTSP_SERVER_REAL&&this->need_subscription))
    {
        // if (this->transport == RTSP_TRANSPORT_RTP)
        // {

        // }

        if (this->state == RTSP_STATE_PAUSED)
        {
            cmd[0] = '\0';
        }
        else
        {
            snprintf(cmd, sizeof(cmd), "Range: 0.000-\r\n");
        }
        rtsp_cmd_send(this, "PLAY", this->control_uri, cmd, this->response, NULL);
        if (this->response->status_code != RTSP_STATUS_OK)
        {
            rtsp_log("rtsp status code:%d", this->response->status_code);
            return -1;
        }
    }
    this->state = RTSP_STATE_STREAMING;
    err = ql_rtos_task_create(&this->rtp_parse_task, 20 * 1024, APP_PRIORITY_HIGH, "rtsp_rtp_parse_thread", rtsp_rtp_parse_thread, (void *)this, 5);
    if (err != QL_OSI_SUCCESS)
    {
        rtsp_log("rtsp_rtp_parse_thread create failed");
        return -1;
    }
    rtsp_log("rtsp play start");
    while (this->state == RTSP_STATE_STREAMING)
    {
        while (1)
        {
            rtsp_response_t reply = {0};
            ret = rtsp_read_reply(this, &reply, NULL, 1, NULL);
            if (ret < 0)
                goto end;
            if (ret == 1)
                break;
            if (this->state != RTSP_STATE_STREAMING)
            {
                ret = 0;
                goto end;
            }
        }

        // ret = recv(this->rtsp_fd, buf, 3, 0);
        ret = rtsp_select_recv(this->rtsp_fd, buf, 3, 3000);
        if (ret != 3)
        {
            rtsp_log("rtsp recv interleaved data error");
            ret = -1;
            goto end;
        }

        // id = buf[0];
        rtp_len = (int)(buf[1] << 8 | buf[2]);
        if (rtp_len > 81920 || rtp_len < 8)
            continue;

        this->recv_buf = NULL;
        this->recv_buf = (uint8_t *)malloc(rtp_len);
        if (this->recv_buf == NULL)
        {
            rtsp_log("rtsp recv buf realloc failed");
            ret = -1;
            goto end;
        }

        // ret = rtsp_select_recv(this->rtsp_fd, this->recv_buf, len, 5000);
        len = 0;
        timeout = 0;
        do
        {
            ret = rtsp_select_recv(this->rtsp_fd, this->recv_buf + len, rtp_len > len ? rtp_len - len : 0, 3000);
            if (ret < 0 || this->rtsp_fd < 0)
            {
                rtsp_log("rtsp recv interleaved data error");
                ret = -1;
                goto end;
            }
            len += ret;
        } while (len != rtp_len && timeout++ < 2);

        if (rtp_len != len)
        {
            free(this->recv_buf);
            rtsp_log("rtsp recv len:%d wait len:%d ", rtp_len, len);
            ret = -1;
            goto end;
        }
        else if (this->queue)
        {
            ebs_mutex_lock(&this->mutex);
            event.id = RTSP_EVENT_DATA;
            event.param1 = (uint32_t)this->recv_buf;
            this->recv_buf = NULL;
            ebs_mutex_unlock(&this->mutex);
            event.param2 = (uint32_t)len;
            if (ebs_msg_queue_send(this->queue, (uint8_t *)&event, sizeof(ebs_event_t), 10) < 0)
            {
                rtsp_log("ebs_msg_queue_send failed");
                // free(this->recv_buf);
                free((void *)event.param1);
                // this->recv_buf = NULL;
            }
        }
        // if (this->cmd_callback)
        //     this->cmd_callback(this->user_arg);
    }

end:
    rtsp_log("rtsp play end");
    this->state = RTSP_STATE_IDLE;
    return ret;
}
#endif

static int rtsp_pause(rtsp_client_t *this)
{
    rtsp_log("rtsp pause");
    if (this->state != RTSP_STATE_STREAMING)
    {
        return 0;
    }

    rtsp_cmd_send(this, "PAUSE", this->control_uri, NULL, this->response, NULL);
    if (this->response->status_code != RTSP_STATUS_OK)
    {
        rtsp_log("rtsp status code:%d", this->response->status_code);
        return -1;
    }
    this->state = RTSP_STATE_PAUSED;
    return 0;
}

static int rtsp_teardown(rtsp_client_t *this)
{
    rtsp_log("rtsp teardown");
    if (this->state != RTSP_STATE_STREAMING)
    {
        return 0;
    }
    this->state = RTSP_STATE_IDLE;

    if (this->rtp_parse_task)
    {
        ql_rtos_task_delete(this->rtp_parse_task);
        this->rtp_parse_task = NULL;
    }

#if RTSP_PLAY_TYPE
    if (this->rtp_recv_task)
    {
        ql_rtos_task_delete(this->rtp_recv_task);
        this->rtp_recv_task = NULL;
    }
#endif
    rtsp_cmd_send(this, "TEARDOWN", this->control_uri, NULL, this->response, NULL);
    if (this->response->status_code != RTSP_STATUS_OK)
    {
        rtsp_log("rtsp status code:%d", this->response->status_code);
        return -1;
    }
    rtsp_disconnect(this);
    rtsp_log("rtsp teardown success");
    return 0;
}

// static rtsp_response_t s_rtsp_response = {
//     .seq = 0,
// };

// rtsp_client_t lt_rtsp_client = {
//     .cconnect = rtsp_connect,
//     .options = rtsp_options,
//     .describe = rtsp_describe,
//     .setup = rtsp_setup,
//     .play = rtsp_play,
//     .pause = rtsp_pause,
//     .teardown = rtsp_teardown,
//     .state = RTSP_STATE_IDLE,
//     .control_uri[0] = 0,
//     .rtp_port_min = 5000,
//     .rtp_port_max = 65535,
//     .seq = 0,
//     .url = NULL,
//     .rtsp_fd = -1,
//     .session_id[0] = 0,
//     .is_redirect = false,
//     .control_uri[0] = 0,
//     .response = &s_rtsp_response,
//     .packet = NULL,
// };

rtsp_client_t *lt_rtsp_client_init(void)
{
    rtsp_client_t *this = (rtsp_client_t *)calloc(sizeof(rtsp_client_t), 1);
    if (this == NULL)
    {
        rtsp_log("malloc failed");
        return NULL;
    }

    this->response = (rtsp_response_t *)calloc(sizeof(rtsp_response_t), 1);
    if (this->response == NULL)
    {
        rtsp_log("malloc failed");
        free(this);
        this = NULL;
        return NULL;
    }

    if (ebs_msg_queue_creat(&this->queue, sizeof(ebs_event_t), 20) != 0)
    {
        rtsp_log("ebs_msg_queue_creat failed");
        free(this->response);
        this->response = NULL;
        free(this);
        this = NULL;
        return NULL;
    }

    this->cconnect = rtsp_connect;
    this->options = rtsp_options;
    this->describe = rtsp_describe;
    this->setup = rtsp_setup;
    this->play = rtsp_play;
    this->pause = rtsp_pause;
    this->teardown = rtsp_teardown;
    this->cmd_callback = NULL;
    this->rtp_callback = NULL;

    this->seq = 0;
    this->state = RTSP_STATE_IDLE;
    this->session_id[0] = '\0';
    this->url = NULL;
    // this->nb_byes = 0;
    this->control_uri[0] = '\0';
    // this->setup_uri = NULL;
    this->setup_uri[0] = '\0';
    this->user_arg = NULL;
    return this;
}

int lt_rtsp_client_destory(rtsp_client_t **client_ptr)
{
    rtsp_log("rtsp client destory");
    if (client_ptr == NULL)
        return 0;

    rtsp_client_t *this = *client_ptr;
    if (this == NULL)
        return 0;

    rtsp_disconnect(this);

    if (this->response)
    {
        free(this->response);
        this->response = NULL;
    }

    if (this->queue)
    {
        uint32_t cnt = 0;
        do
        {
            ebs_msg_queue_get_cnt(this->queue, &cnt);
            if (cnt > 0)
            {
                ebs_event_t event = {0};
                ebs_msg_queue_wait(this->queue, (uint8_t *)&event, sizeof(ebs_event_t), 5);
                if (event.param1)
                {
                    free((void *)event.param1);
                }
            }
        } while (cnt > 0);
        ebs_msg_queue_delete(this->queue);
        this->queue = NULL;
    }
    if (this->url)
    {
        free(this->url);
        this->url = NULL;
    }
    // if (this->setup_uri)
    // {
    //     free(this->setup_uri);
    //     this->setup_uri = NULL;
    // }
    if (this->recv_buf)
    {
        free(this->recv_buf);
        this->recv_buf = NULL;
    }
    if (this->mutex)
        ebs_mutex_destroy(&this->mutex);
    this->user_arg = NULL;
    this->rtp_recv_task = NULL;
    this->rtp_parse_task = NULL;
    this->cmd_callback = NULL;
    this->rtp_callback = NULL;
    free(this);
    *client_ptr = NULL;
    rtsp_log("rtsp client destory success");
    return 0;
}
