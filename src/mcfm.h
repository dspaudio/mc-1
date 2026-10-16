/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 dspaudio */
#ifndef MCFM_H
#define MCFM_H
#include <stdint.h>
#include "presets.h"
#include "midi.h"
#define MC_VERSION "1.0.1"
#define MC_RATE 44100
#define MC_MASTER_MAX (6u*32768u)
#define MC_MASTER_DEFAULT (6u*28000u)
#define MC_PCM_WINDOW 128
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
#define MC_SONG_ENTRIES 128
#define MC_PROJECT_BYTES 35104
#define MC_PROJECT_VERSION 7
#define MC_RECORD_BYTES (MC_PROJECT_BYTES + 20)
enum { MC_LA, MC_ACID };
enum { MC_ACCENT=1, MC_SLIDE=2, MC_TIE=4 };
enum { MC_ARP_UP, MC_ARP_DOWN, MC_ARP_UPDOWN, MC_ARP_ORDER, MC_ARP_RANDOM, MC_ARP_CHORD };
enum { MC_PARAM_LEVEL=12, MC_PARAM_PAN, MC_PARAM_DELAY, MC_PARAM_REVERB,
    MC_PARAM_DRIVE, MC_PARAM_MOD, MC_PARAM_DELAY_TYPE, MC_PARAM_REVERB_TYPE,
    MC_PARAM_DRIVE_TYPE, MC_PARAM_MOD_TYPE, MC_PARAM_HPF, MC_PARAM_LPF, MC_PARAM_DRUM_LEVEL,
    MC_PARAM_RD_SEND=MC_PARAM_DRUM_LEVEL+MC_DRUMS, MC_PARAM_CF_SEND,
    MC_PARAM_RD_TYPE, MC_PARAM_CF_TYPE, MC_PARAMETERS };
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
typedef struct { uint8_t pattern, repeats; } McSongEntry;
typedef struct { uint8_t note[3], velocity, gate, flags; uint16_t drums; } McStep;
typedef struct {
    uint16_t bpm;
    uint8_t swing, drum_kit;
    McPatch patch[3];
    uint8_t level[4], pan[4], drum_level[MC_DRUMS];
    /* Master FX amounts/types/filters stay on-wire for old projects, but are inactive. */
    uint8_t fx[4], demo, padding[3];
    McStep step[MC_PATTERNS][MC_PARTS][MC_STEPS];
    uint8_t track_fx[MC_PARTS][4];
    uint8_t preset[3];
    McArp arp[3];
    uint8_t fx_type[4], track_fx_type[MC_PARTS][4], filter[2], track_filter[MC_PARTS][2];
    uint8_t metronome;
    uint16_t automation_count;
    McAutomation automation[MC_AUTOMATION];
    McSongEntry song[MC_SONG_ENTRIES];
    uint8_t song_length, song_loop, song_mode, midi_channel[4], clock_source;
    uint8_t fx_send[MC_PARTS][2], fx_bus_type[2];
} McProject;
typedef struct {
    uint32_t phase, phase2, increment, target, pcm_pos, gate_left, age, sample_step;
    int32_t envelope, attack_step, decay_step, release_step, filter[4];
    uint8_t active, part, note, velocity, stage, accent, sample_level, midi_out;
    McPatch patch;
} McVoice;
typedef struct { uint32_t phase, age; int32_t env, noise_lp; uint8_t velocity, sound, kit; } McDrum;
/* Per-track state: drive/modulation history and the two filters. */
typedef struct {
    int16_t chorus[2][1024];
    int32_t phaser[2][4], highpass[2], lowpass[2];
    uint32_t phase;
    uint16_t chorus_pos, filter_coef[2];
    uint8_t mod_type, filter_amount[2];
} McTrackFx;
/* Delay and reverb are shared by all tracks, which feed them through their per-track amounts. */
typedef struct { int16_t delay[2][8192]; int32_t tape[2]; uint16_t delay_pos; uint8_t type; } McDelay;
typedef struct {
    int16_t comb[4][2048];
    int32_t damp[4], ap[2];
    uint16_t comb_pos[4];
    uint8_t type;
} McReverb;
/* Complete single-chain effects state, used by the reference API and tests. */
typedef struct { McTrackFx track; McDelay delay; McReverb reverb; } McEffects;
typedef struct {
    union { McDelay delay; McReverb reverb; } rd;
    McTrackFx cf;
    uint8_t rd_type, cf_type, rd_active, cf_active;
} McSharedFx;
/* 보이스별 PCM 읽기 창 캐시. count==0이면 비어 있다. 264바이트. */
typedef struct { int16_t data[MC_PCM_WINDOW]; uint16_t window, count, pcm, level; } McPcmCache;
/* Exact phase increments keyed by the source increment and Q16 bend ratio. */
typedef struct { uint32_t base, ratio, increment, second; } McPitchCache;
typedef struct McEngine {
    McProject project;
    McVoice voice[MC_VOICES];
    McDrum drum[MC_DRUM_VOICES];
    uint32_t random, clock, elapsed, events, steals, peak, clips;
    int32_t dc_in, dc_out;
    uint8_t playing, pattern, queued, step, mute, selected, recording, edit_step;
    uint32_t master, limiter_gain, limiter_hits;
    uint32_t demo_frame;
    McArpState arp[3];
    uint8_t automation_full;
    uint32_t metro_phase;
    uint16_t metro_left;
    uint8_t metro_accent;
    uint8_t song_position, song_repeat, song_finished;
    McMidiState midi;
    McPcmCache pcm_cache[MC_VOICES]; /* Runtime cache; not part of the project format. */
    /* Live mono keys: newest press wins; release returns to the highest held key. */
    uint8_t live_keys[3][16], key_slide[3];
    McPitchCache pitch_cache[MC_VOICES];
    McSharedFx shared_fx;
} McEngine;
extern const McPatch mc_presets[MC_PRESETS];
extern const char *const mc_preset_names[MC_PRESETS];
extern const char *const mc_preset_categories[MC_PRESETS];
extern const char *const mc_kit_names[MC_KITS];
extern const char *const mc_fx_rd_names[8], *const mc_fx_cf_names[8];
#if defined(MC_PERF_PROBE) && MC_PERF_PROBE == 1
extern uint8_t mc_probe_skip;
#define MC_PROBE_SKIP(bit) (mc_probe_skip&(bit))
#else
#define MC_PROBE_SKIP(bit) 0
#endif
void mc_init(McEngine *e);
/* mc_init으로 초기화한 엔진의 데모를 교체하며 이전 MIDI 출력 해제를 보존한다. */
void mc_demo_load(McEngine *e,unsigned demo);
void mc_demo_tick(McEngine *e);
void mc_effects_process(McEffects *f,unsigned bpm,const uint8_t amount[4],int32_t *l,int32_t *r);
void mc_effects_process_typed(McEffects *f,unsigned bpm,const uint8_t amount[4],const uint8_t type[4],const uint8_t filter[2],int32_t *l,int32_t *r);
/* Prepared length is the tempo-derived delay distance, clamped to 1..8191. */
void mc_effects_process_typed_length(McEffects *f,unsigned length,const uint8_t amount[4],const uint8_t type[4],const uint8_t filter[2],int32_t *l,int32_t *r);
/* Block PCM storage must not overlap effect history or parameter arrays. */
void mc_effects_process_block_length(McEffects *f,unsigned length,const uint8_t amount[4],const uint8_t type[4],const uint8_t filter[2],int32_t stereo[][2],unsigned frames);
/* Drive, modulation and filters only; delay and reverb run on the shared buses. */
void mc_effects_process_track(McTrackFx *t,const uint8_t amount[4],const uint8_t type[4],const uint8_t filter[2],int32_t stereo[][2],unsigned frames);
/* Runs only the delay stage at full amount; output is input plus the delayed signal. */
void mc_effects_process_delay_bus(McDelay *d,unsigned length,unsigned type,int32_t stereo[][2],unsigned frames);
/* Runs the shared reverb on a mono input bus and writes its unscaled stereo wet output. */
void mc_reverb_block(McReverb *r,unsigned mode,int active,const int32_t *input,int32_t wet[][2],unsigned frames);
/* 공용 버스의 입력을 wet 출력으로 대체한다. 직접음은 엔진에서 별도로 합친다. */
void mc_fx_rd_block(McSharedFx *f,unsigned bpm,unsigned type,int32_t stereo[][2],unsigned frames);
void mc_fx_cf_block(McSharedFx *f,unsigned type,int32_t stereo[][2],unsigned frames);
void mc_factory(McProject *p);
void mc_start(McEngine *e);
void mc_stop(McEngine *e);
void mc_continue(McEngine *e);
int mc_song_set(McEngine *e,unsigned index,unsigned pattern,unsigned repeats);
int mc_song_length(McEngine *e,unsigned length);
void mc_song_mode(McEngine *e,unsigned mode);
void mc_clock_source(McEngine *e,unsigned source);
void mc_clock_tick(McEngine *e);
void mc_queue(McEngine *e, unsigned pattern);
void mc_mute(McEngine *e, unsigned part);
void mc_note(McEngine *e, unsigned part, unsigned note, unsigned velocity, unsigned flags, uint32_t gate);
void mc_off(McEngine *e, unsigned part, unsigned note);
void mc_drum(McEngine *e, unsigned drum, unsigned velocity);
void mc_render(McEngine *e, int16_t *stereo, unsigned frames);
void mc_output(McEngine *e,int32_t *left,int32_t *right);
void mc_preset(McEngine *e, unsigned part, unsigned preset);
void mc_control(McEngine *e, unsigned part, unsigned knob, int delta);
/* Overlapping ACID keys use legato when GLIDE > 0; zero disables live glide. */
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
