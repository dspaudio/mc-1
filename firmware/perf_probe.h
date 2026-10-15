/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

/* 하드웨어 접근은 앱의 콜백에만 둔다. 호스트도 이 상태 머신을 실행한다. */
typedef struct {
    uint32_t deadline, half, underruns, overruns, stalls, missed;
    uint32_t timer_con, timer_prd, clk1, clk2, alnk_len;
    uint32_t bpm, playing, clock_source, master, step, flags;
} McPerfSnapshot;
static uint32_t mc_perf_ticks(void);
static void mc_perf_snapshot(McPerfSnapshot *s);
static int mc_perf_send(const uint8_t *body, uint32_t n);
static uint32_t mc_perf_lock(void);
static void mc_perf_unlock(uint32_t saved);
#ifndef MC_PERF_PUBLISH
#define MC_PERF_PUBLISH() ((void)0)
#endif
enum { MC_PERF_IDLE, MC_PERF_WARM, MC_PERF_ACTIVE, MC_PERF_FINALIZING, MC_PERF_FROZEN };
static struct {
    volatile uint32_t state;
    uint32_t session, warm, count, first, previous, chunk;
    uint32_t words[42];
    uint64_t render_sum, audio_sum, timer_sum;
    McPerfSnapshot start;
    uint32_t panel_last[2], panel_count[2], panel_gap[2];
    uint8_t error_pending, error_status;
    uint32_t error_session;
} mc_perf;

static uint32_t mc_perf_rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static void mc_perf_wr32(uint8_t *p,uint32_t v)
{
    for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8u*i));
}
static void mc_perf_max(uint32_t *dst,uint32_t value)
{
    if(value>*dst)*dst=value;
}
/* foreground만 시작/송신한다. 리셋은 짧은 IRQ-off 구간 안에서 수행한다. */
static void mc_perf_request(const uint8_t *p,uint32_t n)
{
    uint32_t session=n>=4u?mc_perf_rd32(p):0;
    unsigned invalid=n!=8u || !session;
    if(n==8u)invalid|=p[4]!=1u || p[5] || p[6] || p[7];
    uint32_t saved=mc_perf_lock();
    if(invalid || mc_perf.state!=MC_PERF_IDLE || mc_perf.error_pending) {
        if(!mc_perf.error_pending) {
            mc_perf.error_session=session;
            mc_perf.error_status=(uint8_t)(invalid?2:1);
            mc_perf.error_pending=1;
        }
    } else {
        mc_perf.session=session;mc_perf.warm=0;mc_perf.count=0;mc_perf.chunk=0;
        mc_perf.render_sum=mc_perf.audio_sum=mc_perf.timer_sum=0;
        for(unsigned i=0;i<2;i++)mc_perf.panel_count[i]=mc_perf.panel_gap[i]=0;
        for(unsigned i=0;i<42;i++)mc_perf.words[i]=0;
        MC_PERF_PUBLISH();mc_perf.state=MC_PERF_WARM;
    }
    mc_perf_unlock(saved);
}
/* OTA 진입 시 성공 결과뿐 아니라 이전 오류 응답도 폐기한다. */
static void mc_perf_invalidate(void)
{
    uint32_t saved=mc_perf_lock();
    mc_perf.state=MC_PERF_IDLE;mc_perf.error_pending=0;
    MC_PERF_PUBLISH();mc_perf_unlock(saved);
}
static void mc_perf_audio_begin(uint32_t now)
{
    if(mc_perf.state==MC_PERF_WARM) {
        if(mc_perf.warm<1024u) { mc_perf.warm++;return; }
        uint32_t saved=mc_perf_lock();mc_perf_snapshot(&mc_perf.start);
        mc_perf.first=mc_perf.previous=now;
        mc_perf.panel_last[0]=mc_perf.panel_last[1]=now;
        mc_perf.state=MC_PERF_ACTIVE;mc_perf_unlock(saved);
    }
    if(mc_perf.state==MC_PERF_ACTIVE) {
        if(mc_perf.count)mc_perf_max(&mc_perf.words[11],now-mc_perf.previous);
        mc_perf.previous=now;
    }
}
static void mc_perf_render(uint32_t ticks)
{
    if(mc_perf.state!=MC_PERF_ACTIVE)return;
    mc_perf.render_sum+=ticks;mc_perf_max(&mc_perf.words[7],ticks);
}
static void mc_perf_audio_end(uint32_t begin)
{
    if(mc_perf.state!=MC_PERF_ACTIVE)return;
    uint32_t now=mc_perf_ticks();
    mc_perf.audio_sum+=now-begin;
    mc_perf_max(&mc_perf.words[10],now-begin);
    if(++mc_perf.count!=8192u)return;
    McPerfSnapshot end;
    /* IRQ-off에서 TIMER5 갱신을 닫고 USB 상태를 고정한다. */
    uint32_t saved=mc_perf_lock();mc_perf.state=MC_PERF_FINALIZING;
    mc_perf_snapshot(&end);mc_perf_unlock(saved);
    /* foreground는 오디오 ISR을 선점하지 않는다. TIMER5는 FINALIZING을
     * 갱신하지 않으므로, IRQ 상태를 복원한 뒤에도 결과는 이 ISR만 쓴다. */
    uint32_t *w=mc_perf.words;
    const McPerfSnapshot *s=&mc_perf.start;
    w[0]=1;w[1]=mc_perf.session;w[2]=8192;w[3]=8192u*128u;
    w[4]=now-mc_perf.first;
    w[5]=(uint32_t)mc_perf.render_sum;w[6]=(uint32_t)(mc_perf.render_sum>>32);
    w[8]=(uint32_t)mc_perf.audio_sum;w[9]=(uint32_t)(mc_perf.audio_sum>>32);
    w[12]=end.deadline-s->deadline;w[13]=end.half-s->half;
    w[14]=(uint32_t)mc_perf.timer_sum;w[15]=(uint32_t)(mc_perf.timer_sum>>32);
    w[17]=mc_perf.panel_count[0];w[18]=mc_perf.panel_gap[0];
    w[19]=mc_perf.panel_count[1];w[20]=mc_perf.panel_gap[1];
    w[21]=end.underruns-s->underruns;w[22]=end.overruns-s->overruns;
    w[23]=end.stalls-s->stalls;
    w[24]=s->timer_con;w[25]=s->timer_prd;w[26]=s->clk1;w[27]=s->clk2;w[28]=s->alnk_len;
    w[29]=s->bpm;w[30]=end.bpm;w[31]=s->playing;w[32]=end.playing;
    w[33]=s->clock_source;w[34]=end.clock_source;w[35]=s->master;w[36]=end.master;
    w[37]=s->step;w[38]=end.step;w[39]=s->flags;w[40]=end.flags;
    w[41]=end.missed-s->missed;
    MC_PERF_PUBLISH();mc_perf.state=MC_PERF_FROZEN;
}
/* TIMER5는 오디오 IRQ보다 우선순위가 높다. 진입/종료 모두 활성인 경우만 센다. */
static void mc_perf_timer_end(uint32_t begin,unsigned active)
{
    if(active && mc_perf.state==MC_PERF_ACTIVE) {
        mc_perf.timer_sum+=mc_perf_ticks()-begin;mc_perf.words[16]++;
    }
}
/* foreground는 이벤트만 게시한다. 결과 words는 오디오 ISR만 갱신한다. */
static void mc_perf_panel(unsigned led)
{
    uint32_t saved=mc_perf_lock();
    if(mc_perf.state==MC_PERF_ACTIVE) {
        uint32_t now=mc_perf_ticks();
        mc_perf.panel_count[led]++;
        mc_perf_max(&mc_perf.panel_gap[led],now-mc_perf.panel_last[led]);
        mc_perf.panel_last[led]=now;
    }
    mc_perf_unlock(saved);
}
static void mc_perf_service(void)
{
    uint8_t body[32]={0};
    uint32_t state=mc_perf.state;
    if(state!=MC_PERF_IDLE && state!=MC_PERF_FROZEN)return;
    MC_PERF_PUBLISH();
    if(mc_perf.error_pending) {
        mc_perf_wr32(body,mc_perf.error_session);body[6]=1;body[7]=mc_perf.error_status;
        if(!mc_perf_send(body,sizeof body))mc_perf.error_pending=0;
    } else if(state==MC_PERF_FROZEN) {
        mc_perf_wr32(body,mc_perf.session);body[4]=(uint8_t)mc_perf.chunk;body[5]=7;body[6]=1;
        for(unsigned i=0;i<6;i++)mc_perf_wr32(body+8+4u*i,mc_perf.words[mc_perf.chunk*6u+i]);
        if(!mc_perf_send(body,sizeof body) && ++mc_perf.chunk==7u)mc_perf.state=MC_PERF_IDLE;
    }
}
