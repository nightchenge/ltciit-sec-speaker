/**
 * @file ebm_task_play.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-12-07
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _EBM_TASK_PLAY_H_
#define _EBM_TASK_PLAY_H_
#include "ebm_task_type.h"

int eb_player_channel_priority_set(char *priority);
void eb_player_channel_priority_get(uint8_t *priority);
void eb_player_platform_priority_set(bool is_upper_first);
void eb_player_channel_switch_set(bool is_chn_switch);
void eb_player_ipsc_mode_set(bool is_ipsc);
void eb_player_fm_remain_set(bool status, uint16_t remain_time);
bool eb_player_fm_remain_query(void);
void eb_player_fm_remain_timer_reload(void);
task_result_t eb_player_task_msg_send(ebm_task_msg_t *msg);
int ebs_play_task_status_query(char *channel, uint8_t *ebm_type, uint8_t *event_level, uint8_t *volume);
uint8_t ebs_player_task_ebm_type_query(void);
task_source_t ebs_player_task_source_query(void);
task_result_t eb_player_current_task_delete(void);
task_result_t eb_player_all_task_delete(void);

void ebs_task_play_thread(void *arg);
int32_t eb_player_init(uint32_t stage);
int32_t eb_player_deinit(void);
#if PLATFORM == PLATFORM_Ex800G
void eb_player_prepare_sucess_callback_set(int (*callback)(void));
#endif

extern lt_ebs_player_t eb_player;

#endif
