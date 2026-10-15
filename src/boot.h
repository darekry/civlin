/* Boot sequence (WinMain equivalent) -> SDL3.
 *
 * Ghidra anchors (civ.exe, NE Win3.x), renamed 2026-10-15 (Orig -> New):
 *   WinMain                  @ 1008:0000  (entry; main while loop)
 *   WinMain_Shutdown         @ 1008:0329  (KILLTIMER/FREEPROCINSTANCE/
 *                              DESTROYWINDOW/REMOVEFONTRESOURCE)
 *   Sys_GetCurrentDrive      @ 1000:11bb  (drive -> g_nCurrentDrive;
 *                              see sys.h, boot_setup_paths)
 *   Sys_BuildCurrentDirPath  @ 1000:2826  (curdir -> g_szBasePath;
 *                              see sys.h, boot_setup_paths)
 *   Sys_CheckFreeMemory      @ 11e8:0000  (GETFREESPACE + 1420:43b8 msg)
 *   App_RegisterWindowClasses@ 1098:06f8  (REGISTERCLASS x4, calls
 *                              Ctrl_RegisterCustomClasses, GETSYSTEMMETRICS)
 *   Font_EnumFamiliesInit    @ 1008:0e99  (ENUMFONTFAMILIES +
 *                              ADDFONTRESOURCE civfonts.fon @ 1420:00a4/00fe)
 *   Video_CheckDriverCaps    @ 1008:059a  (GETDEVICECAPS; 1420:0af4 checklist)
 *   Config_LoadPrefs         @ 1008:0b4a  (_LOPEN/_LREAD prefs file civ.prf)
 *   Config_SavePrefs         @ 1008:0bbf  (guarded by Dos_DriveReady @
 *                              1000:1184 + Dos_SetPath @ 1000:116a)
 *   Dos_DriveReady           @ 1000:1184  (drive-ready guard; Linux: always 1)
 *   Dos_SetPath              @ 1000:116a  (chdir guard; Linux: no-op CIV_OK)
 *   Palette_Init             @ 1090:0488  (CREATEPALETTE, godpal.c)
 *   ResMgr_LoadRsc           @ 1190:0000  (OpenResFile : resmgr.c; _LOPEN/
 *                              _HREAD 0xE360; called 5x with 1420:00b1,
 *                              00be, 00cb, 00d8, 00e5 = CivData0..4.rsc)
 *   Art_MaybeConvertArtwork  @ 10b8:0000  (1420:20a9 prompt)
 *   Game_InitGlobalTables    @ 11e8:0337  (key remap, map buffer, Rng_Init,
 *                              string tables; see sys.h, boot_init_tables)
 *   Init_UnitFontTables      @ 10e8:2d46  (FIRSTMOVE/PILLAGE/font map)
 *   Main_RunStartupSequence  @ 11e0:0000  (calls NewGame_Wizard)
 *   GfxPort_Create           @ 10d0:0000  (GR_PortOpen : gr_port.c)
 *   GfxPort_SetActive        @ 10d0:0956
 *   MainPort_GetOrCreate     @ 1098:114a
 *   WdwMap_Open              @ 1208:0af1  (see map.h, boot_open_windows)
 *   WdwSmMap_Open            @ 1210:025b  (see map.h, boot_open_windows)
 *   WdwStatus_Open           @ 1218:0386  (see map.h, boot_open_windows)
 *   Menu_InitMainBar         @ 1048:0000  (CHECKMENUITEM/MODIFYMENU)
 *   Cursor_Set               @ 1098:096c
 *   Game_TurnLoopTick        @ 1088:0000  (while body; see gameloop in game.h)
 *
 * String literals (packet F anchors; read-only reference, never written):
 *   1420:00a4/00fe "civfonts.fon" -> CIV_FONT_FILE (font init, below)
 *   1420:00b1/00be/00cb/00d8/00e5 "CivData0..4.rsc" -> rsc_loader
 *     (ResMgr_LoadRsc @ 1190:0000; DO NOT delete any *.rsc files)
 *   "civ.prf" -> CIV_PREFS_FILE (Config_LoadPrefs/SavePrefs, below)
 *   "civhelp.hlp" -> dialog_open_help (see dialog.h; WINHELP bridge TBD)
 *   "OPENING.wav" @ 1420:3114 -> audio (see audio.h; table @ 1150:0266)
 *   "famehall.civ" @ 1420:14e8 -> halloffame (see halloffame.h; ref only)
 *   MPLOGO.BMP: NO filename string in the binary -> NE resource or RSC
 *     frame (see title.h; ref only, NOT a file load)
 *
 * WinMain order reproduced by boot_run(): drive/path setup -> memory check ->
 * window classes -> timer/accel/font -> 5x RSC -> video caps -> artwork ->
 * prefs -> palette -> global tables -> unit/font tables -> ports/map windows
 * -> menu bar -> cursor -> event/turn loop (caller-owned, see game.h).
 */
#ifndef CIVLIN_BOOT_H
#define CIVLIN_BOOT_H

#include <SDL3/SDL.h>

#include "civ_common.h"

/* WinMain @ 1008:0000 init order (caller-owned loop tail).
 *
 * drive/path (Sys_GetCurrentDrive @ 1000:11bb + Sys_BuildCurrentDirPath @
 *   1000:2826, live via boot_setup_paths) -> memory (Sys_CheckFreeMemory @
 *   11e8:0000, stub) -> classes (App_RegisterWindowClasses @ 1098:06f8,
 *   stub) -> timer/accel/font (200 ms SETTIMER for TIMERPROC @ 1008:0d68 +
 *   accelerators + Font_EnumFamiliesInit @ 1008:0e99, stub) -> 5x RSC
 *   (ResMgr_LoadRsc @ 1190:0000 CivData0..4.rsc, stub) -> video caps
 *   (Video_CheckDriverCaps @ 1008:059a, stub) -> artwork
 *   (Art_MaybeConvertArtwork @ 10b8:0000, stub) -> prefs (Config_LoadPrefs
 *   @ 1008:0b4a, stub; defaults live via config_prefs_defaults) ->
 *   palette (Palette_Init @ 1090:0488, stub) -> tables
 *   (Game_InitGlobalTables @ 11e8:0337 partial + Init_UnitFontTables @
 *   10e8:2d46 stub) -> ports/map windows (boot_open_windows below) ->
 *   menu bar (boot_init_menu below) -> cursor (Cursor_Set @ 1098:096c,
 *   void stub) -> event/turn loop caller-owned (while(!g_nQuitFlag) +
 *   TIMERPROC body via game_tick -> game_turn_tick @ 1088:0000, see
 *   game.h; main.c owns the loop, SDL window/renderer are caller-created
 *   and borrowed, never owned here). Wiring: CIV_OK where OS-free
 *   (boot_setup_paths live, tables partial but overall UNSUPPORTED until
 *   the Win16 selector bridge lands), otherwise CIV_ERR_UNSUPPORTED stub;
 *   first non-OK step short-circuits. No globals, no exit(), read-only. */
civ_error_t boot_run(SDL_Window *win, SDL_Renderer *ren);

/* Single steps, in WinMain order (each UNSUPPORTED until implemented,
 * except boot_setup_paths which is live via sys.h). */
civ_error_t boot_setup_paths(void);
/* Boot memory step: delegates to Sys_CheckFreeMemory @ 11e8:0000 (see
 * sys.h sys_check_free_memory). Stub CIV_ERR_UNSUPPORTED until the
 * SDL_GetSystemRAM mapping lands. */
civ_error_t boot_check_memory(void);
/* Boot window-class step: delegates to App_RegisterWindowClasses @
 * 1098:06f8 (below). Stub CIV_ERR_UNSUPPORTED until the SDL3 window
 * mapping lands. */
civ_error_t boot_register_classes(void);
/* Orig: FUN_1098_06f8 @ 1098:06f8 -> App_RegisterWindowClasses.
 * Registers the 4 WinMain window classes via REGISTERCLASS, then the
 * custom controls via Ctrl_RegisterCustomClasses @ 1058:0b4d (see
 * dialog.h) and caches GETSYSTEMMETRICS. Win16 USER -> SDL3 TBD; stub
 * CIV_ERR_UNSUPPORTED. Owns the registration; boot_register_classes
 * delegates to it. No globals, no exit(), read-only. */
civ_error_t app_register_window_classes(void);
civ_error_t boot_load_fonts(void);
civ_error_t boot_load_resources(void);
civ_error_t boot_check_video(void);
civ_error_t boot_maybe_convert_artwork(void);
civ_error_t boot_load_prefs(void);
civ_error_t boot_init_palette(void);
/* Packet I: global tables (mixed OS-free port + Win16 stub).
 *
 * Orig: FUN_11e8_0337 @ 11e8:0337 -> Game_InitGlobalTables. Called once
 *   from WinMain after Palette_Init @ 1090:0488, before
 *   Main_RunStartupSequence @ 11e0:0000. Orig shape: key-remap loop +
 *   mem_alloc_lock(64000) -> 1420:b394/b396 + Rng_Init @ 11c8:0000
 *   (see sys.h rng_init) + string tables via Tribe_GetNameList @
 *   11f8:0080 (see newgame.h). Linux port: remap loop (identity
 *   skeleton, exact pairs TBD from dump) + rng_init() + malloc(64000)
 *   probe are live in boot.c; Win16 GLOBALALLOC lock selector semantics
 *   (-> 1420:b394/b396 far ptr) have no Linux equivalent -> overall
 *   CIV_ERR_UNSUPPORTED until the map-buffer owner (see map.h
 *   CIV_MAP_BUF_LEN) takes the persistent allocation. No globals here;
 *   no exit(). */
civ_error_t boot_init_tables(void);
/* Orig: FUN_10e8_2d46 @ 10e8:2d46 -> Init_UnitFontTables. Sibling of
 *   NewGame_Wizard @ 10e8:0000; loads FIRSTMOVE/PILLAGE/font-map tables
 *   (exact table bytes/offsets TBD from segment dump; pure memcpy when
 *   decoded). Win16 resource/selector load TBD -> stub
 *   CIV_ERR_UNSUPPORTED. */
civ_error_t boot_init_unit_font_tables(void);
/* Orig: FUN_1098_096c @ 1098:096c -> Cursor_Set. (cursor_id) void, called
 *   after Menu_InitMainBar @ 1048:0000. LOADCURSOR + SETCURSOR Win16
 *   cursor install -> SDL3 system-cursor bridge TBD (see video.h for the
 *   future SDL_CreateSystemCursor owner; impl stays in boot.c per packet
 *   I scope, video.* untouched). Stub: records nothing, returns void. */
void cursor_set(int cursor_id);
/* Orig ports/map windows (partial port; SDL window/renderer caller-owned).
 *
 * GfxPort_Create @ 10d0:0000 (GR_PortOpen) + MainPort_GetOrCreate @
 *   1098:114a + WdwMap_Open @ 1208:0af1 + WdwSmMap_Open @ 1210:025b +
 *   WdwStatus_Open @ 1218:0386 (see video.h gfx_port_create /
 *   mainport_get_or_create + map.h map_open_main/small/status). SDL3:
 *   SDL_Window/SDL_Renderer are created by the caller (main.c via
 *   SDL_CreateWindow/SDL_CreateRenderer) and borrowed here, never
 *   Destroyed/owned; video_init + map_init are live, the Win16
 *   CREATEWINDOW/scroll-range/SETTIMER tails stay stubbed so overall
 *   CIV_ERR_UNSUPPORTED until the window handles land. No globals, no
 *   exit(), read-only. */
civ_error_t boot_open_windows(SDL_Window *win, SDL_Renderer *ren);
/* Orig menu/cursor init (partial port).
 *
 * Menu_InitMainBar @ 1048:0000 (CHECKMENUITEM/MODIFYMENU, see
 *   menu_init_mainbar below) + Cursor_Set @ 1098:096c (void stub
 *   cursor_set() in boot.c; SDL_CreateSystemCursor bridge TBD) +
 *   dialog_init (DLGPROC layer, see dialog.h). menu_init_mainbar +
 *   dialog tails stay stubbed so overall CIV_ERR_UNSUPPORTED; cursor step
 *   is void (installs nothing, keeps WinMain-order fidelity). No globals,
 *   no exit(), read-only. */
civ_error_t boot_init_menu(void);

/* Packet G: main menu bar (stub).
 *
 * Orig: FUN_1048_0000 @ 1048:0000 -> Menu_InitMainBar. Builds/updates the
 *   WinMain menu bar via CHECKMENUITEM + MODIFYMENU (menu strings such as
 *   "&Save Game\tCtrl+S" @ 14b0:0010, "&Auto Save" @ 14b0:0092 live in
 *   segment 14b0; "Save First" @ 14d0:0028, see dialog.h). Win16 USER menu
 *   handles -> SDL3 menu TBD; stub CIV_ERR_UNSUPPORTED. Called from
 *   boot_init_menu ahead of Cursor_Set @ 1098:096c. */
civ_error_t menu_init_mainbar(void);

void boot_shutdown(void);

/* Packet F: font / prefs / DOS guards (anchors above). Win16-backed entries
 * are stubs (CIV_ERR_UNSUPPORTED); DOS guards are OS-free 1:1 ports. */

/* Startup asset names (packet F string literals; read-only, never written). */
#define CIV_FONT_FILE "civfonts.fon" /* 1420:00a4/00fe via ADDFONTRESOURCE */
#define CIV_PREFS_FILE "civ.prf"     /* Config_LoadPrefs @ 1008:0b4a */

/* Orig: FUN_1008_0e99 @ 1008:0e99 -> font_enum_families_init
 * (Font_EnumFamiliesInit). ENUMFONTFAMILIES enumeration callback +
 * ADDFONTRESOURCE(CIV_FONT_FILE "civfonts.fon" @ 1420:00a4/00fe).
 * civfonts.fon is a Win16 NE DLL/font container (244224 B on disk):
 * MZ+NE header + font-directory resources; exact FON header bytes +
 * font-table offsets TBD from the unpacked dump (do not hardcode).
 * SDL_ttf bridge plan (not implemented): read-only parse of the NE
 * font resources -> extract a TTF/bitmap face -> TTF_OpenFont +
 * ENUMFONTFAMILIES-equivalent family list; full TTF raster out of
 * scope here. Win16 GDI font-table install -> SDL_ttf TBD; stub
 * CIV_ERR_UNSUPPORTED. Read-only: never writes civfonts.fon (see
 * video_shutdown_display for the REMOVEFONTRESOURCE tail). */
civ_error_t font_enum_families_init(void);

/* Orig: FUN_1008_0b4a @ 1008:0b4a -> config_load_prefs (Config_LoadPrefs).
 * _LOPEN (OF_READ) + _LREAD of CIV_PREFS_FILE from the base path, then
 * option-struct fill with built-in defaults when missing/short. Prefs
 * field map not yet decoded; stub. Read-only: never creates civ.prf. */
civ_error_t config_load_prefs(void);

/* Orig: FUN_1008_0bbf @ 1008:0bbf -> config_save_prefs (Config_SavePrefs).
 * Guarded by dos_drive_ready @ 1000:1184 (must be ready) + dos_set_path @
 * 1000:116a (cwd in place) before the _LCREAT/_LWRITE of CIV_PREFS_FILE.
 * Win16 file-handle path -> stdio bridge TBD; stub. */
civ_error_t config_save_prefs(void);

/* Orig: FUN_1000_1184 @ 1000:1184 -> dos_drive_ready (Dos_DriveReady).
 * Drive-ready guard checked before the Config_SavePrefs @ 1008:0bbf and
 * Sound_PlayId @ 1150:00c9 disk paths (see sys.h `0 < drive` note).
 * OS-free Linux equivalent: no removable-drive concept -> always 1
 * (ready). Implemented 1:1. */
int dos_drive_ready(int drive);

/* Orig: FUN_1000_116a @ 1000:116a -> dos_set_path (Dos_SetPath). Chdir
 * guard paired with dos_drive_ready before Config_SavePrefs writes
 * civ.prf. Linux equivalent: no drive letters, cwd owned by the launcher
 * -> no-op returning CIV_OK (NULL path reports NOENT instead of crashing
 * like the original far-pointer fault). Implemented 1:1. */
civ_error_t dos_set_path(int drive, const char *path);

/* Packet S2: shutdown/startup (WinMain_Shutdown order, artwork prompt,
 * startup dispatcher, prefs defaults, video caps checklist, timer).
 *
 * Orig anchors (Orig FUN_seg_off @ seg:off kept on each symbol):
 *   WinMain_Shutdown        @ 1008:0329 (lives as boot_shutdown() above;
 *     KILLTIMER / FREEPROCINSTANCE / DESTROYWINDOW / REMOVEFONTRESOURCE
 *     order documented in boot.c; timer detail below)
 *   TIMERPROC               @ 1008:0d68 (200 ms SETTIMER installed by
 *     WdwMap_Open @ 1208:0af1, pumped from the WinMain @ 1008:0000
 *     message loop, torn down by KILLTIMER first in WinMain_Shutdown;
 *     impl map_timer_proc, see map.h; SDL timer bridge TBD, no new
 *     symbol here)
 *   Art_MaybeConvertArtwork @ 10b8:0000 (1420:20a9 prompt; gates
 *     WorldGen_CopyPreset @ 10b8:079d, see worldgen.h)
 *   Main_RunStartupSequence @ 11e0:0000 (dispatcher for MainMenu_WndProc
 *     @ 11e0:0221; NewGame_Wizard @ 10e8:0000 via newgame_wizard_run,
 *     see newgame.h / mainmenu.h)
 *   Config_LoadPrefs        @ 1008:0b4a / Config_SavePrefs @ 1008:0bbf
 *     (CIV_PREFS_FILE "civ.prf"; defaults below; file I/O tail stays
 *     stubbed, read-only: never creates civ.prf)
 *   Video_CheckDriverCaps   @ 1008:059a (GETDEVICECAPS checklist @
 *     1420:0af4; SDL mapping via video_check_driver_caps, see video.h)
 * No globals, no exit(), read-only. Win16 tails stub CIV_ERR_UNSUPPORTED;
 * OS-free parts (prefs defaults, caps index shape) are ported 1:1. */

/* Orig: FUN_10b8_0000 @ 10b8:0000 -> art_maybe_convert_artwork
 * (Art_MaybeConvertArtwork). Optional artwork conversion gated by the
 * 1420:20a9 prompt (prompt bytes not yet decoded from the segment dump,
 * so no literal is ported here); on confirm it converts before
 * WorldGen_CopyPreset @ 10b8:079d copies the 64000 B preset (see
 * worldgen.h worldgen_copy_preset). Conversion format/port bridge TBD
 * -> stub CIV_ERR_UNSUPPORTED. Called from the boot_maybe_convert_artwork
 * step above. No globals, no exit(), read-only. */
civ_error_t art_maybe_convert_artwork(void);

/* Orig: FUN_11e0_0000 @ 11e0:0000 -> main_run_startup_sequence
 * (Main_RunStartupSequence). Dispatcher for the MainMenu_WndProc @
 * 11e0:0221 choices over CIV_MAINMENU_TEXT @ 1420:435b (see mainmenu.h
 * mainmenu_run/mainmenu_dispatch): new-game paths -> NewGame_Wizard @
 * 10e8:0000 via newgame_wizard_run (see newgame.h), Load -> save prompt
 * @ 1130:000e, EARTH -> WorldGen_LoadEarth @ 1108:0243 (see worldgen.h),
 * Hall -> HoF, Quit -> WinMain_Shutdown @ 1008:0329 (boot_shutdown
 * above). SDL modal/event bridge TBD -> stub CIV_ERR_UNSUPPORTED. No
 * globals, no exit(), read-only. */
civ_error_t main_run_startup_sequence(void);

/* Config prefs defaults (civ.prf via Config_LoadPrefs @ 1008:0b4a /
 * Config_SavePrefs @ 1008:0bbf above). g_nOptionFlags @ 12d0:0064 bit
 * 0x10 = sounds on (Sound_PlayId @ 1150:00c9 gate together with
 * DAT_1228:0104, see audio.h audio_play_id + game.h); remaining civ.prf
 * field map TBD from the dump, so the struct carries only the decoded
 * flags word. OS-free 1:1; file I/O tail stays stubbed (config_load_prefs
 * / config_save_prefs above remain CIV_ERR_UNSUPPORTED, read-only). */
#define CIV_PREFS_FLAG_SOUNDS_ON 0x10u /* g_nOptionFlags bit: sounds on */

typedef struct civ_prefs {
    uint16_t option_flags; /* g_nOptionFlags image @ 12d0:0064 */
} civ_prefs_t;

/* Built-in defaults 1:1 (sounds on; other fields TBD). By value, no I/O. */
civ_prefs_t config_prefs_defaults(void);

/* OS-free predicate for the sounds-on shape (NULL -> 0). The DAT_1228:0104
 * second half of the Sound_PlayId gate stays on the file/drive side
 * (dos_drive_ready + config_load_prefs above, TBD). */
int config_prefs_sounds_on(const civ_prefs_t *prefs);

/* Video caps checklist @ 1420:0af4 for Video_CheckDriverCaps @ 1008:059a
 * (GETDEVICECAPS queries: resolution/raster/color-depth/palette family,
 * see video.h video_check_driver_caps; exact entry bytes not yet decoded
 * from the dump, so no byte table is ported). OS-free index shape:
 * count reports decoded entries (0 until the dump lands), describe maps
 * an entry index to its checklist meaning (NULL when out of range or not
 * yet dumped, same contract as tribe_get_name_list, see newgame.h). SDL
 * renderer-caps mapping TBD in video_check_driver_caps; boot_check_video
 * above delegates to it. No globals, no exit(), read-only. */
int boot_video_caps_count(void);
const char *boot_video_caps_describe(int index);

#endif /* CIVLIN_BOOT_H */
