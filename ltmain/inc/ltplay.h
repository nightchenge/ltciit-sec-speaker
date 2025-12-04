/*
 * @Author: zhouhao 
 * @Date: 2023-09-13 10:32:43 
 * @Last Modified by: zhouhao
 * @Last Modified time: 2023-09-13 12:56:36
 */
#ifndef _LTPLAY_H_
#define _LTPLAY_H_

enum SOUND_SRC
{
	SND_STOP 			= 0x00,
	SND_MP3             = 0x01,
	SND_FM				= 0x02,
    SND_EBS             = 0x03,
	SND_SMS				= 0x04,
	SND_TEL				= 0x05,
	SND_CLK             = 0x06,
	SND_MP3_URL         = 0x07,
	SND_FM_URL			= 0x08,		
};

typedef struct play_state_s{
	ql_mutex_t     mutex;
	char 			src;
} play_state_t;


typedef struct play_fsm_s{
	char src;
	char* description;
	void (*enter_play_func)();
	void (*enter_stop_func)();
	void (*enter_keep_func)();	
}play_fsm_t;

int ltplay_get_src();

int ltplay_set_src(char src);

void ltplay_callback_register(play_fsm_t *reg);

void ltplay_check_play(char src);

void ltplay_stop();
void ltplay_init(void);
#endif // _LTPLAY_H_