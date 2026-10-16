/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include <stdint.h>

/* 음수도 C 나눗셈처럼 0 방향으로 절삭한다. 호출하는 shift는 1..31/63이다. */
static inline __attribute__((always_inline)) int32_t mc_div_pow2(int32_t x,unsigned shift)
{
    return (x+(x<0?(int32_t)((UINT32_C(1)<<shift)-1u):0))>>shift;
}
static inline __attribute__((always_inline)) int64_t mc_div_pow2_64(int64_t x,unsigned shift)
{
    return (x+(x<0?(int64_t)((UINT64_C(1)<<shift)-1u):0))>>shift;
}
/* 3*M = 2^32+2; the high product plus sign restores truncation toward zero. */
static inline __attribute__((always_inline)) int32_t mc_div3(int32_t x)
{
    return (int32_t)(((int64_t)x*INT64_C(1431655766))>>32)+(int32_t)((uint32_t)x>>31);
}
/* For M=ceil(2^32/d), exact when |x|*(M*d-2^32) < 2^32.
 * Callers bound the numerator; magnitude conversion also handles INT32_MIN. */
static inline __attribute__((always_inline)) int32_t mc_div_recip(int32_t x,uint32_t reciprocal)
{
    uint32_t sign=(uint32_t)x>>31,mask=0u-sign;
    uint32_t magnitude=((uint32_t)x^mask)+sign;
    uint32_t quotient=(uint32_t)(((uint64_t)magnitude*reciprocal)>>32);
    return (int32_t)((quotient^mask)+sign);
}
/* M*127 = 2^38+119 and |x|*119 < 2^38 over int32.
 * The signed high product plus x is floor(x*M/2^32); correct negative rounding once. */
static inline __attribute__((always_inline)) int32_t mc_div127_scale(int32_t x,unsigned shift)
{
    int32_t high=(int32_t)(((int64_t)x*-INT64_C(2130574327))>>32);
    return ((high+x)>>(shift-32))+(int32_t)((uint32_t)x>>31);
}
static inline __attribute__((always_inline)) int32_t mc_div127(int32_t x)
{
    return mc_div127_scale(x,38);
}
/* 127*M=2^32+111. Validated audio controls bound |x| to 8*30000*127,
 * below 38693400, so the positive high product needs only signed rounding. */
static inline __attribute__((always_inline)) int32_t mc_div127_signal(int32_t x)
{
    return (int32_t)(((int64_t)x*INT64_C(33818641))>>32)+(int32_t)((uint32_t)x>>31);
}
static inline __attribute__((always_inline)) int32_t mc_div254(int32_t x)
{
    return mc_div127_scale(x,39);
}
/* Preserve truncation toward zero even when the signed difference exceeds int32. */
static inline __attribute__((always_inline)) int32_t mc_filter_step(int32_t x,int32_t state,uint16_t coefficient)
{
    uint32_t magnitude=x<state?(uint32_t)state-(uint32_t)x:(uint32_t)x-(uint32_t)state;
    uint32_t step=(uint32_t)(((uint64_t)magnitude*coefficient)>>15);
    return (int32_t)(x<state?0u-step:step);
}
