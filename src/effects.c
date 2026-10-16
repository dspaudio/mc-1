/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio. Original fixed-point effects, no external DSP. */
#include "mcfm.h"
#include "fixed.h"
static int32_t fx_limit(int32_t x) { return x<-16000?-16000:x>16000?16000:x; }
static int32_t fx_bound(int32_t x,int32_t limit) { return x<-limit?-limit:x>limit?limit:x; }
static void fx_clear(void *data,unsigned size) {
    uint8_t *p=data;while(size--)*p++=0;
}
static int32_t fx_drive(int32_t x,unsigned amount) {
    if(!amount)return x;
    int32_t v=fx_limit(x)*(1+(int32_t)(amount>>3));
    int32_t a=v<0?-v:v;
    int32_t wet=v*8192/(8192+a);
    return mc_div127(x*(127-(int32_t)amount)+wet*(int32_t)amount);
}
static inline __attribute__((always_inline)) int32_t fx_distort(int32_t x,unsigned amount,unsigned type) {
    if(!type)return fx_drive(x,amount);
    if(!amount)return x;
    int32_t v=fx_limit(x)*(1+(int32_t)(amount>>3)),wet;
    if(type==1)wet=fx_bound(v,8192);
    else if(type==2) {
        int32_t phase=(v+8192)&32767;
        wet=phase<16384?phase-8192:24576-phase;
    } else {
        v=fx_limit(v);wet=v*(v<0?-v:v)/16000;
        wet=mc_div_pow2(wet,9)*512;
    }
    return mc_div127(x*(127-(int32_t)amount)+wet*(int32_t)amount);
}
static int32_t fx_allpass(int32_t input,int32_t coefficient,int32_t *state) {
    int32_t out=fx_bound(*state-mc_div_pow2(input*coefficient,15),48000);
    *state=fx_bound(input+mc_div_pow2(out*coefficient,15),48000);
    return out;
}
static void fx_modes(McTrackFx *t,McDelay *d,McReverb *r,const uint8_t type[4]) {
    if(d&&d->type!=type[0]) {
        fx_clear(d->delay,sizeof d->delay);fx_clear(d->tape,sizeof d->tape);d->delay_pos=0;d->type=type[0];
    }
    if(r&&r->type!=type[1]) {
        fx_clear(r->comb,sizeof r->comb);fx_clear(r->damp,sizeof r->damp);
        fx_clear(r->comb_pos,sizeof r->comb_pos);fx_clear(r->ap,sizeof r->ap);r->type=type[1];
    }
    if(t&&t->mod_type!=type[3]) {
        fx_clear(t->chorus,sizeof t->chorus);fx_clear(t->phaser,sizeof t->phaser);
        t->chorus_pos=0;t->phase=0;t->mod_type=type[3];
    }
}
/* Four damped feedback combs; distinct prime-ish lengths and stereo taps. */
static const unsigned fx_comb_lengths[4][4]={
    {1557,1617,1491,1422},{2039,1999,1867,1747},
    {997,1153,1301,1451},{331,457,673,911}};
static void fx_reverb_frame(int16_t comb[4][2048],int32_t damp[4],uint16_t comb_pos[4],int32_t ap[2],
    unsigned mode,int active,int32_t input,int32_t *out_l,int32_t *out_r) {
    unsigned damping=mode==0?2:mode==1?3:1;
    int32_t feedback_gain=mode==1?13:mode==2?12:11;
    int32_t wl=0,wr=0,previous=0;
    for(unsigned i=0;i<4;i++) {
        int32_t tap=comb[i][comb_pos[i]];
        damp[i]+=mc_div_pow2(tap-damp[i],damping);
        int32_t feedback=mc_div_pow2(damp[i]*feedback_gain,4);
        if(mode==3&&(i&1))feedback=-feedback;
        comb[i][comb_pos[i]]=(int16_t)fx_limit(input+feedback+(mode==3?mc_div_pow2(previous,2):0));
        previous=tap;
        if(++comb_pos[i]>=fx_comb_lengths[mode][i])comb_pos[i]=0;
        if(i&1)wr+=tap;else wl+=tap;
    }
    if(mode==1||mode==2) {
        wl=fx_allpass(wl,mode==1?16384:24576,&ap[0]);
        wr=fx_allpass(wr,mode==1?16384:24576,&ap[1]);
    } else if(mode==3&&active) {int32_t old=wl;wl-=mc_div_pow2(wr,1);wr-=mc_div_pow2(old,1);}
    *out_l=wl;*out_r=wr;
}
void mc_reverb_block(McReverb *r,unsigned mode,int active,const int32_t *input,int32_t wet[restrict][2],unsigned frames) {
    if(mode>3)mode=0;
    for(unsigned n=0;n<frames;n++)
        fx_reverb_frame(r->comb,r->damp,r->comb_pos,r->ap,mode,active,input[n],&wet[n][0],&wet[n][1]);
}
/* A stage runs only when its state is supplied: track (drive, modulation, filters), delay, reverb. */
static inline __attribute__((always_inline)) void fx_block(McTrackFx *t,McDelay *d,McReverb *r,unsigned length,const uint8_t amount[4],
    const uint8_t type[4],const uint8_t filter[2],int32_t stereo[restrict][2],unsigned frames) {
    if(!frames)return;
    fx_modes(t,d,r,type);
    unsigned delay=amount[0],reverb=amount[1],drive=amount[2],chorus=amount[3];
    /* Stereo triangle-modulated short delay, ~0.3 Hz. Linear interpolation. */
    for(unsigned n=0;t&&!MC_PROBE_SKIP(16)&&n<frames;n++) {
        int32_t left=fx_distort(stereo[n][0],drive,type[2]),right=fx_distort(stereo[n][1],drive,type[2]);
        t->phase+=29217u;
        unsigned tri=t->phase>>20;if(tri>2047)tri=4095-tri;
        unsigned taps[2]={4800+tri*2,4800+(2047-tri)*2};
        if(type[3]==2) {taps[0]=32+tri*4;taps[1]=32+(2047-tri)*4;}
        int32_t in[2]={left,right},wet[2];
        for(unsigned c=0;c<2;c++) {
            if(type[3]==1) {
                wet[c]=fx_limit(in[c]);
                int32_t coefficient=8192+(int32_t)(c?2047-tri:tri)*8;
                for(unsigned stage=0;stage<4;stage++)
                    wet[c]=fx_allpass(wet[c],coefficient+(int32_t)stage*1024,&t->phaser[c][stage]);
            } else {
                if(chorus) {
                    unsigned pos=(t->chorus_pos-(taps[c]>>4))&1023u,next=(pos-1)&1023u,frac=taps[c]&15;
                    wet[c]=mc_div_pow2(t->chorus[c][pos]*(int32_t)(16-frac)+t->chorus[c][next]*(int32_t)frac,4);
                } else wet[c]=0;
                t->chorus[c][t->chorus_pos]=(int16_t)fx_limit(in[c]+(type[3]==2&&chorus?mc_div_pow2(wet[c],1):0));
            }
        }
        t->chorus_pos=(uint16_t)((t->chorus_pos+1)&1023u);
        if(chorus) {
            left+=mc_div254((wet[0]-left)*(int32_t)chorus);
            right+=mc_div254((wet[1]-right)*(int32_t)chorus);
        }
        stereo[n][0]=left;stereo[n][1]=right;
    }
    /* Each stage keeps its original sample order, including zero-amount histories. */
    for(unsigned n=0;d&&n<frames;n++) {
        int32_t left=stereo[n][0],right=stereo[n][1];
        /* Tempo-linked sixteenth, capped at 185.7 ms at low BPM. Feedback < 0.45. */
        int32_t dl=0,dr=0,write_l=0,write_r=0;
        if(delay||type[0]==1) {
            unsigned pos=(d->delay_pos+8192-length)&8191u;
            dl=d->delay[0][pos];dr=d->delay[1][pos];
            if(type[0]==1) {
                d->tape[0]+=mc_div_pow2(dl-d->tape[0],2);d->tape[1]+=mc_div_pow2(dr-d->tape[1],2);
                dl=d->tape[0];dr=d->tape[1];
                if(delay) {
                    write_l=left+fx_drive(dl,32)*3/5;write_r=right+fx_drive(dr,32)*3/5;
                }
            } else if(type[0]==2) {
                write_l=mc_div_pow2(dr,1);write_r=mc_div_pow2(left+right,1)+mc_div_pow2(dl,1);
            } else if(type[0]==3) {
                unsigned half=(d->delay_pos+8192-(length>>1))&8191u;
                unsigned third=(d->delay_pos+8192-length*3/4)&8191u;
                dl=(dl*3+d->delay[1][third]*2+d->delay[0][half])/6;
                dr=(dr*3+d->delay[0][third]*2+d->delay[1][half])/6;
                write_l=left+mc_div_pow2(dl,2);write_r=right+mc_div_pow2(dr,2);
            } else {write_l=left+mc_div_pow2(dr*7,4);write_r=right+mc_div_pow2(dl*7,4);}
        }
        d->delay[0][d->delay_pos]=(int16_t)fx_limit(delay?write_l:0);
        d->delay[1][d->delay_pos]=(int16_t)fx_limit(delay?write_r:0);
        d->delay_pos=(uint16_t)((d->delay_pos+1)&8191u);
        if(delay) {
            left+=mc_div127(dl*(int32_t)delay);right+=mc_div127(dr*(int32_t)delay);
        }
        stereo[n][0]=left;stereo[n][1]=right;
    }
    unsigned mode=type[1]<4?type[1]:0;
    for(unsigned n=0;r&&n<frames;n++) {
        int32_t left=stereo[n][0],right=stereo[n][1],wl,wr;
        fx_reverb_frame(r->comb,r->damp,r->comb_pos,r->ap,mode,reverb!=0,
            reverb?mc_div_pow2(left+right,2):0,&wl,&wr);
        if(reverb) {
            left+=mc_div254(wl*(int32_t)reverb);right+=mc_div254(wr*(int32_t)reverb);
        }
        stereo[n][0]=left;stereo[n][1]=right;
    }
    for(unsigned i=0;t&&!MC_PROBE_SKIP(32)&&i<2;i++) {
        int32_t *state=i?t->lowpass:t->highpass;
        if(t->filter_amount[i]!=filter[i]) {
            if(!t->filter_amount[i]||!filter[i])fx_clear(state,2*sizeof *state);
            unsigned value=filter[i],cutoff=i?40+(127-value)*(127-value):30+value*value/3;
            t->filter_coef[i]=(uint16_t)(cutoff*32768u/(cutoff+7019u));
            t->filter_amount[i]=filter[i];
        }
        if(filter[i])for(unsigned n=0;n<frames;n++) {
            state[0]+=mc_filter_step(stereo[n][0],state[0],t->filter_coef[i]);
            state[1]+=mc_filter_step(stereo[n][1],state[1],t->filter_coef[i]);
            if(i){stereo[n][0]=state[0];stereo[n][1]=state[1];}
            else {stereo[n][0]-=state[0];stereo[n][1]-=state[1];}
        }
    }
}
void mc_effects_process_block_length(McEffects *f,unsigned length,const uint8_t amount[4],
    const uint8_t type[4],const uint8_t filter[2],int32_t stereo[restrict][2],unsigned frames) {
    fx_block(&f->track,&f->delay,&f->reverb,length,amount,type,filter,stereo,frames);
}
void mc_effects_process_track(McTrackFx *t,const uint8_t amount[4],const uint8_t type[4],
    const uint8_t filter[2],int32_t stereo[restrict][2],unsigned frames) {
    fx_block(t,0,0,1,amount,type,filter,stereo,frames);
}
void mc_effects_process_delay_bus(McDelay *d,unsigned length,unsigned type,
    int32_t stereo[restrict][2],unsigned frames) {
    const uint8_t amount[4]={127,0,0,0},types[4]={(uint8_t)type,0,0,0},filter[2]={0,0};
    fx_block(0,d,0,length,amount,types,filter,stereo,frames);
}
void mc_effects_process_typed_length(McEffects *f,unsigned length,const uint8_t amount[4],
    const uint8_t type[4],const uint8_t filter[2],int32_t *l,int32_t *r) {
    int32_t stereo[1][2]={{*l,*r}};
    mc_effects_process_block_length(f,length,amount,type,filter,stereo,1);
    *l=stereo[0][0];*r=stereo[0][1];
}
void mc_effects_process_typed(McEffects *f,unsigned bpm,const uint8_t amount[4],
    const uint8_t type[4],const uint8_t filter[2],int32_t *l,int32_t *r) {
    unsigned length=1;
    if(amount[0]||type[0]==1) {
        length=MC_RATE*15u/(bpm?bpm:1);if(length>8191)length=8191;
        if(!length)length=1;
    }
    mc_effects_process_typed_length(f,length,amount,type,filter,l,r);
}
void mc_effects_process(McEffects *f,unsigned bpm,const uint8_t amount[4],int32_t *l,int32_t *r) {
    static const uint8_t zero[4]={0};mc_effects_process_typed(f,bpm,amount,zero,zero,l,r);
}

const char *const mc_fx_rd_names[8]={
    "ROOM1","ROOM2","ROOM3","HALL1","HALL2","PLATE","DELAY","PANNING DLY"};
const char *const mc_fx_cf_names[8]={
    "CHORUS1","CHORUS2","CHORUS3","CHORUS4","FB CHORUS","FLANGER","SHORT DLY","FB SHORT DLY"};

/* 공유 뱅크는 직접음을 포함하지 않으며, 타입 변경 때 해당 뱅크만 초기화한다. */
void mc_fx_rd_block(McSharedFx *f,unsigned bpm,unsigned type,int32_t stereo[][2],unsigned frames) {
    if(!frames)return;
    if(!f->rd_active||f->rd_type!=type) {
        fx_clear(&f->rd,sizeof f->rd);f->rd_type=(uint8_t)type;f->rd_active=1;
    }
    if(type>=6) {
        McDelay *d=&f->rd.delay;
        unsigned length=MC_RATE*15u/(bpm?bpm:1);
        if(length>8191)length=8191;
        if(!length)length=1;
        for(unsigned n=0;n<frames;n++) {
            unsigned pos=(d->delay_pos-length)&8191u;
            int32_t dl=d->delay[0][pos],dr=d->delay[1][pos];
            int32_t left=fx_limit(stereo[n][0]),right=fx_limit(stereo[n][1]);
            if(type==7) {
                d->delay[0][d->delay_pos]=(int16_t)fx_limit(mc_div_pow2(left+right,1)+mc_div_pow2(dr,1));
                d->delay[1][d->delay_pos]=(int16_t)mc_div_pow2(dl,1);
            } else {
                d->delay[0][d->delay_pos]=(int16_t)fx_limit(left+mc_div_pow2(dr*7,4));
                d->delay[1][d->delay_pos]=(int16_t)fx_limit(right+mc_div_pow2(dl*7,4));
            }
            d->delay_pos=(uint16_t)((d->delay_pos+1)&8191u);
            stereo[n][0]=dl;stereo[n][1]=dr;
        }
    } else {
        static const struct {
            uint16_t length[4],diffusion;
            uint8_t damping,feedback;
        } profile[6]={
            {{331,457,673,911},0,2,9},
            {{751,809,947,1031},8192,2,10},
            {{997,1153,1301,1451},12288,3,11},
            {{1557,1617,1491,1422},16384,2,12},
            {{2039,1999,1867,1747},20480,3,13},
            {{887,1061,1217,1367},24576,1,12}};
        McReverb *r=&f->rd.reverb;
        unsigned damping=profile[type].damping;
        int32_t feedback=profile[type].feedback,diffusion=profile[type].diffusion,rounding=(1<<damping)-1;
        for(unsigned n=0;n<frames;n++) {
            int32_t input=mc_div_pow2(fx_limit(stereo[n][0])+fx_limit(stereo[n][1]),2);
            int32_t wet[2]={0,0};
            for(unsigned i=0;i<4;i++) {
                unsigned pos=r->comb_pos[i];
                int32_t tap=r->comb[i][pos];
                /* 1 LSB 미만의 감쇠도 진행시켜 무입력 잔류 DC를 없앤다. */
                int32_t delta=tap-r->damp[i];
                r->damp[i]+=delta<0?-((-delta+rounding)>>damping):(delta+rounding)>>damping;
                r->comb[i][pos]=(int16_t)fx_limit(input+mc_div_pow2(r->damp[i]*feedback,4));
                if(++pos>=profile[type].length[i])pos=0;
                r->comb_pos[i]=(uint16_t)pos;
                wet[i&1]+=tap;
            }
            if(diffusion) {
                wet[0]=fx_allpass(wet[0],diffusion,&r->ap[0]);
                wet[1]=fx_allpass(wet[1],diffusion,&r->ap[1]);
            }
            stereo[n][0]=mc_div_pow2(wet[0],1);stereo[n][1]=mc_div_pow2(wet[1],1);
        }
    }
}

void mc_fx_cf_block(McSharedFx *f,unsigned type,int32_t stereo[][2],unsigned frames) {
    static const struct {
        uint32_t rate;
        uint16_t base,depth;
        uint8_t feedback;
    } profile[8]={
        {29217u,4800,4094,0},{43825u,6400,2047,0},
        {21913u,3200,6141,0},{58435u,8000,6141,0},
        {36521u,4800,4094,7},{73043u,32,8190,10},
        {0,11200,0,0},{0,11200,0,10}};
    if(!frames)return;
    if(!f->cf_active||f->cf_type!=type) {
        fx_clear(&f->cf,sizeof f->cf);f->cf_type=(uint8_t)type;f->cf_active=1;
    }
    McTrackFx *t=&f->cf;
    unsigned base=profile[type].base,depth=profile[type].depth;
    uint32_t rate=profile[type].rate;
    int32_t feedback=profile[type].feedback;
    for(unsigned n=0;n<frames;n++) {
        t->phase+=rate;
        unsigned tri=t->phase>>20;if(tri>2047)tri=4095-tri;
        unsigned taps[2]={base+((tri*depth)>>11),base+(((2047-tri)*depth)>>11)};
        for(unsigned c=0;c<2;c++) {
            unsigned pos=(t->chorus_pos-(taps[c]>>4))&1023u,next=(pos-1)&1023u,frac=taps[c]&15;
            int32_t wet=mc_div_pow2(t->chorus[c][pos]*(int32_t)(16-frac)+t->chorus[c][next]*(int32_t)frac,4);
            t->chorus[c][t->chorus_pos]=(int16_t)fx_limit(fx_limit(stereo[n][c])+mc_div_pow2(wet*feedback,4));
            stereo[n][c]=wet;
        }
        t->chorus_pos=(uint16_t)((t->chorus_pos+1)&1023u);
    }
}
