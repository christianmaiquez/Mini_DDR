#ifndef INPUT_H
#define INPUT_H

#include <SDL2/SDL.h>

/* ==========================================================================
   input.h — keyboard input for testing (A/S/W/D), tracked per lane.

   This module only knows about WASD right now. When we add the real FSR
   pad later, that will come in as a separate serial-input module with
   the same shape (something calls input_register_press(lane, now_ms)
   from wherever the hit came from, whether that's a keydown event or a
   parsed "HIT:x" serial message) -- so scoring and rendering won't need
   to change based on which input source triggered the press.
   ========================================================================== */

void input_init(void);

/* Call this for every SDL_KEYDOWN event. Internally checks if the key
   matches a lane binding (A/S/W/D) and records the press time if so.
   now_ms should be the current song/game clock in milliseconds. */
void input_handle_keydown(SDL_Scancode scancode, double now_ms);

/* Returns the timestamp (ms) of the most recent press for this lane,
   or -1 if it has never been pressed. Used by the renderer to drive
   the flash/glow feedback at the hit line. */
double input_get_last_press(int lane);

#endif /* INPUT_H */
