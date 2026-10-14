/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#pragma once
#include <stdint.h>

#define MC_OTA_LO 0xE0000u
#define MC_OTA_HI 0xE5000u
#define MC_OTA_IN(off, n, lo, hi) ((uint32_t)(off) >= (lo) && (uint32_t)(off) <= (hi) && \
                                  (uint32_t)(n) <= (hi) - (uint32_t)(off))
#define MC_OTA_WRITE_OK(off, n) MC_OTA_IN(off, n, MC_OTA_LO, MC_OTA_HI)
#define MC_OTA_READ_OK(off, n) (MC_OTA_WRITE_OK(off, n) || MC_OTA_IN(off, n, 0u, 0x4000u))

int mc_ota_flash_probe(void);
int mc_ota_flash_erase(uint32_t off);
int mc_ota_flash_program(uint32_t off, const void *src, uint32_t n);
int mc_ota_flash_read(uint32_t off, void *dst, uint32_t n);
