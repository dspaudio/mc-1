/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static McEngine engine;
static McUi ui;
static uint16_t strip[240 * 8];
static uint32_t milliseconds;
static unsigned sent, rendered, last_y, prepared;
static unsigned last_x, last_w;
static McUi captured_ui;
static McUiFrame captured_frame;
static uint16_t expected[240 * 240], framebuffer[240 * 240];
static uint16_t pending[240 * 8];
static int animate = 1;
static int busy;
static void check_dma(void)
{
    if (busy)
        assert(!memcmp(strip, pending, sizeof strip));
}
static void lcd_sync(void)
{
    check_dma();
    if (busy)
        for (unsigned row = 0; row < 8; row++)
            for (unsigned x = 0; x < last_w; x++) {
                uint16_t pixel = strip[row * last_w + x];
                framebuffer[(last_y + row) * 240u + last_x + x] =
                    (uint16_t)((pixel >> 8) | (pixel << 8));
            }
    busy = 0;
}
static int lcd_idle(void) { check_dma(); return !busy; }
void mc_ui_prepare_frame(const McEngine *e,McUi *u)
{
    (void)e; (void)u;
    prepared++;
    if (animate)
        for (unsigned i = 0; i < 240u * 240u; i++)
            expected[i] = (uint16_t)(i * 37u + prepared * 257u);
}
void mc_ui_strip(const McEngine *e,const McUi *u,unsigned y,unsigned h,uint16_t *pixels)
{
    (void)e; (void)u;
    assert(!busy && h == 8 && y < 240);
    rendered++;
    memcpy(pixels, expected + y * 240u, 240u * h * sizeof *pixels);
}
void mc_ui_capture_frame(const McEngine *e,McUiFrame *frame)
{
    (void)e;
    memset(frame,0,sizeof *frame);captured_frame=*frame;captured_ui=ui;
}
void mc_ui_strip_frame(const McEngine *e,const McUi *u,const McUiFrame *frame,
    unsigned y,unsigned h,uint16_t *pixels)
{
    assert(u!=&ui&&!memcmp(u,&captured_ui,sizeof *u));
    assert(!memcmp(frame,&captured_frame,sizeof *frame));
    mc_ui_strip(e,u,y,h,pixels);
}
static void lcd_blit(unsigned x,unsigned y,unsigned w,unsigned h,const uint16_t *pixels)
{
    assert(!busy && x < 240 && w && x + w <= 240 && h == 8 && y <= 232);
    assert(pixels == strip);
    for (unsigned row = 0; row < h; row++)
        for (unsigned col = 0; col < w; col++) {
            uint16_t value = expected[(y + row) * 240u + x + col];
            assert(pixels[row * w + col] == (uint16_t)((value >> 8) | (value << 8)));
        }
    memcpy(pending, strip, sizeof pending);
    last_x = x; last_w = w; last_y = y; sent++; busy = 1;
}
#include "../firmware/panel_refresh.c"

static void test_scheduler(void)
{
    uint32_t stats[6];
    milliseconds = 50;
    panel_draw_service(milliseconds);
    assert(sent == 1 && rendered == 1 && last_y == 0);
    assert(prepared == 1);
    ui.page=3; /* An input change cannot splice another page into this frame. */
    for (unsigned y = 8; y < 240; y += 8) {
        unsigned before = sent;
        panel_draw_service(++milliseconds);
        assert(sent == before && rendered == before);
        lcd_sync();
        panel_draw_service(milliseconds);
        assert(sent == before + 1 && last_y == y);
    }
    assert(sent == 30 && panel_finishing);
    assert(prepared == 1);
    milliseconds += 17;
    panel_draw_service(milliseconds);
    assert(sent == 30);
    panel_stats(stats);
    assert(stats[0] == 0 && stats[2] == 30 && stats[4] == 240);
    lcd_sync();
    panel_draw_service(milliseconds);
    uint32_t completed = milliseconds;
    assert(sent == 30 && !panel_finishing);
    assert(!memcmp(framebuffer, expected, sizeof expected));
    panel_stats(stats);
    assert(stats[0] == 1 && stats[1] == 0 && stats[2] == 30 && stats[3] == 0);
    assert(stats[4] == 240 && stats[5] == completed);
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

    lcd_sync();
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
    lcd_sync();
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
        lcd_sync();
        panel_draw_service(milliseconds);
        assert(last_y == y);
    }
    assert(sent == before + 30 && panel_finishing && panel_urgent);
    panel_draw_service(milliseconds);
    assert(sent == before + 30);
    lcd_sync();
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
    lcd_sync();
}

static void expect_rect(unsigned y,unsigned x,unsigned width)
{
    unsigned before = sent;
    draw_strip(y);
    assert(sent == before + 1 && busy);
    assert(last_y == y && last_x == x && last_w == width);
    lcd_sync();
    assert(!memcmp(framebuffer, expected, sizeof expected));
    draw_strip(y);
    assert(sent == before + 1 && !busy);
}

static void test_dirty_rectangles(void)
{
    animate = 0;
    panel_refresh_restart();
    draw();
    assert(!memcmp(framebuffer, expected, sizeof expected));
    uint32_t stats[6];
    panel_stats(stats);
    assert(stats[0] == 1 && stats[2] == 30 && stats[3] == 0);
    unsigned before = sent;
    milliseconds += 50;
    for (unsigned i = 0; i < 30; i++)
        panel_draw_service(milliseconds);
    assert(sent == before && !busy && panel_finishing);
    panel_stats(stats);
    assert(stats[0] == 1 && stats[3] == 30);
    panel_draw_service(milliseconds);
    panel_stats(stats);
    assert(stats[0] == 2 && stats[1] == 50 && stats[4] == 240);
    unsigned before_prepared = prepared;
    engine.step++;
    panel_draw_service(++milliseconds);
    assert(prepared == before_prepared + 1 && sent == before);
    assert(panel_y == 8 && !panel_urgent && !busy);
    for (unsigned i = 1; i < 30; i++)
        panel_draw_service(milliseconds);
    panel_draw_service(milliseconds);
    assert(!panel_finishing && sent == before);

    /* Every valid left edge/width exercises forward overlap on all eight rows. */
    for (unsigned left = 0; left < 240; left++)
        for (unsigned width = 1; width <= 240u - left; width++) {
            for (unsigned row = 0; row < 8; row++)
                for (unsigned x = left; x < left + width; x++)
                    expected[(80u + row) * 240u + x] ^= 0x1234u;
            expect_rect(80, left, width);
        }
    /* Disjoint extrema in different rows must include unchanged middle pixels. */
    expected[232u * 240u] ^= 0x00ffu;
    expected[239u * 240u + 239u] ^= 0xff00u;
    expect_rect(232, 0, 240);
    expected[3u * 240u + 17u] ^= 0x0001u;
    expected[6u * 240u + 211u] ^= 0x8000u;
    expect_rect(0, 17, 195);
    expected[239] = 0x12abu;
    expect_rect(0, 239, 1);

    /* A service call cannot render, compact, or count a frame during DMA. */
    expected[0] ^= 0x4321u;
    draw_strip(0);
    before = sent;
    unsigned before_rendered = rendered;
    panel_stats(stats);
    uint32_t completed = stats[0];
    panel_draw_service(milliseconds + 100u);
    assert(sent == before && rendered == before_rendered && busy);
    panel_stats(stats);
    assert(stats[0] == completed);
    /* Direct drawing must synchronize the same buffer before even a skipped strip. */
    draw_strip(8);
    assert(!busy && sent == before);
    assert(!memcmp(framebuffer, expected, sizeof expected));

    /* Restart invalidates every strip without touching an owned DMA buffer. */
    expected[239u * 240u + 239u] ^= 0x9876u;
    draw_strip(232);
    before = sent;
    before_rendered = rendered;
    panel_refresh_restart();
    check_dma();
    panel_stats(stats);
    assert(stats[0] == 0 && stats[1] == 0 && stats[2] == 0 && stats[3] == 0);
    assert(stats[4] == 240 && stats[5] == milliseconds - 50u);
    panel_draw_service(milliseconds);
    assert(sent == before && rendered == before_rendered);
    lcd_sync();
    for (unsigned y = 0; y < 240; y += 8) {
        panel_draw_service(milliseconds);
        assert(busy && last_x == 0 && last_w == 240 && last_y == y);
        lcd_sync();
    }
    panel_stats(stats);
    assert(stats[0] == 0 && stats[2] == 30 && panel_finishing);
    milliseconds += 17;
    panel_draw_service(milliseconds);
    panel_stats(stats);
    assert(stats[0] == 1 && stats[1] == 0 && !panel_finishing);
    assert(!memcmp(framebuffer, expected, sizeof expected));

    /* Completion gaps use unsigned subtraction across millisecond rollover. */
    milliseconds = UINT32_MAX - 10u;
    panel_refresh_restart();
    draw();
    milliseconds += 73u;
    draw();
    panel_stats(stats);
    assert(stats[0] == 2 && stats[1] == 73 && stats[2] == 30 && stats[3] == 30);
}

int main(void)
{
    test_scheduler();
    test_dirty_rectangles();
    puts("panel refresh: exact dirty rectangles, byte order, cache invalidation, DMA ownership, "
         "30 strips, idle gap, transport priority, OTA draw, stats and wrap PASS");
    return 0;
}
