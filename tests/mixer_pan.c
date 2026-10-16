/* SPDX-License-Identifier: GPL-3.0-only */
#include "mcfm.h"
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static McEngine e;
static McUi u;
static McProject decoded;
static uint8_t record[MC_RECORD_BYTES];
static int16_t audio[512*2];
static uint16_t level_view[240*240],pan_view[240*240],muted_view[240*240];
static void draw(uint16_t *pixels) {
    for(unsigned y=0;y<240;y+=8)mc_ui_strip(&e,&u,y,8,pixels+y*240);
}
static void init(void) {
    mc_init(&e);mc_ui_init(&u);
    static const uint8_t pan[4]={10,64,100,127},level[4]={20,40,60,80};
    memcpy(e.project.pan,pan,sizeof pan);memcpy(e.project.level,level,sizeof level);
    mc_ui_button(&e,&u,UI_GLO);assert(u.page==5&&!u.mixer_pan);
}
static void timed(unsigned enc,int delta,uint32_t now) { mc_ui_encoder_at(&e,&u,enc,delta,now); }
static void view_toggle(void) {
    init();
    mc_ui_button(&e,&u,UI_ENV);assert(u.page==5&&u.mixer_pan==1);
    mc_ui_button(&e,&u,UI_ENV);assert(u.page==5&&u.mixer_pan==0);
    mc_ui_button(&e,&u,UI_ENV);mc_ui_button(&e,&u,UI_GLO);assert(u.page==2&&!u.mixer_pan);
    for(unsigned i=0;i<4;i++)mc_ui_button(&e,&u,UI_GLO);
    assert(u.page==5&&!u.mixer_pan);
    mc_ui_button(&e,&u,UI_ENV);mc_ui_button(&e,&u,UI_HOME);assert(u.page==0);
    mc_ui_button(&e,&u,UI_GLO);assert(u.page==5&&!u.mixer_pan);
    u.page=0;mc_ui_button(&e,&u,UI_ENV);assert(u.page==1&&!u.mixer_pan);
    u.page=3;mc_ui_button(&e,&u,UI_ENV);assert(u.fx_page==UI_FX_TYPES&&!u.mixer_pan);
    puts("PASS: ENV toggles pan only on mixer page; GLO re-entry and other pages keep LEVEL");
}
static void four_pan_knobs(void) {
    static const uint8_t base[4]={10,64,100,127},level[4]={20,40,60,80};
    for(unsigned part=0;part<4;part++) {
        init();mc_ui_button(&e,&u,UI_ENV);
        mc_ui_encoder(&e,&u,3+part,-1);
        for(unsigned i=0;i<4;i++) {
            assert(e.project.pan[i]==base[i]-(i==part));
            assert(e.project.level[i]==level[i]);
        }
        mc_ui_encoder(&e,&u,3+part,2);assert(e.project.pan[part]==base[part]+(base[part]<127));
    }
    init();mc_ui_button(&e,&u,UI_ENV);
    mc_ui_encoder(&e,&u,3,127);assert(e.project.pan[0]==127);
    mc_ui_encoder(&e,&u,3,-127);mc_ui_encoder(&e,&u,3,-127);assert(e.project.pan[0]==0);
    mc_ui_encoder(&e,&u,4,-1);assert(e.project.pan[1]==63);
    mc_ui_encoder(&e,&u,4,1);mc_ui_encoder(&e,&u,4,1);assert(e.project.pan[1]==65);
    mc_ui_encoder(&e,&u,4,-1);assert(e.project.pan[1]==64);
    mc_ui_encoder(&e,&u,5,1);assert(e.project.pan[2]==101);
    mc_ui_encoder(&e,&u,6,-127);assert(e.project.pan[3]==0);
    mc_ui_encoder(&e,&u,6,127);mc_ui_encoder(&e,&u,6,127);assert(e.project.pan[3]==127);
    assert(!memcmp(e.project.level,level,sizeof level)&&!e.project.automation_count);
    puts("PASS: four pan knobs, 0/127 clamps, center 64, unchanged levels");
}
static void level_isolation(void) {
    static const uint8_t base[4]={10,64,100,127};
    init();
    for(unsigned part=0;part<4;part++) {
        uint8_t before[4];memcpy(before,e.project.level,sizeof before);
        mc_ui_encoder(&e,&u,3+part,1);
        for(unsigned i=0;i<4;i++)assert(e.project.level[i]==before[i]+(i==part));
        assert(!memcmp(e.project.pan,base,sizeof base));
    }
    mc_ui_button(&e,&u,UI_ENV);mc_ui_button(&e,&u,UI_ENV);
    mc_ui_encoder(&e,&u,3,-1);assert(e.project.level[0]==20&&e.project.pan[0]==10);
    puts("PASS: LEVEL view still edits four independent levels, never pan");
}
static void acceleration_reset(void) {
    init();timed(3,1,100);assert(e.project.level[0]==21);
    timed(3,1,110);assert(e.project.level[0]==24);
    mc_ui_button(&e,&u,UI_ENV);
    timed(3,1,115);assert(e.project.pan[0]==11&&e.project.level[0]==24);
    timed(3,1,120);assert(e.project.pan[0]==14);
    mc_ui_button(&e,&u,UI_ENV);
    timed(3,1,125);assert(e.project.level[0]==25&&e.project.pan[0]==14);
    puts("PASS: view button resets encoder acceleration in both directions");
}
static void mute_indication(void) {
    init();mc_ui_button(&e,&u,UI_ENV);draw(pan_view);
    mc_ui_button(&e,&u,UI_ARP);assert(u.page==5&&u.mixer_pan&&(e.mute&1));
    mc_ui_encoder(&e,&u,3,1);assert(e.project.pan[0]==11);
    draw(muted_view);assert(memcmp(pan_view,muted_view,sizeof pan_view));
    e.selected=2;draw(pan_view);e.selected=0;draw(level_view);assert(memcmp(pan_view,level_view,sizeof pan_view));
    mc_ui_button(&e,&u,UI_ARP);assert(!e.mute);
    mc_ui_button(&e,&u,UI_ENV);draw(level_view);
    mc_ui_button(&e,&u,UI_ENV);draw(pan_view);assert(memcmp(level_view,pan_view,sizeof pan_view));
    puts("PASS: ARP mute works in pan view and changes the rendered mixer");
}
static void roundtrip(void) {
    uint32_t sequence=0;
    init();mc_ui_button(&e,&u,UI_ENV);
    for(unsigned part=0;part<4;part++)mc_ui_encoder(&e,&u,3+part,part&1?-17:17);
    assert(mc_encode(&e.project,7,record,sizeof record)>=0);
    assert(mc_decode(&decoded,&sequence,record,sizeof record)==0&&sequence==7);
    assert(!memcmp(decoded.pan,e.project.pan,sizeof decoded.pan)&&decoded.pan[0]==27&&decoded.pan[1]==47);
    puts("PASS: UI pan edits persist through project encode/decode");
}
static void automation(void) {
    init();mc_ui_button(&e,&u,UI_ENV);
    mc_ui_button(&e,&u,UI_PLAY);mc_ui_button(&e,&u,UI_REC);assert(e.playing&&e.recording);
    for(unsigned part=0;part<4;part++)mc_ui_encoder(&e,&u,3+part,part==1?-5:3);
    assert(e.project.automation_count==4);
    for(unsigned i=0;i<4;i++) {
        const McAutomation *a=&e.project.automation[i];
        assert(a->address==i&&a->step==0&&a->parameter==MC_PARAM_PAN&&a->value==e.project.pan[i]);
    }
    uint8_t recorded[4];memcpy(recorded,e.project.pan,sizeof recorded);
    mc_ui_button(&e,&u,UI_PLAY);assert(!e.playing);
    memset(e.project.pan,0,sizeof e.project.pan);
    mc_ui_button(&e,&u,UI_REC);assert(!e.recording);
    mc_ui_button(&e,&u,UI_PLAY);mc_render(&e,audio,512);
    assert(!memcmp(e.project.pan,recorded,sizeof recorded));
    puts("PASS: PLAY+REC pan edits record and replay MC_PARAM_PAN automation");
}
int main(void) {
    view_toggle();four_pan_knobs();level_isolation();acceleration_reset();
    mute_indication();roundtrip();automation();
    return 0;
}
