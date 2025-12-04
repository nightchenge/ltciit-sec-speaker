#ifndef _LTAUDIO_H_
#define _LTAUDIO_H_
// void lt_play_audio(int type ,char *data, int data_len);
void lt_audio_app_init(void);
// void lt_set_mp3_flag(char flag);
// char lt_get_mp3_flag();
//void lt_audio_pa_init(void);
// void lt_audio_pa_enable(void);
// void lt_audio_pa_disable(void);


void lt_audio_play_callback_register();



void lt_audio_key_callback_register();
void lt_mp3_asr_callback_register();

#endif // _LTAUDIO_H_