/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#include "../src/parallel.h"
#include "../src/ui.h"
#include "../firmware/perf_fixture.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static McEngine reference,parallel_engine;
static McDual worker;
static pthread_t thread;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t event=PTHREAD_COND_INITIALIZER;
static unsigned pending,finished,stopping,live,mode,wait_pending,halts;
static unsigned overlap_entered,overlap_advanced;
static uint32_t overlap_target;
static _Thread_local unsigned on_worker;
static uint64_t compared_frames;
static uint32_t virtual_tick;
static void await_event(unsigned *value) {
    struct timespec until;assert(!clock_gettime(CLOCK_REALTIME,&until));until.tv_sec+=5;
    while(!*value)assert(!pthread_cond_timedwait(&event,&mutex,&until));
}
/* Scheduler-independent host clock: join waits for the exact worker event.
 * Fault modes then advance virtual time, never sleep to cause a timeout. */
static uint32_t ticks(void) {
    if(!on_worker&&wait_pending) {
        assert(!pthread_mutex_lock(&mutex));await_event(&finished);
        wait_pending=0;assert(!pthread_mutex_unlock(&mutex));
        if(mode==1||mode==2)virtual_tick+=worker.budget_ticks;
    }
    return on_worker?0:virtual_tick;
}
static uint32_t profile_ticks(void) {
    if(mode==7&&wait_pending&&parallel_engine.elapsed>=overlap_target) {
        assert(!pthread_mutex_lock(&mutex));
        if(!overlap_advanced) {
            assert(overlap_entered&&mc_acquire(&worker.request)!=mc_acquire(&worker.done));
            overlap_advanced=1;assert(!pthread_cond_broadcast(&event));
        }
        assert(!pthread_mutex_unlock(&mutex));
    }
    return 0;
}
static void *run_worker(void *unused) {
    (void)unused;on_worker=1;
    assert(!pthread_mutex_lock(&mutex));
    for(;;) {
        while(!pending&&!stopping) {
            struct timespec until;assert(!clock_gettime(CLOCK_REALTIME,&until));until.tv_sec+=5;
            assert(!pthread_cond_timedwait(&event,&mutex,&until));
        }
        if(stopping)break;
        pending=0;assert(!pthread_mutex_unlock(&mutex));
        uint32_t job=mc_acquire(&worker.request);
        if(mode==2&&worker.fx_enabled) {
            /* A partially completed job may corrupt only its private state/output. */
            worker.fx[0].rd.delay.delay[0][0]=12345;
            worker.fx[1].cf.phase+=999;worker.job.bus[3][0][0]=777777;
            worker.drums.voice[0].env=12345;worker.drums.random^=0x87654321u;
            worker.drums.sample[0]=777777;
            worker.drums.melodic[0].voice.phase+=123456u;
            worker.drums.melodic[0].pcm.data[0]=1234;
            worker.drums.melodic[0].analog.random^=0x12345678u;
            worker.drums.part[0][0]=888888;
        } else if(mode==7&&worker.fx_enabled&&!overlap_entered) {
            /* Run real R/D1 on CPU1, then require next-slice synthesis before the
             * remaining real FX. An immediate CPU0 join deterministically fails. */
            mc_fx_rd_block(&worker.fx[0],worker.job.bpm,worker.job.type[0],worker.job.bus[0],worker.job.frames);
            assert(!pthread_mutex_lock(&mutex));overlap_entered=1;
            assert(!pthread_cond_broadcast(&event));await_event(&overlap_advanced);
            assert(!pthread_mutex_unlock(&mutex));
            mc_fx_cf_block(&worker.fx[0],worker.job.type[1],worker.job.bus[1],worker.job.frames);
            mc_fx_rd_block(&worker.fx[1],worker.job.bpm,worker.job.type[2],worker.job.bus[2],worker.job.frames);
            mc_fx_cf_block(&worker.fx[1],worker.job.type[3],worker.job.bus[3],worker.job.frames);
            if(worker.drums_enabled)mc_synth_job_run(&worker.drums);
            worker.processed=15;worker.result=MC_DUAL_READY;mc_release(&worker.done,job);
        } else if(mode!=1&&mode!=6) {
            mc_dual_work(&worker,job);
            if(mode==4)worker.result=MC_DUAL_PROTOCOL;
            mc_release(&worker.done,mode==3?job+1u:job);
        }
        assert(!pthread_mutex_lock(&mutex));finished=1;
        assert(!pthread_cond_broadcast(&event));
    }
    assert(!pthread_mutex_unlock(&mutex));return 0;
}
static void notify(McDual *d) {
    assert(d==&worker);
    assert(!pthread_mutex_lock(&mutex));
    pending=1;finished=0;wait_pending=1;
    assert(!pthread_cond_broadcast(&event));
    if(mode==7&&d->fx_enabled&&!overlap_entered)await_event(&overlap_entered);
    assert(!pthread_mutex_unlock(&mutex));
}
static void halt(McDual *d) {
    assert(d==&worker);halts++;
    if(!live)return;
    assert(!pthread_mutex_lock(&mutex));stopping=1;
    assert(!pthread_cond_broadcast(&event));assert(!pthread_mutex_unlock(&mutex));
    assert(!pthread_join(thread,0));live=0;wait_pending=0;
}
static void boot(unsigned fault) {
    memset(&worker,0,sizeof worker);atomic_init(&worker.request,0);atomic_init(&worker.done,0);
    pending=finished=stopping=wait_pending=halts=virtual_tick=0;mode=fault;
    overlap_entered=overlap_advanced=0;
    worker.status=fault==5?MC_DUAL_BOOT_FAILED:MC_DUAL_READY;
    worker.ticks=ticks;worker.notify=notify;worker.halt=halt;
    worker.budget_ticks=12000;worker.spin_limit=8;
    live=fault!=5;
    if(live)assert(!pthread_create(&thread,0,run_worker,0));
}
static void compare(unsigned frames) {
    int16_t a[514],b[514];
    McRenderContext context={.dual=&worker,.ticks=mode==7?profile_ticks:0};
    assert(frames<=257);
    mc_render(&reference,a,frames);
    mc_render_ex(&parallel_engine,b,frames,&context);
    assert(!memcmp(a,b,frames*2u*sizeof *a));
    /* Includes FX history, limiter, sequencer, MIDI FIFO, and all voice state. */
    assert(!memcmp(&reference,&parallel_engine,sizeof reference));
    compared_frames+=frames;
}
static void fixture(unsigned acid) {
    mc_init(&reference);mc_perf_fixture(&reference,acid);
    parallel_engine=reference;
    for(unsigned part=0;part<MC_PARTS;part++) {
        assert(!reference.project.fx_send[part][2]&&!reference.project.fx_send[part][3]);
        if(part<3)assert(!reference.project.analog[part]);
    }
    unsigned voices=0,drums=0;
    for(unsigned i=0;i<MC_VOICES;i++)voices+=reference.voice[i].active!=0;
    for(unsigned i=0;i<MC_DRUM_VOICES;i++)drums+=reference.drum[i].env!=0;
    assert(voices==(acid==3?3u:MC_VOICES)&&drums==6);
}
static void four_fixture(unsigned acid) {
    fixture(acid);mc_perf_fixture_four(&reference,acid);parallel_engine=reference;
}
static void full_load(void) {
    boot(0);
    for(unsigned acid=0;acid<4;acid++)for(unsigned kit=0;kit<8;kit++) {
        fixture(acid);
        reference.project.drum_kit=parallel_engine.project.drum_kit=(uint8_t)kit;
        for(unsigned rd=0;rd<8;rd++)for(unsigned cf=0;cf<8;cf++) {
            reference.project.fx_bus_type[0]=parallel_engine.project.fx_bus_type[0]=(uint8_t)rd;
            reference.project.fx_bus_type[1]=parallel_engine.project.fx_bus_type[1]=(uint8_t)cf;
            compare(128);
        }
    }
    assert(worker.jobs&&worker.status==MC_DUAL_READY&&!halts);
    for(unsigned acid=0;acid<4;acid++) {
        four_fixture(acid);
        for(unsigned rd=0;rd<8;rd++)for(unsigned cf=0;cf<8;cf++)
            for(unsigned rd2=0;rd2<8;rd2++)for(unsigned cf2=0;cf2<8;cf2++) {
                uint8_t types[MC_FX_BANKS]={(uint8_t)rd,(uint8_t)cf,(uint8_t)rd2,(uint8_t)cf2};
                memcpy(reference.project.fx_bus_type,types,sizeof types);
                memcpy(parallel_engine.project.fx_bus_type,types,sizeof types);
                reference.project.drum_kit=parallel_engine.project.drum_kit=(uint8_t)((rd2+cf2)%8u);
                compare(128);assert(worker.processed==15);
            }
    }
    halt(&worker);
    printf("PASS dualcore: legacy 64 FX pairs; all 4096 four-bank combinations, 0/1/2 Acid at %u voices, separate 3-Acid mono\n",
        (unsigned)MC_VOICES);
}
static void changes_and_tails(void) {
    boot(0);
    for(unsigned type=0;type<8;type++) {
        four_fixture(type%3);
        reference.project.fx_bus_type[0]=parallel_engine.project.fx_bus_type[0]=(uint8_t)type;
        reference.project.fx_bus_type[1]=parallel_engine.project.fx_bus_type[1]=(uint8_t)type;
        reference.project.fx_bus_type[2]=parallel_engine.project.fx_bus_type[2]=(uint8_t)((type+3u)%8u);
        reference.project.fx_bus_type[3]=parallel_engine.project.fx_bus_type[3]=(uint8_t)((type+5u)%8u);
        for(unsigned part=0;part<3;part++)
            reference.project.patch[part].release=parallel_engine.project.patch[part].release=127;
        for(unsigned block=0;block<900;block++) {
            if(block==32) { mc_stop(&reference);mc_stop(&parallel_engine); }
            if(block==33)for(unsigned part=0;part<4;part++)for(unsigned bank=0;bank<MC_FX_BANKS;bank++)
                reference.project.fx_send[part][bank]=parallel_engine.project.fx_send[part][bank]=0;
            compare(128);
        }
    }
    for(unsigned demo=0;demo<2;demo++) {
        mc_demo_load(&reference,demo);mc_demo_load(&parallel_engine,demo);
        mc_start(&reference);mc_start(&parallel_engine);
        for(unsigned block=0;block<1024;block++) {
            if(block%64u==0) {
                uint8_t midi[3]={0xb0,1,(uint8_t)(block%128u)};
                for(unsigned i=0;i<3;i++) {
                    mc_midi_byte(&reference,0,midi[i]);mc_midi_byte(&parallel_engine,0,midi[i]);
                }
                McUi a,b;uint16_t pa[240*8],pb[240*8];
                mc_ui_init(&a);mc_ui_init(&b);
                mc_ui_encoder(&reference,&a,0,1);mc_ui_encoder(&parallel_engine,&b,0,1);
                mc_ui_prepare_frame(&reference,&a);mc_ui_prepare_frame(&parallel_engine,&b);
                mc_ui_strip(&reference,&a,0,8,pa);mc_ui_strip(&parallel_engine,&b,0,8,pb);
                assert(!memcmp(pa,pb,sizeof pa));
            }
            if(block%128u==127) {
                mc_stop(&reference);mc_stop(&parallel_engine);
                mc_start(&reference);mc_start(&parallel_engine);
                mc_preset(&reference,1,block%128);mc_preset(&parallel_engine,1,block%128);
            }
            compare(block%3u==0?1u:block%3u==1?17u:128u);
        }
    }
    /* One-frame stopped ARP slices and overlapping Acid glide. */
    fixture(1);mc_stop(&reference);mc_stop(&parallel_engine);
    for(unsigned i=0;i<2;i++) {
        McEngine *e=i?&parallel_engine:&reference;
        e->project.arp[1].enabled=1;mc_key(e,1,60,100,1);
        mc_key(e,0,48,100,1);mc_key(e,0,60,100,1);
    }
    for(unsigned i=0;i<128;i++)compare(128);
    halt(&worker);
    puts("PASS dualcore: long release/tails, demos, MIDI/UI, resets, stop/start, presets, glide, live ARP");
}
static void failures(void) {
    for(unsigned fault=1;fault<=6;fault++) {
        four_fixture(0);boot(fault);
        for(unsigned i=0;i<4;i++)compare(128);
        assert(worker.status==(fault==5?MC_DUAL_BOOT_FAILED:
            fault==3||fault==4?MC_DUAL_PROTOCOL:MC_DUAL_TIMEOUT));
        assert(halts==(fault==5?0u:1u)&&!live);
        /* Quarantine persists across reset of the engine and repeated renders. */
        static McSharedFx quarantined[MC_FX_BANKS/2];
        McSynthJob quarantined_drums=worker.drums;
        memcpy(quarantined,worker.fx,sizeof quarantined);
        four_fixture(2);compare(128);
        assert(!memcmp(quarantined,worker.fx,sizeof quarantined));
        assert(!memcmp(&quarantined_drums,&worker.drums,sizeof quarantined_drums));
    }
    boot(0);fixture(0);
    worker.sequence=UINT32_MAX-1u;
    mc_release(&worker.request,worker.sequence);mc_release(&worker.done,worker.sequence);
    compare(128);assert(worker.sequence>0&&worker.sequence<16);
    halt(&worker);
    boot(5);
    worker.job.frames=17;
    mc_dual_work(&worker,1);assert(worker.result==MC_DUAL_PROTOCOL);
    puts("PASS dualcore: no-start, delayed/partial work, wrong sequence/result, stuck timer, quarantine, wrap");
}
static void lookahead_automation(void) {
    four_fixture(0);boot(7);
    /* The first 16-frame slice finishes one frame before a sequencer step.
     * Next-slice synthesis changes every FX type/send while CPU1 owns slice 0. */
    reference.clock=MC_RATE*60u*100u-reference.project.bpm*400u*17u;
    reference.project.automation_count=8;
    const uint8_t parameters[8]={MC_PARAM_RD_TYPE,MC_PARAM_CF_TYPE,MC_PARAM_RD2_TYPE,MC_PARAM_CF2_TYPE,
        MC_PARAM_RD_SEND,MC_PARAM_CF_SEND,MC_PARAM_RD2_SEND,MC_PARAM_CF2_SEND};
    const uint8_t values[8]={7,6,3,4,52,65,78,91};
    for(unsigned i=0;i<8;i++)
        reference.project.automation[i]=(McAutomation){0,1,parameters[i],values[i]};
    parallel_engine=reference;overlap_target=parallel_engine.elapsed+17u;
    compare(128);
    assert(overlap_entered&&overlap_advanced&&reference.project.fx_bus_type[0]==7);
    assert(worker.jobs==10&&worker.status==MC_DUAL_READY&&!halts);
    halt(&worker);
    puts("PASS dualcore: event-gated FX(n)/synthesis(n+1) overlap, four-bank automation snapshots, render-local drain");
}
static void block_tails(void) {
    static const unsigned sizes[]={0,1,15,16,17,31,32,127,128,129,255,257};
    four_fixture(0);boot(0);
    for(unsigned repeat=0;repeat<32;repeat++)for(unsigned i=0;i<sizeof sizes/sizeof sizes[0];i++) {
        compare(sizes[i]);
        assert(mc_acquire(&worker.request)==mc_acquire(&worker.done));
    }
    halt(&worker);
    puts("PASS dualcore: zero/small/partial/multiple-block renders drain without cross-render jobs");
}
static void analog_voices(void) {
    for(unsigned acid=0;acid<4;acid++) {
        four_fixture(acid);boot(0);
        for(unsigned part=0;part<3;part++) {
            assert(!mc_parameter_set(&reference,part,MC_PARAM_ANALOG,127));
            assert(!mc_parameter_set(&parallel_engine,part,MC_PARAM_ANALOG,127));
        }
        for(unsigned block=0;block<512;block++) {
            if(block==128) {
                mc_key(&reference,0,60,100,1);mc_key(&parallel_engine,0,60,100,1);
                mc_key(&reference,0,72,100,1);mc_key(&parallel_engine,0,72,100,1);
            }
            if(block==256&&acid==1) { halt(&worker);boot(2); }
            compare(128);
        }
        unsigned initialized=0;
        for(unsigned voice=0;voice<MC_VOICES;voice++)
            initialized+=reference.analog_voice[voice].random!=0;
        assert(initialized);
        if(live)halt(&worker);
    }
    puts("PASS dualcore: Analog enabled on all melodic parts, smooth drift/glide and partial-worker fallback");
}
static void sparse_reads(void) {
    static McSharedFx expected,canonical,isolated;
    static const unsigned tempos[]={0,96,240,44100,661500};
    for(unsigned rd=0;rd<8;rd++)for(unsigned cf=0;cf<8;cf++) {
        memset(&expected,0,sizeof expected);memset(&canonical,0,sizeof canonical);
        uint8_t types[2]={(uint8_t)rd,(uint8_t)cf};
        for(unsigned block=0;block<1024;block++) {
            unsigned frames=block%16u+1u,bpm=tempos[(block/32u)%5u];
            int32_t a[2][16][2],b[2][16][2];
            for(unsigned bank=0;bank<2;bank++)for(unsigned i=0;i<frames;i++)for(unsigned c=0;c<2;c++)
                a[bank][i][c]=b[bank][i][c]=block<16?(int32_t)((block*137u+i*71u+c*53u)%20001u)-10000:0;
            if(block==1)expected.cf.phase=canonical.cf.phase=UINT32_MAX-100u;
            memset(&isolated,0xa5,sizeof isolated);
            mc_fx_prepare(&isolated,&canonical,bpm,types,frames,3);
            mc_fx_rd_block(&expected,bpm,rd,a[0],frames);
            mc_fx_cf_block(&expected,cf,a[1],frames);
            mc_fx_rd_block(&isolated,bpm,rd,b[0],frames);
            mc_fx_cf_block(&isolated,cf,b[1],frames);
            mc_fx_commit(&canonical,&isolated,frames,3);
            for(unsigned bank=0;bank<2;bank++)assert(!memcmp(a[bank],b[bank],frames*sizeof a[bank][0]));
            assert(!memcmp(&expected,&canonical,sizeof expected));
        }
    }
    puts("PASS dualcore: poisoned private history, sparse reads, short-delay feedback, cursor/phase wrap and tails");
}
static void drums_without_fx(void) {
    fixture(0);boot(0);
    memset(reference.project.fx_send,0,sizeof reference.project.fx_send);
    parallel_engine=reference;
    uint32_t random=reference.random;
    const unsigned sizes[]={1,15,16,17,128,257};
    for(unsigned i=0;i<sizeof sizes/sizeof sizes[0];i++)compare(sizes[i]);
    assert(worker.jobs&&worker.drums_enabled&&!worker.fx_enabled&&!worker.processed);
    assert(reference.random!=random&&worker.status==MC_DUAL_READY);
    assert(mc_acquire(&worker.request)==mc_acquire(&worker.done));
    halt(&worker);
    puts("PASS dualcore: private drum/PRNG jobs without FX, partial blocks and same-call drain");
}
#if MC_PERF_PROBE == 1
static void probe_masks(void) {
    four_fixture(0);boot(0);
    const uint8_t masks[]={0,1,2,3,8,9,10,11,0};
    for(unsigned repeat=0;repeat<4;repeat++)for(unsigned i=0;i<sizeof masks;i++) {
        mc_probe_skip=masks[i];compare(128);
    }
    mc_probe_skip=0;halt(&worker);
    puts("PASS dualcore: synthesis and R/D/C/F probe skip semantics preserve all four histories");
}
#endif
int main(void) {
    full_load();changes_and_tails();failures();lookahead_automation();block_tails();analog_voices();sparse_reads();drums_without_fx();
#if MC_PERF_PROBE == 1
    probe_masks();
#endif
    printf("PASS dualcore: %llu frames bit-identical; full %zu-byte engine state identical\n",
        (unsigned long long)compared_frames,sizeof reference);
    printf("MEMORY: engine=%zu worker=%zu private_history=%zu bytes; sparse per-job snapshots\n",
        sizeof(McEngine),sizeof(McDual),sizeof worker.fx);
    return 0;
}
