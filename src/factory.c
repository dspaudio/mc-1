/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
/* Generated from presets.json by tools/gen_presets.py. */
const char *const mc_preset_names[MC_PRESETS] = {
    "VECTOR LEAD",
    "ROUND BASS",
    "SILK PAD",
    "GLASS BELL",
    "NYLON PLUCK",
    "ORBITAL MT",
    "ACID SAW",
    "SQUARE RUBBER",
    "HIGH VOLTAGE",
    "SOFT STEP",
    "REED STACK",
    "METAL RAIN",
    "DEEP CELL",
    "CLUB PULSE",
    "CLICK FLOOR",
    "WIRE PICK",
    "DUB CHAMBER",
    "RESONA BASS",
    "LIQUID STEP",
    "BRIGHT COIL",
    "HOLLOW REED",
    "NEEDLE LINE",
    "BUBBLE RUN",
    "VELVET MONO",
    "RAZOR MONO",
    "WIDE SAW",
    "SOFT PIPE",
    "SEQUENCE KEY",
    "POLY CHROME",
    "BRASS BLOCK",
    "SOFT HORNS",
    "STRING VEIL",
    "SLOW HALO",
    "BELL CLOUD",
    "DUSK SCREEN",
    "AIR CHAMBER",
    "HAMMER KEY",
    "SOFT TINES",
    "CLAVI CLICK",
    "DRAWBAR KEY",
    "PERC ORGAN",
    "WOOD MALLET",
    "GLASS TINES",
    "THUMB PLUCK",
    "WIRE ZITHER",
    "CHORD STAB",
    "SPACE SHARD",
    "DIGITAL MIST",
};
/* engine,wave,PCM,mix,cutoff,resonance,A,D,S,R,glide,level */
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
    {0,2,5,110,90,35,1,110,15,90,0,100},
    {0,2,4,12,22,6,1,70,100,22,0,115},
    {0,1,8,52,40,22,1,35,65,12,0,103},
    {0,0,6,100,38,14,1,28,45,10,0,103},
    {0,1,7,108,53,12,1,45,30,24,0,104},
    {0,2,4,28,28,35,3,90,85,50,0,109},
    {1,0,0,0,35,103,1,48,35,12,55,94},
    {1,1,0,0,59,96,1,75,38,18,75,94},
    {1,0,0,0,95,48,1,65,90,25,24,90},
    {1,1,0,0,76,25,5,60,96,30,38,93},
    {1,0,0,0,67,116,1,24,20,8,20,85},
    {1,1,0,0,30,108,1,35,28,8,62,100},
    {1,1,0,0,44,15,14,90,100,40,85,98},
    {1,0,0,0,108,78,1,28,80,16,15,82},
    {0,0,10,18,102,15,5,48,100,28,0,91},
    {0,2,9,38,64,8,9,75,105,38,0,100},
    {0,1,0,68,75,44,1,30,20,14,0,100},
    {0,0,8,34,72,30,5,55,90,40,0,89},
    {0,0,10,72,77,21,9,45,85,22,0,92},
    {0,0,10,28,46,10,20,60,95,42,0,91},
    {0,0,9,24,60,11,70,100,102,88,0,82},
    {0,0,2,12,40,30,115,100,104,120,0,86},
    {0,2,1,60,88,12,45,100,80,110,0,93},
    {0,1,3,42,49,32,82,65,90,104,0,85},
    {0,2,9,65,69,15,100,110,85,120,0,98},
    {0,2,6,114,94,5,1,86,25,45,0,108},
    {0,2,11,76,78,7,1,100,45,65,0,105},
    {0,1,7,114,83,18,1,28,20,15,0,103},
    {0,1,8,16,60,3,2,12,117,12,0,93},
    {0,2,8,90,83,4,1,30,105,15,0,99},
    {0,2,7,105,67,4,1,45,10,28,0,113},
    {0,2,11,116,111,6,1,80,8,66,0,102},
    {0,2,7,86,58,11,1,36,15,25,0,113},
    {0,0,7,108,66,23,1,50,12,38,0,106},
    {0,0,10,65,86,30,1,25,5,20,0,100},
    {0,2,3,122,112,54,1,116,10,116,0,98},
    {0,1,5,100,65,45,64,100,70,112,0,89},
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
