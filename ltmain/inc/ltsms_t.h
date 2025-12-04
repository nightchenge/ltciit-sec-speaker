/*
 * @Descripttion: tts_task_header
 * @version: 
 * @Author: zhouhao
 * @Date: 2023-09-18 17:01:42
 * @LastEditors: zhouhao
 * @LastEditTime: 2024-06-25 09:53:22
 */
#ifndef _LTSMS_T_H_
#define _LTSMS_T_H_


#define TTS_TYPE_SMS 0
#define TTS_TYPE_EBS 1

#define SMS_MAX 134
//void tts_creat_intruct(tts_play_instruct_t *instruct);
void lt_sms_t_task(void *param);

//type TTS_TYPE_SMS 0 TTS_TYPE_EBS 1
void lt_tts_instruct_stop(int type);
/*
    type --类型  sms 还是ebs
    date -- 播放任务的主体
    datelen --长度
    starthandle -- 开播处理接口
    stop_handle -- 停播处理接口
*/
int lt_tts_instruct_add(int type, void *data, int datelen, void (*start_handle)(void *buf, int date_len), void (*stop_handle)(void *buf, int date_len));
//判断是否又未读短信，是否要重启
int ql_tts_ckeck_reboot();
bool ql_tts_check_sim_sms();
#endif // _LTSMS_T_H_