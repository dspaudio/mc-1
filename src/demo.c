/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio. Original NEON SPIRAL composition. */
#include "mcfm.h"
void mc_demo_load(McEngine *e,unsigned demo) {
    if(!demo) { mc_init(e);return; }
    mc_init(e);for(unsigned i=0;i<sizeof e->project.step;i++) ((uint8_t *)e->project.step)[i]=0;
    e->project.bpm=140;e->project.swing=0;
    mc_preset(e,0,7);mc_preset(e,1,8);mc_preset(e,2,0);
    e->project.patch[0].cutoff=65;e->project.patch[0].resonance=35;e->project.patch[0].decay=30;e->project.patch[0].sustain=45;
    e->project.patch[1].resonance=35;e->project.patch[1].glide=55;
    e->project.patch[2].attack=1;e->project.patch[2].decay=35;e->project.patch[2].sustain=15;e->project.patch[2].release=22;e->project.patch[2].pcm_mix=30;
    e->project.level[0]=127;e->project.level[1]=112;e->project.level[2]=82;e->project.level[3]=75;
    e->project.drum_level[0]=115;e->project.drum_level[2]=70;e->project.drum_level[3]=64;e->project.drum_level[4]=85;
    static const uint8_t roots[4]={40,40,43,41};
    static const uint8_t motif[16]={0,12,7,15,12,7,19,12,0,7,12,15,19,15,7,12};
    for(unsigned p=0;p<4;p++) for(unsigned s=0;s<16;s++) {
        McStep *bass=&e->project.step[p][0][s],*acid=&e->project.step[p][1][s],*arp=&e->project.step[p][2][s],*drum=&e->project.step[p][3][s];
        bass->note[0]=(uint8_t)(roots[p]+(s==15?12:0));bass->velocity=(uint8_t)(s%4?100:0);bass->gate=42;
        acid->note[0]=(uint8_t)(roots[p]+12+motif[(s+p*2)%16]);acid->velocity=(uint8_t)(s%4==0?115:90);acid->gate=50;
        acid->flags=(uint8_t)((s%4==0?MC_ACCENT:0)|(s%8==5?MC_SLIDE:0));
        if(s%2==0||s==15) { arp->note[0]=(uint8_t)(roots[p]+24+motif[(s+4)%16]);arp->velocity=88;arp->gate=35; }
        drum->drums=(uint16_t)((s%4==0?1:0)|(s%2==1?4:0)|(s%4==2?8:0)|(s%8==4?16:0)|(s%4==3?(1u<<12):0)|(p==3&&s>=14?(1u<<9):0));
        drum->velocity=(uint8_t)(s%4==0?120:85);drum->gate=50;
    }
    e->project.demo=1;
    e->project.fx[0]=48;e->project.fx[1]=35;e->project.fx[2]=20;e->project.fx[3]=30;
}
void mc_demo_tick(McEngine *e) {
    if(!e->project.demo)return;
    unsigned bar=(e->events?e->events-1:0)/16;
    if(bar>=32) { mc_stop(e);return; }
    uint8_t mute=(uint8_t)(bar<4?6:bar<8?4:bar<16?0:bar<20?13:bar<24?4:0);
    if(mute!=e->mute)for(unsigned p=0;p<4;p++)if((mute^e->mute)&(1u<<p))mc_mute(e,p);
    if(e->queued==255)mc_queue(e,(e->pattern+1)%4);
    if(e->demo_frame%64)return;
    unsigned section=bar<8?0:bar<16?1:bar<20?2:bar<24?3:4;
    unsigned span=MC_RATE*240u*4u/e->project.bpm;
    int rise=(int)((e->demo_frame%span)*90u/span);
    int cutoff=section==0?30+rise/2:section==1?35+rise:section==2?20+rise:section==3?95-rise/2:40+rise;
    int resonance=section==2?35+rise:section>=3?100:45;
    mc_control(e,1,0,cutoff-e->project.patch[1].cutoff);
    mc_control(e,1,1,resonance-e->project.patch[1].resonance);
    mc_control(e,2,0,55+rise/2-e->project.patch[2].cutoff);
}
