/**
 * @file ebm_type.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-12-09
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */

#ifndef _EBM_TASK_TYPE_H_
#define _EBM_TASK_TYPE_H_
#include "ebs.h"
#include "ebs_gb_config.h"
#include "util_circ.h"
#include <stdint.h>
#include <stdbool.h>
#if PLATFORM == PLATFORM_LINUX
#include "util_msgq.h"
#include <pthread.h>
#elif PLATFORM == PLATFORM_Ex800G
#include "util_msg.h"
#endif

#define TASK_RESULT_OK 0
#define TASK_SUSPEND_OK TASK_RESULT_OK
#define TASK_ERR_BASE (-1)
#define TASK_ERR_NULL (TASK_ERR_BASE - 0)      // 任务指针为空
#define TASK_ERR_MAX_CNT (TASK_ERR_BASE - 1)   // 达到任务最大数量
#define TASK_ERR_MALLOC (TASK_ERR_BASE - 2)    // 分配内存失败
#define TASK_ERR_EMPTY (TASK_ERR_BASE - 3)     // 列表为空
#define TASK_ERR_FIND (TASK_ERR_BASE - 4)      // 未发现任务
#define TASK_ERR_LOCK (TASK_ERR_BASE - 5)      // 任务被占用
#define TASK_ERR_EXIST (TASK_ERR_BASE - 6)     // 任务已存在
#define TASK_ERR_MSG_SND (TASK_ERR_BASE - 7)   // 任务发送队列失败
#define TASK_ERR_MSG_STAT (TASK_ERR_BASE - 8)  // 消息队列相关错误
#define TASK_ERR_NO_SWITCH (TASK_ERR_BASE - 9) // 任务调度中没有切换
typedef int task_result_t;

#define EBM_TASK_MAX_CNT 32

typedef enum
{
    TASK_SOURCE_EBS = 0,
    TASK_SOURCE_MQTT,
    TASK_SOURCE_MQTT_2,
    TASK_SOURCE_MQTT_3,
    TASK_SOURCE_CLK,
    TASK_SOURCE_SHOUT,
    TASK_SOURCE_AUDIOURL,
    TASK_SOURCE_FMURL
} task_source_t;

typedef enum task_add_del_type
{
    TASK_ADD_DEL_NULL = 0,
    TASK_ADD,
    TASK_DEL,
    TASK_TS_EB_ADD,
    TASK_DVBC_EB_DEL, // DVB-C通道应急任务删除
    TASK_DTMB_EB_DEL,
    TASK_FM_DEL, // FM通道所有任务删除
#if PLATFORM == PLATFORM_Ex800G
    TASK_CUR_DEL, // 当前任务删除
    TASK_ALL_DEL, // 所有任务删除
#endif
} task_add_del_type_t;

typedef enum audio_source
{
    AUDIO_NONE = 0, // 没有选择的通道
    AUDIO_RTSP,
    AUDIO_RTP,
    AUDIO_UDP,
    AUDIO_FM,
    AUDIO_HTTP,
} audio_source_t;

#ifdef PROT_GUANGXI
typedef enum mpeg_type
{
    MPEG_MP3 = 1,
    MPEG_MP2,
    MPEG_AAC,
} mpeg_type_t;
#endif

typedef enum task_status
{
    TASK_STATUS_NULL = 0,
    TASK_STATUS_WAIT = 1,
    TASK_STATUS_READY = 2,
    TASK_STATUS_RUNNING = 4,
    TASK_STATUS_CMPLT = 8,
    TASK_STATUS_STOP = 16,
    TASK_STATUS_INTRPT = 32,
    TASK_STATUS_ERROR = 64,
    TASK_STATUS_EXIT = 128,
} task_status_t;

typedef enum play_result
{
    PLAY_RESULT_ERR,
    PLAY_RESULT_OK,
    PLAY_RESULT_RUN,
    PLAY_RESULT_NULL,
} play_result_t;

typedef struct ebm_task
{
    task_result_t (*play_result_report)(struct ebm_task *task);
    void (*callback)(void *task);
    task_source_t task_source;
    task_status_t status;
    play_result_t play_result;
    char *play_result_info;
    uint16_t info_len;
    uint8_t play_times;
    audio_source_t audio_source;
#ifdef PROT_GUANGXI
    mpeg_type_t mpeg_type;
    uint8_t src_lvl;
#endif
    uint8_t chn_mask;
    uint8_t volume;
    uint8_t ebm_type;
    uint8_t severity;
    time_t start_time;
    time_t stop_time;
    time_t add_time;
    time_t real_start_time;
    time_t real_stop_time;
    uint32_t fm_freq;
    bool is_ip_fm;
    int run_err_times;
    uint8_t ebm_id[EBM_ID_LEN];
    uint8_t event_type[EVENT_TYPE_LEN];
    char url[256];
    struct ebm_task *prev, *next;
} ebm_task_t;

typedef struct ebm_task_msg
{
    uint8_t type;
    uint8_t buff[512];
} ebm_task_msg_t;

typedef struct ebm_task_list
{
    ebm_task_t *head;
    ebm_task_t *tail;
    int task_num;
} ebm_task_list_t;

typedef struct lt_ebs_player
{
    ebm_task_t *task;
    ebm_task_t *task_high;
    task_result_t (*task_add_del_process)(struct lt_ebs_player *player);
    task_result_t (*schedule)(struct lt_ebs_player *player);
    int (*prepare)(struct lt_ebs_player *player, ebm_task_t *task);
#if PLATFORM == PLATFORM_Ex800G
    int (*prepare_sucess_callback)(void);
#endif
    int (*play)(struct lt_ebs_player *player, ebm_task_t *task);
    int (*finish)(struct lt_ebs_player *player, ebm_task_t *task);
    int (*play_switch_report)(ebm_task_t *task, uint8_t status);
    void (*target_volume_update)(struct lt_ebs_player *player);
    volume_t (*volume_gradient)(struct lt_ebs_player *player, volume_t volume);
    ebm_task_list_t *task_list;
    // mqueue_t *task_queue;
    ebs_queue_t task_queue;
    int prepare_times;
    // int run_err_times;
    bool is_fm_remain;
    bool is_upper_first;
    bool is_chn_switch;
    bool is_ipsc;
    time_t timer_fm_remain;
    uint32_t fm_remain_period;
    time_t timer_log;
    time_t timer_delay;
    volume_t vol;
    volume_t vol_target;
    float vol_step;
    uint8_t play_chn;
    uint8_t chn_prio[CHN_MAX_CNT];
} lt_ebs_player_t;

#endif
