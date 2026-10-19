/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "../src/mcfm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static McEngine engine;
static void u32(FILE *f,uint32_t n) { for(unsigned i=0;i<4;i++)fputc((int)((n>>(8*i))&255),f); }
static void u16(FILE *f,unsigned n) { fputc((int)(n&255),f);fputc((int)((n>>8)&255),f); }
int main(int argc,char **argv) {
    if(argc<3||argc>5) {
        fprintf(stderr,"Usage: %s 0|1 output.wav [seconds [dry]]\n",argv[0]);return 1;
    }
    char *end;
    unsigned demo=(unsigned)strtoul(argv[1],&end,10);
    if(*end||demo>1)return 1;
    mc_init(&engine);mc_demo_load(&engine,demo);
    unsigned music=(MC_RATE*60u*4u*32u)/engine.project.bpm;
    unsigned frames=music+MC_RATE*5u;
    if(argc>3) {
        unsigned seconds=(unsigned)strtoul(argv[3],&end,10);
        if(*end||!seconds||seconds>600)return 1;
        frames=seconds*MC_RATE;
    }
    if(argc>4) {
        if(strcmp(argv[4],"dry"))return 1;
        memset(engine.project.fx_send,0,sizeof engine.project.fx_send);
    }
    FILE *file=fopen(argv[2],"wb");
    if(!file){perror(argv[2]);return 1;}
    fwrite("RIFF",1,4,file);u32(file,36u+frames*4u);
    fwrite("WAVEfmt ",1,8,file);u32(file,16);
    u16(file,1);u16(file,2);u32(file,MC_RATE);u32(file,MC_RATE*4u);u16(file,4);u16(file,16);
    fwrite("data",1,4,file);u32(file,frames*4u);
    mc_start(&engine);
    int16_t pcm[256*2];unsigned stopped=0;
    for(unsigned offset=0;offset<frames;) {
        if(offset>=music&&!stopped){mc_stop(&engine);stopped=1;}
        if(!stopped&&engine.events>=16&&engine.queued==255)mc_queue(&engine,(engine.pattern+1)%4);
        unsigned n=frames-offset;if(n>256)n=256;
        if(!stopped&&n>music-offset)n=music-offset;
        mc_render(&engine,pcm,n);
        for(unsigned i=0;i<n*2;i++)u16(file,(uint16_t)pcm[i]);
        offset+=n;
    }
    int failed=ferror(file);if(fclose(file))failed=1;
    printf("Demo %u: %u frames, 32 bars + tail, peak %u, clips %u, steals %u, part2 Analog %u, part3 PCM %u mix %u\n",
        demo,frames,engine.peak,engine.clips,engine.steals,engine.project.analog[1],
        engine.project.patch[2].pcm,engine.project.patch[2].pcm_mix);
    return failed;
}
