/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once
#include "../src/mcfm.h"

/* Diagnostic RAM workload, loaded with audio paused. No storage or hardware writes. */
static void mc_perf_fixture(McEngine *e,unsigned acid_parts) {
    static const uint8_t programs[3]={77,34,9};
    static const uint8_t peak_note[3]={101,101,100};
    uint32_t master=e->master;
    mc_init(e);e->master=master;
    uint8_t *steps=(uint8_t *)e->project.step;
    for(unsigned i=0;i<sizeof e->project.step;i++)steps[i]=0;
    e->project.demo=0;e->project.bpm=240;e->project.swing=0;
    e->project.drum_kit=6;
    e->project.fx_bus_type[0]=4;e->project.fx_bus_type[1]=5;
    e->project.fx_bus_type[2]=e->project.fx_bus_type[3]=0;
    for(unsigned part=0;part<MC_PARTS;part++) {
        e->project.level[part]=127;e->project.pan[part]=64;
        e->project.fx_send[part][0]=e->project.fx_send[part][1]=127;
        e->project.fx_send[part][2]=e->project.fx_send[part][3]=0;
        if(part==3)continue;
        e->project.analog[part]=0;
        e->project.patch[part]=mc_presets[part<acid_parts?MC_PRESET_ACID_SAW:programs[part]];
        e->project.preset[part]=(uint8_t)(part<acid_parts?MC_PRESET_ACID_SAW:programs[part]);
        McPatch *p=&e->project.patch[part];
        p->wave=0;p->attack=0;p->sustain=127;p->release=0;
        p->cutoff=127;p->resonance=127;p->level=127;
        p->glide=127;if(part>=acid_parts)p->pcm_mix=63;
        e->midi.bend[part]=8191;
    }
    for(unsigned step=0;step<MC_STEPS;step++) {
        McStep *drums=&e->project.step[0][3][step];
        drums->drums=(1u<<0)|(1u<<1)|(1u<<3)|(1u<<8)|(1u<<10)|(1u<<11);
        drums->velocity=127;
        for(unsigned part=0;part<acid_parts;part++) {
            McStep *s=&e->project.step[0][part][step];
            s->note[0]=(uint8_t)(step&1?108:36);s->velocity=127;s->gate=100;
            s->flags=MC_SLIDE|MC_ACCENT;
        }
    }
    mc_start(e);
    if(acid_parts<3)for(unsigned i=0;i<MC_VOICES-acid_parts;i++) {
        unsigned part=acid_parts+i%(3u-acid_parts);
        mc_note(e,part,peak_note[part]-i/(3u-acid_parts),127,MC_ACCENT,0);
    }
}
/* Explicit four-bank stress; the original fixture remains the historical two-bank load. */
static inline void mc_perf_fixture_four(McEngine *e,unsigned acid_parts) {
    mc_perf_fixture(e,acid_parts);
    e->project.fx_bus_type[2]=5;e->project.fx_bus_type[3]=5;
    for(unsigned part=0;part<MC_PARTS;part++)
        e->project.fx_send[part][2]=e->project.fx_send[part][3]=127;
}
