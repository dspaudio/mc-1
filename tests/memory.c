/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef memset
#undef memcpy
#undef memcmp
#define memset mc_test_memset
#define memcpy mc_test_memcpy
#define memcmp mc_test_memcmp
#include "../firmware/libc.c"
#undef memset
#undef memcpy
#undef memcmp

static _Alignas(8) uint8_t source[74016],actual[74016],expected[74016];
int main(void) {
    static const unsigned sizes[]={0,1,2,3,4,5,7,8,15,16,31,32,63,64,
        127,128,255,256,511,512,1023,1024,32784,36944,73888};
    static const int values[]={0,1,0x5a,0xa5,0x100,0x1ff,-1};
    unsigned cases=0;
    for(unsigned i=0;i<sizeof source;i++)source[i]=(uint8_t)(i*37u+11u);
    for(unsigned dst=0;dst<8;dst++)for(unsigned src=0;src<8;src++)
    for(unsigned k=0;k<sizeof sizes/sizeof sizes[0];k++) {
        unsigned n=sizes[k],span=n+32;
        memset(actual,0xa5,span);memset(expected,0xa5,span);
        memcpy(expected+8+dst,source+src,n);
        assert(mc_test_memcpy(actual+8+dst,source+src,n)==actual+8+dst);
        assert(!memcmp(actual,expected,span));
        assert(!mc_test_memcmp(actual,expected,span));
        cases++;
    }
    for(unsigned dst=0;dst<8;dst++)
    for(unsigned k=0;k<sizeof sizes/sizeof sizes[0];k++)
    for(unsigned v=0;v<sizeof values/sizeof values[0];v++) {
        unsigned n=sizes[k],span=n+32;
        memset(actual,0x3c,span);memset(expected,0x3c,span);
        memset(expected+8+dst,values[v],n);
        assert(mc_test_memset(actual+8+dst,values[v],n)==actual+8+dst);
        assert(!memcmp(actual,expected,span));
        cases++;
    }
    printf("PASS memory: %u aligned/unaligned zero/small/FX-history copy+set cases and sentinel bytes\n",cases);
    return 0;
}
