/* SPDX-License-Identifier: GPL-3.0-only */
/* 실제 앱의 OTA 소유권·MIDI 정리·오디오 복귀를 테스트와 공유한다. */
static uint16_t ota_output_channels;
static uint8_t ota_stop_pending;

static int mc_ota_requested(void) { return usb.ota_req != 0; }
static int mc_ota_flash_known(void) { return mc_ota_flash_probe(); }
static void mc_ota_refuse(void) { usb.ota_req = 0; }

static void mc_ota_enter(void)
{
    audio_pause();
    fm1_irq_off();
    ota_output_channels = engine.midi.output_channels;
    ota_stop_pending = (uint8_t)(engine.midi.output_running || engine.midi.stop_pending);
    mc_stop(&engine);
    ota_usb_owner = 1;
    RING_PUBLISH();
    usb_ota_clear_queues();
    usb_ota_flush_tx();
    engine.midi.count = engine.midi.read = engine.midi.write = 0;
    engine.midi.panic = 0;
    engine.midi.cleanup = engine.midi.output_channels ? 3 : 0;
    engine.midi.stop_pending = ota_stop_pending;
    connection_midi_output(&engine);
    ota_poll_tick = fm1_ticks();
    fm1_irq_on();
}

static void mc_ota_leave(void)
{
    fm1_irq_off();
    usb_ota_clear_queues();
    usb_ota_flush_tx();
    /* 링에 넣었다는 사실은 전송 완료가 아니다. 폐기한 정리를 다시 게시한다. */
    engine.midi.output_channels |= ota_output_channels;
    engine.midi.cleanup = engine.midi.output_channels ? 3 : 0;
    engine.midi.stop_pending |= ota_stop_pending;
    connection_midi_resume();
    usb_ota_clear_queues();
    for (unsigned i = 0; i < FRAMES * 4u; i++)
        dma[i] = 0;
    fm1_guard_unlock_top();
    fm1_audio_init(dma, FRAMES * 2, isr_alnk0, 3);
    fm1_guard_lock_top();
    ota_usb_owner = 0;
    RING_PUBLISH();
    panel_refresh_restart();
    fm1_irq_on();
}

static void mc_ota_result(int rc)
{
    ota_result_code = rc;
    ui.message = rc == -15 ? "FLASH UNAVAILABLE" :
        rc == -16 ? "UPDATE CLEANUP FAILED" : "UPDATE FAILED";
}
