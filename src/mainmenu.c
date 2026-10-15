/* Main menu stubs. */
#include "mainmenu.h"

#include "save.h"
#include "worldgen.h"

civ_error_t mainmenu_run(civ_mainmenu_choice_t *out)
{
    /* Orig: FUN_11e0_0221 @ 11e0:0221, now mainmenu_run (MainMenu_WndProc).
     * Orig flow (stubbed: SDL3 modal TBD, no globals here):
     *   Util_StrCopy @ 1000:36b2 (buf, text @ 1420:435b CIV_MAINMENU_TEXT)
     *   + Dialog_MenuZ_Modal @ 1220:0156 loop while choice < 0 || choice > 3
     *   (DAT_1418_0002 preselect, DAT_1418_0004 cancel/back re-prompts;
     *   raw choice = zero-based MenuZ index, i.e. 500-based button id minus
     *   CIV_DIALOG_MENUZ_BASE_ID via dialog_menuz_id_to_index) into
     *   g_nMenuChoice (12d8:44e2), then mainmenu_dispatch. Read-only stub:
     *   validates out, reports NONE, no modal pumped. */
    if (!out)
        return CIV_ERR_NOENT;
    *out = CIV_MENU_NONE;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t mainmenu_dispatch(civ_mainmenu_choice_t choice)
{
    civ_save_dialog_t dlg;

    switch (choice) {
    case CIV_MENU_LOAD:
        /* TODO: save_prompt_open @ 1130:000e; -1 -> choice stays invalid. */
        return save_prompt_open(&dlg);
    case CIV_MENU_EARTH:
        /* Orig FUN_1108_0243 @ 1108:0243 -> worldgen_load_earth (see
         * worldgen.h WorldGen_LoadEarth); nonzero -> invalid choice. */
        return worldgen_load_earth();
    case CIV_MENU_HALL:
        /* TODO: HoF_Load @ 1080:0023 + HoF_Show @ 1080:0119. */
        return CIV_ERR_UNSUPPORTED;
    case CIV_MENU_QUIT:
        /* TODO: WinMain_Shutdown @ 1008:0329. Caller exits its loop. */
        return CIV_ERR_UNSUPPORTED;
    default:
        break;
    }
    /* TODO: choices 0/2 -> NewGame_Wizard paths. */
    return CIV_ERR_UNSUPPORTED;
}
