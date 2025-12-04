/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2024-01-02 13:36:52
 * @LastEditors: zhouhao
 * @LastEditTime: 2024-01-02 15:22:50
 */
#ifndef _LTRECORD_H_
#define _LTRECORD_H_

enum RECORD_TYPE
{
    RECORD_IDEL=0,
    RECORD_MIC,
    RECORD_VOICE,
    RECORD_STOP
};
int record_generate_file_name(char * tel_num);
void ltapi_record(int type,char *data, int data_len);
void lt_record_data_handle();
void lt_record_data_stop_handle();
void lt_record_app_init(void);
#endif // _LTRECORD_H_