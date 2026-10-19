/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Charles Vestal: X0X CPU1 startup, after Kerem Kilic's Melodee.
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments: safe hold.
 * Copyright (C) 2026 Andre Rezemini: SLOOP boot/self-test integration.
 * Via SLOOP v2.7.1 4ec1ac829fd2bdf2c571ea215fb3d62477d01220.
 * Copyright (C) 2026 dspaudio: isolated four-bank MC-1 job, bounded waits and stack checks. */
#pragma once
#include "fm1_time.h"
#include "fm1_guard.h"
#include "../../src/parallel.h"
#define MC_C1_CON (*(volatile uint32_t *)0x1eee004u)
#define MC_C1_CLK (*(volatile uint32_t *)0x10008u)
#define MC_C1_MARK 0x43314d4bu
static McDual mc_cpu1 __attribute__((section(".pool"),aligned(4)));
static McAtomic mc_c1_alive;
#if MC_CPU1_TRACE == 1
static McAtomic mc_c1_trace,mc_c1_heartbeat,mc_c1_peak_ticks;
static void mc_dual_trace(unsigned stage) {
    mc_release(&mc_c1_trace,stage);
    if(stage==97&&mc_cpu1.job_ticks>mc_acquire(&mc_c1_peak_ticks))
        mc_release(&mc_c1_peak_ticks,mc_cpu1.job_ticks);
}
#endif
static unsigned mc_cpu1_held;
extern void mc_c1_entry(void);
extern uint32_t _mc_c1_ustack[],_mc_c1_sstack[];
extern uint32_t _mc_c1_guard0[],_mc_c1_guard1[],_mc_c1_guard2[];

/* SLOOP 2.7.1's hold fix: aborted bus accesses can otherwise trap CPU0.
 * No PLL, divider, voltage or cache configuration is changed. */
static void mc_cpu1_halt(McDual *d) {
    uint32_t saved=fm1_icfg();fm1_irq_off();
    uint32_t locked=!(FM1_DBG_WR_EN&1u),bus=0x003f0030u;
    __asm__("" : "+r"(bus));
    if(locked)FM1_DBG_WR_EN=0xe7u;
    uint32_t enabled=FM1_DBG_EN;
    FM1_DBG_EN=enabled&~bus;
    __asm__ volatile("csync" ::: "memory");
    MC_C1_CON|=2u;MC_C1_CON&=~8u;
    __asm__ volatile("csync" ::: "memory");
    for(volatile unsigned n=0;n<256u;n++) {}
    FM1_DBG_MSG_CLR=bus;
    __asm__ volatile("csync" ::: "memory");
    FM1_DBG_EN=enabled;
    if(locked&&(FM1_DBG_WR_EN&1u))FM1_DBG_WR_EN=0xe7u;
    __asm__ volatile("csync" ::: "memory");
    /* Readback is a configuration check, not a measured silicon bus-drain proof.
     * The failed payload is never reused even if the core does not respond. */
    mc_cpu1_held=(MC_C1_CON&10u)==2u;
    if(!mc_cpu1_held)d->status=MC_DUAL_PROTOCOL;
    fm1_icfg_set(saved);
}
static uint32_t mc_cpu1_ticks(void) { return fm1_ticks(); }
static __attribute__((noinline)) uint32_t mc_cpu1_selftest(void) {
    uint32_t value=20261019u;
    for(unsigned i=0;i<64;i++)value=value*1664525u+1013904223u;
    return value;
}
/* Indirect calls cross the >4 MiB RAM/XIP branch range on pi32v2. */
static uint32_t (*volatile mc_c1_test_fn)(void)=mc_cpu1_selftest;
static void (*volatile mc_c1_job_fn)(McDual *,uint32_t)=mc_dual_work;
/* Acknowledgement is in RAM AFTER every XIP call has returned. Therefore an
 * observed done means the worker cannot fetch flash again without a new job. */
void __attribute__((section(".c1_text"),noreturn,used)) mc_c1_main(void) {
    mc_release(&mc_c1_alive,mc_c1_test_fn());
#if MC_CPU1_TRACE == 1
    unsigned loops=0;
    mc_release(&mc_c1_trace,1);
#endif
    for(;;) {
#if MC_CPU1_TRACE == 1
        if(!(++loops&4095u))mc_release(&mc_c1_heartbeat,loops);
#endif
        uint32_t job=mc_acquire(&mc_cpu1.request);
        if(job!=mc_acquire(&mc_cpu1.done)) {
#if MC_CPU1_TRACE == 1
            mc_release(&mc_c1_trace,2);
#endif
            mc_c1_job_fn(&mc_cpu1,job);
#if MC_CPU1_TRACE == 1
            mc_release(&mc_c1_trace,3);
#endif
            mc_release(&mc_cpu1.done,job);
#if MC_CPU1_TRACE == 1
            mc_release(&mc_c1_trace,4);
#endif
        }
    }
}
static uint32_t mc_cpu1_stack_free(void) {
    unsigned i=0;
    while(i<4096u/4u&&_mc_c1_ustack[i]==MC_C1_MARK)i++;
    return i*4u;
}
static int mc_cpu1_guards(void) {
    for(unsigned i=0;i<8;i++)
        if(_mc_c1_guard0[i]!=MC_C1_MARK||_mc_c1_guard1[i]!=MC_C1_MARK||
           _mc_c1_guard2[i]!=MC_C1_MARK)return 0;
    return 1;
}
static void mc_cpu1_boot(void) {
    mc_cpu1.halt=mc_cpu1_halt;mc_cpu1.ticks=mc_cpu1_ticks;
    mc_cpu1.budget_ticks=12000u; /* 500 us from dispatch, not an 8 ms audio wait. */
    mc_cpu1.spin_limit=100000u; /* Also bounded if TIMER4 is stuck. */
    mc_cpu1_halt(&mc_cpu1);
    if(!mc_cpu1_held) { mc_cpu1.status=MC_DUAL_BOOT_FAILED;return; }
    for(unsigned i=0;i<4096u/4u;i++)_mc_c1_ustack[i]=MC_C1_MARK;
    for(unsigned i=0;i<512u/4u;i++)_mc_c1_sstack[i]=MC_C1_MARK;
    for(unsigned i=0;i<8;i++)_mc_c1_guard0[i]=_mc_c1_guard1[i]=_mc_c1_guard2[i]=MC_C1_MARK;
    for(unsigned i=0;i<32;i++)*(volatile uint32_t *)(0x1eef300u+4u*i)=0;
    *(volatile uint32_t *)0x01c7fff8u=(uint32_t)(uintptr_t)mc_c1_entry;
    /* CPU1 starts through ROM. Open only the PC window during the bounded boot. */
    fm1__dbg_unlock();
    uint32_t low=FM1_PC_LIMIT0_L,high=FM1_PC_LIMIT0_H;
    FM1_PC_LIMIT0_L=0;FM1_PC_LIMIT0_H=0xffffffffu;
    fm1__dbg_lock();
    uint32_t clock=MC_C1_CLK;
    MC_C1_CLK=clock|8u; /* Vendor EnableOtherCpu startup gate; restored below. */
    __asm__ volatile("csync" ::: "memory");
    mc_cpu1_held=0;MC_C1_CON|=8u;MC_C1_CON&=~2u;
    uint32_t begin=fm1_ticks();
    unsigned spins=0;
    while(!mc_acquire(&mc_c1_alive)&&fm1_ticks()-begin<480000u&&spins++<2000000u) {}
    MC_C1_CLK=clock;
    if(mc_acquire(&mc_c1_alive)!=mc_cpu1_selftest()) {
        mc_cpu1.status=MC_DUAL_BOOT_FAILED;mc_cpu1_halt(&mc_cpu1);
    } else mc_cpu1.status=MC_DUAL_READY;
    fm1__dbg_unlock();FM1_PC_LIMIT0_L=low;FM1_PC_LIMIT0_H=high;fm1__dbg_lock();
}
static void mc_cpu1_idle(void) {
    if(mc_cpu1.status==MC_DUAL_READY&&
       (mc_acquire(&mc_cpu1.request)!=mc_acquire(&mc_cpu1.done)||!mc_cpu1_guards()))
        mc_dual_fail(&mc_cpu1,MC_DUAL_PROTOCOL);
}
static int mc_cpu1_flash_safe(void) {
    mc_cpu1_idle();
    return mc_cpu1.status==MC_DUAL_OFF||mc_cpu1_held||
        (mc_cpu1.status==MC_DUAL_READY&&mc_acquire(&mc_cpu1.request)==mc_acquire(&mc_cpu1.done));
}
