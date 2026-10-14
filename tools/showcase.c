/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
#include <stdio.h>
#include <string.h>
static McEngine e;
static void put16(FILE *f,unsigned n) { fputc((int)(n&255),f);fputc((int)((n>>8)&255),f); }
static void put32(FILE *f,unsigned n) { put16(f,n);put16(f,n>>16); }
static int kit_showcase(const char *path) {
    const unsigned segment=MC_RATE*8,frames=segment*MC_KITS;int16_t audio[256*2];
    FILE *f=fopen(path,"wb");if(!f)return 1;
    fwrite("RIFF",1,4,f);put32(f,36+frames*4);fwrite("WAVEfmt ",1,8,f);put32(f,16);
    put16(f,1);put16(f,2);put32(f,MC_RATE);put32(f,MC_RATE*4);put16(f,4);put16(f,16);
    fwrite("data",1,4,f);put32(f,frames*4);
    for(unsigned kit=0;kit<MC_KITS;kit++) {
        mc_init(&e);e.project.drum_kit=(uint8_t)kit;e.mute=7;e.project.bpm=120;e.project.swing=0;
        unsigned next=0;
        for(unsigned at=0;at<segment;) {
            if(at==next&&at<MC_RATE*4) { mc_drum(&e,at*4/MC_RATE,110);next+=MC_RATE/4; }
            if(at==MC_RATE*4)mc_start(&e);
            if(at==MC_RATE*7)mc_stop(&e);
            unsigned boundary=at<MC_RATE*4?next:at<MC_RATE*7?MC_RATE*7:segment;
            unsigned n=boundary-at;if(n>256)n=256;
            mc_render(&e,audio,n);for(unsigned i=0;i<n*2;i++)put16(f,(uint16_t)audio[i]);at+=n;
        }
        printf("kit %u %s: peak=%u clips=%u\n",kit,mc_kit_names[kit],e.peak,e.clips);
        if(e.clips) { fclose(f);return 1; }
    }
    int failed=ferror(f);if(fclose(f))failed=1;return failed;
}
static int recorded_showcase(const char *path) {
    const unsigned segment=MC_RATE*3,frames=segment*MC_RECORDED_SAMPLES;int16_t audio[256*2];
    FILE *f=fopen(path,"wb");if(!f)return 1;
    fwrite("RIFF",1,4,f);put32(f,36+frames*4);fwrite("WAVEfmt ",1,8,f);put32(f,16);
    put16(f,1);put16(f,2);put32(f,MC_RATE);put32(f,MC_RATE*4);put16(f,4);put16(f,16);
    fwrite("data",1,4,f);put32(f,frames*4);
    for(unsigned id=0;id<MC_RECORDED_SAMPLES;id++) {
        unsigned preset=0;while(preset<MC_PRESETS&&mc_presets[preset].pcm!=MC_PCM_ATTACKS+id)preset++;
        if(preset==MC_PRESETS) { fclose(f);return 1; }
        mc_init(&e);mc_preset(&e,0,preset);mc_note(&e,0,60,110,0,MC_RATE*2);
        for(unsigned at=0;at<segment;) {
            unsigned n=segment-at;if(n>256)n=256;
            mc_render(&e,audio,n);for(unsigned i=0;i<n*2;i++)put16(f,(uint16_t)audio[i]);at+=n;
        }
        printf("recorded %u %s: peak=%u clips=%u\n",id,mc_preset_names[preset],e.peak,e.clips);
        if(e.clips) { fclose(f);return 1; }
    }
    int failed=ferror(f);if(fclose(f))failed=1;return failed;
}
static int render(const char *path,unsigned kind) {
    const unsigned seconds=16,frames=MC_RATE*seconds;int16_t audio[256*2];
    mc_init(&e);e.project.bpm=120;e.project.swing=12;
    if(kind==0) {
        mc_preset(&e,0,MC_PRESET_VECTOR_LEAD);mc_preset(&e,1,MC_PRESET_SILK_PAD);mc_preset(&e,2,MC_PRESET_GLASS_BELL);e.mute=8;
        /* LA lead, pad and bell: same factory notes, no drums. */
    } else if(kind==1) {
        e.mute=14;mc_preset(&e,0,MC_PRESET_ACID_SAW);
        for(unsigned p=0;p<4;p++) for(unsigned s=0;s<16;s++) {
            McStep *st=&e.project.step[p][0][s];
            st->note[0]=(uint8_t)(st->note[0]+(s%4==2?7:0));
            st->flags=(uint8_t)((s%4==0?MC_ACCENT:0)|(s%4==2?MC_SLIDE:0));
        }
    } else {
        e.mute=7;
        for(unsigned p=0;p<4;p++) for(unsigned s=0;s<16;s++) {
            McStep *st=&e.project.step[p][3][s];
            if(s%4==2) st->drums|=1u<<12;
            if(s==0&&p%2==0) st->drums|=1u<<10;
            if(s==6||s==14) st->drums|=1u<<13;
            if(p==3&&s>=12) st->drums|=(uint16_t)(1u<<(s==12?5:s==13?15:s==14?9:7));
        }
    }
    FILE *f=fopen(path,"wb");if(!f) return 1;
    fwrite("RIFF",1,4,f);put32(f,36+frames*4);fwrite("WAVEfmt ",1,8,f);put32(f,16);
    put16(f,1);put16(f,2);put32(f,MC_RATE);put32(f,MC_RATE*4);put16(f,4);put16(f,16);
    fwrite("data",1,4,f);put32(f,frames*4);mc_start(&e);
    for(unsigned at=0;at<frames;) {
        unsigned n=frames-at>256?256:frames-at;
        if(e.events>=16&&e.queued==255) mc_queue(&e,(e.pattern+1)%4);
        if(kind==1) {
            int cutoff=25+(int)((at%(MC_RATE*4))*90/(MC_RATE*4));
            mc_control(&e,0,0,cutoff-e.project.patch[0].cutoff);
            /* Second half changes from saw to square while preserving the groove. */
            if(at>=MC_RATE*8&&e.project.patch[0].wave==0) mc_preset(&e,0,MC_PRESET_SQUARE_RUBBER);
        }
        if(kind==0&&at>=MC_RATE*8&&e.project.patch[2].pcm==1) mc_preset(&e,2,MC_PRESET_ORBITAL_MT);
        mc_render(&e,audio,n);
        for(unsigned i=0;i<n*2;i++) put16(f,(uint16_t)audio[i]);
        at+=n;
    }
    int failed=ferror(f);if(fclose(f)) failed=1;
    printf("%s: actual C core, peak=%u, clips=%u\n",path,e.peak,e.clips);
    return failed||e.clips!=0;
}
int main(int argc,char **argv) {
    if(argc!=3) { fprintf(stderr,"usage: showcase la|303|808|kits|recorded OUTPUT.wav\n");return 1; }
    if(!strcmp(argv[1],"recorded"))return recorded_showcase(argv[2]);
    if(!strcmp(argv[1],"kits"))return kit_showcase(argv[2]);
    unsigned kind=!strcmp(argv[1],"la")?0:!strcmp(argv[1],"303")?1:!strcmp(argv[1],"808")?2:3;
    return kind>2?1:render(argv[2],kind);
}
