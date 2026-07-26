#ifndef SCORING_H
#define SCORING_H

#include "common.h"

/* ==========================================================================
   scoring.h — running score/combo totals. Pure bookkeeping.

   Score lives entirely in memory -- it is never written to disk, so it
   always starts back at 0 the next time the program runs.
   ========================================================================== */

void scoring_init(void);

/* Registers a judged hit. Adds points and updates combo: PERFECT/GREAT/GOOD
   extend the combo, BOO/MISS break it back to 0. JUDGE_NONE is a no-op.

   Returns the new combo value if it just crossed a MILESTONE_STEP boundary
   (e.g. 25, 50, 75...), so the caller can trigger a celebration popup.
   Returns 0 if no milestone was crossed this call. */
int scoring_register(Judgment j);

int scoring_get_score(void);
int scoring_get_combo(void);
int scoring_get_max_combo(void);
int scoring_get_count(Judgment j);

#endif /* SCORING_H */
