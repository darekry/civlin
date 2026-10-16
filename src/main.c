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
 *   Game_TurnLoopTick @ 1088:0000 via game_tick; see game.h; SDL pump via
 *   Sys_PumpMessages @ 1108:0000 + sys_yield; quit via sys_poll_quit)
 *   -> boot_shutdown (WinMain_Shutdown @ 1008:0329 order:
 *   KILLTIMER for TIMERPROC / FREEPROCINSTANCE for DLGPROC @ 1098:16e2 +
 *   CIVDIALOGPROC @ 1098:1838 / DESTROYWINDOW for main/map/status windows
 *   / REMOVEFONTRESOURCE for CIV_FONT_FILE; delegates to video/map/dialog/
 *   audio shutdown, then window/renderer destroy here).
 * U1/H: boot_run (honest: CIV_OK or provably vacuous per step, else
 *   UNSUPPORTED; failure aborts like orig MESSAGEBOX+exit, return 1) ->
 *   intro (MPLOGO.BMP + OPENING.wav, skippable, tick-driven SDL stand-in
 *   for Main_RunStartupSequence @ 11e0:0000 loop#1 until the Anim opcode
 *   engine lands) -> menu/game best-effort stubs (logged, never fatal) ->
 *   live tick loop (WinMain @ 1008:0304 shape: while(!g_nQuitFlag) on the
 *   original 60ths clock, window close / ESC quits) -> shutdown. No
 *   exit() in libs (this main may return). No globals. C17, SDL3 only. */
#include <SDL3/SDL.h>

#include <stdlib.h>

#include "boot.h"
#include "civ_common.h"
#include "game.h"
#include "mainmenu.h"
#include "sys.h"
#include "title.h"
#include "video.h"
#include "win16.h"

/* CIVLIN_MAX_TICKS>0 caps main-loop iterations (headless smoke only;
 * unset/0 = interactive until window close / ESC). */
static unsigned long main_max_ticks(void)
{
    const char *env = getenv("CIVLIN_MAX_TICKS");
    char *end = NULL;
    long v;

    if (!env || env[0] == '\0')
        return 0;
    v = strtol(env, &end, 10);
    if (end == env || v < 0)
        return 0;
    return (unsigned long)v;
}

int main(int argc, char *argv[])
{
    SDL_Window *win;
    SDL_Renderer *ren;
    civ_game_t game;
    civ_mainmenu_choice_t choice = CIV_MENU_NONE;
    civ_error_t err;
    unsigned long last = 0;
    unsigned long ticks = 0;
    unsigned long max_ticks = 0;

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

    /* 16 MB startup arena (replaces GLOBALALLOC/GETFREESPACE). */
    if (win16_mem_init() != CIV_OK) {
        SDL_Log("arena: out of memory");
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    /* WinMain @ 1008:0000 init order (boot steps stay honest per the fala
     * H rule in boot.h: CIV_OK or provably vacuous, else UNSUPPORTED).
     * Orig order-step failure -> MESSAGEBOX + exit (e.g. classes @
     * 1008:0070, SETTIMER @ 1008:00e9): no best-effort past a failed boot,
     * intro/loop need the booted state. */
    err = boot_run(win, ren);
    SDL_Log("boot: %s", civ_strerror(err));
    if (err != CIV_OK) {
        game_shutdown(&game);
        boot_shutdown();
        win16_mem_shutdown();
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    /* Main_RunStartupSequence @ 11e0:0000 loop#1 shape (intro over dialog
     * 0x94: Anim_OpenSequence @ 10b0:008a + Anim_AdvanceFrames @ 10b0:0461
     * loop + Anim_CloseSequence @ 10b0:0287 + Sound_PlayId 3/1). U1 SDL
     * stand-in until the Anim opcode engine decodes the script (P0):
     * MPLOGO.BMP + OPENING.wav via title_play_intro_sdl (skippable,
     * tick-driven); the STUB RE-map entry stays in boot.c. */
    err = title_play_intro_sdl(win, ren);
    SDL_Log("intro: %s", civ_strerror(err));

    /* MainMenu_WndProc @ 11e0:0221 (best-effort until SDL modal). */
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

    /* Caller-owned event/turn loop: WinMain @ 1008:0304 shape,
     * while(!g_nQuitFlag @ 12d8:24ee) Game_TurnLoopTick @ 1088:0000
     * (TIMERPROC @ 1008:0d68 only ticks the 24f0 countdown, see boot.h
     * chain-21; the 200 ms map timer is a NULL-proc WM_TIMER, not the
     * turn driver). U1: loop is alive and gated on the original 60ths
     * clock (Sys_GetTickScaled @ 1100:02e1); game_tick stays best-effort
     * until the menu (U2) and turn (P3) tails land. The backbuffer keeps
     * the last intro frame (no invented pixels); re-present keeps the
     * window painted. Quit: window close / ESC (sys_poll_quit). */
    err = game_tick(&game);
    SDL_Log("tick: %s (frontier)", civ_strerror(err));
    last = sys_get_tick_scaled();
    max_ticks = main_max_ticks();
    for (;;) {
        unsigned long now;

        if (sys_poll_quit())
            break;
        now = sys_get_tick_scaled();
        if (now == last) {
            SDL_Delay(1);
            continue;
        }
        last = now;
        (void)game_tick(&game);
        (void)video_present(ren);
        ticks++;
        if (max_ticks > 0 && ticks >= max_ticks)
            break;
    }
    SDL_Log("loop: %lu ticks", ticks);

    /* WinMain_Shutdown @ 1008:0329 order via boot_shutdown (timers/ports/
     * palette/font), then caller-owned window/renderer teardown. */
    game_shutdown(&game);
    boot_shutdown();
    win16_mem_shutdown();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
