/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Adapted from ORBIT's uac_test.c and midi_uart_test.c for the MC-1 app.
 * Runs actual descriptor, packet, ring and UART parser code. Register access,
 * USB enumeration, DMA timing and physical jack operation require hardware. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#define HALF_FRAMES 128
#define MC_USB_APP 1
#define FELUCCA_UAC 1
#define FELUCCA_UAC_48K 0
#define FELUCCA_CDC 0
#ifndef FELUCCA_OTA
#define FELUCCA_OTA 0
#endif
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#include "../firmware/update/usb.c"
#if FELUCCA_OTA
static uint32_t ota_test_ms;
static uint32_t ota_now_ms(void) { return ota_test_ms; }
static void ota_idle(void) { ota_test_ms++; }
#endif
/* 실제 UART HAL의 레지스터 쓰기 순서를 호스트 메모리로 검증한다. */
static volatile uint32_t uart_registers[15];
static volatile uint32_t *uart_register(uint32_t addr)
{
    static const uint32_t addresses[] = {
        0x12100u, 0x12104u, 0x12108u, 0x12110u, 0x1211Cu, 0x12120u, 0x12124u, 0x12128u,
        0x10010u, 0x51024u, 0x51028u, 0x501C8u, 0x501CCu, 0x501D0u, 0x501D4u
    };
    for (unsigned i = 0; i < sizeof addresses / sizeof addresses[0]; i++)
        if (addr == addresses[i])
            return &uart_registers[i];
    assert(0);
    return &uart_registers[0];
}
static void uart_sync(void);
#define FM1_UART_REG(addr) (*uart_register(addr))
#define FM1_UART_SYNC() uart_sync()
#include "../firmware/connection/midi_uart.c"

static int fails;
static int check(const char *what, int ok)
{
    printf("%-72s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
    return !ok;
}

static uint32_t le16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }

static void test_descriptors(void)
{
    const uint8_t *c, *dv;
    uint16_t cl, dl;
    uint32_t total, n, off, nif = 0, i;
    int lens_ok = 1, eps_ok = 1, ac_ok = 0, it_ok = 0, ot_ok = 0, asg_ok = 0, fmt_ok = 0, iso_ok = 0, csep_ok = 0;
    int alt0_ok = 0, midi_ok = 0, dup_ok = 1, coll_ok = 0;
    uint8_t seen_if[16] = {0}, if_class[16] = {0}, if_sub[16] = {0};
    int cur_if = -1, cur_alt = -1, cur_neps = 0, got_eps = 0, ac_len_left = -1, ac_total = 0, ac_wt = -1;
    uint8_t coll[8], ncoll = 0, iad_n = 0;
    uint32_t ep_seen[32] = {0};
    get_desc(0x0100, &dv, &dl);
    get_desc(0x0200, &c, &cl);
    total = le16(c + 2);
    n = cl;
    check("device descriptor: 18 bytes, type 1, EP0 64", dl == 18 && dv[0] == 18 && dv[1] == 1 && dv[7] == 64);
    check("bcdDevice: 44.1 kHz audio app", dv[12] == 0x10u && dv[13] == 3);
    check("device class 0 (per interface)", dv[4] == 0);
    check("configuration: type 2, wTotalLength = the bytes sent", c[1] == 2 && total == n);

    for (off = 0; off < n;) {
        const uint8_t *d = c + off;
        if (d[0] < 2 || off + d[0] > n) {
            lens_ok = 0;
            break;
        }
        if (ac_len_left > 0 && d[1] == 0x24) {
            ac_total += d[0];
            ac_len_left -= d[0];
        } else if (ac_len_left > 0) {
            ac_len_left = -2;                          /* the CS AC block ended early */
        }
        switch (d[1]) {
        case 0x0B:                                     /* IAD */
            iad_n++;
            break;
        case 4:
            if (cur_if >= 0 && got_eps != cur_neps)
                eps_ok = 0;
            cur_if = d[2];
            cur_alt = d[3];
            cur_neps = d[4];
            got_eps = 0;
            if (cur_if < 16) {
                if (!seen_if[cur_if])
                    nif++;
                seen_if[cur_if] = 1;
                if_class[cur_if] = d[5];
                if_sub[cur_if] = d[6];
            }
            if (d[5] == 1 && d[6] == 2 && cur_alt == 0 && d[4] == 0)
                alt0_ok = 1;
            break;
        case 5:
            got_eps++;
            if (d[2] == 0x84) {
                iso_ok = d[0] == 9 && d[3] == 0x05 && le16(d + 4) == UA_MAXF * 4u && le16(d + 4) <= 1023u &&
                         d[6] == 1 && cur_alt == 1 && if_sub[cur_if] == 2;
            }
            if ((d[2] == 0x01 || d[2] == 0x81) && d[3] == 2 && le16(d + 4) == 64 && if_sub[cur_if] == 3)
                midi_ok++;
            if (ep_seen[(d[2] & 15u) + (d[2] >> 7) * 16u]++)
                dup_ok = 0;                            /* each address once (no alt shares one here) */
            break;
        case 0x24:
            if (if_class[cur_if] == 1 && if_sub[cur_if] == 1) {      /* audio control */
                if (d[2] == 1) {
                    ac_ok = d[0] == 8u + d[7] && le16(d + 3) == 0x0100u;
                    ac_len_left = ac_wt = (int)le16(d + 5);
                    ac_total = 0;
                    ac_len_left -= d[0];
                    ac_total += d[0];
                    ncoll = d[7];
                    for (i = 0; i < ncoll && i < sizeof coll; i++)
                        coll[i] = d[8 + i];
                } else if (d[2] == 2) {
                    it_ok = d[0] == 12 && d[3] == 1 && d[7] == 2 && le16(d + 8) == 3u && le16(d + 4) != 0x0101u;
                } else if (d[2] == 3) {
                    ot_ok = d[0] == 9 && d[3] == 2 && le16(d + 4) == 0x0101u && d[7] == 1;
                }
            } else if (if_class[cur_if] == 1 && if_sub[cur_if] == 2) {   /* audio streaming */
                if (d[2] == 1)
                    asg_ok = d[0] == 7 && d[3] == 2 && le16(d + 5) == 1u;
                else if (d[2] == 2)
                    fmt_ok = d[0] == 11 && d[3] == 1 && d[4] == 2 && d[5] == 2 && d[6] == 16 && d[7] == 1 &&
                             (d[8] | d[9] << 8 | d[10] << 16) == 44100;
            }
            break;
        case 0x25:
            if (d[2] == 1 && if_sub[cur_if] == 2)
                csep_ok = d[0] == 7 && (d[3] & 1u);
            break;
        }
        off += d[0];
    }
    if (cur_if >= 0 && got_eps != cur_neps)
        eps_ok = 0;
    coll_ok = ncoll == 2;
    for (i = 0; i < ncoll && i < sizeof coll; i++)
        if (coll[i] >= 16 || if_class[coll[i]] != 1 || (if_sub[coll[i]] != 2 && if_sub[coll[i]] != 3))
            coll_ok = 0;

    check("descriptor lengths add up to wTotalLength", lens_ok && off == n);
    check("bNumInterfaces = the interfaces present", c[4] == nif && nif == 3u);
    check("each interface setting has bNumEndpoints endpoints", eps_ok);
    check("endpoint addresses unique", dup_ok);
    check("AC header: UAC 1.00, length 8 + collection", ac_ok);
    check("AC header wTotalLength = the class-specific AC descriptors", ac_len_left == 0 && ac_total == ac_wt);
    check("AC collection: the MIDI and audio streaming interfaces", coll_ok);
    check("input terminal 1: 2 ch, L R, not USB streaming", it_ok);
    check("output terminal 2: USB streaming, source 1", ot_ok);
    check("AS alt 0 has no endpoint", alt0_ok);
    check("AS general: terminal 2, PCM", asg_ok);
    check("type I format: 2 ch, 2-byte subframe, 16 bit, 44100 Hz only", fmt_ok);
    check("EP 0x84: isochronous async, 184 B (46 frames), every frame, alt 1", iso_ok && UA_MAXF == 46u);
    check("CS endpoint: sampling frequency control", csep_ok);
    check("MIDI bulk endpoints 0x01 / 0x81 unchanged", midi_ok == 2);
    check("no CDC classes or IADs", iad_n == 0 &&
          if_class[0] == 1 && if_class[1] == 1 && if_class[2] == 1);
    {
        static const char *names[] = {"M-VAVE", "MC-1 / M-VAVE FM-1"};
        int names_ok = 1;
        for (i = 0; i < 2; i++) {
            const uint8_t *s;
            uint16_t length;
            get_desc(0x0301u + i, &s, &length);
            names_ok &= length == 2u + 2u * strlen(names[i]) && s[0] == length && s[1] == 3;
            for (uint32_t j = 0; j < strlen(names[i]); j++)
                names_ok &= s[2 + j * 2] == (uint8_t)names[i][j] && s[3 + j * 2] == 0;
        }
        check("USB 제조사 M-VAVE 및 제품명 MC-1 / M-VAVE FM-1", names_ok);
    }
}

#define FS_DEV 44117.6
static uint32_t prod_n, cons_n, cons_bad, cons_rep, cons_zero, sizes[64], npk;
static uint32_t last_frame;
static int32_t blk[64];

static void render_block(void)
{
    uint32_t i;
    for (i = 0; i < 32u; i++, prod_n++) {
        blk[2 * i] = (int16_t)prod_n;
        blk[2 * i + 1] = (int16_t)~prod_n;
    }
    uac_tap(blk, 32);
}

static void take_packet(void)
{
    uint32_t d[UA_MAXF + 2], n = uac_packet(d), i;
    sizes[n]++;
    npk++;
    for (i = 0; i < n; i++) {
        uint16_t l = (uint16_t)d[i], r = (uint16_t)(d[i] >> 16);
        if (d[i] == 0 && !uac.go) {
            cons_zero++;
            continue;
        }
        if ((uint32_t)l + r != 65535u) {
            if (d[i] == 0) {                           /* primed silence ahead of the first render */
                cons_zero++;
                continue;
            }
            cons_bad++;
            continue;
        }
        if (cons_n && l == (uint16_t)last_frame)
            cons_rep++;                                 /* a repeated frame (underrun) */
        else if (cons_n && l != (uint16_t)(last_frame + 1u))
            cons_bad++;
        last_frame = l;
        cons_n++;
    }
}

/* run for `ms`; renders take `rend_ns` (or vary with the load when 0); consumer / producer can be paused */
static uint64_t now_ns, next_half, next_pkt;
static uint32_t blocks_left;
static uint64_t next_block, rend_len;
static uint32_t lcg = 12345;
/* render times: 7 % .. 85 % of a half (the overload shedding starts above 85 %) */
#define REND_MIN ((uint64_t)(HALF_FRAMES * 1e9 / FS_DEV * 0.07))
#define REND_MAX ((uint64_t)(HALF_FRAMES * 1e9 / FS_DEV * 0.85))

static void run(uint32_t ms, uint64_t rend_ns, int produce, int consume)
{
    uint64_t end = now_ns + (uint64_t)ms * 1000000u;
    while (now_ns < end) {
        uint64_t t = end;
        if (next_half < t)
            t = next_half;
        if (blocks_left && next_block < t)
            t = next_block;
        if (next_pkt < t)
            t = next_pkt;
        now_ns = t;
        if (now_ns >= end)
            break;
        if (now_ns == next_half) {
            next_half += (uint64_t)((double)HALF_FRAMES / FS_DEV * 1e9);
            if (produce) {
                lcg = lcg * 1103515245u + 12345u;
                rend_len = rend_ns ? rend_ns : REND_MIN + (lcg >> 8) % (REND_MAX - REND_MIN);
                uac_render_start();
                blocks_left = HALF_FRAMES / 32u;
                next_block = now_ns + rend_len / blocks_left;
            }
        } else if (blocks_left && now_ns == next_block) {
            render_block();
            blocks_left--;
            next_block += rend_len / (HALF_FRAMES / 32u);
        } else if (now_ns == next_pkt) {
            lcg = lcg * 1103515245u + 12345u;
            next_pkt = (now_ns / 1000000u + 1u) * 1000000u + (lcg >> 8) % 500000u;   /* in the next frame */
            if (consume)
                take_packet();
        }
    }
}

static void test_ring(void)
{
    uint32_t i, sum, base_n, base_p;
    char s[120];
    usb.config = 1;
    memset(&uac, 0, sizeof uac);
    uac_stream(1);
    next_half = 300000;
    next_pkt = 1000000;
    run(3, 0, 1, 1);                                    /* alt 1 set: no IN token yet */
    check("not fed before the host reads (no overruns while it waits)", uac.overruns == 0 && ua_w == 0 && cons_zero > 0);
    uac.flowing = 1;                                    /* (uac_service: the first packet went) */
    run(2000, 0, 1, 1);
    memset(sizes, 0, sizeof sizes);
    npk = 0;
    base_n = cons_n;
    base_p = uac.underruns + uac.overruns;
    run(10000, 0, 1, 1);
    for (i = sum = 0; i < 64u; i++)
        sum += sizes[i] * i;
    snprintf(s, sizeof s, "10 s, random render times: sizes 43..46 (44:%u 45:%u 46:%u 43:%u)", sizes[44], sizes[45],
             sizes[46], sizes[43]);
    check(s, sizes[44] + sizes[45] + sizes[46] + sizes[43] == npk);
    snprintf(s, sizeof s, "  mean packet %.4f frames (I2S %.1f Hz)", (double)sum / npk, FS_DEV);
    check(s, (double)sum / npk > 44.05 && (double)sum / npk < 44.2);
    check("  every frame once, in order; no underrun / overrun", cons_bad == 0 && cons_rep == 0 &&
          uac.underruns + uac.overruns == base_p && cons_n - base_n == sum);
    snprintf(s, sizeof s, "  fill at render start stays %u..%u (band %u..%u)", uac.fill_lo, uac.fill_hi, UA_LO, UA_HI);
    check(s, uac.fill_lo >= 46u && uac.fill_hi + HALF_FRAMES <= UA_N);
    memset(sizes, 0, sizeof sizes);
    npk = 0;
    for (i = 0; i < 1000u; i++) {                      /* 1000 frames worst case: renders of 85 % */
        run(1, REND_MAX, 1, 1);
    }
    for (i = sum = 0; i < 64u; i++)
        sum += sizes[i] * i;
    snprintf(s, sizeof s, "1000 packets, %.1f ms renders: mean %.3f, no glitch", REND_MAX / 1e6, (double)sum / npk);
    check(s, (double)sum / npk > 44.0 && (double)sum / npk < 44.25 && cons_bad == 0 && cons_rep == 0 &&
          uac.underruns + uac.overruns == base_p);

    base_p = uac.underruns;
    run(30, 0, 0, 1);                                   /* the render stops (a crash would; a stuck ISR) */
    check("producer stopped 30 ms: underruns counted, packets keep their size", uac.underruns > base_p &&
          cons_bad == 0);
    check("  the empty ring repeats the last frame", cons_rep > 0);
    base_p = uac.overruns;
    run(40, 0, 1, 0);                                   /* the consumer stops (flowing still 1) */
    check("consumer stopped 40 ms: overruns counted, the ring never overfills", uac.overruns > base_p &&
          ua_w - ua_r <= UA_N);
    uac_stream(0);
    run(10, 0, 1, 1);
    check("alt 0: nothing fed, nothing taken", ua_w - ua_r <= UA_N && !uac.feed);
    uac_stream(1);
    uac.flowing = 1;
    cons_n = 0;
    cons_bad = cons_rep = 0;
    base_p = uac.underruns + uac.overruns;
    run(3000, 0, 1, 1);
    check("restart: primed again, in order, no glitch", cons_n > 3000u * 44u && cons_bad == 0 && cons_rep == 0 &&
          uac.underruns + uac.overruns == base_p);
}

static void test_pcm_cadence(void)
{
    const int32_t pcm[] = {INT32_MIN, INT32_MAX, -32768, 32767, -1, 1, 0, 0};
    const uint32_t expected[] = {0x7FFF8000u, 0x7FFF8000u, 0x0001FFFFu, 0};
    uint32_t packets[UA_MAXF + 2], count[47] = {0};
    int ok = 1;
    memset(&uac, 0, sizeof uac);
    ua_w = ua_r = 0;
    uac.feed = 1;
    uac_tap(pcm, 4);
    check("PCM: stereo order, int16 boundaries and int32 saturation",
          ua_w == 4 && memcmp(ua_ring, expected, sizeof expected) == 0);
    for (uint32_t i = 0; i < 1000; i++) {
        uint32_t n = uac_packet(packets);
        if (n <= 46) count[n]++;
        else ok = 0;
    }
    check("nominal cadence: exactly 900 x 44 and 100 x 45 per second",
          ok && count[44] == 900 && count[45] == 100);
    uac.go = 1;
    uac.fill_min = UA_LO - 1;
    uac.acc = 0;
    uint32_t down = uac_packet(packets);
    uac.fill_min = UA_HI + 1;
    uac.acc = 900;
    uint32_t up = uac_packet(packets);
    check("drift endpoints: low fill 43, high fill 46 frames", down == 43 && up == 46);
    uac_rate_set(48000);
    check("48 kHz request cannot enable an unavailable rate", !uac.r48);
    usb.config = 1;
    uac_stream(1);
    uac.flowing = 1;
    uac_render_start();
    uac_tap(pcm, 4);
    uac.go = uac.feed = 0;
    uint32_t paused_r = ua_r, paused_w = ua_w;
    uac_tap(pcm, 4);
    uint32_t silent_frames = uac_packet(packets);
    int silent = 1;
    for (uint32_t i = 0; i < silent_frames; i++)
        silent &= packets[i] == 0;
    check("paused UAC: go/feed zero gives silence without consuming or writing",
          silent && ua_r == paused_r && ua_w == paused_w);
    uac_render_start();
    uac_tap(pcm, 4);
    int reprime_ok = uac.go && uac.feed && ua_w - ua_r == UA_PRIME + 4;
    for (uint32_t i = 0; i < UA_PRIME; i++)
        reprime_ok &= ua_ring[(ua_r + i) & (UA_N - 1)] == 0;
    for (uint32_t i = 0; i < 4; i++)
        reprime_ok &= ua_ring[(ua_r + UA_PRIME + i) & (UA_N - 1)] == expected[i];
    check("paused UAC: next render re-primes silence followed by exact PCM", reprime_ok);
    ua_w = ua_r = 0;
}

/* the RX DMA: bytes land in the ring in order, from where it last stopped */
static uint32_t dma_w, dma_ignored, dma_restarts, uart_stops;
static void dma_put(const uint8_t *p, uint32_t n)
{
    if ((FM1_UT1_CON0 & 0x41u) != 0x41u) {
        dma_ignored += n;
        return;
    }
    while (n--) {
        um_ring[dma_w] = *p++;
        dma_w = (dma_w + 1u) & (UM_RING - 1u);
    }
}

static int ring_clean(void)
{
    uint32_t i;
    for (i = 0; i < UM_RING; i++)
        if (um_ring[i] != UM_EMPTY)
            return 0;
    return 1;
}

static void uart_sync(void)
{
    if (!(FM1_UT1_CON0 & 0x41u))
        uart_stops++;
    else if (!(FM1_UT1_CON0 & 1u)) {
        assert(FM1_UT1_RXSADR == (uint32_t)(uintptr_t)um_ring);
        assert(FM1_UT1_RXEADR == (uint32_t)(uintptr_t)um_ring + UM_RING);
        assert(FM1_UT1_RXCNT == UM_RING && ring_clean());
        assert(!um.rd && !um.st && !um.need && !um.got && !um.d0 && !um.sysex);
        dma_w = 0;
        dma_restarts++;
    }
}

/* the ring read by content (the Salt fix): no byte left behind, across the wrap, line noise skipped */
static int test_uart_ring(void)
{
    /* an MPC Sample pad: note-on, running-status poly aftertouch, note-off, then silence */
    static const uint8_t pad[] = {0x90, 0x3A, 0x11, 0xA0, 0x3A, 0x75, 0x3A, 0x7F, 0x3A, 0x06, 0x80, 0x3A, 0x00};
    uint32_t i, k, w0, ok;
    int bad = 0;
    memset(&um, 0, sizeof um);
    uart_midi_init();
    mi_r = mi_w;
    um_drain();
    bad += check("uart ring: nothing landed, nothing read", mi_w == mi_r && um.rd == 0 && um.bytes == 0);

    dma_put(pad, sizeof pad);
    um_drain();
    bad += check("uart ring: a burst's last message (note-off) at once",
                 mi_w - mi_r == 5u && midi_in_q[(mi_w - 1u) % MQ] == 0x003A8018u && ring_clean());
    mi_r = mi_w;

    ok = 1;                                           /* byte by byte, many times round the ring */
    for (k = 0; k < 40u; k++) {
        w0 = mi_w;
        for (i = 0; i < sizeof pad; i++) {
            dma_put(pad + i, 1);
            um_drain();
            if (i == 2u)
                ok &= mi_w == w0 + 1u;                /* the note-on with its third byte */
        }
        ok &= mi_w == w0 + 5u && midi_in_q[(mi_w - 1u) % MQ] == 0x003A8018u && um.rd == dma_w;
        mi_r = mi_w;
    }
    bad += check("uart ring: byte-by-byte arrival across the wrap", ok && ring_clean());

    dma_put((const uint8_t[]){0x90, 0x3C, 0x40, UM_EMPTY}, 4);   /* line noise as the last byte */
    um_drain();
    ok = mi_w - mi_r == 1u && um.rd != dma_w;        /* the stray byte waits for its successor */
    dma_put((const uint8_t[]){0x80, 0x3C, 0x00}, 3);
    um_drain();
    ok &= mi_w - mi_r == 2u && midi_in_q[(mi_w - 1u) % MQ] == 0x003C8018u && um.rd == dma_w;
    bad += check("uart ring: a received FD is skipped, not taken for empty", ok && ring_clean());
    mi_r = mi_w;
    return bad;
}

static void test_uart_pause(void)
{
    const uint8_t on[] = {0x90, 60, 100}, off[] = {0x80, 60, 0};
    uint8_t erase_burst[131];
    uart_midi_start();
    mi_r = mi_w;
    dma_put(on, sizeof on);
    uart_midi_poll();
    check("UART 중지 전 Note On은 즉시 전달된다", mi_w - mi_r == 1 && um.st == 0x90);
    mi_r = mi_w;
    dma_put((const uint8_t[]){62}, 1);
    uart_midi_poll();
    dma_put(off, sizeof off);
    uint32_t stops = uart_stops, starts = dma_restarts, before = um.bytes, ignored = dma_ignored;
    uart_midi_stop();
    memcpy(erase_burst, off, sizeof off);
    memset(erase_burst + sizeof off, 0xFE, sizeof erase_burst - sizeof off);
    for (unsigned i = 0; i < 12; i++)
        dma_put(erase_burst, sizeof erase_burst);
    uart_midi_poll();
    check("UART 저장 경계: RX/DMA off, 1,572바이트 입력 폐기, 미완성 파서 소비 없음",
          !(FM1_UT1_CON0 & 0x41u) && uart_stops > stops &&
          dma_ignored - ignored == 1572 && mi_w == mi_r && um.bytes == before);
    FM1_UT1_RXSADR = FM1_UT1_RXEADR = FM1_UT1_RXCNT = 0;
    uart_midi_start();
    check("UART 재개: 링·부분 메시지·DMA 커서 초기화, RX는 마지막에 켜진다",
          dma_restarts == starts + 1 && dma_w == 0 && ring_clean() &&
          !um.rd && !um.st && !um.got && um.active && (FM1_UT1_CON0 & 0x41u) == 0x41u);
    dma_put((const uint8_t[]){100, 64, 100}, 3);
    uart_midi_poll();
    check("UART 재개 후 이전 running status의 나머지 데이터는 무시된다", mi_r == mi_w);
    dma_put(on, sizeof on);
    dma_put(off, sizeof off);
    uart_midi_poll();
    check("UART 재개 후 새 Note On/Off는 마지막 바이트까지 순서대로 전달된다",
          mi_w - mi_r == 2 && midi_in_q[mi_r % MQ] == 0x643C9019u &&
          midi_in_q[(mi_r + 1) % MQ] == 0x003C8018u && um.rd == dma_w && ring_clean());
    mi_r = mi_w;
}

/* EP1 OUT (USB-MIDI from the computer): back-pressure instead of drops, malformed events ignored */
static int test_usb_ep1(void)
{
    uint8_t pk[64];
    uint32_t i, w0, n = 0;
    int bad = 0;
    for (i = 0; i < 16u; i++) {                       /* 16 note-ons, channel 1 */
        pk[4 * i] = 0x09; pk[4 * i + 1] = 0x90; pk[4 * i + 2] = (uint8_t)(48 + i); pk[4 * i + 3] = 100;
    }
    mi_r = mi_w;
    mi_w += MQ - 20u;                                 /* the ring nearly full: 20 slots free (< 16 + 8) */
    w0 = mi_w;
    bad += check("usb ep1: ring too full -> packet held, nothing dropped", ep1_take(pk, 64) == 0 && mi_w == w0);
    mi_r = mi_w;                                      /* the sequencer drained it */
    bad += check("usb ep1: room again -> all 16 events taken", ep1_take(pk, 64) == 1 && mi_w == w0 + 16u &&
                 midi_in_q[(mi_w - 1u) % MQ] == 0x643F9009u);
    mi_r = mi_w;
    w0 = mi_w;
    {
        static const uint8_t m[] = {
            0x09, 0x80, 60, 100,                      /* status does not match CIN 9: ignored */
            0x09, 0x90, 0xC0, 100,                    /* data byte with bit 7: ignored */
            0x08, 0x80, 60, 0,                        /* a good note-off */
            0x0F, 0xF8, 0, 0,                         /* clock */
            0x0F, 0xFA, 0, 0,                         /* start */
            0x0F, 0xFE, 0, 0,                         /* active sensing: not queued */
            0x0C, 0xC0, 5, 0,                         /* program change (2 bytes) */
        };
        n = ep1_take(m, sizeof m);
        bad += check("usb ep1: malformed ignored, clock / start / note-off / PC queued",
                     n == 1u && mi_w == w0 + 4u && midi_in_q[w0 % MQ] == 0x003C8008u &&
                     midi_in_q[(w0 + 1u) % MQ] == 0xF80Fu && midi_in_q[(w0 + 2u) % MQ] == 0xFA0Fu &&
                     midi_in_q[(w0 + 3u) % MQ] == 0x0005C00Cu);
    }
    mi_r = mi_w;
    return bad;
}

static void test_uart_parser(void)
{
    static const uint8_t in[] = {
        0x90, 60, 100, 62, 101,          /* note on + running status */
        0xF8, 64, 0xFE, 102,             /* realtime inside a message (the clock is queued, cable 1) */
        0xC1, 5, 6,                      /* program change + running */
        0xF0, 0x22, 0x24, 0x35, 0x7D, 0xF7, 70, 71,   /* SysEx dropped; cancels running status */
        0xB0, 7, 0x7F, 0xF2, 1, 2, 9, 9, /* CC, song position (dropped), data without status */
        0x80, 60, 0,
    };
    static const uint32_t want[] = {
        0x643C9019u, 0x653E9019u, 0x0000F81Fu, 0x66409019u, 0x0005C11Cu, 0x0006C11Cu, 0x7F07B01Bu, 0x003C8018u,
    };
    uint32_t i, bad = 0, n = sizeof want / sizeof want[0];
    for (i = 0; i < sizeof in; i++)
        um_byte(in[i]);
    if (mi_w != n) {
        printf("got %u packets, want %u\n", mi_w, n);
        bad = 1;
    }
    for (i = 0; i < n && i < mi_w; i++)
        if (midi_in_q[i] != want[i]) {
            printf("pkt %u: %08x want %08x\n", i, midi_in_q[i], want[i]);
            bad = 1;
        }
    bad += (uint32_t)check("uart: running status, realtime, SysEx, system common", !bad);
    int trs_source_ok = 1;
    for (i = 0; i < n && i < mi_w; i++)
        trs_source_ok &= ((midi_in_q[i] >> 4) & 15u) == 1u;
    check("TRS channel and realtime packets both carry cable 1", trs_source_ok && mi_w == n);
    {   /* 4-track routing reads the channel from the packet as for USB-MIDI: cable 1, CIN = status >> 4 */
        static const uint8_t chs[] = {0x90, 60, 1, 0x91, 61, 2, 0x92, 62, 3, 0x99, 36, 4, 0x9F, 63, 5, 0x89, 36, 0};
        uint32_t w0 = mi_w, ok = 1;
        for (i = 0; i < sizeof chs; i++)
            um_byte(chs[i]);
        for (i = 0; i < 6u; i++) {
            uint32_t pkt = midi_in_q[(w0 + i) % MQ], st = chs[3u * i];
            ok &= mi_w == w0 + 6u && (pkt & 0xFFu) == (0x10u | (st >> 4)) && ((pkt >> 8) & 0xFFu) == st &&
                  ((pkt >> 16) & 0x7Fu) == chs[3u * i + 1u] && (pkt >> 24) == chs[3u * i + 2u];
        }
        bad += (uint32_t)check("uart: channels 1, 2, 3, 10, 16 -> USB-MIDI packets", ok);
    }
}

static void test_midi_edges(void)
{
    static const uint8_t keys[] = {
        0x04, 0xF0, 0x22, 0x24, 0x07, 0x35, 0x7D, 0xF7,
        0x04, 0xF0, 0x22, 0x24, 0x07, 0x35, 0x7F, 0xF7,
        0x05, 0xF7, 0, 0, 0x06, 0xF0, 0xF7, 0,
    };
    mi_r = mi_w;
    uint32_t w0 = mi_w;
    check("USB SysEx: UBOOT inert and OTA follows enabled configuration",
          ep1_take(keys, sizeof keys) && mi_w == w0 && !usb.uboot_req &&
          usb.ota_req == (unsigned)FELUCCA_OTA);
#if FELUCCA_OTA
    usb_ota_clear_queues();
    static const uint8_t query[] = {0x04,0xf0,1,2,0x07,3,4,0xf7};
    const uint8_t *frame; uint32_t length;
    assert(ep1_take(query, sizeof query) && ota_frame_get(&frame, &length));
    assert(length == 4 && !memcmp(frame, "\1\2\3\4", 4));
    ota_frame_done(); assert(!ota_frame_get(&frame, &length));
    static const uint8_t reply[] = {0xf0,1,2,3,4,0xf7};
    usb.config = 1;
    assert(!ota_wire_send(reply, sizeof reply) && so_w - so_r == 2);
    assert(sx_out_q[so_r % SXQ] == 0x0201f004u && sx_out_q[(so_r + 1) % SXQ] == 0xf7040307u);
    so_r = so_w; usb.config = 0;
#endif
    const uint8_t partial[] = {0x09, 0x90, 60, 100, 0x09, 0x90, 61};
    check("USB incomplete event tail is ignored",
          ep1_take(partial, sizeof partial) && mi_w == w0 + 1);
    mi_r = mi_w;
    w0 = mi_w;
    const uint8_t forged[] = {0x19, 0x91, 60, 100, 0x1F, 0xFA, 0, 0};
    check("USB cable 1 cannot impersonate the TRS source",
          ep1_take(forged, sizeof forged) && mi_w == w0 + 2 &&
          midi_in_q[w0 % MQ] == 0x643C9109u && midi_in_q[(w0 + 1) % MQ] == 0xFA0Fu);
    mi_r = mi_w;
    memset(&um, 0, sizeof um);
    w0 = mi_w;
    um_byte(0x90); um_byte(60);
    check("TRS partial message waits for final data byte", mi_w == w0);
    um_byte(0xF8); um_byte(100);
    check("TRS realtime interleave preserves partial note",
          mi_w == w0 + 2 && midi_in_q[w0 % MQ] == 0xF81Fu &&
          midi_in_q[(w0 + 1) % MQ] == 0x643C9019u);
    mi_r = mi_w;
    w0 = mi_w;
    um_byte(61); um_byte(0xC2); um_byte(7);
    check("TRS new status replaces incomplete running-status note",
          mi_w == w0 + 1 && midi_in_q[w0 % MQ] == 0x0007C21Cu);
    mi_r = mi_w;
    mi_w += MQ;
    w0 = mi_w;
    uint32_t drops = um.drops, queued[MQ];
    memcpy(queued, midi_in_q, sizeof queued);
    um_byte(0x80); um_byte(60); um_byte(0);
    check("TRS full queue drops one message without overwriting",
          mi_w == w0 && um.drops == drops + 1 && memcmp(queued, midi_in_q, sizeof queued) == 0);
    mi_r = mi_w;
}

int main(void)
{
    test_descriptors();
    test_pcm_cadence();
    test_ring();
    test_uart_parser();
    (void)test_uart_ring();
    (void)test_usb_ep1();
    test_midi_edges();
    test_uart_pause();
    printf("connections: %s (%d failures)\n", fails ? "FAIL" : "all ok", fails);
    return fails != 0;
}
