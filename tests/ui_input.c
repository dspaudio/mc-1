/* SPDX-License-Identifier: GPL-3.0-only */
#include "mcfm.h"
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static McEngine e;
static McUi u;
static uint16_t first[240*240],second[240*240];
static void draw(uint16_t *pixels) {
    for(unsigned y=0;y<240;y+=8)mc_ui_strip(&e,&u,y,8,pixels+y*240);
}
static void init(void) { mc_init(&e);mc_ui_init(&u);mc_preset(&e,0,0); }
static void timed(unsigned enc,int delta,uint32_t now) { mc_ui_encoder_at(&e,&u,enc,delta,now); }
static void acceleration(void) {
    init();timed(2,1,100);assert(u.preset[0]==1&&u.preset_popup);
    timed(2,1,159);assert(u.preset[0]==4);
    timed(2,1,219);assert(u.preset[0]==5);
    timed(2,-1,220);assert(u.preset[0]==4);
    timed(2,-1,221);assert(u.preset[0]==1);
    timed(2,1,500);assert(u.preset[0]==2);
    timed(1,1,501);assert(!u.preset_popup);timed(2,1,502);assert(u.preset[0]==3);
    mc_ui_button(&e,&u,UI_HOME);timed(2,1,503);assert(u.preset[0]==4);
    mc_ui_key(&e,&u,0,1);timed(2,1,504);assert(u.preset[0]==5);
    mc_ui_encoder(&e,&u,2,1);assert(!u.preset_popup&&u.preset[0]==6);
    timed(2,1,505);assert(u.preset[0]==7);
    mc_preset(&e,1,10);e.selected=1;timed(2,1,506);assert(u.preset[1]==11);
    u.page=1;timed(2,1,507);assert(u.preset[1]==12);
    init();u.page=2;timed(3,1,0);assert(e.project.bpm==119);
    timed(3,1,59);assert(e.project.bpm==125);
    timed(3,1,119);assert(e.project.bpm==126);
    timed(3,-1,120);assert(e.project.bpm==125);
    timed(3,-1,121);assert(e.project.bpm==119);
    timed(3,127,122);assert(e.project.bpm==240);
    init();timed(2,1,UINT32_MAX-20);timed(2,1,10);assert(u.preset[0]==4);
    puts("PASS: first/59ms/60ms/idle/reversal/encoder/button/key/context/raw reset and uint32 wrap; BPM6x, presets3x");
}
static void small_ranges(void) {
    init();timed(0,1,1);timed(0,1,2);assert(e.selected==2);
    init();e.selected=3;timed(2,1,1);timed(2,1,2);assert(e.project.drum_kit==2);
    timed(3,1,3);timed(3,1,4);assert(u.sound==2);
    timed(4,1,5);timed(4,1,6);assert(e.edit_step==2);
    init();u.editing=1;timed(3,1,1);timed(3,1,2);assert(e.edit_step==2);
    unsigned flags=e.project.step[0][0][2].flags;timed(6,1,3);timed(6,1,4);
    assert(e.project.step[0][0][2].flags==(flags+2)%4);
    init();u.page=4;timed(3,1,1);timed(3,1,2);assert(u.demo_choice==1);
    puts("PASS: parts/kits/lanes/steps/flags/demo choices never accelerate");
}
static void metronome_ui(void) {
    init();assert(!e.project.metronome);
    mc_ui_button(&e,&u,UI_GLO);mc_ui_button(&e,&u,UI_GLO);assert(u.page==2);
    uint8_t levels[MC_PARTS];memcpy(levels,e.project.level,sizeof levels);draw(first);
    timed(6,1,100);assert(e.project.metronome&&!memcmp(levels,e.project.level,sizeof levels));
    draw(second);assert(memcmp(first+140*240,second+140*240,7*240*sizeof(uint16_t)));
    assert(first[140*240+14]==0xdefbu&&second[140*240+14]==0x67e8u);
    timed(6,1,101);assert(e.project.metronome==1);
    timed(6,-1,102);assert(!e.project.metronome);
    timed(6,-1,103);assert(!e.project.metronome);
    mc_ui_encoder(&e,&u,6,127);assert(e.project.metronome==1);
    mc_ui_encoder(&e,&u,6,-127);assert(!e.project.metronome);
    e.playing=e.recording=1;
    for(unsigned part=0;part<MC_PARTS;part++) {
        e.selected=(uint8_t)part;mc_ui_encoder(&e,&u,6,1);assert(e.project.metronome);
        mc_ui_encoder(&e,&u,6,-1);assert(!e.project.metronome);
    }
    assert(!e.project.automation_count&&!memcmp(levels,e.project.level,sizeof levels));
    mc_ui_encoder(&e,&u,6,1);mc_ui_button(&e,&u,UI_HOME);
    mc_ui_button(&e,&u,UI_GLO);assert(u.page==5);
    mc_ui_encoder(&e,&u,6,1);assert(e.project.level[3]==levels[3]+1&&e.project.metronome);
    mc_ui_button(&e,&u,UI_GLO);assert(u.page==2&&e.project.metronome);
    puts("PASS: GLO/GROOVE METRO default-off, directional0/1 clamp, rapid turns, visible greenON, all-part global setting, no automation, mixer level retained");
}
static void song_midi_ui(void) {
    init();mc_ui_button(&e,&u,UI_SEQ);assert(u.page==8&&e.project.song_mode&&!u.editing);
    mc_ui_button(&e,&u,UI_PLAY);assert(!e.playing&&u.message);
    mc_ui_encoder(&e,&u,4,1);assert(!e.project.song[0].pattern&&u.message);
    mc_ui_button(&e,&u,UI_EDIT);assert(u.song_editing&&!u.editing);
    mc_ui_encoder(&e,&u,3,-1);assert(u.song_cursor==127);
    mc_ui_encoder(&e,&u,4,-1);assert(e.project.song[127].pattern==63);
    mc_ui_encoder(&e,&u,5,-1);assert(e.project.song[127].repeats==16);
    mc_ui_encoder(&e,&u,6,128);assert(e.project.song_length==128);
    draw(first);mc_ui_encoder(&e,&u,4,1);assert(e.project.song[127].pattern==0);
    draw(second);assert(memcmp(first,second,sizeof first));
    mc_ui_encoder(&e,&u,5,1);assert(e.project.song[127].repeats==1);
    mc_ui_encoder(&e,&u,3,1);assert(!u.song_cursor);
    timed(5,1,100);timed(5,1,101);assert(e.project.song[0].repeats==3);
    timed(4,1,102);timed(4,1,103);assert(e.project.song[0].pattern==4);
    u.song_cursor=1;timed(4,1,104);assert(e.project.song[1].pattern==1);
    timed(3,1,105);timed(3,1,106);assert(u.song_cursor==5);
    mc_ui_button(&e,&u,UI_LFO);assert(e.project.song_loop);
    mc_ui_button(&e,&u,UI_LFO);assert(!e.project.song_loop);
    McStep before=e.project.step[e.pattern][0][e.edit_step];
    mc_ui_key(&e,&u,0,1);mc_ui_key(&e,&u,0,0);
    assert(!memcmp(&before,&e.project.step[e.pattern][0][e.edit_step],sizeof before));
    mc_ui_button(&e,&u,UI_PLAY);assert(e.playing&&e.pattern==4);
    mc_ui_button(&e,&u,UI_SEQ);assert(!e.project.song_mode&&!e.playing&&u.page==0);
    mc_ui_button(&e,&u,UI_EDIT);assert(u.editing);
    mc_ui_button(&e,&u,UI_ENV);assert(u.page==1);
    for(unsigned k=0;k<4;k++) {
        int value=mc_parameter_get(&e,0,6+k);mc_ui_encoder(&e,&u,3+k,-1);
        assert(mc_parameter_get(&e,0,6+k)==(value?value-1:0));
    }
    for(unsigned n=0;n<5;n++)mc_ui_button(&e,&u,UI_GLO);
    assert(u.page==9&&!u.editing);
    for(unsigned part=0;part<4;part++) {
        uint8_t channels[4];memcpy(channels,e.project.midi_channel,sizeof channels);
        e.project.midi_channel[part]=15;timed(3+part,1,200+part*2);assert(e.project.midi_channel[part]==0);
        timed(3+part,1,201+part*2);assert(e.project.midi_channel[part]==1);
        mc_ui_encoder(&e,&u,3+part,-2);assert(e.project.midi_channel[part]==15);
        for(unsigned j=0;j<4;j++)if(j!=part)assert(e.project.midi_channel[j]==channels[j]);
    }
    mc_ui_button(&e,&u,UI_EDIT);assert(!u.editing);
    draw(first);mc_ui_button(&e,&u,UI_PLAY);assert(e.playing);draw(second);assert(memcmp(first,second,sizeof first));
    mc_ui_button(&e,&u,UI_LFO);assert(e.project.clock_source==1&&!e.playing);
    mc_ui_button(&e,&u,UI_LFO);assert(e.project.clock_source==2);
    mc_ui_button(&e,&u,UI_LFO);assert(!e.project.clock_source);
    e.project.clock_source=3;draw(first);mc_ui_button(&e,&u,UI_LFO);assert(!e.project.clock_source);
    mc_ui_button(&e,&u,UI_GLO);assert(u.page==5);
    init();mc_ui_button(&e,&u,UI_SEQ);mc_ui_button(&e,&u,UI_EDIT);
    mc_ui_encoder(&e,&u,6,128);mc_ui_encoder(&e,&u,4,-1);mc_ui_encoder(&e,&u,5,1);
    mc_ui_encoder(&e,&u,3,-1);mc_ui_encoder(&e,&u,4,62);
    mc_clock_source(&e,1);e.project.swing=0;mc_ui_button(&e,&u,UI_PLAY);
    assert(e.pattern==63&&!e.song_position&&!e.song_repeat);
    for(unsigned n=0;n<96;n++)mc_clock_tick(&e);
    assert(!e.song_position&&e.song_repeat==1&&e.pattern==63);
    for(unsigned n=0;n<96*127;n++)mc_clock_tick(&e);
    assert(e.song_position==127&&e.pattern==62&&e.playing);draw(first);
    for(unsigned n=0;n<96;n++)mc_clock_tick(&e);
    assert(!e.playing&&e.song_finished);draw(second);assert(memcmp(first,second,sizeof first));
    assert(second[49*240+14]==0x4208u);
    mc_ui_button(&e,&u,UI_LFO);mc_ui_button(&e,&u,UI_PLAY);
    for(unsigned n=0;n<96*129;n++)mc_clock_tick(&e);
    assert(e.playing&&!e.song_position&&e.pattern==63&&!e.song_repeat);
    puts("PASS: SONG mode/empty/lock/128 positions/64 patterns/16 repeats/length/loop, timed context, note edit isolation; MIDI channels/clock/BLE unavailable; ADSR retained");
}
static void render_frames(unsigned frames) {
    int16_t audio[256];
    while(frames) { unsigned n=frames>128?128:frames;mc_render(&e,audio,n);frames-=n; }
}
static void digit(unsigned x,unsigned y,const uint8_t rows[7],uint16_t color) {
    for(unsigned row=0;row<7;row++)for(unsigned col=0;col<5;col++)
        assert((first[(y+row)*240+x+col]==color)==((rows[row]&(1u<<(4-col)))!=0));
}
static void big_digit(unsigned x,unsigned y,const uint8_t rows[7],uint16_t color) {
    for(unsigned row=0;row<14;row++)for(unsigned col=0;col<10;col++)
        assert((first[(y+row)*240+x+col]==color)==((rows[row/2]&(1u<<(4-col/2)))!=0));
}
static void patterns64(void) {
    static const uint8_t six[7]={14,16,16,30,17,17,14},four[7]={2,6,10,18,31,2,2};
    static const uint8_t one[7]={4,12,4,4,4,4,14},slash[7]={1,2,2,4,8,8,16};
    assert(MC_PATTERNS==64);
    for(unsigned part=0;part<MC_PARTS;part++) {
        init();e.selected=(uint8_t)part;
        mc_ui_encoder(&e,&u,1,-1);assert(e.pattern==63);
        mc_ui_encoder(&e,&u,1,1);assert(e.pattern==0);
        mc_ui_encoder(&e,&u,1,63);assert(e.pattern==63);
        mc_ui_encoder(&e,&u,1,1);assert(e.pattern==0);
        u.page=2;mc_ui_encoder(&e,&u,5,-1);assert(e.pattern==63);
        mc_ui_encoder(&e,&u,5,1);assert(e.pattern==0);
    }
    init();timed(1,1,100);assert(e.pattern==1);
    timed(1,1,159);assert(e.pattern==4);
    timed(1,1,219);assert(e.pattern==5);
    timed(1,-1,220);assert(e.pattern==4);
    timed(1,-1,221);assert(e.pattern==1);
    mc_ui_encoder(&e,&u,1,-1);timed(1,-1,222);assert(e.pattern==63);
    timed(1,-1,223);assert(e.pattern==60);
    mc_ui_button(&e,&u,UI_HOME);timed(1,1,224);assert(e.pattern==61);
    timed(1,1,225);assert(e.pattern==0);
    init();u.page=2;timed(5,-1,1);assert(e.pattern==63);
    timed(5,-1,2);assert(e.pattern==60);
    timed(5,1,3);assert(e.pattern==61);
    timed(5,1,4);assert(e.pattern==0);
    init();u.editing=1;u.fx_page=1;e.pattern=4;
    unsigned before=e.project.step[4][0][0].velocity;timed(4,1,1);
    assert(e.project.step[4][0][0].velocity==before+1);
    e.pattern=0;before=e.project.step[0][0][0].velocity;timed(4,1,2);
    assert(e.project.step[0][0][0].velocity==before+1);
    init();e.project.bpm=120;e.project.swing=0;mc_start(&e);
    mc_ui_encoder(&e,&u,1,-1);assert(e.pattern==0&&e.queued==63);
    draw(first);digit(196,14,six,0xfb20u);digit(202,14,four,0xfb20u);
    digit(52,173,one,0x67e8u);digit(58,173,slash,0x67e8u);
    digit(64,173,six,0x67e8u);digit(70,173,four,0x67e8u);
    mc_ui_encoder(&e,&u,1,1);assert(e.queued==0);
    mc_ui_encoder(&e,&u,1,-1);assert(e.queued==63);
    render_frames(MC_RATE*2-1);assert(e.pattern==0&&e.step==15);
    render_frames(1);assert(e.pattern==63&&e.step==0&&e.queued==255);
    draw(first);big_digit(8,30,six,0x67e8u);big_digit(20,30,four,0x67e8u);
    big_digit(44,30,six,0x67e8u);big_digit(56,30,four,0x67e8u);
    digit(52,173,six,0x67e8u);digit(58,173,four,0x67e8u);
    digit(64,173,slash,0x67e8u);
    digit(70,173,six,0x67e8u);digit(76,173,four,0x67e8u);
    mc_ui_encoder(&e,&u,1,1);assert(e.pattern==63&&e.queued==0);
    render_frames(MC_RATE*2);assert(e.pattern==0&&e.step==0&&e.queued==255);
    init();mc_start(&e);e.recording=1;mc_ui_encoder(&e,&u,3,1);
    e.pattern=63;mc_ui_encoder(&e,&u,3,1);assert(e.project.automation_count==2);
    for(unsigned n=0;n<4;n++)mc_ui_button(&e,&u,UI_GLO);
    draw(first);digit(68,100,six,0xdefbu);digit(74,100,four,0xdefbu);
    mc_ui_button(&e,&u,UI_EDIT);assert(e.project.automation_count==1);
    assert(e.project.automation[0].address/5==0);
    init();memset(e.project.step,0,sizeof e.project.step);e.pattern=63;
    e.project.step[63][0][0]=(McStep){.note={48,60,72},.velocity=100};
    draw(first);assert(first[61*240+32]==0x0841&&first[69*240+32]==0x0841&&first[76*240+32]==0x0841);
    puts("PASS: all-part 1/64 wraps, pattern3x/59/60ms/reversal/reset, queued bar 1/64, two-digit LCD, high-pattern context/pitch/automation clear");
}
static void popup(void) {
    init();timed(2,1,100);draw(first);assert(first[135*240+10]==0xfb20u);
    assert(!memcmp(&e.project.patch[0],&mc_presets[1],sizeof(McPatch)));
    mc_ui_tick(&u,599);assert(u.preset_popup);draw(second);assert(!memcmp(first,second,sizeof first));
    timed(2,0,599);timed(7,1,599);assert(u.preset_at==100);
    mc_ui_tick(&u,600);assert(!u.preset_popup);draw(second);assert(memcmp(first,second,sizeof first));
    timed(2,1,1000);timed(2,1,1499);mc_ui_tick(&u,1998);assert(u.preset_popup);
    mc_ui_tick(&u,1999);assert(!u.preset_popup);
    timed(2,1,UINT32_MAX-100);mc_ui_tick(&u,398);assert(u.preset_popup);
    mc_ui_tick(&u,399);assert(!u.preset_popup);
    timed(2,1,100);timed(3,1,101);assert(!u.preset_popup);
    timed(2,1,102);mc_ui_button(&e,&u,UI_PLAY);assert(e.playing&&!u.preset_popup);
    timed(2,1,103);mc_ui_key(&e,&u,1,1);assert(!u.preset_popup);
    e.selected=3;timed(2,1,104);assert(u.preset_popup);draw(first);
    mc_ui_tick(&u,604);assert(!u.preset_popup);draw(second);assert(memcmp(first,second,sizeof first));
    uint16_t strip[240*8+2];for(unsigned i=0;i<240*8+2;i++)strip[i]=0x1234;
    for(unsigned p=0;p<MC_PRESETS;p++) {
        e.selected=0;mc_preset(&e,0,p);timed(2,1,p*1000);
        for(unsigned y=0;y<240;y+=8) { mc_ui_strip(&e,&u,y,8,strip+1);assert(strip[0]==0x1234&&strip[240*8+1]==0x1234); }
    }
    puts("PASS: seven-row actual C popup, immediate patch,499/500ms, refresh/zero/invalid/wrap, controls dismiss, all128 strip guards");
}
static void performance_ui(void) {
    init();mc_ui_button(&e,&u,UI_ARP);assert(u.page==6&&!e.mute);
    mc_ui_encoder(&e,&u,3,1);assert(e.project.arp[0].enabled&&e.project.arp[0].mode==MC_ARP_UP);
    mc_ui_encoder(&e,&u,4,1);mc_ui_encoder(&e,&u,5,1);mc_ui_encoder(&e,&u,6,10);
    assert(e.project.arp[0].rate==0&&e.project.arp[0].octaves==2&&e.project.arp[0].gate==85);
    mc_ui_key(&e,&u,0,1);assert(e.arp[0].count==1);
    mc_ui_button(&e,&u,UI_LFO);assert(e.project.arp[0].latch&&!e.arp[0].count);
    mc_ui_key(&e,&u,0,1);mc_ui_encoder(&e,&u,0,1);assert(e.selected==1&&!e.arp[0].count);
    mc_ui_button(&e,&u,UI_ARP);assert(u.page==0);
    mc_ui_button(&e,&u,UI_GLO);mc_ui_button(&e,&u,UI_ARP);assert(u.page==5&&e.mute==2);
    init();mc_start(&e);e.recording=1;mc_ui_encoder(&e,&u,3,1);assert(e.project.automation_count==1);
    mc_ui_button(&e,&u,UI_ENV);mc_ui_encoder(&e,&u,3,1);assert(e.project.automation_count==2);
    mc_ui_button(&e,&u,UI_GLO);mc_ui_encoder(&e,&u,4,1);assert(e.project.automation_count==3);
    mc_ui_button(&e,&u,UI_FX);mc_ui_encoder(&e,&u,3,1);assert(e.project.automation_count==4);
    mc_ui_button(&e,&u,UI_ENV);mc_ui_encoder(&e,&u,3,1);assert(e.project.automation_count==5);
    mc_ui_button(&e,&u,UI_ENV);mc_ui_encoder(&e,&u,4,1);assert(e.project.automation_count==6);
    mc_ui_button(&e,&u,UI_HOME);e.selected=3;mc_ui_encoder(&e,&u,6,1);assert(e.project.automation_count==7);
    for(unsigned n=0;n<4;n++)mc_ui_button(&e,&u,UI_GLO);
    assert(u.page==7);mc_ui_button(&e,&u,UI_EDIT);assert(!e.project.automation_count&&!u.editing);
    init();draw(first);mc_ui_encoder(&e,&u,3,1);draw(second);
    assert(e.project.preset[0]==0&&memcmp(first+30*240,second+30*240,14*240*sizeof(uint16_t)));
    unsigned star=0;for(unsigned y=30;y<44;y++)for(unsigned x=84;x<94;x++)star+=second[y*240+x]==0xdefbu;
    assert(star);mc_ui_encoder(&e,&u,3,-1);draw(second);assert(!memcmp(first,second,sizeof first));
    e.project.patch[0]=mc_presets[1];draw(second);assert(e.project.preset[0]==0);
    for(unsigned y=30;y<44;y++)assert(!memcmp(first+y*240+84,second+y*240+96,120*sizeof(uint16_t)));
    puts("PASS: ARP page/settings/clear, mixer mute, knob automation and pattern clear, origin-name star");
}
static void shared_fx_ui(void) {
    static McEngine before;
    static McProject expected;
    init();
    for(unsigned part=0;part<4;part++)for(unsigned k=0;k<2;k++)
        assert(!mc_parameter_set(&e,part,MC_PARAM_RD_SEND+k,8+part*16+k*4));
    for(unsigned k=0;k<2;k++)assert(!mc_parameter_set(&e,MC_PARTS,MC_PARAM_RD_TYPE+k,k));
    mc_ui_button(&e,&u,UI_FX);assert(u.page==3&&u.fx_page==UI_FX_SENDS);
    for(unsigned part=0;part<4;part++) {
        assert(e.selected==part);
        for(unsigned k=0;k<2;k++) {
            memcpy(&expected,&e.project,sizeof expected);
            mc_ui_encoder(&e,&u,3+k,1);expected.fx_send[part][k]++;
            assert(!memcmp(&expected,&e.project,sizeof expected));
            mc_ui_encoder(&e,&u,3+k,-1000);assert(mc_parameter_get(&e,part,MC_PARAM_RD_SEND+k)==0);
            mc_ui_encoder(&e,&u,3+k,1000);assert(mc_parameter_get(&e,part,MC_PARAM_RD_SEND+k)==127);
            mc_ui_encoder(&e,&u,3+k,1);assert(mc_parameter_get(&e,part,MC_PARAM_RD_SEND+k)==127);
            mc_ui_encoder(&e,&u,3+k,-127);
            timed(3+k,1,100);assert(mc_parameter_get(&e,part,MC_PARAM_RD_SEND+k)==1);
            timed(3+k,1,101);assert(mc_parameter_get(&e,part,MC_PARAM_RD_SEND+k)==4);
        }
        draw(first);mc_ui_button(&e,&u,UI_ENV);assert(u.fx_page==UI_FX_TYPES);
        draw(second);assert(memcmp(first+30*240,second+30*240,132*240*sizeof(uint16_t)));
        for(unsigned k=0;k<2;k++) {
            assert(!mc_parameter_set(&e,MC_PARTS,MC_PARAM_RD_TYPE+k,0));
            memcpy(&expected,&e.project,sizeof expected);
            mc_ui_encoder(&e,&u,3+k,-127);assert(mc_parameter_get(&e,MC_PARTS,MC_PARAM_RD_TYPE+k)==0);
            timed(3+k,1,100);timed(3+k,1,101);expected.fx_bus_type[k]=2;
            assert(!memcmp(&expected,&e.project,sizeof expected));
            mc_ui_encoder(&e,&u,3+k,127);assert(mc_parameter_get(&e,MC_PARTS,MC_PARAM_RD_TYPE+k)==7);
            mc_ui_encoder(&e,&u,3+k,1);assert(mc_parameter_get(&e,MC_PARTS,MC_PARAM_RD_TYPE+k)==7);
            for(unsigned other=0;other<4;other++)assert(mc_parameter_get(&e,other,MC_PARAM_RD_TYPE+k)==7);
        }
        for(unsigned page=0;page<2;page++) {
            assert(u.fx_page==(page?UI_FX_SENDS:UI_FX_TYPES));
            e.playing=e.recording=1;memcpy(&before,&e,sizeof before);
            for(unsigned enc=5;enc<=6;enc++) {
                mc_ui_encoder(&e,&u,enc,127);mc_ui_encoder(&e,&u,enc,-127);
                timed(enc,1,100);timed(enc,1,101);timed(enc,-1,102);
            }
            mc_ui_button(&e,&u,UI_FX);
            assert(u.page==3&&u.fx_page==(page?UI_FX_SENDS:UI_FX_TYPES)&&e.selected==part);
            assert(!memcmp(&before,&e,sizeof e));
            e.playing=e.recording=0;
            mc_ui_button(&e,&u,UI_ENV);
        }
        assert(u.fx_page==UI_FX_TYPES);
        mc_ui_button(&e,&u,UI_HOME);mc_ui_button(&e,&u,UI_FX);
        assert(u.page==3&&u.fx_page==UI_FX_SENDS);
        mc_ui_encoder(&e,&u,0,1);
    }
    assert(!e.selected);
    init();e.project.fx_bus_type[0]=0; /* fixture counts type steps from the legacy zero default */
    e.selected=2;mc_start(&e);e.recording=1;
    mc_ui_button(&e,&u,UI_FX);
    mc_ui_encoder(&e,&u,3,21);mc_ui_encoder(&e,&u,4,31);
    assert(e.project.automation_count==2);
    for(unsigned k=0;k<2;k++) {
        assert(e.project.automation[k].address==2);
        assert(e.project.automation[k].parameter==MC_PARAM_RD_SEND+k);
    }
    mc_ui_button(&e,&u,UI_ENV);
    mc_ui_encoder(&e,&u,3,1);mc_ui_encoder(&e,&u,4,2);
    assert(e.project.automation_count==4);
    for(unsigned k=0;k<2;k++) {
        assert(e.project.automation[k+2].address==MC_PARTS);
        assert(e.project.automation[k+2].parameter==MC_PARAM_RD_TYPE+k);
    }
    for(unsigned part=0;part<4;part++) {
        e.selected=(uint8_t)part;mc_ui_encoder(&e,&u,3,1);
        assert(e.project.automation_count==4&&e.project.automation[2].address==MC_PARTS);
    }
    assert(e.project.automation[2].value==5);
    init();u.editing=1;unsigned flags=e.project.step[0][0][0].flags;
    mc_ui_button(&e,&u,UI_FX);assert(u.page==0&&e.project.step[0][0][0].flags==(flags^MC_SLIDE));
    mc_ui_button(&e,&u,UI_FX);assert(e.project.step[0][0][0].flags==flags);
    puts("PASS: independent four-part R/D C/F sends, 0/127 clamp and acceleration, shared type0/7 clamp without acceleration, SENDS/TYPES toggle, no master scope, unused knobs inert even recording, canonical automation, EDIT-FX slide retained");
}
static void green_feedback(void) {
    init();e.playing=1;e.step=0;e.mute=0;e.project.step[0][0][0].velocity=100;e.project.step[0][0][0].note[0]=60;
    draw(first);assert(first[60*240+29]==0x67e8u);
    e.project.step[0][0][0].velocity=0;draw(first);assert(first[60*240+29]==0x2104u);
    e.project.step[0][0][0].velocity=100;e.mute=1;draw(first);assert(first[60*240+29]!=0x67e8u);
    e.mute=0;e.project.step[0][3][0].drums=1;e.project.step[0][3][0].velocity=0;
    draw(first);assert(first[141*240+29]==0x2104u);
    e.project.step[0][3][0].velocity=100;draw(first);assert(first[141*240+29]==0x67e8u);
    e.mute=8;draw(first);assert(first[141*240+29]!=0x67e8u);
    e.selected=3;e.mute=0;e.project.step[0][3][0].velocity=100;
    draw(first);assert(first[26*240+29]==0x67e8u&&first[35*240+29]!=0x67e8u);
    e.project.step[0][3][0].velocity=0;draw(first);assert(first[26*240+29]!=0x67e8u);
    e.project.step[0][3][0].velocity=100;e.mute=8;draw(first);assert(first[26*240+29]!=0x67e8u);
    puts("PASS: green cursor only on active unmuted melodic/drum hits; silent steps remain dark");
}
static void strip_equivalence(void) {
    init();e.playing=1;
    for(unsigned page=0;page<10;page++)for(unsigned part=0;part<4;part++) {
        u.page=(uint8_t)page;
        e.selected=(uint8_t)part;
        for(unsigned step=0;step<16;step++) {
            e.step=(uint8_t)step;
            u.pitch_ready=0;
            for(unsigned y=0;y<240;y++)mc_ui_strip(&e,&u,y,1,first+y*240);
            mc_ui_prepare_frame(&e,&u);
            McUi before;memcpy(&before,&u,sizeof before);
            draw(second);
            assert(!memcmp(first,second,sizeof first));
            assert(!memcmp(&before,&u,sizeof u));
        }
    }
    puts("PASS: uncached one-row and prepared eight-row pixels match on 10 pages, 4 parts and 16 steps; rendering is pure");
}
static void midi_drain(void) { McMidiMessage m;while(mc_midi_pop(&e,&m)) {} }
static void midi_expect(unsigned status,unsigned note,unsigned velocity) {
    McMidiMessage m;
    assert(mc_midi_pop(&e,&m)&&m.length==3);
    assert(m.data[0]==status&&m.data[1]==note&&m.data[2]==velocity);
}
static void key_velocity(void) {
    static McEngine before;
    const unsigned invalid[]={0,128,256,~0u};
    for(unsigned part=0;part<4;part++)for(unsigned arp=0;arp<2;arp++) {
        init();e.selected=(uint8_t)part;e.recording=1;u.editing=1;
        if(part<3)e.project.arp[part].enabled=(uint8_t)arp;
        u.preset_popup=u.encoder_active=u.save_pending=1;u.message="pending";
        McUi ui_before;memcpy(&ui_before,&u,sizeof u);memcpy(&before,&e,sizeof e);
        for(unsigned i=0;i<sizeof invalid/sizeof *invalid;i++) {
            mc_ui_key_velocity(&e,&u,0,invalid[i],1);
            assert(!memcmp(&before,&e,sizeof e)&&!memcmp(&ui_before,&u,sizeof u));
        }
        mc_ui_key_velocity(&e,&u,27,100,1);
        assert(!memcmp(&before,&e,sizeof e)&&!memcmp(&ui_before,&u,sizeof u));
    }
    for(unsigned velocity=1;velocity<=127;velocity+=126)for(unsigned part=0;part<3;part++) {
        init();e.selected=(uint8_t)part;u.octave=velocity==1?-3:3;
        unsigned key=velocity==1?0:26,note=velocity==1?17:108;
        midi_drain();mc_ui_key_velocity(&e,&u,key,velocity,1);
        assert(e.voice[0].active&&e.voice[0].part==part&&e.voice[0].note==note&&e.voice[0].velocity==velocity);
        midi_expect(0x90u+e.project.midi_channel[part],note,velocity);
        mc_ui_key_velocity(&e,&u,key,velocity==1?0:~0u,0);
        assert(e.voice[0].stage==2&&!e.voice[0].midi_out);
        midi_expect(0x80u+e.project.midi_channel[part],note,0);
        assert(!e.midi.count);
    }
    for(unsigned playing=0;playing<2;playing++)for(unsigned velocity=1;velocity<=127;velocity+=126) {
        init();e.selected=2;e.pattern=63;e.edit_step=15;e.step=4;
        e.playing=(uint8_t)playing;e.recording=(uint8_t)playing;u.editing=(uint8_t)!playing;
        McStep *s=&e.project.step[63][2][playing?4:15];
        *s=(McStep){.note={60,64,67},.flags=MC_TIE|MC_SLIDE|MC_ACCENT};
        midi_drain();mc_ui_key_velocity(&e,&u,3,velocity,1);
        assert(s->note[0]==56&&!s->note[1]&&!s->note[2]&&s->velocity==velocity&&s->gate==75);
        assert(s->flags==(MC_SLIDE|MC_ACCENT)&&e.edit_step==(playing?15:0));
        midi_expect(0x92,56,velocity);
        McStep recorded=*s;mc_ui_key_velocity(&e,&u,3,128,0);
        assert(!memcmp(&recorded,s,sizeof recorded));
        midi_expect(0x82,56,0);
    }
    puts("PASS: velocity 1/127 live notes, selected part/octave/clamp, independent release, invalid note-on state isolation, direct edit/live recording and MIDI");
}
static void key_velocity_arp_drums(void) {
    for(unsigned velocity=1;velocity<=127;velocity+=126) {
        init();memset(e.project.step,0,sizeof e.project.step);
        e.selected=1;e.edit_step=7;e.recording=1;u.editing=1;
        e.project.arp[1]=(McArp){.enabled=1,.rate=2,.octaves=1,.gate=75};
        midi_drain();mc_ui_key_velocity(&e,&u,5,velocity,1);
        assert(e.arp[1].count==1&&e.arp[1].notes[0]==58&&e.arp[1].velocity[0]==velocity);
        assert(!e.project.step[0][1][7].velocity&&e.edit_step==7&&!e.midi.count);
        mc_start(&e);
        McMidiMessage m;assert(mc_midi_pop(&e,&m)&&m.length==1&&m.data[0]==0xfa);
        midi_expect(0x91,58,velocity);
        assert(e.arp[1].events==1&&e.voice[0].velocity==velocity);
        assert(e.project.step[0][1][0].velocity==velocity&&e.project.step[0][1][0].note[0]==58);
        assert(!e.project.step[0][1][7].velocity&&e.edit_step==7);
        mc_ui_key_velocity(&e,&u,5,~0u,0);
        assert(!e.arp[1].count&&e.voice[0].stage==2);
        midi_expect(0x81,58,0);assert(!e.midi.count);
        init();e.project.arp[0].enabled=e.project.arp[0].latch=1;
        mc_ui_key_velocity(&e,&u,0,velocity,1);mc_ui_key_velocity(&e,&u,0,0,0);
        assert(e.arp[0].count==1&&!e.arp[0].held[0]&&e.arp[0].velocity[0]==velocity);
        for(unsigned mode=0;mode<3;mode++) {
            init();e.selected=3;e.pattern=63;e.edit_step=9;e.step=4;
            u.editing=(uint8_t)(mode==1);e.playing=e.recording=(uint8_t)(mode==2);
            McStep *s=&e.project.step[63][3][mode==2?4:9];memset(s,0,sizeof *s);
            midi_drain();mc_ui_key_velocity(&e,&u,26,velocity,1);
            assert(u.sound==10&&e.drum[0].sound==10&&e.drum[0].velocity==velocity&&e.drum[0].env);
            midi_expect(0x99,mc_midi_drums[10],velocity);midi_expect(0x89,mc_midi_drums[10],0);
            assert(s->drums==(mode?1u<<10:0)&&s->velocity==(mode?velocity:0)&&s->gate==(mode?60:0));
            McStep recorded=*s;mc_ui_key_velocity(&e,&u,26,~0u,0);
            assert(!memcmp(&recorded,s,sizeof recorded)&&!e.midi.count&&e.edit_step==9);
            if(mode) {
                mc_ui_key_velocity(&e,&u,26,velocity,1);
                assert(!s->drums&&s->velocity==velocity&&s->gate==60);
            }
        }
    }
    puts("PASS: ARP held/latch velocity, generated notes/record/MIDI without direct double recording, drum live/edit/record/toggle and MIDI");
}
static void key_legacy_equality(void) {
    static McEngine before,expected;
    for(unsigned part=0;part<4;part++)for(unsigned mode=0;mode<5;mode++) {
        init();e.selected=(uint8_t)part;e.edit_step=15;u.octave=-2;
        u.editing=(uint8_t)(mode==1);e.recording=(uint8_t)(mode>=2);e.playing=(uint8_t)(mode==3);
        if(mode==4&&part<3)e.project.arp[part].enabled=1;
        u.preset_popup=u.encoder_active=1;
        memcpy(&before,&e,sizeof e);McUi ui_before,ui_expected;memcpy(&ui_before,&u,sizeof u);
        mc_ui_key(&e,&u,26,1);mc_ui_key(&e,&u,26,0);mc_ui_key(&e,&u,27,1);
        memcpy(&expected,&e,sizeof e);memcpy(&ui_expected,&u,sizeof u);
        memcpy(&e,&before,sizeof e);memcpy(&u,&ui_before,sizeof u);
        mc_ui_key_velocity(&e,&u,26,100,1);mc_ui_key_velocity(&e,&u,26,100,0);mc_ui_key_velocity(&e,&u,27,100,1);
        assert(!memcmp(&expected,&e,sizeof e)&&!memcmp(&ui_expected,&u,sizeof u));
    }
    puts("PASS: legacy fixed-100 wrapper engine/UI equality across all parts and live/edit/record/ARP modes");
}
static void key_glide_record(void) {
    init();mc_preset(&e,0,MC_PRESET_ACID_SAW);e.project.patch[0].glide=64;e.playing=e.recording=1;
    e.step=1;mc_ui_key_velocity(&e,&u,0,100,1);
    assert(!(e.project.step[0][0][1].flags&MC_SLIDE));
    e.step=2;mc_ui_key_velocity(&e,&u,2,80,1);
    assert(e.project.step[0][0][2].note[0]==55);
    assert(e.project.step[0][0][2].flags&MC_SLIDE);
    mc_ui_key_velocity(&e,&u,2,0,0);
    assert(e.voice[0].note==53&&e.voice[0].stage!=2);
    puts("PASS: overlapping mono panel keys record destination SLIDE and release returns to the held key");
}
static void pitch_cache(void) {
    init();memset(e.project.step,0,sizeof e.project.step);
    e.project.step[63][0][0]=(McStep){.note={12,108},.velocity=100};
    e.project.step[0][0][0]=(McStep){.note={60},.velocity=100};
    draw(first);mc_ui_prepare_frame(&e,&u);draw(second);
    assert(u.pitch_ready&&u.pitch_low[0]==12&&u.pitch_high[0]==108);
    assert(!memcmp(first,second,sizeof first));
    e.project.step[63][0][0]=(McStep){.note={60,72},.velocity=100};
    mc_ui_prepare_frame(&e,&u);assert(u.pitch_low[0]==60&&u.pitch_high[0]==72);
    e.project.step[0][0][0]=(McStep){.note={12,108},.velocity=100};
    draw(first);
    assert(first[76*240+31]==0x0841u&&first[61*240+31]==0x0841u);
    mc_ui_prepare_frame(&e,&u);assert(u.pitch_low[0]==12&&u.pitch_high[0]==108);
    puts("PASS: frame pitch cache includes pattern 64 and clamps new recording notes until the next preparation");
}
static void pattern_keys(void) {
    static const unsigned white[16]={0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26};
    static const unsigned black[4]={1,3,5,8};
    McMidiMessage message;
    for(unsigned bank=0;bank<4;bank++) {
        init();u.editing=e.recording=1;midi_drain();
        McProject before;memcpy(&before,&e.project,sizeof before);
        mc_ui_button_state(&e,&u,UI_OCTDN,1);
        mc_ui_key(&e,&u,black[bank],1);mc_ui_key(&e,&u,black[bank],0);
        assert(u.pattern_hold&&u.pattern_bank==bank&&u.octave==0);
        for(unsigned index=0;index<16;index++) {
            mc_ui_key(&e,&u,white[index],1);mc_ui_key(&e,&u,white[index],0);
            assert(e.pattern==bank*16+index&&e.queued==255&&u.octave==0);
        }
        mc_ui_button_state(&e,&u,UI_OCTDN,0);
        assert(!u.pattern_hold&&!u.pattern_keys&&u.octave==0);
        assert(!memcmp(&before,&e.project,sizeof before)&&!e.events&&!mc_midi_pop(&e,&message));
        for(unsigned voice=0;voice<MC_VOICES;voice++)assert(!e.voice[voice].active);
    }
    init();
    mc_ui_button_state(&e,&u,UI_OCTDN,1);assert(u.octave==0);
    mc_ui_button_state(&e,&u,UI_OCTDN,0);assert(u.octave==-1);
    mc_ui_button_state(&e,&u,UI_OCTDN,1);mc_ui_key(&e,&u,10,1);
    mc_ui_button_state(&e,&u,UI_OCTDN,0);mc_ui_key(&e,&u,10,0);
    assert(u.octave==-1&&!u.pattern_keys);
    init();e.project.bpm=120;e.project.swing=0;mc_start(&e);
    mc_ui_button_state(&e,&u,UI_OCTDN,1);
    mc_ui_key(&e,&u,8,1);mc_ui_key(&e,&u,8,0);mc_ui_key(&e,&u,26,1);
    assert(e.pattern==0&&e.queued==63);
    mc_ui_button_state(&e,&u,UI_OCTDN,0);mc_ui_key(&e,&u,26,0);
    assert(u.octave==0&&!u.pattern_keys);
    render_frames(MC_RATE*2);assert(e.pattern==63&&e.queued==255);
    init();mc_preset(&e,0,MC_PRESET_ACID_SAW);mc_ui_key(&e,&u,0,1);
    mc_ui_button_state(&e,&u,UI_OCTDN,1);mc_ui_key(&e,&u,2,1);
    mc_ui_button_state(&e,&u,UI_OCTDN,0);mc_ui_key(&e,&u,2,0);
    assert(e.voice[0].note==53&&e.voice[0].stage!=2);
    mc_ui_key(&e,&u,0,0);assert(e.voice[0].stage==2);
    init();e.project.song_mode=1;
    mc_ui_button_state(&e,&u,UI_OCTDN,1);mc_ui_key(&e,&u,2,1);
    assert(!e.pattern&&e.queued==255&&u.message);
    mc_ui_key(&e,&u,2,0);mc_ui_button_state(&e,&u,UI_OCTDN,0);
    assert(u.octave==0);
    puts("PASS: OCT- tap, four banks/all64 patterns, bar queue, no note/MIDI/record/transpose leakage, release order and SONG isolation");
}
int main(void) { acceleration();small_ranges();metronome_ui();song_midi_ui();patterns64();popup();performance_ui();shared_fx_ui();green_feedback();strip_equivalence();key_velocity();key_velocity_arp_drums();key_legacy_equality();key_glide_record();pitch_cache();pattern_keys();return 0; }
