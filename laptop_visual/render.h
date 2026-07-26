#ifndef RENDER_H
#define RENDER_H

#include <SDL2/SDL.h>
#include "game.h"

/* ==========================================================================
   render.h — everything that draws pixels lives here. Reads game state
   (game_get_notes), input state (input_get_last_press), and scoring state
   (scoring_get_*) but never modifies any of them.
   ========================================================================== */

/* Sets up SDL_ttf and loads fonts. Safe to call even if the font file
   can't be found -- text will just be skipped rather than crashing, so
   a missing font never breaks the demo, it just loses the HUD text. */
void render_init(void);
void render_shutdown(void);

/* Triggers the big center-screen judgment flash (e.g. "PERFECT!") for
   a short duration starting at now_ms. Call this right after a judged
   hit (or an auto-miss) happens. */
void render_set_judgment_flash(Judgment j, double now_ms);

/* Draws one full frame: background, lane panels, hit line, flashing
   target arrows, falling notes, score/combo HUD, and judgment flash. */
void render_frame(SDL_Renderer *ren, double song_time_ms,
                   const Note *notes, int note_count);

#endif /* RENDER_H */
