#ifndef GAME_H
#define GAME_H

#include "common.h"

/* ==========================================================================
   game.h — game state and logic: note spawning, falling, and hit judging.
   No SDL rendering calls in here -- this module only tracks *what* is
   happening in the game, not how it looks on screen.
   ========================================================================== */

typedef struct {
    int lane;
    double spawn_time_ms;
    double y;
    int active;   /* still on screen / eligible to be hit */
    int judged;    /* already scored (hit or auto-missed) -- won't be judged again */
} Note;

void game_init(void);

/* Advances the game by one frame: spawns due notes, updates their
   position, and auto-misses any note that scrolled past the hit line
   without being pressed (registering JUDGE_MISS with scoring.c internally).
   Returns JUDGE_MISS if at least one auto-miss happened this frame (so
   main.c can trigger a "MISS" flash), otherwise JUDGE_NONE.
   Note: if multiple notes auto-miss in the same frame, only one MISS
   flash is reported -- a deliberate simplification, each miss still
   counts toward score/combo regardless. */
Judgment game_update(double song_time_ms);

/* Attempts to hit the closest unjudged note in the given lane. If a note
   is within JUDGE_WINDOW_PX of the hit line, it's judged (PERFECT/GREAT/
   GOOD/BOO based on distance), scored via scoring.c, and consumed.
   If no note is close enough, returns JUDGE_NONE and nothing is consumed
   -- pressing with nothing nearby is simply ignored, not punished. */
Judgment game_try_hit(int lane, double song_time_ms);

/* Read-only access for the renderer. */
const Note *game_get_notes(int *out_count);

#endif /* GAME_H */
