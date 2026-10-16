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

/* --- Chain 2026-10-16 (18): Menu_DispatchCommand table (goal 2) ---
 * See mainmenu.h block. Table data LIVE 1:1 (read-only); matched
 * handler bodies are Win16 -> STUB. */

static const menu_dispatch_entry_t s_menu_dispatch_table[] = {
    /* Game-ish 101..115 (verified: 101 -> CALLF 1198:00a3 save-family;
     * 102 -> PUSH 0x3d + Main_EnqueueResString @ 1008:0a8d res id 61;
     * 106..112 share handler 1048:0216). */
    { 101, 0x1ff }, { 102, 0x207 }, { 103, 0x20c }, { 104, 0x211 },
    { 105, 0x216 }, { 106, 0x216 }, { 107, 0x216 }, { 108, 0x216 },
    { 109, 0x216 }, { 110, 0x216 }, { 111, 0x216 }, { 112, 0x216 },
    { 113, 0x237 }, { 114, 0x23c }, { 115, 0x251 },
    /* View-ish 201..208 (verified: 207 -> Menu_ToggleView1210 @
     * 1210:1134 + Menu_InitMainBar tail; 208 -> Menu_ToggleView1218 @
     * 1218:077d + Menu_InitMainBar tail: SHOWWINDOW toggle bit +0x2c). */
    { 201, 0x2a0 }, { 202, 0x2a3 }, { 203, 0x2a6 }, { 204, 0x2a9 },
    { 205, 0x2ac }, { 207, 0x2af }, { 208, 0x2b7 },
    /* 301..318 share handler 1048:02bf (single submenu dispatcher;
     * bodies around +2ca..+2bf call into 1018 census family). */
    { 301, 0x2bf }, { 302, 0x2bf }, { 303, 0x2bf }, { 304, 0x2bf },
    { 305, 0x2bf }, { 306, 0x2bf }, { 307, 0x2bf }, { 308, 0x2bf },
    { 309, 0x2bf }, { 310, 0x2bf }, { 311, 0x2bf }, { 312, 0x2bf },
    { 313, 0x2bf }, { 315, 0x2bf }, { 316, 0x2bf }, { 318, 0x2bf },
    /* Civilization/Info-ish 401..406 (handlers call Census_ShowCityStatus
     * @ 1018:208f, Census_ShowUnitList @ 1018:067b, City_ShowCensusReport
     * @ 1018:1d5b, demo_show_demographics @ 1188:123a; 405 -> 1048:0310
     * Census_ShowCityProduction @ 1018:0be0; PromptNation to 506/0x374,
     * zob. mainmenu.h chain-19). */
    { 401, 0x2d2 }, { 402, 0x2e4 }, { 403, 0x2f6 }, { 404, 0x2fe },
    { 405, 0x310 }, { 406, 0x322 },
    /* 501..506 (handlers call dialog_res_list_spec @ 1068:0295,
     * Main_EnqueueResString @ 1008:0a8d, Font_EnumFamiliesCallbackTail @
     * 1008:09d3; exact submenu TBD). */
    { 501, 0x334 }, { 502, 0x33c }, { 503, 0x344 }, { 504, 0x35a },
    { 505, 0x363 }, { 506, 0x374 },
    /* 601..606 share handler 1048:037b (TBD; single dispatcher). */
    { 601, 0x37b }, { 602, 0x37b }, { 603, 0x37b }, { 604, 0x37b },
    { 605, 0x37b }, { 606, 0x37b },
    /* 701..709 (handlers +38d..+3ad; exact mapping TBD). */
    { 701, 0x38d }, { 702, 0x391 }, { 703, 0x395 }, { 704, 0x399 },
    { 705, 0x39d }, { 706, 0x3a1 }, { 707, 0x3a5 }, { 708, 0x3a9 },
    { 709, 0x3ad },
    /* 801..803 (handlers +3b7/+3be/+3c5; exact mapping TBD). */
    { 801, 0x3b7 }, { 802, 0x3be }, { 803, 0x3c5 },
};

int menu_dispatch_count(void)
{
    /* Orig table length CX = 0x44 (1048:01e4). */
    return (int)(sizeof(s_menu_dispatch_table) /
        sizeof(s_menu_dispatch_table[0]));
}

int menu_dispatch_find(int cmd)
{
    /* Orig Menu_DispatchCommand @ 1048:01cd scan: BX = 0x3d8, CX = 0x44;
     * while (CX-- != 0) { if (CS:[BX] == cmd) -> handler CS:[BX+0x88];
     * BX += 2; } miss -> return 0. Linux index or -1 on miss. */
    int i;
    int n = menu_dispatch_count();

    for (i = 0; i < n; i++) {
        if (s_menu_dispatch_table[i].id == cmd)
            return i;
    }
    return -1;
}

int menu_dispatch_id_at(int idx)
{
    /* Orig same table CS:0x3d8 row. */
    if (idx < 0 || idx >= menu_dispatch_count())
        return -1;
    return s_menu_dispatch_table[idx].id;
}

int menu_dispatch_target_at(int idx)
{
    /* Orig same table CS:[BX+0x88] handler offset (segment 1048). */
    if (idx < 0 || idx >= menu_dispatch_count())
        return -1;
    return s_menu_dispatch_table[idx].target_off;
}

int menu_command_group(int cmd)
{
    /* Orig ID hundreds (100s Game, 200s View, 300s shared submenu,
     * 400s Civ/Info, 500s/600s/700s/800s TBD submenus): group = cmd/100
     * for 100..899, else 0. Gaps (206/209/314/317) are simply absent
     * from the table (menu_dispatch_find -> -1). */
    if (cmd < 100 || cmd > 899)
        return 0;
    return cmd / 100;
}

civ_error_t menu_dispatch_command(int cmd)
{
    /* Orig Menu_DispatchCommand @ 1048:01cd: match -> indirect handler
     * (returns handler AX); miss -> 0. Lookup LIVE, handler bodies Win16
     * (CALLFs in mainmenu.h) -> STUB (skaner: STUB). Miss (orig 0) ->
     * NOENT; match -> UNSUPPORTED until the SDL action bridge lands. */
    if (menu_dispatch_find(cmd) < 0)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (19): view toggles (goals 3+5) ---
 * See mainmenu.h block. OS-free 1:1 (no Win16, no globals, no exit()). */

int mainmenu_dialog_y_offset(int flag_28fc)
{
    /* Orig MainMenu_WndProc @ 11e0:0221 MenuZ y-offset (decompiled
     * 2026-10-16: if (DAT_12d8_28fc == 0) iVar4 = 0xb4 else 10, added to
     * local_12 for Dialog_MenuZ_Modal @ 1220:0156; mono/VGA branch like
     * opcode 8 in Anim_AdvanceFrames @ 10b0:0461). */
    return (flag_28fc == 0) ? 0xb4 : 10;
}

int menu_toggle_show_cmd(int hidden_flag)
{
    /* Orig Menu_ToggleView1210 @ 1210:1134 / Menu_ToggleView1218 @
     * 1218:077d: *(+0x2c) == 0 ? (*(0x2c) = 1, SHOWWINDOW 5) :
     * (*(0x2c) = 0, SHOWWINDOW 0) (na handle DAT_1420_48fa/4964). */
    return (hidden_flag == 0) ? 5 : 0;
}

int menu_toggle_view_window(int cmd)
{
    /* Orig dispatch handlers 1048:02af/02b7: 207 -> 12b0:001c smmap
     * (WdwSmMap_Open @ 1210:025b), 208 -> 12b0:0034 status (WdwStatus_Open
     * @ 1218:0386); main 12b0:0004 (WdwMap_Open @ 1208:0af1) bez toggla. */
    if (cmd == 207)
        return 0x1c;
    if (cmd == 208)
        return 0x34;
    return -1;
}
