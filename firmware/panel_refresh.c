/* SPDX-License-Identifier: GPL-3.0-only */
/* 앱과 회귀 테스트가 같은 단일 버퍼 DMA 스케줄러를 사용한다. */
static unsigned panel_y = 240;
static uint32_t panel_frame;
static uint8_t panel_finishing;

static void panel_refresh_restart(void)
{
    panel_y = 240;
    panel_finishing = 0;
    panel_frame = milliseconds - 50u;
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
    for (unsigned y = 0; y < 240; y += 8)
        draw_strip(y);
    lcd_sync();
    panel_y = 240;
    panel_finishing = 0;
    panel_frame = milliseconds;
}

static void panel_draw_service(uint32_t now)
{
    if (!lcd_idle())
        return;
    if (panel_finishing) {
        panel_frame = now;
        panel_finishing = 0;
    }
    if (panel_y == 240) {
        if (now - panel_frame < 50u)
            return;
        panel_y = 0;
    }
    draw_strip(panel_y);
    panel_y += 8;
    if (panel_y == 240)
        panel_finishing = 1;
}
