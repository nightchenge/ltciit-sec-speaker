/*
 * @Author: zhaixianwen
 * @Date: 2023-09-07 10:21:41
 * @LastEditTime: 2024-04-02 09:16:32
 * @LastEditors: zhouhao
 * @Description: In User Settings Edit
 * @FilePath: /LTE01R02A01_BETA0726_C_SDK_G/components/ql-application/ltmain/inc/ltsms.h
 */


#ifndef LTADC_H
#define LTADC_H


#ifdef __cplusplus
extern "C" {
#endif

int lt_adc_set_volume_instantly();
void lt_adc_init(void);
int lt_adc_battery_get(void);
#ifdef __cplusplus
} /*"C" */
#endif

#endif /* LTADC_H */


