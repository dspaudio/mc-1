/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/panel_modes.h"

/* app.c의 buttons[] 물리 ID를 구현과 독립적으로 명시한다. */
enum {
    F_FX=1u<<2,F_SEL=1u<<3,F_ENV=1u<<4,F_LFO=1u<<5,
    F_EDIT=1u<<6,F_GLO=1u<<7,F_HOME=1u<<8,F_SAVE=1u<<9,
    F_ARP=1u<<10,F_SEQ=1u<<11,F_PLAY=1u<<12,F_REC=1u<<13
};
static McEngine e;
static McUi u;

/* 바이트 배열이므로 구조체 패딩까지 보존하는 불변 스냅샷을 만든다. */
typedef struct {
    unsigned char engine[sizeof(McEngine)];
    unsigned char ui[sizeof(McUi)];
} Snapshot;

static Snapshot snapshot(void) {
    Snapshot result;
    memcpy(result.engine,&e,sizeof e);
    memcpy(result.ui,&u,sizeof u);
    return result;
}

static unsigned mask(void) {
    const Snapshot before=snapshot();
    unsigned result=mc_panel_mode_mask(&e,&u);
    assert(memcmp(before.engine,&e,sizeof e)==0);
    assert(memcmp(before.ui,&u,sizeof u)==0);
    assert((result&~0x3FFCu)==0);
    assert((result&F_SEL)==0);
    assert(((result&F_REC)!=0)==(e.recording!=0));
    assert(((result&F_PLAY)!=0)==(e.playing&&(!MC_PLAY_LED_PULSE||e.step%4u<2u)));
    return result;
}

static void init(void) {
    mc_init(&e);
    mc_ui_init(&u);
    e.project.demo=0;
    e.project.song_mode=0;
    e.project.song_loop=0;
    e.project.clock_source=0;
    e.selected=0;
}

static void press(unsigned button) { mc_ui_button(&e,&u,button); }

static void basic_modes(void) {
    init();
    assert(mask()==F_HOME);
    press(UI_SEL);
    assert(e.selected==1&&mask()==F_HOME);
    press(UI_ENV);
    assert(u.page==1&&mask()==F_ENV);
    press(UI_EDIT);
    assert(u.editing&&mask()==(F_ENV|F_EDIT));
    press(UI_EDIT);
    assert(!u.editing&&mask()==F_ENV);
    press(UI_ENV);
    assert(u.page==0&&mask()==F_HOME);
    unsigned accent=e.project.step[e.pattern][e.selected][e.edit_step].flags&MC_ACCENT;
    press(UI_LFO);
    assert((e.project.step[e.pattern][e.selected][e.edit_step].flags&MC_ACCENT)!=accent);
    assert(mask()==F_HOME);
    press(UI_EDIT);
    assert(mask()==(F_HOME|F_EDIT));
    press(UI_HOME);
    assert(!u.editing&&mask()==F_HOME);
    unsigned before=mask();
    press(UI_OCTDN);
    assert(u.octave==-1&&mask()==before);
    press(UI_OCTUP);
    assert(u.octave==0&&mask()==before);
}

static void fx_pages(void) {
    init();
    press(UI_FX);
    assert(u.page==3&&u.fx_page==0&&mask()==F_FX);
    for(unsigned part=0;part<MC_PARTS;part++) {
        for(unsigned subpage=0;subpage<2;subpage++) {
            assert(e.selected==part&&u.fx_page==subpage);
            assert(mask()==F_FX);
            press(UI_ENV);
        }
        assert(u.fx_page==0&&mask()==F_FX);
        press(UI_FX);
        assert(e.selected==part&&u.fx_page==0&&mask()==F_FX);
        press(UI_SEL);
    }
    assert(e.selected==0);
    press(UI_ENV);
    press(UI_HOME);
    press(UI_FX);
    assert(u.fx_page==0&&mask()==F_FX);
    press(UI_EDIT);
    assert(mask()==(F_FX|F_EDIT));
    press(UI_HOME);
    assert(mask()==F_HOME);
}

static void glo_pages(void) {
    static const unsigned pages[5]={5,2,4,7,9};
    init();
    press(UI_EDIT);
    for(unsigned i=0;i<5;i++) {
        press(UI_GLO);
        assert(u.page==pages[i]&&!u.editing&&mask()==F_GLO);
        if(u.page==7) {
            press(UI_EDIT);
            assert(!u.editing&&mask()==F_GLO);
        }
        if(u.page==9) {
            press(UI_EDIT);
            assert(!u.editing&&mask()==F_GLO);
        }
    }
    press(UI_GLO);
    assert(u.page==5&&mask()==F_GLO);
    unsigned mute=e.mute;
    press(UI_ARP);
    assert(e.mute==(mute^(1u<<e.selected)));
    assert(u.page==5&&mask()==F_GLO);
}

static void save_mode(void) {
    init();
    press(UI_SAVE);
    assert(u.save_pending&&mask()==(F_HOME|F_SAVE));
    press(UI_SAVE);
    assert(u.save_request&&!u.save_pending&&mask()==F_HOME);
    init();
    press(UI_SAVE);
    press(UI_HOME);
    assert(!u.save_pending&&mask()==F_HOME);
    press(UI_PLAY);
    assert(e.playing);
    press(UI_SAVE);
    assert(!u.save_pending&&!u.save_request&&mask()==(F_HOME|F_PLAY));
    press(UI_PLAY);
    assert(!e.playing&&mask()==F_HOME);
}

static void arp_mode(void) {
    init();
    e.project.arp[0].latch=0;
    press(UI_ARP);
    assert(u.page==6&&mask()==F_ARP);
    press(UI_LFO);
    assert(e.project.arp[0].latch&&mask()==(F_ARP|F_LFO));
    press(UI_LFO);
    assert(!e.project.arp[0].latch&&mask()==F_ARP);
    /* 드럼 설명 화면에서 latch 조작은 상태를 바꾸지 않는다. */
    press(UI_SEL);
    press(UI_SEL);
    press(UI_SEL);
    assert(e.selected==3&&u.page==6&&mask()==F_ARP);
    const Snapshot before=snapshot();
    press(UI_LFO);
    assert(memcmp(before.engine,&e,sizeof e)==0);
    assert(mask()==F_ARP);
    press(UI_ARP);
    assert(u.page==0&&mask()==F_HOME);
}

static void song_mode(void) {
    init();
    press(UI_SEQ);
    assert(e.project.song_mode&&u.page==8&&mask()==F_SEQ);
    press(UI_PLAY);
    assert(!e.playing&&mask()==F_SEQ);
    press(UI_EDIT);
    assert(u.song_editing&&mask()==(F_SEQ|F_EDIT));
    press(UI_LFO);
    assert(e.project.song_loop&&mask()==(F_SEQ|F_EDIT|F_LFO));
    press(UI_EDIT);
    assert(!u.song_editing&&mask()==(F_SEQ|F_LFO));
    press(UI_LFO);
    assert(!e.project.song_loop&&mask()==F_SEQ);
    press(UI_EDIT);
    press(UI_HOME);
    assert(u.song_editing&&!u.editing&&mask()==(F_HOME|F_SEQ));
    press(UI_SEQ);
    assert(!e.project.song_mode&&u.page==0&&mask()==F_HOME);
}

static void all_steps(void) {
    init();
    /* 재생·녹음의 모든 조합에서 16스텝을 검사한다. */
    for(unsigned playing=0;playing<2;playing++) {
        for(unsigned recording=0;recording<2;recording++) {
            e.playing=(uint8_t)playing;
            e.recording=(uint8_t)recording;
            for(unsigned step=0;step<16;step++) {
                e.step=(uint8_t)step;
                unsigned expected=F_HOME|(recording?F_REC:0u);
                if(playing&&(!MC_PLAY_LED_PULSE||step%4u==0u||step%4u==1u))expected|=F_PLAY;
                assert(mask()==expected);
            }
        }
    }
}

static void retained_transport(void) {
    static const unsigned buttons[]={
        UI_FX,UI_ENV,UI_EDIT,UI_GLO,UI_HOME,UI_ARP,UI_LFO,UI_SEL
    };
    for(unsigned phase=0;phase<4;phase++) {
        for(unsigned i=0;i<sizeof buttons/sizeof *buttons;i++) {
            init();
            press(UI_PLAY);
            press(UI_REC);
            e.step=(uint8_t)phase;
            press(buttons[i]);
            assert(e.playing&&e.recording&&e.step==phase);
            assert((mask()&(F_PLAY|F_REC))==(F_REC|(!MC_PLAY_LED_PULSE||phase<2?F_PLAY:0u)));
        }
    }
    /* SEQ와 클럭 소스 변경은 실제 API가 전송을 정지시킨다. */
    init();
    press(UI_PLAY);
    press(UI_REC);
    press(UI_SEQ);
    assert(!e.playing&&e.recording&&mask()==(F_SEQ|F_REC));
    assert(mc_song_set(&e,0,0,1)==0);
    assert(mc_song_length(&e,1)==0);
    press(UI_PLAY);
    assert(e.playing&&mask()==(F_SEQ|F_PLAY|F_REC));
    press(UI_PLAY);
    press(UI_REC);
    assert(!e.playing&&!e.recording&&mask()==F_SEQ);

    init();
    for(unsigned i=0;i<5;i++)press(UI_GLO);
    assert(u.page==9);
    press(UI_PLAY);
    press(UI_REC);
    press(UI_LFO);
    assert(e.project.clock_source==1&&!e.playing&&e.recording);
    assert(mask()==(F_GLO|F_REC));
}

static void render_frames(unsigned frames) {
    int16_t output[256];
    while(frames) {
        unsigned count=frames>128u?128u:frames;
        mc_render(&e,output,count);
        frames-=count;
    }
}

/* n번째 경계의 샘플 수. 짝수 경계에서 두 스텝의 swing이 상쇄된다. */
static unsigned boundary(unsigned bpm,unsigned swing,unsigned n) {
    uint64_t numerator=(uint64_t)MC_RATE*60u*(100u*n+(n%2u?swing:0u));
    unsigned denominator=bpm*400u;
    return (unsigned)((numerator+denominator-1u)/denominator);
}

static void internal_transport(unsigned bpm,unsigned swing) {
    init();
    e.project.bpm=(uint16_t)bpm;
    e.project.swing=(uint8_t)swing;
    press(UI_REC);
    press(UI_PLAY);
    assert(e.playing&&e.step==0&&mask()==(F_HOME|F_PLAY|F_REC));
    unsigned rendered=0;
    for(unsigned n=1;n<=16;n++) {
        unsigned next=boundary(bpm,swing,n);
        unsigned before=e.step;
        unsigned before_mask=mask();
        assert(next>rendered);
        render_frames(next-rendered-1u);
        assert(e.playing&&e.step==before&&mask()==before_mask);
        render_frames(1);
        rendered=next;
        assert(e.step==n%16u);
        unsigned expected=F_HOME|F_REC;
        if(!MC_PLAY_LED_PULSE||n%4u<2u)expected|=F_PLAY;
        assert(mask()==expected);
        if(n%2u==0u)assert(next==boundary(bpm,0,n));
    }
    assert(e.step==0);
    press(UI_PLAY);
    assert(!e.playing&&mask()==(F_HOME|F_REC));
    render_frames(257);
    assert(mask()==(F_HOME|F_REC));
    press(UI_PLAY);
    assert(e.playing&&e.step==0&&mask()==(F_HOME|F_PLAY|F_REC));
}

static void external_transport(unsigned bpm,unsigned swing) {
    init();
    e.project.bpm=(uint16_t)bpm;
    e.project.swing=(uint8_t)swing;
    mc_clock_source(&e,1);
    press(UI_REC);
    press(UI_PLAY);
    assert(e.playing&&e.step==0);
    /* 외부 클록 모드에서 샘플 시간만으로 스텝과 LED 위상이 진행하지 않는다. */
    render_frames(2048);
    assert(e.step==0&&mask()==(F_HOME|F_PLAY|F_REC));
    unsigned first=(6u*(100u+swing)+99u)/100u;
    for(unsigned tick=1;tick<=96;tick++) {
        mc_clock_tick(&e);
        unsigned step=(2u*(tick/12u)+(tick%12u>=first?1u:0u))%16u;
        assert(e.playing&&e.step==step);
        unsigned expected=F_HOME|F_REC;
        if(!MC_PLAY_LED_PULSE||step%4u<2u)expected|=F_PLAY;
        assert(mask()==expected);
    }
    press(UI_PLAY);
    assert(!e.playing&&mask()==(F_HOME|F_REC));
    mc_clock_tick(&e);
    assert(e.step==0&&mask()==(F_HOME|F_REC));
}

int main(void) {
    static const unsigned bpms[3]={96,120,240};
    static const unsigned swings[2]={0,45};
    basic_modes();
    fx_pages();
    glo_pages();
    save_mode();
    arp_mode();
    song_mode();
    all_steps();
    retained_transport();
    for(unsigned i=0;i<3;i++) {
        for(unsigned j=0;j<2;j++) {
            internal_transport(bpms[i],swings[j]);
            external_transport(bpms[i],swings[j]);
        }
    }
    puts("PASS: pages, nested states, immutable snapshots and physical button bounds");
    puts("PASS: configured PLAY mode, REC, 16 steps, 96/120/240 BPM, swing 0/45, internal/external clock");
    return 0;
}
