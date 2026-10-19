/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/mcfm.h"
#include "../firmware/perf_fixture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static McEngine e, replay;
static int16_t audio[2048];
static void render(unsigned frames) {
    while(frames) { unsigned n=frames>1024?1024:frames;mc_render(&e,audio,n);frames-=n; }
}
static void empty(void) {
    mc_init(&e);memset(e.project.step,0,sizeof e.project.step);e.project.swing=0;e.project.bpm=120;
    mc_preset(&e,0,MC_PRESET_VECTOR_LEAD);
}
static void chord(void) { mc_key(&e,0,67,100,1);mc_key(&e,0,60,100,1);mc_key(&e,0,64,100,1); }
static void modes(void) {
    static const uint8_t expected[4][6]={{60,64,67,60,64,67},{67,64,60,67,64,60},
        {60,64,67,64,60,64},{67,60,64,67,60,64}};
    for(unsigned mode=0;mode<4;mode++) {
        empty();e.project.arp[0].enabled=1;e.project.arp[0].mode=(uint8_t)mode;chord();mc_start(&e);
        for(unsigned i=0;i<6;i++) {
            assert(e.arp[0].sounding[0]==expected[mode][i]);render(5513);
        }
        mc_stop(&e);assert(!e.arp[0].count);
    }
    empty();e.project.arp[0].enabled=1;e.project.arp[0].mode=MC_ARP_CHORD;chord();mc_key(&e,0,72,100,1);mc_start(&e);
    assert(e.arp[0].sounding[0]==60&&e.arp[0].sounding[1]==64&&e.arp[0].sounding[2]==67);
    empty();e.project.arp[0].enabled=1;e.project.arp[0].octaves=3;mc_key(&e,0,100,100,1);mc_start(&e);
    for(unsigned i=0;i<5;i++) { assert(e.arp[0].sounding[0]==100);render(5513); }
    empty();e.project.arp[0].enabled=1;e.project.arp[0].mode=MC_ARP_RANDOM;chord();uint32_t seed=e.random;mc_start(&e);
    unsigned seen=0;for(unsigned i=0;i<32;i++) { seen|=1u<<(e.arp[0].sounding[0]-60);render(5513); }
    assert(seen==((1u<<0)|(1u<<4)|(1u<<7))&&e.random==seed);
    puts("ARP modes, chord limit, octave bounds and isolated random: PASS");
}
static void live(void) {
    empty();e.project.arp[0].enabled=1;chord();render(1);
    assert(!e.playing&&e.arp[0].events==1&&e.arp[0].sounding[0]==60);
    mc_key(&e,0,67,100,0);mc_key(&e,0,60,100,0);mc_key(&e,0,64,100,0);
    assert(!e.arp[0].count);render(MC_RATE*3);
    for(unsigned i=0;i<MC_VOICES;i++) assert(!e.voice[i].active);
    e.project.arp[0].latch=1;chord();mc_key(&e,0,67,100,0);mc_key(&e,0,60,100,0);mc_key(&e,0,64,100,0);
    assert(e.arp[0].count==3);mc_key(&e,0,72,100,1);assert(e.arp[0].count==1&&e.arp[0].notes[0]==72);
    mc_arp_clear(&e,0);assert(!e.arp[0].count);
    e.project.arp[0].latch=0;e.project.arp[0].octaves=3;e.project.arp[0].mode=MC_ARP_UPDOWN;
    for(unsigned i=0;i<MC_ARP_KEYS;i++) mc_key(&e,0,36+i,100,1);
    assert(e.arp[0].count==MC_ARP_KEYS);mc_key(&e,0,90,100,1);assert(e.arp[0].count==MC_ARP_KEYS);
    render(MC_RATE*2);
    for(unsigned i=0;i<MC_ARP_KEYS;i++) mc_key(&e,0,36+i,100,0);
    assert(!e.arp[0].count);
    puts("Stopped live ARP, release, latch replace and part cleanup: PASS");
}
static void recording(void) {
    for(unsigned rate=0;rate<3;rate++) {
        empty();e.project.arp[0].enabled=1;e.project.arp[0].rate=(uint8_t)rate;e.recording=1;chord();mc_start(&e);
        unsigned divisor=4u>>rate;
        for(unsigned event=0;event<4;event++) {
            const McStep *s=&e.project.step[0][0][e.step];assert(s->note[0]==e.arp[0].sounding[0]);
            assert(!s->note[1]&&!s->note[2]&&s->velocity==100);
            render(5513*divisor);
        }
        mc_stop(&e);mc_init(&replay);replay.project=e.project;replay.project.arp[0].enabled=0;mc_start(&replay);
        assert(replay.voice[0].note==e.project.step[0][0][0].note[0]);
        if(rate<2) assert(e.project.step[0][0][1].flags&MC_TIE);
    }
    puts("REC stores generated ARP notes with slower-rate continuation ties: PASS");
}
static void automation(void) {
    empty();e.recording=1;mc_start(&e);
    assert(!mc_parameter_set(&e,0,4,12));assert(!mc_parameter_set(&e,0,4,99));
    assert(e.project.automation_count==1&&e.project.automation[0].value==99);
    assert(mc_parameter_set(&e,4,MC_PARAM_REVERB_TYPE,3)==-1&&e.project.automation_count==1);
    assert(!mc_parameter_set(&e,1,MC_PARAM_CF_TYPE,3));
    assert(!mc_parameter_set(&e,3,MC_PARAM_RD_SEND,80));
    e.recording=0;e.project.patch[0].cutoff=0;e.project.fx_bus_type[1]=0;e.project.fx_send[3][0]=0;
    mc_start(&e);assert(e.project.patch[0].cutoff==99&&e.project.fx_bus_type[1]==3&&e.project.fx_send[3][0]==80);
    mc_automation_clear(&e,MC_PATTERNS);e.recording=1;
    for(unsigned i=0;i<MC_AUTOMATION;i++) {
        e.pattern=(uint8_t)(i/96);e.step=(uint8_t)((i/6)%16);
        assert(!mc_parameter_set(&e,0,3+i%6,i%128));
    }
    assert(e.project.automation_count==MC_AUTOMATION);
    e.pattern=0;e.step=0;assert(mc_parameter_set(&e,0,MC_PARAM_LEVEL,9)==1&&e.automation_full);
    assert(!mc_parameter_set(&e,0,3,42)&&e.project.automation_count==MC_AUTOMATION);
    mc_automation_clear(&e,1);assert(e.project.automation_count==288&&!e.automation_full);
    for(unsigned i=0;i<e.project.automation_count;i++) assert(e.project.automation[i].address/5!=1);
    assert(mc_project_valid(&e.project));
    puts("Automation replay, same-step replace, capacity, visible overflow, pattern clear: PASS");
}
static void persistence(void) {
    static uint8_t record[MC_RECORD_BYTES],legacy[2156];McProject p;uint32_t sequence=0;
    assert(MC_RECORD_BYTES<=36*1024);
    e.project.preset[0]=MC_PRESET_VECTOR_LEAD;e.project.patch[0].cutoff=43;
    e.project.arp[0]=(McArp){1,MC_ARP_UPDOWN,1,3,83,1};
    assert(mc_encode(&e.project,42,record,sizeof record)==MC_RECORD_BYTES&&record[4]==MC_PROJECT_VERSION);
    assert(!mc_decode(&p,&sequence,record,sizeof record)&&sequence==42&&!memcmp(&p,&e.project,sizeof p));
    McProject before=p;record[20+72+MC_PATTERNS*4*16*8+16+3+4]=0;
    uint32_t crc=mc_crc(record+20,MC_PROJECT_BYTES);for(unsigned i=0;i<4;i++) record[12+i]=(uint8_t)(crc>>(8*i));
    crc=mc_crc(record,16);for(unsigned i=0;i<4;i++) record[16+i]=(uint8_t)(crc>>(8*i));
    assert(mc_decode(&p,&sequence,record,sizeof record)<0&&!memcmp(&p,&before,sizeof p));
    empty();assert(mc_encode(&e.project,3,record,sizeof record)>0);
    memcpy(legacy,record,sizeof legacy);legacy[4]=3;legacy[6]=88;legacy[7]=8;
    crc=mc_crc(legacy+20,2136);for(unsigned i=0;i<4;i++)legacy[12+i]=(uint8_t)(crc>>(8*i));
    crc=mc_crc(legacy,16);for(unsigned i=0;i<4;i++)legacy[16+i]=(uint8_t)(crc>>(8*i));
    assert(!mc_decode(&p,&sequence,legacy,sizeof legacy));
    assert(p.preset[0]==MC_PRESET_VECTOR_LEAD&&!p.arp[0].enabled&&p.arp[0].gate==75&&!p.automation_count);
    p.arp[0].gate=0;assert(!mc_project_valid(&p));p.arp[0].gate=75;p.automation_count=385;assert(!mc_project_valid(&p));
    puts("v5 round trip, v3 default migration, preset identity and strict validation: PASS");
}
static void recorded_audio(void) {
    static int16_t live_audio[MC_RATE*4],saved_audio[MC_RATE*4];
    for(unsigned swing=0;swing<=30;swing+=30) for(unsigned rate=0;rate<3;rate++) {
        empty();e.project.swing=(uint8_t)swing;e.project.arp[0].enabled=1;e.project.arp[0].rate=(uint8_t)rate;
        e.recording=1;chord();mc_start(&e);mc_render(&e,live_audio,MC_RATE*2-1);
        mc_init(&replay);replay.project=e.project;replay.project.arp[0].enabled=0;mc_start(&replay);
        mc_render(&replay,saved_audio,MC_RATE*2-1);
        unsigned diff=0;while(diff<MC_RATE*4-2&&live_audio[diff]==saved_audio[diff]) diff++;
        if(diff<MC_RATE*4-2) printf("ARP replay mismatch swing=%u rate=%u sample=%u live=%d saved=%d\n",swing,rate,diff,live_audio[diff],saved_audio[diff]);
        assert(diff==MC_RATE*4-2);
    }
    mc_demo_load(&e,1);e.recording=1;mc_start(&e);assert(!mc_parameter_set(&e,1,4,77));
    e.recording=0;render(256);assert(e.project.patch[1].cutoff==77);
    puts("Recorded ARP PCM parity across rates/swing and demo automation priority: PASS");
}
static void live_changes(void) {
    for(unsigned playing=0;playing<2;playing++) {
        empty();e.project.arp[0].enabled=1;chord();e.recording=1;
        if(playing) mc_start(&e);
        for(unsigned change=0;change<18;change++) {
            render(937);
            e.project.arp[0].rate=(uint8_t)(change%3);
            e.project.arp[0].octaves=(uint8_t)(1+change%3);
            e.project.arp[0].gate=(uint8_t)(change%2?100:1);
            uint32_t events=e.arp[0].events;render(MC_RATE);
            assert(e.arp[0].events>events&&e.arp[0].count==3);
            assert(e.arp[0].sounding[0]>=60&&e.arp[0].sounding[0]<=91&&mc_project_valid(&e.project));
        }
        mc_key(&e,0,60,100,0);mc_key(&e,0,64,100,0);mc_key(&e,0,67,100,0);
        assert(!e.arp[0].count);
    }
    puts("Held-key rate, octave and gate changes while stopped/playing: PASS");
}
static void live_glide(void) {
    for(unsigned part=0;part<3;part++) {
        empty();mc_preset(&e,part,MC_PRESET_ACID_SAW);e.project.patch[part].glide=64;
        mc_key(&e,part,48,100,1);render(256);
        McVoice *v=0;
        for(unsigned i=0;i<MC_VOICES;i++)if(e.voice[i].active&&e.voice[i].part==part)v=&e.voice[i];
        assert(v&&v->note==48&&v->stage!=2);
        McVoice before=*v;uint32_t first_target=v->target;
        mc_key(&e,part,60,80,1);
        assert(e.key_slide[part]&&v->note==60&&v->increment==before.increment&&v->target>v->increment);
        assert(v->phase==before.phase&&v->phase2==before.phase2&&v->envelope==before.envelope);
        assert(!memcmp(v->filter,before.filter,sizeof v->filter));
        render(64);assert(v->increment>before.increment&&v->increment<v->target);
        mc_key(&e,part,60,0,0);
        assert(v->note==48&&v->target==first_target&&v->stage!=2);
        mc_key(&e,part,48,0,0);assert(v->stage==2);
        for(unsigned i=0;i<16;i++)assert(!e.live_keys[part][i]);
        mc_key(&e,part,67,100,1);
        assert(!e.key_slide[part]&&v->note==67&&v->increment==v->target&&!v->envelope);
        render(32);mc_key(&e,part,67,90,1);
        assert(!e.key_slide[part]&&!v->envelope);
        mc_stop(&e);
        for(unsigned i=0;i<16;i++)assert(!e.live_keys[part][i]);
        e.project.patch[part].glide=0;
        mc_key(&e,part,48,100,1);render(32);mc_key(&e,part,60,100,1);
        assert(!e.key_slide[part]&&v->note==60&&v->increment==v->target&&!v->envelope);
        mc_stop(&e);e.project.patch[part].glide=64;
        mc_key(&e,part,48,100,1);mc_key(&e,part,60,100,1);mc_key(&e,part,72,100,1);
        mc_key(&e,part,48,0,0);assert(v->note==72&&v->stage!=2);
        mc_key(&e,part,72,0,0);assert(v->note==60&&v->stage!=2);
        mc_midi_all_off(&e,part);
        for(unsigned i=0;i<16;i++)assert(!e.live_keys[part][i]);
    }
    empty();e.project.patch[0].glide=127;
    mc_key(&e,0,60,100,1);mc_key(&e,0,64,100,1);
    unsigned voices=0;
    for(unsigned i=0;i<MC_VOICES;i++)voices+=e.voice[i].active!=0;
    assert(voices==2&&!e.key_slide[0]);
    puts("Live mono glide: all parts, phase/envelope continuity, held-note return, zero/off, retrigger, cleanup and LA polyphony PASS");
}
static void tempo_blocks(void) {
    static const uint16_t tempos[]={40,41,80,81,118,140,239,240};
    static const unsigned pieces[]={1,7,23,97};
    static int16_t split[256];
    for(unsigned demo=0;demo<2;demo++) {
        mc_init(&e);mc_demo_load(&e,demo);mc_start(&e);
        for(unsigned part=0;part<4;part++) {
            e.project.fx_send[part][0]=83;e.project.fx_send[part][1]=109;
        }
        replay=e;
        for(unsigned block=0;block<512;block++) {
            e.project.bpm=replay.project.bpm=tempos[block%(sizeof tempos/sizeof *tempos)];
            e.project.fx_bus_type[0]=replay.project.fx_bus_type[0]=(uint8_t)(block%8);
            e.project.fx_bus_type[1]=replay.project.fx_bus_type[1]=(uint8_t)(block/8%8);
            mc_render(&e,audio,128);
            for(unsigned at=0,piece=0;at<128;piece++) {
                unsigned n=pieces[piece%4];if(n>128-at)n=128-at;
                mc_render(&replay,split+2*at,n);at+=n;
            }
            assert(!memcmp(audio,split,sizeof split)&&!memcmp(&e,&replay,sizeof e));
        }
    }
    puts("Tempo changes: both demos, shared FX types, automation and split-block full PCM/state PASS");
}
static void sliced_automation(void) {
    static int16_t split[514];
    empty();e.project.demo=0;e.project.bpm=240;e.project.swing=45;
    e.project.metronome=1;e.project.patch[0].sustain=127;
    e.project.automation_count=0;
    for(unsigned pattern=0;pattern<2;pattern++)for(unsigned step=0;step<16;step++)
        for(unsigned param=MC_PARAM_RD_SEND;param<=MC_PARAM_CF_TYPE;param++) {
            McAutomation *a=&e.project.automation[e.project.automation_count++];
            a->address=(uint16_t)(pattern*5+(param>=MC_PARAM_RD_TYPE?4:step%4));a->step=(uint8_t)step;a->parameter=(uint8_t)param;
            a->value=(uint8_t)(param>=MC_PARAM_RD_TYPE?step%8:step&1?0:83);
        }
    assert(mc_project_valid(&e.project));
    mc_note(&e,0,60,100,0,0);e.recording=1;mc_start(&e);replay=e;
    for(unsigned block=0;block<512;block++) {
        if(block%47==0) { mc_queue(&e,block/47%2);mc_queue(&replay,block/47%2); }
        if(block%37==0)e.project.swing=replay.project.swing=(uint8_t)(block%74?45:0);
        mc_render(&e,audio,257);
        for(unsigned at=0;at<257;at++)mc_render(&replay,split+2*at,1);
        assert(!memcmp(audio,split,sizeof split)&&!memcmp(&e,&replay,sizeof e));
    }
    puts("FX slices: independent sends, global type changes, swing, queue and metronome PCM/state PASS");
}
static void legacy_master(void) {
    static uint8_t record[MC_RECORD_BYTES];
    static McProject legacy;
    static int16_t comparison[514];
    empty();e.project.patch[0].attack=0;e.project.patch[0].sustain=127;
    e.project.step[0][0][0]=(McStep){{60,0,0},100,100,0,0};
    for(unsigned p=0;p<MC_PARTS;p++) {
        e.project.track_fx[p][0]=(uint8_t)(20+p*7);
        e.project.track_fx[p][1]=(uint8_t)(15+p*5);
        e.project.track_fx_type[p][0]=e.project.track_fx_type[p][1]=(uint8_t)p;
    }
    for(unsigned i=0;i<4;i++) { e.project.fx[i]=127;e.project.fx_type[i]=(uint8_t)(i==3?2:3); }
    e.project.filter[0]=e.project.filter[1]=127;
    e.project.automation_count=2;
    e.project.automation[0]=(McAutomation){4,0,MC_PARAM_REVERB_TYPE,2};
    e.project.automation[1]=(McAutomation){MC_PATTERNS*5-1,0,MC_PARAM_REVERB_TYPE,1};
    assert(mc_project_valid(&e.project));
    e.recording=1;mc_start(&e);legacy=e.project;
    unsigned midi_count=e.midi.count;
    for(unsigned param=0;param<MC_PARAMETERS;param++) {
        if(param==MC_PARAM_RD_TYPE||param==MC_PARAM_CF_TYPE||param==MC_PARAM_RD2_TYPE||param==MC_PARAM_CF2_TYPE)continue;
        assert(mc_parameter_get(&e,4,param)==-1);
        assert(mc_parameter_set(&e,4,param,0)==-1);
    }
    assert(!memcmp(&legacy,&e.project,sizeof legacy)&&e.midi.count==midi_count);
    mc_stop(&e);e.recording=0;
    assert(mc_encode(&e.project,7,record,sizeof record)==MC_RECORD_BYTES&&record[4]==MC_PROJECT_VERSION);
    mc_init(&e);e.project=legacy;mc_init(&replay);uint32_t sequence=0;
    assert(!mc_decode(&replay.project,&sequence,record,sizeof record)&&sequence==7);
    assert(!memcmp(&legacy,&replay.project,sizeof legacy));
    memset(replay.project.fx,0,sizeof replay.project.fx);
    memset(replay.project.fx_type,0,sizeof replay.project.fx_type);
    memset(replay.project.filter,0,sizeof replay.project.filter);
    replay.project.automation_count=0;
    mc_start(&e);mc_start(&replay);unsigned nonzero=0;
    for(unsigned block=0;block<512;block++) {
        mc_render(&e,audio,257);mc_render(&replay,comparison,257);
        assert(!memcmp(audio,comparison,sizeof comparison));
        for(unsigned i=0;i<514;i++)nonzero+=audio[i]!=0;
    }
    assert(nonzero&&!memcmp(&legacy,&e.project,sizeof legacy));
    puts("Legacy master FX and automation stay portable but inactive; master scope rejects edits without mutation: PASS");
}
static void stress_fixture(void) {
    for(unsigned acid=0;acid<=3;acid++) {
        mc_init(&e);e.master=818;
        mc_perf_fixture(&e,acid);
        assert(e.master==818&&e.playing&&mc_project_valid(&e.project));
        assert(e.project.fx_send[3][0]==127&&e.project.fx_send[3][1]==127);
        unsigned expected=acid==3?3:MC_VOICES;
        for(unsigned block=0;block<1024+8192;block++) {
            mc_render(&e,audio,128);
            if(block<1024)continue;
            unsigned voices=0,drums=0,blend=0,moving=0;
            for(unsigned i=0;i<MC_VOICES;i++)if(e.voice[i].active) {
                const McVoice *v=&e.voice[i];voices++;
                if(v->patch.engine==MC_LA&&v->patch.pcm>=MC_PCM_ATTACKS&&v->patch.pcm_mix==63)blend++;
                if(v->patch.engine==MC_ACID&&v->increment!=v->target)moving++;
            }
            for(unsigned i=0;i<MC_DRUM_VOICES;i++)drums+=e.drum[i].env!=0;
            assert(voices==expected&&drums==MC_DRUM_VOICES);
            assert(blend==expected-acid&&moving==acid&&e.master==818);
        }
    }
    puts("RAM stress: PCM+dual oscillators, repeated six drums and sustained acid glide survive the full capture window: PASS");
}
int main(void) { modes();live();recording();automation();persistence();recorded_audio();live_changes();live_glide();tempo_blocks();sliced_automation();legacy_master();stress_fixture();puts("performance: PASS"); }
