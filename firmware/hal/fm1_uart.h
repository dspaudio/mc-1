/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM-1 UART1 as MIDI IN: PH8 -> input
 * channel 1 -> UART1 RX, 31250 baud, RX DMA into a ring, polled (no IRQ).
 * The MIDI parser is src/midi_uart.c. Untested on hardware.
 *
 *   fm1_uart1_midi_init()          TIMER5 시작 전 PORTH 설정, RX는 끈 상태
 *   fm1_uart1_midi_start(ring,len) RX가 꺼져 있어야 함. len은 2의 거듭제곱, ring은 16바이트 정렬
 *   fm1_uart1_midi_stop()          RX와 DMA 중지, 대기 플래그 해제
 *   fm1_uart1_rx_take()              bytes DMA'd since the last call; clears the
 *                                    pendings. A polled tally that can miss a byte:
 *                                    src/midi_uart.c reads the ring by content
 */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

#ifndef FM1_UART_REG
#define FM1_UART_REG(addr) (*(volatile uint32_t *)(addr))
#endif
#ifndef FM1_UART_SYNC
#define FM1_UART_SYNC() __asm__ volatile("csync" ::: "memory")
#endif
#define FM1_UT1_CON0   FM1_UART_REG(0x12100u)
#define FM1_UT1_CON1   FM1_UART_REG(0x12104u)
#define FM1_UT1_BAUD   FM1_UART_REG(0x12108u)
#define FM1_UT1_OTCNT  FM1_UART_REG(0x12110u)
#define FM1_UT1_RXSADR FM1_UART_REG(0x1211Cu)
#define FM1_UT1_RXEADR FM1_UART_REG(0x12120u)
#define FM1_UT1_RXCNT  FM1_UART_REG(0x12124u)
#define FM1_UT1_HRXCNT FM1_UART_REG(0x12128u)
#define FM1_UM_CLK_CON1  FM1_UART_REG(0x10010u)   /* [11:10] UART clock: 1 = PLL48M */
#define FM1_UM_IOMAP2    FM1_UART_REG(0x51024u)   /* input ch1 source [13:8] */
#define FM1_UM_IOMAP3    FM1_UART_REG(0x51028u)   /* UT1: b7 fixed IO, [6:4] RX select */
#define FM1_UM_PH_DIR    FM1_UART_REG(0x501C8u)
#define FM1_UM_PH_DIE    FM1_UART_REG(0x501CCu)
#define FM1_UM_PH_PU     FM1_UART_REG(0x501D0u)
#define FM1_UM_PH_PD     FM1_UART_REG(0x501D4u)

FM1_INLINE void fm1_uart1_midi_stop(void)
{
    FM1_UT1_CON0 = 0x3400u;                            /* off, pendings cleared */
    FM1_UART_SYNC();
}

FM1_INLINE void fm1_uart1_midi_init(void)
{
    fm1_uart1_midi_stop();
    FM1_UT1_CON1 = 0;
    FM1_UM_CLK_CON1 = (FM1_UM_CLK_CON1 & ~(3u << 10)) | (1u << 10);
    FM1_UM_IOMAP3 &= ~0x80u;
    FM1_UM_IOMAP3 = (FM1_UM_IOMAP3 & ~0x70u) | (5u << 4);   /* UT1 RX = input ch 1 */
    FM1_UM_IOMAP2 = (FM1_UM_IOMAP2 & ~0x3F00u) | (49u << 8);/* ch 1 = PH8 */
    FM1_UM_PH_PD &= ~(1u << 8);
    FM1_UM_PH_DIE |= 1u << 8;
    FM1_UM_PH_DIR |= 1u << 8;
    FM1_UM_PH_PU |= 1u << 8;
    FM1_UT1_BAUD = 48000000u / 31250u / 4u - 1u;       /* 383 */
    FM1_UT1_OTCNT = 60000u;
}

FM1_INLINE void fm1_uart1_midi_start(volatile uint8_t *ring, uint32_t len)
{
    /* 수신이 꺼진 상태에서 DMA 커서를 버퍼 시작 위치로 되돌린다. */
    FM1_UT1_RXSADR = (uint32_t)(uintptr_t)ring;
    FM1_UT1_RXEADR = (uint32_t)(uintptr_t)ring + len;
    FM1_UT1_RXCNT = len;
    FM1_UART_SYNC();
    FM1_UT1_CON0 = 0x40u | 0x80u | 0x1000u | 0x400u;   /* RXDMA, RDC, clear R/OT; no IEs */
    FM1_UART_SYNC();
    FM1_UT1_CON0 |= 1u;                                /* UTEN last */
}

FM1_INLINE uint32_t fm1_uart1_rx_take(void)
{
    FM1_UT1_CON0 |= 0x80u;                             /* RDC: latch the count */
    FM1_UART_SYNC();
    FM1_UT1_CON0 |= 0x1400u;                           /* clear RPND / OTPND */
    return FM1_UT1_HRXCNT & 0xFFFFu;
}

