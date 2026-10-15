/* Game state + main loop.
 *
 * Ghidra anchors (civ.exe), renamed 2026-10-15 (Orig FUN_* -> New):
 *   WinMain entry @ 1000:0000 -> WinMain @ 1008:0000 (see boot.h)
 *   Game_TurnLoopTick @ 1088:0000 (body of while(!g_nQuitFlag) in WinMain)
 *   NewGame_Wizard @ 10e8:0000 fills the start-of-game fields below
 *   Unit_ShowInfoPopup @ 11f0:468c (called when turn==0 && difficulty==0)
 *   Game_ProcessGameEnd @ 1088:0b6d (HoF path: 1188:07ad + 1180:0000,
 *     1040:1137 score, HoF_VictoryCeremony/Load/TryInsert/Save)
 *   Game_SpawnUprisings @ 1088:0be9 (barbarian/guerrilla/native spawns)
 *   Game_UpdatePowerRankings @ 1088:108a (civ power scores + rank table)
 *   Game_UpdateStatusPanel @ 1088:13d5 (status bar redraw; also called from
 *     Map_RefreshAllViews, Civ_ProcessUnitMoves, WdwStatus_Refresh)
 *   Game_DoPollutionAndWonderRace @ 1088:1e32 (pollution + AI wonder race)
 *   Game_CalcProgressThreshold @ 1088:1d99 (pure: sum i*(7-diff)/2+1)
 *   Civ_ProcessUnitMoves @ 11f0:0000 (per-civ unit turn; 1 = needs UI pump)
 *   Sys_CheckRuntimeMemory @ 11e8:0075 (per-civ memory warning; see sys.h)
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
 *
 * Unit records (segment 1420): 0x80 units/civ, 0xc bytes each at
 *   1420:62fc + civ*0x600 + idx*0xc. Working field labels (offsets
 *   relative to record base):
 *     +0 x (62fd), +1 y (62fe), +2 type (62ff; 0xff = empty;
 *       0x19 diplomat, 0x1a settler, 0x0e caravan?, 0x0f ?),
 *     +3 moves/counter (6300), +4 build timer (6301),
 *     +5 goal-x (6302; 0xff = none), +6 goal-y (6303),
 *     +7 last-dir (6304; 0xff init), +8 seen-by mask (6305),
 *     +9 next-in-stack (6306), +10 home-city (6307), +11 flags
 *       (62fc bits 0x08/0x09 busy, 0x04 ?, 0xc2 visibility, 0x20 veteran).
 * City records: 0x1c bytes at 1420:5500 + idx*0x1c (+0x5500 x,
 *   +0x5501 y, +0x5502 valid, +0x5503 size/def, +0x5507 owner).
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

/* Turn phases (stubs; game state TBD). Param is the HoF slot on game end. */
civ_error_t game_process_game_end(int hof_slot);
/* Per-turn barbarian/guerrilla/native spawns. */
civ_error_t game_spawn_uprisings(void);
/* Per-turn civ power scores + rank table. */
civ_error_t game_update_power_rankings(void);
/* Status bar redraw (Game_UpdateStatusPanel @ 1088:13d5). */
civ_error_t game_update_status_panel(void);
/* Pollution spread + AI wonder race. */
civ_error_t game_do_pollution_and_wonder_race(void);
/* Per-civ unit turn (Civ_ProcessUnitMoves @ 11f0:0000, 0x500a bytes).
 * Returns 1 when the UI must pump (orig early-returns the tick),
 * 0 when the civ is done. Stub returns 0. */
int civ_process_unit_moves(int civ);

/* --- Game_UpdateStatusPanel callees (frontier 2026-10-15) ---
 * Parent: Game_UpdateStatusPanel @ 1088:13d5 (8 callers incl.
 *   Game_TurnLoopTick @ 1088:0162/0887, Civ_ProcessUnitMoves @ 11f0:0bbb).
 * Rect helpers @ 1218:0956/0a06 live in map.h (WDWSTATUSPROC segment). */

/* Orig: FUN_1088_1a82 @ 1088:1a82 -> status_draw_tech_icons.
 * Draws the separator rule (CREATEPEN 0x8000/1, MOVETO/LINETO at
 * x+0x1b, y-2..y+0x82) then 9 tech-progress icons via FUN_11a8_0cb1
 * indexed from DAT_1420:9632 (Util_Clamp(row,0,3) x col 0..4 x
 * owner 0..2). Player civ copies live rows (0xdc/0xc4); other civs fake
 * them with an LCG (seed DAT_13c0_0000*civ, *5+1, %9) under adjacency
 * (no isolated stacks), budget (count<=clamp(param2+3,3,0x24),
 * height<=count/5, <=3) and 999-try cap. Gated on *(civ*2+0x3efe)!=0.
 * Params match the decompiler: (civ, progress, x, y). */
civ_error_t status_draw_tech_icons(int civ, int progress, int x, int y);

/* Orig: FUN_1088_1dd0 @ 1088:1dd0 -> game_append_year_text.
 * Appends abs(g_nYear) via FUN_1000_2928(...,10) plus 1420:173c ("BC"
 * when year<0) / 1420:1740 ("AD") to the 12d8:24fa line buffer.
 * 5 callers (status panel, 1018:015d, 1138:0000, 11b0:0000, 11b8:14e5). */
civ_error_t game_append_year_text(void);

/* Orig: FUN_11a8_0cb1 @ 11a8:0cb1 -> Gfx_DrawTechIcon.
 * 9-icon blitter called from status_draw_tech_icons @ 1088:1a82
 * (src/game.h:92 frontier). Params: index 0..8 (tech-progress slot),
 * x/y pixel origin in the status port, palette = GDI palette/select
 * object for the blit (exact SELECTPALETTE arg order TBD, see Ghidra).
 * Win16 GDI blit (SELECTPALETTE + BITBLT) -> SDL texture blit TBD. */
civ_error_t gfx_draw_tech_icon(int index, int x, int y, int palette);

/* Orig: FUN_1098_0dda @ 1098:0dda -> status_font_height.
 * GETTEXTMETRICS wrapper returning tmHeight (local_24[0]); the status
 * panel keeps it as line stride (local_8, x2 for the civ block).
 * 4 callers: status panel @ 1088:1460, 1138:0269, Unit_ShowInfoPopup
 * @ 11f0:4754, Dialog_MenuZ_Modal @ 1220:0b88. */
int status_font_height(void);

/* --- Civ_ProcessUnitMoves callees (frontier 2026-10-15) ---
 * Parent sole caller: Game_TurnLoopTick @ 1088:01c6. Unit records: see
 * header top. Order codes
 * below are opaque dispatch keys for the three jump tables inside
 * Civ_ProcessUnitMoves (1420:43b8+0x8f x0x0e, +0x4b x0x11, 1420:435b+0x28
 * x0x20); observed values: 0x00 idle/none, 0x20 force-sleep/reset,
 * 0x50 settler-site fail, 0x62 found-city/irrigate?, 0x66 wait-fortify?,
 * 0x68 sentry/1-move?, 0x69/0x6d/0x6f/0x70/0x72/0x75 move/attack variants,
 * 0x6d road?, 0x70 pollution?, 0xffff no-path. */

/* Orig: FUN_1078_1480 @ 1078:1480 -> unit_choose_order.
 * AI order brain (0x300e bytes). Special-cases: civ==0 delegates to
 * FUN_1078_4f0f; type 0x0f with timer<1 -> 0x68; type 0x0e with no goal
 * picks nearest visible player diplomat (11c8:02ec) or staging city
 * (1420:5500) else 0x20. Type 0x19 scans 0x80 cities for the best
 * bribe target (power/tech/diplo math) else 0x20. Type 0x1a with no
 * goal founds via 11c0:16a4 when home. Then terrain/tactical scoring:
 * hut-hunt (Map_GetTile+0x50), difficulty snipe, city-defense 0x66,
 * stack/settler logic (local_3a 0/2/3/5), combat odds via
 * Combat_ResolveAttack + 11c0:1bc3, explorer random walk. Returns an
 * order code (see list above). Sole caller Civ_ProcessUnitMoves. */
int unit_choose_order(int civ, int unit_idx);

/* Orig: FUN_1128_0000 @ 1128:0000 -> unit_step_toward_goal.
 * Single-step pather toward 1338:0002/0000 (or 6302/6303 when
 * 0x1a3a==1). Player-adjacent (|dx|<2,|dy|<2) resolves directly via
 * 1110:1c4b. Otherwise tries FUN_1128_10de(...,999) corridor and
 * FUN_1128_081c passability, then scores all 8 neighbours by
 * Chebyshev-remaining + terrain cost (1420:17d4*3 or 3, river 1,
 * anti-reverse 0x6304 penalty) gated on sea/land match (0x1a3a vs
 * 1110:16c0==10), ZOC (11c0:23fd), occupancy (1110:18ce/16ee/199b)
 * and 1110:1dbd>4 for ocean. Returns dir 1..8, 0 (arrived/exhausted),
 * or 0xffff (no path, clears 6302/6304). Callers: Civ_ProcessUnitMoves
 * @ 11f0:0ca0, FUN_1078_4f0f @ 1078:55d3. */
int unit_step_toward_goal(int civ, int unit_idx);

/* Orig: FUN_11c0_0395 @ 11c0:0395 (x, y, civ, unit, redraw)
 * -> unit_update_visibility. Sets seen-bits (x*0x32+y+10) for the
 * 0x19-tile diamond (player) / 8 near + 0x10 far (others, 0x1a3a/0x1a46
 * gated), spots cities (1110:180e/16ee -> 11c0:0000/11c8:00c4), flips
 * unit goals on contact (11c0:1ee1), posts diplomacy (1168:0a07,
 * 1078:455c) and paints the minimap layer (Map_SetTile x+0xf0, 1/2).
 * Callers: Civ_ProcessUnitMoves @ 11f0:0259/3688, FUN_11c0_12c8. */
civ_error_t unit_update_visibility(int x, int y, int civ, int unit_idx,
                                   int redraw);

/* Orig: FUN_11c0_0ec2 @ 11c0:0ec2 (civ, unit_or_0x80+) -> unit_wait_for_orders.
 * Modal blink-and-pump: alternates the unit sprite (1110:0448/0dba +
 * 1210:07ae) vs "End of Turn/Press Return" (10d8:00e0 + 10d0:0854) on a
 * Sys_GetTickScaled%0x3c 0x1e-phase clock, pumping FUN_1108_0000 until
 * 11e8:02fb input or 1328:0012 mode change. Sole caller
 * Civ_ProcessUnitMoves @ 11f0:0910. */
civ_error_t unit_wait_for_orders(int civ, int unit_idx);

/* Orig: FUN_11f0_44d4 @ 11f0:44d4 (civ, unit) -> unit_maybe_center_view.
 * Calls 1110:1461 (reveal/center helper) only when the unit belongs to
 * the player or is seen by the player (6305 bit). Caller:
 * Civ_ProcessUnitMoves @ 11f0:030c. */
civ_error_t unit_maybe_center_view(int civ, int unit_idx);

/* Orig: FUN_1078_4f0f @ 1078:4f0f -> Unit_AIPlayerTurn.
 * civ==0 delegate inside unit_choose_order @ 1078:1480; caller of
 * unit_step_toward_goal @ 1128:0000 (@ 1078:55d3). Game logic: picks the
 * human-civ unit order. Returns an order code from the dispatch list in
 * this header (0x00 idle/none, 0x20 force-sleep/reset, 0x62/0x66/0x68
 * city/fortify/sentry, 0x69..0x75 move/attack variants, 0xffff no-path).
 * Stub returns 0x20 (idle) until the order tables are decoded. */
int unit_ai_player_turn(int civ, int unit_idx);

/* Orig: FUN_11c0_12c8 @ 11c0:12c8 -> Unit_RevealArea.
 * Area-reveal wrapper: caller of unit_update_visibility @ 11c0:0395
 * (single-tile seen-bits/diplomacy/minimap update). Takes the map
 * position plus owning civ; fans out to unit_update_visibility per
 * tile/unit in the affected area (exact radius TBD, see Ghidra). */
civ_error_t unit_reveal_area(int x, int y, int civ);

/* Orig: FUN_11f0_4ea0 @ 11f0:4ea0 -> sys_drain_input.
 * Clears 1420:1d44..1d50 + 12b0:004c and drains PEEKMESSAGE queues
 * (0x108/0x100, 0x209/0x200). Called from Civ_ProcessUnitMoves
 * (@ 11f0:06d2/0c58), menu/button/turn paths (28 callers) and the
 * 11c0:0395 visibility tail. */
civ_error_t sys_drain_input(void);

/* Game_CalcProgressThreshold @ 1088:1d99 (pure 1:1): the sum over
 * i = 0..n of i * (7 - difficulty), halved, plus 1. */
unsigned game_calc_progress_threshold(int n, int difficulty);

void game_shutdown(civ_game_t *g);

#endif /* CIVLIN_GAME_H */
