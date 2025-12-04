/*
 * @Descripttion:
 * @version:
 * @Author: zhouhao
 * @Date: 2025-10-29 15:17:10
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2025-11-20 21:07:15
 */
/*
移植说明：
- 该文件实现 EC800G OpenCPU 端作为 UART OTA Master 的逻辑。
- 依赖：你的串口发送函数 lt_uart_send_data(port, data, len)
        以及串口接收数据被缓存在 g_uart_state.ota_recv_buffer（在 ltuart2frx8016.c 回调中收集并在帧超时后调用本文件的处理函数）
- 若你希望串口回调直接调用本文件内部的接收处理，请在 ltuart2frx8016.c 的 frame 超时处理处调用 ota_process_received_frame()（我在 ltuart2frx8016.c 已示例）
*/
#include "ec800g_ota_master.h"
#include "ltuart2frx8016.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// EC800G SDK 头（根据你的工程替换或确认）
#include "ql_fs.h"
#include "ql_uart.h"
#include "ql_log.h"

#include "ql_api_osi.h"

#define QL_UART_OTA_DEMO_LOG(msg, ...) QL_LOG(QL_LOG_LEVEL_INFO, "uart_ota", msg, ##__VA_ARGS__)

// 在文件头部 QL_LOG 定义之后添加
#define QL_HEX(tag, data, len)                                                       \
    do                                                                               \
    {                                                                                \
        char hex_buf[81] = {0};                                                      \
        int _h = 0;                                                                  \
        for (int _i = 0; _i < (len) && _h < 80; _i++)                                \
        {                                                                            \
            _h += snprintf(hex_buf + _h, sizeof(hex_buf) - _h, "%02X ", (data)[_i]); \
            if ((_i % 16 == 15) && (_i != (len) - 1))                                \
            {                                                                        \
                QL_LOG(QL_LOG_LEVEL_DEBUG, tag, "%s", hex_buf);                      \
                _h = 0;                                                              \
                memset(hex_buf, 0, sizeof(hex_buf));                                 \
            }                                                                        \
        }                                                                            \
        if (_h > 0)                                                                  \
        {                                                                            \
            QL_LOG(QL_LOG_LEVEL_DEBUG, tag, "%s", hex_buf);                          \
        }                                                                            \
    } while (0)

// OTA模块依赖此枚举

// OTA模块通过 extern 引用此全局变量，我们在此处定义它
struct lt_uart_port_state_t
{
    lt_uart_mode_e mode;
    ql_task_t normal_target_task_ref;
    uint32_t normal_msg_id;
    bool initialized;
    ql_uart_port_number_e port_num;
    volatile uint8_t ota_state;
    ql_timer_t recv_timer_ref;     // OTA 帧接收超时定时器
    ql_timer_t response_timer_ref; // OTA 命令响应超时定时器
    uint8_t *ota_recv_buffer;
    uint16_t ota_recv_idx;
    uint8_t *ota_send_buffer;
    int64_t ota_fw_handle;
    char *ota_fw_path_copy;
    uint32_t ota_fw_total_len;
    uint32_t ota_base_address;
    uint32_t ota_dev_version;
    uint16_t ota_earse_sector_index;
    uint32_t ota_send_data_offset;
    uint16_t ota_timeout_count;
} g_uart_state; // <-- 定义全局变量

// 定时器 id（使用 SDK 的 timer 句柄）
static ql_timer_t ota_main_timer = NULL;
static ql_queue_t lt_ota_msg_queue;

static const int crc_table[256] =
    {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
        0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
        0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
        0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
        0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
        0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
        0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
        0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
        0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
        0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
        0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
        0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
        0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
        0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
        0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
        0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
        0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
        0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
        0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
        0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
        0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
        0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d};

static uint32_t Crc32CalByByte(uint32_t crc, uint8_t *ptr, uint32_t len)
{
    uint32_t i = 0;
    while (len-- != 0)
    {
        int high = crc / 256;
        crc <<= 8;
        crc ^= crc_table[(high ^ ptr[i]) & 0xff];
        crc &= 0xFFFFFFFF;
        i++;
    }
    return crc & 0xFFFFFFFF;
}

// Forward
static void ota_main_timer_cb(void *param);
static int ota_send_command_internal(enum cmd_type command);
static void ota_process_received_frame_internal(void);
static uint32_t calculate_firmware_crc_from_file(int64_t file_handle, uint32_t firmware_length);

// 公共接口实现
/*
 * OTA模块依赖的发送函数
 * (注意：非 static，供 ec800g_ota_master.c 链接)
 */
int lt_uart_send_data(ql_uart_port_number_e port, uint8_t *data, uint32_t len)
{
    if (!g_uart_state.initialized || port != g_uart_state.port_num)
    {
        return -1;
    }
    // 直接调用 ql_uart_write
    //    QL_HEX(data, len);

    lt_uart2frx8016_send((char *)data, len);

    return 0;
}
/*
 * OTA 帧接收超时回调
 * (当串口数据流停止 UART_RECV_FRAME_TIMEOUT_MS 时触发)
 */
static void ota_recv_frame_timeout_cb(void *param)
{
    ql_rtos_timer_stop(g_uart_state.recv_timer_ref);

    // 如果在OTA模式且收到了数据，则调用OTA处理函数
    if (g_uart_state.mode == UART_MODE_OTA && g_uart_state.ota_recv_idx > 0)
    {
        // 调用 ec800g_ota_master.c 中的 ota_on_frame_received
        // (该函数在 ec800g_ota_master.h 中声明)
        extern void ota_on_frame_received(void);
        ota_on_frame_received();
    }
}

/*
 * OTA 命令响应超时回调
 * (ec800g_ota_master.c 中的 ota_main_timer_cb 似乎已处理了超时计数，
 * 但 ota_send_command_internal 仍会启动此定时器，
 * 我们保留它以防万一，但主要逻辑在 ota_main_timer_cb)
 */
static void ota_response_timeout_cb(void *param)
{
    // QL_UART_OTA_DEMO_LOG("OTA response timer fired (handled by main timer)");
}

void ec_ota_init(void)
{
    // 初始化资源（如果需要）
    // 假设 g_uart_state 已在 ltuart2frx8016.c 初始化
    if (ota_main_timer == NULL)
    {
        ql_rtos_timer_create(&ota_main_timer, NULL, ota_main_timer_cb, NULL);
    }
}
static int fh = -1;
int ec_ota_start(int port, const char *firmware_path)
{
    if (!firmware_path)
        return -1;
    if (!g_uart_state.initialized)
        return -2;
    if (g_uart_state.mode == UART_MODE_OTA)
        return -3; // 已在 OTA

    // 打开文件获取长度
    // int fh = -1;
    fh = ql_fopen(firmware_path, "rb");
    if (fh < 0)
    {
        QL_UART_OTA_DEMO_LOG("Open firmware file %s fail fh=%lld", firmware_path, fh);
        return -4;
    }

    uint32_t file_size = 0;
    file_size = ql_fsize(fh);
    // ql_fs_size((u8*)firmware_path, &file_size);

    // 保存固件信息到全局 state
    g_uart_state.mode = UART_MODE_OTA;
    // 复制路径
    if (g_uart_state.ota_fw_path_copy)
    {
        free(g_uart_state.ota_fw_path_copy);
        g_uart_state.ota_fw_path_copy = NULL;
    }
    g_uart_state.ota_fw_path_copy = (char *)malloc(strlen(firmware_path) + 1);
    if (g_uart_state.ota_fw_path_copy)
        strcpy(g_uart_state.ota_fw_path_copy, firmware_path);
    g_uart_state.ota_fw_handle = fh;
    g_uart_state.ota_fw_total_len = file_size;
    g_uart_state.ota_base_address = 0;
    g_uart_state.ota_dev_version = 0;
    g_uart_state.ota_earse_sector_index = 0;
    g_uart_state.ota_send_data_offset = 0;
    g_uart_state.ota_timeout_count = 0;
    g_uart_state.ota_recv_idx = 0;
    if (!g_uart_state.ota_recv_buffer)
    {
        g_uart_state.ota_recv_buffer = (uint8_t *)malloc(UART_OTA_RECV_BUFF_LEN);
    }
    if (!g_uart_state.ota_send_buffer)
    {
        g_uart_state.ota_send_buffer = (uint8_t *)malloc(UART_OTA_SEND_BUFF_LEN + UART_OTA_HEAD_MAX);
    }
    if (!g_uart_state.ota_recv_buffer || !g_uart_state.ota_send_buffer)
    {
        QL_UART_OTA_DEMO_LOG("malloc OTA buffers fail");
        if (g_uart_state.ota_fw_path_copy)
        {
            free(g_uart_state.ota_fw_path_copy);
            g_uart_state.ota_fw_path_copy = NULL;
        }
        if (g_uart_state.ota_recv_buffer)
        {
            free(g_uart_state.ota_recv_buffer);
            g_uart_state.ota_recv_buffer = NULL;
        }
        if (g_uart_state.ota_send_buffer)
        {
            free(g_uart_state.ota_send_buffer);
            g_uart_state.ota_send_buffer = NULL;
        }
        ql_fclose(fh);
        g_uart_state.mode = UART_MODE_NORMAL;
        return -5;
    }

    // 初始 ota 状态：请求获取基地址
    g_uart_state.ota_state = OTA_STATE_WAIT_BASE;

    // 启动主定时器（100ms 周期），在定时器回调里驱动状态机
    ql_rtos_timer_start(ota_main_timer, UART_OTA_TIMER_MS, TRUE);

    QL_UART_OTA_DEMO_LOG("OTA start: file=%s len=%u", firmware_path, file_size);
    return 0;
}

void ec_ota_stop(uint8_t status)
{
    // 停止定时器并释放资源
    if (ota_main_timer)
        ql_rtos_timer_stop(ota_main_timer);
    if (g_uart_state.ota_fw_handle >= 0)
    {
        ql_fclose(g_uart_state.ota_fw_handle);
        g_uart_state.ota_fw_handle = -1;
    }
    if (g_uart_state.ota_fw_path_copy)
    {
        free(g_uart_state.ota_fw_path_copy);
        g_uart_state.ota_fw_path_copy = NULL;
    }
    if (g_uart_state.ota_recv_buffer)
    {
        free(g_uart_state.ota_recv_buffer);
        g_uart_state.ota_recv_buffer = NULL;
    }
    if (g_uart_state.ota_send_buffer)
    {
        free(g_uart_state.ota_send_buffer);
        g_uart_state.ota_send_buffer = NULL;
    }

    // 设置状态
    if (status == 0)
        g_uart_state.ota_state = OTA_STATE_SUCCESS;
    else
        g_uart_state.ota_state = OTA_STATE_ERROR;

    g_uart_state.mode = UART_MODE_NORMAL;
    QL_UART_OTA_DEMO_LOG("OTA stopped status=%u", status);
}

uint8_t ec_ota_get_state(void)
{
    return g_uart_state.ota_state;
}

void lt_ota_msg_send(ec_ota_state_e type)
{
    QlOSStatus err = QL_OSI_SUCCESS;
    lt_ota_msg_t msg;
    msg.type = type;
    msg.datalen = 0;
    msg.data = NULL;

    if ((err = ql_rtos_queue_release(lt_ota_msg_queue, sizeof(lt_ota_msg_t), (uint8 *)&msg, 1000)) != QL_OSI_SUCCESS)
    {
        QL_UART_OTA_DEMO_LOG("send msg to lt_ota_msg_queue failed, err=%d", err);
    }
}
// OTA 主定时器回调（100ms 周期）
static void ota_main_timer_cb(void *param)
{
    // 若当前不在 OTA 模式或在等待从机回复，则跳过
    if (g_uart_state.mode != UART_MODE_OTA)
        return;

    // 若等待回复（标记位设置在 ota_state 高位），则检查超时计数
    if (g_uart_state.ota_state & 0x10)
    {
        // 我把 0x10 用作“等待回复”标志，和原代码一致
        if (++g_uart_state.ota_timeout_count >= UART_OTA_OUT_TIMER_MAX)
        {
            QL_UART_OTA_DEMO_LOG("OTA response timeout");
            ec_ota_stop(1);
            return;
        }
        return;
    }

    // 驱动状态机：当没有等待回复时，发送下一个命令
    uint8_t progress = g_uart_state.ota_state & 0x0F;
    if (progress != OTA_STATE_WAIT_BASE && progress != OTA_STATE_WAIT_VERSION && progress != OTA_STATE_ERASE && progress != OTA_STATE_WRITE && progress != OTA_STATE_RESET)
    {
        g_uart_state.ota_state = OTA_STATE_WAIT_BASE;
        return;
    }
    lt_ota_msg_send(progress);
}

// 内部：发送命令（构造并写串口）
// 此处保持与原协议字段一致
static int ota_send_command_internal(enum cmd_type command)
{
    if (g_uart_state.mode != UART_MODE_OTA)
        return -1;
    if (!g_uart_state.ota_send_buffer)
        return -2;

    uint8_t *buf = g_uart_state.ota_send_buffer;
    int cmd_len = 0;

    switch (command)
    {
    case get_base_addr:
        buf[cmd_len++] = get_base_addr;
        buf[cmd_len++] = 0x00;
        buf[cmd_len++] = 0x00;
        break;
    case get_version:
        buf[cmd_len++] = get_version;
        buf[cmd_len++] = 0x06;
        buf[cmd_len++] = 0x00;
        break;
    case earse_sector:
    {
        // 每次擦除一个扇区（假设扇区大小 0x1000，跟原代码一致）
        uint32_t erase_addr = g_uart_state.ota_earse_sector_index * 0x1000 + g_uart_state.ota_base_address;
        buf[cmd_len++] = earse_sector;
        buf[cmd_len++] = 0x06;
        buf[cmd_len++] = 0x00;
        memcpy(&buf[cmd_len], (uint8_t *)&erase_addr, 4);
        cmd_len += 4;
        g_uart_state.ota_earse_sector_index++;
        break;
    }
    case write_data:
    {
        buf[cmd_len++] = write_data;
        uint16_t totallen = (UART_OTA_SEND_BUFF_LEN + UART_OTA_HEAD_MAX) & 0xFFFF;
        buf[cmd_len++] = (totallen & 0xff);
        buf[cmd_len++] = (totallen >> 8) & 0xff;
        uint32_t base_addr_temp = g_uart_state.ota_send_data_offset + g_uart_state.ota_base_address;
        memcpy(&buf[cmd_len], (uint8_t *)&base_addr_temp, 4);
        cmd_len += 4;
        buf[cmd_len++] = (UART_OTA_SEND_BUFF_LEN & 0xff);
        buf[cmd_len++] = (UART_OTA_SEND_BUFF_LEN >> 8) & 0xff;

        // 计算剩余固件长度并读取文件
        uint32_t remain = g_uart_state.ota_fw_total_len - g_uart_state.ota_send_data_offset;
        QL_UART_OTA_DEMO_LOG("remain=%u", remain);
        if (remain == 0)
        {
            // nothing to send
        }
        else
        {
            uint32_t read_len = (remain >= (uint32_t)UART_OTA_SEND_BUFF_LEN) ? UART_OTA_SEND_BUFF_LEN : remain;
            // 从文件读取
            if (g_uart_state.ota_fw_handle < 0)
                return -3;
            ql_fseek(g_uart_state.ota_fw_handle, g_uart_state.ota_send_data_offset, SEEK_SET);
            QL_UART_OTA_DEMO_LOG(" g_uart_state.ota_send_data_offsett=%d", g_uart_state.ota_send_data_offset);

            int r = ql_fread(&buf[cmd_len], read_len, 1, g_uart_state.ota_fw_handle);
            if (r != read_len)
            {
                QL_UART_OTA_DEMO_LOG("fs read error want=%u got=%d", read_len, r);
                return -4;
            }
            cmd_len += read_len;
            g_uart_state.ota_send_data_offset += read_len;
        }
        break;
    }
    case rest_device:
    {
        buf[cmd_len++] = rest_device;
#ifdef OTA_CRC_CHECK
        buf[cmd_len++] = 0x08;
        buf[cmd_len++] = 0x00;
        uint32_t firmware_len = g_uart_state.ota_fw_total_len;
        memcpy(&buf[cmd_len], (uint8_t *)&firmware_len, 4);
        cmd_len += 4;
        uint32_t crc = calculate_firmware_crc_from_file(g_uart_state.ota_fw_handle, g_uart_state.ota_fw_total_len);
        memcpy(&buf[cmd_len], (uint8_t *)&crc, 4);
        cmd_len += 4;
#else
        buf[cmd_len++] = 0x00;
        buf[cmd_len++] = 0x00;
        // pad zeros
        buf[cmd_len++] = 0;
        buf[cmd_len++] = 0;
        buf[cmd_len++] = 0;
        buf[cmd_len++] = 0;
#endif
        break;
    }
    default:
        return -10;
    }

    // 标记为“等待回复”，等待 response timer 驱动超时处理
    g_uart_state.ota_state |= 0x10; // 前四位设为 1 表示等待
    g_uart_state.ota_timeout_count = 0;

    // 发送
    int wret = lt_uart_send_data(g_uart_state.port_num, buf, cmd_len);
    if (wret < 0)
    {
        QL_UART_OTA_DEMO_LOG("lt_uart_send_data fail ret=%d", wret);
        g_uart_state.ota_state &= ~0x10;
        return -20;
    }

    // 启动 response 超时定时器（使用 g_uart_state.response_timer_ref 在 ltuart2frx8016.c 创建）
    if (g_uart_state.response_timer_ref)
    {
        ql_rtos_timer_stop(g_uart_state.response_timer_ref);
        ql_rtos_timer_start(g_uart_state.response_timer_ref, UART_OTA_RESPONSE_TIMEOUT_MS, FALSE);
    }
    return 0;
}

// 当串口帧接收完成（ltuart2frx8016.c 在 frame timeout 时应调用本函数）
// 你也可以在 ltuart2frx8016.c 中直接实现解析逻辑并复用 app_ota_hdr_t 结构
void ota_process_received_frame_internal(void)
{
    // 如果不是 OTA 模式或无数据，返回
    if (g_uart_state.mode != UART_MODE_OTA)
    {
        g_uart_state.ota_recv_idx = 0;
        return;
    }
    if (g_uart_state.ota_recv_idx == 0)
        return;

    uint8_t *p = g_uart_state.ota_recv_buffer;
    uint16_t len = g_uart_state.ota_recv_idx;

    // 简要检查：必须至少包含头部 (result + opcode + length)
    if (len < 4)
    {
        g_uart_state.ota_recv_idx = 0;
        return;
    }

    struct app_ota_hdr_t *hdr = (struct app_ota_hdr_t *)p;
    uint8_t opcode = hdr->org_opcode;

    // 清除等待标记
    g_uart_state.ota_state &= 0x0F; // 清除前4bit（等待标志等）
    // 解析响应并设置下一步状态
    switch (opcode)
    {
    case get_base_addr:
        // 注意：长度与字段解析依赖从机协议，对齐到你的从机实现
        g_uart_state.ota_base_address = hdr->rsp.baseaddr.baseaddr;
        // 下一步获取版本
        g_uart_state.ota_state = OTA_STATE_WAIT_VERSION;
        break;
    case get_version:
        g_uart_state.ota_dev_version = hdr->rsp.version.firmware_version;
        // 下一步擦除
        g_uart_state.ota_state = OTA_STATE_ERASE;
        g_uart_state.ota_earse_sector_index = 0;
        break;
    case earse_sector:
        // 检查是否所有扇区已擦除（基于 file size 和扇区大小）
        {
            uint32_t sectors_needed = (g_uart_state.ota_fw_total_len + 0x0FFF) / 0x1000;
            if (g_uart_state.ota_earse_sector_index >= sectors_needed)
            {
                g_uart_state.ota_state = OTA_STATE_WRITE;
                g_uart_state.ota_send_data_offset = 0;
            }
            else
            {
                // 仍处于 ERASE 进度：保持同一状态，让定时器再发送下一条 earse_sector
                g_uart_state.ota_state = OTA_STATE_ERASE;
            }
            break;
        }
    case write_data:
    {
        // 检查是否发送完所有数据
        if (g_uart_state.ota_send_data_offset >= g_uart_state.ota_fw_total_len)
        {
            g_uart_state.ota_state = OTA_STATE_RESET;
        }
        else
        {
            g_uart_state.ota_state = OTA_STATE_WRITE; // 继续写下一包
        }
    }
    break;
    case rest_device:
        g_uart_state.ota_state = OTA_STATE_SUCCESS;
        // OTA 完成：关闭资源
        ec_ota_stop(0);
        break;
    default:
        QL_UART_OTA_DEMO_LOG("unknown opcode rsp %u", opcode);
        break;
    }

    // reset receive buffer index
    g_uart_state.ota_recv_idx = 0;

    // 停止 response timer
    if (g_uart_state.response_timer_ref)
        ql_rtos_timer_stop(g_uart_state.response_timer_ref);
}

// CRC 计算：直接从文件分片读取（不影响当前位置，使用 seek + read）
static uint32_t calculate_firmware_crc_from_file(int64_t file_handle, uint32_t firmware_length)
{
    if (file_handle < 0)
        return 0;
    uint8_t *buf = (uint8_t *)malloc(256);
    if (!buf)
        return 0;
    uint32_t crc = 0;
    uint32_t offset = 0;

    // 先跳过前 256 字节（与原实现一致，从 new_bin_addr+256 开始）
    offset = 256;
    if (firmware_length <= 256)
    {
        free(buf);
        return 0;
    }
    uint32_t remain = firmware_length - 256;
    ql_fseek(file_handle, offset, SEEK_SET);

    while (remain > 0)
    {
        uint32_t chunk = remain > 256 ? 256 : remain;
        int r = ql_fread(buf, 1, chunk, file_handle);
        if (r != (int)chunk)
        {
            QL_UART_OTA_DEMO_LOG("crc read fail at offset %u want=%u got=%d", offset, chunk, r);
            break;
        }
        crc = Crc32CalByByte(crc, buf, chunk);
        remain -= chunk;
        offset += chunk;
    }
    free(buf);
    QL_UART_OTA_DEMO_LOG("crc computed 0x%08x", crc);
    return crc;
}

// 判断是否在OTA模式
bool is_ota_mode(void) 
{
    return (g_uart_state.mode == UART_MODE_OTA);
}

/* 供外部 frame timeout 回调调用：
   当 ltuart2frx8016.c 的接收定时器触发（帧结束），
   请调用本函数： ota_on_frame_received()
   我们会调用内部的 ota_process_received_frame_internal() 来解析。
*/
void ota_on_frame_received(void)
{
    ota_process_received_frame_internal();
}

void lt_ota_msg_thread(void *param)
{
    while (1)
    {
        lt_ota_msg_t msg;
        ql_rtos_queue_wait(lt_ota_msg_queue, (uint8 *)(&msg), sizeof(lt_ota_msg_queue), 0xFFFFFFFF);

        QL_UART_OTA_DEMO_LOG("Test1:read_len=%d", msg.datalen);

        int ret = 0;
        switch (msg.type)
        {
        case OTA_STATE_WAIT_BASE:
            ret = ota_send_command_internal(get_base_addr);
            break;
        case OTA_STATE_WAIT_VERSION:
            ret = ota_send_command_internal(get_version);
            break;
        case OTA_STATE_ERASE:
            ret = ota_send_command_internal(earse_sector);
            break;
        case OTA_STATE_WRITE:
            ret = ota_send_command_internal(write_data);
            break;
        case OTA_STATE_RESET:
            ret = ota_send_command_internal(rest_device);
            break;
        default:
            // 默认：设置为获取基地址
            g_uart_state.ota_state = OTA_STATE_WAIT_BASE;
            break;
        }
        if (ret < 0)
        {
            QL_UART_OTA_DEMO_LOG("ota_send_command_internal fail ret=%d", ret);
            ec_ota_stop(1);
        }

        do
        {
            if (msg.data)
            {
                free((void *)msg.data);
                msg.data = NULL;
            }
        } while (0);
    }
}
void lt_uart_ota_init(void)
{
    int ret = 0;
    QlOSStatus err = 0;
    ql_task_t uartota_task = NULL;
    // ======== MODIFICATION START ========
    // 初始化 OTA 相关的全局状态
    memset(&g_uart_state, 0, sizeof(g_uart_state));
    g_uart_state.mode = UART_MODE_NORMAL;
    g_uart_state.port_num = QL_CUR_UART_PORT;
    g_uart_state.ota_state = 0; // OTA_STATE_IDLE
    g_uart_state.ota_fw_handle = -1;
    g_uart_state.initialized = TRUE; // 标记UART层已初始化

    // 创建 OTA 依赖的定时器
    // 1. 帧接收定时器 (在 lt_uart_notify_cb 中启动)
    ret = ql_rtos_timer_create(&g_uart_state.recv_timer_ref, NULL, ota_recv_frame_timeout_cb, NULL);
    if (ret != QL_OSI_SUCCESS)
    {
        QL_UART_OTA_DEMO_LOG("Create ota_recv_timer fail");
    }
    // 2. 命令响应定时器 (在 ota_send_command_internal 中启动)
    ret = ql_rtos_timer_create(&g_uart_state.response_timer_ref, NULL, ota_response_timeout_cb, NULL);
    if (ret != QL_OSI_SUCCESS)
    {
        QL_UART_OTA_DEMO_LOG("Create ota_response_timer fail");
    }

    /*开启一个uart信息处理的msg——queue*/
    ql_rtos_queue_create(&lt_ota_msg_queue, sizeof(lt_ota_msg_t), 5);

    /*开启串口信息处理线程*/
    err = ql_rtos_task_create(&uartota_task, 40969, APP_PRIORITY_NORMAL,
                              "ltuartota", lt_ota_msg_thread, NULL, 5);
    if (err != QL_OSI_SUCCESS)
    {
        QL_UART_OTA_DEMO_LOG("ltuartota task created failed");
        return;
    }
    // 初始化 OTA 模块 (ec800g_ota_master.c)
    ec_ota_init();
    QL_UART_OTA_DEMO_LOG("OTA module initialized.");
    // ======== MODIFICATION END ========
}