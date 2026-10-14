/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
static volatile uint8_t connection_paused;

/* 오디오 IRQ와 전역 IRQ를 막은 뒤 호출한다. 저장 중 놓치는 Note Off를 선제 해제한다. */
static void connection_midi_pause(McEngine *engine) {
    connection_paused=1;
    uart_midi_stop();
    mi_r=mi_w;
    mc_midi_reset(engine,1);mc_midi_reset(engine,2);
}

/* TIMER5는 저장 중에도 USB를 서비스하되, 연주 입력은 재개 전까지 폐기한다. */
static void connection_midi_poll(unsigned due) {
    if(due&1u)usb_poll();
    if(connection_paused) { mi_r=mi_w;return; }
    if(due&2u)uart_midi_poll();
}

/* 전역 IRQ가 꺼진 상태에서 소프트웨어 큐와 마지막 EP1 패킷을 함께 비운다. */
static void connection_midi_resume(void) {
    mi_r=mi_w;
    if(usb.up&&usb.config)ep1_rx();
    mi_r=mi_w;
    uart_midi_start();
    connection_paused=0;
}

/* 오디오 ISR 전용. TIMER5가 수신을 게시하며 저장 중에는 엔진 소비를 멈춘다. */
static void connection_midi_input(McEngine *engine) {
    static uint32_t resets,drops;
    static uint8_t suspended;
    unsigned usb_lost=resets!=usb.resets||(!suspended&&usb.suspended);
    unsigned trs_lost=drops!=um.drops;
    if(usb_lost)mc_midi_reset(engine,1);
    if(trs_lost)mc_midi_reset(engine,2);
    resets=usb.resets;drops=um.drops;suspended=usb.suspended;
    for(unsigned count=0;count<MQ&&mi_r!=mi_w;count++) {
        RING_PUBLISH();
        uint32_t packet=midi_in_q[mi_r%MQ];
        RING_PUBLISH();mi_r++;
        unsigned cin=packet&15u,source=(packet&0xF0u)?2u:1u;
        if((source==1&&(usb_lost||usb.suspended))||(source==2&&trs_lost))continue;
        unsigned length=cin==15?1u:cin==12||cin==13?2u:3u;
        for(unsigned i=0;i<length;i++)mc_midi_byte(engine,source,(uint8_t)(packet>>(8u*(i+1))));
    }
}
static void connection_midi_output(McEngine *engine) {
    McMidiMessage message;
    for(unsigned count=0;count<256u;count++) {
        if(usb.config&&!usb.suspended&&mo_w-mo_r>=MQ)break;
        if(!mc_midi_pop(engine,&message))break;
        if(!usb.config||usb.suspended)continue;
        unsigned status=message.data[0];
        unsigned cin=status>=0xF8u?15u:status==0xF2u?3u:status>=0xF0u?2u:status>>4;
        uint32_t packet=cin|(uint32_t)status<<8;
        if(message.length>1)packet|=(uint32_t)message.data[1]<<16;
        if(message.length>2)packet|=(uint32_t)message.data[2]<<24;
        midi_out_event(packet);
    }
}
