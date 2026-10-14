/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MC_STORE_H
#define MC_STORE_H
#include "mcfm.h"
#define MC_STORE_A 0x97000u
#define MC_STORE_B 0x98000u
#define MC_SECTOR 4096u
typedef struct {
    int (*read)(uint32_t,void *,unsigned);
    int (*erase)(uint32_t);
    int (*program)(uint32_t,const void *,unsigned);
} McStorage;
int mc_store_load(const McStorage *io,McProject *p,uint32_t *seq);
int mc_store_save(const McStorage *io,const McProject *p);
#endif
