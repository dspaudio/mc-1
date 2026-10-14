/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "store.h"
/* Shared scratch: only call from the foreground, with transport stopped. */
static uint8_t record[MC_RECORD_BYTES];
_Static_assert(MC_RECORD_BYTES<=MC_STORE_BYTES,"Project exceeds storage bank");
static int read_record(const McStorage *io,uint32_t addr,unsigned *length) {
    if(io->read(addr,record,20)) return -2;
    unsigned n=20u+record[6]+((unsigned)record[7]<<8);
    if(n>sizeof record) return -1;
    if(io->read(addr+20,record+20,n-20)) return -2;
    *length=n;return 0;
}
static int inspect(const McStorage *io,uint32_t addr,uint32_t *seq) {
    unsigned n=0;int rc=read_record(io,addr,&n);
    return rc?rc:mc_record_valid(record,n,seq);
}
static int newer(const int valid[2],const uint32_t seq[2]) {
    if(valid[0]&&valid[1]) return -1;
    return !valid[1]&&(valid[0]||(int32_t)(seq[1]-seq[0])>0)?1:0;
}
static int scan(const McStorage *io,int valid[2],uint32_t seq[2]) {
    valid[0]=inspect(io,MC_STORE_A,&seq[0]);
    valid[1]=inspect(io,MC_STORE_B,&seq[1]);
    return newer(valid,seq);
}
int mc_store_load(const McStorage *io,McProject *p,uint32_t *seq) {
    int valid[2];uint32_t sequence[2]={0,0};
    int active=scan(io,valid,sequence);
    if(active<0) return -1;
    unsigned n=0;
    if(read_record(io,MC_STORE_A+(unsigned)active*MC_STORE_BYTES,&n)||mc_decode(p,seq,record,n)) return -1;
    return active;
}
int mc_store_save(const McStorage *io,const McProject *p) {
    int valid[2];uint32_t sequence[2]={0,0};
    int active=scan(io,valid,sequence);
    /* An unreadable destination may hold the only recoverable project. */
    if(valid[0]==-2||valid[1]==-2) return -1;
    uint32_t seq=active<0?0:sequence[active];
    unsigned dest=active==0?1u:0u;
    if(mc_encode(p,seq+1,record,sizeof record)<0) return -1;
    uint32_t addr=dest?MC_STORE_B:MC_STORE_A;
    for(unsigned off=0;off<MC_STORE_BYTES;off+=MC_SECTOR)
        if(io->erase(addr+off)) return -1;
    /* Payload first; header (including CRC) commits only after a complete write. */
    if(io->program(addr+20,record+20,MC_PROJECT_BYTES)) return -1;
    if(io->program(addr,record,20)) return -1;
    uint8_t verify[256];
    for(unsigned off=0;off<MC_RECORD_BYTES;off+=sizeof verify) {
        unsigned n=MC_RECORD_BYTES-off;if(n>sizeof verify)n=sizeof verify;
        if(io->read(addr+off,verify,n)) return -1;
        for(unsigned i=0;i<n;i++) if(record[off+i]!=verify[i]) return -1;
    }
    return 0;
}
