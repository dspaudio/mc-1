/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static McEngine e;
static McProject p;
static uint8_t record[MC_RECORD_BYTES];
static void clocks(unsigned n) { while(n--) mc_clock_tick(&e); }
static void crc(void) {
    uint32_t v=mc_crc(record+20,record[6]+((unsigned)record[7]<<8));
    for(unsigned i=0;i<4;i++) record[12+i]=(uint8_t)(v>>(i*8));
    v=mc_crc(record,16);for(unsigned i=0;i<4;i++) record[16+i]=(uint8_t)(v>>(i*8));
}
int main(void) {
    mc_init(&e);assert(mc_project_valid(&e.project));
    assert(!mc_song_set(&e,0,4,2)&&!mc_song_set(&e,1,63,1));
    assert(mc_song_set(&e,128,0,1)<0&&mc_song_set(&e,0,64,1)<0&&mc_song_set(&e,0,0,0)<0);
    mc_song_mode(&e,1);mc_start(&e);assert(!e.playing);
    assert(!mc_song_length(&e,2));mc_clock_source(&e,1);mc_start(&e);
    assert(e.pattern==4&&e.song_position==0&&e.step==0);
    clocks(96);assert(e.pattern==4&&e.song_repeat==1&&e.playing);
    clocks(96);assert(e.pattern==63&&e.song_position==1&&e.song_repeat==0);
    clocks(95);assert(e.playing);clocks(1);assert(!e.playing);
    e.project.song_loop=1;mc_start(&e);clocks(288);assert(e.playing&&e.pattern==4&&e.song_position==0);
    clocks(18);mc_stop(&e);unsigned step=e.step;mc_continue(&e);assert(e.step==step&&e.playing);
    mc_queue(&e,10);assert(e.queued==255);mc_song_mode(&e,0);assert(!e.playing);
    mc_start(&e);mc_queue(&e,42);clocks((16-e.step)*6);assert(e.pattern==42);
    mc_stop(&e);assert(!mc_song_length(&e,128));
    for(unsigned i=0;i<128;i++) assert(!mc_song_set(&e,i,i%64,i%16+1));
    e.project.song_mode=1;e.project.midi_channel[0]=15;e.project.clock_source=2;
    assert(mc_encode(&e.project,99,record,sizeof record)==MC_RECORD_BYTES&&record[4]==6);
    uint32_t sequence=0;assert(!mc_decode(&p,&sequence,record,sizeof record)&&sequence==99);
    assert(!memcmp(&p,&e.project,sizeof p));
    record[20+34830+1]=0;crc();assert(mc_decode(&p,&sequence,record,sizeof record)<0);
    assert(mc_encode(&e.project,99,record,sizeof record)>0);
    record[4]=5;record[6]=34830&255;record[7]=34830>>8;crc();
    assert(!mc_decode(&p,&sequence,record,34850)&&!p.song_length&&!p.song_mode&&p.midi_channel[3]==9);
    for(unsigned flag=MC_SLIDE;flag<=MC_TIE;flag*=2) for(unsigned scenario=0;scenario<5;scenario++) {
        mc_init(&e);memset(e.project.step,0,sizeof e.project.step);e.project.swing=0;
        mc_song_set(&e,0,4,(scenario==1||scenario==4)?2:1);mc_song_set(&e,1,63,1);mc_song_length(&e,2);
        mc_song_mode(&e,1);mc_clock_source(&e,1);e.project.song_loop=(uint8_t)(scenario==3);
        unsigned source=(scenario==2||scenario==3)?63:4;
        unsigned target=scenario==4?63:scenario==1?4:scenario>=2?4:63;
        e.project.step[source][0][15]=(McStep){{60,0,0},100,25,0,0};
        e.project.step[target][0][0]=(McStep){{60,0,0},100,75,(uint8_t)flag,0};
        mc_start(&e);if(scenario==4) mc_song_set(&e,0,63,2);
        clocks((scenario==2||scenario==3)?186:90);assert(e.step==15&&e.pattern==source);
        unsigned found=0;uint32_t samples=MC_RATE*15u/e.project.bpm;
        for(unsigned v=0;v<MC_VOICES;v++) if(e.voice[v].active&&e.voice[v].part==0&&e.voice[v].note==60) {
            assert(e.voice[v].gate_left==(scenario==2?samples/4:samples+2));found=1;
        }
        assert(found);
    }
    printf("PASS song: repeats, bar boundaries, end, loop, continue, 128 entries, v6 roundtrip/v5 import; record=%u\n",MC_RECORD_BYTES);
}
