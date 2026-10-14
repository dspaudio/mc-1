/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static McEngine e,a;
static int16_t buffer[4096];
static int16_t split[4096];
static void render(unsigned n) { while(n) { unsigned k=n>2048?2048:n;mc_render(&e,buffer,k);n-=k; } }
static void input(unsigned s,unsigned status,unsigned x,unsigned y) {
    mc_midi_byte(&e,s,(uint8_t)status);mc_midi_byte(&e,s,(uint8_t)x);
    if((status&0xe0)!=0xc0) mc_midi_byte(&e,s,(uint8_t)y);
}
static unsigned active(unsigned part,unsigned note) {
    for(unsigned i=0;i<MC_VOICES;i++) if(e.voice[i].active&&e.voice[i].stage!=2&&e.voice[i].part==part&&e.voice[i].note==note) return 1;
    return 0;
}
static void drain(void) { McMidiMessage m;while(mc_midi_pop(&e,&m)) {} }
int main(void) {
    mc_init(&e);e.project.patch[0].engine=MC_LA;input(1,0x90,60,90);assert(active(0,60)&&!e.midi.count);
    mc_midi_byte(&e,1,64);mc_midi_byte(&e,1,0xf8);mc_midi_byte(&e,1,100);assert(active(0,64));
    input(2,0x91,67,80);assert(active(1,67));
    input(1,0xb0,64,127);input(1,0x80,60,0);assert(active(0,60));
    input(1,0xb0,64,0);assert(!active(0,60));input(1,0x90,64,0);assert(!active(0,64));
    input(1,0xb0,7,87);input(1,0xb0,10,23);input(1,0xb0,74,91);
    assert(e.project.level[0]==87&&e.project.pan[0]==23&&e.project.patch[0].cutoff==91&&!e.midi.count);
    input(1,0xc0,127,0);assert(e.project.preset[0]==127);input(1,0xc9,7,0);assert(e.project.drum_kit==7);
    input(1,0xe0,127,127);assert(e.midi.bend[0]==8191);input(1,0xe0,0,0);assert(e.midi.bend[0]==-8192);
    mc_midi_byte(&e,1,0xf0);mc_midi_byte(&e,1,60);mc_midi_byte(&e,1,0xf7);mc_midi_byte(&e,1,127);assert(!active(0,60));
    mc_midi_reset(&e,2);assert(!active(1,67));
    mc_init(&e);memset(e.project.step,0,sizeof e.project.step);e.recording=1;mc_start(&e);drain();
    input(1,0x90,60,99);input(1,0x99,36,88);
    assert(e.project.step[0][0][0].note[0]==60&&e.project.step[0][0][0].velocity==99&&e.project.step[0][3][0].drums==1);
    assert(!e.midi.count);input(1,0xb0,123,0);assert(!active(0,60));
    mc_init(&e);mc_note(&e,0,60,100,0,20);McMidiMessage m;assert(mc_midi_pop(&e,&m)&&m.data[0]==0x90&&m.data[1]==60);
    render(20);assert(mc_midi_pop(&e,&m)&&m.data[0]==0x80&&!e.voice[0].midi_out);
    mc_note(&e,0,62,100,0,0);drain();mc_mute(&e,0);assert(mc_midi_pop(&e,&m)&&m.data[1]==123);
    for(unsigned i=0;i<MC_MIDI_QUEUE+10;i++) mc_midi_send(&e,0x90,60,100,3);
    assert(e.midi.dropped&&e.midi.panic);unsigned panic=0;while(mc_midi_pop(&e,&m)) { assert(m.data[1]==123);panic++; }assert(panic==16);
    for(unsigned bpm=60;bpm<=240;bpm*=2) {
        mc_init(&e);memset(e.project.step,0,sizeof e.project.step);mc_clock_source(&e,1);drain();
        mc_midi_byte(&e,1,0xfa);assert(e.playing);unsigned phase=0;
        for(unsigned tick=0;tick<49;tick++) {
            mc_midi_byte(&e,1,0xf8);phase+=MC_RATE*60u;unsigned frames=phase/(bpm*24);phase%=bpm*24;render(frames);
        }
        assert(e.step==8&&e.project.bpm>=bpm-1&&e.project.bpm<=bpm+1);
        mc_midi_byte(&e,1,0xfc);assert(!e.playing);mc_midi_byte(&e,1,0xfb);assert(e.playing&&e.step==8);
    }
    mc_init(&e);memset(e.project.step,0,sizeof e.project.step);mc_start(&e);drain();render(MC_RATE/2);
    unsigned clocks=0;while(mc_midi_pop(&e,&m)) if(m.data[0]==0xf8) clocks++;assert(clocks==e.project.bpm/5);
    mc_init(&e);input(1,0x90,60,100);a=e;input(1,0xe0,127,127);render(1);mc_render(&a,buffer,1);
    assert(e.voice[0].phase>a.voice[0].phase&&e.voice[0].phase<a.voice[0].phase*113u/100u);
    input(1,0xb0,120,0);for(unsigned i=0;i<MC_VOICES;i++) assert(!e.voice[i].active||e.voice[i].part!=0);
    mc_init(&e);memset(e.project.step,0,sizeof e.project.step);mc_clock_source(&e,1);drain();
    e.project.step[0][0][1]=(McStep){{60,0,0},100,75,0,0};
    e.project.swing=0;
    mc_midi_byte(&e,1,0xfa);drain();for(unsigned i=0;i<6;i++) mc_midi_byte(&e,1,0xf8);
    assert(mc_midi_pop(&e,&m)&&m.data[0]==0x90&&m.data[1]==60);
    mc_init(&e);memset(e.project.step,0,sizeof e.project.step);e.project.arp[0].enabled=1;e.recording=1;
    input(1,0x90,60,90);input(1,0x90,64,80);mc_start(&e);
    assert(e.project.step[0][0][0].note[0]==60);unsigned notes=0;
    while(mc_midi_pop(&e,&m)) {
        if(m.data[0]==0x90) { assert(m.data[1]==60);notes++; }
    }
    assert(notes==1);
    mc_init(&e);mc_note(&e,0,60,100,0,441);input(1,0xe0,0,90);a=e;
    mc_render(&e,buffer,2048);for(unsigned i=0;i<2048;i+=32) mc_render(&a,split+2*i,32);
    assert(!memcmp(buffer,split,sizeof buffer)&&!memcmp(&e,&a,sizeof e));
    mc_init(&e);e.project.patch[0].engine=MC_ACID;
    mc_key(&e,0,53,100,1);assert(mc_midi_pop(&e,&m)&&m.data[0]==0x90&&m.data[1]==53);
    input(1,0x90,54,100);assert(mc_midi_pop(&e,&m)&&m.data[0]==0x80&&m.data[1]==53&&!e.midi.count);
    mc_key(&e,0,53,0,0);assert(!e.midi.count&&active(0,54));
    for(unsigned action=0;action<5;action++) {
        mc_init(&e);mc_key(&e,0,53,100,1);drain();
        if(action==0) input(1,0xc0,5,0);
        else if(action==1) input(1,0xb0,123,0);
        else if(action==2) input(1,0xb0,120,0);
        else if(action==3) mc_midi_reset(&e,1);
        else input(1,0x80,53,0);
        assert(mc_midi_pop(&e,&m)&&m.data[0]==0x80&&m.data[1]==53&&!e.midi.count);
    }
    printf("PASS MIDI: parser/running status/realtime/sustain/CC/program/bend/drum+note record/no echo/gate off/panic/60-240 BPM clock\n");
}
