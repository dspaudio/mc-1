/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/mcfm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static McEngine e;
static McProject decoded;
static uint8_t record[MC_RECORD_BYTES],other[MC_RECORD_BYTES],legacy[3746];
static int16_t audio[2048];
static void crc_header(uint8_t *data,unsigned version,unsigned body) {
    data[4]=(uint8_t)version;data[6]=(uint8_t)body;data[7]=(uint8_t)(body>>8);
    uint32_t crc=mc_crc(data+20,body);for(unsigned i=0;i<4;i++)data[12+i]=(uint8_t)(crc>>(8*i));
    crc=mc_crc(data,16);for(unsigned i=0;i<4;i++)data[16+i]=(uint8_t)(crc>>(8*i));
}
static void render(unsigned frames) {
    while(frames) { unsigned n=frames>1024?1024:frames;mc_render(&e,audio,n);frames-=n; }
}
static void factory(void) {
    mc_init(&e);
    for(unsigned demo=0;demo<2;demo++) {
        mc_demo_load(&e,demo);
        for(unsigned p=4;p<MC_PATTERNS;p++) for(unsigned t=0;t<4;t++) for(unsigned s=0;s<16;s++) {
            const McStep empty={0};assert(!memcmp(&e.project.step[p][t][s],&empty,sizeof empty));
        }
    }
    puts("Factory and demos keep patterns 5-64 blank: PASS");
}
static void patterns(void) {
    mc_init(&e);e.project.bpm=240;e.project.swing=0;e.project.demo=0;
    memset(e.project.step,0,sizeof e.project.step);
    for(unsigned p=0;p<MC_PATTERNS;p++) {
        e.project.step[p][0][0]=(McStep){{(uint8_t)(36+p),0,0},100,50,0,0};
        mc_queue(&e,p);assert(e.pattern==p);mc_start(&e);e.recording=1;
        assert(!mc_parameter_set(&e,0,4,20+p));assert(!mc_parameter_set(&e,3,MC_PARAM_RD_SEND,p));
        e.recording=0;mc_stop(&e);
    }
    assert(e.project.automation_count==MC_PATTERNS*2&&e.project.automation[127].address==318);
    for(unsigned p=0;p<MC_PATTERNS;p++) {
        mc_queue(&e,(p+1)%MC_PATTERNS);mc_start(&e);mc_queue(&e,p);
        assert(e.queued==p&&e.pattern==(p+1)%MC_PATTERNS);render(MC_RATE);
        assert(e.pattern==p&&e.step==0&&e.queued==255&&e.project.patch[0].cutoff==20+p&&e.project.fx_send[3][0]==p);
        mc_stop(&e);
    }
    mc_queue(&e,64);assert(e.pattern==63);
    uint32_t seq=0;assert(mc_encode(&e.project,123,record,sizeof record)==MC_RECORD_BYTES);
    assert(!mc_record_valid(record,sizeof record,&seq)&&seq==123);
    assert(!mc_decode(&decoded,&seq,record,sizeof record)&&!memcmp(&decoded,&e.project,sizeof decoded));
    assert(mc_encode(&decoded,123,other,sizeof other)==MC_RECORD_BYTES&&!memcmp(record,other,sizeof record));
    mc_automation_clear(&e,63);assert(e.project.automation_count==126);
    for(unsigned i=0;i<126;i++)assert(e.project.automation[i].address/5!=63);
    puts("All 64 queued transitions, high automation addresses, exact codec round trip: PASS");
}
static void invalid(void) {
    uint32_t seq=123,hash=mc_crc(&decoded,sizeof decoded);
    unsigned last=20+72+63*512;
    record[last]=109;crc_header(record,5,MC_PROJECT_BYTES);
    assert(mc_decode(&decoded,&seq,record,sizeof record)<0&&seq==123&&mc_crc(&decoded,sizeof decoded)==hash);
    memcpy(record,other,sizeof record);unsigned events=20+32910;
    record[events]=64;record[events+1]=1;crc_header(record,5,MC_PROJECT_BYTES);
    assert(mc_record_valid(record,sizeof record,&seq)<0&&seq==123);
    memcpy(record,other,sizeof record);memcpy(record+events+5,record+events,5);crc_header(record,5,MC_PROJECT_BYTES);
    assert(mc_decode(&decoded,&seq,record,sizeof record)<0&&mc_crc(&decoded,sizeof decoded)==hash);
    assert(mc_recover(&decoded,&seq,record,other)==1&&seq==123);
    for(unsigned n=0;n<MC_RECORD_BYTES;n+=197)assert(mc_record_valid(other,n,&seq)<0);
    puts("Invalid last pattern, out-of-range/duplicate automation, truncated records and recovery: PASS");
}
static void imports(void) {
    mc_init(&e);mc_start(&e);e.recording=1;assert(!mc_parameter_set(&e,1,4,33));mc_stop(&e);
    assert(mc_encode(&e.project,77,record,sizeof record)>0);
    for(unsigned version=1;version<=4;version++) {
        unsigned body=version==1?2112:version==2?2120:version==3?2136:3726;
        memset(legacy,0,sizeof legacy);memcpy(legacy,record,20+64);
        if(version==1)memcpy(legacy+20+64,record+20+72,2048);
        else memcpy(legacy+20+64,record+20+64,2056);
        if(version>=3)memcpy(legacy+20+2120,record+20+32840,16);
        if(version==4) {
            memcpy(legacy+20+2136,record+20+32856,54);
            for(unsigned i=0;i<MC_AUTOMATION;i++) {
                const uint8_t *in=record+20+32910+i*5;uint8_t *out=legacy+20+2190+i*4;
                out[0]=in[0];memcpy(out+1,in+2,3);
            }
        }
        crc_header(legacy,version,body);uint32_t seq=0;
        assert(!mc_decode(&decoded,&seq,legacy,body+20)&&seq==77);
        assert(!memcmp(decoded.step,e.project.step,2048));
        for(unsigned p=4;p<MC_PATTERNS;p++) for(unsigned t=0;t<4;t++) for(unsigned s=0;s<16;s++)assert(!decoded.step[p][t][s].velocity);
        assert(decoded.automation_count==(version==4?1:0));
        if(version==4)assert(decoded.automation[0].address==1&&decoded.automation[0].value==33);
    }
    printf("Version 1-4 imports: PASS; project RAM=%zu, body=%u, record=%u\n",sizeof(McProject),MC_PROJECT_BYTES,MC_RECORD_BYTES);
}
int main(void) { factory();patterns();invalid();imports();puts("patterns: PASS"); }
