/* ==========================================================================
   main.c — DDR laptop visual, entry point.
   --------------------------------------------------------------------------
   This file only does orchestration: set up SDL, run the game loop, and
   call into game.c / input.c / render.c for everything else. No drawing
   code, no game-state logic, and no input-parsing logic lives here --
   that separation is the whole point of splitting the project up.

   Build (Windows / MSYS2 MINGW64 terminal), from inside laptop_visual/:
     gcc main.c game.c input.c render.c -o arrows_visual.exe -lmingw32 -lSDL2main -lSDL2 -lm
     ./arrows_visual.exe

   Requires SDL2 2.0.18+ (SDL_RenderGeometry, used in render.c). If you get
   an "undefined reference to SDL_RenderGeometry" linker error, update SDL2:
     pacman -Syu
     pacman -S mingw-w64-x86_64-SDL2

   Controls:
     A / S / W / D = Left / Down / Up / Right
     ESC            = quit
   ========================================================================== */

#include <SDL2/SDL.h>
#include <stdio.h>

#include "common.h"
#include "game.h"
#include "input.h"
#include "render.h"

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow(
        "DDR Visual - Step 2: Keyboard Input (WASD)",
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

    game_init();
    input_init();

    Uint32 start_ticks = SDL_GetTicks();
    int running = 1;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = 0;
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    running = 0;
                } else {
                    double now_ms = (double)(SDL_GetTicks() - start_ticks);
                    input_handle_keydown(e.key.keysym.scancode, now_ms);
                }
            }
        }

        double song_time_ms = (double)(SDL_GetTicks() - start_ticks);
        game_update(song_time_ms);

        int note_count = 0;
        const Note *notes = game_get_notes(&note_count);

        render_frame(ren, song_time_ms, notes, note_count);
        SDL_RenderPresent(ren);
        SDL_Delay(1);
    }

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
