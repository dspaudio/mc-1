/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "mcfm.h"
static void put32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i)); }
static uint32_t get32(const uint8_t *p) { return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
uint32_t mc_crc(const void *data,unsigned n) {
    const uint8_t *b=data; uint32_t c=0xffffffffu;
    while(n--) { c^=*b++; for(unsigned i=0;i<8;i++) c=(c>>1)^(0xedb88320u*(c&1)); }
    return ~c;
}
int mc_project_valid(const McProject *p) {
    if(p->bpm<40||p->bpm>240||p->swing>45||p->drum_kit>=MC_KITS||p->demo>1||p->padding[0]||p->padding[1]||p->padding[2]) return 0;
    for(unsigned i=0;i<4;i++) if(p->fx[i]>127) return 0;
    for(unsigned t=0;t<4;t++) for(unsigned i=0;i<4;i++) if(p->track_fx[t][i]>127) return 0;
    if(p->reserved||p->automation_count>MC_AUTOMATION) return 0;
    for(unsigned t=0;t<3;t++) {
        const McArp *a=&p->arp[t];
        if((p->preset[t]>=MC_PRESETS&&p->preset[t]!=255)||a->enabled>1||a->mode>MC_ARP_CHORD||a->rate>2||
           a->octaves<1||a->octaves>3||a->gate<1||a->gate>100||a->latch>1) return 0;
    }
    for(unsigned t=0;t<5;t++) {
        const uint8_t *type=t==4?p->fx_type:p->track_fx_type[t];
        const uint8_t *filter=t==4?p->filter:p->track_filter[t];
        for(unsigned i=0;i<4;i++) if(type[i]>(i==3?2:3)) return 0;
        if(filter[0]>127||filter[1]>127) return 0;
    }
    for(unsigned i=0;i<MC_AUTOMATION;i++) {
        const McAutomation *a=&p->automation[i];
        if(i>=p->automation_count) { if(a->address||a->step||a->parameter||a->value) return 0;continue; }
        unsigned part=a->address%5,param=a->parameter;
        unsigned maximum=param==0?1:param==1?2:param==2?MC_PCM_SOURCES-1:
            param>=MC_PARAM_DELAY_TYPE&&param<MC_PARAM_HPF?(param==MC_PARAM_MOD_TYPE?2:3):127;
        if(a->address>=20||a->step>=16||param>=MC_PARAMETERS||a->value>maximum||
           (part>=3&&param<12)||(part==4&&param<MC_PARAM_DELAY)||(param>=MC_PARAM_DRUM_LEVEL&&part!=3)) return 0;
        for(unsigned j=0;j<i;j++) if(p->automation[j].address==a->address&&p->automation[j].step==a->step&&
            p->automation[j].parameter==param) return 0;
    }
    for(unsigned t=0;t<3;t++) {
        const McPatch *s=&p->patch[t]; const uint8_t *b=(const uint8_t *)s;
        if(s->engine>1||s->wave>2||s->pcm>=MC_PCM_SOURCES) return 0;
        if(s->engine==MC_ACID&&s->pcm>=MC_PCM_ATTACKS) return 0;
        for(unsigned j=3;j<sizeof *s;j++) if(b[j]>127) return 0;
    }
    for(unsigned t=0;t<4;t++) if(p->level[t]>127||p->pan[t]>127) return 0;
    for(unsigned d=0;d<MC_DRUMS;d++) if(p->drum_level[d]>127) return 0;
    for(unsigned b=0;b<4;b++) for(unsigned t=0;t<4;t++) for(unsigned i=0;i<16;i++) {
        const McStep *s=&p->step[b][t][i];
        if(s->velocity>127||s->gate>100||s->flags>7) return 0;
        for(unsigned c=0;c<3;c++) if(s->note[c]&&(s->note[c]<12||s->note[c]>108)) return 0;
        if(t<3&&s->velocity&&s->note[0]&&!s->gate) return 0;
    }
    return 1;
}
static void pack(const McProject *p,uint8_t *out) {
    *out++=(uint8_t)p->bpm; *out++=(uint8_t)(p->bpm>>8); *out++=p->swing; *out++=p->drum_kit;
    for(unsigned t=0;t<3;t++) for(unsigned i=0;i<12;i++) *out++=((const uint8_t *)&p->patch[t])[i];
    for(unsigned t=0;t<4;t++) *out++=p->level[t];
    for(unsigned t=0;t<4;t++) *out++=p->pan[t];
    for(unsigned d=0;d<16;d++) *out++=p->drum_level[d];
    for(unsigned i=0;i<4;i++) *out++=p->fx[i];
    *out++=p->demo;for(unsigned i=0;i<3;i++) *out++=0;
    for(unsigned b=0;b<4;b++) for(unsigned t=0;t<4;t++) for(unsigned i=0;i<16;i++) {
        const McStep *s=&p->step[b][t][i];
        for(unsigned c=0;c<3;c++) *out++=s->note[c];
        *out++=s->velocity; *out++=s->gate; *out++=s->flags;
        *out++=(uint8_t)s->drums; *out++=(uint8_t)(s->drums>>8);
    }
    for(unsigned t=0;t<4;t++) for(unsigned i=0;i<4;i++) *out++=p->track_fx[t][i];
    for(unsigned t=0;t<3;t++) *out++=p->preset[t];
    for(unsigned t=0;t<3;t++) for(unsigned i=0;i<6;i++) *out++=((const uint8_t *)&p->arp[t])[i];
    for(unsigned i=0;i<4;i++) *out++=p->fx_type[i];
    for(unsigned t=0;t<4;t++) for(unsigned i=0;i<4;i++) *out++=p->track_fx_type[t][i];
    for(unsigned i=0;i<2;i++) *out++=p->filter[i];
    for(unsigned t=0;t<4;t++) for(unsigned i=0;i<2;i++) *out++=p->track_filter[t][i];
    *out++=0;*out++=(uint8_t)p->automation_count;*out++=(uint8_t)(p->automation_count>>8);
    for(unsigned n=0;n<MC_AUTOMATION;n++) for(unsigned i=0;i<4;i++) *out++=((const uint8_t *)&p->automation[n])[i];
}
static void unpack(McProject *p,const uint8_t *in,unsigned version) {
    p->bpm=(uint16_t)(in[0]|(unsigned)in[1]<<8); in+=2; p->swing=*in++; p->drum_kit=*in++;
    for(unsigned t=0;t<3;t++) for(unsigned i=0;i<12;i++) ((uint8_t *)&p->patch[t])[i]=*in++;
    for(unsigned t=0;t<4;t++) p->level[t]=*in++;
    for(unsigned t=0;t<4;t++) p->pan[t]=*in++;
    for(unsigned d=0;d<16;d++) p->drum_level[d]=*in++;
    for(unsigned i=0;i<4;i++) p->fx[i]=version>=2?*in++:0;
    p->demo=version>=2?*in++:0;
    for(unsigned i=0;i<3;i++) p->padding[i]=version>=2?*in++:0;
    for(unsigned b=0;b<4;b++) for(unsigned t=0;t<4;t++) for(unsigned i=0;i<16;i++) {
        McStep *s=&p->step[b][t][i];
        for(unsigned c=0;c<3;c++) s->note[c]=*in++;
        s->velocity=*in++; s->gate=*in++; s->flags=*in++;
        s->drums=(uint16_t)(in[0]|(unsigned)in[1]<<8); in+=2;
    }
    for(unsigned t=0;t<4;t++) for(unsigned i=0;i<4;i++) p->track_fx[t][i]=version>=3?*in++:0;
    mc_project_defaults(p);
    if(version>=4) {
        for(unsigned t=0;t<3;t++) p->preset[t]=*in++;
        for(unsigned t=0;t<3;t++) for(unsigned i=0;i<6;i++) ((uint8_t *)&p->arp[t])[i]=*in++;
        for(unsigned i=0;i<4;i++) p->fx_type[i]=*in++;
        for(unsigned t=0;t<4;t++) for(unsigned i=0;i<4;i++) p->track_fx_type[t][i]=*in++;
        for(unsigned i=0;i<2;i++) p->filter[i]=*in++;
        for(unsigned t=0;t<4;t++) for(unsigned i=0;i<2;i++) p->track_filter[t][i]=*in++;
        p->reserved=*in++;p->automation_count=(uint16_t)(in[0]|(unsigned)in[1]<<8);in+=2;
        for(unsigned n=0;n<MC_AUTOMATION;n++) for(unsigned i=0;i<4;i++) ((uint8_t *)&p->automation[n])[i]=*in++;
    }
}
int mc_encode(const McProject *p,uint32_t sequence,uint8_t *out,unsigned n) {
    if(n<MC_RECORD_BYTES||!mc_project_valid(p)) return -1;
    put32(out,0x4d46434du); out[4]=4; out[5]=0;
    out[6]=MC_PROJECT_BYTES&255; out[7]=MC_PROJECT_BYTES>>8;
    put32(out+8,sequence); pack(p,out+20);
    put32(out+12,mc_crc(out+20,MC_PROJECT_BYTES)); put32(out+16,mc_crc(out,16));
    return MC_RECORD_BYTES;
}
int mc_decode(McProject *p,uint32_t *sequence,const uint8_t *in,unsigned n) {
    McProject temp;
    if(n<20) return -1;
    unsigned body=in[4]==1?2112:in[4]==2?2120:in[4]==3?2136:in[4]==4?MC_PROJECT_BYTES:0;
    if(!body||n<body+20||(n!=body+20&&n!=MC_RECORD_BYTES&&!(in[4]==1&&n==2140))||get32(in)!=0x4d46434du||in[5]||
       (in[6]|(unsigned)in[7]<<8)!=body||get32(in+16)!=mc_crc(in,16)||
       get32(in+12)!=mc_crc(in+20,body)) return -1;
    if(in[4]<3&&in[23]) return -1;
    unpack(&temp,in+20,in[4]); if(!mc_project_valid(&temp)) return -1;
    if(in[4]<4) for(unsigned b=0;b<4;b++) for(unsigned t=0;t<4;t++) for(unsigned i=0;i<16;i++)
        if(temp.step[b][t][i].flags>3) return -1;
    *p=temp; *sequence=get32(in+8); return 0;
}
int mc_recover(McProject *p,uint32_t *sequence,const uint8_t *a,const uint8_t *b) {
    McProject temp; uint32_t sa=0,sb=0;
    int va=mc_decode(&temp,&sa,a,MC_RECORD_BYTES)==0;
    int vb=mc_decode(&temp,&sb,b,MC_RECORD_BYTES)==0;
    if(!va&&!vb) return -1;
    unsigned chosen=vb&&(!va||(int32_t)(sb-sa)>0);
    mc_decode(p,sequence,chosen?b:a,MC_RECORD_BYTES); return (int)chosen;
}
