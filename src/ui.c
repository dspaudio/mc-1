/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "ui.h"
const char *const mc_drum_names[16]={"KICK","SNARE","CLOSED HAT","OPEN HAT","CLAP","LOW TOM","KICK 2","RIMSHOT","SNARE 2","HIGH TOM","CRASH","RIDE","SHAKER","COWBELL","CLAVES","MID TOM"};
static const char *const labels[16]={"BD","SN","CH","OH","CP","LT","B2","RS","S2","HT","CR","RD","SH","CB","CL","MT"};
static int limit(int v,int lo,int hi) { return v<lo?lo:v>hi?hi:v; }
static unsigned wrap(int v,unsigned n) { while(v<0) v+=(int)n; return (unsigned)v%n; }
void mc_ui_init(McUi *u) {
    u->page=u->sound=u->editing=u->save_pending=u->save_request=u->load_request=0;u->demo_choice=1; u->octave=0; u->message=0;
    u->preset[0]=6;u->preset[1]=2;u->preset[2]=3;
}
void mc_ui_button(McEngine *e,McUi *u,unsigned button) {
    if(button!=UI_SAVE) { u->save_pending=0; u->message=0; }
    switch(button) {
    case UI_PLAY: if(e->playing) mc_stop(e); else mc_start(e); break;
    case UI_REC: e->recording^=1; break;
    case UI_ARP: mc_mute(e,e->selected); break;
    case UI_SEL: e->selected=(uint8_t)((e->selected+1)%4); break;
    case UI_HOME: u->page=0;u->editing=0; break;
    case UI_SEQ: u->page=0;u->editing^=1; break;
    case UI_EDIT: u->editing^=1;break;
    case UI_ENV: u->page=u->page==1?0:1; break;
    case UI_GLO: u->page=u->page==2?4:u->page==4?0:2; break;
    case UI_LFO: e->project.step[e->pattern][e->selected][e->edit_step].flags^=MC_ACCENT; break;
    case UI_FX: if(u->editing) e->project.step[e->pattern][e->selected][e->edit_step].flags^=MC_SLIDE; else u->page=u->page==3?0:3; break;
    case UI_OCTDN: u->octave=(int8_t)limit(u->octave-1,-3,3); break;
    case UI_OCTUP: u->octave=(int8_t)limit(u->octave+1,-3,3); break;
    case UI_SAVE:
        if(e->playing) { u->message="STOP BEFORE SAVE";break; }
        if(u->save_pending) { u->save_request=1;u->save_pending=0; }
        else { u->save_pending=1;u->message="SAVE AGAIN: SLOT 0"; } break;
    default: break;
    }
}
void mc_ui_encoder(McEngine *e,McUi *u,unsigned enc,int delta) {
    if(!delta||enc>6) return;
    u->message=0;u->save_pending=0;
    unsigned p=e->selected;
    if(enc==0) { e->selected=(uint8_t)wrap((int)p+delta,4); return; }
    if(enc==1) {
        if(p==3) mc_queue(e,wrap((e->queued<4?e->queued:e->pattern)+delta,4));
        else {
            unsigned preset=wrap(u->preset[p]+delta,MC_PRESETS);
            u->preset[p]=(uint8_t)preset; mc_preset(e,p,preset);
        } return;
    }
    if(enc==2) { mc_queue(e,wrap((e->queued<4?e->queued:e->pattern)+delta,4)); return; }
    unsigned k=enc-3;
    if(u->page==4) {
        if(k==0) u->demo_choice=(uint8_t)wrap(u->demo_choice+delta,2);
        if(k==3&&delta>0) {
            if(e->playing) u->message="STOP BEFORE LOAD";
            else { u->load_request=(uint8_t)(u->demo_choice+1);u->message="LOADING"; }
        }
    } else if(u->page==3) {
        e->project.fx[k]=(uint8_t)limit(e->project.fx[k]+delta,0,127);
    } else if(u->page==2) {
        if(k==0) e->project.bpm=(uint16_t)limit(e->project.bpm+delta,40,240);
        if(k==1) e->project.swing=(uint8_t)limit(e->project.swing+delta,0,45);
        if(k==2) mc_queue(e,wrap((e->queued<4?e->queued:e->pattern)+delta,4));
        if(k==3) e->project.level[p]=(uint8_t)limit(e->project.level[p]+delta,0,127);
    } else if(p==3) {
        if(k==0) u->sound=(uint8_t)wrap(u->sound+delta,16);
        if(k==1) e->edit_step=(uint8_t)wrap(e->edit_step+delta,16);
        if(k==2) {
            McStep *s=&e->project.step[e->pattern][3][e->edit_step];
            if(delta>0) { s->drums|=(uint16_t)(1u<<u->sound); if(!s->velocity) s->velocity=100; }
            else s->drums&=(uint16_t)~(1u<<u->sound);
        }
        if(k==3) e->project.drum_level[u->sound]=(uint8_t)limit(e->project.drum_level[u->sound]+delta,0,127);
    } else if(u->page==1) {
        McPatch *s=&e->project.patch[p]; uint8_t *v=k==0?&s->attack:k==1?&s->decay:k==2?&s->sustain:&s->release;
        *v=(uint8_t)limit(*v+delta,0,127);
    } else if(u->editing) {
        McStep *s=&e->project.step[e->pattern][p][e->edit_step];
        if(k==0) e->edit_step=(uint8_t)wrap(e->edit_step+delta,16);
        if(k==1) s->velocity=(uint8_t)limit(s->velocity+delta,0,127);
        if(k==2) s->gate=(uint8_t)limit(s->gate+delta,1,100);
        if(k==3) s->flags=(uint8_t)wrap(s->flags+delta,4);
    } else {
        if(k==3&&e->project.patch[p].engine==MC_ACID)
            e->project.patch[p].glide=(uint8_t)limit(e->project.patch[p].glide+delta,0,127);
        else mc_control(e,p,k,delta);
    }
}
void mc_ui_key(McEngine *e,McUi *u,unsigned key,int down) {
    if(key>26) return;
    unsigned p=e->selected;
    if(p==3) {
        if(!down) return;
        u->sound=(uint8_t)(key%16);
        if(u->editing||e->recording) {
            unsigned step=e->recording&&e->playing?e->step:e->edit_step;
            McStep *s=&e->project.step[e->pattern][3][step];
            s->drums^=(uint16_t)(1u<<u->sound); s->velocity=100;s->gate=60;
        }
        mc_drum(e,u->sound,100);return;
    }
    unsigned note=(unsigned)limit(53+(int)key+u->octave*12,12,108);
    if(!down) { mc_off(e,p,note);return; }
    mc_note(e,p,note,100,0,0);
    if(u->editing||e->recording) {
        unsigned step=e->recording&&e->playing?e->step:e->edit_step;
        McStep *s=&e->project.step[e->pattern][p][step];
        s->note[0]=(uint8_t)note;s->note[1]=s->note[2]=0;s->velocity=100;s->gate=75;
        if(!e->playing) e->edit_step=(uint8_t)((e->edit_step+1)%16);
    }
}
/* Original 5x7 bitmap alphabet: no upstream icons or proprietary panel art. */
static const uint8_t font[][7]={
{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},
{14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
{14,4,4,4,4,4,14},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
{17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,27,17},{17,17,10,4,10,17,17},
{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
{0,0,0,31,0,0,0},{1,2,2,4,8,8,16},{0,4,4,0,4,4,0},{0,4,4,31,4,4,0}
};
#define BG 0x0841u
#define DIM 0x4208u
#define GRID 0x2104u
#define WHITE 0xdefbu
#define ORANGE 0xfb20u
#define HIT 0x9a00u
typedef struct { unsigned y,h;uint16_t *p; } Canvas;
static void rect(Canvas *c,int x,int y,int w,int h,uint16_t color) {
    for(int py=y;py<y+h;py++) if(py>=(int)c->y&&py<(int)(c->y+c->h)&&py<240)
        for(int px=x;px<x+w;px++) if(px>=0&&px<240) c->p[(unsigned)(py-(int)c->y)*240+(unsigned)px]=color;
}
static void text(Canvas *c,int x,int y,const char *s,uint16_t color,int scale) {
    while(*s) {
        unsigned ch=(unsigned char)*s++; int index=ch>='0'&&ch<='9'?(int)ch-'0':ch>='A'&&ch<='Z'?(int)ch-'A'+10:ch=='-'?36:ch=='/'?37:ch==':'?38:ch=='+'?39:-1;
        if(index>=0) for(int row=0;row<7;row++) for(int col=0;col<5;col++)
            if(font[index][row]&(1<<(4-col))) rect(c,x+col*scale,y+row*scale,scale,scale,color);
        x+=6*scale;
    }
}
static void number(Canvas *c,int x,int y,unsigned n,uint16_t color) {
    char b[6];unsigned len=0;
    do { b[len++]=(char)('0'+n%10);n/=10; } while(n&&len<5);
    for(unsigned i=0;i<len/2;i++) { char t=b[i];b[i]=b[len-1-i];b[len-1-i]=t; } b[len]=0;
    text(c,x,y,b,color,1);
}
static void knob_scaled(Canvas *c,int x,const char *label,unsigned value,unsigned maximum,uint16_t color) {
    static const int8_t dx[9]={-8,-9,-7,-4,0,4,7,9,8},dy[9]={5,0,-5,-8,-9,-8,-5,0,5};
    for(unsigned i=0;i<9;i++) rect(c,x+dx[i],199+dy[i],3,3,color);
    unsigned pos=value*8/maximum;if(pos>8) pos=8;
    rect(c,x,200,2,2,WHITE);rect(c,x+dx[pos]/2,200+dy[pos]/2,2,2,WHITE);
    text(c,x-17,214,label,DIM,1);number(c,x-5,227,value,WHITE);
}
static void knob(Canvas *c,int x,const char *label,unsigned value,uint16_t color) {
    knob_scaled(c,x,label,value,127,color);
}
void mc_ui_strip(const McEngine *e,const McUi *u,unsigned y,unsigned height,uint16_t *pixels) {
    if(y>=240||height>8||y+height>240) return;
    Canvas c={y,height,pixels}; rect(&c,0,(int)y,240,(int)height,BG);
    unsigned p=e->selected;
    text(&c,3,4,p==3?"808":"MC-1",ORANGE,1);
    text(&c,63,4,p==3?mc_drum_names[u->sound]:e->project.patch[p].engine?"303":"LA",WHITE,1);
    number(&c,163,4,e->pattern+1,WHITE);text(&c,169,4,"/4",DIM,1);
    text(&c,191,4,e->playing?"+":"-",WHITE,1);number(&c,211,4,e->project.bpm,WHITE);
    if(e->queued<4) { text(&c,163,14,"NEXT",ORANGE,1);number(&c,196,14,e->queued+1,ORANGE); }
    if(u->page==4) {
        text(&c,14,40,"DEMO LIBRARY",WHITE,2);
        text(&c,14,77,u->demo_choice?"NEON SPIRAL":"NIGHT CIRCUIT",ORANGE,2);
        text(&c,14,108,"STOP THEN TURN LOAD",WHITE,1);
        text(&c,14,126,"REPLACES CURRENT PROJECT",DIM,1);
        text(&c,14,146,"PLAY STARTS THE DEMO",WHITE,1);
        knob_scaled(&c,26,"SONG",u->demo_choice+1,2,0x3317);
        knob_scaled(&c,205,"LOAD",u->load_request?1:0,1,HIT);
    } else if(u->page==3) {
        text(&c,14,42,"MASTER EFFECTS",WHITE,2);
        text(&c,14,78,"DELAY / REVERB",ORANGE,1);
        text(&c,14,98,"SOFT DRIVE / CHORUS",ORANGE,1);
        text(&c,14,124,"ZERO IS DRY",DIM,1);
        text(&c,14,146,"SAVE STORES FX SETTINGS",DIM,1);
        knob(&c,26,"DELAY",e->project.fx[0],0x3317);knob(&c,85,"REVERB",e->project.fx[1],0x3d48);
        knob(&c,145,"DRIVE",e->project.fx[2],DIM);knob(&c,205,"CHORUS",e->project.fx[3],HIT);
    } else if(u->page==2) {
        text(&c,14,42,"GROOVE CONTROL",WHITE,2);
        text(&c,14,75,"FOUR PATTERNS / FOUR PARTS",DIM,1);
        text(&c,14,99,"PRESETS: QUEUE PATTERN",WHITE,1);
        text(&c,14,116,"ARP: MUTE SELECTED PART",WHITE,1);
        text(&c,14,140,"HOST AND HARDWARE CORE",DIM,1);
        knob_scaled(&c,26,"TEMPO",e->project.bpm,240,0x3317);knob(&c,85,"SWING",e->project.swing,0x3d48);
        knob(&c,145,"PATT",e->pattern+1,DIM);knob(&c,205,"LEVEL",e->project.level[p],HIT);
    } else if(p==3) {
        for(unsigned row=0;row<16;row++) {
            int yy=24+(int)row*10;
            if(row==u->sound) rect(&c,0,yy,23,8,ORANGE);
            text(&c,2,yy+1,labels[row],row==u->sound?BG:DIM,1);
            for(unsigned s=0;s<16;s++) {
                const McStep *st=&e->project.step[e->pattern][3][s];
                int x=29+(int)s*13;
                uint16_t color=st->drums&(1u<<row)?(row==u->sound?ORANGE:HIT):GRID;
                if(e->playing&&s==e->step) color=st->drums&(1u<<row)?WHITE:DIM;
                rect(&c,x,yy,11,8,color);
                if(row==u->sound&&s==e->edit_step) { rect(&c,x,yy,11,1,WHITE);rect(&c,x,yy+7,11,1,WHITE); }
            }
        }
        knob(&c,26,"SOUND",u->sound+1,0x3317);knob(&c,85,"STEP",e->edit_step+1,0x3d48);
        knob(&c,145,"HIT",(e->project.step[e->pattern][3][e->edit_step].drums>>u->sound)&1,DIM);
        knob(&c,205,"LEVEL",e->project.drum_level[u->sound],HIT);
    } else {
        unsigned match=MC_PRESETS;
        for(unsigned pr=0;pr<MC_PRESETS;pr++) {
            unsigned same=1;
            for(unsigned j=0;j<12;j++) if(((const uint8_t *)&e->project.patch[p])[j]!=((const uint8_t *)&mc_presets[pr])[j]) same=0;
            if(same) { match=pr;break; }
        }
        text(&c,8,30,match<MC_PRESETS?mc_preset_names[match]:"CUSTOM SOUND",WHITE,2);
        for(unsigned t=0;t<4;t++) {
            int yy=59+(int)t*27;
            number(&c,4,yy+5,t+1,t==p?ORANGE:DIM);
            if((e->mute>>t)&1) text(&c,12,yy+5,"M",HIT,1);
            for(unsigned s=0;s<16;s++) {
                const McStep *st=&e->project.step[e->pattern][t][s];
                uint16_t color=(t==3?st->drums!=0:st->velocity&&st->note[0])?(t==p?ORANGE:HIT):GRID;
                if(e->playing&&s==e->step) color=WHITE;
                rect(&c,29+(int)s*13,yy,11,19,color);
                if(t==p&&s==e->edit_step) rect(&c,29+(int)s*13,yy+20,11,2,WHITE);
            }
        }
        const McPatch *s=&e->project.patch[p];
        if(u->page==1) { knob(&c,26,"ATK",s->attack,0x3317);knob(&c,85,"DEC",s->decay,0x3d48);knob(&c,145,"SUS",s->sustain,DIM);knob(&c,205,"REL",s->release,HIT); }
        else if(u->editing) {
            const McStep *st=&e->project.step[e->pattern][p][e->edit_step];
            knob(&c,26,"STEP",e->edit_step+1,0x3317);knob(&c,85,"VEL",st->velocity,0x3d48);knob(&c,145,"GATE",st->gate,DIM);knob(&c,205,"A/S",st->flags,HIT);
        } else { knob(&c,26,"CUT",s->cutoff,0x3317);knob(&c,85,"RES",s->resonance,0x3d48);knob(&c,145,"DEC",s->decay,DIM);knob(&c,205,s->engine?"GLIDE":"PCM",s->engine?s->glide:s->pcm_mix,HIT); }
    }
    if(u->message) { rect(&c,0,171,240,12,BG);text(&c,4,173,u->message,ORANGE,1); }
    else if(e->recording) text(&c,3,14,"REC",ORANGE,1);
    else if(u->editing) text(&c,3,14,"EDIT",WHITE,1);
}
