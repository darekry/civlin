/* CivLin entry (WinMain @ 1008:0000 SDL3 port).
 *
 * Order: SDL_Init + SDL_CreateWindow/SDL_CreateRenderer (caller-owned;
 *   borrowed by boot_run/video, never owned there) -> boot_run (WinMain @
 *   1008:0000 order: drive/path -> memory -> classes -> timer/accel/font ->
 *   5x RSC -> video caps -> artwork -> prefs -> palette -> tables ->
 *   ports/map windows -> menu bar -> cursor; see boot.h) ->
 *   mainmenu_run/dispatch (Main_RunStartupSequence @ 11e0:0000 dispatcher
 *   for MainMenu_WndProc @ 11e0:0221 over CIV_MAINMENU_TEXT @ 1420:435b;
 *   see mainmenu.h) -> game loop caller-owned (while(!g_nQuitFlag) +
 *   TIMERPROC @ 1008:0d68 body via game_tick -> game_turn_tick @
 *   1088:0000; see game.h; SDL pump via Sys_PumpMessages @ 1108:0000 +
 *   sys_yield) -> boot_shutdown (WinMain_Shutdown @ 1008:0329 order:
 *   KILLTIMER for TIMERPROC / FREEPROCINSTANCE for DLGPROC @ 1098:16e2 +
 *   CIVDIALOGPROC @ 1098:1838 / DESTROYWINDOW for main/map/status windows
 *   / REMOVEFONTRESOURCE for CIV_FONT_FILE; delegates to video/map/dialog/
 *   audio shutdown, then window/renderer destroy here).
 * Y1: boot_run + menu + tick are best-effort (UNSUPPORTED stubs logged,
 *   never fatal); the loop runs one pump until the turn tail lands. No
 *   exit() in libs (this main may return). No globals. C17, SDL3 only. */
#include <SDL3/SDL.h>

#include "boot.h"
#include "civ_common.h"
#include "game.h"
#include "mainmenu.h"

int main(int argc, char *argv[])
{
    SDL_Window *win;
    SDL_Renderer *ren;
    civ_game_t game;
    civ_mainmenu_choice_t choice = CIV_MENU_NONE;
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

    /* WinMain @ 1008:0000 init order (best-effort: UNSUPPORTED logged). */
    err = boot_run(win, ren);
    SDL_Log("boot: %s", civ_strerror(err));

    /* Main_RunStartupSequence @ 11e0:0000 (best-effort until SDL modal). */
    err = mainmenu_run(&choice);
    SDL_Log("menu: %s (%d)", civ_strerror(err), (int)choice);
    if (err == CIV_OK) {
        err = mainmenu_dispatch(choice);
        SDL_Log("dispatch: %s", civ_strerror(err));
    }

    /* Game state init (OS-free live) + new-game tail (stub). */
    err = game_init(&game);
    SDL_Log("game_init: %s", civ_strerror(err));
    if (err == CIV_OK) {
        err = game_new(&game);
        SDL_Log("game_new: %s", civ_strerror(err));
    }

    /* Caller-owned event/turn loop: while(!g_nQuitFlag) game_tick ->
     * game_turn_tick @ 1088:0000 (TIMERPROC @ 1008:0d68 body). Y1 runs a
     * single pump; the turn tail stays UNSUPPORTED so one pass then out. */
    while (game.quit_flag == 0) {
        err = game_tick(&game);
        SDL_Log("tick: %s", civ_strerror(err));
        break;
    }

    /* WinMain_Shutdown @ 1008:0329 order via boot_shutdown (timers/ports/
     * palette/font), then caller-owned window/renderer teardown. */
    game_shutdown(&game);
    boot_shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
