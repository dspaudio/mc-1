/* SPDX-License-Identifier: GPL-3.0-only */
#include "ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static McEngine engine;
static McUi ui;
static uint16_t first[240*240],queued[240*240],frame[240*240];
static int16_t audio[256];
static void render(uint16_t *pixels)
{
    for(unsigned y=0;y<240;y+=8)mc_ui_strip(&engine,&ui,y,8,pixels+y*240);
}
static void same_glyph(const uint16_t *a,unsigned ax,const uint16_t *b,unsigned bx)
{
    for(unsigned y=30;y<44;y++)assert(!memcmp(a+y*240+ax,b+y*240+bx,10*sizeof *a));
}
static void separator(const uint16_t *pixels,unsigned x)
{
    for(unsigned y=0;y<14;y++)for(unsigned dx=0;dx<10;dx++)
        assert(pixels[(30+y)*240+x+dx]==(y==6||y==7?0x67e8:0x0841));
}
static int region_differs(const uint16_t *a,const uint16_t *b,unsigned x,unsigned y,unsigned w,unsigned h)
{
    for(unsigned row=y;row<y+h;row++)
        if(memcmp(a+row*240+x,b+row*240+x,w*sizeof *a))return 1;
    return 0;
}
static void queue_pixels(unsigned page,unsigned selected)
{
    mc_init(&engine);mc_ui_init(&ui);engine.project.bpm=120;engine.project.swing=0;
    ui.page=(uint8_t)page;engine.selected=(uint8_t)selected;
    mc_start(&engine);render(first);mc_queue(&engine,1);render(queued);
    assert(!region_differs(first,queued,0,171,150,12));
    assert(region_differs(first,queued,163,14,45,7));
    if(!selected&&page==0)assert(region_differs(first,queued,8,30,68,14));
}
int main(void)
{
    queue_pixels(0,0);queue_pixels(0,3);queue_pixels(1,0);queue_pixels(1,3);
    mc_init(&engine);mc_ui_init(&ui);engine.project.bpm=120;engine.project.swing=0;
    render(first);same_glyph(first,8,first,32);separator(first,20);
    mc_start(&engine);mc_queue(&engine,1);render(queued);
    same_glyph(first,8,queued,8);separator(queued,20);
    unsigned different=0;
    for(unsigned y=30;y<44;y++)
        if(memcmp(first+y*240+32,queued+y*240+32,10*sizeof *first))different=1;
    assert(different);
    for(unsigned n=0;engine.pattern!=1&&n<MC_RATE*3u;n+=128)mc_render(&engine,audio,128);
    assert(engine.pattern==1&&engine.queued==255);render(frame);
    same_glyph(queued,32,frame,8);same_glyph(queued,32,frame,32);separator(frame,20);
    mc_ui_button(&engine,&ui,UI_EDIT);render(frame);
    same_glyph(queued,32,frame,8);same_glyph(queued,32,frame,32);
    mc_stop(&engine);mc_queue(&engine,63);render(frame);
    same_glyph(frame,8,frame,44);same_glyph(frame,20,frame,56);separator(frame,32);
    mc_start(&engine);mc_queue(&engine,0);render(frame);
    same_glyph(first,8,frame,44);separator(frame,32);
    for(unsigned y=30;y<44;y++)for(unsigned x=56;x<68;x++)assert(frame[y*240+x]==0x0841);
    mc_stop(&engine);mc_preset(&engine,0,MC_PRESET_GLASS_BELL);render(first);
    assert(!mc_parameter_set(&engine,0,4,(engine.project.patch[0].cutoff+1u)%128u));render(frame);
    for(unsigned y=30;y<44;y++)
        assert(!memcmp(first+y*240+84,frame+y*240+96,120*sizeof *frame));
    static const unsigned star[7]={0,21,14,31,14,21,0};
    for(unsigned y=0;y<14;y++)for(unsigned x=0;x<10;x++)
        assert(frame[(30+y)*240+84+x]==((star[y/2]>>(4-x/2))&1?0xdefb:0x0841));
    mc_preset(&engine,0,MC_PRESET_CELLO_ENSEMBLE);
    assert(strlen(mc_preset_names[MC_PRESET_CELLO_ENSEMBLE])>12);
    assert(!mc_parameter_set(&engine,0,4,(engine.project.patch[0].cutoff+1u)%128u));render(frame);
    for(unsigned y=30;y<44;y++)for(unsigned x=228;x<240;x++)assert(frame[y*240+x]==0x0841);
    engine.project.song_mode=1;engine.project.song_length=1;
    engine.project.song[0]=(McSongEntry){63,1};engine.pattern=63;
    engine.song_position=engine.song_repeat=engine.song_finished=0;
    render(frame);separator(frame,32);
    unsigned end_ink=0;
    for(unsigned y=30;y<44;y++) {
        for(unsigned x=68;x<78;x++)if(frame[y*240+x]==0x67e8)end_ink++;
        for(unsigned x=79;x<84;x++)assert(frame[y*240+x]==0x0841);
    }
    assert(end_ink);
    puts("pattern title: current/queued/transition/edit/64-END, left star and name clipping PASS");
    return 0;
}
