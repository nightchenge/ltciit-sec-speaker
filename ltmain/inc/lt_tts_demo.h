/*
 * @Descripttion:
 * @version:
 * @Author: zhouhao
 * @Date: 2024-07-11 14:40:47
 * @LastEditors: ubuntu
 * @LastEditTime: 2025-12-02 20:00:33
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
void formatNumbersForTTS(char *input, char *output) ;
#ifdef __cplusplus
} /*"C" */
#endif

#endif // _LT_TTS_DEMO_H_
