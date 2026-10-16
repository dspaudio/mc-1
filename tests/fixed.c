/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/fixed.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void check32(int32_t x)
{
    assert(mc_div3(x)==x/3);
    assert(mc_div127(x)==x/127);
    assert(mc_div254(x)==x/254);
    for(unsigned bits=1;bits<=31;bits++)
        assert(mc_div_pow2(x,bits)==(int32_t)((int64_t)x/(INT64_C(1)<<bits)));
}
static void check64(int64_t x)
{
    for(unsigned bits=1;bits<63;bits++)
        assert(mc_div_pow2_64(x,bits)==x/(INT64_C(1)<<bits));
    assert(mc_div_pow2_64(x,63)==(x==INT64_MIN?-1:0));
}
static void check_filter(int32_t x,int32_t state,uint16_t coefficient)
{
    assert(mc_filter_step(x,state,coefficient)==
           (int32_t)(((int64_t)x-state)*coefficient/32768));
}
static void filter_steps(void)
{
    for(unsigned kind=0;kind<2;kind++)for(unsigned value=0;value<128;value++) {
        unsigned cutoff=kind?40+(127-value)*(127-value):30+value*value/3;
        uint16_t coefficient=(uint16_t)(cutoff*32768u/(cutoff+7019u));
        for(uint32_t remainder=0;remainder<32768;remainder++) {
            check_filter((int32_t)remainder,0,coefficient);
            check_filter(0,(int32_t)remainder,coefficient);
            int32_t state=(int32_t)((int64_t)INT32_MAX-(UINT32_MAX-remainder));
            check_filter(INT32_MAX,state,coefficient);
            check_filter(state,INT32_MAX,coefficient);
        }
    }
    static const int32_t boundaries[]={INT32_MIN,INT32_MIN+1,-65536,-32768,-1,0,1,32767,65536,INT32_MAX-1,INT32_MAX};
    static const uint16_t coefficients[]={0,1,32767,32768,65535};
    for(unsigned i=0;i<sizeof boundaries/sizeof boundaries[0];i++)
        for(unsigned j=0;j<sizeof boundaries/sizeof boundaries[0];j++)
            for(unsigned c=0;c<sizeof coefficients/sizeof coefficients[0];c++)
                check_filter(boundaries[i],boundaries[j],coefficients[c]);
}
int main(void)
{
    assert(UINT64_C(30480000)*111u<(UINT64_C(1)<<32));
    for(int32_t x=0;x<=30480000;x++) {
        assert(mc_div127_signal(x)==x/127);
        assert(mc_div127_signal(-x)==-x/127);
    }
    static const unsigned drum_divisors[][2]={
        {4,65535},{6,98303},{5,81919},{3,49151},{8,131071},{7,114687},
        {2,32767},{3,32767},{4,32767},{5,32767},{6,32767},{7,32767},{8,32767},
        {260,2129920},{420,3440640},{160,1310720},{110,901120},
        {340,2785280},{490,4014080},{740,6062080},{200,1638400}
    };
    for(unsigned i=0;i<sizeof drum_divisors/sizeof drum_divisors[0];i++) {
        unsigned d=drum_divisors[i][0],bound=drum_divisors[i][1];
        uint32_t reciprocal=UINT32_MAX/d+1u;
        assert((uint64_t)bound*((uint64_t)reciprocal*d-(UINT64_C(1)<<32))<(UINT64_C(1)<<32));
        for(int32_t x=0;x<=(int32_t)bound;x++) {
            assert(mc_div_recip(x,reciprocal)==x/(int32_t)d);
            assert(mc_div_recip(-x,reciprocal)==-x/(int32_t)d);
        }
    }
    for(int32_t x=-65536;x<=65536;x++)check32(x);
    check32(INT32_MIN);check32(INT32_MAX);
    check64(INT64_MIN);check64(INT64_MAX);check64(-1);check64(0);check64(1);
    for(unsigned bit=0;bit<31;bit++) {
        int64_t boundary=INT64_C(1)<<bit;
        for(int delta=-2;delta<=2;delta++) {
            int64_t x=boundary+delta;
            if(x>=INT32_MIN&&x<=INT32_MAX)check32((int32_t)x);
            if(-x>=INT32_MIN&&-x<=INT32_MAX)check32((int32_t)-x);
        }
        int64_t multiple=(INT32_MAX/127/(int32_t)boundary)*(int64_t)127*boundary;
        for(int delta=-2;delta<=2;delta++) {
            int64_t x=multiple+delta;
            if(x>=INT32_MIN&&x<=INT32_MAX)check32((int32_t)x);
            if(-x>=INT32_MIN&&-x<=INT32_MAX)check32((int32_t)-x);
        }
    }
    uint64_t random=UINT64_C(0x3141592653589793);
    for(unsigned i=0;i<8192;i++) {
        random=random*UINT64_C(6364136223846793005)+UINT64_C(1442695040888963407);
        int64_t x=(int64_t)(random>>1);
        if(random&1u)x=-x;
        check64(x);
        check32((int32_t)(x%INT32_MAX));
    }
    filter_steps();
    puts("fixed arithmetic: exact signed divisors, filter steps, boundaries and carry passed");
    return 0;
}
