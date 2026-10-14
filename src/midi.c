/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
const uint8_t mc_midi_drums[16]={36,38,42,46,39,45,35,37,40,50,49,51,70,56,75,47};
static const uint8_t controls[MC_PARAMETERS]={255,255,255,255,74,71,73,75,70,72,5,255,7,10,94,91,12,93,20,21,22,23,24,25};
void mc_midi_send(McEngine *e,unsigned status,unsigned a,unsigned b,unsigned length) {
    McMidiState *m=&e->midi;
    if(m->receiving) return;
    if(m->panic) { m->dropped++;return; }
    if(m->count==MC_MIDI_QUEUE) { m->dropped++;m->panic=16;m->count=0;m->read=m->write=0;return; }
    McMidiMessage *v=&m->queue[m->write];
    v->data[0]=(uint8_t)status;v->data[1]=(uint8_t)a;v->data[2]=(uint8_t)b;v->length=(uint8_t)length;
    m->write=(uint16_t)((m->write+1)%MC_MIDI_QUEUE);m->count++;
}
int mc_midi_pop(McEngine *e,McMidiMessage *message) {
    McMidiState *m=&e->midi;
    if(m->stop_pending) {
        message->data[0]=0xfc;message->data[1]=message->data[2]=0;message->length=1;
        m->stop_pending=m->output_running=0;return 1;
    }
    if(m->cleanup) {
        unsigned channel=0;while(!(m->output_channels&(1u<<channel)))channel++;
        static const uint8_t controls[3]={123,120,64};
        message->data[0]=(uint8_t)(0xb0u+channel);message->data[1]=controls[--m->cleanup];
        message->data[2]=0;message->length=3;
        if(!m->cleanup) {
            m->output_channels&=(uint16_t)~(1u<<channel);
            if(m->output_channels)m->cleanup=3;
        }
        return 1;
    }
    if(m->panic) { message->data[0]=(uint8_t)(0xb0+16-m->panic--);message->data[1]=123;message->data[2]=0;message->length=3;return 1; }
    if(!m->count) return 0;
    *message=m->queue[m->read];m->read=(uint16_t)((m->read+1)%MC_MIDI_QUEUE);m->count--;
    /* 소비자에게 넘긴 채널은 대기열 밖에서도 발음 중일 수 있다. */
    if((message->data[0]&0xf0)==0x90&&message->data[2])m->output_channels|=(uint16_t)(1u<<(message->data[0]&15));
    if(message->data[0]==0xfa||message->data[0]==0xfb)m->output_running=1;
    else if(message->data[0]==0xfc)m->output_running=0;
    return 1;
}
void mc_midi_note(McEngine *e,unsigned part,unsigned note,unsigned velocity) {
    if(part<4) mc_midi_send(e,(velocity?0x90u:0x80u)+e->project.midi_channel[part],note,velocity,3);
}
void mc_midi_release(McEngine *e,unsigned part,unsigned note) {
    uint8_t receiving=e->midi.receiving;e->midi.receiving=0;
    mc_midi_note(e,part,note,0);e->midi.receiving=receiving;
}
void mc_midi_parameter(McEngine *e,unsigned part,unsigned parameter,unsigned value) {
    if(part<4&&parameter<MC_PARAM_DRUM_LEVEL&&controls[parameter]!=255)
        mc_midi_send(e,0xb0u+e->project.midi_channel[part],controls[parameter],value,3);
}
void mc_midi_all_off(McEngine *e,unsigned part) {
    if(part>=4) return;
    mc_midi_send(e,0xb0u+e->project.midi_channel[part],123,0,3);
    if(part<3) {
        for(unsigned i=0;i<MC_VOICES;i++) if(e->voice[i].part==part) {
            if(e->voice[i].midi_out) mc_midi_release(e,part,e->voice[i].note);
            e->voice[i].stage=2;e->voice[i].midi_out=0;
        }
        mc_arp_clear(e,part);
    }
    else for(unsigned i=0;i<MC_DRUM_VOICES;i++) e->drum[i].env=0;
    e->midi.sustain[part]=0;
    for(unsigned i=0;i<16;i++) e->midi.held[part][i]=e->midi.deferred[part][i]=0;
}
void mc_midi_reset(McEngine *e,unsigned source) {
    if(source<1||source>3) return;
    McMidiParser *p=&e->midi.parser[source-1];p->status=p->count=p->skip=0;
    e->midi.receiving=1;
    for(unsigned i=0;i<4;i++) mc_midi_all_off(e,i);
    if(e->project.clock_source==source) mc_stop(e);
    e->midi.receiving=0;
}
static void key(McEngine *e,unsigned part,unsigned note,unsigned velocity) {
    unsigned mask=1u<<(note%8),index=note/8;
    if(velocity) {
        e->midi.held[part][index]|=(uint8_t)mask;
        e->midi.deferred[part][index]&=(uint8_t)~mask;
        if(part==3) {
            for(unsigned d=0;d<16;d++) if(mc_midi_drums[d]==note) {
                mc_drum(e,d,velocity);
                if(e->playing&&e->recording) { McStep *s=&e->project.step[e->pattern][3][e->step];s->drums|=(uint16_t)(1u<<d);s->velocity=(uint8_t)velocity; }
            }
        } else {
            mc_key(e,part,note,velocity,1);
            if(e->playing&&e->recording&&!e->project.arp[part].enabled&&note>=12&&note<=108) {
                McStep *s=&e->project.step[e->pattern][part][e->step];
                for(unsigned i=0;i<3;i++) if(!s->note[i]||s->note[i]==note) { s->note[i]=(uint8_t)note;break; }
                s->velocity=(uint8_t)velocity;s->gate=75;s->flags=0;
            }
        }
    } else {
        e->midi.held[part][index]&=(uint8_t)~mask;
        if(e->midi.sustain[part]) e->midi.deferred[part][index]|=(uint8_t)mask;
        else if(part<3) mc_key(e,part,note,0,0);
    }
}
static void dispatch(McEngine *e,unsigned status,unsigned a,unsigned b) {
    for(unsigned part=0;part<4;part++) if(e->project.midi_channel[part]==(status&15)) {
        switch(status&0xf0) {
        case 0x80:key(e,part,a,0);break;
        case 0x90:key(e,part,a,b);break;
        case 0xc0:if(part<3) mc_preset(e,part,a);else if(a<MC_KITS) e->project.drum_kit=(uint8_t)a;break;
        case 0xe0:e->midi.bend[part]=(int16_t)((int)(a|(b<<7))-8192);break;
        case 0xb0:
            if(a==120||a==123) {
                mc_midi_all_off(e,part);
                if(a==120) for(unsigned i=0;i<MC_VOICES;i++) if(e->voice[i].part==part) e->voice[i].active=0;
                break;
            }
            if(a==64) {
                e->midi.sustain[part]=(uint8_t)(b>=64);
                if(b<64) for(unsigned note=0;note<128;note++) if(e->midi.deferred[part][note/8]&(1u<<(note%8))) {
                    if(part<3) mc_key(e,part,note,0,0);
                    e->midi.deferred[part][note/8]&=(uint8_t)~(1u<<(note%8));
                }
                break;
            }
            for(unsigned p=0;p<MC_PARAM_DRUM_LEVEL;p++) if(controls[p]==a) { (void)mc_parameter_set(e,part,p,b);break; }
            break;
        default:break;
        }
    }
}
void mc_midi_byte(McEngine *e,unsigned source,uint8_t byte) {
    if(source<1||source>3) return;
    McMidiParser *p=&e->midi.parser[source-1];e->midi.received[source-1]++;e->midi.receiving=1;
    if(byte>=0xf8) {
        if(source==e->project.clock_source) {
            if(byte==0xf8) {
                if(e->midi.have_tick) {
                    uint32_t delta=e->elapsed-e->midi.last_tick;
                    if(delta>=MC_RATE*60u/(24u*240u)&&delta<=MC_RATE*60u/(24u*40u)+1) {
                        e->midi.interval=delta;
                        e->project.bpm=(uint16_t)((MC_RATE*60u+delta*12u)/(delta*24u));
                    }
                }
                e->midi.last_tick=e->elapsed;e->midi.have_tick=1;
                e->midi.receiving=0;mc_clock_tick(e);e->midi.receiving=1;
            }
            else if(byte==0xfa) mc_start(e);
            else if(byte==0xfb) mc_continue(e);
            else if(byte==0xfc) mc_stop(e);
        }
    } else if(byte&128) {
        p->count=0;p->status=byte<0xf0?byte:0;p->skip=(uint8_t)(byte==0xf0);
    } else if(p->status&&!p->skip) {
        p->data[p->count++]=byte;
        unsigned need=(p->status&0xe0)==0xc0?1:2;
        if(p->count==need) { dispatch(e,p->status,p->data[0],need==2?p->data[1]:0);p->count=0; }
    }
    e->midi.receiving=0;
}
void mc_midi_sample(McEngine *e) {
    if(e->playing&&!e->project.clock_source) {
        e->midi.clock+=e->project.bpm*24u;
        if(e->midi.clock>=MC_RATE*60u) { e->midi.clock-=MC_RATE*60u;mc_midi_send(e,0xf8,0,0,1); }
    }
}
