/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "ui.h"
const char *const mc_drum_names[16]={"KICK","SNARE","CLOSED HAT","OPEN HAT","CLAP","LOW TOM","KICK 2","RIMSHOT","SNARE 2","HIGH TOM","CRASH","RIDE","SHAKER","COWBELL","CLAVES","MID TOM"};
static const char *const arp_names[6]={"UP","DOWN","UP-DOWN","ORDER","RANDOM","CHORD"};
static const char *const fx_names[4][4]={{"DIGITAL","TAPE","PINGPONG","MULTITAP"},{"ROOM","HALL","PLATE","SPRING"},{"SOFT","HARD","FOLD","FUZZ"},{"CHORUS","PHASER","FLANGER",""}};
static const char *const labels[16]={"BD","SN","CH","OH","CP","LT","B2","RS","S2","HT","CR","RD","SH","CB","CL","MT"};
static int limit(int v,int lo,int hi) { return v<lo?lo:v>hi?hi:v; }
static unsigned wrap(int v,unsigned n) { while(v<0) v+=(int)n; return (unsigned)v%n; }
static unsigned preset_match(const McPatch *patch) {
    for(unsigned pr=0;pr<MC_PRESETS;pr++) {
        unsigned same=1;
        for(unsigned j=0;j<sizeof *patch;j++) if(((const uint8_t *)patch)[j]!=((const uint8_t *)&mc_presets[pr])[j]) same=0;
        if(same)return pr;
    }
    return MC_PRESETS;
}
static void input_reset(McUi *u) { u->preset_popup=u->encoder_active=0; }
void mc_ui_tick(McUi *u,uint32_t now_ms) {
    if(u->preset_popup&&now_ms-u->preset_at>=500u)u->preset_popup=0;
}
void mc_ui_init(McUi *u) {
    for(unsigned i=0;i<sizeof *u;i++)((uint8_t *)u)[i]=0;
    u->page=u->sound=u->editing=u->save_pending=u->save_request=u->load_request=u->fx_master=0;u->demo_choice=1; u->octave=0; u->message=0;
    u->preset[0]=MC_PRESET_ACID_SAW;u->preset[1]=MC_PRESET_SILK_PAD;u->preset[2]=MC_PRESET_GLASS_BELL;
}
void mc_ui_button(McEngine *e,McUi *u,unsigned button) {
    if(button>UI_OCTUP)return;
    input_reset(u);
    if(button!=UI_SAVE) { u->save_pending=0; u->message=0; }
    switch(button) {
    case UI_PLAY: if(e->playing) mc_stop(e); else mc_start(e); break;
    case UI_REC: e->recording^=1; break;
    case UI_ARP: if(u->page==5)mc_mute(e,e->selected);else { u->page=u->page==6?0:6;u->editing=0; } break;
    case UI_SEL: mc_arp_clear(e,e->selected);e->selected=(uint8_t)((e->selected+1)%4); break;
    case UI_HOME: u->page=0;u->editing=0; break;
    case UI_SEQ: u->page=0;u->editing^=1; break;
    case UI_EDIT: if(u->page==7)mc_automation_clear(e,e->pattern);else u->editing^=1;break;
    case UI_ENV: if(u->page==3)u->fx_page=(uint8_t)((u->fx_page+1)%3);else u->page=u->page==1?0:1; break;
    case UI_GLO: u->page=u->page==5?2:u->page==2?4:u->page==4?7:5; break;
    case UI_LFO:
        if(u->page==6) { if(e->selected<3) { e->project.arp[e->selected].latch^=1;mc_arp_clear(e,e->selected); } }
        else { e->project.step[e->pattern][e->selected][e->edit_step].flags^=MC_ACCENT; }
        break;
    case UI_FX:
        if(u->editing) e->project.step[e->pattern][e->selected][e->edit_step].flags^=MC_SLIDE;
        else { if(u->page!=3)u->fx_page=0;u->fx_master=u->page==3?(uint8_t)!u->fx_master:0;u->page=3; } break;
    case UI_OCTDN: u->octave=(int8_t)limit(u->octave-1,-3,3); break;
    case UI_OCTUP: u->octave=(int8_t)limit(u->octave+1,-3,3); break;
    case UI_SAVE:
        if(e->playing) { u->message="STOP BEFORE SAVE";break; }
        if(u->save_pending) { u->save_request=1;u->save_pending=0; }
        else { u->save_pending=1;u->message="SAVE AGAIN: SLOT 0"; } break;
    default: break;
    }
}
static void parameter_delta(McEngine *e,McUi *u,unsigned part,unsigned parameter,int delta) {
    int value=mc_parameter_get(e,part,parameter);
    if(value>=0&&mc_parameter_set(e,part,parameter,(unsigned)limit(value+delta,0,127))>0)
        u->message="AUTOMATION FULL";
}
static void encoder_apply(McEngine *e,McUi *u,unsigned enc,int delta) {
    if(!delta||enc>6) return;
    u->message=0;u->save_pending=0;
    unsigned p=e->selected;
    if(enc==0) { mc_arp_clear(e,p);e->selected=(uint8_t)wrap((int)p+delta,4); return; }
    if(enc==2) {
        if(p==3) e->project.drum_kit=(uint8_t)wrap(e->project.drum_kit+delta,MC_KITS);
        else {
            unsigned current=e->project.preset[p];
            if(current>=MC_PRESETS)current=preset_match(&e->project.patch[p]);
            unsigned preset=wrap((int)(current<MC_PRESETS?current:u->preset[p])+delta,MC_PRESETS);
            u->preset[p]=(uint8_t)preset; mc_preset(e,p,preset);
        } return;
    }
    if(enc==1) { mc_queue(e,wrap((e->queued<4?e->queued:e->pattern)+delta,4)); return; }
    unsigned k=enc-3;
    if(u->page==6) {
        if(p==3)return;
        McArp *arp=&e->project.arp[p];
        if(k==0) { unsigned mode=wrap((arp->enabled?(int)arp->mode+1:0)+delta,7);mc_arp_clear(e,p);arp->enabled=mode!=0;if(mode)arp->mode=(uint8_t)(mode-1); }
        if(k==1)arp->rate=(uint8_t)wrap(arp->rate+delta,3);
        if(k==2)arp->octaves=(uint8_t)(1+wrap((int)arp->octaves-1+delta,3));
        if(k==3)arp->gate=(uint8_t)limit(arp->gate+delta,1,100);
    } else if(u->page==7) {
        return;
    } else if(u->page==5) {
        parameter_delta(e,u,k,MC_PARAM_LEVEL,delta);
    } else if(u->page==4) {
        if(k==0) u->demo_choice=(uint8_t)wrap(u->demo_choice+delta,2);
        if(k==3&&delta>0) {
            if(e->playing) u->message="STOP BEFORE LOAD";
            else { u->load_request=(uint8_t)(u->demo_choice+1);u->message="LOADING"; }
        }
    } else if(u->page==3) {
        unsigned part=u->fx_master?4:p;
        if(u->fx_page==0)parameter_delta(e,u,part,MC_PARAM_DELAY+k,delta);
        else if(u->fx_page==1) {
            unsigned param=MC_PARAM_DELAY_TYPE+k;
            unsigned value=wrap(mc_parameter_get(e,part,param)+delta,k==3?3:4);
            if(mc_parameter_set(e,part,param,value)>0)u->message="AUTOMATION FULL";
        } else if(k<2)parameter_delta(e,u,part,MC_PARAM_HPF+k,delta);
    } else if(u->page==2) {
        if(k==0) e->project.bpm=(uint16_t)limit(e->project.bpm+delta,40,240);
        if(k==1) e->project.swing=(uint8_t)limit(e->project.swing+delta,0,45);
        if(k==2) mc_queue(e,wrap((e->queued<4?e->queued:e->pattern)+delta,4));
        if(k==3)parameter_delta(e,u,p,MC_PARAM_LEVEL,delta);
    } else if(p==3) {
        if(k==0) u->sound=(uint8_t)wrap(u->sound+delta,16);
        if(k==1) e->edit_step=(uint8_t)wrap(e->edit_step+delta,16);
        if(k==2) {
            McStep *s=&e->project.step[e->pattern][3][e->edit_step];
            if(delta>0) { s->drums|=(uint16_t)(1u<<u->sound); if(!s->velocity) s->velocity=100; }
            else s->drums&=(uint16_t)~(1u<<u->sound);
        }
        if(k==3)parameter_delta(e,u,3,MC_PARAM_DRUM_LEVEL+u->sound,delta);
    } else if(u->page==1) {
        parameter_delta(e,u,p,6+k,delta);
    } else if(u->editing) {
        McStep *s=&e->project.step[e->pattern][p][e->edit_step];
        if(k==0) e->edit_step=(uint8_t)wrap(e->edit_step+delta,16);
        if(k==1) s->velocity=(uint8_t)limit(s->velocity+delta,0,127);
        if(k==2) s->gate=(uint8_t)limit(s->gate+delta,1,100);
        if(k==3) s->flags=(uint8_t)wrap(s->flags+delta,4);
    } else {
        unsigned param=k==0?4:k==1?5:k==2?7:e->project.patch[p].engine==MC_ACID?10:3;
        parameter_delta(e,u,p,param,delta);
    }
}
void mc_ui_encoder(McEngine *e,McUi *u,unsigned enc,int delta) {
    if(!delta||enc>6)return;
    input_reset(u);encoder_apply(e,u,enc,delta);
}
static unsigned encoder_range(const McEngine *e,const McUi *u,unsigned enc) {
    if(enc==0||enc==1)return 4;
    if(enc==2)return e->selected==3?MC_KITS:MC_PRESETS;
    unsigned k=enc-3;
    if(u->page==6)return e->selected==3?0:k==0?7:k<3?3:100;
    if(u->page==7)return 0;
    if(u->page==3)return u->fx_page==0?128:u->fx_page==1?(k==3?3:4):k<2?128:0;
    if(u->page==5)return 128;
    if(u->page==4)return 2;
    if(u->page==2)return k==0?201:k==1?46:k==2?4:128;
    if(e->selected==3)return k<2?16:k==2?2:128;
    if(u->page==1)return 128;
    if(u->editing)return k==0?16:k==1?128:k==2?100:4;
    return 128;
}
void mc_ui_encoder_at(McEngine *e,McUi *u,unsigned enc,int delta,uint32_t now_ms) {
    if(!delta||enc>6)return;
    uint32_t context=e->selected|((uint32_t)u->page<<2)|((uint32_t)u->editing<<5)|((uint32_t)u->fx_master<<6)|((uint32_t)u->fx_page<<18);
    if(enc>=3) {
        if(e->selected==3)context|=(uint32_t)u->sound<<7;
        else context|=(uint32_t)e->project.patch[e->selected].engine<<11;
        if(u->editing||e->selected==3)context|=((uint32_t)e->edit_step<<12)|((uint32_t)e->pattern<<16);
    }
    int direction=delta>0?1:-1;
    unsigned range=encoder_range(e,u,enc),factor=1;
    if(u->encoder_active&&u->encoder_last==enc&&u->encoder_context==context&&
       u->encoder_direction==direction&&now_ms-u->encoder_at<60u&&range>40)
        factor=range>150?6u:3u;
    delta=limit(delta,-127,127)*(int)factor;
    u->encoder_active=1;u->encoder_last=(uint8_t)enc;u->encoder_direction=(int8_t)direction;
    u->encoder_at=now_ms;u->encoder_context=context;
    u->preset_popup=enc==2;
    if(u->preset_popup)u->preset_at=now_ms;
    encoder_apply(e,u,enc,delta);
}
void mc_ui_key(McEngine *e,McUi *u,unsigned key,int down) {
    if(key>26) return;
    input_reset(u);
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
    if(!down) { mc_key(e,p,note,100,0);return; }
    mc_key(e,p,note,100,1);
    if(e->project.arp[p].enabled)return;
    if(u->editing||e->recording) {
        unsigned step=e->recording&&e->playing?e->step:e->edit_step;
        McStep *s=&e->project.step[e->pattern][p][step];
        s->note[0]=(uint8_t)note;s->note[1]=s->note[2]=0;s->velocity=100;s->gate=75;s->flags&=(uint8_t)~MC_TIE;
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
{0,0,0,31,0,0,0},{1,2,2,4,8,8,16},{0,4,4,0,4,4,0},{0,4,4,31,4,4,0},
{0,0,14,1,15,17,15},{16,16,22,25,17,17,30},{0,0,14,16,16,17,14},{1,1,13,19,17,17,15},
{0,0,14,17,31,16,14},{6,9,8,28,8,8,8},{0,0,15,17,15,1,14},{16,16,22,25,17,17,17},
{4,0,12,4,4,4,14},{2,0,6,2,2,18,12},{16,16,18,20,24,20,18},{12,4,4,4,4,4,14},
{0,0,26,21,21,21,21},{0,0,22,25,17,17,17},{0,0,14,17,17,17,14},{0,0,30,17,30,16,16},
{0,0,15,17,15,1,1},{0,0,22,25,16,16,16},{0,0,15,16,14,1,30},{8,8,28,8,8,9,6},
{0,0,17,17,17,19,13},{0,0,17,17,17,10,4},{0,0,17,17,21,21,10},{0,0,17,10,4,10,17},
{0,0,17,17,15,1,14},{0,0,31,2,4,8,31},{0,21,14,31,14,21,0}
};
#define BG 0x0841u
#define DIM 0x4208u
#define GRID 0x2104u
#define WHITE 0xdefbu
#define ORANGE 0xfb20u
#define HIT 0x9a00u
#define GREEN 0x67e8u
typedef struct { unsigned y,h;uint16_t *p; } Canvas;
static void rect(Canvas *c,int x,int y,int w,int h,uint16_t color) {
    for(int py=y;py<y+h;py++) if(py>=(int)c->y&&py<(int)(c->y+c->h)&&py<240)
        for(int px=x;px<x+w;px++) if(px>=0&&px<240) c->p[(unsigned)(py-(int)c->y)*240+(unsigned)px]=color;
}
static void text(Canvas *c,int x,int y,const char *s,uint16_t color,int scale) {
    while(*s) {
        unsigned ch=(unsigned char)*s++; int index=ch>='0'&&ch<='9'?(int)ch-'0':ch>='A'&&ch<='Z'?(int)ch-'A'+10:ch>='a'&&ch<='z'?(int)ch-'a'+40:ch=='-'?36:ch=='/'?37:ch==':'?38:ch=='+'?39:ch=='*'?66:-1;
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
static void preset_popup(Canvas *c,const McEngine *e,const McUi *u) {
    unsigned drums=e->selected==3,total=drums?MC_KITS:MC_PRESETS;
    unsigned selected=drums?e->project.drum_kit:u->preset[e->selected];
    rect(c,4,22,232,214,GRID);rect(c,6,24,228,210,BG);
    text(c,12,30,drums?"DRUM KITS":"PRESETS",WHITE,2);
    text(c,12,50,drums?"DRUMS":mc_preset_categories[selected],ORANGE,1);
    number(c,188,50,selected+1,WHITE);text(c,207,50,drums?"/8":"/128",DIM,1);
    for(unsigned row=0;row<7;row++) {
        unsigned index=wrap((int)selected+(int)row-3,total);
        int y=66+(int)row*23;uint16_t color=row==3?BG:WHITE;
        rect(c,10,y,220,21,row==3?ORANGE:GRID);
        number(c,15,y+7,index+1,color);
        text(c,43,y+7,drums?mc_kit_names[index]:mc_preset_names[index],color,1);
    }
}
void mc_ui_strip(const McEngine *e,const McUi *u,unsigned y,unsigned height,uint16_t *pixels) {
    if(y>=240||height>8||y+height>240) return;
    Canvas c={y,height,pixels}; rect(&c,0,(int)y,240,(int)height,BG);
    unsigned p=e->selected;
    text(&c,3,4,p==3?"DRUMS":"MC-1",ORANGE,1);
    text(&c,63,4,p==3?mc_drum_names[u->sound]:e->project.patch[p].engine?"303":e->project.patch[p].pcm>=MC_PCM_ATTACKS?"PCM":"LA",WHITE,1);
    number(&c,163,4,e->pattern+1,WHITE);text(&c,169,4,"/4",DIM,1);
    text(&c,191,4,e->playing?"+":"-",e->playing?GREEN:WHITE,1);number(&c,211,4,e->project.bpm,WHITE);
    if(e->queued<4) { text(&c,163,14,"NEXT",ORANGE,1);number(&c,196,14,e->queued+1,ORANGE); }
    if(p==3) text(&c,3,14,mc_kit_names[e->project.drum_kit],WHITE,1);
    if(u->preset_popup) { preset_popup(&c,e,u);return; }
    if(u->page==5) {
        static const char *const names[4]={"PART1","PART2","PART3","DRUMS"};
        text(&c,14,30,"GLOBAL MIXER",WHITE,2);
        for(unsigned part=0;part<4;part++) {
            int x=part?25+(int)part*60:26;
            uint16_t color=e->mute&(1u<<part)?DIM:ORANGE;
            text(&c,x-17,61,names[part],part==p?ORANGE:WHITE,1);
            rect(&c,x-8,79,18,64,GRID);
            int level=(int)e->project.level[part]*64/127;
            rect(&c,x-8,143-level,18,level,color);
            if(e->mute&(1u<<part)) text(&c,x-11,153,"MUTE",DIM,1);
            else text(&c,x-5,153,"ON",WHITE,1);
            knob(&c,x,names[part],e->project.level[part],color);
        }
        if(!u->message) text(&c,66,173,"SELECT-ARP to mute",WHITE,1);
    } else if(u->page==4) {
        text(&c,14,40,"DEMO LIBRARY",WHITE,2);
        text(&c,14,77,u->demo_choice?"NEON SPIRAL":"NIGHT CIRCUIT",ORANGE,2);
        text(&c,14,108,"STOP THEN TURN LOAD",WHITE,1);
        text(&c,14,126,"REPLACES CURRENT PROJECT",DIM,1);
        text(&c,14,146,"PLAY STARTS THE DEMO",WHITE,1);
        knob_scaled(&c,26,"SONG",u->demo_choice+1,2,0x3317);
        knob_scaled(&c,205,"LOAD",u->load_request?1:0,1,HIT);
    } else if(u->page==3) {
        const uint8_t *fx=u->fx_master?e->project.fx:e->project.track_fx[p];
        const uint8_t *types=u->fx_master?e->project.fx_type:e->project.track_fx_type[p];
        const uint8_t *filter=u->fx_master?e->project.filter:e->project.track_filter[p];
        static const char *const names[4]={"DELAY","REVERB","DRIVE","MOD"};
        text(&c,14,30,u->fx_master?"MASTER FX":p==3?"DRUMS FX":"PART FX",WHITE,2);
        if(!u->fx_master&&p<3)number(&c,116,35,p+1,ORANGE);
        text(&c,14,55,u->fx_page==0?"AMOUNTS":u->fx_page==1?"TYPES":"FILTERS",ORANGE,1);
        if(u->fx_page==2) {
            text(&c,14,81,"HPF: CUT LOW FREQUENCIES",WHITE,1);
            text(&c,14,105,"LPF: CUT HIGH FREQUENCIES",WHITE,1);
            text(&c,14,132,"ZERO IS BYPASS",DIM,1);
            knob(&c,26,"HPF",filter[0],0x3317);knob(&c,85,"LPF",filter[1],0x3d48);
        } else {
            for(unsigned k=0;k<4;k++) {
                text(&c,14,78+(int)k*21,names[k],DIM,1);
                text(&c,74,78+(int)k*21,fx_names[k][types[k]],WHITE,1);
                if(u->fx_page==0)number(&c,190,78+(int)k*21,fx[k],ORANGE);
                knob_scaled(&c,k?25+(int)k*60:26,names[k],u->fx_page?types[k]+1:fx[k],u->fx_page?(k==3?3:4):127,k==0?0x3317:k==1?0x3d48:k==2?DIM:HIT);
            }
        }
        text(&c,14,157,u->fx_master?"FX: SELECTED TRACK":"FX: MASTER EFFECTS",DIM,1);
        text(&c,14,173,u->fx_page==0?"ENV: TYPES":u->fx_page==1?"ENV: FILTERS":"ENV: AMOUNTS",WHITE,1);
    } else if(u->page==6) {
        text(&c,14,32,"ARPEGGIATOR",WHITE,2);
        if(p==3)text(&c,14,88,"MELODIC PARTS ONLY",ORANGE,1);
        else {
            const McArp *arp=&e->project.arp[p];
            static const char *const rates[3]={"1/4","1/8","1/16"};
            text(&c,14,66,arp->enabled?arp_names[arp->mode]:"OFF",arp->enabled?GREEN:DIM,2);
            text(&c,14,101,"RATE",DIM,1);text(&c,50,101,rates[arp->rate],WHITE,1);
            text(&c,106,101,"PART",DIM,1);number(&c,136,101,p+1,WHITE);
            text(&c,14,128,"HOLD KEYS TO PLAY",DIM,1);
            text(&c,14,151,arp->latch?"LFO: LATCH ON":"LFO: LATCH OFF",arp->latch?GREEN:WHITE,1);
            text(&c,14,173,"PLAY + REC: RECORD ARP",DIM,1);
            knob_scaled(&c,26,"MODE",arp->enabled?arp->mode+1:0,6,0x3317);
            knob_scaled(&c,85,"RATE",arp->rate+1,3,0x3d48);
            knob_scaled(&c,145,"OCT",arp->octaves,3,DIM);
            knob_scaled(&c,205,"GATE",arp->gate,100,HIT);
        }
    } else if(u->page==7) {
        unsigned count=0;for(unsigned i=0;i<e->project.automation_count;i++)if(e->project.automation[i].address/5==e->pattern)count++;
        text(&c,14,32,"KNOB AUTOMATION",WHITE,2);
        text(&c,14,76,"EVENTS",DIM,1);number(&c,68,76,e->project.automation_count,ORANGE);text(&c,92,76,"/384",DIM,1);
        text(&c,14,100,"PATTERN",DIM,1);number(&c,68,100,e->pattern+1,WHITE);
        number(&c,92,100,count,ORANGE);text(&c,116,100,"EVENTS",DIM,1);
        text(&c,14,126,e->recording?"REC ON":"REC OFF",e->recording?ORANGE:WHITE,1);
        text(&c,14,149,e->automation_full?"AUTOMATION FULL":"PLAY + REC THEN TURN KNOBS",DIM,1);
        text(&c,14,173,"EDIT: CLEAR PATTERN",WHITE,1);
    } else if(u->page==2) {
        text(&c,14,42,"GROOVE CONTROL",WHITE,2);
        text(&c,14,75,"FOUR PATTERNS / FOUR PARTS",DIM,1);
        text(&c,14,99,"ALGORITHM: QUEUE PATTERN",WHITE,1);
        text(&c,14,116,"ARP: ARPEGGIATOR PAGE",WHITE,1);
        text(&c,14,140,"HOST AND HARDWARE CORE",DIM,1);
        knob_scaled(&c,26,"TEMPO",e->project.bpm,240,0x3317);knob(&c,85,"SWING",e->project.swing,0x3d48);
        knob(&c,145,"PATT",e->pattern+1,DIM);knob(&c,205,"LEVEL",e->project.level[p],HIT);
    } else if(p==3) {
        for(unsigned row=0;row<16;row++) {
            int yy=24+(int)row*9;
            if(row==u->sound) rect(&c,0,yy,23,8,ORANGE);
            text(&c,2,yy+1,labels[row],row==u->sound?BG:DIM,1);
            for(unsigned s=0;s<16;s++) {
                const McStep *st=&e->project.step[e->pattern][3][s];
                int x=29+(int)s*13;
                uint16_t color=st->drums&(1u<<row)?(row==u->sound?ORANGE:HIT):GRID;
                if(e->playing&&s==e->step&&st->velocity&&(st->drums&(1u<<row))&&!(e->mute&8))color=GREEN;
                rect(&c,x,yy,11,8,color);
                if(row==u->sound&&s==e->edit_step) { rect(&c,x,yy,11,1,WHITE);rect(&c,x,yy+7,11,1,WHITE); }
            }
        }
        knob(&c,26,"SOUND",u->sound+1,0x3317);knob(&c,85,"STEP",e->edit_step+1,0x3d48);
        knob(&c,145,"HIT",(e->project.step[e->pattern][3][e->edit_step].drums>>u->sound)&1,DIM);
        knob(&c,205,"LEVEL",e->project.drum_level[u->sound],HIT);
    } else {
        unsigned match=e->project.preset[p];
        char title[20];const char *name=match<MC_PRESETS?mc_preset_names[match]:"IMPORTED SOUND";unsigned n=0,changed=0;
        while(name[n]&&n<18) { title[n]=name[n];n++; }
        if(match<MC_PRESETS)for(unsigned i=0;i<sizeof(McPatch);i++)
            if(((const uint8_t *)&e->project.patch[p])[i]!=((const uint8_t *)&mc_presets[match])[i])changed=1;
        if(changed)title[n++]='*';
        title[n]=0;
        text(&c,8,30,title,WHITE,2);
        if(match<MC_PRESETS) {
            text(&c,8,49,mc_preset_categories[match],DIM,1);
            number(&c,188,49,match+1,ORANGE);text(&c,207,49,"/128",DIM,1);
        }
        for(unsigned t=0;t<4;t++) {
            int yy=59+(int)t*27;
            unsigned low=108,high=12;
            if(t<3) for(unsigned pattern=0;pattern<4;pattern++) for(unsigned step=0;step<16;step++) {
                const McStep *st=&e->project.step[pattern][t][step];
                if(st->velocity) for(unsigned note=0;note<3;note++) if(st->note[note]) {
                    if(st->note[note]<low) low=st->note[note];
                    if(st->note[note]>high) high=st->note[note];
                }
            }
            number(&c,4,yy+5,t+1,t==p?ORANGE:DIM);
            if((e->mute>>t)&1) text(&c,12,yy+5,"M",HIT,1);
            for(unsigned s=0;s<16;s++) {
                const McStep *st=&e->project.step[e->pattern][t][s];
                uint16_t color=(t==3?st->velocity&&st->drums!=0:st->velocity&&(st->note[0]||st->note[1]||st->note[2]))?(t==p?ORANGE:HIT):GRID;
                if(e->playing&&s==e->step&&color!=GRID&&!(e->mute&(1u<<t)))color=GREEN;
                rect(&c,29+(int)s*13,yy,11,19,color);
                if(t<3&&st->velocity) for(unsigned note=0;note<3;note++) if(st->note[note]) {
                    int pitch=high>low?(int)((st->note[note]-low)*15/(high-low)):7;
                    rect(&c,31+(int)s*13,yy+17-pitch,7,1,BG);
                }
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
    if(!u->message&&u->page<2) {
        text(&c,4,173,"PATTERN",DIM,1);number(&c,52,173,e->pattern+1,WHITE);text(&c,58,173,"/4",DIM,1);
        if(e->queued<4) { text(&c,94,173,"NEXT",DIM,1);number(&c,124,173,e->queued+1,ORANGE); }
    }
    if(u->message) { rect(&c,0,171,240,12,BG);text(&c,4,173,u->message,ORANGE,1); }
    else if(e->recording) text(&c,p==3?112:3,14,"REC",ORANGE,1);
    else if(u->editing) text(&c,p==3?112:3,14,"EDIT",WHITE,1);
}
