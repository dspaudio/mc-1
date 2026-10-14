/* SPDX-License-Identifier: GPL-3.0-only */
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

static void check(unsigned playing,unsigned step) {
    memset(writes,0,sizeof writes);
    mc_panel_step_leds(playing,step,set_key);
    unsigned white=0,lit=0;
    for(unsigned key=0;key<27u;key++) {
        unsigned expected=white_key(key)&&playing&&white==step;
        assert(writes[14u+key]==1u);
        assert(leds[14u+key]==expected);
        lit+=leds[14u+key];
        if(white_key(key))white++;
    }
    assert(white==16u);
    assert(lit==(unsigned)(playing&&step<16u));
    for(unsigned id=0;id<14u;id++)assert(leds[id]==1u&&!writes[id]);
}

int main(void) {
    for(unsigned id=0;id<41u;id++)leds[id]=1u;
    check(0,0);
    for(unsigned step=0;step<16u;step++)check(1,step);
    check(1,0);
    puts("PASS: F3..G5 흰 건반 16개에 순서대로 하나만 점등, 검은 건반 소등, 마지막 스텝에서 첫 스텝으로 복귀");
    for(unsigned step=0;step<16u;step++) {
        check(1,step);
        check(0,step);
    }
    check(1,16);
    check(1,255);
    puts("PASS: 모든 스텝에서 정지 시 전체 건반 소등, 버튼 LED 보존, 범위 밖 스텝 소등");
    return 0;
}
