/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
#include "store.h"
#include "ui.h"
#include "presets_012.h"
#include "recorded_pcm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
static McEngine e,a,b;
static int16_t x[4096*2],y[4096*2];
static uint8_t flash[8192], backup[8192];
static int cut=-1;
static uint32_t unreadable;
static int read_store(uint32_t off,void *p,unsigned n) {
    if(off==unreadable||off<MC_STORE_A||off>MC_STORE_B+4096||n>MC_STORE_B+4096-off) return -1;
    memcpy(p,flash+(off-MC_STORE_A),n);return 0;
}
static int write_byte(unsigned off,uint8_t v,int erase) {
    if(cut==0) return -1;
    if(cut>0) cut--;
    if(!erase&&(flash[off]&v)!=v) return -1;
    flash[off]=erase?255:v;return 0;
}
static int erase_store(uint32_t off) {
    if(off!=MC_STORE_A&&off!=MC_STORE_B) return -1;
    for(unsigned i=0;i<4096;i++) if(write_byte(off-MC_STORE_A+i,255,1)) return -1;
    return 0;
}
static int program_store(uint32_t off,const void *p,unsigned n) {
    const uint8_t *q=p;
    if(off<MC_STORE_A||off>MC_STORE_B+4096||n>MC_STORE_B+4096-off) return -1;
    for(unsigned i=0;i<n;i++) if(write_byte(off-MC_STORE_A+i,q[i],0)) return -1;
    return 0;
}
static const McStorage io={read_store,erase_store,program_store};
static uint64_t energy(const int16_t *v,unsigned n) {
    uint64_t en=0;for(unsigned i=0;i<n;i++) en+=(uint64_t)((int32_t)v[i]*v[i]);return en;
}
static void render_n(McEngine *s,unsigned frames) {
    while(frames) { unsigned n=frames>4096?4096:frames;mc_render(s,x,n);frames-=n; }
}
static void engines(void) {
    uint32_t hashes[MC_PRESETS];
    for(unsigned p=0;p<MC_PRESETS;p++) {
        mc_init(&e);mc_preset(&e,0,p);assert(mc_project_valid(&e.project));
        uint8_t record[MC_RECORD_BYTES];McProject decoded;uint32_t seq;
        assert(mc_encode(&e.project,1,record,sizeof record)>0);assert(!mc_decode(&decoded,&seq,record,sizeof record));
        assert(!memcmp(&decoded.patch[0],&mc_presets[p],sizeof(McPatch)));mc_note(&e,0,60,110,0,MC_RATE);
        mc_render(&e,x,4096);assert(energy(x,8192)>10000);hashes[p]=mc_crc(x,sizeof x);
        render_n(&e,MC_RATE*5);for(unsigned i=0;i<MC_VOICES;i++) assert(!e.voice[i].active);
        assert(!e.clips);
    }
    for(unsigned i=0;i<MC_PRESETS;i++) for(unsigned j=i+1;j<MC_PRESETS;j++) assert(hashes[i]!=hashes[j]);
    for(unsigned sound=0;sound<MC_DRUMS;sound++) {
        mc_init(&e);mc_drum(&e,sound,120);mc_render(&e,x,4096);assert(energy(x,8192)>10000);
        render_n(&e,MC_RATE*4);for(unsigned i=0;i<MC_DRUM_VOICES;i++) assert(!e.drum[i].env);
    }
    mc_init(&e);mc_drum(&e,3,100);mc_drum(&e,2,100);
    for(unsigned i=0;i<MC_DRUM_VOICES;i++) assert(!e.drum[i].env||e.drum[i].sound!=3);
    /* Acid can run on each melodic part; slide keeps one voice and preserves phase/envelope. */
    for(unsigned part=0;part<3;part++) {
        mc_init(&e);mc_preset(&e,part,MC_PRESET_ACID_SAW);mc_note(&e,part,45,100,0,0);mc_render(&e,x,1024);
        int32_t env=e.voice[0].envelope;uint32_t phase=e.voice[0].phase,inc=e.voice[0].increment;
        mc_note(&e,part,57,100,MC_SLIDE,0);assert(e.voice[0].envelope==env&&e.voice[0].phase==phase);
        assert(e.voice[0].increment==inc&&e.voice[0].target>inc);mc_render(&e,x,128);
        assert(e.voice[0].increment>inc&&e.voice[0].increment<e.voice[0].target);
        for(unsigned i=1;i<MC_VOICES;i++) assert(!e.voice[i].active);
    }
    mc_init(&e);mc_preset(&e,0,MC_PRESET_VECTOR_LEAD);
    for(unsigned i=0;i<30;i++) { mc_note(&e,0,40+i,100,0,0);mc_render(&e,x,1); }
    assert(e.steals==22);
    puts("PASS: 128 distinct presets, 16 drums, release, choke, shared voice cap and per-part acid slides");
}
static void knob_audio(void) {
    McUi ua,ub;
    /* Every displayed melodic performance knob must alter rendered sound.
       PCM is tested at note attack; glide requires a legato destination note. */
    for(unsigned engine=0;engine<2;engine++) for(unsigned knob=0;knob<4;knob++) {
        mc_init(&a);mc_ui_init(&ua);mc_preset(&a,0,engine?MC_PRESET_ACID_SAW:MC_PRESET_VECTOR_LEAD);b=a;ub=ua;
        mc_ui_encoder(&a,&ua,knob+3,-127);mc_ui_encoder(&b,&ub,knob+3,127);
        mc_note(&a,0,48,110,0,0);mc_note(&b,0,48,110,0,0);
        if(engine&&knob==3) {
            render_n(&a,512);render_n(&b,512);
            mc_note(&a,0,72,110,MC_SLIDE,0);mc_note(&b,0,72,110,MC_SLIDE,0);
        }
        mc_render(&a,x,4096);mc_render(&b,y,4096);
        assert(memcmp(x,y,sizeof x));
        uint64_t difference=0;
        for(unsigned i=0;i<8192;i++) { int32_t d=x[i]-y[i];difference+=(uint64_t)(d*d); }
        assert(difference>100000);assert(!a.clips&&!b.clips);
    }
    /* LA resonance at maximum remains bounded over the complete keyboard/cutoff range. */
    for(unsigned note=12;note<=108;note+=12) for(unsigned cut=0;cut<=127;cut+=127) {
        mc_init(&e);mc_preset(&e,0,MC_PRESET_VECTOR_LEAD);e.project.patch[0].resonance=127;
        e.project.patch[0].cutoff=(uint8_t)cut;mc_note(&e,0,note,127,0,4096);
        render_n(&e,MC_RATE*2);assert(!e.clips);
        for(unsigned v=0;v<MC_VOICES;v++) assert(!e.voice[v].active);
    }
    for(unsigned k=0;k<4;k++) {
        mc_init(&a);mc_ui_init(&ua);ua.page=1;b=a;ub=ua;
        mc_ui_encoder(&a,&ua,k+3,-127);mc_ui_encoder(&b,&ub,k+3,127);
        mc_note(&a,0,60,100,0,MC_RATE/4);mc_note(&b,0,60,100,0,MC_RATE/4);
        uint32_t ha=0,hb=0;
        for(unsigned t=0;t<16;t++) { mc_render(&a,x,4096);mc_render(&b,y,4096);ha^=mc_crc(x,sizeof x);hb^=mc_crc(y,sizeof y); }
        assert(ha!=hb);
    }
    mc_init(&a);mc_ui_init(&ua);a.selected=3;b=a;ub=ua;
    mc_ui_encoder(&a,&ua,6,-127);mc_ui_encoder(&b,&ub,6,127);
    mc_drum(&a,0,100);mc_drum(&b,0,100);mc_render(&a,x,4096);mc_render(&b,y,4096);
    assert(!energy(x,8192)&&energy(y,8192)>10000);
    puts("PASS: LA/303 performance and envelope knob audio effects, drum level, LA resonance stability");
}
static void sequence(void) {
    mc_init(&a);b=a;mc_start(&a);mc_start(&b);
    mc_render(&a,x,4096);
    for(unsigned i=0;i<4096;i+=64) mc_render(&b,y+i*2,64);
    assert(!memcmp(x,y,sizeof x));assert(a.clock==b.clock&&a.step==b.step);
    mc_init(&e);e.project.bpm=120;e.project.swing=0;mc_start(&e);mc_queue(&e,2);
    render_n(&e,MC_RATE*2-1);assert(e.pattern==0&&e.step==15);
    render_n(&e,1);assert(e.pattern==2&&e.step==0&&e.queued==255&&e.events==17);
    mc_init(&e);e.project.bpm=120;e.project.swing=45;mc_start(&e);
    render_n(&e,MC_RATE*60);assert(e.events==481&&e.step==0);
    for(unsigned p=0;p<4;p++) mc_mute(&e,p);
    mc_render(&e,x,4096);assert(energy(x,8192)==0);
    mc_stop(&e);unsigned before=e.events;render_n(&e,MC_RATE);assert(e.events==before);
    puts("PASS: block-independent DSP, exact bar switching, 60-second swing clock and mute/stop");
}
static void persistence(void) {
    uint8_t blob[MC_RECORD_BYTES];McProject p,q;uint32_t seq;
    mc_factory(&p);assert(mc_encode(&p,123,blob,sizeof blob)==sizeof blob);
    assert(mc_decode(&q,&seq,blob,sizeof blob)==0&&seq==123&&!memcmp(&p,&q,sizeof p));
    /* Legacy v1 payload imports with dry FX and no demo automation. */
    uint8_t legacy[2132];memcpy(legacy,blob,84);memcpy(legacy+84,blob+92,2048);
    legacy[4]=1;legacy[6]=64;legacy[7]=8;
    uint32_t crc=mc_crc(legacy+20,2112);for(unsigned j=0;j<4;j++)legacy[12+j]=(uint8_t)(crc>>(j*8));
    crc=mc_crc(legacy,16);for(unsigned j=0;j<4;j++)legacy[16+j]=(uint8_t)(crc>>(j*8));
    assert(mc_decode(&q,&seq,legacy,sizeof legacy)==0&&q.fx[0]==0&&q.demo==0);
    uint8_t padded[2140]={0};memcpy(padded,legacy,sizeof legacy);
    assert(!mc_decode(&q,&seq,padded,sizeof padded)&&!q.drum_kit&&!q.track_fx[3][3]);
    p.fx[0]=51;p.fx[1]=62;p.fx[2]=13;p.fx[3]=37;p.demo=1;
    assert(mc_encode(&p,123,blob,sizeof blob)>0&&mc_decode(&q,&seq,blob,sizeof blob)==0&&!memcmp(&p,&q,sizeof p));
    mc_factory(&p);assert(mc_encode(&p,123,blob,sizeof blob)>0);
    for(unsigned n=0;n<sizeof blob;n++) assert(mc_decode(&q,&seq,blob,n)<0);
    for(unsigned i=0;i<sizeof blob;i++) {
        blob[i]^=1;assert(mc_decode(&q,&seq,blob,sizeof blob)<0);blob[i]^=1;
    }
    McProject invalid=p;invalid.patch[2].pcm=255;assert(!mc_project_valid(&invalid));
    invalid=p;invalid.bpm=0;assert(mc_encode(&invalid,1,blob,sizeof blob)<0);
    memset(flash,255,sizeof flash);assert(mc_store_load(&io,&q,&seq)<0);
    assert(mc_store_save(&io,&p)==0);assert(mc_store_load(&io,&q,&seq)==0&&seq==1);
    memcpy(backup,flash,sizeof flash);p.bpm=142;
    for(int n=0;n<4096+MC_RECORD_BYTES;n++) {
        memcpy(flash,backup,sizeof flash);cut=n;
        assert(mc_store_save(&io,&p)<0);cut=-1;
        assert(mc_store_load(&io,&q,&seq)==0&&seq==1&&q.bpm==118);
    }
    memcpy(flash,backup,sizeof flash);cut=-1;assert(mc_store_save(&io,&p)==0);
    assert(mc_store_load(&io,&q,&seq)==1&&seq==2&&q.bpm==142);
    flash[4096+50]^=1;assert(mc_store_load(&io,&q,&seq)==0&&q.bpm==118);
    /* Sequence rollover and valid-CRC malicious semantic payload. */
    assert(mc_encode(&p,0xffffffffu,flash,MC_RECORD_BYTES)>0);
    assert(mc_encode(&p,0,flash+4096,MC_RECORD_BYTES)>0);
    assert(mc_store_load(&io,&q,&seq)==1&&seq==0);
    memcpy(backup,flash,sizeof flash);
    unreadable=MC_STORE_A;assert(mc_store_load(&io,&q,&seq)==1);
    assert(mc_store_save(&io,&p)<0&&!memcmp(flash,backup,sizeof flash));
    unreadable=MC_STORE_B;assert(mc_store_load(&io,&q,&seq)==0);
    assert(mc_store_save(&io,&p)<0&&!memcmp(flash,backup,sizeof flash));unreadable=0;
    printf("PASS: portable project format, truncation/corruption, validation, rollover and %u power cuts\n",4096+MC_RECORD_BYTES);
}
static void screen(const McEngine *engine,const McUi *ui,uint16_t *pixels) {
    for(unsigned row=0;row<240;row+=8) mc_ui_strip(engine,ui,row,8,pixels+row*240);
}
static void phrase_grid(void) {
    McUi u;mc_ui_init(&u);mc_demo_load(&e,1);mc_start(&e);
    static uint16_t previous[240*240],current[240*240];screen(&e,&u,previous);
    for(unsigned pattern=1;pattern<4;pattern++) {
        for(unsigned frame=0;e.pattern!=pattern&&frame<MC_RATE*2;frame++) render_n(&e,1);
        assert(e.pattern==pattern&&e.step==0);screen(&e,&u,current);
        int changed=memcmp(previous+59*240,current+59*240,81*240*sizeof(uint16_t))!=0;
        fprintf(stderr,"Demo pattern %u: acid note %u, melodic grid changed %d\n",pattern+1,e.project.step[pattern][1][0].note[0],changed);
        assert(changed);memcpy(previous,current,sizeof previous);
    }
    mc_init(&e);mc_ui_init(&u);memset(e.project.step,0,sizeof e.project.step);
    e.project.step[0][0][0]=(McStep){.note={48,60,72},.velocity=100};
    screen(&e,&u,current);
    assert(current[61*240+32]==0x0841&&current[69*240+32]==0x0841&&current[76*240+32]==0x0841);
    assert(current[62*240+32]==0xfb20);
    e.project.step[0][0][0].note[0]=0;screen(&e,&u,current);
    assert(current[62*240+32]==0xfb20);
    mc_start(&e);screen(&e,&u,previous);mc_queue(&e,2);screen(&e,&u,current);
    assert(e.pattern==0&&e.queued==2&&memcmp(previous+173*240,current+173*240,7*240*sizeof(uint16_t)));
    memcpy(previous,current,sizeof previous);
    for(unsigned frame=0;e.pattern!=2&&frame<MC_RATE*3;frame++) render_n(&e,1);
    screen(&e,&u,current);assert(e.pattern==2&&e.queued==255);
    assert(memcmp(previous+173*240,current+173*240,7*240*sizeof(uint16_t)));
    u.message="SAVED";screen(&e,&u,previous);e.pattern=3;screen(&e,&u,current);
    assert(!memcmp(previous+171*240,current+171*240,12*240*sizeof(uint16_t)));
    u.message=0;e.selected=3;u.sound=15;e.project.step[3][3][0].drums=32768;e.project.step[3][3][0].velocity=100;
    screen(&e,&u,previous);assert(previous[162*240+32]==0x67e8);
    mc_queue(&e,0);screen(&e,&u,current);
    assert(!memcmp(previous+159*240,current+159*240,8*240*sizeof(uint16_t)));
    assert(memcmp(previous+173*240,current+173*240,7*240*sizeof(uint16_t)));
    puts("PASS: demo pattern progression changes pitch grid; chord and non-leading notes rendered");
    puts("PASS: home PATTERN/NEXT status follows queued bar transition; messages retain priority");
}
static void mixer(void) {
    McUi u;mc_init(&e);mc_ui_init(&u);mc_ui_button(&e,&u,UI_GLO);assert(u.page==5);
    uint8_t levels[4];memcpy(levels,e.project.level,sizeof levels);
    for(unsigned part=0;part<4;part++) {
        mc_ui_encoder(&e,&u,part+3,-1000);assert(e.project.level[part]==0);
        for(unsigned other=part+1;other<4;other++) assert(e.project.level[other]==levels[other]);
        mc_ui_encoder(&e,&u,part+3,1000);assert(e.project.level[part]==127);
    }
    static uint16_t pixels[240*240];screen(&e,&u,pixels);assert(pixels[120*240+26]==0xfb20);
    const char *hint="SELECT-ARP to mute";
    for(unsigned letter=0;hint[letter];letter++) {
        unsigned ink=0;
        for(unsigned row=0;row<7;row++) for(unsigned col=0;col<5;col++)
            if(pixels[(173+row)*240+66+letter*6+col]!=0x0841) ink++;
        assert(hint[letter]==' '?ink==0:ink>0);
    }
    assert(pixels[173*240+139]==0x0841&&pixels[175*240+139]==0xdefb);
    u.message="SAVED";screen(&e,&u,pixels);assert(pixels[175*240+139]==0x0841);u.message=0;
    mc_ui_button(&e,&u,UI_ARP);screen(&e,&u,pixels);assert(pixels[120*240+26]==0x4208);
    mc_ui_encoder(&e,&u,3,-127);screen(&e,&u,pixels);assert(pixels[120*240+26]==0x2104);
    uint8_t blob[MC_RECORD_BYTES];McProject project;uint32_t sequence;
    assert(mc_encode(&e.project,1,blob,sizeof blob)>0&&!mc_decode(&project,&sequence,blob,sizeof blob));
    assert(!memcmp(project.level,e.project.level,sizeof levels));
    mc_ui_button(&e,&u,UI_GLO);assert(u.page==2);mc_ui_button(&e,&u,UI_GLO);assert(u.page==4);
    mc_ui_button(&e,&u,UI_GLO);assert(u.page==7);mc_ui_button(&e,&u,UI_GLO);assert(u.page==5);mc_ui_button(&e,&u,UI_HOME);assert(u.page==0);
    for(unsigned part=0;part<4;part++) {
        mc_init(&a);b=a;mc_ui_init(&u);mc_ui_button(&a,&u,UI_GLO);mc_ui_encoder(&a,&u,part+3,-127);
        if(part<3) { mc_note(&a,part,60,100,0,0);mc_note(&b,part,60,100,0,0); }
        else { mc_drum(&a,0,100);mc_drum(&b,0,100); }
        mc_render(&a,x,4096);mc_render(&b,y,4096);assert(!energy(x,8192)&&energy(y,8192)>10000);
    }
    puts("PASS: GLO mixer levels, independent knobs, clamp, mute pixels, persistence and page cycle");
}
static void panel(void) {
    McUi u;mc_init(&e);mc_ui_init(&u);e.selected=3;
    mc_ui_encoder(&e,&u,3,15);assert(u.sound==15);
    mc_ui_encoder(&e,&u,4,7);assert(e.edit_step==7);
    mc_ui_encoder(&e,&u,5,1);assert(e.project.step[0][3][7].drums&32768);
    mc_ui_encoder(&e,&u,5,-1);assert(!(e.project.step[0][3][7].drums&32768));
    mc_ui_encoder(&e,&u,6,1000);assert(e.project.drum_level[15]==127);
    mc_ui_button(&e,&u,UI_SAVE);assert(!u.save_request&&u.save_pending);
    mc_ui_button(&e,&u,UI_SAVE);assert(u.save_request);
    u.save_request=0;mc_start(&e);mc_ui_button(&e,&u,UI_SAVE);assert(!u.save_request);
    mc_stop(&e);e.selected=0;mc_ui_button(&e,&u,UI_GLO);mc_ui_button(&e,&u,UI_GLO);
    mc_ui_encoder(&e,&u,3,1000);assert(e.project.bpm==240);
    mc_ui_encoder(&e,&u,4,1000);assert(e.project.swing==45);
    mc_ui_encoder(&e,&u,5,1);assert(e.pattern==1);
    mc_ui_encoder(&e,&u,6,-1000);assert(e.project.level[0]==0);
    mc_ui_button(&e,&u,UI_HOME);mc_ui_button(&e,&u,UI_SEQ);
    mc_ui_encoder(&e,&u,3,1);unsigned step=e.edit_step;
    mc_ui_encoder(&e,&u,4,-1000);assert(e.project.step[e.pattern][0][step].velocity==0);
    mc_ui_encoder(&e,&u,5,1000);assert(e.project.step[e.pattern][0][step].gate==100);
    mc_ui_encoder(&e,&u,6,3);assert(e.project.step[e.pattern][0][step].flags<4);
    mc_ui_key(&e,&u,4,1);assert(e.project.step[e.pattern][0][step].note[0]==57);
    uint16_t pixels[240*8+2];pixels[0]=123;pixels[240*8+1]=456;
    for(unsigned part=0;part<4;part++) for(unsigned page=0;page<6;page++) for(unsigned row=0;row<240;row+=8) {
        e.selected=(uint8_t)part;u.page=(uint8_t)page;mc_ui_strip(&e,&u,row,8,pixels+1);
        assert(pixels[0]==123&&pixels[240*8+1]==456);
    }
    puts("PASS: drum-grid controls, save confirmation and all screen strip bounds");
}
static void effects_demo(void) {
    for(unsigned fx=0;fx<4;fx++) {
        mc_init(&e);e.project.fx[fx]=127;uint64_t tail=0,changed=0;int32_t endpeak=0;
        for(unsigned i=0;i<MC_RATE*5;i++) {
            int32_t dry=i<64?12000:0,l=dry,r=dry;
            mc_effects(&e,&l,&r);assert(l>-60000&&l<60000&&r>-60000&&r<60000);
            if(l!=dry||r!=dry)changed++;
            if(i>=64)tail+=(uint64_t)((int64_t)l*l+(int64_t)r*r);
            if(i>MC_RATE*4) { int32_t peak=abs(l)+abs(r);if(peak>endpeak)endpeak=peak; }
        }
        assert(changed>0);if(fx!=2)assert(tail>0);assert(endpeak<32);
    }
    mc_demo_load(&a,1);assert(a.project.demo&&a.project.fx[0]&&mc_project_valid(&a.project));b=a;
    mc_start(&a);mc_start(&b);assert(a.pattern==0&&a.mute==6);
    mc_render(&a,x,4096);for(unsigned i=0;i<4096;i+=64)mc_render(&b,y+i*2,64);
    assert(!memcmp(x,y,sizeof x));
    render_n(&a,MC_RATE*60);assert(!a.playing&&a.events>=512&&a.clips==0);
    McUi u;mc_ui_init(&u);mc_init(&e);mc_ui_button(&e,&u,UI_FX);assert(u.page==3);
    for(unsigned k=0;k<4;k++) { mc_ui_encoder(&e,&u,k+3,50);assert(e.project.track_fx[0][k]==50&&e.project.fx[k]==0); }
    mc_ui_button(&e,&u,UI_GLO);mc_ui_button(&e,&u,UI_GLO);mc_ui_button(&e,&u,UI_GLO);assert(u.page==4);
    mc_start(&e);mc_ui_encoder(&e,&u,6,1);assert(!u.load_request);
    mc_stop(&e);mc_ui_encoder(&e,&u,6,1);assert(u.load_request==2);
    mc_demo_load(&e,u.load_request-1);assert(e.project.demo&&e.project.bpm==140);
    puts("PASS: four effect impulse/decay tests, native demo end, block identity and FX/demo UI");
}
static void stress(void) {
    clock_t begin=clock();mc_init(&e);e.project.bpm=240;e.project.swing=45;
    for(unsigned fx=0;fx<4;fx++) { e.project.fx[fx]=127;for(unsigned p=0;p<4;p++)e.project.track_fx[p][fx]=127; }
    for(unsigned p=0;p<3;p++) { mc_preset(&e,p,MC_PRESET_VECTOR_LEAD);e.project.patch[p].resonance=127;e.project.patch[p].cutoff=127; }
    for(unsigned i=0;i<8;i++) mc_note(&e,i%3,84+i*3,127,MC_ACCENT,0);
    for(unsigned i=0;i<16;i++) mc_drum(&e,i,127);
    mc_start(&e);render_n(&e,MC_RATE*10);
    printf("PASS: 10s stress; engine %zu B, project %zu B, %.3fs host CPU, peak %u, clips %u\n",sizeof e,sizeof e.project,(double)(clock()-begin)/CLOCKS_PER_SEC,e.peak,e.clips);
}
static void record_crc(uint8_t *record,unsigned body) {
    uint32_t crc=mc_crc(record+20,body);
    for(unsigned j=0;j<4;j++)record[12+j]=(uint8_t)(crc>>(j*8));
    crc=mc_crc(record,16);for(unsigned j=0;j<4;j++)record[16+j]=(uint8_t)(crc>>(j*8));
}
static void cascaded_gain(void) {
    for(unsigned bpm=40;bpm<=240;bpm+=40) {
        mc_init(&e);e.master=32767;e.project.bpm=(uint16_t)bpm;
        for(unsigned p=0;p<4;p++) {
            e.project.level[p]=127;e.project.pan[p]=0;
            e.project.track_fx[p][0]=e.project.track_fx[p][1]=127;
            if(p<3) {
                McPatch *patch=&e.project.patch[p];patch->engine=MC_LA;patch->wave=2;patch->pcm_mix=0;
                patch->level=127;patch->attack=0;patch->sustain=127;patch->resonance=0;patch->cutoff=127;
            }
        }
        e.project.fx[0]=e.project.fx[1]=127;
        for(unsigned v=0;v<MC_VOICES;v++)mc_note(&e,v%3,12,127,MC_ACCENT,0);
        for(unsigned d=0;d<MC_DRUMS;d++)mc_drum(&e,d,127);
        render_n(&e,MC_RATE*2);assert(e.clips>0&&e.peak>32767);
        for(unsigned i=0;i<8192;i++)assert(x[i]>=-32767);
    }
    puts("PASS: cascaded track/master maximum gain clamps without signed overflow at six tempos");
}
static void kits_and_tracks(void) {
    uint32_t hashes[MC_KITS][MC_DRUMS];McUi u;uint8_t record[MC_RECORD_BYTES];McProject q;uint32_t seq;
    for(unsigned kit=0;kit<MC_KITS;kit++) {
        for(unsigned sound=0;sound<MC_DRUMS;sound++) {
            mc_init(&e);e.project.drum_kit=(uint8_t)kit;mc_drum(&e,sound,120);
            mc_render(&e,x,4096);assert(energy(x,8192)>10000&&!e.clips);hashes[kit][sound]=mc_crc(x,sizeof x);
            render_n(&e,MC_RATE*4);for(unsigned i=0;i<MC_DRUM_VOICES;i++)assert(!e.drum[i].env);
        }
        mc_init(&e);e.project.drum_kit=(uint8_t)kit;mc_drum(&e,3,100);mc_drum(&e,2,100);
        for(unsigned i=0;i<MC_DRUM_VOICES;i++)assert(!e.drum[i].env||e.drum[i].sound!=3);
        assert(mc_encode(&e.project,7,record,sizeof record)>0&&record[23]==kit);
        assert(!mc_decode(&q,&seq,record,sizeof record)&&q.drum_kit==kit);
    }
    for(unsigned s=0;s<MC_DRUMS;s++)for(unsigned k=0;k<MC_KITS;k++)for(unsigned j=k+1;j<MC_KITS;j++)assert(hashes[k][s]!=hashes[j][s]);
    mc_init(&e);mc_ui_init(&u);e.selected=3;mc_ui_encoder(&e,&u,2,-1);assert(e.project.drum_kit==7&&e.pattern==0);
    mc_ui_encoder(&e,&u,1,1);assert(e.pattern==1&&e.project.drum_kit==7);
    mc_ui_encoder(&e,&u,2,1);assert(e.project.drum_kit==0);
    for(unsigned p=0;p<3;p++) {
        e.selected=(uint8_t)p;McPatch original=e.project.patch[p];mc_ui_encoder(&e,&u,1,1);
        assert(!memcmp(&original,&e.project.patch[p],sizeof original));mc_ui_encoder(&e,&u,2,1);
        assert(!memcmp(&e.project.patch[p],&mc_presets[u.preset[p]],sizeof original));
    }
    mc_init(&a);mc_drum(&a,0,120);b=a;b.project.drum_kit=7;mc_render(&a,x,4096);mc_render(&b,y,4096);assert(!memcmp(x,y,sizeof x));
    for(unsigned p=0;p<4;p++)for(unsigned fx=0;fx<4;fx++) {
        mc_init(&a);b=a;a.project.track_fx[p][fx]=127;
        if(p<3) { mc_note(&a,p,60,110,0,MC_RATE);mc_note(&b,p,60,110,0,MC_RATE); }
        else { mc_drum(&a,0,120);mc_drum(&b,0,120); }
        unsigned changed=0;for(unsigned n=0;n<12;n++) { mc_render(&a,x,4096);mc_render(&b,y,4096);changed|=memcmp(x,y,sizeof x)!=0; }
        assert(changed&&!a.clips);a.mute=(uint8_t)(1u<<p);mc_render(&a,x,4096);assert(energy(x,8192)==0);
        a.project.track_fx[p][fx]=0;mc_render(&a,x,1);assert(!(a.track_fx_active&(1u<<p)));
        McEffects empty={0};assert(!memcmp(&empty,&a.track_effects[p],sizeof empty));
        mc_init(&a);b=a;a.project.track_fx[(p+1)%4][fx]=127;
        if(p<3) { mc_note(&a,p,60,110,0,MC_RATE);mc_note(&b,p,60,110,0,MC_RATE); }
        else { mc_drum(&a,0,120);mc_drum(&b,0,120); }
        mc_render(&a,x,4096);mc_render(&b,y,4096);assert(!memcmp(x,y,sizeof x));
    }
    mc_init(&e);mc_ui_init(&u);mc_ui_button(&e,&u,UI_FX);assert(u.page==3&&!u.fx_master);
    for(unsigned p=0;p<4;p++)for(unsigned fx=0;fx<4;fx++) { e.selected=(uint8_t)p;mc_ui_encoder(&e,&u,fx+3,(int)(p*16+fx+1)); }
    mc_ui_button(&e,&u,UI_FX);assert(u.fx_master);mc_ui_encoder(&e,&u,3,37);assert(e.project.fx[0]==37);
    mc_ui_button(&e,&u,UI_HOME);mc_ui_button(&e,&u,UI_FX);assert(!u.fx_master);
    e.project.drum_kit=7;assert(mc_encode(&e.project,9,record,sizeof record)>0&&record[4]==4);
    assert(!mc_decode(&q,&seq,record,sizeof record)&&!memcmp(&q,&e.project,sizeof q));
    record[23]=8;record_crc(record,MC_PROJECT_BYTES);McProject before=q;assert(mc_decode(&q,&seq,record,sizeof record)<0&&!memcmp(&q,&before,sizeof q));
    record[23]=0;record[2140]=128;record_crc(record,MC_PROJECT_BYTES);assert(mc_decode(&q,&seq,record,sizeof record)<0);
    record[4]=2;record[6]=72;record[7]=8;record_crc(record,2120);
    assert(!mc_decode(&q,&seq,record,2140)&&q.fx[0]==37&&q.drum_kit==0);
    for(unsigned p=0;p<4;p++)for(unsigned fx=0;fx<4;fx++)assert(!q.track_fx[p][fx]);
    record[23]=1;record_crc(record,2120);assert(mc_decode(&q,&seq,record,2140)<0);
    puts("PASS: 128 audible unique kit lanes, all-kit choke/decay, stable active voices, ALGORITHM patterns/PRESETS sounds");
    puts("PASS: 16 isolated track FX paths, mute tails, dry buffer reset, track/master UI, v2 migration and v4 validation");
}
static void sorted_bank(void) {
    const char *categories[]={"Bass","Acid","Lead","Sequence","Keys","Organ","Brass/Reed","Strings","Pad","Pluck","Mallet/Bell","Voice","Hit/Percussion","Texture"};
    const unsigned counts[]={12,16,12,10,10,8,8,6,12,10,8,6,5,5};
    unsigned at=0;for(unsigned c=0;c<14;c++)for(unsigned n=0;n<counts[c];n++,at++) {
        assert(!strcmp(mc_preset_categories[at],categories[c]));assert(strlen(mc_preset_names[at])<=18);
    }
    assert(at==MC_PRESETS);
    for(unsigned old=0;old<64;old++) {
        unsigned found=MC_PRESETS;for(unsigned p=0;p<MC_PRESETS;p++)if(!strcmp(presets_012[old].name,mc_preset_names[p]))found=p;
        assert(found<MC_PRESETS&&!memcmp(&presets_012[old].patch,&mc_presets[found],sizeof(McPatch)));
        mc_init(&e);mc_preset(&e,0,found);mc_note(&e,0,60,110,0,MC_RATE);mc_render(&e,x,4096);
        assert(mc_crc(x,sizeof x)==presets_012[old].pcm_crc);
    }
    uint8_t record[MC_RECORD_BYTES];for(unsigned demo=0;demo<2;demo++) {
        mc_demo_load(&e,demo);assert(mc_encode(&e.project,1,record,sizeof record)>0);
        record[4]=3;record[6]=88;record[7]=8;record_crc(record,2136);
        assert(mc_crc(record,2156)==demo_012_crc[demo]);
    }
    McUi u;for(unsigned part=0;part<3;part++) {
        mc_init(&e);e.selected=(uint8_t)part;mc_preset(&e,part,127);mc_ui_init(&u);
        McPatch other[3];memcpy(other,e.project.patch,sizeof other);
        for(unsigned p=0;p<MC_PRESETS;p++) {
            mc_ui_encoder(&e,&u,2,1);assert(u.preset[part]==p&&!memcmp(&e.project.patch[part],&mc_presets[p],sizeof(McPatch)));
            for(unsigned t=0;t<3;t++)if(t!=part)assert(!memcmp(&other[t],&e.project.patch[t],sizeof(McPatch)));
        }
        mc_ui_encoder(&e,&u,2,1);assert(u.preset[part]==0);mc_ui_encoder(&e,&u,2,-1);assert(u.preset[part]==127);
        assert(mc_encode(&e.project,1,record,sizeof record)>0);mc_init(&e);uint32_t seq;
        assert(!mc_decode(&e.project,&seq,record,sizeof record));e.selected=(uint8_t)part;mc_ui_init(&u);
        mc_ui_encoder(&e,&u,2,1);assert(u.preset[part]==0);
    }
    puts("PASS: 128 presets in14 contiguous families, original64 patch/PCM snapshots and both demo project snapshots unchanged");
    puts("PASS: all128 preset forward/backward wrap on each melodic part and browsing relative to loaded patch");
}
static void recorded_samples(void) {
    unsigned count=0,used=0;
    for(unsigned p=0;p<MC_PRESETS;p++)if(mc_presets[p].pcm>=MC_PCM_ATTACKS) {
        assert(mc_presets[p].engine==MC_LA&&mc_presets[p].pcm_mix==127);
        used|=1u<<(mc_presets[p].pcm-MC_PCM_ATTACKS);count++;
        mc_init(&e);mc_preset(&e,0,p);uint8_t record[MC_RECORD_BYTES];McProject q;uint32_t seq;
        assert(mc_encode(&e.project,1,record,sizeof record)>0&&!mc_decode(&q,&seq,record,sizeof record));
        assert(!memcmp(&e.project,&q,sizeof q));
    }
    assert(count==64&&used==65535);
    for(unsigned id=0;id<MC_RECORDED_SAMPLES;id++) {
        const McRecordedSample *sample=&mc_recorded[id];
        for(unsigned mip=0;mip<7;mip++) {
            const McRecordedLevel *l=&sample->levels[mip];assert(l->length>=2&&l->length<32768);
            assert((!l->loop_start&&!l->loop_end)||(l->loop_start+1<l->loop_end&&l->loop_end<=l->length));
            for(unsigned i=0;i<l->length;i++)assert(abs(l->data[i])<=16000);
        }
        unsigned preset=0;while(mc_presets[preset].pcm!=id+MC_PCM_ATTACKS)preset++;
        mc_init(&a);mc_preset(&a,0,preset);b=a;a.project.patch[0].wave=0;b.project.patch[0].wave=2;
        mc_note(&a,0,60,110,0,0);mc_note(&b,0,60,110,0,0);
        mc_render(&a,x,4096);mc_render(&b,y,4096);assert(!memcmp(x,y,sizeof x)&&energy(x,8192)>0);
        mc_init(&b);mc_preset(&b,0,preset);b.project.patch[0].pcm_mix=0;mc_note(&b,0,60,110,0,0);
        mc_render(&b,y,4096);assert(memcmp(x,y,sizeof x));
        for(unsigned note=12;note<=108;note+=24) {
            mc_init(&a);mc_preset(&a,0,preset);a.project.patch[0].sustain=127;a.project.patch[0].release=0;
            mc_note(&a,0,note,127,0,0);assert(a.voice[0].sample_step>0&&a.voice[0].sample_step<=65536);
            b=a;mc_render(&a,x,4096);for(unsigned n=0;n<4096;n+=64)mc_render(&b,y+n*2,64);
            assert(!memcmp(x,y,sizeof x));
            render_n(&a,MC_RATE);const McRecordedLevel *l=&sample->levels[a.voice[0].sample_level];
            if(l->loop_end)assert(a.voice[0].pcm_pos<((uint32_t)l->loop_end<<16));
            else assert(a.voice[0].pcm_pos<=((uint32_t)l->length<<16)+65536);
            mc_off(&a,0,note);render_n(&a,MC_RATE);assert(!a.voice[0].active&&!a.clips);
        }
    }
    mc_init(&e);e.project.patch[0].pcm=MC_PCM_SOURCES;uint8_t record[MC_RECORD_BYTES];
    assert(mc_encode(&e.project,1,record,sizeof record)<0);
    e.project.patch[0].pcm=MC_PCM_ATTACKS;e.project.patch[0].engine=MC_ACID;
    assert(mc_encode(&e.project,1,record,sizeof record)<0);
    clock_t begin=clock();mc_init(&e);
    for(unsigned p=0;p<3;p++) { unsigned n=0;while(mc_presets[n].pcm<MC_PCM_ATTACKS)n++;mc_preset(&e,p,n); }
    for(unsigned fx=0;fx<4;fx++) { e.project.fx[fx]=127;for(unsigned p=0;p<4;p++)e.project.track_fx[p][fx]=127; }
    for(unsigned n=0;n<8;n++)mc_note(&e,n%3,108-n,127,MC_ACCENT,0);
    render_n(&e,MC_RATE*10);
    printf("PASS: 64 recorded presets,16 sources,112 mip bounds/peaks,80 pitch/loop/release cases, PCM-only and blend, persistence;10s sampled stress %.3fs host CPU\n",(double)(clock()-begin)/CLOCKS_PER_SEC);
}
int main(void) {
    assert(sizeof(McPatch)==12&&sizeof(McStep)==8&&sizeof(McProject)==MC_PROJECT_BYTES);
    phrase_grid();mixer();engines();knob_audio();sequence();persistence();panel();effects_demo();kits_and_tracks();cascaded_gain();sorted_bank();recorded_samples();stress();puts("ALL TESTS PASSED");return 0;
}
