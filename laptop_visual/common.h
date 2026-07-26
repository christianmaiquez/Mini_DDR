#ifndef COMMON_H
#define COMMON_H

/* ==========================================================================
   common.h — shared config and layout constants used across modules.
   Small constant arrays are declared "static const" here rather than
   "extern" so each .c file that includes this gets its own private copy.
   For arrays this small, that's simpler than adding a common.c just to
   define them, and avoids any linker symbol clashes.
   ========================================================================== */

#include <SDL2/SDL.h>

#define WINDOW_W        800
#define WINDOW_H        600
#define NUM_LANES       4
#define LANE_WIDTH      120
#define LANE_GAP        20
#define HIT_LINE_Y      500
#define ARROW_SIZE      48
#define NOTE_SPEED_PXMS 0.35
#define SPAWN_Y         (-ARROW_SIZE)
#define MAX_NOTES       256
#define FLASH_DURATION_MS 150.0

/* Bright neon lane colors. Lane order: 0=Left, 1=Down, 2=Up, 3=Right */
static const SDL_Color LANE_COLORS[NUM_LANES] = {
    {255,   0, 160, 255},   /* left  - neon magenta/pink */
    {  0, 230, 255, 255},   /* down  - neon cyan         */
    { 80, 255,  60, 255},   /* up    - neon green        */
    {255, 220,   0, 255}    /* right - neon yellow       */
};

static const SDL_Color BG_TOP    = { 45, 10, 70, 255 };
static const SDL_Color BG_BOTTOM = { 10,  5, 25, 255 };
static const SDL_Color GRID_COLOR = { 0, 200, 255, 40 };

/* Maps a lane index (0..3) to its horizontal pixel position on screen.
   Pure layout math -- used by both game logic (to know where a note
   should render) and the renderer itself. */
static inline int lane_x(int lane) {
    int total_w = NUM_LANES * LANE_WIDTH + (NUM_LANES - 1) * LANE_GAP;
    int start_x = (WINDOW_W - total_w) / 2;
    return start_x + lane * (LANE_WIDTH + LANE_GAP);
}

/* ---------------------------- Scoring / judgment ---------------------------
   Shared between game.c (decides which judgment a press earns), scoring.c
   (converts a judgment into points/combo effects), and render.c (shows
   the judgment as neon flash text) -- so it lives here rather than in any
   one of those modules. */

typedef enum {
    JUDGE_NONE,     /* no note was close enough to judge -- press ignored */
    JUDGE_PERFECT,
    JUDGE_GREAT,
    JUDGE_GOOD,
    JUDGE_BOO,
    JUDGE_MISS      /* note scrolled past unhit */
} Judgment;

/* Distance (pixels) from the hit line within which a press counts as
   each judgment tier. Beyond JUDGE_WINDOW_PX, a press is ignored entirely
   (treated as pressing with nothing there yet) rather than punished.
   Tune these after playtesting -- tighter = harder. */
#define PERFECT_PX      12
#define GREAT_PX        28
#define GOOD_PX         50
#define JUDGE_WINDOW_PX 80   /* BOO tier upper bound; also the auto-miss distance */

static inline int judgment_points(Judgment j) {
    switch (j) {
        case JUDGE_PERFECT: return 10;
        case JUDGE_GREAT:   return 5;
        case JUDGE_GOOD:    return 2;
        case JUDGE_BOO:     return 0;
        case JUDGE_MISS:    return 0;
        default:            return 0;
    }
}

static inline const char *judgment_label(Judgment j) {
    switch (j) {
        case JUDGE_PERFECT: return "PERFECT!";
        case JUDGE_GREAT:   return "GREAT";
        case JUDGE_GOOD:    return "GOOD";
        case JUDGE_BOO:     return "BOO";
        case JUDGE_MISS:    return "MISS";
        default:            return "";
    }
}

static inline SDL_Color judgment_color(Judgment j) {
    switch (j) {
        case JUDGE_PERFECT: return (SDL_Color){ 255,  20, 220, 255 }; /* hot magenta */
        case JUDGE_GREAT:   return (SDL_Color){   0, 230, 255, 255 }; /* cyan */
        case JUDGE_GOOD:    return (SDL_Color){  80, 255,  60, 255 }; /* green */
        case JUDGE_BOO:     return (SDL_Color){ 255, 220,   0, 255 }; /* yellow */
        case JUDGE_MISS:    return (SDL_Color){ 255,  40,  40, 255 }; /* red */
        default:            return (SDL_Color){ 255, 255, 255, 255 };
    }
}

/* Font used for HUD/score text. Change this to a .ttf you actually have if
   this default path doesn't exist on your machine (e.g. bundle a font next
   to the .exe and point this at "./font.ttf" instead). */
#define FONT_PATH "C:\\Windows\\Fonts\\arialbd.ttf"
#define FONT_SIZE_LARGE 40
#define FONT_SIZE_SMALL 22

#endif /* COMMON_H */

/* ---------------------------- Polish / effects config ---------------------- */

#define MILESTONE_STEP           25     /* show a "X COMBO!" popup every N combo */
#define BPM                      128.0  /* drives the background beat-pulse */
#define MAX_PARTICLES            300
#define PARTICLE_LIFE_MS         450.0
#define SCREEN_FLASH_DURATION_MS 150.0
#define MILESTONE_FLASH_DURATION_MS 900.0
