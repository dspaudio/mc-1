/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
/* Original release miniatures: host shared-engine audio, not FM-1 recordings. */
#include "mcfm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(MC_VOICES == 8, "Release samples require the eight-voice engine");
enum { PPQ=96, BAR=PPQ*4, BARS=8, MAX_EVENTS=1024 };
typedef struct {
    unsigned frame, gate, part, note, velocity, order;
} Event;
static McEngine e;
static Event events[MAX_EVENTS];
static unsigned count, tempo;
static const char *const names[]={
    "hip-hop-piano.wav","pop-guitar.wav","chamber-strings.wav"
};

static unsigned frame_at(unsigned tick) {
    return (unsigned)((uint64_t)tick*MC_RATE*60u/(tempo*PPQ));
}
static void add(unsigned tick,unsigned part,unsigned note,unsigned velocity,unsigned length) {
    if(count==MAX_EVENTS) {
        fputs("Score event capacity exceeded\n",stderr);
        exit(1);
    }
    events[count]=(Event){frame_at(tick),frame_at(tick+length)-frame_at(tick),
        part,note,velocity,count};
    count++;
}
static int compare(const void *a,const void *b) {
    const Event *x=a,*y=b;
    if(x->frame!=y->frame)return x->frame<y->frame?-1:1;
    return (x->order>y->order)-(x->order<y->order);
}
static void piano(void) {
    /* D minor ninth, G thirteen, C major ninth, A altered; final D minor six. */
    static const uint8_t bass[BARS]={38,43,36,33,38,43,33,38};
    static const uint8_t voicing[BARS][3]={
        {53,60,64},{53,59,64},{52,59,62},{55,61,65},
        {53,60,64},{53,59,64},{55,61,65},{53,57,59}
    };
    static const uint8_t tune[BARS][8]={
        {0,69,72,76,0,74,72,69},{67,0,69,71,74,0,76,71},
        {67,71,0,74,76,74,0,71},{73,76,0,79,77,76,73,0},
        {69,0,76,81,79,0,76,74},{71,74,76,0,74,71,69,0},
        {73,0,76,79,77,76,73,69},{74,0,0,0,0,0,0,0}
    };
    for(unsigned bar=0;bar<BARS;bar++) {
        unsigned start=bar*BAR;
        add(start,2,bass[bar],84,bar==7?220:104);
        for(unsigned c=0;c<3;c++) {
            add(start+c*2,1,voicing[bar][c],76-c*4,bar==7?220:68);
            if(bar<7)add(start+240+c*2,1,voicing[bar][c],64-c*3,60);
        }
        for(unsigned n=0;n<8;n++)if(tune[bar][n]) {
            /* Delayed offbeats give a restrained, deterministic swing. */
            unsigned tick=start+n*48+(n%2?10:0);
            add(tick,0,tune[bar][n],n%2?94:105,bar==7?240:34);
        }
        if(bar<7) {
            add(start,3,0,74,1);
            add(start+96,3,1,62,1);
            add(start+202,3,0,58,1);
            add(start+288,3,1,67,1);
            for(unsigned h=0;h<8;h++)
                add(start+h*48+(h%2?10:0),3,2,h%2?33:43,1);
        }
    }
}
static void guitar(void) {
    /* G, D/F#, Em7, Cadd9; a two-bar hook returns before the G cadence. */
    static const uint8_t root[BARS]={43,42,40,48,43,42,48,43};
    static const uint8_t shape[BARS][3]={
        {59,62,67},{57,62,66},{55,59,62},{55,62,64},
        {59,62,67},{57,62,66},{55,62,64},{59,62,67}
    };
    static const uint8_t hook[BARS][8]={
        {74,0,71,69,0,71,74,0},{73,0,69,66,0,69,0,0},
        {74,0,71,67,0,71,76,0},{74,0,72,71,0,67,0,0},
        {74,0,71,69,0,71,79,0},{78,0,74,73,0,69,0,0},
        {76,0,74,72,0,71,69,0},{67,0,0,0,0,0,0,0}
    };
    static const unsigned pick[4]={0,2,1,2};
    for(unsigned bar=0;bar<BARS;bar++) {
        unsigned start=bar*BAR;
        add(start,2,root[bar],100,bar==7?220:58);
        if(bar==7) {
            for(unsigned c=0;c<3;c++)add(start+4+c*4,1,shape[bar][c],89-c*5,210);
        } else {
            for(unsigned n=0;n<8;n++) {
                if(n==3||n==7) {
                    /* Short three-string upstrokes between the picked notes. */
                    for(unsigned c=0;c<3;c++)
                        add(start+n*48+c*3,1,shape[bar][2-c],65+c*4,22);
                } else add(start+n*48,1,shape[bar][pick[n%4]],78+n%3*5,30);
            }
            add(start+192,2,root[bar]+12,78,52);
        }
        for(unsigned n=0;n<8;n++)if(hook[bar][n])
            add(start+n*48+8,0,hook[bar][n],n?103:112,bar==7?230:32);
    }
}
static void strings(void) {
    /* Four moving lines with open spacing, a rising answer and a soft D cadence. */
    static const uint8_t lower[BARS][3]={
        {50,57,65},{46,58,65},{43,58,62},{45,57,64},
        {50,57,65},{46,58,62},{45,55,64},{50,57,65}
    };
    static const uint8_t upper[BARS][2]={
        {69,72},{70,69},{67,65},{64,69},
        {74,77},{74,70},{73,69},{74,0}
    };
    for(unsigned bar=0;bar<BARS;bar++) {
        unsigned start=bar*BAR;
        unsigned velocity=bar<4?78+bar*3:92-(bar-4)*6;
        add(start,0,lower[bar][0],velocity,bar==7?280:286);
        add(start+5,1,lower[bar][1],velocity-8,bar==7?275:280);
        add(start+9,1,lower[bar][2],velocity-13,bar==7?270:276);
        add(start+12,2,upper[bar][0],velocity+4,bar==7?280:130);
        if(upper[bar][1])add(start+204,2,upper[bar][1],velocity,124);
    }
}
static void u16(FILE *f,unsigned v) {
    fputc((int)(v&255),f);fputc((int)((v>>8)&255),f);
}
static void u32(FILE *f,unsigned v) { u16(f,v);u16(f,v>>16); }
static int render(unsigned kind,const char *directory) {
    char path[1024];
    int16_t audio[256*2];
    tempo=kind==0?86u:kind==1?110u:90u;
    count=0;
    if(kind==0)piano();else if(kind==1)guitar();else strings();
    qsort(events,count,sizeof events[0],compare);
    const unsigned frames=frame_at(BARS*BAR)+MC_RATE*5u;
    int written=snprintf(path,sizeof path,"%s/%s",directory,names[kind]);
    if(written<0||(unsigned)written>=sizeof path)return 1;
    FILE *f=fopen(path,"wb");
    if(!f) { perror(path);return 1; }
    fwrite("RIFF",1,4,f);u32(f,36+frames*4);fwrite("WAVEfmt ",1,8,f);u32(f,16);
    u16(f,1);u16(f,2);u32(f,MC_RATE);u32(f,MC_RATE*4);u16(f,4);u16(f,16);
    fwrite("data",1,4,f);u32(f,frames*4);
    mc_init(&e);
    memset(e.project.fx_send,0,sizeof e.project.fx_send);
    memset(e.project.track_fx,0,sizeof e.project.track_fx);
    memset(e.project.fx,0,sizeof e.project.fx);
    memset(e.project.analog,0,sizeof e.project.analog);
    memset(e.project.arp,0,sizeof e.project.arp);
    memset(e.project.track_filter,0,sizeof e.project.track_filter);
    memset(e.project.fx_bus_type,0,sizeof e.project.fx_bus_type);
    e.project.bpm=(uint16_t)tempo;
    e.project.metronome=0;
    e.project.fx_bus_type[0]=(uint8_t)(kind==2?3:1); /* HALL1 or ROOM2. */
    for(unsigned part=0;part<3;part++) {
        unsigned preset=kind==0?MC_PRESET_PIANO_BRIGHT:MC_PRESET_NYLON_PLUCK;
        if(kind==2) {
            static const unsigned ensemble[]={
                MC_PRESET_CELLO_ENSEMBLE,MC_PRESET_VIOLA_CHAMBER,MC_PRESET_VIOLIN_SILVER
            };
            preset=ensemble[part];
        }
        mc_preset(&e,part,preset);
        e.project.level[part]=(uint8_t)(part==0?92:part==1?73:80);
        e.project.pan[part]=(uint8_t)(kind==2?48+part*16:part==1?53:part==2?73:64);
        e.project.fx_send[part][0]=(uint8_t)(kind==2?37:kind==0?26:18);
    }
    e.project.level[3]=50;
    e.project.pan[3]=64;
    e.project.drum_kit=0;
    unsigned next=0,max_voices=0,tail_peak=0,pcm_peak=0;
    for(unsigned at=0;at<frames;) {
        while(next<count&&events[next].frame==at) {
            const Event *v=&events[next++];
            if(v->part==3)mc_drum(&e,v->note,v->velocity);
            else mc_note(&e,v->part,v->note,v->velocity,0,v->gate);
            unsigned active=0;
            for(unsigned i=0;i<MC_VOICES;i++)active+=e.voice[i].active!=0;
            if(active>max_voices)max_voices=active;
        }
        unsigned boundary=next<count?events[next].frame:frames;
        unsigned n=boundary-at;
        if(n>256)n=256;
        mc_render(&e,audio,n);
        for(unsigned i=0;i<n*2;i++) {
            unsigned magnitude=(unsigned)(audio[i]<0?-(int)audio[i]:audio[i]);
            if(magnitude>pcm_peak)pcm_peak=magnitude;
            if(at+i/2>=frames-MC_RATE/2u&&magnitude>tail_peak)tail_peak=magnitude;
            u16(f,(uint16_t)audio[i]);
        }
        at+=n;
    }
    int failed=ferror(f);
    if(fclose(f))failed=1;
    printf("%s: bpm=%u frames=%u seconds=%.6f peak=%u pcm_peak=%u clips=%u "
        "steals=%u max_voices=%u limiter_hits=%u tail_peak=%u\n",
        path,tempo,frames,(double)frames/MC_RATE,e.peak,pcm_peak,e.clips,e.steals,
        max_voices,e.limiter_hits,tail_peak);
    if(failed||e.clips||e.steals||!pcm_peak||tail_peak>8) {
        fprintf(stderr,"Render failed: %s (I/O, clips, steals, silence or unfinished tail)\n",path);
        return 1;
    }
    return 0;
}
int main(int argc,char **argv) {
    if(argc!=2) {
        fputs("usage: render-release-samples OUTPUT_DIRECTORY\n",stderr);
        return 1;
    }
    int failed=0;
    for(unsigned kind=0;kind<3;kind++)failed|=render(kind,argv[1]);
    return failed;
}
