/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
#include "tables.h"
#include "recorded_pcm.h"
#include "fixed.h"
const char *const mc_kit_names[MC_KITS]={"BASE 808","CLUB 909","TIGHT 606","VINTAGE","HOUSE","BREAK JUNGLE","IRON WORKS","ELECTRO LATIN"};
typedef struct {
    uint16_t hz[MC_DRUMS],decay[MC_DRUMS],sweep,click,clap;
    uint8_t noise_weight,highpass;
} DrumKit;
/* Original oscillator/noise recipes; BASE preserves the original arithmetic. */
static const DrumKit kits[MC_KITS]={
    {{48,180,4021,4021,0,110,65,1100,260,185,4021,4021,4021,540,1700,140},
     {32760,32749,32685,32758,32747,32755,32757,32500,32738,32748,32762,32759,32600,32745,32400,32752},550,130,441,3,4},
    {{55,205,6340,5820,0,125,72,1350,310,210,7310,4930,8120,620,1920,165},
     {32757,32753,32692,32753,32751,32751,32755,32540,32744,32744,32760,32756,32570,32740,32370,32749},920,210,320,5,6},
    {{62,230,5180,4870,0,145,81,1450,345,235,6190,5270,9200,680,2140,185},
     {32749,32736,32620,32739,32734,32739,32745,32400,32728,32735,32750,32745,32480,32725,32250,32738},390,80,270,4,3},
    {{43,155,3140,2890,0,96,58,870,225,155,3560,2670,4390,430,1390,120},
     {32753,32738,32660,32745,32732,32748,32751,32470,32730,32741,32755,32748,32520,32734,32320,32745},300,55,580,2,2},
    {{51,195,4590,5230,0,132,69,1260,285,198,6470,4320,7130,590,1810,158},
     {32762,32751,32675,32761,32756,32755,32759,32570,32746,32749,32763,32760,32640,32748,32440,32753},730,170,510,4,5},
    {{59,280,7820,6590,0,172,78,1640,390,257,8470,5930,10100,745,2380,212},
     {32754,32756,32635,32751,32749,32743,32752,32450,32750,32738,32758,32751,32550,32732,32280,32741},1100,245,230,7,7},
    {{37,122,2370,1930,0,79,53,640,365,127,2730,1570,3690,310,980,103},
     {32763,32758,32710,32763,32759,32760,32762,32640,32754,32755,32764,32762,32700,32758,32590,32758},1500,370,710,6,8},
    {{67,245,5630,6090,0,197,89,1890,330,294,6830,4580,11100,820,2760,246},
     {32751,32742,32648,32747,32743,32748,32747,32430,32734,32743,32753,32749,32500,32752,32200,32746},460,100,370,2,3}
};
#define DRUM_RECIP(d) (UINT32_MAX/(d)+1u)
/* Noise, highpass and click divisors, in kit order. */
static const uint32_t drum_reciprocal[MC_KITS][3]={
    {DRUM_RECIP(4),DRUM_RECIP(4),DRUM_RECIP(260)},
    {DRUM_RECIP(6),DRUM_RECIP(6),DRUM_RECIP(420)},
    {DRUM_RECIP(5),DRUM_RECIP(3),DRUM_RECIP(160)},
    {DRUM_RECIP(3),DRUM_RECIP(2),DRUM_RECIP(110)},
    {DRUM_RECIP(5),DRUM_RECIP(5),DRUM_RECIP(340)},
    {DRUM_RECIP(8),DRUM_RECIP(7),DRUM_RECIP(490)},
    {DRUM_RECIP(7),DRUM_RECIP(8),DRUM_RECIP(740)},
    {DRUM_RECIP(3),DRUM_RECIP(3),DRUM_RECIP(200)}
};
#undef DRUM_RECIP
#if defined(MC_PERF_PROBE) && MC_PERF_PROBE == 1
uint8_t mc_probe_skip;
#endif
static int32_t clamp(int32_t x,int32_t lo,int32_t hi) { return x<lo?lo:x>hi?hi:x; }
static void zero(void *p,unsigned n) { uint8_t *b=p; while(n--) *b++=0; }
static int32_t mul(int32_t x,int32_t y) {
    int32_t product=x*y;
    return (product+(product<0?32767:0))>>15;
}
static uint32_t random32(McEngine *e) {
    uint32_t r=e->random; r^=r<<13; r^=r>>17; r^=r<<5; return e->random=r;
}
void mc_init(McEngine *e) {
    zero(e,sizeof *e); mc_factory(&e->project); e->random=0x4d43464d;
    e->queued=255;e->master=MC_MASTER_DEFAULT;e->limiter_gain=1u<<24;
    for(unsigned p=0;p<3;p++) e->arp[p].random=0x61727031u+p;
}
void mc_project_defaults(McProject *p) {
    zero(p->arp,sizeof p->arp); zero(p->fx_type,sizeof p->fx_type);
    zero(p->track_fx_type,sizeof p->track_fx_type); zero(p->filter,sizeof p->filter);
    zero(p->track_filter,sizeof p->track_filter); zero(p->automation,sizeof p->automation);
    zero(p->fx_send,sizeof p->fx_send);zero(p->fx_bus_type,sizeof p->fx_bus_type);
    p->automation_count=0;p->metronome=0;
    for(unsigned i=0;i<MC_SONG_ENTRIES;i++) { p->song[i].pattern=0;p->song[i].repeats=1; }
    p->song_length=0;p->song_loop=0;p->song_mode=0;p->clock_source=0;
    for(unsigned i=0;i<4;i++) p->midi_channel[i]=(uint8_t)(i==3?9:i);
    for(unsigned t=0;t<3;t++) {
        p->arp[t].rate=2; p->arp[t].octaves=1; p->arp[t].gate=75; p->preset[t]=255;
        for(unsigned n=0;n<MC_PRESETS;n++) {
            unsigned same=1;
            for(unsigned i=0;i<sizeof(McPatch);i++)
                if(((const uint8_t *)&p->patch[t])[i]!=((const uint8_t *)&mc_presets[n])[i]) same=0;
            if(same) { p->preset[t]=(uint8_t)n; break; }
        }
    }
}
void mc_off(McEngine *e,unsigned part,unsigned note) {
    for(unsigned i=0;i<MC_VOICES;i++) {
        McVoice *v=&e->voice[i];
        if(v->active&&v->part==part&&v->note==note) {
            if(v->midi_out) mc_midi_release(e,part,note);
            v->midi_out=0;v->stage=2;
        }
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
        if(v->midi_out) mc_midi_release(e,part,v->note);
        mc_midi_note(e,part,note,velocity);v->midi_out=(uint8_t)!e->midi.receiving;
        v->target=mc_frequency[note]; v->note=(uint8_t)note;
        v->gate_left=gate; v->patch=*p; v->accent=(flags&MC_ACCENT)!=0;
        return; /* legato preserves oscillator, filter and amplitude envelope */
    }
    if(!v) for(unsigned i=0;i<MC_VOICES;i++) if(!e->voice[i].active) { v=&e->voice[i]; break; }
    if(!v) {
        for(unsigned i=1;i<MC_VOICES;i++) if(e->voice[i].age>e->voice[oldest].age) oldest=i;
        v=&e->voice[oldest]; e->steals++;
    }
    if(v->active&&v->midi_out) mc_midi_release(e,v->part,v->note);
    mc_midi_note(e,part,note,velocity);
    zero(v,sizeof *v); v->midi_out=(uint8_t)!e->midi.receiving;v->active=1; v->part=(uint8_t)part; v->note=(uint8_t)note;
    v->velocity=(uint8_t)velocity; v->accent=(flags&MC_ACCENT)!=0;
    v->patch=*p; v->increment=v->target=mc_frequency[note];
    if(p->engine==MC_LA&&p->pcm>=MC_PCM_ATTACKS) {
        v->sample_step=mc_recorded[p->pcm-MC_PCM_ATTACKS].step[note];
        while(v->sample_step>65536&&v->sample_level<6) { v->sample_step=(v->sample_step+1)/2;v->sample_level++; }
    }
    v->phase2=0x31000000u; v->gate_left=gate;
    /* Attack 1..~920 ms; decay/release 20 ms..~2.3 s. */
    v->attack_step=32767/(1+p->attack*p->attack*MC_RATE/18000);
    if(!v->attack_step) v->attack_step=1;
    v->decay_step=1+32767/(880+p->decay*p->decay*6);
    v->release_step=1+32767/(440+p->release*p->release*6);
}
void mc_drum(McEngine *e,unsigned sound,unsigned velocity) {
    if(sound>=MC_DRUMS||!velocity||velocity>127||(e->mute&8)) return;
    mc_midi_note(e,3,mc_midi_drums[sound],velocity);mc_midi_note(e,3,mc_midi_drums[sound],0);
    McDrum *d=0; unsigned oldest=0;
    for(unsigned i=0;i<MC_DRUM_VOICES;i++) {
        if(sound==2&&e->drum[i].sound==3) e->drum[i].env=0; /* closed hat chokes open */
        if(e->drum[i].env&&e->drum[i].sound==sound) d=&e->drum[i];
        if(e->drum[i].age>e->drum[oldest].age) oldest=i;
    }
    if(!d) for(unsigned i=0;i<MC_DRUM_VOICES;i++) if(!e->drum[i].env) { d=&e->drum[i]; break; }
    if(!d) d=&e->drum[oldest];
    zero(d,sizeof *d); d->env=32767; d->velocity=(uint8_t)velocity; d->sound=(uint8_t)sound;
    d->kit=e->project.drum_kit;
}
static uint32_t step_samples(const McEngine *e) {
    unsigned swing=e->step&1?100-e->project.swing:100+e->project.swing;
    return (MC_RATE*15u* swing)/(e->project.bpm*100u);
}
void mc_arp_clear(McEngine *e,unsigned part) {
    if(part>=3) return;
    zero(e->live_keys[part],sizeof e->live_keys[part]);e->key_slide[part]=0;
    McArpState *a=&e->arp[part];
    for(unsigned i=0;i<3;i++) if(a->sounding[i]) mc_off(e,part,a->sounding[i]);
    for(unsigned i=0;i<a->count;i++) mc_off(e,part,a->notes[i]);
    uint32_t random=a->random;
    zero(a,sizeof *a); a->random=random?random:0x61727031u+part;
}
void mc_key(McEngine *e,unsigned part,unsigned note,unsigned velocity,unsigned on) {
    if(part>=3||note<12||note>108||velocity>127||(on&&!velocity)) return;
    e->key_slide[part]=0;
    McArpState *a=&e->arp[part];
    const McArp *settings=&e->project.arp[part];
    if(!settings->enabled) {
        if(a->count) mc_arp_clear(e,part);
        if(e->project.patch[part].engine==MC_ACID) {
            uint8_t *keys=e->live_keys[part];
            unsigned index=note/8,bit=1u<<(note%8),was=keys[index]&bit;
            if(on) {
                unsigned held=0,slide=0;
                for(unsigned i=0;i<16;i++)held|=keys[i];
                if(held&&!was&&e->project.patch[part].glide)
                    for(unsigned i=0;i<MC_VOICES;i++)
                        if(e->voice[i].active&&e->voice[i].part==part&&e->voice[i].stage!=2)
                            slide=MC_SLIDE;
                keys[index]|=(uint8_t)bit;
                mc_note(e,part,note,velocity,slide,0);
                e->key_slide[part]=(uint8_t)(slide!=0);
            } else {
                keys[index]&=(uint8_t)~bit;
                if(was)for(unsigned i=0;i<MC_VOICES;i++) {
                    McVoice *v=&e->voice[i];
                    if(v->active&&v->part==part&&v->note==note)
                        for(unsigned n=108;n>=12;n--)if(keys[n/8]&(1u<<(n%8))) {
                            mc_note(e,part,n,v->velocity,e->project.patch[part].glide?MC_SLIDE:0,0);
                            return;
                        }
                }
                mc_off(e,part,note);
            }
        } else {
            zero(e->live_keys[part],sizeof e->live_keys[part]);
            if(on) mc_note(e,part,note,velocity,0,0); else mc_off(e,part,note);
        }
        return;
    }
    if(on&&settings->latch) {
        unsigned held=0;
        for(unsigned i=0;i<a->count;i++) held+=a->held[i];
        if(!held) mc_arp_clear(e,part);
    }
    unsigned i=0; while(i<a->count&&a->notes[i]!=note) i++;
    if(on) {
        if(i==MC_ARP_KEYS) return;
        if(i==a->count) a->count++;
        a->notes[i]=(uint8_t)note; a->velocity[i]=(uint8_t)velocity; a->held[i]=1;
    } else if(i<a->count) {
        a->held[i]=0;
        if(!settings->latch) {
            for(unsigned j=i+1;j<a->count;j++) {
                a->notes[j-1]=a->notes[j];a->velocity[j-1]=a->velocity[j];a->held[j-1]=a->held[j];
            }
            a->count--;
            if(!a->count) mc_arp_clear(e,part);
        }
    }
}
static void arp_trigger(McEngine *e,unsigned part,uint32_t interval) {
    McArpState *a=&e->arp[part]; const McArp *s=&e->project.arp[part];
    if(!s->enabled||!a->count) return;
    uint8_t notes[MC_ARP_KEYS*3],velocities[MC_ARP_KEYS*3]; unsigned count=0;
    for(unsigned octave=0;octave<s->octaves;octave++) for(unsigned i=0;i<a->count;i++) {
        unsigned note=a->notes[i]+octave*12;
        if(note<=108) { notes[count]=(uint8_t)note;velocities[count++]=a->velocity[i]; }
    }
    if(s->mode!=MC_ARP_ORDER) for(unsigned i=1;i<count;i++) {
        unsigned j=i;uint8_t note=notes[i],velocity=velocities[i];
        while(j&&notes[j-1]>note) { notes[j]=notes[j-1];velocities[j]=velocities[j-1];j--; }
        notes[j]=note;velocities[j]=velocity;
    }
    unsigned index=a->position%count;
    if(s->mode==MC_ARP_DOWN) index=count-1-index;
    else if(s->mode==MC_ARP_UPDOWN&&count>1) {
        unsigned pos=a->position%(count*2-2);index=pos<count?pos:count*2-2-pos;
    } else if(s->mode==MC_ARP_RANDOM) {
        uint32_t r=a->random;r^=r<<13;r^=r>>17;r^=r<<5;a->random=r;index=r%count;
    }
    for(unsigned i=0;i<3;i++) { if(a->sounding[i]) mc_off(e,part,a->sounding[i]);a->sounding[i]=0; }
    unsigned voices=s->mode==MC_ARP_CHORD?(count<3?count:3):1;
    unsigned velocity=velocities[s->mode==MC_ARP_CHORD?0:index];
    uint32_t gate=interval*s->gate/100;
    a->gate_remaining=(uint16_t)(s->gate*(4u>>s->rate));
    if(e->playing) gate=a->gate_remaining>100?step_samples(e)+2:step_samples(e)*a->gate_remaining/100;
    for(unsigned i=0;i<voices;i++) {
        unsigned n=s->mode==MC_ARP_CHORD?i:index;
        a->sounding[i]=notes[n];mc_note(e,part,notes[n],velocity,0,gate);
    }
    if(e->playing&&e->recording) {
        unsigned divisor=4u>>s->rate,remaining=s->gate*divisor;
        for(unsigned offset=0;offset<divisor;offset++) {
            McStep *step=&e->project.step[e->pattern][part][e->step+offset];
            zero(step,sizeof *step);
            if(remaining) {
                for(unsigned i=0;i<3;i++) step->note[i]=a->sounding[i];
                step->velocity=(uint8_t)velocity;step->gate=(uint8_t)(remaining>100?100:remaining);
                step->flags=offset?MC_TIE:0;remaining-=step->gate;
            }
        }
    }
    unsigned cycle=s->mode==MC_ARP_UPDOWN&&count>1?count*2-2:count;
    a->position=(uint8_t)((a->position+1)%cycle);a->events++;
}
static uint8_t *parameter(McProject *p,unsigned part,unsigned param) {
    if(param==MC_PARAM_RD_TYPE||param==MC_PARAM_CF_TYPE)
        return part<=MC_PARTS?&p->fx_bus_type[param-MC_PARAM_RD_TYPE]:0;
    if(part>=MC_PARTS||param>=MC_PARAMETERS) return 0;
    if(param==MC_PARAM_RD_SEND||param==MC_PARAM_CF_SEND) return &p->fx_send[part][param-MC_PARAM_RD_SEND];
    if(param>=MC_PARAM_DRUM_LEVEL) return part==3?&p->drum_level[param-MC_PARAM_DRUM_LEVEL]:0;
    if(param<12) return part<3?((uint8_t *)&p->patch[part])+param:0;
    if(param==MC_PARAM_LEVEL) return &p->level[part];
    if(param==MC_PARAM_PAN) return &p->pan[part];
    return 0;
}
int mc_parameter_get(const McEngine *e,unsigned part,unsigned param) {
    const uint8_t *v=parameter((McProject *)&e->project,part,param);return v?*v:-1;
}
static int parameter_apply(McEngine *e,unsigned part,unsigned param,unsigned value) {
    uint8_t *v=parameter(&e->project,part,param);
    unsigned maximum=param==0?1:param==1?2:param==2?MC_PCM_SOURCES-1:
        param==MC_PARAM_RD_TYPE||param==MC_PARAM_CF_TYPE?7:127;
    if(!v||value>maximum) return -1;
    if(part<3&&((param==0&&value==MC_ACID&&e->project.patch[part].pcm>=MC_PCM_ATTACKS)||
       (param==2&&value>=MC_PCM_ATTACKS&&e->project.patch[part].engine==MC_ACID))) return -1;
    *v=(uint8_t)value;
    if(part<3&&param<12) for(unsigned i=0;i<MC_VOICES;i++) if(e->voice[i].part==part) {
        McVoice *voice=&e->voice[i]; const McPatch *patch=&e->project.patch[part];
        voice->patch.cutoff=patch->cutoff;voice->patch.resonance=patch->resonance;voice->patch.pcm_mix=patch->pcm_mix;
        voice->decay_step=1+32767/(880+patch->decay*patch->decay*6);
    }
    return 0;
}
int mc_parameter_set(McEngine *e,unsigned part,unsigned param,unsigned value) {
    if(part<=MC_PARTS&&(param==MC_PARAM_RD_TYPE||param==MC_PARAM_CF_TYPE))part=MC_PARTS;
    if(parameter_apply(e,part,param,value)) return -1;
    mc_midi_parameter(e,part,param,value);
    if(!e->playing||!e->recording) return 0;
    unsigned address=e->pattern*5+part,index=0;
    for(;index<e->project.automation_count;index++) {
        McAutomation *a=&e->project.automation[index];
        if(a->address==address&&a->step==e->step&&a->parameter==param) break;
    }
    if(index==MC_AUTOMATION) { e->automation_full=1;return 1; }
    if(index==e->project.automation_count) e->project.automation_count++;
    McAutomation *a=&e->project.automation[index];
    a->address=(uint16_t)address;a->step=e->step;a->parameter=(uint8_t)param;a->value=(uint8_t)value;
    return 0;
}
void mc_automation_clear(McEngine *e,unsigned pattern) {
    unsigned count=0;
    for(unsigned i=0;i<e->project.automation_count;i++)
        if(pattern<MC_PATTERNS&&e->project.automation[i].address/5!=pattern)
            e->project.automation[count++]=e->project.automation[i];
    zero(e->project.automation+count,(MC_AUTOMATION-count)*sizeof(McAutomation));
    e->project.automation_count=(uint16_t)count;e->automation_full=0;
}
static unsigned following_pattern(const McEngine *e) {
    if(!e->project.song_mode) return e->queued<MC_PATTERNS?e->queued:e->pattern;
    unsigned position=e->song_position;
    if(e->song_repeat+1u<e->project.song[position].repeats) return e->project.song[position].pattern;
    if(++position>=e->project.song_length) {
        if(!e->project.song_loop) return MC_PATTERNS;
        position=0;
    }
    return e->project.song[position].pattern;
}
static void trigger(McEngine *e) {
    uint32_t samples=step_samples(e);
    if(e->playing&&e->recording&&e->project.metronome&&e->step%4==0) {
        e->metro_phase=0;e->metro_left=MC_RATE/50;e->metro_accent=e->step==0;
    }
    for(unsigned i=0;i<e->project.automation_count;i++) {
        const McAutomation *a=&e->project.automation[i];
        if(a->address/5==e->pattern&&a->step==e->step) {
            parameter_apply(e,a->address%5,a->parameter,a->value);
            mc_midi_parameter(e,a->address%5,a->parameter,a->value);
        }
    }
    for(unsigned p=0;p<4;p++) {
        if(p<3&&e->project.arp[p].enabled&&e->arp[p].count) {
            unsigned divisor=4u>>e->project.arp[p].rate;
            if(e->step%divisor==0) arp_trigger(e,p,MC_RATE*15u*divisor/e->project.bpm);
            else {
                McArpState *a=&e->arp[p];
                a->gate_remaining=(uint16_t)(a->gate_remaining>100?a->gate_remaining-100:0);
                uint32_t gate=a->gate_remaining>100?samples+2:samples*a->gate_remaining/100;
                if(gate) for(unsigned i=0;i<MC_VOICES;i++)
                    if(e->voice[i].active&&e->voice[i].stage!=2&&e->voice[i].part==p)
                        for(unsigned n=0;n<3;n++) if(e->voice[i].note==a->sounding[n]) e->voice[i].gate_left=gate;
            }
            continue;
        }
        const McStep *s=&e->project.step[e->pattern][p][e->step];
        if(p==3) {
            for(unsigned d=0;d<MC_DRUMS;d++) if(s->drums&(1u<<d)) mc_drum(e,d,s->velocity);
        } else if(s->velocity) {
            /* Slide belongs to the destination step. Keep the source gate alive to it. */
            unsigned next=(e->step+1)%16;
            unsigned nextpat=next==0?following_pattern(e):e->pattern;
            const McStep *n=nextpat<MC_PATTERNS?&e->project.step[nextpat][p][next]:0;
            uint32_t gate=(n&&n->velocity&&(n->flags&(MC_SLIDE|MC_TIE)))?samples+2:samples*s->gate/100;
            for(unsigned c=0;c<3;c++) if(s->note[c]) {
                unsigned tied=0;
                if(s->flags&MC_TIE) for(unsigned v=0;v<MC_VOICES;v++)
                    if(e->voice[v].active&&e->voice[v].stage!=2&&e->voice[v].part==p&&e->voice[v].note==s->note[c]) {
                        e->voice[v].gate_left=gate;tied=1;
                    }
                if(!tied) mc_note(e,p,s->note[c],s->velocity,s->flags,gate);
            }
        }
    }
    e->events++;
}
void mc_start(McEngine *e) {
    if(e->project.song_mode&&!e->project.song_length) return;
    e->song_position=0;e->song_repeat=0;e->song_finished=0;e->queued=255;e->midi.ticks=0;e->midi.clock=0;
    if(e->project.song_mode) e->pattern=e->project.song[0].pattern;
    mc_midi_send(e,0xfa,0,0,1);
    e->playing=1;
    if(e->project.demo&&!e->project.song_mode) { e->demo_frame=0;e->pattern=0;e->queued=255;e->events=0;mc_demo_tick(e); }
    for(unsigned p=0;p<3;p++) { e->arp[p].position=0;e->arp[p].clock=0; }
    e->playing=1;e->step=0;e->clock=0;
    uint8_t receiving=e->midi.receiving;e->midi.receiving=0;trigger(e);e->midi.receiving=receiving;
}
void mc_stop(McEngine *e) {
    mc_midi_send(e,0xfc,0,0,1);
    uint8_t receiving=e->midi.receiving;e->midi.receiving=0;
    for(unsigned p=0;p<3;p++) mc_midi_all_off(e,p);
    mc_midi_send(e,0xb0u+e->project.midi_channel[3],123,0,3);
    e->midi.sustain[3]=0;
    for(unsigned i=0;i<16;i++) e->midi.held[3][i]=e->midi.deferred[3][i]=0;
    e->midi.receiving=receiving;
    e->playing=0; e->queued=255;
    e->metro_left=0;
    for(unsigned p=0;p<3;p++) mc_arp_clear(e,p);
    for(unsigned i=0;i<MC_VOICES;i++) e->voice[i].stage=2;
}
void mc_continue(McEngine *e) {
    if(e->playing||(e->project.song_mode&&(!e->project.song_length||e->song_finished))) return;
    e->playing=1;mc_midi_send(e,0xfb,0,0,1);
}
int mc_song_set(McEngine *e,unsigned index,unsigned pattern,unsigned repeats) {
    if(index>=MC_SONG_ENTRIES||pattern>=MC_PATTERNS||!repeats||repeats>16) return -1;
    e->project.song[index].pattern=(uint8_t)pattern;e->project.song[index].repeats=(uint8_t)repeats;return 0;
}
int mc_song_length(McEngine *e,unsigned length) {
    if(length>MC_SONG_ENTRIES) return -1;
    if(e->project.song_mode&&e->song_position>=length) mc_stop(e);
    e->project.song_length=(uint8_t)length;return 0;
}
void mc_song_mode(McEngine *e,unsigned mode) {
    if(mode>1||mode==e->project.song_mode) return;
    mc_stop(e);e->project.song_mode=(uint8_t)mode;e->song_position=0;e->song_repeat=0;e->song_finished=0;
}
void mc_clock_source(McEngine *e,unsigned source) {
    if(source>3||source==e->project.clock_source) return;
    mc_stop(e);e->project.clock_source=(uint8_t)source;e->clock=0;e->midi.ticks=0;e->midi.have_tick=0;
}
static void advance(McEngine *e) {
    e->step=(uint8_t)((e->step+1)%16);
    if(!e->step) {
        if(e->project.song_mode) {
            if(!e->project.song_length) { mc_stop(e);return; }
            if(++e->song_repeat>=e->project.song[e->song_position].repeats) {
                e->song_repeat=0;
                if(++e->song_position>=e->project.song_length) {
                    e->song_position=0;
                    if(!e->project.song_loop) { e->song_finished=1;mc_stop(e);return; }
                }
            }
            e->pattern=e->project.song[e->song_position].pattern;
        } else if(e->queued<MC_PATTERNS) { e->pattern=e->queued;e->queued=255; }
    }
    trigger(e);
}
void mc_clock_tick(McEngine *e) {
    if(!e->playing||!e->project.clock_source) return;
    unsigned swing=e->step&1?100-e->project.swing:100+e->project.swing;
    e->midi.ticks+=100;
    if(e->midi.ticks>=6*swing) { e->midi.ticks-=6*swing;advance(e); }
}
void mc_queue(McEngine *e,unsigned p) {
    if(p>=MC_PATTERNS||e->project.song_mode) return;
    if(e->playing) e->queued=(uint8_t)p; else { e->pattern=(uint8_t)p; e->queued=255; }
}
void mc_mute(McEngine *e,unsigned p) {
    if(p>=4) return;
    e->mute^=(uint8_t)(1u<<p);
    if((e->mute>>p)&1) {
        mc_midi_all_off(e,p);
        for(unsigned i=0;i<MC_VOICES;i++) if(e->voice[i].part==p) e->voice[i].active=0;
        if(p==3) for(unsigned i=0;i<MC_DRUM_VOICES;i++) e->drum[i].env=0;
    }
}
void mc_preset(McEngine *e,unsigned part,unsigned preset) {
    if(part>=3||preset>=MC_PRESETS) return;
    mc_midi_all_off(e,part);mc_midi_send(e,0xc0u+e->project.midi_channel[part],preset,0,2);
    for(unsigned i=0;i<MC_VOICES;i++) if(e->voice[i].part==part) e->voice[i].stage=2;
    e->project.patch[part]=mc_presets[preset];
    e->project.preset[part]=(uint8_t)preset;
}
void mc_control(McEngine *e,unsigned p,unsigned knob,int delta) {
    if(p>=3||knob>=4) return;
    unsigned param=knob==0?4:knob==1?5:knob==2?7:3;
    if(e->project.demo&&e->playing) for(unsigned i=0;i<e->project.automation_count;i++) {
        const McAutomation *a=&e->project.automation[i];
        if(a->address==e->pattern*5+p&&a->parameter==param) return;
    }
    McPatch *s=&e->project.patch[p];
    uint8_t *v=knob==0?&s->cutoff:knob==1?&s->resonance:knob==2?&s->decay:&s->pcm_mix;
    *v=(uint8_t)clamp(*v+delta,0,127);
    for(unsigned i=0;i<MC_VOICES;i++) if(e->voice[i].part==p) {
        e->voice[i].patch.cutoff=s->cutoff; e->voice[i].patch.resonance=s->resonance;
        e->voice[i].patch.pcm_mix=s->pcm_mix;
        e->voice[i].decay_step=1+32767/(880+s->decay*s->decay*6);
    }
}
static int32_t oscillator(uint32_t phase,unsigned wave,const int16_t *table) {
    if(wave==2) return table[phase>>22];
    unsigned i=phase>>24, frac=(phase>>16)&255;
    return table[i]+mc_div_pow2((table[(i+1)&255]-table[i])*(int32_t)frac,8);
}
/* 미스 때만 level->data[base..]에서 최대 128샘플을 복사한다. 끝 창은 length까지만. */
static void pcm_fill(McPcmCache *c,const McRecordedLevel *level,unsigned id,unsigned mip,unsigned base) {
    unsigned n=level->length-base;
    if(n>MC_PCM_WINDOW) n=MC_PCM_WINDOW;
    for(unsigned i=0;i<n;i++) c->data[i]=level->data[base+i];
    c->window=(uint16_t)base;c->count=(uint16_t)n;c->pcm=(uint16_t)id;c->level=(uint16_t)mip;
}
static inline __attribute__((always_inline)) int32_t recorded(McPcmCache *c,McVoice *v,uint32_t ratio,const McRecordedLevel *level) {
    unsigned id=v->patch.pcm-MC_PCM_ATTACKS,mip=v->sample_level;
    unsigned pos=v->pcm_pos>>16;
    if(pos>=level->length) return 0;
    unsigned next=pos+1;
    if(level->loop_end&&next>=level->loop_end) next=level->loop_start;
    else if(next>=level->length)next=level->length-1;
    unsigned offset=pos-c->window;
    if(c->pcm!=id||c->level!=mip||offset>=c->count||
        (next==pos+1&&offset+1u>=c->count)) {
        pcm_fill(c,level,id,mip,pos);
        offset=0;
    }
    int32_t sample=c->data[offset];
    /* 루프 되감김 next는 창 밖일 수 있어 그때만 직접 읽는다. */
    unsigned after_offset=next-c->window;
    int32_t after=after_offset<c->count?c->data[after_offset]:level->data[next];
    sample+=mc_div_pow2((after-sample)*(int32_t)(v->pcm_pos&65535),16);
    v->pcm_pos+=(uint32_t)(((uint64_t)v->sample_step*ratio)>>16);
    if(level->loop_end&&v->pcm_pos>=((uint32_t)level->loop_end<<16))
        v->pcm_pos-=((uint32_t)(level->loop_end-level->loop_start)<<16);
    return sample;
}
typedef struct {
    const int16_t *table;
    McRecordedLevel pcm;
    uint32_t ratio,increment,second;
    int32_t cutoff,sustain;
    uint8_t fixed_pitch,refresh;
} McVoiceRender;
static void prepare_voice(const McEngine *e,const McVoice *v,unsigned index,uint32_t ratio,McVoiceRender *r) {
    const McPatch *p=&v->patch;
    r->ratio=ratio;r->refresh=1;
    r->cutoff=250+mc_div_pow2(p->cutoff*p->cutoff,1);
    if(p->engine!=MC_ACID)r->cutoff=clamp(r->cutoff,250,14000);
    r->sustain=p->sustain*258;
    r->table=mc_sine;
    if(p->wave!=2) {
        unsigned note=v->note,band=note>=96?6:note<36?0:(((note-36)*171u)>>11)+1;
        r->table=p->wave?mc_square[band]:mc_saw[band];
    }
    if(p->engine==MC_LA&&p->pcm>=MC_PCM_ATTACKS)
        r->pcm=mc_recorded[p->pcm-MC_PCM_ATTACKS].levels[v->sample_level];
    r->fixed_pitch=(uint8_t)(p->engine==MC_LA&&v->increment==v->target);
    if(r->fixed_pitch) {
        const McPitchCache *pitch=&e->pitch_cache[index];
        if(pitch->base==v->increment&&pitch->ratio==ratio) {
            r->increment=pitch->increment;r->second=pitch->second;
        } else {
            r->increment=(uint32_t)(((uint64_t)v->increment*ratio)>>16);
            r->second=r->increment+r->increment/401;
        }
    }
}
static __attribute__((noinline)) int32_t melodic(McEngine *e,McVoice *v,unsigned index,McVoiceRender *r) {
    McPatch *p=&v->patch;
    if(v->gate_left&&!--v->gate_left) {
        if(v->midi_out) mc_midi_release(e,v->part,v->note);
        v->midi_out=0;v->stage=2;
    }
    if(v->stage==0) {
        v->envelope+=v->attack_step;
        if(v->envelope>=32767) { v->envelope=32767; v->stage=1; }
    } else if(v->stage==1) {
        int32_t sustain=r->sustain;
        if(v->envelope>sustain) v->envelope=clamp(v->envelope-v->decay_step,sustain,32767);
    } else {
        v->envelope-=v->release_step;
        if(v->envelope<=0) { v->active=0; return 0; }
    }
    unsigned moving=v->increment!=v->target;
    if(moving) {
        int32_t diff=(int32_t)v->target-(int32_t)v->increment;
        int32_t step=diff/(20+p->glide*16);
        if(!step) step=diff>0?1:-1;
        v->increment=(uint32_t)((int32_t)v->increment+step);
    }
    McPitchCache *pitch=&e->pitch_cache[index];
    uint32_t increment,second;
    uint32_t ratio=r->ratio;
    if(r->fixed_pitch) {
        increment=r->increment;second=r->second;
        if(r->refresh) {
            *pitch=(McPitchCache){v->increment,ratio,increment,second};
            r->refresh=0;
        }
    } else if(!moving&&pitch->base==v->increment&&pitch->ratio==ratio) {
        increment=pitch->increment;second=pitch->second;
    } else {
        increment=(uint32_t)(((uint64_t)v->increment*ratio)>>16);
        second=increment+increment/401;
        if(!moving)*pitch=(McPitchCache){v->increment,ratio,increment,second};
    }
    v->phase+=increment;v->phase2+=second;
    int32_t sample;
    if(p->engine==MC_LA&&p->pcm>=MC_PCM_ATTACKS&&p->pcm_mix==127) {
        sample=recorded(&e->pcm_cache[index],v,ratio,&r->pcm);
    } else {
        sample=oscillator(v->phase,p->wave,r->table);
        if(p->engine==MC_LA) {
            sample=mc_div_pow2(sample+oscillator(v->phase2,p->wave,r->table),1);
            unsigned pos=v->pcm_pos>>16;
            if(p->pcm>=MC_PCM_ATTACKS) {
                int32_t pcm=recorded(&e->pcm_cache[index],v,ratio,&r->pcm);
                sample=mc_div127_signal(sample*(127-p->pcm_mix)+pcm*p->pcm_mix);
            } else if(pos<511) {
                int32_t pcm=mc_attack[p->pcm][pos];
                pcm+=mc_div_pow2((mc_attack[p->pcm][pos+1]-pcm)*(int32_t)(v->pcm_pos&65535),16);
                int32_t weight=mc_div_pow2(p->pcm_mix*(512-(int32_t)pos),9);
                sample=mc_div127_signal(sample*(127-weight))+mc_div127_signal(pcm*weight);
                v->pcm_pos+=(uint32_t)(((uint64_t)mc_pcm_rate[v->note]*ratio)>>16);
            }
        }
    }
    /* Bounded four-stage feedback lowpass. Original approximation, not circuit emulation. */
    int32_t cutoff=r->cutoff;
    if(p->engine==MC_ACID) {
        cutoff+=mul(v->envelope,3500+v->accent*4500);
        cutoff=clamp(cutoff,250,14000);
    }
    unsigned stages=p->engine==MC_ACID?4u:2u;
    int32_t x=clamp(sample-mc_div127_signal(v->filter[stages-1]*(p->resonance*3)),-24000,24000);
    v->filter[0]+=mul(x-v->filter[0],cutoff);x=v->filter[0];
    v->filter[1]+=mul(x-v->filter[1],cutoff);x=v->filter[1];
    if(stages==4) {
        v->filter[2]+=mul(x-v->filter[2],cutoff);x=v->filter[2];
        v->filter[3]+=mul(x-v->filter[3],cutoff);x=v->filter[3];
    }
    v->age++;
    x=mul(x,v->envelope); x=mc_div127_signal(x*v->velocity);
    x=mc_div127_signal(x*p->level);
    if(v->accent) x=mc_div_pow2(x*5,2);
    return x;
}
static int32_t percussion(McEngine *e,McDrum *d) {
    if(!d->env) return 0;
    unsigned sound=d->sound;
    const DrumKit *kit=&kits[d->kit];
    int32_t noise=(int32_t)(random32(e)>>17)-16384, s=0;
    /* BD, SN, CH, OH, CP, LT, B2, RS, S2, HT, CR, RD, SH, CB, CL, MT. */
    if(sound==0||sound==6||sound==5||sound==9||sound==15) {
        unsigned hz=kit->hz[sound];
        uint32_t sweep=(uint32_t)mc_div_pow2(d->env*d->env,15);
        d->phase+=hz*97392u+sweep*(sound<7?kit->sweep:(unsigned)kit->sweep*5/11);
        s=mc_sine[d->phase>>22];
        if(d->age<kit->click) s+=mc_div_recip(noise*((int)kit->click-(int)d->age),drum_reciprocal[d->kit][2]);
    } else if(sound==1||sound==8) {
        d->phase+=kit->hz[sound]*97392u;
        s=mc_div_recip(noise*kit->noise_weight+mc_sine[d->phase>>22],drum_reciprocal[d->kit][0]);
    } else if(sound==4) {
        s=noise;
        if(d->age<(unsigned)kit->clap*3&&d->age%kit->clap>kit->clap/2) s=mc_div_pow2(s,2);
    } else if(sound==7||sound==14) {
        d->phase+=kit->hz[sound]*97392u;
        s=mc_div_pow2(noise+mc_sine[d->phase>>22],1);
    } else if(sound==13) {
        d->phase+=kit->hz[sound]*97392u;
        s=((d->phase&0x80000000u)?7500:-7500)+(((d->phase*3/2)&0x80000000u)?7000:-7000);
    } else {
        d->noise_lp+=mc_div_recip(noise-d->noise_lp,drum_reciprocal[d->kit][1]);
        d->phase+=kit->hz[sound]*97392u;
        s=mc_div_pow2(noise-d->noise_lp,1)+((d->phase&0x80000000u)?3500:-3500);
    }
    s=mc_div127_signal(mul(s,d->env)*d->velocity);
    s=mc_div127_signal(s*e->project.drum_level[sound]);
    d->env=mul(d->env,kit->decay[sound]); d->age++;
    if(d->env<16) d->env=0;
    return s;
}
static uint32_t limiter_target(int64_t peak) {
    uint32_t ceiling=MC_OUTPUT_CEILING;
    while(peak>0x7fffffff) { peak=(peak+1)>>1;ceiling>>=1; }
    uint32_t divisor=(uint32_t)peak,numerator=ceiling<<16;
    uint32_t gain=numerator/divisor,remainder=numerator%divisor;
    for(unsigned bit=0;bit<8;bit++) {
        gain<<=1;remainder<<=1;
        if(remainder>=divisor) { gain++;remainder-=divisor; }
    }
    return gain;
}
void mc_output(McEngine *e,int32_t *left,int32_t *right) {
    const uint32_t unity=1u<<24;
    uint32_t master=e->master>MC_MASTER_MAX?MC_MASTER_MAX:e->master;
    int64_t l=(int64_t)*left*master,r=(int64_t)*right*master;
    l=(l+(l<0?32767:0))>>15;r=(r+(r<0?32767:0))>>15;
    int64_t al=l<0?-l:l,ar=r<0?-r:r,peak=al>ar?al:ar;
    uint32_t needed=peak>MC_OUTPUT_CEILING?limiter_target(peak):unity;
    if(needed<unity)e->limiter_hits++;
    if(needed<e->limiter_gain)e->limiter_gain=needed;
    else if(e->limiter_gain<unity) {
        e->limiter_gain+=(unity-e->limiter_gain+2204)/2205;
        if(e->limiter_gain>needed)e->limiter_gain=needed;
    }
    if(e->limiter_gain!=unity) {
        l*=e->limiter_gain;r*=e->limiter_gain;
        l=(l+(l<0?16777215:0))>>24;r=(r+(r<0?16777215:0))>>24;
    }
    if(l>32767||l< -32767||r>32767||r< -32767)e->clips++;
    al=l<0?-l:l;ar=r<0?-r:r;
    if((uint32_t)al>e->peak)e->peak=(uint32_t)al;
    if((uint32_t)ar>e->peak)e->peak=(uint32_t)ar;
    *left=clamp((int32_t)l,-32767,32767);*right=clamp((int32_t)r,-32767,32767);
}
void mc_render(McEngine *e,int16_t *stereo,unsigned frames) {
    /* MIDI/tempo input precedes rendering; sequencing does not change BPM or pitch bend. */
    uint32_t ratio[3];
    for(unsigned part=0;part<3;part++) {
        int bend=e->midi.bend[part];
        ratio[part]=(uint32_t)(65536+bend*(bend<0?7150:8026)/8192);
    }
    int32_t slice[4][16][2],click[16];uint8_t mute[16];McVoiceRender voice_render[MC_VOICES];
    for(unsigned offset=0;offset<frames;) {
        unsigned count=frames-offset;if(count>16)count=16;
        /* Stopped arpeggiators may change notes on any frame; keep their preparation exact. */
        if(!e->playing)for(unsigned p=0;p<3;p++)
            if(e->project.arp[p].enabled&&e->arp[p].count)count=1;
        /* FX automation changes only at a sequencer step; never cross it in a slice. */
        if(count>1&&e->playing&&!e->project.clock_source&&e->project.bpm) {
            unsigned swing=e->step&1?100-e->project.swing:100+e->project.swing;
            uint32_t threshold=MC_RATE*60u*swing,increment=e->project.bpm*400u;
            if(e->clock+increment>=threshold)count=1;
            else {
                unsigned before=(threshold-1-e->clock)/increment;
                if(count>before)count=before;
            }
        }
        for(unsigned f=0;f<count;f++) {
            unsigned prepare=!f;
            for(unsigned p=0;p<3;p++) if(!e->playing&&e->project.arp[p].enabled&&e->arp[p].count) {
                McArpState *a=&e->arp[p];
                if(!a->clock) arp_trigger(e,p,MC_RATE*60u/(e->project.bpm*(1u<<e->project.arp[p].rate)));
                a->clock+=e->project.bpm*(1u<<e->project.arp[p].rate);
                if(a->clock>=MC_RATE*60u) a->clock-=MC_RATE*60u;
                if(a->clock<e->project.bpm*(1u<<e->project.arp[p].rate)) a->clock=0;
            }
            mc_midi_sample(e);
            if(e->playing&&e->project.demo&&!e->project.song_mode) {
                /* Demo controls update every 64 frames, independently of sequencer steps. */
                prepare|=(e->demo_frame&63u)==0;
                mc_demo_tick(e);e->demo_frame++;
            }
            if(e->playing&&!e->project.clock_source) {
                unsigned swing=e->step&1?100-e->project.swing:100+e->project.swing;
                e->clock+=e->project.bpm*400u;
                if(e->clock>=MC_RATE*60u*swing) {
                    e->clock-=MC_RATE*60u*swing;
                    advance(e);
                }
            }
            int32_t part[4]={0,0,0,0};
            if(!MC_PROBE_SKIP(8)) {
                if(prepare)for(unsigned i=0;i<MC_VOICES;i++)if(e->voice[i].active)
                    prepare_voice(e,&e->voice[i],i,ratio[e->voice[i].part],&voice_render[i]);
                for(unsigned i=0;i<MC_VOICES;i++)if(e->voice[i].active)
                    part[e->voice[i].part]+=melodic(e,&e->voice[i],i,&voice_render[i]);
                for(unsigned i=0;i<MC_DRUM_VOICES;i++) part[3]+=percussion(e,&e->drum[i]);
            }
            for(unsigned p=0;p<4;p++) {
                int32_t v=mc_div127_signal(part[p]*e->project.level[p]);
                slice[p][f][0]=mc_div127_signal(v*(127-e->project.pan[p]));
                slice[p][f][1]=mc_div127_signal(v*e->project.pan[p]);
            }
            mute[f]=e->mute;click[f]=0;
            if(!e->playing||!e->recording||!e->project.metronome)e->metro_left=0;
            if(e->metro_left) {
                e->metro_phase+=(e->metro_accent?1760u:1320u)*97392u;
                int32_t sample=mc_sine[e->metro_phase>>22]*(e->metro_accent?1600:1200)/32768;
                click[f]=sample*e->metro_left/(MC_RATE/50);e->metro_left--;
            }
            e->elapsed++;
        }
        int32_t rd[16][2],cf[16][2];
        unsigned rd_active=e->shared_fx.rd_active,cf_active=e->shared_fx.cf_active;
        for(unsigned p=0;p<MC_PARTS;p++) {
            rd_active|=e->project.fx_send[p][0];
            cf_active|=e->project.fx_send[p][1];
        }
        for(unsigned f=0;f<count;f++) {
            int32_t l=0,r=0,rl=0,rr=0,cl=0,cr=0;
            for(unsigned p=0;p<MC_PARTS;p++) if(!((mute[f]>>p)&1)) {
                int32_t pl=slice[p][f][0],pr=slice[p][f][1];
                l+=pl;r+=pr;
                if(e->project.fx_send[p][0]) {
                    rl+=mc_div127_signal(pl*(int32_t)e->project.fx_send[p][0]);
                    rr+=mc_div127_signal(pr*(int32_t)e->project.fx_send[p][0]);
                }
                if(e->project.fx_send[p][1]) {
                    cl+=mc_div127_signal(pl*(int32_t)e->project.fx_send[p][1]);
                    cr+=mc_div127_signal(pr*(int32_t)e->project.fx_send[p][1]);
                }
            }
            slice[0][f][0]=l;slice[0][f][1]=r;
            rd[f][0]=rl;rd[f][1]=rr;cf[f][0]=cl;cf[f][1]=cr;
        }
        if(rd_active&&!MC_PROBE_SKIP(2))
            mc_fx_rd_block(&e->shared_fx,e->project.bpm,e->project.fx_bus_type[0],rd,count);
        else zero(rd,sizeof rd);
        if(cf_active&&!MC_PROBE_SKIP(1))
            mc_fx_cf_block(&e->shared_fx,e->project.fx_bus_type[1],cf,count);
        else zero(cf,sizeof cf);
        for(unsigned f=0;f<count;f++) {
            int32_t l=mc_div3(slice[0][f][0]+mc_div_pow2(rd[f][0],1)+mc_div_pow2(cf[f][0],1))+click[f];
            int32_t r=mc_div3(slice[0][f][1]+mc_div_pow2(rd[f][1],1)+mc_div_pow2(cf[f][1],1))+click[f];
            mc_output(e,&l,&r);
            stereo[(offset+f)*2]=(int16_t)l;stereo[(offset+f)*2+1]=(int16_t)r;
        }
        offset+=count;
    }
}
