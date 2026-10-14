/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static McEngine engine;
static McUi ui;
static void u32(FILE *f,uint32_t n) { for(int i=0;i<4;i++) fputc((int)((n>>(i*8))&255),f); }
static void u16(FILE *f,uint16_t n) { fputc(n&255,f);fputc(n>>8,f); }
static int wav(const char *path,unsigned seconds,int demo) {
    if(seconds>600||!seconds) return 1;
    FILE *f=fopen(path,"wb"); if(!f) { perror(path);return 1; }
    unsigned frames=seconds*MC_RATE;int16_t audio[256*2];
    fwrite("RIFF",1,4,f);u32(f,36+frames*4);fwrite("WAVEfmt ",1,8,f);u32(f,16);
    u16(f,1);u16(f,2);u32(f,MC_RATE);u32(f,MC_RATE*4);u16(f,4);u16(f,16);
    fwrite("data",1,4,f);u32(f,frames*4);
    clock_t start=clock();
    for(unsigned i=0;i<frames;) {
        unsigned n=frames-i>256?256:frames-i;
        if(demo&&engine.events>=16&&engine.queued==255) mc_queue(&engine,(engine.pattern+1)%4);
        mc_render(&engine,audio,n);
        for(unsigned j=0;j<n*2;j++) u16(f,(uint16_t)audio[j]);
        i+=n;
    }
    int bad=ferror(f);if(fclose(f)) bad=1;
    printf("%s: %u frames, peak %u, clips %u, steals %u, CPU %.3fs (host only)\n",path,frames,engine.peak,engine.clips,engine.steals,(double)(clock()-start)/CLOCKS_PER_SEC);
    return bad;
}
static int screenshot(const char *path) {
    FILE *f=fopen(path,"wb"); if(!f) return 1;
    uint16_t strip[240*8];fprintf(f,"P6\n240 240\n255\n");
    for(unsigned y=0;y<240;y+=8) {
        mc_ui_strip(&engine,&ui,y,8,strip);
        for(unsigned i=0;i<240*8;i++) { unsigned c=strip[i];fputc((int)((c>>11)*255/31),f);fputc((int)(((c>>5)&63)*255/63),f);fputc((int)((c&31)*255/31),f); }
    }
    int bad=ferror(f);if(fclose(f)) bad=1;return bad;
}
static int save(const char *path) {
    uint8_t blob[MC_RECORD_BYTES];if(mc_encode(&engine.project,1,blob,sizeof blob)<0) return 1;
    FILE *f=fopen(path,"wb");if(!f) return 1;
    int rc=fwrite(blob,1,sizeof blob,f)!=sizeof blob;if(fclose(f)) rc=1;return rc;
}
static int load(const char *path) {
    uint8_t blob[MC_RECORD_BYTES+1];uint32_t seq=0;
    FILE *f=fopen(path,"rb");if(!f) return 1;
    unsigned n=(unsigned)fread(blob,1,sizeof blob,f);fclose(f);
    int rc=mc_decode(&engine.project,&seq,blob,n);if(!rc) mc_stop(&engine);return rc;
}
int main(int argc,char **argv) {
    mc_init(&engine);mc_ui_init(&ui);
    if(argc>1&&!strcmp(argv[1],"render")) { mc_start(&engine);return wav(argc>2?argv[2]:"demo.wav",argc>3?(unsigned)atoi(argv[3]):16,1); }
    if(argc>1&&!strcmp(argv[1],"screen")) { engine.selected=argc>3?(uint8_t)atoi(argv[3]):3; if(engine.selected>3) return 1;return screenshot(argc>2?argv[2]:"screen.ppm"); }
    if(argc>1&&!strcmp(argv[1],"preset")) {
        if(argc<4) return 1;
        unsigned p=(unsigned)atoi(argv[2]); if(p>=MC_PRESETS) return 1;
        mc_preset(&engine,0,p);mc_note(&engine,0,p>=6&&p<=9?45:60,110,0,MC_RATE*2);return wav(argv[3],4,0);
    }
    puts("MC-1 host console. Shared firmware DSP, sequencer and panel. No device I/O.");
    puts("Commands: play, stop, part 0..3, pattern 0..3, preset 0..11, knob 0..6 delta, button 0..13,");
    puts("key 0..26 down(1/0), render path seconds, screen path.ppm, save path.mcf, load path.mcf, quit");
    char line[512],cmd[32],path[384];int a,b;
    while(fputs("mc1> ",stdout),fflush(stdout),fgets(line,sizeof line,stdin)) {
        if(sscanf(line,"%31s",cmd)!=1) continue;
        if(!strcmp(cmd,"quit")) break;
        if(!strcmp(cmd,"play")) mc_start(&engine);
        else if(!strcmp(cmd,"stop")) mc_stop(&engine);
        else if(sscanf(line,"part %d",&a)==1&&a>=0&&a<4) engine.selected=(uint8_t)a;
        else if(sscanf(line,"pattern %d",&a)==1) mc_queue(&engine,(unsigned)a);
        else if(sscanf(line,"preset %d",&a)==1) { mc_preset(&engine,engine.selected,(unsigned)a);if(engine.selected<3&&a>=0&&a<12) ui.preset[engine.selected]=(uint8_t)a; }
        else if(sscanf(line,"knob %d %d",&a,&b)==2) mc_ui_encoder(&engine,&ui,(unsigned)a,b);
        else if(sscanf(line,"button %d",&a)==1) mc_ui_button(&engine,&ui,(unsigned)a);
        else if(sscanf(line,"key %d %d",&a,&b)==2) mc_ui_key(&engine,&ui,(unsigned)a,b);
        else if(sscanf(line,"render %383s %d",path,&a)==2) { if(wav(path,(unsigned)a,0)) puts("Render failed"); }
        else if(sscanf(line,"screen %383s",path)==1) { if(screenshot(path)) puts("Screen failed"); }
        else if(sscanf(line,"save %383s",path)==1) { if(save(path)) puts("Save failed"); }
        else if(sscanf(line,"load %383s",path)==1) { if(load(path)) puts("Load rejected"); }
        else puts("Unknown or invalid command");
        if(ui.load_request&&!engine.playing) { unsigned demo=ui.load_request-1;mc_demo_load(&engine,demo);mc_ui_init(&ui);ui.message="DEMO LOADED - PLAY"; }
    }
    return 0;
}
