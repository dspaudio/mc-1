/* SPDX-License-Identifier: GPL-3.0-only */
/* 실제 engine.c를 포함해 recorded()를 캐시 없는 오라클과 비교한다. */
#include "../src/engine.c"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

_Static_assert(sizeof(((McEngine *)0)->pcm_cache)<=2400,"추가 풀 한도");
_Static_assert(offsetof(McEngine,live_keys)==offsetof(McEngine,pcm_cache)+sizeof(((McEngine *)0)->pcm_cache),"Transient state must follow the cache");
_Static_assert(sizeof(McEngine)<=offsetof(McEngine,pcm_cache)+2400+sizeof(McReverb)+sizeof(McDelay)+8,"All appended transient state must fit the pool budget");

static McEngine eng;
static uint32_t rng=0x1234abcdu;
static unsigned long calls,misses,words;
static const struct { uint32_t step,ratio; } motion[5]={
    {65536,65536},{24317,65536},{131849,70001},{5003,58000},{196609,65536}};

static uint32_t rnd(void) { rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng; }
static const McRecordedLevel *level_of(unsigned k,unsigned mip) { return &mc_recorded[k].levels[mip]; }
/* 원본 알고리즘 그대로, 캐시 없이 level->data를 직접 읽는 독립 오라클. */
static int32_t oracle(McVoice *v,uint32_t ratio) {
    const McRecordedLevel *level=level_of(v->patch.pcm-MC_PCM_ATTACKS,v->sample_level);
    unsigned pos=v->pcm_pos>>16;
    if(pos>=level->length) return 0;
    unsigned next=pos+1;
    if(level->loop_end&&next>=level->loop_end) next=level->loop_start;
    else if(next>=level->length) next=level->length-1u;
    int32_t sample=level->data[pos];
    sample+=(level->data[next]-sample)*(int32_t)(v->pcm_pos&65535)/65536;
    v->pcm_pos+=(uint32_t)(((uint64_t)v->sample_step*ratio)>>16);
    if(level->loop_end&&v->pcm_pos>=((uint32_t)level->loop_end<<16))
        v->pcm_pos-=((uint32_t)(level->loop_end-level->loop_start)<<16);
    return sample;
}
static void both(McPcmCache *c,McVoice *v,McVoice *o,uint32_t ratio) {
    McPcmCache before=*c;
    const McRecordedLevel *level=level_of(v->patch.pcm-MC_PCM_ATTACKS,v->sample_level);
    int32_t got=recorded(c,v,ratio,level),want=oracle(o,ratio);
    assert(got==want);
    assert(v->pcm_pos==o->pcm_pos);
    calls++;
    if(c->window!=before.window||c->count!=before.count||c->pcm!=before.pcm||c->level!=before.level) {
        misses++;words+=c->count;
    }
    if(c->count) {
        const McRecordedLevel *lv=level_of(c->pcm,c->level);
        assert(c->count<=MC_PCM_WINDOW&&(unsigned)c->window+c->count<=lv->length);
    }
}
static uint32_t random_pos(unsigned len) {
    unsigned s;
    if(rnd()&1) s=rnd()%(len+8u);
    else { unsigned back=rnd()%200u;s=len>back?len-back:0; }
    return ((uint32_t)s<<16)|(rnd()&0xffffu);
}
static void retarget(McVoice *v,McVoice *o,unsigned k,unsigned mip,uint32_t pos,uint32_t step) {
    memset(v,0,sizeof *v);
    v->patch.pcm=(uint8_t)(MC_PCM_ATTACKS+k);v->sample_level=(uint8_t)mip;
    v->pcm_pos=pos;v->sample_step=step;*o=*v;
}
static void random_retarget(McVoice *v,McVoice *o,uint32_t *ratio) {
    unsigned k=rnd()%MC_RECORDED_SAMPLES,mip=rnd()%7u,m=rnd()%5u;
    retarget(v,o,k,mip,random_pos(level_of(k,mip)->length),motion[m].step);
    *ratio=motion[rnd()%5u].ratio;
}
static void sweep_all_levels(void) {
    unsigned scenario=0,levels=0;
    for(unsigned k=0;k<MC_RECORDED_SAMPLES;k++) for(unsigned mip=0;mip<7;mip++) {
        const McRecordedLevel *lv=level_of(k,mip);
        unsigned len=lv->length;
        uint32_t starts[7]={0,125,len>140?len-140u:0,len>130?len-130u:0,len>3?len-3u:0,
            lv->loop_end>3?lv->loop_end-3u:0,lv->loop_start};
        levels++;
        for(unsigned s=0;s<7;s++) for(unsigned m=0;m<5;m++) {
            McVoice v,o;
            McPcmCache *c=&eng.pcm_cache[scenario&7u];
            if(scenario&1u) memset(c,0,sizeof *c); /* 짝수는 이전 태그가 남은 채 시작 */
            scenario++;
            retarget(&v,&o,k,mip,(starts[s]<<16)|((s*0x2493u)&0xffffu),motion[m].step);
            for(unsigned n=0;n<300;n++) both(c,&v,&o,motion[m].ratio);
        }
    }
    assert(levels==112);
}
static void swap_sources(void) {
    McVoice v,o;uint32_t ratio;
    memset(eng.pcm_cache,0,sizeof eng.pcm_cache);
    random_retarget(&v,&o,&ratio);
    for(unsigned it=0;it<30000;it++) {
        uint32_t r=rnd();
        if(r%53u==0) random_retarget(&v,&o,&ratio);
        else if(r%211u==1) { v.sample_level=(uint8_t)(rnd()%7u);o=v; }
        else if(r%307u==2) { v.patch.pcm=(uint8_t)(MC_PCM_ATTACKS+rnd()%MC_RECORDED_SAMPLES);o=v; }
        both(&eng.pcm_cache[0],&v,&o,ratio);
    }
}
static void interleave_voices(void) {
    McVoice v[MC_VOICES],o[MC_VOICES];uint32_t ratio[MC_VOICES];
    memset(eng.pcm_cache,0,sizeof eng.pcm_cache);
    for(unsigned i=0;i<MC_VOICES;i++) random_retarget(&v[i],&o[i],&ratio[i]);
    for(unsigned it=0;it<80000;it++) {
        unsigned i=it%MC_VOICES;
        if(it%997u==i) random_retarget(&v[i],&o[i],&ratio[i]);
        both(&eng.pcm_cache[i],&v[i],&o[i],ratio[i]);
    }
}
static void sequential_bound(void) {
    for(unsigned k=0;k<MC_RECORDED_SAMPLES;k++) for(unsigned mip=0;mip<7;mip++) {
        const McRecordedLevel *lv=level_of(k,mip);
        if(lv->loop_end) continue;
        McVoice v,o;
        unsigned long c0=calls,m0=misses;
        memset(&eng.pcm_cache[0],0,sizeof eng.pcm_cache[0]);
        retarget(&v,&o,k,mip,0,65536);
        for(unsigned n=0;n<(unsigned)lv->length+2u;n++) both(&eng.pcm_cache[0],&v,&o,65536);
        assert(misses-m0<=lv->length/127u+2u);
        printf("sequential one-shot k=%u mip=%u: calls=%lu misses=%lu\n",k,mip,calls-c0,misses-m0);
        return;
    }
    printf("sequential bound skipped: no one-shot level\n");
}
static void phase_frame(void) {
    uint32_t phase[MC_VOICES],second[MC_VOICES],base[MC_VOICES],ratio[MC_VOICES];
    unsigned moving[MC_VOICES];
    int16_t stereo[2];
    for(unsigned i=0;i<MC_VOICES;i++) {
        const McVoice *v=&eng.voice[i];
        phase[i]=v->phase;second[i]=v->phase2;base[i]=v->increment;
        moving[i]=v->increment!=v->target;
        int bend=eng.midi.bend[v->part];
        ratio[i]=(uint32_t)(65536+bend*(bend<0?7150:8026)/8192);
        if(!v->active)continue;
        assert(v->stage!=2&&!v->gate_left);
        if(moving[i]) {
            int32_t diff=(int32_t)v->target-(int32_t)base[i];
            int32_t step=diff/(20+v->patch.glide*16);
            if(!step)step=diff>0?1:-1;
            base[i]=(uint32_t)((int32_t)base[i]+step);
        }
        uint32_t advance=(uint32_t)(((uint64_t)base[i]*ratio[i])>>16);
        phase[i]+=advance;second[i]+=advance+advance/401;
    }
    mc_render(&eng,stereo,1);
    for(unsigned i=0;i<MC_VOICES;i++) {
        const McVoice *v=&eng.voice[i];
        assert(v->phase==phase[i]&&v->phase2==second[i]&&v->increment==base[i]);
        if(v->active&&!moving[i]) {
            assert(eng.pitch_cache[i].base==base[i]&&eng.pitch_cache[i].ratio==ratio[i]);
            uint32_t advance=(uint32_t)(((uint64_t)base[i]*ratio[i])>>16);
            assert(eng.pitch_cache[i].increment==advance);
            assert(eng.pitch_cache[i].second==advance+advance/401);
        }
    }
}
static void phase_cache(void) {
    static const int16_t bends[5]={-8192,-1,0,1,8191};
    mc_init(&eng);
    for(unsigned part=0;part<3;part++)eng.project.patch[part].engine=MC_LA;
    for(unsigned i=0;i<MC_VOICES;i++)mc_note(&eng,i%3,12+i*12,100,0,0);
    for(unsigned bend=0;bend<5;bend++) {
        for(unsigned part=0;part<3;part++)eng.midi.bend[part]=bends[(bend+part)%5];
        for(unsigned sample=0;sample<3;sample++)phase_frame();
    }
    mc_note(&eng,0,108,100,0,0);phase_frame();
    mc_init(&eng);mc_preset(&eng,0,MC_PRESET_ACID_SAW);
    eng.project.patch[0].glide=64;mc_note(&eng,0,36,100,0,0);phase_frame();
    mc_note(&eng,0,60,100,MC_SLIDE,0);
    for(unsigned sample=0;sample<1024;sample++) {
        if(sample%128==0)eng.midi.bend[0]=bends[(sample/128)%5];
        if(sample==512)assert(!mc_parameter_set(&eng,0,10,0));
        phase_frame();
    }
    puts("PASS: exact phase cache, all8 slots, bend extremes/reversal, reuse, glide and live speed changes");
}
static void slice_preparation(void) {
    static McEngine other;
    int16_t block[257*2],frame[2];
    mc_init(&eng);
    eng.project.bpm=240;
    for(unsigned p=0;p<3;p++) {
        mc_preset(&eng,p,p==2?MC_PRESET_ACID_SAW:77);
        eng.project.arp[p].enabled=1;eng.project.arp[p].rate=2;
        eng.project.patch[p].glide=127;
        mc_key(&eng,p,48+p*12,100,1);mc_key(&eng,p,55+p*12,127,1);
        eng.arp[p].clock=MC_RATE*60u-eng.project.bpm*(1u<<2)*(7u+p);
    }
    other=eng;
    for(unsigned block_index=0;block_index<32;block_index++) {
        for(unsigned p=0;p<3;p++) {
            int bend=(int)((block_index*997u+p*2311u)%16384u)-8192;
            eng.midi.bend[p]=other.midi.bend[p]=(int16_t)bend;
            unsigned cutoff=(block_index*17u+p*37u)%128u;
            assert(!mc_parameter_set(&eng,p,offsetof(McPatch,cutoff),cutoff));
            assert(!mc_parameter_set(&other,p,offsetof(McPatch,cutoff),cutoff));
        }
        mc_render(&eng,block,257);
        for(unsigned n=0;n<257;n++) {
            mc_render(&other,frame,1);
            assert(block[n*2]==frame[0]&&block[n*2+1]==frame[1]);
        }
        assert(!memcmp(&eng,&other,sizeof eng));
    }
    puts("PASS: stopped three-part ARP source preparation, pitch/filter changes and complete state across 257/1-frame blocks");
    mc_demo_load(&eng,1);mc_start(&eng);
    eng.events=65;eng.demo_frame=63;
    other=eng;
    mc_render(&eng,block,257);
    for(unsigned n=0;n<257;n++) {
        mc_render(&other,frame,1);
        assert(block[n*2]==frame[0]&&block[n*2+1]==frame[1]);
    }
    assert(!memcmp(&eng,&other,sizeof eng));
    puts("PASS: demo filter automation inside a slice preserves PCM and complete state");
}
int main(void) {
    sweep_all_levels();
    swap_sources();
    interleave_voices();
    sequential_bound();
    phase_cache();
    slice_preparation();
    memset(eng.pcm_cache,0xff,sizeof eng.pcm_cache);
    mc_init(&eng); /* 콜드 리셋은 캐시를 비운다 */
    for(unsigned i=0;i<sizeof eng.pcm_cache;i++) assert(((const uint8_t *)eng.pcm_cache)[i]==0);
    printf("pcm_cache offset=%zu extra=%zu bytes calls=%lu window misses=%lu copied=%lu int16 (uncached ~%lu reads)\n",
        offsetof(McEngine,pcm_cache),sizeof eng.pcm_cache,calls,misses,words,calls*2ul);
    puts("host 계수일 뿐 장치 타이밍 주장 아님");
    return 0;
}
