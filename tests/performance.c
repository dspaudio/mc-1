/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/mcfm.h"
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
    assert(!mc_parameter_set(&e,4,MC_PARAM_REVERB_TYPE,3));
    assert(!mc_parameter_set(&e,3,MC_PARAM_HPF,80));
    e.recording=0;e.project.patch[0].cutoff=0;e.project.fx_type[1]=0;e.project.track_filter[3][0]=0;
    mc_start(&e);assert(e.project.patch[0].cutoff==99&&e.project.fx_type[1]==3&&e.project.track_filter[3][0]==80);
    mc_automation_clear(&e,4);e.recording=1;
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
    assert(sizeof(McProject)==MC_PROJECT_BYTES&&MC_RECORD_BYTES<=4096);
    e.project.preset[0]=MC_PRESET_VECTOR_LEAD;e.project.patch[0].cutoff=43;
    e.project.arp[0]=(McArp){1,MC_ARP_UPDOWN,1,3,83,1};
    assert(mc_encode(&e.project,42,record,sizeof record)==MC_RECORD_BYTES&&record[4]==4);
    assert(!mc_decode(&p,&sequence,record,sizeof record)&&sequence==42&&!memcmp(&p,&e.project,sizeof p));
    McProject before=p;record[20+2136+3+4]=0;
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
    puts("v4 round trip, v3 default migration, preset identity and strict validation: PASS");
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
int main(void) { modes();live();recording();automation();persistence();recorded_audio();live_changes();puts("performance: PASS"); }
