/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
#include "tables.h"
static int32_t clamp(int32_t x,int32_t lo,int32_t hi) { return x<lo?lo:x>hi?hi:x; }
static void zero(void *p,unsigned n) { uint8_t *b=p; while(n--) *b++=0; }
static int32_t mul(int32_t x,int32_t y) { return (x*y)/32768; }
static uint32_t random32(McEngine *e) {
    uint32_t r=e->random; r^=r<<13; r^=r>>17; r^=r<<5; return e->random=r;
}
void mc_init(McEngine *e) {
    zero(e,sizeof *e); mc_factory(&e->project); e->random=0x4d43464d;
    e->queued=255; e->master=28000;
}
void mc_off(McEngine *e,unsigned part,unsigned note) {
    for(unsigned i=0;i<MC_VOICES;i++) {
        McVoice *v=&e->voice[i];
        if(v->active&&v->part==part&&v->note==note) v->stage=2;
    }
}
void mc_note(McEngine *e,unsigned part,unsigned note,unsigned velocity,unsigned flags,uint32_t gate) {
    McVoice *v=0; unsigned oldest=0;
    if(part>=3||note<12||note>108||!velocity||velocity>127||((e->mute>>part)&1)) return;
    McPatch *p=&e->project.patch[part];
    /* The acid engine is monophonic per selected part, never fixed to part 1. */
    if(p->engine==MC_ACID) for(unsigned i=0;i<MC_VOICES;i++)
        if(e->voice[i].active&&e->voice[i].part==part) { v=&e->voice[i]; break; }
    if(v&&(flags&MC_SLIDE)&&v->stage!=2) {
        v->target=mc_frequency[note]; v->note=(uint8_t)note;
        v->gate_left=gate; v->patch=*p; v->accent=(flags&MC_ACCENT)!=0;
        return; /* legato preserves oscillator, filter and amplitude envelope */
    }
    if(!v) for(unsigned i=0;i<MC_VOICES;i++) if(!e->voice[i].active) { v=&e->voice[i]; break; }
    if(!v) {
        for(unsigned i=1;i<MC_VOICES;i++) if(e->voice[i].age>e->voice[oldest].age) oldest=i;
        v=&e->voice[oldest]; e->steals++;
    }
    zero(v,sizeof *v); v->active=1; v->part=(uint8_t)part; v->note=(uint8_t)note;
    v->velocity=(uint8_t)velocity; v->accent=(flags&MC_ACCENT)!=0;
    v->patch=*p; v->increment=v->target=mc_frequency[note];
    v->phase2=0x31000000u; v->gate_left=gate;
    /* Attack 1..~920 ms; decay/release 20 ms..~2.3 s. */
    v->attack_step=32767/(1+p->attack*p->attack*MC_RATE/18000);
    if(!v->attack_step) v->attack_step=1;
    v->decay_step=1+32767/(880+p->decay*p->decay*6);
    v->release_step=1+32767/(440+p->release*p->release*6);
}
void mc_drum(McEngine *e,unsigned sound,unsigned velocity) {
    if(sound>=MC_DRUMS||!velocity||velocity>127||(e->mute&8)) return;
    McDrum *d=0; unsigned oldest=0;
    for(unsigned i=0;i<MC_DRUM_VOICES;i++) {
        if(sound==2&&e->drum[i].sound==3) e->drum[i].env=0; /* closed hat chokes open */
        if(e->drum[i].env&&e->drum[i].sound==sound) d=&e->drum[i];
        if(e->drum[i].age>e->drum[oldest].age) oldest=i;
    }
    if(!d) for(unsigned i=0;i<MC_DRUM_VOICES;i++) if(!e->drum[i].env) { d=&e->drum[i]; break; }
    if(!d) d=&e->drum[oldest];
    zero(d,sizeof *d); d->env=32767; d->velocity=(uint8_t)velocity; d->sound=(uint8_t)sound;
}
static uint32_t step_samples(const McEngine *e) {
    unsigned swing=e->step&1?100-e->project.swing:100+e->project.swing;
    return (MC_RATE*15u* swing)/(e->project.bpm*100u);
}
static void trigger(McEngine *e) {
    uint32_t samples=step_samples(e);
    for(unsigned p=0;p<4;p++) {
        const McStep *s=&e->project.step[e->pattern][p][e->step];
        if(p==3) {
            for(unsigned d=0;d<MC_DRUMS;d++) if(s->drums&(1u<<d)) mc_drum(e,d,s->velocity);
        } else if(s->velocity) {
            /* Slide belongs to the destination step. Keep the source gate alive to it. */
            unsigned next=(e->step+1)%16;
            unsigned nextpat=(next==0&&e->queued<4)?e->queued:e->pattern;
            const McStep *n=&e->project.step[nextpat][p][next];
            uint32_t gate=(n->velocity&&(n->flags&MC_SLIDE))?samples+2:samples*s->gate/100;
            for(unsigned c=0;c<3;c++) if(s->note[c]) mc_note(e,p,s->note[c],s->velocity,s->flags,gate);
        }
    }
    e->events++;
}
void mc_start(McEngine *e) { e->playing=1; e->step=0; e->clock=0; trigger(e); }
void mc_stop(McEngine *e) {
    e->playing=0; e->queued=255;
    for(unsigned i=0;i<MC_VOICES;i++) e->voice[i].stage=2;
}
void mc_queue(McEngine *e,unsigned p) {
    if(p>=4) return;
    if(e->playing) e->queued=(uint8_t)p; else { e->pattern=(uint8_t)p; e->queued=255; }
}
void mc_mute(McEngine *e,unsigned p) {
    if(p>=4) return;
    e->mute^=(uint8_t)(1u<<p);
    if((e->mute>>p)&1) {
        for(unsigned i=0;i<MC_VOICES;i++) if(e->voice[i].part==p) e->voice[i].active=0;
        if(p==3) for(unsigned i=0;i<MC_DRUM_VOICES;i++) e->drum[i].env=0;
    }
}
void mc_preset(McEngine *e,unsigned part,unsigned preset) {
    if(part>=3||preset>=MC_PRESETS) return;
    for(unsigned i=0;i<MC_VOICES;i++) if(e->voice[i].part==part) e->voice[i].stage=2;
    e->project.patch[part]=mc_presets[preset];
}
void mc_control(McEngine *e,unsigned p,unsigned knob,int delta) {
    if(p>=3||knob>=4) return;
    McPatch *s=&e->project.patch[p];
    uint8_t *v=knob==0?&s->cutoff:knob==1?&s->resonance:knob==2?&s->decay:&s->pcm_mix;
    *v=(uint8_t)clamp(*v+delta,0,127);
    for(unsigned i=0;i<MC_VOICES;i++) if(e->voice[i].part==p) {
        e->voice[i].patch.cutoff=s->cutoff; e->voice[i].patch.resonance=s->resonance;
        e->voice[i].patch.pcm_mix=s->pcm_mix;
        e->voice[i].decay_step=1+32767/(880+s->decay*s->decay*6);
    }
}
static int32_t oscillator(uint32_t phase,unsigned wave,unsigned note) {
    if(wave==2) return mc_sine[phase>>22];
    unsigned band=note<36?0:(note-36)/12+1;
    if(band>6) band=6;
    const int16_t *table=wave?mc_square[band]:mc_saw[band];
    unsigned i=phase>>24, frac=(phase>>16)&255;
    return table[i]+((table[(i+1)&255]-table[i])*(int32_t)frac)/256;
}
static int32_t melodic(McVoice *v) {
    if(!v->active) return 0;
    McPatch *p=&v->patch;
    if(v->gate_left&&!--v->gate_left) v->stage=2;
    if(v->stage==0) {
        v->envelope+=v->attack_step;
        if(v->envelope>=32767) { v->envelope=32767; v->stage=1; }
    } else if(v->stage==1) {
        int32_t sustain=p->sustain*258;
        if(v->envelope>sustain) v->envelope=clamp(v->envelope-v->decay_step,sustain,32767);
    } else {
        v->envelope-=v->release_step;
        if(v->envelope<=0) { v->active=0; return 0; }
    }
    if(v->increment!=v->target) {
        int32_t diff=(int32_t)v->target-(int32_t)v->increment;
        int32_t step=diff/(20+p->glide*16);
        if(!step) step=diff>0?1:-1;
        v->increment=(uint32_t)((int32_t)v->increment+step);
    }
    v->phase+=v->increment; v->phase2+=v->increment+v->increment/401;
    int32_t sample=oscillator(v->phase,p->wave,v->note);
    if(p->engine==MC_LA) {
        sample=(sample+oscillator(v->phase2,p->wave,v->note))/2;
        unsigned pos=v->pcm_pos>>16;
        if(pos<511) {
            int32_t pcm=mc_attack[p->pcm][pos];
            pcm+=(mc_attack[p->pcm][pos+1]-pcm)*(int32_t)(v->pcm_pos&65535)/65536;
            int32_t weight=p->pcm_mix*(512-(int32_t)pos)/512;
            sample=sample*(127-weight)/127+pcm*weight/127;
            v->pcm_pos+=mc_pcm_rate[v->note];
        }
    }
    /* Bounded four-stage feedback lowpass. Original approximation, not circuit emulation. */
    int32_t cutoff=250+p->cutoff*p->cutoff/2;
    if(p->engine==MC_ACID) cutoff+=mul(v->envelope,3500+v->accent*4500);
    cutoff=clamp(cutoff,250,14000);
    int32_t x=clamp(sample-v->filter[3]*(p->resonance*3)/127,-24000,24000);
    for(unsigned k=0;k<(p->engine==MC_ACID?4u:2u);k++) {
        v->filter[k]+=mul(x-v->filter[k],cutoff); x=v->filter[k];
    }
    v->age++;
    x=mul(x,v->envelope); x=x*v->velocity/127;
    x=x*p->level/127;
    if(v->accent) x=x*5/4;
    return x;
}
static int32_t percussion(McEngine *e,McDrum *d) {
    if(!d->env) return 0;
    unsigned sound=d->sound;
    int32_t noise=(int32_t)(random32(e)>>17)-16384, s=0;
    /* BD, SN, CH, OH, CP, LT, B2, RS, S2, HT, CR, RD, SH, CB, CL, MT. */
    if(sound==0||sound==6||sound==5||sound==9||sound==15) {
        unsigned hz=sound==0?48:sound==6?65:sound==5?110:sound==9?185:140;
        uint32_t sweep=(uint32_t)(d->env*d->env/32768);
        d->phase+=hz*97392u+sweep*(sound<7?550u:250u);
        s=mc_sine[d->phase>>22];
        if(d->age<130) s+=noise*(130-(int)d->age)/260;
    } else if(sound==1||sound==8) {
        d->phase+=(sound==1?180u:260u)*97392u;
        s=(noise*3+mc_sine[d->phase>>22])/4;
    } else if(sound==4) {
        s=noise;
        if(d->age<1323&&d->age%441>220) s/=4;
    } else if(sound==7||sound==14) {
        d->phase+=(sound==7?1100u:1700u)*97392u;
        s=(noise+mc_sine[d->phase>>22])/2;
    } else if(sound==13) {
        d->phase+=540u*97392u;
        s=((d->phase&0x80000000u)?7500:-7500)+(((d->phase*3/2)&0x80000000u)?7000:-7000);
    } else {
        d->noise_lp+=(noise-d->noise_lp)/4;
        d->phase+=4021u*97392u;
        s=(noise-d->noise_lp)/2+((d->phase&0x80000000u)?3500:-3500);
    }
    static const uint16_t decay[16]={32760,32749,32685,32758,32747,32755,32757,32500,32738,32748,32762,32759,32600,32745,32400,32752};
    s=mul(s,d->env)*d->velocity/127;
    s=s*e->project.drum_level[sound]/127;
    d->env=mul(d->env,decay[sound]); d->age++;
    if(d->env<16) d->env=0;
    return s;
}
void mc_render(McEngine *e,int16_t *stereo,unsigned frames) {
    for(unsigned f=0;f<frames;f++) {
        if(e->playing) {
            unsigned swing=e->step&1?100-e->project.swing:100+e->project.swing;
            e->clock+=e->project.bpm*400u;
            if(e->clock>=MC_RATE*60u*swing) {
                e->clock-=MC_RATE*60u*swing;
                e->step=(uint8_t)((e->step+1)%16);
                if(!e->step&&e->queued<4) { e->pattern=e->queued; e->queued=255; }
                trigger(e);
            }
        }
        int32_t part[4]={0,0,0,0}, l=0,r=0;
        for(unsigned i=0;i<MC_VOICES;i++) part[e->voice[i].part]+=melodic(&e->voice[i]);
        for(unsigned i=0;i<MC_DRUM_VOICES;i++) part[3]+=percussion(e,&e->drum[i]);
        for(unsigned p=0;p<4;p++) {
            int32_t v=part[p]*e->project.level[p]/127;
            if((e->mute>>p)&1) v=0;
            l+=v*(127-e->project.pan[p])/127; r+=v*e->project.pan[p]/127;
        }
        /* Conservative fixed headroom, then a bounded safety clamp; no wrapping. */
        l=mul(l/3,e->master); r=mul(r/3,e->master);
        if(l>32767||l< -32767||r>32767||r< -32767) e->clips++;
        int32_t al=l<0?-l:l, ar=r<0?-r:r;
        if((uint32_t)al>e->peak) e->peak=(uint32_t)al;
        if((uint32_t)ar>e->peak) e->peak=(uint32_t)ar;
        l=clamp(l,-32767,32767); r=clamp(r,-32767,32767);
        stereo[f*2]=(int16_t)l; stereo[f*2+1]=(int16_t)r; e->elapsed++;
    }
}
