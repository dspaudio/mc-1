/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "ota_flash.h"
/* 별도 컴파일 단위의 RAM 드라이버는 staging만 쓴다. 앱 저장 허용목록은 바꾸지 않는다. */
#define FL_RANGE_OK(off, n) MC_OTA_WRITE_OK(off, n)
#include "../hal/fm1_flash.h"

static int chip_known;

int mc_ota_flash_probe(void)
{
    uint32_t flags = irq_save();
    chip_known = FL_FAR(fl_jedec_ram)() == 0x856014u;
    irq_restore(flags);
    return chip_known;
}

int mc_ota_flash_erase(uint32_t off)
{
    uint32_t took;
    if (!chip_known || !MC_OTA_WRITE_OK(off, 0x1000u) || (off & 0xFFFu))
        return -1;
    return fl_erase4k(off, &took);
}

int mc_ota_flash_read(uint32_t off, void *dst, uint32_t n)
{
    uint32_t flags;
    int rc;
    if (!chip_known || !MC_OTA_READ_OK(off, n))
        return -1;
    flags = irq_save();
    rc = FL_FAR(fl_read_ram)(off, dst, n);
    irq_restore(flags);
    return rc;
}

int mc_ota_flash_program(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *p = src;
    uint8_t back[256];
    if (!chip_known || !n || !MC_OTA_WRITE_OK(off, n))
        return -1;
    while (n) {
        uint32_t chunk = 256u - (off & 0xFFu), i;
        if (chunk > n)
            chunk = n;
        if (fl_write(off, p, chunk) || mc_ota_flash_read(off, back, chunk))
            return -2;
        for (i = 0; i < chunk; i++)
            if (back[i] != p[i])
                return -3;
        off += chunk;
        p += chunk;
        n -= chunk;
    }
    return 0;
}
