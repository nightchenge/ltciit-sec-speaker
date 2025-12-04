/*
 * @Author: zhouhao 
 * @Date: 2023-09-06 15:15:13 
 * @Last Modified by:   zhouhao 
 * @Last Modified time: 2023-09-06 15:15:13 
 */


#ifndef _MY_KEY_DRIVER_H_
#define _MY_KEY_DRIVER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MYKEY_EVENT_CLICK       ((unsigned char)0x01U)          /** 单击 */
#define MYKEY_EVENT_DBLCLICK    ((unsigned char)0x02U)          /** 双击 */
#define MYKEY_EVENT_LONG_PRESS  ((unsigned char)0x04U)          /** 长按 */
#define MYKEY_EVENT_REPEAT      ((unsigned char)0x08U)          /** 连续触发、重复触发 */
#define MYKEY_EVENT_RELASE      ((unsigned char)0x10U)          /** 松开 */

#define MYKEY_EVENT_REPEAT_TIME             200                //ms

#define MYKEY_EVENT_LONG_PRESS_TIME         500               //ms




/**
 * @brief 按键句柄
 *
 */
typedef void *MyKeyHandle;

//按键消息结构体定义
typedef struct {
    MyKeyHandle KeyID;                          /** 按键ID */
    unsigned char KeyEvent;                     /** 按键事件 */
    unsigned char KeyClickCount;                /** 按键次数计数 */
} myKeyMsg_t;


/**
 * @brief 按键状态读取函数，按下返回1，弹起返回0
 *
 */
typedef int (*KeyStatusFunc)(int i);

/**
 * @brief 初始化按键扫描器
 *
 * @return int 0:success, other:failed
 */
int MyKey_Init(void);

/**
 * @brief 注销按键扫描器
 *
 * @return int 0:success, other:failed
 */
int MyKey_Deinit(void);

/**
 * @brief 注册一个按键
 *
 * @param Key  按键句柄
 * @param func  按键状态读取函数，按下返回true，弹起返回false
 * @param Mode  按键功能，按键事件集合
 * @param RepeatSpeed  长按时连续触发周期，单位ms
 * @param LongPressTime  长按时间
 * @return int 0:success, other:failed
 */
int MyKey_Register(MyKeyHandle *Key, int key_num,KeyStatusFunc func, unsigned char Mode, size_t RepeatSpeed, size_t LongPressTime);

/**
 * @brief 卸载一个按键
 *
 * @param Key  按键句柄
 * @return int 0:success, other:failed
 */
int MyKey_Unregister(MyKeyHandle *Key);

/**
 * @brief 按键扫描,需要周期调用
 *
 * @param InterVal  调用间隔，单位ms
 */
void MyKey_Scan(size_t InterVal);

/**
 * @brief 打印出已注册的按键ID
 *
 */
void MyKey_PrintKeyInfo(void);

/**
 * @brief 从按键消息队列中获取一个按键消息
 *
 * @param KeyID 按键句柄
 * @param KeyEvent 按键事件，单击、双击还是长按等等
 * @param KeyClickCount 按键点击次数
 * @return int 0:success, other:failed
 */
int MyKey_Read(MyKeyHandle *KeyID, unsigned char *KeyEvent, unsigned char *KeyClickCount);

#ifdef __cplusplus
}
#endif

#endif
