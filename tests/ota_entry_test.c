/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/ui.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#define FRAMES 128u
#define MQ 256u
#define RING_PUBLISH() ((void)0)
static McEngine engine;
static McUi ui;
static int32_t dma[FRAMES * 4];
static struct { uint8_t config, suspended, up, ota_req, uboot_req; uint32_t resets; } usb;
static struct { unsigned drops; } um;
static uint32_t midi_in_q[MQ], midi_out_q[MQ], mi_r, mi_w, mo_r, mo_w;
static uint8_t ota_usb_owner, flash_ok;
static uint32_t ota_poll_tick;
static int32_t ota_result_code;
static unsigned paused, restarted, panel_restarts, known, probes, sessions, cleanups;
static int irq_on, store_ready, session_rc, cleanup_rc, commit;
static jmp_buf reboot;
static void uart_midi_stop(void) { paused = 1; }
static void uart_midi_start(void) { paused = 0; }
static void uart_midi_poll(void) {}
static void usb_poll(void) {}
static void ep1_rx(void) { midi_in_q[mi_w++ % MQ] = 0x643c9009u; }
static void midi_out_event(uint32_t packet) {
    assert(mo_w - mo_r < MQ);
    midi_out_q[mo_w++ % MQ] = packet;
}
#include "../firmware/connection/routing.c"
static void fm1_irq_off(void) { irq_on = 0; }
static void fm1_irq_on(void) { assert(store_ready); irq_on = 1; }
static uint32_t fm1_ticks(void) { return 24000; }
static void fm1_guard_unlock_top(void) {}
static void fm1_guard_lock_top(void) {}
static void isr_alnk0(void) {}
static void fm1_audio_init(int32_t *p,unsigned n,void (*isr)(void),unsigned priority) {
    assert(!irq_on && p == dma && n == FRAMES * 2 && isr == isr_alnk0 && priority == 3);
    for (unsigned i = 0; i < FRAMES * 4; i++) assert(!dma[i]);
    restarted++;
}
static void audio_pause(void) {
    fm1_irq_off(); connection_midi_pause(&engine); fm1_irq_on();
}
static void usb_ota_clear_queues(void) {
    assert(!irq_on && ota_usb_owner);
    mi_r = mi_w; mo_r = mo_w; usb.ota_req = usb.uboot_req = 0;
}
static void usb_ota_flush_tx(void) { assert(!irq_on && ota_usb_owner); }
static void panel_refresh_restart(void) { panel_restarts++; }
static int mc_ota_flash_probe(void) {
    assert(irq_on && store_ready);
    probes++; return (int)known;
}
#include "../firmware/update/ota_app.c"
static void ota_service(void) {}
static int ota_session(void) {
    assert(ota_usb_owner && paused && irq_on && !engine.playing);
    sessions++;
    if (commit) longjmp(reboot, 1);
    return session_rc;
}
static int ota_boot_cleanup(void) {
    assert(ota_usb_owner && paused && store_ready);
    cleanups++; return cleanup_rc;
}
#include "../firmware/update/ota_entry.c"

static void reset(void) {
    memset(&usb, 0, sizeof usb); memset(&um, 0, sizeof um);
    mc_init(&engine); memset(&ui, 0, sizeof ui); memset(dma, 0x7f, sizeof dma);
    usb.config = usb.up = 1;
    mi_r = mi_w = mo_r = mo_w = 0;
    ota_usb_owner = paused = restarted = panel_restarts = probes = sessions = cleanups = 0;
    ota_result_code = 0;
    known = flash_ok = 1; store_ready = irq_on = 1;
    session_rc = -13; cleanup_rc = 0; commit = 0;
    connection_paused = 0;
    connection_midi_poll(3); connection_midi_input(&engine);
}

static void sounding(void) {
    mc_note(&engine, 0, 60, 100, 0, 0);
    mc_midi_send(&engine, 0xfa, 0, 0, 1);
    connection_midi_output(&engine);
    assert(engine.midi.output_channels == 1 && engine.midi.output_running);
    /* 큐에 게시된 초기 Note On과 Start가 실제 소비된 상태에서 실패를 주입한다. */
    mo_r = mo_w;
    engine.playing = 1;
}

int main(void) {
    reset(); sounding(); known = 0; usb.ota_req = 1;
    uint32_t phase = engine.voice[0].phase;
    assert(mc_ota_entry_service() == 1 && !usb.ota_req && probes == 1);
    assert(!sessions && !restarted && !paused && !ota_usb_owner && engine.playing);
    assert(engine.voice[0].phase == phase && engine.midi.output_channels == 1);
    assert(ota_result_code == -15 && mc_ota_entry_service() == 0);

    static const int failures[] = {-1, -13, -16};
    for (unsigned i = 0; i < sizeof failures / sizeof failures[0]; i++) {
        reset(); sounding(); session_rc = failures[i]; usb.ota_req = 1;
        assert(mc_ota_entry_service() == 1);
        assert(ota_result_code == failures[i] && sessions == 1 && restarted == 1);
        assert(ota_poll_tick == fm1_ticks());
        assert(irq_on && !paused && !ota_usb_owner && !engine.playing && mi_r == mi_w);
        assert(engine.midi.output_channels == 1 && engine.midi.cleanup == 3);
        assert(engine.midi.stop_pending && !usb.ota_req);
        assert(mc_ota_entry_service() == 0 && sessions == 1);

        mo_w = mo_r + MQ; /* 복귀 뒤에도 EP1의 전송 정체가 계속된다. */
        mc_note(&engine, 0, 65, 100, 0, 0);
        connection_midi_output(&engine);
        assert(engine.midi.cleanup == 3 && engine.midi.stop_pending);
        mo_r = mo_w;
        uint32_t first = mo_w;
        connection_midi_output(&engine);
        assert(mo_w - first == 5);
        static const uint32_t expected[5] = {0x0000fc0f,0x0040b00b,0x0078b00b,0x007bb00b,0x64419009};
        for (unsigned j = 0; j < 5; j++) assert(midi_out_q[(first + j) % MQ] == expected[j]);
        assert(!engine.midi.cleanup && !engine.midi.stop_pending);
        mo_r = mo_w;
        usb.ota_req = 1;
        assert(mc_ota_entry_service() == 1 && sessions == 2);
    }

    for (unsigned present = 0; present < 2; present++) {
        reset(); flash_ok = (uint8_t)present; mc_ota_boot_service();
        assert(probes == present && cleanups == present && restarted == present);
        assert(irq_on && !paused && !ota_usb_owner);
    }
    reset(); known = 0; mc_ota_boot_service();
    assert(probes == 1 && !cleanups && !restarted);
    reset(); cleanup_rc = -16; mc_ota_boot_service();
    assert(cleanups == 1 && ota_result_code == -16 && !paused);
    reset(); usb.ota_req = commit = 1;
    if (!setjmp(reboot)) {
        (void)mc_ota_entry_service();
        assert(0);
    }
    assert(sessions == 1 && !restarted && ota_usb_owner && paused);
    puts("OTA app: refusal, failure/backpressure cleanup, retry, boot order and commit PASS");
    return 0;
}
