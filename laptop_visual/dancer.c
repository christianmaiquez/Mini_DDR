#include "dancer.h"

/* ---------------------------- Pose definitions -----------------------------
   Everything is relative to an anchor point at the figure's hips (0,0).
   Negative y = up. Only hand/foot positions vary between poses -- head,
   neck, and hip attachment points stay fixed so poses blend visually. */

typedef struct {
    int lh_x, lh_y;  /* left hand */
    int rh_x, rh_y;  /* right hand */
    int lf_x, lf_y;  /* left foot */
    int rf_x, rf_y;  /* right foot */
} Pose;

static const Pose POSE_IDLE  = { -22, -15,  22, -15,  -14, 40,  14, 40 };
static const Pose POSE_LEFT  = { -26, -10, -14, -30,  -22, 38,   8, 42 };
static const Pose POSE_DOWN  = { -28,  10,  28,  10,   -8, 28,   8, 28 };
static const Pose POSE_UP    = { -28, -55,  28, -55,  -18, 42,  18, 42 };
static const Pose POSE_RIGHT = {  14, -30,  26, -10,   -8, 42,  22, 38 };

static const int NECK_Y = -38;
static const int HEAD_Y = -52;
static const int HEAD_SIZE = 16;
static const int HIP_SPREAD = 6; /* legs attach slightly apart from center hip */

/* Anchor position on screen (right-side margin, clear of the lanes) */
#define ANCHOR_X (WINDOW_W - 70)
#define ANCHOR_Y 300

/* ---------------------------- State ----------------------------------------- */

static int g_current_lane = -1;      /* -1 = idle */
static double g_pose_until_ms = -1e18;
static SDL_Color g_pose_color = { 200, 230, 255, 255 };

static int g_hype_active = 0;
static double g_hype_until_ms = -1e18;

/* ---------------------------- Drawing helpers ------------------------------- */

static void draw_neon_line(SDL_Renderer *ren, int x1, int y1, int x2, int y2, SDL_Color color) {
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, 55);
    static const int offsets[4][2] = { {-1,0}, {1,0}, {0,-1}, {0,1} };
    for (int i = 0; i < 4; i++) {
        SDL_RenderDrawLine(ren, x1 + offsets[i][0], y1 + offsets[i][1],
                                 x2 + offsets[i][0], y2 + offsets[i][1]);
    }
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(ren, x1, y1, x2, y2);
}

static void draw_pose(SDL_Renderer *ren, const Pose *pose, SDL_Color color) {
    int ax = ANCHOR_X, ay = ANCHOR_Y;

    /* torso */
    draw_neon_line(ren, ax, ay, ax, ay + NECK_Y, color);
    /* arms (from neck) */
    draw_neon_line(ren, ax, ay + NECK_Y, ax + pose->lh_x, ay + pose->lh_y, color);
    draw_neon_line(ren, ax, ay + NECK_Y, ax + pose->rh_x, ay + pose->rh_y, color);
    /* legs (from hip, slightly spread) */
    draw_neon_line(ren, ax - HIP_SPREAD, ay, ax + pose->lf_x, ay + pose->lf_y, color);
    draw_neon_line(ren, ax + HIP_SPREAD, ay, ax + pose->rf_x, ay + pose->rf_y, color);

    /* head: a small filled square with glow, fitting the blocky neon style */
    int hx = ax - HEAD_SIZE / 2;
    int hy = ay + HEAD_Y - HEAD_SIZE / 2;
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, 60);
    SDL_Rect glow_head = { hx - 3, hy - 3, HEAD_SIZE + 6, HEAD_SIZE + 6 };
    SDL_RenderFillRect(ren, &glow_head);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, color.r, color.g, color.b, color.a);
    SDL_Rect head = { hx, hy, HEAD_SIZE, HEAD_SIZE };
    SDL_RenderFillRect(ren, &head);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderDrawRect(ren, &head);
}

/* ---------------------------- Public API ------------------------------------ */

void dancer_init(void) {
    g_current_lane = -1;
    g_pose_until_ms = -1e18;
    g_hype_active = 0;
    g_hype_until_ms = -1e18;
}

void dancer_on_hit(int lane, Judgment j, double now_ms) {
    if (lane < 0 || lane >= NUM_LANES) return;
    g_current_lane = lane;
    g_pose_until_ms = now_ms + POSE_HOLD_MS;
    g_pose_color = judgment_color(j);
}

void dancer_trigger_hype(double now_ms) {
    g_hype_active = 1;
    g_hype_until_ms = now_ms + HYPE_HOLD_MS;
}

void dancer_draw(SDL_Renderer *ren, double time_ms) {
    const Pose *pose;
    SDL_Color color;

    if (g_hype_active && time_ms < g_hype_until_ms) {
        pose = &POSE_UP;
        color = (SDL_Color){ 255, 215, 0, 255 }; /* gold, matches milestone popup */
    } else {
        if (time_ms >= g_hype_until_ms) g_hype_active = 0;

        if (g_current_lane != -1 && time_ms < g_pose_until_ms) {
            switch (g_current_lane) {
                case 0: pose = &POSE_LEFT;  break;
                case 1: pose = &POSE_DOWN;  break;
                case 2: pose = &POSE_UP;    break;
                case 3: pose = &POSE_RIGHT; break;
                default: pose = &POSE_IDLE; break;
            }
            color = g_pose_color;
        } else {
            if (time_ms >= g_pose_until_ms) g_current_lane = -1;
            pose = &POSE_IDLE;
            color = (SDL_Color){ 200, 230, 255, 255 };
        }
    }

    draw_pose(ren, pose, color);
}
