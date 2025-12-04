/*
 * @Author: zhaixianwen
 * @Date: 2023-09-07 10:21:26
 * @LastEditTime: 2024-06-24 16:16:43
 * @LastEditors: zhouhao
 * @Description: 短信处理模块
 * @FilePath: /LTE01R02A01_BETA0726_C_SDK_G/components/ql-application/ltmain/src/ltsms/ltsms.c
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_api_common.h"
#include "ql_api_osi.h"
#include "ql_api_sim.h"
#include "ql_api_sms.h"
#include "ql_log.h"
#include "ltkey.h"
#include "ql_api_voice_call.h"
#include "ql_power.h"
#include "ltsms_t.h"
#include "ltclk_t.h"
#include "lt_tts_demo.h"
#include "ltplay.h"
#include "ltconf.h"

ql_task_t sms_task = NULL;
ql_sem_t sms_init_sem = NULL;
ql_sem_t sms_list_sem = NULL;
static char s_index = 0xff;
void user_sms_event_callback(uint8_t nSim, int event_id, void *ctx)
{
	switch (event_id)
	{
	case QL_SMS_INIT_OK_IND:
	{
		QL_SMS_LOG("QL_SMS_INIT_OK_IND");
		ql_rtos_semaphore_release(sms_init_sem);
		break;
	}
	case QL_SMS_NEW_MSG_IND:
	{
		ql_sms_new_s *msg = (ql_sms_new_s *)ctx;
		QL_SMS_LOG("sim=%d, index=%d, storage memory=%d", nSim, msg->index, msg->mem);
		 s_index=msg->index;
		break;
	}
	case QL_SMS_LIST_IND:
	{
		break;
	}
	case QL_SMS_LIST_EX_IND:
	{
		ql_sms_recv_s *msg = (ql_sms_recv_s *)ctx;
		QL_SMS_LOG("index=%d,os=%s,tooa=%u,status=%d,fo=0x%x,dcs=0x%x,scst=%d/%d/%d %d:%d:%d±%d,uid=%u,total=%u,seg=%u,dataLen=%d,data=%s",
				   msg->index, msg->oa, msg->tooa, msg->status, msg->fo, msg->dcs,
				   msg->scts.uYear, msg->scts.uMonth, msg->scts.uDay, msg->scts.uHour, msg->scts.uMinute, msg->scts.uSecond, msg->scts.iZone,
				   msg->uid, msg->msg_total, msg->msg_seg, msg->dataLen, msg->data);
		s_index = msg->index;

		break;
	}
	case QL_SMS_LIST_END_IND:
	{
		QL_SMS_LOG("QL_SMS_LIST_END_IND");
		ql_rtos_semaphore_release(sms_list_sem);
		break;
	}
	case QL_SMS_MEM_FULL_IND:
	{
		ql_sms_new_s *msg = (ql_sms_new_s *)ctx;
		QL_SMS_LOG("QL_SMS_MEM_FULL_IND sim=%d, memory=%d", nSim, msg->mem);
		break;
	}
	case QL_SMS_REPORT_IND:
	{
		ql_sms_report_s *msg = (ql_sms_report_s *)ctx;
		QL_SMS_LOG("QL_SMS_REPORT_IND sim=%d, fo=%02x, mr=%02x, st=%02x, ra_size=%d, tora=%02x, ra=%s, scst=%d/%d/%d %d:%d:%d,%d, dt=%d/%d/%d %d:%d:%d,%d",
				   nSim, msg->fo, msg->mr, msg->st, msg->ra_size, msg->tora, (char *)&msg->ra,
				   msg->scts.uYear, msg->scts.uMonth, msg->scts.uDay, msg->scts.uHour, msg->scts.uMinute, msg->scts.uSecond, msg->scts.iZone,
				   msg->dt.uYear, msg->dt.uMonth, msg->dt.uDay, msg->dt.uHour, msg->dt.uMinute, msg->dt.uSecond, msg->dt.iZone);
		break;
	}
	default:
		break;
	}
}
void instruct_play_handle(void *buf,int date_len)
{
	QL_SMS_LOG("instruct_play_handle buf==%s,datelen==%d",(char *)buf,date_len);

}

void lt_simCheck_reboot()
{
	QL_SMS_LOG("lt_simCheck_reboot");
	char *sim_check = "检测到电话卡插入,系统将在5秒后重启";
	ltapi_play_tts(sim_check, strlen(sim_check));
	ql_rtos_task_sleep_s(10);
	ql_power_reset(RESET_NORMAL);
}

// sim卡热插拔回调
static void lt_user_sim_hotplug_callback(uint8_t physical_sim_id, ql_sim_hotplug_status_e status)
{
	if (QL_SIM_HOTPLUG_STATUS_IN == status)
	{

		QL_SMS_LOG("sim=%d,status=%d:plug in !!!", physical_sim_id, status);
		lt_simCheck_reboot();
	}
	else
	{
		QL_SMS_LOG("sim=%d,status=%d:plug out !!!", physical_sim_id, status);
	}
}
//sim卡热插拔初始化接口
void lt_sim_hotplug_init()
{
	ql_sim_errcode_e ret = QL_SIM_SUCCESS;
	uint8_t physical_sim_id = 0;

	ql_pin_set_func(103, 3);
	ql_sim_hotplug_register_cb(lt_user_sim_hotplug_callback);

	ql_sim_hotplug_gpio_s gpio_cfg = {GPIO_44, LVL_LOW};
	ret = ql_sim_set_hotplug(physical_sim_id,
							 QL_SIM_HOTPLUG_ENABLE,
							 &gpio_cfg);
	QL_SMS_LOG("ql_sim1_set_hotplug ret:0x%x", ret);
}
//去掉号码中的国家码。防止白名单对比出错
void removeCountryCode(char *phoneNumber) {
    // 检查字符串是否以"+86"开头
    if (strncmp(phoneNumber, "+86", 3) == 0) {
        // 将字符串中的"+86"部分去掉
        memmove(phoneNumber, phoneNumber + 3, strlen(phoneNumber));
    }
}
void lt_sms_task(void *param)
{
	char addr[20] = {0};
	uint8_t nSim = 0;
	uint8_t sms_init_done = false;
	int sms_flag = 0;				//sms flag
	unsigned char SmsBuf[5][SMS_MAX];	//短信暂存buf
	int SmsLen = 0;				//短信总长度				
	QL_SMS_LOG("enter");

	lt_sim_hotplug_init();//注册sim卡的热插拔


	if (ql_rtos_semaphore_create(&sms_init_sem, 0) != QL_OSI_SUCCESS)
	{
		QL_SMS_LOG("sms_init_sem created failed");
		goto exit;
	}

	if (ql_rtos_semaphore_create(&sms_list_sem, 0) != QL_OSI_SUCCESS)
	{
		QL_SMS_LOG("sms_init_sem created failed");
		goto exit;
	}


	ql_sms_callback_register(user_sms_event_callback);
	// wait sms ok
	ql_sms_get_init_status(nSim, &sms_init_done);
	if (!sms_init_done)
	{
		if (ql_rtos_semaphore_wait(sms_init_sem, QL_WAIT_FOREVER))
		{
			QL_SMS_LOG("Waiting for SMS init timeout");
		}
	}

	if (QL_SMS_SUCCESS == ql_sms_get_center_address(nSim, addr, sizeof(addr)))
	{
		QL_SMS_LOG("ql_sms_get_center_address OK, addr=%s", addr);
	}
	else
	{
		QL_SMS_LOG("ql_sms_get_center_address FAIL");
	}

	ql_sms_set_code_mode(QL_CS_GSM);
	ql_sms_set_storage(nSim, SM, SM, SM);
	// set sms storage as SIM.
	ql_rtos_task_sleep_s(3); 
	if (QL_SMS_SUCCESS == ql_sms_read_msg_list(nSim, TEXT)) // 获取短信list
	{
		if (ql_rtos_semaphore_wait(sms_list_sem, QL_WAIT_FOREVER))
		{
			QL_SMS_LOG("sms_list_sem time out");
		}
	}
	else{
		QL_SMS_LOG("get msg list FAIL");
	}
	static bool long_sms_full =  TRUE; //sms长短信收全的标志
	while (1)
	{
		ql_rtos_task_sleep_s(1); // 1秒轮询一次新短信

		if (s_index != 0xff || !long_sms_full)
		{
			if (QL_SMS_SUCCESS == ql_sms_read_msg_list(nSim, TEXT)) // 获取短信list
			{
				if (ql_rtos_semaphore_wait(sms_list_sem, QL_WAIT_FOREVER))
				{
					QL_SMS_LOG("sms_list_sem time out");
				}
			}
			else
			{
				QL_SMS_LOG("get msg list FAIL");
			}

			ql_sms_stor_info_s stor_info;
			if (QL_SMS_SUCCESS == ql_sms_get_storage_info(nSim, &stor_info))
			{
				QL_SMS_LOG("ql_sms_get_storage_info OK");
				QL_SMS_LOG("SM used=%u,SM total=%u,SM unread=%u,ME used=%u,ME total=%u,ME unread=%u, newSmsStorId=%u",
						   stor_info.usedSlotSM, stor_info.totalSlotSM, stor_info.unReadRecordsSM,
						   stor_info.usedSlotME, stor_info.totalSlotME, stor_info.unReadRecordsME,
						   stor_info.newSmsStorId);
			}
			else
			{
				QL_SMS_LOG("ql_sms_get_storage_info FAIL");
			}
			ql_sms_recv_s *sms_recv = NULL;

			while (stor_info.unReadRecordsSM--)
			{
				sms_recv = (ql_sms_recv_s *)calloc(1, sizeof(ql_sms_recv_s));
				if (sms_recv == NULL)
				{
					QL_SMS_LOG("calloc FAIL");
					continue;
				}
				QL_SMS_LOG("s_index-stor_info.unReadRecordsSM == %d", s_index - stor_info.unReadRecordsSM);
				if (QL_SMS_SUCCESS == ql_sms_read_msg_ex(nSim, s_index - stor_info.unReadRecordsSM, TEXT, sms_recv))
				{
					QL_SMS_LOG("index=%d,os=%s,tooa=%u,status=%d,fo=0x%x,dcs=0x%x,scst=%d/%d/%d %d:%d:%d±%d,uid=%u,total=%u,seg=%u,dataLen=%d,data=%s",
							   sms_recv->index, sms_recv->oa, sms_recv->tooa, sms_recv->status, sms_recv->fo, sms_recv->dcs,
							   sms_recv->scts.uYear, sms_recv->scts.uMonth, sms_recv->scts.uDay, sms_recv->scts.uHour, sms_recv->scts.uMinute, sms_recv->scts.uSecond, sms_recv->scts.iZone,
							   sms_recv->uid, sms_recv->msg_total, sms_recv->msg_seg, sms_recv->dataLen, sms_recv->data);

					QL_SMS_LOG("TEST1: msg_total = %d", sms_recv->msg_total);
					if (sms_recv->msg_total < 6)
					{
						char sms_num_cl[QL_TEL_MAX_LEN*4+1]; //去掉国家码之后的号码
						
						memcpy(sms_num_cl,sms_recv->oa,sizeof(sms_recv->oa));
						removeCountryCode(sms_num_cl);
						if (sms_recv->msg_total > 1)
						{
							long_sms_full =  FALSE;
							QL_SMS_LOG("TEST1: seg = %d", sms_recv->msg_seg);

							SmsLen += sms_recv->dataLen;

							for (int i = 0; i < SMS_MAX; i++)
							{
								SmsBuf[sms_recv->msg_seg - 1][i] = sms_recv->data[i];
							}

							sms_flag++;

							if (sms_flag == sms_recv->msg_total)
							{
								long_sms_full = TRUE;
								unsigned char sms_data[SMS_MAX * sms_flag];
								int k = 0;

								for (int i = 0; i < sms_flag; i++)
								{
									for (int j = 0; j < SMS_MAX; j++)
									{
										sms_data[k++] = SmsBuf[i][j];
									}
								}


								
								if (AUTHED == phone_number_auth_verif(sms_num_cl, strlen(sms_num_cl)))
								{
									QL_SMS_LOG("phone_number_auth_verif SUCCESS");
									lt_tts_instruct_add(TTS_TYPE_SMS, sms_data, SmsLen, instruct_play_handle, NULL); // 添加播放任务
								}
								else
								{
									QL_SMS_LOG("phone_number_auth_verif FAIL");
								}
								QL_SMS_LOG("TEST2: datalen = %d", SmsLen);
								sms_flag = 0;
								SmsLen = 0;
								memset(sms_data, 0, sizeof(sms_data));
								memset(SmsBuf, 0, sizeof(SmsBuf));
							}
						}
						else
						{
								if (AUTHED == phone_number_auth_verif(sms_num_cl, strlen(sms_num_cl)))
								{
									QL_SMS_LOG("phone_number_auth_verif SUCCESS");
									lt_tts_instruct_add(TTS_TYPE_SMS, sms_recv->data, sms_recv->dataLen, instruct_play_handle, NULL); // 添加播放任务
								}
								else
								{
									QL_SMS_LOG("phone_number_auth_verif FAIL");
								}

						}
					}
					else
						QL_SMS_LOG("sms_total more than 5!!!");
				}
				else
				{
					QL_SMS_LOG("read sms FAIL");
				}
				if (sms_recv)
				{
					free(sms_recv);
				}
			}
			int w_index = stor_info.usedSlotSM;
			if (s_index >= w_index)
			{
				QL_SMS_LOG("ql_sms_delete_msg s_index is wrong, %d", s_index);
				if (ql_sms_delete_msg(nSim, s_index) != QL_SMS_SUCCESS)
				{
					QL_SMS_LOG("ql_sms_delete_msg FAIL, %d", s_index);
				}
			}
			if (stor_info.usedSlotSM >= stor_info.totalSlotSM - 5)
			{
				int del_index = stor_info.usedSlotSM;
				while (--del_index)
				{
					if (ql_sms_delete_msg(nSim, del_index) != QL_SMS_SUCCESS)
					{
						QL_SMS_LOG("ql_sms_delete_msg FAIL, %d", del_index);
					}
				}
			}
			s_index= 0xff;
		}
	}
	goto exit;
exit:
	if (sms_init_sem)
	{
		ql_rtos_semaphore_delete(sms_init_sem);
	}
	if (sms_list_sem)
	{
		ql_rtos_semaphore_delete(sms_list_sem);
	}
	ql_rtos_task_delete(NULL);
}

// static void lt__tts_stop_callback_register()
// {
//     lt_tts_callback_t reg = {
// 		.lt_tts_start_handle = ltplay_checkPA,
//         .lt_tts_stop_handle = lt_tts_instruct_stop,
//     };
//     lt_tts_callback_register(&reg);
// } 

QlOSStatus lt_sms_init(void)
{
	QlOSStatus err = QL_OSI_SUCCESS;

	err = ql_rtos_task_create(&sms_task,  4*1024, APP_PRIORITY_NORMAL, "ltsms", lt_sms_task, NULL, 2);
	if (err != QL_OSI_SUCCESS)
	{
		QL_SMS_LOG("sms_task created failed, ret = 0x%x", err);
	}
//	lt__tts_stop_callback_register();
	ql_task_t sms_t_task = NULL;

	err = ql_rtos_task_create(&sms_t_task, 10*1024, APP_PRIORITY_NORMAL, "sms_t_task", lt_sms_t_task, NULL, 2);
	if (err != QL_OSI_SUCCESS)
	{
		QL_SMS_LOG("sms_t_task created failed, ret = 0x%x", err);
	} 
	ql_task_t clk_t_task = NULL;
	err = ql_rtos_task_create(&clk_t_task, 10*1024, APP_PRIORITY_NORMAL, "clk_t_task", lt_clk_t_task, NULL, 2);
	if (err != QL_OSI_SUCCESS)
	{
		QL_SMS_LOG("lt_clk_t_task created failed, ret = 0x%x", err);
	}

	return err;
}
