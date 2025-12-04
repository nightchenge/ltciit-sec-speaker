/**
 * @file lt_rtspcodes.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-09-07
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef __LT_RTSPCODES_H__
#define __LT_RTSPCODES_H__
enum RTSPStatusCode
{
    RTSP_STATUS_CONTINUE = 100,
    RTSP_STATUS_OK = 200,
    RTSP_STATUS_CREATED = 201,
    RTSP_STATUS_LOW_ON_STORAGE_SPACE = 250,
    RTSP_STATUS_MULTIPLE_CHOICES = 300,
    RTSP_STATUS_MOVED_PERMANENTLY = 301,
    RTSP_STATUS_MOVED_TEMPORARILY = 302,
    RTSP_STATUS_SEE_OTHER = 303,
    RTSP_STATUS_NOT_MODIFIED = 304,
    RTSP_STATUS_USE_PROXY = 305,
    RTSP_STATUS_BAD_REQUEST = 400,
    RTSP_STATUS_UNAUTHORIZED = 401,
    RTSP_STATUS_PAYMENT_REQUIRED = 402,
    RTSP_STATUS_FORBIDDEN = 403,
    RTSP_STATUS_NOT_FOUND = 404,
    RTSP_STATUS_METHOD = 405,
    RTSP_STATUS_NOT_ACCEPTABLE = 406,
    RTSP_STATUS_PROXY_AUTH_REQUIRED = 407,
    RTSP_STATUS_REQ_TIME_OUT = 408,
    RTSP_STATUS_GONE = 410,
    RTSP_STATUS_LENGTH_REQUIRED = 411,
    RTSP_STATUS_PRECONDITION_FAILED = 412,
    RTSP_STATUS_REQ_ENTITY_2LARGE = 413,
    RTSP_STATUS_REQ_URI_2LARGE = 414,
    RTSP_STATUS_UNSUPPORTED_MTYPE = 415,
    RTSP_STATUS_PARAM_NOT_UNDERSTOOD = 451,
    RTSP_STATUS_CONFERENCE_NOT_FOUND = 452,
    RTSP_STATUS_BANDWIDTH = 453,
    RTSP_STATUS_SESSION = 454,
    RTSP_STATUS_STATE = 455,
    RTSP_STATUS_INVALID_HEADER_FIELD = 456,
    RTSP_STATUS_INVALID_RANGE = 457,
    RTSP_STATUS_RONLY_PARAMETER = 458,
    RTSP_STATUS_AGGREGATE = 459,
    RTSP_STATUS_ONLY_AGGREGATE = 460,
    RTSP_STATUS_TRANSPORT = 461,
    RTSP_STATUS_UNREACHABLE = 462,
    RTSP_STATUS_INTERNAL = 500,
    RTSP_STATUS_NOT_IMPLEMENTED = 501,
    RTSP_STATUS_BAD_GATEWAY = 502,
    RTSP_STATUS_SERVICE = 503,
    RTSP_STATUS_GATEWAY_TIME_OUT = 504,
    RTSP_STATUS_VERSION = 505,
    RTSP_STATUS_UNSUPPORTED_OPTION = 551,
};

static const __attribute__((unused)) char *const rtsp_status_strings[] = {
    [RTSP_STATUS_CONTINUE] = "Continue",
    [RTSP_STATUS_OK] = "OK",
    [RTSP_STATUS_CREATED] = "Created",
    [RTSP_STATUS_LOW_ON_STORAGE_SPACE] = "Low on Storage Space",
    [RTSP_STATUS_MULTIPLE_CHOICES] = "Multiple Choices",
    [RTSP_STATUS_MOVED_PERMANENTLY] = "Moved Permanently",
    [RTSP_STATUS_MOVED_TEMPORARILY] = "Moved Temporarily",
    [RTSP_STATUS_SEE_OTHER] = "See Other",
    [RTSP_STATUS_NOT_MODIFIED] = "Not Modified",
    [RTSP_STATUS_USE_PROXY] = "Use Proxy",
    [RTSP_STATUS_BAD_REQUEST] = "Bad Request",
    [RTSP_STATUS_UNAUTHORIZED] = "Unauthorized",
    [RTSP_STATUS_PAYMENT_REQUIRED] = "Payment Required",
    [RTSP_STATUS_FORBIDDEN] = "Forbidden",
    [RTSP_STATUS_NOT_FOUND] = "Not Found",
    [RTSP_STATUS_METHOD] = "Method Not Allowed",
    [RTSP_STATUS_NOT_ACCEPTABLE] = "Not Acceptable",
    [RTSP_STATUS_PROXY_AUTH_REQUIRED] = "Proxy Authentication Required",
    [RTSP_STATUS_REQ_TIME_OUT] = "Request Time-out",
    [RTSP_STATUS_GONE] = "Gone",
    [RTSP_STATUS_LENGTH_REQUIRED] = "Length Required",
    [RTSP_STATUS_PRECONDITION_FAILED] = "Precondition Failed",
    [RTSP_STATUS_REQ_ENTITY_2LARGE] = "Request Entity Too Large",
    [RTSP_STATUS_REQ_URI_2LARGE] = "Request URI Too Large",
    [RTSP_STATUS_UNSUPPORTED_MTYPE] = "Unsupported Media Type",
    [RTSP_STATUS_PARAM_NOT_UNDERSTOOD] = "Parameter Not Understood",
    [RTSP_STATUS_CONFERENCE_NOT_FOUND] = "Conference Not Found",
    [RTSP_STATUS_BANDWIDTH] = "Not Enough Bandwidth",
    [RTSP_STATUS_SESSION] = "Session Not Found",
    [RTSP_STATUS_STATE] = "Method Not Valid in This State",
    [RTSP_STATUS_INVALID_HEADER_FIELD] = "Header Field Not Valid for Resource",
    [RTSP_STATUS_INVALID_RANGE] = "Invalid Range",
    [RTSP_STATUS_RONLY_PARAMETER] = "Parameter Is Read-Only",
    [RTSP_STATUS_AGGREGATE] = "Aggregate Operation no Allowed",
    [RTSP_STATUS_ONLY_AGGREGATE] = "Only Aggregate Operation Allowed",
    [RTSP_STATUS_TRANSPORT] = "Unsupported Transport",
    [RTSP_STATUS_UNREACHABLE] = "Destination Unreachable",
    [RTSP_STATUS_INTERNAL] = "Internal Server Error",
    [RTSP_STATUS_NOT_IMPLEMENTED] = "Not Implemented",
    [RTSP_STATUS_BAD_GATEWAY] = "Bad Gateway",
    [RTSP_STATUS_SERVICE] = "Service Unavailable",
    [RTSP_STATUS_GATEWAY_TIME_OUT] = "Gateway Time-out",
    [RTSP_STATUS_VERSION] = "RTSP Version not Supported",
    [RTSP_STATUS_UNSUPPORTED_OPTION] = "Option not supported",
};

#define RTSP_STATUS_CODE2STRING(x)                                                                               \
    (x >= 100 && x < (sizeof(rtsp_status_strings) / sizeof((rtsp_status_strings)[0])) && rtsp_status_strings[x]) \
        ? rtsp_status_strings[x]                                                                                 \
        : NULL

enum RTSPMethod
{
    DESCRIBE,
    ANNOUNCE,
    OPTIONS,
    SETUP,
    PLAY,
    PAUSE,
    TEARDOWN,
    GET_PARAMETER,
    SET_PARAMETER,
    REDIRECT,
    RECORD,
    UNKNOWN = -1,
};

#endif //__LT_RTSPCODES_H__