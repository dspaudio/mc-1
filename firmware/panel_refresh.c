/* SPDX-License-Identifier: GPL-3.0-only */
/* The application and regression tests share this single-buffer DMA scheduler. */
static unsigned panel_y = 240;
static uint32_t panel_frame;
static uint8_t panel_finishing;
static uint8_t panel_urgent, panel_playing, panel_step, panel_pattern;
#ifndef MC_PANEL_CACHE_ATTR
#define MC_PANEL_CACHE_ATTR
#endif
static uint16_t panel_cache[240 * 240] MC_PANEL_CACHE_ATTR;
static uint8_t panel_valid[30];
static uint32_t panel_completed_frames, panel_max_frame_gap_ms;
static uint32_t panel_submitted_rects, panel_skipped_strips;
static McUi panel_ui;
static McUiFrame panel_view;
#ifndef MC_PANEL_CAPTURE
#define MC_PANEL_CAPTURE(u,f) do { *(u)=ui;mc_ui_capture_frame(&engine,(f)); } while (0)
#endif

static void panel_prepare_frame(void)
{
    mc_ui_prepare_frame(&engine, &ui);
    MC_PANEL_CAPTURE(&panel_ui, &panel_view);
}

static void panel_stats(uint32_t values[6])
{
    values[0] = panel_completed_frames;
    values[1] = panel_max_frame_gap_ms;
    values[2] = panel_submitted_rects;
    values[3] = panel_skipped_strips;
    values[4] = panel_y;
    values[5] = panel_frame;
}

static void panel_complete(uint32_t now)
{
    if (panel_completed_frames && now - panel_frame > panel_max_frame_gap_ms)
        panel_max_frame_gap_ms = now - panel_frame;
    panel_completed_frames++;
    panel_frame = now;
    panel_finishing = 0;
}

static void panel_transport_sync(void)
{
    panel_playing = engine.playing;
    panel_step = engine.step;
    panel_pattern = engine.pattern;
}

static void panel_refresh_restart(void)
{
    panel_y = 240;
    panel_finishing = 0;
    panel_frame = milliseconds - 50u;
    panel_urgent = 0;
    for (unsigned i = 0; i < 30; i++)
        panel_valid[i] = 0;
    panel_completed_frames = panel_max_frame_gap_ms = 0;
    panel_submitted_rects = panel_skipped_strips = 0;
    panel_transport_sync();
}

static void draw_strip(unsigned y)
{
    lcd_sync();
    mc_ui_strip_frame(&engine, &panel_ui, &panel_view, y, 8, strip);
    unsigned minx = 240, maxx = 0;
    uint16_t *cached = panel_cache + y * 240u;
    if (!panel_valid[y / 8u]) {
        minx = 0;
        maxx = 239;
    } else {
        for (unsigned row = 0; row < 8; row++)
            for (unsigned x = 0; x < 240; x++)
                if (strip[row * 240u + x] != cached[row * 240u + x]) {
                    if (x < minx) minx = x;
                    if (x > maxx) maxx = x;
                }
    }
    if (minx == 240) {
        panel_skipped_strips++;
        return;
    }
    for (unsigned i = 0; i < 240u * 8u; i++)
        cached[i] = strip[i];
    panel_valid[y / 8u] = 1;
    unsigned width = maxx - minx + 1u;
    /* Forward compaction never overwrites a later native-RGB565 source pixel. */
    for (unsigned row = 0; row < 8; row++)
        for (unsigned x = 0; x < width; x++) {
            uint16_t pixel = strip[row * 240u + minx + x];
            strip[row * width + x] = (uint16_t)((pixel >> 8) | (pixel << 8));
        }
    lcd_blit(minx, y, width, 8, strip);
    panel_submitted_rects++;
}

static void draw(void)
{
    panel_prepare_frame();
    for (unsigned y = 0; y < 240; y += 8)
        draw_strip(y);
    lcd_sync();
    panel_y = 240;
    panel_complete(milliseconds);
    panel_urgent = 0;
    panel_transport_sync();
}

static void panel_draw_service(uint32_t now)
{
    if (panel_playing != engine.playing || panel_step != engine.step ||
        panel_pattern != engine.pattern) {
        panel_urgent = 1;
        panel_transport_sync();
    }
    if (!lcd_idle())
        return;
    if (panel_finishing) {
        panel_complete(now);
    }
    if (panel_y == 240) {
        if (!panel_urgent && now - panel_frame < 50u)
            return;
        panel_urgent = 0;
        panel_y = 0;
        panel_prepare_frame();
    }
    draw_strip(panel_y);
    panel_y += 8;
    if (panel_y == 240)
        panel_finishing = 1;
}
