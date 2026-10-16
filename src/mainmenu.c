/* Main menu dispatchers (PARTIAL modal hosts + LIVE OS-free gates). */
#include "mainmenu.h"

#include "boot.h"
#include "dialog.h"
#include "save.h"
#include "worldgen.h"

civ_error_t mainmenu_run(civ_mainmenu_choice_t *out)
{
    /* Orig: FUN_11e0_0221 @ 11e0:0221, now mainmenu_run (MainMenu_WndProc).
     * PARTIAL dispatcher: OS-free gates below are LIVE 1:1 (no Win16, no
     * globals, read-only); the SDL3 modal pump stays STUB (never fake
     * CIV_OK). Orig flow: Util_StrCopy @ 1000:36b2 (buf, text @ 1420:435b
     * CIV_MAINMENU_TEXT, ref unpacked/civ.exe file offset 0xbb35b:
     * "_Start a New Game_Load a Saved Game_Play on EARTH_Customize World_
     * View Hall of Fame_Quit") + Dialog_MenuZ_Modal @ 1220:0156 loop while
     * choice < 0 || choice > 3 (DAT_1418_0002 preselect, DAT_1418_0004
     * cancel/back re-prompts; raw choice = zero-based MenuZ index, i.e.
     * 500-based button id minus CIV_DIALOG_MENUZ_BASE_ID via
     * dialog_menuz_id_to_index) into g_nMenuChoice (12d8:44e2), then
     * mainmenu_dispatch. SDL modal shape (TBD, documented, not pumped):
     * Dlg_CreateTracked @ 1098:124f host (0x94/0x41 intro constants via
     * main_startup_intro_dialog_id/flags) + MenuZ radio 500+i fan-out +
     * SETTIMER 100 (menuz_timer_interval_ms/dialog_menuz_timer_ms; 0 =
     * 'Can''t initialize timer in MenuZ' @ 1420:4a41, ref unpacked/civ.exe
     * file offset 0xbba41, windows.c) + PEEKMESSAGE/ISDIALOGMESSAGE pump to
     * cancel -1 (DAT_12b0_00a0/DAT_1418_0004 via dialog_menuz_cancel_code/
     * is_cancel) + re-prompt gate (mainmenu_should_loop /
     * dialog_menuz_should_loop) + dispatch.
     * TODO: Orig FUN_11e0_0221 @ 11e0:0221 SDL3 modal bridge TBD
     * (Dlg_CreateTracked @ 1098:124f host + Dialog_MenuZ_Modal @ 1220:0156
     * over CIV_MAINMENU_TEXT @ 1420:435b + PEEKMESSAGE/ISDIALOGMESSAGE pump;
     * OS-free '_' split already LIVE in mainmenu_item_count/at below). */
    int n_items;
    int split_n;
    int y_color;
    int y_mono;
    char probe[64];
    int pre_ok;
    int btn_id;
    int back;
    int valid;
    int dis;
    int timer_ms;
    int cancel;
    int loop_none;
    int loop_zero;
    int center;
    int show;
    int need_preset;
    int w;
    int h;
    int ctx;
    int pal;
    int off;
    int is_init;
    int is_extra;
    int row;
    int row_id;
    int row_target;
    int group;
    int intro_id;
    int intro_flags;

    if (!out)
        return CIV_ERR_NOENT;
    /* OS-free 1:1 packet-N split (ref unpacked/civ.exe file offset 0xbb35b:
     * CIV_MAINMENU_TEXT @ 1420:435b, 6 items): proves the '_' buffer shape
     * feeding Dialog_MenuZ_Modal @ 1220:0156 without pumping the Win16
     * PEEKMESSAGE/ISDIALOGMESSAGE modal. Read-only. */
    n_items = mainmenu_item_count();
    split_n = dialog_split_menu_count(CIV_MAINMENU_TEXT);
    y_color = mainmenu_dialog_y_offset(0);
    y_mono = mainmenu_dialog_y_offset(1);
    if (n_items != 6 || split_n != n_items)
        return CIV_ERR_UNSUPPORTED;
    if (y_color != 0xb4 || y_mono != 10)
        return CIV_ERR_BAD_DATA;
    if (mainmenu_item_at(0, probe, sizeof(probe)) != CIV_OK)
        return CIV_ERR_UNSUPPORTED;
    if (probe[0] == '\0')
        return CIV_ERR_UNSUPPORTED;
    /* OS-free 1:1 MenuZ id/gates @ 1220:0156 (RanDoMRadio 500+i fan-out +
     * DAT_1418_0002 preselect + DAT_12d8_24da disable mask + 100 ms timer +
     * -1 cancel/back; baseline mask 0 above = nothing disabled). */
    pre_ok = dialog_menuz_preselect_ok(-1, n_items);
    btn_id = dialog_menuz_index_to_id(0);
    back = dialog_menuz_id_to_index(btn_id);
    valid = dialog_menuz_id_is_valid(btn_id, n_items);
    dis = dialog_menuz_item_disabled(0, 0);
    timer_ms = dialog_menuz_timer_ms();
    cancel = dialog_menuz_cancel_code();
    if (pre_ok != 0 || btn_id != CIV_DIALOG_MENUZ_BASE_ID || back != 0)
        return CIV_ERR_BAD_DATA;
    if (valid == 0 || dis != 0)
        return CIV_ERR_BAD_DATA;
    if (timer_ms != menuz_timer_interval_ms())
        return CIV_ERR_BAD_DATA;
    if (cancel != CIV_DIALOG_MENUZ_CANCEL)
        return CIV_ERR_BAD_DATA;
    /* OS-free 1:1 re-prompt gate (choice < 0 || choice > 3): NONE (-1,
     * cancel/back) must loop, 0 (new game) must accept; dialog twin must
     * agree (same Orig @ 11e0:0221 / @ 1220:0156 shape). */
    loop_none = mainmenu_should_loop(CIV_MENU_NONE);
    loop_zero = mainmenu_should_loop(0);
    if (loop_none != 1 || loop_zero != 0)
        return CIV_ERR_BAD_DATA;
    if (dialog_menuz_should_loop(CIV_MENU_NONE) != loop_none)
        return CIV_ERR_BAD_DATA;
    if (dialog_menuz_is_cancel(0, -1) != 0)
        return CIV_ERR_BAD_DATA;
    /* OS-free 1:1 tracked-host gates @ 1098:124f for the modal host
     * (intro dialog 0x94/0x41 via main_startup_intro_dialog_id/flags in
     * boot.h; preset 0x200x0x140 + 0x18 ctx + 0xec palette + push slot +
     * ctl 0x65-0x6e init + 0x8f earth-extra 0x323-0x332 shape). */
    intro_id = main_startup_intro_dialog_id();
    intro_flags = main_startup_intro_dialog_flags();
    center = dlg_create_should_center((unsigned)intro_flags);
    show = dlg_create_should_show((unsigned)intro_flags);
    need_preset = dlg_id_needs_preset_size(intro_id);
    w = dlg_preset_w(intro_id);
    h = dlg_preset_h(intro_id);
    ctx = dlg_tracked_ctx_size();
    pal = dlg_tracked_pal_count();
    off = dlg_tracked_push_off(0);
    is_init = dlg_ctl_is_tracked_init(CIV_DLG_TRACKED_CTL_FIRST);
    is_extra = dlg_ctl_is_earth_extra(0x323);
    if (intro_id != CIV_DLG_INTRO_ID || intro_flags != CIV_DLG_INTRO_FLAGS)
        return CIV_ERR_BAD_DATA;
    if (center != 1 || show != 1 || need_preset == 0)
        return CIV_ERR_BAD_DATA;
    if (w != CIV_DLG_PRESET_W || h != CIV_DLG_PRESET_H)
        return CIV_ERR_BAD_DATA;
    if (ctx != CIV_DLG_TRACKED_CTX_SIZE || pal != CIV_DLG_TRACKED_PAL_COUNT)
        return CIV_ERR_BAD_DATA;
    if (off != 0 || is_init == 0 || is_extra == 0)
        return CIV_ERR_BAD_DATA;
    /* OS-free 1:1 dispatch-row probe @ 1048:01cd (ref unpacked/civ.exe seg
     * 1048 table @ CS:0x3d8 + handler offsets @ CS:0x460, CX = 0x44): first
     * row 101 -> handler 0x1ff, group 1. Proves the scan hit before the
     * Win16 handler bridge. Read-only. */
    row = menu_dispatch_find(101);
    row_id = menu_dispatch_id_at(row);
    row_target = menu_dispatch_target_at(row);
    group = menu_command_group(101);
    if (row != 0 || row_id != 101 || row_target != 0x1ff || group != 1)
        return CIV_ERR_BAD_DATA;
    *out = CIV_MENU_NONE;
    return CIV_ERR_UNSUPPORTED;
}

int mainmenu_item_count(void)
{
    /* Orig packet-N split @ 11e0:0221 / 1420:435b (see mainmenu.h), same
     * shape as Dialog_SplitMenuText @ 1220:0000: leading '_' ends the
     * (empty) title, each later '_'/'\0' closes one item. Shipped text
     * (verified at unpacked/civ.exe offset 0xbb35c) has 6 items, so
     * count = separators after the leading one + 1 (the '\0'-closed
     * tail, e.g. Quit). */
    const char *t = CIV_MAINMENU_TEXT;
    int n = 0;
    int i = 0;

    if (t[0] == '\0')
        return 0;
    for (i = 0; t[i] != '\0'; i++) {
        if (t[i] == '_' && i > 0)
            n++;
    }
    return n + 1;
}

civ_error_t mainmenu_item_at(int index, char *out, size_t cap)
{
    /* Orig same split: copy the index-th '_' field (0-based MenuZ index).
     * Pure string scan, no Win16, no globals, no exit(). */
    const char *t = CIV_MAINMENU_TEXT;
    int cur = 0;
    size_t start = 0;
    size_t end = 0;
    size_t len = 0;
    size_t i = 0;

    if (!out)
        return CIV_ERR_NOENT;
    if (cap == 0)
        return CIV_ERR_RANGE;
    if (index < 0 || index >= mainmenu_item_count())
        return CIV_ERR_RANGE;
    /* Skip to the index-th field: field 0 starts after t[0] == '_'. */
    start = 1;
    for (cur = 0; cur < index; cur++) {
        while (t[start] != '\0' && t[start] != '_')
            start++;
        if (t[start] == '_')
            start++;
    }
    end = start;
    while (t[end] != '\0' && t[end] != '_')
        end++;
    len = end - start;
    if (len + 1u > cap)
        return CIV_ERR_RANGE;
    for (i = 0; i < len; i++)
        out[i] = t[start + i];
    out[len] = '\0';
    return CIV_OK;
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
     * NOENT; match -> UNSUPPORTED until the SDL action bridge lands.
     * TODO: Orig FUN_1048_01cd @ 1048:01cd handler bodies (CALLFs in
     * mainmenu.h: 101 save-family @ 1198:00a3, 207/208 toggles @
     * 1210:1134/1218:077d, 506 nation @ 1070:1f58, census family @ 1018)
     * -> SDL action bridge TBD; stub CIV_ERR_UNSUPPORTED on match. */
    int idx;
    int row_id;
    int row_target;
    int group;

    idx = menu_dispatch_find(cmd);
    if (idx < 0)
        return CIV_ERR_NOENT;
    /* OS-free 1:1 row decode (ref unpacked/civ.exe seg 1048 table @
     * CS:0x3d8 + handler offsets @ CS:0x460, CX = 0x44 entries): proves
     * the scan hit before the Win16 handler bridge. Read-only. */
    row_id = menu_dispatch_id_at(idx);
    row_target = menu_dispatch_target_at(idx);
    group = menu_command_group(cmd);
    (void)row_target;
    (void)group;
    if (row_id != cmd)
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

int mainmenu_should_loop(int choice)
{
    /* Orig MainMenu_WndProc @ 11e0:0221 re-prompt gate: do { Util_StrCopy
     * @ 1000:36b2 + Dialog_MenuZ_Modal @ 1220:0156 -> g_nMenuChoice @
     * 12d8:44e2 } while (choice < 0 || choice > 3) (DAT_1418_0004
     * cancel/back re-prompts; 0..3 enter the startup dispatch, 4/5 are
     * handled in-loop via HoF/quit paths). Sibling gate
     * dialog_menuz_should_loop @ 1220:0156 must agree (probed in
     * mainmenu_run above). */
    return (choice < 0 || choice > 3) ? 1 : 0;
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
