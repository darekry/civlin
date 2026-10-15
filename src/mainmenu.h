/* Main menu (mode select) -> SDL3.
 *
 * Ghidra anchors (civ.exe):
 *   MainMenu_WndProc  @ 11e0:0221  (PUSH 0x435b @ 11e0:0687 proves the site)
 *   Dialog_MenuZ_Modal@ 1220:0156  (MenuZ : windows.c; shared modal choice)
 *   Util_StrCopy      @ 1000:36b2 / Util_StrAppend @ 1000:363e (buffer build)
 *   save_prompt_open  @ 1130:000e  (choice 1 = Load a Saved Game)
 *   FUN_1108_0243     @ 1108:0243  (choice 3 = Play on EARTH; earth-map load)
 *   HoF_Load          @ 1080:0023 + HoF_Show @ 1080:0119 (choice 4 = Hall)
 *   WinMain_Shutdown  @ 1008:0329  (choice 5 = Quit)
 *
 * Menu text @ 1420:435b (0-terminated, '_' separates items):
 *   "_Start a New Game_Load a Saved Game_Play on EARTH_Customize World_"
 *   "View Hall of Fame_Quit"
 * Raw modal indices need re-confirmation against Dialog_MenuZ_Modal's
 * 500-based button ids (DAT_1418_0002 preselect); the enum below records
 * the dispatch values observed in MainMenu_WndProc.
 */
#ifndef CIVLIN_MAINMENU_H
#define CIVLIN_MAINMENU_H

#include "civ_common.h"

#define CIV_MAINMENU_TEXT \
    "_Start a New Game_Load a Saved Game_Play on EARTH_Customize World_View Hall of Fame_Quit"

/* Dispatch values as observed in MainMenu_WndProc (g_nMenuChoice). */
typedef enum civ_mainmenu_choice {
    CIV_MENU_NONE = -1,
    CIV_MENU_LOAD = 1,   /* -> save_prompt_open */
    CIV_MENU_EARTH = 3,  /* -> FUN_1108_0243 earth map */
    CIV_MENU_HALL = 4,   /* -> HoF_Load + HoF_Show */
    CIV_MENU_QUIT = 5    /* -> WinMain_Shutdown */
    /* 0/2 fall through to NewGame_Wizard paths; exact mapping TBD */
} civ_mainmenu_choice_t;

/* Shows the menu (Util_StrCopy + Dialog_MenuZ_Modal loop
 * while choice<0 || choice>3) and returns the raw choice. */
civ_error_t mainmenu_run(civ_mainmenu_choice_t *out);

/* Dispatches one choice to load/earth/hall/quit/new-game.
 * CIV_MENU_EARTH (3) -> worldgen_load_earth (Orig FUN_1108_0243 @ 1108:0243,
 * see worldgen.h); nonzero result keeps the menu choice invalid. */
civ_error_t mainmenu_dispatch(civ_mainmenu_choice_t choice);

#endif /* CIVLIN_MAINMENU_H */
