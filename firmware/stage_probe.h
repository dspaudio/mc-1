/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#pragma once
#include "../src/parallel.h"
#include "perf_probe.h"
/* Shares the existing 1024 warm-up / 8192 measured block window. Version 5
 * adds R/D2 and C/F2 times: seven pages of six little-endian words. */
enum { MC_STAGE_COUNT=MC_TIME_COUNT+2,MC_STAGE_STATUS=4+MC_STAGE_COUNT*3,
    MC_STAGE_WORDS=MC_STAGE_STATUS+5,MC_STAGE_PAGES=MC_STAGE_WORDS/6 };
_Static_assert(MC_STAGE_WORDS==42,"Stage schema 5 contains 42 words");
static struct {
    uint64_t sum[MC_STAGE_COUNT];
    uint32_t peak[MC_STAGE_COUNT],words[MC_STAGE_WORDS],read_cost;
    uint8_t pending,body[32];
} mc_stage;
static void mc_stage_begin(void) {
    if(mc_perf.state==MC_PERF_ACTIVE&&!mc_perf.count) {
        for(unsigned i=0;i<MC_STAGE_COUNT;i++)mc_stage.sum[i]=mc_stage.peak[i]=0;
        for(unsigned i=0;i<MC_STAGE_WORDS;i++)mc_stage.words[i]=0;
    }
}
static void mc_stage_end(uint32_t audio,uint32_t render,const McRenderContext *context) {
    if(mc_perf.state!=MC_PERF_ACTIVE)return;
    for(unsigned i=0;i<MC_STAGE_COUNT;i++) {
        uint32_t value=i<MC_TIME_COUNT?context->time[i]:i==MC_TIME_COUNT?audio-render:audio;
        mc_stage.sum[i]+=value;
        if(value>mc_stage.peak[i])mc_stage.peak[i]=value;
    }
    if(mc_perf.count!=8191u)return;
    uint32_t *w=mc_stage.words;
    w[0]=5;w[1]=mc_perf.session;w[2]=8192;w[3]=mc_stage.read_cost;
    for(unsigned i=0;i<MC_STAGE_COUNT;i++) {
        w[4+3*i]=(uint32_t)mc_stage.sum[i];
        w[5+3*i]=(uint32_t)(mc_stage.sum[i]>>32);
        w[6+3*i]=mc_stage.peak[i];
    }
#if MC_DUAL_CORE == 1
    w[MC_STAGE_STATUS]=mc_cpu1.status;w[MC_STAGE_STATUS+1]=mc_cpu1.jobs;
    w[MC_STAGE_STATUS+2]=mc_cpu1.failures;
    unsigned quiet=mc_cpu1_held||(mc_cpu1.status==MC_DUAL_READY&&
        mc_acquire(&mc_cpu1.request)==mc_acquire(&mc_cpu1.done));
    w[MC_STAGE_STATUS+3]=quiet?mc_cpu1_stack_free():0;
    w[MC_STAGE_STATUS+4]=quiet?(uint32_t)mc_cpu1_guards():0;
#endif
}
static int mc_stage_request(const uint8_t *p,uint32_t n) {
    if(n!=8u||p[4]!=5u)return 0;
    if(mc_stage.pending)return 1;
    uint32_t session=mc_perf_rd32(p),saved=mc_perf_lock();
    unsigned valid=(mc_perf.state==MC_PERF_FROZEN||mc_perf.state==MC_PERF_IDLE)&&
        session==mc_perf.session&&session==mc_stage.words[1]&&mc_stage.words[2]==8192u&&
        session&&p[5]<MC_STAGE_PAGES&&!p[6]&&!p[7];
    mc_perf_wr32(mc_stage.body,session);
    mc_stage.body[4]=p[5];mc_stage.body[5]=MC_STAGE_PAGES;mc_stage.body[6]=5;
    mc_stage.body[7]=(uint8_t)(valid?0:1);
    for(unsigned i=0;i<6;i++)
        mc_perf_wr32(mc_stage.body+8+4*i,valid?mc_stage.words[p[5]*6u+i]:0);
    mc_stage.pending=1;mc_perf_unlock(saved);return 1;
}
static void mc_stage_service(void) {
    if(mc_stage.pending&&!mc_perf_send(mc_stage.body,32))mc_stage.pending=0;
}
