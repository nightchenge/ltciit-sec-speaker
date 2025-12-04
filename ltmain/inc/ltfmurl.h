/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2024-05-28 10:51:36
 * @LastEditors: zhouhao
 * @LastEditTime: 2024-06-23 15:00:06
 */
#ifndef _LTFMURL_H_
#define _LTFMURL_H_


void lt_fm_add_url(char * url_name);
void lt_fm_del_allurl();
void lt_fmurl_app_init(void);
void lt_fm_add_fmTag(int Tagnum, uint32_t Tag_val);
#endif // _LTFMURL_H_