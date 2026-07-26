#ifndef GAME_H
#define GAME_H

#include "common.h"

/* ==========================================================================
   game.h — game state and logic: note spawning (now randomized), falling,
   and hit judging. No SDL rendering calls in here.
   ========================================================================== */

typedef struct {
    int lane;
    double spawn_time_ms;
    double y;
    int active;
    int judged;
} Note;

typedef enum { DIFF_EASY, DIFF_NORMAL, DIFF_HARD } Difficulty;

/* Sets note speed, spawn density, and chord frequency for the given
   difficulty. Call this BEFORE game_init() (typically right after the
   player picks a difficulty on the menu screen). */
void game_set_difficulty(Difficulty d);

/* Resets game state: clears notes and restarts the random spawn clock.
   Does NOT reset difficulty -- call game_set_difficulty() separately if
   you want to change it. */
void game_init(void);

/* Advances the game by one frame: randomly spawns due notes, updates
   their position, and auto-misses anything that scrolled past the judge
   window unhit. Returns JUDGE_MISS if at least one auto-miss happened
   this frame (for triggering a flash), otherwise JUDGE_NONE. */
Judgment game_update(double song_time_ms);

/* Attempts to hit the closest unjudged note in the given lane. If out_milestone
   is non-NULL, it's set to the new combo value if this hit just crossed a
   MILESTONE_STEP boundary, or 0 otherwise. Returns JUDGE_NONE (no note
   consumed) if nothing was close enough to judge. */
Judgment game_try_hit(int lane, double song_time_ms, int *out_milestone);

const Note *game_get_notes(int *out_count);

#endif /* GAME_H */
