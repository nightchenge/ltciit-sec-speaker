/*
 * @Descripttion:
 * @version:
 * @Author: zhouhao
 * @Date: 2025-09-29 15:53:34
 * @LastEditors: zhouhao
 * @LastEditTime: 2025-09-29 15:54:08
 */

#ifndef _LTWEB_VOLUME_KNOB_H_
#define _LTWEB_VOLUME_KNOB_H_

typedef void (*on_volume_changed)(unsigned char volume);

void ltapi_set_on_volume_changed(on_volume_changed cb);
int lt_volume_knob_init(void);

#endif // _LTWEB_VOLUME_KNOB_H_
