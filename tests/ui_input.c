/* SPDX-License-Identifier: GPL-3.0-only */
#include "mcfm.h"
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static McEngine e;
static McUi u;
static uint16_t first[240*240],second[240*240];
static void draw(uint16_t *pixels) {
    for(unsigned y=0;y<240;y+=8)mc_ui_strip(&e,&u,y,8,pixels+y*240);
}
static void init(void) { mc_init(&e);mc_ui_init(&u);mc_preset(&e,0,0); }
static void timed(unsigned enc,int delta,uint32_t now) { mc_ui_encoder_at(&e,&u,enc,delta,now); }
static void acceleration(void) {
    init();timed(2,1,100);assert(u.preset[0]==1&&u.preset_popup);
    timed(2,1,159);assert(u.preset[0]==4);
    timed(2,1,219);assert(u.preset[0]==5);
    timed(2,-1,220);assert(u.preset[0]==4);
    timed(2,-1,221);assert(u.preset[0]==1);
    timed(2,1,500);assert(u.preset[0]==2);
    timed(1,1,501);assert(!u.preset_popup);timed(2,1,502);assert(u.preset[0]==3);
    mc_ui_button(&e,&u,UI_HOME);timed(2,1,503);assert(u.preset[0]==4);
    mc_ui_key(&e,&u,0,1);timed(2,1,504);assert(u.preset[0]==5);
    mc_ui_encoder(&e,&u,2,1);assert(!u.preset_popup&&u.preset[0]==6);
    timed(2,1,505);assert(u.preset[0]==7);
    mc_preset(&e,1,10);e.selected=1;timed(2,1,506);assert(u.preset[1]==11);
    u.page=1;timed(2,1,507);assert(u.preset[1]==12);
    init();u.page=2;timed(3,1,0);assert(e.project.bpm==119);
    timed(3,1,59);assert(e.project.bpm==125);
    timed(3,1,119);assert(e.project.bpm==126);
    timed(3,-1,120);assert(e.project.bpm==125);
    timed(3,-1,121);assert(e.project.bpm==119);
    timed(3,127,122);assert(e.project.bpm==240);
    init();timed(2,1,UINT32_MAX-20);timed(2,1,10);assert(u.preset[0]==4);
    puts("PASS: first/59ms/60ms/idle/reversal/encoder/button/key/context/raw reset and uint32 wrap; BPM6x, presets3x");
}
static void small_ranges(void) {
    init();timed(0,1,1);timed(0,1,2);assert(e.selected==2);
    init();timed(1,1,1);timed(1,1,2);assert(e.pattern==2);
    init();e.selected=3;timed(2,1,1);timed(2,1,2);assert(e.project.drum_kit==2);
    timed(3,1,3);timed(3,1,4);assert(u.sound==2);
    timed(4,1,5);timed(4,1,6);assert(e.edit_step==2);
    init();u.editing=1;timed(3,1,1);timed(3,1,2);assert(e.edit_step==2);
    unsigned flags=e.project.step[0][0][2].flags;timed(6,1,3);timed(6,1,4);
    assert(e.project.step[0][0][2].flags==(flags+2)%4);
    init();u.page=4;timed(3,1,1);timed(3,1,2);assert(u.demo_choice==1);
    puts("PASS: parts/patterns/kits/lanes/steps/flags/demo choices never accelerate");
}
static void popup(void) {
    init();timed(2,1,100);draw(first);assert(first[135*240+10]==0xfb20u);
    assert(!memcmp(&e.project.patch[0],&mc_presets[1],sizeof(McPatch)));
    mc_ui_tick(&u,599);assert(u.preset_popup);draw(second);assert(!memcmp(first,second,sizeof first));
    timed(2,0,599);timed(7,1,599);assert(u.preset_at==100);
    mc_ui_tick(&u,600);assert(!u.preset_popup);draw(second);assert(memcmp(first,second,sizeof first));
    timed(2,1,1000);timed(2,1,1499);mc_ui_tick(&u,1998);assert(u.preset_popup);
    mc_ui_tick(&u,1999);assert(!u.preset_popup);
    timed(2,1,UINT32_MAX-100);mc_ui_tick(&u,398);assert(u.preset_popup);
    mc_ui_tick(&u,399);assert(!u.preset_popup);
    timed(2,1,100);timed(3,1,101);assert(!u.preset_popup);
    timed(2,1,102);mc_ui_button(&e,&u,UI_PLAY);assert(e.playing&&!u.preset_popup);
    timed(2,1,103);mc_ui_key(&e,&u,1,1);assert(!u.preset_popup);
    e.selected=3;timed(2,1,104);assert(u.preset_popup);draw(first);
    mc_ui_tick(&u,604);assert(!u.preset_popup);draw(second);assert(memcmp(first,second,sizeof first));
    uint16_t strip[240*8+2];for(unsigned i=0;i<240*8+2;i++)strip[i]=0x1234;
    for(unsigned p=0;p<MC_PRESETS;p++) {
        e.selected=0;mc_preset(&e,0,p);timed(2,1,p*1000);
        for(unsigned y=0;y<240;y+=8) { mc_ui_strip(&e,&u,y,8,strip+1);assert(strip[0]==0x1234&&strip[240*8+1]==0x1234); }
    }
    puts("PASS: seven-row actual C popup, immediate patch,499/500ms, refresh/zero/invalid/wrap, controls dismiss, all128 strip guards");
}
static void performance_ui(void) {
    init();mc_ui_button(&e,&u,UI_ARP);assert(u.page==6&&!e.mute);
    mc_ui_encoder(&e,&u,3,1);assert(e.project.arp[0].enabled&&e.project.arp[0].mode==MC_ARP_UP);
    mc_ui_encoder(&e,&u,4,1);mc_ui_encoder(&e,&u,5,1);mc_ui_encoder(&e,&u,6,10);
    assert(e.project.arp[0].rate==0&&e.project.arp[0].octaves==2&&e.project.arp[0].gate==85);
    mc_ui_key(&e,&u,0,1);assert(e.arp[0].count==1);
    mc_ui_button(&e,&u,UI_LFO);assert(e.project.arp[0].latch&&!e.arp[0].count);
    mc_ui_key(&e,&u,0,1);mc_ui_encoder(&e,&u,0,1);assert(e.selected==1&&!e.arp[0].count);
    mc_ui_button(&e,&u,UI_ARP);assert(u.page==0);
    mc_ui_button(&e,&u,UI_GLO);mc_ui_button(&e,&u,UI_ARP);assert(u.page==5&&e.mute==2);
    mc_ui_button(&e,&u,UI_HOME);mc_ui_button(&e,&u,UI_FX);assert(u.page==3&&!u.fx_page);
    mc_ui_button(&e,&u,UI_ENV);assert(u.fx_page==1);
    for(unsigned k=0;k<4;k++)mc_ui_encoder(&e,&u,k+3,1);
    for(unsigned k=0;k<4;k++)assert(e.project.track_fx_type[1][k]==1);
    mc_ui_button(&e,&u,UI_FX);assert(u.fx_master&&u.fx_page==1);
    mc_ui_button(&e,&u,UI_ENV);mc_ui_encoder(&e,&u,3,20);mc_ui_encoder(&e,&u,4,30);
    mc_ui_encoder(&e,&u,5,127);assert(e.project.filter[0]==20&&e.project.filter[1]==30);
    mc_ui_button(&e,&u,UI_HOME);mc_ui_button(&e,&u,UI_FX);assert(!u.fx_page&&!u.fx_master);
    init();mc_start(&e);e.recording=1;mc_ui_encoder(&e,&u,3,1);assert(e.project.automation_count==1);
    mc_ui_button(&e,&u,UI_ENV);mc_ui_encoder(&e,&u,3,1);assert(e.project.automation_count==2);
    mc_ui_button(&e,&u,UI_GLO);mc_ui_encoder(&e,&u,4,1);assert(e.project.automation_count==3);
    mc_ui_button(&e,&u,UI_FX);mc_ui_encoder(&e,&u,3,1);assert(e.project.automation_count==4);
    mc_ui_button(&e,&u,UI_ENV);mc_ui_encoder(&e,&u,3,1);assert(e.project.automation_count==5);
    mc_ui_button(&e,&u,UI_ENV);mc_ui_encoder(&e,&u,3,1);assert(e.project.automation_count==6);
    mc_ui_button(&e,&u,UI_HOME);e.selected=3;mc_ui_encoder(&e,&u,6,1);assert(e.project.automation_count==7);
    for(unsigned n=0;n<4;n++)mc_ui_button(&e,&u,UI_GLO);
    assert(u.page==7);mc_ui_button(&e,&u,UI_EDIT);assert(!e.project.automation_count&&!u.editing);
    init();draw(first);mc_ui_encoder(&e,&u,3,1);draw(second);
    assert(e.project.preset[0]==0&&memcmp(first+30*240,second+30*240,14*240*sizeof(uint16_t)));
    unsigned star=0;for(unsigned y=30;y<44;y++)for(unsigned x=128;x<138;x++)star+=second[y*240+x]==0xdefbu;
    assert(star);mc_ui_encoder(&e,&u,3,-1);draw(second);assert(!memcmp(first,second,sizeof first));
    e.project.patch[0]=mc_presets[1];draw(second);assert(e.project.preset[0]==0);
    for(unsigned y=30;y<44;y++)assert(!memcmp(first+y*240+8,second+y*240+8,120*sizeof(uint16_t)));
    puts("PASS: ARP page/settings/clear, mixer mute, FX amount/type/filter scope, knob automation and pattern clear, origin-name star");
}
static void green_feedback(void) {
    init();e.playing=1;e.step=0;e.mute=0;e.project.step[0][0][0].velocity=100;e.project.step[0][0][0].note[0]=60;
    draw(first);assert(first[60*240+29]==0x67e8u);
    e.project.step[0][0][0].velocity=0;draw(first);assert(first[60*240+29]==0x2104u);
    e.project.step[0][0][0].velocity=100;e.mute=1;draw(first);assert(first[60*240+29]!=0x67e8u);
    e.mute=0;e.project.step[0][3][0].drums=1;e.project.step[0][3][0].velocity=0;
    draw(first);assert(first[141*240+29]==0x2104u);
    e.project.step[0][3][0].velocity=100;draw(first);assert(first[141*240+29]==0x67e8u);
    e.mute=8;draw(first);assert(first[141*240+29]!=0x67e8u);
    e.selected=3;e.mute=0;e.project.step[0][3][0].velocity=100;
    draw(first);assert(first[26*240+29]==0x67e8u&&first[35*240+29]!=0x67e8u);
    e.project.step[0][3][0].velocity=0;draw(first);assert(first[26*240+29]!=0x67e8u);
    e.project.step[0][3][0].velocity=100;e.mute=8;draw(first);assert(first[26*240+29]!=0x67e8u);
    puts("PASS: green cursor only on active unmuted melodic/drum hits; silent steps remain dark");
}
int main(void) { acceleration();small_ranges();popup();performance_ui();green_feedback();return 0; }
