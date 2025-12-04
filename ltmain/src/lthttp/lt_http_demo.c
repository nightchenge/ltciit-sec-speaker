/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2024-07-11 09:18:40
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-11-26 23:16:23
 */
/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2024-05-28 10:57:47
 * @LastEditors: zhouhao
 * @LastEditTime: 2024-05-28 10:57:47
 */
/*================================================================
  Copyright (c) 2020 Quectel Wireless Solution, Co., Ltd.  All Rights Reserved.
  Quectel Wireless Solution Proprietary and Confidential.
=================================================================*/
/*=================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

WHEN              WHO         WHAT, WHERE, WHY
------------     -------     -------------------------------------------------------------------------------

=================================================================*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_api_osi.h"

#include "ql_log.h"
#include "ql_api_datacall.h"
#include "ql_http_client.h"
#include "ql_fs.h"
#include "ql_sdmmc.h"
#define QL_HTTP_LOG_LEVEL           	QL_LOG_LEVEL_INFO
#define QL_HTTP_LOG_PUSH(msg, ...)	    QL_LOG_PUSH("lt_HTTP", msg, ##__VA_ARGS__)
#define QL_HTTP_LOG(msg, ...)			QL_LOG(QL_HTTP_LOG_LEVEL, "lt_HTTP", msg, ##__VA_ARGS__)

typedef enum{
	QHTTPC_EVENT_RESPONSE     	= 1001,
	QHTTPC_EVENT_END,
}qhttpc_event_code_e;

typedef struct
{
	http_client_t  			  	http_client;
	ql_queue_t 					queue;

	ql_mutex_t                	simple_lock;
	bool						dl_block;
	int							dl_high_line;
	int							dl_total_len;

	QFILE						upload_fd;
	QFILE						dload_fd;
}qhttpc_ctx_t;


#define 		HTTP_MAX_MSG_CNT		8
#define 		HTTP_DLOAD_HIGH_LINE	40960
ql_task_t 		http_task = NULL;
qhttpc_ctx_t 	http_demo_client = {0};

static void http_event_cb(http_client_t *client, int evt, int evt_code, void *arg)
{
	qhttpc_ctx_t *client_ptr = (qhttpc_ctx_t *)arg;
	ql_event_t qhttpc_event_send = {0};

	QL_HTTP_LOG("enter");
	
	if(client_ptr == NULL)
		return;
	
	QL_HTTP_LOG("*client:%d, http_cli:%d", *client, client_ptr->http_client);

	if(*client != client_ptr->http_client)
		return;
	QL_HTTP_LOG("evt:%d, evt_code:%d", evt, evt_code);
	switch(evt){
	case HTTP_EVENT_SESSION_ESTABLISH:{
			if(evt_code != QL_HTTP_OK){
				QL_HTTP_LOG("HTTP session create failed!!!!!");
				qhttpc_event_send.id = QHTTPC_EVENT_END;
				qhttpc_event_send.param1 = (uint32)client_ptr;
				ql_rtos_queue_release(client_ptr->queue, sizeof(ql_event_t), (uint8 *)&qhttpc_event_send, QL_WAIT_FOREVER);
			}
		}
		break;
	case HTTP_EVENT_RESPONE_STATE_LINE:{
			if(evt_code == QL_HTTP_OK){
				int resp_code = 0;
				int content_length = 0;
				int chunk_encode = 0;
				char *location = NULL;
				ql_httpc_getinfo(client, HTTP_INFO_RESPONSE_CODE, &resp_code);
				QL_HTTP_LOG("response code:%d", resp_code);
				ql_httpc_getinfo(client, HTTP_INFO_CHUNK_ENCODE, &chunk_encode);
				if(chunk_encode == 0){
					ql_httpc_getinfo(client, HTTP_INFO_CONTENT_LEN, &content_length);
					QL_HTTP_LOG("content_length:%d",content_length);
				}else{
					QL_HTTP_LOG("http chunk encode!!!");
				}

				if(resp_code >= 300 && resp_code < 400){
					ql_httpc_getinfo(client, HTTP_INFO_LOCATION, &location);
					QL_HTTP_LOG("redirect location:%s", location);
					free(location);
				}
			}
		}
		break;
	case HTTP_EVENT_SESSION_DISCONNECT:{
			if(evt_code == QL_HTTP_OK){
				QL_HTTP_LOG("===http transfer end!!!!");
			}else{
				QL_HTTP_LOG("===http transfer occur exception!!!!!");
			}			
			qhttpc_event_send.id = QHTTPC_EVENT_END;
			qhttpc_event_send.param1 = (uint32)client_ptr;
			ql_rtos_queue_release(client_ptr->queue, sizeof(ql_event_t), (uint8 *)&qhttpc_event_send, QL_WAIT_FOREVER);
		}
		break;
	}
}

static int http_write_response_data(http_client_t *client, void *arg, char *data, int size, unsigned char end)
{
	int ret = size;
	uint32 msg_cnt = 0;
	char *read_buff = NULL;
	qhttpc_ctx_t *client_ptr = (qhttpc_ctx_t *)arg;
	ql_event_t qhttpc_event_send = {0};

	QL_HTTP_LOG("enter");	
	
	if(client_ptr == NULL)
		return 0;
	
	QL_HTTP_LOG("*client:%d, http_cli:%d", *client, client_ptr->http_client);

	if(*client != client_ptr->http_client)
		return 0;

	// 为数据分配额外的1字节用于 '\0' 终止符，以防 API 请求需要打印
	read_buff = (char *)malloc(size+1);
	if(read_buff == NULL)
	{
		QL_HTTP_LOG("mem faild");
		return 0;
	}

	memcpy(read_buff, data, size);

	if(QL_OSI_SUCCESS != ql_rtos_queue_get_cnt(client_ptr->queue, &msg_cnt))
	{
		free(read_buff);
		QL_HTTP_LOG("ql_rtos_queue_get_cnt faild");
		return 0;
	}
	
	ql_rtos_mutex_lock(client_ptr->simple_lock, 100);
	if(msg_cnt >= (HTTP_MAX_MSG_CNT-1) || (client_ptr->dl_total_len + size) >= client_ptr->dl_high_line)
	{
		client_ptr->dl_block = true;
		ret = QL_HTTP_ERROR_WONDBLOCK;
	}
	ql_rtos_mutex_unlock(client_ptr->simple_lock);

	QL_HTTP_LOG("msg_cnt %d, total_len+size %d", msg_cnt, (client_ptr->dl_total_len + size));

	qhttpc_event_send.id = QHTTPC_EVENT_RESPONSE;
	qhttpc_event_send.param1 = (uint32)client_ptr;
	qhttpc_event_send.param2 = (uint32)read_buff;
	qhttpc_event_send.param3 = (uint32)size;
	if(QL_OSI_SUCCESS != ql_rtos_queue_release(client_ptr->queue, sizeof(ql_event_t), (uint8 *)&qhttpc_event_send, 0))
	{
		free(read_buff);
		QL_HTTP_LOG("ql_rtos_queue_release faild");
		return 0;
	}
	
	ql_rtos_mutex_lock(client_ptr->simple_lock, 100);
	client_ptr->dl_total_len += size;
	ql_rtos_mutex_unlock(client_ptr->simple_lock);
	
	QL_HTTP_LOG("http response :%d bytes data", size);
	
	return ret;
}
#if 0
static int http_read_request_data(http_client_t *client, void *arg, char *data, int size)
{
	int ret = 0;
	QFILE fd = 0;
	qhttpc_ctx_t *client_ptr = (qhttpc_ctx_t *)arg;
	
	QL_HTTP_LOG("enter");	
	
	if(client_ptr == NULL)
		return 0;
	
	QL_HTTP_LOG("*client:%d, http_cli:%d", *client, client_ptr->http_client);

	if(*client != client_ptr->http_client)
		return 0;

	ql_rtos_mutex_lock(client_ptr->simple_lock, 100);
	fd = client_ptr->upload_fd;
	ql_rtos_mutex_unlock(client_ptr->simple_lock);

	QL_HTTP_LOG("fd:%d", fd);	
	
	if(fd < 0)
		return 0;
	QL_HTTP_LOG("read size:%d", size);
	ret = ql_fread(data, size, 1, fd);
	QL_HTTP_LOG("http read :%d bytes data", ret);
	if(ret > 0)
		return ret;

	return 0;
}
#endif
extern  void api_mqtt_location_pulish(char *buf);
// 【修改点 1】修改数据处理函数，根据文件描述符判断是写入文件还是打印日志
static void http_write_response_data_func(void *param)
{
	int ret = 0;
	int size = 0;
	QFILE fd = 0;
	bool dload_block = false;
	char *read_buff = NULL;
	qhttpc_ctx_t *client_ptr = NULL;
	ql_event_t *qhttpc_event_recv = (ql_event_t *)param;

	if(qhttpc_event_recv == NULL || qhttpc_event_recv->param1 == 0 || qhttpc_event_recv->param2 == 0 || qhttpc_event_recv->param3 == 0)
		return;

	client_ptr = (qhttpc_ctx_t *)qhttpc_event_recv->param1;
	read_buff = (char *)qhttpc_event_recv->param2;
	size = (int)qhttpc_event_recv->param3;
	fd = (QFILE)client_ptr->dload_fd; // 根据 fd 状态判断操作类型

    // 如果 fd >= 0，则执行文件写入（原MP3下载功能）
    if (fd >= 0) {
        ret = ql_fwrite(read_buff, size, 1, fd);
        QL_HTTP_LOG("HTTP write to file: %d bytes data", ret);
    } else {
    // 如果 fd < 0，则执行 API 响应打印到日志（新增功能）
        // 确保数据以 '\0' 结尾，方便打印
        if (size > 0) {
             read_buff[size] = '\0';
             QL_HTTP_LOG("HTTP API Response: %s", read_buff);
			 api_mqtt_location_pulish(read_buff);
        } else {
             QL_HTTP_LOG("HTTP API Response: Empty data block");
        }
        ret = size; // 逻辑上认为处理了所有数据
    }

	free(read_buff);

	ql_rtos_mutex_lock(client_ptr->simple_lock, 100);
	client_ptr->dl_total_len -= size;
	if(client_ptr->dl_total_len < 0)
		client_ptr->dl_total_len = 0;
	if(client_ptr->dl_block == true && client_ptr->dl_total_len < client_ptr->dl_high_line)
	{
		dload_block = client_ptr->dl_block;
		client_ptr->dl_block = false;
	}
	ql_rtos_mutex_unlock(client_ptr->simple_lock);

	if(dload_block == true)
		ql_httpc_continue_dload(&client_ptr->http_client);

	QL_HTTP_LOG("http response data processed: %d bytes", ret); // 统一日志
}
#if 0
// ... (http_app_thread, 已被注释掉)
#endif

// 【修改点 2】新增消息类型，区分 API 请求和文件下载
typedef enum msg_type
{
	API_RECV_MSG = 0, // 新增：用于API请求，打印到日志
	FILE_RECV_MSG = 1, // 原有：用于文件下载，保存到文件
	SEND_MSG = 2
} msg_type_t;

typedef struct lt_http_msg
{
	msg_type_t type;
	char url[128];
    uint32_t datalen;
    uint8_t *data; // 用于存储文件路径/名称
} lt_http_msg_t;

typedef struct lt_http
{
    ql_task_t http_task;
    ql_task_t http_msg_task;
    int htpp_connected;
    ql_queue_t http_msg_queue;
} lt_http_t;
static lt_http_t lt_http = {0};
#if 1

// 【修改点 3】修改原下载函数，将类型设置为 FILE_RECV_MSG
void lt_http_dload_mp3( const char *file_name ,char *url)
{
	if(	ql_sdmmc_is_mount() == false)
	{
		QL_HTTP_LOG("sd is unmount");
		return;
	}else
	{
		QL_HTTP_LOG("sd is mount");
	}
	QlOSStatus err = QL_OSI_SUCCESS;
    lt_http_msg_t msg;
	msg.type = FILE_RECV_MSG; // 设置为文件下载类型
	QL_HTTP_LOG("url==%s\n",url);
    memset(msg.url, 0x00, sizeof(msg.url));
    memcpy(msg.url, url, strlen(url));
	msg.datalen = strlen(file_name)+strlen("SD:")+1;
	msg.data = malloc(msg.datalen);

	sprintf((char*)msg.data,"SD:%s",file_name);
	char mp3_name[255]={0};
	sprintf(mp3_name,"SD:%s",file_name);
	if (ql_file_exist(mp3_name) == QL_FILE_OK)
	{
		QL_HTTP_LOG("http download fail,mp3file eexsist!!");
		if(msg.data)
			free(msg.data);
		return ;
	}
	QL_HTTP_LOG("sg.data ==%s\n",msg.data);
	if ((err = ql_rtos_queue_release(lt_http.http_msg_queue, sizeof(lt_http_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
    {
        QL_HTTP_LOG("send msg to queue failed, err=%d", err);
		if(msg.data)
			free(msg.data);
    }
}

/*************************************************************************************** */

// 【新增】MD5上下文结构体
typedef struct 
{
    uint32_t state[4];        // 状态 (ABCD)
    uint32_t count[2];         // 位数计数，模2^64 (lsb first)
    unsigned char buffer[64]; // 输入缓冲区
} MD5_CTX;

// 【新增】MD5函数声明
static void MD5Init(MD5_CTX *context);
static void MD5Update(MD5_CTX *context, unsigned char *input, unsigned int inputLen);
static void MD5Final(unsigned char digest[16], MD5_CTX *context);
static void MD5Transform(uint32_t state[4], unsigned char block[64]);
static void Encode(unsigned char *output, uint32_t *input, unsigned int len);
static void Decode(uint32_t *output, unsigned char *input, unsigned int len);

// 【新增】MD5计算函数
static int calculate_file_md5(const char *filename, char *md5_str)
{
    if (!filename || !md5_str) 
	{
        QL_HTTP_LOG("MD5 calculate fail,filename is NULL");
        return -1;
    }
    
    // 检查文件是否存在
    if (ql_file_exist(filename) != QL_FILE_OK) 
	{
        QL_HTTP_LOG("MD5 calculate fail,file not exist: %s", filename);
        return -1;
    }
    
    // 打开文件
    QFILE fd = ql_fopen(filename, "rb");
    if (fd < 0) {
        QL_HTTP_LOG("MD5 calculate fail,open file fail: %s", filename);
        return -1;
    }
    
    MD5_CTX context;
    unsigned char buffer[1024];
    unsigned char digest[16];
    int bytes_read;
    
    // 初始化MD5上下文
    MD5Init(&context);
    
    // 读取文件并更新MD5
    while ((bytes_read = ql_fread(buffer, 1, sizeof(buffer), fd)) > 0) {
        MD5Update(&context, buffer, bytes_read);
    }
    
    // 获取最终的MD5摘要
    MD5Final(digest, &context);
    
    // 关闭文件
    ql_fclose(fd);
    
    // 将MD5摘要转换为十六进制字符串
    for (int i = 0; i < 16; i++) {
        sprintf(md5_str + (i * 2), "%02x", digest[i]);
    }
    md5_str[32] = '\0'; // 确保字符串以null结尾
    
    QL_HTTP_LOG("file: %s md5: %s", filename, md5_str);
    return 0;
}

// 【新增】MD5算法实现
static void MD5Init(MD5_CTX *context)
{
    context->count[0] = context->count[1] = 0;
    context->state[0] = 0x67452301;
    context->state[1] = 0xefcdab89;
    context->state[2] = 0x98badcfe;
    context->state[3] = 0x10325476;
}

static void MD5Update(MD5_CTX *context, unsigned char *input, unsigned int inputLen)
{
    unsigned int i, index, partLen;

    // 计算当前缓冲区中的字节数
    index = (unsigned int)((context->count[0] >> 3) & 0x3F);

    // 更新位数计数
    if ((context->count[0] += ((uint32_t)inputLen << 3)) < ((uint32_t)inputLen << 3))
        context->count[1]++;
    context->count[1] += ((uint32_t)inputLen >> 29);

    partLen = 64 - index;

    // 尽可能多地转换数据
    if (inputLen >= partLen) {
        memcpy(&context->buffer[index], input, partLen);
        MD5Transform(context->state, context->buffer);

        for (i = partLen; i + 63 < inputLen; i += 64)
            MD5Transform(context->state, &input[i]);

        index = 0;
    } else {
        i = 0;
    }

    // 缓冲剩余输入
    memcpy(&context->buffer[index], &input[i], inputLen - i);
}

static void MD5Final(unsigned char digest[16], MD5_CTX *context)
{
    unsigned char bits[8];
    unsigned int index, padLen;

    // 保存位数
    Encode(bits, context->count, 8);

    // 填充到56 mod 64
    index = (unsigned int)((context->count[0] >> 3) & 0x3f);
    padLen = (index < 56) ? (56 - index) : (120 - index);
    MD5Update(context, (unsigned char*)"\x80", 1);
    while (padLen-- > 1)
        MD5Update(context, (unsigned char*)"", 1);

    // 附加长度（在填充之前）
    MD5Update(context, bits, 8);

    // 存储状态到摘要中
    Encode(digest, context->state, 16);

    // 清零敏感信息
    memset(context, 0, sizeof(*context));
}

// 【新增】MD5转换函数（核心算法）
static void MD5Transform(uint32_t state[4], unsigned char block[64])
{
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];

    Decode(x, block, 64);

    // 定义MD5基本函数
    #define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
    #define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
    #define H(x, y, z) ((x) ^ (y) ^ (z))
    #define I(x, y, z) ((y) ^ ((x) | (~z)))
    
    #define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))
    
    // 定义MD5变换宏
    #define FF(a, b, c, d, x, s, ac) { \
        (a) += F((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    
    #define GG(a, b, c, d, x, s, ac) { \
        (a) += G((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    
    #define HH(a, b, c, d, x, s, ac) { \
        (a) += H((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    
    #define II(a, b, c, d, x, s, ac) { \
        (a) += I((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }

    // 第1轮（16步）
    FF(a, b, c, d, x[0],  7, 0xd76aa478);
    FF(d, a, b, c, x[1], 12, 0xe8c7b756);
    FF(c, d, a, b, x[2], 17, 0x242070db);
    FF(b, c, d, a, x[3], 22, 0xc1bdceee);
    FF(a, b, c, d, x[4],  7, 0xf57c0faf);
    FF(d, a, b, c, x[5], 12, 0x4787c62a);
    FF(c, d, a, b, x[6], 17, 0xa8304613);
    FF(b, c, d, a, x[7], 22, 0xfd469501);
    FF(a, b, c, d, x[8],  7, 0x698098d8);
    FF(d, a, b, c, x[9], 12, 0x8b44f7af);
    FF(c, d, a, b, x[10],17, 0xffff5bb1);
    FF(b, c, d, a, x[11],22, 0x895cd7be);
    FF(a, b, c, d, x[12], 7, 0x6b901122);
    FF(d, a, b, c, x[13],12, 0xfd987193);
    FF(c, d, a, b, x[14],17, 0xa679438e);
    FF(b, c, d, a, x[15],22, 0x49b40821);

    // 第2轮（16步）
    GG(a, b, c, d, x[1],  5, 0xf61e2562);
    GG(d, a, b, c, x[6],  9, 0xc040b340);
    GG(c, d, a, b, x[11],14, 0x265e5a51);
    GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);
    GG(a, b, c, d, x[5],  5, 0xd62f105d);
    GG(d, a, b, c, x[10], 9, 0x02441453);
    GG(c, d, a, b, x[15],14, 0xd8a1e681);
    GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);
    GG(a, b, c, d, x[9],  5, 0x21e1cde6);
    GG(d, a, b, c, x[14], 9, 0xc33707d6);
    GG(c, d, a, b, x[3], 14, 0xf4d50d87);
    GG(b, c, d, a, x[8], 20, 0x455a14ed);
    GG(a, b, c, d, x[13], 5, 0xa9e3e905);
    GG(d, a, b, c, x[2],  9, 0xfcefa3f8);
    GG(c, d, a, b, x[7], 14, 0x676f02d9);
    GG(b, c, d, a, x[12],20, 0x8d2a4c8a);

    // 第3轮（16步）
    HH(a, b, c, d, x[5],  4, 0xfffa3942);
    HH(d, a, b, c, x[8], 11, 0x8771f681);
    HH(c, d, a, b, x[11],16, 0x6d9d6122);
    HH(b, c, d, a, x[14],23, 0xfde5380c);
    HH(a, b, c, d, x[1],  4, 0xa4beea44);
    HH(d, a, b, c, x[4], 11, 0x4bdecfa9);
    HH(c, d, a, b, x[7], 16, 0xf6bb4b60);
    HH(b, c, d, a, x[10],23, 0xbebfbc70);
    HH(a, b, c, d, x[13], 4, 0x289b7ec6);
    HH(d, a, b, c, x[0], 11, 0xeaa127fa);
    HH(c, d, a, b, x[3], 16, 0xd4ef3085);
    HH(b, c, d, a, x[6], 23, 0x04881d05);
    HH(a, b, c, d, x[9],  4, 0xd9d4d039);
    HH(d, a, b, c, x[12],11, 0xe6db99e5);
    HH(c, d, a, b, x[15],16, 0x1fa27cf8);
    HH(b, c, d, a, x[2], 23, 0xc4ac5665);

    // 第4轮（16步）
    II(a, b, c, d, x[0],  6, 0xf4292244);
    II(d, a, b, c, x[7], 10, 0x432aff97);
    II(c, d, a, b, x[14],15, 0xab9423a7);
    II(b, c, d, a, x[5], 21, 0xfc93a039);
    II(a, b, c, d, x[12], 6, 0x655b59c3);
    II(d, a, b, c, x[3], 10, 0x8f0ccc92);
    II(c, d, a, b, x[10],15, 0xffeff47d);
    II(b, c, d, a, x[1], 21, 0x85845dd1);
    II(a, b, c, d, x[8],  6, 0x6fa87e4f);
    II(d, a, b, c, x[15],10, 0xfe2ce6e0);
    II(c, d, a, b, x[6], 15, 0xa3014314);
    II(b, c, d, a, x[13],21, 0x4e0811a1);
    II(a, b, c, d, x[4],  6, 0xf7537e82);
    II(d, a, b, c, x[11],10, 0xbd3af235);
    II(c, d, a, b, x[2], 15, 0x2ad7d2bb);
    II(b, c, d, a, x[9], 21, 0xeb86d391);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;

    // 清零敏感信息
    memset(x, 0, sizeof(x));
}

static void Encode(unsigned char *output, uint32_t *input, unsigned int len)
{
    unsigned int i, j;

    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[j] = (unsigned char)(input[i] & 0xff);
        output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
        output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
        output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
    }
}

static void Decode(uint32_t *output, unsigned char *input, unsigned int len)
{
    unsigned int i, j;

    for (i = 0, j = 0; j < len; i++, j += 4) {
        output[i] = ((uint32_t)input[j]) | (((uint32_t)input[j+1]) << 8) |
                   (((uint32_t)input[j+2]) << 16) | (((uint32_t)input[j+3]) << 24);
    }
}

// 【修改】更新MD5校验函数，使用新的MD5计算功能
int verify_file_md5(const char *filename, const char *expected_md5)
{
    if (!filename || !expected_md5 || strlen(expected_md5) == 0) 
	{
        QL_HTTP_LOG("MD5 check fail !");
        return -1;
    }
    
    QL_HTTP_LOG("file_name: %s, expected_md5: %s", filename, expected_md5);
    
    char calculated_md5[33] = {0};
    
    // 计算文件的MD5值
    if (calculate_file_md5(filename, calculated_md5) != 0) 
	{
        QL_HTTP_LOG("calculated MD5 fail");
        return -1;
    }
    
    QL_HTTP_LOG("calculated_md5: %s", calculated_md5);
    
    // 比较MD5值（不区分大小写）
    if (strcasecmp(calculated_md5, expected_md5) != 0) 
	{
        QL_HTTP_LOG("MD5 check fail ! expected_md5: %s, calculated_md5: %s", expected_md5, calculated_md5);
        return -1;
    }
    
    QL_HTTP_LOG("MD5 check success");

    return 0;
}

// 【新增函数】下载单片机固件
char lt_http_dload_ble( const char *file_name ,char *url ,char *md5)
{
	QlOSStatus err = QL_OSI_SUCCESS;
    lt_http_msg_t msg;
	msg.type = FILE_RECV_MSG; // 设置为文件下载类型
	QL_HTTP_LOG("url==%s\n",url);
    memset(msg.url, 0x00, sizeof(msg.url));
    memcpy(msg.url, url, strlen(url));
	msg.datalen = strlen(file_name)+strlen("UFS:")+1;
	msg.data = malloc(msg.datalen);

	sprintf((char*)msg.data,"UFS:%s",file_name);
	char ble_name[255]={0};
	sprintf(ble_name,"UFS:%s",file_name);
	if (ql_file_exist(ble_name) == QL_FILE_OK)
	{		
		ql_remove(ble_name);
		QL_HTTP_LOG("File deleted successfully");
	}
	QL_HTTP_LOG("sg.data ==%s\n",msg.data);
	if ((err = ql_rtos_queue_release(lt_http.http_msg_queue, sizeof(lt_http_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
    {
        QL_HTTP_LOG("send msg to queue failed, err=%d", err);
		if(msg.data)
			free(msg.data);
		return -1;
    }

	ql_rtos_task_sleep_ms(5000);	//	等待下载完成

	// 在下载完成后进行校验
	if (verify_file_md5(ble_name, md5) == 0) 
	{
    	QL_HTTP_LOG("MQTT_TEST:check md5 success");
		return 0;
	} 
	else 
	{
    	QL_HTTP_LOG("MQTT_TEST:check md5 fail");
		ql_remove(ble_name);
		return -1;
	}

	return 0;
}

// 【新增函数】下载配置文件
char lt_http_dload_config( const char *file_name ,char *url ,char *md5)
{
	QlOSStatus err = QL_OSI_SUCCESS;
    lt_http_msg_t msg;
	msg.type = FILE_RECV_MSG; // 设置为文件下载类型
	QL_HTTP_LOG("url==%s\n",url);
    memset(msg.url, 0x00, sizeof(msg.url));
    memcpy(msg.url, url, strlen(url));
	msg.datalen = strlen(file_name)+strlen("UFS:")+1;
	msg.data = malloc(msg.datalen);

	sprintf((char*)msg.data,"UFS:%s",file_name);
	char config_name[255]={0};
	sprintf(config_name,"UFS:%s",file_name);
	if (ql_file_exist(config_name) == QL_FILE_OK)
	{		
		ql_remove(config_name);
		QL_HTTP_LOG("File deleted successfully");
	}
	QL_HTTP_LOG("sg.data ==%s\n",msg.data);
	if ((err = ql_rtos_queue_release(lt_http.http_msg_queue, sizeof(lt_http_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
    {
        QL_HTTP_LOG("send msg to queue failed, err=%d", err);
		if(msg.data)
			free(msg.data);
		return -1;
    }

	ql_rtos_task_sleep_ms(5000);	//	等待下载完成

	// 在下载完成后进行校验
	if (verify_file_md5(config_name, md5) == 0) 
	{
    	QL_HTTP_LOG("MQTT_TEST:check md5 success");
		return 0;
	} 
	else 
	{
    	QL_HTTP_LOG("MQTT_TEST:check md5 fail");
		ql_remove(config_name);
		return -1;
	}

	return 0;
}

// 【新增函数】用于发起 API GET 请求，只打印到日志
void lt_http_get_api_request(char *url)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    lt_http_msg_t msg;
    
    msg.type = API_RECV_MSG; // 设置为 API 请求类型
    
    QL_HTTP_LOG("API URL==%s\n", url);
    memset(msg.url, 0x00, sizeof(msg.url));
    memcpy(msg.url, url, strlen(url));
    
    // API 请求不需要文件路径/名称
    msg.datalen = 0;
    msg.data = NULL;

    QL_HTTP_LOG("Sending API GET request to queue");
    if ((err = ql_rtos_queue_release(lt_http.http_msg_queue, sizeof(lt_http_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
    {
        QL_HTTP_LOG("send msg to queue failed, err=%d", err);
    }
}


// 【修改点 4】修改 HTTP 任务线程，支持两种请求类型
static void http_fs_dload_thread(void * arg)
{
    int ret = 0, i = 0;
    int profile_idx = 1;
	struct stat dload_stat;
	uint8_t nSim = 0;
	int flags_break = 0;
	ql_event_t qhttpc_event_msg = {0};
	
	ql_rtos_task_sleep_s(5);
	QL_HTTP_LOG("========== http demo start ==========");
	QL_HTTP_LOG("wait for network register done");
	
	while((ret = ql_network_register_wait(nSim, 120)) != 0 && i < 10){
    	i++;
		ql_rtos_task_sleep_ms(1000);
	}
	if(ret == 0){                                                                                              
		i = 0;
		QL_HTTP_LOG("====network registered!!!!====");
        
        // 【新增】在网络注册成功后，自动发起一次您的 API 测试请求
        QL_HTTP_LOG("==== Sending Test API Request ====");
       // lt_http_get_api_request("http://api.cellocation.com:84/cell/?coord=bd09&output=csv&mcc=460&mnc=1&lac=20300&ci=86107006");
        // 如果您想测试 MP3 下载，可以再添加一个 lt_http_dload_mp3("test.mp3", "http://example.com/some.mp3")

	}else{
		QL_HTTP_LOG("====network register failure!!!!!====");
		goto exit;
	}

	while (1)
	{
		lt_http_msg_t msg;
		ql_rtos_queue_wait(lt_http.http_msg_queue, (uint8 *)(&msg), sizeof(lt_http_msg_t), 0xFFFFFFFF);

		if (msg.type == FILE_RECV_MSG || msg.type == API_RECV_MSG) // 检查两种接收消息类型
		{
			QL_HTTP_LOG("MSG url:%s, type:%d", msg.url, msg.type);
			int http_method = HTTP_METHOD_NONE;

			memset(&http_demo_client, 0x00, sizeof(qhttpc_ctx_t));

			http_demo_client.dl_block = false;
			http_demo_client.dl_high_line = HTTP_DLOAD_HIGH_LINE;
            http_demo_client.dload_fd = -1; // 默认设置为无效值

			ret = ql_rtos_mutex_create(&http_demo_client.simple_lock);
			if (ret)
			{
				QL_HTTP_LOG("ql_rtos_mutex_create failed!!!!");
				break;
			}

			ret = ql_rtos_queue_create(&http_demo_client.queue, sizeof(ql_event_t), HTTP_MAX_MSG_CNT);
			if (ret)
			{
				QL_HTTP_LOG("ql_rtos_queue_create failed!!!!");
				break;
			}

			if (ql_httpc_new(&http_demo_client.http_client, http_event_cb, (void *)&http_demo_client) != QL_HTTP_OK)
			{
				QL_HTTP_LOG("http client create failed!!!!");
				break;
			}
			
            // 只有文件下载类型才执行文件操作
            if (msg.type == FILE_RECV_MSG) {
                http_demo_client.dload_fd = ql_fopen((const char *)msg.data, "w+");
                if (http_demo_client.dload_fd < 0)
                {
                    QL_HTTP_LOG("open file failed!!!!");
                    ql_httpc_release(&http_demo_client.http_client);
                    
                    // 清理资源并跳过本次请求
                    ql_rtos_mutex_delete(http_demo_client.simple_lock);
                    ql_rtos_queue_delete(http_demo_client.queue);
                    if (msg.data) free(msg.data);
                    continue; 
                }
            }


			ql_httpc_setopt(&http_demo_client.http_client, HTTP_CLIENT_OPT_SIM_ID, nSim);
			ql_httpc_setopt(&http_demo_client.http_client, HTTP_CLIENT_OPT_PDPCID, profile_idx);
			ql_httpc_setopt(&http_demo_client.http_client, HTTP_CLIENT_OPT_WRITE_FUNC, http_write_response_data);
			ql_httpc_setopt(&http_demo_client.http_client, HTTP_CLIENT_OPT_WRITE_DATA, (void *)&http_demo_client);

			http_method = HTTP_METHOD_GET;
			ql_httpc_setopt(&http_demo_client.http_client, HTTP_CLIENT_OPT_METHOD, http_method);
			ql_httpc_setopt(&http_demo_client.http_client, HTTP_CLIENT_OPT_URL, (char *)msg.url);

			if (ql_httpc_perform(&http_demo_client.http_client) == QL_HTTP_OK)
			{
				QL_HTTP_LOG("wait http perform end!!!!!!");

				flags_break = 0;
				for (;;)
				{
					memset(&qhttpc_event_msg, 0x00, sizeof(ql_event_t));

					ql_rtos_queue_wait(http_demo_client.queue, (uint8 *)&qhttpc_event_msg, sizeof(ql_event_t), QL_WAIT_FOREVER);

					switch (qhttpc_event_msg.id)
					{
					case QHTTPC_EVENT_RESPONSE:
					{
						http_write_response_data_func((void *)&qhttpc_event_msg);
					}
					break;
					case QHTTPC_EVENT_END:
					{
						flags_break = 1;
					}
					break;
					default:
						break;
					}

					if (flags_break)
						break;
				}
			}
			else
			{
				QL_HTTP_LOG("http perform failed!!!!!!!!!!");
			}
            
            // 只有文件下载类型才执行文件统计和关闭操作
            if (msg.type == FILE_RECV_MSG) {
                memset(&dload_stat, 0x00, sizeof(struct stat));
                ql_fstat(http_demo_client.dload_fd, &dload_stat);
                QL_HTTP_LOG("=========dload_file_size:%d", dload_stat.st_size);
                if (http_demo_client.dload_fd >= 0)
                {
                    ql_fclose(http_demo_client.dload_fd);
                    http_demo_client.dload_fd = -1;
                }
            }
			
			// ql_rtos_mutex_lock(http_demo_client.simple_lock, 100);
			// if (http_demo_client.upload_fd >= 0)
			// {
			// 	ql_fclose(http_demo_client.upload_fd);
			// 	http_demo_client.upload_fd = -1;
			// }
			// ql_rtos_mutex_unlock(http_demo_client.simple_lock);

			ql_httpc_release(&http_demo_client.http_client);
			http_demo_client.http_client = 0;
			QL_HTTP_LOG("==============http_client_test_end================\n");

			if (http_demo_client.queue != NULL)
			{
				while (1)
				{
					memset(&qhttpc_event_msg, 0x00, sizeof(ql_event_t));

					if (QL_OSI_SUCCESS != ql_rtos_queue_wait(http_demo_client.queue, (uint8 *)&qhttpc_event_msg, sizeof(ql_event_t), 0))
						break;

					switch (qhttpc_event_msg.id)
					{
					case QHTTPC_EVENT_RESPONSE:
					{
						free((void *)(qhttpc_event_msg.param2));
					}
					break;
					default:
						break;
					}
				}
				ql_rtos_queue_delete(http_demo_client.queue);
				http_demo_client.queue = NULL;
			}

			ql_rtos_mutex_delete(http_demo_client.simple_lock);
			http_demo_client.simple_lock = NULL;

			//ql_rtos_task_sleep_s(3);
		}
		do
		{
            // 只有文件下载类型才有 msg.data 需要释放
			if (msg.type == FILE_RECV_MSG && msg.data)
			{
				free((void *)msg.data);
				msg.data = NULL;
			}
		} while (0);

	}

exit:
	if(http_demo_client.queue != NULL)
	{
		while(1)
		{
			memset(&qhttpc_event_msg, 0x00, sizeof(ql_event_t));
			
			if(QL_OSI_SUCCESS != ql_rtos_queue_wait(http_demo_client.queue, (uint8 *)&qhttpc_event_msg, sizeof(ql_event_t), 0))
				break;

			switch(qhttpc_event_msg.id)
			{
				case QHTTPC_EVENT_RESPONSE:
				{
					free((void *)(qhttpc_event_msg.param2));
				}
					break;
				default:
					break;
			}
		}
		ql_rtos_queue_delete(http_demo_client.queue);
	}

	if(http_demo_client.simple_lock != NULL)
		ql_rtos_mutex_delete(http_demo_client.simple_lock);
	
  	ql_rtos_task_delete(http_task);	
    return;		
}
#endif 
void lt_http_app_init(void)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    

    ql_rtos_queue_create(&lt_http.http_msg_queue, sizeof(lt_http_msg_t), 10);


    err = ql_rtos_task_create(&lt_http.http_task, 8 * 1024, APP_PRIORITY_ABOVE_NORMAL, "http_task", http_fs_dload_thread, NULL, 5);

    if (err != QL_OSI_SUCCESS)
    {
        QL_HTTP_LOG("mqtt_task init failed");
    }


    // err = ql_rtos_task_create(&http_task, 4096, APP_PRIORITY_NORMAL, "QhttpApp", http_app_thread, NULL, 5);
	// if(err != QL_OSI_SUCCESS)
	// {
	// 	QL_HTTP_LOG("created task failed");
	// }
}