/* SPDX-License-Identifier: GPL-3.0-only */
#include "mcfm.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static McEffects effects[5];
static const uint8_t zero[4]={0};
static uint32_t hash_sample(uint32_t hash,int32_t left,int32_t right) {
    hash^=(uint32_t)left;hash*=16777619u;hash^=(uint32_t)right;return hash*16777619u;
}
static void legacy(void) {
    static const unsigned bpm[4]={40,118,140,240};
    static const uint32_t expected[4]={1324590517u,3549991746u,3869937633u,2595260296u};
    for(unsigned b=0;b<4;b++) {
        memset(effects,0,sizeof effects);uint32_t hash=2166136261u;
        for(unsigned n=0;n<MC_RATE*2;n++) {
            uint8_t amount[4]={(uint8_t)((n/11025)%2?127:48),35,20,30};
            int32_t l=n<MC_RATE?(int32_t)(n*997%24001)-12000:0;
            int32_t r=n<MC_RATE?(int32_t)(n*487%24001)-12000:0;
            mc_effects_process(&effects[0],bpm[b],amount,&l,&r);
            hash=hash_sample(hash,l,r);
        }
        printf("legacy bpm=%u hash=%u\n",bpm[b],hash);assert(hash==expected[b]);
    }
}
static void impulse(void) {
    for(unsigned family=0;family<4;family++) {
        uint32_t hashes[4]={0};unsigned count=family==3?3:4;
        for(unsigned mode=0;mode<count;mode++) {
            memset(effects,0,sizeof effects);
            uint8_t amount[4]={0},type[4]={0};amount[family]=127;type[family]=(uint8_t)mode;
            uint32_t hash=2166136261u;unsigned first=0;int32_t first_l=0,first_r=0;
            for(unsigned n=0;n<MC_RATE;n++) {
                int32_t l=n==0?12000:0,r=0;
                mc_effects_process_typed(&effects[0],120,amount,type,zero,&l,&r);
                if(n&&!first&&(l||r)){first=n;first_l=l;first_r=r;}
                hash=hash_sample(hash,l,r);
            }
            hashes[mode]=hash;
            printf("impulse family=%u mode=%u hash=%u first=%u stereo=%d,%d\n",family,mode,hash,first,first_l,first_r);
            if(family==0) {
                assert(first==(mode==3?MC_RATE*15/120/2:MC_RATE*15/120));
                if(mode==2)assert(first_l==0&&first_r>0);
                else assert(first_l>0);
            }
            if(family==1)assert(first>0);
            for(unsigned previous=0;previous<mode;previous++)assert(hash!=hashes[previous]);
        }
    }
}
static double sine_response(unsigned hz,const uint8_t amount[4],const uint8_t type[4],const uint8_t filter[2],int fixed_phase) {
    memset(effects,0,sizeof effects);double input=0,output=0;
    for(unsigned n=0;n<MC_RATE;n++) {
        int32_t l=(int32_t)(6000*sin(6.283185307179586*(double)hz*n/MC_RATE)),r=l;
        int32_t dry=l;
        if(fixed_phase)effects[0].phase=0;
        mc_effects_process_typed(&effects[0],120,amount,type,filter,&l,&r);
        if(n>MC_RATE/2){input+=(double)dry*dry;output+=(double)l*l;}
    }
    return sqrt(output/input);
}
static void filters(void) {
    uint8_t hp[2]={100,0},lp[2]={0,100};
    double hp_low=sine_response(80,zero,zero,hp,0),hp_high=sine_response(8000,zero,zero,hp,0);
    double lp_low=sine_response(80,zero,zero,lp,0),lp_high=sine_response(8000,zero,zero,lp,0);
    printf("filters HPF 80Hz=%.6f 8kHz=%.6f LPF 80Hz=%.6f 8kHz=%.6f\n",hp_low,hp_high,lp_low,lp_high);
    assert(hp_low<hp_high/10&&hp_high>0.5);
    assert(lp_high<lp_low/8&&lp_low>0.9);
    assert(sine_response(8000,zero,zero,zero,0)==1.0);
}
static void phaser(void) {
    uint8_t amount[4]={0,0,0,127},type[4]={0,0,0,1};
    double minimum=10,maximum=0;unsigned notch=0;
    for(unsigned hz=200;hz<=18000;hz+=200) {
        double response=sine_response(hz,amount,type,zero,1);
        if(response<minimum){minimum=response;notch=hz;}
        if(response>maximum)maximum=response;
    }
    printf("phaser fixed-LFO notch=%uHz gain=%.6f passband=%.6f\n",notch,minimum,maximum);
    assert(minimum<0.08&&maximum>0.9&&maximum<1.02);
}
static void legacy_modulation(McEffects *f,unsigned type,unsigned amount,
    int32_t *left,int32_t *right) {
    int32_t in[2]={*left,*right},wet[2];
    f->phase+=29217u;
    unsigned tri=f->phase>>20;if(tri>2047)tri=4095-tri;
    unsigned taps[2]={4800+tri*2,4800+(2047-tri)*2};
    if(type==2) {taps[0]=32+tri*4;taps[1]=32+(2047-tri)*4;}
    for(unsigned c=0;c<2;c++) {
        unsigned pos=(f->chorus_pos-(taps[c]>>4))&1023u,next=(pos-1)&1023u,frac=taps[c]&15;
        wet[c]=(f->chorus[c][pos]*(int32_t)(16-frac)+f->chorus[c][next]*(int32_t)frac)/16;
        f->chorus[c][f->chorus_pos]=(int16_t)(in[c]+(type==2&&amount?wet[c]/2:0));
    }
    f->chorus_pos=(uint16_t)((f->chorus_pos+1)&1023u);
    *left+= (wet[0]-*left)*(int32_t)amount/254;
    *right+=(wet[1]-*right)*(int32_t)amount/254;
}
static void zero_to_wet(void) {
    for(unsigned type=0;type<=2;type+=2) {
        memset(&effects[0],0,sizeof effects[0]);memset(&effects[1],0,sizeof effects[1]);
        uint8_t amount[4]={0},mode[4]={0};mode[3]=(uint8_t)type;
        for(unsigned n=0;n<4096;n++) {
            int32_t l=(int32_t)(n*43u%16001u)-8000,r=(int32_t)(n*97u%14001u)-7000;
            int32_t reference_l=l,reference_r=r;
            legacy_modulation(&effects[1],type,0,&reference_l,&reference_r);
            mc_effects_process_typed(&effects[0],120,amount,mode,zero,&l,&r);
            assert(l==reference_l&&r==reference_r);
        }
        amount[3]=91;
        int32_t l=7311,r=-4927,reference_l=l,reference_r=r;
        legacy_modulation(&effects[1],type,91,&reference_l,&reference_r);
        mc_effects_process_typed(&effects[0],120,amount,mode,zero,&l,&r);
        assert(l==reference_l&&r==reference_r);
        assert(effects[0].phase==effects[1].phase);
        assert(effects[0].chorus_pos==effects[1].chorus_pos);
        assert(!memcmp(effects[0].chorus,effects[1].chorus,sizeof effects[0].chorus));
    }
    puts("zero-to-wet chorus/flanger arithmetic and history: PASS");
}
static void transitions(void) {
    for(unsigned family=0;family<4;family++) {
        uint8_t amount[4]={0},type[4]={0};amount[family]=127;
        memset(effects,0,sizeof effects);
        for(unsigned n=0;n<10000;n++) {
            int32_t l=5000,r=-3000;
            mc_effects_process_typed(&effects[0],120,amount,type,zero,&l,&r);
        }
        type[family]=1;
        for(unsigned n=0;n<10000;n++) {
            int32_t l=0,r=0,reference_l=0,reference_r=0;
            mc_effects_process_typed(&effects[0],120,amount,type,zero,&l,&r);
            mc_effects_process_typed(&effects[1],120,amount,type,zero,&reference_l,&reference_r);
            assert(l==reference_l&&r==reference_r);
        }
    }
    printf("mode changes clear selected family tails: PASS\n");
}
static void stress(void) {
    uint8_t amount[4]={127,127,127,127};uint32_t random=0x12345678;int32_t peak=0;
    for(unsigned combination=0;combination<192;combination++) {
        uint8_t type[4]={(uint8_t)(combination%4),(uint8_t)(combination/4%4),
            (uint8_t)(combination/16%4),(uint8_t)(combination/64)};
        memset(effects,0,sizeof effects);
        for(unsigned n=0;n<MC_RATE;n++) {
            random=random*1664525u+1013904223u;
            int32_t l=n<MC_RATE/2?(int32_t)(random%200001)-100000:0,r=n&1?-l:l;
            amount[2]=(uint8_t)((n/4096)%2?127:0);
            uint8_t filter[2]={(uint8_t)(n<MC_RATE/3?0:127),(uint8_t)(n<MC_RATE*2/3?0:127)};
            for(unsigned chain=0;chain<5;chain++) {
                mc_effects_process_typed(&effects[chain],40+combination,amount,type,filter,&l,&r);
                assert(l>-1000000&&l<1000000&&r>-1000000&&r<1000000);
                int32_t magnitude_l=l<0?-l:l,magnitude_r=r<0?-r:r;
                if(magnitude_l>peak)peak=magnitude_l;
                if(magnitude_r>peak)peak=magnitude_r;
            }
        }
    }
    printf("stress 192 mode combinations x 44100 samples x 5 cascades peak=%d state=%zu bytes\n",peak,sizeof(McEffects));
}
int main(void) {
    legacy();impulse();filters();phaser();zero_to_wet();transitions();stress();
    puts("effects modes PASS");return 0;
}
