#include "game.h"
#include "common.h"

/* ---------------------------- Chart ---------------------------------------
   Simple hard-coded pattern: {lane, time_ms}. Placeholder rhythm --
   swap for real song timing later (e.g. generated from actual music
   beat-mapping data). Kept private to this file -- nothing outside
   game.c needs to know the chart format. */

typedef struct { int lane; double time_ms; } ChartEntry;

static ChartEntry CHART[] = {
    {0,  1000}, {1,  1400}, {2,  1800}, {3,  2200},
    {0,  2600}, {1,  2600}, {2,  3000}, {3,  3000},
    {0,  3600}, {2,  3600}, {1,  4000}, {3,  4000},
    {0,  4600}, {1,  4900}, {2,  5200}, {3,  5500},
};
static const int CHART_LEN = sizeof(CHART) / sizeof(CHART[0]);
static const double CHART_LOOP_MS = 6500.0; /* restart the pattern every 6.5s */

/* ---------------------------- State --------------------------------------- */

static Note notes[MAX_NOTES];
static int note_count = 0;
static int next_chart_index = 0;
static double chart_loop_offset_ms = 0.0;

/* ---------------------------- Internal helpers ----------------------------- */

static void spawn_note(int lane, double spawn_time_ms) {
    if (note_count >= MAX_NOTES) return;
    Note *n = &notes[note_count++];
    n->lane = lane;
    n->spawn_time_ms = spawn_time_ms;
    n->y = SPAWN_Y;
    n->active = 1;
}

static void update_spawner(double song_time_ms) {
    while (1) {
        if (next_chart_index >= CHART_LEN) {
            next_chart_index = 0;
            chart_loop_offset_ms += CHART_LOOP_MS;
        }
        double abs_time = CHART[next_chart_index].time_ms + chart_loop_offset_ms;
        double travel_ms = HIT_LINE_Y / NOTE_SPEED_PXMS;
        if (abs_time - travel_ms <= song_time_ms) {
            spawn_note(CHART[next_chart_index].lane, abs_time);
            next_chart_index++;
        } else {
            break;
        }
    }
}

static void update_notes(double song_time_ms) {
    for (int i = 0; i < note_count; i++) {
        Note *n = &notes[i];
        if (!n->active) continue;
        double elapsed = song_time_ms - n->spawn_time_ms;
        n->y = SPAWN_Y + elapsed * NOTE_SPEED_PXMS;
        if (n->y > WINDOW_H + 50) {
            n->active = 0;
        }
    }
}

/* ---------------------------- Public API ----------------------------------- */

void game_init(void) {
    note_count = 0;
    next_chart_index = 0;
    chart_loop_offset_ms = 0.0;
}

void game_update(double song_time_ms) {
    update_spawner(song_time_ms);
    update_notes(song_time_ms);
}

const Note *game_get_notes(int *out_count) {
    if (out_count) *out_count = note_count;
    return notes;
}
