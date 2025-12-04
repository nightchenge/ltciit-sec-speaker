/*
 * @Author: your name
 * @Date: 2023-09-07 10:21:41
 * @LastEditTime: 2025-03-05 16:48:00
 * @LastEditors: zhouhao
 * @Description: In User Settings Edit
 * @FilePath: /LTE01R02A01_BETA0726_C_SDK_G/components/ql-application/ltmain/inc/ltsms.h
 */

#ifndef LTVOICECALL_H
#define LTVOICECALL_H

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum voice_call_status
    {
        STATUS_NONE = 0,            // 空闲
        STATUS_CALL_FAMILY,         // 主动FAMILY呼出
        STATUS_CALL_SOS,            // 主动SOS呼出
        STATUS_CALL_FAMILY_CONNECT, // 主动FAMILY呼出_接通中
        STATUS_CALL_SOS_CONNECT,    // 主动SOS呼出_接通中
        STATUS_CALLED,              // 被叫
        STATUS_CALLED_CONNECT,      // 被叫_接通中
        STATUS_CALL_WATER_SERVICE,  // 主动WATER呼出
        STATUS_CALL_WATER_CONNECT,  // 主动WATER呼出_接通中
        STATUS_CALL_ELEC_SERVICE,   // 主动ELEC呼出
        STATUS_CALL_ELEC_CONNECT,   // 主动ELEC呼出_接通中
        STATUS_CALL_GAS_SERVICE,    // 主动GAS呼出
        STATUS_CALL_GAS_CONNECT,    // 主动GAS呼出_接通中
    } voice_call_status_t;

    typedef struct vc_callback_msg
    {
        voice_call_status_t callType; // callType 0：空闲 1：F主叫 2：S主叫 3：被叫
        unsigned char callState;      // CallState 0：未通话 1：已通话
        char callNumber[23];          // callNumber 主叫/被叫号码
        time_t callRingTime;          // callRingTime 主叫，被叫,呼出的时间
        time_t callStartTime;         // callStartTime 开始时间(拒接状态该值为呼出时间)
        time_t callEndTime;           // CallEndTime 结束时间(拒接状态该值为正常)
        int callTime;                 // callTime 通话时间(拒接状态该值为0)
    } vc_callback_msg_t;

    typedef void (*vc_callback)(vc_callback_msg_t *st);     // 回调函数

    /* 注册通话状态改变通知回调 */
    void lt_voice_call_status_onchanged(vc_callback); 

    /* 获取通话状态 */
    voice_call_status_t lt_voice_call_status_get(void);

    /* 电话模块初始化 */
    QlOSStatus lt_voice_call_init(void);

    /* 定位信息*/
    typedef struct
    {
        int mcc;
        int mnc;
        int lac;
        int cellid;
    } CellInfo;
    //获取定位信息
    void lt_get_gnsss_t(CellInfo *cell);

void sosPhone_key_click_cb();

#ifdef __cplusplus
} /*"C" */
#endif

#endif /* LTVOICECALL_H */
