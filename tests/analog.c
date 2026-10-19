/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "../src/mcfm.h"
#include "../src/ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static McEngine warm,dry,split;
static int16_t a[514],b[514],c[514];
static void fixture(McEngine *e,unsigned amount) {
    mc_init(e);
    memset(e->project.step,0,sizeof e->project.step);
    memset(e->project.fx_send,0,sizeof e->project.fx_send);
    for(unsigned p=0;p<3;p++) {
        e->project.patch[p]=(McPatch){MC_LA,0,0,0,100,0,0,0,127,100,0,100};
        assert(!mc_parameter_set(e,p,MC_PARAM_ANALOG,amount));
    }
}
static void notes(McEngine *e) {
    for(unsigned i=0;i<MC_VOICES;i++)mc_note(e,i%3,60,100,0,0);
}
static void bounds(const McEngine *e) {
    for(unsigned i=0;i<MC_VOICES;i++) {
        const McAnalogVoice *v=&e->analog_voice[i];
        assert(v->tuning>=-307&&v->tuning<=306);
        assert(v->drift>=-32768&&v->drift<=32512);
        assert(v->target>=-32768&&v->target<=32512);
    }
}
static void partition_identity(unsigned amount) {
    fixture(&warm,amount);notes(&warm);split=warm;
    unsigned changed=0;
    for(unsigned block=0;block<160;block++) {
        mc_render(&warm,a,257);
        for(unsigned f=0;f<257;) {
            unsigned n=(f+block)%17+1;if(n>257-f)n=257-f;
            mc_render(&split,c+f*2,n);f+=n;
        }
        assert(!memcmp(a,c,sizeof a)&&!memcmp(&warm,&split,sizeof warm));
        if(amount) {
            bounds(&warm);
            for(unsigned i=0;i<MC_VOICES;i++)changed|=warm.analog_voice[i].drift!=0;
        }
    }
    if(amount)assert(changed);
    else {
        McAnalogVoice zero[MC_VOICES]={0};
        assert(!memcmp(warm.analog_voice,zero,sizeof zero));
    }
    assert(!warm.steals&&!warm.clips);
}
static void variance_and_drift(void) {
    fixture(&warm,127);fixture(&dry,0);notes(&warm);notes(&dry);
    assert(warm.random==dry.random);
    mc_render(&warm,a,1);mc_render(&dry,b,1);
    unsigned distinct=0;
    for(unsigned i=0;i<MC_VOICES;i++) {
        distinct|=warm.analog_voice[i].tuning!=warm.analog_voice[0].tuning;
        int32_t delta=(int32_t)(warm.voice[i].phase-dry.voice[i].phase);
        if(delta<0)delta=-delta;
        assert((uint32_t)delta<dry.voice[i].phase/100+1);
    }
    assert(distinct);
    unsigned changed=0;
    for(unsigned n=0;n<160;n++) {
        mc_render(&warm,a,257);mc_render(&dry,b,257);
        changed|=memcmp(a,b,sizeof a)!=0;
        assert(warm.random==dry.random);
        bounds(&warm);
    }
    assert(changed&&!warm.steals&&!warm.clips);
    fixture(&warm,127);fixture(&dry,0);
    for(unsigned i=0;i<MC_DRUM_VOICES;i++){mc_drum(&warm,i,100);mc_drum(&dry,i,100);}
    for(unsigned n=0;n<32;n++) {
        mc_render(&warm,a,257);mc_render(&dry,b,257);
        assert(!memcmp(a,b,sizeof a)&&warm.random==dry.random);
    }
    puts("PASS: bounded independent voice variance and smoothed drift; original drum randomness unchanged");
}
static void live_changes(void) {
    fixture(&warm,0);notes(&warm);mc_render(&warm,a,257);
    assert(!mc_parameter_set(&warm,0,MC_PARAM_ANALOG,127));
    dry=warm;dry.project.analog[0]=0;
    mc_render(&warm,a,1);mc_render(&dry,b,1);
    assert(warm.analog_voice[0].random&&!warm.steals);
    assert(warm.voice[0].envelope==dry.voice[0].envelope);
    assert(!mc_parameter_set(&warm,0,MC_PARAM_ANALOG,0));
    dry=warm;mc_render(&warm,a,257);mc_render(&dry,b,257);
    assert(!memcmp(a,b,sizeof a)&&!memcmp(&warm,&dry,sizeof warm));
    fixture(&warm,127);warm.project.patch[0].engine=MC_ACID;warm.project.patch[0].glide=90;
    mc_note(&warm,0,48,100,0,0);mc_render(&warm,a,257);
    McAnalogVoice before=warm.analog_voice[0];
    mc_note(&warm,0,55,100,MC_SLIDE,0);
    assert(!memcmp(&before,&warm.analog_voice[0],sizeof before));
    mc_render(&warm,a,257);assert(warm.voice[0].note==55&&!warm.steals);
    puts("PASS: live Analog changes retain envelopes and voice count; Acid legato preserves voice identity");
}
static void controls_and_storage(void) {
    static uint8_t record[MC_RECORD_BYTES];
    static McProject loaded;
    McUi u;uint32_t seq=0;
    fixture(&warm,0);mc_ui_init(&u);
    assert(mc_parameter_get(&warm,3,MC_PARAM_ANALOG)<0);
    assert(mc_parameter_set(&warm,3,MC_PARAM_ANALOG,1)<0);
    assert(mc_parameter_set(&warm,4,MC_PARAM_ANALOG,1)<0);
    assert(mc_parameter_set(&warm,0,MC_PARAM_ANALOG,128)<0);
    for(unsigned i=0;i<6;i++)mc_ui_button(&warm,&u,UI_GLO);
    assert(u.page==10);
    for(unsigned part=0;part<3;part++) {
        warm.selected=(uint8_t)part;
        mc_ui_encoder(&warm,&u,3,1000);assert(warm.project.analog[part]==127);
        mc_ui_encoder(&warm,&u,3,-1000);assert(!warm.project.analog[part]);
        mc_ui_encoder_at(&warm,&u,3,1,100);mc_ui_encoder_at(&warm,&u,3,1,101);
        assert(warm.project.analog[part]==4);
        McProject before=warm.project;
        for(unsigned k=4;k<7;k++)mc_ui_encoder(&warm,&u,k,127);
        assert(!memcmp(&before,&warm.project,sizeof before));
    }
    warm.selected=3;McProject before=warm.project;
    mc_ui_encoder(&warm,&u,3,127);assert(!memcmp(&before,&warm.project,sizeof before));
    assert(mc_encode(&warm.project,91,record,sizeof record)==MC_RECORD_BYTES);
    assert(!memcmp(record+20+35114,warm.project.analog,3));
    assert(!mc_decode(&loaded,&seq,record,sizeof record)&&seq==91);
    assert(!memcmp(&loaded,&warm.project,sizeof loaded));
    warm.project.analog[2]=128;assert(!mc_project_valid(&warm.project));
    warm.project.analog[2]=4;
    warm.selected=1;warm.recording=1;mc_start(&warm);
    mc_ui_encoder(&warm,&u,3,9);
    assert(warm.project.automation_count==1);
    assert(warm.project.automation[0].address==1&&warm.project.automation[0].parameter==MC_PARAM_ANALOG);
    warm.recording=0;warm.project.analog[1]=0;mc_stop(&warm);mc_start(&warm);
    assert(warm.project.analog[1]==13);
    assert(mc_project_valid(&warm.project));
    puts("PASS: Analog bounds, GLO page, knob acceleration, melodic-only automation and v8 roundtrip");
}
int main(void) {
    partition_identity(0);partition_identity(127);
    puts("PASS: Analog off/on PCM and complete engine state independent of render partition");
    variance_and_drift();live_changes();controls_and_storage();
    return 0;
}
