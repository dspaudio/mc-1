/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "../src/mcfm.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static McEngine e,a;
static int16_t buffer[4096];
static int16_t split[4096];
_Static_assert(offsetof(McEngine,live_keys)==offsetof(McEngine,pcm_cache)+2112,"Transient state must follow the PCM cache");
_Static_assert(sizeof(McEngine)-offsetof(McEngine,pcm_cache)<=2400+sizeof(McReverb)+sizeof(McDelay)+8,"Transient state budget exceeded");
_Static_assert(sizeof(McVoice)==84,"Voice layout must not change");
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
static void cleanup(unsigned channels,unsigned stopped) {
    McMidiMessage m;
    if(stopped)assert(mc_midi_pop(&e,&m)&&m.length==1&&m.data[0]==0xfc);
    const unsigned controls[3]={64,120,123};
    for(unsigned channel=0;channel<16;channel++) if(channels&(1u<<channel)) for(unsigned i=0;i<3;i++) {
        assert(mc_midi_pop(&e,&m)&&m.length==3&&m.data[0]==0xb0+channel);
        assert(m.data[1]==controls[i]&&!m.data[2]);
    }
    assert(!e.midi.cleanup&&!e.midi.stop_pending&&!e.midi.output_channels);
}
static void overflow_cleanup(void) {
    McMidiMessage m;
    for(unsigned channel=0;channel<16;channel++) {
        assert(mc_midi_pop(&e,&m)&&m.length==3&&m.data[0]==0xb0+channel);
        assert(m.data[1]==123&&!m.data[2]);
    }
    assert(!e.midi.panic&&!mc_midi_pop(&e,&m));
}
static void demo_cleanup(void) {
    McMidiMessage m;
    for(unsigned demo=0;demo<2;demo++) {
        mc_init(&e);mc_demo_load(&e,demo);unsigned programs=0;
        while(mc_midi_pop(&e,&m)) { assert((m.data[0]&0xf0)==0xc0);programs++; }
        assert(programs==(demo?3u:0u));
    }
    for(unsigned demo=0;demo<2;demo++) for(unsigned mode=0;mode<4;mode++) {
        mc_init(&e);e.project.midi_channel[0]=14;
        e.project.patch[0].engine=MC_LA;
        if(mode==1) { e.project.arp[0].enabled=1;e.project.arp[0].latch=1; }
        mc_key(&e,0,60,100,1);render(1);
        unsigned notes=0;
        while(mc_midi_pop(&e,&m)) if(m.data[0]==0x9e)notes++;
        assert(notes);
        if(mode==2) { input(1,0xbe,64,127);input(1,0x9e,64,100);input(1,0x8e,64,0); }
        /* 출력 정체 중 남은 Note On과 이미 발생한 overflow도 데모 교체로 폐기한다. */
        for(unsigned i=0;i<(mode==3?MC_MIDI_QUEUE+1u:8u);i++)mc_midi_send(&e,0x9e,72,100,3);
        if(mode==2)mc_midi_reset(&e,1);
        mc_demo_load(&e,demo);
        assert(!e.playing&&!active(0,60)&&!e.midi.sustain[0]);
        mc_key(&e,0,60,0,0);render(1);
        /* 정리 명령이 아직 전송되지 않아도 새 연주는 정리 뒤에 전달한다. */
        mc_note(&e,0,67,100,0,0);
        assert(!e.midi.panic);cleanup(1u<<14,0);notes=0;
        while(mc_midi_pop(&e,&m)) {
            assert((m.data[0]&0xf0)!=0xb0);
            if((m.data[0]&0xf0)==0x90) { assert(m.data[0]==0x90&&m.data[1]==67);notes++; }
        }
        assert(notes==1&&e.midi.output_channels==1);
    }
    /* 채널 변경 전 출력과 전송 도중 재로드에서도 미완료 채널을 잊지 않는다. */
    mc_init(&e);e.project.midi_channel[0]=4;mc_note(&e,0,60,100,0,0);drain();
    mc_midi_all_off(&e,0);e.project.midi_channel[0]=15;mc_note(&e,0,64,100,0,0);drain();
    mc_demo_load(&e,0);assert(mc_midi_pop(&e,&m)&&m.data[0]==0xb4&&m.data[1]==64);
    mc_demo_load(&e,1);cleanup((1u<<4)|(1u<<15),0);drain();
    mc_note(&e,0,60,100,0,0);e.project.midi_channel[1]=15;mc_note(&e,1,64,100,0,0);drain();
    mc_demo_load(&e,0);for(unsigned i=0;i<4;i++)assert(mc_midi_pop(&e,&m));
    mc_demo_load(&e,0);cleanup(1u<<15,0);assert(!mc_midi_pop(&e,&m));
    /* 이미 출력된 Start만 Stop으로 마감하며 새 Start는 정리 뒤에 전달한다. */
    mc_init(&e);memset(e.project.step,0,sizeof e.project.step);mc_start(&e);drain();
    mc_stop(&e);mc_demo_load(&e,0);mc_demo_load(&e,1);cleanup(0,1);drain();
    memset(e.project.step,0,sizeof e.project.step);mc_start(&e);mc_note(&e,0,60,100,0,0);mc_drum(&e,0,100);
    drain();mc_demo_load(&e,0);mc_start(&e);cleanup((1u<<0)|(1u<<9),1);
    assert(mc_midi_pop(&e,&m)&&m.data[0]==0xfa);drain();
    /* 정리 도중 overflow도 정리를 유지하며 기존 16채널 복구 정책을 따른다. */
    mc_init(&e);e.project.midi_channel[0]=14;mc_note(&e,0,60,100,0,0);drain();mc_demo_load(&e,0);
    for(unsigned i=0;i<MC_MIDI_QUEUE+10u;i++)mc_midi_send(&e,0x90,72,100,3);
    cleanup(1u<<14,0);overflow_cleanup();
    puts("PASS MIDI demo: owned channels, live/ARP/sustain/reset, pending output, reload, transport and overflow");
}
static void factory_default_reverb(void) {
    static const uint8_t sends[MC_PARTS]={12,72,24,8};
    McProject stored,loaded;uint8_t blob[MC_RECORD_BYTES];uint32_t seq;
    mc_init(&e);
    assert(e.project.fx_bus_type[0]==1&&!e.project.fx_bus_type[1]);
    for(unsigned part=0;part<MC_PARTS;part++) assert(e.project.fx_send[part][0]==sends[part]&&!e.project.fx_send[part][1]);
    /* Stored v7 sends are portable and never replaced by the factory reverb. */
    stored=e.project;memset(stored.fx_send,0,sizeof stored.fx_send);stored.fx_bus_type[0]=4;stored.fx_send[2][0]=99;
    assert(mc_encode(&stored,9,blob,sizeof blob)>0&&mc_decode(&loaded,&seq,blob,sizeof blob)==0);
    assert(!memcmp(loaded.fx_send,stored.fx_send,sizeof stored.fx_send)&&loaded.fx_bus_type[0]==4);
    /* Audible but subtle: the wet render differs from dry by a bounded amount. */
    int64_t diff=0,dry_sum=0;
    mc_init(&e);mc_init(&a);memset(a.project.fx_send,0,sizeof a.project.fx_send);
    mc_note(&e,0,48,100,0,3000);mc_note(&a,0,48,100,0,3000);
    for(unsigned block=0;block<8;block++) {
        mc_render(&e,buffer,1024);mc_render(&a,split,1024);
        for(unsigned i=0;i<2048;i++) {
            int32_t d=buffer[i]-split[i];diff+=d<0?-d:d;dry_sum+=split[i]<0?-split[i]:split[i];
        }
    }
    assert(diff>0&&diff<dry_sum/2);
    puts("PASS factory reverb: ROOM2 sends 12/72/24/8, no chorus, bounded wet-vs-dry difference, v7 sends preserved");
}
static void legacy_pcm_reference(void) {
    /* Original PCM references, retained after excluding pressure from this release. */
    static const uint32_t reference[3]={2313758043u,2814300718u,308011833u};
    for(unsigned mode=0;mode<3;mode++) {
        mc_init(&e);memset(e.project.fx_send,0,sizeof e.project.fx_send); /* dry legacy reference */
        for(unsigned part=0;part<3;part++) {
            e.project.patch[part].engine=(uint8_t)(mode==1?MC_ACID:MC_LA);
            e.project.patch[part].cutoff=43;
            if(mode==2) { e.project.patch[part].pcm=MC_PCM_ATTACKS;e.project.patch[part].pcm_mix=127; }
            mc_note(&e,part,48+part*7,100,0,3000);
        }
        uint32_t hash=2166136261u;
        for(unsigned block=0;block<4;block++) {
            render(1024);
            for(unsigned i=0;i<2048;i++) {
                unsigned sample=(uint16_t)buffer[i];
                hash=(hash^(sample&255))*16777619u;
                hash=(hash^(sample>>8))*16777619u;
            }
        }
        assert(hash==reference[mode]);
    }
    puts("PASS legacy PCM: bit-identical LA, acid and recorded PCM references");
}
static void shared_fx_controls(void) {
    mc_init(&e);memset(e.project.fx_send,0,sizeof e.project.fx_send);
    input(1,0xb0,91,77);input(1,0xb1,93,66);
    input(1,0xb9,20,7);input(1,0xb2,23,5);
    assert(e.project.fx_send[0][0]==77&&e.project.fx_send[1][1]==66);
    assert(!e.project.fx_send[1][0]&&!e.project.fx_send[0][1]);
    assert(e.project.fx_bus_type[0]==7&&e.project.fx_bus_type[1]==5&&!e.midi.count);
    input(1,0xb0,20,8);assert(e.project.fx_bus_type[0]==7);
    input(1,0xb0,12,127);assert(!e.project.track_fx[0][2]);
    assert(!mc_parameter_set(&e,2,MC_PARAM_RD_SEND,88));
    McMidiMessage m;
    assert(mc_midi_pop(&e,&m)&&m.data[0]==0xb2&&m.data[1]==91&&m.data[2]==88);
    assert(!mc_parameter_set(&e,3,MC_PARAM_CF_TYPE,7));
    assert(mc_midi_pop(&e,&m)&&m.data[0]==0xb0&&m.data[1]==23&&m.data[2]==7);
    assert(!mc_midi_pop(&e,&m));
    puts("PASS MIDI FX: independent part sends, global types, strict bounds and output CC routing");
}
static void ignored_pressure_messages(void) {
    mc_init(&e);e.project.patch[0].engine=MC_LA;
    input(1,0x90,60,100);McVoice before=e.voice[0];
    input(1,0xa0,60,127);input(1,0xd0,127,0);
    assert(!memcmp(&before,&e.voice[0],sizeof before)&&!e.midi.count);
    input(1,0x90,64,100);assert(active(0,64)&&!e.midi.count);
    puts("PASS MIDI: excluded pressure messages are consumed without voice changes or parser desynchronization");
}
int main(void) {
    factory_default_reverb();
    legacy_pcm_reference();
    shared_fx_controls();
    ignored_pressure_messages();
    demo_cleanup();
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
    drain();for(unsigned i=0;i<MC_MIDI_QUEUE+10u;i++) mc_midi_send(&e,0x90,60,100,3);
    assert(e.midi.dropped&&e.midi.panic);overflow_cleanup();
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
    mc_init(&e);mc_drum(&e,0,100);render(100);drain();
    int32_t tail=e.drum[0].env;assert(tail>0);mc_stop(&e);assert(e.drum[0].env==tail);
    unsigned drum_cleanup=0;while(mc_midi_pop(&e,&m)) if(m.data[0]==0xb9&&m.data[1]==123) drum_cleanup++;
    assert(drum_cleanup==1);render(1);assert(buffer[0]||buffer[1]);
    input(1,0xb9,120,0);for(unsigned i=0;i<MC_DRUM_VOICES;i++) assert(!e.drum[i].env);
    printf("PASS MIDI: parser/running status/realtime/sustain/CC/program/bend/drum+note record/no echo/gate off/panic/60-240 BPM clock\n");
}
