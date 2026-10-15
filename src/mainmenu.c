/* Main menu stubs. */
#include "mainmenu.h"

#include "save.h"

civ_error_t mainmenu_run(civ_mainmenu_choice_t *out)
{
    if (!out)
        return CIV_ERR_NOENT;
    *out = CIV_MENU_NONE;
    /* TODO: mirror MainMenu_WndProc @ 11e0:0221 —
     * Util_StrCopy(buf, CIV_MAINMENU_TEXT) + Dialog_MenuZ_Modal()
     * into g_nMenuChoice (12d8:44e2). */
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
        /* TODO: FUN_1108_0243 earth-map load; nonzero -> invalid choice. */
        return CIV_ERR_UNSUPPORTED;
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
