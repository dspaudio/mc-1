/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio. Original fixed-point effects, no external DSP. */
#include "mcfm.h"
static int32_t fx_limit(int32_t x) { return x<-16000?-16000:x>16000?16000:x; }
static int32_t fx_bound(int32_t x,int32_t limit) { return x<-limit?-limit:x>limit?limit:x; }
static void fx_clear(void *data,unsigned size) {
    uint8_t *p=data;while(size--)*p++=0;
}
static int32_t fx_drive(int32_t x,unsigned amount) {
    if(!amount)return x;
    int32_t v=fx_limit(x)*(1+(int32_t)amount/8);
    int32_t a=v<0?-v:v;
    int32_t wet=v*8192/(8192+a);
    return (x*(127-(int32_t)amount)+wet*(int32_t)amount)/127;
}
static int32_t fx_distort(int32_t x,unsigned amount,unsigned type) {
    if(!type)return fx_drive(x,amount);
    if(!amount)return x;
    int32_t v=fx_limit(x)*(1+(int32_t)amount/8),wet;
    if(type==1)wet=fx_bound(v,8192);
    else if(type==2) {
        int32_t phase=(v+8192)%32768;if(phase<0)phase+=32768;
        wet=phase<16384?phase-8192:24576-phase;
    } else {
        v=fx_limit(v);wet=v*(v<0?-v:v)/16000;
        wet=(wet/512)*512;
    }
    return (x*(127-(int32_t)amount)+wet*(int32_t)amount)/127;
}
static int32_t fx_allpass(int32_t input,int32_t coefficient,int32_t *state) {
    int32_t out=fx_bound(*state-input*coefficient/32768,48000);
    *state=fx_bound(input+out*coefficient/32768,48000);
    return out;
}
static void fx_modes(McEffects *f,const uint8_t type[4]) {
    for(unsigned i=0;i<4;i++)if(f->type[i]!=type[i]) {
        if(i==0) {
            fx_clear(f->delay,sizeof f->delay);fx_clear(f->tape,sizeof f->tape);f->delay_pos=0;
        } else if(i==1) {
            fx_clear(f->comb,sizeof f->comb);fx_clear(f->damp,sizeof f->damp);
            fx_clear(f->comb_pos,sizeof f->comb_pos);fx_clear(f->reverb_ap,sizeof f->reverb_ap);
        } else if(i==3) {
            fx_clear(f->chorus,sizeof f->chorus);fx_clear(f->phaser,sizeof f->phaser);
            f->chorus_pos=0;f->phase=0;
        }
        f->type[i]=type[i];
    }
}
void mc_effects_process_typed(McEffects *f,unsigned bpm,const uint8_t amount[4],
    const uint8_t type[4],const uint8_t filter[2],int32_t *l,int32_t *r) {
    fx_modes(f,type);
    unsigned delay=amount[0],reverb=amount[1],drive=amount[2],chorus=amount[3];
    int32_t left=fx_distort(*l,drive,type[2]),right=fx_distort(*r,drive,type[2]);
    /* Stereo triangle-modulated short delay, ~0.3 Hz. Linear interpolation. */
    {
        f->phase+=29217u;
        unsigned tri=f->phase>>20;if(tri>2047)tri=4095-tri;
        unsigned taps[2]={4800+tri*2,4800+(2047-tri)*2};
        if(type[3]==2) {taps[0]=32+tri*4;taps[1]=32+(2047-tri)*4;}
        int32_t in[2]={left,right},wet[2];
        for(unsigned c=0;c<2;c++) {
            if(type[3]==1) {
                wet[c]=fx_limit(in[c]);
                int32_t coefficient=8192+(int32_t)(c?2047-tri:tri)*8;
                for(unsigned stage=0;stage<4;stage++)
                    wet[c]=fx_allpass(wet[c],coefficient+(int32_t)stage*1024,&f->phaser[c][stage]);
            } else {
                if(chorus) {
                    unsigned pos=(f->chorus_pos-(taps[c]>>4))&1023u,next=(pos-1)&1023u,frac=taps[c]&15;
                    wet[c]=(f->chorus[c][pos]*(int32_t)(16-frac)+f->chorus[c][next]*(int32_t)frac)/16;
                } else wet[c]=0;
                f->chorus[c][f->chorus_pos]=(int16_t)fx_limit(in[c]+(type[3]==2&&chorus?wet[c]/2:0));
            }
        }
        f->chorus_pos=(uint16_t)((f->chorus_pos+1)&1023u);
        left+=(wet[0]-left)*(int32_t)chorus/254;right+=(wet[1]-right)*(int32_t)chorus/254;
    }
    {
        /* Tempo-linked sixteenth, capped at 185.7 ms at low BPM. Feedback < 0.45. */
        unsigned length=MC_RATE*15u/(bpm?bpm:1);if(length>8191)length=8191;
        if(!length)length=1;
        unsigned pos=(f->delay_pos+8192-length)&8191u;
        int32_t dl=f->delay[0][pos],dr=f->delay[1][pos];
        int32_t write_l,write_r;
        if(type[0]==1) {
            f->tape[0]+=(dl-f->tape[0])/4;f->tape[1]+=(dr-f->tape[1])/4;
            dl=f->tape[0];dr=f->tape[1];
            write_l=left+fx_drive(dl,32)*3/5;write_r=right+fx_drive(dr,32)*3/5;
        } else if(type[0]==2) {
            write_l=dr/2;write_r=(left+right)/2+dl/2;
        } else if(type[0]==3) {
            unsigned half=(f->delay_pos+8192-length/2)&8191u;
            unsigned third=(f->delay_pos+8192-length*3/4)&8191u;
            dl=(dl*3+f->delay[1][third]*2+f->delay[0][half])/6;
            dr=(dr*3+f->delay[0][third]*2+f->delay[1][half])/6;
            write_l=left+dl/4;write_r=right+dr/4;
        } else {write_l=left+dr*7/16;write_r=right+dl*7/16;}
        f->delay[0][f->delay_pos]=(int16_t)fx_limit(delay?write_l:0);
        f->delay[1][f->delay_pos]=(int16_t)fx_limit(delay?write_r:0);
        f->delay_pos=(uint16_t)((f->delay_pos+1)&8191u);
        left+=dl*(int32_t)delay/127;right+=dr*(int32_t)delay/127;
    }
    {
        /* Four damped feedback combs; distinct prime-ish lengths and stereo taps. */
        static const unsigned lengths[4][4]={
            {1557,1617,1491,1422},{2039,1999,1867,1747},
            {997,1153,1301,1451},{331,457,673,911}};
        unsigned mode=type[1]<4?type[1]:0;
        int32_t input=reverb?(left+right)/4:0,wl=0,wr=0;
        int32_t previous=0;
        for(unsigned i=0;i<4;i++) {
            int32_t tap=f->comb[i][f->comb_pos[i]];
            f->damp[i]+=(tap-f->damp[i])/(mode==0?4:mode==1?8:2);
            int32_t feedback=f->damp[i]*(mode==1?13:mode==2?12:11)/16;
            if(mode==3&&(i&1))feedback=-feedback;
            f->comb[i][f->comb_pos[i]]=(int16_t)fx_limit(input+feedback+(mode==3?previous/4:0));
            previous=tap;
            if(++f->comb_pos[i]>=lengths[mode][i])f->comb_pos[i]=0;
            if(i&1)wr+=tap;else wl+=tap;
        }
        if(mode==1||mode==2) {
            wl=fx_allpass(wl,mode==1?16384:24576,&f->reverb_ap[0]);
            wr=fx_allpass(wr,mode==1?16384:24576,&f->reverb_ap[1]);
        } else if(mode==3) {int32_t old=wl;wl-=wr/2;wr-=old/2;}
        left+=wl*(int32_t)reverb/254;right+=wr*(int32_t)reverb/254;
    }
    for(unsigned i=0;i<2;i++) {
        int32_t *state=i?f->lowpass:f->highpass;
        if(f->filter_amount[i]!=filter[i]) {
            if(!f->filter_amount[i]||!filter[i])fx_clear(state,2*sizeof *state);
            unsigned value=filter[i],cutoff=i?40+(127-value)*(127-value):30+value*value/3;
            f->filter_coef[i]=(uint16_t)(cutoff*32768u/(cutoff+7019u));
            f->filter_amount[i]=filter[i];
        }
        if(filter[i]) {
            state[0]+=(int32_t)(((int64_t)left-state[0])*f->filter_coef[i]/32768);
            state[1]+=(int32_t)(((int64_t)right-state[1])*f->filter_coef[i]/32768);
            if(i){left=state[0];right=state[1];}
            else {left-=state[0];right-=state[1];}
        }
    }
    *l=left;*r=right;
}
void mc_effects_process(McEffects *f,unsigned bpm,const uint8_t amount[4],int32_t *l,int32_t *r) {
    static const uint8_t zero[4]={0};mc_effects_process_typed(f,bpm,amount,zero,zero,l,r);
}
void mc_effects(McEngine *e,int32_t *l,int32_t *r) {
    mc_effects_process_typed(&e->effects,e->project.bpm,e->project.fx,e->project.fx_type,e->project.filter,l,r);
}
