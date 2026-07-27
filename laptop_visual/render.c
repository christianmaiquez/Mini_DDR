#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "render.h"
#include "common.h"
#include "input.h"
#include "scoring.h"
#include "dancer.h"
#include <SDL2/SDL_ttf.h>

#define ARC_PI 3.14159265358979323846

static Uint8 clampu8(double v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (Uint8)v;
}

/* Scales visual intensity (glow/alpha) up as combo climbs, so the whole
   screen feels progressively hotter the longer you're on a streak. Caps
   out at 2.2x so it doesn't blow out to solid white at huge combos. */
static double combo_intensity(void) {
    double factor = 1.0 + (double)scoring_get_combo() / 60.0;
    if (factor > 2.2) factor = 2.2;
    return factor;
}

/* ---------------------------- Font state ----------------------------------- */

static int g_ttf_ok = 0;
static TTF_Font *g_font_large = NULL;
static TTF_Font *g_font_small = NULL;
static TTF_Font *g_font_huge = NULL;

/* ---------------------------- Judgment / milestone / screen-flash state ---- */

static Judgment g_flash_judgment = JUDGE_NONE;
static double g_flash_time_ms = -1e18;
#define JUDGMENT_FLASH_DURATION_MS 550.0

static int g_milestone_value = 0;
static double g_milestone_time_ms = -1e18;

static double g_screen_flash_time_ms = -1e18;
static SDL_Color g_screen_flash_color = {255, 255, 255, 255};

/* ---------------------------- Particles ------------------------------------ */

typedef struct {
    float x0, y0;
    float vx, vy;   /* px per ms */
    SDL_Color color;
    double born_ms;
    int active;
} Particle;

static Particle g_particles[MAX_PARTICLES];
static int g_particle_next = 0;

static double rand01_local(void);

static void spawn_particles(int lane, SDL_Color color, double now_ms) {
    int origin_x = lane_x(lane) + LANE_WIDTH / 2;
    int origin_y = HIT_LINE_Y;
    int count = 14;

    for (int i = 0; i < count; i++) {
        Particle *p = &g_particles[g_particle_next];
        g_particle_next = (g_particle_next + 1) % MAX_PARTICLES;

        double angle = rand01_local() * 2.0 * ARC_PI;
        double speed = 0.06 + rand01_local() * 0.10; /* px/ms */

        p->x0 = (float)origin_x;
        p->y0 = (float)origin_y;
        p->vx = (float)(cos(angle) * speed);
        p->vy = (float)(sin(angle) * speed - 0.03);
        p->color = color;
        p->born_ms = now_ms;
        p->active = 1;
    }
}

static void draw_particles(SDL_Renderer *ren, double song_time_ms) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
    for (int i = 0; i < MAX_PARTICLES; i++) {
        Particle *p = &g_particles[i];
        if (!p->active) continue;
        double age = song_time_ms - p->born_ms;
        if (age < 0 || age > PARTICLE_LIFE_MS) { p->active = 0; continue; }

        float t = (float)(age / PARTICLE_LIFE_MS);
        float gravity = 0.00025f;
        float x = p->x0 + p->vx * (float)age;
        float y = p->y0 + p->vy * (float)age + gravity * (float)age * (float)age;

        SDL_Color c = p->color;
        c.a = (Uint8)(255 * (1.0f - t));

        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
        int size = (int)(5 * (1.0f - t)) + 2;
        SDL_Rect r = { (int)x - size / 2, (int)y - size / 2, size, size };
        SDL_RenderFillRect(ren, &r);
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
}

static double rand01_local(void) {
    return (double)rand() / (double)RAND_MAX;
}

/* ---------------------------- Arrow shape drawing --------------------------- */

static const float ARROW_PTS[9][2] = {
    {0.5f, 0.0f}, {1.0f, 0.45f}, {0.0f, 0.45f},
    {0.3f, 0.45f}, {0.7f, 0.45f}, {0.7f, 1.0f},
    {0.3f, 0.45f}, {0.7f, 1.0f}, {0.3f, 1.0f},
};

static void map_arrow_point(int lane, float u, float v,
                             float x, float y, float w, float h,
                             float *out_x, float *out_y) {
    switch (lane) {
        case 0: *out_x = x + v * w;         *out_y = y + u * h;         break;
        case 1: *out_x = x + u * w;         *out_y = y + (1.0f - v) * h; break;
        case 2: *out_x = x + u * w;         *out_y = y + v * h;         break;
        case 3: default:
                *out_x = x + (1.0f - v) * w; *out_y = y + u * h;         break;
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

static void draw_arrow_neon(SDL_Renderer *ren, int box_x, int box_y, int size,
                             int lane, SDL_Color color) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
    for (int i = 3; i >= 1; i--) {
        int grow = i * 6;
        SDL_Color glow = color;
        glow.a = (Uint8)(35 / i);
        draw_arrow(ren, (float)(box_x - grow / 2), (float)(box_y - grow / 2),
                   (float)(size + grow), (float)(size + grow), lane, glow);
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_Color outline = {0, 0, 0, 255};
    int ol = 5;
    draw_arrow(ren, (float)(box_x - ol / 2), (float)(box_y - ol / 2),
               (float)(size + ol), (float)(size + ol), lane, outline);
    draw_arrow(ren, (float)box_x, (float)box_y, (float)size, (float)size, lane, color);
}

/* ---------------------------- Beat pulse ------------------------------------
   Simple sawtooth pulse synced to a fixed BPM: snaps bright on the beat,
   decays until the next one. Purely a visual metronome -- not tied to any
   real audio yet. */
static double beat_pulse(double time_ms) {
    double beat_period_ms = 60000.0 / BPM;
    double phase = fmod(time_ms, beat_period_ms) / beat_period_ms; /* 0..1 */
    return 1.0 - phase;
}

/* ---------------------------- Background / lanes ---------------------------- */

static void draw_background(SDL_Renderer *ren, double time_ms) {
    for (int row = 0; row < WINDOW_H; row++) {
        float t = (float)row / (float)WINDOW_H;
        Uint8 r = (Uint8)(BG_TOP.r + (BG_BOTTOM.r - BG_TOP.r) * t);
        Uint8 g = (Uint8)(BG_TOP.g + (BG_BOTTOM.g - BG_TOP.g) * t);
        Uint8 b = (Uint8)(BG_TOP.b + (BG_BOTTOM.b - BG_TOP.b) * t);
        SDL_SetRenderDrawColor(ren, r, g, b, 255);
        SDL_RenderDrawLine(ren, 0, row, WINDOW_W, row);
    }

    double pulse = beat_pulse(time_ms);
    Uint8 grid_alpha = (Uint8)(GRID_COLOR.a * (0.5 + 0.5 * pulse));

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, GRID_COLOR.r, GRID_COLOR.g, GRID_COLOR.b, grid_alpha);
    for (int x = 0; x < WINDOW_W; x += 40) SDL_RenderDrawLine(ren, x, 0, x, WINDOW_H);
    for (int y = 0; y < WINDOW_H; y += 40) SDL_RenderDrawLine(ren, 0, y, WINDOW_W, y);
}

static void draw_lanes(SDL_Renderer *ren, double time_ms) {
    double pulse = beat_pulse(time_ms);
    for (int lane = 0; lane < NUM_LANES; lane++) {
        SDL_Color c = LANE_COLORS[lane];
        SDL_Rect lane_rect = { lane_x(lane), 0, LANE_WIDTH, WINDOW_H };

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 22);
        SDL_RenderFillRect(ren, &lane_rect);

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
        Uint8 border_alpha = (Uint8)(90 * (0.6 + 0.4 * pulse));
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, border_alpha);
        SDL_RenderDrawRect(ren, &lane_rect);
        SDL_Rect inset = { lane_rect.x + 1, lane_rect.y, lane_rect.w - 2, lane_rect.h };
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, (Uint8)(border_alpha * 0.55));
        SDL_RenderDrawRect(ren, &inset);
    }
}

static void draw_hit_line(SDL_Renderer *ren) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
    for (int i = 3; i >= 1; i--) {
        SDL_SetRenderDrawColor(ren, 255, 255, 255, (Uint8)(60 / i));
        SDL_Rect glow_line = { lane_x(0), HIT_LINE_Y - i,
                                lane_x(NUM_LANES - 1) + LANE_WIDTH - lane_x(0), 4 + i * 2 };
        SDL_RenderFillRect(ren, &glow_line);
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
    SDL_Rect hitline = { lane_x(0), HIT_LINE_Y, lane_x(NUM_LANES - 1) + LANE_WIDTH - lane_x(0), 4 };
    SDL_RenderFillRect(ren, &hitline);
}

static void draw_target_arrows(SDL_Renderer *ren, double song_time_ms) {
    for (int lane = 0; lane < NUM_LANES; lane++) {
        SDL_Color c = LANE_COLORS[lane];
        int box_x = lane_x(lane) + (LANE_WIDTH - ARROW_SIZE) / 2;
        int box_y = HIT_LINE_Y - ARROW_SIZE / 2;

        double last_press = input_get_last_press(lane);
        double since_press = (last_press < 0) ? 1e18 : (song_time_ms - last_press);

        if (since_press < FLASH_DURATION_MS) {
            double t = since_press / FLASH_DURATION_MS;
            int flash_size = ARROW_SIZE + (int)(14 * (1.0 - t));
            int flash_box_x = lane_x(lane) + (LANE_WIDTH - flash_size) / 2;
            int flash_box_y = HIT_LINE_Y - flash_size / 2;
            SDL_Color bright = c;
            bright.a = (Uint8)(255 * (1.0 - t) + 60 * t);
            draw_arrow_neon(ren, flash_box_x, flash_box_y, flash_size, lane, bright);
        } else {
            SDL_Color dim = { c.r, c.g, c.b, 90 };
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
            draw_arrow(ren, (float)box_x, (float)box_y, (float)ARROW_SIZE, (float)ARROW_SIZE, lane, dim);
        }
    }
}

static void draw_falling_notes(SDL_Renderer *ren, const Note *notes, int note_count) {
    for (int i = 0; i < note_count; i++) {
        const Note *n = &notes[i];
        if (!n->active) continue;
        SDL_Color c = LANE_COLORS[n->lane];
        int box_x = lane_x(n->lane) + (LANE_WIDTH - ARROW_SIZE) / 2;
        int box_y = (int)n->y;
        draw_arrow_neon(ren, box_x, box_y, ARROW_SIZE, n->lane, c);
    }
}

/* ---------------------------- Screen flash / CRT overlay -------------------- */

static void draw_screen_flash(SDL_Renderer *ren, double song_time_ms) {
    double age = song_time_ms - g_screen_flash_time_ms;
    if (age < 0 || age >= SCREEN_FLASH_DURATION_MS) return;
    double t = age / SCREEN_FLASH_DURATION_MS;
    Uint8 alpha = (Uint8)(90 * (1.0 - t));

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(ren, g_screen_flash_color.r, g_screen_flash_color.g,
                            g_screen_flash_color.b, alpha);
    SDL_Rect full = { 0, 0, WINDOW_W, WINDOW_H };
    SDL_RenderFillRect(ren, &full);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
}

static void draw_crt_overlay(SDL_Renderer *ren) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 35);
    for (int y = 0; y < WINDOW_H; y += 3) {
        SDL_RenderDrawLine(ren, 0, y, WINDOW_W, y);
    }

    /* cheap vignette: a few nested translucent black rectangle borders */
    for (int i = 0; i < 40; i += 8) {
        SDL_SetRenderDrawColor(ren, 0, 0, 0, (Uint8)(6));
        SDL_Rect r = { i, i, WINDOW_W - i * 2, WINDOW_H - i * 2 };
        SDL_RenderDrawRect(ren, &r);
    }
}

/* Combo-based "fire mode": once combo passes FIRE_COMBO_THRESHOLD, a warm
   pulsing border kicks in around the screen edge, intensifying with combo
   and pulsing on the beat -- a visual reward for a long streak, similar
   to "full combo" glow effects in real rhythm-game cabinets. */
static void draw_combo_fire(SDL_Renderer *ren, double time_ms) {
    int combo = scoring_get_combo();
    if (combo < FIRE_COMBO_THRESHOLD) return;

    double intensity = (double)(combo - FIRE_COMBO_THRESHOLD) / 60.0;
    if (intensity > 1.0) intensity = 1.0;

    double pulse = beat_pulse(time_ms);
    Uint8 alpha = (Uint8)(50 * intensity * (0.5 + 0.5 * pulse));

    SDL_Color fire = { 255, 90, 20, 255 }; /* warm orange-red */
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
    for (int i = 0; i < 24; i += 4) {
        SDL_SetRenderDrawColor(ren, fire.r, fire.g, fire.b, (Uint8)(alpha / (i / 4 + 1)));
        SDL_Rect r = { i, i, WINDOW_W - i * 2, WINDOW_H - i * 2 };
        SDL_RenderDrawRect(ren, &r);
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
}

/* ---------------------------- Text / HUD ------------------------------------ */

static void render_text_neon(SDL_Renderer *ren, TTF_Font *font, const char *text,
                              int x, int y, SDL_Color color, int centered) {
    if (!font || !text || !text[0]) return;

    SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    int w = surf->w, h = surf->h;
    SDL_FreeSurface(surf);
    if (!tex) return;

    int dst_x = centered ? (x - w / 2) : x;

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_ADD);
    for (int i = 3; i >= 1; i--) {
        int grow = i * 4;
        SDL_SetTextureAlphaMod(tex, (Uint8)(50 / i));
        SDL_Rect dst = { dst_x - grow / 2, y - grow / 2, w + grow, h + grow };
        SDL_RenderCopy(ren, tex, NULL, &dst);
    }

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(tex, 255);
    SDL_Rect dst2 = { dst_x, y, w, h };
    SDL_RenderCopy(ren, tex, NULL, &dst2);

    SDL_DestroyTexture(tex);
}

static void draw_hud(SDL_Renderer *ren, double song_time_ms) {
    char buf[96];
    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color cyan   = {  0, 230, 255, 255};
    SDL_Color yellow = {255, 220,   0, 255};

    snprintf(buf, sizeof(buf), "SCORE: %d", scoring_get_score());
    render_text_neon(ren, g_font_large, buf, 20, 15, white, 0);

    snprintf(buf, sizeof(buf), "COMBO: %d", scoring_get_combo());
    render_text_neon(ren, g_font_small, buf, 20, 65, cyan, 0);

    snprintf(buf, sizeof(buf), "MAX COMBO: %d", scoring_get_max_combo());
    render_text_neon(ren, g_font_small, buf, WINDOW_W - 180, 15, yellow, 0);

    double remaining_ms = GAME_DURATION_MS - song_time_ms;
    if (remaining_ms < 0.0) remaining_ms = 0.0;
    int remaining_seconds = (int)ceil(remaining_ms / 1000.0);
    snprintf(buf, sizeof(buf), "TIME: %02d", remaining_seconds);
    render_text_neon(ren, g_font_small, buf, WINDOW_W / 2, 20, white, 1);
}

static void draw_judgment_flash(SDL_Renderer *ren, double song_time_ms) {
    if (g_flash_judgment == JUDGE_NONE) return;
    double age = song_time_ms - g_flash_time_ms;
    if (age < 0 || age >= JUDGMENT_FLASH_DURATION_MS) return;

    double t = age / JUDGMENT_FLASH_DURATION_MS;
    SDL_Color color = judgment_color(g_flash_judgment);
    color.a = (Uint8)(255 * (1.0 - t) + 40 * t);

    int y_offset = (int)(-20 * (1.0 - t));
    render_text_neon(ren, g_font_large, judgment_label(g_flash_judgment),
                      WINDOW_W / 2, HIT_LINE_Y - 90 + y_offset, color, 1);
}

static void draw_milestone_flash(SDL_Renderer *ren, double song_time_ms) {
    if (g_milestone_value <= 0) return;
    double age = song_time_ms - g_milestone_time_ms;
    if (age < 0 || age >= MILESTONE_FLASH_DURATION_MS) return;

    double t = age / MILESTONE_FLASH_DURATION_MS;
    SDL_Color gold = { 255, 215, 0, 255 };
    gold.a = (Uint8)(255 * (1.0 - t) + 40 * t);

    /* quick pop-in scale via vertical offset + using the huge font */
    int y_offset = (int)(-15 * (1.0 - t) * (1.0 - t));
    char buf[48];
    snprintf(buf, sizeof(buf), "%d COMBO!", g_milestone_value);
    render_text_neon(ren, g_font_huge ? g_font_huge : g_font_large, buf,
                      WINDOW_W / 2, 110 + y_offset, gold, 1);
}

/* ---------------------------- Menu screen ------------------------------------ */

void render_draw_menu(SDL_Renderer *ren, double time_ms) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    draw_background(ren, time_ms);
    draw_crt_overlay(ren);

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color magenta = {255, 0, 160, 255};
    SDL_Color cyan = {0, 230, 255, 255};
    SDL_Color green = {80, 255, 60, 255};
    SDL_Color yellow = {255, 220, 0, 255};

    render_text_neon(ren, g_font_huge ? g_font_huge : g_font_large,
                      "SELECT DIFFICULTY", WINDOW_W / 2, 120, magenta, 1);

    render_text_neon(ren, g_font_large, "1  -  EASY",   WINDOW_W / 2, 260, cyan, 1);
    render_text_neon(ren, g_font_large, "2  -  NORMAL", WINDOW_W / 2, 330, green, 1);
    render_text_neon(ren, g_font_large, "3  -  HARD",   WINDOW_W / 2, 400, yellow, 1);

    render_text_neon(ren, g_font_small, "Press 1, 2, or 3 to start  |  ESC to quit",
                      WINDOW_W / 2, 500, white, 1);
}

/* ---------------------------- Results screen -------------------------------- */

void render_draw_results(SDL_Renderer *ren, double time_ms) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    draw_background(ren, time_ms);

    SDL_Color white   = {255, 255, 255, 255};
    SDL_Color magenta = {255,   0, 160, 255};
    SDL_Color cyan    = {  0, 230, 255, 255};
    SDL_Color green   = { 80, 255,  60, 255};
    SDL_Color yellow  = {255, 220,   0, 255};
    SDL_Color red     = {255,  40,  40, 255};

    char buf[96];
    render_text_neon(ren, g_font_huge ? g_font_huge : g_font_large,
                     "RESULTS", WINDOW_W / 2, 45, magenta, 1);

    snprintf(buf, sizeof(buf), "FINAL SCORE: %d", scoring_get_score());
    render_text_neon(ren, g_font_large, buf, WINDOW_W / 2, 125, white, 1);

    snprintf(buf, sizeof(buf), "MAX COMBO: %d", scoring_get_max_combo());
    render_text_neon(ren, g_font_small, buf, WINDOW_W / 2, 185, yellow, 1);

    snprintf(buf, sizeof(buf), "PERFECT: %d", scoring_get_count(JUDGE_PERFECT));
    render_text_neon(ren, g_font_small, buf, WINDOW_W / 2, 250, magenta, 1);
    snprintf(buf, sizeof(buf), "GREAT: %d", scoring_get_count(JUDGE_GREAT));
    render_text_neon(ren, g_font_small, buf, WINDOW_W / 2, 285, cyan, 1);
    snprintf(buf, sizeof(buf), "GOOD: %d", scoring_get_count(JUDGE_GOOD));
    render_text_neon(ren, g_font_small, buf, WINDOW_W / 2, 320, green, 1);
    snprintf(buf, sizeof(buf), "BOO: %d", scoring_get_count(JUDGE_BOO));
    render_text_neon(ren, g_font_small, buf, WINDOW_W / 2, 355, yellow, 1);
    snprintf(buf, sizeof(buf), "MISS: %d", scoring_get_count(JUDGE_MISS));
    render_text_neon(ren, g_font_small, buf, WINDOW_W / 2, 390, red, 1);

    render_text_neon(ren, g_font_small,
                     "R = replay  |  M = difficulty menu  |  ESC = quit",
                     WINDOW_W / 2, 500, white, 1);
    draw_crt_overlay(ren);
}

/* ---------------------------- Public API ----------------------------------- */

void render_init(void) {
    dancer_init();
    g_ttf_ok = (TTF_Init() == 0);
    if (!g_ttf_ok) {
        fprintf(stderr, "TTF_Init failed (continuing without text): %s\n", TTF_GetError());
        return;
    }
    g_font_large = TTF_OpenFont(FONT_PATH, FONT_SIZE_LARGE);
    g_font_small = TTF_OpenFont(FONT_PATH, FONT_SIZE_SMALL);
    g_font_huge  = TTF_OpenFont(FONT_PATH, FONT_SIZE_LARGE + 20);
    if (!g_font_large || !g_font_small) {
        fprintf(stderr, "TTF_OpenFont failed for '%s' (continuing without text): %s\n",
                FONT_PATH, TTF_GetError());
    }
}

void render_reset_game(void) {
    g_flash_judgment = JUDGE_NONE;
    g_flash_time_ms = -1e18;
    g_milestone_value = 0;
    g_milestone_time_ms = -1e18;
    g_screen_flash_time_ms = -1e18;
    g_particle_next = 0;
    for (int i = 0; i < MAX_PARTICLES; i++) {
        g_particles[i].active = 0;
    }
    dancer_init();
}

void render_shutdown(void) {
    if (g_font_large) TTF_CloseFont(g_font_large);
    if (g_font_small) TTF_CloseFont(g_font_small);
    if (g_font_huge)  TTF_CloseFont(g_font_huge);
    if (g_ttf_ok) TTF_Quit();
}

void render_set_judgment_flash(Judgment j, double now_ms) {
    g_flash_judgment = j;
    g_flash_time_ms = now_ms;
}

void render_on_hit(int lane, Judgment j, int milestone_combo, double now_ms) {
    render_set_judgment_flash(j, now_ms);

    SDL_Color color = judgment_color(j);
    spawn_particles(lane, color, now_ms);
    dancer_on_hit(lane, j, now_ms);

    if (j == JUDGE_PERFECT) {
        g_screen_flash_time_ms = now_ms;
        g_screen_flash_color = color;
    }

    if (milestone_combo > 0) {
        g_milestone_value = milestone_combo;
        g_milestone_time_ms = now_ms;
        dancer_trigger_hype(now_ms);
    }
}

void render_frame(SDL_Renderer *ren, double song_time_ms,
                   const Note *notes, int note_count) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    draw_background(ren, song_time_ms);
    draw_lanes(ren, song_time_ms);
    draw_hit_line(ren);
    draw_target_arrows(ren, song_time_ms);
    draw_falling_notes(ren, notes, note_count);
    draw_particles(ren, song_time_ms);
    dancer_draw(ren, song_time_ms);
    draw_hud(ren, song_time_ms);
    draw_judgment_flash(ren, song_time_ms);
    draw_milestone_flash(ren, song_time_ms);
    draw_screen_flash(ren, song_time_ms);
    draw_combo_fire(ren, song_time_ms);
    draw_crt_overlay(ren);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
}
