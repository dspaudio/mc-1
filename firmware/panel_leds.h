/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

static inline void mc_panel_key_leds(unsigned playing,unsigned step,uint32_t held,
                                     void (*set_key)(uint32_t,int)) {
    /* Held keys and the white-key step cursor share the same physical LEDs. */
    static const uint8_t white_keys[16]={0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26};
    uint32_t active=held&0x07ffffffu;
    if(playing&&step<16u)active|=1u<<white_keys[step];
    for(unsigned key=0;key<27u;key++)set_key(14u+key,(int)((active>>key)&1u));
}
