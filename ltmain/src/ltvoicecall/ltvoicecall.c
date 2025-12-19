/*
 * @Author: zhaixianwen
 * @Date: 2023-09-07 10:21:26

 * @LastEditTime: 2025-12-10 09:47:06
 * @LastEditors: ubuntu
 * @Description: 电话处理模块
 * @FilePath: /LTE01R02A01_BETA0726_C_SDK_G/components/ql-application/ltmain/src/ltsms/ltsms.c
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_api_common.h"
#include "ql_api_osi.h"
#include "ql_api_sms.h"
#include "ql_api_sim.h"
#include "ql_log.h"
#include "ltkey.h"
#include "ql_api_voice_call.h"
#include "ql_codec_config.h"
#include "ltvoicecall.h"
#include "ltconf.h"
#include "led.h"
#include "ltmqtt.h"
#include "ebs.h"
#include "time.h"
#include "ltplay.h"
#include "ql_gpio.h"
#include "ql_audio.h"
#include "ql_i2c.h"
#include "lt_tts_demo.h"
#include "ltuart2frx8016.h"
#include "ltsystem.h"
#include "ltrecord.h"
#include "ltsms_t.h"
#include "lsm6ds3tr.h"
#include "ql_api_nw.h"
#define LT_VOICE_CALL_LOG_LEVEL QL_LOG_LEVEL_INFO
#define LT_VOICE_CALL_LOG(msg, ...) QL_LOG(LT_VOICE_CALL_LOG_LEVEL, "ltvoicecall", msg, ##__VA_ARGS__)

ql_codec_reg_t g_es8311CloseRegList[] = ES8311_CLOSE_CONFIG;

/* === begin: family number list definitions === */
#define FAMILY_LIST_NUM 6
typedef struct
{
	int id;
	char name[32];
	char number[20];
} family_t;

typedef struct
{
	unsigned int family_count;
	unsigned int family_index;
	family_t family[FAMILY_LIST_NUM];
} family_list_t;

/* 静态示例数据：按需替换为从 MQTT/flash/配置读取 */

static family_list_t g_family_list ;

static void g_family_list_init()
{
	unsigned int family_index_t = 0;
	family_index_t = g_family_list.family_index;
	memset(&g_family_list, 0, sizeof(g_family_list));
	g_family_list.family_index = family_index_t;
	// char familyPhone[256] = "18360730746,18360730746;18360730746,18360730746,18360730746";
	char familyPhone[512] = {0};
	char familyname[256] = {0};

	if (mqtt_param_phone_get(familyPhone, sizeof(familyPhone), FAMILY_TYPE) == 0)
	{
		char *token;
		char *rest = familyPhone;
		int index = 0;

		while ((token = strtok_r(rest, ",", &rest)) && index < FAMILY_LIST_NUM)
		{

			g_family_list.family_count++;
			g_family_list.family[index].id = index;
			strncpy(g_family_list.family[index].number, token, sizeof(g_family_list.family[index].number) - 1);
			index++;
		}
	}
	if (mqtt_param_phone_get(familyname, sizeof(familyname), FAMILY_NAME_TYPE) == 0)
	{
		char *token;
		char *rest = familyname;
		int index = 0;
		while ((token = strtok_r(rest, ",", &rest)) && index < FAMILY_LIST_NUM)
		{
			strncpy(g_family_list.family[index].name, token, sizeof(g_family_list.family[index].name) - 1);
			index++;
		}
	}
	int i = 0;
	for (i = 0; i < g_family_list.family_count; i++)
	{
		LT_VOICE_CALL_LOG("family %d: %s, %s", g_family_list.family[i].id, g_family_list.family[i].name, g_family_list.family[i].number);
	}
}

//电话定时器，目前只在sos主动呼出时使用。
ql_timer_t lt_voicecall_pool_timer = NULL;
//电话定时器，目前只在sos号码被叫主动接听时使用。
ql_timer_t lt_voicecall_called_timer = NULL;

ql_timer_t lt_voicecall_96296_timer = NULL;
typedef struct lt_voice_call
{
	uint8_t nSim;
	ql_task_t vc_task;
	vc_callback_msg_t msg;
	vc_callback status_onchanged;
	uint8_t sos_index;//当前轮询到的号码
	uint8_t sos_num;//sos包含的号码个数
	time_t sos_Time;// 把触发sos的时间用作id号 
	bool    hang_up;//是否主动挂断
	bool    sos_next;
    /* 新增亲情号码当前索引 */
    uint8_t family_index;

} lt_voice_call_t;

static lt_voice_call_t ltvoicecall = {
	.msg = {0},
	.vc_task = NULL,
	.nSim = 0,
	.status_onchanged = NULL,
	.sos_next =  FALSE,
};

static void lt_voice_call_answer();
void lt_get_gnsss_t(CellInfo *cell)
{
		ql_nw_cell_info_s *cell_info = (ql_nw_cell_info_s *)calloc(1, sizeof(ql_nw_cell_info_s));
       int ret = ql_nw_get_cell_info(ltvoicecall.nSim, cell_info);
        LT_VOICE_CALL_LOG("ret=0x%x", ret);
		int cell_index = 0;
        if(cell_info->gsm_info_valid)
        {
            for(cell_index = 0; cell_index < cell_info->gsm_info_num; cell_index++)
            {
                LT_VOICE_CALL_LOG("Cell_%d [GSM] flag:%d, cid:0x%X, mcc:%d, mnc:%02d, lac:0x%X, arfcn:%d, bsic:%d, rssi:%d",
                cell_index,
                cell_info->gsm_info[cell_index].flag,
                cell_info->gsm_info[cell_index].cid,
                cell_info->gsm_info[cell_index].mcc,
                cell_info->gsm_info[cell_index].mnc,
                cell_info->gsm_info[cell_index].lac,
                cell_info->gsm_info[cell_index].arfcn,
                cell_info->gsm_info[cell_index].bsic,
				cell_info->gsm_info[cell_index].rssi);
            }
        }
        if(cell_info->lte_info_valid)
        {
            for(cell_index = 0; cell_index < cell_info->lte_info_num; cell_index++)
            {
                LT_VOICE_CALL_LOG("Cell_%d [LTE] flag:%d, cid:0x%X, mcc:%d, mnc:%02d, tac:0x%X, pci:%d, earfcn:%d, rssi:%d",
                cell_index,
                cell_info->lte_info[cell_index].flag,
                cell_info->lte_info[cell_index].cid,
                cell_info->lte_info[cell_index].mcc,
                cell_info->lte_info[cell_index].mnc,
                cell_info->lte_info[cell_index].tac,
                cell_info->lte_info[cell_index].pci,
                cell_info->lte_info[cell_index].earfcn,
				cell_info->lte_info[cell_index].rssi);
				cell->cellid =  cell_info->lte_info[cell_index].cid;
				cell->lac = cell_info->lte_info[cell_index].tac;
				cell->mcc = cell_info->lte_info[cell_index].mcc;
				cell->mnc =  cell_info->lte_info[cell_index].mnc;
				
            }
        }
}

ql_audio_errcode_e app_es8311_write_reg(uint8 RegAddr, uint8 RegData)
{
	uint8 retry_count = 5;

	while (retry_count--)
	{
		if (QL_I2C_SUCCESS == ql_I2cWrite(i2c_2, ES8311_I2C_SLAVE_ADDR, RegAddr, &RegData, 1))
		{
			return QL_AUDIO_SUCCESS;
		}
	}
	return QL_AUDIO_CODEC_WR_FAIL;
}

void app_es8311_write_list(ql_codec_reg_t *regList, uint16_t len)
{
	uint16_t regCount;

	for (regCount = 0; regCount < len; regCount++)
	{
		app_es8311_write_reg(regList[regCount].addr, regList[regCount].data);
	}
}

void lt_voice_call_status_set(voice_call_status_t st)
{
	if (ltvoicecall.msg.callType == st)
	{
		return;
	}

	if (st == STATUS_CALLED ||
		st == STATUS_CALL_FAMILY ||
		st == STATUS_CALL_SOS ||
		st == STATUS_CALL_WATER_SERVICE ||
		st == STATUS_CALL_ELEC_SERVICE ||
		st == STATUS_CALL_GAS_SERVICE)
	{
		ltvoicecall.msg.callRingTime = ebs_time(NULL);
	}

	if (st == STATUS_CALLED_CONNECT ||
		st == STATUS_CALL_FAMILY_CONNECT ||
		st == STATUS_CALL_SOS_CONNECT ||
		st == STATUS_CALL_WATER_CONNECT ||
        st == STATUS_CALL_ELEC_CONNECT ||
        st == STATUS_CALL_GAS_CONNECT)
	{
		ltvoicecall.msg.callState = 1;
		ltvoicecall.msg.callStartTime = ebs_time(NULL);
	}

	if (ltvoicecall.msg.callType == STATUS_CALLED_CONNECT ||
		ltvoicecall.msg.callType == STATUS_CALL_FAMILY_CONNECT ||
		ltvoicecall.msg.callType == STATUS_CALL_SOS_CONNECT ||
		ltvoicecall.msg.callType == STATUS_CALL_WATER_CONNECT ||
        ltvoicecall.msg.callType == STATUS_CALL_ELEC_CONNECT ||
        ltvoicecall.msg.callType == STATUS_CALL_GAS_CONNECT)
	{
		if (st == STATUS_NONE)//接通后结束
		{
			ltvoicecall.msg.callState = 0;
			ltvoicecall.msg.callEndTime = ebs_time(NULL);
			ltvoicecall.msg.callTime = difftime(ltvoicecall.msg.callEndTime, ltvoicecall.msg.callStartTime);

			mqtt_call_msg_t call_msg = {
				.callTime = ltvoicecall.msg.callTime,
				.callType = ltvoicecall.msg.callType == STATUS_CALLED_CONNECT ? 1 : 0,
				.callStartTime = ltvoicecall.msg.callStartTime,
				.callEndTime = ltvoicecall.msg.callEndTime,
				.callState = 1};
				if (ltvoicecall.msg.callType == STATUS_CALL_SOS_CONNECT)
				{
					call_msg.callId = ltvoicecall.sos_Time;
				}
			sprintf(call_msg.callNumber, "%s", ltvoicecall.msg.callNumber);
			api_mqtt_CallLog_Publish(&call_msg);
		}
	}
	if (ltvoicecall.msg.callType == STATUS_CALLED ||
		ltvoicecall.msg.callType == STATUS_CALL_FAMILY ||
		ltvoicecall.msg.callType == STATUS_CALL_SOS ||
		ltvoicecall.msg.callType == STATUS_CALL_WATER_SERVICE ||
        ltvoicecall.msg.callType == STATUS_CALL_ELEC_SERVICE ||
        ltvoicecall.msg.callType == STATUS_CALL_GAS_SERVICE)
	{
		if (st == STATUS_NONE)//未接通结束
		{
			ltvoicecall.msg.callState = 0;
			ltvoicecall.msg.callEndTime = ebs_time(NULL);
			ltvoicecall.msg.callTime = difftime(ltvoicecall.msg.callEndTime, ltvoicecall.msg.callStartTime);

			mqtt_call_msg_t call_msg = {
				.callTime = 0,
				.callType = ltvoicecall.msg.callType == STATUS_CALLED ? 1 : 0,
				.callStartTime = ltvoicecall.msg.callRingTime,
				.callEndTime = ltvoicecall.msg.callEndTime,
				.callState = 0};
				if (ltvoicecall.msg.callType == STATUS_CALL_SOS)
				{
					call_msg.callId = ltvoicecall.sos_Time;
				}
			sprintf(call_msg.callNumber, "%s", ltvoicecall.msg.callNumber);
			api_mqtt_CallLog_Publish(&call_msg);
		}
	}	

	do
	{
		ltvoicecall.msg.callType = st;

		if (ltvoicecall.msg.callType == STATUS_NONE)
		{
			ltapi_record(RECORD_STOP,NULL, 0); //结束录音
#if PACK_TYPE == PACK_NANJING
			set_blink("water", 0);
			set_blink("gas", 0);
			set_blink("elec", 0);
#else
			set_blink("voice", 0);
#endif
			set_blink("sos", 0);
			ltplay_check_play(SND_STOP);
			set_function_state(RT_TIME, 0);

			ql_gpio_set_direction(GPIO_35, GPIO_OUTPUT);
			ql_gpio_set_level(GPIO_35, LVL_LOW);
			app_es8311_write_list(g_es8311CloseRegList, sizeof(g_es8311CloseRegList) / sizeof(ql_codec_reg_t));

			memset(&ltvoicecall.msg, 0x00, sizeof(ltvoicecall.msg));
		}
		else
		{
			ltplay_check_play(SND_TEL);
			set_function_state(CALL, 0);
			if (ltvoicecall.msg.callType == STATUS_CALL_SOS || ltvoicecall.msg.callType == STATUS_CALL_SOS_CONNECT)
			{
				set_blink("sos",2);
			}
#if PACK_TYPE == PACK_NANJING
			else if (ltvoicecall.msg.callType == STATUS_CALL_WATER_SERVICE)
			{
				set_blink("water", 2);
			}
			else if (ltvoicecall.msg.callType == STATUS_CALL_ELEC_SERVICE)
			{
				set_blink("elec", 2);
			}
			else if (ltvoicecall.msg.callType == STATUS_CALL_GAS_SERVICE)
			{
				set_blink("gas", 2);
			}
#else
			set_blink("voice",2);
#endif
		}

		if (ltvoicecall.status_onchanged != NULL)
		{
			ltvoicecall.status_onchanged(&ltvoicecall.msg);
		}
	} while (0);
}

static void lt_user_voice_call_event_callback(uint8_t sim, int event_id, void *ctx)
{
	ql_event_t vc_event = {0};
	ql_vc_info_s *vc_info = NULL;

	switch (event_id)
	{
	case QL_VC_RING_IND:
		// MT call, incoming a call and ring.
		vc_info = (ql_vc_info_s *)ctx;
		LT_VOICE_CALL_LOG("1QL_VC_RING_IND TEL: sim:%d,idx:%d,dir:%d,status:%d,numstr:%s",
						  sim, vc_info->idx, vc_info->direction, vc_info->status, vc_info->number);
		memcpy(ltvoicecall.msg.callNumber, vc_info->number, sizeof(ltvoicecall.msg.callNumber));
		LT_VOICE_CALL_LOG("2QL_VC_RING_IND TEL: sim:%d,idx:%d,dir:%d,status:%d,numstr:%s",
						  sim, vc_info->idx, vc_info->direction, vc_info->status, ltvoicecall.msg.callNumber);
		break;
	case QL_VC_CCWA_IND:
		// MT call, incoming call again during a call.
		vc_info = (ql_vc_info_s *)ctx;
		LT_VOICE_CALL_LOG("QL_VC_CCWA_IND TEL: sim:%d,idx:%d,dir:%d,status:%d,numstr:%s",
						  sim, vc_info->idx, vc_info->direction, vc_info->status, vc_info->number);
		memcpy(ltvoicecall.msg.callNumber, vc_info->number, sizeof(ltvoicecall.msg.callNumber));
		break;
	case QL_VC_DIAL_IND:
		// MO call, dial call to the network.
		vc_info = (ql_vc_info_s *)ctx;
		LT_VOICE_CALL_LOG("QL_VC_DIAL_IND TEL: sim:%d,idx:%d,dir:%d,status:%d,numstr:%s",
						  sim, vc_info->idx, vc_info->direction, vc_info->status, vc_info->number);
		memcpy(ltvoicecall.msg.callNumber, vc_info->number, sizeof(ltvoicecall.msg.callNumber));
		break;
	case QL_VC_ALERT_IND:
		// MO call, destination is alert.
		vc_info = (ql_vc_info_s *)ctx;
		LT_VOICE_CALL_LOG("QL_VC_ALERT_IND TEL: sim:%d,idx:%d,dir:%d,status:%d,numstr:%s",
						  sim, vc_info->idx, vc_info->direction, vc_info->status, vc_info->number);
		memcpy(ltvoicecall.msg.callNumber, vc_info->number, sizeof(ltvoicecall.msg.callNumber));
		break;
	case QL_VC_CONNECT_IND:
		vc_info = (ql_vc_info_s *)ctx;
		// MO or MT call connected.
		LT_VOICE_CALL_LOG("QL_VC_CONNECT_IND TEL: sim:%d,idx:%d,dir:%d,status:%d,numstr:%s",
						  sim, vc_info->idx, vc_info->direction, vc_info->status, vc_info->number);
		memcpy(ltvoicecall.msg.callNumber, vc_info->number, sizeof(ltvoicecall.msg.callNumber));
		break;

	/*
	 * When the call ends, both events QL_VC_DISCONNECT_IND and QL_VC_NOCARRIER_IND are sent
	 */
	case QL_VC_DISCONNECT_IND:
		// MO or MT call disconnected.
		vc_info = (ql_vc_info_s *)ctx;
		LT_VOICE_CALL_LOG("QL_VC_DISCONNECT_IND TEL: sim:%d,idx:%d,dir:%d,status:%d,numstr:%s",
						  sim, vc_info->idx, vc_info->direction, vc_info->status, vc_info->number);
		memcpy(ltvoicecall.msg.callNumber, vc_info->number, sizeof(ltvoicecall.msg.callNumber));
		vc_event.param2 = vc_info->status;
		break;
	case QL_VC_NOCARRIER_IND:
		// MO or MT call disconnected. refer to ql_vc_errcode_e for reason
		LT_VOICE_CALL_LOG("QL_VC_NOCARRIER_IND, reason = %d", *(uint32 *)ctx);
		break;
	default:
		break;
	}

	vc_event.id = event_id;  
	vc_event.param1 = sim;
	// 
	ql_rtos_event_send(ltvoicecall.vc_task, &vc_event);
}
static void sosPhone_list_cb();
void lt_voice_call_task(void *param)
{                                  
	ql_event_t vc_event = {0};
	while (1)
	{
		ql_event_wait(&vc_event, QL_WAIT_FOREVER);
		LT_VOICE_CALL_LOG("event id = 0x%x,status = %d", vc_event.id,vc_event.param2);
		switch (vc_event.id)
		{
		case QL_VC_RING_IND:
		{
			ltvoicecall.sos_next = FALSE;
			LT_VOICE_CALL_LOG("ltvoicecall.msg.callNumber == %s",ltvoicecall.msg.callNumber);
			if (phone_number_auth_verif(ltvoicecall.msg.callNumber,strlen(ltvoicecall.msg.callNumber)) != AUTHED)
			{
				LT_VOICE_CALL_LOG("phone_number_auth_verif success!!");
				ql_voice_call_end(ltvoicecall.nSim);
				break;
			}
			lt_voice_call_status_set(STATUS_CALLED);

			LT_VOICE_CALL_LOG("phone_number_auth_verif success!!");
			if (sos_number_auth_verif(ltvoicecall.msg.callNumber, strlen(ltvoicecall.msg.callNumber)) == AUTHED)
			{
				ql_rtos_timer_start(lt_voicecall_called_timer, 3000, 0);
			}
			break;
		}
		case QL_VC_CONNECT_IND: 
		{
			set_function_state(CALL,0);
			record_generate_file_name(ltvoicecall.msg.callNumber); //设置录音文件名
    		ltapi_record(RECORD_VOICE,NULL, 0);	//开始录音

			switch (lt_voice_call_status_get())
			{
			case STATUS_CALL_FAMILY:
				lt_voice_call_status_set(STATUS_CALL_FAMILY_CONNECT);
				set_blink("voice",1);
				break;
#if PACK_TYPE == PACK_NANJING
			case STATUS_CALL_WATER_SERVICE:
				lt_voice_call_status_set(STATUS_CALL_WATER_CONNECT);
				set_blink("water", 1);
				break;
			case STATUS_CALL_ELEC_SERVICE:
				lt_voice_call_status_set(STATUS_CALL_ELEC_CONNECT);
				set_blink("elec", 1);
				break;
			case STATUS_CALL_GAS_SERVICE:
				lt_voice_call_status_set(STATUS_CALL_GAS_CONNECT);
				set_blink("gas", 1);
				break;
#endif
			case STATUS_CALL_SOS:
				lt_voice_call_status_set(STATUS_CALL_SOS_CONNECT);
				// 新增 ： 跌倒时候接通上报处理
				if(get_fall_call_flag() == 1)
				{
					lt_fall_call_pub(3);
				}
				set_blink("sos",1); 
				break;
			case STATUS_CALLED:
				lt_voice_call_status_set(STATUS_CALLED_CONNECT);
				set_blink("voice",1);
				break;
			default:
				break;
			}
			LT_VOICE_CALL_LOG("CONNECT");
			break;
		}
		case QL_VC_DISCONNECT_IND:
		{
			LT_VOICE_CALL_LOG("QL_VC_DISCONNECT_IND");
			if ((lt_voice_call_status_get() == STATUS_CALL_SOS) && (vc_event.param2 != 0 ) && !ltvoicecall.hang_up)
			{
				ltvoicecall.sos_next = TRUE;
				LT_VOICE_CALL_LOG("QL_VC_DISCONNECT_IND, reson = %d", vc_event.param2);
				lt_voice_call_status_set(STATUS_NONE);
				sosPhone_list_cb(); // 呼叫下一个列表
				break;
			}
		}
		case QL_VC_NOCARRIER_IND:
		{
			if(!ltvoicecall.sos_next)
			{
				ltvoicecall.sos_next = FALSE;
				LT_VOICE_CALL_LOG("sos_next, reson = %d", vc_event.param2);
				lt_voice_call_status_set(STATUS_NONE);
			}
			LT_VOICE_CALL_LOG("NOCARRIER, reson = %d", vc_event.param2);
			break;
		}
		default:
			ltvoicecall.sos_next = FALSE;
			break;
		}
	}
	ql_rtos_task_delete(NULL);
}

static void family_key_record_stop_cb()
{
	lt_record_data_stop_handle();
	return;
}

static void family_key_record_cb()
{
	ltapi_play_tts(TTS_STR_VCMD_QUERY, strlen(TTS_STR_VCMD_QUERY));
	ql_rtos_task_sleep_s(5);			/* 延时等tts播放完成 */
	lt_record_data_handle();			/* 录音处理并上报 */
	return;
}

static void family_announce_current(void)
{
    if (g_family_list.family_count == 0)
    {
        ltapi_play_tts(TTS_STR_NO_FAMILY_NUM, strlen(TTS_STR_NO_FAMILY_NUM));
        return;
    }
    /* 播报格式：当前亲情号码是 XXX */
    char tts_buf[128];
	if(strlen(g_family_list.family[g_family_list.family_index].name)==0)
	{	
		char tts_number[32];
		formatNumbersForTTS(g_family_list.family[g_family_list.family_index].number, tts_number);
		snprintf(tts_buf, sizeof(tts_buf), "当前待呼出号码是 %s",tts_number);
	}
	else
	{
		snprintf(tts_buf, sizeof(tts_buf), "当前待呼出对象是 %s", g_family_list.family[g_family_list.family_index].name);
	}
    ltapi_play_tts(tts_buf, strlen(tts_buf));
    /* 视 TTS 长度给一点延时，保证播完（可调整） */
    ql_rtos_task_sleep_s(3);
}

/* 长按:播报当前亲情号码 */
void family_key_longclick_cb()
{
	g_family_list_init();
	if (g_family_list.family_count == 0)
	{
		ltapi_play_tts(TTS_STR_NO_FAMILY_NUM, strlen(TTS_STR_NO_FAMILY_NUM));
		return;
	}
	family_announce_current();
}
// 双击：切换到下一个并播报
void family_key_double_cb()
{
	g_family_list_init();
	if (g_family_list.family_count == 0)
	{
		ltapi_play_tts(TTS_STR_NO_FAMILY_NUM, strlen(TTS_STR_NO_FAMILY_NUM));
		return;
	}
	// fm_info_t *this = &ltfm_info;
	if (ql_tts_is_running())
	{
		ltapi_stop_tts_encoding();
		ql_rtos_task_sleep_ms(500);
	}
	/* 循环切换 */
	g_family_list.family_index = (g_family_list.family_index + 1) % g_family_list.family_count;

	/* 播报新选择的亲情号码 */
	family_announce_current();
	// family_announce_current();
}
void family_key_repeat_cb(void *data)
{
	// fm_info_t *this = &ltfm_info;
	int shutdown_repat = *((int *)data);

	if (shutdown_repat % 15 != 0)
	{
		return;
	}
	if (ql_tts_is_running())
	{
		ltapi_stop_tts_encoding();
		ql_rtos_task_sleep_ms(500);
	}
	/* 循环切换 */
	// ltvoicecall.family_index = (ltvoicecall.family_index + 1) % g_family_count;

	g_family_list.family_index = (g_family_list.family_index + 1) % g_family_list.family_count;
	/* 播报新选择的亲情号码 */
	family_announce_current();
}

static void family_key_click_cb_new()
{
    ql_sim_status_e card_status = 0;
    ql_sim_get_card_status(0, &card_status);

    if (card_status == QL_SIM_STATUS_NOSIM)
    {
        ltapi_play_tts(TTS_STR_NO_SIM_CARD, strlen(TTS_STR_NO_SIM_CARD));
        return;
    }
    if (1 != lt_connect_status_get())
    {
        ltapi_play_tts(TTS_STR_NET_UNAVAILABLE, strlen(TTS_STR_NET_UNAVAILABLE));
        return;
    }
	g_family_list_init();
    voice_call_status_t st = lt_voice_call_status_get();
    if (st == STATUS_NONE)
    {
        if (g_family_list.family_count == 0)
        {
            ltapi_play_tts(TTS_STR_NO_FAMILY_NUM, strlen(TTS_STR_NO_FAMILY_NUM));
            return;
        }

        /* 取当前选择的姓名与号码 */
        const char *call_name = g_family_list.family[g_family_list.family_index].name;
		
        const char *call_number = g_family_list.family[g_family_list.family_index].number;

        /* 播报并发起呼叫 */
		char tts_call[128];
		if (strlen(g_family_list.family[g_family_list.family_index].name) == 0)
		{
			char tts_number[32];
			formatNumbersForTTS(g_family_list.family[g_family_list.family_index].number, tts_number);

			snprintf(tts_call, sizeof(tts_call), "正在呼出%s，请稍候", tts_number);
		}
		else
		{
			snprintf(tts_call, sizeof(tts_call), "正在呼出%s的电话，请稍候", call_name);
		}

		ltapi_play_tts(tts_call, strlen(tts_call));
        /* 给 TTS 一点时间 */
        ql_rtos_task_sleep_s(5);

        LT_VOICE_CALL_LOG("familyPhone number:%s", call_number);
        lt_voice_call_status_set(STATUS_CALL_FAMILY); /* 设置状态为家庭呼叫 */
        ql_rtos_task_sleep_s(1);

        if (ql_voice_call_start(ltvoicecall.nSim, (char *)call_number) != QL_VC_SUCCESS)
        {
            LT_VOICE_CALL_LOG("ql_voice_call_start FAIL");
            return;
        }
        api_yuanLiuMqtt_KeyPhone_Publish(1);
    }
    else if (st == STATUS_CALLED)
    {
		lt_voice_call_answer();
    }
    else
    {
        /* 已有通话：挂断逻辑保持不变 */
        ltvoicecall.sos_index =  0;//当前号码置0
        ltvoicecall.hang_up = TRUE;//手动挂断
        if (ql_voice_call_end(ltvoicecall.nSim) == QL_VC_SUCCESS)
        {
            LT_VOICE_CALL_LOG("ql_voice_call_end SUCCESS");
            if(get_fall_call_flag() == 1)
            {
                lt_fall_call_pub(3);
            }
        }
        else
        {
            LT_VOICE_CALL_LOG("ql_voice_call_end FAIL");
        }
    }
}


static void family_key_click_cb()
{
	ql_sim_status_e card_status = 0;
	ql_sim_get_card_status(0, &card_status);

	if (card_status == QL_SIM_STATUS_NOSIM)
	{
		ltapi_play_tts(TTS_STR_NO_SIM_CARD, strlen(TTS_STR_NO_SIM_CARD));
		return;
	}
	if (1 != lt_connect_status_get())
	{

		ltapi_play_tts(TTS_STR_NET_UNAVAILABLE, strlen(TTS_STR_NET_UNAVAILABLE));
		return;
	}
	voice_call_status_t st = lt_voice_call_status_get();
	if (st == STATUS_NONE)
	{

		char familyPhone[15] = {0};
		if (mqtt_param_phone_get(familyPhone, sizeof(familyPhone), FAMILY_TYPE) == 0)
		{
			lt_voice_call_status_set(STATUS_CALL_FAMILY);
			ql_rtos_task_sleep_s(1); 
			ltapi_play_tts(TTS_STR_CALLING_FAMILY, strlen(TTS_STR_CALLING_FAMILY));
			ql_rtos_task_sleep_s(5); 
			LT_VOICE_CALL_LOG("familyPhone number:%s", familyPhone);
			if (ql_voice_call_start(ltvoicecall.nSim, familyPhone) != QL_VC_SUCCESS)
			{
				LT_VOICE_CALL_LOG("ql_voice_call_start FAIL");
				return;
			}
			api_yuanLiuMqtt_KeyPhone_Publish(1);
		}
		else
		{
			ltapi_play_tts(TTS_STR_NO_FAMILY_NUM, strlen(TTS_STR_NO_FAMILY_NUM));
		}
	}else if(st ==STATUS_CALLED)
	{
		lt_voice_call_answer();
	}
	else
	{
		ltvoicecall.sos_index =  0;//当前号码置0
		ltvoicecall.hang_up = TRUE;//手动挂断
		if (ql_voice_call_end(ltvoicecall.nSim) == QL_VC_SUCCESS)
		{
			LT_VOICE_CALL_LOG("ql_voice_call_end SUCCESS");
			// 新增 ： 跌倒时候主动挂断上报处理
			if(get_fall_call_flag() == 1)
			{
				lt_fall_call_pub(3);
			}
			//lt_voice_call_status_set(STATUS_NONE);//处理平台收到两条开播记录
		}
		else
		{
			LT_VOICE_CALL_LOG("ql_voice_call_end FAIL");
		}
	}
}

static void lt_voice_call_start(char *buf_call, phone_type_t type)
{
    if (NULL == buf_call)
    {
        return;
    }

    ltapi_play_tts(buf_call, strlen(buf_call));
    ql_rtos_task_sleep_s(5);
    char phone[15] = {0};
    if (0 == mqtt_param_phone_get(phone, sizeof(phone), type))
    {
        LT_VOICE_CALL_LOG("service type:%d phone number:%s", type, phone);
        if (ql_voice_call_start(ltvoicecall.nSim, phone) != QL_VC_SUCCESS)
        {
            LT_VOICE_CALL_LOG("ql_voice_call_start FAIL");
            return;
        }
        api_yuanLiuMqtt_KeyPhone_Publish(1);
        if (WATER_SERVICE_TYPE == type)
        {
            lt_voice_call_status_set(STATUS_CALL_WATER_SERVICE);
        }
        else if (ELECTRIC_SERVICE_TYPE == type)
        {
            lt_voice_call_status_set(STATUS_CALL_ELEC_SERVICE);
        }
        else
        {
            lt_voice_call_status_set(STATUS_CALL_GAS_SERVICE);
        }
    }
    else
    {
        if (WATER_SERVICE_TYPE == type)
        {
            ltapi_play_tts(TTS_STR_NO_WATER_NUM, strlen(TTS_STR_NO_WATER_NUM));
        }
        else if (ELECTRIC_SERVICE_TYPE == type)
        {
			ltapi_play_tts(TTS_STR_NO_ELEC_NUM, strlen(TTS_STR_NO_ELEC_NUM));
        }
        else
        {
			ltapi_play_tts(TTS_STR_NO_GAS_NUM, strlen(TTS_STR_NO_GAS_NUM));
        }
    }
    return;
}

static void lt_voice_call_answer()
{
	int n = 0;
	while (0 != ql_voice_call_answer(ltvoicecall.nSim) && n < 20)
	{
		LT_VOICE_CALL_LOG("ql_voice_call_answer fail%d", n);
		ql_rtos_task_sleep_ms(500);
		n++;
	}
	api_yuanLiuMqtt_KeyPhone_Publish(0);
	//	if (err != QL_VC_SUCCESS)
	if (n == 20)
	{
		LT_VOICE_CALL_LOG("ql_voice_call_answer FAIL");
	}
	else
	{
		LT_VOICE_CALL_LOG("ql_voice_call_answer OK");
	}

	return;
}

static void lt_voice_call_end()
{
    if (ql_voice_call_end(ltvoicecall.nSim) == QL_VC_SUCCESS)
    {
        LT_VOICE_CALL_LOG("ql_voice_call_end SUCCESS");
        lt_voice_call_status_set(STATUS_NONE);
    }
    else
    {
        LT_VOICE_CALL_LOG("ql_voice_call_end FAIL");
    }
    return;
}

static void water_key_click_cb()
{
    ql_sim_status_e card_status = QL_SIM_STATUS_READY;
    ql_sim_get_card_status(0, &card_status);

    if (QL_SIM_STATUS_NOSIM == card_status)
    {
        ltapi_play_tts(TTS_STR_NO_SIM_CARD, strlen(TTS_STR_NO_SIM_CARD));
        return;
    }

    voice_call_status_t st = lt_voice_call_status_get();
    LT_VOICE_CALL_LOG("voice call state:%d", st);
    if (STATUS_NONE == st)
    {
        lt_voice_call_start(TTS_STR_CALLING_WATER, WATER_SERVICE_TYPE);
    }
    else if (STATUS_CALLED == st)
    {
        lt_voice_call_answer();
    }
	else if (STATUS_CALLED_CONNECT == st)		/* 接通状态时挂断电话 */
	{
		lt_voice_call_end();
	}
    else
    {
        /* 先挂断当前拨号通话，然后判断如果是触发新的拨号则开始对应的拨号业务*/
        lt_voice_call_end();
        LT_VOICE_CALL_LOG("state:%d", st);
        if ((st != STATUS_CALL_WATER_SERVICE) && (st != STATUS_CALL_WATER_CONNECT))
        {
            lt_voice_call_start(TTS_STR_CALLING_WATER, WATER_SERVICE_TYPE);
        }
    }
}

static void gas_key_click_cb()
{
    ql_sim_status_e card_status = QL_SIM_STATUS_READY;
    ql_sim_get_card_status(0, &card_status);

    if (QL_SIM_STATUS_NOSIM == card_status)
    {
        ltapi_play_tts(TTS_STR_NO_SIM_CARD, strlen(TTS_STR_NO_SIM_CARD));
        return;
    }

    voice_call_status_t st = lt_voice_call_status_get();
    LT_VOICE_CALL_LOG("voice call state:%d", st);
    if (st == STATUS_NONE)
    {
        char *buf_call_f = "正在拨打燃气服务电话，请稍候";
        lt_voice_call_start(buf_call_f, GAS_SERVICE_TYPE);
    }
    else if (STATUS_CALLED == st)
    {
        lt_voice_call_answer();
    }
	else if (STATUS_CALLED_CONNECT == st)		/* 接通状态时挂断电话 */
	{
		lt_voice_call_end();
	}
    else
    {
        /* 先挂断当前拨号通话，然后判断如果是触发新的拨号则开始对应的拨号业务*/
        lt_voice_call_end();
        LT_VOICE_CALL_LOG("state:%d", st);
        if ((st != STATUS_CALL_GAS_SERVICE) && (st != STATUS_CALL_GAS_CONNECT))
        {
            char *buf_call_f = "正在拨打燃气服务电话，请稍候";
            lt_voice_call_start(buf_call_f, GAS_SERVICE_TYPE);
        }
    }
}

static void electric_key_click_cb()
{
    ql_sim_status_e card_status = QL_SIM_STATUS_READY;
    ql_sim_get_card_status(0, &card_status);

    if (QL_SIM_STATUS_NOSIM == card_status)
    {
		ltapi_play_tts(TTS_STR_NO_SIM_CARD, strlen(TTS_STR_NO_SIM_CARD));
        return;
    }

    voice_call_status_t st = lt_voice_call_status_get();
    LT_VOICE_CALL_LOG("voice call state:%d", st);
    if (st == STATUS_NONE)
    {
        char *buf_call_f = "正在拨打电力服务电话，请稍候";
        lt_voice_call_start(buf_call_f, ELECTRIC_SERVICE_TYPE);
    }
    else if (STATUS_CALLED == st)
    {
        lt_voice_call_answer();
    }
	else if (STATUS_CALLED_CONNECT == st)			/* 接通状态时挂断电话 */
	{
		lt_voice_call_end();
	}
    else
    {
        /* 先挂断当前拨号通话，然后判断如果是触发新的拨号则开始对应的拨号业务*/
        lt_voice_call_end();
        LT_VOICE_CALL_LOG("state:%d", st);
        if ((st != STATUS_CALL_ELEC_SERVICE) && (st != STATUS_CALL_ELEC_CONNECT))
        {
            char *buf_call_f = "正在拨打电力服务电话，请稍候";
            lt_voice_call_start(buf_call_f, ELECTRIC_SERVICE_TYPE);
        }
    }
    return;
}

void sosPhone_key_click_cb()
{
	ql_sim_status_e card_status = 0;
	ql_sim_get_card_status(0, &card_status);

	if (card_status == QL_SIM_STATUS_NOSIM)
	{
		ltapi_play_tts(TTS_STR_NO_SIM_CARD, strlen(TTS_STR_NO_SIM_CARD));
		return;
	}
	if (1 != lt_connect_status_get())
	{
		ltapi_play_tts(TTS_STR_NET_UNAVAILABLE, strlen(TTS_STR_NET_UNAVAILABLE));
		return;
	}
	voice_call_status_t st = lt_voice_call_status_get();
	if (st == STATUS_NONE)
	{

		ltvoicecall.sos_Time = ebs_time(NULL);;
		ltvoicecall.sos_num = mqtt_param_sosPhonenum_get();
		ltvoicecall.sos_index = 0;
		ltvoicecall.hang_up = FALSE;
		char sosPhone[15] = {0};
		//	if (mqtt_param_sosPhone_get(sosPhone, sizeof(sosPhone)) == 0)
		if (mqtt_param_sosPhonelist_get(sosPhone, sizeof(sosPhone), ltvoicecall.sos_index) == 0)
		{
			lt_voice_call_status_set(STATUS_CALL_SOS);
			ql_rtos_task_sleep_s(1);
			ltapi_play_tts(TTS_STR_CALLING_SOS, strlen(TTS_STR_CALLING_SOS));
			ql_rtos_task_sleep_s(5); //

			LT_VOICE_CALL_LOG("sosPhone begin number:%s", sosPhone);
			if (ql_voice_call_start(ltvoicecall.nSim, sosPhone) != QL_VC_SUCCESS)	
			{
				LT_VOICE_CALL_LOG("ql_voice_call_start FAIL");
				return;
			}
			ql_rtos_timer_start(lt_voicecall_pool_timer, 30000, 0);
			api_mqtt_sosPhone_Publish();
			
			mqtt_alarm_msg_t alarm_msg = {
				.alarmType = 1,
				.alarmId = ltvoicecall.sos_Time
			};

			api_mqtt_Sos_Publish(&alarm_msg);

			ltvoicecall.sos_index++;
		}
		else
		{
			ltapi_play_tts(TTS_STR_NO_SOS_NUM, strlen(TTS_STR_NO_SOS_NUM));
			QL_SMS_LOG("sos have no  number!!");
		}
	}
	else
	{
		ltvoicecall.sos_index =  0;//当前号码置0
		ltvoicecall.hang_up = TRUE;//手动挂断
		if (ql_voice_call_end(ltvoicecall.nSim) == QL_VC_SUCCESS)
		{
			LT_VOICE_CALL_LOG("ql_voice_call_end SUCCESS");
			// 新增 ： 跌倒时候主动挂断上报处理
			if(get_fall_call_flag() == 1)
			{
				lt_fall_call_pub(3);
			}
		//	lt_voice_call_status_set(STATUS_NONE);//处理平台收到两条开播记录
		}
		else
		{
			LT_VOICE_CALL_LOG("ql_voice_call_end FAIL");
		}
	}
}

static void sosPhone_list_cb()
{
	ql_sim_status_e card_status = 0;
	ql_sim_get_card_status(0, &card_status);

	if (card_status == QL_SIM_STATUS_NOSIM)
	{
		ltapi_play_tts(TTS_STR_NO_SIM_CARD, strlen(TTS_STR_NO_SIM_CARD));
		return;
	}
//	voice_call_status_t st = lt_voice_call_status_get();
	
	if( ltvoicecall.sos_index != ltvoicecall.sos_num)
	{
		char sosPhone[15] = {0};
		if (mqtt_param_sosPhonelist_get(sosPhone, sizeof(sosPhone),ltvoicecall.sos_index) == 0)
		{
			ltapi_play_tts(TTS_STR_SOS_NEXT, strlen(TTS_STR_SOS_NEXT));
			ql_rtos_task_sleep_s(5);
			LT_VOICE_CALL_LOG("sosPhone begin number:%s", sosPhone);
    
			if (ql_voice_call_start(ltvoicecall.nSim, sosPhone) != QL_VC_SUCCESS)
			{
				LT_VOICE_CALL_LOG("ql_voice_call_start FAIL");
				return;
			}
		 	ql_rtos_timer_start(lt_voicecall_pool_timer, 30000, 0);
			//sos只上报一次
			// api_mqtt_sosPhone_Publish();
			// api_mqtt_Sos_Publish(1);
			lt_voice_call_status_set(STATUS_CALL_SOS);
		}
		else
		{
			ltapi_play_tts(TTS_STR_SOS_END, strlen(TTS_STR_SOS_END));                                        
        	QL_SMS_LOG("sos have no  number!!");
			
			ql_rtos_task_sleep_s(5);   
			lt_voice_call_status_set(STATUS_NONE);
		}
		ltvoicecall.sos_index ++;
	}
	else
	{
		ltvoicecall.sos_Time = 0;

		ltvoicecall.sos_index =  0;
		ltvoicecall.sos_next = FALSE;
		LT_VOICE_CALL_LOG("sosPhone sos_index:%d sos_num:%d", ltvoicecall.sos_index,ltvoicecall.sos_num);

		mqtt_alarm_msg_t alarm_msg = {
			.alarmType = 3,
			.alarmId = ltvoicecall.sos_Time};
		api_mqtt_Sos_Publish(&alarm_msg);
		
		// 新增 ： 跌倒时候未接听上报处理
		if(get_fall_call_flag() == 1)
		{
			lt_fall_call_pub(2);
		}

		if (ql_voice_call_end(ltvoicecall.nSim) == QL_VC_SUCCESS)
		{
			LT_VOICE_CALL_LOG("ql_voice_call_end SUCCESS");
			lt_voice_call_status_set(STATUS_NONE);
		}
		else
		{
			lt_voice_call_status_set(STATUS_NONE); 
			LT_VOICE_CALL_LOG("ql_voice_call_end FAIL");
		}
	}
}
#if 1
static void lt_call_family_asr_callback_register()
{
    asr_type_state_t reg;

    reg.function = ASR_CALL;
    reg.state = ASR_FAMILY;
    reg.start_function = family_key_click_cb;
    reg.stop_function = family_key_click_cb;
    //reg.pause_function = NULL;    
	reg.next_function = NULL;
	reg.last_function = NULL;                                                                                                                                                                

    ltasr_callback_register(&reg);
} // 回调参考上溯例子

//定时任务，主要处理sos多列表时超时直接拨打下一个
void lt_voicecall_timer_callback(void *ctx)
{
	if ((lt_voice_call_status_get() == STATUS_CALL_SOS) && !ltvoicecall.hang_up && (ltvoicecall.sos_num != 1))
	{
		ql_voice_call_end(ltvoicecall.nSim);
	}
}
//定时任务，主要处理sos号码自动接听
void lt_voicecall_called_callback(void *ctx)
{
	LT_VOICE_CALL_LOG("sos_number_auth_verif success!!");

	if ((lt_voice_call_status_get() == STATUS_CALLED))
	{
		lt_voice_call_answer();
	}
}

//按键触发任务 96296
// void lt_96296_timer_callback(void *ctx)
// {
// 	 char *num = "96296";
// 	 	LT_VOICE_CALL_LOG("96296 lt_96296_timer_callback");
// 	QlOSStatus err = QL_OSI_SUCCESS;
// 	if (((lt_voice_call_status_get() == STATUS_CALL_FAMILY_CONNECT) || (lt_voice_call_status_get() == STATUS_CALL_SOS_CONNECT) )&& !ltvoicecall.hang_up && !(strncmp(ltvoicecall.msg.callNumber, num, strlen(num))))
// 	{
// 		LT_VOICE_CALL_LOG("96296 need touch zero");
// 		err = ql_voice_call_start_dtmf(ltvoicecall.nSim, "0", 0);
// 		if (err != QL_VC_SUCCESS)
// 		{
// 			QL_VC_LOG("ql_voice_call_start_dtmf FAIL");
// 		}
// 		else
// 		{
// 			QL_VC_LOG("ql_voice_call_start_dtmf OK");
// 		}
// 	}
// }
static void lt_call_sos_asr_callback_register()
{
    asr_type_state_t reg;

    reg.function = ASR_CALL;
    reg.state = ASR_SOS;
    reg.start_function = sosPhone_key_click_cb;
    reg.stop_function = sosPhone_key_click_cb;
    //reg.pause_function = NULL;
	reg.next_function = NULL;
	reg.last_function = NULL;

    ltasr_callback_register(&reg);
} // 回调参考上溯例子
#endif
voice_call_status_t lt_voice_call_status_get()
{
	return ltvoicecall.msg.callType;
}

static void ql_repeat_shutdown(void *data)
{
	// fm_info_t *this = &ltfm_info;
	int shutdown_repat = *((int *)data);

	if (shutdown_repat != 5)
	{
		return;
	}
	LT_VOICE_CALL_LOG("tts_turnoff");
	if (1 == ql_tts_ckeck_reboot())
	{
		ltapi_play_tts(TTS_STR_SMS_SHUTDOWN_WARN, strlen(TTS_STR_SMS_SHUTDOWN_WARN));
	}
	else
	{
		ltplay_stop();
		lt_shutdown();
	}
}

QlOSStatus lt_voice_call_init(void)
{
	QlOSStatus err = QL_OSI_SUCCESS;
	
	//录音回传回调
	lt_key_callback_t r_cb = {
		.lt_key_click_handle = NULL,
		.lt_key_doubleclick_handle = family_key_record_stop_cb,
		.lt_key_longclick_handle = family_key_record_cb,
		.lt_key_repeat_callback = NULL,
	};
	
	lt_key_callback_t f_cb = {
		.lt_key_click_handle = family_key_click_cb_new,
		.lt_key_doubleclick_handle = family_key_double_cb,
		.lt_key_longclick_handle = family_key_longclick_cb,
		.lt_key_repeat_callback = family_key_repeat_cb,
	};

	lt_key_callback_t sos_cb = {
		.lt_key_click_handle = sosPhone_key_click_cb,
		.lt_key_doubleclick_handle = NULL,
		.lt_key_repeat_callback = NULL,
	};

    lt_key_callback_t w_cb = {
        .lt_key_click_handle = water_key_click_cb,
        .lt_key_doubleclick_handle = NULL,
        .lt_key_longclick_handle = NULL,
        .lt_key_repeat_callback = NULL,
    };

    lt_key_callback_t g_cb = {                                                                                                                                                
        .lt_key_click_handle = gas_key_click_cb,
        .lt_key_doubleclick_handle = NULL,
        .lt_key_longclick_handle = NULL,
        .lt_key_repeat_callback = NULL,
    };

    lt_key_callback_t e_cb = {
        .lt_key_click_handle = electric_key_click_cb,
        .lt_key_doubleclick_handle = NULL,
        .lt_key_longclick_handle = NULL,
        .lt_key_repeat_callback = NULL,
    };

    lt_key_callback_t shutdown_cb = {
        .lt_key_click_handle = NULL,
        .lt_key_doubleclick_handle = NULL,
        .lt_key_longclick_handle =NULL,
        .lt_key_repeat_callback = ql_repeat_shutdown,
    };
	

	lt_key_callback_register("key_shutdown",&shutdown_cb);   // 注册打电话按钮功能
	lt_key_callback_register("key_voicecall",&f_cb);   // 注册打电话按钮功能
    lt_key_callback_register("key_watercall",&w_cb);    /* 注册水务打电话按钮功能 */
    lt_key_callback_register("key_gascall",&g_cb);    /* 注册燃气服务打电话按钮功能 */
    lt_key_callback_register("key_electriccall",&e_cb);    /* 注册电力服务打电话按钮功能 */
	lt_key_callback_register("key_soscall",&sos_cb); // 注册一键通按钮功能
	lt_key_callback_register("key_record",&r_cb); // 注册录音回传按钮功能

	
	lt_call_family_asr_callback_register();
	lt_call_sos_asr_callback_register();

	ql_voice_call_callback_register(lt_user_voice_call_event_callback); // 注册打电话状态回调

	err = ql_rtos_task_create(&ltvoicecall.vc_task, 4096, APP_PRIORITY_NORMAL, "ltvc", lt_voice_call_task, NULL, 2);
	if (err != QL_OSI_SUCCESS)
	{
		LT_VOICE_CALL_LOG("vc_task created failed, ret = 0x%x", err);
		return err;
	}
	// 创建定时器，目前主要在sos多号码时，给超时时间，设备自动挂断

	err = ql_rtos_timer_create(&lt_voicecall_pool_timer, ltvoicecall.vc_task, lt_voicecall_timer_callback, NULL);
	if (err != QL_OSI_SUCCESS)
	{
		LT_VOICE_CALL_LOG("Create voicecall pool timer fail err = %d", err);
	}
	// 创建被呼叫定时器，主要用于sos电话自动接听
	err = ql_rtos_timer_create(&lt_voicecall_called_timer, ltvoicecall.vc_task, lt_voicecall_called_callback, NULL);
	if (err != QL_OSI_SUCCESS)
	{
		LT_VOICE_CALL_LOG("Create voicecall pool timer fail err = %d", err);
	}

	// err = ql_rtos_timer_create(&lt_voicecall_96296_timer, ltvoicecall.vc_task, lt_96296_timer_callback, NULL);
	// if (err != QL_OSI_SUCCESS)
	// {
	// 	LT_VOICE_CALL_LOG("Create voicecall 96296 timer fail err = %d", err);
	// }

	return err;
} 
