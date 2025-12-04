/*
 * @Author: zhouhao 
 * @Date: 2023-09-06 16:55:45 
 * @Last Modified by:   zhouhao 
 * @Last Modified time: 2023-09-06 16:55:45 
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
#ifndef _LTMP3_H_
#define _LTMP3_H_

#ifdef __cplusplus
extern "C" {
#endif

#define LTAPCU_MAX_AUDIO_CNT		64
#define LTAPCU_AUDIO_TYPE_MP3		0

#define LTAPCU_STA_STOP				0
#define LTAPCU_STA_PLAY				1
#define LTAPCU_STA_PAUSE			2

typedef struct{
	char 			path[64];
	char 			name[64];
	unsigned char	type;
}lt_audio_t;

typedef struct{
	int				play_sta; //LTAPCU_STA_STOP;LTAPCU_STA_PLAY;LTAPCU_STA_PAUSE
	int				play_lst_cnt;
	int				play_lst_idx;
	lt_audio_t 		play_lst[LTAPCU_MAX_AUDIO_CNT];
}ltmp3_sta_t;

int ltmp3_get_audio_sta();
int ltmp3_set_audio_sta(int sta);
int ltmp3_get_audio_cnt();
int ltmp3_get_cur_audio_index();
int ltmp3_set_cur_audio_index(int idx);
int ltmp3_get_cur_audio_name(char* title);
int ltmp3_play_next();
int ltmp3_play_cur();
int ltmp3_init(const char *path);
int ltmp3_deinit();

int ltmp3_check();
#ifdef __cplusplus
} /*"C" */
#endif

#endif // _LTMP3_H_










