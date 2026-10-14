/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio
 * NEON SPIRAL: original psy-trance study. No Roland notes/samples imported.
 * Arrangement is host-driven performance of the actual shared firmware engine. */
#include "mcfm.h"
#include <stdio.h>
#include <string.h>
static McEngine e;
static void put16(FILE *f,unsigned n) { fputc((int)(n&255),f);fputc((int)((n>>8)&255),f); }
static void put32(FILE *f,unsigned n) { put16(f,n);put16(f,n>>16); }
int main(int argc,char **argv) {
    if(argc!=2) return 1;
    mc_init(&e);memset(e.project.step,0,sizeof e.project.step);
    e.project.bpm=140;e.project.swing=0;
    mc_preset(&e,0,7);mc_preset(&e,1,8);mc_preset(&e,2,0);
    e.project.patch[0].cutoff=65;e.project.patch[0].resonance=35;e.project.patch[0].decay=30;e.project.patch[0].sustain=45;
    e.project.patch[1].resonance=35;e.project.patch[1].glide=55;
    e.project.patch[2].attack=1;e.project.patch[2].decay=35;e.project.patch[2].sustain=15;e.project.patch[2].release=22;e.project.patch[2].pcm_mix=30;
    e.project.level[0]=127;e.project.level[1]=112;e.project.level[2]=82;e.project.level[3]=75;
    e.project.drum_level[0]=115;e.project.drum_level[2]=70;e.project.drum_level[3]=64;e.project.drum_level[4]=85;
    static const uint8_t roots[4]={40,40,43,41};
    static const uint8_t motif[16]={0,12,7,15,12,7,19,12,0,7,12,15,19,15,7,12};
    for(unsigned p=0;p<4;p++) for(unsigned s=0;s<16;s++) {
        McStep *bass=&e.project.step[p][0][s],*acid=&e.project.step[p][1][s],*arp=&e.project.step[p][2][s],*drum=&e.project.step[p][3][s];
        bass->note[0]=(uint8_t)(roots[p]+(s==15?12:0));bass->velocity=(uint8_t)(s%4?100:0);bass->gate=42;
        acid->note[0]=(uint8_t)(roots[p]+12+motif[(s+p*2)%16]);acid->velocity=(uint8_t)(s%4==0?115:90);acid->gate=50;
        acid->flags=(uint8_t)((s%4==0?MC_ACCENT:0)|(s%8==5?MC_SLIDE:0));
        if(s%2==0||s==15) { arp->note[0]=(uint8_t)(roots[p]+24+motif[(s+4)%16]);arp->velocity=88;arp->gate=35; }
        drum->drums=(uint16_t)((s%4==0?1:0)|(s%2==1?4:0)|(s%4==2?8:0)|(s%8==4?16:0)|(s%4==3?(1u<<12):0)|(p==3&&s>=14?(1u<<9):0));
        drum->velocity=(uint8_t)(s%4==0?120:85);drum->gate=50;
    }
    unsigned songframes=MC_RATE*240u*32u/140u,frames=songframes+MC_RATE;
    FILE *f=fopen(argv[1],"wb");if(!f)return 1;
    fwrite("RIFF",1,4,f);put32(f,36+frames*4);fwrite("WAVEfmt ",1,8,f);put32(f,16);
    put16(f,1);put16(f,2);put32(f,MC_RATE);put32(f,MC_RATE*4);put16(f,4);put16(f,16);fwrite("data",1,4,f);put32(f,frames*4);
    mc_start(&e);int16_t buf[128*2];unsigned previous=999;
    for(unsigned at=0;at<frames;) {
        unsigned n=frames-at>128?128:frames-at;
        unsigned bar=at/(MC_RATE*12u/7u);if(bar>31)bar=32;
        /* Break at bars 16..19; return with bass/drums, then final full mix. */
        uint8_t mute=(uint8_t)(bar<4?6:bar<8?4:bar<16?0:bar<20?13:bar<24?4:0);
        if(mute!=e.mute) for(unsigned p=0;p<4;p++) if((mute^e.mute)&(1u<<p)) mc_mute(&e,p);
        if(bar!=previous) { printf("bar %u: mute=%u\n",bar+1,mute);previous=bar; }
        unsigned section=bar<8?0:bar<16?1:bar<20?2:bar<24?3:4;
        unsigned phase=at%(MC_RATE*12u/7u*4u);
        int rise=(int)(phase*90u/(MC_RATE*12u/7u*4u));
        int cutoff=section==0?30+rise/2:section==1?35+rise:section==2?20+rise:section==3?95-rise/2:40+rise;
        int resonance=section==2?35+rise:section>=3?100:45;
        mc_control(&e,1,0,cutoff-e.project.patch[1].cutoff);
        mc_control(&e,1,1,resonance-e.project.patch[1].resonance);
        mc_control(&e,2,0,55+rise/2-e.project.patch[2].cutoff);
        if(e.events>=16&&e.queued==255)mc_queue(&e,(e.pattern+1)%4);
        if(at>songframes-MC_RATE&&at<songframes) e.master=(uint16_t)(28000u*(songframes-at)/MC_RATE);
        if(at>=songframes&&e.playing)mc_stop(&e);
        mc_render(&e,buf,n);
        for(unsigned i=0;i<n*2;i++)put16(f,(uint16_t)buf[i]);
        at+=n;
    }
    int failed=ferror(f);if(fclose(f))failed=1;
    printf("NEON SPIRAL: %u frames, peak %u, clips %u; host only\n",frames,e.peak,e.clips);
    return failed||e.clips!=0;
}
