/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the Felucca update loader (firmware/loader/ldr_core.c): a fake
 * host serves a package over the SysEx protocol, the "device" flash starts as
 * another package's flash.bin (as if that firmware were installed).
 *   ldr_test OLD.fwsc NEW.fwsc */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FELUCCA_ID "ota-FM-1_900"
#define FELUCCA_OTA_DRYRUN 0

static uint8_t nor[0x100000], *logical;
static size_t logical_len;
static uint8_t rx[1024];
static uint32_t rx_len, rx_full, now_ms, requests, erases, bad_range, record_cleared, f0_asked;
static uint32_t corrupt_at, corrupt_once, no_finish;   /* the host serves a damaged byte / never says "success" */
static uint32_t bad_finish, record_fault, record_erase_attempted, prog_fail_at;
static uint32_t record_check_off = 0xE4000u;
static uint32_t last_idle_ms, slow_record_erases;

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8;
        while (nb >= 7) { out[o++] = acc & 0x7F; acc >>= 7; nb -= 7; }
    }
    if (nb) out[o++] = acc & 0x7F;
    return o;
}
static uint32_t unpack7h(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 7;
        if (nb >= 8) { out[o++] = (uint8_t)acc; acc >>= 8; nb -= 8; }
    }
    return o;
}

static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], m[600];
    uint32_t addr, len, i, s = 0;
    unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) return 0;
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    requests++;
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0; m[6] = 0;
    memcpy(m + 7, &addr, 4);
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr == 0xF0000000u && no_finish) {
        f0_asked++;
        return 0;                                    /* no reply */
    }
    if (addr >= 0xE0000000u) {
        f0_asked += addr == 0xF0000000u;
        memset(m + 14, 0, len);
        memcpy(m + 14, "success", 8);
        if (addr == 0xF0000000u && bad_finish)
            memcpy(m + 14, bad_finish == 1 ? "failure" : "success!", 8);
    } else {
        if (addr + len > logical_len) { printf("read past the package %#x\n", addr); exit(1); }
        memcpy(m + 14, logical + addr, len);
        if (corrupt_at && addr <= corrupt_at && corrupt_at < addr + len) {
            m[14 + corrupt_at - addr] ^= 0x08;       /* (the frame's own checksum is fine: only the CRC sees it) */
            if (corrupt_once)
                corrupt_at = 0;
        }
    }
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    rx_len = pack7(m, 15 + len, rx);
    rx_full = 1;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { if (!rx_full) return 0; *p = rx; *n = rx_len; return 1; }
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { last_idle_ms = ++now_ms; }
static int ota_erase(uint32_t off) { (void)off; return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }
#include "../firmware/update/ota.c"

static int ldr_fread(uint32_t off, void *p, uint32_t n)
{
    if ((record_fault == 1 && off == 0xFBF00u) ||
        (record_fault == 4 && record_erase_attempted && off == record_check_off))
        return -1;
    memcpy(p, nor + off, n);
    return 0;
}
static int ldr_erase(uint32_t off)
{
    if (off < 0x4000u || off >= 0xFC000u || (off & 0xFFFu)) bad_range++;
    if (off < 0x4000u) { printf("ERASE IN THE HEAD %#x\n", off); exit(1); }
    erases++;
    if (slow_record_erases && off >= 0x93000u) {
        /* 각 기록 삭제를 드라이버의 최대 대기시간인 500 ms로 모델링한다. */
        now_ms += 500;
        if (now_ms - last_idle_ms >= 8000u) return -1;
    }
    if (off == 0xE4000u) {
        record_erase_attempted = 1;
        if (record_fault == 2) return -1;
        if (record_fault == 3) return 0;
    }
    memset(nor + off, 0xFF, 0x1000);
    if (off == 0xE4000u && record_fault == 5) nor[record_check_off] = 0;
    return 0;
}
static int ldr_prog(uint32_t off, const void *p, uint32_t n)
{
    const uint8_t *s = p;
    uint32_t i;
    if (prog_fail_at && off == prog_fail_at) return -1;
    if (off < 0x4000u || off + n > 0x93000u || ((off & 0xFFu) + n) > 256u) bad_range++;
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static void ldr_record_clear(void) { record_cleared++; }
static uint32_t flash_unknown;
static int ldr_flash_known(void) { return !flash_unknown; }
static void ldr_progress(uint32_t done, uint32_t total) { (void)done; (void)total; }
#include "../firmware/loader/ldr_core.c"

static uint8_t *load_logical(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *raw = malloc(0x200000), *lg;
    size_t n, i;
    if (!f) { perror(path); exit(2); }
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    lg = malloc(n);
    for (i = 0; i < 20; i++) memcpy(lg + i * 47, raw + i * 48, 47);
    memcpy(lg + 20 * 47, raw + 20 * 48, n - 20 * 48);
    *len = n - 20;
    free(raw);
    return lg;
}

static uint32_t flash_off(const uint8_t *lg)        /* flash.bin offset in the logical image */
{
    uint8_t h[0x400];
    uint32_t i, n;
    memcpy(h, lg, sizeof h);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 0) return ota_rd32(e + 8);
    }
    return 0;
}

static int check(const char *what, int ok) { printf("%-56s %s\n", what, ok ? "ok" : "FAIL"); return ok ? 0 : 1; }

static void put_record(void)                         /* a valid update record at 0xE4F00 (the SPL runs the loader) */
{
    uint8_t r[112] = {0};
    r[2] = 0x0D; r[3] = 0x5A; r[4] = 0x01; r[5] = 0x5A; r[6] = 0x41; r[7] = 0x54;
    ota_wr16(r, ota_crc16(r + 2, 78, 0));
    memcpy(nor + 0xE4F00, r, sizeof r);
}

int main(int argc, char **argv)
{
    size_t olen;
    uint8_t *old, head[0x4000];
    uint32_t ofo, nfo;
    int bad = 0, rc;
    if (argc < 3) { printf("usage: ldr_test OLD.fwsc NEW.fwsc\n"); return 2; }
    old = load_logical(argv[1], &olen);
    logical = load_logical(argv[2], &logical_len);
    ofo = flash_off(old);
    nfo = flash_off(logical);
    /* device: OLD installed, plus a valid update record at 0xE4F00 */
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, old + ofo, 0x93000);
    memcpy(head, nor, sizeof head);
    memset(nor + 0x97000, 0xA5, 0x14000);
    memset(nor + 0xFC000, 0x5A, 0x4000);
    put_record();
    rc = ldr_session();
    printf("  rc %d, %u requests, %u sector erases\n", rc, requests, erases);
    bad += check("install completes", rc == 0);
    bad += check("app area == the new package's flash.bin", !memcmp(nor + 0x4000, logical + nfo + 0x4000, 0x93000 - 0x4000));
    bad += check("flash head [0, 0x4000) untouched", !memcmp(nor, head, sizeof head));
    bad += check("finish asked (0xF0000000)", f0_asked >= 1);
    bad += check("update record cleared (RAM + flash)", record_cleared && nor[0xE4F06] == 0xFF);
    bad += check("no write outside the allowed windows", bad_range == 0);
    for (unsigned i=0x97000; i<0xAB000; i++) if(nor[i]!=0xA5) { bad++; break; }
    for (unsigned i=0xFC000; i<0x100000; i++) if(nor[i]!=0x5A) { bad++; break; }
    bad += check("project banks and calibration/global/tail preserved", bad == 0);
    /* same package again: nothing to erase */
    erases = 0; requests = 0;
    rc = ldr_session();
    bad += check("re-run with the same package erases nothing", rc == 0 && erases == 0);
    /* a package for another chip key is refused before any erase */
    {
        uint8_t save = logical[nfo + 0x4000 + 5];
        logical[nfo + 0x4000 + 5] ^= 0x5A;           /* app area head no longer decrypts */
        memcpy(nor, old + ofo, 0x93000);
        erases = 0;
        rc = ldr_session();
        bad += check("foreign key / damaged app head refused, nothing erased", rc == -6 && erases == 0);
        logical[nfo + 0x4000 + 5] = save;
    }
    /* one bit damaged on the way, once: the CRC sees it, the next pass rewrites that sector */
    {
        const uint8_t r6 = 0x41;                     /* byte 6 of a valid record ("AT"); 0xFF once erased */
        memcpy(nor, old + ofo, 0x93000);
        put_record();
        record_cleared = 0;
        corrupt_at = nfo + 0x30123u;
        corrupt_once = 1;
        rc = ldr_session();
        bad += check("a byte damaged once: caught by the CRC, fixed by a 2nd pass", rc == 0 && record_cleared
                     && nor[0xE4F06] == 0xFF && !memcmp(nor + 0x4000, logical + nfo + 0x4000, 0x93000 - 0x4000));
        /* the package itself damaged (every pass): the record stays, nothing is booted */
        memcpy(nor, old + ofo, 0x93000);
        put_record();
        record_cleared = 0;
        corrupt_at = nfo + 0x30123u;
        corrupt_once = 0;
        rc = ldr_session();
        bad += check("a damaged package: refused after 3 passes, the record stays", rc == -12 && !record_cleared
                     && nor[0xE4F06] == r6);
        corrupt_at = nfo + 0x100u;                   /* in the head: never written, still checked */
        rc = ldr_session();
        bad += check("... also when the damage is in the head", rc == -12 && !record_cleared);
        corrupt_at = 0;
        /* no "success" from the host: the record stays (the host can finish later) */
        no_finish = 1;
        rc = ldr_session();
        bad += check("no \"success\" from the host: the record stays", rc == -13 && !record_cleared && nor[0xE4F06] == r6);
        no_finish = 0;
        rc = ldr_session();
        bad += check("... and the next session finishes", rc == 0 && record_cleared && nor[0xE4F06] == 0xFF);
    }
    /* 부분 설치 후 플래시 식별에 실패해도 손상된 앱으로 부팅하지 않도록 기록을 유지한다. */
    {
        static uint8_t interrupted[sizeof nor];
        memcpy(nor, old + ofo, 0x93000);
        put_record();
        record_cleared = 0;
        prog_fail_at = 0x4000u;
        /* 첫 섹터가 반드시 갱신되도록 기존 이미지를 변경한다. */
        nor[0x4000u] ^= 1;
        rc = ldr_session();
        bad += check("interrupted app programming preserves recovery records",
                     rc == -10 && !record_cleared && nor[0xE4F06] == 0x41);
        prog_fail_at = 0;
        memcpy(interrupted, nor, sizeof nor);
        erases = 0;
        flash_unknown = 1;
        rc = ldr_session();
        bad += check("unknown flash after interruption preserves RAM and flash",
                     rc == LDR_EFLASH && !record_cleared && erases == 0
                     && !memcmp(nor, interrupted, sizeof nor));
        flash_unknown = 0;
        rc = ldr_session();
        bad += check("interrupted installation resumes after flash recognition",
                     rc == 0 && record_cleared && nor[0xE4F06] == 0xFF
                     && !memcmp(nor + 0x4000, logical + nfo + 0x4000, 0x8F000));
    }
    /* 수신 성공만으로 완료로 간주하지 않고 NUL을 포함한 응답 전체를 검증한다. */
    for (bad_finish = 1; bad_finish <= 2; bad_finish++) {
        put_record();
        record_cleared = 0;
        f0_asked = 0;
        erases = 0;
        rc = ldr_session();
        bad += check("negative/malformed finish response preserves records",
                     rc == LDR_ENOFIN && !record_cleared && !erases
                     && f0_asked == 4 && nor[0xE4F06] == 0x41);
    }
    bad_finish = 0;
    /* 읽기 검증 실패와 불완전 삭제는 섹터의 모든 256 B 구간에서 각각 주입한다. */
    for (uint32_t fault = 1; fault <= 5; fault++) {
        uint32_t failures_ok = 1, retries_ok = 1;
        uint32_t chunks = fault >= 4 ? 16u : 1u;
        for (uint32_t chunk = 0; chunk < chunks; chunk++) {
            memset(nor + 0xE4000, 0xFF, 0x1000);
            put_record();
            record_cleared = 0;
            record_erase_attempted = 0;
            record_check_off = 0xE4000u + 256u * chunk;
            record_fault = fault;
            rc = ldr_session();
            failures_ok &= rc == LDR_ERECORD && !record_cleared;
            if (fault <= 3)
                failures_ok &= nor[0xE4F06] == 0x41;
            record_fault = 0;
            rc = ldr_session();
            retries_ok &= rc == 0 && record_cleared && nor[0xE4F06] == 0xFF;
        }
        printf("  record fault %u, %u positions\n", fault, chunks);
        bad += check("record cleanup failure blocks success and RAM clear", failures_ok);
        bad += check("cleanup retry finishes with recovery records invalidated", retries_ok);
    }
    /* 여러 기록 중 앞의 기록만 지운 뒤 실패해도 RAM과 남은 복구 기록을 보존한다. */
    {
        put_record();
        memcpy(nor + 0xFBF00, nor + 0xE4F00, 112);
        memcpy(nor + 0x93F00, nor + 0xE4F00, 112);
        record_cleared = 0;
        record_fault = 2;
        erases = 0;
        rc = ldr_session();
        bad += check("partial record cleanup keeps RAM and remaining flash records",
                     rc == LDR_ERECORD && !record_cleared && erases == 2
                     && nor[0xFBF06] == 0xFF && nor[0xE4F06] == 0x41 && nor[0x93F06] == 0x41);
        record_fault = 0;
        erases = 0;
        rc = ldr_session();
        bad += check("retry clears remaining records including the lowest sector",
                     rc == 0 && record_cleared && erases == 2
                     && nor[0xFBF06] == 0xFF && nor[0xE4F06] == 0xFF && nor[0x93F06] == 0xFF);
    }
    /* 8초가 넘는 다중 기록 정리에서도 watchdog을 계속 갱신한다. */
    {
        int cleared = 1;
        put_record();
        for (uint32_t off = 0xE5F00u; off <= 0xF4F00u; off += 0x1000u)
            memcpy(nor + off, nor + 0xE4F00, 112);
        record_cleared = 0;
        erases = 0;
        last_idle_ms = now_ms;
        slow_record_erases = 1;
        rc = ldr_records_drop();
        slow_record_erases = 0;
        for (uint32_t off = 0xE4F00u; off <= 0xF4F00u; off += 0x1000u)
            cleared &= nor[off + 6] == 0xFF;
        bad += check("long record cleanup feeds the watchdog between sectors",
                     rc == 0 && record_cleared && erases == 17 && cleared);
    }
    {
        int preserved = bad_range == 0 && !memcmp(nor, head, sizeof head);
        for (unsigned i = 0x97000; i < 0xAB000; i++) preserved &= nor[i] == 0xA5;
        for (unsigned i = 0xFC000; i < 0x100000; i++) preserved &= nor[i] == 0x5A;
        bad += check("all fault retries preserve head, project banks and calibration", preserved);
    }
    printf("%s\n", bad ? "LOADER TEST FAILED" : "loader test passed");
    free(old);free(logical);
    return bad != 0;
}
