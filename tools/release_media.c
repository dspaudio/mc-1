/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "../src/ui.h"
#include "../src/parallel.h"
#include <stdio.h>

static McEngine engine;
static McUi ui;

static int screenshot(const char *path) {
    FILE *file=fopen(path,"wb");
    if(!file)return 1;
    uint16_t strip[240*8];
    fprintf(file,"P6\n240 240\n255\n");
    for(unsigned y=0;y<240;y+=8) {
        mc_ui_strip(&engine,&ui,y,8,strip);
        for(unsigned i=0;i<240*8;i++) {
            unsigned c=strip[i];
            fputc((int)((c>>11)*255/31),file);
            fputc((int)(((c>>5)&63)*255/63),file);
            fputc((int)((c&31)*255/31),file);
        }
    }
    int failed=ferror(file);
    if(fclose(file))failed=1;
    return failed;
}

static void preview(unsigned demo,unsigned part) {
    mc_init(&engine);mc_demo_load(&engine,demo);mc_ui_init(&ui);
    engine.selected=(uint8_t)part;
    for(unsigned i=0;i<3;i++)ui.preset[i]=engine.project.preset[i];
    mc_start(&engine);
    int16_t pcm[256*2];
    for(unsigned remaining=MC_RATE/3u;remaining;) {
        unsigned n=remaining>256?256:remaining;
        mc_render(&engine,pcm,n);remaining-=n;
    }
}
static int capture(const char *directory,const char *name) {
    char path[512];
    if(snprintf(path,sizeof path,"%s/%s.ppm",directory,name)>=(int)sizeof path)return 1;
    mc_ui_prepare_frame(&engine,&ui);
    if(screenshot(path))return 1;
    printf("%s: page %u part %u pattern %u step %u Analog %u\n",
        name,ui.page,engine.selected,engine.pattern,engine.step,
        engine.selected<3?engine.project.analog[engine.selected]:0);
    return 0;
}
static void automation(void) {
    engine.recording=1;
    for(unsigned pattern=0;pattern<2;pattern++) {
        engine.pattern=(uint8_t)pattern;
        for(unsigned step=0;step<16;step++) {
            engine.step=(uint8_t)step;
            for(unsigned part=0;part<3;part++)
                (void)mc_parameter_set(&engine,part,MC_PARAM_LEVEL,80+step);
        }
    }
    engine.pattern=0;engine.step=2;
    ui.page=7;
}
int main(int argc,char **argv) {
    if(argc!=2) { fprintf(stderr,"Usage: %s output-directory\n",argv[0]);return 1; }
    const char *directory=argv[1];
#define CAPTURE(name) do { if(capture(directory,name))return 1; } while(0)
    preview(0,0);
    CAPTURE("v1-home");CAPTURE("home-playing-green");
    preview(0,3);
    CAPTURE("v1-drums");CAPTURE("drums-playing-green");
    preview(0,0);
    mc_preset(&engine,0,MC_PRESET_WOOD_MALLET);
    ui.preset[0]=MC_PRESET_WOOD_MALLET;ui.preset_popup=1;
    CAPTURE("v1-presets");CAPTURE("preset-popup");
    preview(0,1);ui.page=3;
    CAPTURE("v1-fx-sends");CAPTURE("fx-amounts");
    ui.fx_page=UI_FX_TYPES;
    CAPTURE("v1-fx-types");CAPTURE("fx-types");
    /* Retain old URLs, showing the current replacement for removed filters. */
    ui.fx_page=UI_FX_SENDS;CAPTURE("fx-filters");
    ui.fx_page=UI_FX_TYPES;CAPTURE("master-filters");
    preview(0,1);ui.page=10;CAPTURE("v1-analog");
    preview(0,1);ui.page=6;
    engine.project.arp[1]=(McArp){1,MC_ARP_UPDOWN,2,2,75,1};
    CAPTURE("v1-arp");CAPTURE("arp-updown-latch");
    engine.project.arp[1].enabled=0;CAPTURE("arp-off");
    preview(0,0);automation();
    CAPTURE("v1-automation");
    for(unsigned step=0;step<16;step++)for(unsigned part=0;part<4;part++)
        for(unsigned param=MC_PARAM_RD_SEND;param<=MC_PARAM_CF_SEND;param++) {
            engine.step=(uint8_t)step;
            (void)mc_parameter_set(&engine,part,param,40+step);
        }
    /* Capacity fixture uses only valid parameter writes, never invented events. */
    for(unsigned pattern=2;pattern<4;pattern++)for(unsigned step=0;step<16;step++)
        for(unsigned part=0;part<4;part++) {
            engine.pattern=(uint8_t)pattern;engine.step=(uint8_t)step;
            (void)mc_parameter_set(&engine,part,MC_PARAM_LEVEL,90+step);
            (void)mc_parameter_set(&engine,part,MC_PARAM_PAN,64);
        }
    engine.pattern=0;engine.step=2;CAPTURE("automation-full");
    mc_automation_clear(&engine,0);CAPTURE("automation-pattern-cleared");
    preview(1,1);CAPTURE("neon-spiral-origin");
    mc_ui_encoder(&engine,&ui,3,1);CAPTURE("preset-origin-star");
    preview(0,0);ui.page=5;CAPTURE("v1-mixer");
    preview(0,0);mc_stop(&engine);
    for(unsigned i=0;i<MC_SONG_ENTRIES;i++)
        if(mc_song_set(&engine,i,i%4,1))return 1;
    if(mc_song_length(&engine,MC_SONG_ENTRIES))return 1;
    mc_song_mode(&engine,1);mc_start(&engine);
    ui.page=8;ui.song_editing=1;
    CAPTURE("v1-song");CAPTURE("song-playing-128");
    preview(0,0);ui.page=9;
    CAPTURE("v1-midi");CAPTURE("midi-internal");
    for(unsigned demo=0;demo<2;demo++) {
        mc_init(&engine);mc_demo_load(&engine,demo);
        printf("DEMO %u bpm=%u voices=%u drums=%u worker=%u Analog2=%u preset3=%s PCM3=%u mix3=%u FX=%u,%u,%u,%u sends2=%u,%u,%u,%u\n",
            demo,engine.project.bpm,MC_VOICES,MC_DRUM_VOICES,MC_WORKER_VOICES,
            engine.project.analog[1],mc_preset_names[engine.project.preset[2]],
            engine.project.patch[2].pcm,engine.project.patch[2].pcm_mix,
            engine.project.fx_bus_type[0],engine.project.fx_bus_type[1],
            engine.project.fx_bus_type[2],engine.project.fx_bus_type[3],
            engine.project.fx_send[1][0],engine.project.fx_send[1][1],
            engine.project.fx_send[1][2],engine.project.fx_send[1][3]);
    }
    return 0;
}
