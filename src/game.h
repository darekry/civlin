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
 *   Game_PickWonderForAI @ 1170:3181 (tech-table wonder pick, callee of
 *     Game_DoPollutionAndWonderRace per game.c pollution tail)
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

/* Orig WinMain @ 1008:0000 state init (DGROUP 1420 + segment 12d8 image) ->
 * game_init. OS-free 1:1 (live): zeroes the caller-owned civ_game_t
 * (initialized/quit_flag/turn/difficulty/player_civ/alive_mask/year/
 * menu_choice/civ_count; g_nQuitFlag @ 12d8:24ee = run, g_nTurn @
 * 12d8:42fe, g_nDifficulty @ 12d8:42f6, g_nPlayerCiv @ 12d8:4300,
 * g_nCivAliveMask @ 12d8:42f8, g_nYear @ 12d8:42fa, g_nMenuChoice @
 * 12d8:44e2, g_nCivCount @ 12d8:3b9a). Returns CIV_OK; NULL -> NOENT.
 * No globals, no exit(), read-only. */
civ_error_t game_init(civ_game_t *g);
/* Orig NewGame_Wizard @ 10e8:0000 + Map_GenerateRandomOrLoad @ 1028:0000
 * tail -> game_new. Fills start-of-game fields (see game_init list) via
 * newgame_wizard_run (see newgame.h) -> worldgen. Win16 dialog/map tails
 * TBD -> stub CIV_ERR_UNSUPPORTED; NULL -> NOENT. No globals, no exit(). */
civ_error_t game_new(civ_game_t *g);
/* Orig TIMERPROC @ 1008:0d68 body, pumped from the while(!g_nQuitFlag)
 * loop in WinMain @ 1008:0000 (200 ms SETTIMER installed by WdwMap_Open
 * @ 1208:0af1, torn down first by KILLTIMER in WinMain_Shutdown @
 * 1008:0329). Tick -> game_turn_tick @ 1088:0000 linkage: per-frame pump
 * here (Sys_PumpMessages @ 1108:0000 + Sys_Yield @ 1098:0157, see sys.h;
 * both live SDL_PumpEvents) then one Game_TurnLoopTick pass (stub tail).
 * Partial: pump live, turn TBD -> overall CIV_ERR_UNSUPPORTED; NULL ->
 * NOENT, quit_flag set -> CIV_OK (no work). No globals, no exit(). */
civ_error_t game_tick(civ_game_t *g);

/* One Game_TurnLoopTick @ 1088:0000 pass: iterate civs, bump g_nTurn/g_nYear,
 * scoring every 0x32 turns, endings via HoF path (sets quit_flag).
 * Scoring branch (every 0x32 turns) scales via Game_CalcProgressThreshold
 * @ 1088:1d99 (game_calc_progress_threshold below, implemented: sum
 * i*(7-diff)/2+1); stub covers the branch via the TODO in game.c. */
civ_error_t game_turn_tick(civ_game_t *g);

/* --- Game-end / turn batch (frontier 2026-10-15) ---
 * OS-free logic is ported 1:1 with explicit params (no globals, no Win16);
 * Win16-sensitive tails stay CIV_ERR_UNSUPPORTED with TODO Orig refs.
 * Owned-module calls (HoF/worldgen/map/sys/dialog) are documented, not
 * linked, so the parallel map/sys/HoF/worldgen rewrites stay decoupled. */

/* Orig FUN_1088_0b6d @ 1088:0b6d -> Game_ProcessGameEnd.
 * End-of-game flow (stub): unless quit==2, runs the 1188:07ad + 1180:0000
 * teardown pair, computes the final score via 1040:1137, then the HoF path
 * HoF_VictoryCeremony / HoF_Load / HoF_TryInsert / HoF_Save (see
 * halloffame.h, owned by parallel agent). Win16 dialogs + ceremony
 * frames + file I/O -> stub.
 * Param hof_slot is the HoF slot context on game end (exact slot/index
 * encoding TBD, see Ghidra callers in Game_TurnLoopTick). */
civ_error_t game_process_game_end(int hof_slot);

/* Orig FUN_1088_0be9 @ 1088:0be9 -> Game_SpawnUprisings.
 * Per-turn barbarian/guerrilla/native spawns (stub tail).
 * Head (OS-free, see game_uprising_rng_scale below): difficulty-gated
 * Rng_Range @ 11c8:003d draws for candidate tiles. Tile gates via
 * 1110:16c0 / 1110:18ce / 1110:1dbd (land/occupancy/distance checks, exact
 * predicates TBD), spawn reveal via Unit_RevealArea @ 11c0:12c8 (see
 * unit_reveal_area in this header). Tail (Win16 stub): Travellers /
 * Barbarian / Native / Guerrilla message + dialog pump -> UNSUPPORTED. */
civ_error_t game_spawn_uprisings(void);

/* Orig Rng_Range @ 11c8:003d scaling used by Game_SpawnUprisings @
 * 1088:0be9 head -> game_uprising_rng_scale. OS-free 1:1 (implemented):
 * (rand15 * n) >> 15, 0 when n <= 0. Takes the raw rand15 draw
 * explicitly so game.c needs no sys.h/global RNG state; callers pass
 * rng_next_raw() output (or a test vector). */
unsigned game_uprising_rng_scale(unsigned rand15, int n);

/* Power inputs for Orig FUN_1088_108a @ 1088:108a.
 * cities = live city count, territory = land/territory score,
 * treasury = gold stock (divided by 0x20 in the score), tech =
 * pre-weighted tech contribution (sum of tech weights, table TBD). */
typedef struct civ_power_state {
    int cities;
    int territory;
    int treasury;
    int tech;
} civ_power_state_t;

/* Orig FUN_1088_108a @ 1088:108a score kernel -> game_calc_power_score.
 * OS-free 1:1 (implemented): cities*8 + territory + treasury/0x20 + tech.
 * Integer division; callers pass non-negative game values. */
int game_calc_power_score(int cities, int territory, int treasury, int tech);

/* Orig FUN_1088_108a @ 1088:108a -> Game_UpdatePowerRankings.
 * OS-free 1:1 (implemented): scores_out[i] = game_calc_power_score(...)
 * per civ, then rank_out[i] = 0-based rank (0 = strongest; ties break to
 * the lower civ index, stable O(n^2) like the decompiler loops). The
 * 0x3e84 base + 0x1a rank-table region holds these outputs (exact byte
 * stride/packing TBD, see Ghidra); the 12a8:000c rebellion flag update
 * in the same function is Win16/game-state owned and NOT done here.
 * Params: states/scores_out/rank_out are n_civs arrays. Returns CIV_OK,
 * CIV_ERR_NOENT on NULL, CIV_ERR_RANGE on n_civs <= 0. */
civ_error_t game_update_power_rankings(const civ_power_state_t *states,
                                       int n_civs, int *scores_out,
                                       int *rank_out);

/* Status bar redraw (Game_UpdateStatusPanel @ 1088:13d5). */
civ_error_t game_update_status_panel(void);

/* Orig FUN_1088_1e32 @ 1088:1e32 head (12d8:42f0) ->
 * game_accum_pollution. OS-free 1:1 (implemented): pollution-counter
 * accumulation (orig 16-bit word add, wraps; unsigned preserves wrap).
 * Caller adds per-turn production, then (in the full function) calls
 * Map_SpreadPollution @ 1028:1afd (worldgen.h, parallel-owned). */
unsigned game_accum_pollution(unsigned counter, unsigned produced);

/* Orig FUN_1088_1e32 @ 1088:1e32 -> Game_DoPollutionAndWonderRace.
 * Mixed (stub tail): OS-free head accumulates 12d8:42f0 via
 * game_accum_pollution above, then Map_SpreadPollution @ 1028:1afd
 * (worldgen.h, parallel-owned). Wonder-race head picks via
 * Game_PickWonderForAI @ 1170:3181 (game_pick_wonder_for_ai below).
 * Tail (Win16 stub): "... builds ..." message + shield deduction +
 * dialog pump -> UNSUPPORTED. */
civ_error_t game_do_pollution_and_wonder_race(void);

/* Orig FUN_1170_3181 @ 1170:3181 -> Game_PickWonderForAI.
 * OS-free 1:1 (implemented): tech-table wonder pick, callee of
 * Game_DoPollutionAndWonderRace @ 1088:1e32 per game.c:79.
 * Eligible[w] = !wonder_done[w] && (prereq < 0 || (0 <= prereq < n_tech
 * && tech_owned[prereq] != 0)). Out-of-range prereq (>= n_tech) is
 * ineligible (unknown tech). Returns the eligible[roll % eligible_count]
 * wonder index (pass roll=0 for first-fit); -1 when no wonder qualifies
 * or on NULL/bad counts. Exact wonder/tech table ids TBD, see Ghidra;
 * roll is the pre-scaled Rng_Range draw so no sys.h dependency. */
int game_pick_wonder_for_ai(const int *tech_owned, int n_tech,
                            const int *wonder_prereq,
                            const int *wonder_done, int n_wonders,
                            unsigned roll);
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

/* --- packet D (2026-10-15): info popup ---
 * New helper only; existing HoF-end functions above untouched. */

/* Orig: FUN_11f0_468c @ 11f0:468c -> Unit_ShowInfoPopup.
 * Unit/city info popup called when turn==0 && difficulty==0 (new-game
 * tutorial hint path out of Game_TurnLoopTick / Civ_ProcessUnitMoves;
 * see header top), and from WdwStatus_Refresh @ 1218:0801 via
 * Game_UpdateStatusPanel. Measures with status_font_height @ 1098:0dda
 * (@ 11f0:4754) and draws via the Dialog_MenuZ_Modal @ 1220:0156 family.
 * Params: owning civ + unit index (unit record 1420:62fc layout, see
 * header top). Win16 dialog + GDI text -> SDL TBD; stub. */
civ_error_t unit_show_info_popup(int civ, int unit_idx);

/* --- packet O (2026-10-15): combat/unit tactical ---
 * Anchors: unit_choose_order @ 1078:1480 (combat odds via
 *   Combat_ResolveAttack + 11c0:1bc3), unit_step_toward_goal @
 *   1128:0000 (ZOC gate 11c0:23fd in neighbour scoring),
 *   unit_update_visibility @ 11c0:0395 (city spot via 1110:180e/16ee
 *   -> 11c0:0000/11c8:00c4, contact flip 11c0:1ee1). Map_/City_
 *   names below are packet-O kernels owned here per task scope; the
 *   parallel map/sys/save/dialog rewrites must not duplicate them.
 *   OS-free kernels take explicit params (no globals, read-only);
 *   Win16/city-state tails stay CIV_ERR_UNSUPPORTED with TODO Orig. */

/* Orig Combat_ResolveAttack @ TBD (near unit_choose_order @ 1078:1480
 * combat-odds tail with 11c0:1bc3; exact seg:off TBD from decompile)
 * -> Combat_ResolveAttack (combat_resolve_attack). OS-free odds math
 * (attacker/defender strength, terrain, veteran) once decoded; stub
 * for now with formula doc. Expected shape: odds_out (0..100) =
 * f(atk_strength vs def_strength * terrain * veteran); the 11c0:1bc3
 * helper (combat_calc_odds below) does the final atk/(atk+def) scale.
 * Returns CIV_OK + odds, CIV_ERR_NOENT on NULL odds_out, else
 * CIV_ERR_UNSUPPORTED until the multipliers are decoded. */
civ_error_t combat_resolve_attack(int atk_strength, int def_strength,
                                  int terrain_bonus, int vet_bonus,
                                  int *odds_out);

/* Orig FUN_11c0_1bc3 @ 11c0:1bc3 -> Combat_CalcOdds (combat_calc_odds).
 * Tactical scoring helper in the unit_choose_order @ 1078:1480 combat
 * tail (with Combat_ResolveAttack above). OS-free 1:1 v1 (implemented):
 * percent odds from effective strengths, atk*100/(atk+def); exact
 * veteran/terrain pre-scale + rounding TBD vs decompiler. Returns
 * 0..100, -1 on negative strengths or both zero. */
int combat_calc_odds(int atk_eff, int def_eff);

/* Orig FUN_11c0_16a4 @ 11c0:16a4 -> Unit_FoundCityIfHome
 * (unit_found_city_if_home). Type 0x1a settler founds via 11c0:16a4
 * when home (home-city field +10/6307 in the 1420:62fc record layout,
 * see header top). Needs city records 0x1c bytes at 1420:5500
 * (+owner/size) + map tile claim; stub. */
civ_error_t unit_found_city_if_home(int civ, int unit_idx);

/* Orig FUN_11c0_23fd @ 11c0:23fd -> Map_CheckZOC (map_check_zoc).
 * ZOC gate in unit_step_toward_goal @ 1128:0000 neighbour scoring.
 * OS-free 1:1 (implemented): 8-neighbour enemy scan; nonzero
 * enemy_map cell = enemy unit. Sea/land match (0x1a3a vs 1110:16c0),
 * occupancy (1110:18ce/16ee/199b) and 1110:1dbd gates stay in the
 * caller. Params: enemy_map is w*h row-major (read-only). Returns
 * 1 = ZOC-blocked (enemy adjacent), 0 = clear, -1 on NULL/bad size
 * or (x,y) out of bounds. */
int map_check_zoc(const int *enemy_map, int w, int h, int x, int y);

/* Orig FUN_11c0_0000 @ 11c0:0000 -> City_SpotReveal (city_spot_reveal).
 * Spots cities in the unit_update_visibility @ 11c0:0395 tail via
 * 1110:180e/16ee -> 11c0:0000/11c8:00c4 (sibling
 * city_update_visibility below). Needs city records 1420:5500 +
 * seen-bits; stub. */
civ_error_t city_spot_reveal(int x, int y, int civ);

/* Orig FUN_11c8_00c4 @ 11c8:00c4 -> City_UpdateVisibility
 * (city_update_visibility). Second spot helper in the same 0395 tail
 * (sibling of city_spot_reveal above). Needs city records + seen-bits;
 * stub. */
civ_error_t city_update_visibility(int x, int y, int civ);

/* Orig FUN_11c8_02ec @ 11c8:02ec -> Unit_FindDiplomatTarget
 * (unit_find_diplomat_target). Type 0x0e with no goal picks the
 * nearest visible player diplomat (caller unit_choose_order @
 * 1078:1480). OS-free 1:1 (implemented): linear scan for the minimum
 * Chebyshev distance max(|dx|,|dy|) among visible[i] != 0; ties break
 * to the lower index. Arrays dx/dy/visible are length n (read-only);
 * (ux,uy) is the seeker tile. Returns the diplomat index, -1 when none
 * qualifies or on NULL/n <= 0. Exact distance metric TBD vs decompiler
 * (Chebyshev matches the 1128:0000 step scoring). */
int unit_find_diplomat_target(int ux, int uy, const int *dx, const int *dy,
                              const int *visible, int n);

/* --- T2 turnloop + unit orders (2026-10-15) ---
 * Parent: Game_TurnLoopTick @ 1088:0000 + Civ_ProcessUnitMoves @ 11f0:0000.
 * Order codes below are opaque dispatch keys for the three jump tables
 * inside Civ_ProcessUnitMoves (1420:43b8+0x8f x0x0e, +0x4b x0x11,
 * 1420:435b+0x28 x0x20); exact handler seg:off per case TBD from
 * decompile, so each stub cites its order key + parent + table. Unit
 * records: 1420:62fc layout, see header top (goal 6302/6303, last-dir
 * 6304, home-city 6307). OS-free predicates take explicit order codes
 * only (no Win16, no globals); Win16 map/city/dialog tails stay
 * CIV_ERR_UNSUPPORTED with TODO Orig refs. */

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x62 case -> Unit_DoFoundCity
 * (unit_do_found_city). Found-city/irrigate?: type 0x1a settler founds via
 * Unit_FoundCityIfHome @ 11c0:16a4 (unit_found_city_if_home above) when
 * home (home-city field +10/6307); irrigate vs found disambiguation TBD
 * vs decompiler. Needs city records 0x1c bytes at 1420:5500 + map tile
 * claim + dialog pump; stub. */
civ_error_t unit_do_found_city(int civ, int unit_idx);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x66 case -> Unit_DoFortify
 * (unit_do_fortify). Wait-fortify?: sets the fortified/wait flag on the
 * 1420:62fc record (exact flag bit TBD) so the unit skips moves until
 * contacted; ZOC/defense bonus applied by the combat tail. Needs unit
 * flag writes + status redraw; stub. */
civ_error_t unit_do_fortify(int civ, int unit_idx);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x68 case -> Unit_DoSentry
 * (unit_do_sentry). Sentry/1-move?: parks the unit until an enemy enters
 * visibility (unit_update_visibility @ 11c0:0395 wakes it) or one move
 * elapses; exact wake predicate TBD. Needs seen-bits + turn-counter
 * state; stub. */
civ_error_t unit_do_sentry(int civ, int unit_idx);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 orders
 * 0x69/0x6d/0x6f/0x70/0x72/0x75 cases -> Unit_DoMoveAttack variants
 * (unit_do_move_attack). Move/attack dispatch: opaque order keys into the
 * three jump tables above (1420:43b8+0x8f x0x0e, +0x4b x0x11,
 * 1420:435b+0x28 x0x20); 0x6d road?, 0x70 pollution? per frontier notes,
 * exact per-key semantics (road build vs goto vs attack-move) TBD.
 * Single dispatcher takes the order key explicitly; steps via
 * unit_step_toward_goal @ 1128:0000, odds via Combat_ResolveAttack +
 * 11c0:1bc3. Needs map occupancy/ZOC + combat state; stub. */
civ_error_t unit_do_move_attack(int civ, int unit_idx, int order);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x00 -> unit_order_is_idle.
 * OS-free 1:1 (implemented): idle/none predicate, nonzero (1) when
 * order == 0x00. Takes the order code explicitly; no Win16, no globals. */
int unit_order_is_idle(int order);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x20 -> unit_order_is_sleep.
 * OS-free 1:1 (implemented): force-sleep/reset predicate, nonzero (1)
 * when order == 0x20. Takes the order code explicitly; no Win16. */
int unit_order_is_sleep(int order);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x50 -> unit_order_is_fail.
 * OS-free 1:1 (implemented): settler-site fail predicate, nonzero (1)
 * when order == 0x50. Takes the order code explicitly; no Win16. */
int unit_order_is_fail(int order);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0xffff ->
 * unit_order_is_no_path. OS-free 1:1 (implemented): no-path predicate,
 * nonzero (1) when order == 0xffff (16-bit -1; callers may pass -1).
 * The 1128:0000 no-path path clears goal 6302/goal 6303?/last-dir 6304
 * (see unit_step_toward_goal above: clears 6302/6304); the clear itself
 * stays in the stepper, this helper is the pure code test. */
int unit_order_is_no_path(int order);

/* Orig Game_DoPollutionAndWonderRace @ 1088:1e32 tail ->
 * Unit_NotifyWonder (unit_notify_wonder). Pollution/wonder message tail:
 * "... builds ..." message + shield deduction + dialog pump after the
 * wonder-race pick. Pick itself is done via Game_PickWonderForAI @
 * 1170:3181 (game_pick_wonder_for_ai above, implemented, tech-table
 * gated); exact wonder/tech table ids TBD. Needs city shields state +
 * Win16 dialog; stub. Param wonder is the game_pick_wonder_for_ai index. */
civ_error_t unit_notify_wonder(int civ, int wonder);

/* Trivial teardown (caller-owned civ_game_t only). Marks uninitialized;
 * full WinMain_Shutdown @ 1008:0329 order (KILLTIMER for TIMERPROC @
 * 1008:0d68 / FREEPROCINSTANCE for DLGPROC @ 1098:16e2 + CIVDIALOGPROC @
 * 1098:1838 / DESTROYWINDOW for main/map/status windows /
 * REMOVEFONTRESOURCE for CIV_FONT_FILE) lives in boot_shutdown (see
 * boot.h/boot.c), which delegates to video/map/dialog/audio shutdown.
 * NULL-safe, void, no globals, no exit(). */
void game_shutdown(civ_game_t *g);

#endif /* CIVLIN_GAME_H */
