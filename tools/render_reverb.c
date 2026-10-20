/* SPDX-License-Identifier: GPL-3.0-only */
#include "mcfm.h"
#include "fixed.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

enum { FRAMES=MC_RATE*12 };
static McSharedFx fx;
static int32_t source[FRAMES],wet[FRAMES][2];
static void le16(FILE *f,unsigned v) {fputc((int)(v&255u),f);fputc((int)((v>>8)&255u),f);}
static void le32(FILE *f,unsigned v) {le16(f,v);le16(f,v>>16);}
static void wav(const char *prefix,const char *name,unsigned send,unsigned kind) {
    char path[512];
    int length=snprintf(path,sizeof path,"%s-%s-send%u-%s.wav",prefix,name,send,kind?"percussion":"impulse");
    assert(length>0&&(unsigned)length<sizeof path);
    FILE *f=fopen(path,"wb");assert(f);
    fwrite("RIFF",1,4,f);le32(f,36+FRAMES*4);fwrite("WAVEfmt ",1,8,f);
    le32(f,16);le16(f,1);le16(f,2);le32(f,MC_RATE);le32(f,MC_RATE*4);le16(f,4);le16(f,16);
    fwrite("data",1,4,f);le32(f,FRAMES*4);
    for(unsigned n=0;n<FRAMES;n++)for(unsigned c=0;c<2;c++) {
        int32_t v=source[n]+wet[n][c];assert(v>=-32768&&v<=32767);
        le16(f,(unsigned)(uint16_t)v);
    }
    assert(!ferror(f));assert(fclose(f)==0);
}
int main(int argc,char **argv) {
    assert(argc==2);
    puts("input,mode,send,wet_dry_db,early_energy,tail_250ms_energy,tail_1s_energy,t20_seconds,density_100_500ms,peak,last_seconds");
    for(unsigned kind=0;kind<2;kind++) {
        memset(source,0,sizeof source);uint32_t random=1;
        for(unsigned n=0;n<FRAMES;n++) {
            if(!kind)source[n]=n==0?12000:0;
            else if(n<MC_RATE) {
                unsigned t=n%(MC_RATE/4);double time=(double)t/MC_RATE;
                random=random*1664525u+1013904223u;
                source[n]=(int32_t)(exp(-time*35)*(4500*sin(6.283185307179586*220*time)
                    +2000*sin(6.283185307179586*659*time)+((int32_t)(random>>16)-32768)*0.035));
            }
        }
        for(unsigned type=0;type<6;type++)for(unsigned s=0;s<4;s++) {
            const unsigned sends[4]={0,40,80,127};unsigned send=sends[s];
            memset(&fx,0,sizeof fx);
            for(unsigned at=0;at<FRAMES;) {
                unsigned count=FRAMES-at;if(count>128)count=128;
                for(unsigned n=0;n<count;n++)wet[at+n][0]=wet[at+n][1]=mc_div127_signal(source[at+n]*(int32_t)send);
                mc_fx_rd_block(&fx,120,type,&wet[at],count);
                /* Match engine.c's shared-return mix; common master gain cancels
                 * in the wet/dry ratio and is omitted from these comparisons. */
                for(unsigned n=0;n<count;n++)for(unsigned c=0;c<2;c++)wet[at+n][c]=mc_div_pow2(wet[at+n][c],1);
                at+=count;
            }
            double energy=0,dry=0,early=0,tail=0,late=0;unsigned last=0,dense=0;int32_t peak=0;
            for(unsigned n=0;n<FRAMES;n++) {
                double e=(double)wet[n][0]*wet[n][0]+(double)wet[n][1]*wet[n][1];
                energy+=e;dry+=2.0*source[n]*source[n];
                if(n<MC_RATE/4)early+=e;else tail+=e;
                if(n>=MC_RATE)late+=e;
                if(n>=MC_RATE/10&&n<MC_RATE/2&&(wet[n][0]||wet[n][1]))dense++;
                for(unsigned c=0;c<2;c++) {
                    int32_t v=source[n]+wet[n][c],a=v<0?-v:v;if(a>peak)peak=a;
                    if(wet[n][c])last=n;
                }
            }
            double accumulated=0;unsigned t10=0,t30=0;
            for(unsigned n=0;n<FRAMES&&energy>0;n++) {
                accumulated+=(double)wet[n][0]*wet[n][0]+(double)wet[n][1]*wet[n][1];
                if(!t10&&accumulated>=energy*0.9)t10=n;
                if(accumulated>=energy*0.999){t30=n;break;}
            }
            printf("%s,%s,%u,%.3f,%.0f,%.0f,%.0f,%.6f,%.6f,%d,%.6f\n",
                kind?"percussion":"impulse",mc_fx_rd_names[type],send,energy>0?10*log10(energy/dry):-200.0,
                early,tail,late,(double)(t30-t10)/MC_RATE,(double)dense/(MC_RATE*0.4),peak,(double)last/MC_RATE);
            if(send==0)assert(energy==0);
            wav(argv[1],mc_fx_rd_names[type],send,kind);
        }
    }
    return 0;
}
