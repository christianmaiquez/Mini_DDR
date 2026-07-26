#include <stdio.h>
#include "render.h"
#include "common.h"
#include "input.h"
#include "scoring.h"

#ifdef _WIN32
#include <SDL2/SDL_ttf.h>
#else
#include <SDL2/SDL_ttf.h>
#endif

/* ---------------------------- Font state ----------------------------------- */

static int g_ttf_ok = 0;
static TTF_Font *g_font_large = NULL;
static TTF_Font *g_font_small = NULL;

/* ---------------------------- Judgment flash state -------------------------- */

static Judgment g_flash_judgment = JUDGE_NONE;
static double g_flash_time_ms = -1e18;
#define JUDGMENT_FLASH_DURATION_MS 550.0

/* ---------------------------- Arrow shape drawing -------------------------
   Same arrow-silhouette approach as before: triangular head + rectangular
   shaft, built with SDL_RenderGeometry, rotated per lane direction. */

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

/* ---------------------------- Background / lanes ---------------------------- */

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
    for (int x = 0; x < WINDOW_W; x += 40) SDL_RenderDrawLine(ren, x, 0, x, WINDOW_H);
    for (int y = 0; y < WINDOW_H; y += 40) SDL_RenderDrawLine(ren, 0, y, WINDOW_W, y);
}

static void draw_lanes(SDL_Renderer *ren) {
    for (int lane = 0; lane < NUM_LANES; lane++) {
        SDL_Color c = LANE_COLORS[lane];
        SDL_Rect lane_rect = { lane_x(lane), 0, LANE_WIDTH, WINDOW_H };

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 22);
        SDL_RenderFillRect(ren, &lane_rect);

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 90);
        SDL_RenderDrawRect(ren, &lane_rect);
        SDL_Rect inset = { lane_rect.x + 1, lane_rect.y, lane_rect.w - 2, lane_rect.h };
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 50);
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

/* ---------------------------- Text / HUD ------------------------------------
   Renders text with a soft neon glow: a few enlarged additive-blended
   copies behind a crisp copy on top. Falls back to doing nothing if the
   font failed to load, so a missing font never crashes the game. */

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

static void draw_hud(SDL_Renderer *ren) {
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
}

static void draw_judgment_flash(SDL_Renderer *ren, double song_time_ms) {
    if (g_flash_judgment == JUDGE_NONE) return;
    double age = song_time_ms - g_flash_time_ms;
    if (age >= JUDGMENT_FLASH_DURATION_MS) return;

    double t = age / JUDGMENT_FLASH_DURATION_MS; /* 0..1 */
    SDL_Color color = judgment_color(g_flash_judgment);
    color.a = (Uint8)(255 * (1.0 - t) + 40 * t);

    /* pop-in scale effect: start slightly big, settle down */
    int y_offset = (int)(-20 * (1.0 - t));
    render_text_neon(ren, g_font_large, judgment_label(g_flash_judgment),
                      WINDOW_W / 2, HIT_LINE_Y - 90 + y_offset, color, 1);
}

/* ---------------------------- Public API ----------------------------------- */

void render_init(void) {
    g_ttf_ok = (TTF_Init() == 0);
    if (!g_ttf_ok) {
        fprintf(stderr, "TTF_Init failed (continuing without text): %s\n", TTF_GetError());
        return;
    }
    g_font_large = TTF_OpenFont(FONT_PATH, FONT_SIZE_LARGE);
    g_font_small = TTF_OpenFont(FONT_PATH, FONT_SIZE_SMALL);
    if (!g_font_large || !g_font_small) {
        fprintf(stderr, "TTF_OpenFont failed for '%s' (continuing without text): %s\n",
                FONT_PATH, TTF_GetError());
    }
}

void render_shutdown(void) {
    if (g_font_large) TTF_CloseFont(g_font_large);
    if (g_font_small) TTF_CloseFont(g_font_small);
    if (g_ttf_ok) TTF_Quit();
}

void render_set_judgment_flash(Judgment j, double now_ms) {
    g_flash_judgment = j;
    g_flash_time_ms = now_ms;
}

void render_frame(SDL_Renderer *ren, double song_time_ms,
                   const Note *notes, int note_count) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    draw_background(ren);
    draw_lanes(ren);
    draw_hit_line(ren);
    draw_target_arrows(ren, song_time_ms);
    draw_falling_notes(ren, notes, note_count);
    draw_hud(ren);
    draw_judgment_flash(ren, song_time_ms);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
}
