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

/* Orig: FUN_1040_068e @ 1040:068e -> City_ProcessDisaster.
 * Random city disaster, called 2x per Game_TurnLoopTick @ 1088:0000 pass
 * once g_nTurn > 0x32 for random cities (Rng_Range @ 11c8:003d draw
 * 0x80, valid via 1420:5502 != -1 + pop via 1420:3efe > 1 + size via
 * 1420:5503 > 4). Switch Rng_Range(10): building-flag checks on the
 * 0x1c city rec mask @ 1420:54fc (bits 0x10/0x100/0x80/0x08/0x04/0x02/
 * 0x40) + messages @ 1420:07f0 Earthquake / 0x839 Rioting / 0x7ff-0x802
 * destroyed + demands GRANARY/TEMPLE/COURTHOUSE/MARKETPLACE/CATHEDRAL +
 * 0x860/0x873/0x87a/0x885/0x891/0x89b tails + shared viewer
 * FUN_1030_0000 (see title.h correction: shared PIC player, not intro)
 * + dialog FUN_1220_00d2 + Map_CenterOnTile @ 1110:03ae. Win16 dialog /
 * city-record mutation tails TBD -> stub CIV_ERR_UNSUPPORTED. Param is
 * the 0-based city index into the 1420:5500 0x1c records (see header
 * top). No globals, no exit(), read-only. */
civ_error_t city_process_disaster(int city_idx);

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
 * GENERIC helper (NOT the Orig addr: 1170:3181 is the Tech_HasTech
 * bit-predicate below, verified 2026-10-16: (tech == -1) ||
 * (tech < 0x47 && civ != 0 && bit set in the ESCHISM+5+civ*10 table).
 * This helper keeps the previous tech-gated shortlist model used by the
 * trade bonus (has_t31/has_t26 via Tech_HasTech) and the wonder shortlist
 * inside City_ChooseProduction @ 1170:05ca: eligible[w] = !done[w] &&
 * (prereq < 0 || owned[prereq]). Returns eligible[roll % n] (roll = 0 for
 * first-fit); -1 when none/NULL/bad. OS-free, no globals, no exit(). */
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
 * Appends abs(year) via Util_IntToDec @ 1000:2928 (...,10) plus " BC"
 * (year < 0, 1420:173c) / " AD" (else, 1420:1740) to the caller's line
 * buffer (orig 12d8:24fa via Util_StrAppend @ 1000:363e). OS-free 1:1 on
 * explicit out/cap: "<abs> BC" or "<abs> AD" with the orig leading space
 * before the era (spaceless "BC"/"AD" is the filename form, see
 * save_build_filename @ 1198:00a3). The 1000:386e/3ba0/3d90 neighbours
 * (Date_Convert @ 1000:386e + Float_ParseFormat @ 1000:3ba0 +
 * Date_CheckValid @ 1000:3d90 via Sys_GetDate @ 1000:18d4, using
 * Util_StrCopyN @ 1000:377e + Util_ParseInt @ 1000:1ed2) are the C-runtime
 * date/float chain, NOT the game-year chain: they were misread as
 * DOS-file helpers next to the qsort/string thunks, but the true
 * year<->text path is this function + year_suffix below (5 callers:
 * status panel @ 1088:13d5, 1018:015d, 1138:0000, 11b0:0000, 11b8:14e5).
 * NOENT on NULL out, RANGE when cap == 0 or the text (incl. NUL) does
 * not fit, else CIV_OK. No globals, no exit(). */
civ_error_t game_append_year_text(int year, char *out, size_t cap);

/* AG2: year suffix (1420:173c "BC" when year<0 / 1420:1740 "AD").
 * OS-free 1:1 port (year<0 ? "BC" : "AD"); complements game_append_year_text
 * @ 1088:1dd0 (abs via Util_IntToDec @ 1000:2928 + this suffix onto the
 * 12d8:24fa line buffer). Returns a read-only literal, never writes
 * assets; no globals, no exit(). */
const char *year_suffix(int year);

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

/* --- AJ3 unit bodies: explicit record layout (no globals) ---
 * Unit records (seg 1420): 0x80 units/civ, 0xc bytes each at
 *   1420:62fc + civ*0x600 + idx*0xc (field labels see header top).
 * Callers pass the per-civ slice (or global image slice) explicitly as
 * `units` + `unit_idx` + `n_units`; civ is folded into the slice so no
 * segment globals are touched. Moves (+3) zeroes the remaining moves;
 * flags (+11) busy bits 0x08/0x09 per header top (0x04 ?, 0xc2
 * visibility, 0x20 veteran): Linux port uses 0x08 = fortified/busy and
 * 0x09 = 0x08|0x01 sentry-wait/busy; exact 0x08 vs 0x09 split TBD vs
 * decompiler, but both test as busy (flag & 0x08). */
#define CIV_UNITS_PER_CIV 0x80
#define CIV_UNIT_REC_LEN 0x0c
#define CIV_UNIT_OFF_X 0
#define CIV_UNIT_OFF_Y 1
#define CIV_UNIT_OFF_TYPE 2
#define CIV_UNIT_OFF_MOVES 3
#define CIV_UNIT_OFF_GOAL_X 5
#define CIV_UNIT_OFF_GOAL_Y 6
#define CIV_UNIT_OFF_HOME 10
#define CIV_UNIT_OFF_FLAGS 11
#define CIV_UNIT_TYPE_SETTLER 0x1a
#define CIV_UNIT_FLAG_FORTIFIED 0x08
#define CIV_UNIT_FLAG_SENTRY 0x09

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x62 case -> Unit_DoFoundCity
 * (unit_do_found_city). Found-city/irrigate: type 0x1a settler founds via
 * Unit_FoundCityIfHome @ 11c0:16a4 (unit_found_city_if_home) when home
 * (home-city field +10/6307 in the 1420:62fc record) matches city_idx.
 * Join path (OS-free 1:1, implemented): units[unit_idx].type == 0x1a
 * and home == city_idx -> consume settler (type -> 0xff empty, moves
 * -> 0) + grow the city (size+1 via city_grow on the 1420:5500 0x1c
 * records, +3 size/def; read via city_get_size). Irrigate vs found
 * disambiguation TBD vs decompiler. New-city path (home != city_idx,
 * e.g. 0xff none, or OOR city) needs map tile claim (map_get_tile /
 * map_set_tile @ 11e8:0dcf/0e19, see map.h) + city-record alloc +
 * dialog pump -> CIV_ERR_UNSUPPORTED (TODO Orig 11c0:16a4 tail +
 * 1420:5500 alloc + map claim + Win16 dialog). Explicit bases, no
 * globals, no exit(). Returns CIV_OK on join, UNSUPPORTED on new-city,
 * NOENT on NULL, RANGE on bad idx/count/type/size. */
civ_error_t unit_do_found_city(uint8_t *units, int unit_idx, int n_units,
                               uint8_t *cities, int city_idx, int n_cities);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x66 case -> Unit_DoFortify
 * (unit_do_fortify). Wait-fortify: sets the fortified/wait flag on the
 * 1420:62fc record +11 (ORs 0x08 busy, see AJ3 block above) and zeroes
 * moves (+3) so the unit holds until contacted; ZOC/defense bonus is
 * applied by the combat tail (not here). OS-free 1:1 port on explicit
 * base (no globals, no exit(), no map/dialog I/O). Returns CIV_OK,
 * CIV_ERR_NOENT on NULL, CIV_ERR_RANGE on n_units <= 0 or idx OOR. */
civ_error_t unit_do_fortify(uint8_t *units, int unit_idx, int n_units);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x68 case -> Unit_DoSentry
 * (unit_do_sentry). Sentry/1-move: parks the unit until an enemy enters
 * visibility (unit_update_visibility @ 11c0:0395 wakes it) or one move
 * elapses; Linux port ORs 0x09 (0x08 busy | 0x01 sentry-wait, see AJ3
 * block above) on +11 and zeroes moves (+3). Exact wake predicate +
 * 0x08 vs 0x09 split TBD vs decompiler. OS-free 1:1 port on explicit
 * base, same contract as unit_do_fortify (OK/NOENT/RANGE, no globals,
 * no exit()). */
civ_error_t unit_do_sentry(uint8_t *units, int unit_idx, int n_units);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 orders
 * 0x69/0x6d/0x6f/0x70/0x72/0x75 cases -> Unit_DoMoveAttack variants
 * (unit_do_move_attack). Move/attack dispatch: opaque order keys into the
 * three jump tables above (1420:43b8+0x8f x0x0e, +0x4b x0x11,
 * 1420:435b+0x28 x0x20); 0x6d road?, 0x70 pollution? per frontier notes,
 * exact per-key semantics (road build vs goto vs attack-move) TBD.
 * Single dispatcher takes the order key explicitly; AL1 wires the
 * explicit kernels: step via unit_move_attack_step (Chebyshev-remaining
 * + unit_move_cost + unit_can_enter + unit_dir_to_dxdy, moves-1, goal
 * clear on 0xffff; mirrors unit_step_toward_goal @ 1128:0000) and odds
 * via unit_attack_resolve (combat_calc_odds @ 11c0:1bc3 + Rng_Range @
 * 11c8:003d, veteran 0x20 TBD). Full board images (1420:62fc units,
 * 64 KB tile buffer, 1420:17d4 cost) + UI pump tail via
 * unit_wait_for_orders @ 11c0:0ec2 stay TBD -> CIV_ERR_UNSUPPORTED.
 * TODO Orig per-key handlers. No globals, no exit(). */
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
/* --- AI-C unit/city tables (2026-10-15) ---
 * Unit records (seg 1420): 0x80 units/civ, 0xc bytes each at
 *   1420:62fc + civ*0x600 + idx*0xc (field labels see header top).
 * City records: 0x1c bytes at 1420:5500 + idx*0x1c (+0x5500 x,
 *   +0x5501 y, +0x5502 valid, +0x5503 size/def, +0x5507 owner).
 * OS-free static-const ports: no globals, no exit(), read-only, explicit
 * base pointers (no seg globals). No new FUN_* claimed: type codes are
 * dump-observed values seen around unit_choose_order @ 1078:1480
 * (0x19/0x1a/0x0e/0x0f); undecoded slots stay "?" (TODO, no fake
 * Militia/Phalanx/Legion names beyond confirmed). */
#define CIV_UNIT_TYPE_MAX_CONFIRMED 0x1a
#define CIV_UNIT_TYPE_EMPTY 0xff
#define CIV_CITY_REC_LEN 0x1c
#define CIV_CITY_OFF_X 0
#define CIV_CITY_OFF_Y 1
#define CIV_CITY_OFF_VALID 2
#define CIV_CITY_OFF_SIZE 3
#define CIV_CITY_OFF_OWNER 7

/* Dump-observed unit type name (Orig unit record +2 type @ 1420:62ff;
 * 0xff = empty slot). OS-free static const port (implemented): confirmed
 * subset only -- 0x19 "Diplomat", 0x1a "Settler", 0x0e "Caravan?"
 * (uncertain, ? suffix), 0x0f "?" (observed but unidentified), 0xff
 * "empty"; every other code returns "?" (TODO, no fake beyond confirmed).
 * Returns a read-only literal, never NULL. No globals, no exit(). */
const char *unit_type_name(int type);

/* Unit type count (Orig type range 0x00..0x1a?). OS-free (implemented):
 * returns 0x1b (27) = max confirmed index 0x1a + 1. Civ1 is commonly
 * cited as 28 types (0x00..0x1b); the 28th slot (0x1b) is TBD vs dump,
 * see TODO on unit_type_name. No globals, no exit(). */
int unit_type_count(void);

/* City record accessors (Orig 0x1c-byte records at 1420:5500 +
 * idx*0x1c). OS-free ports (implemented): explicit base ptr + idx, no
 * globals, read-only. base points at the 1420:5500 image (n_cities
 * records); idx is the 0-based city index. Returns the byte 0..255 at
 * the field offset, -1 on NULL base, n_cities <= 0, or idx OOR. */
/* +0x5500 x (record +0). */
int city_get_x(const uint8_t *base, int idx, int n_cities);
/* +0x5501 y (record +1). */
int city_get_y(const uint8_t *base, int idx, int n_cities);
/* +0x5502 valid (record +2). */
int city_get_valid(const uint8_t *base, int idx, int n_cities);
/* +0x5503 size/def (record +3). */
int city_get_size(const uint8_t *base, int idx, int n_cities);
/* +0x5507 owner (record +7). */
int city_get_owner(const uint8_t *base, int idx, int n_cities);

/* --- AJ3 city body: size bounds (no globals) ---
 * City size lives at +3 size/def in the 0x1c records (1420:5500 +
 * idx*0x1c). Byte-storable 0..255; 0 = invalid/empty slot, 255 cannot
 * grow (overflow). Gameplay cap (aqueduct/sewer/food-box limit,
 * commonly cited ~30) TBD vs decompiler -- Linux port enforces only
 * byte bounds + minimum 1, never fabricates gameplay caps. */
#define CIV_CITY_SIZE_MIN 1
#define CIV_CITY_SIZE_MAX 0xff

/* Orig city size+1 helper (1420:5500 0x1c stride, +3 size/def) ->
 * city_grow. OS-free 1:1 port on explicit base (no globals, no exit(),
 * no map/dialog I/O): increments the size byte at
 * base + city_idx*0x1c + 3 (read via city_get_size). Bounds 1..255
 * (see AJ3 block above); gameplay max TBD. Returns CIV_OK,
 * CIV_ERR_NOENT on NULL, CIV_ERR_RANGE on n_cities <= 0, idx OOR,
 * or size 0/0xff (invalid/full). */
civ_error_t city_grow(uint8_t *cities, int city_idx, int n_cities);

/* --- AK1 move + accessors (2026-10-15) ---
 * Parent: unit_step_toward_goal @ 1128:0000 neighbour scoring
 *   (Chebyshev-remaining + terrain cost gated on sea/land, ZOC,
 *   occupancy, ocean-clear). Unit records 0xc bytes (see header top):
 *   +0 x, +1 y, +2 type, +3 moves, +4 timer, +5 goal-x, +6 goal-y,
 *   +7 last-dir (0x6304, 0xff init), +8 seen-by, +9 next, +10 home,
 *   +11 flags. City 0x1c at 1420:5500 (+0 x, +1 y, +2 valid,
 *   +3 size/def, +7 owner; prod offset TBD, see city_get_prod).
 * OS-free ports: explicit params only (tile codes + cost-table image,
 *   dir tables, record bases), no globals, no exit(), read-only
 *   (except dx/dy outs). Map-side twin tile_move_cost lives in map.h
 *   (same 1420:17d4*3-or-3 formula, duplicated here to keep game/map
 *   decoupled for the parallel rewrite). */
#define CIV_UNIT_OFF_TIMER 4
#define CIV_UNIT_OFF_LAST_DIR 7
#define CIV_UNIT_OFF_SEEN 8
#define CIV_UNIT_OFF_NEXT 9
#define CIV_UNIT_DIR_ARRIVED 0
#define CIV_UNIT_DIR_NO_PATH 0xffff
#define CIV_UNIT_REVERSE_PENALTY 2
#define CIV_UNIT_RIVER_COST 1

/* Orig unit_step_toward_goal @ 1128:0000 terrain-cost tail ->
 * unit_move_cost. OS-free 1:1 v1 (implemented): base via the
 * 1420:17d4*3-or-3 rule (see tile_move_cost in map.h, inlined here to
 * keep game/map decoupled) + river 1 when is_river != 0 +
 * anti-reverse penalty (CIV_UNIT_REVERSE_PENALTY) when move_dir 1..8 is
 * opposite last_dir 1..8 (opposite = ((last - 1 + 4) % 8) + 1; 0xff
 * init / 0 / 0xffff never penalised). Params: tile is the map tile
 * code, cost_table is the 1420:17d4 image (read-only, cost_n entries).
 * Returns total cost (>= 3), -1 on NULL table or cost_n <= 0. Exact
 * zero-entry/OOR fallback + penalty value TBD vs decompiler. No
 * globals, no exit(), read-only. */
int unit_move_cost(int tile, const uint8_t *cost_table, int cost_n,
                   int move_dir, int last_dir, int is_river);

/* Orig unit_step_toward_goal @ 1128:0000 entry gates -> unit_can_enter.
 * OS-free 1:1 v1 (implemented, 1 = enter, 0 = blocked): sea/land match
 * (is_sea_unit != is_sea_tile -> 0; 0x1a3a domain vs Map_IsSeaLand @
 * 1110:16c0 == 10), ZOC (8-neighbour enemy scan, nonzero = enemy, same
 * as Map_CheckZOC @ 11c0:23fd but inlined to keep game/map decoupled),
 * occupancy (linear (x,y) scan, same as Map_IsOccupied @ 1110:18ce /
 * 16ee), city-blocked (linear scan, same as Map_IsCityBlocked @
 * 1110:199b), ocean (is_sea_tile && ocean_depth <= 4 -> 0;
 * Map_OceanClearDepth @ 1110:1dbd > 4 rule, caller passes the
 * map_ocean_clear_depth depth 0..5). NULL/empty enemy/unit/city maps
 * mean clear for that gate (matches map_is_occupied/city_blocked);
 * (x,y) OOB vs ew/eh when enemy_map given -> 0 (blocked, cf.
 * path_is_passable). No globals, no exit(), read-only. */
int unit_can_enter(int is_sea_unit, int is_sea_tile,
                   const int *enemy_map, int ew, int eh, int x, int y,
                   const int *unit_xs, const int *unit_ys, int n_units,
                   const int *city_xs, const int *city_ys, int n_cities,
                   int ocean_depth);

/* Orig dir encoding in unit_step_toward_goal @ 1128:0000 +
 * Map_ResolveDirect @ 1110:1c4b -> unit_dir_to_dxdy. OS-free 1:1
 * (implemented): dir 1..8 (1 = N clockwise, same as map_resolve_direct;
 * exact orig numbering TBD) to dx/dy via static const dx[9],dy[9]
 * (index 0 = arrived (0,0)). dir 0 = arrived (*dx_out = 0, *dy_out = 0,
 * CIV_OK); dir 0xffff/-1 = no-path (CIV_ERR_RANGE, outs untouched, test
 * via unit_order_is_no_path first); other OOR -> CIV_ERR_RANGE; NULL
 * outs -> CIV_ERR_NOENT. No globals (function-local tables only), no
 * exit(). */
civ_error_t unit_dir_to_dxdy(int dir, int *dx_out, int *dy_out);

/* Unit record accessors (1420:62fc 0xc stride, explicit base, no
 * globals): +10 home-city (unit_home): returns home index 0..254, -1
 * on NULL, n_units <= 0, idx OOR, or stored 0xff (none). +3 moves
 * (unit_moves_left) and +8 seen-by mask (unit_seen_by): return byte
 * 0..255, -1 on NULL / n_units <= 0 / idx OOR. OS-free, read-only, no
 * exit(). */
int unit_home(const uint8_t *units, int unit_idx, int n_units);
int unit_moves_left(const uint8_t *units, int unit_idx, int n_units);
int unit_seen_by(const uint8_t *units, int unit_idx, int n_units);

/* Orig city production field? (0x1c records at 1420:5500) ->
 * city_get_prod. Known: +0 x, +1 y, +2 valid, +3 size/def, +7 owner
 * (see city_get_*). Prod/shields offset TBD vs decompile (no fake
 * offset defined here). Stub: always -1 (TBD, no fake), same -1
 * contract as other city_get_* (NULL / n_cities <= 0 / idx OOR also
 * -1). TODO Orig prod offset. No globals, no exit(), read-only. */
int city_get_prod(const uint8_t *base, int idx, int n_cities);

/* --- AL1 move_attack + tables (2026-10-15) ---
 * Parent: Civ_ProcessUnitMoves @ 11f0:0000 orders 0x69/0x6d/0x6f/0x70/
 *   0x72/0x75 (same three jump tables as unit_do_move_attack above;
 *   0x6d road?, 0x70 pollution? per frontier, exact per-key TBD).
 * Stepper unit_move_attack_step is the explicit-params single step
 *   toward goal via unit_step_toward_goal @ 1128:0000 logic
 *   (Chebyshev-remaining + move_cost + can_enter + dir_to_dxdy,
 *   moves-1, goal clear on 0xffff); attacker unit_attack_resolve is
 *   odds + roll via combat_calc_odds @ 11c0:1bc3 (atk*100/(atk+def))
 *   + Rng_Range @ 11c8:003d (via game_uprising_rng_scale) with veteran
 *   flag 0x20 TBD. Tables tech_names/wonder_names/terrain_cost_table
 *   are OS-free static-const ports (no fake beyond confirmed, see
 *   per-func TODOs). Dispatcher unit_do_move_attack wires step + attack
 *   and keeps the stub tail for the UI pump via unit_wait_for_orders @
 *   11c0:0ec2. OS-free, no globals, no exit(), read-only except the
 *   stepper's unit-record write (x/y/moves/last-dir/goal clear).
 * Map parity (read, not edited; see map.h, parallel-owned):
 *   map_is_sea_land @ 1110:16c0 == 10 (sea), map_is_occupied @
 *   1110:18ce/16ee, map_is_city_blocked @ 1110:199b, map_ocean_clear_depth
 *   @ 1110:1dbd > 4 (capped at 5), map_resolve_direct @ 1110:1c4b
 *   (|dx|<2,|dy|<2), tile_move_cost 1420:17d4*3-or-3, path_is_passable @
 *   1128:081c OOB = blocked; game-side twins unit_move_cost (river 1 +
 *   reverse 2), unit_can_enter gates, unit_dir_to_dxdy 1=N clockwise. */
#define CIV_UNIT_FLAG_VETERAN 0x20

/* Orig unit_step_toward_goal @ 1128:0000 single-step core ->
 * unit_move_attack_step. OS-free 1:1 v1 (implemented): explicit unit +
 * board params, no globals. Reads x/y/moves(+3)/goal(+5/+6, 0xff none)/
 * last-dir(+7, 0xff init) from units[unit_idx] (0xc stride, see header
 * top). Direct (|dx|<2,|dy|<2, cf. map_resolve_direct @ 1110:1c4b) then
 * 8-neighbour best-score (Chebyshev max(|gx-nx|,|gy-ny|) + unit_move_cost
 * via cost_table 1420:17d4*3-or-3 + river 1 + reverse 2, gated by
 * unit_can_enter: sea match 0x1a3a vs 10, ZOC/enemy, occupancy, city,
 * ocean >4 with depth counted as map_ocean_clear_depth @ 1110:1dbd
 * capped at 5; sea = tile 10, river = tile 0x0c per map.h; OOB blocked
 * cf. path_is_passable @ 1128:081c). Dirs via unit_dir_to_dxdy 1=N
 * clockwise. On step: x/y = nx/ny, last-dir = dir, moves-1 (exact cost
 * deduction TBD, currently 1 per step). On 0xffff: clears goal-x/goal-y/
 * last-dir to 0xff (exact 6302/6304 mask TBD, 6303? included here).
 * Returns dir 1..8 (moved), 0 arrived (x==goal) / exhausted (moves<=0) /
 * no-goal (0xff) with no write, 0xffff no-path (goal cleared), -1 on
 * NULL/bad (units/tiles/cost_table, counts, idx OOR; no write). Ties to
 * lowest dir (TBD). No globals, no exit(). */
int unit_move_attack_step(uint8_t *units, int unit_idx, int n_units,
                          const uint8_t *tiles, int bw, int bh,
                          const uint8_t *cost_table, int cost_n,
                          const int *enemy_map, int ew, int eh,
                          const int *unit_xs, const int *unit_ys,
                          int n_occ_units,
                          const int *city_xs, const int *city_ys, int n_cities,
                          int is_sea_unit);

/* Orig Combat_ResolveAttack (near unit_choose_order @ 1078:1480) +
 * FUN_11c0_1bc3 @ 11c0:1bc3 -> unit_attack_resolve. OS-free 1:1 v1
 * (implemented): odds via combat_calc_odds (atk*100/(atk+def)) on
 * effective strengths (currently raw pass-through; veteran/terrain
 * pre-scale from combat_resolve_attack stub TBD, no fake), roll via
 * Rng_Range @ 11c8:003d (game_uprising_rng_scale(rand15,100) 0..99).
 * atk_flags/def_flags bit CIV_UNIT_FLAG_VETERAN (0x20, unit +11, see
 * header top) accepted for future decoding, currently ignored (void).
 * Attacker wins when roll < odds (odds 100 always wins, 0 never).
 * Returns 1 attacker wins, 0 defender wins, -1 on bad (negative or
 * both-zero strengths via combat_calc_odds). odds_out optional (may be
 * NULL); when non-NULL writes odds 0..100 on success, -1 on bad. No
 * Win16, no globals, no exit(), read-only. */
int unit_attack_resolve(int atk_strength, int def_strength,
                        int atk_flags, int def_flags,
                        unsigned rand15, int *odds_out);

/* Orig tech table for Game_UpdatePowerRankings @ 1088:108a (tech
 * contribution, table TBD) + Game_PickWonderForAI @ 1170:3181
 * (tech-table gated via game_pick_wonder_for_ai prereq indices) ->
 * tech_names. OS-free static-const port (stub, no fake): checked game.c
 * (only prereq indices, no string literals) so no dump-confirmed names
 * yet; Alphabet/.../Nuclear hypothesis stays TODO. Returns "?" for any
 * tech (confirmed subset empty + "?" unknown, never NULL, read-only
 * literal). Count TBD (callers pass n_tech explicitly). No globals, no
 * exit().
 * CHAIN ANSWER (2026-10-16): NO — live tech strings are NOT this table.
 * They arrive via ResMgr_GetString @ 1100:078a entry 0x82 through
 * Res_GetStringCopy @ 11f8:0080 (NewGame_Wizard @ 10e8:03c6 finalize
 * loop gated by FUN_1170_3181 + Replay_PlayLog @ 1180:02d3 case 5
 * discover); see res_blob_entry_kind()/RES_BLOB_TECH in rsc_loader.h. */
const char *tech_names(int tech);

/* Orig wonder table for Game_PickWonderForAI @ 1170:3181 (eligible[w]
 * via game_pick_wonder_for_ai, tech-gated) + "... builds ..." tail via
 * unit_notify_wonder @ 1088:1e32 -> wonder_names. OS-free port (stub,
 * no fake): checked game.c (only wonder indices, no string literals)
 * so no confirmed names; 7-wonders hypothesis (Pyramids/Hanging/
 * Oracle/etc.) stays TODO. Returns "?" for any wonder (never NULL).
 * Count TBD (callers pass n_wonders explicitly). No globals, no exit().
 * CHAIN ANSWER (2026-10-16): NO — live wonder/build strings are NOT
 * this table. They arrive via ResMgr_GetString @ 1100:078a entries
 * 0x83 (build, Replay_PlayLog case 10 + 0x3967 when < 8) / 0x84
 * (produce-first, case 6) through Res_GetStringCopy @ 11f8:0080; see
 * RES_BLOB_WONDER_BUILD/RES_BLOB_PRODUCE in rsc_loader.h. */
const char *wonder_names(int wonder);

/* Orig 1420:17d4 terrain base-cost table for unit_move_cost /
 * tile_move_cost (*3-or-3) -> terrain_cost_table. OS-free port (stub,
 * no fake): entries/stride TBD (0x?? entries? stride? vs decompile),
 * so no image yet; consumers take the explicit cost_table image until
 * the fixed image is decoded (see unit_move_cost + tile_move_cost in
 * map.h, same formula). Returns base cost per tile when known, -1 TBD
 * for now on any tile (no fake entries; tile param void). TODO Orig
 * 1420:17d4 layout. No globals, no exit(), read-only.
 * CHAIN ANSWER (2026-10-16): NOT the ResMgr blob — terrain costs live
 * in the 1420:17d4 byte table (sea = tile 10, river = tile 0x0c per
 * map.h; Map_IsSeaLand @ 1110:16c0 == 10 gate in unit_can_enter). */
int terrain_cost_table(int tile);

void game_shutdown(civ_game_t *g);

/* --- City pop/name chain (2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   City_FormatPopCount @ 11c8:03ac (pop -> "1,230" style via
 *     Util_IntToDec @ 1000:2928 + Util_StrAppend @ 1000:363e with DGROUP
 *     separators @ 12d8:42ec/42ee/42f0; callers City_CalcPopTotal @
 *     11c8:0334 + City_ShowInfoDialog @ 1030:8b39 + Replay_PlayLog @
 *     1180:02d3 case 0xb + HoF_Show @ 1080:0119).
 *   City_CalcPopTotal @ 11c8:0334 (triangular sum over the 0x80 0x1c
 *     city records @ 1420:5500: owner @ +7 == civ and valid @ +2 != -1,
 *     sum 1..size @ +3, then City_FormatPopCount; 5 callers:
 *     Game_TurnLoopTick @ 1088:0000 scoring (u16 split /0x100 + &0xff
 *       via FUN_11c0_3beb), Game_UpdateStatusPanel @ 1088:13d5 (pop line
 *       at local_14+4/local_12+2 via TEXTOUT + "00,000 citizens." @
 *       1420:172b tail), HoF_TryInsert @ 1080:0595 (slot pop @ +0x27
 *       stride w6), City_ShowCensusReport @ 1018:1d5b (census totals +
 *       FUN_1018_202a ratios), Demo_ShowDemographics @ 1188:123a
 *       (per-civ Population row)).
 *   City_FormatName @ 1048:0101 (ResMgr_GetString 0x88,4/5 via
 *     Menu_CheckCityNameId @ 1008:0d93 gate + GLOBALLOCK/UNLOCK +
 *     Res_GetStringCopy tail + Res_AppendPascal @ 11f8:014a x2; sole
 *     caller Menu_InitMainBar @ 1048:0000 with city ids 0x1c/0x34 into
 *     the 256 B local_106 Pascal buffer, Util_PascalToCStr, MODIFYMENU
 *     0xcf/0xd0).
 * Status-panel neighbours (Game_UpdateStatusPanel @ 1088:13d5 context):
 *   Status_DrawPanelFrame @ 11f8:0278 (FILLRECT via Gfx_MakeBrush @
 *     1098:00b5 + white/black CREATEPEN 0xffff/0 + MOVETO/LINETO border;
 *     upper rect via FUN_1218_0956 + lower via FUN_1218_0a06, see map.h).
 *   Status_AppendYearText @ 1088:1dd0 (abs(year) + " BC" @ 1420:173c /
 *     " AD" @ 1420:1740 onto 12d8:24fa; alias of game_append_year_text).
 *   Status_DrawTechIcons @ 1088:1a82 (separator rule + 9 icons via
 *     Gfx_DrawTechIcon @ 11a8:0cb1 from DAT_1420:9632; player rows live,
 *     others LCG-faked; see game.h status_draw_tech_icons).
 *   Text_SetColor @ 1098:0e0b (DAT_12d8_45be color at @ 1088:1554 before
 *     the pop line; Gfx_LookupColor @ 1098:01b0 + SETTEXTCOLOR +
 *     CREATEPEN + Gfx_MakeBrush @ 1098:00b5 tail stays Win16).
 * OS-free number/name math below is LIVE 1:1 on explicit params (no
 * Win16, no globals, read-only); DGROUP separator bytes stay caller
 * params (exact 42ec/42ee/42f0 literals TBD vs runtime dump). */

/* Orig: City_FormatPopCount @ 11c8:03ac — pop-count text.
 * 1:1 shape: if pop > 99 { q = pop / 100; append dec(q); append
 * thousands sep; if pop % 100 < 10 append zero pad; } append
 * dec(pop % 100); append suffix. Separators are caller params (orig
 * DGROUP @ 12d8:42ec "," / 42ee "0" / 42f0 suffix hypothesis from the
 * "00,000 citizens." @ 1420:172b neighbour; exact bytes TBD, never
 * hardcoded here beyond the caller test). Returns CIV_OK, NOENT on
 * NULL out/sep, RANGE when cap == 0 or the text (incl. NUL) overflows.
 * No globals, no exit(). */
civ_error_t city_format_pop_count(int pop, const char *sep_thousands,
                                  const char *pad_zero, const char *suffix,
                                  char *out, size_t cap);

/* Orig: City_CalcPopTotal @ 11c8:0334 — triangular pop total.
 * Sum over i in 0..n-1 with owner_of[i] == owner and valid_of[i] != -1
 * of (1 + ... + size_of[i]). owner/valid/size arrays are the 0x1c
 * record fields (+7 owner, +2 valid, +3 size @ 1420:5500). NULL arrays
 * or n <= 0 -> -1 (orig faulted). No globals, no exit(). */
int city_calc_pop_total(const int *owner_of, const int *valid_of,
                        const int *size_of, int n, int owner);

/* Orig: City_FormatName @ 1048:0101 + Res_AppendPascal @ 11f8:014a —
 * pascal city-name join (dst[0] = 0, then append p1 + p2, each
 * length-prefixed u8 + bytes). p1/p2 NULL = skip that part (orig
 * always had both via 0x88,4/5 + Res_GetStringCopy tail). Returns
 * CIV_OK, NOENT on NULL dst, RANGE when cap < 1 or the join (len byte
 * + chars) overflows cap-1. No globals, no exit(). */
civ_error_t city_format_name(const uint8_t *p1, const uint8_t *p2,
                             uint8_t *dst, size_t cap);

/* --- Census / demographics / status-content (2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   City_ShowCensusReport @ 1018:1d5b (per-city rows via City_OpenCensusView
 *     @ 1018:0000 + shared PIC viewer FUN_1030_0000 + building icons 0x2618
 *     mask via FUN_1030_84be + totals + City_FormatCensusRatio @ 1018:202a
 *     x3 + City_CloseCensusView @ 1018:0380; empty-city guard local_10==0
 *     -> 1; scroll at 0xbe rows via PresentRect + FUN_1038_0c1d).
 *   City_FormatCensusRatio @ 1018:202a (percent/ratio text: num<300 ?
 *     (num*100)/den : (num*10)/(den/10), dec via Util_IntToDec @ 1000:2928
 *     + append onto 12d8:24fa; sole caller City_ShowCensusReport x3:
 *     surplus/total, (total-surplus-decay)/total, decay/total).
 *   Demo_ShowDemographics @ 1188:123a (per-civ rows 1..7 over 0x80 0x1c
 *     city records @ 1420:5500 + tile land scan via Map_IsSeaLand @
 *     1110:16c0 != 10 / Map 1110:16ee owner + Demo_RankRow @ 1188:220a per
 *     row; rows: Approval (happy*0x32/pop), Population (City_CalcPopTotal
 *     @ 11c8:0334), GNP (trade*2+lux?), Mfg (1260:00d8), Land (tile
 *     counts, "000 sq miles"), Literacy (((schools+pop*2)*2)/pop x2 per
 *     tech 0/0x16/0x1e/0xd via Game_PickWonderForAI tech table @ 1170:3181
 *     clamp 0..100), Disease ((pop*0x32)/(pop+impr) /2 with medicine 10,
 *     then 0x5dc/(v+0x14)), Pollution (smoke totals - smoke*10/pop),
 *     Life Expectancy (clamp(impr,0x14,99)), Family Size
 *     ((food*0x28)/pop+0x14, x/10 + "." + x%10 + " children", rank /10),
 *     Military ((3eee count*10)/pop), Income (trade x tax-rate/pop when
 *     small else trade/pop x rate, "per capita"), Productivity
 *     (output*10/pop); live player + LCG-fake? NO: demo sums live city
 *     records for every civ (no LCG fake unlike Status_DrawTechIcons @
 *     1088:1a82); AI rows are live sums, not fakes).
 *   Status refresh (Game_UpdateStatusPanel @ 1088:13d5): NOT polled by
 *     Game_TurnLoopTick @ 1088:0000 on a timer; explicit push from 7
 *     callers (Game_TurnLoopTick @ 1088:0162/0887 scoring + turn tail,
 *     Civ_ProcessUnitMoves @ 11f0:0bbb, Map_RefreshAllViews @ 1110:03e9,
  *     WdwStatus_Refresh @ 1218:0801 dirty-flag path, Trade_DeliverCaravanGoods
  *     @ 1038:0d09, Diplomat_CityAction @ 11b8:0000, Diplomat_BribeUnit @
  *     11b8:07c2 gold/city rows). Regions in order:
 *     upper frame (WdwStatus_CalcUpperRect @ 1218:0956 + Status_DrawPanelFrame
 *     @ 11f8:0278 color DAT_12d8_45f4, tech-progress bar when
 *     DAT_12d8_38f0 > 1) + Status_DrawTechIcons @ 1088:1a82 (separator
 *     rule + 9 icons, LCG-faked AI rows here only) + lower frame
 *     (WdwStatus_CalcLowerRect @ 1218:0a06 + frame color 45fb) + pop row
 *     (Text_SetColor @ 1098:0e0b color DAT_12d8_45be at @ 1088:1554 +
 *     City_CalcPopTotal + 0x16e9 + TEXTOUT) + year row
 *     (Status_AppendYearText @ 1088:1dd0) + progress icon (clamp(science
 *     <<2 / ((diff*2+20+6)*tax+1)...)*... 0..3 via 1420:98e4) + pollution
 *     icons (42f0%4 / /4 clamp 0..3) + treasury row (treasury dec + 16eb +
 *     16ed + (10 - lux - tax) + 16f0 + lux + 16f2 + tax + TEXTOUT) +
 *     milestone popup (Dialog_ShowMessage @ 1220:00d2 when the pop tier
 *     rises and DAT_1420_14e6 > -2, "00,000 citizens." @ 1420:172b tail).
 *     Status_DrawPanelFrame @ 11f8:0278 itself has 4 xrefs (2x inside
 *     Game_UpdateStatusPanel @ 1088:1483/1540 upper+lower, Unit_ShowInfoPopup
 *     @ 11f0:4703, WdwStatus_Refresh @ 1218:08b7), not 7: the "7" is the
 *     Game_UpdateStatusPanel caller fan-out above.
 * OS-free number math below is LIVE 1:1 on explicit params (no Win16, no
 * globals, read-only); GDI/dialog tails stay STUB with TODO Orig. */

/* Orig City_FormatCensusRatio @ 1018:202a -> city_format_census_ratio.
 * 1:1 quotient text (remainder is computed but only the quotient is
 * appended in orig): num < 300 ? (num*100)/den : (num*10)/(den/10).
 * Writes the decimal quotient via Util_IntToDec @ 1000:2928 shape.
 * Returns CIV_OK, NOENT on NULL out, RANGE when cap == 0, den <= 0,
 * den/10 == 0 on the big arm, or the text (incl. NUL) does not fit.
 * No globals, no exit(). */
civ_error_t city_format_census_ratio(int num, int den, char *out, size_t cap);

/* Orig Demo_ShowDemographics @ 1188:123a approval row -> demo_approval.
 * 1:1 (happy*0x32)/pop; pop <= 0 -> -1 (orig divided live totals,
 * caller guarantees pop >= 1 via local_2a init 1). No globals. */
int demo_approval(int happy, int pop);

/* Orig Demo literacy row (Demo_ShowDemographics @ 1188:123a +
 * Game_PickWonderForAI tech gate @ 1170:3181) -> demo_literacy.
 * 1:1 base = ((schools + pop*2)*2)/pop, then x2 per owned tech among
 * (0, 0x16, 0x1e, 0x0d), then Util_Clamp(base,0,100). tech_owned is an
 * array of n_tech flags (nonzero = owned); NULL with n_tech > 0 -> base
 * un-doubled (no fake). pop <= 0 -> -1. No globals, no exit(). */
int demo_literacy(int schools, int pop, const int *tech_owned, int n_tech);

/* Orig Demo disease row (Demo_ShowDemographics @ 1188:123a +
 * medicine tech 10 @ 1170:3181) -> demo_disease_rate.
 * 1:1 v = (pop*0x32)/(pop+impr); has_medicine != 0 -> v /= 2.
 * Returns v, -1 on pop+impr <= 0 or pop < 0. The 0x5dc/(v+0x14)
 * life-expectancy helper below takes v. No globals. */
int demo_disease_rate(int pop, int impr, int has_medicine);

/* Orig Demo life-expectancy helper ((Demo_ShowDemographics @ 1188:123a
 * disease row tail) -> demo_life_years. 1:1 0x5dc/(v+0x14) with v from
 * demo_disease_rate; denominator always > 0 for v >= 0. Returns -1 on
 * v < 0. No globals. */
int demo_life_years(int v);

/* Orig Demo_RankRow @ 1188:220a ranking half -> demo_rank_player.
 * 1:1 rank of the player value among alive civs 1..7 (bit mask
 * alive_mask, bit civ): rank = 1 + #(alive civ != player with
 * vals[civ] > vals[player]); dead/empty slots ignored. Returns 1-based
 * rank, -1 on NULL vals or player outside 1..7. The bar/text tail via
 * FUN_10d8_0000/01fa/05b8 stays Win16 (see demo_rank_row STUB below).
 * No globals, no exit(), read-only. */
int demo_rank_player(const int *vals, int n, int player, unsigned alive_mask);

/* Orig Status_DrawTechIcons @ 1088:1a82 LCG half -> status_lcg_next.
 * 1:1 AI-fake draw step (player rows copy live 0xdc/0xc4 rows; other
 * civs iterate seed = seed*5+1 and pick (seed & 0xff) % 9): this helper
 * is one step. seed is DAT_13c0_0000*civ start. Returns the next seed
 * (32-bit wrap). No globals. */
unsigned status_lcg_next(unsigned seed);

/* Orig Demo_ShowDemographics @ 1188:123a -> demo_show_demographics.
 * Full-screen demo (FUN_1188_0000 open + per-row Demo_RankRow @ 1188:220a
 * + FUN_1018_0380 close). Ranking kernels above are LIVE; GDI/dialog
 * tails -> STUB. */
civ_error_t demo_show_demographics(int civ);

/* Orig Demo_RankRow @ 1188:220a bar/text tail -> demo_rank_row.
 * Rank bars via FUN_10d8_0000/01fa/05b8 + "1st/2nd/3rd/Nth" text. Ranking
 * half lives in demo_rank_player above (LIVE); GDI tail -> STUB. */
civ_error_t demo_rank_row(const int *vals, int n, int player);

/* Orig City_ShowCensusReport @ 1018:1d5b -> city_show_census_report.
 * Per-city rows + totals + City_FormatCensusRatio x3 (dialog/viewer
 * tails via City_OpenCensusView @ 1018:0000 + FUN_1030_0000 + FUN_1038_0c1d
 * + City_CloseCensusView @ 1018:0380). Win16 dialog/GDI -> STUB. */
civ_error_t city_show_census_report(int civ);

/* --- Trade / diplomat / revolt / shared-PIC chain (2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Trade_DeliverCaravanGoods @ 1038:0d09 (caravan delivery; sole caller
 *     Pic_PlayShared @ 1030:0000 prod-0x1b branch; callees Map_WrapStepDist
 *     @ 11c8:02ec, Map_GetContinentId @ 1110:1d99 x2, Game_PickWonderForAI
 *     tech gate @ 1170:3181 x2 (0x31, 0x26), City_AppendName @ 1038:0bba,
 *     Res_GetStringCopy @ 11f8:0080 (0x8e), Dialog_ShowMessage @ 1220:00d2,
 *     Game_UpdateStatusPanel @ 1088:13d5, Unit_FoundCityIfHome @ 11c0:16a4
 *     consume tail + 3-slot route table @ 1420:5513).
 *   Diplomat_CityAction @ 11b8:0000 (diplomat-enters-city menu; caller
 *     Civ_ProcessUnitMoves @ 11f0:0000 city-entry branch at 11f0:1ff7
 *     CALLF 11b8:0000, diplomat/city split vs the 0x1b caravan check at
 *     11f0:1fff; cases: 0 embassy (diplo bit 0x40 + FUN_1018_1739),
 *     1 investigate (FUN_1170_05af), 2 steal tech (Rng_Range @ 11c8:003d
 *     0x48 scan, 5502&0x20 already-stolen gate, Res 0x82 names),
 *     3 sabotage (Rng_Range(2)/0x18 building pick on 54fc mask or
 *     5505 prod sabotage, Res 0x83/0x84), 4 incite revolt (cost rule
 *     below + Dialog 100 + City_DoRevolt), 5 leave (moves -> 0)).
 *   Diplomat_BribeUnit @ 11b8:07c2 (unit bribe; caller unit_choose_order
 *     @ 1078:1480 per frontier; cost rule below + Map_CenterOnTile @
 *     1110:03ae + Unit_FoundCityIfHome-consume @ 11c0:16a4 + 11c0:12c8
 *     respawn + seen-bit flip + treasury deduct + status panel).
 *   City_DoRevolt @ 11b8:0f26 (city secession; callers Diplomat_CityAction
 *     case 4 + Unit_DoMove @ 1038:0000 paths; param_2 == -1 auto-picks the
 *     best foreign city ((00de-00e0)*0x20/dist max); flips seen-bits at
 *     x*0x32+y+10 + 0x14 diamond neighbours, moves near units via
 *     1110:171f/1906 + 11c0:12c8, then Unit_DoMove(new_owner,city,1);
 *     message via 1420:4168/4182 + Dialog_ShowMessage @ 1220:00d2).
 *   Pic_PlayShared @ 1030:0000 (shared city modal/sim; NOT the intro loop,
 *     see title.h correction; 13 callers: City_DoRevolt @ 11b8:0f26,
 *     City_ProcessDisaster @ 1040:068e, City_ShowCensusReport @ 1018:1d5b,
 *     City_ShowTopFive @ 1080:099b, Demo_ShowDemographics @ 1188:123a,
 *     1018:0be0, 1018:208f, 1040:1137, 1170:0000, 1170:05af, 1170:05ca,
 *     1188:0db8, Unit_DoMove @ 1038:0000; params (city_idx, mode): mode 1
 *     = open modal city view (Dlg_CreateTracked @ 1098:124f 0x94 +
 *     FILLRECT/FRAMERECT + 1030:79ce/7c28/... tile helpers), mode 0 =
 *     background sim tick (growth/prod/trade incl. the
 *     Trade_DeliverCaravanGoods call above); no Art_LoadFrameData @
 *     10b8:079d frame path inside (city GDI tiles, not RSC frames)).
 *   Map_GetContinentId @ 1110:1d99 (renamed 2026-10-16 from FUN_1110_1d99;
 *     Map_GetTile @ 11e8:0dcf(x, y+0x32) byte; same-id = same
 *     continent/sea, gates the Trade halve + Unit_DoMove settler 0x1b
 *     branch; exact layer meaning TBD vs tile-buffer map).
 * OS-free bonus/cost math below is LIVE 1:1 on explicit params (no
 * globals, no Win16, read-only); treasury/city-record/dialog tails stay
 * STUB with TODO Orig. */

/* Orig Trade_DeliverCaravanGoods @ 1038:0d09 bonus kernel ->
 * trade_calc_bonus. 1:1 base = (dist+10)*(size_a+size_b)/0x18, then
 * same_cont != 0 -> /2 (Map_GetContinentId @ 1110:1d99 equality),
 * domestic != 0 -> /2 (owner(dst) == civ), has_t31 != 0 -> -= /3
 * (tech 0x31 via Game_PickWonderForAI @ 1170:3181), has_t26 != 0 ->
 * -= /3 (tech 0x26). dist is the Map_WrapStepDist @ 11c8:02ec value
 * (map_wrap_step_dist, see map.h); sizes are the 1420:5506 city bytes.
 * Negative inputs clamp to 0 (orig used non-negative game values).
 * No globals, no exit(). */
int trade_calc_bonus(int dist, int size_a, int size_b, int same_cont,
                     int domestic, int has_t31, int has_t26);

/* Orig Trade_DeliverCaravanGoods @ 1038:0d09 route-slot value ->
 * trade_route_value. 1:1 local_8 = size, foreign != 0 -> size*2 (the
 * 1420:5513 3-slot table keeps the best routes by this value, replacing
 * the worst slot). size < 0 -> -1. No globals. */
int trade_route_value(int size, int foreign);

/* Orig Trade_DeliverCaravanGoods @ 1038:0d09 -> trade_deliver_caravan_goods.
 * Full delivery (treasury += bonus x2 cells + player message via Res 0x8e
 * + City_AppendName + Dialog_ShowMessage + Game_UpdateStatusPanel, then
 * Unit_FoundCityIfHome-consume @ 11c0:16a4 + 5513 route update). Needs
 * city records @ 1420:5500 + treasury cells + Win16 dialog -> STUB. */
civ_error_t trade_deliver_caravan_goods(int civ, int unit_idx, int city_idx);

/* Orig Diplomat_CityAction @ 11b8:0000 case 4 revolt-price ->
 * diplomat_revolt_cost. 1:1 ((treasury+1000)/(dist+3))*size, half != 0
 * -> /2 (city flag 1420:5502&1, temple/courthouse-class dampener, exact
 * building TBD). dist is the 1168:12c1 value (explicit param). Returns
 * the price, -1 on dist < 0, size < 0, or treasury < 0. No globals. */
int diplomat_revolt_cost(int treasury, int dist, int size, int half);

/* Orig Diplomat_BribeUnit @ 11b8:07c2 price ->
 * diplomat_bribe_cost. 1:1 ((treasury+0x2ee)/(dist+2))*unit_cost, then
 * type != 0 -> /2 (type 0 capital-class pays full, exact class split
 * TBD; unit_cost is the 0x1a44 shield-cost table entry for the unit
 * type). dist is the 1168:12c1 value (explicit param). Returns the
 * price, -1 on dist < -1, unit_cost < 0, or treasury < 0. No globals. */
int diplomat_bribe_cost(int treasury, int dist, int unit_cost, int type);

/* Orig Diplomat_CityAction @ 11b8:0000 -> diplomat_city_action.
 * Menu cases 0..5 (embassy/investigate/steal/sabotage/revolt/leave;
 * cost kernels above are LIVE). Needs diplo bits @ 12d8:3a0a + city
 * records + Rng + Win16 dialog -> STUB. */
civ_error_t diplomat_city_action(int city_idx, int civ, int unit_idx);

/* Orig Diplomat_BribeUnit @ 11b8:07c2 -> diplomat_bribe_unit.
 * Price kernel above is LIVE; treasury deduct + consume/respawn +
 * Map_CenterOnTile @ 1110:03ae + dialog tails -> STUB. */
civ_error_t diplomat_bribe_unit(int civ, int unit_idx, int by_civ);

/* Orig City_DoRevolt @ 11b8:0f26 -> city_do_revolt.
 * Seen-bit flips (x*0x32+y+10 + diamond), near-unit moves, message +
 * Unit_DoMove(new_owner, city, 1) tail need map/city records + Win16
 * dialog -> STUB. new_owner -1 = auto-pick best foreign city (orig
 * (00de-00e0)*0x20/dist scan). */
civ_error_t city_do_revolt(int city_idx, int new_owner);

/* Orig Pic_PlayShared @ 1030:0000 -> pic_play_shared.
 * Shared city modal/sim player (mode 1 = modal view via
 * Dlg_CreateTracked @ 1098:124f + tile helpers 1030:79ce/7c28/...;
 * mode 0 = background sim tick incl. caravan prod 0x1b). Needs city
 * records + GDI modal + tile helpers -> STUB. See title.h: this is NOT
 * the intro loop (title_play_anim_loop stays a separate intro stub). */
civ_error_t pic_play_shared(int city_idx, int mode);

/* --- Chain 2026-10-16: Pic modes + 0x1b caravan + unit loop + tech ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Pic_PlayShared @ 1030:0000 (city_idx, mode): invalid city
 *     (1420:5502 == -1) -> return 0 immediately; mode 1 = modal city
 *     view (Dlg_CreateTracked @ 1098:124f dialog 0x94 + FILLRECT/FRAMERECT
 *     + City_RenderWorkedTile @ 1030:79ce / City_TileYield @ 1030:7c28 /
 *     City_IsWonderOpen @ 1030:7e57 / Tech_FindFirstOwner @ 1030:7ece /
 *     City_SetSpecialist @ 1030:7f65 / City_GetSpecialist @ 1030:7fc1
 *     tile/citizen helpers); mode 0 = background sim tick (growth /
 *     production / trade incl. Trade_DeliverCaravanGoods @ 1038:0d09 for
 *     prod 0x1b). 13 callers, see chain block above.
 *   City_RenderWorkedTile @ 1030:79ce (city, slot, draw): per-slot worked
 *     tile claim/draw; draw == 1 centers (1330:4010/400e = x-2/y-2) +
 *     paints via Pic tile blit @ 1030:90a8 + yield icons via 11a8:15ce
 *     (icon stride local_e = 2*iVar5: totals <5 -> 6, <7 -> 3 else 2);
 *     else Map_SetTile(x+0x50, y, owner). Accumulates DAT_1420_00d6
 *     food/shield/trade triple (local per-kind loop 0..2 via
 *     City_TileYield). Needs GDI + map buffer -> STUB below.
 *   City_TileYield @ 1030:7c28 (x, y, kind 0/1/2 food/shield/trade):
 *     base via Map 1110:168f gate (0 outside) + tile class @ 1110:16c0 +
 *     1110:1c72 irrigated/road flag (1420:17d6 vs 0x1836 tables), then
 *     occupancy @ 1110:199b modifiers (food/shield/trade trims, forest
 *     +1 trade), ocean-river zero, owner(3) trade +1, pollution halving
 *     (0x40), gov penalties (despotism -1 over 2, 1250:0000 double-tax
 *     -2), trade doubling (0x40 flag + gov threshold), wonder-trade
 *     halve (0x40). Needs map tables -> STUB below.
 *   Tech_HasTech @ 1170:3181 (civ, tech): 1 when tech == -1, else
 *     tech < 0x47 && civ != 0 && bit (tech%0x10) in word (tech/0x10) of
 *     the per-civ 5-word row (ESCHISM+5+civ*10). Gate for trade tech
 *     0x31/0x26 (has_t31/has_t26 in trade_calc_bonus), wonder shortlist
 *     + City_IsWonderOpen/Tech_FindFirstOwner + City_ChooseProduction
 *     unit/building/wonder gates. OS-free bit test -> LIVE below.
 *   City_IsWonderOpen @ 1030:7e57 (civ, tech): 0 when any civ 1..7 owns
 *     tech (via Tech_HasTech), else 1 when the discover-table owner
 *     (s_discover_1420_393d[tech]) == civ. OS-free on explicit
 *     per-civ flags -> LIVE below.
 *   Tech_FindFirstOwner @ 1030:7ece (tech): first civ 1..7 owning tech
 *     via Tech_HasTech, else the discover-table value. OS-free on
 *     explicit flags -> LIVE below.
 *   City_GetSpecialist @ 1030:7fc1 (slot): 2-bit citizen packing at
 *     1420:5510 (word per city at rec+0x10, slot*2 bits); slot >= 8
 *     -> 1. City_SetSpecialist @ 1030:7f65 (slot, value): masked set
 *     of the same 2 bits (slot >= 8 = no-op). OS-free -> LIVE below.
 *   Civ_ProcessUnitMoves @ 11f0:0000 city-entry/unit dispatch (verified
 *     2026-10-16 via 11f0:1e80..21b6 disasm): type < 0x1a -> normal move
 *     (jmp 228a); type == 0x1a settler: owner(dst city @ 11c8:00c4) ==
 *     civ -> fall to diplomat test (jmp 1fff), else settler found path;
 *     diplomat test at 1f48: owner(dst) == player civ (12d8:4300) -> set
 *     seen-bit (6305 |= 1<<civ) + reveal via 11c0:231d/11e8:02a4/11c0:2c32
 *     then Diplomat_CityAction @ 11b8:0000 (CALLF 11f0:1ff7, sole xref),
 *     else jmp 1ff0 (direct diplomat call); type == 0x1b caravan branch
 *     at 11f0:2012 (JZ 201d): home-city (6307) x/y + dist via
 *     Map_WrapStepDist @ 11c8:02ec, owner(dst)==civ + prod(5505) >= 0xe8
 *     + dist < 0xa gates, then Map_GetContinentId @ 1110:1d99 x2
 *     (home vs unit tile): same continent -> jmp 2228 (skip bonus),
 *     differ -> caravan message + Dialog_ShowMessage @ 1220:00d2 (same
 *     same_cont halve as trade_calc_bonus). OS-free dispatch/gates ->
 *     LIVE predicates below; reveal/dialog tails stay STUB.
 *   City_ChooseProduction @ 1170:05ca (civ, city): AI production picker.
 *     Opens Pic_PlayShared(city, 0xffff) probe, builds the eligible list
 *     (units 0x00..0x1b via Tech_HasTech on 0x1a4c/0x1a38 tables with the
 *     0x19/0x1b specials, buildings -1..-0x18 via tech 0x1928 + 54fc mask
 *     + continent/sea gates, wonders -0x19..-0x2e via tech + discover ==
 *     -1), scores units (supply/local_120 + continent/sea/defense/tech
 *     multipliers) vs buildings/wonders (fixed table + local_16 sea
 *     scaling /3eee), picks min cost; civ 0 human -> dialog (Dlg 0x1000
 *     + SENDMESSAGE list + 1170:1f84 pump, 0x66 accept / 0x65 cancel).
 *     Needs city records + Rng + Win16 dialog -> STUB below; range
 *     classifiers + tech gates above are LIVE.
 *   City_DestroyCity @ 1170:0241 (city, x, y): city teardown (Replay log
 *     1 + prod-counter decrement + valid -> 0xff + 1110:1a8f clear +
 *     route-table 5513 purge + discover-table release + home-unit resettle
 *     via 11c0:16a4 + 0x2d-tile restore via 1128:177e/11c8:0124 +
 *     Map tile restore + view refresh). Needs records -> STUB.
 *   City_OpenProductionView @ 1170:05af (?, city): Pic_PlayShared(city, 1)
 *     modal tail. Win16 dialog -> STUB.
 * OS-free predicates take explicit params (no globals, no Win16,
 * read-only except the specialist word write); Win16/GDI/dialog/map tails
 * stay CIV_ERR_UNSUPPORTED with TODO Orig refs. */

/* Orig Tech_HasTech @ 1170:3181. OS-free 1:1 (implemented): 1 when
 * tech == -1 (unconditional, even on NULL bits), else 0 unless
 * 0 <= tech < 0x47 && civ != 0 with bit (tech%0x10) set in word
 * (tech/0x10) of the civ row. bits is the n_civs*5 u16 image of the
 * ESCHISM+5+civ*10 rows (row-major, 5 words/civ); civ indexes 0..n-1
 * (civ 0 never owns per orig civ != 0 gate). Returns 1 owned / 0 not.
 * No globals, no exit(), read-only. */
int tech_has_tech(int civ, int tech, const uint16_t *bits, int n_civs);

/* Orig Pic_PlayShared @ 1030:0000 mode split. OS-free 1:1 (implemented):
 * modal == (mode == 1) opens the Dlg_CreateTracked @ 1098:124f 0x94
 * city view; sim == (mode == 0) runs the background growth/prod/trade
 * tick (incl. caravan 0x1b). Other modes (e.g. 0xffff probe from
 * City_ChooseProduction @ 1170:05ca) are neither. No globals. */
int pic_mode_is_modal(int mode);
int pic_mode_is_sim(int mode);

/* Orig City_GetSpecialist @ 1030:7fc1. OS-free 1:1 (implemented): 2-bit
 * citizen value (0..3) at slot*2 in the city specialist word
 * (1420:5510: word at base+idx*0x1c+0x10, LE). slot >= 8 -> 1 (orig
 * default). Returns value or 1/ -1 (NULL base, n <= 0, idx OOR,
 * slot < 0). No globals, no exit(), read-only. */
int city_get_specialist(const uint8_t *base, int city_idx, int n_cities,
                         int slot);

/* Orig City_SetSpecialist @ 1030:7f65. OS-free 1:1 (implemented): sets
 * the same 2 bits to value & 3 (orig shifted raw value; mask keeps the
 * neighbour slots intact). slot >= 8 is a no-op CIV_OK (orig skipped
 * the write). Returns CIV_OK, NOENT on NULL, RANGE on n <= 0 / idx OOR
 * / slot < 0. No globals, no exit(). */
civ_error_t city_set_specialist(uint8_t *base, int city_idx, int n_cities,
                                int slot, int value);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 type dispatch (CMP 62ff,0x1a @
 * 11f0:1ecd + CMP 62ff,0x1b @ 11f0:2012). OS-free 1:1 (implemented):
 * settler == (type == 0x1a), caravan == (type == 0x1b). Types are the
 * 1420:62ff unit-type bytes (0xff empty). No globals. */
int unit_is_settler(int type);
int unit_is_caravan(int type);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 caravan 0x1b gates (11f0:20b0
 * dist < 0xa via Map_WrapStepDist @ 11c8:02ec; 11f0:20a8 prod 5505 >=
 * 0xe8 signed (-24); 11f0:211b/2126 continent differ via
 * Map_GetContinentId @ 1110:1d99 x2). OS-free 1:1 (implemented):
 * dist_ok = dist < 10; prod_ok = prod >= -24 (signed-char compare, 0xe8
 * = -24: wonders/buildings less negative than -24 pass); foreign_bonus
 * = (cont_home != cont_dst) (same_cont halve in trade_calc_bonus).
 * dist/cont are caller ints; prod is the signed 5505 byte. No globals. */
int caravan_dist_ok(int dist);
int caravan_prod_ok(int prod);
int caravan_foreign_bonus(int cont_home, int cont_dst);

/* Orig City_IsWonderOpen @ 1030:7e57. OS-free 1:1 (implemented): 0 when
 * any has_tech[1..7] != 0 (Tech_HasTech per civ), else 1 when
 * discover_owner == civ. has_tech indexes 0..n-1 with civ ids 1..7
 * (index 0 unused, like the orig civ loop). Returns 1 open / 0 taken,
 * -1 on NULL/n <= civ. No globals, no exit(), read-only. */
int city_is_wonder_open(int civ, const int *has_tech, int n,
                         int discover_owner);

/* Orig Tech_FindFirstOwner @ 1030:7ece. OS-free 1:1 (implemented):
 * first civ 1..7 with has_tech[civ] != 0, else discover_fallback
 * (the s_discover_1420_393d[tech] value). Same indexing as
 * city_is_wonder_open. Returns civ id or fallback, -1 on NULL.
 * No globals, no exit(), read-only. */
int tech_find_first_owner(const int *has_tech, int n,
                           int discover_fallback);

/* Orig City_ChooseProduction @ 1170:05ca prod ranges. OS-free 1:1
 * (implemented classifiers): units are 0x00..0x1b (eligible loop
 * local_6 < 0x1c, incl. 0x19 diplomat / 0x1a settler / 0x1b caravan
 * specials); buildings are negative -1..-0x18 (loop 1..0x18 ->
 * local_e8 = -local_6); wonders are negative -0x19..-0x2e (loop
 * 0x19..0x2e -> -local_6). Returns nonzero (1) when prod is in the
 * class. No globals. */
int city_prod_is_unit(int prod);
int city_prod_is_building(int prod);
int city_prod_is_wonder(int prod);

/* Orig City_ChooseProduction @ 1170:05ca -> city_choose_production.
 * AI prod picker (eligible lists + unit/building/wonder scoring + human
 * dialog via Dlg 0x1000 + 1170:1f84 pump). Needs city records + Rng +
 * Win16 dialog -> STUB. Params: owning civ + city index (0x1c records
 * at 1420:5500, see header top). */
civ_error_t city_choose_production(int civ, int city_idx);

/* Orig City_TileYield @ 1030:7c28 -> city_tile_yield. Per-tile
 * food/shield/trade (kind 0/1/2) with map/occupancy/gov/pollution
 * modifiers. Needs map tables (1420:17d6/0x1836, 1110:168f/16c0/1c72/
 * 199b) -> STUB. out may be NULL (pure probe returns TBD code). */
civ_error_t city_tile_yield(int x, int y, int kind, int *out);

/* Orig City_RenderWorkedTile @ 1030:79ce -> city_render_worked_tile.
 * Per-slot worked-tile claim/draw (draw != 0 paints via 1030:90a8 +
 * 11a8:15ce icons, else Map_SetTile claim). Needs GDI + map buffer +
 * DAT_1420_00d6 accumulators -> STUB. */
civ_error_t city_render_worked_tile(int city_idx, int slot, int draw);

/* --- Chain 2026-10-16 (2): production shortlist/scoring + TileYield tables
 * + Unit_DoMove dispatch + unit-loop order ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   City_ChooseProduction @ 1170:05ca eligible loops: units 0x00..0x1b
 *     (local_6 < 0x1c; 0x19 needs DAT_12d8_3960 != -1 + Tech(0x1b) via
 *     Tech_HasTech @ 1170:3181; prereq @ 1420:1a4c owned + obsolete @
 *     1420:1a38 NOT owned + sea gate (5502&2 || domain @ 1420:1a3a != 2));
 *     buildings 1..0x18 (local_e8 = -local_6; tech @ 1420:1928 + 54fc mask
 *     + 7/0xf/0x10/0xc/0x14/0x13/0x15 + continent local_2a + gov 12d8:3964/
 *     12d0:00b6 gates); wonders 0x19..0x2e (-local_6; tech @ 1420:1928 +
 *     discover @ 1420:3905+9 == -1). Scoring: units supply/local_120 +
 *     continent/sea/defense/tech multipliers vs cost ((1a44+2)/((1a40+1a42)*
 *     (1a3c+1))); buildings/wonders (base*sea*3)/(cost+3). Human civ 0 ->
 *     dialog Dlg 0x1000 + SENDMESSAGE list + City_PumpProductionDialog @
 *     1170:1f84 (0x66 accept / 0x65 cancel). Renamed 2026-10-16:
 *     City_AppendTurnsText @ 1170:2106 (cost " (N turns)" line),
 *     City_PumpProductionDialog @ 1170:1f84.
 *   City_TileYield @ 1030:7c28 base: Map_CheckTileBounds @ 1110:168f gate
 *     (0 outside; 0x50 x 0x32 game bounds) + tile class @ 1110:16c0 +
 *     Map_CheckTileImproved @ 1110:1c72 irrigated/road flag (1420:17d6 vs
 *     0x1836 tables, stride 8, kind 0/1/2 food/shield/trade at +0/+1/+2)
 *     + Map_GetOccupancyFlags @ 1110:199b modifiers (GetTile y+100 +
 *     y+0x96*0x10). Tables below are the 1420:17d6/0x1836 dumps (base
 *     17 tiles x8, irr 11 tiles x8 0..10 ocean-inclusive; beyond TBD).
 *   Unit_DoMove @ 1038:0000 city transfer (param_1 new owner, param_2 city,
 *     param_3 mode): treasury pair @ 1420:43b8+0x8c (loser -= iVar7,
 *     winner += iVar7 via Util_Clamp @ 11a0 range), replay log + 1078:4c1a
 *     + seen-bit diamond + Map 1110:171f/0448 + message/dialog tails.
 *   Civ_ProcessUnitMoves @ 11f0:0000 city-entry/unit dispatch (verified
 *     2026-10-16 via 11f0:1e80..21b6 disasm): type < 0x1a -> normal move
 *     (jmp 228a); type == 0x1a settler (6307 home vs dst city @ 11c8:00c4);
 *     owner(dst) == player (12d8:4300) -> seen-bit (6305 |= 1<<civ) +
 *     Unit_EnsureOnScreen @ 11c0:231d + Unit_PlayMoveAnimation @ 11c0:2c32
 *     + Diplomat_CityAction @ 11b8:0000 (CALLF 11f0:1ff7 sole xref);
 *     type == 0x1b caravan branch @ 11f0:2012 (home 6307 x/y + dist via
 *     Map_WrapStepDist @ 11c8:02ec + 5505 prod >= -24 + dist < 10 +
 *     continent differ via Map_GetContinentId @ 1110:1d99 x2 -> bonus
 *     message + Dialog @ 1220:00d2). Renamed 2026-10-16:
 *     Map_CheckTileBounds @ 1110:168f, Map_CheckTileImproved @ 1110:1c72,
 *     Map_GetOccupancyFlags @ 1110:199b, Unit_EnsureOnScreen @ 11c0:231d,
 *     Unit_PlayMoveAnimation @ 11c0:2c32. Loop order: per-civ 0x80 units
 *     (1420:62fc stride 0xc) outer, per-pass do-while inner (bVar5 dirty
 *     flag + local_38 wrap cursor + DAT_1310_0002 early-out); per unit:
 *     visibility via Unit_RevealArea @ 11c0:12c8 / update @ 11c0:0395,
 *     centering via Unit_MaybeCenterView @ 11f0:44d4, orders via
 *     Unit_ChooseOrder @ 1078:1480 / stepper @ 1128:0000 jump tables,
 *     then reveal/dialog tails. OS-free gates/scores/tables below are
 *     LIVE 1:1 on explicit params (no globals, no Win16, read-only
 *     except stated writes); Win16/GDI/dialog tails stay STUB. */

/* Orig City_ChooseProduction @ 1170:05ca unit gate -> city_prod_unit_gate.
 * OS-free 1:1 (implemented): unit 0x00..0x1b else 0; prereq_owned == 0
 * -> 0 (Tech @ 1420:1a4c); obsolete_owned != 0 -> 0 (Tech @ 1420:1a38
 * must be NOT owned); is_sea_unit != 0 && coastal_ok == 0 -> 0 (5502&2
 * || domain @ 1420:1a3a != 2); unit == 0x19 && special_ok == 0 -> 0
 * (DAT_12d8_3960 != -1 + Tech(0x1b)). Returns 1 eligible / 0 not. */
int city_prod_unit_gate(int unit, int prereq_owned, int obsolete_owned,
                        int is_sea_unit, int coastal_ok, int special_ok);

/* Orig City_ChooseProduction @ 1170:05ca unit score -> city_prod_unit_score.
 * OS-free 1:1 v1 (implemented): base = (supply * upkeep * cost_plus2) /
 * (atk_plus_def * moves_plus1) mirroring (iVar7*iVar4*(1a44+2)) /
 * (((1a40)+(1a42))*((1a3c)+1)); then x sea_mult x gov_mult (orig x2/x4
 * continent/sea/defense/tech branches, exact branch TBD). Returns base,
 * -1 on atk_plus_def <= 0 || moves_plus1 <= 0. No globals. */
int city_prod_unit_score(int supply, int upkeep, int cost_plus2,
                         int atk_plus_def, int moves_plus1,
                         int sea_mult, int gov_mult);

/* Orig City_ChooseProduction @ 1170:05ca building/wonder score ->
 * city_prod_building_score. OS-free 1:1 (implemented): (base_value *
 * sea_factor * 3) / (cost_plus3) mirroring (iVar4*iVar7*3)/(cost+3)
 * (sea local_16 match 10 vs 0x14 branches + 54fc&2 quarter bonus stay in
 * caller). Returns value, -1 on cost_plus3 <= 0. No globals. */
int city_prod_building_score(int base_value, int sea_factor, int cost_plus3);

/* Orig City_OpenProductionView @ 1170:05af -> city_open_production_view.
 * Pic_PlayShared(city, 1) modal tail (Dlg_CreateTracked @ 1098:124f 0x94
 * + tile helpers). Win16 dialog -> STUB. */
civ_error_t city_open_production_view(int city_idx, int prod);

/* Orig City_TileYield @ 1030:7c28 base table -> city_tile_base_yield.
 * OS-free 1:1 (implemented): tile 0..16, kind 0/1/2 (food/shield/trade),
 * irrigated != 0 selects the 0x1836 table (tiles 0..10; beyond falls back
 * to base). Tables are the 1420:17d6/0x1836 dumps (stride 8, kind at
 * +0/+1/+2; bytes 3..7 are next-terrain/cost data, not yields). Returns
 * byte 0..255, -1 on OOR tile/kind. No globals, read-only. */
int city_tile_base_yield(int tile, int irrigated, int kind);

/* Orig City_TileYield @ 1030:7c28 modifiers -> city_tile_yield_adjust.
 * OS-free 1:1 v1 (implemented): occupancy trims (kind 0 + (occ&2) ->
 * base += -1-pen0 [tile*0xc]; kind 1 + (occ&4) -> base += -1-pen4
 * [tile*0xc+4]; kind 2 + (occ&8) + land_bonus != 0 -> base += 1) +
 * trade_bonus (Tech(3) owner + gov threshold +1 each, caller sums) +
 * gov_penalty (despotism -1 over 2, caller passes 0/1) + pollution_halve
 * ((base+1)/2 when != 0, 0x40 flag); clamps < 1 -> 0. Exact tile*0xc
 * table ids + 1250:0000 double-tax -2 TBD. Returns adjusted yield. */
int city_tile_yield_adjust(int base, int kind, unsigned occ_flags,
                           int pen0, int pen4, int land_bonus,
                           int trade_bonus, int gov_penalty,
                           int pollution_halve);

/* Orig Civ_ProcessUnitMoves @ 11f0:1e80..21b6 type branches.
 * OS-free 1:1 (implemented): diplomat == (type == 0x19) (frontier
 * unit_choose_order special; city-entry diplomat path @ 11f0:1fff via
 * Diplomat_CityAction @ 11b8:0000); valid == (0x00 <= type <= 0x1b)
 * (eligible loops local_6 < 0x1c); dispatch: 0 = settler (0x1a), 1 =
 * caravan (0x1b), 2 = diplomat (0x19), 3 = normal (other valid),
 * -1 invalid/empty (0xff). No globals. */
int unit_is_diplomat(int type);
int unit_type_is_valid(int type);
int unit_dispatch_class(int type);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 order keys (three jump tables).
 * OS-free 1:1 (implemented predicates): fortify == 0x66 (Unit_DoFortify
 * LIVE in game.c), sentry == 0x68 (Unit_DoSentry LIVE), found == 0x62
 * (Unit_DoFoundCity PARTIAL), move == one of 0x69/0x6d/0x6f/0x70/0x72/
 * 0x75 (Unit_DoMoveAttack PARTIAL). Exact per-key semantics (road vs
 * goto vs attack-move, 0x6d road? 0x70 pollution? TBD) stay in the
 * dispatcher TODO. Returns nonzero (1) on match. */
int unit_order_is_fortify(int order);
int unit_order_is_sentry(int order);
int unit_order_is_found(int order);
int unit_order_is_move(int order);

/* Orig Unit_DoMove @ 1038:0000 treasury pair -> city_transfer_treasury.
 * OS-free 1:1 (implemented): loser/city shields via Util_Clamp @ 11a0
 * range (iVar7): is_winner != 0 -> treas + amount else treas - amount.
 * Caller passes the clamped iVar7; negative treas clamps to 0+ (orig
 * used non-negative game values). No globals. */
int city_transfer_treasury(int treas, int amount, int is_winner);

/* Orig Civ_ProcessUnitMoves @ 11f0:0000 loop order -> civ_unit_turn_step.
 * PARTIAL dispatcher (implemented classify + STUB tails): documents the
 * per-civ 0x80-unit outer / per-pass do-while inner (bVar5 dirty +
 * local_38 wrap + DAT_1310_0002 gate) with per-unit order visibility
 * (Unit_RevealArea @ 11c0:12c8 / update @ 11c0:0395) -> centering
 * (Unit_MaybeCenterView @ 11f0:44d4) -> choose/step (1078:1480 /
 * 1128:0000) -> jump tables -> reveal/dialog. IDLE (0x00) returns CIV_OK
 * (no work); all other orders hit Win16/map/city tails -> STUB
 * CIV_ERR_UNSUPPORTED; bad order/civ/unit -> RANGE/NOENT. No globals. */
civ_error_t civ_unit_turn_step(int civ, int unit_idx, int order);

#endif /* CIVLIN_GAME_H */
