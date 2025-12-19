/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2025-10-29 15:10:31
 * @LastEditors: ubuntu
 * @LastEditTime: 2025-12-10 12:29:19
 */
#ifndef EC800G_OTA_MASTER_H
#define EC800G_OTA_MASTER_H

#include <stdint.h>
#include <stdbool.h> 

// 使用和原来相同的协议头定义，方便与 FR801H 从机互通
#define UART_OTA_HEAD_MAX        9
#define UART_OTA_SEND_BUFF_LEN   (188 - UART_OTA_HEAD_MAX)
#define UART_OTA_RECV_BUFF_LEN   128

// 定时器与超时参数
#define UART_RECV_FRAME_TIMEOUT_MS    20    // 50ms 帧终止判断
#define UART_OTA_RESPONSE_TIMEOUT_MS  500   // 等待回应超时 500ms
#define UART_OTA_TIMER_MS             50   // OTA 主状态机触发定时器 100ms
#define UART_OTA_OUT_TIMER_MAX        60    // 超时重试次数上限 (100ms 计数)

// 如果要启用 CRC 校验，请保留宏
#define OTA_CRC_CHECK

// 命令类型，与原 .h 保持一致
enum cmd_type
{
    get_base_addr = 1,
    get_version   = 2,
    earse_sector  = 3,
    write_data    = 5,
    rest_device   = 9,
};
typedef enum
{
    UART_MODE_NORMAL = 0,
    UART_MODE_OTA
} lt_uart_mode_e;
typedef enum
{
    OTA_STATE_IDLE = 0,
    OTA_STATE_WAIT_BASE = 1,
    OTA_STATE_WAIT_VERSION = 2,
    OTA_STATE_ERASE = 3,
    OTA_STATE_WRITE = 4,
    OTA_STATE_RESET = 5,
    OTA_STATE_SUCCESS = 0xF0,
    OTA_STATE_ERROR = 0xFF
} ec_ota_state_e;

// 与原包结构一致（用于解析从机响应）
#pragma pack(1)
struct firmware_version
{
    uint32_t firmware_version;
};

struct storage_baseaddr
{
    uint32_t baseaddr;
};

struct page_erase_rsp
{
    uint32_t base_address;
};

struct write_data_rsp
{
    uint32_t base_address;
    uint16_t length;
};

struct app_ota_hdr_t
{
    uint8_t result;
    uint8_t org_opcode;
    uint16_t length;
    union
    {
        struct firmware_version version;
        struct storage_baseaddr baseaddr;
        struct page_erase_rsp page_erase;
        struct write_data_rsp write_data;
    } rsp;
};

typedef struct lt_ota_msg
{
    ec_ota_state_e type;
    uint32_t datalen;
    uint8_t *data;
} lt_ota_msg_t;

#pragma pack()

// 对外接口
// 在主任务初始化时调用（确保串口层已初始化）
void ec_ota_init(void);

// 启动 OTA：port 等由串口层负责（这里传入你用的串口端口号）
// firmware_path: EC800G 本地文件系统路径（例如 "/data/firmware.bin"）
// 返回 0 成功启动 OTA，<0 失败
int ec_ota_start(int port, const char *firmware_path);

// 主动停止 OTA，status 可用作成功/失败码
void ec_ota_stop(uint8_t status);

// 取得 OTA 当前状态（0 空闲，其他为进度/状态码）
uint8_t ec_ota_get_state(void);

void ota_on_frame_received(void);

void lt_uart_ota_init(void);

bool is_ota_mode(void);

#endif // EC800G_OTA_MASTER_H
