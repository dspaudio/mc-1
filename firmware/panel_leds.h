/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

static inline void mc_panel_step_leds(unsigned playing,unsigned step,
                                      void (*set_key)(uint32_t,int)) {
    /* F3..G5의 흰 건반 16개를 왼쪽부터 스텝에 대응한다. */
    static const uint8_t white_keys[16]={0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26};
    unsigned active=playing&&step<16u?white_keys[step]:27u;
    for(unsigned key=0;key<27u;key++)set_key(14u+key,key==active);
}
