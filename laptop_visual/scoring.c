#include "scoring.h"

static int score = 0;
static int combo = 0;
static int max_combo = 0;
static int judgment_counts[6] = {0};

void scoring_init(void) {
    score = 0;
    combo = 0;
    max_combo = 0;
    for (int i = 0; i < 6; i++) judgment_counts[i] = 0;
}

int scoring_register(Judgment j) {
    if (j == JUDGE_NONE) return 0;

    judgment_counts[j]++;
    score += judgment_points(j);

    int milestone = 0;

    if (j == JUDGE_PERFECT || j == JUDGE_GREAT || j == JUDGE_GOOD) {
        combo++;
        if (combo > max_combo) max_combo = combo;
        if (combo > 0 && combo % MILESTONE_STEP == 0) {
            milestone = combo;
        }
    } else { /* BOO or MISS breaks the combo */
        combo = 0;
    }

    return milestone;
}

int scoring_get_score(void)      { return score; }
int scoring_get_combo(void)      { return combo; }
int scoring_get_max_combo(void)  { return max_combo; }

int scoring_get_count(Judgment j) {
    if (j < 0 || j > JUDGE_MISS) return 0;
    return judgment_counts[j];
}
