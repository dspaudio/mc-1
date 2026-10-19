/* SPDX-License-Identifier: GPL-3.0-only */
#include "../firmware/stage_probe.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t wire[32];
static unsigned sends,backpressure,locked;
static uint32_t mc_perf_ticks(void) { return 0; }
static void mc_perf_snapshot(McPerfSnapshot *s) { memset(s,0,sizeof *s); }
static uint32_t mc_perf_lock(void) { assert(!locked);locked=1;return 0; }
static void mc_perf_unlock(uint32_t saved) { (void)saved;assert(locked);locked=0; }
static int mc_perf_send(const uint8_t *p,uint32_t n) {
    assert(!locked&&n==32);memcpy(wire,p,32);sends++;return backpressure?-1:0;
}
int main(void) {
    McRenderContext c={0};
    uint8_t request[8]={123,0,0,0,5,0,0,0};
    mc_perf.session=123;mc_perf.state=MC_PERF_WARM;
    mc_stage_begin();mc_stage_end(500,300,&c);assert(!mc_stage.words[2]);
    assert(mc_stage_request(request,8));mc_stage_service();assert(wire[7]==1);
    mc_perf.state=MC_PERF_ACTIVE;mc_perf.count=0;mc_stage_begin();
    for(unsigned i=0;i<MC_TIME_COUNT;i++)c.time[i]=(i+1u)*10u;
    for(unsigned i=0;i<8192;i++) {
        mc_perf.count=i;mc_stage_end(500,300,&c);
    }
    assert(mc_stage.words[2]==8192&&mc_stage.words[4]==81920);
    assert(mc_stage.words[4+3*MC_TIME_COUNT]==8192u*200u);
    assert(mc_stage.words[6+3*(MC_TIME_COUNT+1)]==500);
    uint32_t assembled[MC_STAGE_WORDS];
    mc_perf.state=MC_PERF_FROZEN;
    for(unsigned page=0;page<MC_STAGE_PAGES;page++) {
        request[5]=(uint8_t)page;
        assert(mc_stage_request(request,8));
        backpressure=1;mc_stage_service();assert(mc_stage.pending);
        backpressure=0;mc_stage_service();assert(!mc_stage.pending&&!wire[7]);
        for(unsigned i=0;i<6;i++)assembled[page*6+i]=mc_perf_rd32(wire+8+4*i);
    }
    assert(!memcmp(assembled,mc_stage.words,sizeof assembled));
    /* Existing v1 results finish sending before the host requests stage pages. */
    mc_perf.state=MC_PERF_IDLE;
    assert(mc_stage_request(request,8));mc_stage_service();assert(!wire[7]);
    request[0]++;assert(mc_stage_request(request,8));mc_stage_service();assert(wire[7]==1);
    request[0]--;request[5]=MC_STAGE_PAGES;
    assert(mc_stage_request(request,8));mc_stage_service();assert(wire[7]==1);
    request[5]=0;mc_stage.words[2]=0; /* OTA invalidation. */
    assert(mc_stage_request(request,8));mc_stage_service();assert(wire[7]==1);
    assert(sends&& !mc_stage_request(request,7));
    puts("PASS stage probe v5: four bank timings, frozen window, 64-bit sums, seven pages, backpressure, stale/OTA rejection");
    return 0;
}
