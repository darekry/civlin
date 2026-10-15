/* Boot sequence. Order mirrors WinMain @ 1008:0000; see boot.h for anchors.
 * No exit() here; no globals. Each step delegates to its owning module. */
#include "boot.h"

#include "audio.h"
#include "dialog.h"
#include "game.h"
#include "map.h"
#include "video.h"

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
    /* TODO: Font_EnumFamiliesInit @ 1008:0e99 + ADDFONTRESOURCE(civfonts.fon). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_load_resources(void)
{
    /* TODO: 5x ResMgr_LoadRsc @ 1190:0000 (CivData0..4.rsc via rsc_loader). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_check_video(void)
{
    /* TODO: Video_CheckDriverCaps @ 1008:059a (SDL3 renderer caps equiv). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_maybe_convert_artwork(void)
{
    /* TODO: Art_MaybeConvertArtwork @ 10b8:0000 (1420:20a9 prompt). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_load_prefs(void)
{
    /* TODO: Config_LoadPrefs @ 1008:0b4a. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_init_palette(void)
{
    /* TODO: Palette_Init @ 1090:0488 (godpal.c). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_init_tables(void)
{
    /* TODO: Init_UnitFontTables @ 10e8:2d46. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_open_windows(SDL_Window *win, SDL_Renderer *ren)
{
    civ_error_t err;

    /* TODO: GfxPort_Create @ 10d0:0000 + WdwMap/SmMap/Status opens
     * (@ 1208:0af1, 1210:025b, 1218:0386). */
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

civ_error_t boot_run(SDL_Window *win, SDL_Renderer *ren)
{
    static civ_error_t (*const steps[])(void) = {
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
