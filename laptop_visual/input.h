#ifndef INPUT_H
#define INPUT_H

#include <SDL2/SDL.h>

/* ==========================================================================
   input.h — keyboard input for testing (A/S/W/D), tracked per lane.

   Keyboard events are mapped here. ESP32 serial events are parsed by
   serial_input.c, then registered here by lane so both input sources share
   the same renderer feedback and game/scoring path.
   ========================================================================== */

void input_init(void);

/* Records a press timestamp for whichever lane this key maps to (used by
   the renderer for the flash/glow feedback at the hit line). Call this
   for every SDL_KEYDOWN event; it's a no-op if the key isn't bound. */
void input_handle_keydown(SDL_Scancode scancode, double now_ms);

/* Records a lane press from any input source, including the ESP32 FSR pad.
   This gives serial hits the same lane flash/glow as keyboard hits. */
void input_register_lane_press(int lane, double now_ms);

/* Returns the lane index (0..3) this scancode is bound to, or -1 if it
   isn't a lane key. main.c uses this to know whether/which lane to pass
   to game_try_hit(). */
int input_lane_for_scancode(SDL_Scancode scancode);

/* Returns the timestamp (ms) of the most recent press for this lane,
   or -1 if it has never been pressed. */
double input_get_last_press(int lane);

#endif /* INPUT_H */
