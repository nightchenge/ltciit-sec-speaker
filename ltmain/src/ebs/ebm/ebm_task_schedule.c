/**
 * @file ebm_task_schedule.c
 * @author lijintang (lijintang@lootom.com)
 * @brief
 * @version 0.1
 * @date 2021-12-07
 *
 * @copyright Copyright (c) Lootom Telcovideo Network Wuxi Co.,Ltd.
 *
 */
#include "ebm_task_schedule.h"
#include "ip_gb_comm.h"

#if PLATFORM == PLATFORM_LINUX
#include "util_msgq.h"
#include "ebs_log.h"
#include <arpa/inet.h>
#elif PLATFORM == PLATFORM_Ex800G
#include "util_msg.h"
#include "arpa/inet.h"
#endif
#include <string.h>
#include <stdlib.h>

static ebm_task_t ebm_task_arry[EBM_TASK_MAX_CNT] = {0};

static ebm_task_t *malloc_task(void)
{
    int index = 0;
    uint8_t ebm_id[EBM_ID_LEN] = {0};
    for (index = 0; index < EBM_TASK_MAX_CNT; index++)
    {
        if (!memcmp(ebm_task_arry[index].ebm_id, ebm_id, EBM_ID_LEN))
            return &ebm_task_arry[index];
    }
    return NULL;
}

static task_result_t free_task(ebm_task_t *task)
{
    int index;
    for (index = 0; index < EBM_TASK_MAX_CNT; index++)
    {
        if (!memcmp(ebm_task_arry[index].ebm_id, task->ebm_id, EBM_ID_LEN))
        {
            memset(&ebm_task_arry[index], 0, sizeof(ebm_task_t));
            return TASK_RESULT_OK;
        }
    }
    return TASK_ERR_FIND;
}

static ebm_task_t *find_task_in_list(ebm_task_list_t *list, uint8_t *ebm_id, direction_t d)
{
    if (list->task_num <= 0)
        return NULL;

    ebm_task_t *task = d ? list->tail : list->head;
    int deep = 0;
    while (task != NULL && deep < EBM_TASK_MAX_CNT)
    {
        if (!memcmp(task->ebm_id, ebm_id, EBM_ID_LEN))
        {
            return task;
        }
        task = d ? task->prev : task->next;
        deep++;
    }
    return NULL;
}

// static ebm_task_t *find_task_in_list_by_status(ebm_task_list_t *list, task_status_t status, direction_t d)
// {
//     if (list->task_num <= 0)
//         return NULL;

//     ebm_task_t *task = d ? list->tail : list->head;
//     int deep = 0;
//     while (task != NULL && deep < EBM_TASK_MAX_CNT)
//     {
//         if (task->status && status)
//         {
//             return task;
//         }
//         task = d ? task->prev : task->next;
//         deep++;
//     }
//     return NULL;
// }

// static ebm_task_t *find_playing_task_in_list(ebm_task_list_t *list, direction_t d)
// {
//     return find_task_in_list_by_status(list, TASK_STATUS_RUNNING, d);
// }

#define CONVERT_TO_BYTES(b, x)     \
    do                             \
    {                              \
        b[0] = ((x) >> 24) & 0xFF; \
        b[1] = ((x) >> 16) & 0xFF; \
        b[2] = ((x) >> 8) & 0xFF;  \
        b[3] = (x)&0xFF;           \
    } while (0)
static task_result_t ebm_task_play_result_report(ebm_task_t *task)
{
    lbk_msg_t msg = {.type = 0, .len = 0, .data = {0}};
    time_t utc;
    lbk_play_result_t *result = (lbk_play_result_t *)msg.data;
    msg.len += sizeof(lbk_play_result_t);
    memcpy(result->ebm_id, task->ebm_id, EBM_ID_LEN);
    result->result_code = task->play_result;
    result->info_len[0] = (task->info_len >> 8) & 0xFF;
    result->info_len[1] = (task->info_len & 0xFF);
    if (task->play_result_info != NULL)
    {
        memcpy(result->info, task->play_result_info, task->info_len);
        msg.len += task->info_len;
        free(task->play_result_info);
    }

    play_result_time_t *play_time = (play_result_time_t *)(&msg.data[msg.len]);
    msg.len += sizeof(play_result_time_t);
    utc = ebs_time(NULL);
    CONVERT_TO_BYTES(play_time->start_time, HTONL(task->real_start_time));
    CONVERT_TO_BYTES(play_time->stop_time, HTONL(task->real_stop_time));
    play_time->play_times = task->play_times;
    CONVERT_TO_BYTES(play_time->report_time, HTONL(utc));
    msg.type = LBK_PLAY_RESULT;
    if (!ebs_lbk_msg_send(&msg))
        return TASK_ERR_MSG_SND;
    return TASK_RESULT_OK;
}

static task_result_t ebm_task_add_to_list(ebm_task_list_t *list, ebm_task_t *task, direction_t d)
{
    if (task == NULL)
        return TASK_ERR_NULL;

    if (list->task_num >= EBM_TASK_MAX_CNT)
        return TASK_ERR_MAX_CNT;

    ebm_task_t *t_add = malloc_task();
    if (t_add == NULL)
        return TASK_ERR_MALLOC;

    memset(t_add, 0, sizeof(ebm_task_t));
    memcpy(t_add, task, sizeof(ebm_task_t));
#ifndef PROT_GUANGXI
    t_add->play_result_report = ebm_task_play_result_report;
#else

#endif
    t_add->next = NULL;
    t_add->prev = NULL;
    t_add->add_time = ebs_time(NULL);
    task->status = TASK_STATUS_WAIT;
    if (list->head == NULL)
    {
        list->head = list->tail = t_add;
        list->task_num = 1;
        return TASK_RESULT_OK;
    }
    if (!d)
    {
        t_add->next = list->head;
        list->head->prev = t_add;
        list->head = t_add;
        list->task_num++;
    }
    else
    {
        t_add->prev = list->tail;
        list->tail->next = t_add;
        list->tail = t_add;
        list->task_num++;
    }
    return TASK_RESULT_OK;
}

static task_result_t ebm_task_del_from_list(ebm_task_list_t *list, ebm_task_t *task, bool is_report, direction_t d)
{
    if (task == NULL)
        return TASK_ERR_NULL;

    if (list->task_num == 0 || (list->head == NULL && list->tail == NULL))
        return TASK_ERR_EMPTY;

    if (task->status == TASK_STATUS_RUNNING) // 当前播放的任务不能清除，需要先标记状态做停播处理后再删除
    {
        task->status = TASK_STATUS_STOP;
        return TASK_SUSPEND_OK;
    }

    ebm_task_t *t_del = NULL;
    if (!memcmp(list->head->ebm_id, task->ebm_id, EBM_ID_LEN))
    {
        t_del = list->head;
        if (list->task_num == 1)
        {
            list->head = NULL;
            list->tail = NULL;
            if (t_del->play_result_report && is_report)
                t_del->play_result_report(t_del);
            free_task(t_del);
            t_del = NULL;
        }
        else
        {
            list->head = t_del->next;
            list->head->prev = NULL;
            if (t_del->play_result_report && is_report)
                t_del->play_result_report(t_del);
            free_task(t_del);
            t_del = NULL;
        }
        list->task_num--;
        return TASK_RESULT_OK;
    }
    else if (!memcmp(list->tail->ebm_id, task->ebm_id, EBM_ID_LEN))
    {
        t_del = list->tail;
        list->tail = t_del->prev;
        list->tail->next = NULL;
        if (t_del->play_result_report && is_report)
            t_del->play_result_report(t_del);
        free_task(t_del);
        t_del = NULL;
        list->task_num--;
        return TASK_RESULT_OK;
    }
    else
    {
        t_del = find_task_in_list(list, task->ebm_id, d);
        if (t_del == NULL)
            return TASK_ERR_FIND;
        ebm_task_t *t_prev = t_del->prev;
        ebm_task_t *t_next = t_del->next;
        t_prev->next = t_next;
        t_next->prev = t_prev;
        if (t_del->play_result_report && is_report)
            t_del->play_result_report(t_del);
        free_task(t_del);
        t_del = NULL;
        list->task_num--;
        return TASK_RESULT_OK;
    }
}

static task_result_t ebm_playing_task_del_from_list(ebm_task_list_t *list)
{
    if (list->task_num == 0)
        return TASK_RESULT_OK;

    int deep = 0;
    ebm_task_t *t_find = list->head;
    while (t_find != NULL && deep < list->task_num)
    {
        if (t_find->status == TASK_STATUS_RUNNING)
        {
            return ebm_task_del_from_list(list, t_find, true, 0);
        }
        t_find = t_find->next;
        deep++;
    }
    return TASK_ERR_FIND;
}

static task_result_t ebm_all_task_del_from_list(ebm_task_list_t *list)
{
    if (list->task_num == 0)
        return TASK_RESULT_OK;

    int deep = 0;
    ebm_task_t *t_find = list->head;
    ebm_task_t *t_find_next = NULL;
    while (t_find != NULL && deep < list->task_num)
    {
        t_find_next = t_find->next;
        ebm_task_del_from_list(list, t_find, true, 0);
        t_find = t_find_next;
        deep++;
    }
    return TASK_RESULT_OK;
}

static uint8_t find_task_highest_channel(lt_ebs_player_t *player, ebm_task_t *task)
{
    uint8_t i;
    for (i = 0; i < CHN_MAX_CNT; i++)
    {
        if (task->chn_mask & player->chn_prio[i])
            return i;
    }
    return 0xFF;
}

static task_result_t task_schedule_common(lt_ebs_player_t *this, ebm_task_t **task)
{
    if (this->task_list->task_num == 0)
        return TASK_ERR_NO_SWITCH;

    ebm_task_t *t_find = NULL, *t_new = NULL, *t_find_next = NULL;
    int deep = 0;
    uint8_t index = 0xFF;
    t_find = this->task_list->head;
    t_new = *task == NULL ? this->task_list->head : *task;
    while (t_find != NULL && deep < this->task_list->task_num)
    {
        t_find_next = t_find->next;
        if (t_find->ebm_type != 5 && t_new->ebm_type != 5)
        {
            if (t_find->ebm_type > t_new->ebm_type)
            {
                t_new = t_find;
            }
            else if (t_find->ebm_type == t_new->ebm_type)
            {
                if (t_find->severity < t_new->severity)
                    t_new = t_find;
                else if (t_find->severity == t_new->severity)
                {
                    if ((t_find->ebm_id[0] & 0xF) == (t_new->ebm_id[0] & 0xF))
                    {
                        if (t_find->add_time < t_new->add_time)
                            t_new = t_find;
                    }
                    else
                    {
                        if (((t_find->ebm_id[0] & 0xF) < (t_new->ebm_id[0] & 0xF)) == this->is_upper_first)
                            t_new = t_find;
                    }
                }
            }
        }
        else if (t_find->ebm_type != 5 && t_new->ebm_type == 5)
        {
            t_new = t_find;
        }
        else if (t_find->ebm_type == 5 && t_new->ebm_type == 5)
        {
            if (t_find->severity < t_new->severity)
                t_new = t_find;
            else if (t_find->severity == t_new->severity)
            {
                if ((t_find->ebm_id[0] & 0xF) == (t_new->ebm_id[0] & 0xF))
                {
                    if (t_find->add_time < t_new->add_time)
                        t_new = t_find;
                }
                else
                {
                    if (((t_find->ebm_id[0] & 0xF) < (t_new->ebm_id[0] & 0xF)) == this->is_upper_first)
                        t_new = t_find;
                }
            }
        }
        t_find = t_find_next;
        deep++;
    }
    if (t_new != *task)
    {
        *task = t_new;
        return TASK_RESULT_OK;
    }
    else
    {
        if (t_new->chn_mask & (t_new->chn_mask - 1))
        {
            if ((index = find_task_highest_channel(this, t_new)) < CHN_MAX_CNT)
            {
                if (this->is_chn_switch && this->task->status == TASK_STATUS_RUNNING && (t_new->chn_mask & this->chn_prio[index]) != this->play_chn)
                    return TASK_RESULT_OK;
            }
        }
    }
    return TASK_ERR_NO_SWITCH;
}

task_result_t ebm_task_add_del_process(lt_ebs_player_t *this)
{
    task_result_t ret;
    ebm_task_t *t = NULL, *t_find = NULL;
#if PLATFORM == PLATFORM_LINUX
    mqueue_msg_t *msg;
    msg = mqueue_timed_get(this->task_queue, 200);
    if (msg != NULL)
    {
        uint8_t type = (uint8_t)mqueue_msg_type_get(msg);
        uint8_t *data = (uint8_t *)mqueue_msg_body(msg);
#elif PLATFORM == PLATFORM_Ex800G
    ebs_msg_t msg = {0};
    uint32_t timeout = ((this->task == NULL) && (this->task_list->task_num == 0)) ? QL_WAIT_FOREVER : 200; //! 当前没有任务在播时，阻塞等待
    if (ebs_msg_queue_wait(this->task_queue, (uint8_t *)&msg, sizeof(ebs_msg_t), timeout) == 0)
    {
        uint8_t type = (uint8_t)msg.type;
        uint8_t *data = (uint8_t *)msg.data;
#endif
        switch (type)
        {
        case TASK_ADD:
        {
            ebs_log("=======EBM TASK ADD=======\n");
            t = (ebm_task_t *)data;
            if ((t_find = find_task_in_list(this->task_list, t->ebm_id, t->ebm_type == 5)) != NULL)
            {
                if (t->chn_mask & CHN_IP_MASK)
                {
                    if (strlen(t_find->url) != strlen(t->url) || memcmp(t_find->url, t->url, strlen(t->url)))
                    {
                        memcpy(t_find->url, t->url, sizeof(t->url));
                        if (this->task == t_find)
                        {
                            this->task->status = TASK_STATUS_INTRPT;
                            this->task_high = t_find;
                        }
                    }
                }
                else if (t->chn_mask & CHN_4G_MASK)
                {
                    if (strlen(t_find->url) != strlen(t->url) || memcmp(t_find->url, t->url, strlen(t->url)))
                    {
                        memcpy(t_find->url, t->url, sizeof(t->url));
                        if (this->task == t_find)
                        {
                            this->task->status = TASK_STATUS_INTRPT;
                            this->task_high = t_find;
                        }
                    }
                }
                else if (t->chn_mask & CHN_FM_MASK)
                {
                    if (t_find->fm_freq != t->fm_freq || t_find->is_ip_fm != t->is_ip_fm)
                    {
                        t_find->fm_freq = t->fm_freq;
                        t_find->is_ip_fm = t->is_ip_fm;
                        if (this->task == t_find)
                        {
                            this->task->status = TASK_STATUS_INTRPT;
                            this->task_high = t_find;
                        }
                    }
                }
                else
                {
                    ebs_msg_free(msg.data);
                    return TASK_ERR_FIND;
                }
                t_find->chn_mask |= t->chn_mask;
                if (t_find->volume != t->volume)
                {
                    t_find->volume = t->volume;
                    this->target_volume_update(this);
                }
                ebs_msg_free(msg.data);
                return TASK_RESULT_OK;
            }
            else
            {
                ret = ebm_task_add_to_list(this->task_list, t, t->ebm_type == 5);
                // mqueue_msg_free(msg);
                ebs_msg_free(msg.data);
                return ret;
            }
        }
        break;
        case TASK_DEL:
        {
            ebs_log("=======EBM TASK DEL=======\n");
            uint8_t i;
            char ebm_id[EBM_ID_LEN * 2] = {0};
            for (i = 0; i < EBM_ID_LEN; i++)
            {
                sprintf(ebm_id + i * 2, "%02X", data[i]);
            }
            ebs_log("ebm id:%s \r\n", ebm_id);
            if ((t_find = find_task_in_list(this->task_list, data, 0)) != NULL)
            {
                ret = ebm_task_del_from_list(this->task_list, t_find, true, t_find->ebm_type == 5);
                // mqueue_msg_free(msg);
                ebs_msg_free(msg.data);
                return ret;
                // return ebm_task_del_from_list(this->task_list, t_find, true, t_find->ebm_type == 5);
            }
            else
            {
                ebs_msg_free(msg.data);
                return TASK_ERR_FIND;
            }
        }
        break;
        case TASK_CUR_DEL:
        {
            ebs_log("=======EBM TASK CUR DEL=======\n");
            return ebm_playing_task_del_from_list(this->task_list);
        }
        break;
        case TASK_ALL_DEL:
        {
            ebs_log("=======EBM TASK ALL DEL=======\n");
            return ebm_all_task_del_from_list(this->task_list);
        }
        break;
        default:
            ebs_msg_free(msg.data);
            break;
        }
    }
    return TASK_ERR_EMPTY;
}

task_result_t ebm_task_schedule(lt_ebs_player_t *this)
{
    ebm_task_t *t_find = NULL;
    ebm_task_t *t_find_next = NULL;
    ebm_task_t *t_new = NULL;
    task_result_t result = TASK_RESULT_OK;
    int deep = 0;
    if (this->task_list->task_num == 0)
        return TASK_ERR_EMPTY;

    if (this->task_list->task_num) //! 遍历，TASK_STATUS_WAIT->TASK_STATUS_READY 删除已finish任务
    {
        deep = 0;
        t_find = this->task_list->head;
        while (t_find != NULL && deep < this->task_list->task_num)
        {
            t_find_next = t_find->next;
            if (t_find->status == TASK_STATUS_WAIT)
            {
                t_find->status = TASK_STATUS_READY;
            }
            else if (t_find->status == TASK_STATUS_EXIT)
            {
                if ((result = ebm_task_del_from_list(this->task_list, t_find, true, t_find->ebm_type == 5)) != TASK_RESULT_OK)
                    ebs_log("task del error:%d \n", result);
            }
            t_find = t_find_next;
            deep++;
        }
    }

    if (this->task_list->task_num)
    {
        if (this->task != NULL)
        {
            if (this->task->status > TASK_STATUS_RUNNING)
                return result;
            else if (this->task->status == TASK_STATUS_NULL)
                this->task = NULL;
        }

        t_new = this->task;
        if (TASK_RESULT_OK == task_schedule_common(this, &t_new))
        {
            ebs_log("Current task(%p), switch task(%p)\n", this->task, t_new);
            if (this->task == t_new)
            {
                this->task->status = TASK_STATUS_INTRPT;
                this->task_high = t_new;
            }
            else
            {
                if (NULL != this->task)
                {
                    this->task->status = TASK_STATUS_INTRPT;
                    t_new->status = TASK_STATUS_READY;
                    this->task_high = t_new;
                }
                else
                {
                    this->task = t_new;
                    this->task->status = TASK_STATUS_READY;
                }
            }
        }
    }
    return result;
}
