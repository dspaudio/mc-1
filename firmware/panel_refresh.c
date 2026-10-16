/* SPDX-License-Identifier: GPL-3.0-only */
/* The application and regression tests share this single-buffer DMA scheduler. */
static unsigned panel_y = 240;
static uint32_t panel_frame;
static uint8_t panel_finishing;
static uint8_t panel_urgent, panel_playing, panel_step, panel_pattern;

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
    panel_transport_sync();
}

static void draw_strip(unsigned y)
{
    lcd_sync();
    mc_ui_strip(&engine, &ui, y, 8, strip);
    for (unsigned i = 0; i < 240u * 8u; i++)
        strip[i] = (uint16_t)((strip[i] >> 8) | (strip[i] << 8));
    lcd_blit(0, y, 240, 8, strip);
}

static void draw(void)
{
    mc_ui_prepare_frame(&engine,&ui);
    for (unsigned y = 0; y < 240; y += 8)
        draw_strip(y);
    lcd_sync();
    panel_y = 240;
    panel_finishing = 0;
    panel_frame = milliseconds;
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
        panel_frame = now;
        panel_finishing = 0;
    }
    if (panel_y == 240) {
        if (!panel_urgent && now - panel_frame < 50u)
            return;
        panel_urgent = 0;
        panel_y = 0;
        mc_ui_prepare_frame(&engine,&ui);
    }
    draw_strip(panel_y);
    panel_y += 8;
    if (panel_y == 240)
        panel_finishing = 1;
}
