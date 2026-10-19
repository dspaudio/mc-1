/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#ifndef MC_PARALLEL_H
#define MC_PARALLEL_H
#include "mcfm.h"
#if MC_CPU1_TRACE == 1
static void mc_dual_trace(unsigned stage);
#define MC_DUAL_TRACE(stage) mc_dual_trace(stage)
#else
#define MC_DUAL_TRACE(stage) ((void)0)
#endif

/* The payload and history live in internal SRAM, never SDRAM/XIP. */
#if MC_DUAL_WL82 == 1
typedef volatile uint32_t McAtomic;
static inline __attribute__((always_inline)) uint32_t mc_acquire(const McAtomic *p) {
    uint32_t v=*p;__asm__ volatile("csync" ::: "memory");return v;
}
static inline __attribute__((always_inline)) void mc_release(McAtomic *p,uint32_t v) {
    __asm__ volatile("csync" ::: "memory");*p=v;
    __asm__ volatile("csync" ::: "memory");
}
#else
#include <stdatomic.h>
typedef _Atomic uint32_t McAtomic;
static inline uint32_t mc_acquire(const McAtomic *p) { return atomic_load_explicit(p,memory_order_acquire); }
static inline void mc_release(McAtomic *p,uint32_t v) { atomic_store_explicit(p,v,memory_order_release); }
#endif

enum { MC_DUAL_OFF, MC_DUAL_READY, MC_DUAL_BOOT_FAILED, MC_DUAL_TIMEOUT, MC_DUAL_PROTOCOL };
enum { MC_TIME_SYNTH, MC_TIME_ROUTE, MC_TIME_RD, MC_TIME_CF,
    MC_TIME_WAIT, MC_TIME_TRANSFER, MC_TIME_OUTPUT, MC_TIME_RD2, MC_TIME_CF2, MC_TIME_COUNT };
typedef struct {
    int32_t bus[MC_FX_BANKS][16][2];
    uint32_t frames,bpm;
    uint8_t type[MC_FX_BANKS],sends,skip;
} McFxJob;
#ifndef MC_WORKER_VOICES
#define MC_WORKER_VOICES 3
#endif
enum { MC_LOCAL_VOICES=MC_VOICES-MC_WORKER_VOICES };
typedef struct {
    McVoice voice;
    McPcmCache pcm;
    McPitchCache pitch;
    McAnalogVoice analog;
    uint32_t ratio;
    uint8_t amount;
} McWorkerVoice;
typedef struct {
    McDrum voice[MC_DRUM_VOICES];
    uint32_t random,frames;
    uint8_t level[MC_DRUMS];
    int32_t sample[16];
    McWorkerVoice melodic[MC_WORKER_VOICES];
    int32_t part[3][16];
} McSynthJob;
/* One outstanding job; CPU1 never sees engine/project/CPU0 stack pointers.
 * Each job snapshots only the taps it will read. Completed slices commit dirty taps.
 * Failed payload/history remain quarantined until boot, even if halt fails. */
typedef struct McDual {
    McAtomic request,done;
    McSharedFx fx[MC_FX_BANKS/2];
    McFxJob job;
    McSynthJob drums;
    uint32_t fx_enabled,drums_enabled;
    uint32_t job_ticks,bank_ticks[MC_FX_BANKS],processed,result,measure;
    uint32_t sequence,previous_done,dispatch_tick,budget_ticks,spin_limit;
    uint32_t status,jobs,failures,wait_ticks;
    uint32_t (*ticks)(void);
    void (*notify)(struct McDual *);
    void (*halt)(struct McDual *);
} McDual;
typedef struct {
    McDual *dual;
    uint32_t (*ticks)(void);
    uint32_t time[MC_TIME_COUNT];
} McRenderContext;

void mc_fx_cf_state(McTrackFx *t,unsigned type,int32_t stereo[][2],unsigned frames);
void mc_fx_commit(McSharedFx *dst,const McSharedFx *src,unsigned frames,unsigned mask);
void mc_fx_prepare(McSharedFx *dst,const McSharedFx *src,unsigned bpm,
    const uint8_t type[2],unsigned frames,unsigned mask);
void mc_render_ex(McEngine *e,int16_t *stereo,unsigned frames,McRenderContext *context);
void mc_synth_job_run(McSynthJob *job);

static inline uint32_t mc_render_tick(const McRenderContext *c) { return c&&c->ticks?c->ticks():0; }
static inline void mc_render_time(McRenderContext *c,unsigned stage,uint32_t begin) {
    if(c&&c->ticks)c->time[stage]+=c->ticks()-begin;
}
static inline unsigned mc_fx_stage(unsigned bank) {
    return bank<2?MC_TIME_RD+bank:MC_TIME_RD2+bank-2u;
}
static inline unsigned mc_fx_job_run(McSharedFx *first,McSharedFx *second,McFxJob *job,
    uint32_t elapsed[MC_FX_BANKS],uint32_t (*ticks)(void)) {
    unsigned processed=0;
    for(unsigned bank=0;bank<MC_FX_BANKS;bank++) {
        MC_DUAL_TRACE(32u+bank);
        McSharedFx *fx=bank<2?first:second;
        unsigned active=bank&1u?fx->cf_active:fx->rd_active;
        unsigned skip=bank&1u?1u:2u;
        uint32_t begin=ticks?ticks():0;
        MC_DUAL_TRACE(48u+bank);
        if((active||(job->sends&(1u<<bank)))&&!(job->skip&skip)) {
            if(bank&1u)mc_fx_cf_block(fx,job->type[bank],job->bus[bank],job->frames);
            else mc_fx_rd_block(fx,job->bpm,job->type[bank],job->bus[bank],job->frames);
            processed|=1u<<bank;
        } else for(unsigned i=0;i<job->frames;i++)job->bus[bank][i][0]=job->bus[bank][i][1]=0;
        MC_DUAL_TRACE(64u+bank);
        elapsed[bank]=ticks?ticks()-begin:0;
        MC_DUAL_TRACE(80u+bank);
    }
    return processed;
}
static inline void mc_dual_fail(McDual *d,unsigned reason) {
    d->status=reason;d->failures++;
    d->halt(d);
}
static inline void mc_dual_begin(McDual *d,unsigned measure) {
    if(!d||d->status!=MC_DUAL_READY)return;
    if(mc_acquire(&d->request)!=mc_acquire(&d->done)) {
        mc_dual_fail(d,MC_DUAL_PROTOCOL);return;
    }
    d->measure=measure;
}
static inline int mc_dual_submit(McDual *d,const McFxJob *job,
    const McSharedFx *first,const McSharedFx *second,const McSynthJob *drums) {
    if(!d||d->status!=MC_DUAL_READY)return 0;
    if(!job&&!drums)return 0;
    unsigned invalid=0;
    if(job) {
        invalid=!job->frames||job->frames>16||job->sends>=16||job->skip>=4;
        for(unsigned bank=0;bank<MC_FX_BANKS;bank++)invalid|=job->type[bank]>=8;
    }
    if(drums)invalid|=!drums->frames||drums->frames>16;
    if(invalid||mc_acquire(&d->done)!=d->sequence||mc_acquire(&d->request)!=d->sequence) {
        mc_dual_fail(d,MC_DUAL_PROTOCOL);return 0;
    }
    if(job) {
        unsigned active=job->sends|first->rd_active|((unsigned)first->cf_active<<1)|
            ((unsigned)second->rd_active<<2)|((unsigned)second->cf_active<<3);
        unsigned skipped=(job->skip&1u?10u:0u)|(job->skip&2u?5u:0u);
        active&=~skipped;
        mc_fx_prepare(&d->fx[0],first,job->bpm,job->type,job->frames,active&3u);
        mc_fx_prepare(&d->fx[1],second,job->bpm,job->type+2,job->frames,(active>>2)&3u);
        d->job=*job;
    }
    if(drums)d->drums=*drums;
    d->fx_enabled=job!=0;d->drums_enabled=drums!=0;
    d->previous_done=d->sequence;
    if(!++d->sequence)++d->sequence;
    d->dispatch_tick=d->ticks();d->wait_ticks=0;
    mc_release(&d->request,d->sequence);
    if(d->notify)d->notify(d);
    return 1;
}
/* CPU1 returns to the RAM idle loop before that loop release-publishes done. */
static inline void mc_dual_work(McDual *d,uint32_t job) {
    MC_DUAL_TRACE(16);
    uint32_t next=mc_acquire(&d->done)+1u;if(!next)next=1;
    d->result=MC_DUAL_PROTOCOL;
    if(job!=next||(!d->fx_enabled&&!d->drums_enabled))return;
    if(d->fx_enabled) {
        if(!d->job.frames||d->job.frames>16||d->job.sends>=16||d->job.skip>=4)return;
        for(unsigned bank=0;bank<MC_FX_BANKS;bank++)if(d->job.type[bank]>=8)return;
    }
    if(d->drums_enabled&&(!d->drums.frames||d->drums.frames>16))return;
    MC_DUAL_TRACE(17);
    uint32_t begin=d->ticks();
    MC_DUAL_TRACE(18);
    d->processed=0;
    for(unsigned bank=0;bank<MC_FX_BANKS;bank++)d->bank_ticks[bank]=0;
    if(d->fx_enabled)
        d->processed=mc_fx_job_run(&d->fx[0],&d->fx[1],&d->job,d->bank_ticks,d->measure?d->ticks:0);
    if(d->drums_enabled)mc_synth_job_run(&d->drums);
    d->job_ticks=d->ticks()-begin;d->result=MC_DUAL_READY;
    MC_DUAL_TRACE(97);
}
static inline int mc_dual_join(McDual *d,McSharedFx *first,McSharedFx *second,
    McFxJob *job,const McSynthJob **drums) {
    uint32_t begin=d->ticks();
    for(uint32_t spins=0;;spins++) {
        uint32_t done=mc_acquire(&d->done);
        if(done==d->sequence)break;
        if(done!=d->previous_done) {
            d->wait_ticks=d->ticks()-begin;mc_dual_fail(d,MC_DUAL_PROTOCOL);return 0;
        }
        if(d->ticks()-d->dispatch_tick>=d->budget_ticks||spins>=d->spin_limit) {
            d->wait_ticks=d->ticks()-begin;mc_dual_fail(d,MC_DUAL_TIMEOUT);return 0;
        }
    }
    d->wait_ticks=d->ticks()-begin;
    if(d->result!=MC_DUAL_READY) { mc_dual_fail(d,MC_DUAL_PROTOCOL);return 0; }
    if(d->fx_enabled&&job) {
        mc_fx_commit(first,&d->fx[0],job->frames,d->processed&3u);
        mc_fx_commit(second,&d->fx[1],job->frames,(d->processed>>2)&3u);
        for(unsigned bank=0;bank<MC_FX_BANKS;bank++)
            for(unsigned i=0;i<job->frames;i++)for(unsigned c=0;c<2;c++)job->bus[bank][i][c]=d->job.bus[bank][i][c];
    }
    /* The completed slot stays immutable until the next submission.
     * Failed/quarantined work never exposes a result view. */
    if(d->drums_enabled&&drums)*drums=&d->drums;
    d->jobs++;
    return 1;
}
#endif
