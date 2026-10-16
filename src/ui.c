/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "ui.h"
const char *const mc_drum_names[16]={"KICK","SNARE","CLOSED HAT","OPEN HAT","CLAP","LOW TOM","KICK 2","RIMSHOT","SNARE 2","HIGH TOM","CRASH","RIDE","SHAKER","COWBELL","CLAVES","MID TOM"};
static const char *const arp_names[6]={"UP","DOWN","UP-DOWN","ORDER","RANDOM","CHORD"};
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
    u->page=u->sound=u->editing=u->save_pending=u->save_request=u->load_request=0;u->demo_choice=1; u->octave=0; u->message=0;
    u->preset[0]=MC_PRESET_ACID_SAW;u->preset[1]=MC_PRESET_SILK_PAD;u->preset[2]=MC_PRESET_GLASS_BELL;
}
void mc_ui_button(McEngine *e,McUi *u,unsigned button) {
    if(button>UI_OCTUP)return;
    input_reset(u);
    if(button!=UI_SAVE) { u->save_pending=0; u->message=0; }
    switch(button) {
    case UI_PLAY: if(e->playing) mc_stop(e); else { mc_start(e);if(e->project.song_mode&&!e->project.song_length)u->message="EMPTY SONG: SET LENGTH"; } break;
    case UI_REC: e->recording^=1; break;
    case UI_ARP: if(u->page==5)mc_mute(e,e->selected);else { u->page=u->page==6?0:6;u->editing=0; } break;
    case UI_SEL: mc_arp_clear(e,e->selected);e->selected=(uint8_t)((e->selected+1)%4); break;
    case UI_HOME: u->page=0;u->editing=0; break;
    case UI_SEQ: mc_song_mode(e,!e->project.song_mode);u->page=e->project.song_mode?8:0;u->editing=0;break;
    case UI_EDIT: if(u->page==8)u->song_editing^=1;else if(u->page==7)mc_automation_clear(e,e->pattern);else if(u->page!=9)u->editing^=1;break;
    case UI_ENV: if(u->page==3)u->fx_page^=1;else if(u->page==5)u->mixer_pan^=1;else u->page=u->page==1?0:1; break;
    case UI_GLO: u->page=u->page==5?2:u->page==2?4:u->page==4?7:u->page==7?9:5;u->editing=0;u->mixer_pan=0;break;
    case UI_LFO:
        if(u->page==8)e->project.song_loop^=1;
        else if(u->page==9)mc_clock_source(e,e->project.clock_source<2?e->project.clock_source+1:0);
        else if(u->page==6) { if(e->selected<3) { e->project.arp[e->selected].latch^=1;mc_arp_clear(e,e->selected); } }
        else { e->project.step[e->pattern][e->selected][e->edit_step].flags^=MC_ACCENT; }
        break;
    case UI_FX:
        if(u->editing) e->project.step[e->pattern][e->selected][e->edit_step].flags^=MC_SLIDE;
        else { if(u->page!=3)u->fx_page=UI_FX_SENDS;u->page=3; } break;
    case UI_OCTDN: u->octave=(int8_t)limit(u->octave-1,-3,3); break;
    case UI_OCTUP: u->octave=(int8_t)limit(u->octave+1,-3,3); break;
    case UI_SAVE:
        if(e->playing) { u->message="STOP BEFORE SAVE";break; }
        if(u->save_pending) { u->save_request=1;u->save_pending=0; }
        else { u->save_pending=1;u->message="SAVE AGAIN: PROJECT"; } break;
    default: break;
    }
}
void mc_ui_button_state(McEngine *e,McUi *u,unsigned button,int down) {
    if(button!=UI_OCTDN) { if(down)mc_ui_button(e,u,button);return; }
    if(down) {
        if(!u->pattern_hold) {
            input_reset(u);u->message=0;u->pattern_hold=1;u->pattern_used=0;
        }
    } else if(u->pattern_hold) {
        u->pattern_hold=0;
        if(!u->pattern_used)mc_ui_button(e,u,UI_OCTDN);
        u->pattern_used=0;
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
    if(enc==1) { mc_queue(e,wrap((e->queued<MC_PATTERNS?e->queued:e->pattern)+delta,MC_PATTERNS)); return; }
    unsigned k=enc-3;
    if(u->page==8) {
        McSongEntry *entry=&e->project.song[u->song_cursor];
        if(k==0)u->song_cursor=(uint8_t)wrap(u->song_cursor+delta,MC_SONG_ENTRIES);
        else if(!u->song_editing)u->message="EDIT TO CHANGE SONG";
        else if(k==1)mc_song_set(e,u->song_cursor,wrap(entry->pattern+delta,MC_PATTERNS),entry->repeats);
        else if(k==2)mc_song_set(e,u->song_cursor,entry->pattern,1+wrap((int)entry->repeats-1+delta,16));
        else mc_song_length(e,(unsigned)limit(e->project.song_length+delta,0,MC_SONG_ENTRIES));
    } else if(u->page==9) {
        mc_midi_all_off(e,k);
        e->project.midi_channel[k]=(uint8_t)wrap(e->project.midi_channel[k]+delta,16);
    } else if(u->page==6) {
        if(p==3)return;
        McArp *arp=&e->project.arp[p];
        if(k==0) { unsigned mode=wrap((arp->enabled?(int)arp->mode+1:0)+delta,7);mc_arp_clear(e,p);arp->enabled=mode!=0;if(mode)arp->mode=(uint8_t)(mode-1); }
        if(k==1)arp->rate=(uint8_t)wrap(arp->rate+delta,3);
        if(k==2)arp->octaves=(uint8_t)(1+wrap((int)arp->octaves-1+delta,3));
        if(k==3)arp->gate=(uint8_t)limit(arp->gate+delta,1,100);
    } else if(u->page==7) {
        return;
    } else if(u->page==5) {
        parameter_delta(e,u,k,u->mixer_pan?MC_PARAM_PAN:MC_PARAM_LEVEL,delta);
    } else if(u->page==4) {
        if(k==0) u->demo_choice=(uint8_t)wrap(u->demo_choice+delta,2);
        if(k==3&&delta>0) {
            if(e->playing) u->message="STOP BEFORE LOAD";
            else { u->load_request=(uint8_t)(u->demo_choice+1);u->message="LOADING"; }
        }
    } else if(u->page==3) {
        if(k>=2)return;
        if(u->fx_page==UI_FX_SENDS)parameter_delta(e,u,p,MC_PARAM_RD_SEND+k,delta);
        else {
            unsigned param=MC_PARAM_RD_TYPE+k;
            unsigned value=(unsigned)limit(mc_parameter_get(e,MC_PARTS,param)+delta,0,7);
            if(mc_parameter_set(e,MC_PARTS,param,value)>0)u->message="AUTOMATION FULL";
        }
    } else if(u->page==2) {
        if(k==0) e->project.bpm=(uint16_t)limit(e->project.bpm+delta,40,240);
        if(k==1) e->project.swing=(uint8_t)limit(e->project.swing+delta,0,45);
        if(k==2) mc_queue(e,wrap((e->queued<MC_PATTERNS?e->queued:e->pattern)+delta,MC_PATTERNS));
        if(k==3)e->project.metronome=(uint8_t)(delta>0);
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
    if(enc==0)return MC_PARTS;
    if(enc==1)return MC_PATTERNS;
    if(enc==2)return e->selected==3?MC_KITS:MC_PRESETS;
    unsigned k=enc-3;
    if(u->page==6)return e->selected==3?0:k==0?7:k<3?3:100;
    if(u->page==7)return 0;
    if(u->page==8)return k==0?128:!u->song_editing?0:k==1?64:k==2?16:129;
    if(u->page==9)return 16;
    if(u->page==3)return k>=2?0:u->fx_page==UI_FX_SENDS?128:8;
    if(u->page==5)return 128;
    if(u->page==4)return 2;
    if(u->page==2)return k==0?201:k==1?46:k==2?MC_PATTERNS:2;
    if(e->selected==3)return k<2?16:k==2?2:128;
    if(u->page==1)return 128;
    if(u->editing)return k==0?16:k==1?128:k==2?100:4;
    return 128;
}
void mc_ui_encoder_at(McEngine *e,McUi *u,unsigned enc,int delta,uint32_t now_ms) {
    if(!delta||enc>6)return;
    uint32_t context=e->selected|((uint32_t)u->page<<2)|((uint32_t)(u->page==8?u->song_editing:u->editing)<<6)|((uint32_t)(u->page==5&&u->mixer_pan)<<7)|((uint32_t)u->fx_page<<23);
    if(enc>=3) {
        if(e->selected==3)context|=(uint32_t)u->sound<<8;
        else context|=(uint32_t)e->project.patch[e->selected].engine<<12;
        if(u->editing||e->selected==3)context|=((uint32_t)e->edit_step<<13)|((uint32_t)e->pattern<<17);
    }
    if(u->page==8&&enc>3)context|=(uint32_t)u->song_cursor<<25;
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
void mc_ui_key_velocity(McEngine *e,McUi *u,unsigned key,unsigned velocity,int down) {
    if(key>26||(down&&(!velocity||velocity>127))) return;
    uint32_t bit=1u<<key;
    if(!down&&(u->pattern_keys&bit)) { u->pattern_keys&=~bit;return; }
    input_reset(u);
    if(down&&u->pattern_hold) {
        static const uint8_t white[16]={0,2,4,6,7,9,11,12,14,16,18,19,21,23,24,26};
        static const uint8_t black[4]={1,3,5,8};
        u->pattern_used=1;u->pattern_keys|=bit;
        for(unsigned bank=0;bank<MC_PATTERNS/16;bank++)if(key==black[bank])u->pattern_bank=(uint8_t)bank;
        for(unsigned index=0;index<16;index++)if(key==white[index]) {
            if(e->project.song_mode)u->message="EXIT SONG TO PICK PATTERN";
            else mc_queue(e,u->pattern_bank*16u+index);
        }
        return;
    }
    unsigned p=e->selected;
    if(p==3) {
        if(!down) return;
        u->sound=(uint8_t)(key%16);
        if(u->editing||e->recording) {
            unsigned step=e->recording&&e->playing?e->step:e->edit_step;
            McStep *s=&e->project.step[e->pattern][3][step];
            s->drums^=(uint16_t)(1u<<u->sound); s->velocity=(uint8_t)velocity;s->gate=60;
        }
        mc_drum(e,u->sound,velocity);return;
    }
    unsigned note=(unsigned)limit(53+(int)key+u->octave*12,12,108);
    if(!down) { mc_key(e,p,note,100,0);return; }
    mc_key(e,p,note,velocity,1);
    if(e->project.arp[p].enabled)return;
    if(u->editing||e->recording) {
        unsigned step=e->recording&&e->playing?e->step:e->edit_step;
        McStep *s=&e->project.step[e->pattern][p][step];
        s->note[0]=(uint8_t)note;s->note[1]=s->note[2]=0;s->velocity=(uint8_t)velocity;s->gate=75;s->flags&=(uint8_t)~MC_TIE;
        if(e->key_slide[p])s->flags|=MC_SLIDE;
        if(!e->playing) e->edit_step=(uint8_t)((e->edit_step+1)%16);
    }
}
void mc_ui_key(McEngine *e,McUi *u,unsigned key,int down) {
    mc_ui_key_velocity(e,u,key,100,down);
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
    if(y+7*scale<=(int)c->y||y>=(int)(c->y+c->h))return;
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
/* ADSR 그래프 배치: 모두 240x240 안, 정수 연산만 사용한다. */
#define ENV_X 12
#define ENV_W 216
#define ENV_TOP 62
#define ENV_H 84
#define ENV_MIN 8
#define ENV_PLATEAU 28
/* 정수 제곱근: 긴 구간이 짧은 구간을 지우지 않도록 폭을 압축하는 데 쓴다. */
static unsigned isqrt_u32(uint32_t v) {
    unsigned r=0;
    for(unsigned bit=1u<<15;bit;bit>>=1) if((uint32_t)((r|bit)*(r|bit))<=v) r|=bit;
    return r;
}
/* 두께 2의 Bresenham 선. 외곽 상자가 현재 스트립 밖이면 즉시 건너뛴다. */
static void line(Canvas *c,int x0,int y0,int x1,int y1,uint16_t color) {
    int lo=y0<y1?y0:y1,hi=y0<y1?y1:y0;
    if(hi+1<(int)c->y||lo>=(int)(c->y+c->h))return;
    int dx=x1>x0?x1-x0:x0-x1,dy=y0>y1?y1-y0:y0-y1;
    int sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx+dy;
    for(;;) {
        rect(c,x0,y0,2,2,color);
        if(x0==x1&&y0==y1)break;
        int e2=2*err;
        if(e2>=dy) { err+=dy;x0+=sx; }
        if(e2<=dx) { err+=dx;y0+=sy; }
    }
}
/* 엔진 엔벨로프 식의 샘플 수를 정규화해 A-D-S-R 곡선, 제어점, 라벨과 값을 그린다. */
static void env_graph(Canvas *c,const McPatch *s) {
    if((int)(c->y+c->h)<=ENV_TOP-6||(int)c->y>=ENV_TOP+ENV_H+20)return;
    static const char *const names[4]={"ATK","DEC","SUS","REL"};
    static const uint16_t seg[4]={0x3317,0x3d48,ORANGE,HIT};
    uint32_t a=s->attack,d=s->decay,r=s->release,level=(uint32_t)s->sustain*258u;
    uint32_t astep=32767u/(1u+a*a*MC_RATE/18000u);
    if(!astep)astep=1;
    uint32_t ta=32767u/astep,td=(32767u-level)/(1u+32767u/(880u+d*d*6u)),tr=level/(1u+32767u/(440u+r*r*6u));
    unsigned wa=isqrt_u32(ta)+1,wd=isqrt_u32(td)+1,wr=isqrt_u32(tr)+1,sum=wa+wd+wr;
    unsigned rem=ENV_W-ENV_PLATEAU-3*ENV_MIN;
    int base=ENV_TOP+ENV_H,px[5],py[5];
    px[0]=ENV_X;
    px[1]=px[0]+ENV_MIN+(int)(rem*wa/sum);
    px[2]=px[1]+ENV_MIN+(int)(rem*wd/sum);
    px[3]=px[2]+ENV_PLATEAU;
    px[4]=ENV_X+ENV_W;
    py[0]=py[4]=base;py[1]=ENV_TOP;
    py[2]=py[3]=base-(int)s->sustain*ENV_H/127;
    rect(c,ENV_X-2,base+2,ENV_W+4,1,GRID);rect(c,ENV_X-2,ENV_TOP-2,1,ENV_H+5,GRID);
    for(unsigned i=1;i<4;i++)rect(c,px[i],py[i],1,base-py[i],GRID);
    for(unsigned i=0;i<4;i++)line(c,px[i],py[i],px[i+1],py[i+1],seg[i]);
    for(unsigned i=0;i<5;i++) {
        rect(c,px[i]-2,py[i]-2,5,5,seg[i?i-1:0]);rect(c,px[i]-1,py[i]-1,3,3,WHITE);
    }
    unsigned v[4]={s->attack,s->decay,s->sustain,s->release};
    for(unsigned i=0;i<4;i++) {
        text(c,8+(int)i*56,base+8,names[i],seg[i],1);number(c,28+(int)i*56,base+8,v[i],WHITE);
    }
}
static void pitch_range(const McEngine *e,unsigned part,uint8_t *low,uint8_t *high) {
    unsigned lo=108,hi=12;
    for(unsigned pattern=0;pattern<MC_PATTERNS;pattern++)for(unsigned step=0;step<16;step++) {
        const McStep *st=&e->project.step[pattern][part][step];
        if(st->velocity)for(unsigned note=0;note<3;note++)if(st->note[note]) {
            if(st->note[note]<lo)lo=st->note[note];
            if(st->note[note]>hi)hi=st->note[note];
        }
    }
    *low=(uint8_t)lo;*high=(uint8_t)hi;
}
void mc_ui_prepare_frame(const McEngine *e,McUi *u) {
    u->pitch_ready=0;
    if(u->page||e->selected==3||u->preset_popup)return;
    for(unsigned part=0;part<3;part++)pitch_range(e,part,&u->pitch_low[part],&u->pitch_high[part]);
    u->pitch_ready=1;
}
void mc_ui_strip(const McEngine *e,const McUi *u,unsigned y,unsigned height,uint16_t *pixels) {
    if(y>=240||height>8||y+height>240) return;
    Canvas c={y,height,pixels}; rect(&c,0,(int)y,240,(int)height,BG);
    unsigned next=e->queued<MC_PATTERNS?e->queued:e->pattern;
    if(e->project.song_mode) {
        unsigned pos=e->song_position,length=e->project.song_length;
        next=MC_PATTERNS;
        if(length&&!e->song_finished) {
            if(e->song_repeat+1<e->project.song[pos].repeats)next=e->project.song[pos].pattern;
            else if(pos+1<length||e->project.song_loop)next=e->project.song[(pos+1)%length].pattern;
        }
    }
    char pattern[7];unsigned current=e->pattern+1,digits=0;
    if(current>=10)pattern[digits++]=(char)('0'+current/10);
    pattern[digits++]=(char)('0'+current%10);pattern[digits++]='-';
    if(next<MC_PATTERNS) {
        next++;
        if(next>=10)pattern[digits++]=(char)('0'+next/10);
        pattern[digits++]=(char)('0'+next%10);
    } else {
        pattern[digits++]='E';pattern[digits++]='N';pattern[digits++]='D';
    }
    pattern[digits]=0;
    unsigned p=e->selected;
    text(&c,3,4,e->project.song_mode?"SONG":p==3?"DRUMS":"MC-1",ORANGE,1);
    text(&c,63,4,p==3?mc_drum_names[u->sound]:e->project.patch[p].engine?"303":e->project.patch[p].pcm>=MC_PCM_ATTACKS?"PCM":"LA",WHITE,1);
    if(p==3||u->page>1||u->preset_popup) {
        text(&c,151,4,pattern,GREEN,1);
    }
    text(&c,191,4,e->playing?"+":"-",e->playing?GREEN:WHITE,1);number(&c,211,4,e->project.bpm,WHITE);
    if(e->queued<MC_PATTERNS) { text(&c,163,14,"NEXT",ORANGE,1);number(&c,196,14,e->queued+1,ORANGE); }
    if(p==3) text(&c,3,14,mc_kit_names[e->project.drum_kit],WHITE,1);
    else if(u->page==1)text(&c,63,14,"ADSR",ORANGE,1);
    if(u->preset_popup) { preset_popup(&c,e,u);return; }
    if(u->page==8) {
        const McSongEntry *entry=&e->project.song[u->song_cursor];
        unsigned length=e->project.song_length,pos=e->song_finished&&length?length-1:e->song_position;
        unsigned repeat=e->song_finished&&length?e->project.song[pos].repeats:e->song_repeat+1;
        text(&c,14,30,u->song_editing?"SONG EDIT":"SONG ORDER",WHITE,2);
        text(&c,14,49,e->song_finished?"DONE":e->playing?"PLAY":"STOP",e->playing?GREEN:DIM,1);
        text(&c,48,49,"POS",DIM,1);number(&c,72,49,length?pos+1:0,WHITE);
        text(&c,90,49,"/",DIM,1);number(&c,96,49,length,WHITE);
        text(&c,126,49,"REP",DIM,1);number(&c,150,49,length?repeat:0,WHITE);
        text(&c,162,49,"/",DIM,1);number(&c,168,49,length?e->project.song[pos].repeats:0,WHITE);
        text(&c,20,62,"POS",DIM,1);text(&c,76,62,"PATTERN",DIM,1);text(&c,150,62,"REPEAT",DIM,1);
        unsigned start=u->song_cursor<2?0:u->song_cursor>125?123:u->song_cursor-2;
        for(unsigned row=0;row<5;row++) {
            unsigned index=start+row;int yy=73+(int)row*16;
            uint16_t color=index<length?WHITE:DIM;
            if(index==u->song_cursor) { rect(&c,14,yy-2,210,13,u->song_editing?ORANGE:GRID);color=u->song_editing?BG:ORANGE; }
            if(e->playing&&index==pos)rect(&c,5,yy,4,7,GREEN);
            number(&c,20,yy,index+1,color);number(&c,76,yy,e->project.song[index].pattern+1,color);
            number(&c,150,yy,e->project.song[index].repeats,color);
        }
        text(&c,14,155,"NOW",DIM,1);number(&c,38,155,e->pattern+1,WHITE);
        text(&c,80,155,"NEXT",DIM,1);
        if(!length)text(&c,110,155,"EMPTY SONG",ORANGE,1);
        else if(e->song_finished)text(&c,110,155,"END",DIM,1);
        else if(e->song_repeat+1<e->project.song[pos].repeats)number(&c,110,155,e->project.song[pos].pattern+1,ORANGE);
        else if(pos+1<length||e->project.song_loop)number(&c,110,155,e->project.song[(pos+1)%length].pattern+1,ORANGE);
        else text(&c,110,155,"END",DIM,1);
        text(&c,14,173,e->project.song_loop?"LFO:LOOP ON":"LFO:LOOP OFF",e->project.song_loop?GREEN:WHITE,1);
        text(&c,110,173,u->song_editing?"EDIT:LOCK":"EDIT:CHANGE",WHITE,1);
        knob_scaled(&c,26,"POS",u->song_cursor+1,128,0x3317);
        knob_scaled(&c,85,"PATT",entry->pattern+1,64,0x3d48);
        knob_scaled(&c,145,"REPT",entry->repeats,16,DIM);
        knob_scaled(&c,205,"LEN",length,128,HIT);
    } else if(u->page==9) {
        static const char *const clocks[4]={"INTERNAL","USB","TRS","BLE UNAVAILABLE"};
        static const char *const channels[4]={"PART1","PART2","PART3","DRUMS"};
        text(&c,14,30,"GLOBAL MIDI",WHITE,2);
        text(&c,14,62,"CLOCK",DIM,1);text(&c,56,62,clocks[e->project.clock_source],ORANGE,1);
        text(&c,14,84,"TRANSPORT",DIM,1);text(&c,80,84,e->playing?"PLAY":"STOP",e->playing?GREEN:WHITE,1);
        text(&c,14,106,"RX USB",DIM,1);number(&c,62,106,e->midi.received[0]%100000,WHITE);
        text(&c,116,106,"RX TRS",DIM,1);number(&c,164,106,e->midi.received[1]%100000,WHITE);
        text(&c,14,130,"BLE UNAVAILABLE",DIM,1);
        text(&c,14,151,"KNOBS: TRACK CHANNELS",WHITE,1);
        text(&c,14,173,"LFO: INTERNAL / USB / TRS",WHITE,1);
        for(unsigned k=0;k<4;k++)knob_scaled(&c,k?25+(int)k*60:26,channels[k],e->project.midi_channel[k]+1,16,k==0?0x3317:k==1?0x3d48:k==2?DIM:HIT);
    } else if(u->page==5) {
        static const char *const names[4]={"PART1","PART2","PART3","DRUMS"};
        text(&c,14,30,"GLOBAL MIXER",WHITE,2);
        text(&c,176,34,u->mixer_pan?"PAN":"LEVEL",ORANGE,1);
        for(unsigned part=0;part<4;part++) {
            int x=part?25+(int)part*60:26;
            uint16_t color=e->mute&(1u<<part)?DIM:ORANGE;
            text(&c,x-17,61,names[part],part==p?ORANGE:WHITE,1);
            if(u->mixer_pan) {
                unsigned pan=e->project.pan[part];
                int mx=x-24+(int)pan*48/127;
                text(&c,x-24,79,"L",WHITE,1);text(&c,x-2,79,"C",WHITE,1);text(&c,x+19,79,"R",WHITE,1);
                rect(&c,x-24,102,49,3,GRID);rect(&c,x,96,1,15,DIM);
                rect(&c,mx-2,94,5,19,part==p&&color!=DIM?WHITE:color);
                if(pan==64)text(&c,x-2,122,"C",color,1);
                else { text(&c,x-8,122,pan<64?"L":"R",color,1);number(&c,x,122,pan<64?64-pan:pan-64,color); }
                if(part==p)rect(&c,x-17,70,30,1,ORANGE);
                if(e->mute&(1u<<part)) text(&c,x-11,153,"MUTE",DIM,1);
                else text(&c,x-5,153,"ON",WHITE,1);
                knob(&c,x,names[part],pan,color);
                continue;
            }
            rect(&c,x-8,79,18,64,GRID);
            int level=(int)e->project.level[part]*64/127;
            rect(&c,x-8,143-level,18,level,color);
            if(e->mute&(1u<<part)) text(&c,x-11,153,"MUTE",DIM,1);
            else text(&c,x-5,153,"ON",WHITE,1);
            knob(&c,x,names[part],e->project.level[part],color);
        }
        if(!u->message) text(&c,66,173,"SELECT-ARP to mute",WHITE,1);
        text(&c,14,163,u->mixer_pan?"ENV: LEVEL":"ENV: PAN",DIM,1);
    } else if(u->page==4) {
        text(&c,14,40,"DEMO LIBRARY",WHITE,2);
        text(&c,14,77,u->demo_choice?"NEON SPIRAL":"NIGHT CIRCUIT",ORANGE,2);
        text(&c,14,108,"STOP THEN TURN LOAD",WHITE,1);
        text(&c,14,126,"REPLACES CURRENT PROJECT",DIM,1);
        text(&c,14,146,"PLAY STARTS THE DEMO",WHITE,1);
        knob_scaled(&c,26,"SONG",u->demo_choice+1,2,0x3317);
        knob_scaled(&c,205,"LOAD",u->load_request?1:0,1,HIT);
    } else if(u->page==3) {
        unsigned types=u->fx_page==UI_FX_TYPES;
        text(&c,14,30,types?"FX TYPES":"FX SENDS",WHITE,2);
        if(types)text(&c,14,55,"GLOBAL / SHARED",GREEN,1);
        else if(p==3)text(&c,14,55,"DRUMS",ORANGE,1);
        else {
            text(&c,14,55,"PART",ORANGE,1);number(&c,44,55,p+1,ORANGE);
        }
        for(unsigned k=0;k<2;k++) {
            unsigned type=(unsigned)mc_parameter_get(e,MC_PARTS,MC_PARAM_RD_TYPE+k);
            unsigned value=types?type:(unsigned)mc_parameter_get(e,p,MC_PARAM_RD_SEND+k);
            const char *label=types?(k?"C/F TYPE":"R/D TYPE"):(k?"C/F SEND":"R/D SEND");
            int yy=78+(int)k*40;
            text(&c,14,yy,label,DIM,1);number(&c,190,yy,value,ORANGE);
            if(types)text(&c,202,yy,"/7",DIM,1);
            text(&c,14,yy+17,k?mc_fx_cf_names[type]:mc_fx_rd_names[type],WHITE,1);
            knob_scaled(&c,k?85:26,label,value,types?7:127,k?GREEN:ORANGE);
        }
        if(types) {
            text(&c,14,157,"TYPE 0-7 / ALL FOUR PARTS",DIM,1);
        } else {
            text(&c,14,157,"SEND 0-127 / 0 IS DRY",DIM,1);
        }
        text(&c,132,214,"UNUSED",DIM,1);text(&c,192,214,"UNUSED",DIM,1);
        text(&c,14,173,types?"ENV: SENDS":"ENV: TYPES",WHITE,1);
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
        text(&c,14,75,"64 PATTERNS / FOUR PARTS",DIM,1);
        text(&c,14,99,"ALGORITHM: QUEUE PATTERN",WHITE,1);
        text(&c,14,116,"ARP: ARPEGGIATOR PAGE",WHITE,1);
        text(&c,14,140,e->project.metronome?"METRONOME ON":"METRONOME OFF",e->project.metronome?GREEN:WHITE,1);
        text(&c,14,157,"PLAY + REC ONLY",DIM,1);
        knob_scaled(&c,26,"TEMPO",e->project.bpm,240,0x3317);knob(&c,85,"SWING",e->project.swing,0x3d48);
        knob_scaled(&c,145,"PATT",e->pattern+1,MC_PATTERNS,DIM);
        knob_scaled(&c,205,"METRO",e->project.metronome,1,e->project.metronome?GREEN:DIM);
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
        char title[13];const char *name=match<MC_PRESETS?mc_preset_names[match]:"IMPORTED SOUND";unsigned n=0,changed=0;
        if(match<MC_PRESETS)for(unsigned i=0;i<sizeof(McPatch);i++)
            if(((const uint8_t *)&e->project.patch[p])[i]!=((const uint8_t *)&mc_presets[match])[i])changed=1;
        if(changed)title[n++]='*';
        for(unsigned i=0;name[i]&&n<sizeof title-1;i++)title[n++]=name[i];
        title[n]=0;
        text(&c,8,30,pattern,GREEN,2);text(&c,84,30,title,WHITE,2);
        text(&c,8,49,"CURRENT-NEXT",DIM,1);
        if(match<MC_PRESETS) {
            text(&c,84,49,mc_preset_categories[match],DIM,1);
            number(&c,188,49,match+1,ORANGE);text(&c,207,49,"/128",DIM,1);
        }
        for(unsigned t=0;t<4&&u->page!=1;t++) {
            int yy=59+(int)t*27;
            if(yy+22<=(int)y||yy>=(int)(y+height))continue;
            uint8_t low=108,high=12;
            if(t<3) {
                if(u->pitch_ready) { low=u->pitch_low[t];high=u->pitch_high[t]; }
                else pitch_range(e,t,&low,&high);
            }
            number(&c,4,yy+5,t+1,t==p?ORANGE:DIM);
            if((e->mute>>t)&1) text(&c,12,yy+5,"M",HIT,1);
            for(unsigned s=0;s<16;s++) {
                const McStep *st=&e->project.step[e->pattern][t][s];
                uint16_t color=(t==3?st->velocity&&st->drums!=0:st->velocity&&(st->note[0]||st->note[1]||st->note[2]))?(t==p?ORANGE:HIT):GRID;
                if(e->playing&&s==e->step&&color!=GRID&&!(e->mute&(1u<<t)))color=GREEN;
                rect(&c,29+(int)s*13,yy,11,19,color);
                if(t<3&&st->velocity) for(unsigned note=0;note<3;note++) if(st->note[note]) {
                    int pitch=high>low?limit(((int)st->note[note]-low)*15/(high-low),0,15):7;
                    rect(&c,31+(int)s*13,yy+17-pitch,7,1,BG);
                }
                if(t==p&&s==e->edit_step) rect(&c,29+(int)s*13,yy+20,11,2,WHITE);
            }
        }
        const McPatch *s=&e->project.patch[p];
        if(u->page==1) { env_graph(&c,s);knob(&c,26,"ATK",s->attack,0x3317);knob(&c,85,"DEC",s->decay,0x3d48);knob(&c,145,"SUS",s->sustain,ORANGE);knob(&c,205,"REL",s->release,HIT); }
        else if(u->editing) {
            const McStep *st=&e->project.step[e->pattern][p][e->edit_step];
            knob(&c,26,"STEP",e->edit_step+1,0x3317);knob(&c,85,"VEL",st->velocity,0x3d48);knob(&c,145,"GATE",st->gate,DIM);knob(&c,205,"A/S",st->flags,HIT);
        } else { knob(&c,26,"CUT",s->cutoff,0x3317);knob(&c,85,"RES",s->resonance,0x3d48);knob(&c,145,"DEC",s->decay,DIM);knob(&c,205,s->engine?"GLIDE":"PCM",s->engine?s->glide:s->pcm_mix,HIT); }
    }
    if(!u->message&&u->page<2) {
        char count[6];unsigned n=0,value=e->pattern+1;
        if(value>=10)count[n++]=(char)('0'+value/10);
        count[n++]=(char)('0'+value%10);
        count[n++]='/';count[n++]='6';count[n++]='4';count[n]=0;
        text(&c,4,173,"PATTERN",DIM,1);text(&c,52,173,count,GREEN,1);
    }
    if(u->pattern_hold) {
        rect(&c,0,171,240,12,BG);text(&c,4,173,"PATTERN BANK",DIM,1);
        number(&c,82,173,u->pattern_bank+1,GREEN);text(&c,94,173,"/4",DIM,1);
        text(&c,118,173,"WHITE:SELECT",WHITE,1);
    }
    if(u->message) { rect(&c,0,171,240,12,BG);text(&c,4,173,u->message,ORANGE,1); }
    else if(e->recording) text(&c,p==3?112:3,14,"REC",ORANGE,1);
    else if(u->editing) text(&c,p==3?112:3,14,"EDIT",WHITE,1);
}
