/* SPDX-License-Identifier: GPL-3.0-only */
#include "mcfm.h"
#include "store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define BASE (MC_STORE_A-MC_SECTOR)
#define TOTAL (MC_STORE_BYTES*2+MC_SECTOR*2)
static uint8_t flash[TOTAL],snapshot[TOTAL];
static McProject original,changed,loaded;
static int budget=-1,corrupt_write,fail_verify;
static uint32_t unreadable;
static unsigned erased,programmed,reads,max_read;
static int read_flash(uint32_t off,void *out,unsigned n) {
    assert(off>=MC_STORE_A&&off<=MC_STORE_END&&n<=MC_STORE_END-off);
    reads++;if(n>max_read)max_read=n;
    if((unreadable&&off>=unreadable&&off<unreadable+MC_STORE_BYTES)||(fail_verify&&programmed==MC_RECORD_BYTES))return -1;
    memcpy(out,flash+(off-BASE),n);return 0;
}
static int byte(uint32_t off,uint8_t value,int erase) {
    assert(off>=MC_STORE_A&&off<MC_STORE_END);
    if(budget==0)return -1;
    if(budget>0)budget--;
    uint8_t *p=flash+(off-BASE);
    if(!erase)assert((*p&value)==value);
    *p=erase?255:value;return 0;
}
static int erase_flash(uint32_t off) {
    assert(off>=MC_STORE_A&&off<MC_STORE_END&&off%MC_SECTOR==0);
    for(unsigned i=0;i<MC_SECTOR;i++)if(byte(off+i,255,1))return -1;
    erased++;return 0;
}
static int program_flash(uint32_t off,const void *data,unsigned n) {
    const uint8_t *p=data;
    assert(off>=MC_STORE_A&&off<=MC_STORE_END&&n<=MC_STORE_END-off);
    assert(erased==9);
    if(off==MC_STORE_A||off==MC_STORE_B)assert(programmed==MC_PROJECT_BYTES&&n==20);
    else assert((off==MC_STORE_A+20||off==MC_STORE_B+20)&&programmed==0&&n==MC_PROJECT_BYTES);
    for(unsigned i=0;i<n;i++) {
        if(byte(off+i,p[i],0))return -1;
        programmed++;
    }
    if(corrupt_write&&n>20)flash[off-BASE+99]^=1;
    return 0;
}
static const McStorage io={read_flash,erase_flash,program_flash};
static void reset_io(void) { budget=-1;corrupt_write=fail_verify=0;unreadable=0;erased=programmed=reads=max_read=0; }
static void guards(void) {
    for(unsigned i=0;i<MC_SECTOR;i++)assert(flash[i]==0xa5&&flash[TOTAL-MC_SECTOR+i]==0xa5);
}
static void cut_at(unsigned cutoff) {
    memcpy(flash,snapshot,sizeof flash);reset_io();budget=(int)cutoff;
    assert(mc_store_save(&io,&changed)<0);assert(budget==0);budget=-1;
    uint32_t seq=999;
    assert(mc_store_load(&io,&loaded,&seq)==0&&seq==1);
    assert(!memcmp(&loaded,&original,sizeof loaded));guards();
}
int main(void) {
    assert(MC_RECORD_BYTES==35124&&MC_STORE_BYTES==36864);
    assert(MC_STORE_A==0x99000&&MC_STORE_B==0xa2000&&MC_STORE_END==0xab000);
    mc_factory(&original);
    original.fx_send[0][0]=127;original.fx_send[3][1]=93;
    original.fx_bus_type[0]=7;original.fx_bus_type[1]=5;
    changed=original;changed.bpm=142;
    changed.step[63][2][15].note[0]=99;changed.step[63][2][15].velocity=101;changed.step[63][2][15].gate=75;
    assert(mc_project_valid(&original)&&mc_project_valid(&changed));
    memset(flash,255,sizeof flash);memset(flash,0xa5,MC_SECTOR);memset(flash+TOTAL-MC_SECTOR,0xa5,MC_SECTOR);
    uint32_t seq=77;assert(mc_store_load(&io,&loaded,&seq)<0&&seq==77);
    assert(!mc_store_save(&io,&original)&&erased==9&&programmed==MC_RECORD_BYTES);
    assert(mc_store_load(&io,&loaded,&seq)==0&&seq==1&&!memcmp(&loaded,&original,sizeof loaded));
    memcpy(snapshot,flash,sizeof flash);
    unsigned cuts=0;
    for(unsigned sector=0;sector<=9;sector++)for(int d=-1;d<=1;d++) {
        int pos=(int)(sector*MC_SECTOR)+d;if(pos>=0){cut_at((unsigned)pos);cuts++;}
    }
    for(unsigned page=0;page<MC_PROJECT_BYTES;page+=256)for(int d=-1;d<=1;d++) {
        int pos=(int)(MC_STORE_BYTES+page)+d;cut_at((unsigned)pos);cuts++;
    }
    for(unsigned i=0;i<20;i++){cut_at(MC_STORE_BYTES+MC_PROJECT_BYTES+i);cuts++;}
    printf("PASS: %u interrupted erase/page/header positions preserve full prior64-pattern project; guard sectors unchanged\n",cuts);
    memcpy(flash,snapshot,sizeof flash);reset_io();assert(!mc_store_save(&io,&changed));
    assert(mc_store_load(&io,&loaded,&seq)==1&&seq==2&&!memcmp(&loaded,&changed,sizeof loaded));guards();
    reset_io();assert(!mc_store_save(&io,&original));assert(mc_store_load(&io,&loaded,&seq)==0&&seq==3);
    puts("PASS: alternating nine-sector banks commit payload before header; pattern64 round trip");
    memcpy(flash,snapshot,sizeof flash);reset_io();unreadable=MC_STORE_B;
    assert(mc_store_load(&io,&loaded,&seq)==0);assert(mc_store_save(&io,&changed)<0&&!erased);
    unreadable=MC_STORE_A;assert(mc_store_load(&io,&loaded,&seq)<0);assert(mc_store_save(&io,&changed)<0&&!erased);
    reset_io();corrupt_write=1;assert(mc_store_save(&io,&changed)<0);corrupt_write=0;
    assert(mc_store_load(&io,&loaded,&seq)==0&&seq==1);guards();
    memcpy(flash,snapshot,sizeof flash);reset_io();fail_verify=1;assert(mc_store_save(&io,&changed)<0);fail_verify=0;
    assert(mc_store_load(&io,&loaded,&seq)==1&&seq==2);
    puts("PASS: unreadable banks refuse save; corrupted readback rejected; committed data survives verification-read failure");
    reset_io();assert(mc_encode(&original,0xffffffffu,flash+(MC_STORE_A-BASE),MC_RECORD_BYTES)>0);
    assert(mc_encode(&changed,0,flash+(MC_STORE_B-BASE),MC_RECORD_BYTES)>0);
    assert(mc_store_load(&io,&loaded,&seq)==1&&seq==0);
    assert(!mc_store_save(&io,&original));assert(mc_store_load(&io,&loaded,&seq)==0&&seq==1);
    flash[MC_STORE_B-BASE+6]=255;flash[MC_STORE_B-BASE+7]=255;
    assert(mc_store_load(&io,&loaded,&seq)==0);guards();
    puts("PASS: sequence rollover and oversized corrupt header; no read/write outside new72KiB banks");
    reset_io();memset(flash+(MC_STORE_A-BASE),255,MC_STORE_BYTES*2);
    changed=original;memset(changed.track_fx,0,sizeof changed.track_fx);
    changed.track_fx[1][1]=80;changed.track_fx_type[1][1]=2;
    changed.track_fx[3][0]=50;changed.track_fx[2][3]=67;changed.track_fx_type[2][3]=2;
    uint8_t *old=flash+(MC_STORE_A-BASE);
    assert(mc_encode(&changed,42,old,MC_RECORD_BYTES)>0);
    old[4]=6;old[6]=35094&255;old[7]=35094>>8;
    uint32_t crc=mc_crc(old+20,35094);for(unsigned i=0;i<4;i++) old[12+i]=(uint8_t)(crc>>(8*i));
    crc=mc_crc(old,16);for(unsigned i=0;i<4;i++) old[16+i]=(uint8_t)(crc>>(8*i));
    assert(mc_store_load(&io,&loaded,&seq)==0&&seq==42&&max_read<=35094);
    assert(loaded.fx_send[1][0]==80&&loaded.fx_send[3][0]==50&&loaded.fx_send[2][1]==67);
    assert(loaded.fx_bus_type[0]==5&&loaded.fx_bus_type[1]==5&&loaded.bpm==changed.bpm);
    changed=loaded;
    assert(!mc_store_save(&io,&loaded));
    assert(mc_store_load(&io,&loaded,&seq)==1&&seq==43&&!memcmp(&loaded,&changed,sizeof loaded));
    assert(flash[MC_STORE_B-BASE+4]==7);guards();
    puts("PASS: fixed-size v6 bank migrates and saves as v7 without changing storage ranges");
    return 0;
}
