/*
 * @Descripttion: Volume Knob Implementation (Event-Driven)
 * @version: 2.0
 * @Author: zhouhao
 * @Date: 2025-11-19
 */

#include "ql_api_osi.h"
#include "ql_log.h"
#include "ql_gpio.h"
#include "ltsystem.h"
#include "volume_knob.h"
#include "ql_audio.h"
#include "ltfm.h"

// --- 日志定义---
#define QL_APP_VOL_LOG_LEVEL QL_LOG_LEVEL_INFO
#define QL_VOLDEMO_LOG(msg, ...) QL_LOG(QL_APP_VOL_LOG_LEVEL, "lt_vol", msg, ##__VA_ARGS__)

// --- 宏定义 ---
#define PIN_ENCODER_A GPIO_25
#define PIN_ENCODER_B GPIO_24
#define VOLUME_STEP 7

// 定义事件ID 
#define CMD_EVENT_VOLUME_CHANGE (0x1001)
// 全局变量
volatile int volume = 50;

// 任务句柄 (必须是全局的，以便中断回调可以使用它)
// 参考 lsm6ds3tr.c 中的 ql_task_t lsm_task = NULL;
static ql_task_t volume_knob_task = NULL;

static on_volume_changed on_volume_changed_cb = NULL;

void ltapi_set_on_volume_changed(on_volume_changed cb)
{
    on_volume_changed_cb = cb;
}

void ltapi_set_knobvolume(int vol)
{
    if (vol > volume)
    {
        volume = vol;
    }
    if (volume_knob_task != NULL)
    {
        ql_event_t event = {0};

        // 2. 组装事件
        event.id = CMD_EVENT_VOLUME_CHANGE;

        event.param1 = 0;
            // 3. 发送事件 (参考 lsm6ds3tr.c: ql_rtos_event_send(lsm_task, &event);)
        ql_rtos_event_send(volume_knob_task, &event);
    }
}
// ====================================================================
// ===== 中断回调函数 (参考 _gpioint_callback01)                  =====
// ====================================================================
static void encoder_isr_callback(void *ctx)
{
    // 只有当任务句柄有效时才发送
    if (volume_knob_task != NULL)
    {
        ql_int_disable(PIN_ENCODER_A);
        ql_LvlMode level_b;
        ql_event_t event = {0};

        // 1. 获取B相电平判断方向
        ql_gpio_get_level(PIN_ENCODER_B, &level_b);

        // 2. 组装事件
        event.id = CMD_EVENT_VOLUME_CHANGE;

        // 利用 param1 传递方向参数：1 表示顺时针，-1 表示逆时针
        // 这样主线程就知道是加音量还是减音量
        event.param1 = (level_b == LVL_HIGH) ? 1 : -1;
        // 3. 发送事件 (参考 lsm6ds3tr.c: ql_rtos_event_send(lsm_task, &event);)
        ql_rtos_event_send(volume_knob_task, &event);
    }
}

// ====================================================================
// ===== 主任务线程 (参考 lsm6ds3tr_thread)                       =====
// ====================================================================
static void volume_knob_thread(void *arg)
{
    int last_volume = -1;
    int mapped_volume = 0;
    int fm_volume = 0;
    ql_event_t event = {0};

    QL_VOLDEMO_LOG("音量旋钮任务启动 (Event Driven)");

    // 1. 初始化GPIO
    ql_gpio_init(PIN_ENCODER_A, GPIO_INPUT, PULL_UP, 0);
    ql_gpio_init(PIN_ENCODER_B, GPIO_INPUT, PULL_UP, 0);

    // 2. 注册并使能中断
    // 参考 lsm6ds3tr.c 的中断注册流程
    ql_int_register(PIN_ENCODER_A, EDGE_TRIGGER, DEBOUNCE_EN, EDGE_FALLING, PULL_UP, encoder_isr_callback, NULL);
    ql_int_enable(PIN_ENCODER_A);

    ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_VOICE, 6);
    ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_LOCAL, 6);
    QL_VOLDEMO_LOG("中断初始化完成");

    while (1)
    {
        // QL_WAIT_FOREVER 意味着如果没有旋转，任务将完全休眠，不占用任何 CPU
        if (ql_event_wait(&event, QL_WAIT_FOREVER) != 0)
        {
            continue; // 发生错误继续等待
        }

        switch (event.id)
        {
        case CMD_EVENT_VOLUME_CHANGE:
        {
            // 从事件参数中提取方向
            int direction = (int)event.param1;

            // 计算新音量
            int delta = direction * VOLUME_STEP;
            volume += delta;

            // 限制范围
            volume = (volume > 100) ? 100 : (volume < 0) ? 0
                                                         : volume;

            // 只有音量实际变化时才执行耗时的音频设置操作
            if (volume != last_volume)
            {
                // 映射音量逻辑
                mapped_volume = (volume * 11 + 50) / 100;
                mapped_volume = (mapped_volume > 11) ? 11 : mapped_volume;

                fm_volume = (volume * 49 + 50) / 100;
                fm_volume = (fm_volume > 49) ? 49 : fm_volume;

                QL_VOLDEMO_LOG("音量更新: Vol=%d, Map=%d, FM=%d", volume, mapped_volume, fm_volume);

                // 执行音频设置 (这些操作比较耗时，放在主线程做是完美的)
                ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_VOICE, mapped_volume);
                ql_aud_set_volume(QL_AUDIO_PLAY_TYPE_LOCAL, mapped_volume);

                if (on_volume_changed_cb)
                {
                    on_volume_changed_cb(mapped_volume);
                }

                lt_fm_setvol(fm_volume);

                last_volume = volume;
            }
        }
        default:
            ql_int_enable(PIN_ENCODER_A);
            break;
        }
    }
}

// --- 应用入口函数 ---
int lt_volume_knob_init(void)
{
    int ret = 0;

    // 创建任务并赋值给全局句柄 volume_knob_task
    // 参考 lsm6ds3tr.c: ql_rtos_task_create(&lsm_task, ...)
    ret = ql_rtos_task_create(&volume_knob_task,
                              4096,                // 栈大小
                              APP_PRIORITY_NORMAL, // 优先级
                              "VolumeKnobThread",  // 任务名
                              volume_knob_thread,  // 任务函数
                              NULL,                // 参数
                              10);                 // 事件队列深度 (建议设大一点防止快速旋转丢包)

    if (ret != QL_OSI_SUCCESS)
    {
        QL_VOLDEMO_LOG("创建音量旋钮任务失败");
        return -1;
    }

    QL_VOLDEMO_LOG("应用程序初始化成功");
    return 0;
}