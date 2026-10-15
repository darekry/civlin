#include <SDL3/SDL.h>

#include "boot.h"
#include "civ_common.h"

int main(int argc, char *argv[])
{
    SDL_Window *win;
    SDL_Renderer *ren;
    civ_error_t err;
    bool quit;

    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    win = SDL_CreateWindow("CivLin", 640, 480, 0);
    if (!win) {
        SDL_Log("CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    ren = SDL_CreateRenderer(win, NULL);
    if (!ren) {
        SDL_Log("CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    /* WinMain @ 1008:0000 init order (all steps still UNSUPPORTED stubs). */
    err = boot_run(win, ren);
    SDL_Log("boot: %s", civ_strerror(err));

    /* Skeleton loop. Real loop: while (!g.quit_flag) game_turn_tick(&g)
     * (Game_TurnLoopTick @ 1088:0000) with title/mainmenu/newgame/hof
     * phases in front (see boot.h / game.h). */
    quit = false;
    while (!quit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT)
                quit = true;
            if (ev.type == SDL_EVENT_KEY_DOWN)
                quit = true;
        }
        SDL_SetRenderDrawColor(ren, 16, 32, 64, 255);
        SDL_RenderClear(ren);
        SDL_RenderPresent(ren);
        SDL_Delay(16);
    }

    boot_shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
