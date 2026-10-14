/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio. Original fixed-point effects, no external DSP. */
#include "mcfm.h"
static int32_t fx_limit(int32_t x) { return x<-16000?-16000:x>16000?16000:x; }
static int32_t fx_drive(int32_t x,unsigned amount) {
    if(!amount)return x;
    int32_t v=fx_limit(x)*(1+(int32_t)amount/8);
    int32_t a=v<0?-v:v;
    int32_t wet=v*8192/(8192+a);
    return (x*(127-(int32_t)amount)+wet*(int32_t)amount)/127;
}
void mc_effects_process(McEffects *f,unsigned bpm,const uint8_t amount[4],int32_t *l,int32_t *r) {
    unsigned delay=amount[0],reverb=amount[1],drive=amount[2],chorus=amount[3];
    int32_t left=fx_drive(*l,drive),right=fx_drive(*r,drive);
    /* Stereo triangle-modulated short delay, ~0.3 Hz. Linear interpolation. */
    {
        f->phase+=29217u;
        unsigned tri=f->phase>>20;if(tri>2047)tri=4095-tri;
        unsigned taps[2]={4800+tri*2,4800+(2047-tri)*2};
        int32_t in[2]={left,right},wet[2];
        for(unsigned c=0;c<2;c++) {
            unsigned pos=(f->chorus_pos-(taps[c]>>4))&1023u,next=(pos-1)&1023u,frac=taps[c]&15;
            wet[c]=(f->chorus[c][pos]*(int32_t)(16-frac)+f->chorus[c][next]*(int32_t)frac)/16;
            f->chorus[c][f->chorus_pos]=(int16_t)fx_limit(in[c]);
        }
        f->chorus_pos=(uint16_t)((f->chorus_pos+1)&1023u);
        left+=(wet[0]-left)*(int32_t)chorus/254;right+=(wet[1]-right)*(int32_t)chorus/254;
    }
    {
        /* Tempo-linked sixteenth, capped at 185.7 ms at low BPM. Feedback < 0.45. */
        unsigned length=MC_RATE*15u/bpm;if(length>8191)length=8191;
        unsigned pos=(f->delay_pos+8192-length)&8191u;
        int32_t dl=f->delay[0][pos],dr=f->delay[1][pos];
        f->delay[0][f->delay_pos]=(int16_t)fx_limit(delay?left+dr*7/16:0);
        f->delay[1][f->delay_pos]=(int16_t)fx_limit(delay?right+dl*7/16:0);
        f->delay_pos=(uint16_t)((f->delay_pos+1)&8191u);
        left+=dl*(int32_t)delay/127;right+=dr*(int32_t)delay/127;
    }
    {
        /* Four damped feedback combs; distinct prime-ish lengths and stereo taps. */
        static const unsigned lengths[4]={1557,1617,1491,1422};
        int32_t input=reverb?(left+right)/4:0,wl=0,wr=0;
        for(unsigned i=0;i<4;i++) {
            int32_t tap=f->comb[i][f->comb_pos[i]];
            f->damp[i]+=(tap-f->damp[i])/4;
            f->comb[i][f->comb_pos[i]]=(int16_t)fx_limit(input+f->damp[i]*11/16);
            if(++f->comb_pos[i]>=lengths[i])f->comb_pos[i]=0;
            if(i&1)wr+=tap;else wl+=tap;
        }
        left+=wl*(int32_t)reverb/254;right+=wr*(int32_t)reverb/254;
    }
    *l=left;*r=right;
}
void mc_effects(McEngine *e,int32_t *l,int32_t *r) {
    mc_effects_process(&e->effects,e->project.bpm,e->project.fx,l,r);
}
