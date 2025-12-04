
/*================================================================
  Copyright (c) 2021, Quectel Wireless Solutions Co., Ltd. All rights reserved.
  Quectel Wireless Solutions Proprietary and Confidential.
=================================================================*/

/*=================================================================

						EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

WHEN			  WHO		  WHAT, WHERE, WHY
------------	 -------	 -------------------------------------------------------------------------------

=================================================================*/

/*===========================================================================
 * include files
 ===========================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ql_log.h"
#include "osi_api.h"
#include "ql_sdmmc.h"
#include "ql_api_osi.h"
#include "ql_fs.h"
#include "ql_gpio.h"
#include "ql_pin_cfg.h"
#include "ltsdmmc.h"
#include "ltmp3.h"

#include "ql_usb.h"
/*===========================================================================
 *Definition
 ===========================================================================*/
#define LT_SDMMC_DEMO_LOG_LEVEL QL_LOG_LEVEL_INFO
#define LT_SDMMC_DEMO_LOG(msg, ...) QL_LOG(LT_SDMMC_DEMO_LOG_LEVEL, "lt_sdmmc", msg, ##__VA_ARGS__)
#define LT_SDMMC_DEMO_LOG_PUSH(msg, ...) QL_LOG_PUSH("lt_sdmmc", msg, ##__VA_ARGS__)

#define LT_FM_FAT32 0x02
#define LT_SDMMC_TASK_STACK_SIZE 1024 * 8
#define LT_SDMMC_TASK_PRIO APP_PRIORITY_NORMAL
#define LT_SDMMC_TASK_EVENT_CNT 5

#define LT_SDMMC_FILE_PATH "SD:test.txt"
#define LT_SDMMC_FILE_PATH1 "SD1:test.txt"
#define LT_SDMMC_TEST_STR "1234567890abcdefg"
#define LT_SDMMC_CLK_FREQ 50000000
#define LT_SDMMC_BLOCK_NUM 10

#define LT_SDMMC_EVENT_PLUGOUT 0
#define LT_SDMMC_EVENT_INSERT 1
#define LT_SDMMC_DET_TEST 1

/*#############################################################################*/

#define LT_SDMMC_ONLY_USE_DRIVER 0	  // 0--使用文件系统 1--仅使用sdmmc驱动层
#define LT_SDMMC_USE_1BIT_DATA_SIZE 1 // 0--使用默认4bit的数据宽度 1--使用1bit的数据宽度

/*#############################################################################*/

#define QL_USB_TASK_STACK 8 * 1024

/*===========================================================================
 * Variate
 ===========================================================================*/
#ifdef LT_SDMMC_DET_TEST
ql_task_t lt_sdmmc_det_task = NULL;
#define LT_SDMMC_DET_DEBOUNCE_TIME 50
ql_timer_t lt_sdmmc_det_debounce_timer = NULL;
#endif

ql_task_t lt_sdmmc_while_task = NULL;

ql_task_t ql_usb_task = NULL;

/*===========================================================================
 * Functions
 ===========================================================================*/
#ifdef LT_SDMMC_DET_TEST
static int lt_sdmmc_state  =0 ;
int lt_sdmmc_get_state()
{
	return lt_sdmmc_state;
}
void lt_sdmmc_det_callback(void *ctx)
{
	ql_event_t ql_event;
	ql_LvlMode sdmmc_det_value;
	ql_event.id = QUEC_SDDET_EVENT_IND;
	ql_rtos_timer_stop(lt_sdmmc_det_debounce_timer);
	ql_gpio_get_level(GPIO_37, &sdmmc_det_value);
	if (sdmmc_det_value == LVL_LOW)
	{
		ql_event.param1 = LT_SDMMC_EVENT_INSERT;
	}
	else
	{
		ql_event.param1 = LT_SDMMC_EVENT_PLUGOUT;
	}
	ql_rtos_event_send(lt_sdmmc_det_task, &ql_event);
}

void lt_sdmmc_det_debounce_callback(void *ctx)
{
	if (lt_sdmmc_det_debounce_timer == NULL)
	{
		QlOSStatus err = QL_SDMMC_SUCCESS;

		err = ql_rtos_timer_create(&lt_sdmmc_det_debounce_timer, lt_sdmmc_det_task, lt_sdmmc_det_callback, NULL);
		if (err != QL_OSI_SUCCESS)
		{
			LT_SDMMC_DEMO_LOG("creat timer task fail err = %d", err);
		}

		LT_SDMMC_DEMO_LOG("lt_sdmmc_det_debounce_timer == NULL");
		return;
	}
	if (ql_rtos_timer_is_running(lt_sdmmc_det_debounce_timer))
	{
		ql_rtos_timer_stop(lt_sdmmc_det_debounce_timer);
		LT_SDMMC_DEMO_LOG("lt_sdmmc_det_debounce_timer is running");
		// return ;
	}
	ql_rtos_timer_start(lt_sdmmc_det_debounce_timer, LT_SDMMC_DET_DEBOUNCE_TIME, 1);
	LT_SDMMC_DEMO_LOG("sd_det timer start");
}

void lt_sdmmc_pin_reboot()
{
	LT_SDMMC_DEMO_LOG("lt_sdmmc_pin_reboot");
	ql_gpio_set_level(GPIO_41, LVL_LOW);
	ql_rtos_task_sleep_s(1);
	ql_gpio_set_level(GPIO_41, LVL_HIGH);
	ql_rtos_task_sleep_s(1);
}
void lt_sdmmc_pin_init(void)
{

	// ql_gpio_init(QUEC_GPIO_PWM_AUD_PA, GPIO_OUTPUT,PULL_NONE, LVL_LOW);
	ql_gpio_init(GPIO_41, GPIO_OUTPUT, PULL_UP, LVL_HIGH); // power pin
#if LT_SDMMC_DET_TEST
	ql_pin_set_func(QL_SDMMC_PIN_DET, 2); // Pin reuse
#endif
	ql_pin_set_func(QL_PIN_SDMMC_CMD, QL_PIN_SDMMC_MODE_FUNC);	  // Pin reuse
	ql_pin_set_func(QL_PIN_SDMMC_DATA_0, QL_PIN_SDMMC_MODE_FUNC); // Pin reuse

	ql_pin_set_func(QL_PIN_SDMMC_DATA_1, QL_PIN_SDMMC_MODE_FUNC); // Pin reuse
	ql_pin_set_func(QL_PIN_SDMMC_DATA_2, QL_PIN_SDMMC_MODE_FUNC); // Pin reuse
	ql_pin_set_func(QL_PIN_SDMMC_DATA_3, QL_PIN_SDMMC_MODE_FUNC); // Pin reuse
	ql_pin_set_func(QL_PIN_SDMMC_CLK, QL_PIN_SDMMC_MODE_FUNC);	  // Pin reuse

#if LT_SDMMC_USE_1BIT_DATA_SIZE
	ql_sdmmc_cfg_t cfg = {
		.dev = QL_SDMMC_SD_CARD_ONLY,		  // 先以emmc流程进行初始化,如果失败再以SD卡流程进行初始化
		.sd_mv = 1800,						  // SD卡默认电压域3.2v
		.emmc_mv = 0,						  // emmc默认电压域1.8v
		.data_size = QL_SDMMC_DATA_SIZE_1BIT, // 使用1bit的数据总线
	};
	ql_sdmmc_set_dev_cfg(cfg);

#endif
}

ql_errcode_sdmmc_e lt_sdmmc_mount_demo(void)
{
	int mount_time = 1;
sdmmc_start:
	ql_sdmmc_umount();
	if (QL_SDMMC_SUCCESS != ql_sdmmc_mount())
	{
		lt_sdmmc_state = 0;
		if (mount_time % 3 != 0)
		{
			ql_sdmmc_umount();
			mount_time++;
			ql_rtos_task_sleep_s(1);
			goto sdmmc_start;
		}
		else if (mount_time <= 5)
		{
			mount_time++;
			lt_sdmmc_pin_reboot();
			goto sdmmc_start;
		}

		return QL_SDMMC_MOUNT_ERR;
	}
	else
	{
		mount_time = 0;
		// int ret = ql_sdmmc_set_clk(LT_SDMMC_CLK_FREQ);
		// if (ret)
		// {
		// 	LT_SDMMC_DEMO_LOG("sdmmc set clk fail:%d", ret);
		// }else
		// {

		// 	LT_SDMMC_DEMO_LOG("sdmmc set clk :%d ok", LT_SDMMC_CLK_FREQ);
		// }
		ltmp3_init(NULL); // 开机mount成功直接获取MP3列表
		lt_sdmmc_state = 1;
		LT_SDMMC_DEMO_LOG("Mount succeed");
	}
	// ql_rtos_task_sleep_s(3);
	return QL_SDMMC_SUCCESS;
}

ql_errcode_sdmmc_e lt_sdmmc_det_init(void)
{
	/*sd det interrup*/
	if (QL_GPIO_SUCCESS != ql_int_register(GPIO_37, EDGE_TRIGGER, DEBOUNCE_EN, EDGE_BOTH, PULL_UP, lt_sdmmc_det_debounce_callback, NULL))
	{
		LT_SDMMC_DEMO_LOG("det init reg err");
		return QL_SDMMC_INIT_ERR;
	}

	ql_int_enable(GPIO_37);

	return QL_SDMMC_SUCCESS;
}

static void lt_sdmmc_demo_det_thread(void *param)
{
	QlOSStatus err = QL_SDMMC_SUCCESS;
	lt_sdmmc_pin_init();
	err = lt_sdmmc_mount_demo();
	if (err != QL_SDMMC_SUCCESS)
	{
		LT_SDMMC_DEMO_LOG("sdmmc mount demo fail err = %d", err);
	}

	if (QL_SDMMC_SUCCESS != lt_sdmmc_det_init())
	{
		LT_SDMMC_DEMO_LOG("exit det init err");
		ql_rtos_task_delete(NULL);
	}

	while (1)
	{
		ql_event_t ql_event = {0};
		if (ql_event_try_wait(&ql_event) != 0)
		{
			continue;
		}

		if (ql_event.id == QUEC_SDDET_EVENT_IND)
		{
			if (ql_event.param1 == LT_SDMMC_EVENT_INSERT)
			{

				LT_SDMMC_DEMO_LOG("sd detect plug_in");
				ql_rtos_task_sleep_ms(500);
				if (QL_SDMMC_SUCCESS != lt_sdmmc_mount_demo()) // 挂载所有分区())
				{
					LT_SDMMC_DEMO_LOG("det mount failed");
				}
				else
				{

					LT_SDMMC_DEMO_LOG("det mount succeed");
					//	ltmp3_init(NULL);
				}
			}
			else if (ql_event.param1 == LT_SDMMC_EVENT_PLUGOUT)
			{
				lt_sdmmc_state = 0;
				LT_SDMMC_DEMO_LOG("sd detect plug_out");
				ql_sdmmc_umount();
				ltmp3_deinit();
				LT_SDMMC_DEMO_LOG("det umount succeed");
			}
		}
	}
}
#endif

/*
	模块USB 作为device 在PC虚拟出U盘
	配置映射模块的内置flash/sd卡/6线flash等存储器到PC端, 模块作为虚拟U盘
	注意：
		  1. 在target.config中,打开CONFIG_QUEC_PROJECT_FEATURE_USB_MASS_STORAGE宏
		  2. protocol参数中支持MTP和MSG两种协议,差异详见ql_usb_protocol_e定义处
		  3. 烧录代码后第一次开机进app可能会比较慢,如果sd卡/外置flash在app挂载,则烧录后第一次开机可能虚拟不出sd卡/外置flash,后续开机不会无法映射
*/
static void ql_usb_device_mass_storage(void *param)
{
	ql_usb_msc_cfg_t msc_cfg = {0};

	msc_cfg.msc_device = QL_USB_MSC_SDCARD;
	msc_cfg.protocol = QL_USB_PROTOCOL_MTP;
	strcpy(msc_cfg.dev_name, "ANDROID");
	ql_usb_set_enum_mode(QL_USB_ENUM_MASS_STORAGE); // 重启生效
	ql_usb_msc_config_set(&msc_cfg);				// 重启生效

	ql_rtos_task_delete(NULL);
}

void lt_sdmmc_app_init(void)
{
	QlOSStatus err = QL_SDMMC_SUCCESS;

	/*sd pin init*/
	// 开机自动先mount

#ifdef LT_SDMMC_DET_TEST
	err = ql_rtos_task_create(&lt_sdmmc_det_task, LT_SDMMC_TASK_STACK_SIZE, LT_SDMMC_TASK_PRIO, "lt_sdmmc_det", lt_sdmmc_demo_det_thread, NULL, LT_SDMMC_TASK_EVENT_CNT);
	if (err != QL_OSI_SUCCESS)
	{
		LT_SDMMC_DEMO_LOG("creat sd task fail err = %d", err);
	}

	err = ql_rtos_timer_create(&lt_sdmmc_det_debounce_timer, lt_sdmmc_det_task, lt_sdmmc_det_callback, NULL);
	if (err != QL_OSI_SUCCESS)
	{
		LT_SDMMC_DEMO_LOG("creat timer task fail err = %d", err);
	}

	err = ql_rtos_task_create(&ql_usb_task, QL_USB_TASK_STACK, APP_PRIORITY_NORMAL, "ql_usb_device_mass_storage", ql_usb_device_mass_storage, NULL, 5);
	if (err != QL_OSI_SUCCESS)
	{
		LT_SDMMC_DEMO_LOG("usb task create failed! err = %d", err);
	}
#endif
}
