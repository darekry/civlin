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
 * Raw modal index is the zero-based MenuZ item index (500-based button id
 * minus CIV_DIALOG_MENUZ_BASE_ID 500; see dialog_menuz_id_to_index /
 * dialog_menuz_index_to_id @ 1220:0156, DAT_1418_0002 preselect,
 * DAT_1418_0004 cancel/back re-prompts); the enum below records the
 * dispatch values observed in MainMenu_WndProc. Packet N: orig loops
 * Util_StrCopy(buf, text @ 1420:435b) + Dialog_MenuZ_Modal @ 1220:0156
 * while choice < 0 || choice > 3, then dispatches. Stub stays read-only
 * (validates out only, no globals, no exit) until the SDL3 modal lands.
 */
#ifndef CIVLIN_MAINMENU_H
#define CIVLIN_MAINMENU_H

#include "civ_common.h"

#define CIV_MAINMENU_TEXT \
    "_Start a New Game_Load a Saved Game_Play on EARTH_Customize World_View Hall of Fame_Quit"

/* Dispatch values as observed in MainMenu_WndProc (g_nMenuChoice).
 * Raw choice is the zero-based MenuZ index over CIV_MAINMENU_TEXT @ 1420:435b
 * (ref unpacked/civ.exe file offset 0xbb35b, 6 items); 0 = Start New Game,
 * 1 = Load, 2 = Customize World, 3 = EARTH, 4 = Hall, 5 = Quit. 0/2 both
 * enter NewGame_Wizard @ 10e8:0000 (2 = custom-world steps 3/4 path). */
typedef enum civ_mainmenu_choice {
    CIV_MENU_NONE = -1,
    CIV_MENU_NEWGAME = 0,  /* -> NewGame_Wizard @ 10e8:0000 */
    CIV_MENU_LOAD = 1,   /* -> save_prompt_open */
    CIV_MENU_CUSTOMIZE = 2, /* -> NewGame_Wizard @ 10e8:0000 steps 3/4 */
    CIV_MENU_EARTH = 3,  /* -> FUN_1108_0243 earth map */
    CIV_MENU_HALL = 4,   /* -> HoF_Load + HoF_Show */
    CIV_MENU_QUIT = 5    /* -> WinMain_Shutdown */
} civ_mainmenu_choice_t;

/* --- Chain 2026-10-16 (19): view toggles + nation entry (goals 3-5) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Menu_ToggleView1210 @ 1210:1134 (SHOWWINDOW-toggle 12b0:001c smmap +
 *     Menu_InitMainBar tail: if (12b0:001c != 0 && DAT_1420_48fa != 0) {
 *     GLOBALLOCK -> *(+0x2c) == 0 ? (*(0x2c) = 1, SHOWWINDOW 5) :
 *     (*(0x2c) = 0, SHOWWINDOW 0); GLOBALUNLOCK; } Menu_InitMainBar @
 *     1048:0000. Handler 1048:02af (CALLF 1210:1134 + JMP 03cc) = ID 207.
 *     Jakie widoki: 001c = World Map (smmap, WdwSmMap_Open @ 1210:025b,
 *     map_open_small STUB); 0034 (ponizej) = Status (WdwStatus_Open @
 *     1218:0386, map_open_status STUB); glowna mapa to 0004 (WdwMap_Open
 *     @ 1208:0af1, map_open_main STUB). Zwiazek z WdwMap/SmMap/Status_Open
 *     i map_refresh_status: toggles chowaja/pokazuja dziecko + odswiezaja
 *     pasek (InitMainBar); pelny repaint przez Map_RefreshAllViews @
 *     1110:03e9 (STUB) -> Game_UpdateStatusPanel @ 1088:13d5 +
 *     WdwStatus_Refresh @ 1218:0801 (dirty +0x2c, map_refresh_status STUB).
 *     SmMap_CenterOnClick @ 1210:056e (renamed) to recenter smmap z pompy
 *     Sys_PumpMessages @ 1108:0000 (0x1c + DAT_1420_12b0 + coords), nie
 *     toggle (toggle nie bierze coords).
 *   Menu_ToggleView1218 @ 1218:077d (SHOWWINDOW-toggle 12b0:0034 status +
 *     Menu_InitMainBar tail, ten sam ksztalt +0x2c na handle DAT_1420_4964;
 *     handler 1048:02b7 = ID 208).
 *   Menu_PromptNation @ 1070:1f58 (Which-nation + Dialog_ShowMessage:
 *     if ((DAT_12d0_00b6 & 0xfe00) != 0) { Util_StrCopy Which-nation? @
 *     1420:146d; for (i = 1; i < 8; i++) { StrAppend "_" @ 1420:147b +
 *     StrAppend 12d8:03da+i*0x20 (runtime tribe table 7 x 0x20, z
 *     Tribe_GetNameList @ 11f8:0080 / Res blob 0x94, zob. newgame.h) };
 *     DAT_12d8_24da = DAT_12d0_00b6>>9 ^ 0xff; i = Dialog_ShowMessage @
 *     1220:00d2 + 1; if (0 < i) { DAT_1290_0114 = i; Ship_OpenView @
 *     1070:0000 (renamed) + Ship_ShowView(i,0xffff,1) @ 1070:0327 +
 *     Ship_CloseView @ 1070:02b5 } }. Kiedy: podglad spaceship dla nacji
 *     (nie wybor startowy — startowy to NewGame_Wizard @ 10e8:0000 step 2
 *     Pick tribe; to jest podglad/launch ship dla istniejacej nacji, brat
 *     Ship_LaunchAndShow @ 1070:2015 + Ship_BuildPart @ 1070:16c3 +
 *     Ship_EditParts @ 1070:1952). Lista nacji: 7 wpisow 12d8:03da+
 *     (barbarians osobno: nie w tej petli; 12d0:00b6 maska launch,
 *     0xfe00 gate). Zwiazek z tribe_get_name_list (STUB w newgame.c) i
 *     Text_FillResStringList @ 1098:1f4f: ta sama runtime tabela 12d8:03da
 *     (FillResStringList to ogolny list-builder, PromptNation to jego
 *     Which-nation instancja).
 *   Menu_DispatchCommand wpisy 207/208 (toggLe 1210/1218) i NATION (goal
 *     mowil 405, verified DISASM 1048:0374 = ID 506): 207 -> 1048:02af
 *     (CALLF 1210:1134), 208 -> 1048:02b7 (CALLF 1218:077d) — spojne z
 *     tabela 68 wpisow LIVE (menu_dispatch_*); 405 -> 1048:0310 to
 *     Census_ShowCityProduction @ 1018:0be0 (NIE PromptNation); PromptNation
 *     to handler 1048:0374 (CALLF 1070:1f58) = ID 506 (0x1fa), sasiad 505
 *     -> 0363 / 504 -> 035a w tej samej 40x grupie. Tabela w mainmenu.c
 *     juz ma {506,0x374} LIVE — korekta tylko tego komentarza (poprzedni
 *     wpis 405 byl bledem; 405 zostaje census, bez zmian w kodzie).
 * Port identyfikatorow widokow LIVE 1:1 (no Win16, no globals); SHOWWINDOW
 * + Menu_InitMainBar ogony STUB. No exit(). */

/* Orig MainMenu_WndProc @ 11e0:0221 MenuZ y-offset (decompiled 2026-10-16:
 * DAT_12d8_28fc == 0 ? iVar4 = 0xb4 : 10, dodane do local_12 dla
 * Dialog_MenuZ_Modal; mono/VGA branch jak w opcode 8 AdvanceFrames) ->
 * mainmenu_dialog_y_offset. 1:1 0xb4 gdy color (flag == 0), 10 gdy mono. */
int mainmenu_dialog_y_offset(int flag_28fc);

/* Orig MainMenu_WndProc @ 11e0:0221 packet-N text split (Util_StrCopy @
 * 1000:36b2 over CIV_MAINMENU_TEXT @ 1420:435b, verified in unpacked/civ.exe
 * at file offset 0xbb35b: "_Start a New Game_..._Quit") feeding
 * Dialog_MenuZ_Modal @ 1220:0156 -> mainmenu_item_count / mainmenu_item_at.
 * OS-free 1:1 '_' split, same shape as Dialog_SplitMenuText @ 1220:0000
 * (leading '_' ends the empty title; each later '_'/'\0' closes one item;
 * no empty items): count = separators after the leading one + 1 (6 for
 * the shipped text). at() copies the index-th item (0-based MenuZ index,
 * same numbering as the raw choice in mainmenu_run) NUL-terminated.
 * NOENT on NULL out, RANGE on index OOR / cap == 0 / text does not fit.
 * No Win16, no globals, no exit(), read-only. */
int mainmenu_item_count(void);
/* Orig same split @ 1420:435b (Util_StrCopy @ 1000:36b2, modal @ 1220:0156)
 * -> mainmenu_item_at (field copy, see block above). */
civ_error_t mainmenu_item_at(int index, char *out, size_t cap);

/* Orig Menu_ToggleView1210/1218 +0x2c shape -> menu_toggle_show_cmd.
 * 1:1: hidden_flag == 0 -> 5 (SW_SHOW), inaczej 0 (SW_HIDE). */
int menu_toggle_show_cmd(int hidden_flag);

/* Orig handler IDs 207/208 -> menu_toggle_view_window. 1:1: 207 -> 0x1c
 * (smmap World Map), 208 -> 0x34 (status), inaczej -1. Glowna mapa 0004
 * nie ma toggla (zawsze widoczna). */
int menu_toggle_view_window(int cmd);

/* Orig: FUN_11e0_0221 @ 11e0:0221, now mainmenu_run — shows the menu
 * (Util_StrCopy @ 1000:36b2 + Dialog_MenuZ_Modal @ 1220:0156 loop while
 * choice<0 || choice>3 over CIV_MAINMENU_TEXT @ 1420:435b) and returns the
 * raw choice. Partial dispatcher: OS-free split (mainmenu_item_count/at 1:1
 * over CIV_MAINMENU_TEXT @ 1420:435b, ref unpacked/civ.exe file offset
 * 0xbb35b) + y-offset (mainmenu_dialog_y_offset) + MenuZ id/gates
 * (dialog_menuz_* @ 1220:0156, timer 100, cancel -1; fala 23 tail:
 * dialog_menuz_art_mode(-1/0/3) = 0/1/2 + dialog_menuz_is_cancel triple +
 * dlg_center_tracked_window @ 1098:0b33 centered 0x200x0x140 at (64,80))
 * + tracked host gates (dlg_create_* @ 1098:124f for the 0x94/0x41 intro
 * host: preset 0x200x0x140 + 0x18 ctx + ctl 0x65-0x6e, ref unpacked/civ.exe
 * "RanDoMRadio" @ 0xb7ac6 / "RanDoMStatic" @ 0xb7ad2; close pop(1) = 0 via
 * dlg_tracked_pop_count @ 1098:1612 + rebase 1100 - 1000 = 100 via
 * anim_rebase_clock @ 10b0:005b) + dispatch-row
 * probe (menu_dispatch_* @ 1048:01cd) are wired read-only; the SDL3 modal
 * pump (Dlg_CreateTracked host + PEEKMESSAGE/ISDIALOGMESSAGE loop +
 * RanDoMRadio 500+i fan-out) stays CIV_ERR_UNSUPPORTED, never fake CIV_OK.
 * NOENT on NULL out, else UNSUPPORTED with *out=NONE; read-only (no modal
 * pumped until the SDL3 bridge lands). Raw choice is the
 * zero-based MenuZ index; 500-based button ids map via dialog_menuz_*
 * (CIV_DIALOG_MENUZ_BASE_ID). */
civ_error_t mainmenu_run(civ_mainmenu_choice_t *out);

/* Orig MainMenu_WndProc @ 11e0:0221 re-prompt gate (choice < 0 || choice > 3)
 * -> mainmenu_should_loop. 1:1 nonzero (1) when the modal must re-prompt
 * (cancel/back DAT_1418_0004 or OOR pick), 0 when the choice is accepted
 * into the startup dispatch (0..3 feed Main_RunStartupSequence @ 11e0:0000;
 * sibling gate dialog_menuz_should_loop @ 1220:0156 must agree). No Win16,
 * no globals, no exit(), read-only. */
int mainmenu_should_loop(int choice);

/* Orig MainMenu_WndProc @ 11e0:0221 dispatch gate (choice range over
 * CIV_MAINMENU_TEXT @ 1420:435b items, ref unpacked/civ.exe file offset
 * 0xbb35b, 6 items; same '_' split as Dialog_SplitMenuText @ 1220:0000,
 * cap CIV_SPLIT_MAX 0x18) -> mainmenu_choice_is_valid. 1:1 nonzero (1)
 * for a dispatchable MenuZ choice 0..5 (0/2 = NewGame_Wizard paths TBD,
 * 1 = Load, 3 = EARTH, 4 = Hall, 5 = Quit; see enum above), 0 for
 * CIV_MENU_NONE (-1, cancel/back) and any OOR value the re-prompt gate
 * (mainmenu_should_loop) would loop on. No Win16, no globals, no exit(). */
int mainmenu_choice_is_valid(int choice);

/* Dispatches one choice to load/earth/hall/quit/new-game. PARTIAL:
 * OS-free choice gate (mainmenu_choice_is_valid above, 0..5 over
 * CIV_MAINMENU_TEXT @ 1420:435b ref unpacked/civ.exe file offset 0xbb35b)
 * is LIVE 1:1; each arm delegates to its owner and propagates the owner
 * rc (never fake CIV_OK); Win16 modal/action tails inside the owners stay
 * CIV_ERR_UNSUPPORTED with TODO Orig below.
 * Orig targets: 0/2 NewGame_Wizard @ 10e8:0000 (via newgame_wizard_run,
 * see newgame.h), 1 save_prompt_open @ 1130:000e, 3 FUN_1108_0243 @
 * 1108:0243 (worldgen_load_earth, see worldgen.h), 4 HoF_Load @ 1080:0023
 * + HoF_Show @ 1080:0119 (see halloffame.h), 5 WinMain_Shutdown @
 * 1008:0329 (via boot_shutdown, see boot.h); caller MainMenu_WndProc @
 * 11e0:0221.
 * Invalid (non-dispatchable) choice -> CIV_ERR_NOENT (orig re-prompts,
 * never dispatches); valid arms return the owner rc (currently
 * CIV_ERR_UNSUPPORTED until each owner modal lands).
 * CIV_MENU_EARTH (3) -> worldgen_load_earth (Orig FUN_1108_0243 @ 1108:0243,
 * see worldgen.h); nonzero result keeps the menu choice invalid. */
civ_error_t mainmenu_dispatch(civ_mainmenu_choice_t choice);

/* --- Chain 2026-10-16 (18): Menu_DispatchCommand table (goal 2) ---
 * Orig Menu_DispatchCommand @ 1048:01cd (civ.exe, x86:LE:16:Protected
 * Mode): CX = 0x44 linear scan over the ID table at CS:0x3d8 (68 u16:
 * 101..115, 201..205, 207, 208, 301..313, 315, 316, 318, 401..406,
 * 501..506, 601..606, 701..709, 801..803; gaps 206/209/314/317 simply
 * absent); on match JMP CS:[BX+0x88] (handler offsets 1048:01ff..03c5,
 * second table half at CS:0x460), else return 0. Decoded handler
 * anchors (raw CS bytes read this pass, CALLF = 9A oo oo ss ss):
 *   101 -> 1048:01ff CALLF 1198:00a3 (save-family; cf. save_save_game
 *     callers @ 1198:00a3 in save.h)
 *   102 -> 1048:0207 PUSH 0x3d + Main_EnqueueResString @ 1008:0a8d
 *   207 -> 1048:02af tail Menu_ToggleView1210 @ 1210:1134 (SHOWWINDOW
 *     toggle 12b0:001c + Menu_InitMainBar)
 *   208 -> 1048:02b7 tail Menu_ToggleView1218 @ 1218:077d (SHOWWINDOW
 *     toggle 12b0:0034 + Menu_InitMainBar)
 *   40x handlers call Census_ShowCityStatus @ 1018:208f,
 *     Census_ShowUnitList @ 1018:067b, City_ShowCensusReport @ 1018:1d5b,
 *     demo_show_demographics @ 1188:123a (405 -> 1048:0310 =
 *     Census_ShowCityProduction @ 1018:0be0, NIE PromptNation)
 *   506 -> 1048:0374 CALLF Menu_PromptNation @ 1070:1f58 (Which-nation
 *     Dialog_ShowMessage @ 1220:00d2 + Ship_OpenView @ 1070:0000 /
 *     Ship_ShowView @ 1070:0327 / Ship_CloseView @ 1070:02b5 tail;
 *     KOREKTA 2026-10-16: wczesniej blednie 405, verified DISASM
 *     1048:0374 = ID 506; 405 zostaje census)
 *   50x handlers call dialog_res_list_spec @ 1068:0295 (see dialog.h)
 * Callers of the dispatcher: Main_DispatchQuit @ 1098:062f (queued
 *   DAT_1420_54c6/54c8 + g_nQuitFlag @ 12d8:24ee -> WinMain_Shutdown @
 *   1008:0329), Main_PumpMessage @ 1098:02d1 (accel path), and
 *   Pic_PumpMenuCommand @ 1030:9aed (WM_COMMAND 0x111 ->
 *   Menu_DispatchCommand(DAT_1420_54ea,0); maps DAT_12d8_0030 1/2/3 ->
 *   0x96/0x97/0x98 pic modes).
 * Zwiazek z reszta menu: to NIE jest MainMenu_WndProc @ 11e0:0221
 *   (choice 1/3/4/5 = startowy dialog trybu nad CIV_MAINMENU_TEXT, patrz
 *   mainmenu_run; modal Dialog_MenuZ_Modal @ 1220:0156) ani
 *   Menu_InitMainBar (STUB w boot.c: pasek menu glownego, wolany tez z
 *   obu toggli 1210/1218 powyzej). Dispatch to komendy paska menu w grze
 *   (WM_COMMAND wParam), skroty przechodza przez TRANSLATEACCELERATOR w
 *   Main_PumpMessage/Main_DispatchQuit przed trafieniem tutaj.
 * Main_DispatchQuitEntry @ 1098:061a to czysta far-trampolina do
 *   Main_DispatchQuit @ 1098:062f (MOV AX,DS prolog + PUSH CS near CALL
 *   062f; zero warunkow/hookow). Quit ma wylacznie near-xref z Entry;
 *   Entry wola far (CALLF): Main_PumpQuitLoop @ 1008:0f77 (kreci Entry
 *   wokol flagi DAT_1420_1d4c), Sys_PumpMessages @ 1108:0000 (modalny
 *   pump: DispatchQuitEntry + mapowanie coords + sys_drain_input @
 *   11f0:4ea0) i Pic_PlayShared @ 1030:0000. Nikt nie wola Quit wprost.
 * Port: tabela LIVE 1:1 (read-only, dostep przez funkcje ponizej);
 *   akcje handlerow STUB (menu_dispatch_command -> UNSUPPORTED przy
 *   trafieniu, NOENT przy pudle jak orig 0). No globals, no exit(). */

/* Orig row: ID (CS:0x3d8) + handler offset w segmencie 1048 (CS:0x460). */
typedef struct menu_dispatch_entry {
    int id; /* WM_COMMAND id */
    int target_off; /* handler offset @ seg 1048 */
} menu_dispatch_entry_t;

/* --- Chain 2026-10-16 (P1): MenuZ modal procedural widgets (goal P1) ---
 * Orig Dialog_MenuZ_Modal @ 1220:0156 + Dlg_CreateTracked @ 1098:124f host
 * (civ.exe, x86:LE:16:Protected Mode), decompiled read-only vs
 * unpacked/civ.exe: modal creates RanDoMStatic 0x4a6e title +
 * RanDoMRadio 500+i rows (DAT_1418_000a cols, OFFSETRECT per item) +
 * SETTIMER 100 (menuz_timer_interval_ms) + PEEKMESSAGE/ISDIALOGMESSAGE pump
 * to cancel -1, over CIV_MAINMENU_TEXT @ 1420:435b (file offset 0xbb35b:
 * "_Start a New Game_..._Quit", 6 items). Procedural widgets below are LIVE
 * 1:1 on explicit params (no Win16, no globals, read-only): row count +
 * row-y place the 500+i fan-out without any bitmap, so the menu is drivable
 * (mainmenu_run PARTIAL dispatcher) before frames land. Faithful look waits
 * for the frame decode via rsc_get_indexed @ 1190:0a29 (+ payload via
 * rsc_load_to_mem @ 1190:0fda, see rsc_loader.h); menu idle clock uses the
 * rebase ticks below (MainMenu_WndProc @ 11e0:0884 idle 100000 +
 * @ 11e0:090d resync 0x48c, owned here per boot.h chain-28, not in boot).
 * Pump/host tails stay STUB in mainmenu_run with TODO Orig 1220:0156 /
 * 1098:124f. No exit(), C17. */

/* Orig MainMenu_WndProc @ 11e0:0884 menu-idle rebase ->
 * mainmenu_idle_rebase_ticks. 1:1 100000 (Anim_RebaseClock @ 10b0:005b param
 * for the menu idle loop; startup loop#2 uses 1000 via
 * main_startup_rebase_ticks in boot.h, not here). No globals. */
int mainmenu_idle_rebase_ticks(void);

/* Orig MainMenu_WndProc @ 11e0:090d menu resync rebase ->
 * mainmenu_resync_rebase_ticks. 1:1 0x48c (1164, re-sync of the menu loop
 * when 0x8c < tick < 0x48c; see anim_rebase_clock in dialog.h). No globals. */
int mainmenu_resync_rebase_ticks(void);

/* Orig Dialog_MenuZ_Modal @ 1220:0156 row fan-out (DAT_1418_000a cols,
 * OFFSETRECT per item) -> mainmenu_procedural_rows. 1:1 row count for
 * n_items radio buttons in cols columns: (n + cols - 1) / cols (C trunc).
 * cols <= 0 or n_items < 0 -> -1; n_items == 0 -> 0. No globals. */
int mainmenu_procedural_rows(int n_items, int cols);

/* Orig Dialog_MenuZ_Modal @ 1220:0156 OFFSETRECT per item ->
 * mainmenu_widget_row_y. 1:1 procedural row origin: top + index * row_h
 * (row_h <= 0 -> top, keeps the title row visible when the metric is not
 * yet measured). index < 0 -> -1 (OOR, caller re-prompts like
 * dialog_menuz_id_to_index). No globals. */
int mainmenu_widget_row_y(int top, int row_h, int index);

/* Orig frame source gate (faithful bitmaps via rsc_get_indexed @ 1190:0a29)
 * -> mainmenu_needs_rsc_frame. 1:1 1 (procedural boxes are drawn until the
 * RSC frame decode lands; never 0 while the faithful look is pending).
 * No globals. */
int mainmenu_needs_rsc_frame(void);

/* Orig Dlg_CreateTracked @ 1098:124f palette tail (0xec entries via
 * Palette_CopyRange @ 1098:201d, exercised through anim_pal_copy @
 * 10b0:0461 case 2; host entry Dlg_CreateTracked(0x94,0x41) in
 * Main_RunStartupSequence @ 11e0:0000, ref unpacked/civ.exe strings
 * "RanDoMRadio" @ 0xb7ac6 / "RanDoMStatic" @ 0xb7ad2) ->
 * mainmenu_host_palette_ok. 1:1 1 when the tracked palette count is 0xec
 * (dlg_tracked_pal_count) and a 2-entry anim_pal_copy self-test round-trips
 * (proves the u32-entry copy shape without touching video/GDI); 0
 * otherwise. OS-free, no Win16 (CREATEDIALOG + MAKEPROCINSTANCE +
 * GETDC/SELECTPALETTE/REALIZEPALETTE tails stay STUB in dlg_create_tracked
 * @ 1098:124f with TODO Orig there), no globals, no exit(), read-only
 * except stack locals. */
int mainmenu_host_palette_ok(void);

/* --- Chain 2026-10-16 (P1): MenuZ modal SDL bridge (goal P1) ---
 * Orig Dialog_MenuZ_Modal @ 1220:0156 modal pump (civ.exe, x86:LE:16:
 * Protected Mode; ref unpacked/civ.exe): SETTIMER(hwnd,2,0x64,NULL) @
 * 1220:0202 (100 ms tick via dialog_menuz_timer_ms / menuz_timer_interval_ms;
 * 0 = 'Can''t initialize timer in MenuZ' @ 1420:4a41, file offset 0xbba41,
 * windows.c) + PEEKMESSAGE/ISDIALOGMESSAGE drain to cancel -1
 * (DAT_12b0_00a0 / DAT_1418_0004 via dialog_menuz_cancel_code /
 * dialog_menuz_is_cancel) over CIV_MAINMENU_TEXT @ 1420:435b (file offset
 * 0xbb35b, 6 items, RanDoMRadio 500+i fan-out). SDL mapping (no Win16, no
 * globals, read-only except the explicit break out): SDL_PollEvent drain
 * replaces PEEKMESSAGE, dismiss events set the break image (loop re-prompt
 * via mainmenu_should_loop / dialog_menuz_should_loop), 100 ms pacing LIVE
 * caller-side via mainmenu_tick_is_due below (menuz_timer_interval_ms +
 * Sys_GetTickScaled @ 1100:02e1, ref unpacked/civ.exe). Helpers below
 * are LIVE 1:1 on explicit params; the device tails (CREATEDIALOG +
 * MAKEPROCINSTANCE(DLGPROC @ 1098:16e2 / CIVDIALOGPROC @ 1098:1838) +
 * GETDC/SELECTPALETTE/REALIZEPALETTE + 0xec palette via Palette_CopyRange @
 * 1098:201d + tracked-stack storage @ 1420:1ea0/1ec8 + RSC decode via
 * rsc_get_indexed @ 1190:0a29) stay STUB in dlg_create_tracked @ 1098:124f
 * (see dialog.h) and in mainmenu_run above with TODO Orig 1220:0156 /
 * 1098:124f, never fake CIV_OK. No exit(), C17. */

/* Orig same dismiss shape (button/ESC/close vs motion/window noise) ->
 * mainmenu_event_is_dismiss. 1:1 1 for the SDL dismiss family
 * (SDL_EVENT_QUIT close + SDL_EVENT_KEY_DOWN key incl. ESC +
 * SDL_EVENT_MOUSE_BUTTON_DOWN click = MenuZ button/close), 0 for
 * everything else (motion/window/user-noise = keep pumping, like
 * ISDIALOGMESSAGE-non-dialog msgs). Takes the SDL event type explicitly
 * (Uint32 image, no SDL init needed for the predicate). No globals. */
int mainmenu_event_is_dismiss(uint32_t sdl_type);

/* Orig same loop-done shape (dismiss sets DAT_12b0_00a0 / DAT_1418_0004,
 * caller re-prompts while mainmenu_should_loop == 1) ->
 * mainmenu_modal_is_done. 1:1 1 when break_flag != 0 (dismiss latched),
 * 0 while busy (no dismiss = keep pumping; MenuZ has no Anim advance
 * twin, unlike main_startup_modal_is_done @ 11e0:0000). No globals. */
int mainmenu_modal_is_done(int break_flag);

/* Orig same modal pump (PEEKMESSAGE/ISDIALOGMESSAGE drain per 100 ms tick)
 * -> mainmenu_pump_events. SDL3 drain bridge LIVE (no Win16, no globals,
 * read-only except the explicit out): SDL_PollEvent loop, any dismiss
 * event (see mainmenu_event_is_dismiss) sets *break_flag = 1 (sticky:
 * already-1 stays 1; DAT_12b0_00a0 / DAT_1418_0004 image). Noise events
 * are dropped like ISDIALOGMESSAGE-non-dialog msgs. NULL break_flag ->
 * NOENT (untouched); else CIV_OK after draining (0 events is still OK,
 * caller keeps pumping while mainmenu_modal_is_done == 0 and paces ticks
 * caller-side via mainmenu_tick_is_due below). Safe before
 * SDL_Init (SDL_PollEvent reports none). TODO: Orig Dialog_MenuZ_Modal @
 * 1220:0156 (CREATEDIALOG + MAKEPROCINSTANCE + GETDC/SELECTPALETTE/
 * REALIZEPALETTE + 0xec palette + tracked-stack tails live in
 * dlg_create_tracked @ 1098:124f, STUB there; never fake CIV_OK). */
civ_error_t mainmenu_pump_events(int *break_flag);

/* Orig Dialog_MenuZ_Modal @ 1220:0156 SETTIMER(hwnd,2,0x64,NULL) tick (@
 * 1220:0202, id -> [BP-0x46], KILLTIMER @ 1220:0cfd; fail = 'Can''t
 * initialize timer in MenuZ' @ 1420:4a41, ref unpacked/civ.exe file offset
 * 0xbba41, windows.c; menu text @ 1420:435b, file offset 0xbb35b) paced
 * caller-side via Sys_GetTickScaled @ 1100:02e1 (GETTICKCOUNT * 60 / 1000,
 * see sys_tick_scaled_from in sys.h) -> mainmenu_tick_is_due. 1:1 due when
 * (now_scaled - last_scaled) >= sys_tick_scaled_from(100 ms) (100 ms =
 * 6 scaled ticks; unsigned subtraction, so a 32-bit wrap still measures
 * the elapsed span). Pure predicate on explicit scaled ticks (no SDL, no
 * Win16, no globals, no exit(), read-only); the caller drains via
 * mainmenu_pump_events above once per due tick and re-prompts via
 * mainmenu_should_loop / dialog_menuz_should_loop. */
int mainmenu_tick_is_due(unsigned long last_scaled, unsigned long now_scaled);

/* --- Chain 2026-10-17 (P1): MenuZ timer + tail gates (goal P1
 * Dialog_MenuZ_Modal @ 1220:0156) ---
 * Orig Dialog_MenuZ_Modal @ 1220:0156 modal tick/tail (civ.exe,
 * x86:LE:16:Protected Mode; ref unpacked/civ.exe read-only NE Win3.x
 * 832512 B): SETTIMER(hwnd,2,0x64,NULL) @ 1220:0202 (100 ms via
 * menuz_timer_interval_ms / dialog_menuz_timer_ms; fail = 'Can''t
 * initialize timer in MenuZ' @ 1420:4a41, file offset 0xbba41, windows.c;
 * KILLTIMER @ 1220:0cfd on exit) paced caller-side via Sys_GetTickScaled
 * @ 1100:02e1 (GETTICKCOUNT * 60 / 1000, see sys_tick_scaled_from in
 * sys.h; 100 ms = 6 scaled ticks) + fala-23 tail (DAT_1418_0006 art
 * branch via dialog_menuz_art_mode: -1 = plain, 0..2 = small pic,
 * >= 3 = tech badge; DAT_1418_0008/DAT_12b0_00a0 exit via
 * dialog_menuz_is_cancel: flag_0008 >= 1 && done_00a0 == -1 = cancel/back)
 * over CIV_MAINMENU_TEXT @ 1420:435b (file offset 0xbb35b, 6 items,
 * RanDoMRadio 500+i fan-out). OS-free 1:1 on explicit params (no Win16,
 * no globals, read-only); device tails (CREATEDIALOG + MAKEPROCINSTANCE +
 * GETDC/SELECTPALETTE/REALIZEPALETTE + 0xec palette + tracked-stack +
 * RSC decode via rsc_get_indexed @ 1190:0a29) stay STUB in mainmenu_run
 * above with TODO Orig 1220:0156 / 1098:124f, never fake CIV_OK. No
 * exit(), C17. */

/* Orig same SETTIMER 100 ms tick -> mainmenu_menuz_timer_ok. 1:1 1 when
 * menuz_timer_interval_ms() == 100 && dialog_menuz_timer_ms() == 100 &&
 * sys_tick_scaled_from(100) == 6; 0 otherwise. Proves the modal pacing
 * before the PEEKMESSAGE pump bridge. No globals. */
int mainmenu_menuz_timer_ok(void);

/* Orig same fala-23 tail -> mainmenu_menuz_tail_ok. 1:1 1 when
 * dialog_menuz_art_mode(-1) == 0 && dialog_menuz_art_mode(0) == 1 &&
 * dialog_menuz_art_mode(3) == 2 && dialog_menuz_is_cancel(0,-1) == 0 &&
 * dialog_menuz_is_cancel(1,-1) == 1 && dialog_menuz_is_cancel(1,0) == 0;
 * 0 otherwise. Same triple as the inline probe in mainmenu_run (kept as
 * the caller-side audit; this helper is the reusable gate). No globals. */
int mainmenu_menuz_tail_ok(void);

/* Orig CX = 0x44: liczba wpisow (68). */
int menu_dispatch_count(void);
/* Orig scan 1:1: indeks wpisu o danym id, -1 przy pudle. */
int menu_dispatch_find(int cmd);
/* Orig wiersz tabeli: id / target danego indeksu, -1 przy OOR. */
int menu_dispatch_id_at(int idx);
int menu_dispatch_target_at(int idx);
/* Setki ID (1..8 dla 100..899, 0 wpp). Nazwy podmenu TBD. */
int menu_command_group(int cmd);
/* Orig Menu_DispatchCommand @ 1048:01cd -> dyspozytor (lookup LIVE,
 * akcje STUB; skaner STATUS.md: STUB): trafienie -> CIV_ERR_UNSUPPORTED,
 * pudlo (orig 0) -> CIV_ERR_NOENT. */
civ_error_t menu_dispatch_command(int cmd);

/* --- Batch 2026-10-17 fala 4 (T): help contents ---
 *   Menu_ShowHelpContents @ 1048:04e8. */

/* Orig Menu_ShowHelpContents @ 1048:04e8 (if g_nCurrentDrive @ 1228:0002
 * > 0 && Dos_DriveReady @ 1000:1184 == 0: Dos_SetPath @ 1000:116a; then
 * WINHELP @ 1608:016c on hwnd @ 12b0:0004 with helpfile DS:0x9c8,
 * HELP_CONTENTS = 3; one case of Menu_DispatchCommand @ 1048:01cd
 * jumptable). DOS + WinHelp, no SDL help twin -> STUB UNSUPPORTED. */
civ_error_t menu_show_help_contents(void);

/* --- Batch 2026-10-17 fala 5 (U): menu dispatch ---
 *   Menu_DispatchOrEnableRange @ 1098:0bdd. */

/* Orig Menu_DispatchOrEnableRange @ 1098:0bdd (hMenu = GETMENU @ 1608:0164
 * on DAT_12b0_0004; linear scan 8-entry CS table @ 1098:0c97 ids
 * 101/201/301/401/501/601/701/801 for cmd; hit -> indirect handler at
 * table+0x10 (targets 0c1d..0c55, proven 0c25: 201-208); miss/after ->
 * ENABLEMENUITEM @ 1608:0160 range loop; 19 callers Pic_PlayShared x18 +
 * WdwMap_Open). win16 menu facade + full (SI,DI) dump TBD -> STUB. */
civ_error_t menu_dispatch_or_enable_range(int cmd_id, int enable);

/* --- Batch 2026-10-17 fala 6 (Z): palace improve modal ---
 *   Menu_PalaceImprove @ 1158:0000, Menu_PalaceRebuild @ 1158:08e7,
 *   Menu_PalaceClose @ 1158:123b, Menu_PalaceCalcIcon @ 1158:1277,
 *   Menu_PalaceDrawIcon @ 1158:140c. */

/* Orig Menu_PalaceImprove @ 1158:0000 (~1817 B modal: PALACE header +
 * PalaceRebuild(0) + leader wav + section list + PresentRect + key/mouse
 * input loop + validity gate + CalcIcon/CacheAlloc + "Select new section."
 * + commit + close + wait; sole caller Game_TurnLoopTick @ 1088:087f).
 * Dialog/GfxPort/input/audio + 1350/1420/12d8 ctx TBD -> STUB. */
civ_error_t menu_palace_improve(void);

/* Orig Menu_PalaceRebuild @ 1158:08e7 (palace port rebuild: rect save +
 * CalcIcon per section + CacheAlloc(mask,1) + CreateTracked(0x94) +
 * Art frames Allows(+1) + per-section DrawIcon + wonder overlays +
 * SHOWWINDOW + PresentRect; callers 1158:0000 x2 + FUN_11d8_0000
 * spaceship path). Art 1198 RLE/tagged-frame decoder NOT guessed ->
 * STUB. */
civ_error_t menu_palace_rebuild(int variant);

/* Orig Menu_PalaceClose @ 1158:123b (GfxPort_Close 0x56 + DestroyTracked
 * b12a + *(2990+0x10) = 0; callers 0000 x3 + 11d8:0000). SDL port
 * teardown + dialog pop TBD -> STUB. */
civ_error_t menu_palace_close(void);

/* Orig Menu_PalaceCalcIcon @ 1158:1277 (pure icon-index kernel on
 * ES:1350: lvl = [sec*2+0xdc]; 0..3 kind switch with -1 sentinel + sec
 * 1/5/7 edges; 0094/0095 outs; callers 0000 + 08e7). LIVE pure on
 * explicit params (hidden ES -> outs). */
void menu_palace_calc_icon(int sec, int preview, int lvl, int da, int de,
                           int d8, unsigned char *out94,
                           unsigned char *out95);

/* Orig Menu_PalaceDrawIcon @ 1158:140c (coord/icon-select wrapper over
 * FUN_11a8_15ce blitter: px = (x<<4)/10, py scaled, icon from 0094/0095
 * indices; 10 callers in 08e7). Math LIVE, blit tail STUB. */
civ_error_t menu_palace_draw_icon(int lvl, int sec, int x, int y, int flag);
int menu_palace_icon_px(int x);
int menu_palace_icon_py(int y, int lvl, int extra);

#endif /* CIVLIN_MAINMENU_H */
