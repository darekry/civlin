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

/* Dispatch values as observed in MainMenu_WndProc (g_nMenuChoice). */
typedef enum civ_mainmenu_choice {
    CIV_MENU_NONE = -1,
    CIV_MENU_LOAD = 1,   /* -> save_prompt_open */
    CIV_MENU_EARTH = 3,  /* -> FUN_1108_0243 earth map */
    CIV_MENU_HALL = 4,   /* -> HoF_Load + HoF_Show */
    CIV_MENU_QUIT = 5    /* -> WinMain_Shutdown */
    /* 0/2 fall through to NewGame_Wizard paths; exact mapping TBD */
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
 * raw choice. Stub: NOENT on NULL out, else UNSUPPORTED with *out=NONE;
 * read-only (no modal pumped until SDL3 bridge lands). Raw choice is the
 * zero-based MenuZ index; 500-based button ids map via dialog_menuz_*
 * (CIV_DIALOG_MENUZ_BASE_ID). */
civ_error_t mainmenu_run(civ_mainmenu_choice_t *out);

/* Dispatches one choice to load/earth/hall/quit/new-game.
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

#endif /* CIVLIN_MAINMENU_H */
