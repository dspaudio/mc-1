/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct { unsigned unused; } McEngine;
typedef struct { uint8_t data[3],length; } McMidiMessage;
#define MQ 64u
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
static uint32_t midi_in_q[MQ],mi_r,mi_w,mo_r,mo_w,output[MQ];
static struct { uint32_t resets;uint8_t config,suspended; } usb;
static struct { uint32_t drops; } um;
static unsigned source_log[MQ*3],data_log[MQ*3],received,resets[3];
static McMidiMessage pending[256];static unsigned pending_n,pending_r;
static void mc_midi_byte(McEngine *e,unsigned source,uint8_t byte) {
    (void)e;assert(received<MQ*3);source_log[received]=source;data_log[received++]=byte;
}
static void mc_midi_reset(McEngine *e,unsigned source) { (void)e;assert(source==1||source==2);resets[source]++; }
static int mc_midi_pop(McEngine *e,McMidiMessage *message) {
    (void)e;if(pending_r==pending_n)return 0;*message=pending[pending_r++];return 1;
}
static void midi_out_event(uint32_t packet) { assert(usb.config&&mo_w-mo_r<MQ);output[mo_w++%MQ]=packet; }
#include "../firmware/connection/routing.c"
static void input(uint32_t packet) { assert(mi_w-mi_r<MQ);midi_in_q[mi_w++%MQ]=packet; }
static void send(unsigned status,unsigned a,unsigned b,unsigned length) {
    assert(pending_n<256);pending[pending_n++]=(McMidiMessage){{(uint8_t)status,(uint8_t)a,(uint8_t)b},(uint8_t)length};
}
int main(void) {
    McEngine engine={0};
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
    return 0;
}
