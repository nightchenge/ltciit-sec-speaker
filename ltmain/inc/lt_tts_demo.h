/*
 * @Descripttion:
 * @version:
 * @Author: zhouhao
 * @Date: 2024-07-11 14:40:47
 * @LastEditors: ubuntu
 * @LastEditTime: 2025-12-04 15:27:30
 */

#ifndef _LT_TTS_DEMO_H_
#define _LT_TTS_DEMO_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "ql_api_tts.h"

/*========================================================================
 *  Variable Definition
 *========================================================================*/
#define QL_TTS_TASK_PRIO 25
#define QL_TTS_TASK_STACK 20 * 1024

/* ========================================================
   1. 系统与重启 (System & Reboot)
   ======================================================== */
#define TTS_STR_SYS_REBOOT_5S       "设备将在5秒后重新启动."
#define TTS_STR_SYS_RESET_REBOOT    "设备重置成功,将在5秒后自动重启."
#define TTS_STR_SYS_SHUTDOWN_SOON   "设备即将关闭"
#define TTS_STR_SIM_INS_REBOOT      "检测到电话卡插入,系统将在5秒后重启"
#define TTS_STR_NO_SIM_CARD         "未插入电话卡."  // 或 "未插入SIM卡."，根据实际需求统一
#define TTS_STR_NET_UNAVAILABLE     "当前网络未连接,不可用"
#define TTS_STR_BATTERY_LOW         "当前电量低，请及时充电"

/* ========================================================
   2. 跌倒检测 (Fall Detection)
   ======================================================== */
#define TTS_STR_FALL_ALERT          "跌倒警报！15秒后自动拨打紧急求助电话。若为误报，请立即取消。"
#define TTS_STR_FALL_CANCEL         "当前告警已取消"

/* ========================================================
   3. 音频与模式切换 (Audio & Mode)
   ======================================================== */
#define TTS_STR_MODE_FM_NO_LIST     "未配置网络电台"
#define TTS_STR_MODE_SWITCH_FM      "切换到调频电台模式"
#define TTS_STR_MODE_SWITCH_NET_RADIO "切换到网络电台模式"
#define TTS_STR_MODE_SWITCH_NET_AUDIO "切换到网络音频模式"
#define TTS_STR_MODE_SWITCH_LOC_AUDIO "切换到本地音频模式"

#define TTS_STR_MP3_NO_LIST         "当前没有音乐列表"
#define TTS_STR_MP3_NET_NO_LIST     "当前没有获取网络音乐列表"

/* ========================================================
   4. 信号质量 (Signal Quality)
   ======================================================== */
#define TTS_STR_SIG_EXCELLENT       "当前信号质量优."
#define TTS_STR_SIG_GOOD            "当前信号质量一般."
#define TTS_STR_SIG_POOR            "当前信号质量差."
#define TTS_STR_SIG_NONE            "当前信号未连接."

/* ========================================================
   5. 短信提示 (SMS)
   ======================================================== */
#define TTS_STR_SMS_NONE_UNREAD     "当前没有未读信息"
#define TTS_STR_SMS_NEW_MSG         "您有新的消息，请注意查收"
#define TTS_STR_SMS_SHUTDOWN_WARN   "您有未读短信,请尽快读取,仍需关机,请保持长按"

/* ========================================================
   6. 语音指令 (Voice Command)
   ======================================================== */
#define TTS_STR_VCMD_QUERY          "请说出您的语音指令查询请求"

/* ========================================================
   7. 亲情通话 (Family Call)
   ======================================================== */
#define TTS_STR_NO_FAMILY_NUM       "未配置亲情号码"
#define TTS_STR_CALLING_FAMILY      "正在呼出家庭电话，请稍候"

/* ========================================================
   8. 生活服务 (Service Call)
   ======================================================== */
// 水务
#define TTS_STR_NO_WATER_NUM        "未配置水务号码"
#define TTS_STR_CALLING_WATER       "正在拨打水务电话，请稍候"
// 电力
#define TTS_STR_NO_ELEC_NUM         "未配置电力服务号码"
#define TTS_STR_CALLING_ELEC        "正在拨打电力服务电话，请稍候"
// 燃气
#define TTS_STR_NO_GAS_NUM          "未配置燃气服务号码"
#define TTS_STR_CALLING_GAS         "正在拨打燃气服务电话，请稍候"

/* ========================================================
   9. 紧急呼叫 (SOS)
   ======================================================== */
#define TTS_STR_NO_SOS_NUM          "未设置一键通号码"
#define TTS_STR_CALLING_SOS         "正在拨打一键通号码，请稍候"
#define TTS_STR_SOS_NEXT            "拨打下一个一键通号码"
#define TTS_STR_SOS_END             "一键通号码轮询结束"
  /*========================================================================
   *  function Definition
   *========================================================================*/
  void poc_demo_test(void);

  void ql_tts_demo2_init(void);
  int ql_tts_init(pUserCallback mCallback); // TTS 初始化
  int ql_tts_deinit(void);                  // TTS去初始化，释放资源
  int ql_tts_play(ql_tts_encoding_e mode, const char *string, uint len);
  void ql_pcm_poc_init_ex(void);                     // 开机后调用，只需初始化一次
  int ql_pcm_play_ex(uint8_t *data, uint32_t count); // 播放音频数据
  void ql_pcm_play_stop_ex(void);                    // 中止当前播放内容
  void ql_pcm_record_init_ex(void);                  // 打开录音模式
  int ql_pcm_record_ex(void *data, uint32_t count);  // 获取录音数据
  void ql_pcm_record_deinit_ex(void);                // 关闭录音,切换到播放模式
  void ql_pcm_poc_deinit_ex(void);                   // POC去初始化

  typedef void (*lt_tts_callback)(int type);

  typedef struct lt_tts_msg
  {
    int type;
    ql_tts_encoding_e encoding;
    uint32_t datalen;
    lt_tts_callback before_start;
    lt_tts_callback after_stop;
    uint8_t *data;
  } lt_tts_msg_t;

  typedef struct lt_tts_callback_s
  {
    void (*lt_tts_start_handle)(int type);
    void (*lt_tts_stop_handle)(int type);
  } lt_tts_callback_t;

  void lt_tts_callback_register(lt_tts_callback_t *reg);

  void ltapi_stop_tts_encoding(); // 停播
  /*QL_TTS_GBK =0, QL_TTS_UTF8 =1, QL_TTS_UCS2 = 2,*/
  void ltapi_play_tts_encoding(char *data, int data_len, ql_tts_encoding_e encoding);
  void ltapi_play_tts_encoding_withcallback(char *data, int data_len, ql_tts_encoding_e encoding, lt_tts_callback before_start, lt_tts_callback after_stop);
  /*QL_TTS_UTF8*/
  void ltapi_play_tts(char *data, int data_len);

  void ltapi_play_tts_withcallback(char *data, int data_len, ql_tts_encoding_e encoding, lt_tts_callback before_start, lt_tts_callback after_stop);

  void lt_tts_demo_init(void);
  // 格式化数字为 TTS 字符串
  void formatNumbersForTTS(char *input, char *output);
#ifdef __cplusplus
} /*"C" */
#endif

#endif // _LT_TTS_DEMO_H_
