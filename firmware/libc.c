/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The few libc routines the compiler may call implicitly (struct copies,
 * zeroing). Freestanding: no other libc. */
#include <stdint.h>
typedef uint64_t McMemoryWord __attribute__((__may_alias__));
typedef uint32_t McMemoryHalf __attribute__((__may_alias__));
void *memset(void *d, int c, unsigned n)
{
    unsigned char *p = d;
    while(n&&((uintptr_t)p&7u)){*p++=(unsigned char)c;n--;}
    uint64_t word=(unsigned char)c;
    word|=word<<8;word|=word<<16;word|=word<<32;
    McMemoryWord *wide=(McMemoryWord *)p;
    while(n>=32){wide[0]=word;wide[1]=word;wide[2]=word;wide[3]=word;wide+=4;n-=32;}
    while(n>=8){*wide++=word;n-=8;}
    p=(unsigned char *)wide;
    while (n--)
        *p++ = (unsigned char)c;
    return d;
}

void *memcpy(void *d, const void *s, unsigned n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    if(n>=4&&((uintptr_t)p&7u)==4u&&((uintptr_t)q&7u)==4u) {
        *(McMemoryHalf *)p=*(const McMemoryHalf *)q;p+=4;q+=4;n-=4;
    }
    if(!(((uintptr_t)p|(uintptr_t)q)&7u)) {
        McMemoryWord *wide=(McMemoryWord *)p;
        const McMemoryWord *read=(const McMemoryWord *)q;
        while(n>=32) {
            wide[0]=read[0];wide[1]=read[1];wide[2]=read[2];wide[3]=read[3];
            wide+=4;read+=4;n-=32;
        }
        while(n>=8){*wide++=*read++;n-=8;}
        p=(unsigned char *)wide;q=(const unsigned char *)read;
    }
    if(!(((uintptr_t)p|(uintptr_t)q)&3u)) {
        McMemoryHalf *half=(McMemoryHalf *)p;
        const McMemoryHalf *read=(const McMemoryHalf *)q;
        while(n>=4){*half++=*read++;n-=4;}
        p=(unsigned char *)half;q=(const unsigned char *)read;
    }
    while (n--)
        *p++ = *q++;
    return d;
}

int memcmp(const void *a, const void *b, unsigned n)
{
    const unsigned char *p = a, *q = b;
    for (; n; n--, p++, q++)
        if (*p != *q)
            return *p - *q;
    return 0;
}

static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static unsigned str_len(const char *s)
{
    unsigned n = 0;
    while (s[n])
        n++;
    return n;
}

static void str_cpy(char *d, const char *s, unsigned max)
{
    unsigned i = 0;
    while (s[i] && i + 1 < max) {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
}

/* decimal with an optional fixed point: fmt_fix(buf, -63, 1) -> "-6.3" */
static void fmt_fix(char *b, int32_t v, int decimals)
{
    char t[12];
    int n = 0, neg = v < 0, i;
    uint32_t u = (uint32_t)(neg ? -v : v);
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
        if (n == decimals)
            t[n++] = '.';
    } while (u || n <= decimals);
    if (neg)
        t[n++] = '-';
    for (i = 0; i < n; i++)
        b[i] = t[n - 1 - i];
    b[n] = 0;
}

static void fmt_int(char *b, int32_t v) { fmt_fix(b, v, 0); }

static uint32_t rng_state = 0x1234567u;
static uint32_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
