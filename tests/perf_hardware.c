/* SPDX-License-Identifier: GPL-3.0-only */
#include "../firmware/perf_hardware.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned reads,sends,backpressure;
static uint8_t status,wire[32];
static unsigned controls;
static uint8_t control_status,control_done;
static const uint32_t values[6]={0x1c1,0x0750310c,8,0x12345678,0x83,0xfb};
static uint8_t mc_hw_snapshot(uint32_t out[6])
{
    reads++;
    for(unsigned i=0;i<6;i++)out[i]=values[i];
    return status;
}
static uint8_t mc_hw_control(unsigned action) { assert(action<8);controls++;return control_status; }
static uint8_t mc_hw_control_done(void) { return control_done; }
static int mc_hw_send(const uint8_t *p,uint32_t n)
{
    assert(n==32);memcpy(wire,p,n);sends++;
    return backpressure?-1:0;
}
int main(void)
{
    uint8_t request[9]={0x81,0x23,0x45,0xff,2,0,0,0,0};
    for(unsigned n=0;n<10;n++)if(n!=8)assert(!mc_hw_request(request,n));
    uint8_t invalid[9];
    static const uint8_t versions[4]={0,1,4,255};
    for(unsigned i=0;i<4;i++) {
        memcpy(invalid,request,sizeof invalid);invalid[4]=versions[i];
        assert(!mc_hw_request(invalid,8));
    }
    for(unsigned i=5;i<8;i++) {
        memcpy(invalid,request,sizeof invalid);invalid[i]=1;
        assert(!mc_hw_request(invalid,8));
    }
    memcpy(invalid,request,sizeof invalid);memset(invalid,0,4);
    assert(!mc_hw_request(invalid,8));assert(!reads&&!sends);
    assert(mc_hw_request(request,8)&&reads==1&&mc_hw.pending);
    request[0]=17;
    assert(mc_hw_request(request,8)&&reads==1); /* 대기 응답은 덮어쓰지 않는다. */
    backpressure=1;mc_hw_service();assert(mc_hw.pending&&sends==1);
    uint8_t first[32];memcpy(first,wire,sizeof first);
    backpressure=0;mc_hw_service();assert(!mc_hw.pending&&sends==2);
    assert(!memcmp(first,wire,sizeof first));
    assert(wire[0]==0x81&&wire[1]==0x23&&wire[2]==0x45&&wire[3]==0xff);
    assert(wire[4]==0&&wire[5]==1&&wire[6]==2&&wire[7]==0);
    for(unsigned i=0;i<6;i++)for(unsigned b=0;b<4;b++)
        assert(wire[8u+4u*i+b]==(uint8_t)(values[i]>>(8u*b)));
    for(status=1;status<=2;status++) {
        assert(mc_hw_request(request,8));mc_hw_service();assert(wire[7]==status);
    }
    assert(mc_hw_request(request,8));mc_hw_invalidate();
    unsigned sent=sends;mc_hw_service();assert(sends==sent&&!mc_hw.pending);
    request[4]=3;
    for(unsigned action=4;action<8;action++) {
        request[5]=(uint8_t)action;control_status=0;
        assert(mc_hw_request(request,8));mc_hw_service();assert(wire[6]==3&&wire[7]==0);
    }
    request[5]=8;assert(!mc_hw_request(request,8));
    sent=sends;unsigned before_controls=controls;
    request[5]=2;control_status=control_done=2;
    assert(mc_hw_request(request,8)&&controls==before_controls+1&&mc_hw.pending==2);
    mc_hw_service();assert(sends==sent); /* 게시만 된 데모 요청을 완료로 응답하지 않는다. */
    control_done=0;mc_hw_service();
    assert(sends==sent+1&&!mc_hw.pending&&wire[6]==3&&wire[7]==0);
    control_status=1;assert(mc_hw_request(request,8));mc_hw_service();assert(wire[7]==1);
    puts("hardware probe: bounded version2 reads, nonce, status, backpressure and OTA invalidation PASS");
    return 0;
}
