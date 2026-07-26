#include "render.h"
#include "common.h"
#include "input.h"

/* ---------------------------- Arrow shape drawing -------------------------
   Builds an actual arrow silhouette (triangular head + rectangular shaft)
   using SDL_RenderGeometry. Template points below are for an UP-pointing
   arrow in a normalized 0..1 x 0..1 box; map_arrow_point() rotates it to
   whichever direction a lane needs. All of this is internal to render.c --
   nothing outside this file needs to know how arrows are actually drawn. */

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
   fill on top -- gives it that punchy arcade-cabinet look. */
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

/* Target arrows at the hit line: dim by default, flash bright+glowing
   when input.c reports a recent keypress for that lane. */
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

/* ---------------------------- Public API ----------------------------------- */

void render_frame(SDL_Renderer *ren, double song_time_ms,
                   const Note *notes, int note_count) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    draw_background(ren);
    draw_lanes(ren);
    draw_hit_line(ren);
    draw_target_arrows(ren, song_time_ms);
    draw_falling_notes(ren, notes, note_count);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
}
