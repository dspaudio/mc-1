/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MC_UI_H
#define MC_UI_H
#include "mcfm.h"
enum { UI_FX,UI_SEL,UI_ENV,UI_LFO,UI_EDIT,UI_GLO,UI_HOME,UI_SAVE,UI_ARP,UI_SEQ,UI_PLAY,UI_REC,UI_OCTDN,UI_OCTUP };
enum { UI_FX_SENDS,UI_FX_TYPES };
typedef struct { uint8_t page, sound, preset[3], editing, save_pending, save_request, load_request, demo_choice, fx_page; int8_t octave; const char *message;
    uint32_t preset_at, encoder_at, encoder_context;
    uint8_t preset_popup, encoder_active, encoder_last, song_cursor, song_editing;
    int8_t encoder_direction;
    uint8_t pitch_low[3], pitch_high[3], pitch_ready;
    uint32_t pattern_keys;
    uint8_t pattern_hold, pattern_used, pattern_bank;
} McUi;
extern const char *const mc_drum_names[16];
void mc_ui_init(McUi *u);
void mc_ui_button(McEngine *e,McUi *u,unsigned button);
/* Physical edges: OCT- taps transpose on release; held chords select patterns. */
void mc_ui_button_state(McEngine *e,McUi *u,unsigned button,int down);
void mc_ui_encoder(McEngine *e,McUi *u,unsigned encoder,int delta);
void mc_ui_encoder_at(McEngine *e,McUi *u,unsigned encoder,int delta,uint32_t now_ms);
void mc_ui_tick(McUi *u,uint32_t now_ms);
void mc_ui_key(McEngine *e,McUi *u,unsigned key,int down);
/* Note-on velocity is 1..127; release ignores velocity. */
void mc_ui_key_velocity(McEngine *e,McUi *u,unsigned key,unsigned velocity,int down);
/* Optional once-per-frame pitch-range cache; projects and engine state are untouched. */
void mc_ui_prepare_frame(const McEngine *e,McUi *u);
/* Host and device render the same RGB565 screen, at most eight rows at a time. */
void mc_ui_strip(const McEngine *e,const McUi *u,unsigned y,unsigned height,uint16_t *pixels);
#endif
