/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MC_PERF_PROBE 1
#define OTA_IDENTITY "probe-test"
static void test_publish(void);
#define MC_PERF_PUBLISH() test_publish()
#include "../firmware/perf_probe.h"
#include "../firmware/update/ota.c"

static uint32_t ticks, sent, attempts, snapshots;
static unsigned locked, fail_send, frame_pending;
static McPerfSnapshot registers;
static uint8_t received[OTA_FRAME_MAX], response[40], last_wire[50];
static uint32_t received_n, last_wire_n;
/* bit0은 모의 IRQ 플래그다. 실제 ICFG 비트 배치를 가정하지 않는다. */
static uint32_t mock_icfg, finalize_events, final_publish_checks;
static const uint32_t *expected_words;
static void finalizing_interleave(void)
{
    uint64_t sum=mc_perf.timer_sum;
    uint32_t count=mc_perf.words[16], before=attempts;
    assert(mc_perf.state==MC_PERF_FINALIZING);
    /* 캡처 중 진입했던 TIMER5까지 가정해도 종료된 누계를 바꾸지 못한다. */
    mc_perf_timer_end(ticks-7u,1);mc_perf_service();
    assert(mc_perf.timer_sum==sum && mc_perf.words[16]==count && attempts==before);
    finalize_events++;
}
static void test_publish(void)
{
    if(mc_perf.state==MC_PERF_FINALIZING) {
        if(expected_words)assert(!memcmp(mc_perf.words,expected_words,sizeof mc_perf.words));
        finalizing_interleave();final_publish_checks++;
    }
}
static uint32_t mc_perf_ticks(void) { return ticks; }
static uint32_t mc_perf_lock(void)
{
    assert(!locked);
    uint32_t saved=mock_icfg;
    mock_icfg&=~1u;locked=1;return saved;
}
static void mc_perf_unlock(uint32_t saved)
{
    assert(locked && !(mock_icfg&1u));
    mock_icfg=saved;locked=0;
    if(mc_perf.state==MC_PERF_FINALIZING)finalizing_interleave();
}
static void mc_perf_snapshot(McPerfSnapshot *s)
{
    assert(locked && !(mock_icfg&1u));*s=registers;snapshots++;
    if(mc_perf.state==MC_PERF_FINALIZING)finalizing_interleave();
}
static int mc_perf_send(const uint8_t *p,uint32_t n) { return ota_send_msg(0x71,p,n); }
static void ota_probe_request(const uint8_t *p,uint32_t n) { mc_perf_request(p,n); }
static int ota_wire_send(const uint8_t *p,uint32_t n)
{
    assert(!locked && n<=sizeof last_wire);
    attempts++;last_wire_n=n;memcpy(last_wire,p,n);
    if(fail_send)return -1;
    assert(p[0]==0xf0 && p[n-1]==0xf7);
    uint32_t d=ota_unpack7(p+1,n-2,response,sizeof response);
    assert(d==39 && response[0]==0 && response[1]==0x59 && response[2]==0x71);
    assert(ota_rd24(response+3)==32);
    if(!response[13]) {
        assert(mc_perf.state==MC_PERF_FROZEN);
        if(expected_words)assert(!memcmp(mc_perf.words,expected_words,sizeof mc_perf.words));
    }
    uint8_t sum=0;
    for(unsigned i=6;i<39;i++)sum=(uint8_t)(sum+response[i]);
    assert(sum==255);sent++;return 0;
}
static int ota_frame_get(const uint8_t **p,uint32_t *n)
{
    if(!frame_pending)return 0;
    *p=received;*n=received_n;return 1;
}
static void ota_frame_done(void) { frame_pending=0; }
static uint32_t ota_now_ms(void) { return 0; }
static void ota_idle(void) {}
static int ota_erase(uint32_t off) { (void)off;assert(0);return -1; }
static int ota_prog(uint32_t off,const void *p,uint32_t n) { (void)off;(void)p;(void)n;assert(0);return -1; }
static int ota_fread(uint32_t off,void *p,uint32_t n) { (void)off;(void)p;(void)n;assert(0);return -1; }
static void ota_show(uint32_t step,int32_t code) { (void)step;(void)code;assert(0); }
static void ota_commit(const uint8_t *p) { (void)p;assert(0); }

static void reset(void)
{
    memset(&mc_perf,0,sizeof mc_perf);memset(&registers,0,sizeof registers);
    ticks=sent=attempts=snapshots=0;locked=fail_send=frame_pending=0;
    mock_icfg=0xA5A50001u;finalize_events=final_publish_checks=0;expected_words=0;
}
/* 실제 정상 OTA 디코더/체크섬/요청 훅을 통해 요청한다. */
static void request(uint32_t session,unsigned schema,unsigned reserved,uint32_t n)
{
    uint8_t raw[24]={0,0x59,0x70};
    raw[3]=(uint8_t)n;ota_wr32(raw+6,session);raw[10]=(uint8_t)schema;
    raw[11]=(uint8_t)reserved;raw[12]=(uint8_t)(reserved>>8);raw[13]=(uint8_t)(reserved>>16);
    uint32_t sum=0;
    for(unsigned i=6;i<6+n;i++)sum+=raw[i];
    raw[6+n]=(uint8_t)~sum;
    received_n=ota_pack7(raw,7+n,received);frame_pending=1;
    ota_service();assert(!frame_pending);
}
static void block(uint32_t start,uint32_t render,uint32_t audio)
{
    ticks=start;mc_perf_audio_begin(start);
    mc_perf_render(render);ticks=start+audio;mc_perf_audio_end(start);
}
static void warm(void)
{
    uint32_t before=snapshots;
    for(unsigned i=0;i<1024;i++)block(i*100u,99,99);
    assert(mc_perf.state==MC_PERF_WARM && !mc_perf.count && snapshots==before);
}
static void complete(void)
{
    uint32_t before=snapshots;
    warm();
    for(unsigned i=0;i<8192;i++)block(200000u+i*100u,20,50);
    assert(mc_perf.state==MC_PERF_FROZEN && snapshots==before+2u);
}
static void error(unsigned status,uint32_t session)
{
    mc_perf_service();
    assert(response[10]==0 && response[11]==0 && response[12]==1 && response[13]==status);
    assert(ota_rd32(response+6)==session);
    for(unsigned i=14;i<38;i++)assert(response[i]==0);
}
static void test_boundaries_and_frozen_words(void)
{
    reset();request(0x12345678,1,0,8);warm();
    registers=(McPerfSnapshot){.deadline=UINT32_MAX-1u,.half=UINT32_MAX,
        .underruns=UINT32_MAX,.overruns=8,.stalls=9,.missed=10,
        .timer_con=9,.timer_prd=UINT32_MAX,.clk1=11,.clk2=12,.alnk_len=256,
        .bpm=118,.playing=1,.clock_source=2,.master=999,.step=3,.flags=7};
    uint32_t first=UINT32_MAX-40u;
    for(unsigned i=0;i<8192;i++) {
        uint32_t begin=first+i*1000u;
        ticks=begin;mc_perf_audio_begin(begin);
        if(i==0) {
            mc_perf_service();assert(!attempts);
            ticks=begin+10;mc_perf_panel(0);mc_perf_panel(1);
            ticks=begin+20;mc_perf_timer_end(begin+10,1);
            ticks=begin+30;mc_perf_panel(0);mc_perf_panel(1);
            registers.deadline=3;registers.half=2;
            registers.underruns=4;registers.overruns=10;registers.stalls=12;registers.missed=15;
            registers.bpm=140;registers.playing=0;registers.clock_source=3;
            registers.master=123;registers.step=15;registers.flags=1;
        }
        mc_perf_render(i==1?UINT32_MAX:100u);
        ticks=begin+(i==1?UINT32_MAX:200u);mc_perf_audio_end(begin);
        if(i==8190)assert(mc_perf.state==MC_PERF_ACTIVE);
    }
    const uint32_t expected[42]={1,0x12345678,8192,1048576,8191200,
        819099,1,UINT32_MAX,1638199,1,UINT32_MAX,1000,5,3,
        10,0,1,2,20,2,20,5,2,3,9,UINT32_MAX,11,12,256,
        118,140,1,0,2,3,999,123,3,15,7,1,5};
    assert(!memcmp(mc_perf.words,expected,sizeof expected));
    /* 완료 후 모든 훅과 외부 상태 변경이 고정 결과를 바꾸지 않는다. */
    memset(&registers,0xa5,sizeof registers);
    block(0,UINT32_MAX,UINT32_MAX);mc_perf_timer_end(0,1);mc_perf_panel(0);mc_perf_panel(1);
    assert(!memcmp(mc_perf.words,expected,sizeof expected));
    uint32_t assembled[42];
    for(unsigned chunk=0;chunk<7;chunk++) {
        mc_perf_service();
        assert(ota_rd32(response+6)==0x12345678 && response[10]==chunk);
        assert(response[11]==7 && response[12]==1 && !response[13]);
        for(unsigned i=0;i<6;i++)assembled[chunk*6+i]=ota_rd32(response+14+i*4u);
    }
    assert(!memcmp(assembled,expected,sizeof expected));
    assert(sent==7 && mc_perf.state==MC_PERF_IDLE);
    request(77,1,0,8);complete();
    assert(mc_perf.words[1]==77 && mc_perf.words[5]==8192u*20u && mc_perf.words[6]==0);
}
static void test_busy_invalid_and_backpressure(void)
{
    reset();request(0,1,0,8);error(2,0);
    request(1,2,0,8);error(2,1);
    request(2,1,1,8);error(2,2);
    request(2,1,256,8);error(2,2);
    request(2,1,65536,8);error(2,2);
    request(3,1,0,7);error(2,3);
    request(3,1,0,9);error(2,3);
    request(4,1,0,3);error(2,0);
    request(4,1,0,0);
    fail_send=1;uint32_t prior=sent;mc_perf_service();assert(sent==prior && mc_perf.error_pending);
    fail_send=0;error(2,0);
    request(5,1,0,8);request(5,1,0,8);
    assert(mc_perf.session==5 && mc_perf.error_pending);
    unsigned before=sent;mc_perf_service();assert(sent==before);
    complete();error(1,5);
    request(6,1,0,8);error(1,6);
    fail_send=1;mc_perf_service();
    uint8_t saved[50];uint32_t n=last_wire_n;memcpy(saved,last_wire,n);
    assert(!mc_perf.chunk && mc_perf.state==MC_PERF_FROZEN);
    mc_perf_service();assert(last_wire_n==n && !memcmp(saved,last_wire,n));
    fail_send=0;mc_perf_service();
    assert(!memcmp(saved,last_wire,n) && mc_perf.chunk==1);
    for(unsigned i=1;i<6;i++)mc_perf_service();
    request(7,1,0,8);error(1,7);
    assert(mc_perf.state==MC_PERF_FROZEN && mc_perf.chunk==6);
    fail_send=1;mc_perf_service();assert(mc_perf.state==MC_PERF_FROZEN && mc_perf.chunk==6);
    fail_send=0;mc_perf_service();assert(mc_perf.state==MC_PERF_IDLE);
    request(8,1,0,8);assert(mc_perf.state==MC_PERF_WARM && mc_perf.session==8);
}
static void test_timer_window_and_carry(void)
{
    reset();request(21,1,0,8);
    ticks=100;mc_perf_timer_end(0,1);warm();
    ticks=UINT32_MAX-8u;mc_perf_audio_begin(ticks);
    uint32_t begin=ticks;
    ticks=UINT32_MAX;mc_perf_timer_end(0,1);
    ticks=12;mc_perf_timer_end(UINT32_MAX-6u,1);
    mc_perf_timer_end(0,0);
    mc_perf_render(10);ticks=begin+50u;mc_perf_audio_end(begin);
    request(22,1,0,8);mc_perf_service();assert(!sent && mc_perf.session==21);
    for(unsigned i=1;i<8192;i++)block(begin+i*100u,10,50);
    assert(mc_perf.words[14]==18 && mc_perf.words[15]==1 && mc_perf.words[16]==2);
    error(1,22);
}
static void test_ota_invalidation(void)
{
    for(unsigned phase=0;phase<3;phase++) {
        reset();request(12,1,0,8);
        if(phase) { warm();block(200000,10,20); }
        if(phase==2)for(unsigned i=1;i<8192;i++)block(200000u+i*100u,10,20);
        request(13,1,0,8);mc_perf_invalidate();
        block(1,10,20);mc_perf_service();
        assert(mc_perf.state==MC_PERF_IDLE && !sent && !mc_perf.error_pending);
        request(14,1,0,8);assert(mc_perf.state==MC_PERF_WARM);
    }
}
static void test_irq_preservation_and_final_publish(void)
{
    static const uint32_t configs[]={0xA5A51200u,0x5A5A1301u};
    for(unsigned c=0;c<2;c++) {
        reset();mock_icfg=configs[c];
        uint32_t foreground=mock_icfg, audio=foreground&~1u;
        request(90u+c,1,0,8);assert(mock_icfg==foreground);
        warm();assert(mock_icfg==foreground);
        const uint32_t expected[42]={[0]=1,[1]=90u+c,[2]=8192,[3]=1048576,[4]=819150,
            [5]=163840,[7]=20,[8]=409600,[10]=50,[11]=100,[14]=10,[16]=1,
            [17]=1,[18]=50,[19]=1,[20]=50};
        expected_words=expected;
        for(unsigned i=0;i<8192;i++) {
            uint32_t begin=200000u+i*100u;
            /* 모의 오디오 ISR 진입 때 이미 IRQ가 꺼진 상태를 보존해야 한다. */
            mock_icfg=audio;ticks=begin;mc_perf_audio_begin(begin);
            assert(mock_icfg==audio && !locked);
            if(!i) { ticks=begin+10u;mc_perf_timer_end(begin,1); }
            mc_perf_render(20);ticks=begin+50u;mc_perf_audio_end(begin);
            assert(mock_icfg==audio && !locked);
            mock_icfg=foreground;
            if(!i) {
                mc_perf_panel(0);mc_perf_panel(1);
                assert(mock_icfg==foreground && !locked);
            }
        }
        assert(finalize_events==3 && final_publish_checks==1);
        assert(mc_perf.state==MC_PERF_FROZEN && !attempts);
        for(unsigned i=0;i<7;i++) { mc_perf_service();assert(mock_icfg==foreground); }
        assert(sent==7 && mc_perf.state==MC_PERF_IDLE);
        mc_perf_invalidate();assert(mock_icfg==foreground && !locked);
        expected_words=0;
    }
}
int main(void)
{
    test_boundaries_and_frozen_words();
    test_busy_invalid_and_backpressure();
    test_timer_window_and_carry();
    test_ota_invalidation();
    test_irq_preservation_and_final_publish();
    puts("perf probe: deterministic capture, wire, backpressure and invalidation passed");
    return 0;
}
