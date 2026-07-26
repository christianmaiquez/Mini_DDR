#include "input.h"
#include "common.h"
#include <stdio.h>

/* Key bindings for keyboard test mode, matching lane order 0..3
   (Left, Down, Up, Right) -> A, S, W, D */
static const SDL_Scancode LANE_KEYS[NUM_LANES] = {
    SDL_SCANCODE_A,
    SDL_SCANCODE_S,
    SDL_SCANCODE_W,
    SDL_SCANCODE_D
};
static const char *LANE_KEY_NAMES[NUM_LANES] = { "A", "S", "W", "D" };

static double last_press_time_ms[NUM_LANES] = { -1, -1, -1, -1 };

void input_init(void) {
    for (int i = 0; i < NUM_LANES; i++) {
        last_press_time_ms[i] = -1;
    }
}

int input_lane_for_scancode(SDL_Scancode scancode) {
    for (int lane = 0; lane < NUM_LANES; lane++) {
        if (scancode == LANE_KEYS[lane]) return lane;
    }
    return -1;
}

void input_handle_keydown(SDL_Scancode scancode, double now_ms) {
    int lane = input_lane_for_scancode(scancode);
    if (lane == -1) return;
    last_press_time_ms[lane] = now_ms;
    printf("Key press: lane %d (%s)\n", lane, LANE_KEY_NAMES[lane]);
    fflush(stdout);
}

double input_get_last_press(int lane) {
    if (lane < 0 || lane >= NUM_LANES) return -1;
    return last_press_time_ms[lane];
}
