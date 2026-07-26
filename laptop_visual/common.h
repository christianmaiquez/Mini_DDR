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

#endif /* COMMON_H */
