/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../src/mcfm.h"
#define MQ 64u
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
static uint32_t midi_in_q[MQ],mi_r,mi_w,mo_r,mo_w,output[MQ];
static struct { uint32_t resets;uint8_t config,suspended,up; } usb;
static struct { uint32_t drops; } um;
static unsigned source_log[MQ*3],data_log[MQ*3],received,resets[3];
static McMidiMessage pending[256];static unsigned pending_n,pending_r;
static void tracked_midi_byte(McEngine *e,unsigned source,uint8_t byte) {
    assert(received<MQ*3);source_log[received]=source;data_log[received++]=byte;
    mc_midi_byte(e,source,byte);
}
static void tracked_midi_reset(McEngine *e,unsigned source) {
    assert(source==1||source==2);resets[source]++;mc_midi_reset(e,source);
}
static int pending_midi_pop(McEngine *e,McMidiMessage *message) {
    (void)e;if(pending_r==pending_n)return 0;*message=pending[pending_r++];return 1;
}
static void midi_out_event(uint32_t packet) { assert(usb.config&&mo_w-mo_r<MQ);output[mo_w++%MQ]=packet; }
static unsigned uart_active,uart_stops,uart_starts,uart_polls,usb_polls,ep1_reads;
static uint32_t usb_incoming,ep1_pending;
static void input(uint32_t packet) { assert(mi_w-mi_r<MQ);midi_in_q[mi_w++%MQ]=packet; }
static void uart_midi_stop(void) { uart_active=0;uart_stops++; }
static void uart_midi_start(void) { uart_active=1;uart_starts++; }
static void uart_midi_poll(void) { assert(uart_active);uart_polls++; }
static void usb_poll(void) { usb_polls++;if(usb_incoming)input(usb_incoming); }
static void ep1_rx(void) { ep1_reads++;if(ep1_pending)input(ep1_pending);ep1_pending=0; }
#define mc_midi_byte tracked_midi_byte
#define mc_midi_reset tracked_midi_reset
#define mc_midi_pop pending_midi_pop
#include "../firmware/connection/routing.c"
#undef mc_midi_byte
#undef mc_midi_reset
#undef mc_midi_pop
static void send(unsigned status,unsigned a,unsigned b,unsigned length) {
    assert(pending_n<256);pending[pending_n++]=(McMidiMessage){{(uint8_t)status,(uint8_t)a,(uint8_t)b},(uint8_t)length};
}
int main(void) {
    static McEngine engine;
    mc_init(&engine);
    input(0x643c9009u);input(0x503d9119u);input(0x0000f80fu);input(0x0007c21cu);
    connection_midi_input(&engine);
    assert(received==9&&mi_r==mi_w);
    assert(source_log[0]==1&&source_log[3]==2&&source_log[6]==1&&source_log[7]==2);
    assert(data_log[0]==0x90&&data_log[1]==60&&data_log[2]==100&&data_log[6]==0xf8&&data_log[8]==7);
    puts("PASS: exact USB/TRS source and byte order; channel, realtime, two-byte messages");
    received=0;usb.resets++;input(0x643c9009u);input(0x503d9119u);connection_midi_input(&engine);
    assert(resets[1]==1&&received==3&&source_log[0]==2);
    received=0;um.drops++;input(0x643c9009u);input(0x503d9119u);connection_midi_input(&engine);
    assert(resets[2]==1&&received==3&&source_log[0]==1);
    received=0;usb.suspended=1;input(0x643c9009u);connection_midi_input(&engine);assert(resets[1]==2&&!received);
    connection_midi_input(&engine);assert(resets[1]==2);usb.suspended=0;
    puts("PASS: bus reset/disconnect and UART overflow panic affected source and discard stale messages");
    usb.config=1;send(0x90,60,100,3);send(0xc2,7,0,2);send(0xf8,0,0,1);connection_midi_output(&engine);
    assert(mo_w==3&&output[0]==0x643c9009u&&output[1]==0x0007c20cu&&output[2]==0x0000f80fu);
    pending_n=pending_r=0;mo_r=mo_w;for(unsigned i=0;i<256;i++)send(0x80,60,0,3);
    connection_midi_output(&engine);assert(mo_w-mo_r==MQ&&pending_r==MQ);
    connection_midi_output(&engine);assert(pending_r==MQ);
    mo_r=mo_w;connection_midi_output(&engine);assert(pending_r==MQ*2);
    usb.config=0;connection_midi_output(&engine);assert(pending_r==pending_n);
    pending_n=pending_r=0;send(0x90,60,100,3);usb.config=usb.suspended=1;
    unsigned before=mo_w;connection_midi_output(&engine);assert(pending_r==1&&mo_w==before);
    puts("PASS: USB output packet identity, bounded FIFO backpressure, disconnected/suspended output drained without stale sends");

    mc_init(&engine);received=0;usb.up=usb.config=1;usb.suspended=0;
    engine.project.arp[1].enabled=1;
    input(0x7f40b01bu);input(0x643c9019u);input(0x003c8018u);input(0x64409109u);
    connection_midi_input(&engine);
    mc_midi_byte(&engine,1,0x90);mc_midi_byte(&engine,1,62);
    assert(engine.midi.sustain[0]&&engine.midi.deferred[0][60/8]&&engine.arp[1].count);
    assert(engine.midi.parser[0].count==1&&engine.midi.parser[1].status);
    static McProject saved_project;
    saved_project=engine.project;
    unsigned usb_resets=resets[1],trs_resets=resets[2];
    input(0x643e9019u);input(0x64409009u);uart_active=1;
    connection_midi_pause(&engine);
    assert(connection_paused&&!uart_active&&uart_stops==1&&mi_r==mi_w);
    assert(resets[1]==usb_resets+1&&resets[2]==trs_resets+1);
    for(unsigned p=0;p<4;p++) {
        assert(!engine.midi.sustain[p]);
        for(unsigned i=0;i<16;i++)assert(!engine.midi.held[p][i]&&!engine.midi.deferred[p][i]);
        if(p<3)assert(!engine.arp[p].count);
    }
    for(unsigned i=0;i<2;i++)assert(!engine.midi.parser[i].status&&!engine.midi.parser[i].count);
    for(unsigned i=0;i<MC_VOICES;i++)assert(!engine.voice[i].active||engine.voice[i].stage==2);
    assert(!memcmp(&saved_project,&engine.project,sizeof saved_project));
    puts("PASS: 저장 진입 시 실제 엔진의 sustain·held·ARP·양쪽 파서 해제, 이전 수신 큐 폐기");

    received=0;usb_incoming=0x64429009u;
    for(unsigned i=0;i<1000;i++)connection_midi_poll(3);
    assert(usb_polls==1000&&!uart_polls&&mi_r==mi_w&&!received);
    input(0x64449009u);ep1_pending=0x64469009u;
    connection_midi_resume();
    assert(!connection_paused&&uart_active&&uart_starts==1&&ep1_reads==1&&!ep1_pending&&mi_r==mi_w);
    connection_midi_input(&engine);assert(!received);
    usb_incoming=0;connection_midi_poll(3);assert(usb_polls==1001&&uart_polls==1);
    int16_t silence[512];
    for(unsigned i=0;i<MC_RATE/256u+1u;i++)mc_render(&engine,silence,256);
    for(unsigned i=0;i<MC_VOICES;i++)assert(!engine.voice[i].active);
    for(unsigned i=0;i<512;i++)assert(!silence[i]);
    puts("PASS: 저장 중 USB 서비스 유지·입력 폐기, 재개 직전 소프트웨어 큐와 EP1 폐기 후 지속음 없음");

    input(0x64489019u);connection_midi_input(&engine);
    assert(engine.midi.held[0][72/8]&(1u<<(72%8)));
    input(0x00488018u);connection_midi_input(&engine);
    assert(!(engine.midi.held[0][72/8]&(1u<<(72%8))));
    puts("PASS: 재개 후 새 TRS Note On/Off가 실제 엔진에 정상 전달됨");
    return 0;
}
