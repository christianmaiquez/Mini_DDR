#ifndef RENDER_H
#define RENDER_H

#include <SDL2/SDL.h>
#include "game.h"

/* ==========================================================================
   render.h — everything that draws pixels lives here.
   ========================================================================== */

void render_init(void);
void render_shutdown(void);

/* Triggers the big center-screen judgment flash (e.g. "PERFECT!"). */
void render_set_judgment_flash(Judgment j, double now_ms);

/* Call after a successful game_try_hit(): spawns a particle burst at the
   hit lane, and (for PERFECT only) a full-screen color flash. Also sets
   the judgment flash internally, so you don't need to call
   render_set_judgment_flash() separately for direct hits. */
void render_on_hit(int lane, Judgment j, int milestone_combo, double now_ms);

/* Draws the difficulty-select menu screen. time_ms just needs to be a
   steadily increasing clock (doesn't need to be the gameplay clock --
   the menu runs before gameplay starts). */
void render_draw_menu(SDL_Renderer *ren, double time_ms);

/* Draws one full gameplay frame. */
void render_frame(SDL_Renderer *ren, double song_time_ms,
                   const Note *notes, int note_count);

#endif /* RENDER_H */
