/**
 * @file lt_ebs.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-08-08
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#include "lt_ebs.h"
#include "ebm_task_play.h"
#include "ebm_task_schedule.h"
#include "ebs_param.h"
#include "ip_gb_comm.h"
#include "ql_api_datacall.h"
#include "ql_api_dev.h"
#include "ql_api_nw.h"
#include "ql_api_datacall.h"
#include "lt_datacall.h"
#include "ltsystem.h"
static ql_task_t lt_ebs_play_task;

static void lt_lp_ebs_callback_register()
{
    lt_lp_callback_cfg_t reg;
    reg.lp_id = EBS_PRJ;
    reg.lt_lpdown_callback = NULL;
    reg.lt_lpup_callback = NULL;
    lt_lp_callback_register(&reg);
}

int lt_ebs_app_init(void)
{
    QlOSStatus err = QL_OSI_SUCCESS;

    ebs_log("lootom ebs app init\n");
    lt_lp_ebs_callback_register();//注册低功耗回调函数
    ebs_param_init(0);

    if (ebs_comm_task_init() != 0)
    {
        ebs_log("ebs_comm_task_init failed\n");
        return -1;
    }

    eb_player_init(0);
    err = ql_rtos_task_create(&lt_ebs_play_task, 1024 * 20, APP_PRIORITY_NORMAL, "ebs_play", ebs_task_play_thread, NULL, 0);
    if (err != QL_OSI_SUCCESS)
    {
        ebs_log("ebs_play task create failed\n");
        return -1;
    }

    ebs_log("lootom ebs app init success\n");
    return 0;
}
