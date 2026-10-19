/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio
 * FM-1 integration based on the GPL SLOOP/Felucca HAL by Leo Kuroshita. */
#include <stdint.h>
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "fm1_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#include "panel_leds.h"
/* 프로젝트 드라이버는 calibration, bootloader, OTA, samples, global settings를 쓰지 않는다. */
#define FL_RANGE_OK(off,n) ((uint32_t)(off)>=0x99000u && (uint32_t)(off)<=0xab000u && (uint32_t)(n)<=0xab000u-(uint32_t)(off))
#include "fm1_flash.h"
#include "libc.c"
#include "lcd.c"
#if MC_DSP_SPLIT != 1
#include "../src/effects.c"
#endif
#include "../src/demo.c"
#if MC_DSP_SPLIT != 1
#include "../src/engine.c"
#endif
#include "../src/factory.c"
#include "../src/project.c"
#include "../src/store.c"
#include "../src/ui.c"
#include "../src/midi.c"
#include "panel_modes.h"
#include "../src/parallel.h"
#if MC_DUAL_CORE == 1
#include "fm1_cpu1.h"
#endif
#define HALF_FRAMES 128u
#define FELUCCA_CDC 0
#define FELUCCA_OTA 1
#define FELUCCA_OTA_DRYRUN 0
#define OTA_IDENTITY "FM-1_900"
#define FELUCCA_UAC 1
#define FELUCCA_UAC_48K 0
#define MC_USB_APP 1
#define MC_OTA_TX_LOCK() fm1_irq_off()
#define MC_OTA_TX_UNLOCK() fm1_irq_on()
#define RING_PUBLISH() __asm__ volatile("csync" ::: "memory")
#include "update/usb.c"
#include "update/ota.c"
#include "update/ota_flash.h"
#include "connection/midi_uart.c"
#include "connection/routing.c"
#define FRAMES 128u
static McEngine engine __attribute__((section(".pool")));
static McUi ui;
static int32_t dma[FRAMES*4] __attribute__((aligned(4)));
static int16_t block[FRAMES*2];
static int32_t usb_block[FRAMES*2];
static volatile uint8_t in_audio;
static uint16_t strip[240*8];
static volatile uint32_t milliseconds, max_audio_us, deadline_misses;
static volatile uint32_t max_render_us, max_audio_interval_us, audio_half_changes;
static uint32_t previous_audio_start;
#if MC_DUAL_CORE == 1 || MC_RENDER_PROFILE == 1
static McRenderContext render_context;
#endif
static uint32_t stage_ticks(void) { return fm1_ticks(); }
static uint8_t flash_ok,irq_ready;
static volatile uint8_t ota_usb_owner;
static uint32_t ota_poll_tick;
static volatile uint32_t ota_step;
static volatile int32_t ota_result_code;
#if MC_PERF_PROBE == 1
#define MC_PERF_PUBLISH() RING_PUBLISH()
#include "perf_probe.h"
#include "perf_hardware.h"
#include "perf_fixture.h"
#if MC_RENDER_PROFILE == 1
#include "stage_probe.h"
#endif
static uint32_t mc_perf_ticks(void) { return fm1_ticks(); }
static uint32_t mc_perf_lock(void) {
    uint32_t saved=fm1_icfg();fm1_irq_off();return saved;
}
static void mc_perf_unlock(uint32_t saved) { RING_PUBLISH();fm1_icfg_set(saved); }
static int mc_perf_send(const uint8_t *body,uint32_t n) { return ota_send_msg(0x71,body,n); }
static void ota_probe_request(const uint8_t *body,uint32_t n) {
#if MC_RENDER_PROFILE == 1
    if(mc_stage_request(body,n))return;
#endif
    if(!mc_hw_request(body,n))mc_perf_request(body,n);
}
static int mc_hw_send(const uint8_t *body,uint32_t n) { return ota_send_msg(0x71,body,n); }
static uint32_t mc_hw_load_count,mc_hw_load_expected;
static uint8_t mc_hw_loaded_demo,mc_hw_wanted_demo;
/* 0/1 stop/start, 2/3 demos, 4..7 two-bank, 8..11 four-bank,
 * 12..15 four-bank + Analog stress (0..3 Acid), 16 controlled CPU1 fallback.
 * RAM-only diagnostics: no SAVE, clock, voltage or flash writes. */
static uint8_t mc_hw_control(unsigned action) {
    uint32_t saved=fm1_icfg();fm1_irq_off();
    if(mc_perf.state!=MC_PERF_IDLE||ui.load_request||ui.save_request) {
        fm1_icfg_set(saved);return 1;
    }
    if(action==16) {
        uint8_t status=3;
#if MC_DUAL_CORE == 1
        if(mc_cpu1.status==MC_DUAL_READY&&
           mc_acquire(&mc_cpu1.request)==mc_acquire(&mc_cpu1.done)) {
            mc_dual_fail(&mc_cpu1,MC_DUAL_PROTOCOL);
            status=(uint8_t)(mc_cpu1_held?0:3);
        }
#endif
        RING_PUBLISH();fm1_icfg_set(saved);return status;
    }
    if(action==1)mc_start(&engine);
    else mc_stop(&engine);
    if(action>=2) {
        mc_hw_wanted_demo=(uint8_t)(action-2);
        mc_hw_load_expected=mc_hw_load_count+1u;
        ui.load_request=(uint8_t)(action-1);
    }
    RING_PUBLISH();fm1_icfg_set(saved);
    return (uint8_t)(action>=2?2:0);
}
static uint8_t mc_hw_control_done(void) {
    if(mc_hw_load_count!=mc_hw_load_expected)return 2;
    return (uint8_t)(mc_hw_loaded_demo==mc_hw_wanted_demo?0:3);
}
/* 정지·비계측 상태에서만 고정된 레지스터를 읽는다. PLL·전원 값은 쓰지 않는다. */
static uint8_t mc_hw_snapshot(uint32_t values[6]) {
    uint32_t saved=fm1_icfg();fm1_irq_off();
    if(engine.playing||mc_perf.state!=MC_PERF_IDLE) {
        fm1_icfg_set(saved);return 1;
    }
    uint32_t timeouts=fm1_p33_timeouts;
    values[0]=*(volatile uint32_t *)0x1000cu;   /* CLK_CON0 */
    values[1]=*(volatile uint32_t *)0x119acu;   /* PLL2_CON1 */
    values[2]=*(volatile uint32_t *)0x1eee004u; /* C1_CON */
    values[3]=*(volatile uint32_t *)0x1eee008u; /* CACHE_CON */
    values[4]=fm1_p33_read(0x06u);             /* P3_ANA_CON6 */
    values[5]=fm1_p33_read(0x09u);             /* P3_ANA_CON9 */
    uint8_t status=(uint8_t)(fm1_p33_timeouts!=timeouts?2:0);
    RING_PUBLISH();fm1_icfg_set(saved);return status;
}
static uint8_t mc_hw_worker_snapshot(uint32_t values[6]) {
#if MC_DUAL_CORE == 1
    uint32_t saved=fm1_icfg();fm1_irq_off();
#if MC_CPU1_TRACE == 1
    values[0]=mc_acquire(&mc_c1_trace);values[1]=mc_acquire(&mc_c1_heartbeat);
#endif
    values[2]=mc_acquire(&mc_cpu1.request);values[3]=mc_acquire(&mc_cpu1.done);
    values[4]=mc_cpu1.result;values[5]=mc_cpu1.job_ticks;
#if MC_CPU1_TRACE == 1
    values[5]=mc_acquire(&mc_c1_peak_ticks);
#endif
    fm1_icfg_set(saved);return 0;
#else
    (void)values;return 3;
#endif
}
static void mc_perf_snapshot(McPerfSnapshot *s) {
    s->deadline=deadline_misses;s->half=audio_half_changes;
    s->underruns=uac.underruns;s->overruns=uac.overruns;s->stalls=uac.stalls;s->missed=uac.missed;
    s->timer_con=FM1_T4_CON;s->timer_prd=FM1_T4_PRD;
    s->clk1=FM1_CLK_CON1;s->clk2=FM1_CLK_CON2;s->alnk_len=FM1_ALNK_LEN;
    s->bpm=engine.project.bpm;s->playing=engine.playing;s->clock_source=engine.project.clock_source;
    s->master=engine.master;s->step=engine.step;
    s->flags=(uint32_t)(uac.alt!=0)|((uint32_t)(uac.go!=0)<<1)|((uint32_t)(uac.feed!=0)<<2);
}
#if MC_VOICE_PROBE == 1
static uint8_t mc_hw_voice_snapshot(uint32_t values[6]) {
    uint32_t saved=fm1_icfg();fm1_irq_off();
    if(mc_perf.state!=MC_PERF_IDLE) {
        fm1_icfg_set(saved);return 1;
    }
    values[0]=MC_VOICES;values[1]=MC_WORKER_VOICES;
    for(unsigned i=0;i<MC_VOICES;i++)values[2]+=engine.voice[i].active!=0;
    for(unsigned i=0;i<MC_DRUM_VOICES;i++)values[3]+=engine.drum[i].env!=0;
    values[4]=MC_DRUM_VOICES;
#if MC_DUAL_CORE == 1
    values[5]=mc_cpu1.status;
#endif
    fm1_icfg_set(saved);return 0;
}
#endif
#endif
extern void isr_alnk0(void),isr_timer5(void);
void fm1_timer5_irq(void) {
#if MC_PERF_PROBE == 1
    uint32_t probe_begin=fm1_ticks();
    unsigned probe_active=mc_perf.state==MC_PERF_ACTIVE;
#endif
    static uint32_t last,acc,sub,owed;
    fm1_timer5_ack();fm1_input_tick();
    uint32_t now=fm1_ticks();acc+=now-last;last=now;
    while(acc>=24000u) { acc-=24000u;milliseconds++; }
    /* staging 중에는 main만 SIE INDEX와 USB 큐를 소비한다. 시각·패널 스캔은 유지한다. */
    if(ota_usb_owner) { owed=0;goto timer_done; }
    unsigned usb_due=sub%5u==0;
    if(usb_due)owed|=1u;
    if(sub%5u==2u)owed|=2u;
    if(++sub==10u)sub=0;
    /* TIMER5 can preempt audio: only the EP4 consumer runs while rendering.
     * Descriptor/reset and MIDI producers wait until the render is finished. */
    if(in_audio) { if(usb_due)uac_service();goto timer_done; }
    connection_midi_poll(owed);
    owed=0;
timer_done:
#if MC_PERF_PROBE == 1
    mc_perf_timer_end(probe_begin,probe_active);
#else
    (void)0;
#endif
}
void fm1_alnk0_irq(void) {
    uint8_t p=fm1_audio_pending();fm1_audio_ack_aux(p);
    if(p&FM1_AUDIO_HALF) {
        uint32_t start=fm1_ticks(),half=fm1_audio_free_half();
#if MC_PERF_PROBE == 1
        mc_perf_audio_begin(start);
#if MC_RENDER_PROFILE == 1
        mc_stage_begin();
#endif
#endif
        if(previous_audio_start) {
            uint32_t interval=(start-previous_audio_start)/24;
            if(interval>max_audio_interval_us)max_audio_interval_us=interval;
        }
        previous_audio_start=start;
        in_audio=1;RING_PUBLISH();
        connection_midi_input(&engine);
        uac_render_start();
        uint32_t render_start=fm1_ticks();
#if MC_DUAL_CORE == 1 || MC_RENDER_PROFILE == 1
        mc_render_ex(&engine,block,FRAMES,&render_context);
#else
        mc_render(&engine,block,FRAMES);
#endif
#if MC_PERF_PROBE == 1
        uint32_t probe_render_ticks=fm1_ticks()-render_start;
        uint32_t render_us=probe_render_ticks/24;
#else
        uint32_t render_us=(fm1_ticks()-render_start)/24;
#endif
        if(render_us>max_render_us)max_render_us=render_us;
#if MC_PERF_PROBE == 1
        mc_perf_render(probe_render_ticks);
#endif
        for(unsigned i=0;i<FRAMES*2;i++)usb_block[i]=block[i];
        uac_tap(usb_block,FRAMES);
        connection_midi_output(&engine);
        /* Retain inherited 24-bit format and -6 dB hardware output ceiling. */
        for(unsigned i=0;i<FRAMES*2;i++) dma[half*FRAMES*2+i]=(int32_t)block[i]*128;
        unsigned changed=fm1_audio_free_half()!=half;
        if(changed)audio_half_changes++;
        fm1_audio_ack_half();
#if MC_PERF_PROBE == 1 && MC_RENDER_PROFILE == 1
        mc_stage_end(fm1_ticks()-start,probe_render_ticks,&render_context);
#endif
        uint32_t us=(fm1_ticks()-start)/24;
        if(us>max_audio_us) max_audio_us=us;
        if(us>=FRAMES*1000000u/MC_RATE||changed) deadline_misses++;
        RING_PUBLISH();in_audio=0;
#if MC_PERF_PROBE == 1
        mc_perf_audio_end(start);
#endif
    }
}
static int read_flash(uint32_t off,void *out,unsigned n) {
#if MC_DUAL_CORE == 1
    if(!mc_cpu1_flash_safe())return -1;
#endif
    uint8_t *p=out;
    if(!FL_RANGE_OK(off,n)) return -1;
    while(n) {
        unsigned k=n>256?256:n;
        fm1_irq_off();int rc=FL_FAR(fl_read_ram)(off,p,k);
        if(irq_ready)fm1_irq_on();
        if(rc) return rc;
        p+=k;off+=k;n-=k;
    } return 0;
}
static int erase_flash(uint32_t off) {
#if MC_DUAL_CORE == 1
    if(!mc_cpu1_flash_safe())return -1;
#endif
    uint32_t took;fm1_wdt_feed();return fl_erase4k(off,&took);
}
static int program_flash(uint32_t off,const void *data,unsigned n) {
#if MC_DUAL_CORE == 1
    if(!mc_cpu1_flash_safe())return -1;
#endif
    return fl_write(off,data,n);
}
static const McStorage storage={read_flash,erase_flash,program_flash};
static void audio_pause(void) {
    fm1_irq_off();fm1_irq_mask(FM1_IRQ_ALNK0);
#if MC_DUAL_CORE == 1
    mc_cpu1_idle();
#endif
    fm1_audio_stop();uac.go=0;RING_PUBLISH();uac.feed=0;
    previous_audio_start=0;
    connection_midi_pause(&engine);fm1_irq_on();
}
#if MC_PERF_PROBE == 1
/* 실제 strip 제출 한 번을 센다. 공유 패널 스케줄러는 변경하지 않는다. */
static void probe_lcd_blit(unsigned x,unsigned y,unsigned w,unsigned h,const uint16_t *p) {
    lcd_blit(x,y,w,h,p);mc_perf_panel(0);
}
#define lcd_blit probe_lcd_blit
#endif
#define MC_PANEL_CACHE_ATTR __attribute__((section(".pool"),aligned(8)))
static void panel_capture(McUi *view,McUiFrame *frame) {
    uint32_t saved=fm1_icfg();fm1_irq_off();RING_PUBLISH();
    *view=ui;mc_ui_capture_frame(&engine,frame);
    RING_PUBLISH();fm1_icfg_set(saved);
}
#define MC_PANEL_CAPTURE(u,f) panel_capture((u),(f))
#include "panel_refresh.c"
#undef MC_PANEL_CAPTURE
#undef MC_PANEL_CACHE_ATTR
#if MC_PERF_PROBE == 1
#undef lcd_blit
static uint8_t mc_hw_panel_snapshot(uint32_t values[6]) {
    uint32_t saved=fm1_icfg();fm1_irq_off();
    panel_stats(values);
    fm1_icfg_set(saved);return 0;
}
#endif
static uint32_t ota_now_ms(void) { return milliseconds; }
static void ota_idle(void) {
    fm1_wdt_feed();
    if(ota_usb_owner) {
        uint32_t now=fm1_ticks();
        if(now-ota_poll_tick>=500u*FM1_TICKS_PER_US) {
            ota_poll_tick=now;usb_poll();mi_r=mi_w;
        }
    }
}
static int ota_erase(uint32_t off) {
    ota_idle();int rc=mc_ota_flash_erase(off);ota_idle();return rc;
}
static int ota_prog(uint32_t off,const void *p,uint32_t n) {
    ota_idle();int rc=mc_ota_flash_program(off,p,n);ota_idle();return rc;
}
static int ota_fread(uint32_t off,void *p,uint32_t n) { return mc_ota_flash_read(off,p,n); }
static void ota_show(uint32_t step,int32_t code) {
    ota_step=step;ota_result_code=code;
    ui.message=step==9?(code==-16?"UPDATE CLEANUP FAILED":"UPDATE FAILED"):
        step==5?"UPDATE REBOOT":"UPDATING - KEEP USB";
    draw();lcd_sync();ota_idle();
}
static void ota_commit(const uint8_t *parm) {
#if MC_DUAL_CORE == 1
    mc_cpu1_halt(&mc_cpu1);
    if(!mc_cpu1_held)return;
#endif
    fm1_irq_off();usb_detach();fm1_guard_unlock_top();fm1_enter_update(parm);
}
#if MC_PERF_PROBE == 1
static void probe_audio_pause(void) {
    mc_hw_invalidate();mc_perf_invalidate();
#if MC_RENDER_PROFILE == 1
    mc_stage.pending=0;mc_stage.words[2]=0;
#endif
    audio_pause();
}
#define audio_pause probe_audio_pause
#endif
#if MC_DUAL_CORE == 1
static int mc_cpu1_flash_probe(void) { return mc_cpu1_flash_safe()&&mc_ota_flash_probe(); }
#define mc_ota_flash_probe mc_cpu1_flash_probe
#endif
#include "update/ota_app.c"
#if MC_DUAL_CORE == 1
#undef mc_ota_flash_probe
#endif
#if MC_PERF_PROBE == 1
#undef audio_pause
#endif
#include "update/ota_entry.c"
static void fm1_fault(const fm1_crash_t *crash) {
#if MC_DUAL_CORE == 1
    mc_cpu1_halt(&mc_cpu1);
    if(!mc_cpu1_held) { fm1_audio_stop();return; }
#endif
    (void)crash;fm1_audio_stop();fm1_guard_unlock_top();fm1_enter_uboot();
}
extern uint32_t _data_start[],_data_end[],_data_load[],_bss_start[],_bss_end[];
extern uint32_t _pool_start[],_pool_end[],_rt_start[],_rt_end[],_rt_load[];
extern uint32_t _c1_start[],_c1_end[],_c1_load[];
void fm1_cstart(void) {
    uint32_t *d,*s;
    fm1_irq_off();fm1_time_init();fm1_wdt_arm(0x0d);fm1_irq_init();
    for(d=_bss_start;d<_bss_end;d++) *d=0;
    for(d=_pool_start;d<_pool_end;d++) *d=0;
    for(s=_data_load,d=_data_start;d<_data_end;s++,d++) *d=*s;
    for(s=_rt_load,d=_rt_start;d<_rt_end;s++,d++) *d=*s;
    for(s=_c1_load,d=_c1_start;d<_c1_end;s++,d++) *d=*s;
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK|FM1_GUARD_WRITE|FM1_GUARD_BUS|FM1_GUARD_PC);
    mc_init(&engine);mc_ui_init(&ui);lcd_init();fm1_input_init();fm1_adc_init();
#if MC_DUAL_CORE == 1
    /* Quiesce an inherited core before the first flash-off probe or restore. */
    mc_cpu1_halt(&mc_cpu1);
    flash_ok=(uint8_t)(mc_cpu1_held&&FL_FAR(fl_jedec_ram)()==0x856014u);
#else
    flash_ok=FL_FAR(fl_jedec_ram)()==0x856014u;
#endif
    /* 프로젝트 복원이 끝날 때까지 IRQ와 오디오/MIDI 소비를 시작하지 않는다. */
    if(flash_ok) { uint32_t seq; (void)mc_store_load(&storage,&engine.project,&seq); }
#if MC_DUAL_CORE == 1
    mc_cpu1_boot();render_context.dual=&mc_cpu1;
#endif
#if MC_RENDER_PROFILE == 1
    render_context.ticks=stage_ticks;
#if MC_PERF_PROBE == 1
    mc_stage.read_cost=0xffffffffu;
    for(unsigned i=0;i<32;i++) {
        uint32_t begin=stage_ticks(),cost=stage_ticks()-begin;
        if(cost<mc_stage.read_cost)mc_stage.read_cost=cost;
    }
#endif
#endif
    usb_start();uart_midi_init();
    fm1_timer5_start(isr_timer5,4);fm1_audio_init(dma,FRAMES*2,isr_alnk0,3);
    fm1_guard_lock_top();irq_ready=1;fm1_irq_enable_all();
    mc_ota_boot_service();
    uint32_t notes=0,both_since=0;
    uint8_t led_playing=255,led_step=255;
    uint32_t led_notes=UINT32_MAX;
    uint16_t led_modes=0xffffu;
    uint8_t held_part[27]={0},held_note[27]={0};
    static const uint8_t buttons[14]={2,3,4,5,6,7,8,9,10,11,12,13,0,1};
    static const uint8_t encoders[7]={0,1,6,2,3,4,5};
#if MC_DUAL_CORE == 1
    unsigned cpu1_reported=MC_DUAL_READY;
#endif
    for(;;) {
        fm1_wdt_feed();uint32_t now=milliseconds;mc_ui_tick(&ui,now);usb_retry(now);
#if MC_DUAL_CORE == 1
        if(cpu1_reported!=mc_cpu1.status) {
            cpu1_reported=mc_cpu1.status;
            ui.message=mc_cpu1_held?"CPU1 FAILED - 1 CORE":"CPU1 FAILED - NO SAVE";
        }
#endif
        if(mc_ota_entry_service()) {
            /* 업데이트 중 쌓인 패널 조작이 복귀 직후 실행되지 않게 한다. */
            (void)fm1_input_edges(0);
            for(unsigned i=0;i<7;i++)(void)fm1_enc_take(encoders[i]);
            notes=fm1_in.notes;both_since=0;continue;
        }
#if MC_PERF_PROBE == 1
        mc_perf_service();
        mc_hw_service();
#if MC_RENDER_PROFILE == 1
        mc_stage_service();
#endif
#endif
        uint32_t released=0,pressed=fm1_input_edges(&released),current=fm1_in.notes;
        for(unsigned i=0;i<14;i++) if((pressed|released)&(1u<<buttons[i])) {
            fm1_irq_off();
            if(pressed&(1u<<buttons[i]))mc_ui_button_state(&engine,&ui,i,1);
            if(released&(1u<<buttons[i]))mc_ui_button_state(&engine,&ui,i,0);
            fm1_irq_on();
        }
        for(unsigned i=0;i<7;i++) {
            int step=fm1_enc_take(encoders[i]);
            if(step) { fm1_irq_off();mc_ui_encoder_at(&engine,&ui,i,step,now);fm1_irq_on(); }
        }
        for(unsigned i=0;i<27;i++) if((current^notes)&(1u<<i)) {
            fm1_irq_off();ui.preset_popup=ui.encoder_active=0;
            if(current&(1u<<i)) {
                held_part[i]=engine.selected;held_note[i]=(uint8_t)limit(53+(int)i+ui.octave*12,12,108);
                mc_ui_key(&engine,&ui,i,1);
                if(ui.pattern_keys&(1u<<i))held_part[i]=255;
            } else if(ui.pattern_keys&(1u<<i))mc_ui_key(&engine,&ui,i,0);
            else if(held_part[i]<3) mc_key(&engine,held_part[i],held_note[i],100,0);
            fm1_irq_on();
        }
        notes=current;
        if((fm1_in.buttons&3u)==3u&&!engine.playing) {
            if(!both_since) both_since=now;
            if(now-both_since>=5000) {
#if MC_DUAL_CORE == 1
                mc_cpu1_halt(&mc_cpu1);
                if(!mc_cpu1_held) {
                    ui.message="CPU1 FAILED - NO SAVE";both_since=now;continue;
                }
#endif
                fm1_audio_stop();fm1_guard_unlock_top();fm1_enter_uboot();
            }
        } else both_since=0;
        int volume=fm1_adc_read(FM1_ADC_MASTER);
        if(volume>=0) {
            unsigned value=(unsigned)limit(volume,0,1023);
            engine.master=(uint32_t)((uint64_t)value*value*MC_MASTER_MAX/(1023u*1023u));
        }
        if(ui.load_request) {
            unsigned demo=ui.load_request-1;ui.load_request=0;
            if(!engine.playing) {
                audio_pause();
#if MC_PERF_PROBE == 1
                if(demo>=10) {
                    mc_perf_fixture_four(&engine,demo-10);
                    for(unsigned p=0;p<3;p++)engine.project.analog[p]=127;
                }
                else if(demo>=6)mc_perf_fixture_four(&engine,demo-6);
                else if(demo>=2)mc_perf_fixture(&engine,demo-2);
                else
#endif
                    mc_demo_load(&engine,demo);
                mc_ui_init(&ui);ui.message="DEMO LOADED - PLAY";
                fm1_irq_off();for(unsigned i=0;i<FRAMES*4;i++) dma[i]=0;
                fm1_guard_unlock_top();fm1_audio_init(dma,FRAMES*2,isr_alnk0,3);fm1_guard_lock_top();
                connection_midi_resume();fm1_irq_on();
                panel_refresh_restart();
#if MC_PERF_PROBE == 1
                mc_hw_loaded_demo=(uint8_t)demo;mc_hw_load_count++;
#endif
            }
        }
        if(ui.save_request) {
            ui.save_request=0;
            if(!engine.playing&&flash_ok) {
                audio_pause();
                ui.message=mc_store_save(&storage,&engine.project)==0?"SAVED":"SAVE FAILED";
                fm1_irq_off();
                for(unsigned i=0;i<FRAMES*4;i++) dma[i]=0;
                fm1_guard_unlock_top();fm1_audio_init(dma,FRAMES*2,isr_alnk0,3);fm1_guard_lock_top();
                connection_midi_resume();fm1_irq_on();
            } else ui.message="FLASH UNAVAILABLE";
        }
        uint16_t modes=mc_panel_led_mask(&engine,&ui,now);
        if(modes!=led_modes) {
            mc_panel_button_leds(modes,fm1_led_key);
            led_modes=modes;
        }
        if(led_playing!=engine.playing||led_step!=engine.step||led_notes!=notes) {
            led_playing=engine.playing;led_step=engine.step;
            led_notes=notes;
            mc_panel_key_leds(led_playing,led_step,led_notes,fm1_led_key);
#if MC_PERF_PROBE == 1
            mc_perf_panel(1);
#endif
        }
        panel_draw_service(milliseconds);
    }
}
