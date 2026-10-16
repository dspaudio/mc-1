/* SPDX-License-Identifier: GPL-3.0-only */
#include "mcfm.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "fixed.h"

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
        if(fixed_phase)effects[0].track.phase=0;
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
    f->track.phase+=29217u;
    unsigned tri=f->track.phase>>20;if(tri>2047)tri=4095-tri;
    unsigned taps[2]={4800+tri*2,4800+(2047-tri)*2};
    if(type==2) {taps[0]=32+tri*4;taps[1]=32+(2047-tri)*4;}
    for(unsigned c=0;c<2;c++) {
        unsigned pos=(f->track.chorus_pos-(taps[c]>>4))&1023u,next=(pos-1)&1023u,frac=taps[c]&15;
        wet[c]=(f->track.chorus[c][pos]*(int32_t)(16-frac)+f->track.chorus[c][next]*(int32_t)frac)/16;
        f->track.chorus[c][f->track.chorus_pos]=(int16_t)(in[c]+(type==2&&amount?wet[c]/2:0));
    }
    f->track.chorus_pos=(uint16_t)((f->track.chorus_pos+1)&1023u);
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
        assert(effects[0].track.phase==effects[1].track.phase);
        assert(effects[0].track.chorus_pos==effects[1].track.chorus_pos);
        assert(!memcmp(effects[0].track.chorus,effects[1].track.chorus,sizeof effects[0].track.chorus));
    }
    puts("zero-to-wet chorus/flanger arithmetic and history: PASS");
}
/* 구 식(부호 있는 / 연산, 무조건 계산)을 그대로 옮긴 독립 기준 구현. */
static int32_t ref_limit(int32_t x) {return x<-16000?-16000:x>16000?16000:x;}
static int32_t ref_bound(int32_t x,int32_t m) {return x<-m?-m:x>m?m:x;}
static int32_t ref_drive(int32_t x,unsigned amount) {
    if(!amount)return x;
    int32_t v=ref_limit(x)*(1+(int32_t)amount/8),a=v<0?-v:v,wet=v*8192/(8192+a);
    return (x*(127-(int32_t)amount)+wet*(int32_t)amount)/127;
}
static int32_t ref_allpass(int32_t in,int32_t c,int32_t *s) {
    int32_t out=ref_bound(*s-in*c/32768,48000);
    *s=ref_bound(in+out*c/32768,48000);return out;
}
static void ref_delay_reverb(McEffects *f,unsigned bpm,unsigned delay,unsigned reverb,
    unsigned t0,unsigned t1,int32_t *l,int32_t *r) {
    int32_t left=*l,right=*r;
    unsigned length=MC_RATE*15u/(bpm?bpm:1);if(length>8191)length=8191;
    if(!length)length=1;
    unsigned pos=(f->delay.delay_pos+8192-length)&8191u;
    int32_t dl=f->delay.delay[0][pos],dr=f->delay.delay[1][pos],wl_,wr_;
    if(t0==1) {
        f->delay.tape[0]+=(dl-f->delay.tape[0])/4;f->delay.tape[1]+=(dr-f->delay.tape[1])/4;
        dl=f->delay.tape[0];dr=f->delay.tape[1];
        wl_=left+ref_drive(dl,32)*3/5;wr_=right+ref_drive(dr,32)*3/5;
    } else if(t0==2) {
        wl_=dr/2;wr_=(left+right)/2+dl/2;
    } else if(t0==3) {
        unsigned half=(f->delay.delay_pos+8192-length/2)&8191u;
        unsigned third=(f->delay.delay_pos+8192-length*3/4)&8191u;
        dl=(dl*3+f->delay.delay[1][third]*2+f->delay.delay[0][half])/6;
        dr=(dr*3+f->delay.delay[0][third]*2+f->delay.delay[1][half])/6;
        wl_=left+dl/4;wr_=right+dr/4;
    } else {wl_=left+dr*7/16;wr_=right+dl*7/16;}
    f->delay.delay[0][f->delay.delay_pos]=(int16_t)ref_limit(delay?wl_:0);
    f->delay.delay[1][f->delay.delay_pos]=(int16_t)ref_limit(delay?wr_:0);
    f->delay.delay_pos=(uint16_t)((f->delay.delay_pos+1)&8191u);
    left+=dl*(int32_t)delay/127;right+=dr*(int32_t)delay/127;
    static const unsigned lengths[4][4]={
        {1557,1617,1491,1422},{2039,1999,1867,1747},
        {997,1153,1301,1451},{331,457,673,911}};
    unsigned mode=t1<4?t1:0;
    int32_t input=reverb?(left+right)/4:0,wl=0,wr=0,previous=0;
    for(unsigned i=0;i<4;i++) {
        int32_t tap=f->reverb.comb[i][f->reverb.comb_pos[i]];
        f->reverb.damp[i]+=(tap-f->reverb.damp[i])/(mode==0?4:mode==1?8:2);
        int32_t feedback=f->reverb.damp[i]*(mode==1?13:mode==2?12:11)/16;
        if(mode==3&&(i&1))feedback=-feedback;
        f->reverb.comb[i][f->reverb.comb_pos[i]]=(int16_t)ref_limit(input+feedback+(mode==3?previous/4:0));
        previous=tap;
        if(++f->reverb.comb_pos[i]>=lengths[mode][i])f->reverb.comb_pos[i]=0;
        if(i&1)wr+=tap;else wl+=tap;
    }
    if(mode==1||mode==2) {
        wl=ref_allpass(wl,mode==1?16384:24576,&f->reverb.ap[0]);
        wr=ref_allpass(wr,mode==1?16384:24576,&f->reverb.ap[1]);
    } else if(mode==3) {int32_t old=wl;wl-=wr/2;wr-=old/2;}
    left+=wl*(int32_t)reverb/254;right+=wr*(int32_t)reverb/254;
    *l=left;*r=right;
}
static void seed_history(McEffects *f) {
    uint32_t s=0xC0FFEEu;
    for(unsigned c=0;c<2;c++)for(unsigned i=0;i<8192;i++) {
        s=s*1664525u+1013904223u;f->delay.delay[c][i]=(int16_t)((int32_t)(s>>16)%24001-12000);
    }
    for(unsigned c=0;c<4;c++)for(unsigned i=0;i<2048;i++) {
        s=s*1664525u+1013904223u;f->reverb.comb[c][i]=(int16_t)((int32_t)(s>>16)%24001-12000);
    }
    for(unsigned i=0;i<4;i++){s=s*1664525u+1013904223u;f->reverb.damp[i]=(int32_t)(s>>16)%9001-4500;}
    for(unsigned i=0;i<2;i++) {
        s=s*1664525u+1013904223u;f->delay.tape[i]=(int32_t)(s>>16)%9001-4500;
        f->reverb.ap[i]=(int32_t)(s>>8)%7001-3500;
    }
}
static int same_history(const McEffects *a,const McEffects *b) {
    return !memcmp(a->delay.delay,b->delay.delay,sizeof a->delay.delay)&&!memcmp(a->delay.tape,b->delay.tape,sizeof a->delay.tape)
        &&a->delay.delay_pos==b->delay.delay_pos&&!memcmp(a->reverb.comb,b->reverb.comb,sizeof a->reverb.comb)
        &&!memcmp(a->reverb.damp,b->reverb.damp,sizeof a->reverb.damp)&&!memcmp(a->reverb.comb_pos,b->reverb.comb_pos,sizeof a->reverb.comb_pos)
        &&!memcmp(a->reverb.ap,b->reverb.ap,sizeof a->reverb.ap);
}
static void zero_amount_history(void) {
    static const unsigned bpm[2]={40,140};
    static const uint8_t level[8]={0,0,64,127,0,0,31,0};
    for(unsigned b=0;b<2;b++)for(unsigned t0=0;t0<4;t0++)for(unsigned t1=0;t1<4;t1++) {
        memset(&effects[0],0,sizeof effects[0]);memset(&effects[1],0,sizeof effects[1]);
        seed_history(&effects[0]);seed_history(&effects[1]);
        uint8_t type[4]={(uint8_t)t0,(uint8_t)t1,0,0};
        effects[0].delay.type=(uint8_t)t0;effects[0].reverb.type=(uint8_t)t1;
        for(unsigned n=0;n<16000;n++) {
            unsigned block=n/1000;
            uint8_t amount[4]={level[block%8],level[(block+3)%8],0,0};
            int32_t l=n%3000<1500?(int32_t)(n*37u%20001)-10000:0,r=n%3000<1500?(int32_t)(n*71u%18001)-9000:0;
            int32_t reference_l=l,reference_r=r;
            mc_effects_process_typed(&effects[0],bpm[b],amount,type,zero,&l,&r);
            ref_delay_reverb(&effects[1],bpm[b],amount[0],amount[1],t0,t1,&reference_l,&reference_r);
            assert(l==reference_l&&r==reference_r);
            if(n%1000==999||n%1000==0)assert(same_history(&effects[0],&effects[1]));
        }
        assert(same_history(&effects[0],&effects[1]));
    }
    puts("zero-amount delay/tape/reverb outputs and history: PASS");
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
static void filter_history(void) {
    static const uint8_t values[8]={0,12,37,127,1,0,100,64};
    memset(effects,0,sizeof effects);
    seed_history(&effects[0]);seed_history(&effects[1]);
    for(unsigned n=0;n<32768;n++) {
        uint8_t amount[4]={83,67,50,109};
        uint8_t type[4]={(uint8_t)(n/4096%4),(uint8_t)(n/8192%4),0,(uint8_t)(n/16384%3)};
        uint8_t filter[2]={values[n/511%8],values[(n/733+3)%8]};
        int32_t l=n<24000?(int32_t)(n*997u%30001)-15000:0;
        int32_t r=n<24000?(int32_t)(n*487u%30001)-15000:0;
        int32_t reference_l=l,reference_r=r;
        mc_effects_process_typed(&effects[0],118,amount,type,filter,&l,&r);
        uint8_t previous[2]={effects[1].track.filter_amount[0],effects[1].track.filter_amount[1]};
        effects[1].track.filter_amount[0]=effects[1].track.filter_amount[1]=0;
        mc_effects_process_typed(&effects[1],118,amount,type,zero,&reference_l,&reference_r);
        effects[1].track.filter_amount[0]=previous[0];effects[1].track.filter_amount[1]=previous[1];
        for(unsigned i=0;i<2;i++) {
            int32_t *state=i?effects[1].track.lowpass:effects[1].track.highpass;
            if(previous[i]!=filter[i]) {
                if(!previous[i]||!filter[i])memset(state,0,2*sizeof *state);
                unsigned value=filter[i],cutoff=i?40+(127-value)*(127-value):30+value*value/3;
                effects[1].track.filter_coef[i]=(uint16_t)(cutoff*32768u/(cutoff+7019u));
                effects[1].track.filter_amount[i]=filter[i];
            }
            if(filter[i]) {
                state[0]+=(int32_t)(((int64_t)reference_l-state[0])*effects[1].track.filter_coef[i]/32768);
                state[1]+=(int32_t)(((int64_t)reference_r-state[1])*effects[1].track.filter_coef[i]/32768);
                if(i){reference_l=state[0];reference_r=state[1];}
                else {reference_l-=state[0];reference_r-=state[1];}
            }
        }
        assert(l==reference_l&&r==reference_r);
        assert(!memcmp(&effects[0],&effects[1],sizeof effects[0]));
    }
    puts("filter outputs and complete FX history across coefficients, toggles, modes and tails: PASS");
}
static void prepared_tempo(void) {
    static const unsigned tempos[]={0,1,40,80,81,118,140,240,MC_RATE*15u,UINT32_MAX};
    memset(effects,0,sizeof effects);seed_history(&effects[0]);effects[1]=effects[0];
    for(unsigned n=0;n<32768;n++) {
        unsigned bpm=tempos[n/37%(sizeof tempos/sizeof *tempos)];
        uint64_t distance=(uint64_t)MC_RATE*15u/(bpm?bpm:1);
        if(distance>8191)distance=8191;
        if(!distance)distance=1;
        uint8_t amount[4]={(uint8_t)(n/127%2?83:0),67,50,109};
        uint8_t type[4]={(uint8_t)(n/257%4),(uint8_t)(n/511%4),0,(uint8_t)(n/733%3)};
        uint8_t filter[2]={12,37};
        int32_t l=(int32_t)(n*997u%30001)-15000,r=(int32_t)(n*487u%30001)-15000;
        int32_t prepared_l=l,prepared_r=r;
        mc_effects_process_typed(&effects[0],bpm,amount,type,filter,&l,&r);
        mc_effects_process_typed_length(&effects[1],(unsigned)distance,amount,type,filter,&prepared_l,&prepared_r);
        assert(l==prepared_l&&r==prepared_r&&!memcmp(&effects[0],&effects[1],sizeof effects[0]));
    }
    puts("Prepared delay length: tempo edges, per-call changes, modes and complete history PASS");
}
static void blocks(void) {
    int32_t block[65][2],single[65][2];
    memset(effects,0,sizeof effects);seed_history(&effects[0]);effects[1]=effects[0];
    for(unsigned batch=0;batch<1024;batch++) {
        unsigned frames=batch%65+1,length=batch%3?5605:8191;
        uint8_t amount[4]={(uint8_t)(batch/7%2?83:0),(uint8_t)(batch/11%2?67:0),
            (uint8_t)(batch/13%2?50:0),(uint8_t)(batch/17%2?109:0)};
        uint8_t type[4]={(uint8_t)(batch/19%4),(uint8_t)(batch/23%4),
            (uint8_t)(batch/29%4),(uint8_t)(batch/31%3)};
        uint8_t filter[2]={(uint8_t)(batch/37%2?127:0),(uint8_t)(batch/41%2?37:0)};
        for(unsigned n=0;n<frames;n++) {
            unsigned at=batch*65+n;
            block[n][0]=(int32_t)(at*997u%30001)-15000;
            block[n][1]=(int32_t)(at*487u%30001)-15000;
        }
        memcpy(single,block,frames*sizeof *block);
        mc_effects_process_block_length(&effects[0],length,amount,type,filter,block,frames);
        for(unsigned n=0;n<frames;n++)
            mc_effects_process_typed_length(&effects[1],length,amount,type,filter,&single[n][0],&single[n][1]);
        assert(!memcmp(block,single,frames*sizeof *block));
        assert(!memcmp(&effects[0],&effects[1],sizeof effects[0]));
        mc_effects_process_block_length(&effects[0],length,zero,zero,zero,block,0);
        assert(!memcmp(&effects[0],&effects[1],sizeof effects[0]));
    }
    puts("Block FX: PCM and complete state across sizes, modes, dry tails and filter changes PASS");
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
static McSharedFx shared[3];
static void shared_block(McSharedFx *f,unsigned bank,unsigned bpm,unsigned type,int32_t stereo[][2],unsigned frames) {
    if(bank)mc_fx_cf_block(f,type,stereo,frames);
    else mc_fx_rd_block(f,bpm,type,stereo,frames);
}
/* Hard output bound: delay, C/F keep 24000; reverb returns are no longer halved (allpass 48000, ROOM1 two 16000 taps). */
static int32_t shared_bound(unsigned bank,unsigned type) {
    if(bank||type>=6)return 24000;
    return type?48000:32000;
}
/* Independent reference of the shared reverb; old = pre-1.0.1 feedback and halved return. */
typedef struct { int16_t comb[4][2048]; int32_t damp[4],ap[2]; unsigned pos[4]; } RefReverb;
static const struct { uint16_t length[4],diffusion; uint8_t damping,old_fb,new_fb; } ref_profile[6]={
    {{331,457,673,911},0,2,9,9},{{751,809,947,1031},8192,2,10,10},{{997,1153,1301,1451},12288,3,11,11},
    {{1557,1617,1491,1422},16384,2,12,13},{{2039,1999,1867,1747},20480,3,13,14},{{887,1061,1217,1367},24576,1,12,13}};
static int32_t ref_clip(int32_t x,int32_t limit) { return x<-limit?-limit:x>limit?limit:x; }
static int32_t rv_allpass(int32_t in,int32_t c,int32_t *s) {
    int32_t out=ref_clip(*s-mc_div_pow2(in*c,15),48000);
    *s=ref_clip(in+mc_div_pow2(out*c,15),48000);return out;
}
static void ref_reverb(RefReverb *s,unsigned type,int is_new,int32_t l,int32_t r,int32_t out[2]) {
    int32_t feedback=is_new?ref_profile[type].new_fb:ref_profile[type].old_fb,wet[2]={0,0};
    int32_t round=(1<<ref_profile[type].damping)-1,input=mc_div_pow2(ref_clip(l,16000)+ref_clip(r,16000),2);
    for(unsigned i=0;i<4;i++) {
        int32_t tap=s->comb[i][s->pos[i]],delta=tap-s->damp[i];
        s->damp[i]+=delta<0?-((-delta+round)>>ref_profile[type].damping):(delta+round)>>ref_profile[type].damping;
        s->comb[i][s->pos[i]]=(int16_t)ref_clip(input+mc_div_pow2(s->damp[i]*feedback,4),16000);
        if(++s->pos[i]>=ref_profile[type].length[i])s->pos[i]=0;
        wet[i&1]+=tap;
    }
    if(ref_profile[type].diffusion)for(unsigned c=0;c<2;c++)wet[c]=rv_allpass(wet[c],ref_profile[type].diffusion,&s->ap[c]);
    out[0]=is_new?wet[0]:mc_div_pow2(wet[0],1);out[1]=is_new?wet[1]:mc_div_pow2(wet[1],1);
}
static RefReverb ref_state[2];
static int32_t reverb_actual[MC_RATE*4][2],reverb_old[MC_RATE*4][2],reverb_new[MC_RATE*4][2];
static void reverb_metrics(const int32_t out[][2],double *energy,int32_t *peak,unsigned *last) {
    *energy=0;*peak=0;*last=0;
    for(unsigned n=0;n<MC_RATE*4;n++)for(unsigned c=0;c<2;c++) {
        int32_t v=out[n][c],a=v<0?-v:v;
        *energy+=(double)v*v;if(a>*peak)*peak=a;if(a)*last=n;
    }
}
static void shared_reverb_strength(void) {
    for(unsigned type=0;type<6;type++) {
        memset(&shared[0],0,sizeof shared[0]);memset(ref_state,0,sizeof ref_state);
        for(unsigned at=0;at<MC_RATE*4;) {
            int32_t block[128][2]={0};
            unsigned frames=MC_RATE*4-at;if(frames>128)frames=128;
            if(!at){block[0][0]=12000;block[0][1]=6000;}
            mc_fx_rd_block(&shared[0],120,type,block,frames);
            for(unsigned n=0;n<frames;n++) {
                int32_t in_l=at+n?0:12000,in_r=at+n?0:6000;
                reverb_actual[at+n][0]=block[n][0];reverb_actual[at+n][1]=block[n][1];
                ref_reverb(&ref_state[0],type,0,in_l,in_r,reverb_old[at+n]);
                ref_reverb(&ref_state[1],type,1,in_l,in_r,reverb_new[at+n]);
            }
            at+=frames;
        }
        double e_act,e_old,e_new;int32_t p_act,p_old,p_new;unsigned l_act,l_old,l_new;
        reverb_metrics(reverb_actual,&e_act,&p_act,&l_act);
        reverb_metrics(reverb_old,&e_old,&p_old,&l_old);
        reverb_metrics(reverb_new,&e_new,&p_new,&l_new);
        printf("reverb metric type=%u energy=%.0f peak=%d last_ms=%.3f ref_old_energy=%.0f ref_old_peak=%d ref_old_last_ms=%.3f ratio=%.4f\n",
            type,e_act,p_act,l_act*1000.0/MC_RATE,e_old,p_old,l_old*1000.0/MC_RATE,e_act/e_old);
        assert(!memcmp(reverb_actual,reverb_new,sizeof reverb_actual));
        assert(p_act<=shared_bound(0,type)&&l_act>=l_old);
        /* Return gain is strengthened: energy ~4x for unchanged feedback (+6.02 dB), more with longer tails. */
        if(type<3) {
            assert(l_act>=l_old&&e_act>3.9*e_old&&e_act<4.1*e_old&&p_act>=2*p_old-2);
            for(unsigned n=0;n<MC_RATE*4;n++)for(unsigned c=0;c<2;c++) {
                int32_t d=reverb_new[n][c]-2*reverb_old[n][c];assert(d>=-1&&d<=1);
            }
        } else assert(l_act>l_old&&e_act>4.0*e_old);
        assert(!l_act||l_act<MC_RATE*3);
    }
    /* Hot sustained input reaches the strengthened bound that the old 24000 contract could not. */
    memset(&shared[0],0,sizeof shared[0]);
    int32_t peak=0;
    for(unsigned at=0;at<MC_RATE/2;at+=64) {
        int32_t block[64][2];
        for(unsigned n=0;n<64;n++){block[n][0]=16000;block[n][1]=16000;}
        mc_fx_rd_block(&shared[0],120,0,block,64);
        for(unsigned n=0;n<64;n++)for(unsigned c=0;c<2;c++)if(block[n][c]>peak)peak=block[n][c];
    }
    assert(peak>24000&&peak<=32000);
    puts("Shared reverb: independent reference, +6.02 dB return, longer HALL1/HALL2/PLATE tails, bounds PASS");
}
static void shared_impulses(void) {
    for(unsigned bank=0;bank<2;bank++) {
        uint32_t hashes[8]={0};
        for(unsigned type=0;type<8;type++) {
            memset(&shared[0],0,sizeof shared[0]);
            uint32_t hash=2166136261u;
            uint64_t early=0,late=0;
            unsigned first=0;
            int32_t peak=0;
            for(unsigned at=0;at<MC_RATE*4;) {
                int32_t block[128][2]={0};
                unsigned frames=MC_RATE*4-at;if(frames>128)frames=128;
                if(!at){block[0][0]=12000;block[0][1]=6000;}
                shared_block(&shared[0],bank,120,type,block,frames);
                if(!at)assert(block[0][0]==0&&block[0][1]==0);
                for(unsigned n=0;n<frames;n++) {
                    unsigned frame=at+n;
                    int32_t l=block[n][0],r=block[n][1];
                    int32_t bound=shared_bound(bank,type);
                    assert(l>=-bound&&l<=bound&&r>=-bound&&r<=bound);
                    if(!first&&(l||r))first=frame;
                    int32_t al=l<0?-l:l,ar=r<0?-r:r;
                    if(al>peak)peak=al;
                    if(ar>peak)peak=ar;
                    uint64_t energy=(uint64_t)((int64_t)l*l)+(uint64_t)((int64_t)r*r);
                    if(frame<MC_RATE)early+=energy;
                    if(frame>=MC_RATE*3)late+=energy;
                    if(!bank&&type>=6) {
                        if(frame<MC_RATE*15/120)assert(!l&&!r);
                        if(frame==MC_RATE*15/120) {
                            if(type==6)assert(l==12000&&r==6000);
                            else assert(l==9000&&r==0);
                        }
                        if(type==7&&frame==2*(MC_RATE*15/120))assert(l==0&&r==4500);
                    }
                    hash=hash_sample(hash,l,r);
                }
                at+=frames;
            }
            assert(first>0&&early>0&&late==0);
            if(!bank&&type>=6)assert(first==MC_RATE*15/120);
            if(bank&&type>=6)assert(first==700);
            assert(bank?shared[0].cf_active:shared[0].rd_active);
            assert(bank?shared[0].cf_type==type:shared[0].rd_type==type);
            assert(bank?mc_fx_cf_names[type][0]:mc_fx_rd_names[type][0]);
            for(unsigned previous=0;previous<type;previous++)assert(hash!=hashes[previous]);
            hashes[type]=hash;
            printf("shared bank=%u type=%u impulse=%u first=%u peak=%d decayed=0\n",bank,type,hash,first,peak);
        }
    }
}
static void shared_resets(void) {
    int32_t block[257][2],reference[257][2];
    for(unsigned bank=0;bank<2;bank++)for(unsigned type=0;type<8;type++) {
        memset(shared,0,sizeof shared);
        if(bank)memset(&shared[0].cf,0x5a,sizeof shared[0].cf);
        else memset(&shared[0].rd,0x5a,sizeof shared[0].rd);
        shared[0].rd_type=shared[1].rd_type=(uint8_t)type;
        shared[0].cf_type=shared[1].cf_type=(uint8_t)type;
        memset(block,0,sizeof block);memset(reference,0,sizeof reference);
        shared_block(&shared[0],bank,120,type,block,257);
        shared_block(&shared[1],bank,120,type,reference,257);
        assert(!memcmp(block,reference,sizeof block)&&!memcmp(&shared[0],&shared[1],sizeof shared[0]));
        for(unsigned next=0;next<8;next++)if(next!=type) {
            memset(&shared[0],0,sizeof shared[0]);
            for(unsigned n=0;n<257;n++){block[n][0]=12000;block[n][1]=-3000;}
            shared_block(&shared[0],!bank,120,4,block,257);
            for(unsigned n=0;n<257;n++){block[n][0]=12000;block[n][1]=-3000;}
            shared_block(&shared[0],bank,120,type,block,257);
            shared[1]=shared[2]=shared[0];
            if(bank)shared[1].cf_active=0;else shared[1].rd_active=0;
            for(unsigned n=0;n<257;n++) {
                block[n][0]=(int32_t)(n*97%9001)-4500;
                block[n][1]=(int32_t)(n*43%7001)-3500;
            }
            memcpy(reference,block,sizeof block);
            shared_block(&shared[0],bank,120,next,block,257);
            shared_block(&shared[1],bank,120,next,reference,257);
            assert(!memcmp(block,reference,sizeof block)&&!memcmp(&shared[0],&shared[1],sizeof shared[0]));
            if(bank) {
                assert(!memcmp(&shared[0].rd,&shared[2].rd,sizeof shared[0].rd));
                assert(shared[0].rd_type==shared[2].rd_type&&shared[0].rd_active==shared[2].rd_active);
            } else {
                assert(!memcmp(&shared[0].cf,&shared[2].cf,sizeof shared[0].cf));
                assert(shared[0].cf_type==shared[2].cf_type&&shared[0].cf_active==shared[2].cf_active);
            }
        }
    }
    puts("Shared FX: first use and all type transitions reset only the selected bank PASS");
}
static void shared_blocks(void) {
    static const unsigned sizes[]={1,2,7,65,128,257};
    static const unsigned tempos[]={0,1,40,118,240,MC_RATE*15u,UINT32_MAX};
    int32_t block[257][2],single[257][2];
    for(unsigned bank=0;bank<2;bank++) {
        memset(shared,0,sizeof shared);
        for(unsigned batch=0;batch<1024;batch++) {
            unsigned frames=sizes[batch%6],type=batch/19%8,bpm=tempos[batch/13%7];
            for(unsigned n=0;n<frames;n++) {
                unsigned at=batch*257+n;
                block[n][0]=batch%7?(int32_t)(at*997u%200001)-100000:0;
                block[n][1]=batch%7?(int32_t)(at*487u%200001)-100000:0;
            }
            memcpy(single,block,frames*sizeof *block);
            shared_block(&shared[0],bank,bpm,type,block,frames);
            for(unsigned n=0;n<frames;n++)shared_block(&shared[1],bank,bpm,type,&single[n],1);
            assert(!memcmp(block,single,frames*sizeof *block));
            assert(!memcmp(&shared[0],&shared[1],sizeof shared[0]));
            shared_block(&shared[0],bank,bpm,(type+1)%8,block,0);
            assert(!memcmp(&shared[0],&shared[1],sizeof shared[0]));
            assert(!memcmp(block,single,frames*sizeof *block));
        }
    }
    puts("Shared FX: block sizes 1..257, tempo edges, type changes, zero frames and full state PASS");
}
static void shared_bounds(void) {
    static const int32_t input[]={INT32_MIN,INT32_MAX,-1000000,1000000,-16000,16000};
    for(unsigned bank=0;bank<2;bank++)for(unsigned type=0;type<8;type++) {
        memset(&shared[0],0,sizeof shared[0]);
        uint64_t tail=0;
        for(unsigned at=0;at<MC_RATE*5;) {
            int32_t block[128][2]={0};
            unsigned frames=MC_RATE*5-at;if(frames>128)frames=128;
            for(unsigned n=0;n<frames&&at+n<MC_RATE;n++) {
                block[n][0]=input[(at+n)/17%6];block[n][1]=input[(at+n)/31%6];
            }
            shared_block(&shared[0],bank,40,type,block,frames);
            for(unsigned n=0;n<frames;n++)for(unsigned c=0;c<2;c++) {
                assert(block[n][c]>=-shared_bound(bank,type)&&block[n][c]<=shared_bound(bank,type));
                if(at+n>=MC_RATE*4)tail+=(uint64_t)(block[n][c]<0?-block[n][c]:block[n][c]);
            }
            at+=frames;
        }
        assert(tail==0);
        assert(bank?shared[0].cf_active:shared[0].rd_active);
    }
    puts("Shared FX: full int32 send bounds and sustained-input tail decay PASS");
}
int main(void) {
    legacy();impulse();filters();phaser();zero_to_wet();zero_amount_history();transitions();filter_history();prepared_tempo();blocks();stress();
    shared_impulses();shared_reverb_strength();shared_resets();shared_blocks();shared_bounds();
    puts("effects modes PASS");return 0;
}
