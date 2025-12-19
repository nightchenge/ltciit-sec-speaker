/*
 * @Descripttion:
 * @version:
 * @Author: zhouhao
 * @Date: 2025-09-29 15:53:34
 * @LastEditors: ubuntu
 * @LastEditTime: 2025-12-08 13:59:27
 */

#ifndef _LTWEB_VOLUME_KNOB_H_
#define _LTWEB_VOLUME_KNOB_H_

typedef void (*on_volume_changed)(unsigned char volume);
void ltapi_set_knobvolume(int vol);
void ltapi_set_on_volume_changed(on_volume_changed cb);
int lt_volume_knob_init(void);

#endif // _LTWEB_VOLUME_KNOB_H_
