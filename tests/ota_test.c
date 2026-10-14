/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* dspaudio/orbit 07e3fed30e8e6757dbe3c6810276599987e4970e의 호스트 테스트를 기반으로 한다.
 * 실제 .fwsc를 M-UPGRADE SysEx로 제공하고 NOR에 앱 단계의 오류를 주입한다.
 *   cc -std=c11 -Wall -Wextra -Werror -Wno-unused-function tests/ota_test.c -o ota_test
 *   ./ota_test FILE.fwsc
 * chip key와 실제 앱 설치 검증은 별도 tests/ldr_test.c에서 수행한다. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OTA_IDENTITY "FM-1_900"
#define FELUCCA_OTA_DRYRUN 0

static uint8_t nor[0x100000], saved_nor[0x100000];
static uint8_t *logical;
static size_t logical_len;
static uint8_t rx[1024], committed_parm[112];
static uint32_t rx_len, rx_full, now_ms, requests, ident_replies, finish_requests;
static uint32_t mute_after, bad_range, erases, programs, commits, order_errors;
static uint32_t fail_erase, fail_program, record_fault, readback_fault;
static uint32_t corrupt_at, corruption, wire_fault, wire_fault_once, send_failure;
static uint32_t finish_fault, finish_confirmed, flash_off, flash_len, loader_off, loader_len;
static uint32_t cleanup_fault, cleanup_check_off, record_written, record_fault_once;
static uint32_t record_verified, record_erased, loader_mutations, record_fault_hits, cleanup_fault_hits;
static int32_t last_code;

static void die(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(2);
}

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8;
        while (nb >= 7) { out[o++] = (uint8_t)(acc & 0x7F); acc >>= 7; nb -= 7; }
    }
    if (nb) out[o++] = (uint8_t)(acc & 0x7F);
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

static void host_reply(const uint8_t *msg, uint32_t n)
{
    rx_len = pack7(msg, n, rx);
    rx_full = 1;
}

static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], m[600];
    uint32_t d, addr, len, i, s = 0;
    if (n < 2 || n > 64 || p[0] != 0xF0 || p[n - 1] != 0xF7)
        die("잘못된 장치 SysEx 프레임");
    d = unpack7h(p + 1, n - 2, u);
    if (d < 7) die("짧은 장치 메시지");
    if (u[2] == 0x11) { ident_replies++; return 0; }
    if (d != 15 || u[2] != 0x30) die("예상하지 못한 장치 메시지");
    addr = (uint32_t)u[7] | (uint32_t)u[8] << 8 | (uint32_t)u[9] << 16 | (uint32_t)u[10] << 24;
    len = (uint32_t)u[11] | (uint32_t)u[12] << 8 | (uint32_t)u[13] << 16;
    if (len > 512) die("요청 길이가 테스트 프레임을 초과함");
    requests++;
    if (send_failure) return -1;
    if (requests > mute_after) return 0;
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0;
    m[6] = 0;
    for (i = 0; i < 4; i++) m[7 + i] = (uint8_t)(addr >> (i * 8));
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr == 0xE0000000u) {
        finish_requests++;
        if (len != 8) die("완료 요청 길이가 8이 아님");
        for (i = 0; i < 112; i++) if (nor[0xE4F00 + i] != 0xFF) order_errors++;
        if (finish_fault == 1) return 0;
        memcpy(m + 14, finish_fault == 2 ? "failure" : finish_fault == 3 ? "success!" : "success", 8);
        finish_confirmed = finish_fault == 0;
    } else {
        if (addr > logical_len || len > logical_len - addr) die("패키지 범위 밖 읽기");
        memcpy(m + 14, logical + addr, len);
        if (corruption && addr <= corrupt_at && corrupt_at - addr < len)
            m[14 + corrupt_at - addr] ^= 1;
    }
    if (wire_fault == 2) m[7] ^= 1;        /* 다른 주소의 응답 */
    if (wire_fault == 3) m[3] ^= 1;        /* 선언된 길이와 실제 길이가 다름 */
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    if (wire_fault == 1) m[14 + len] ^= 1;
    if (wire_fault_once) wire_fault = 0;
    host_reply(m, 15 + len);
    return 0;
}

static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!rx_full) return 0;
    *p = rx; *n = rx_len;
    return 1;
}
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms++; }
static int ota_erase(uint32_t off)
{
    if (off < 0xE0000u || off >= 0xE5000u || (off & 0xFFFu)) { bad_range++; return -1; }
    if (off < 0xE4000u) {
        loader_mutations++;
        if (!record_erased || record_verified != 0xFFFFu) order_errors++;
    } else {
        record_verified = record_erased = 0;
        if (cleanup_fault == 1 || cleanup_fault == 2) {
            erases++;
            cleanup_fault_hits++;
            return cleanup_fault == 1 ? -1 : 0;
        }
    }
    if (++erases == fail_erase) return -1;
    memset(nor + off, 0xFF, 4096);
    if (off == 0xE4000u) record_erased = 1;
    return 0;
}
static int ota_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (off < 0xE0000u || off > 0xE5000u || n > 0xE5000u - off) { bad_range++; return -1; }
    programs++;
    if (off < 0xE4000u) {
        loader_mutations++;
        if (!record_erased || record_verified != 0xFFFFu || n > 0xE4000u - off) order_errors++;
    }
    if (programs == fail_program) return -1;
    if (off == 0xE0000u && memcmp(nor + 0xE0020, logical + loader_off + 32, loader_len - 32))
        order_errors++;
    if (off == 0xE4F00u) {
        record_written = 1;
        if (!finish_confirmed) order_errors++;
        if (record_fault == 1) return -1;
        if (record_fault == 4) {
            for (i = 0; i < n / 2; i++) nor[off + i] &= s[i];
            return -1;
        }
    }
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static int ota_fread(uint32_t off, void *dst, uint32_t n)
{
    uint32_t fault = 0;
    if (off > sizeof nor || n > sizeof nor - off) { bad_range++; return -1; }
    /* 기록 검증의 단발 오류와 정리까지 지속되는 오류를 별도로 모델링한다. */
    if (record_written && off == 0xE4F00u && (record_fault == 2 || record_fault == 3)) {
        fault = record_fault;
        record_fault_hits++;
        if (record_fault_once && n == 112u) record_fault = 0;
    }
    if (record_erased && n == 256u && off == cleanup_check_off && cleanup_fault >= 3) {
        fault = cleanup_fault == 3 ? 2u : 3u;
        cleanup_fault_hits++;
    }
    if (fault == 2 || (readback_fault == 1 && off == 0xE0020u))
        return -1;
    memcpy(dst, nor + off, n);
    if (n && (fault == 3 || (readback_fault == 2 && off == 0xE0020u)))
        ((uint8_t *)dst)[0] ^= 1;
    if (record_erased && off >= 0xE4000u && off < 0xE5000u && n == 256u) {
        uint32_t i;
        for (i = 0; i < n && ((uint8_t *)dst)[i] == 0xFF; i++) ;
        if (i == n) record_verified |= 1u << ((off - 0xE4000u) / 256u);
    }
    return 0;
}
static void ota_show(uint32_t step, int32_t code) { if (step == 9) last_code = code; }
static void ota_commit(const uint8_t *parm)
{
    if (!finish_confirmed || memcmp(nor + 0xE4F00, parm, 112)) order_errors++;
    commits++;
    memcpy(committed_parm, parm, sizeof committed_parm);
}

#include "../firmware/update/ota.c"

static int check(const char *what, int ok)
{
    printf("%s: %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static int area_erased(void)
{
    uint32_t i;
    for (i = OTA_AREA; i < OTA_AREA + OTA_AREA_LEN; i++) if (nor[i] != 0xFF) return 0;
    return 1;
}

static int protected_intact(void)
{
    return !bad_range && !memcmp(nor, saved_nor, OTA_AREA)
        && !memcmp(nor + OTA_AREA + OTA_AREA_LEN, saved_nor + OTA_AREA + OTA_AREA_LEN,
                   sizeof nor - OTA_AREA - OTA_AREA_LEN);
}

static int rejected(int rc, int expected)
{
    return rc == expected && last_code == expected && !commits && !order_errors
        && area_erased() && protected_intact();
}

static void reset_dev(void)
{
    uint32_t i;
    for (i = 0; i < sizeof nor; i++) nor[i] = (uint8_t)((i * 37u) ^ (i >> 12));
    memcpy(nor, logical + flash_off, flash_len);
    memset(nor + OTA_AREA, 0xFF, OTA_AREA_LEN);
    memcpy(saved_nor, nor, sizeof nor);
    memset(committed_parm, 0, sizeof committed_parm);
    rx_full = now_ms = requests = ident_replies = finish_requests = 0;
    bad_range = erases = programs = commits = order_errors = 0;
    fail_erase = fail_program = record_fault = readback_fault = 0;
    corrupt_at = corruption = wire_fault = wire_fault_once = send_failure = 0;
    finish_fault = finish_confirmed = 0;
    cleanup_fault = record_written = record_fault_once = 0;
    record_verified = record_erased = loader_mutations = record_fault_hits = cleanup_fault_hits = 0;
    cleanup_check_off = 0xE4000u;
    last_code = 0;
    mute_after = UINT32_MAX;
    ota_deadline = 0;
}

static void load_package(const char *path)
{
    FILE *f = fopen(path, "rb");
    uint8_t *raw;
    uint8_t header[0x400];
    size_t n, i;
    uint32_t nent;
    if (!f) { perror(path); exit(2); }
    raw = malloc(0x200000);
    if (!raw) die("패키지 버퍼 할당 실패");
    n = fread(raw, 1, 0x200000, f);
    if (ferror(f) || n <= 0x414 || n == 0x200000) die("패키지 크기 또는 읽기 오류");
    fclose(f);
    logical_len = n - 20;
    logical = malloc(logical_len);
    if (!logical) die("논리 패키지 버퍼 할당 실패");
    for (i = 0; i < 20; i++) memcpy(logical + i * 47, raw + i * 48, 47);
    memcpy(logical + 20 * 47, raw + 20 * 48, n - 20 * 48);
    free(raw);
    memcpy(header, logical, sizeof header);
    ota_jl_enc(header, 0x40);
    nent = ota_rd16(header + 8);
    if (!nent || nent > 11 || ota_rd16(header) != ota_crc16(header + 2, 0x3E, 0)
        || ota_rd16(header + 2) != ota_crc16(header + 0x40, nent * 0x50, 0))
        die("기준 패키지 헤더 CRC가 유효하지 않음");
    for (i = 0; i < nent; i++) {
        uint8_t *entry = header + 0x40 + i * 0x50;
        ota_jl_enc(entry, 0x50);
        if (ota_rd16(entry) == 0) { flash_off = ota_rd32(entry + 8); flash_len = ota_rd32(entry + 12); }
        if (ota_rd16(entry) == 100) { loader_off = ota_rd32(entry + 8); loader_len = ota_rd32(entry + 12); }
    }
    if (!flash_off || !loader_off || flash_len > OTA_AREA || flash_len < 0x4020u
        || flash_off > logical_len || flash_len > logical_len - flash_off
        || loader_off > logical_len || loader_len > logical_len - loader_off || loader_len < 512)
        die("기준 패키지의 flash.bin 또는 ota.bin 범위가 유효하지 않음");
}

/* 엔트리의 암호화와 CRC를 유지하여 의미 검증 경로까지 도달한다. */
static void hide_entry(uint32_t type)
{
    uint32_t i, nent;
    ota_jl_enc(logical, 0x40);
    nent = ota_rd16(logical + 8);
    for (i = 0; i < nent; i++) {
        uint8_t *entry = logical + 0x40 + i * 0x50;
        ota_jl_enc(entry, 0x50);
        if (ota_rd16(entry) == type) ota_wr16(entry, 0xFFFF);
        ota_jl_enc(entry, 0x50);
    }
    ota_wr16(logical + 2, ota_crc16(logical + 0x40, nent * 0x50, 0));
    ota_wr16(logical, ota_crc16(logical + 2, 0x3E, 0));
    ota_jl_enc(logical, 0x40);
}

static void set_entry_length(uint32_t type, uint32_t length)
{
    uint32_t i, nent;
    ota_jl_enc(logical, 0x40);
    nent = ota_rd16(logical + 8);
    for (i = 0; i < nent; i++) {
        uint8_t *entry = logical + 0x40 + i * 0x50;
        ota_jl_enc(entry, 0x50);
        if (ota_rd16(entry) == type) ota_wr32(entry + 12, length);
        ota_jl_enc(entry, 0x50);
    }
    ota_wr16(logical + 2, ota_crc16(logical + 0x40, nent * 0x50, 0));
    ota_wr16(logical, ota_crc16(logical + 2, 0x3E, 0));
    ota_jl_enc(logical, 0x40);
}

int main(int argc, char **argv)
{
    uint8_t saved_header[0x400], saved_loader[512];
    uint8_t previous_loader[0x4000], previous_record[112];
    uint32_t total_requests, total_programs, body_len;
    int bad = 0, rc;
    if (argc != 2) { fprintf(stderr, "사용법: ota_test FILE.fwsc\n"); return 2; }
    load_package(argv[1]);
    memcpy(saved_header, logical, sizeof saved_header);
    memcpy(saved_loader, logical + loader_off, sizeof saved_loader);
    body_len = ota_rd32(logical + loader_off + 8);

    reset_dev();
    {
        static const uint8_t handshake[7] = {0, 0x59, 0x11, 0, 0, 0, 0xFF};
        host_reply(handshake, sizeof handshake);
        ota_service();
        bad += check("M-UPGRADE handshake와 앱 identity", ident_replies == 1 && ota_msg[2] == 0x11
                     && !memcmp(ota_msg + 6, OTA_IDENTITY, sizeof OTA_IDENTITY - 1));
    }

    reset_dev();
    rc = ota_session();
    total_requests = requests;
    total_programs = programs;
    printf("정상 경로: %u 요청, %u 삭제, %u 쓰기\n", requests, erases, programs);
    memcpy(previous_loader, nor + OTA_AREA, sizeof previous_loader);
    memcpy(previous_record, nor + OTA_RES, sizeof previous_record);
    bad += check("호스트 완료 확인 이후 flash와 RAM record commit", rc == 0 && commits == 1
                 && finish_requests == 1 && !order_errors && protected_intact());
    bad += check("staging loader 전체가 실제 패키지와 일치", body_len <= 0x4000u - 32u
                 && !memcmp(nor + OTA_AREA + 32, logical + loader_off + 32, body_len));
    bad += check("LOADER.BIN 헤더 주소·길이·CRC", ota_rd16(nor + OTA_AREA) == ota_crc16(nor + OTA_AREA + 2, 30, 0)
                 && ota_rd32(nor + OTA_AREA + 4) == OTA_AREA && ota_rd32(nor + OTA_AREA + 8) == body_len + 32
                 && !memcmp(nor + OTA_AREA + 16, "LOADER.BIN", 10));
    bad += check("UPDATA_PARM flash/RAM 일치와 CRC·타입·매직·주소", !memcmp(nor + OTA_RES, committed_parm, 112)
                 && ota_rd16(committed_parm) == ota_crc16(committed_parm + 2, 78, 0)
                 && ota_rd16(committed_parm + 2) == 0x5A0D && ota_rd16(committed_parm + 4) == 0x5A01
                 && ota_rd16(committed_parm + 6) == 0x5441 && ota_rd32(committed_parm + 72) == OTA_AREA);
    bad += check("부팅 정리 후 staging 삭제와 보호영역 보존", ota_boot_cleanup() == 1 && area_erased() && protected_intact());
    erases = 0;
    bad += check("다음 부팅은 삭제하지 않음", ota_boot_cleanup() == 0 && !erases);

    /* Given: 이전 유효 기록과 로더. When: 정리 실패/재시도. Then: 로더 전체 보존. */
    for (uint32_t fault = 1; fault <= 4; fault++) {
        int cleanup_ok = 1, stage_ok = 1, retry_ok = 1;
        uint32_t chunks = fault >= 3 ? 16u : 1u;
        for (uint32_t chunk = 0; chunk < chunks; chunk++) {
            reset_dev();
            memcpy(nor + OTA_AREA, previous_loader, sizeof previous_loader);
            memcpy(nor + OTA_RES, previous_record, sizeof previous_record);
            memcpy(saved_nor, nor, sizeof nor);
            cleanup_fault = fault;
            cleanup_check_off = 0xE4000u + chunk * 256u;
            rc = ota_unstage();
            if (!chunk) printf("정리 fault=%u rc=%d loader_mutations=%u loader_equal=%d record_crc_valid=%d\n",
                fault, rc, loader_mutations, !memcmp(nor + OTA_AREA, previous_loader, sizeof previous_loader),
                ota_rd16(nor + OTA_RES) == ota_crc16(nor + OTA_RES + 2, 78, 0));
            cleanup_ok &= rc == -16 && !commits && !loader_mutations && !order_errors
                && cleanup_fault_hits == 1 && protected_intact()
                && !memcmp(nor + OTA_AREA, previous_loader, sizeof previous_loader);
            if (fault <= 2)
                cleanup_ok &= !memcmp(nor + OTA_RES, previous_record, sizeof previous_record)
                    && ota_rd16(nor + OTA_RES) == ota_crc16(nor + OTA_RES + 2, 78, 0);
            /* 동일 고장이 지속되는 재스테이징도 로더를 변경할 수 없다. */
            reset_dev();
            memcpy(nor + OTA_AREA, previous_loader, sizeof previous_loader);
            memcpy(nor + OTA_RES, previous_record, sizeof previous_record);
            memcpy(saved_nor, nor, sizeof nor);
            cleanup_fault = fault;
            cleanup_check_off = 0xE4000u + chunk * 256u;
            rc = ota_session();
            if (!chunk) printf("스테이징 fault=%u rc=%d loader_mutations=%u loader_equal=%d\n",
                fault, rc, loader_mutations, !memcmp(nor + OTA_AREA, previous_loader, sizeof previous_loader));
            stage_ok &= rc == -16 && last_code == -16 && !commits && !programs
                && !loader_mutations && !order_errors && protected_intact()
                && !memcmp(nor + OTA_AREA, previous_loader, sizeof previous_loader);
            cleanup_fault = 0;
            last_code = 0;
            rc = ota_session();
            retry_ok &= rc == 0 && commits == 1 && !order_errors && protected_intact();
        }
        printf("기록 섹터 지속 오류 %u: %u 위치\n", fault, chunks);
        bad += check("기록 삭제·전체 readback 실패는 로더 16384바이트 보존", cleanup_ok);
        bad += check("지속 오류 중 재스테이징은 -16·RAM commit 없음·로더 보존", stage_ok);
        bad += check("고장 제거 후 기록 검증을 먼저 완료하고 재스테이징", retry_ok);
    }

    /* 매 요청 경계에서 연결을 끊어 header/body/확인 단계 전체를 검사한다. */
    {
        int all_ok = total_requests > 4;
        for (uint32_t cut = 0; cut < total_requests; cut++) {
            reset_dev();
            mute_after = cut;
            rc = ota_session();
            all_ok &= rc < 0 && rejected(rc, rc) && now_ms <= 120000u;
        }
        printf("연결 중단 위치: %u개\n", total_requests);
        bad += check("모든 통신 중단에서 commit 없음·staging 정리·보호영역 보존", all_ok);
    }
    reset_dev();
    send_failure = 1;
    bad += check("전송 오류 거부", rejected(ota_session(), -1));
    for (uint32_t fault = 1; fault <= 3; fault++) {
        reset_dev();
        wire_fault = fault;
        bad += check("잘못된 체크섬·주소·길이 프레임 거부", rejected(ota_session(), -1));
        reset_dev();
        wire_fault = fault;
        wire_fault_once = 1;
        rc = ota_session();
        bad += check("일회성 프레임 오류는 재요청으로 복구", rc == 0 && commits == 1
                     && requests == total_requests + 1 && !order_errors && protected_intact());
    }

    for (uint32_t offset = 0; offset <= 0x40; offset += 0x40) {
        reset_dev();
        corruption = 1;
        corrupt_at = offset;
        bad += check("UFW header·entry table CRC 오류 거부", rejected(ota_session(), -2) && !programs);
    }
    for (uint32_t type = 0; type <= 100; type += 100) {
        reset_dev();
        hide_entry(type);
        bad += check("필수 flash.bin·ota.bin 엔트리 누락 거부", rejected(ota_session(), -3) && !programs);
        memcpy(logical, saved_header, sizeof saved_header);
    }
    for (uint32_t fault = 0; fault < 5; fault++) {
        uint8_t *loader = logical + loader_off;
        reset_dev();
        if (fault == 0) loader[0] ^= 1;
        if (fault == 1) { loader[12] = 0; ota_wr16(loader, ota_crc16(loader + 2, 30, 0)); }
        if (fault == 2) {
            ota_wr32(loader + 0x28, 0x01C0A804);
            ota_wr16(loader + 0x20, ota_crc16(loader + 0x22, 30, 0));
        }
        if (fault == 3) { ota_wr32(loader + 8, OTA_AREA_LEN); ota_wr16(loader, ota_crc16(loader + 2, 30, 0)); }
        if (fault == 4) memset(loader + 0x48, 0, 64);
        bad += check("loader 헤더 CRC·타입·진입주소·크기·식별자 오류 거부", rejected(ota_session(), -7) && !programs);
        memcpy(loader, saved_loader, sizeof saved_loader);
    }
    /* Given: 이전 복구 상태와 CRC가 맞는 잘못된 길이. When: 세션. Then: 삭제/쓰기 없음. */
    for (uint32_t fault = 0; fault < 8; fault++) {
        uint8_t *loader = logical + loader_off;
        int expected = fault == 1 || fault == 2 || fault >= 6 ? -3 : -7;
        reset_dev();
        memcpy(nor + OTA_AREA, previous_loader, sizeof previous_loader);
        memcpy(nor + OTA_RES, previous_record, sizeof previous_record);
        memcpy(saved_nor, nor, sizeof nor);
        if (fault == 0) set_entry_length(100, 511);
        if (fault == 1) set_entry_length(0, 0);
        if (fault == 2) set_entry_length(0, UINT32_MAX);
        if (fault == 3) set_entry_length(100, UINT32_MAX);
        if (fault == 4 || fault == 5) {
            uint32_t length = fault == 4 ? 0x4000u - 31u : UINT32_MAX;
            ota_wr32(loader + 8, length);
            ota_wr16(loader, ota_crc16(loader + 2, 30, 0));
            set_entry_length(100, length + 32u);
        }
        if (fault == 6) set_entry_length(0, 0x92FFFu);
        if (fault == 7) set_entry_length(0, 0x100001u);
        rc = ota_session();
        bad += check("잘못된 패키지·loader 길이는 파괴적 쓰기 전에 거부",
                     rc == expected && last_code == expected && !commits && !erases && !programs
                     && !order_errors && !memcmp(nor, saved_nor, sizeof nor));
        memcpy(logical, saved_header, sizeof saved_header);
        memcpy(loader, saved_loader, sizeof saved_loader);
    }
    /* 최대 허용 본문은 정확히 0xE4000 앞에서 끝나며 기록 섹터와 겹치지 않는다. */
    {
        uint8_t *original = logical;
        size_t original_len = logical_len;
        uint32_t original_loader_len = loader_len, limit = 0x4000u - 32u;
        logical_len = original_len > loader_off + 0x4000u ? original_len : loader_off + 0x4000u;
        logical = calloc(logical_len, 1);
        if (!logical) die("경계 패키지 버퍼 할당 실패");
        memcpy(logical, original, original_len);
        loader_len = limit + 32u;
        ota_wr32(logical + loader_off + 8, limit);
        ota_wr16(logical + loader_off + 2, ota_crc16(logical + loader_off + 32, limit, 0));
        ota_wr16(logical + loader_off, ota_crc16(logical + loader_off + 2, 30, 0));
        set_entry_length(100, loader_len);
        reset_dev();
        rc = ota_session();
        bad += check("최대 loader 본문 16352바이트는 기록 섹터 비중첩으로 staging",
                     rc == 0 && commits == 1 && !order_errors && protected_intact()
                     && !memcmp(nor + OTA_AREA + 32, logical + loader_off + 32, limit));
        free(logical);
        logical = original;
        logical_len = original_len;
        loader_len = original_loader_len;
    }
    reset_dev();
    corruption = 1;
    corrupt_at = loader_off + 0x220;
    bad += check("SysEx 체크섬이 맞아도 loader body CRC 오류 거부", rejected(ota_session(), -11));
    for (uint32_t fault = 1; fault <= 2; fault++) {
        reset_dev();
        readback_fault = fault;
        bad += check("loader flash readback 실패·CRC 불일치 거부", rejected(ota_session(), -11));
    }

    /* chip key는 stage2가 검사하므로 stage1은 앱·head를 쓰지 않는지 확인한다. */
    reset_dev();
    logical[flash_off + 0x4005] ^= 0x5A;
    rc = ota_session();
    bad += check("앱 head가 달라도 stage1은 loader만 staging하고 기존 앱을 보존", rc == 0
                 && commits == 1 && protected_intact() && !order_errors);
    logical[flash_off + 0x4005] ^= 0x5A;

    for (uint32_t fault = 1; fault <= 3; fault++) {
        reset_dev();
        finish_fault = fault;
        bad += check("완료 무응답·failure·NUL 없는 success 응답 거부", rejected(ota_session(), -13)
                     && finish_requests == (fault == 1 ? 12u : 4u));
    }
    {
        int erase_ok = 1, program_ok = 1;
        for (uint32_t fault = 1; fault <= OTA_AREA_LEN / 0x1000u; fault++) {
            reset_dev();
            fail_erase = fault;
            erase_ok &= rejected(ota_session(), -8);
        }
        for (uint32_t fault = 1; fault <= total_programs; fault++) {
            int expected = fault == total_programs ? -14 : fault + 1 == total_programs ? -12 : -10;
            reset_dev();
            fail_program = fault;
            program_ok &= rejected(ota_session(), expected);
        }
        printf("flash 오류 위치: 삭제 %u개, 쓰기 %u개\n", OTA_AREA_LEN / 0x1000u, total_programs);
        bad += check("모든 staging 삭제 오류 후 정리와 보호영역 보존", erase_ok);
        bad += check("모든 body·header·record 쓰기 오류 후 정리와 보호영역 보존", program_ok);
    }
    for (uint32_t fault = 1; fault <= 4; fault++) {
        reset_dev();
        record_fault = fault;
        record_fault_once = 1;
        rc = ota_session();
        bad += check("record 단발 쓰기·읽기·불일치·부분쓰기 실패는 -14와 검증된 정리",
                     rejected(rc, -14) && (fault == 2 || fault == 3 ? record_fault_hits == 1 : 1));
        record_fault = 0;
        last_code = 0;
        rc = ota_session();
        bad += check("record 실패 이후 새 세션으로 복구", rc == 0 && commits == 1
                     && protected_intact() && !order_errors);
    }
    /* Given: 기록 읽기의 지속 고장. When: commit 검증 실패. Then: 정리도 -16으로 실패. */
    for (uint32_t fault = 2; fault <= 3; fault++) {
        reset_dev();
        record_fault = fault;
        rc = ota_session();
        bad += check("record 지속 읽기·불일치는 -16·commit 0·로더 보존·보호영역 보존",
                     rc == -16 && last_code == -16 && !commits && !order_errors
                     && record_fault_hits == 2 && erases == 6 && protected_intact()
                     && !memcmp(nor + OTA_AREA, previous_loader, sizeof previous_loader));
        record_fault = 0;
        last_code = 0;
        rc = ota_session();
        bad += check("지속 record 오류 제거 후 새 세션 복구", rc == 0 && commits == 1
                     && !order_errors && protected_intact());
    }
    reset_dev();
    nor[OTA_RES] = 0;
    bad += check("record만 남은 부팅도 staging 정리", ota_boot_cleanup() == 1 && area_erased() && protected_intact());
    reset_dev();
    record_written = record_fault = 2;
    bad += check("부팅 정리 읽기 실패는 오류 반환·삭제 없음", ota_boot_cleanup() == -1 && !erases && protected_intact());

    printf("%s\n", bad ? "OTA TEST FAILED" : "ota test passed");
    free(logical);
    return bad != 0;
}
