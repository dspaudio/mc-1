/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
/* Original short pieces rendered by the shared engine, not external instruments. */
#include "mcfm.h"
#include <stdio.h>
#include <string.h>
static McEngine e;
static const char *const names[]={"piano","violin","strings","guitar"};
static const uint8_t chords[8][4]={
    {60,64,67,71},{57,60,64,67},{53,57,60,64},{55,59,62,65},
    {60,64,67,71},{57,60,64,67},{53,57,60,65},{55,60,64,67}
};
static const uint8_t melody[8][8]={
    {72,76,79,76,74,72,71,67},{72,76,81,79,76,74,72,71},
    {69,72,77,76,74,72,69,67},{71,74,79,77,74,71,69,67},
    {76,79,84,83,79,76,74,72},{76,81,79,76,74,72,71,69},
    {72,77,81,79,77,76,74,71},{74,71,67,71,72,72,72,72}
};
static void u16(FILE *f,unsigned v) { fputc((int)(v&255),f);fputc((int)((v>>8)&255),f); }
static void u32(FILE *f,unsigned v) { u16(f,v);u16(f,v>>16); }
static int render(unsigned kind,unsigned wet,const char *directory) {
    static const unsigned presets[]={MC_PRESET_PIANO_BRIGHT,MC_PRESET_VIOLIN_BOW,
        MC_PRESET_VIOLA_CHAMBER,MC_PRESET_NYLON_PLUCK};
    const unsigned tick=MC_RATE*60u/(112u*2u),frames=tick*64+MC_RATE*3;
    char path[512];int16_t audio[256*2];
    snprintf(path,sizeof path,"%s/%s-%s.wav",directory,names[kind],wet?"reverb":"dry");
    FILE *f=fopen(path,"wb");if(!f)return 1;
    fwrite("RIFF",1,4,f);u32(f,36+frames*4);fwrite("WAVEfmt ",1,8,f);u32(f,16);
    u16(f,1);u16(f,2);u32(f,MC_RATE);u32(f,MC_RATE*4);u16(f,4);u16(f,16);
    fwrite("data",1,4,f);u32(f,frames*4);
    mc_init(&e);memset(e.project.fx_send,0,sizeof e.project.fx_send);
    memset(e.project.analog,0,sizeof e.project.analog);
    e.project.fx_bus_type[0]=1; /* ROOM2 only; no pitch modulation. */
    for(unsigned part=0;part<3;part++) {
        mc_preset(&e,part,presets[kind]);e.project.level[part]=90;
        e.project.fx_send[part][0]=(uint8_t)(wet?40:0);
    }
    if(kind==2) {
        mc_preset(&e,0,MC_PRESET_CELLO_ENSEMBLE);
        mc_preset(&e,1,MC_PRESET_VIOLA_CHAMBER);
        mc_preset(&e,2,MC_PRESET_VIOLIN_SILVER);
        e.project.pan[0]=48;e.project.pan[2]=80;
        e.project.level[2]=72;
    }
    for(unsigned at=0;at<frames;) {
        if(at<tick*64&&at%tick==0) {
            unsigned step=at/tick,bar=step/8,beat=step%8;
            const uint8_t *chord=chords[bar];
            if(kind==0) {
                mc_note(&e,0,melody[bar][beat],beat%2?92:108,0,tick*9/10);
                mc_note(&e,1,chord[(beat+1)%4],70,0,tick*4/5);
                if(!(beat%4))mc_note(&e,2,chord[0]-12u,84,0,tick*3);
            } else if(kind==1) {
                if(!(beat%2))mc_note(&e,0,melody[bar][beat],104,0,tick*19/10);
            } else if(kind==2) {
                if(!beat) {
                    mc_note(&e,0,chord[0]-12u,86,0,tick*7);
                    mc_note(&e,1,chord[1],82,0,tick*7);
                    mc_note(&e,1,chord[2],78,0,tick*7);
                    mc_note(&e,2,chord[3],88,0,tick*7);
                }
            } else {
                static const unsigned pick[8]={0,2,1,3,0,2,3,1};
                unsigned note=chord[pick[beat]];
                if(!beat||beat==4)note-=12;
                mc_note(&e,0,note,beat%2?95:112,0,tick*17/10);
                if(beat==6)mc_note(&e,1,melody[bar][beat]-12u,76,0,tick);
            }
        }
        unsigned boundary=at<tick*64?(at/tick+1)*tick:frames;
        unsigned n=boundary-at;if(n>256)n=256;
        mc_render(&e,audio,n);
        for(unsigned i=0;i<n*2;i++)u16(f,(uint16_t)audio[i]);
        at+=n;
    }
    int failed=ferror(f);if(fclose(f))failed=1;
    printf("%s: %u frames, peak=%u clips=%u steals=%u\n",path,frames,e.peak,e.clips,e.steals);
    return failed||e.clips||e.steals;
}
int main(int argc,char **argv) {
    if(argc<2||argc>3) { fputs("usage: render-instruments OUTPUT_DIRECTORY [piano|violin|strings|guitar]\n",stderr);return 1; }
    unsigned first=0,last=4;
    if(argc==3) {
        while(first<4&&strcmp(argv[2],names[first]))first++;
        if(first==4)return 1;
        last=first+1;
    }
    for(unsigned kind=first;kind<last;kind++)for(unsigned wet=0;wet<2;wet++)
        if(render(kind,wet,argv[1]))return 1;
    return 0;
}
