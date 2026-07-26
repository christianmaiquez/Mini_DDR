/* ==========================================================================
   STEP 1: FALLING ARROWS VISUAL — ARCADE / NEON STYLE
   --------------------------------------------------------------------------
   Same core mechanic as before (4 lanes, arrows fall to a hit line), now
   styled to look more like an actual arcade DDR cabinet:
     - Bright neon color palette (magenta / cyan / green / yellow)
     - Gradient background with a retro synthwave grid, not flat dark gray
     - Arrows have a black outline + soft glow for contrast and pop
     - Neon-bordered lanes with a translucent colored panel

   No scoring, no key input yet -- still purely the visual foundation.
   We'll add:
     Step 2: key press detection (WASD test mode)
     Step 3: scoring based on hits
     Step 4: serial input from the real FSR pad

   Build (Windows / MSYS2 MINGW64 terminal):
     gcc arrows_visual.c -o arrows_visual.exe -lmingw32 -lSDL2main -lSDL2 -lm
     ./arrows_visual.exe

   Requires SDL2 2.0.18+ (SDL_RenderGeometry). If you get an "undefined
   reference to SDL_RenderGeometry" linker error, update SDL2:
     pacman -Syu
     pacman -S mingw-w64-x86_64-SDL2

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
#define ARROW_SIZE      48        /* falling arrows are drawn in a square box this size */
#define NOTE_SPEED_PXMS 0.35     /* pixels per millisecond fall speed */
#define SPAWN_Y         (-ARROW_SIZE)
#define MAX_NOTES       256

/* Bright neon lane colors. Lane order: 0=Left, 1=Down, 2=Up, 3=Right */
static const SDL_Color LANE_COLORS[NUM_LANES] = {
    {255,   0, 160, 255},   /* left  - neon magenta/pink */
    {  0, 230, 255, 255},   /* down  - neon cyan         */
    { 80, 255,  60, 255},   /* up    - neon green        */
    {255, 220,   0, 255}    /* right - neon yellow       */
};

/* Background gradient: deep purple/indigo at top -> near-black indigo at
   bottom, so neon elements pop but it's not flat dark gray. */
static const SDL_Color BG_TOP    = { 45, 10, 70, 255 };
static const SDL_Color BG_BOTTOM = { 10,  5, 25, 255 };

/* Retro grid line color (dim cyan) */
static const SDL_Color GRID_COLOR = { 0, 200, 255, 40 };

/* ---------------------------- Data types --------------------------------- */

typedef struct {
    int lane;
    double spawn_time_ms;
    double y;
    int active;
} Note;

/* ---------------------------- Chart ---------------------------------------
   Simple hard-coded pattern: {lane, time_ms}. Placeholder rhythm --
   swap for real song timing later. */

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

/* ---------------------------- Arrow shape drawing -------------------------
   Builds an actual arrow silhouette (triangular head + rectangular shaft)
   using SDL_RenderGeometry. Template points below are for an UP-pointing
   arrow in a normalized 0..1 x 0..1 box; map_arrow_point() rotates it to
   whichever direction a lane needs. */

static const float ARROW_PTS[9][2] = {
    /* head triangle: tip, right-base, left-base */
    {0.5f, 0.0f}, {1.0f, 0.45f}, {0.0f, 0.45f},
    /* shaft triangle 1 */
    {0.3f, 0.45f}, {0.7f, 0.45f}, {0.7f, 1.0f},
    /* shaft triangle 2 */
    {0.3f, 0.45f}, {0.7f, 1.0f}, {0.3f, 1.0f},
};

static void map_arrow_point(int lane, float u, float v,
                             float x, float y, float w, float h,
                             float *out_x, float *out_y) {
    switch (lane) {
        case 0: /* left */
            *out_x = x + v * w;
            *out_y = y + u * h;
            break;
        case 1: /* down */
            *out_x = x + u * w;
            *out_y = y + (1.0f - v) * h;
            break;
        case 2: /* up */
            *out_x = x + u * w;
            *out_y = y + v * h;
            break;
        case 3: /* right */
        default:
            *out_x = x + (1.0f - v) * w;
            *out_y = y + u * h;
            break;
    }
}

static void draw_arrow(SDL_Renderer *ren, float x, float y, float w, float h,
                        int lane, SDL_Color color) {
    SDL_Vertex verts[9];
    for (int i = 0; i < 9; i++) {
        float px, py;
        map_arrow_point(lane, ARROW_PTS[i][0], ARROW_PTS[i][1], x, y, w, h, &px, &py);
        verts[i].position.x = px;
        verts[i].position.y = py;
        verts[i].color = color;
        verts[i].tex_coord.x = 0.0f;
        verts[i].tex_coord.y = 0.0f;
    }
    SDL_RenderGeometry(ren, NULL, verts, 9, NULL, 0);
}

/* Draws an arrow with a soft additive glow, a black outline, and a bright
   fill on top -- gives it that punchy arcade-cabinet look instead of a
   flat silhouette. */
static void draw_arrow_neon(SDL_Renderer *ren, int box_x, int box_y, int size,
                             int lane, SDL_Color color) {
    /* Glow: a few enlarged, low-alpha copies using additive blending */
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
    for (int i = 3; i >= 1; i--) {
        int grow = i * 6;
        SDL_Color glow = color;
        glow.a = (Uint8)(35 / i);
        draw_arrow(ren, (float)(box_x - grow / 2), (float)(box_y - grow / 2),
                   (float)(size + grow), (float)(size + grow), lane, glow);
    }

    /* Black outline: slightly enlarged solid black copy behind the fill */
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_Color outline = {0, 0, 0, 255};
    int ol = 5;
    draw_arrow(ren, (float)(box_x - ol / 2), (float)(box_y - ol / 2),
               (float)(size + ol), (float)(size + ol), lane, outline);

    /* Bright neon fill on top */
    draw_arrow(ren, (float)box_x, (float)box_y, (float)size, (float)size, lane, color);
}

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

/* Vertical gradient background + a faint retro grid overlay */
static void draw_background(SDL_Renderer *ren) {
    for (int row = 0; row < WINDOW_H; row++) {
        float t = (float)row / (float)WINDOW_H;
        Uint8 r = (Uint8)(BG_TOP.r + (BG_BOTTOM.r - BG_TOP.r) * t);
        Uint8 g = (Uint8)(BG_TOP.g + (BG_BOTTOM.g - BG_TOP.g) * t);
        Uint8 b = (Uint8)(BG_TOP.b + (BG_BOTTOM.b - BG_TOP.b) * t);
        SDL_SetRenderDrawColor(ren, r, g, b, 255);
        SDL_RenderDrawLine(ren, 0, row, WINDOW_W, row);
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, GRID_COLOR.r, GRID_COLOR.g, GRID_COLOR.b, GRID_COLOR.a);
    for (int x = 0; x < WINDOW_W; x += 40) {
        SDL_RenderDrawLine(ren, x, 0, x, WINDOW_H);
    }
    for (int y = 0; y < WINDOW_H; y += 40) {
        SDL_RenderDrawLine(ren, 0, y, WINDOW_W, y);
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
        "DDR Visual - Arcade Style",
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
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        draw_background(ren);

        /* lane panels: translucent neon-tinted fill + bright glowing border */
        for (int lane = 0; lane < NUM_LANES; lane++) {
            SDL_Color c = LANE_COLORS[lane];
            SDL_Rect lane_rect = { lane_x(lane), 0, LANE_WIDTH, WINDOW_H };

            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 22);
            SDL_RenderFillRect(ren, &lane_rect);

            /* glowing border: a couple of additive outline passes */
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 90);
            SDL_RenderDrawRect(ren, &lane_rect);
            SDL_Rect inset = { lane_rect.x + 1, lane_rect.y, lane_rect.w - 2, lane_rect.h };
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 50);
            SDL_RenderDrawRect(ren, &inset);
        }

        /* hit line: bright white with neon glow */
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
        for (int i = 3; i >= 1; i--) {
            SDL_SetRenderDrawColor(ren, 255, 255, 255, (Uint8)(60 / i));
            SDL_Rect glow_line = { lane_x(0), HIT_LINE_Y - i, lane_x(NUM_LANES - 1) + LANE_WIDTH - lane_x(0), 4 + i * 2 };
            SDL_RenderFillRect(ren, &glow_line);
        }
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_Rect hitline = { lane_x(0), HIT_LINE_Y, lane_x(NUM_LANES - 1) + LANE_WIDTH - lane_x(0), 4 };
        SDL_RenderFillRect(ren, &hitline);

        /* dim target arrow outlines sitting on the hit line */
        for (int lane = 0; lane < NUM_LANES; lane++) {
            SDL_Color c = LANE_COLORS[lane];
            SDL_Color dim = { c.r, c.g, c.b, 90 };
            int box_x = lane_x(lane) + (LANE_WIDTH - ARROW_SIZE) / 2;
            int box_y = HIT_LINE_Y - ARROW_SIZE / 2;
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            draw_arrow(ren, (float)box_x, (float)box_y, (float)ARROW_SIZE, (float)ARROW_SIZE, lane, dim);
        }

        /* falling notes, drawn as glowing outlined neon arrows */
        for (int i = 0; i < note_count; i++) {
            Note *n = &notes[i];
            if (!n->active) continue;
            SDL_Color c = LANE_COLORS[n->lane];
            int box_x = lane_x(n->lane) + (LANE_WIDTH - ARROW_SIZE) / 2;
            int box_y = (int)n->y;
            draw_arrow_neon(ren, box_x, box_y, ARROW_SIZE, n->lane, c);
        }

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_RenderPresent(ren);
        SDL_Delay(1);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}