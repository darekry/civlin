/* Boot sequence. Order mirrors WinMain @ 1008:0000; see boot.h for anchors.
 * No exit() here; no globals. Each step delegates to its owning module. */
#include <stdint.h>
#include <stdlib.h>

#include "boot.h"

#include "audio.h"
#include "dialog.h"
#include "game.h"
#include "map.h"
#include "newgame.h"
#include "sys.h"
#include "video.h"

civ_error_t boot_setup_paths(void)
{
    /* WinMain prologue: Sys_GetCurrentDrive @ 1000:11bb +
     * Sys_BuildCurrentDirPath @ 1000:2826 (see sys.h). */
    return sys_init_paths();
}

civ_error_t boot_check_memory(void)
{
    /* Boot step delegating to Orig FUN_11e8_0000 @ 11e8:0000 ->
     * Sys_CheckFreeMemory (see sys.h sys_check_free_memory; GETFREESPACE
     * + 1420:43b8 msg, SDL_GetSystemRAM TBD). */
    return sys_check_free_memory();
}

civ_error_t app_register_window_classes(void)
{
    /* Orig: FUN_1098_06f8 @ 1098:06f8 -> App_RegisterWindowClasses.
     * REGISTERCLASS x4 + Ctrl_RegisterCustomClasses @ 1058:0b4d (see
     * dialog.h) + GETSYSTEMMETRICS. Win16 USER -> SDL3 TBD; stub
     * CIV_ERR_UNSUPPORTED. No globals, no exit(), read-only. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_register_classes(void)
{
    /* Boot step delegating to Orig FUN_1098_06f8 @ 1098:06f8 ->
     * App_RegisterWindowClasses (above). */
    return app_register_window_classes();
}

civ_error_t boot_load_fonts(void)
{
    /* WinMain font step: Font_EnumFamiliesInit @ 1008:0e99 +
     * ADDFONTRESOURCE(CIV_FONT_FILE) (see boot.h). */
    return font_enum_families_init();
}

civ_error_t boot_load_resources(void)
{
    /* TODO: 5x ResMgr_LoadRsc @ 1190:0000 (CivData0..4.rsc via rsc_loader). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_check_video(void)
{
    /* TODO: Video_CheckDriverCaps @ 1008:059a (see video.h
     * video_check_driver_caps; renderer threading TBD, so this step stays
     * UNSUPPORTED until boot_open_windows owns an SDL_Renderer to check). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_maybe_convert_artwork(void)
{
    /* WinMain artwork step delegating to Orig FUN_10b8_0000 @ 10b8:0000
     * -> Art_MaybeConvertArtwork (below; 1420:20a9 prompt, gates
     * WorldGen_CopyPreset @ 10b8:079d, see worldgen.h). */
    return art_maybe_convert_artwork();
}

civ_error_t boot_load_prefs(void)
{
    /* WinMain prefs step: Config_LoadPrefs @ 1008:0b4a (see boot.h). */
    return config_load_prefs();
}

civ_error_t boot_init_palette(void)
{
    /* WinMain palette step: Palette_Init @ 1090:0488 (see video.h). */
    return palette_init();
}

civ_error_t boot_init_tables(void)
{
    /* Orig: FUN_11e8_0337 @ 11e8:0337 -> Game_InitGlobalTables. Called
     * once from WinMain after Palette_Init @ 1090:0488, before
     * Main_RunStartupSequence @ 11e0:0000. Orig shape: key-remap loop +
     * mem_alloc_lock(64000) -> 1420:b394/b396 + Rng_Init @ 11c8:0000 +
     * string tables via Tribe_GetNameList @ 11f8:0080 (see newgame.h).
     * Mixed port: OS-free parts 1:1 below; Win16 GLOBALALLOC lock
     * selector semantics (-> 1420:b394/b396 far ptr, persistent) have no
     * Linux equivalent -> overall CIV_ERR_UNSUPPORTED. No globals (probe
     * buffer is malloc'd + freed here; the persistent 64 KB owner lives
     * in map.h CIV_MAP_BUF_LEN); read-only; no exit(). */
    uint8_t key_remap[256];
    unsigned i;
    void *probe;

    /* 1) Key-remap loop (OS-free shape 1:1; exact remap pairs TBD from
     * the 11e8:0337 dump, so identity skeleton for now). */
    for (i = 0; i < sizeof(key_remap); i++)
        key_remap[i] = (uint8_t)i;
    (void)key_remap; /* consumed once the dump gives the remap pairs */

    /* 2) Rng_Init @ 11c8:0000 (live via sys.h). */
    rng_init();

    /* 3) mem_alloc_lock(64000) probe: proves the host can hold the
     * 1420:b394/b396 map buffer (see map.h CIV_MAP_BUF_LEN). Freed
     * at once because the persistent locked handle needs the Win16
     * selector bridge (UNSUPPORTED below). */
    probe = malloc(64000u);
    if (!probe)
        return CIV_ERR_NOMEM;
    free(probe);

    /* 4) String-table anchor (see newgame.h): exercises the
     * Tribe_GetNameList @ 11f8:0080 dependency without requiring the
     * seg 1420 dump yet (still NULL stub). */
    (void)tribe_get_name_list(0);

    /* 5) Sibling tables (see below): Init_UnitFontTables @ 10e8:2d46. */
    (void)boot_init_unit_font_tables();

    /* TODO: Win16 GLOBALALLOC lock semantics (-> 1420:b394/b396). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_init_unit_font_tables(void)
{
    /* Orig: FUN_10e8_2d46 @ 10e8:2d46 -> Init_UnitFontTables. Sibling of
     * NewGame_Wizard @ 10e8:0000; loads FIRSTMOVE/PILLAGE/font-map
     * tables. Segment dump has no decoded table bytes yet, so no pure
     * tables/memcpy port is possible -> stub CIV_ERR_UNSUPPORTED.
     * Read-only; no globals; no exit(). */
    return CIV_ERR_UNSUPPORTED;
}

void cursor_set(int cursor_id)
{
    /* Orig: FUN_1098_096c @ 1098:096c -> Cursor_Set. (cursor_id) void,
     * called after Menu_InitMainBar @ 1048:0000. LOADCURSOR + SETCURSOR
     * Win16 cursor install -> SDL3 system-cursor bridge TBD (future
     * SDL_CreateSystemCursor owner documented in boot.h/video.h; impl
     * stays here per packet I scope, video.* untouched). Stub: takes
     * the id for WinMain-order fidelity, installs nothing. */
    (void)cursor_id;
}

civ_error_t boot_open_windows(SDL_Window *win, SDL_Renderer *ren)
{
    civ_error_t err;

    /* TODO: GfxPort_Create @ 10d0:0000 + WdwMap_Open @ 1208:0af1,
     * WdwSmMap_Open @ 1210:025b, WdwStatus_Open @ 1218:0386 (see map.h). */
    err = video_init(win, ren);
    if (err != CIV_OK)
        return err;
    err = map_init();
    if (err != CIV_OK)
        return err;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t menu_init_mainbar(void)
{
    /* Orig: FUN_1048_0000 @ 1048:0000 -> Menu_InitMainBar.
     * TODO: CHECKMENUITEM + MODIFYMENU main-bar build/update ("&Save
     * Game\tCtrl+S" @ 14b0:0010, "&Auto Save" @ 14b0:0092, "Save First"
     * @ 14d0:0028; see dialog.h). Win16 USER menu handles -> SDL3 menu
     * TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_init_menu(void)
{
    civ_error_t err;

    /* WinMain menu step: Menu_InitMainBar @ 1048:0000 + Cursor_Set @
     * 1098:096c (cursor_set() stub in this file; SDL cursor TBD).
     * dialog_init owns the DLGPROC layer. */
    err = menu_init_mainbar();
    if (err != CIV_OK)
        return err;
    err = dialog_init();
    if (err != CIV_OK)
        return err;
    return CIV_ERR_UNSUPPORTED;
}

/* --- Packet F: font / prefs / DOS guards (see boot.h anchors) --- */

int dos_drive_ready(int drive)
{
    /* Orig: FUN_1000_1184 @ 1000:1184 -> Dos_DriveReady. OS-free Linux
     * equivalent: no removable-drive concept -> always ready (1). */
    (void)drive;
    return 1;
}

civ_error_t dos_set_path(int drive, const char *path)
{
    /* Orig: FUN_1000_116a @ 1000:116a -> Dos_SetPath. Linux: no drive
     * letters, cwd owned by the launcher -> no-op. */
    (void)drive;
    if (!path)
        return CIV_ERR_NOENT;
    return CIV_OK;
}

civ_error_t font_enum_families_init(void)
{
    /* Orig FUN_1008_0e99 @ 1008:0e99 -> Font_EnumFamiliesInit (stub).
     * ENUMFONTFAMILIES enumeration callback + ADDFONTRESOURCE(CIV_FONT_FILE
     * "civfonts.fon" @ 1420:00a4/00fe). civfonts.fon is a Win16 NE
     * DLL/font container (244224 B): MZ+NE header + font-directory
     * resources; exact FON header bytes + table offsets TBD from the
     * unpacked dump (do not implement full TTF here). SDL_ttf bridge
     * plan: read-only NE parse -> TTF_OpenFont face + family list
     * (ENUMFONTFAMILIES equivalent); raster via SDL_ttf when a face
     * lands. Win16 GDI font-table install -> SDL_ttf TBD. Read-only:
     * never writes to civ1win/ (REMOVEFONTRESOURCE tail in
     * video_shutdown_display, see video.h). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t config_load_prefs(void)
{
    /* TODO: Orig FUN_1008_0b4a @ 1008:0b4a -> Config_LoadPrefs. _LOPEN
     * (OF_READ) + _LREAD of CIV_PREFS_FILE "civ.prf" from the base path
     * (see sys_base_path), then option-struct fill with built-in defaults
     * when the file is missing/short. Prefs field map not yet decoded.
     * Read-only: never creates civ.prf. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t config_save_prefs(void)
{
    /* TODO: Orig FUN_1008_0bbf @ 1008:0bbf -> Config_SavePrefs. Guarded by
     * dos_drive_ready @ 1000:1184 (must be ready) + dos_set_path @
     * 1000:116a (cwd in place) before the _LCREAT/_LWRITE of CIV_PREFS_FILE
     * "civ.prf". Win16 file-handle path -> stdio bridge TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_run(SDL_Window *win, SDL_Renderer *ren)
{
    static civ_error_t (*const steps[])(void) = {
        boot_setup_paths,
        boot_check_memory,
        boot_register_classes,
        boot_load_fonts,
        boot_load_resources,
        boot_check_video,
        boot_maybe_convert_artwork,
        boot_load_prefs,
        boot_init_palette,
        boot_init_tables,
    };
    size_t i;
    civ_error_t err;

    for (i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        err = steps[i]();
        if (err != CIV_OK)
            return err;
    }
    err = boot_open_windows(win, ren);
    if (err != CIV_OK)
        return err;
    err = boot_init_menu();
    if (err != CIV_OK)
        return err;
    err = audio_init();
    if (err != CIV_OK)
        return err;
    return CIV_OK;
}

void boot_shutdown(void)
{
    /* Orig: FUN_1008_0329 @ 1008:0329 -> WinMain_Shutdown. Win16 teardown
     * order reproduced against the SDL3 owners: KILLTIMER for TIMERPROC
     * @ 1008:0d68 (200 ms SETTIMER installed by WdwMap_Open @ 1208:0af1,
     * pumped from the WinMain @ 1008:0000 message loop, impl map_timer_proc
     * -> game_tick, see map.h/game.h; SDL timer bridge TBD) first, then
     * FREEPROCINSTANCE for the DLGPROC @ 1098:16e2 / CIVDIALOGPROC @
     * 1098:1838 MAKEPROCINSTANCE targets (see dialog.h dlg_proc /
     * civ_dialog_proc via Dlg_CreateTracked @ 1098:124f), DESTROYWINDOW
     * for the main/map/status windows (see map.h), and
     * REMOVEFONTRESOURCE(CIV_FONT_FILE "civfonts.fon" @ 1420:00a4/00fe,
     * added by Font_EnumFamiliesInit @ 1008:0e99). SDL3 tails below in
     * the same order (timers/windows via video/map, dialogs, audio MCI
     * close). Void stub: installs/removes nothing yet (Win16 handles
     * TBD); no globals, no exit(), read-only. */
    video_shutdown();
    map_shutdown();
    dialog_shutdown();
    audio_shutdown();
}

/* --- Packet S2: shutdown/startup (see boot.h anchors) --- */

civ_error_t art_maybe_convert_artwork(void)
{
    /* Orig: FUN_10b8_0000 @ 10b8:0000 -> Art_MaybeConvertArtwork. Orig
     * shape: 1420:20a9 prompt, then optional artwork conversion gating
     * WorldGen_CopyPreset @ 10b8:079d (HMEMCPY 64000 B preset, see
     * worldgen.h worldgen_copy_preset). Prompt bytes not yet decoded
     * from the dump (no literal ported); conversion format/port bridge
     * TBD -> stub CIV_ERR_UNSUPPORTED. No globals, no exit(),
     * read-only. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t main_run_startup_sequence(void)
{
    /* Orig: FUN_11e0_0000 @ 11e0:0000 -> Main_RunStartupSequence.
     * Dispatcher for the MainMenu_WndProc @ 11e0:0221 choices over
     * CIV_MAINMENU_TEXT @ 1420:435b (see mainmenu.h): new-game paths ->
     * NewGame_Wizard @ 10e8:0000 via newgame_wizard_run (see newgame.h),
     * Load -> save_prompt_open @ 1130:000e, EARTH -> WorldGen_LoadEarth @
     * 1108:0243 (see worldgen.h), Hall -> HoF, Quit -> WinMain_Shutdown
     * @ 1008:0329 (boot_shutdown above). SDL modal/event bridge TBD ->
     * stub CIV_ERR_UNSUPPORTED. No globals, no exit(), read-only. */
    return CIV_ERR_UNSUPPORTED;
}

civ_prefs_t config_prefs_defaults(void)
{
    /* OS-free defaults 1:1 for civ.prf (Config_LoadPrefs @ 1008:0b4a /
     * Config_SavePrefs @ 1008:0bbf fill with these when the file is
     * missing/short): sounds on (g_nOptionFlags @ 12d0:0064 bit 0x10,
     * Sound_PlayId @ 1150:00c9 gate with DAT_1228:0104, see audio.h).
     * Remaining civ.prf fields TBD from the dump. By value, no I/O, no
     * globals, no exit(), read-only. */
    civ_prefs_t prefs;

    prefs.option_flags = CIV_PREFS_FLAG_SOUNDS_ON;
    return prefs;
}

int config_prefs_sounds_on(const civ_prefs_t *prefs)
{
    /* OS-free predicate for the sounds-on shape (Sound_PlayId gate head;
     * DAT_1228:0104 second half stays on the file/drive side, TBD). */
    if (!prefs)
        return 0;
    return (prefs->option_flags & CIV_PREFS_FLAG_SOUNDS_ON) ? 1 : 0;
}

int boot_video_caps_count(void)
{
    /* Video caps checklist @ 1420:0af4 entry count for
     * Video_CheckDriverCaps @ 1008:059a (GETDEVICECAPS; SDL mapping via
     * video_check_driver_caps, see video.h). Exact entry bytes not yet
     * decoded from the dump -> 0 until the dump lands. OS-free, no
     * globals, no exit(), read-only. */
    return 0;
}

const char *boot_video_caps_describe(int index)
{
    /* Checklist meaning by entry index (NULL when out of range or not
     * yet dumped, same contract as tribe_get_name_list, see newgame.h).
     * No entries decoded yet -> always NULL. */
    (void)index;
    return NULL;
}
