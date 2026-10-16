/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/ui.h"
#include <assert.h>
#include <stdio.h>

static McEngine engine;
static McUi ui;
static uint16_t strip[240 * 8];
static uint32_t milliseconds;
static unsigned sent, rendered, last_y, prepared;
static int busy;
static void lcd_sync(void) { busy = 0; }
static int lcd_idle(void) { return !busy; }
void mc_ui_prepare_frame(const McEngine *e,McUi *u) { (void)e;(void)u;prepared++; }
void mc_ui_strip(const McEngine *e,const McUi *u,unsigned y,unsigned h,uint16_t *pixels)
{
    (void)e; (void)u;
    assert(!busy && h == 8 && y < 240);
    rendered++;
    for (unsigned i = 0; i < 240u * h; i++)
        pixels[i] = (uint16_t)(y + 0x1234u);
}
static void lcd_blit(unsigned x,unsigned y,unsigned w,unsigned h,const uint16_t *pixels)
{
    assert(!busy && !x && w == 240 && h == 8);
    uint16_t value = (uint16_t)(y + 0x1234u);
    assert(pixels[0] == (uint16_t)((value >> 8) | (value << 8)));
    assert(pixels[240 * 8 - 1] == pixels[0]);
    last_y = y; sent++; busy = 1;
}
#include "../firmware/panel_refresh.c"

int main(void)
{
    milliseconds = 50;
    panel_draw_service(milliseconds);
    assert(sent == 1 && rendered == 1 && last_y == 0);
    assert(prepared == 1);
    for (unsigned y = 8; y < 240; y += 8) {
        unsigned before = sent;
        panel_draw_service(++milliseconds);
        assert(sent == before && rendered == before);
        busy = 0;
        panel_draw_service(milliseconds);
        assert(sent == before + 1 && last_y == y);
    }
    assert(sent == 30 && panel_finishing);
    assert(prepared == 1);
    milliseconds += 17;
    panel_draw_service(milliseconds);
    assert(sent == 30);
    busy = 0;
    panel_draw_service(milliseconds);
    uint32_t completed = milliseconds;
    assert(sent == 30 && !panel_finishing);
    milliseconds = completed + 49;
    panel_draw_service(milliseconds);
    assert(sent == 30);
    milliseconds++;
    panel_draw_service(milliseconds);
    assert(sent == 31 && last_y == 0);

    draw();
    assert(sent == 61 && last_y == 232 && !busy && panel_y == 240);
    panel_draw_service(milliseconds + 49);
    assert(sent == 61);
    milliseconds += 50;
    panel_draw_service(milliseconds);
    assert(sent == 62 && last_y == 0);

    busy = 0;
    milliseconds = UINT32_MAX - 10u;
    draw();
    unsigned before = sent;
    milliseconds += 49;
    panel_draw_service(milliseconds);
    assert(sent == before);
    milliseconds++;
    panel_draw_service(milliseconds);
    assert(sent == before + 1 && last_y == 0);
    panel_refresh_restart();
    panel_draw_service(milliseconds);
    assert(sent == before + 1);
    busy = 0;
    panel_draw_service(milliseconds);
    assert(sent == before + 2 && last_y == 0);

    draw();
    before = sent;
    engine.playing = 1;
    panel_draw_service(++milliseconds);
    assert(sent == before + 1 && last_y == 0);
    engine.step = 1;
    panel_draw_service(milliseconds);
    assert(sent == before + 1 && panel_urgent);
    for (unsigned y = 8; y < 240; y += 8) {
        busy = 0;
        panel_draw_service(milliseconds);
        assert(last_y == y);
    }
    assert(sent == before + 30 && panel_finishing && panel_urgent);
    panel_draw_service(milliseconds);
    assert(sent == before + 30);
    busy = 0;
    panel_draw_service(milliseconds);
    assert(sent == before + 31 && last_y == 0 && !panel_urgent);

    draw();
    before = sent;
    engine.pattern = 1;
    panel_draw_service(++milliseconds);
    assert(sent == before + 1 && last_y == 0);
    draw();
    before = sent;
    engine.playing = 0;
    panel_draw_service(++milliseconds);
    assert(sent == before + 1 && last_y == 0);
    puts("panel refresh: DMA ownership, 30 strips, idle gap, transport priority, OTA draw and wrap PASS");
    return 0;
}
