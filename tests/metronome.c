/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/mcfm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static McEngine a,b;
static McProject decoded;
static uint8_t record[MC_RECORD_BYTES];
static int16_t x[8192],y[8192];
static void empty(void) {
    mc_init(&a);assert(!a.project.metronome);memset(a.project.step,0,sizeof a.project.step);
    a.project.metronome=1;a.recording=1;a.project.bpm=120;a.project.swing=0;
}
static void silence(void) {
    mc_render(&a,x,4096);for(unsigned i=0;i<8192;i++)assert(x[i]==0);
}
static void gating(void) {
    empty();silence();a.recording=0;mc_start(&a);silence();mc_stop(&a);
    a.recording=1;a.project.metronome=0;mc_start(&a);silence();mc_stop(&a);
    a.project.metronome=1;mc_start(&a);mc_render(&a,x,64);assert(a.metro_left&&x[0]);
    a.recording=0;silence();assert(!a.metro_left);
    a.recording=1;mc_start(&a);mc_render(&a,x,64);a.project.metronome=0;silence();assert(!a.metro_left);
    a.project.metronome=1;mc_start(&a);mc_render(&a,x,64);mc_stop(&a);assert(!a.metro_left);silence();
    puts("Default off; requires PLAY+REC+toggle; REC off/toggle off/STOP cut clicks: PASS");
}
static void timing(void) {
    static const unsigned tempos[]={96,120,240};
    for(unsigned t=0;t<3;t++)for(unsigned swing=0;swing<=45;swing+=45) {
        empty();a.project.bpm=(uint16_t)tempos[t];a.project.swing=(uint8_t)swing;
        uint32_t before=mc_crc(&a.project,sizeof a.project);mc_start(&a);
        unsigned onsets[4]={0},count=0,peaks[4]={0},frames=MC_RATE*240u/tempos[t]-1;
        for(unsigned frame=0;frame<frames;frame++) {
            mc_render(&a,x,1);
            if(a.metro_left==MC_RATE/50-1) { assert(count<4);onsets[count++]=frame; }
            unsigned peak=(unsigned)abs(x[0]);if(count&&peak>peaks[count-1])peaks[count-1]=peak;
        }
        assert(count==4&&onsets[0]==0);
        for(unsigned i=1;i<4;i++)assert(onsets[i]==(MC_RATE*60u*i+tempos[t]-1)/tempos[t]-1);
        assert(peaks[0]>peaks[1]*6/5&&peaks[1]==peaks[2]&&peaks[2]==peaks[3]);
        assert(mc_crc(&a.project,sizeof a.project)==before&&!a.project.automation_count);
    }
    puts("Quarter-note onset positions at 96/120/240 BPM, swing 0/45, stronger downbeat, no note/automation recording: PASS");
}
static void isolation(void) {
    empty();b=a;b.mute=15;
    for(unsigned k=0;k<4;k++) { b.project.fx[k]=127;for(unsigned p=0;p<4;p++)b.project.track_fx[p][k]=127; }
    mc_start(&a);mc_start(&b);mc_render(&a,x,4096);
    for(unsigned offset=0;offset<4096;) { unsigned n=offset%127+1;if(n>4096-offset)n=4096-offset;mc_render(&b,y+offset*2,n);offset+=n; }
    assert(!memcmp(x,y,sizeof x)&&a.metro_left==b.metro_left&&a.metro_phase==b.metro_phase);
    empty();mc_start(&a);a.master=0;silence();
    puts("Independent of track mute and all FX, shared master control and varying-block identity: PASS");
}
static void persistence(void) {
    empty();assert(mc_encode(&a.project,55,record,sizeof record)==MC_RECORD_BYTES&&record[20+32907]==1);
    uint32_t seq=0;assert(!mc_decode(&decoded,&seq,record,sizeof record)&&decoded.metronome==1&&seq==55);
    uint32_t before=mc_crc(&decoded,sizeof decoded);record[20+32907]=2;
    uint32_t crc=mc_crc(record+20,MC_PROJECT_BYTES);for(unsigned i=0;i<4;i++)record[12+i]=(uint8_t)(crc>>(i*8));
    crc=mc_crc(record,16);for(unsigned i=0;i<4;i++)record[16+i]=(uint8_t)(crc>>(i*8));
    assert(mc_decode(&decoded,&seq,record,sizeof record)<0&&seq==55&&mc_crc(&decoded,sizeof decoded)==before);
    a.project.metronome=2;assert(!mc_project_valid(&a.project));
    a.project.metronome=0;assert(mc_encode(&a.project,56,record,sizeof record)==MC_RECORD_BYTES);
    assert(!mc_decode(&decoded,&seq,record,sizeof record)&&!decoded.metronome&&seq==56);
    puts("v5 metronome ON/OFF round trip and valid-CRC value 2 rejection; record size unchanged: PASS");
}
int main(void) { gating();timing();isolation();persistence();puts("metronome: PASS"); }
