/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GUARD 64
#define G_TOP 58
#define G_BOT 150
#define COL_A 0x3317u
#define COL_D 0x3d48u
#define COL_S 0xfb20u
#define COL_R 0x9a00u
static McEngine e,before_e;
static McUi u,before_u;
static uint16_t full[240*240],tall[240*240],one[240*240],five[240*240],frames[9][240*240];
/* 같은 화면을 1행, 5행, 8행 스트립으로 각각 그린다. */
static void draw_n(uint16_t *px,unsigned n) {
    for(unsigned y=0;y<240;y+=n)mc_ui_strip(&e,&u,y,n,px+y*240);
}
static void setup(unsigned part) {
    mc_init(&e);mc_ui_init(&u);
    if(part<3)mc_preset(&e,part,part*40);
    e.selected=(uint8_t)part;
    mc_ui_button(&e,&u,UI_ENV);
}
/* 실제 노브 API만 사용: 먼저 0으로 내린 뒤 value만큼 올린다. */
static void set_all(unsigned value) {
    for(unsigned k=0;k<4;k++) {
        mc_ui_encoder(&e,&u,3+k,-127);
        if(value)mc_ui_encoder(&e,&u,3+k,(int)value);
    }
}
static int region_equal(const uint16_t *a,const uint16_t *b) {
    return !memcmp(a+G_TOP*240,b+G_TOP*240,(G_BOT-G_TOP+1)*240*sizeof *a);
}
static unsigned count(const uint16_t *px,uint16_t color,unsigned *min_row,unsigned *max_row) {
    unsigned n=0;*min_row=240;*max_row=0;
    for(unsigned y=G_TOP;y<=G_BOT;y++)for(unsigned x=0;x<240;x++)if(px[y*240+x]==color) {
        n++;if(y<*min_row)*min_row=y;if(y>*max_row)*max_row=y;
    }
    return n;
}
static void guards(void) {
    static uint16_t buf[GUARD+240*8+GUARD];
    for(unsigned y=0;y<=232;y+=8) {
        for(unsigned i=0;i<sizeof buf/sizeof *buf;i++)buf[i]=0xa5a5u;
        mc_ui_strip(&e,&u,y,8,buf+GUARD);
        for(unsigned i=0;i<GUARD;i++)assert(buf[i]==0xa5a5u&&buf[GUARD+240*8+i]==0xa5a5u);
    }
    for(unsigned i=0;i<sizeof buf/sizeof *buf;i++)buf[i]=0xa5a5u;
    mc_ui_strip(&e,&u,0,9,buf+GUARD);mc_ui_strip(&e,&u,236,8,buf+GUARD);
    for(unsigned i=0;i<sizeof buf/sizeof *buf;i++)assert(buf[i]==0xa5a5u);
}
static void ppm(const char *path) {
    FILE *f=fopen(path,"wb");assert(f);
    fprintf(f,"P6\n720 720\n255\n");
    for(unsigned y=0;y<720;y++)for(unsigned panel=0;panel<3;panel++)for(unsigned x=0;x<240;x++) {
        unsigned p=frames[(y/240)*3+panel][(y%240)*240+x],r=(p>>11)&31u,g=(p>>5)&63u,b=p&31u;
        unsigned char rgb[3]={(unsigned char)((r<<3)|(r>>2)),(unsigned char)((g<<2)|(g>>4)),(unsigned char)((b<<3)|(b>>2))};
        fwrite(rgb,1,3,f);
    }
    fclose(f);
}
int main(void) {
    unsigned lo,hi;
    for(unsigned part=0;part<3;part++) {
        setup(part);set_all(64);
        draw_n(full,8);draw_n(one,1);draw_n(five,5);
        assert(!memcmp(full,one,sizeof full)&&!memcmp(full,five,sizeof full));
        assert(count(full,COL_A,&lo,&hi)&&count(full,COL_D,&lo,&hi)&&count(full,COL_S,&lo,&hi)&&count(full,COL_R,&lo,&hi));
        before_e=e;before_u=u;draw_n(tall,8);guards();
        assert(!memcmp(&before_e,&e,sizeof e)&&!memcmp(&before_u,&u,sizeof u));
        memcpy(frames[part*3+1],full,sizeof full);
        McPatch *p=&e.project.patch[part];
        uint8_t *fields[4]={&p->attack,&p->decay,&p->sustain,&p->release};
        for(unsigned k=0;k<4;k++) {
            McProject expect;memcpy(&expect,&e.project,sizeof expect);
            assert(*fields[k]==64);
            mc_ui_encoder(&e,&u,3+k,40);
            assert(*fields[k]==104);
            ((uint8_t *)&expect)[fields[k]-(uint8_t *)&e.project]=104;
            assert(!memcmp(&expect,&e.project,sizeof expect));
            draw_n(tall,8);assert(!region_equal(full,tall));
            mc_ui_encoder(&e,&u,3+k,-40);
            draw_n(tall,8);assert(!memcmp(full,tall,sizeof full));
        }
        set_all(0);draw_n(frames[part*3],8);draw_n(one,1);
        assert(!memcmp(frames[part*3],one,sizeof one));guards();
        assert(count(frames[part*3],COL_A,&lo,&hi)&&count(frames[part*3],COL_D,&lo,&hi)&&count(frames[part*3],COL_R,&lo,&hi));
        assert(count(frames[part*3],COL_S,&lo,&hi)&&lo>=144);
        set_all(127);draw_n(frames[part*3+2],8);draw_n(one,1);
        assert(!memcmp(frames[part*3+2],one,sizeof one));guards();
        assert(count(frames[part*3+2],COL_A,&lo,&hi)&&count(frames[part*3+2],COL_D,&lo,&hi)&&count(frames[part*3+2],COL_R,&lo,&hi));
        assert(count(frames[part*3+2],COL_S,&lo,&hi)&&hi<=64);
        assert(!region_equal(frames[part*3],frames[part*3+2]));
    }
    /* 드럼 ENV 페이지는 page 0 화면과 같고 ADSR 색이 없어야 한다. */
    setup(3);draw_n(full,8);
    e.selected=3;mc_ui_init(&u);draw_n(tall,8);
    assert(!memcmp(full,tall,sizeof full));
    assert(!count(full,COL_A,&lo,&hi));
    const char *path=getenv("MC1_ENV_PPM");
    if(path&&*path)ppm(path);
    puts("PASS: ENV graph melodic parts/min-max/knob reaction/strip equality/guards/drum page/state untouched");
    return 0;
}
