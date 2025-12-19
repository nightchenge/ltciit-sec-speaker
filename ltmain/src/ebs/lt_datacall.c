/**
 * @file lt_datacall.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2023-08-21
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#if PLATFORM == PLATFORM_Ex800G
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_api_osi.h"
#include "ql_api_nw.h"
#include "ql_api_datacall.h"
#include "ebs_param.h"
#include "ltsystem.h"
#include "dynamic_config.h"

#define QL_SIM_NUM 2

static ql_task_t lt_datacall_task;

typedef struct
{
    bool state;
    ql_sem_t act_sem;
    ql_sem_t deact_sem;
} datacall_context_s;

ql_sem_t lt_data_reg_sem[QL_SIM_NUM] = {0};
datacall_context_s datacall_ctx[QL_SIM_NUM][PROFILE_IDX_NUM] = {0};
uint8_t sim_id = 0;
int profile_idx = 1;
ql_nw_reg_status_info_s nw_info = {0};
ql_data_call_info_s info = {0};
ql_nw_operator_info_s oper_info = {0};

static void lt_nw_ind_callback(uint8_t sim_id, unsigned int ind_type, void *ind_msg_buf)
{
    if (QUEC_NW_DATA_REG_STATUS_IND == ind_type)
    {
        ql_nw_common_reg_status_info_s *data_reg_status = (ql_nw_common_reg_status_info_s *)ind_msg_buf;
        if ((QL_NW_REG_STATE_HOME_NETWORK == data_reg_status->state) || (QL_NW_REG_STATE_ROAMING == data_reg_status->state))
        {
            ql_rtos_semaphore_release(lt_data_reg_sem[sim_id]);
        }
    }
}

static void lt_datacall_ind_callback(uint8_t sim_id, unsigned int ind_type, int profile_idx, bool result, void *ctx)
{
    if (profile_idx < PROFILE_IDX_MIN || profile_idx > PROFILE_IDX_MAX)
    {
        return;
    }
    ql_task_t *task = (ql_task_t *)ctx;
    switch (ind_type)
    {
    case QUEC_DATACALL_ACT_RSP_IND: // only received in asyn mode
    {
        datacall_ctx[sim_id][profile_idx - 1].state = (result == true) ? QL_PDP_ACTIVED : QL_PDP_DEACTIVED;
        ql_rtos_semaphore_release(datacall_ctx[sim_id][profile_idx - 1].act_sem);
        break;
    }
    case QUEC_DATACALL_DEACT_RSP_IND: // only received in asyn mode
    {
        datacall_ctx[sim_id][profile_idx - 1].state = (result == true) ? QL_PDP_DEACTIVED : QL_PDP_ACTIVED;
        ql_rtos_semaphore_release(datacall_ctx[sim_id][profile_idx - 1].deact_sem);
        break;
    }
    case QUEC_DATACALL_PDP_DEACTIVE_IND: // received in both asyn mode and sync mode
    {
        datacall_ctx[sim_id][profile_idx - 1].state = QL_PDP_DEACTIVED;

        ql_event_t event = {0};
        event.id = QUEC_DATACALL_PDP_DEACTIVE_IND;
        event.param1 = profile_idx;
        ql_rtos_event_send(*task, &event);
        break;
    }
    }
}

static int lt_data_call(ql_task_t *task)
{
    int ret = 0;

    ql_rtos_semaphore_create(&lt_data_reg_sem[sim_id], 0);
    ret = ql_nw_register_cb(lt_nw_ind_callback);
    if (ret != 0)
    {
        ebs_log("ql_nw_register_cb failed, ret=%d", ret);
        ql_rtos_semaphore_delete(lt_data_reg_sem[sim_id]);
        return -1;
    }

    ret = ql_nw_get_reg_status(sim_id, &nw_info);
    if ((QL_NW_REG_STATE_HOME_NETWORK != nw_info.data_reg.state) && (QL_NW_REG_STATE_ROAMING != nw_info.data_reg.state))
    {
        ql_rtos_semaphore_wait(lt_data_reg_sem[sim_id], QL_WAIT_FOREVER);
    }

    ret = ql_nw_get_operator_name(sim_id, &oper_info);
    ebs_log("ret=0x%x, long_oper_name:%s, short_oper_name:%s, mcc:%s, mnc:%s",
            ret, oper_info.long_oper_name, oper_info.short_oper_name, oper_info.mcc, oper_info.mnc);

// #if TYPE_FUNC == TYPE_QIXIA
    if (get_type_func() == TYPE_QIXIA)
    {
        // 栖霞定制 新增广电卡限制
        if (strstr(oper_info.short_oper_name, "CBN") == NULL)
        {
            ebs_log("short_oper_name is not CBN!!");
            ql_rtos_semaphore_delete(lt_data_reg_sem[sim_id]);
            while (1)
            {
                ql_rtos_task_sleep_ms(2000);
            }
            // return -1;
        }
    }
// #endif

    ret = ql_datacall_register_cb(sim_id, profile_idx, lt_datacall_ind_callback, task);
    ebs_log("ql_datacall_register_cb ret=%d", ret);

    ret = ql_set_data_call_asyn_mode(sim_id, profile_idx, 1);
    ql_rtos_semaphore_create(&datacall_ctx[sim_id][profile_idx - 1].act_sem, 0);
    ql_rtos_semaphore_create(&datacall_ctx[sim_id][profile_idx - 1].deact_sem, 0);

    ret = ql_start_data_call(sim_id, profile_idx, QL_PDP_TYPE_IP, "cmnet", NULL, NULL, 0);
    if (0 != ret)
    {
        ebs_log("ql_start_data_call failed, ret=%d", ret);
        goto exit;
    }

    ql_rtos_semaphore_wait(datacall_ctx[sim_id][profile_idx - 1].act_sem, QL_WAIT_FOREVER);
    ebs_log("datacall state=%d", datacall_ctx[sim_id][profile_idx - 1].state);

    ret = ql_get_data_call_info(sim_id, profile_idx, &info);
    ebs_log("ql_get_data_call_info ret=%d", ret);
    ebs_log("info.profile_idx: %d, info.ip_version: %d", info.profile_idx, info.ip_version);
    ebs_log("info.v4.state: %d, info.v6.state: %d", info.v4.state, info.v6.state);
    if (info.v4.state)
    {
        ebs_log("info.v4.addr.ip: %s", ip4addr_ntoa(&(info.v4.addr.ip)));
        ebs_log("info.v4.addr.pri_dns: %s", ip4addr_ntoa(&(info.v4.addr.pri_dns)));
        ebs_log("info.v4.addr.sec_dns: %s", ip4addr_ntoa(&(info.v4.addr.sec_dns)));
        ebs_nms.ip = info.v4.addr.ip.addr;
    }
    return 0;

exit:
    ql_rtos_semaphore_delete(lt_data_reg_sem[sim_id]);
    ql_rtos_semaphore_delete(datacall_ctx[sim_id][profile_idx - 1].act_sem);
    ql_rtos_semaphore_delete(datacall_ctx[sim_id][profile_idx - 1].deact_sem);
    ql_datacall_unregister_cb(sim_id, profile_idx, lt_datacall_ind_callback, task);

    return -1;
}

static void lt_datacall_app_task(void *arg)
{
    ql_task_t *task = (ql_task_t *)arg;
    int ret = 0, retry_count = 0;
    ql_event_t event;

    if (lt_data_call(task) != 0)
    {
        ebs_log("lt_data_call failed");
        return;
    }

    while (1)
    {
        if (ql_event_try_wait(&event) != 0)
        {
            continue;
        }

        if (QUEC_DATACALL_PDP_DEACTIVE_IND == event.id)
        {
            ebs_sleep(5);

            ret = ql_nw_get_reg_status(sim_id, &nw_info);
            if ((QL_NW_REG_STATE_HOME_NETWORK != nw_info.data_reg.state) && (QL_NW_REG_STATE_ROAMING != nw_info.data_reg.state))
            {
                ql_rtos_semaphore_wait(lt_data_reg_sem[sim_id], QL_WAIT_FOREVER);
            }

            ret = ql_set_data_call_asyn_mode(sim_id, profile_idx, 0);
            while (((ret == ql_start_data_call(sim_id, profile_idx, QL_PDP_TYPE_IP, "cmnet", NULL, NULL, 0)) != 0) && (retry_count < 10))
            {
                retry_count++;
                ebs_sleep(20);
            }

            if (QL_DATACALL_SUCCESS == ret)
            {
                retry_count = 0;
                datacall_ctx[sim_id][profile_idx - 1].state = QL_PDP_ACTIVED;
                ret = ql_get_data_call_info(sim_id, profile_idx, &info);
                if (info.v4.state)
                {
                    ebs_log("info.v4.addr.ip: %s", ip4addr_ntoa(&(info.v4.addr.ip)));
                    ebs_log("info.v4.addr.pri_dns: %s", ip4addr_ntoa(&(info.v4.addr.pri_dns)));
                    ebs_log("info.v4.addr.sec_dns: %s", ip4addr_ntoa(&(info.v4.addr.sec_dns)));
                    ebs_nms.ip = info.v4.addr.ip.addr;
                }
            }
        }
    }
    ebs_nms.ip = 0;

    ql_rtos_task_delete(*task);
}

int lt_datacall_app_init(void)
{
    QlOSStatus err = QL_OSI_SUCCESS;

    err = ql_rtos_task_create(&lt_datacall_task, 1024 * 10, APP_PRIORITY_NORMAL, "lt_datacall_app_task", lt_datacall_app_task, &lt_datacall_task, 10);
    if (err != QL_OSI_SUCCESS)
    {
        ebs_log("datacall_app_task task create failed\n");
        return -1;
    }
    return 0;
}

#endif