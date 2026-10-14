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
/* No write can reach calibration, bootloader, OTA, samples or global settings. */
#define FL_RANGE_OK(off,n) ((uint32_t)(off)>=0x99000u && (uint32_t)(off)<=0xab000u && (uint32_t)(n)<=0xab000u-(uint32_t)(off))
#include "fm1_flash.h"
#include "libc.c"
#include "lcd.c"
#include "../src/effects.c"
#include "../src/demo.c"
#include "../src/engine.c"
#include "../src/factory.c"
#include "../src/project.c"
#include "../src/store.c"
#include "../src/ui.c"
#define FRAMES 128u
static McEngine engine __attribute__((section(".pool")));
static McUi ui;
static int32_t dma[FRAMES*4] __attribute__((aligned(4)));
static int16_t block[FRAMES*2];
static uint16_t strip[240*8];
static volatile uint32_t milliseconds, max_audio_us, deadline_misses;
static uint8_t flash_ok;
extern void isr_alnk0(void),isr_timer5(void);
void fm1_timer5_irq(void) {
    static uint32_t last,acc;
    fm1_timer5_ack();fm1_input_tick();
    uint32_t now=fm1_ticks();acc+=now-last;last=now;
    while(acc>=24000u) { acc-=24000u;milliseconds++; }
}
void fm1_alnk0_irq(void) {
    uint8_t p=fm1_audio_pending();fm1_audio_ack_aux(p);
    if(p&FM1_AUDIO_HALF) {
        uint32_t start=fm1_ticks(),half=fm1_audio_free_half();
        mc_render(&engine,block,FRAMES);
        /* Retain inherited 24-bit format and -6 dB hardware output ceiling. */
        for(unsigned i=0;i<FRAMES*2;i++) dma[half*FRAMES*2+i]=(int32_t)block[i]*128;
        fm1_audio_ack_half();
        uint32_t us=(fm1_ticks()-start)/24;
        if(us>max_audio_us) max_audio_us=us;
        if(us>=FRAMES*1000000u/MC_RATE) deadline_misses++;
    }
}
static int read_flash(uint32_t off,void *out,unsigned n) {
    uint8_t *p=out;
    if(!FL_RANGE_OK(off,n)) return -1;
    while(n) {
        unsigned k=n>256?256:n;
        fm1_irq_off();int rc=FL_FAR(fl_read_ram)(off,p,k);fm1_irq_on();
        if(rc) return rc;
        p+=k;off+=k;n-=k;
    } return 0;
}
static int erase_flash(uint32_t off) { uint32_t took;fm1_wdt_feed();return fl_erase4k(off,&took); }
static int program_flash(uint32_t off,const void *data,unsigned n) { return fl_write(off,data,n); }
static const McStorage storage={read_flash,erase_flash,program_flash};
static void draw(void) {
    for(unsigned y=0;y<240;y+=8) {
        lcd_sync();mc_ui_strip(&engine,&ui,y,8,strip);
        for(unsigned i=0;i<240*8;i++) strip[i]=(uint16_t)((strip[i]>>8)|(strip[i]<<8));
        lcd_blit(0,y,240,8,strip);
    }
}
static void fm1_fault(const fm1_crash_t *crash) {
    (void)crash;fm1_audio_stop();fm1_guard_unlock_top();fm1_enter_uboot();
}
extern uint32_t _data_start[],_data_end[],_data_load[],_bss_start[],_bss_end[];
extern uint32_t _pool_start[],_pool_end[],_rt_start[],_rt_end[],_rt_load[];
void fm1_cstart(void) {
    uint32_t *d,*s;
    fm1_irq_off();fm1_time_init();fm1_wdt_arm(0x0d);fm1_irq_init();
    for(d=_bss_start;d<_bss_end;d++) *d=0;
    for(d=_pool_start;d<_pool_end;d++) *d=0;
    for(s=_data_load,d=_data_start;d<_data_end;s++,d++) *d=*s;
    for(s=_rt_load,d=_rt_start;d<_rt_end;s++,d++) *d=*s;
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK|FM1_GUARD_WRITE|FM1_GUARD_BUS|FM1_GUARD_PC);
    mc_init(&engine);mc_ui_init(&ui);lcd_init();fm1_input_init();fm1_adc_init();
    flash_ok=FL_FAR(fl_jedec_ram)()==0x856014u;
    /* Initialise all vectors before allowing any foreground flash read to restore IRQs. */
    fm1_timer5_start(isr_timer5,4);fm1_audio_init(dma,FRAMES*2,isr_alnk0,3);
    fm1_guard_lock_top();fm1_irq_enable_all();
    if(flash_ok) { uint32_t seq; (void)mc_store_load(&storage,&engine.project,&seq); }
    uint32_t notes=0,both_since=0,frame=0;
    uint8_t held_part[27]={0},held_note[27]={0};
    static const uint8_t buttons[14]={2,3,4,5,6,7,8,9,10,11,12,13,0,1};
    static const uint8_t encoders[7]={0,1,6,2,3,4,5};
    for(;;) {
        fm1_wdt_feed();uint32_t now=milliseconds;mc_ui_tick(&ui,now);
        uint32_t pressed=fm1_input_edges(0),current=fm1_in.notes;
        for(unsigned i=0;i<14;i++) if(pressed&(1u<<buttons[i])) {
            fm1_irq_off();mc_ui_button(&engine,&ui,i);fm1_irq_on();
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
            } else if(held_part[i]<3) mc_key(&engine,held_part[i],held_note[i],100,0);
            fm1_irq_on();
        }
        notes=current;
        if((fm1_in.buttons&3u)==3u&&!engine.playing) {
            if(!both_since) both_since=now;
            if(now-both_since>=5000) { fm1_audio_stop();fm1_guard_unlock_top();fm1_enter_uboot(); }
        } else both_since=0;
        int volume=fm1_adc_read(FM1_ADC_MASTER);
        if(volume>=0) {
            unsigned value=(unsigned)limit(volume,0,1023);
            engine.master=(uint32_t)((uint64_t)value*value*MC_MASTER_MAX/(1023u*1023u));
        }
        if(ui.load_request) {
            unsigned demo=ui.load_request-1;ui.load_request=0;
            if(!engine.playing) {
                fm1_audio_stop();mc_demo_load(&engine,demo);mc_ui_init(&ui);ui.message="DEMO LOADED - PLAY";
                fm1_irq_off();for(unsigned i=0;i<FRAMES*4;i++) dma[i]=0;
                fm1_guard_unlock_top();fm1_audio_init(dma,FRAMES*2,isr_alnk0,3);fm1_guard_lock_top();fm1_irq_on();
            }
        }
        if(ui.save_request) {
            ui.save_request=0;
            if(!engine.playing&&flash_ok) {
                fm1_audio_stop();
                ui.message=mc_store_save(&storage,&engine.project)==0?"SAVED":"SAVE FAILED";
                fm1_irq_off();
                for(unsigned i=0;i<FRAMES*4;i++) dma[i]=0;
                fm1_guard_unlock_top();fm1_audio_init(dma,FRAMES*2,isr_alnk0,3);fm1_guard_lock_top();fm1_irq_on();
            } else ui.message="FLASH UNAVAILABLE";
        }
        fm1_led_key(12,engine.playing);fm1_led_key(13,engine.recording);
        for(unsigned i=0;i<16;i++) fm1_led_key(14+i,engine.playing&&engine.step==i);
        if(now-frame>=50) { draw();frame=now; }
    }
}
