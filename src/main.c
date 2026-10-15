#include <SDL3/SDL.h>

#include "audio.h"
#include "boot.h"
#include "civ_common.h"
#include "title.h"
#include "video.h"

int main(int argc, char *argv[])
{
    SDL_Window *win;
    SDL_Renderer *ren;
    civ_error_t err;

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

    /* WinMain @ 1008:0000 init order. boot_run() is best-effort: UNSUPPORTED
     * stubs (RSC walk, menus, map windows) are logged but do not block the
     * title animation below — the goal stage is "intro with sound, then may
     * exit", full game loop (Game_TurnLoopTick @ 1088:0000) comes later. */
    err = boot_run(win, ren);
    SDL_Log("boot: %s", civ_strerror(err));

    /* Video/audio backends are live (video_init/video_check_driver_caps +
     * palette_init via boot stubs; audio_init -> Sound_MciOpen @ 1150:001c).
     * Log only; the intro tolerates silent fallback. */
    err = video_init(win, ren);
    SDL_Log("video: %s", civ_strerror(err));
    err = audio_init();
    SDL_Log("audio: %s", civ_strerror(err));

    /* Startup animation (Title_PlayAnimLoop @ 1030:0000 shape):
     * MPLOGO.BMP + Sound_PlayId(CIV_SOUND_OPENING)=OPENING.wav,
     * skippable via title_poll_skip(). May exit afterwards per goal. */
    err = title_play_intro_sdl(win, ren);
    SDL_Log("title: %s", civ_strerror(err));

    boot_shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
