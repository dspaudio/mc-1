/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#ifndef MC_MIDI_H
#define MC_MIDI_H
#include <stdint.h>
#define MC_MIDI_QUEUE 256
typedef struct { uint8_t data[3], length; } McMidiMessage;
typedef struct { uint8_t status, data[2], count, skip; } McMidiParser;
typedef struct {
    McMidiParser parser[3];
    McMidiMessage queue[MC_MIDI_QUEUE];
    uint16_t read, write, count;
    uint32_t dropped, received[3], clock, ticks, last_tick, interval;
    uint8_t receiving, panic, have_tick, sustain[4], held[4][16], deferred[4][16];
    int16_t bend[4];
} McMidiState;
struct McEngine;
void mc_midi_byte(struct McEngine *e,unsigned source,uint8_t byte);
void mc_midi_reset(struct McEngine *e,unsigned source);
int mc_midi_pop(struct McEngine *e,McMidiMessage *message);
void mc_midi_send(struct McEngine *e,unsigned status,unsigned a,unsigned b,unsigned length);
void mc_midi_note(struct McEngine *e,unsigned part,unsigned note,unsigned velocity);
void mc_midi_release(struct McEngine *e,unsigned part,unsigned note);
void mc_midi_parameter(struct McEngine *e,unsigned part,unsigned parameter,unsigned value);
void mc_midi_sample(struct McEngine *e);
void mc_midi_all_off(struct McEngine *e,unsigned part);
extern const uint8_t mc_midi_drums[16];
#endif
