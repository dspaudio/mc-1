/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio
 * NEON SPIRAL: original psy-trance study. No Roland notes/samples imported.
 * The same built-in arrangement and effects run on the firmware and host. */
#include "mcfm.h"
#include <stdio.h>
#include <string.h>
static McEngine e;
static void put16(FILE *f,unsigned n) { fputc((int)(n&255),f);fputc((int)((n>>8)&255),f); }
static void put32(FILE *f,unsigned n) { put16(f,n);put16(f,n>>16); }
int main(int argc,char **argv) {
    if(argc!=2) return 1;
    mc_init(&e);mc_demo_load(&e,1);
    unsigned songframes=MC_RATE*240u*32u/140u,frames=songframes+MC_RATE*4;
    FILE *f=fopen(argv[1],"wb");if(!f)return 1;
    fwrite("RIFF",1,4,f);put32(f,36+frames*4);fwrite("WAVEfmt ",1,8,f);put32(f,16);
    put16(f,1);put16(f,2);put32(f,MC_RATE);put32(f,MC_RATE*4);put16(f,4);put16(f,16);fwrite("data",1,4,f);put32(f,frames*4);
    mc_start(&e);int16_t buf[128*2];
    for(unsigned at=0;at<frames;) {
        unsigned n=frames-at>128?128:frames-at;
        mc_render(&e,buf,n);
        for(unsigned i=0;i<n*2;i++)put16(f,(uint16_t)buf[i]);
        at+=n;
    }
    int failed=ferror(f);if(fclose(f))failed=1;
    printf("NEON SPIRAL: %u frames, peak %u, clips %u; host only\n",frames,e.peak,e.clips);
    return failed||e.clips!=0;
}
