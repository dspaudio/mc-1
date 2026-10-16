/* SPDX-License-Identifier: GPL-3.0-only */
#include "../firmware/hal/fm1_gpio.h"
static volatile uint32_t gpio[8][8];
#undef FM1_PR
#define FM1_PR(p,r) gpio[(p)][(r)/4u]
#include "../firmware/hal/fm1_input.h"
#include "../src/ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static McEngine engine;
static McUi ui;
static unsigned encoder,drain;
static void sample(unsigned value)
{
    const uint8_t *m=FM1_ENC[encoder];
    fm1_in.raw[m[0]]=(uint8_t)((fm1_in.raw[m[0]]&~(1u<<m[1]))|(((value>>1)&1u)<<m[1]));
    fm1_in.raw[m[2]]=(uint8_t)((fm1_in.raw[m[2]]&~(1u<<m[3]))|((value&1u)<<m[3]));
    fm1__frame();
    if(drain&&fm1_in.enc_steps[encoder]) {
        int steps=fm1_in.enc_steps[encoder];fm1_in.enc_steps[encoder]=0;
        mc_ui_encoder_at(&engine,&ui,3,steps,fm1_in.frames*11u/10u);
    }
}
static void state(unsigned value,unsigned frames)
{
    while(frames--)sample(value);
}
static void reset(unsigned id)
{
    encoder=id;drain=0;
    memset((void *)&fm1_in,0,sizeof fm1_in);
    for(unsigned i=0;i<FM1_NENC;i++)fm1_in.enc_prev[i]=fm1_in.enc_last[i]=0xff;
    state(3,4);
}
static void turn(unsigned frames,unsigned clicks,int direction,unsigned bounce)
{
    static const unsigned positive[4]={2,0,1,3},negative[4]={1,0,2,3};
    const unsigned *cycle=direction>0?positive:negative;
    unsigned previous=3;
    for(unsigned n=0;n<clicks;n++)for(unsigned i=0;i<4;i++) {
        if(bounce) { state(cycle[i],1);state(previous,1); }
        state(cycle[i],frames);previous=cycle[i];
    }
}
static unsigned parameter_change(unsigned frames,unsigned tempo)
{
    reset(2);mc_init(&engine);mc_ui_init(&ui);
    if(tempo) { mc_ui_button(&engine,&ui,UI_GLO);mc_ui_button(&engine,&ui,UI_GLO);engine.project.bpm=40; }
    else engine.project.patch[0].cutoff=0;
    drain=1;turn(frames,16,1,0);
    return tempo?engine.project.bpm-40u:engine.project.patch[0].cutoff;
}
int main(void)
{
    for(unsigned id=0;id<FM1_NENC;id++)for(int direction=-1;direction<=1;direction+=2) {
        reset(id);turn(20,16,direction,0);int slow=fm1_in.enc_steps[id];
        reset(id);turn(1,16,direction,0);int fast=fm1_in.enc_steps[id];
        printf("encoder=%u direction=%d slow=%d fast=%d\n",id,direction,slow,fast);
        assert(slow==direction*16&&fast==direction*16);
        reset(id);turn(1,16,direction,1);assert(fm1_in.enc_steps[id]==direction*16);
        reset(id);for(unsigned i=0;i<16;i++) { state(2,1);state(3,1); }
        assert(!fm1_in.enc_steps[id]); /* 1접점 bounce는 완전한 회전이 아니다. */
        reset(id);turn(1,8,1,0);turn(1,8,-1,0);assert(!fm1_in.enc_steps[id]);
    }
    unsigned slow=parameter_change(20,0),fast=parameter_change(1,0);
    assert(slow==16&&fast==46&&fast>slow);
    assert(parameter_change(20,1)==16&&parameter_change(1,1)==91);
    puts("encoder input: fast/slow, all7, both directions, bounce and actual UI acceleration PASS");
    return 0;
}
