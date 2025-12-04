/**
 * @file ebm_task_schedule.h
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-12-07
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#ifndef _EBM_TASK_SCHEDULE_H_
#define _EBM_TASK_SCHEDULE_H_
#include "ebs.h"
#include "ebm_task_type.h"

task_result_t ebm_task_add_del_process(lt_ebs_player_t *this);
task_result_t ebm_task_schedule(lt_ebs_player_t *this);

#endif
