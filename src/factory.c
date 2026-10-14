/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
const char *const mc_preset_names[MC_PRESETS] = {
    "VECTOR LEAD", "ROUND BASS", "SILK PAD", "GLASS BELL", "NYLON PLUCK", "ORBITAL MT",
    "ACID SAW", "SQUARE RUBBER", "HIGH VOLTAGE", "SOFT STEP", "REED STACK", "METAL RAIN"
};
/* engine,wave,attack PCM,mix,cutoff,resonance,A,D,S,R,glide,level */
const McPatch mc_presets[MC_PRESETS] = {
    {0,0,0,75,85,20,1,40,90,25,0,95},
    {0,1,4,40,42,10,1,35,80,15,0,105},
    {0,0,2,25,55,15,90,80,95,100,0,85},
    {0,2,1,105,100,8,1,100,15,90,0,100},
    {0,1,0,95,70,12,1,50,25,25,0,110},
    {0,2,3,90,78,25,15,100,65,90,0,90},
    {1,0,0,0,48,90,1,55,45,10,55,100},
    {1,1,0,0,38,70,1,70,70,15,70,100},
    {1,0,0,0,80,110,1,40,35,8,40,90},
    {1,1,0,0,50,35,2,85,85,25,85,100},
    {0,1,4,60,68,20,8,50,90,40,0,90},
    {0,2,5,110,90,35,1,110,15,90,0,100}
};
static void clear_bytes(void *p, unsigned n) { uint8_t *b=p; while(n--) *b++=0; }
void mc_factory(McProject *p)
{
    static const uint8_t roots[4]={45,41,48,43};
    static const uint8_t melody[16]={0,0,7,0,12,10,0,7,3,0,7,10,12,0,15,10};
    clear_bytes(p,sizeof *p);
    for(unsigned d=0;d<MC_DRUMS;d++) p->drum_level[d]=100;
    p->bpm=118; p->swing=10;
    p->patch[0]=mc_presets[6]; p->patch[1]=mc_presets[2]; p->patch[2]=mc_presets[3];
    for(unsigned t=0;t<4;t++) { p->level[t]=t==1?75:100; p->pan[t]=t==1?42:t==2?88:64; }
    for(unsigned b=0;b<4;b++) for(unsigned s=0;s<16;s++) {
        McStep *bass=&p->step[b][0][s], *pad=&p->step[b][1][s];
        McStep *lead=&p->step[b][2][s], *drum=&p->step[b][3][s];
        bass->note[0]=(uint8_t)(roots[b]+((s==6||s==14)?12:0));
        bass->velocity=(s%4==0)?115:85; bass->gate=65;
        bass->flags=(s%4==0?MC_ACCENT:0)|(s==5||s==13?MC_SLIDE:0);
        if(s==3||s==11) bass->velocity=0;
        if(s%8==0) {
            pad->note[0]=roots[b]+12; pad->note[1]=roots[b]+15; pad->note[2]=roots[b]+19;
            pad->velocity=76; pad->gate=100; /* long pad release bridges rests */
        }
        if(s%2==0||s==7||s==15) {
            lead->note[0]=(uint8_t)(roots[b]+24+melody[s]); lead->velocity=(uint8_t)(80+(s%3)*10); lead->gate=55;
        }
        drum->drums=(s%4==0?1:0)|(s%8==4?2:0)|(s%2==0?4:0)|(s==14?8:0)|(s==12?16:0)|(b==3&&s==15?32:0);
        drum->velocity=s%4==0?112:85; drum->gate=50;
    }
}
