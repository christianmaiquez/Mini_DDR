/* ==========================================================================
   STEP 1: FALLING ARROWS VISUAL
   --------------------------------------------------------------------------
   The simplest possible version of the game visual:
     - 4 lanes (Left, Down, Up, Right)
     - Colored blocks ("notes") spawn at the top and fall toward a
       horizontal hit line
     - Notes disappear once they pass the bottom of the screen

   No scoring, no key input yet -- this is purely the visual foundation.
   We'll add:
     Step 2: key press detection (WASD test mode)
     Step 3: scoring based on hits
     Step 4: serial input from the real FSR pad

   Build (Windows / MSYS2 MINGW64 terminal):
     gcc arrows_visual.c -o arrows_visual.exe -lmingw32 -lSDL2main -lSDL2 -lm
     ./arrows_visual.exe

   (No SDL2_ttf needed yet -- we're not drawing any text in this step.)

   Controls:
     ESC = quit
   ========================================================================== */

#include <SDL2/SDL.h>
#include <stdio.h>

/* ---------------------------- Config ------------------------------------ */

#define WINDOW_W        800
#define WINDOW_H        600
#define NUM_LANES       4
#define LANE_WIDTH      120
#define LANE_GAP        20
#define HIT_LINE_Y      500
#define NOTE_H          28
#define NOTE_SPEED_PXMS 0.35     /* pixels per millisecond fall speed */
#define SPAWN_Y         (-NOTE_H)
#define MAX_NOTES       256

/* Lane order: 0=Left, 1=Down, 2=Up, 3=Right */
static const SDL_Color LANE_COLORS[NUM_LANES] = {
    {230, 60, 60, 255},   /* left  - red    */
    {60, 200, 90, 255},   /* down  - green  */
    {70, 130, 230, 255},  /* up    - blue   */
    {230, 200, 40, 255}   /* right - yellow */
};

/* ---------------------------- Data types --------------------------------- */

typedef struct {
    int lane;
    double spawn_time_ms;
    double y;
    int active;
} Note;

/* ---------------------------- Chart ---------------------------------------
   Simple hard-coded pattern: {lane, time_ms}. This is just a placeholder
   rhythm so you have something to look at -- swap for real song timing
   later. */

typedef struct { int lane; double time_ms; } ChartEntry;

static ChartEntry CHART[] = {
    {0,  1000}, {1,  1400}, {2,  1800}, {3,  2200},
    {0,  2600}, {1,  2600}, {2,  3000}, {3,  3000},
    {0,  3600}, {2,  3600}, {1,  4000}, {3,  4000},
    {0,  4600}, {1,  4900}, {2,  5200}, {3,  5500},
};
static const int CHART_LEN = sizeof(CHART) / sizeof(CHART[0]);
static const double CHART_LOOP_MS = 6500.0; /* restart the pattern every 6.5s */

/* ---------------------------- Globals ------------------------------------ */

static Note notes[MAX_NOTES];
static int note_count = 0;
static int next_chart_index = 0;
static double chart_loop_offset_ms = 0.0;

/* ---------------------------- Helpers ------------------------------------ */

static int lane_x(int lane) {
    int total_w = NUM_LANES * LANE_WIDTH + (NUM_LANES - 1) * LANE_GAP;
    int start_x = (WINDOW_W - total_w) / 2;
    return start_x + lane * (LANE_WIDTH + LANE_GAP);
}

static void spawn_note(int lane, double spawn_time_ms) {
    if (note_count >= MAX_NOTES) return;
    Note *n = &notes[note_count++];
    n->lane = lane;
    n->spawn_time_ms = spawn_time_ms;
    n->y = SPAWN_Y;
    n->active = 1;
}

/* Pulls notes from CHART[] as the clock reaches their spawn time, looping
   the pattern forever so the demo runs indefinitely. */
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

/* ---------------------------- Main ---------------------------------------- */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "DDR Visual - Step 1: Falling Arrows",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN
    );
    if (!win) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    Uint32 start_ticks = SDL_GetTicks();
    int running = 1;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                running = 0;
            }
        }

        double song_time_ms = (double)(SDL_GetTicks() - start_ticks);
        update_spawner(song_time_ms);
        update_notes(song_time_ms);

        /* ---- render ---- */
        SDL_SetRenderDrawColor(ren, 15, 15, 25, 255);
        SDL_RenderClear(ren);

        /* lane backgrounds */
        for (int lane = 0; lane < NUM_LANES; lane++) {
            SDL_Rect lane_rect = { lane_x(lane), 0, LANE_WIDTH, WINDOW_H };
            SDL_SetRenderDrawColor(ren, 30, 30, 45, 255);
            SDL_RenderFillRect(ren, &lane_rect);

            SDL_SetRenderDrawColor(ren, 60, 60, 80, 255);
            SDL_RenderDrawRect(ren, &lane_rect);
        }

        /* hit line */
        SDL_SetRenderDrawColor(ren, 220, 220, 220, 255);
        SDL_Rect hitline = { lane_x(0), HIT_LINE_Y, lane_x(NUM_LANES - 1) + LANE_WIDTH - lane_x(0), 4 };
        SDL_RenderFillRect(ren, &hitline);

        /* falling notes */
        for (int i = 0; i < note_count; i++) {
            Note *n = &notes[i];
            if (!n->active) continue;
            SDL_Color c = LANE_COLORS[n->lane];
            SDL_Rect r = { lane_x(n->lane) + 8, (int)n->y, LANE_WIDTH - 16, NOTE_H };
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 255);
            SDL_RenderFillRect(ren, &r);
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
            SDL_RenderDrawRect(ren, &r);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(1);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
