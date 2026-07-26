/* ==========================================================================
   main.c — DDR laptop visual, entry point.
   --------------------------------------------------------------------------
   Orchestration only. Flow: difficulty menu -> gameplay loop.

   Build (Windows / MSYS2 MINGW64 terminal), from inside laptop_visual/:
     gcc main.c game.c input.c render.c scoring.c dancer.c -o arrows_visual.exe -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lm
     ./arrows_visual.exe

   Requires SDL2 2.0.18+ (SDL_RenderGeometry) and SDL2_ttf. If "undefined
   reference to SDL_RenderGeometry": pacman -Syu. If SDL2_ttf is missing:
     pacman -S mingw-w64-x86_64-SDL2_ttf

   Score/combo live only in memory -- they reset to 0 every time you
   relaunch the program.

   Controls:
     Menu:     1 / 2 / 3 = Easy / Normal / Hard
     Gameplay: A / S / W / D = Left / Down / Up / Right
     ESC = quit (from either screen)
   ========================================================================== */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "game.h"
#include "input.h"
#include "render.h"
#include "scoring.h"

typedef enum { STATE_MENU, STATE_PLAYING } AppState;

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    srand((unsigned int)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "DDR Visual - Arcade Edition",
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

    render_init();
    input_init();

    AppState state = STATE_MENU;
    Uint32 start_ticks = SDL_GetTicks(); /* used both for menu animation and, after
                                              a difficulty is chosen, reset to mark
                                              the start of the gameplay clock */
    int running = 1;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;

            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    running = 0;
                } else if (state == STATE_MENU) {
                    Difficulty chosen;
                    int picked = 1;
                    switch (e.key.keysym.scancode) {
                        case SDL_SCANCODE_1: chosen = DIFF_EASY;   break;
                        case SDL_SCANCODE_2: chosen = DIFF_NORMAL; break;
                        case SDL_SCANCODE_3: chosen = DIFF_HARD;   break;
                        default: picked = 0; chosen = DIFF_NORMAL; break;
                    }
                    if (picked) {
                        game_set_difficulty(chosen);
                        game_init();
                        input_init();
                        scoring_init();
                        start_ticks = SDL_GetTicks(); /* gameplay clock starts now */
                        state = STATE_PLAYING;
                    }
                } else { /* STATE_PLAYING */
                    double now_ms = (double)(SDL_GetTicks() - start_ticks);
                    int lane = input_lane_for_scancode(e.key.keysym.scancode);

                    input_handle_keydown(e.key.keysym.scancode, now_ms);

                    if (lane != -1) {
                        int milestone = 0;
                        Judgment j = game_try_hit(lane, now_ms, &milestone);
                        if (j != JUDGE_NONE) {
                            render_on_hit(lane, j, milestone, now_ms);
                        }
                    }
                }
            }
        }

        double time_ms = (double)(SDL_GetTicks() - start_ticks);

        if (state == STATE_MENU) {
            render_draw_menu(ren, (double)SDL_GetTicks());
        } else {
            Judgment auto_result = game_update(time_ms);
            if (auto_result == JUDGE_MISS) {
                render_set_judgment_flash(JUDGE_MISS, time_ms);
            }

            int note_count = 0;
            const Note *notes = game_get_notes(&note_count);
            render_frame(ren, time_ms, notes, note_count);
        }

        SDL_RenderPresent(ren);
        SDL_Delay(1);
    }

    render_shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
