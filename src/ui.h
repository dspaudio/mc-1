/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef MC_UI_H
#define MC_UI_H
#include "mcfm.h"
enum { UI_FX,UI_SEL,UI_ENV,UI_LFO,UI_EDIT,UI_GLO,UI_HOME,UI_SAVE,UI_ARP,UI_SEQ,UI_PLAY,UI_REC,UI_OCTDN,UI_OCTUP };
typedef struct { uint8_t page, sound, preset[3], editing, save_pending, save_request, load_request, demo_choice, fx_master; int8_t octave; const char *message; } McUi;
extern const char *const mc_drum_names[16];
void mc_ui_init(McUi *u);
void mc_ui_button(McEngine *e,McUi *u,unsigned button);
void mc_ui_encoder(McEngine *e,McUi *u,unsigned encoder,int delta);
void mc_ui_key(McEngine *e,McUi *u,unsigned key,int down);
/* Host and device render the same RGB565 screen, at most eight rows at a time. */
void mc_ui_strip(const McEngine *e,const McUi *u,unsigned y,unsigned height,uint16_t *pixels);
#endif
