/* Boot sequence. Order mirrors WinMain @ 1008:0000; see boot.h for anchors.
 * No exit() here; no globals. Each step delegates to its owning module. */
#include "boot.h"

#include "audio.h"
#include "dialog.h"
#include "game.h"
#include "map.h"
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
    /* TODO: Sys_CheckFreeMemory @ 11e8:0000 (GETFREESPACE/host equiv). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_register_classes(void)
{
    /* TODO: App_RegisterWindowClasses @ 1098:06f8 (+ Ctrl_RegisterCustomClasses). */
    return CIV_ERR_UNSUPPORTED;
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
    /* TODO: Art_MaybeConvertArtwork @ 10b8:0000 (1420:20a9 prompt). */
    return CIV_ERR_UNSUPPORTED;
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
    /* TODO: Game_InitGlobalTables @ 11e8:0337 (key remap, 64 KB map buffer,
     * Rng_Init, string tables via 11f8:0080) + Init_UnitFontTables @
     * 10e8:2d46. */
    return CIV_ERR_UNSUPPORTED;
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

civ_error_t boot_init_menu(void)
{
    civ_error_t err;

    /* TODO: Menu_InitMainBar @ 1048:0000 + Cursor_Set @ 1098:096c. */
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
    /* TODO: Orig FUN_1008_0e99 @ 1008:0e99 -> Font_EnumFamiliesInit.
     * ENUMFONTFAMILIES enumeration callback + ADDFONTRESOURCE(CIV_FONT_FILE
     * "civfonts.fon" @ 1420:00a4/00fe). Win16 GDI font-table install ->
     * SDL3 text backend TBD. Read-only: never writes to civ1win/. */
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
    /* TODO: WinMain_Shutdown @ 1008:0329 order (timers, ports, palette,
     * font resource, window). */
    video_shutdown();
    map_shutdown();
    dialog_shutdown();
    audio_shutdown();
}
