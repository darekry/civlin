/* Boot sequence (WinMain equivalent) -> SDL3.
 *
 * Ghidra anchors (civ.exe, NE Win3.x), renamed 2026-10-15:
 *   WinMain                  @ 1008:0000  (entry; main while loop)
 *   WinMain_Shutdown         @ 1008:0329  (KILLTIMER/FREEPROCINSTANCE/
 *                              DESTROYWINDOW/REMOVEFONTRESOURCE)
 *   Sys_CheckFreeMemory      @ 11e8:0000  (GETFREESPACE + 1420:43b8 msg)
 *   App_RegisterWindowClasses@ 1098:06f8  (REGISTERCLASS x4, calls
 *                              Ctrl_RegisterCustomClasses, GETSYSTEMMETRICS)
 *   Font_EnumFamiliesInit    @ 1008:0e99  (ENUMFONTFAMILIES)
 *   Video_CheckDriverCaps    @ 1008:059a  (GETDEVICECAPS; 1420:0af4 checklist)
 *   Config_LoadPrefs         @ 1008:0b4a  (_LOPEN/_LREAD prefs file)
 *   Palette_Init             @ 1090:0488  (CREATEPALETTE, godpal.c)
 *   ResMgr_LoadRsc           @ 1190:0000  (OpenResFile : resmgr.c; _LOPEN/
 *                              _HREAD 0xE360; called 5x with 1420:00b1,
 *                              00be, 00cb, 00d8, 00e5 = CivData0..4.rsc)
 *   Art_MaybeConvertArtwork  @ 10b8:0000  (1420:20a9 prompt)
 *   Init_UnitFontTables      @ 10e8:2d46  (FIRSTMOVE/PILLAGE/font map)
 *   Main_RunStartupSequence  @ 11e0:0000  (calls NewGame_Wizard)
 *   GfxPort_Create           @ 10d0:0000  (GR_PortOpen : gr_port.c)
 *   GfxPort_SetActive        @ 10d0:0956
 *   MainPort_GetOrCreate     @ 1098:114a
 *   Menu_InitMainBar         @ 1048:0000  (CHECKMENUITEM/MODIFYMENU)
 *   Cursor_Set               @ 1098:096c
 *   Game_TurnLoopTick        @ 1088:0000  (while body; see gameloop in game.h)
 *
 * WinMain order reproduced by boot_run(): memory check -> window classes ->
 * timer/accel/font -> 5x RSC -> video caps -> artwork -> prefs -> palette ->
 * unit/font tables -> ports/map windows -> menu bar -> cursor ->
 * event/turn loop (caller-owned, see game.h).
 */
#ifndef CIVLIN_BOOT_H
#define CIVLIN_BOOT_H

#include <SDL3/SDL.h>

#include "civ_common.h"

/* Runs the WinMain init sequence in order. Window/renderer are created by
 * the caller (main.c); loop ownership stays with the caller too. */
civ_error_t boot_run(SDL_Window *win, SDL_Renderer *ren);

/* Single steps, in WinMain order (each UNSUPPORTED until implemented). */
civ_error_t boot_check_memory(void);
civ_error_t boot_register_classes(void);
civ_error_t boot_load_fonts(void);
civ_error_t boot_load_resources(void);
civ_error_t boot_check_video(void);
civ_error_t boot_maybe_convert_artwork(void);
civ_error_t boot_load_prefs(void);
civ_error_t boot_init_palette(void);
civ_error_t boot_init_tables(void);
civ_error_t boot_open_windows(SDL_Window *win, SDL_Renderer *ren);
civ_error_t boot_init_menu(void);

void boot_shutdown(void);

#endif /* CIVLIN_BOOT_H */
