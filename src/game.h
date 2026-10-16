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

/* --- Chain 2026-10-16 (3): production dialog pump + tile family + AI + reveal ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   City_PumpProductionDialog @ 1170:1f84 (already named; sole caller
 *     City_ChooseProduction @ 1170:05ca dialog tail). Modal Win16 pump:
 *     MAKEPROCINSTANCE + SETWINDOWLONG hook, PEEKMESSAGE loop on
 *     DAT_12d8_0030, ISDIALOGMESSAGE, then hook restore + FREEPROCINSTANCE;
 *     writes *param_2 = DAT_12d8_0030, returns 0. Result codes come from
 *     the ChooseProduction tail loop: do { Pump(); SENDMESSAGE 0x409 list
 *     sel -> local_e8[sel+1]; } while (local_2e != 0x66 && != 0x65):
 *     0x66 = accept (returns local_e8[0] current prod), 0x65 = cancel
 *     (returns the newly picked iVar4). Actions in the loop: list-select
 *     change production, "?" help via FUN_1020_0000 + Pic 1030:9203,
 *     advisor compare (Military/Domestic text via 11d0:0115/022d +
 *     1068:2072/255a when g_nOptionFlags&1 and local_24/local_22 set).
 *     Scoring relation: the advisor pair (local_24 best unit, local_22
 *     best building/wonder) and overall best (local_10/local_c minima)
 *     are the argmin picks below; the human dialog shows them as advisor
 *     text while the AI takes local_22 (or local_10 when human-automated).
 *   Map_CheckTileBounds @ 1110:168f + Map_CheckTileImproved @ 1110:1c72 +
 *     Map_GetOccupancyFlags @ 1110:199b + Map_GetTile @ 11e8:0dcf +
 *     Map_SetTile @ 11e8:0e19 + Map_GetContinentId @ 1110:1d99 (full tile
 *     family; see map.h for the LIVE ports + reader fan-out).
 *   Unit_ChooseOrder @ 1078:1480 (renamed from FUN_1078_1480; 0x300e B AI
 *     brain; sole caller Civ_ProcessUnitMoves @ 11f0:0000): civ==0
 *     delegates to Unit_AIPlayerTurn @ 1078:4f0f; 0x0f+timer<1 -> 0x68;
 *     0x0e+no-goal -> nearest visible player diplomat (Map_WrapStepDist @
 *     11c8:02ec, unit_find_diplomat_target LIVE) or staging city else
 *     0x20; 0x19 -> bribe-target scan (power/tech/diplo, 0x80 cities) else
 *     0x20; 0x1a+no-goal+home -> Unit_FoundCityIfHome @ 11c0:16a4; then
 *     hut-hunt (tile+0x50), difficulty snipe, city-defense 0x66, stack /
 *     settler (local_3a class 0/2/3/5), combat odds via Combat_CalcOdds @
 *     11c0:1bc3 + Combat_ResolveAttack @ 1050:0000, explorer walk.
 *   Unit_StepTowardGoal @ 1128:0000 (renamed from FUN_1128_0000):
 *     player-adjacent (|dx|<2,|dy|<2) via Util_SignStep @ 1110:1c4b, else
 *     corridor FUN_1128_10de(999) + passability FUN_1128_081c, then
 *     8-neighbour score (Chebyshev-remaining + 1420:17d4*3-or-3, river 1,
 *     anti-reverse 6304 penalty; sea/land, ZOC, occupancy, ocean>4 gates).
 *     Returns dir 1..8 / 0 arrived / 0xffff no-path (clears 6302/6304).
 *   Unit_AIPlayerTurn @ 1078:4f0f (renamed from FUN_1078_4f0f; civ==0
 *     delegate): barbarian-shore landing (0x75), city-garrison 0x66,
 *     neighbour best-score walk, settler escort/found, diplomat/bribe
 *     interplay with Unit_ChooseOrder. Calls Unit_StepTowardGoal @
 *     1128:0000 at 1078:55d3.
 *   Unit_UpdateVisibility @ 11c0:0395 (renamed from FUN_11c0_0395; x, y,
 *     civ, unit, redraw): player gets the 0x19-tile diamond (offsets via
 *     Map_WrapX @ 1128:177e table, far 9.. gated on unit 0x1a46&2 +
 *     domain 0x1a3a vs sea 10); others get 8 near + 0x10 far (0x1a46&2
 *     gate). Seen-bits at x*0x32+y+10 |= 1<<civ (fog-of-war; City_DoRevolt
 *     @ 11b8:0f26 flips the same bytes on revolt). City contact via
 *     Map_FindUnitAt @ 1110:180e + Map_GetTileOwner @ 1110:16ee ->
 *     City_SpotReveal @ 11c0:0000 / City_FindByTile @ 11c8:00c4, goal flips
 *     via 11c0:1ee1, diplomacy via 1168:0a07 + Unit_QueueOrder @ 1078:455c,
 *     minimap via Map_SetTile(x+0xf0, 1/2). Called per unit from
 *     Civ_ProcessUnitMoves @ 11f0:0259/3688 and from Unit_SpawnUnit.
 *   Unit_SpawnUnit @ 11c0:12c8 (renamed from FUN_11c0_12c8; NOT a reveal
 *     wrapper: unit factory): scans 0x80 slots for type -1 (else the
 *     FIRSTUNIT message + -1), inits x/y/type/seen/goal 0xff/timer from
 *     0x1a3e/home via City_FindNearest @ 11c8:0124, count++, Map 1110:171f
 *     /1767 place, seen-bit OR, then Unit_UpdateVisibility when
 *     DAT_13b8_0000 == 0 + FIRSTUNIT1/2 tutorial messages for the player.
 * OS-free predicates/kernels below are LIVE 1:1 on explicit params (no
 * globals, no Win16, read-only); Win16 modal/map/city/diplomacy tails stay
 * STUB with TODO Orig refs. */

/* Orig City_PumpProductionDialog @ 1170:1f84 modal pump -> result codes.
 * OS-free 1:1 (implemented predicates): accept == (code == 0x66),
 * cancel == (code == 0x65) (the ChooseProduction do/while exit keys).
 * Takes the DAT_12d8_0030/modal result code explicitly. No globals. */
int city_pump_result_is_accept(int code);
int city_pump_result_is_cancel(int code);

/* Orig City_ChooseProduction @ 1170:05ca best-pick loop (local_c/local_28/
 * local_26 minima over the local_e8 eligible list) -> argmin.
 * OS-free 1:1 (implemented): returns the index 0..n-1 of the minimum
 * score (ties break to the lower index, like the orig strict-< loop);
 * -1 on NULL scores or n <= 0. Callers map the index back through
 * local_e8 (positive = unit 0x00..0x1b, negative = -building/-wonder).
 * The human dialog shows the same minima as advisor text (local_24 unit
 * vs local_22 building/wonder vs local_10 overall); the AI takes the
 * corresponding minimum directly. No globals. */
int city_production_best_pick(const int *scores, int n);

/* Orig City_PumpProductionDialog @ 1170:1f84 -> city_pump_production_dialog.
 * Win16 modal pump (MAKEPROCINSTANCE + SETWINDOWLONG hook + PEEKMESSAGE /
 * ISDIALOGMESSAGE loop on DAT_12d8_0030 + restore + FREEPROCINSTANCE).
 * Needs Win16 message state + Dlg_CreateTracked @ 1098:124f dialog; stub
 * returns CIV_ERR_UNSUPPORTED (result_out untouched). NOENT on NULL. */
civ_error_t city_pump_production_dialog(int *result_out);

/* Orig Unit_ChooseOrder @ 1078:1480 head branches (OS-free 1:1 predicates,
 * implemented): civ==0 delegates to Unit_AIPlayerTurn @ 1078:4f0f (the
 * human-civ order path); type 0x0f with build timer < 1 parks as sentry
 * 0x68; type 0x0e with no goal (0xff/-1) hunts the nearest visible player
 * diplomat (unit_find_diplomat_target @ 11c8:02ec) or a staging city else
 * 0x20. goal uses the 6302 byte (0xff = none, -1 when read signed).
 * Returns nonzero (1) when the branch fires. No globals. */
int unit_choose_delegates_to_player(int civ);
int unit_needs_sentry_idle(int type, int timer);
int unit_diplomat_needs_goal(int type, int goal);

/* Orig Unit_AIPlayerTurn @ 1078:4f0f + Unit_StepTowardGoal @ 1128:0000
 * neighbour scoring (OS-free 1:1 kernels, implemented): best-pick is the
 * argmax over the 8-neighbour scores (ties break to the lower dir, like
 * the orig strict-> loop; dirs 1..8, 1=N clockwise per map_resolve_direct
 * @ 1110:1c4b); reverse_penalty is the 6304 anti-reverse term
 * (0 when last_dir is 0xff/0/0xffff or move_dir OOR 1..8, else
 * dist4(move-last)^2 where dist4 folds >4 to 8-dist, cf. the 1128:0000
 * local_26 block). Returns index/dir or penalty. No globals. */
int unit_ai_best_dir(const int *scores, int n);
int unit_ai_reverse_penalty(int move_dir, int last_dir);

/* Orig Unit_UpdateVisibility @ 11c0:0395 seen/minimap kernels (OS-free 1:1,
 * implemented): seen_index = x*0x32+y+10 (the fog byte for the tile; the
 * same x*0x32+y+10 bytes City_DoRevolt @ 11b8:0f26 flips); seen_bit =
 * 1<<civ (civ masked &0x1f like orig; -1 on civ OOR 0..31); minimap_value
 * = (tile == 1) ? 1 : 2 (Map_SetTile(x+0xf0, y, v) in the redraw tail);
 * sight_far_allowed = ((range_flags & 2) != 0) && (domain != 2 ||
 * tile_is_sea != 0) (the 0x1a46&2 + 0x1a3a-vs-10 gate for the 0x10 far
 * ring; player diamond is 0x19 tiles, others 8 near + 0x10 far).
 * sight_count folds it: player != 0 -> 0x19, else far_allowed ? 0x18
 * (8+0x10) : 8. No globals. */
int unit_seen_index(int x, int y);
int unit_seen_bit(int civ);
int unit_reveal_minimap_value(int tile);
int unit_sight_far_allowed(int range_flags, int domain, int tile_is_sea);
int unit_sight_count(int is_player, int range_flags, int domain,
                     int tile_is_sea);

/* Orig Unit_SpawnUnit @ 11c0:12c8 slot scan + City_FindByTile @ 11c8:00c4 /
 * City_FindNearest @ 11c8:0124 (OS-free 1:1, implemented on explicit
 * arrays, no globals, read-only): free_slot = first i with types[i] ==
 * -1/0xff (the 0x80 1420:62ff scan; -1 when full/NULL/n <= 0); find_by_tile
 * = first i with valid[i] != -1 && xs[i] == x && ys[i] == y (the 0x80 0x1c
 * city scan; -1 when none); nearest = argmin Map_WrapStepDist @ 11c8:02ec
 * (map_wrap_step_dist, see map.h) with dist_out the winning distance
 * (DAT_1420_b386 in orig; -1/NULL-safe, dist_out may be NULL). */
int unit_find_free_slot(const int *types, int n);
int city_find_by_tile(const int *xs, const int *ys, const int *valids, int n,
                      int x, int y);
int city_find_nearest(int ux, int uy, const int *xs, const int *ys,
                      const int *valids, int n, int *dist_out);

/* --- Chain 2026-10-16 (4): combat/ZOC/spawn/queue/visibility ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   Unit_IterateStack @ 11c0:1ee1 (stack walk: next 0x6306 chain from
 *     param_2, callback param_3 per node, stop on -1 / loop-back / >9
 *     steps; backs Combat_CalcOdds @ 11c0:1bc3, Unit_CountDefenders @
 *     11c0:1d70, Unit_UpdateVisibility @ 11c0:0395 goal flips, Combat_
 *     ResolveAttack @ 1050:0000 tails).
 *   Unit_PickStackTop @ 11c0:1a2c (top resolve via IterateStack into
 *     DAT_1420_b380/b37e; backs Unit_DrawStacked @ 1110:1516).
 *   Combat_ScoreAdjacent @ 1050:13af (diplomat 0x19 area tally: 8-neighbour
 *     Map_FindUnitAt @ 1110:180e + city 0x10000 gate + Combat_CalcOdds per
 *     node + IterateStack; called from Combat_ResolveAttack when atk type
 *     0x19).
 *   Unit_QueueShift @ 1078:4837 (make-room shift in the 0x20 AI queue:
 *     rows 0x1e..slot shift +1 when prio != 0; backs Unit_QueueOrder).
 *   Map_MirrorOccupancy @ 1110:1a2f (copy (x,y+100)->(x+0x50,y+100) +
 *     (x,y+0x96)->(x+0x50,y+0x96); backs visibility/ZOC tails).
 *   Map_SetTileOwner @ 1110:171f (tile(x+0xa0,y) = (old&8)|civ; backs
 *     Unit_SpawnUnit place + City_DoRevolt moves).
 *   Map_PlaceUnitAt @ 1110:1767 (Map_FindUnitAt + Unit_LinkStack @
 *     1050:1105 + SetTile(civ|8); backs Unit_SpawnUnit place).
 *   Unit_LinkStack @ 1050:1105 (insert into the 0x6306 next chain).
 *   Diplo_PostContact @ 1168:0a07 (diplomacy post on land contact in the
 *     0395 tail; needs diplo bits @ 12d8:3a0a + dialogs -> STUB).
 *   Unit_DrawStacked @ 1110:1516 (pick-top via 11c0:1a2c + Unit_DrawSingle
 *     @ 1110:0dba; needs GfxPort -> STUB) + Unit_DrawSingle @ 1110:0dba
 *     (sprite blit via 1208:2061 + Gfx_DrawTechIcon @ 11a8:0cb1 -> SDL TBD).
 * Combat (callers of Combat_CalcOdds @ 11c0:1bc3: Combat_ResolveAttack @
 *   1050:0000, Diplomat_BribeUnit @ 11b8:07c2, FUN_1050_13af=Combat_
 *   ScoreAdjacent above, FUN_1128_10de corridor @ 1128:10de, Unit_
 *   ChooseOrder @ 1078:1480): CalcOdds itself only stores atk/def globals
 *   + IterateStack + returns DAT_1420_b37c (formula lives in ResolveAttack
 *   below); CountDefenders @ 11c0:1d70 only zeroes DAT_13b8_0002 +
 *   IterateStack + returns it (tally is inside the 1ee1 callback, exact
 *   predicate TBD); Unit_FindAtTile @ 11c8:01ab is the nearest-unit scan
 *   (0x80 62ff active, exclude idx, argmin Map_WrapStepDist @ 11c8:02ec,
 *   DAT_1420_b386 = winning dist; sibling of City_FindNearest @ 11c8:0124).
 *   ResolveAttack @ 1050:0000 formula (decoded 2026-10-16): atk =
 *   1a40[type]<<3; def = 1a42[type]*4*terrain(17d5) (*3/2 fortified 0x08,
 *   *2 city 0x20, *3 walls 0x80+!0x0c vs non-1a3a==1, flat <<3 for air/sea
 *   1a3a 1/2); city 3efe==1 -> atk 0 when atk civ == 0; palace 54fc&1 halves
 *   atk; vet 0x20 +50% both; moves<3 scales atk *moves/3; preview when
 *   param_5==0 is (atk<<3)/(def+1) else Rng_Range(atk) vs Rng_Range(def)
 *   rolls (+ city-holder reroll local_22). Difficulty: civ==0 attacker
 *   halves/quarters vs player (g_nDifficulty+1)/4 or /2; param_5!=0 halves
 *   vs player when diff<2 and doubles player atk when diff==0. Tables
 *   1a40/1a42/17d5/1a3a/1a46 exact bytes TBD vs dump (no fake values here).
 * ZOC (Map_CheckZOC @ 11c0:23fd + Map_ScanZOCDetail @ 11c0:2437): Check is
 *   the occupancy gate (Map_GetOccupancyFlags @ 1110:199b &1 city -> 0, else
 *   ScanDetail); ScanDetail is the 8-neighbour enemy scan (offsets via the
 *   local_6*2 table + Map_WrapX @ 1128:177e, Y unwrapped): sea match
 *   (GetTileClass @ 1110:16c0 == 10 both sides), Map_GetUnitAt @ 1110:18ce
 *   != -1 with owner != self -> 1 (blocked) + player reveal side effects
 *   (seen-bit OR + Map_RevealCenter @ 1110:1461 + city 5504=5503 + 1110:1a2f
 *   when player; Win16). No diplomat/caravan/sea exceptions in the scan
 *   itself: sea/land match is the only domain gate, occupancy/city/ocean
 *   gates stay in Unit_StepTowardGoal @ 1128:0000 / Unit_CanEnter. Callers:
 *   Unit_StepTowardGoal neighbour scoring, Unit_ChooseOrder, Unit_
 *   AIPlayerTurn @ 1078:4f0f, FUN_1078_0000 @ 1078:0000, Game_SpawnUprisings
 *   @ 1088:0be9.
 * Spawn (Unit_SpawnUnit @ 11c0:12c8 factory, NOT a reveal wrapper): scans
 *   0x80 62ff slots for -1 (full past 0x7e -> FIRSTUNIT message once +
 *   -1), inits x/y/type/seen(1<<civ)/goal 0xff/last-dir 0xff/next 0xff via
 *   Map 1110:171f/1767 place + seen-bit OR, timer from type*0x16+0x1a3e
 *   (-1 once when nonzero), home via City_FindNearest @ 11c8:0124 (foreign
 *   owner -> 0xff), count++, then Unit_UpdateVisibility when DAT_13b8_0000
 *   == 0 + FIRSTUNIT1/2 tutorial for the player. Callers: production @
 *   10e8:12b5/1d48, revolt @ 11b8:0f26, bribe @ 11b8:07c2, uprisings @
 *   1088:0be9, hut 11c0:2677, City_SpotReveal @ 11c0:0000 (player-city unit
 *   pair) ; relation to Unit_FoundCityIfHome @ 11c0:16a4 (consume: desert
 *   count--, type -> 0xff, moves -> 0, 1110:17bc unlink + reveal; settler
 *   0x17/sea-10 gates via IterateStack messages stay Win16).
 * Queue->visibility (Unit_QueueOrder @ 1078:455c 0x20 slots/civ at
 *   0x612/0x512/0x412/0x312 x/y/kind/prio: dup (same x/y/kind, stored prio
 *   <= new) -> drop; AI intercept (13f0:000c==civ != player, kind 3/4:
 *   same-continent in-range unit gets goal + 62fc|0x10&0xf0); else first
 *   kind==-1 or prio<new via Unit_QueueShift, then store): kinds 1/2/3/4
 *   are contact orders (1 = attack/contact, 2 = defend?, 3 = naval?, 4 =
 *   prio; exact semantics TBD, never faked). Visibility @ 11c0:0395 fans
 *   out per unit (diamond above) into City_SpotReveal (player-city pair
 *   spawn + 5502|4), Diplo_PostContact, QueueOrder(1..4), minimap
 *   (Map_SetTile x+0xf0 1/2 via unit_reveal_minimap_value) and view
 *   (Map_RevealCenter @ 1110:1461 -> Map_RefreshTileView @ 1110:0448 GfxPort
 *   blit; needs SDL view state -> STUB). Reveal radius: player 0x19,
 *   others 8 near + 0x10 far (0x1a46&2 + 0x1a3a-vs-10 gate) via
 *   unit_sight_count (LIVE already).
 * Wrap/lookup (Map_WrapX @ 1128:177e single-step +-0x50 on X only, Y never
 *   wraps; Map_GetUnitAt @ 1110:18ce = civ slot from tile(x+0xa0,y): bit3
 *   clear -> -1 else bits0-2; Map_FindUnitAt @ 1110:180e = owner verify:
 *   GetUnitAt slot -> linear 0x80 scan for x/y match (sets 12d8:4304/4306,
 *   clears stale tile to 0) else -1; Map_GetTileOwner @ 1110:16ee =
 *   tile(x+0xa0,y)&7 tile-owner nibble (map layer) vs city owner @
 *   1420:5507 (record) vs unit owner (slot civ): tile nibble is the fast
 *   path, records are authoritative.
 * OS-free kernels below are LIVE 1:1 on explicit params (no globals, no
 * Win16, read-only except stated writes); Win16/GDI/dialog/city-state
 * tails stay CIV_ERR_UNSUPPORTED with TODO Orig refs. */
#define CIV_UNIT_QUEUE_LEN 0x20

/* Orig Combat_ResolveAttack @ 1050:0000 atk kernel -> combat_effective_attack.
 * 1:1: atk = base<<3; veteran (0x20 @ 62fc) -> atk += atk/2; moves (+3 @
 * 6300) < 3 -> atk = atk*moves/3. base is the 1420:1a40[type] entry
 * (caller reads the table; exact bytes TBD, never faked here). Difficulty
 * (civ==0 vs player, (diff+1)/4 or /2, diff<2 halves, diff==0 doubles) and
 * city 3efe==1 -> 0 stay in the caller (need game state). Returns atk >= 0,
 * -1 on base < 0 or moves < 0. No globals, no exit(). */
int combat_effective_attack(int base, int veteran, int moves_left);

/* Orig Combat_ResolveAttack @ 1050:0000 def kernel -> combat_effective_defense.
 * 1:1: flat (air/sea domain 1a3a 1/2) -> base<<3 (terrain/fort/city ignored);
 * else def = base*4*terrain_mult (terrain_mult is the 1420:17d5 tile entry,
 * caller reads it); fortified (0x08 @ 62fc) -> def = def*3/2; city_mult 2
 * (city 0x20 occupancy, no walls) -> *2, 3 (walls 0x80 + atk != 0x0c,
 * non-air) -> *3 (caller picks 1/2/3, 1 = open); veteran (0x20) ->
 * def += def/2. base is 1420:1a42[type]. Returns def >= 0, -1 on
 * base < 0, terrain_mult < 0, city_mult < 1, or flat/veteran/fortified
 * outside 0/1. No globals, no exit(). */
int combat_effective_defense(int base, int terrain_mult, int fortified,
                             int city_mult, int flat, int veteran);

/* Orig Combat_ResolveAttack @ 1050:0000 preview (param_5 == 0) ->
 * combat_preview_score. 1:1 (local_10<<3)/(local_12+1) on the effective
 * strengths above. Returns score >= 0, -1 on either < 0 or def == -1
 * (div-by-zero guard; orig def >= 0 so def+1 >= 1). No globals. */
int combat_preview_score(int atk_eff, int def_eff);

/* Orig Unit_IterateStack @ 11c0:1ee1 count shape -> unit_count_stack.
 * 1:1 walk of the 0x6306 next chain: count start + follows until 0xff,
 * loop-back to start, OOR, or >9 extra steps (orig local_8 > 9 cap, i.e.
 * at most 10 nodes past start? this port counts start + up to 10 follows,
 * matching the cap). next[i] is 0xff none or 0..n-1. Returns count >= 1,
 * -1 on NULL, n <= 0, or start OOR. No globals, read-only. */
int unit_count_stack(const uint8_t *next, int n, int start);

/* Orig Unit_LinkStack @ 1050:1105 insert -> unit_stack_insert.
 * 1:1 link fresh after head: next[fresh] = next[head], next[head] = fresh
 * (head already linked or 0xff keeps fresh tail 0xff). Returns CIV_OK,
 * NOENT on NULL, RANGE on n <= 0 or head/fresh OOR or head == fresh.
 * Exact orig param order (civ/new/head) TBD vs decompiler; this port takes
 * the chain explicitly. No globals, no exit(). */
civ_error_t unit_stack_insert(uint8_t *next, int n, int head, int fresh);

/* Orig Unit_FindAtTile @ 11c8:01ab nearest (sibling of City_FindNearest @
 * 11c8:0124) -> unit_find_nearest_at_tile. 1:1 argmin Map_WrapStepDist @
 * 11c8:02ec (map_wrap_step_dist) over i in 0..n-1 with valids[i] != -1
 * (NULL valids = all live) && i != exclude, DAT_1420_b386 = winning dist.
 * dist_out may be NULL (else -1 when none). Returns unit index, -1 when
 * none/NULL/n <= 0. No globals, read-only. */
int unit_find_nearest_at_tile(const int *xs, const int *ys,
                               const int *valids, int n, int exclude,
                               int x, int y, int *dist_out);

/* Orig Unit_SpawnUnit @ 11c0:12c8 alloc/init -> unit_spawn_unit.
 * 1:1 on the caller-owned per-civ slice (0x80 x 0xc, see header top):
 * first idx with rec[TYPE] == 0xff else -1 (orig past 0x7e shows FIRSTUNIT
 * once + -1; message tail stays Win16). Inits rec[X]=x, rec[Y]=y,
 * rec[TYPE]=type, rec[MOVES]=0, rec[GOAL_X]=rec[GOAL_Y]=rec[LAST_DIR]=0xff,
 * rec[NEXT]=0xff, rec[SEEN]=(uint8_t)seen_bit, rec[HOME]=(uint8_t)home
 * (caller passes City_FindNearest result or 0xff; foreign-owner 0xff rule
 * stays in caller), rec[TIMER]=table[type] (0 when table NULL/OOR) minus 1
 * when nonzero (orig -1 once). Flags (+11) untouched (orig leaves them;
 * City_SpotReveal ORs 8/0x20 after). Returns slot 0..n-1, -1 on NULL,
 * n <= 0, type < 0, x/y < 0, or full. No globals, no exit(). */
int unit_spawn_unit(uint8_t *units, int n_units, int type, int x, int y,
                    int home, const uint8_t *timer_table, int timer_n,
                    int seen_bit);

/* Orig Unit_QueueOrder @ 1078:455c dup gate -> unit_queue_is_duplicate.
 * 1:1: nonzero (1) when any i has qx[i]==x && qy[i]==y && qk[i]==kind &&
 * (uint8_t)qp[i] <= (uint8_t)prio (stored prio at/below new -> drop).
 * qx/qy/qk/qp are CIV_UNIT_QUEUE_LEN parallel arrays (0x612/0x512/0x412/
 * 0x312 bases); kind 1..4 contact orders, prio 1..4 (exact semantics TBD).
 * NULL arrays or n <= 0 -> 0. No globals, read-only. */
int unit_queue_is_duplicate(const uint8_t *qx, const uint8_t *qy,
                             const uint8_t *qk, const uint8_t *qp, int n,
                             int x, int y, int kind, int prio);

/* Orig Unit_QueueOrder @ 1078:455c slot pick -> unit_queue_pick_slot.
 * 1:1 first i with kinds[i] == 0xff (-1 empty) or (int8_t)prios[i] < prio
 * (lower prio evicted; Unit_QueueShift @ 1078:4837 makes room first).
 * kinds/prios are int8_t images (0xff empty, else kind/prio bytes).
 * Returns slot 0..n-1, -1 when full/NULL/n <= 0. No globals. */
int unit_queue_pick_slot(const int8_t *kinds, const int8_t *prios, int n,
                         int prio);

/* Orig Unit_QueueShift @ 1078:4837 make-room -> unit_queue_shift.
 * 1:1 shift rows [at..n-2] to [at+1..n-1] (orig loops 0x1e..at down when
 * qp[row] != 0) on the explicit parallel arrays; frees [at] for the new
 * entry (caller stores x/y/kind/prio after). Returns CIV_OK, NOENT on
 * NULL, RANGE on n <= 0 or at OOR. No globals, no exit(). */
civ_error_t unit_queue_shift(uint8_t *qx, uint8_t *qy, uint8_t *qk,
                             uint8_t *qp, int n, int at);

/* --- Chain 2026-10-16 (5): combat rolls/promotion/difficulty + stack walk/
 * defender pick + draw-layer pick + adjacent tally (task goals 1/2/4/5) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   Combat_ResolveAttack @ 1050:0000 (full flow decoded: effective atk/def
 *     kernels above, then preview (param_5 == 0) else real rolls via
 *     Rng_Range @ 11c8:003d + city-holder reroll, animation/sound/Gfx tails
 *     via Unit_EnsureOnScreen @ 11c0:231d + Map_RefreshTileView @ 1110:0448
 *     + Sound_PlayId @ 1150:00c9 + Gfx_DrawTechIcon @ 11a8:0cb1 + GfxPort
 *     blits @ 10d0:0854/0956, diplo tails via Combat_CheckPeaceBlock @
 *     1050:12be + Diplo_SetWar @ 1168:0e8e, outcomes: diplomat 0x19 ->
 *     Combat_ScoreAdjacent @ 1050:13af, defender wins -> 396a++ +
 *     Unit_FoundCityIfHome @ 11c0:16a4 + veteran Rng_Range(2), attacker
 *     wins -> pillage/settler-capture/city-holder reroll + 396a += CalcOdds
 *     + IterateStack + message tails, both -> veteran Rng_Range(2)).
 *     Caller: Unit_ChooseOrder @ 1078:1480 (sole xref). OS-free roll/difficulty
 *     kernels below are LIVE 1:1 on explicit params (rolls via rng_range in
 *     sys.c, no globals); animation/sound/dialog/city-state tails stay
 *     CIV_ERR_UNSUPPORTED with TODO Orig in combat_resolve_attack.
 *   Combat_CheckPeaceBlock @ 1050:12be (peace-bit gate vs player: no peace
 *     bit -> 0; else message + 1/0xffff by city count; backs ResolveAttack).
 *   Unit_IterateStack @ 11c0:1ee1 (0x6306 next-chain walk: callback per node,
 *     stop on 0xff/-1, loop-back to start, or >9 extra steps; backs
 *     Combat_CalcOdds @ 11c0:1bc3, Unit_CountDefenders @ 11c0:1d70,
 *     Unit_UpdateVisibility @ 11c0:0395 goal flips, ResolveAttack tails).
 *     Structure: singly-linked next chain (next = +9/0x6306 byte, 0xff none);
 *     NOT next-prev double links (no prev field in the 0xc record); max ~11
 *     nodes visited (start + up to 10 follows, local_8 > 9 cap).
 *   Unit_PickStackTop @ 11c0:1a2c (top resolve: when next[start] != -1, seed
 *     DAT_1420_b380 = start / DAT_1420_b37e = 0xffff then IterateStack with
 *     Unit_ScoreStackDefender @ 11c0:1a81, result = b380; backs
 *     Unit_DrawStacked @ 1110:1516).
 *   Unit_ScoreStackDefender @ 11c0:1a81 (defender score per node: sea tile
 *     (Map_GetTileClass @ 1110:16c0 == 10) skips unless domain @ 1a3a == 2;
 *     land domain 0: def*8*terrain*(fort 0x08 ? 3 : 2); sea/air else:
 *     def<<4; vet 0x20 -> += /2; argmax strict > into b380/b37e).
 *   Combat_ScoreStackCallback @ 11c0:1bfd (CalcOdds per-node tally: mode
 *     b37a 0 = += cost @ 1a44, 1 = += def @ 1a42, 2 = count++, 3 = += atk @
 *     1a40, 4 = count++ when idx < b378 && domain @ 1a4a == 2).
 *   Unit_LinkStack @ 1050:1105 (chain insert for Map_PlaceUnitAt @ 1110:1767;
 *     alone dst (next == -1) forms a 2-cycle dst<->fresh, else splices fresh
 *     after head; unit_stack_insert above keeps the 0xff-tail single-link
 *     form used by Map_PlaceUnitAt fresh place, circular pair stays TODO).
 *   Unit_DrawStacked @ 1110:1516 (stack pick: land/single -> PickStackTop;
 *     sea tile + stack: head domain == 2 -> PickStackTop, else first
 *     domain-2 node in chain, else head; then Unit_DrawSingle @ 1110:0dba).
 *   Unit_DrawSingle @ 1110:0dba (single blit: view clip via Gfx_ClipTileRect
 *     @ 1208:2061 (was FUN_1208_2061: SETRECT tile 0x20 + clip call; 0 =
 *     off-screen skip); sea tile + occupant flag &1 + domain != 2 -> hidden;
 *     else stacked badge via Gfx_DrawTechIcon @ 11a8:0cb1 (frame
 *     (civ*0x20+type)*2-0x6c30) + Unit_DrawStatusBadge @ 1110:0ff6 (was
 *     FUN_1110_0ff6: civ-color flag icon type*0x20+civ+0x40, fort 0x1d /
 *     move-goal markers, 0xc2 status letters with civ colors, frame rect)).
 *     What is drawn: unit type icon + civ flag color + stacked badge when
 *     next != -1 + status letters (fort/sentry/move/visibility); HP is NOT
 *     drawn (no HP field in the 0xc record: combat is one-roll kill, veteran
 *     flag 0x20 is the only persistent combat state). Blit via GfxPort
 *     (SELECTPALETTE/BITBLT) -> SDL TBD (see video.h video_draw_unit_sprite
 *     STUB). Enum helpers below are LIVE 1:1 (type/frame/pos pick, no
 *     Win16); blit stays STUB.
 *   Combat_ScoreAdjacent @ 1050:13af (diplomat 0x19 area tally, called from
 *     ResolveAttack when atk type 0x19: message unless player, pillage via
 *     11b8:0c3d, then 8-neighbour loop: Map_FindUnitAt @ 1110:180e +
 *     Map_GetOccupancyFlags @ 1110:199b city 0x10000 gate (city message +
 *     skip), else Combat_CalcOdds(owner,unit,2) += into 396a power table +
 *     IterateStack count + owner != atk -> diplo |= 0x88; tail 1050:158a.
 *     Purpose: diplomat-attack threat assess for the AI (accumulates the
 *     neighbouring stacks' power into the 396a table Unit_ChooseOrder @
 *     1078:1480 reads for bribe-target scoring, and marks contact bits).
 *     Neighbour + accum helpers below are LIVE 1:1; city/dialog tails stay
 *     STUB.
 *   Unit_QueueShift @ 1078:4837 (make-room shift, unit_queue_shift LIVE
 *     already: rows 0x1e..slot shift +1 when prio != 0; backs Unit_QueueOrder
 *     @ 1078:455c slot store).
 * OS-free kernels below are LIVE 1:1 on explicit params (no globals, no
 * Win16, read-only except the walker ctx); Win16/GDI/dialog tails stay
 * CIV_ERR_UNSUPPORTED with TODO Orig refs. */

/* Orig Combat_ResolveAttack @ 1050:0000 real-fight rolls -> combat_roll_winner.
 * 1:1: atk_tentative = (def_roll < atk_roll); when city_holder != 0 and the
 * attacker tentatively wins, reroll once: defender wins when atk_roll2 <
 * def_roll2 (city-holder second chance). Rolls are Rng_Range @ 11c8:003d
 * draws (rng_range in sys.c); caller passes atk in [0,atk_eff) style values.
 * Returns 1 attacker wins, 0 defender wins, -1 on atk_eff/def_eff < 0. No
 * globals, no exit(). */
int combat_roll_winner(int atk_eff, int def_eff, unsigned atk_roll,
                       unsigned def_roll, unsigned atk_roll2,
                       unsigned def_roll2, int city_holder);

/* Orig Combat_ResolveAttack @ 1050:0000 veteran tails (both outcomes via
 * Rng_Range(2) @ 11c8:003d: defender-wins promotes defender, attacker-wins
 * promotes attacker) -> combat_should_promote. 1:1: nonzero (1) when
 * rand2 != 0 (unit gets flag 0x20 @ 62fc). Takes the Rng_Range(2) draw
 * explicitly (rng_range(2) in sys.c). No globals. */
int combat_should_promote(unsigned rand2);

/* Orig Combat_ResolveAttack @ 1050:0000 difficulty scaling (two sites) ->
 * combat_difficulty_scale. 1:1: when real_fight == 0 (odds-setup site,
 * atk civ == 0 only): player defends -> atk = atk*(diff+1)/4 else atk /= 2.
 * When real_fight != 0 (post-roll site, atk civ != 0 only): diff < 2 and
 * player == def -> atk /= 2; diff == 0 and player == atk -> atk *= 2. Other
 * civ combos pass through. Returns scaled atk (>= 0), -1 on atk < 0 or
 * difficulty outside 0..4. No globals. */
int combat_difficulty_scale(int atk, int atk_civ, int def_civ, int player_civ,
                            int difficulty, int real_fight);

/* Orig Unit_IterateStack @ 11c0:1ee1 generic walk -> unit_iterate_stack.
 * 1:1 on the explicit next[] chain (0xff/-1 none, else 0..n-1): visits start
 * then follows until 0xff/-1, OOR, loop-back to start, or >9 extra steps
 * (local_8 > 9 cap, i.e. at most 10 nodes: start + 9 follows, same shape
 * as unit_count_stack above). cb(idx, ctx) runs per visited node (may be
 * NULL = count only); nonzero cb return stops early. Returns visits >= 0
 * (0 when cb stops at once, else like unit_count_stack), -1 on NULL next,
 * n <= 0, or start OOR. No globals, no exit(). */
typedef int (*unit_stack_visit_fn)(int idx, void *ctx);
int unit_iterate_stack(const uint8_t *next, int n, int start,
                       unit_stack_visit_fn cb, void *ctx);

/* Orig Unit_ScoreStackDefender @ 11c0:1a81 per-node score ->
 * unit_stack_defense_score. 1:1: sea tile (tile_is_sea != 0) with non-sea
 * domain (domain_is_sea == 0) -> -1 skip; land domain (!domain_is_sea):
 * base*8*terrain*(fortified ? 3 : 2); sea/air domain: base<<4; vet 0x20
 * (veteran != 0) -> += /2. base is 1420:1a42[type], terrain is 1420:17d5
 * tile entry (caller reads tables; exact bytes TBD). Returns score >= 0,
 * -1 on skip or base/terrain < 0 or bad flags. No globals. */
int unit_stack_defense_score(int base_def, int terrain_mult, int fortified,
                             int veteran, int tile_is_sea,
                             int domain_is_sea);

/* Orig Unit_PickStackTop @ 11c0:1a2c argmax (DAT_1420_b37e strict <) ->
 * unit_stack_best. 1:1 index of the maximum score (ties break to the lower
 * index, like the orig strict-< loop; seed b37e = 0xffff so negatives never
 * win: all-negative -> -1). scores is n entries (read-only). Returns index
 * or -1 on NULL/n <= 0/none. No globals. */
int unit_stack_best(const int *scores, int n);

/* Orig Unit_DrawStacked @ 1110:1516 pick -> unit_stack_draw_pick. 1:1 which
 * chain index to draw: land tile (!tile_is_sea) or no stack (!has_stack) ->
 * head (caller runs PickStackTop scoring first via unit_stack_best); sea
 * tile + stack: head sea-domain (head_domain_is_sea) -> head (PickStackTop);
 * else first i in domains[] with sea domain, else head. domains is the chain
 * order head-first, n entries of 0/1 (sea domain?). Returns the picked index
 * 0..n-1, -1 on NULL/empty/head OOR. No globals. */
int unit_stack_draw_pick(int tile_is_sea, int head_domain_is_sea,
                         const int *domains, int n, int head, int has_stack);

/* Orig Unit_DrawSingle @ 1110:0dba visibility/badge layer ->
 * unit_draw_single_layer. 1:1: clipped (off-screen via Gfx_ClipTileRect @
 * 1208:2061) -> 0 hidden; sea tile + occupant flag (flag & 1, land-unit on
 * ship?) with non-sea domain -> 0 hidden; else 1 visible, with *badge_out =
 * has_stack (stacked badge via Gfx_DrawTechIcon when next != -1; badge_out
 * may be NULL). Returns 1 draw / 0 skip. No globals. */
int unit_draw_single_layer(int clipped, int tile_is_sea, int occupant_flag,
                           int domain_is_sea, int has_stack, int *badge_out);

/* Orig Combat_ScoreAdjacent @ 1050:13af accum -> combat_adjacent_add. 1:1
 * power-table add: score + odds (Combat_CalcOdds @ 11c0:1bc3 mode-2 tally per
 * adjacent enemy stack into 396a). Negative odds clamp to 0 (orig tallies
 * are non-negative counts). No globals. */
int combat_adjacent_add(int score, int odds);

/* Orig Combat_ScoreAdjacent @ 1050:13af contact mark -> combat_adjacent_flag.
 * 1:1 diplo mark when the adjacent stack owner differs from the attacker:
 * owner != atk -> 0x88 else 0x00 (ORed into 3a0a by the caller). No globals. */
int combat_adjacent_diplo_flag(int owner, int atk);

/* Orig Diplo_NearestPalaceDist @ 1168:12c1 -> diplo_nearest_palace_dist.
 * 1:1 min Map_WrapStepDist @ 11c8:02ec (map_wrap_step_dist, see map.h) over
 * palace cities (city_palace[i] != 0, i.e. 1420:54fc & 1) owned by civ,
 * seed 0x10. city_xs/ys/owners/palace are parallel n_cities arrays
 * (read-only; caller filters valid 5502 != -1). Feeds diplomat_revolt_cost
 * + diplomat_bribe_cost dist params. Returns dist (0x10 when none/NULL).
 * No globals, no exit(). */
int diplo_nearest_palace_dist(int civ, const int *city_xs,
                              const int *city_ys, const int *city_owners,
                              const int *city_palace, int n_cities, int x,
                              int y);

/* --- Chain 2026-10-16 (6): peace-block gate + stacked sea-search (goals 4/5) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Combat_CheckPeaceBlock @ 1050:12be (peace-bit gate vs player, caller
 *     Combat_ResolveAttack @ 1050:0000 real-fight site + Diplomat_CityAction @
 *     11b8:0000: no peace bit (player-vs-target & 2 == 0) -> 0 free attack;
 *     else Dialog_ShowMessage @ 1220:00d2 Yes/No (0x50,0x50 via 24fa/12d8 +
 *     0xa24/0xa4c strings); choice != 1 -> 0xffff treaty-break allowed; choice
 *     == 1 and government @ 3b77+0x13 < 4 (despotism/monarchy) -> 1 senate
 *     block; else second message (0xa6b) + 0xffff allowed. Returns int, not
 *     bool: 0 free, 1 blocked, 0xffff allowed-after-warning).
 *   Unit_ScoreStackDefender @ 11c0:1a81 (per-node defender score, LIVE as
 *     unit_stack_defense_score above; invoked indirectly via Unit_IterateStack
 *     @ 11c0:1ee1 callback from Unit_PickStackTop @ 11c0:1a2c, hence Ghidra
 *     shows no direct callers: PickStackTop seeds b380/b37e then IterateStack
 *     with the 1a81 callback; other IterateStack users: Combat_CalcOdds @
 *     11c0:1bc3, Unit_CountDefenders @ 11c0:1d70, Unit_UpdateVisibility @
 *     11c0:0395 goal flips, Combat_ResolveAttack @ 1050:0000 tails).
 *   Unit_DrawStacked @ 1110:1516 layer order (decomposed, terrain comes from
 *     Map_RefreshTileView @ 1110:0448 underneath, not drawn here): 1 terrain
 *     (map tile, Map_GetTileClass @ 1110:16c0 sea == 10 gate), 2 city/palace
 *     underlay (occupancy, not a sprite), 3 unit type icon via Gfx_DrawTechIcon
 *     @ 11a8:0cb1 (frame (civ*0x20+type), see video_unit_frame_index), 4 status
 *     badge via Unit_DrawStatusBadge @ 1110:0ff6 (flag icon type*0x20+civ+0x40
 *     via Unit_DrawFlagIcon @ 11c8:008a + fort/move letters; HP NOT drawn:
 *     one-roll kill, veteran 0x20 is the only combat state). Sea tile + stack
 *     with land head searches the chain for the first sea-domain node first
 *     (unit_stack_draw_pick LIVE); clip via Gfx_ClipTileRect @ 1208:2061
 *     (video_unit_tile_box + video_clip_rect LIVE, off-screen skips).
 * OS-free gate/branch math below is LIVE 1:1 (no globals, no Win16);
 * dialog/GfxPort blit tails stay STUB with TODO Orig. */

/* Orig Combat_CheckPeaceBlock @ 1050:12be gate -> combat_check_peace_block.
 * 1:1 on explicit params (no globals): peace_flags is the player-vs-target
 * diplo word @ 12d8:3a0a (bit 0x02 peace); gov is the government id @
 * 3b77+0x13 (0 anarchy .. 5 communism); choice is the Dialog_ShowMessage @
 * 1220:00d2 result (1 = Yes insist). Returns 0 free attack (no peace bit, or
 * peace + choice != 1 path falls through to 0xffff? no: orig returns 0xffff
 * allowed-after-warning, see below), 1 senate-blocked (peace + choice == 1 +
 * gov < 4), 0xffff allowed-after-warning (peace + (choice != 1 or gov >= 4)).
 * As int: 0, 1, 0xffff (65535). No globals, no exit(). */
int combat_check_peace_block(int peace_flags, int gov, int choice);

/* Orig Unit_DrawStacked @ 1110:1516 sea-search branch ->
 * unit_stack_draw_needs_sea_search. 1:1 when the stacked sea-tile path must
 * scan the 0x6306 chain for the first sea-domain (1a3a == 2) node: tile_is_sea
 * && has_stack && !head_domain_is_sea -> 1 search, else 0 (land/single or sea
 * head goes straight to Unit_PickStackTop @ 11c0:1a2c scoring via
 * unit_stack_best). Complements unit_stack_draw_pick (which returns the picked
 * index). No globals. */
int unit_stack_draw_needs_sea_search(int tile_is_sea, int has_stack,
                                     int head_domain_is_sea);

/* --- Chain 2026-10-16 (7): census flag-icon views (goal 4) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   Census_ShowTechBars @ 1018:03f0 (tech-progress bars: Res 0x8a/5 title via
 *     City_OpenCensusView @ 1018:0000 + bar width via Util_Clamp(0x980/
 *     (val+1),1,0x40) segments drawn with Unit_DrawFlagIcon @ 11c8:008a id
 *     0x0c per segment + tech list Res 0x82 with owner color; close via
 *     City_CloseCensusView @ 1018:0380).
 *   Census_ShowUnitList @ 1018:067b (per-unit rows 0x1b entries: counts at
 *     4121+0xd/3f5c+0x12 per civ*0x38 + Unit_DrawFlagIcon civ*0x20+unit+0x40
 *     via unit_flag_icon_index (see video.h) + Res 0x84 names + atk/def/moves
 *     at 1a40/1a42/1a3c; chained from Census_ShowTechBars tail).
 *   Census_ShowTerrainCounts @ 1018:09cc (per-terrain rows 0x1b: any-civ
 *     nonzero at 2b90 per 0x38 + same flag icons + Res 0x84 names + per-civ
 *     counts; closes the census chain).
 * All three are Win16 dialog/GDI (ResMgr strings + GfxPort blits + Sys
 * wait) -> SDL TBD; STUBs below. Flag-index math lives in
 * unit_flag_icon_index (video.h, LIVE). No globals, no exit(). */

/* Orig Census_ShowTechBars @ 1018:03f0 -> census_show_tech_bars.
 * Tech-progress bars + tech list. Win16 dialog/GDI -> SDL TBD. */
civ_error_t census_show_tech_bars(int civ);

/* Orig Census_ShowUnitList @ 1018:067b -> census_show_unit_list.
 * Per-unit census rows with flag icons. Win16 -> SDL TBD. */
civ_error_t census_show_unit_list(int civ);

/* Orig Census_ShowTerrainCounts @ 1018:09cc -> census_show_terrain_counts.
 * Per-terrain census rows with flag icons. Win16 -> SDL TBD. */
civ_error_t census_show_terrain_counts(int civ);

#endif /* CIVLIN_GAME_H */
