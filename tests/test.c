/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
#include "store.h"
#include "ui.h"
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
    uint32_t hashes[12];
    for(unsigned p=0;p<MC_PRESETS;p++) {
        mc_init(&e);mc_preset(&e,0,p);mc_note(&e,0,60,110,0,MC_RATE);
        mc_render(&e,x,4096);assert(energy(x,8192)>10000);hashes[p]=mc_crc(x,sizeof x);
        render_n(&e,MC_RATE*5);for(unsigned i=0;i<MC_VOICES;i++) assert(!e.voice[i].active);
        assert(!e.clips);
    }
    for(unsigned i=0;i<12;i++) for(unsigned j=i+1;j<12;j++) assert(hashes[i]!=hashes[j]);
    for(unsigned sound=0;sound<MC_DRUMS;sound++) {
        mc_init(&e);mc_drum(&e,sound,120);mc_render(&e,x,4096);assert(energy(x,8192)>10000);
        render_n(&e,MC_RATE*4);for(unsigned i=0;i<MC_DRUM_VOICES;i++) assert(!e.drum[i].env);
    }
    mc_init(&e);mc_drum(&e,3,100);mc_drum(&e,2,100);
    for(unsigned i=0;i<MC_DRUM_VOICES;i++) assert(!e.drum[i].env||e.drum[i].sound!=3);
    /* Acid can run on each melodic part; slide keeps one voice and preserves phase/envelope. */
    for(unsigned part=0;part<3;part++) {
        mc_init(&e);mc_preset(&e,part,6);mc_note(&e,part,45,100,0,0);mc_render(&e,x,1024);
        int32_t env=e.voice[0].envelope;uint32_t phase=e.voice[0].phase,inc=e.voice[0].increment;
        mc_note(&e,part,57,100,MC_SLIDE,0);assert(e.voice[0].envelope==env&&e.voice[0].phase==phase);
        assert(e.voice[0].increment==inc&&e.voice[0].target>inc);mc_render(&e,x,128);
        assert(e.voice[0].increment>inc&&e.voice[0].increment<e.voice[0].target);
        for(unsigned i=1;i<MC_VOICES;i++) assert(!e.voice[i].active);
    }
    mc_init(&e);mc_preset(&e,0,0);
    for(unsigned i=0;i<30;i++) { mc_note(&e,0,40+i,100,0,0);mc_render(&e,x,1); }
    assert(e.steals==22);
    puts("PASS: 12 distinct presets, 16 drums, release, choke, shared voice cap and per-part acid slides");
}
static void knob_audio(void) {
    McUi ua,ub;
    /* Every displayed melodic performance knob must alter rendered sound.
       PCM is tested at note attack; glide requires a legato destination note. */
    for(unsigned engine=0;engine<2;engine++) for(unsigned knob=0;knob<4;knob++) {
        mc_init(&a);mc_ui_init(&ua);mc_preset(&a,0,engine?6:0);b=a;ub=ua;
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
        mc_init(&e);mc_preset(&e,0,0);e.project.patch[0].resonance=127;
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
    puts("PASS: portable project format, truncation/corruption, validation, rollover and 6236 power cuts");
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
    mc_stop(&e);e.selected=0;mc_ui_button(&e,&u,UI_GLO);
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
    for(unsigned part=0;part<4;part++) for(unsigned page=0;page<5;page++) for(unsigned row=0;row<240;row+=8) {
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
    for(unsigned k=0;k<4;k++) { mc_ui_encoder(&e,&u,k+3,50);assert(e.project.fx[k]==50); }
    mc_ui_button(&e,&u,UI_GLO);mc_ui_button(&e,&u,UI_GLO);assert(u.page==4);
    mc_start(&e);mc_ui_encoder(&e,&u,6,1);assert(!u.load_request);
    mc_stop(&e);mc_ui_encoder(&e,&u,6,1);assert(u.load_request==2);
    mc_demo_load(&e,u.load_request-1);assert(e.project.demo&&e.project.bpm==140);
    puts("PASS: four effect impulse/decay tests, native demo end, block identity and FX/demo UI");
}
static void stress(void) {
    clock_t begin=clock();mc_init(&e);e.project.bpm=240;e.project.swing=45;
    for(unsigned fx=0;fx<4;fx++)e.project.fx[fx]=127;
    for(unsigned p=0;p<3;p++) { mc_preset(&e,p,0);e.project.patch[p].resonance=127;e.project.patch[p].cutoff=127; }
    for(unsigned i=0;i<8;i++) mc_note(&e,i%3,84+i*3,127,MC_ACCENT,0);
    for(unsigned i=0;i<16;i++) mc_drum(&e,i,127);
    mc_start(&e);render_n(&e,MC_RATE*10);
    printf("PASS: 10s stress; engine %zu B, project %zu B, %.3fs host CPU, peak %u, clips %u\n",sizeof e,sizeof e.project,(double)(clock()-begin)/CLOCKS_PER_SEC,e.peak,e.clips);
}
int main(void) {
    assert(sizeof(McPatch)==12&&sizeof(McStep)==8&&sizeof(McProject)==MC_PROJECT_BYTES);
    engines();knob_audio();sequence();persistence();panel();effects_demo();stress();puts("ALL TESTS PASSED");return 0;
}
