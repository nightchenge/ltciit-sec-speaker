#ifndef _LTFM_H_
#define _LTFM_H_

typedef enum lt_fm_status
{
    FM_POWR_OFF,
    FM_POWR_ON
}lt_fm_status_t;

//fm_ino
typedef struct fm_q_info
{
    uint32_t fm_freq;
    uint32_t fm_vol;
    bool    fm_CH; //FALSE 不在搜台，TRUE，在搜台
} fm_q_info_t;


/**
 * @brief 打开/关闭FM电源控制开关
 * @param lt_fm_status_t  : FM电源开关状态
 * @return 
 *       void
 */
void lt_fm_power_status_set(lt_fm_status_t st);

/**
 * @brief FM电源状态获取
 * @param void ： 无
 * @return 
 *       lt_fm_status_t  : FM电源开关状态
 */
lt_fm_status_t lt_fm_power_status_get(void);

void lt_fm_setvol(int vol);
void lt_fm_app_init(void);

void lt_fm_key_callback_register();


void lt_fm_play_callback_register();


void lt_fm_asr_callback_register();



#endif // _LTFM_H_