/* Game state + main loop.
 *
 * Ghidra anchors (civ.exe), renamed 2026-10-15:
 *   WinMain entry @ 1000:0000 -> WinMain @ 1008:0000 (see boot.h)
 *   Game_TurnLoopTick @ 1088:0000 (body of while(!g_nQuitFlag) in WinMain)
 *   NewGame_Wizard @ 10e8:0000 fills the start-of-game fields below
 *   Unit_ShowInfoPopup @ 11f0:468c (called when turn==0 && difficulty==0)
 *   exports: WDWMAPPROC @ 1208:0054, WDWSMMAPPROC @ 1210:0000,
 *            WDWSTATUSPROC @ 1218:0000, TIMERPROC @ 1008:0d68 (see map.h)
 *
 * Live globals (segment 0x12d8 unless noted), renamed in Ghidra:
 *   g_nQuitFlag   @ 12d8:24ee  0 = run, 1 = quit/end, -1 = defeat, 2 = ?(menu)
 *   g_nTurn       @ 12d8:42fe  turn counter (WinMain branches on ==0 at start)
 *   g_nDifficulty @ 12d8:42f6  0=Chieftain..4=Emperor (see newgame.h)
 *   g_nPlayerCiv  @ 12d8:4300  active civilization index
 *   g_nCivAliveMask@12d8:42f8  bitmask of live civs
 *   g_nYear       @ 12d8:42fa  year; 0xF060 at new-game finalize
 *   g_nMenuChoice @ 12d8:44e2  Dialog_MenuZ_Modal result
 *   g_nCivCount   @ 12d8:3b9a  2..6 (default 6)
 *   g_nOptionFlags@ 12d0:0064  bit 0x10 = sounds on (see audio.h)
 */
#ifndef CIVLIN_GAME_H
#define CIVLIN_GAME_H

#include "civ_common.h"

typedef struct civ_game {
    int initialized;
    int quit_flag;   /* g_nQuitFlag */
    int turn;        /* g_nTurn */
    int difficulty;  /* g_nDifficulty */
    int player_civ;  /* g_nPlayerCiv */
    int alive_mask;  /* g_nCivAliveMask */
    int year;        /* g_nYear */
    int menu_choice; /* g_nMenuChoice */
    int civ_count;   /* g_nCivCount */
} civ_game_t;

civ_error_t game_init(civ_game_t *g);
civ_error_t game_new(civ_game_t *g);
civ_error_t game_tick(civ_game_t *g);

/* One Game_TurnLoopTick @ 1088:0000 pass: iterate civs, bump g_nTurn/g_nYear,
 * scoring every 0x32 turns, endings via HoF path (sets quit_flag). */
civ_error_t game_turn_tick(civ_game_t *g);

void game_shutdown(civ_game_t *g);

#endif /* CIVLIN_GAME_H */
