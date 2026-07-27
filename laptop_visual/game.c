#include <math.h>
#include <stdlib.h>
#include "game.h"
#include "scoring.h"

/* ---------------------------- Difficulty table -----------------------------
   note_speed_pxms: how fast notes fall (higher = faster/harder)
   min/max_gap_ms:   random spacing between successive note "beats"
   chord_chance:      probability [0,1] that a beat spawns 2 lanes at once */

typedef struct {
    double note_speed_pxms;
    double min_gap_ms;
    double max_gap_ms;
    double chord_chance;
} DifficultyParams;

static const DifficultyParams DIFF_PARAMS[3] = {
    /* EASY   */ { 0.22, 700.0, 1100.0, 0.05 },
    /* NORMAL */ { 0.35, 450.0,  800.0, 0.15 },
    /* HARD   */ { 0.50, 250.0,  500.0, 0.30 },
};

static Difficulty current_difficulty = DIFF_NORMAL;
static double current_note_speed = 0.35;

/* ---------------------------- State --------------------------------------- */

static Note notes[MAX_NOTES];
static int note_count = 0;

/* next_hit_time_ms is the intended arrival time (at the hit line) of the
   next randomly generated note. Notes are actually spawned earlier than
   this, timed so they visually arrive exactly at this moment -- same
   idea as the old fixed chart, just generated on the fly instead of
   read from a hard-coded array. */
static double next_hit_time_ms = 1500.0;
static int last_lane = -1;

/* ---------------------------- Internal helpers ----------------------------- */

static double rand01(void) {
    return (double)rand() / (double)RAND_MAX;
}

static void spawn_note(int lane, double spawn_time_ms) {
    if (note_count >= MAX_NOTES) return;
    Note *n = &notes[note_count++];
    n->lane = lane;
    n->spawn_time_ms = spawn_time_ms;
    n->y = SPAWN_Y;
    n->active = 1;
    n->judged = 0;
}

/* Randomly generates upcoming notes as the song clock approaches their
   scheduled arrival time. Hit times are quantised to the same 120 BPM
   eighth-note grid as the ESP32 music. No target is scheduled after the
   45-second game boundary. */
static void update_spawner(double song_time_ms) {
    double travel_ms = HIT_LINE_Y / current_note_speed;
    const DifficultyParams *dp = &DIFF_PARAMS[current_difficulty];

    while (next_hit_time_ms <= GAME_DURATION_MS &&
           next_hit_time_ms - travel_ms <= song_time_ms) {
        int lane = rand() % NUM_LANES;
        /* light anti-repeat: avoid the exact same lane twice in a row
           when there's more than one lane to choose from */
        if (NUM_LANES > 1 && lane == last_lane) {
            lane = (lane + 1 + rand() % (NUM_LANES - 1)) % NUM_LANES;
        }

        double spawn_time = next_hit_time_ms - travel_ms;
        spawn_note(lane, spawn_time);
        last_lane = lane;

        /* occasional chord: a second, different lane at the same beat */
        if (rand01() < dp->chord_chance) {
            int lane2 = rand() % NUM_LANES;
            if (lane2 != lane) {
                spawn_note(lane2, spawn_time);
            }
        }

        /* The ESP32 track is 120 BPM, so one eighth note is 250 ms.
           Quantise random gaps to that grid so note arrivals land on beats. */
        const double eighth_note_ms = 250.0;
        int min_steps = (int)ceil(dp->min_gap_ms / eighth_note_ms);
        int max_steps = (int)floor(dp->max_gap_ms / eighth_note_ms);
        if (max_steps < min_steps) max_steps = min_steps;

        int step_gap = min_steps + rand() % (max_steps - min_steps + 1);
        next_hit_time_ms += step_gap * eighth_note_ms;
    }
}

static Judgment classify_distance(double dist_px) {
    if (dist_px <= PERFECT_PX) return JUDGE_PERFECT;
    if (dist_px <= GREAT_PX)   return JUDGE_GREAT;
    if (dist_px <= GOOD_PX)    return JUDGE_GOOD;
    if (dist_px <= JUDGE_WINDOW_PX) return JUDGE_BOO;
    return JUDGE_NONE;
}

static Judgment update_notes(double song_time_ms) {
    Judgment miss_this_frame = JUDGE_NONE;

    for (int i = 0; i < note_count; i++) {
        Note *n = &notes[i];
        if (!n->active) continue;

        double elapsed = song_time_ms - n->spawn_time_ms;
        n->y = SPAWN_Y + elapsed * current_note_speed;

        if (!n->judged && n->y > HIT_LINE_Y + JUDGE_WINDOW_PX) {
            n->judged = 1;
            n->active = 0;
            scoring_register(JUDGE_MISS);
            miss_this_frame = JUDGE_MISS;
        }

        if (n->y > WINDOW_H + 50) {
            n->active = 0;
        }
    }
    return miss_this_frame;
}

/* ---------------------------- Public API ----------------------------------- */

void game_set_difficulty(Difficulty d) {
    if (d < DIFF_EASY || d > DIFF_HARD) d = DIFF_NORMAL;
    current_difficulty = d;
    current_note_speed = DIFF_PARAMS[d].note_speed_pxms;
}

void game_init(void) {
    note_count = 0;
    next_hit_time_ms = 1500.0;
    last_lane = -1;
}

Judgment game_update(double song_time_ms) {
    update_spawner(song_time_ms);
    return update_notes(song_time_ms);
}

Judgment game_try_hit(int lane, double song_time_ms, int *out_milestone) {
    if (out_milestone) *out_milestone = 0;

    int best_idx = -1;
    double best_dist = 1e18;

    for (int i = 0; i < note_count; i++) {
        Note *n = &notes[i];
        if (!n->active || n->judged || n->lane != lane) continue;
        double dist = fabs(n->y - HIT_LINE_Y);
        if (dist < best_dist) {
            best_dist = dist;
            best_idx = i;
        }
    }

    if (best_idx == -1) return JUDGE_NONE;

    Judgment j = classify_distance(best_dist);
    if (j == JUDGE_NONE) return JUDGE_NONE;

    Note *n = &notes[best_idx];
    n->judged = 1;
    n->active = 0;

    int milestone = scoring_register(j);
    if (out_milestone) *out_milestone = milestone;

    return j;
}

const Note *game_get_notes(int *out_count) {
    if (out_count) *out_count = note_count;
    return notes;
}

int game_has_active_notes(void) {
    for (int i = 0; i < note_count; i++) {
        if (notes[i].active && !notes[i].judged) return 1;
    }
    return 0;
}
