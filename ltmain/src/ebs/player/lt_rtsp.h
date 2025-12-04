/**
 * @file lt_rtsp.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-09-07
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef __LT_RTSP_H__
#define __LT_RTSP_H__
#include <stdint.h>
#include <stdbool.h>

#include "ebs.h"
#include "util_msg.h"
#include "util_event.h"
#include "util_mutex.h"

#include "lt_rtspcodes.h"
#include "lt_sdp_parse.h"

enum RTSPLowerTransport
{
    RTSP_LOWER_TRANSPORT_UDP = 0,
    RTSP_LOWER_TRANSPORT_TCP = 1,
    RTSP_LOWER_TRANSPORT_UDP_MULTICAST = 2,
    RTSP_LOWER_TRANSPORT_NB,
    RTSP_LOWER_TRANSPORT_HTTP = 8,

    RTSP_LOWER_TRANSPORT_HTTPS,
    RTSP_LOWER_TRANSPORT_CUSTOM = 16,
};

enum RTSPTransport
{
    RTSP_TRANSPORT_RTP,
    RTSP_TRANSPORT_RDT,
    RTSP_TRANSPORT_RAW,
    RTSP_TRANSPORT_NB,
};

enum RTSPControlTransport
{
    RTSP_MODE_PLAIN,
    RTSP_MODE_TUNNEL,
};

#define RTSP_DEFAULT_PORT 554
#define RTSPS_DEFAULT_PORT 322
#define RTSP_MAX_TRANSPORTS 8
#define RTSP_DEFAULT_AUDIO_SAMPLERATE 44100
#define RTSP_RTP_PORT_MIN 5000
#define RTSP_RTP_PORT_MAX 65000
// #define SDP_MAX_SIZE 16384
#define SDP_MAX_SIZE 2048
#define MAX_URL_SIZE 4096

// typedef struct RTSPTransportField
// {
//     int interleaved_min, interleaved_max;
//     int port_min, port_max;
//     int client_port_min, client_port_max;
//     int server_port_min, server_port_max;

//     int ttl;
//     int mode_record;
//     // struct sockaddr_storage destination;
//     char source[46 + 1];
//     enum RTSPTransport transport;
//     enum RTSPLowerTransport lower_transport;
// } RTSPTransportField;

// typedef struct RTSPMessageHeader
// {
//     int content_length;
//     enum RTSPStatusCode status_code;

//     int nb_transports;

//     int64_t range_start, range_end;

//     RTSPTransportField transports[RTSP_MAX_TRANSPORTS];
//     int seq;
//     char ssession_id[512];
//     char location[4096];
//     char real_challenge[64];
//     char server[64];
//     int timeout;
//     int notice;
//     char reason[256];
//     char content_type[64];
//     char stream_id[64];
// } RTSPMessageHeader;

enum RTSPClientState
{
    RTSP_STATE_IDLE,
    RTSP_STATE_STREAMING,
    RTSP_STATE_PAUSED,
    RTSP_STATE_SEEKING,
};

enum RTSPServerType
{
    RTSP_SERVER_RTP,
    RTSP_SERVER_REAL,
    RTSP_SERVER_WMS,
    RTSP_SERVER_SATIP,
    RTSP_SERVER_NB,
};

enum RTSPEventType
{
    RTSP_EVENT_DATA = 1001,
    RTSP_EVENT_END,
};

#define RTSP_FLAG_FILTER_SRC 0x1
#define RTSP_FLAG_LISTEN 0x2
#define RTSP_FLAG_CUSTOM_IO 0x4
#define RTSP_FLAG_RTCP_TO_SOURCE 0x8
#define RTSP_FLAG_PREFER_TCP 0x10
#define RTSP_FLAG_SATIP_RAW 0x20

typedef struct rtsp_transport_field
{
    /** interleave ids, if TCP transport; each TCP/RTSP data packet starts
     * with a '$', stream length and stream ID. If the stream ID is within
     * the range of this interleaved_min-max, then the packet belongs to
     * this stream. */
    int interleaved_min, interleaved_max;

    /** UDP multicast port range; the ports to which we should connect to
     * receive multicast UDP data. */
    int port_min, port_max;

    /** UDP client ports; these should be the local ports of the UDP RTP
     * (and RTCP) sockets over which we receive RTP/RTCP data. */
    int client_port_min, client_port_max;

    /** UDP unicast server port range; the ports to which we should connect
     * to receive unicast UDP RTP/RTCP data. */
    int server_port_min, server_port_max;

    /** time-to-live value (required for multicast); the amount of HOPs that
     * packets will be allowed to make before being discarded. */
    int ttl;

    /** transport set to record data */
    int mode_record;

    // struct sockaddr_storage destination; /**< destination IP address */
    char source[46 + 1]; /**< source IP address */

    /** data/packet transport protocol; e.g. RTP or RDT */
    enum RTSPTransport transport;

    /** network layer transport protocol; e.g. TCP or UDP uni-/multicast */
    enum RTSPLowerTransport lower_transport;
} rtsp_transport_field;

typedef struct rtsp_response
{
    int content_length;

    enum RTSPStatusCode status_code; /** response code from server */

    int nb_transports;              /** number of items in the 'transports' variable below */
    int64_t range_start, range_end; /** Time range of the streams that the server will stream */

    rtsp_transport_field transports[RTSP_MAX_TRANSPORTS]; /** describe the complete "Transport:" line of the server in response to a SETUP RTSP command by the client*/

    int seq;
    char session_id[512];

    char location[4096]; /** the "Location:" field. This value is used to handle redirection*/

    char real_challenge[64]; /** the "RealChallenge1:" filed from server*/
    char server[64];
    int timeout;
    int notice;
    char reason[256];
    char content_type[64];
    char stream_id[64];
} rtsp_response_t;

typedef struct rtsp_client
{
    int (*cconnect)(struct rtsp_client *this, const char *url);  // 与RTSP服务器建立连接
    int (*options)(struct rtsp_client *this);                    // 发送OPTIONS命令获取RTSP服务器支持的命令
    int (*describe)(struct rtsp_client *this);                   // 获取媒体流的描述信息
    int (*setup)(struct rtsp_client *this);                      // 设置媒体流的传输参数
    int (*play)(struct rtsp_client *this);                       // 发送播放命令开始媒体流播放
    int (*pause)(struct rtsp_client *this);                      // 发送暂停命令暂停媒体流播放
    int (*teardown)(struct rtsp_client *this);                   // 发送停止命令关闭媒体流连接
    int (*cmd_callback)(void *);                                 // 命令回调函数,用于接收用户发送过来的命令
    int (*rtp_callback)(uint8_t *buf, int size, void *user_arg); // 接收和处理RTP包//!< 该函数需要用户实现()

    enum RTSPClientState state;

    char *url; /** RTSP url */

    bool is_redirect; /** whether the server has redirected us to another url */

    int seq;              /**< RTSP command sequence number*/
    char session_id[512]; /** copy of rtsp_response->session_id*/
    int timeout;          /** copy of rtsp_response->timeout*/

    int64_t last_cmd_time;                   /** timestamp of the last RTSP command that client sent to the RTSP server*/
    enum RTSPTransport transport;            /** the negotiated data/packet transport protocol; e.g. RTP or RDT*/
    enum RTSPLowerTransport lower_transport; /** the negotiated network layer transport protocol; e.g. TCP or UDP*/
    enum RTSPServerType server_type;
    char real_challenge[64]; /** the "RealChallenge1:" field from the server */

    // char auth[128];        /** plaintext authorization line (username:password)*/ //! not supported yet
    //! auth_state;
    // char last_reply[2048]; /** The last replay of the server to a RTSP command*/

    char control_uri[4096];                      /** some MS RTSP streams contain a URL in the SDP, need to use for all subsequent RTSP requests, rather than the input URI*/
    char setup_uri[1024];                        /** the URI to send the SETUP command to*/
    enum RTSPControlTransport control_transport; /** RTSP transport mode, such as plain or tunneled */

    // int nb_byes; /** Number of RTCP BYE packets the RTSP seesion has received. An EOF is propagated back if nb_byes == nb_streams. This is reset after a seek. */

    uint8_t *recv_buf; /** Reusable buffer for receiving packets */

    int lower_transport_mask; /** A mask with all requested transport methods */
    // uint64_t packets;            /** The number of returned packets */
    // int get_parameter_supported; /** Whether the server supports the GET_PARAMETER method */
    // int rtp_port_min, rtp_port_max; /** Minimum and Maximum local UDP ports */
    // int64_t stimeout;          /** Timeout of socket i/o operations */
    // int reordering_queue_size; /** Size of RTP packet reordering queue */
    // int buffer_size;
    // int pkt_size;

    int rtsp_fd;

    void *user_arg;

    ql_task_t rtp_recv_task;
    ql_task_t rtp_parse_task;
    ebs_queue_t queue;
    ebs_mutex_t mutex;

    rtsp_response_t *response;
} rtsp_client_t;

rtsp_client_t *lt_rtsp_client_init(void);
int lt_rtsp_client_destory(rtsp_client_t **client_ptr);

#endif // __LT_RTSP_H__