/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#ifndef MCFM_H
#define MCFM_H
#include <stdint.h>
#include "presets.h"
#define MC_VERSION "0.2.0"
#define MC_RATE 44100
#define MC_MASTER_MAX (6u*32768u)
#define MC_MASTER_DEFAULT (6u*28000u)
#define MC_OUTPUT_CEILING 30000
#define MC_VOICES 8
#define MC_DRUMS 16
#define MC_DRUM_VOICES 6
#define MC_PATTERNS 64
#define MC_STEPS 16
#define MC_PARTS 4
#define MC_PRESETS 128
#define MC_PCM_ATTACKS 24
#define MC_RECORDED_SAMPLES 16
#define MC_PCM_SOURCES (MC_PCM_ATTACKS + MC_RECORDED_SAMPLES)
#define MC_KITS 8
#define MC_AUTOMATION 384
#define MC_ARP_KEYS 32
#define MC_PROJECT_BYTES 34830
#define MC_RECORD_BYTES (MC_PROJECT_BYTES + 20)
enum { MC_LA, MC_ACID };
enum { MC_ACCENT=1, MC_SLIDE=2, MC_TIE=4 };
enum { MC_ARP_UP, MC_ARP_DOWN, MC_ARP_UPDOWN, MC_ARP_ORDER, MC_ARP_RANDOM, MC_ARP_CHORD };
enum { MC_PARAM_LEVEL=12, MC_PARAM_PAN, MC_PARAM_DELAY, MC_PARAM_REVERB,
    MC_PARAM_DRIVE, MC_PARAM_MOD, MC_PARAM_DELAY_TYPE, MC_PARAM_REVERB_TYPE,
    MC_PARAM_DRIVE_TYPE, MC_PARAM_MOD_TYPE, MC_PARAM_HPF, MC_PARAM_LPF, MC_PARAM_DRUM_LEVEL,
    MC_PARAMETERS=MC_PARAM_DRUM_LEVEL+MC_DRUMS };
typedef struct { uint8_t enabled, mode, rate, octaves, gate, latch; } McArp;
typedef struct { uint16_t address; uint8_t step, parameter, value; } McAutomation;
typedef struct {
    uint8_t notes[MC_ARP_KEYS], velocity[MC_ARP_KEYS], held[MC_ARP_KEYS], count, position, direction, sounding[3];
    uint16_t gate_remaining;
    uint32_t clock, random, events;
} McArpState;
typedef struct {
    uint8_t engine, wave, pcm, pcm_mix, cutoff, resonance, attack, decay;
    uint8_t sustain, release, glide, level;
} McPatch;
typedef struct { uint8_t note[3], velocity, gate, flags; uint16_t drums; } McStep;
typedef struct {
    uint16_t bpm;
    uint8_t swing, drum_kit;
    McPatch patch[3];
    uint8_t level[4], pan[4], drum_level[MC_DRUMS];
    uint8_t fx[4], demo, padding[3]; /* delay, reverb, drive, chorus; demo automation */
    McStep step[MC_PATTERNS][MC_PARTS][MC_STEPS];
    uint8_t track_fx[MC_PARTS][4];
    uint8_t preset[3];
    McArp arp[3];
    uint8_t fx_type[4], track_fx_type[MC_PARTS][4], filter[2], track_filter[MC_PARTS][2];
    uint8_t metronome;
    uint16_t automation_count;
    McAutomation automation[MC_AUTOMATION];
} McProject;
typedef struct {
    uint32_t phase, phase2, increment, target, pcm_pos, gate_left, age, sample_step;
    int32_t envelope, attack_step, decay_step, release_step, filter[4];
    uint8_t active, part, note, velocity, stage, accent, sample_level;
    McPatch patch;
} McVoice;
typedef struct { uint32_t phase, age; int32_t env, noise_lp; uint8_t velocity, sound, kit; } McDrum;
typedef struct {
    int16_t delay[2][8192], comb[4][2048], chorus[2][1024];
    int32_t damp[4];
    uint32_t phase;
    uint16_t delay_pos, comb_pos[4], chorus_pos;
    int32_t tape[2], phaser[2][4], reverb_ap[2], highpass[2], lowpass[2];
    uint8_t type[4], filter_amount[2];
    uint16_t filter_coef[2];
} McEffects;
typedef struct {
    McProject project;
    McVoice voice[MC_VOICES];
    McDrum drum[MC_DRUM_VOICES];
    uint32_t random, clock, elapsed, events, steals, peak, clips;
    int32_t dc_in, dc_out;
    uint8_t playing, pattern, queued, step, mute, selected, recording, edit_step;
    uint32_t master, limiter_gain, limiter_hits;
    uint32_t demo_frame;
    McEffects effects;
    McEffects track_effects[MC_PARTS];
    uint8_t track_fx_active;
    McArpState arp[3];
    uint8_t automation_full;
    uint32_t metro_phase;
    uint16_t metro_left;
    uint8_t metro_accent;
} McEngine;
extern const McPatch mc_presets[MC_PRESETS];
extern const char *const mc_preset_names[MC_PRESETS];
extern const char *const mc_preset_categories[MC_PRESETS];
extern const char *const mc_kit_names[MC_KITS];
void mc_init(McEngine *e);
void mc_demo_load(McEngine *e,unsigned demo);
void mc_demo_tick(McEngine *e);
void mc_effects(McEngine *e,int32_t *l,int32_t *r);
void mc_effects_process(McEffects *f,unsigned bpm,const uint8_t amount[4],int32_t *l,int32_t *r);
void mc_effects_process_typed(McEffects *f,unsigned bpm,const uint8_t amount[4],const uint8_t type[4],const uint8_t filter[2],int32_t *l,int32_t *r);
void mc_factory(McProject *p);
void mc_start(McEngine *e);
void mc_stop(McEngine *e);
void mc_queue(McEngine *e, unsigned pattern);
void mc_mute(McEngine *e, unsigned part);
void mc_note(McEngine *e, unsigned part, unsigned note, unsigned velocity, unsigned flags, uint32_t gate);
void mc_off(McEngine *e, unsigned part, unsigned note);
void mc_drum(McEngine *e, unsigned drum, unsigned velocity);
void mc_render(McEngine *e, int16_t *stereo, unsigned frames);
void mc_output(McEngine *e,int32_t *left,int32_t *right);
void mc_preset(McEngine *e, unsigned part, unsigned preset);
void mc_control(McEngine *e, unsigned part, unsigned knob, int delta);
void mc_key(McEngine *e,unsigned part,unsigned note,unsigned velocity,unsigned on);
void mc_arp_clear(McEngine *e,unsigned part);
int mc_parameter_get(const McEngine *e,unsigned part,unsigned parameter);
int mc_parameter_set(McEngine *e,unsigned part,unsigned parameter,unsigned value);
void mc_automation_clear(McEngine *e,unsigned pattern);
void mc_project_defaults(McProject *p);
int mc_project_valid(const McProject *p);
uint32_t mc_crc(const void *data, unsigned n);
int mc_encode(const McProject *p, uint32_t sequence, uint8_t *out, unsigned n);
int mc_decode(McProject *p, uint32_t *sequence, const uint8_t *in, unsigned n);
int mc_record_valid(const uint8_t *in,unsigned n,uint32_t *sequence);
/* Decode both committed records; corruption never modifies the destination. */
int mc_recover(McProject *p, uint32_t *sequence, const uint8_t *a, const uint8_t *b);
#endif
