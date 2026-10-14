/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "store.h"
/* Shared scratch: only call from the foreground, with transport stopped. */
static uint8_t records[2][MC_RECORD_BYTES];
static int read_both(const McStorage *io) {
    return io->read(MC_STORE_A,records[0],MC_RECORD_BYTES)||io->read(MC_STORE_B,records[1],MC_RECORD_BYTES);
}
int mc_store_load(const McStorage *io,McProject *p,uint32_t *seq) {
    /* A failed bank read must not hide a readable, valid other bank.
     * Save remains stricter: an unreadable bank is never safe to erase. */
    int a=io->read(MC_STORE_A,records[0],MC_RECORD_BYTES);
    int b=io->read(MC_STORE_B,records[1],MC_RECORD_BYTES);
    if(a&&b) return -1;
    if(a) return mc_decode(p,seq,records[1],MC_RECORD_BYTES)<0?-1:1;
    if(b) return mc_decode(p,seq,records[0],MC_RECORD_BYTES)<0?-1:0;
    return mc_recover(p,seq,records[0],records[1]);
}
int mc_store_save(const McStorage *io,const McProject *p) {
    McProject previous; uint32_t seq=0;
    if(read_both(io)) return -1;
    int active=mc_recover(&previous,&seq,records[0],records[1]);
    unsigned dest=active==0?1:0;
    uint32_t addr=dest?MC_STORE_B:MC_STORE_A;
    if(mc_encode(p,seq+1,records[dest],MC_RECORD_BYTES)<0) return -1;
    if(io->erase(addr)) return -1;
    /* Payload first; header (including CRC) commits only after a complete write. */
    if(io->program(addr+20,records[dest]+20,MC_PROJECT_BYTES)) return -1;
    if(io->program(addr,records[dest],20)) return -1;
    if(io->read(addr,records[dest],MC_RECORD_BYTES)) return -1;
    return mc_decode(&previous,&seq,records[dest],MC_RECORD_BYTES);
}
