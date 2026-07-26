/* ==========================================================================
   DDR VISUAL — laptop fallback display for the arcade DDR project
   --------------------------------------------------------------------------
   Purpose:
     Standalone C/SDL2 program that renders the same game visuals your TFT
     was going to show: 4 scrolling note lanes, a hit line, live score,
     combo counter, and hit-judgment feedback (PERFECT / GOOD / MISS).

     Use this as a backup demo if your physical TFT isn't working — same
     game logic, just displayed in a window on your laptop instead of the
     ST7735.

   Controls:
     A / S / W / D  =  Left / Down / Up / Right  =  lanes 0,1,2,3
     ESC             =  quit
     Each correct keypress adds 1 to your score (simple counter, not
     tied to note timing).

   Build (Linux/macOS with SDL2 + SDL2_ttf installed):
     gcc ddr_visual.c -o ddr_visual -lSDL2 -lSDL2_ttf -lm
     ./ddr_visual

   Build (macOS with Homebrew):
     brew install sdl2 sdl2_ttf
     gcc ddr_visual.c -o ddr_visual -I/opt/homebrew/include -L/opt/homebrew/lib -lSDL2 -lSDL2_ttf -lm

   Build (Windows / MSYS2):
     pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf
     gcc ddr_visual.c -o ddr_visual.exe -lSDL2 -lSDL2_ttf -lm

   Notes:
     - If SDL2_ttf or a font file can't be loaded, the program still runs —
       it just skips text rendering and shows lanes/notes/judgment colors
       only. This means a missing font will NOT crash your demo.
     - FONT_PATH below defaults to a common Linux path. Change it to a
       .ttf file you actually have (e.g. bundle one next to the binary and
       set FONT_PATH to "./font.ttf").
   ========================================================================== */

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ---------------------------- Serial config ------------------------------
   Set this to the COM port your ESP32 shows up as. Check in:
   Windows Device Manager -> Ports (COM & LPT) -> "USB-SERIAL (COMx)"
   or in the Arduino IDE / PlatformIO port dropdown.
   --------------------------------------------------------------------- */
#define SERIAL_PORT "\\\\.\\COM5"   /* <-- CHANGE THIS to match your ESP32 */
#define SERIAL_BAUD CBR_115200

/* ---------------------------- Config ------------------------------------ */

#define WINDOW_W        800
#define WINDOW_H        600
#define NUM_LANES       4
#define LANE_WIDTH      120
#define LANE_GAP        20
#define HIT_LINE_Y      500
#define NOTE_H          28
#define NOTE_SPEED_PXMS 0.35     /* pixels per millisecond note fall speed */
#define SPAWN_Y         (-NOTE_H)

#define HIT_WINDOW_PERFECT_PX 18
#define HIT_WINDOW_GOOD_PX    45
#define MISS_PAST_PX           40   /* how far past hit line before auto-miss */

#define MAX_NOTES 256
#define FONT_PATH "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
#define FONT_SIZE_LARGE 36
#define FONT_SIZE_SMALL 22

/* Lane key bindings, in lane order 0..3
   A = left, S = down, W = up, D = right */
static const SDL_Scancode LANE_KEYS[NUM_LANES] = {
    SDL_SCANCODE_A,
    SDL_SCANCODE_S,
    SDL_SCANCODE_W,
    SDL_SCANCODE_D
};

static const char *LANE_LABELS[NUM_LANES] = { "A", "S", "W", "D" };

/* Lane colors (R,G,B) */
static const SDL_Color LANE_COLORS[NUM_LANES] = {
    {230, 60, 60, 255},   /* left  - red    */
    {60, 200, 90, 255},   /* down  - green  */
    {70, 130, 230, 255},  /* up    - blue   */
    {230, 200, 40, 255}   /* right - yellow */
};

/* ---------------------------- Data types --------------------------------- */

typedef struct {
    int lane;
    double spawn_time_ms;   /* when this note should appear at SPAWN_Y */
    double y;                /* current on-screen y position */
    int active;               /* still alive / not yet judged */
    int judged;               /* has been hit or missed */
} Note;

typedef enum { JUDGE_NONE, JUDGE_PERFECT, JUDGE_GOOD, JUDGE_MISS } Judgment;

typedef struct {
    int score;
    int combo;
    int max_combo;
    Judgment last_judgment;
    double last_judgment_time_ms;
} GameState;

/* ---------------------------- Chart / notes ------------------------------ */
/* A simple hard-coded note chart for demo purposes: {lane, time_ms}.
   Replace/generate this from your actual song timing data. */

typedef struct { int lane; double time_ms; } ChartEntry;

static ChartEntry CHART[] = {
    {0,  1000}, {1,  1400}, {2,  1800}, {3,  2200},
    {0,  2600}, {1,  2600}, {2,  3000}, {3,  3000},
    {0,  3600}, {2,  3600}, {1,  4000}, {3,  4000},
    {0,  4600}, {1,  4900}, {2,  5200}, {3,  5500},
    {0,  6000}, {1,  6000}, {2,  6000}, {3,  6000}, /* jump chord */
};
static const int CHART_LEN = sizeof(CHART) / sizeof(CHART[0]);
static const double CHART_LOOP_MS = 7000.0; /* loop the chart every 7s for demo */

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
    n->judged = 0;
}

/* Pull notes from the chart as the song clock reaches their spawn window.
   Loops the chart continuously so the demo runs indefinitely. */
static void update_spawner(double song_time_ms) {
    while (1) {
        if (next_chart_index >= CHART_LEN) {
            next_chart_index = 0;
            chart_loop_offset_ms += CHART_LOOP_MS;
        }
        double abs_time = CHART[next_chart_index].time_ms + chart_loop_offset_ms;
        /* spawn slightly early so the note has travel time to reach the line */
        double travel_ms = HIT_LINE_Y / NOTE_SPEED_PXMS;
        if (abs_time - travel_ms <= song_time_ms) {
            spawn_note(CHART[next_chart_index].lane, abs_time);
            next_chart_index++;
        } else {
            break;
        }
    }
}

static void update_notes(double song_time_ms, GameState *gs) {
    for (int i = 0; i < note_count; i++) {
        Note *n = &notes[i];
        if (!n->active) continue;

        double elapsed = song_time_ms - n->spawn_time_ms;
        n->y = SPAWN_Y + elapsed * NOTE_SPEED_PXMS;

        /* auto-miss if it scrolled well past the hit line unjudged */
        if (!n->judged && n->y > HIT_LINE_Y + MISS_PAST_PX) {
            n->judged = 1;
            n->active = 0;
            gs->combo = 0;
            gs->last_judgment = JUDGE_MISS;
            gs->last_judgment_time_ms = song_time_ms;
        }

        if (n->y > WINDOW_H + 50) {
            n->active = 0;
        }
    }
}

/* Handle a key press for a given lane: find the closest unjudged note
   in that lane and score it based on distance from the hit line. */
static void try_hit(int lane, double song_time_ms, GameState *gs) {
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

    if (best_idx == -1) return; /* nothing to hit in this lane */

    Note *n = &notes[best_idx];

    if (best_dist <= HIT_WINDOW_PERFECT_PX) {
        n->judged = 1;
        n->active = 0;
        gs->score += 100;
        gs->combo += 1;
        if (gs->combo > gs->max_combo) gs->max_combo = gs->combo;
        gs->last_judgment = JUDGE_PERFECT;
        gs->last_judgment_time_ms = song_time_ms;
    } else if (best_dist <= HIT_WINDOW_GOOD_PX) {
        n->judged = 1;
        n->active = 0;
        gs->score += 50;
        gs->combo += 1;
        if (gs->combo > gs->max_combo) gs->max_combo = gs->combo;
        gs->last_judgment = JUDGE_GOOD;
        gs->last_judgment_time_ms = song_time_ms;
    }
    /* if outside GOOD window, ignore the press (do not penalize early taps) */
}

/* ---------------------------- Scoring helper ------------------------------
   Shared by both keyboard (WASD test mode) and serial (real FSR pad) input,
   so both paths score identically. */
static void register_hit(int lane, GameState *gs, double now_ms) {
    if (lane < 0 || lane >= NUM_LANES) return;
    gs->score += 1;
    gs->combo += 1;
    if (gs->combo > gs->max_combo) gs->max_combo = gs->combo;
    gs->last_judgment = JUDGE_PERFECT; /* reused purely for the on-screen flash */
    gs->last_judgment_time_ms = now_ms;
}

/* ---------------------------- Serial (FSR pad) input ----------------------
   Opens the ESP32's USB-serial port and reads "HIT:<lane>\n" lines as they
   arrive. Non-blocking: if the port isn't available, the game still runs
   fine on keyboard input alone -- this is intentional so you can test the
   visuals without hardware plugged in. */

#ifdef _WIN32
static HANDLE g_serial = INVALID_HANDLE_VALUE;
static char g_serial_buf[256];
static int g_serial_buf_len = 0;

static int serial_open(void) {
    g_serial = CreateFileA(SERIAL_PORT, GENERIC_READ, 0, NULL,
                            OPEN_EXISTING, 0, NULL);
    if (g_serial == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Could not open %s (continuing with keyboard input only)\n", SERIAL_PORT);
        return 0;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(g_serial, &dcb)) {
        fprintf(stderr, "GetCommState failed, closing serial port\n");
        CloseHandle(g_serial);
        g_serial = INVALID_HANDLE_VALUE;
        return 0;
    }
    dcb.BaudRate = SERIAL_BAUD;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    SetCommState(g_serial, &dcb);

    /* Non-blocking-style reads: return immediately with whatever is
       available rather than waiting for a full buffer. */
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = 0;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    SetCommTimeouts(g_serial, &timeouts);

    printf("Serial port %s opened.\n", SERIAL_PORT);
    return 1;
}

static void serial_close(void) {
    if (g_serial != INVALID_HANDLE_VALUE) {
        CloseHandle(g_serial);
        g_serial = INVALID_HANDLE_VALUE;
    }
}

/* Reads any available bytes, splits on newlines, and parses "HIT:<lane>".
   Calls register_hit() for each complete hit message found. */
static void serial_poll(GameState *gs, double now_ms) {
    if (g_serial == INVALID_HANDLE_VALUE) return;

    char chunk[128];
    DWORD bytes_read = 0;
    if (!ReadFile(g_serial, chunk, sizeof(chunk) - 1, &bytes_read, NULL)) return;
    if (bytes_read == 0) return;

    for (DWORD i = 0; i < bytes_read; i++) {
        char c = chunk[i];
        if (c == '\n' || c == '\r') {
            if (g_serial_buf_len > 0) {
                g_serial_buf[g_serial_buf_len] = '\0';
                int lane = -1;
                if (sscanf(g_serial_buf, "HIT:%d", &lane) == 1) {
                    register_hit(lane, gs, now_ms);
                }
                g_serial_buf_len = 0;
            }
        } else if (g_serial_buf_len < (int)sizeof(g_serial_buf) - 1) {
            g_serial_buf[g_serial_buf_len++] = c;
        }
    }
}
#else
/* Non-Windows stub so the file still compiles on Linux/macOS during
   development -- keyboard-only mode there. */
static int serial_open(void) { return 0; }
static void serial_close(void) {}
static void serial_poll(GameState *gs, double now_ms) { (void)gs; (void)now_ms; }
#endif

/* ---------------------------- Text rendering ------------------------------ */

static void render_text(SDL_Renderer *ren, TTF_Font *font, const char *text,
                         int x, int y, SDL_Color color, int centered) {
    if (!font || !text || !text[0]) return;
    SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    if (!tex) { SDL_FreeSurface(surf); return; }

    SDL_Rect dst;
    dst.w = surf->w;
    dst.h = surf->h;
    dst.x = centered ? (x - surf->w / 2) : x;
    dst.y = y;

    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

/* ---------------------------- Main ---------------------------------------- */

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    int ttf_ok = (TTF_Init() == 0);
    if (!ttf_ok) {
        fprintf(stderr, "TTF_Init failed (continuing without text): %s\n", TTF_GetError());
    }

    SDL_Window *win = SDL_CreateWindow(
        "DDR Visual (Laptop Fallback)",
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

    TTF_Font *font_large = NULL;
    TTF_Font *font_small = NULL;
    if (ttf_ok) {
        font_large = TTF_OpenFont(FONT_PATH, FONT_SIZE_LARGE);
        font_small = TTF_OpenFont(FONT_PATH, FONT_SIZE_SMALL);
        if (!font_large || !font_small) {
            fprintf(stderr, "TTF_OpenFont failed for '%s' (continuing without text): %s\n",
                    FONT_PATH, TTF_GetError());
        }
    }

    GameState gs = {0};
    gs.last_judgment = JUDGE_NONE;

    int serial_ok = serial_open();
    if (!serial_ok) {
        printf("Running in keyboard-only mode (A S W D). Connect the ESP32 and set SERIAL_PORT to enable pad input.\n");
    }

    Uint32 start_ticks = SDL_GetTicks();
    int running = 1;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) running = 0;
                for (int lane = 0; lane < NUM_LANES; lane++) {
                    if (e.key.keysym.scancode == LANE_KEYS[lane]) {
                        register_hit(lane, &gs, (double)(SDL_GetTicks() - start_ticks));
                    }
                }
            }
        }

        double song_time_ms = (double)(SDL_GetTicks() - start_ticks);
        serial_poll(&gs, song_time_ms);
        update_spawner(song_time_ms);
        update_notes(song_time_ms, &gs);

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

        /* lane key labels under hit line */
        for (int lane = 0; lane < NUM_LANES; lane++) {
            SDL_Color c = LANE_COLORS[lane];
            SDL_Rect key_box = { lane_x(lane) + LANE_WIDTH/2 - 20, HIT_LINE_Y + 15, 40, 40 };
            SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 255);
            SDL_RenderDrawRect(ren, &key_box);
            if (font_small) {
                render_text(ren, font_small, LANE_LABELS[lane],
                            lane_x(lane) + LANE_WIDTH/2, HIT_LINE_Y + 22,
                            (SDL_Color){255,255,255,255}, 1);
            }
        }

        /* notes */
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

        /* score / combo */
        char buf[64];
        if (font_large) {
            snprintf(buf, sizeof(buf), "SCORE: %d", gs.score);
            render_text(ren, font_large, buf, 20, 15, (SDL_Color){255,255,255,255}, 0);
        }
        if (font_small) {
            snprintf(buf, sizeof(buf), "COMBO: %d", gs.combo);
            render_text(ren, font_small, buf, 20, 60, (SDL_Color){255, 220, 120, 255}, 0);

            snprintf(buf, sizeof(buf), "MAX COMBO: %d", gs.max_combo);
            render_text(ren, font_small, buf, WINDOW_W - 20 - 150, 15, (SDL_Color){200,200,200,255}, 0);
        }

        /* judgment flash text, fades out after ~500ms */
        if (gs.last_judgment != JUDGE_NONE) {
            double age = song_time_ms - gs.last_judgment_time_ms;
            if (age < 500.0) {
                const char *txt = "";
                SDL_Color col = {255,255,255,255};
                if (gs.last_judgment == JUDGE_PERFECT) { txt = "PERFECT!"; col = (SDL_Color){80,255,120,255}; }
                else if (gs.last_judgment == JUDGE_GOOD) { txt = "GOOD"; col = (SDL_Color){255,220,80,255}; }
                else if (gs.last_judgment == JUDGE_MISS) { txt = "MISS"; col = (SDL_Color){255,80,80,255}; }

                if (font_large) {
                    render_text(ren, font_large, txt, WINDOW_W/2, HIT_LINE_Y - 60, col, 1);
                }
            }
        }

        /* controls hint */
        if (font_small) {
            render_text(ren, font_small, "A S W D to play  |  ESC to quit",
                        WINDOW_W/2, WINDOW_H - 30, (SDL_Color){150,150,150,255}, 1);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(1); /* small yield; vsync handles main pacing */
    }

    serial_close();
    if (font_large) TTF_CloseFont(font_large);
    if (font_small) TTF_CloseFont(font_small);
    if (ttf_ok) TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
