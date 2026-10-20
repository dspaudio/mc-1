/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/engine.c"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static McEngine e,other;
static double held_level(unsigned note) {
    mc_init(&e);memset(e.project.fx_send,0,sizeof e.project.fx_send);
    e.project.patch[0]=(McPatch){MC_LA,2,MC_PCM_ATTACKS+6,0,127,0,0,0,127,0,0,100};
    mc_note(&e,0,note,100,0,0);
    double low=1e30,high=0,real=0,imaginary=0,phase=0;
    double omega=6.283185307179586*(double)mc_frequency[note]/4294967296.0;
    for(unsigned window=0;window<150;window++) {
        int16_t audio[882*2];double power=0;
        mc_render(&e,audio,882);
        for(unsigned i=0;i<882;i++) {
            double value=audio[i*2];
            power+=value*value;
            if(window>=50) { real+=value*cos(phase);imaginary+=value*sin(phase); }
            phase+=omega;
        }
        if(window>=50) {
            if(power<low)low=power;
            if(power>high)high=power;
        }
    }
    printf("LA held note %u RMS range %.3f dB\n",note,10*log10(high/low));
    assert(high/low<1.2);
    return real*real+imaginary*imaginary;
}
static void attack_filter(void) {
    mc_init(&e);memset(e.project.fx_send,0,sizeof e.project.fx_send);
    e.project.patch[0]=(McPatch){MC_LA,2,MC_PCM_ATTACKS,127,0,0,0,0,127,0,0,127};
    mc_note(&e,0,60,127,0,0);
    const McHybridZone *zone=hybrid_zone(&e.voice[0].patch,60);
    e.voice[0].pcm_pos=(uint32_t)(zone->fade_start/2)<<16;
    other=e;other.voice[0].patch.cutoff=127;
    int16_t a[64*2],b[64*2];
    mc_render(&e,a,64);mc_render(&other,b,64);
    assert(!memcmp(a,b,sizeof a));
    unsigned energy=0;for(unsigned i=0;i<128;i++)energy+=(unsigned)abs(a[i]);
    assert(energy>100);
}
static void source_bank(void) {
    unsigned used=0,pcm_bytes=0;
    for(unsigned z=0;z<MC_HYBRID_ZONES;z++) {
        const McHybridZone *zone=&mc_hybrid[z];
        used|=1u<<zone->family;
        assert(zone->fade_end-zone->fade_start==256&&zone->morph_frames>0);
        for(unsigned mip=0;mip<7;mip++) {
            const McHybridAttack *attack=&zone->attack[mip];
            const McHybridWave *wave=&zone->wave[mip];
            assert(attack->length==(zone->attack[0].length>>mip));
            assert(wave->length>=4&&!(wave->length&(wave->length-1u)));
            pcm_bytes+=(unsigned)attack->length*2u+(unsigned)wave->length*4u;
            for(unsigned i=0;i<attack->length;i++)assert(abs(attack->data[i])<=16000);
            for(unsigned i=0;i<wave->length;i++)
                assert(abs(wave->early[i])<=16000&&abs(wave->late[i])<=16000);
        }
        for(unsigned i=0;i<128;i++)assert(zone->decay[i]<=32767);
        if(zone->family!=0&&zone->family!=16) {
            const McHybridWave *wave=&zone->wave[0];
            double early=0,late=0;
            for(unsigned i=0;i<wave->length;i++) {
                early+=(double)wave->early[i]*wave->early[i];
                late+=(double)wave->late[i]*wave->late[i];
            }
            printf("PCM family %u early/late power ratio %.4f\n",zone->family,early/late);
            assert(early/late>0.97&&early/late<1.03);
            if(zone->family<6||zone->family>8) {
                const McHybridAttack *attack=&zone->attack[0];
                double tail=0;
                for(unsigned i=(unsigned)attack->length-512;i<attack->length;i++)
                    tail+=(double)attack->data[i]*attack->data[i];
                double ratio=(early/wave->length)/(tail/512);
                printf("PCM family %u attack/body power ratio %.4f\n",zone->family,ratio);
                assert(ratio>0.94&&ratio<1.06);
            }
        }
    }
    assert(used==131071&&pcm_bytes<=390144);
    printf("LA real-source bank: %u zones, %u PCM bytes, all 17 families\n",MC_HYBRID_ZONES,pcm_bytes);
}
static void attack_colours(void) {
    int16_t audio[3][8192];uint64_t edge[3]={0,0,0};
    for(unsigned mode=0;mode<3;mode++) {
        mc_init(&e);memset(e.project.fx_send,0,sizeof e.project.fx_send);
        e.master=28000;
        e.project.patch[0]=(McPatch){MC_LA,(uint8_t)mode,MC_PCM_ATTACKS,127,90,0,0,0,127,0,0,127};
        mc_note(&e,0,60,127,0,0);mc_render(&e,audio[mode],4096);
        for(unsigned i=2;i<8192;i+=2) {
            int32_t delta=(int32_t)audio[mode][i]-audio[mode][i-2];
            edge[mode]+=(uint64_t)((int64_t)delta*delta);
        }
    }
    assert(memcmp(audio[0],audio[1],sizeof audio[0]));
    assert(memcmp(audio[1],audio[2],sizeof audio[0]));
    assert(edge[0]<edge[1]&&edge[1]<edge[2]);
    puts("PASS: rounded/soft/natural real attacks retain distinct, ordered high-frequency energy");
}
static void soft_mix_onset(void) {
    static const uint8_t mix[]={84,104,108,116};
    for(unsigned i=0;i<sizeof mix;i++) {
        mc_init(&e);memset(e.project.fx_send,0,sizeof e.project.fx_send);
        e.project.patch[0]=(McPatch){MC_LA,1,MC_PCM_ATTACKS+5,mix[i],116,0,0,0,127,0,0,100};
        mc_note(&e,0,60,100,0,0);
        int16_t audio[256];mc_render(&e,audio,1);
        assert(!audio[0]&&!audio[1]);
        mc_render(&e,audio,128);uint64_t energy=0;
        for(unsigned n=0;n<256;n++)energy+=(uint64_t)((int32_t)audio[n]*audio[n]);
        assert(energy>1000);
    }
}
static void resonance_decay(void) {
    static const uint8_t cutoffs[]={0,63,80,100,110,120,127};
    static const uint8_t resonances[]={0,32,64,96,127};
    for(unsigned c=0;c<sizeof cutoffs;c++)for(unsigned r=0;r<sizeof resonances;r++) {
        mc_init(&e);
        e.project.patch[0]=(McPatch){MC_LA,2,0,0,cutoffs[c],resonances[r],0,0,127,0,0,100};
        mc_note(&e,0,60,100,0,0);
        McVoice *v=&e.voice[0];
        v->increment=v->target=v->phase=v->phase2=0;v->filter[0]=1000;
        McVoiceRender render;
        prepare_voice(v,&e.pcm_cache[0],&e.pitch_cache[0],&e.analog_voice[0],0,65536,&render);
        render.hybrid=0;render.table=mc_sine; /* Zero source isolates the real filter recurrence. */
        int peak=0;
        for(unsigned frame=0;frame<16384;frame++) {
            int value=melodic(v,0,&render);
            if(frame>=16128&&abs(value)>peak)peak=abs(value);
        }
        assert(peak<=256); /* Includes the low-cutoff sub-LSB Q15 integrator residue. */
    }
}
int main(void) {
    double middle=held_level(60),upper=held_level(84);
    printf("LA open-filter C6/C4 fundamental ratio %.3f\n",sqrt(upper/middle));
    assert(upper/middle>0.75&&upper/middle<1.25);
    attack_filter();source_bank();attack_colours();soft_mix_onset();resonance_decay();
    puts("PASS: LA steady pitch/sustain, real attack, spectral bandwidth, PCM budget and resonance stability");
    return 0;
}
