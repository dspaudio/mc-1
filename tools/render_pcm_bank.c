/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
/* Remaining PCM presets, grouped by family, through the actual shared C engine. */
#include "mcfm.h"
#include <stdio.h>
#include <string.h>
static McEngine e;
static void u16(FILE *f,unsigned v) { fputc((int)(v&255),f);fputc((int)((v>>8)&255),f); }
static void u32(FILE *f,unsigned v) { u16(f,v);u16(f,v>>16); }
static int approved(unsigned preset) {
    return preset==MC_PRESET_PIANO_BRIGHT||preset==MC_PRESET_VIOLIN_BOW||
        preset==MC_PRESET_VIOLA_CHAMBER||preset==MC_PRESET_CELLO_ENSEMBLE||
        preset==MC_PRESET_VIOLIN_SILVER||preset==MC_PRESET_NYLON_PLUCK;
}
static int group(const char *directory,unsigned first,unsigned end,FILE *index) {
    const unsigned segment=MC_RATE*4;
    const char *category=mc_preset_categories[first];
    unsigned count=0;
    for(unsigned p=first;p<end;p++)if(mc_presets[p].engine==MC_LA&&!approved(p))count++;
    if(!count)return 0;
    char slug[32],path[512];
    unsigned size=(unsigned)strlen(category);
    for(unsigned i=0;i<size;i++) {
        char c=category[i];
        slug[i]=c>='A'&&c<='Z'?(char)(c-'A'+'a'):c==' '||c=='/'?'-':c;
    }
    slug[size]=0;
    snprintf(path,sizeof path,"%s/%s.wav",directory,slug);
    FILE *f=fopen(path,"wb");if(!f)return 1;
    unsigned frames=segment*count;
    fwrite("RIFF",1,4,f);u32(f,36+frames*4);fwrite("WAVEfmt ",1,8,f);u32(f,16);
    u16(f,1);u16(f,2);u32(f,MC_RATE);u32(f,MC_RATE*4);u16(f,4);u16(f,16);
    fwrite("data",1,4,f);u32(f,frames*4);
    unsigned position=0;
    for(unsigned p=first;p<end;p++) {
        if(mc_presets[p].engine!=MC_LA||approved(p))continue;
        unsigned note=!strcmp(category,"Bass")?36:!strcmp(category,"Mallet/Bell")?72:60;
        mc_init(&e);memset(e.project.fx_send,0,sizeof e.project.fx_send);
        memset(e.project.analog,0,sizeof e.project.analog);
        mc_preset(&e,0,p);
        for(unsigned at=0;at<segment;) {
            if(!at)mc_note(&e,0,note,92,0,MC_RATE*7u/20u);
            if(at==MC_RATE/2)mc_note(&e,0,note+7,92,0,MC_RATE*7u/20u);
            if(at==MC_RATE)mc_note(&e,0,note+12,92,0,MC_RATE*3u/2u);
            unsigned boundary=at<MC_RATE/2?MC_RATE/2:at<MC_RATE?MC_RATE:segment;
            unsigned n=boundary-at;if(n>256)n=256;
            int16_t audio[256*2];mc_render(&e,audio,n);
            for(unsigned i=0;i<n*2;i++)u16(f,(uint16_t)audio[i]);
            at+=n;
        }
        fprintf(index,"%s,%u,%s,%s.wav,%u,4,%u,%u,%u\n",category,p+1,
            mc_preset_names[p],slug,position*4,e.peak,e.clips,e.steals);
        if(e.clips||e.steals) { fclose(f);return 1; }
        position++;
    }
    int failed=ferror(f);if(fclose(f))failed=1;
    printf("%s: %u presets, %u frames, no clips or steals\n",path,count,frames);
    return failed;
}
int main(int argc,char **argv) {
    if(argc!=2) { fputs("usage: render-pcm-bank OUTPUT_DIRECTORY\n",stderr);return 1; }
    char path[512];snprintf(path,sizeof path,"%s/bank-index.csv",argv[1]);
    FILE *index=fopen(path,"wb");if(!index)return 1;
    fputs("category,preset,name,file,start_seconds,duration_seconds,peak,clips,steals\n",index);
    int failed=0;
    for(unsigned first=0;first<MC_PRESETS;) {
        unsigned end=first+1;
        while(end<MC_PRESETS&&!strcmp(mc_preset_categories[first],mc_preset_categories[end]))end++;
        if(group(argv[1],first,end,index)) { failed=1;break; }
        first=end;
    }
    if(ferror(index))failed=1;
    if(fclose(index))failed=1;
    return failed;
}
