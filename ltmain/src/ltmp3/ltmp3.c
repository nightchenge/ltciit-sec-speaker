/*
 * @Author: mikey.zhaopeng 
 * @Date: 2023-09-06 15:07:55 
 * @Last Modified by: zhouhao
 * @Last Modified time: 2023-09-We 01:20:24
 */

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

/*===========================================================================i
 * include files
 ===========================================================================*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_api_osi.h"
#include "ql_log.h"
#include "ql_pin_cfg.h"

#include "ql_fs.h"
//#include "audio_demo.h"
#include "ltmp3.h"

#define LT_MP3_LOG_LEVEL	            QL_LOG_LEVEL_INFO
#define LT_MP3_LOG(msg, ...)			QL_LOG(LT_MP3_LOG_LEVEL, "lt_mp3", msg, ##__VA_ARGS__)
#define LT_MP3_LOG_PUSH(msg, ...)		QL_LOG_PUSH("lt_mp3", msg, ##__VA_ARGS__)
static ltmp3_sta_t s_ltau_sta = {
	.play_sta = LTAPCU_STA_STOP, 
	.play_lst_cnt = 0,
	.play_lst_idx = -1,
};
// /
// 	QDIR *dir = NULL;
// 	qdirent *fp = NULL;
// 	if(!(dir = ql_opendir("SD:"))){
//         QL_keyDEMO_LOG("dir NULL;");
//         return;
// 	}
// 	while(1){
//         fp = ql_readdir(dir);
int ltmp3_check()
{
	if (!s_ltau_sta.play_lst_cnt){
		LT_MP3_LOG("no audio resoureces, try to call ltapcu_init first!\n");
		return -1;
	}

	return 0;
}

int ltmp3_get_audio_sta()
{	
	return s_ltau_sta.play_sta;
}
int ltmp3_set_audio_sta(int sta)
{	
	s_ltau_sta.play_sta = sta;
	return 0;
}
int ltmp3_get_audio_cnt()
{
	return s_ltau_sta.play_lst_cnt;
}
/** 
 * @brief     获取当前音频文件下标
 * @return    当前音频文件下标
 */
int ltmp3_get_cur_audio_index()
{
	return s_ltau_sta.play_lst_idx;
}

/** 
 * @brief     设置当前音频文件下标
 * @return    当前音频文件下标
 */
int ltmp3_set_cur_audio_index(int idx)
{
	s_ltau_sta.play_lst_idx = idx;
	return 0;
}

/** 
 * @brief     获取当前音频文件名称
 * @param     title  存放文件名称指针
 * @return    当前文件字符串长*/


int ltmp3_get_cur_audio_name(char* title)
{
	strncpy(title,s_ltau_sta.play_lst[s_ltau_sta.play_lst_idx].path,strlen(s_ltau_sta.play_lst[s_ltau_sta.play_lst_idx].path)+1);
	return  strlen(s_ltau_sta.play_lst[s_ltau_sta.play_lst_idx].path);
}


int ltmp3_play_next()
{
	if (-1 == ltmp3_check())
		return -1;
    ++s_ltau_sta.play_lst_idx;
	s_ltau_sta.play_lst_idx = s_ltau_sta.play_lst_idx%s_ltau_sta.play_lst_cnt;
	return 0;
}
int ltmp3_play_cur()
{
    --s_ltau_sta.play_lst_idx;
	s_ltau_sta.play_lst_idx = s_ltau_sta.play_lst_idx%s_ltau_sta.play_lst_cnt;
	return 0;
}
/**
 * @name: ltmp3_init
 * @description: MP3初始化用来更新MP3列表
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
int ltmp3_init(const char *path)
{
    
	QDIR *dir = NULL;
	qdirent *fp = NULL;
    if(NULL == path || !strlen(path))
    {
		path = "SD:";
    }
	if(!(dir = ql_opendir(path))){
		LT_MP3_LOG("fail to open directory %s!\n",path);
		return -1;
	}
	s_ltau_sta.play_lst_cnt = 0;
	s_ltau_sta.play_lst_idx = 0;
	s_ltau_sta.play_sta = LTAPCU_STA_STOP;
	while((fp = ql_readdir(dir))!=NULL){
		if (strlen(fp->d_name) < 4 )
			continue;
		//if (!strncmp(&fp->d_name[strlen(fp->d_name) - 4], ".mp3", 4))
		if (strncmp(&fp->d_name[strlen(fp->d_name) - 4], ".mp3", 4)==0 || strncmp(&fp->d_name[strlen(fp->d_name) - 4], ".aac", 4)==0 || strncmp(&fp->d_name[strlen(fp->d_name) - 4], ".wav", 4)==0)
		{
			s_ltau_sta.play_lst[s_ltau_sta.play_lst_cnt].type = LTAPCU_AUDIO_TYPE_MP3;
			sprintf(s_ltau_sta.play_lst[s_ltau_sta.play_lst_cnt].path, "%s%s", path, fp->d_name);
			strcpy(s_ltau_sta.play_lst[s_ltau_sta.play_lst_cnt].name, fp->d_name);
            LT_MP3_LOG("s_ltau_sta.play_lst[s_ltau_sta.play_lst_cnt].name ==%s",s_ltau_sta.play_lst[s_ltau_sta.play_lst_cnt].name);
			s_ltau_sta.play_lst_cnt++;
			if(LTAPCU_MAX_AUDIO_CNT == s_ltau_sta.play_lst_cnt)
				break;
		}
	}

	ql_closedir(dir);
	return 0;
}
int ltmp3_deinit()
{
	s_ltau_sta.play_lst_idx = 0;
	s_ltau_sta.play_lst_cnt = 0;
//	s_ltau_sta.play_sta = LTAPCU_STA_STOP;
	return 0;

}
