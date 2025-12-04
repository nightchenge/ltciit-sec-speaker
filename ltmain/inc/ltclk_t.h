/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2023-09-27 10:17:11
 * @LastEditors: zhouhao
 * @LastEditTime: 2023-10-19 13:47:44
 */
#ifndef _LTCLK_T_H_
#define _LTCLK_T_H_

enum CLK_ACT
{
	CLK_DEL 			= 0x00,
	CLK_ADD             = 0x01,
	CLK_ON				= 0x02,
    CLK_OFF             = 0x03,
	CLK_EDIT			= 0x04,
};

int lt_clk_instruct_add(char *clockID, int  clockAction, char *  alarmClockSet, char * clockUrl,int url_len,void (*start_handle)(void * id,void *date,int  date_len), void (*stop_handle)());

void lt_clk_t_task(void *param);
void lt_clk_instruct_del(char *clockID);
#endif // _LTCLK_T_H_