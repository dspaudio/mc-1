/* SPDX-License-Identifier: GPL-3.0-only */
#include "../firmware/hal/fm1_gpio.h"
static volatile uint32_t gpio[8][8];
#undef FM1_PR
#define FM1_PR(p,r) gpio[(p)][(r)/4u]
#include "../firmware/hal/fm1_input.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/panel_leds.h"

static unsigned leds[41],writes[41];

static void set_key(uint32_t id,int on) {
    assert(id>=14u&&id<41u);
    assert(on==0||on==1);
    leds[id]=(unsigned)on;
    writes[id]++;
}

static int white_key(unsigned key) {
    /* 구현의 오프셋 표와 독립적으로 F3(MIDI 53)부터 자연음을 판별한다. */
    switch((53u+key)%12u) {
        case 0:case 2:case 4:case 5:case 7:case 9:case 11:return 1;
        default:return 0;
    }
}

static void check(unsigned playing,unsigned step,uint32_t held) {
    memset(writes,0,sizeof writes);
    mc_panel_key_leds(playing,step,held,set_key);
    unsigned white=0,lit=0,expected_lit=0;
    for(unsigned key=0;key<27u;key++) {
        unsigned expected=((held>>key)&1u)||(white_key(key)&&playing&&white==step);
        assert(writes[14u+key]==1u);
        assert(leds[14u+key]==expected);
        lit+=leds[14u+key];
        expected_lit+=expected;
        if(white_key(key))white++;
    }
    assert(white==16u);
    assert(lit==expected_lit);
    for(unsigned id=0;id<14u;id++)assert(leds[id]==1u&&!writes[id]);
}

static void play_green(void) {
    for(unsigned playing=0;playing<2;playing++)for(unsigned recording=0;recording<2;recording++) {
        memset(fm1_led,0x1e,sizeof fm1_led);
        fm1_led_key(12,(int)playing);
        fm1_led_key(13,(int)recording);
        for(unsigned col=0;col<FM1_NCOL;col++) {
            unsigned expected=0x1eu;
            if(col==8u)expected=(expected&~2u)|(playing<<1);
            if(col==10u)expected&=~4u;
            if(col==7u)expected=(expected&~4u)|(recording<<2);
            assert(fm1_led[col]==expected);
        }
        mc_panel_key_leds(playing,0,0x07ffffffu,fm1_led_key);
        assert(((fm1_led[8]>>1)&1u)==playing);
        assert(!(fm1_led[10]&4u));
        assert(((fm1_led[7]>>2)&1u)==recording);
        mc_panel_key_leds(playing,15,0,fm1_led_key);
        assert(((fm1_led[8]>>1)&1u)==playing);
        assert(!(fm1_led[10]&4u));
        assert(((fm1_led[7]>>2)&1u)==recording);
        fm1_led_key(12,0);
        assert(!(fm1_led[8]&2u)&&!(fm1_led[10]&4u));
        assert(((fm1_led[7]>>2)&1u)==recording);
    }
    puts("PASS: physical green PLAY, red die off, independent REC and held/cursor preservation");
}

int main(void) {
    play_green();
    for(unsigned id=0;id<41u;id++)leds[id]=1u;
    check(0,0,0);
    for(unsigned step=0;step<16u;step++)check(1,step,0);
    check(1,0,0);
    puts("PASS: 16 white-key step LEDs, black keys off without held notes, and wrap");
    for(unsigned step=0;step<16u;step++) {
        check(1,step,0);
        check(0,step,0);
    }
    check(1,16,0);
    check(1,255,0);
    for(unsigned key=0;key<27u;key++) {
        check(0,0,1u<<key);
        for(unsigned step=0;step<16u;step++) {
            check(1,step,1u<<key);
            check(1,step,0);
        }
        check(0,0,0);
    }
    for(unsigned step=0;step<16u;step++)
        check(1,step,(1u<<0)|(1u<<1)|(1u<<26));
    check(0,0,0x07ffffffu);
    check(0,0,0xf8000000u);
    puts("PASS: all 27 held LEDs, chords, cursor overlap/release, stopped playing and button preservation");
    return 0;
}
