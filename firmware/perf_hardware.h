/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

static uint8_t mc_hw_snapshot(uint32_t values[6]);
static uint8_t mc_hw_worker_snapshot(uint32_t values[6]);
static uint8_t mc_hw_panel_snapshot(uint32_t values[6]);
#if MC_VOICE_PROBE == 1
static uint8_t mc_hw_voice_snapshot(uint32_t values[6]);
#endif
static uint8_t mc_hw_control(unsigned action);
static uint8_t mc_hw_control_done(void);
static int mc_hw_send(const uint8_t *body,uint32_t n);
static struct { uint8_t pending,body[32]; } mc_hw;

/* 기존 0x70 캡처와 구분되는 version 2의 고정 레지스터 읽기 요청이다. */
static int mc_hw_request(const uint8_t *p,uint32_t n)
{
    if(n!=8u||p[6]||p[7]||!(p[0]|p[1]|p[2]|p[3]))return 0;
    if(!(((p[4]==2u||p[4]==6u||p[4]==7u
#if MC_VOICE_PROBE == 1
        ||p[4]==8u
#endif
        )&&!p[5])||(p[4]==3u&&p[5]<17u)))return 0;
    if(mc_hw.pending)return 1;
    uint32_t values[6]={0};
    uint8_t status=p[4]==2u?mc_hw_snapshot(values):
        p[4]==6u?mc_hw_worker_snapshot(values):
        p[4]==7u?mc_hw_panel_snapshot(values):
#if MC_VOICE_PROBE == 1
        p[4]==8u?mc_hw_voice_snapshot(values):
#endif
        mc_hw_control(p[5]);
    for(unsigned i=0;i<4;i++)mc_hw.body[i]=p[i];
    mc_hw.body[4]=0;mc_hw.body[5]=1;mc_hw.body[6]=p[4];mc_hw.body[7]=status;
    for(unsigned i=0;i<6;i++)for(unsigned b=0;b<4;b++)
        mc_hw.body[8u+4u*i+b]=(uint8_t)(values[i]>>(8u*b));
    mc_hw.pending=(uint8_t)(p[4]==3u&&status==2u?2:1);
    return 1;
}
static void mc_hw_service(void)
{
    if(mc_hw.pending==2) {
        uint8_t status=mc_hw_control_done();
        if(status==2)return;
        mc_hw.body[7]=status;mc_hw.pending=1;
    }
    if(mc_hw.pending&&!mc_hw_send(mc_hw.body,sizeof mc_hw.body))mc_hw.pending=0;
}
static void mc_hw_invalidate(void) { mc_hw.pending=0; }
