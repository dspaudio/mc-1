/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/mcfm.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static McEngine a,b;
static int16_t x[8192],y[8192];
static void limiter(void) {
    mc_init(&a);assert(a.master==MC_MASTER_DEFAULT&&a.limiter_gain==(1u<<24)&&!a.limiter_hits);
    a.master=32768;
    for(int input=-30000;input<=30000;input+=75) {
        int32_t l=input,r=-input/2;mc_output(&a,&l,&r);assert(l==input&&r==-input/2);
    }
    assert(!a.limiter_hits);
    int32_t l=100000,r=-50000;mc_output(&a,&l,&r);
    assert(abs(l)<=MC_OUTPUT_CEILING&&abs(r)<=MC_OUTPUT_CEILING&&abs(l+2*r)<=1&&a.limiter_hits==1);
    uint32_t attacked=a.limiter_gain;
    for(unsigned i=0;i<2205;i++) { l=0;r=0;mc_output(&a,&l,&r);assert(!l&&!r); }
    assert(a.limiter_gain>attacked&&a.limiter_gain<(1u<<24));
    double remaining=(double)((1u<<24)-a.limiter_gain)/((1u<<24)-attacked);
    assert(remaining>0.36&&remaining<0.38);
    for(unsigned i=0;i<MC_RATE;i++) { l=0;r=0;mc_output(&a,&l,&r); }
    assert(a.limiter_gain==(1u<<24));
    a.master=UINT32_MAX;l=INT32_MIN;r=INT32_MAX;mc_output(&a,&l,&r);
    assert(abs(l)<=MC_OUTPUT_CEILING&&abs(r)<=MC_OUTPUT_CEILING&&!a.clips);
    mc_init(&a);mc_render(&a,x,4096);for(unsigned i=0;i<8192;i++)assert(x[i]==0);
    assert(a.limiter_gain==(1u<<24)&&!a.limiter_hits);
    puts("Output transparency, linked stereo ratio, 50 ms recovery, extreme inputs, reset and silence: PASS");
}
static void hot(void) {
    mc_init(&a);a.master=MC_MASTER_MAX;
    for(unsigned t=0;t<4;t++) {
        a.project.level[t]=127;a.project.pan[t]=64;
        for(unsigned k=0;k<4;k++)a.project.track_fx[t][k]=127;
        if(t<3) { mc_preset(&a,t,MC_PRESET_VECTOR_LEAD);a.project.patch[t].level=127;a.project.patch[t].cutoff=127;a.project.patch[t].sustain=127; }
    }
    for(unsigned k=0;k<4;k++)a.project.fx[k]=127;
    for(unsigned i=0;i<MC_VOICES;i++)mc_note(&a,i%3,36+i,127,MC_ACCENT,0);
    for(unsigned i=0;i<MC_DRUMS;i++)mc_drum(&a,i,127);
    b=a;mc_render(&a,x,4096);
    for(unsigned offset=0;offset<4096;) { unsigned n=(offset%127)+1;if(n>4096-offset)n=4096-offset;mc_render(&b,y+offset*2,n);offset+=n; }
    assert(!memcmp(x,y,sizeof x)&&a.limiter_gain==b.limiter_gain&&a.limiter_hits==b.limiter_hits);
    for(unsigned block=0;block<64;block++) {
        mc_render(&a,x,4096);for(unsigned i=0;i<8192;i++)assert(abs(x[i])<=MC_OUTPUT_CEILING);
    }
    assert(a.limiter_hits&&!a.clips&&a.peak<=MC_OUTPUT_CEILING);
    printf("Hot polyphony + all track/master FX, block identity: PASS; peak=%u limited_frames=%u clips=%u\n",a.peak,a.limiter_hits,a.clips);
}
static void demos(void) {
    for(unsigned demo=0;demo<2;demo++) {
        mc_demo_load(&a,demo);mc_demo_load(&b,demo);b.master=28000;mc_start(&a);mc_start(&b);
        uint64_t sum=0,reference=0;unsigned frames=MC_RATE*60;
        for(unsigned offset=0;offset<frames;) {
            unsigned n=frames-offset>4096?4096:frames-offset;mc_render(&a,x,n);mc_render(&b,y,n);
            for(unsigned i=0;i<n*2;i++) { sum+=(uint64_t)((int32_t)x[i]*x[i]);reference+=(uint64_t)((int32_t)y[i]*y[i]);assert(abs(x[i])<=MC_OUTPUT_CEILING); }
            offset+=n;
        }
        double rms=10*log10((double)sum/(frames*2.0*32768.0*32768.0));
        double gain=10*log10((double)sum/(double)reference);
        assert(gain>14.0&&gain<15.7&&!a.clips&&a.peak<=MC_OUTPUT_CEILING);
        if(demo==1)assert(rms>-22.0&&rms<-17.0);
        printf("Demo %u default: peak=%u rms=%.3f dBFS gain=%.3f dB limited_frames=%u clips=%u\n",demo,a.peak,rms,gain,a.limiter_hits,a.clips);
    }
    puts("Both demos default loudness versus explicit legacy master=28000: PASS");
}
int main(void) { limiter();hot();demos();puts("output: PASS"); }
