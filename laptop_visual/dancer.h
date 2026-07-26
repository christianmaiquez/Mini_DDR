#ifndef DANCER_H
#define DANCER_H

#include <SDL2/SDL.h>
#include "common.h"

/* ==========================================================================
   dancer.h — a small neon stick-figure that reacts to gameplay: strikes a
   pose matching whichever lane you just hit, and does a bigger "hype" pose
   on combo milestones. Purely decorative -- doesn't affect scoring.
   ========================================================================== */

void dancer_init(void);

/* Call this whenever a lane is successfully judged (from render_on_hit).
   lane should be 0..3 (Left/Down/Up/Right); any judgment tier triggers
   a pose, so the dancer reacts even to a Boo, just less enthusiastically
   colored than a Perfect. */
void dancer_on_hit(int lane, Judgment j, double now_ms);

/* Call this on a combo milestone -- triggers a bigger celebratory pose
   regardless of which lane was last hit. */
void dancer_trigger_hype(double now_ms);

void dancer_draw(SDL_Renderer *ren, double time_ms);

#endif /* DANCER_H */
