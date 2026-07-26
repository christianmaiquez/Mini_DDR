#ifndef SCORING_H
#define SCORING_H

#include "common.h"

/* ==========================================================================
   scoring.h — running score/combo totals. Pure bookkeeping: given a
   Judgment (decided by game.c), this module just tracks points and combo
   state. It doesn't know anything about notes, timing, or rendering.

   Score lives entirely in memory (a plain variable) -- it is never written
   to disk, so it always starts back at 0 the next time the program runs.
   That's what "refreshes every time the application is closed" means in
   practice: there's no save file to reset, because nothing persists.
   ========================================================================== */

void scoring_init(void);

/* Call once per judged hit (from game.c). Adds points for the judgment
   and updates combo: PERFECT/GREAT/GOOD extend the combo, BOO/MISS break
   it back to 0. JUDGE_NONE is a no-op (nothing was actually judged). */
void scoring_register(Judgment j);

int scoring_get_score(void);
int scoring_get_combo(void);
int scoring_get_max_combo(void);

/* How many times each judgment tier has occurred this session -- handy
   for an end-of-run breakdown (e.g. "12 PERFECTs, 3 GOODs, 1 MISS"). */
int scoring_get_count(Judgment j);

#endif /* SCORING_H */
