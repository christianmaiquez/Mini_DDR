#ifndef RENDER_H
#define RENDER_H

#include <SDL2/SDL.h>
#include "game.h"

/* ==========================================================================
   render.h — everything that draws pixels lives here. This module reads
   game state (via game_get_notes) and input state (via input_get_last_press)
   but never modifies either -- rendering is a one-way consumer of state,
   never a source of truth for it.
   ========================================================================== */

/* Draws one full frame: background, lane panels, hit line, flashing
   target arrows, and falling notes. Call once per frame between
   SDL_RenderClear-equivalent setup and SDL_RenderPresent. */
void render_frame(SDL_Renderer *ren, double song_time_ms,
                   const Note *notes, int note_count);

#endif /* RENDER_H */
