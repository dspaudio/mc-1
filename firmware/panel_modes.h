/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "../src/ui.h"
#ifndef MC_PLAY_LED_PULSE
#define MC_PLAY_LED_PULSE 1
#endif

/* 반환 비트는 물리 버튼 ID다. OCT와 건반은 포함하지 않는다.
 * FX/GLO 하위 화면에는 임의의 별도 표시 조합을 만들지 않는다. */
static inline uint16_t mc_panel_mode_mask(const McEngine *e,const McUi *u) {
    unsigned mask=0;
    switch(u->page) {
    case 0: mask|=1u<<8; break;
    case 1: mask|=1u<<4; break;
    case 2: case 4: case 5: case 7: case 9: case 10:
        mask|=1u<<7; break;
    case 3: mask|=1u<<2; break;
    case 6:
        mask|=1u<<10;
        if(e->selected<3&&e->project.arp[e->selected].latch)mask|=1u<<5;
        break;
    case 8:
        mask|=1u<<11;
        if(e->project.song_loop)mask|=1u<<5;
        break;
    default: break;
    }
    if(e->project.song_mode)mask|=1u<<11;
    if(u->page==8?u->song_editing:u->editing)mask|=1u<<6;
    if(u->save_pending)mask|=1u<<9;
    /* Quarter-note blink: two sixteenths on, two off, from the audio sequencer. */
    if(e->playing&&(!MC_PLAY_LED_PULSE||(e->step&3u)<2u))mask|=1u<<12;
    if(e->recording)mask|=1u<<13;
    return (uint16_t)mask;
}

static inline uint16_t mc_panel_led_mask(const McEngine *e,const McUi *u,uint32_t now_ms) {
    unsigned selected=mc_panel_mode_mask(e,u)&~(1u<<12);
    if(e->playing)selected&=~(1u<<8);
    unsigned lit=0x3fffu;
    if(now_ms%500u>=250u)lit&=~selected;
    if(e->playing&&MC_PLAY_LED_PULSE&&(e->step&3u)>=2u)lit&=~(1u<<12);
    return (uint16_t)lit;
}
