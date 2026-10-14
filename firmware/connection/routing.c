/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
/* Audio ISR only. TIMER5 publishes RX; all engine mutation remains in audio. */
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
