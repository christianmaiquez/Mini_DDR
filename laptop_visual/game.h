#ifndef GAME_H
#define GAME_H

/* ==========================================================================
   game.h — game state and logic: note spawning, falling, and (later)
   scoring. Deliberately has no SDL rendering calls in it -- this module
   only tracks *what* is happening in the game, not how it looks on
   screen. That separation is what let us swap the TFT for a laptop
   window without touching any of this logic.
   ========================================================================== */

typedef struct {
    int lane;
    double spawn_time_ms;
    double y;
    int active;
} Note;

/* Resets game state (called once at startup). */
void game_init(void);

/* Advances the game by one frame: spawns any notes that are due, and
   updates the vertical position of all active notes. Call this once
   per frame with the current song clock in milliseconds. */
void game_update(double song_time_ms);

/* Returns a read-only pointer to the current notes array and writes the
   active count into *out_count. The renderer uses this to know what to
   draw; nothing outside game.c should modify notes directly. */
const Note *game_get_notes(int *out_count);

#endif /* GAME_H */
