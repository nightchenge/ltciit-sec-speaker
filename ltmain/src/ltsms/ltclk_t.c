/*
 * @Descripttion: 
 * @version: 
 * @Author: zhouhao
 * @Date: 2023-09-26 13:45:15
 * @LastEditors: zhouhao
 * @LastEditTime: 2025-08-25 09:53:57
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "ql_api_common.h"
#include "ql_api_osi.h"
#include "ql_api_sms.h"
#include "ql_api_rtc.h"
#include "ql_log.h"
#include "ltkey.h"
#include "ql_api_voice_call.h"
#include "ltclk_t.h"
#include "ltplay.h"
#include "led.h"
#define clk_log(msg, ...) QL_LOG(QL_LOG_LEVEL_INFO, "clk", msg, ##__VA_ARGS__)

#define CLK_MAX_INSTRUCTION_LEN 1024
#define CLK_MAX_INSTRUCTION_CNT 10
//获取ms时间戳
uint64_t clk_instruct_gettime(void)
{
    ql_timeval_t time;

    memset(&time, 0, sizeof(ql_timeval_t));
    ql_gettimeofday(&time);
    return ((uint64_t)time.sec * 1000 + time.usec / 1000);
}

typedef struct clk_play_instruct_s
{
    char clockID[30];
    int clockAction;
    char alarmClockSet[32];
    uint8_t *clockUrl;
    int url_len;
    int    repeatFlag;
    int    status;
    void (*instruct_play_handle)(void * id,void *date,int date_len);
    void (*instruct_stop_handle)();    
} clk_play_instruct_t;

typedef struct clk_play_lst_s
{
    int used;
    clk_play_instruct_t instruct;
} clk_play_lst_t;


static clk_play_lst_t g_clk_play_lst[CLK_MAX_INSTRUCTION_CNT] = {0}; //初始化clk播放队列默认大小10
//static clk_play_instruct_t *play_instruct = NULL;  //当前在播的通道
/**
 * @name: 
 * @description:  添加clk播放队列
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
static int clk_instruct_add(clk_play_instruct_t *instruct)
{
    if (instruct)
    {
        int i = 0;
        if(instruct->clockAction != CLK_OFF)
        {
            lt_set_led_colour(1, GREEN);
        }

        for (i = 0; i < CLK_MAX_INSTRUCTION_CNT; i++)
        {
            //memcmp(g_clk_play_lst[i].instruct.clockID, instruct->clockID, IP_EBMID_LEN)
            if (!memcmp(g_clk_play_lst[i].instruct.clockID, instruct->clockID, strlen(instruct->clockID)))
                break;
        }

        if (CLK_MAX_INSTRUCTION_CNT == i)
        { /* not match */
            for (i = 0; i < CLK_MAX_INSTRUCTION_CNT; i++)
            {
                if (!g_clk_play_lst[i].used)
                    break;
            }
        }

        if (CLK_MAX_INSTRUCTION_CNT == i)
        { /* full */
            clk_log("clk struction list is full!\n");
            return -1;
        }
        if (instruct->clockAction == CLK_EDIT)
        {
             clk_log(" g_clk_play_lst[i].instruct.clockAction= %d\n", g_clk_play_lst[i].instruct.clockAction);
           instruct->clockAction =  g_clk_play_lst[i].instruct.clockAction;
        }
        memcpy(&g_clk_play_lst[i].instruct, instruct, sizeof(clk_play_instruct_t));
        g_clk_play_lst[i].used = 1;
    }

    return 0;
}

/**
 * @name: 
 * @description: 删除队列中的clk
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
static int clk_instruct_del(clk_play_instruct_t *instruct)
{
    int i = 0;

    for (i = 0; i < CLK_MAX_INSTRUCTION_CNT; i++)
    {
        if (!memcmp(g_clk_play_lst[i].instruct.clockID, instruct->clockID, strlen(instruct->clockID)))
        {
            g_clk_play_lst[i].used = 0;
            if(g_clk_play_lst[i].instruct.clockUrl)
            {
                free(g_clk_play_lst[i].instruct.clockUrl);
                clk_log("clk free instruct.data\n");
            }
            g_clk_play_lst[i].instruct.instruct_play_handle =NULL;
            g_clk_play_lst[i].instruct.instruct_stop_handle =NULL;
        }

    }
    
    return 0;
}
/**
 * @name: 
 * @description: 查找队列中clk
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
// static clk_play_instruct_t *clk_instruct_find(char * clockid)
// {
//     int i = 0;
    
//     for (i = 0; i < CLK_MAX_INSTRUCTION_CNT; i++)
//     {
//         if (g_clk_play_lst[i].used &&  (!memcmp(g_clk_play_lst[i].instruct.clockID,clockid,strlen(clockid))))
//         {
//             return &g_clk_play_lst[i].instruct;
//         }
//     }

//     return NULL;
// }


/**
 * @name: 
 * @description: 解析当前的clk任务是否符合要求
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
static int check_alarmClockSet(clk_play_instruct_t *instruct)
{
    if(!instruct)
        return -1;
    if((instruct->clockAction != 1) &&  (instruct->clockAction != 2) &&(instruct->clockAction != 3)&&(instruct->clockAction != 4))
    {
        clk_log("当前闹钟未启用");
        return 2;
    }

    if((instruct->clockAction == 3) )
    {
        clk_log("当前闹钟禁用");
        return 3;
    }

    char *token;
    // 使用strtok函数分割字符串
    char alarmClockSet[32]={0};
    memcpy(alarmClockSet,instruct->alarmClockSet,32);
    token = strtok(alarmClockSet, "#");
    if (token != NULL)
    {
        int hour = atoi(token); // 将小时部分转换为整数
        token = strtok(NULL, "#"); // 获取分钟部分
        if (token != NULL)
        {
            int minute = atoi(token); // 将分钟部分转换为整数
            token = strtok(NULL, "#"); // 获取标志位部分
            if (token != NULL)
            {
                char flags[8]; // 存储标志位的字符数组
                strcpy(flags, token);
                token = strtok(NULL, "#"); // 获取重复标志位部分
                if (token != NULL)
                {
                    int repeatFlag = atoi(token); // 将重复标志位转换为整数

                    ql_rtc_time_t tm;
                  //  ql_rtc_set_timezone(32);    //UTC+32
                    ql_rtc_get_localtime(&tm);
                    // 获取当前时间
                    int currentHour = tm.tm_hour;  // 当前小时
                    int currentMinute = tm.tm_min; // 当前分钟
                    int currentDayOfWeek = (tm.tm_wday == 0) ? 7 : (tm.tm_wday); // 当前是星期几 (1-7, 7代表星期日)

                    // 检查标志位对应的星期几是否满足条件
                    int flagIndex =  currentDayOfWeek - 1; // flags中对应的标志位索引
                    instruct->repeatFlag = repeatFlag; // 判断是否重复触发 0 不重复，1 重复
                    if(currentHour == 23 && currentMinute == 59  && instruct->status == 0xff) //每天0点清空状态值，
                    {
                         instruct->status = 0;
                    } 
                    clk_log("currentHour =%d,currentMinute=%d,hour=%d,minute=%d",currentHour,currentMinute,hour,minute);
                    // 如果当前时间过了指定时间，触发条件  源流的一次性闹钟星期字段全为0 兼容
                 //   if ((flags[flagIndex] == '1' || !strcmp(flags, "0000000") ) && (currentHour > hour || (currentHour == hour && currentMinute >= minute))  && (instruct->status == 0) )
                    if ((flags[flagIndex] == '1' || !strcmp(flags, "0000000") ) &&  ((currentHour == hour) && (currentMinute == minute))  && (instruct->status == 0) )
                    {
                        if(instruct->status == 0)
                        {
                            instruct->status = 0xff;
                            clk_log("满足触发条件\n");
                            return 1;
                        }else if(instruct->status == 0xff)
                        {
                            clk_log("满足触发条件，但当前已触发过\n");
                            return 2;  
                        }
                    }
                    else
                    {
                        clk_log("未满足触发条件\n");
                        return 2;
                    }
                    // 如果 repeatFlag 为 1，表示触发条件重复，可以进行相应处理
                }
                else
                {
                    clk_log("未找到重复标志位\n");
                }
            }
            else
            {
                clk_log("未找到标志位\n");
            }
        }
        else
        {
            clk_log("未找到分钟\n");
        }
    }
    else
    {
        clk_log("未找到小时\n");
    }
    return 0;
}
/**
 * @name: 
 * @description: 获取当前播放队列中优先级别最高的clk
 * @param {*}
 * @return {*}
 * @author: zhouhao
 */
static int clk_instruct_task()
{
    clk_play_instruct_t *instruct = NULL;
    int i = 0;
    int clk_t = 0; //用来判断是否存在闹钟任务
    for (i = 0; i < CLK_MAX_INSTRUCTION_CNT; i++)
    {
        if (!g_clk_play_lst[i].used)
        {
            continue;
        }
        else
        {
            clk_t = 1;
            clk_log("lt_clk_instruct_add i ==%d!!",i);
            instruct = &g_clk_play_lst[i].instruct;
           
            int err_t = check_alarmClockSet(instruct);
            if(err_t == 1) // 判断成功需要执行play接口
            {

                if(instruct->instruct_play_handle && ((SND_STOP == ltplay_get_src()) || (SND_MP3 == ltplay_get_src()) ||  (SND_FM == ltplay_get_src()) || (SND_FM_URL == ltplay_get_src()) || (SND_MP3_URL == ltplay_get_src()) ))
                {
                    instruct->instruct_play_handle(instruct->clockID,instruct->clockUrl,instruct->url_len);
                }
                if(instruct->repeatFlag == 0)
                {
                      clk_log("repeatFlag ==%d!!",instruct->repeatFlag);
                    clk_instruct_del(instruct);
                }

            }else if(err_t != 2) //如果不是判断失败，则数据中有不合理地方，直接删除
            {
                clk_instruct_del(instruct); 
            }else if(err_t == 3)
            {
                clk_t = 0;
            }
        }


    }
    if (clk_t == 1)
    {
        lt_set_led_colour(1, GREEN); // 有闹钟就亮
    }
    else
    {
        lt_set_led_colour(1, OFF);  //没闹钟灭
    }
    return 0;
}

//外部任务删除调用的接口
void lt_clk_instruct_del(char *clockID)
{
    clk_play_instruct_t *instruct = NULL;
    int i = 0;

    lt_set_led_colour(1, OFF);

    for (i = 0; i < CLK_MAX_INSTRUCTION_CNT; i++)
    {
        if (!g_clk_play_lst[i].used)
        {
            continue;
        }
        else 
        {
            clk_log("lt_clk_instruct_add i ==%d!!",i);
            instruct = &g_clk_play_lst[i].instruct;
            if(!memcmp(instruct->clockID, clockID, strlen(clockID)))
            {
                clk_log("find clockID ,del task");
                clk_instruct_del(instruct);
            }
        }
    }
}
//外部有任务时加入队列的接口
/**
 * @name: 
 * @description: 
 * @param {int type, char *data, int datelen, void (*start_handle)(char *buf, int date_len), void (*stop_handle)(char *buf, int date_len)}
 * @return {*}
 * @author: zhouhao
 */
//int lt_clk_instruct_add(int type, void *data, int datelen, void (*start_handle)(void *buf, int date_len), void (*stop_handle)(void *buf, int date_len))
//int lt_clk_instruct_add(char *clockID, int  clockAction, char *  alarmClockSet, char * clockUrl,int url_len,void (*start_handle)(void * id,void *date,int  date_len), void (*stop_handle)());
int lt_clk_instruct_add(char *clockID, int  clockAction, char *  alarmClockSet, char * clockUrl,int url_len,void (*start_handle)(void * id,void *date,int  date_len), void (*stop_handle)())
{
    // 	CLK_DEL 			= 0x00,
	// CLK_ADD             = 0x01,
	// CLK_ON				= 0x02,
    // CLK_OFF             = 0x03,
	// CLK_EDIT			= 0x04
    if(CLK_DEL == clockAction) //如果是删除clk，直接调用删除接口
    {
        clk_log("lt_clk_instruct_del clockid == %s",clockID);     
        lt_clk_instruct_del(clockID);
        return 0;
    }
    clk_play_instruct_t instruct;
    memcpy(instruct.clockID, clockID, strlen(clockID));
    instruct.clockAction = clockAction;

    memcpy(instruct.alarmClockSet, alarmClockSet, strlen(alarmClockSet));
    instruct.url_len = url_len;
    instruct.clockUrl = malloc(url_len);
    memcpy(instruct.clockUrl, clockUrl, url_len);

    instruct.repeatFlag = 0;
    instruct.status = 0;
    if (start_handle)
        instruct.instruct_play_handle = start_handle;
    if (stop_handle)
        instruct.instruct_stop_handle = stop_handle;
    if (-1 == clk_instruct_add(&instruct))
    {
        clk_log("lt_clk_instruct_add fail!!");
        if (instruct.clockUrl)
        {
            free(instruct.clockUrl);
        }
        return -1;
    }
    else
    {
        clk_log("lt_clk_instruct_add success!!");
        return 0;
    }
}

void clk_instruct_play_handle(void *url)
{
	clk_log("clk_instruct_play_handle buf==%s",(char *)url);

}

//int lt_clk_instruct_add(char *clockID, int  clockAction, char *  alarmClockSet, char * clockUrl,int url_len,void (*start_handle)(void *url), void (*stop_handle)())
void lt_clk_t_task(void *param)
{
    // char clockID[30] = "1370281908836700162";
    // char alarmClockSet[32] = "11#00#1111111#1";
    // char *url="123456789";
     //  lt_set_led_colour(1, 1);
    // lt_clk_instruct_add(clockID,1,alarmClockSet,url,9,clk_instruct_play_handle,NULL);
    ql_rtos_task_sleep_s(10);
    while (1)
    {
        clk_log("lt_clk_t_task!!");
        clk_instruct_task();
  
        ql_rtos_task_sleep_s(5);
    }
}
