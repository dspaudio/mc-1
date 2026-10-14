/* SPDX-License-Identifier: GPL-3.0-only */
#include "mcfm.h"
#include "ui.h"
static McEngine engine;
static McUi ui;
static uint32_t ui_now;
void host_tick(uint32_t now_ms) { ui_now=now_ms;mc_ui_tick(&ui,now_ms); }
static uint8_t held_part[27],held_note[27];
static int16_t audio[4410*2];
static uint16_t screen[240*240]; /* Host only; device uses a 3840-byte strip. */
void host_init(void) { mc_init(&engine);mc_ui_init(&ui); }
void host_action(unsigned kind,int a,int b) {
    if(kind>=3&&kind<=5)ui.preset_popup=ui.encoder_active=0;
    if(kind==0) mc_ui_button(&engine,&ui,(unsigned)a);
    else if(kind==1) mc_ui_encoder_at(&engine,&ui,(unsigned)a,b,ui_now);
    else if(kind==2&&a>=0&&a<27) {
        ui.preset_popup=ui.encoder_active=0;
        if(b) {
            held_part[a]=engine.selected;int note=53+a+ui.octave*12;
            held_note[a]=(uint8_t)(note<12?12:note>108?108:note);mc_ui_key(&engine,&ui,(unsigned)a,1);
        } else if(held_part[a]<3) mc_key(&engine,held_part[a],held_note[a],100,0);
    }
    else if(kind==3&&a>=0&&a<4) { mc_arp_clear(&engine,engine.selected);engine.selected=(uint8_t)a; }
    else if(kind==4&&a>=0&&a<MC_PRESETS&&engine.selected<3) { ui.preset[engine.selected]=(uint8_t)a;mc_preset(&engine,engine.selected,(unsigned)a); }
    else if(kind==5) mc_queue(&engine,(unsigned)a);
    if(ui.load_request&&!engine.playing) { unsigned demo=ui.load_request-1;mc_demo_load(&engine,demo);mc_ui_init(&ui);ui.message="DEMO LOADED - PLAY"; }
    if(ui.save_request) { ui.message="USE EXPORT PROJECT";ui.save_request=0; }
}
const int16_t *host_audio(void) { mc_render(&engine,audio,4410);return audio; }
const uint16_t *host_screen(void) {
    for(unsigned y=0;y<240;y+=8) mc_ui_strip(&engine,&ui,y,8,screen+y*240);
    return screen;
}
int host_export(uint8_t *out) { return mc_encode(&engine.project,1,out,MC_RECORD_BYTES); }
int host_import(const uint8_t *in,unsigned n) {
    uint32_t seq;McProject p;
    int rc=mc_decode(&p,&seq,in,n);
    if(!rc) { mc_init(&engine);engine.project=p;mc_ui_init(&ui); }
    return rc;
}
