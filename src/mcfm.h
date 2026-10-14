/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#ifndef MCFM_H
#define MCFM_H
#include <stdint.h>
#include "presets.h"
#define MC_VERSION "0.1.3"
#define MC_RATE 44100
#define MC_VOICES 8
#define MC_DRUMS 16
#define MC_DRUM_VOICES 6
#define MC_PATTERNS 4
#define MC_STEPS 16
#define MC_PARTS 4
#define MC_PRESETS 128
#define MC_PCM_ATTACKS 24
#define MC_RECORDED_SAMPLES 16
#define MC_PCM_SOURCES (MC_PCM_ATTACKS + MC_RECORDED_SAMPLES)
#define MC_KITS 8
#define MC_PROJECT_BYTES 2136
#define MC_RECORD_BYTES (MC_PROJECT_BYTES + 20)
enum { MC_LA, MC_ACID };
enum { MC_ACCENT=1, MC_SLIDE=2 };
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
} McEffects;
typedef struct {
    McProject project;
    McVoice voice[MC_VOICES];
    McDrum drum[MC_DRUM_VOICES];
    uint32_t random, clock, elapsed, events, steals, peak, clips;
    int32_t dc_in, dc_out;
    uint8_t playing, pattern, queued, step, mute, selected, recording, edit_step;
    uint16_t master;
    uint32_t demo_frame;
    McEffects effects;
    McEffects track_effects[MC_PARTS];
    uint8_t track_fx_active;
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
void mc_factory(McProject *p);
void mc_start(McEngine *e);
void mc_stop(McEngine *e);
void mc_queue(McEngine *e, unsigned pattern);
void mc_mute(McEngine *e, unsigned part);
void mc_note(McEngine *e, unsigned part, unsigned note, unsigned velocity, unsigned flags, uint32_t gate);
void mc_off(McEngine *e, unsigned part, unsigned note);
void mc_drum(McEngine *e, unsigned drum, unsigned velocity);
void mc_render(McEngine *e, int16_t *stereo, unsigned frames);
void mc_preset(McEngine *e, unsigned part, unsigned preset);
void mc_control(McEngine *e, unsigned part, unsigned knob, int delta);
int mc_project_valid(const McProject *p);
uint32_t mc_crc(const void *data, unsigned n);
int mc_encode(const McProject *p, uint32_t sequence, uint8_t *out, unsigned n);
int mc_decode(McProject *p, uint32_t *sequence, const uint8_t *in, unsigned n);
/* Decode both committed records; corruption never modifies the destination. */
int mc_recover(McProject *p, uint32_t *sequence, const uint8_t *a, const uint8_t *b);
#endif
