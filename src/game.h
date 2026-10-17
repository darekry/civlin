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
 * tail -> game_new. Entry to the turn loop (WinMain @ 1008:0304 shape:
 * while(!g_nQuitFlag @ 12d8:24ee) Game_TurnLoopTick @ 1088:0000; TIMERPROC
 * @ 1008:0d68 only ticks the 24f0 countdown, see boot.h chain-21).
 * PARTIAL: OS-free entry defaults below are LIVE 1:1 on the caller-owned
 * civ_game_t (wizard-entry state before the modal steps: difficulty 0,
 * civ_count 6, alive_mask 0xff, player_civ 0, year 0xF060/-4000, turn 0,
 * quit 0; menu_choice preserved for the WorldGen_PrepareEarth @ 1108:01bc
 * dispatch); wizard modal tails (Dialog_MenuZ_Modal @ 1220:0156 steps 0..2
 * + custom variant @ 1220:16fd steps 3/4, Ctrl_* @ 1058, Res blob 0x94 via
 * 11f8:0080, Sound_PlayId @ 1150:00c9) + map tails (Map_GenerateRandomOrLoad
 * @ 1028:0000 random/preset via WorldGen_CopyPreset @ 10b8:079d 64000B +
 * Map_FillRect @ 1028:1d15 / Map_LabelContinents @ 1028:0a93, map buffer
 * owned via map.h) stay CIV_ERR_UNSUPPORTED with TODO Orig in game.c.
 * Owned-module helpers (newgame_wizard_run/newgame_finalize, worldgen_*
 * in newgame.h/worldgen.h, parallel-owned) are documented, not linked, so
 * the newgame/worldgen rewrites stay decoupled: game-side twins below keep
 * the same 1:1 math without cross-module calls. Ref unpacked/civ.exe
 * (832512 B EDI LZSS2 unpack from civ1win/CIV.EXE, 335182 B packed)
 * read-only; civ1win/ untouched. NULL -> NOENT, bad entry mask ->
 * RANGE. No globals, no exit(). */
civ_error_t game_new(civ_game_t *g);
/* OS-free new-game entry gates (Orig NewGame_Wizard @ 10e8:0000 finalize +
 * WorldGen_PrepareEarth @ 1108:01bc -> Map_GenerateRandomOrLoad @ 1028:0000;
 * ref unpacked/civ.exe read-only). Twins of the owned-module helpers
 * (newgame_* in newgame.h, worldgen_* in worldgen.h, parallel-owned) so
 * src/game.* stays decoupled: same 1:1 math, game_new_* names, no cross
 * includes. No Win16, no globals, no exit(), read-only. */
int game_new_difficulty_ok(int difficulty);
int game_new_civ_count_ok(int civ_count);
int game_new_alive_mask(int civ_count);
unsigned game_new_start_year_raw(void);
int game_new_start_year_signed(void);
int game_new_player_civ_ok(int player_civ);
int game_new_map_param(int menu_choice);
/* Orig WinMain @ 1008:0304 loop body: while(!g_nQuitFlag @ 12d8:24ee)
 * Game_TurnLoopTick @ 1088:0000 (direct calls, NOT a timer callback:
 * TIMERPROC @ 1008:0d68 only ticks the 24f0 countdown, see boot.h
 * chain-21; the 200 ms map timer is a NULL-proc WM_TIMER, see map.h
 * map_timer_interval_ms; KILLTIMER first in WinMain_Shutdown @ 1008:0329).
 * Tick -> game_turn_tick @ 1088:0000 linkage: per-frame pump
 * here (Sys_PumpMessages @ 1108:0000 + Sys_Yield @ 1098:0157, see sys.h;
 * both live SDL_PumpEvents) then one Game_TurnLoopTick pass (stub tail).
 * Partial: pump live, turn TBD -> overall CIV_ERR_UNSUPPORTED; NULL ->
 * NOENT, quit_flag set -> CIV_OK (no work). No globals, no exit(). */
civ_error_t game_tick(civ_game_t *g);

/* One Game_TurnLoopTick @ 1088:0000 pass (P3 skeleton, decompiled
 * 2026-10-16; callees verified via get_function_callees: Diplo_ProcessTurn @
 * 1168:0000, FUN_1078_0000/448f, FUN_1158_0000, FUN_1170_0000,
 * Civ_ProcessUnitMoves @ 11f0:0000, Sys_CheckRuntimeMemory @ 11e8:0075,
 * Game_SpawnUprisings @ 1088:0be9, Game_DoPollutionAndWonderRace @ 1088:1e32,
 * City_ProcessDisaster @ 1040:068e (2x, g_nTurn > 0x32), Game_UpdatePowerRankings
 * @ 1088:108a, Game_CalcProgressThreshold @ 1088:1d99, Status_FormatMainRow @
 * 1138:0000, Game_UpdateStatusPanel @ 1088:13d5, Game_ProcessGameEnd @
 * 1088:0b6d, Quiz_RunQuizPair @ 1178:0000, Map_RefreshAllViews @ 1110:03e9,
 * Replay_AppendLog @ 11c0:3beb, Save_SaveGame @ 1198:00a3; sole caller WinMain @
 * 1008:0000):
 *   per-civ loop (alive mask 12d8:42f8): Diplo + 1078 pair + 1158/1170 +
 *     Sys_CheckRuntimeMemory (per-civ, przed Civ_ProcessUnitMoves) + Civ moves
 *     (1 = needs UI pump -> early return); potem SpawnUprisings + Pollution/
 *     WonderRace + 2x Disaster (turn > 0x32) + PowerRankings + memory-warning
 *     branch (treasury < 100) + turn++/year-step + 0-year doubling + 0x32
 *     scoring branch (Quiz/Map_RefreshAllViews, Save/RefreshViews gdy
 *     option&2 i year != diff*-0x14+0x834, Replay log 0xb) + endings via HoF.
 * Live skeleton on caller-owned civ_game_t: turn++ + year-step + score/uprising
 * predicates below; per-civ/diplo/map/dialog/quiz/save/HoF tails stay STUB via
 * the TODO in game.c (PARTIAL). */
civ_error_t game_turn_tick(civ_game_t *g);

/* Orig Game_TurnLoopTick @ 1088:0000 year-step tail -> game_turn_year_step.
 * OS-free 1:1 (implemented): era ladder gated on (opt_flags & 0xfe) == 0
 * (DAT_12d0_00b6 quiz/launch bits; nonzero = frozen year): year < 1000 -> +20
 * (0x14); else year < 0x5dc (1500) -> +10; else year < 0x6d6 (1752) -> +5;
 * else year < 0x73a (1852) -> +2; else +1. Follow-ups caller-side (orig
 * inline): year == 0 -> double treasuries + year = 1; year == 0x15 (21) ->
 * year = 0x14 (20). Takes year/opt_flags explicitly; no globals. */
int game_turn_year_step(int year, int opt_flags);

/* Orig same 0x32 scoring branch -> game_turn_should_score. OS-free 1:1
 * (implemented): (turn % 0x32 == 0) && player_civ != 0xffff. Quiz sub-gate
 * ((opt & 1) == 0 && year < 0) stays caller-side. No globals. */
int game_turn_should_score(int turn, int player_civ);

/* Orig Game_SpawnUprisings @ 1088:0be9 head gate -> game_uprising_due.
 * OS-free 1:1 (implemented): limit = (5 - difficulty) * 0x1e; due (1) when
 * limit - turn == 0 || limit < turn, else 0. Tile gates (Map_GetTileClass @
 * 1110:16c0 == 10 ocean, Map_GetUnitAt @ 1110:18ce == -1, Map_GetOceanOrContVal
 * >= 0x10) + Rng_Range @ 11c8:003d draws stay caller-side. No globals. */
int game_uprising_due(int difficulty, int turn);

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

/* Orig Game_UpdateStatusPanel @ 1088:13d5 (body 1088:13d5-1a81) ->
 * game_update_status_panel. PARTIAL dispatcher: OS-free
 * milestone/pollution/progress gates are LIVE 1:1 on explicit params (no
 * globals, no Win16, read-only; game_status_progress_index @ 1088:1554,
 * game_status_pollution_icon/count (42f0%4 + Clamp(42f0/4,0,3)),
 * game_pollution_spread_due/alarm_due, game_status_milestone_due with the
 * "00,000 citizens." @ 1420:172b tail; pop/year/tech-icon helpers
 * city_calc_pop_total/city_format_pop_count/game_append_year_text/
 * status_draw_tech_icons/gfx_tech_icon_valid/status_tech_count_ok/
 * height_ok/status_lcg_pick/next wired in game.c, results ignored until
 * city records + GDI land). GDI/dialog tails (Gfx_GetCurrentDC +
 * GfxPort_SetActive @ 10d0:0956 + Text_DispatchFontHandler +
 * Text_GetFontHeight @ 1098:0dda + Text_SetColor @ 1098:0e0b + frames via
 * WdwStatus_CalcUpperRect @ 1218:0956 / CalcLowerRect @ 1218:0a06 +
 * Status_DrawPanelFrame @ 11f8:0278 + TEXTOUT x3 + Gfx_DrawTechIcon @
 * 11a8:0cb1 + GfxPort_PresentRect @ 10d0:0854 + Dialog_ShowMessage @
 * 1220:00d2 milestone popup) stay CIV_ERR_UNSUPPORTED with TODO Orig in
 * game.c (no fake-OK). Ref unpacked/civ.exe 832512 B read-only. */
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

/* Orig: FUN_11f0_4ea0 @ 11f0:4ea0 -> sys_drain_input (PARTIAL).
 * Clears 1420:1d44..1d50 + 12b0:004c and drains PEEKMESSAGE queues
 * (0x108/0x100, 0x209/0x200). Called from Civ_ProcessUnitMoves
 * (@ 11f0:06d2/0c58), menu/button/turn paths (28 callers) and the
 * 11c0:0395 visibility tail. PARTIAL: OS-free range gates
 * (sys_drain_state_words/queue_cap/reset/peek_key/mouse, all LIVE below)
 * + SDL_PollEvent drain (bez HWND, via sys_drain_sdl_should_drop in sys.h)
 * is LIVE; HWND routing (main vs status vs smmap vs popup DAT_1420_9948)
 * + DGROUP stores (1420:1d44..1d50 + 12b0:004c) stay CIV_ERR_UNSUPPORTED
 * with TODO Orig in game.c (no fake-OK). */
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

/* --- Chain 2026-10-16 (15): Unit_ShowInfoPopup layout (goal 3) ---
 * Orig Unit_ShowInfoPopup @ 11f0:468c (civ.exe, x86:LE:16:Protected
 * Mode), decompiled this pass (body 11f0:468c-4e25; note: 11f0:4a76 is
 * INSIDE this body, not a separate City_AppendName — the true
 * City_AppendName is @ 1038:0bba, callee below; the @4a76 label in the
 * task is the city-line site inside the popup, kept as a code offset,
 * not a function).
 * Full layout (paint only, no wait loop inside):
 *   frame: Gfx_GetCurrentDC + GfxPort_SetActive + color DAT_12d8_45fb
 *     (or DAT_12d8_45ee when DAT_12d8_28fc mono != 0) + WdwStatus_
 *     CalcPanelRect @ 1218:0abf + Status_DrawPanelFrame @ 11f8:0278
 *     (FILLRECT via Gfx_MakeBrush @ 1098:00b5 + white/black border, see
 *     game.h status_draw_tech_icons neighbours) + limit iVar4 =
 *     (local_1a - local_1e) - 8 = unit_popup_text_limit LIVE (see
 *     dialog.h) + Text_DispatchFontHandler(1) (slot DAT_12d8_0024 via
 *     Text_SelectFont @ 1100:0a33; first paint, e.g. here) +
 *     SELECTOBJECT font slot + Text_GetFontHeight @ 1098:0dda line
 *     stride iVar5.
 *   branch param_1 == -1: empty (frame only, no lines).
 *   branch param_2 == 0x80 (end-of-turn card): 3 centered lines via
 *     Text_DrawDirect @ 10d8:00e0 at (local_1e+0xe, local_1c+0xe + k*h):
 *     "End of Turn" @ 1420:47dd + "Press Return" @ 1420:47e9 +
 *     "to continue" @ 1420:47f6 (0xe indent, DAT_13f0_0004/0006 pos).
 *   else (normal unit card, x0 = local_1e+4, y steps by h):
 *     1. unit name: Util_StrCopy @ 1000:36b2 (0x3da+civ*0x20) +
 *        Text_FitPascalEllipsis @ 1098:0000 (limit above, site @ 11f0:4808)
 *        + Text_DrawDirect.
 *     2. unit type: Res_GetStringCopy @ 11f8:0080 (0x84, type @ 62ff) +
 *        DrawDirect.
 *     3. opt "Veteran" @ 1420:4803 when flags @ 62fc & 0x20
 *        (unit_popup_has_veteran LIVE).
 *     4. "Moves " @ 1420:480b + dec(Util_Clamp(moves/3,0,99)) + opt
 *        fraction ("..." @ 0x4813 + dec(rem) when moves%3 != 0; moves @
 *        6300, full = moves/3 clamped, frac = moves%3, both LIVE) + opt
 *        build tail ("..." @ 0x4815 + dec(full+rate) + "..." @ 0x4817 when
 *        *(type*0x16+0x1a3e) != 0, rate = *(6301) * 1420:1a3c[type*0xb]).
 *     5. city line: City_AppendName @ 1038:0bba (home @ 6307; -1 appends
 *        0x795, else Util_StrLen + copy from 0x4da+...) + Fit (site @
 *        11f0:4a76 = second Fit, same limit) + DrawDirect.
 *     6. terrain: copy 0x4819 + Res_GetStringCopy(0x85, Map_GetTileClass
 *        @ 1110:16c0 (x @ 62fd, y @ 62fe)) + append 0x481b + DrawDirect.
 *     7. opt occupancy (Map_GetOccupancyFlags @ 1110:199b bits):
 *        0x10 Railroad @ 1420:481d else 0x08 river? @ 0x4828; 0x02
 *        Irrigation @ 1420:482f else 0x04 Mining @ 1420:483c; 0x40
 *        Pollution @ 1420:4845 (each DrawDirect + h step).
 *     8. badges: Unit_DrawStatusBadge @ 1110:0ff6 per stack link
 *        (next @ 6306 chain, x += 0x20, wrap to left+2 / y += 0x20 when
 *        panel_right-0x24 <= x, LIVE wrap predicate below) + city-guard
 *        2 slots when occupancy & 1 (City_FindByTile @ 11c8:00c4 +
 *        DAT_1420_5516, temp 68f3/68f0/68f6 badge draw).
 *   tail: SELECTOBJECT restore + SETRECT reset + GfxPort_PresentRect @
 *     10d0:0854 (BITBLT 0xcc0020 SRCCOPY) + ClearActive @ 10d0:09a2 +
 *     SetActive + Gfx_StoreDC @ 1098:1104.
 * Ikony: badge icons 0x20 px steps (flag sheet civ*0x20+type+0x40, see
 *   video.h unit_flag_icon_index); kolory: panel frame color byte above
 *   (45fb/45ee) + Text_SetColor @ 1098:0e0b slot font; timeout/klik:
 *   BRAK w popupie (paint + present only) — modal wait lives
 *   caller-side via Sys_WaitForInput @ 1038:0c1d (PEEKMESSAGE loop:
 *   0x102 key -> return DAT_1420_54ea, 0x201 click in DAT_1420_9948 ->
 *   DAT_1328_0012 = 1 + coords + return 1, else
 *   TRANSLATE/DISPATCH; 20+ callers: census 1018:067b/0be0/1d5b/208f,
 *   HoF 1080:0119/06ff, NewGame_Wizard @ 10e8:0000, Pic_PlayShared @
 *   1030:0000, Replay @ 1180:0000/02d3/152f, etc.; SDL bridge TBD ->
 *   sys_wait_for_input STUB in sys.h, unit_show_info_popup stays STUB
 *   for the GDI/present/modal tail). OS-free line predicates below are
 *   LIVE 1:1 (no Win16, no globals, read-only); modal stays STUB. */
int unit_popup_is_end_of_turn(int unit_idx);
int unit_popup_has_veteran(int flags);
int unit_popup_moves_full(int moves_raw);
int unit_popup_moves_frac(int moves_raw);
int unit_popup_badge_needs_wrap(int x, int panel_right);

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
 * mode 0 = background sim tick incl. caravan 0x1b via
 * Trade_DeliverCaravanGoods @ 1038:0d09). Triage FUN_1030_0000
 * (~31 KB, size 0x79cd, 16 wolajacych per task; 13 verified: City_DoRevolt
 * @ 11b8:0f26, City_ProcessDisaster @ 1040:068e, City_ShowCensusReport @
 * 1018:1d5b, City_ShowTopFive @ 1080:099b, Demo_ShowDemographics @
 * 1188:123a, 1018:0be0, 1018:208f, 1040:1137, 1170:0000, 1170:05af,
 * 1170:05ca, 1188:0db8, Unit_DoMove @ 1038:0000; ref unpacked/civ.exe
 * 832512 B NE Windows 3.x read-only, civ1win/ untouched): PARTIAL
 * dispatcher -- OS-free gates LIVE 1:1 in pic_mode_is_modal/pic_mode_is_sim
 * + pic_shared_city_ok (1420:5502 valid gate) + city/tech/prod kernels
 * below (wired in game.c, results ignored until city records land);
 * GDI blit/modal tails (Dlg_CreateTracked @ 1098:124f +
 * FILLRECT/FRAMERECT + 1030:79ce/7c28/7e57/7ece/7f65/7fc1/900d/94d2/96ad/
 * 982f/9aed helpers + PEEKMESSAGE pump via Sys_DrainInput @ 11f0:4ea0 +
 * Sys_WaitForInput @ 1038:0c1d) stay STUB with TODO Orig in game.c.
 * See title.h: this is NOT the intro loop (title_play_anim_loop stays a
 * separate intro stub for Main_RunStartupSequence @ 11e0:0000 loop#1). */
civ_error_t pic_play_shared(int city_idx, int mode);

/* Orig Pic_PlayShared @ 1030:0000 city gate (1420:5502 valid byte) ->
 * pic_shared_city_ok. OS-free 1:1 (implemented): invalid city
 * (valid == -1/0xff, empty slot in the 0x80 0x1c records @ 1420:5500)
 * -> 0 (orig returns 0 immediately, no modal/sim); else nonzero (1).
 * Takes the valid byte explicitly (read via city_get_valid); no globals,
 * no Win16, no exit(), read-only. Ref unpacked/civ.exe read-only. */
int pic_shared_city_ok(int valid);

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
 * local_38 wrap + DAT_1310_0002 gate) with per-unit order:
 *   (1) visibility: LIVE kernels unit_seen_index/bit/minimap/sight_*
 *     (11c0:0395 math) + STUB unit_update_visibility @ 11c0:0395
 *     (verified Ghidra name) / unit_reveal_area @ 11c0:12c8 (Unit_SpawnUnit
 *     factory per chain-3, reveal fan-out TBD) + map_spot_city (map.h,
 *     parallel-owned);
 *   (2) centering: LIVE unit_should_center_view + STUB unit_maybe_center_view
 *     @ 11f0:44d4 (Ghidra still FUN_11f0_44d4, rename pending) ->
 *     Map_RevealCenter @ 1110:1461 (verified) / Map_CenterOnTile @
 *     1110:03ae (map.h, SDL view/scroll TBD);
 *   (3) orders-dialog: LIVE unit_wait_blink_phase + STUB unit_wait_for_orders
 *     @ 11c0:0ec2 (Ghidra still FUN_11c0_0ec2, rename pending) -> SDL modal
 *     bridge TBD (dialog.h Dialog_ShowMessage @ 1220:00d2, sys.h
 *     Sys_WaitForInput @ 1038:0c1d);
 *   (4) move-animation: Unit_EnsureOnScreen @ 11c0:231d (verified callers
 *     Combat_ResolveAttack @ 1050:0000 / Unit_AIPlayerTurn @ 1078:4f0f /
 *     Unit_DoMove @ 1038:0000) + Unit_PlayMoveAnimation @ 11c0:2c32
 *     (move-path tail, no direct xref) -> map/video SDL owners TBD, no
 *     game-side predicate (no fake).
 * IDLE (0x00) returns CIV_OK (no work); all other known orders hit the
 * Win16/map/city/dialog tails above -> STUB CIV_ERR_UNSUPPORTED; bad
 * order/civ/unit -> RANGE/NOENT. No globals. Ref: unpacked/civ.exe
 * read-only (civ1win/ untouched); direct Civ_ProcessUnitMoves callees
 * verified via get_function_callees (0395/44d4/0ec2/03ae/1480/1128:0000/
 * 4ea0 present). */
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

/* --- Chain 2026-10-16 (8): census counting kernels (task goal 1) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Census_ShowTechBars @ 1018:03f0 (bary 0x0c flag id via
 *     Unit_DrawFlagIcon @ 11c8:008a + tech list 0x48 via Tech_HasTech @
 *     1170:3181 + Res 0x82 names, owner color 0xf/0xb; open via
 *     City_OpenCensusView @ 1018:0000 + Res 0x8a/5, close via
 *     City_CloseCensusView @ 1018:0380; bar via Gfx_FillBarRect @
 *     11e8:0504 + Census_DrawTechBadge @ 1018:24c5).
 *   Census_ShowUnitList @ 1018:067b (0x1c unit rows: counts at
 *     4121+0xd / 3f5c+0x12 per civ*0x38 + flag civ*0x20+unit+0x40 via
 *     unit_flag_icon_index (video.h LIVE) + Res 0x84 names + atk/def/
 *     moves at 1a40/1a42/1a3c; row y = 0x20+n*9, flag x = (n&1)*0x16+2,
 *     y-5; chains into Census_ShowTerrainCounts).
 *   Census_ShowTerrainCounts @ 1018:09cc (0x1b terrain rows: any-civ
 *     nonzero at 2b90 per 0x38 + same flags + Res 0x84 + per-civ counts
 *     at 0x68 + ord*0x1c, color civ+0x10 (0xf when mono DAT_12d8_28fc);
 *     row y = 0x1c+n*7).
 *   City_ShowCensusReport @ 1018:1d5b (0x80 city rows owner == civ &&
 *     valid != -1 via Pic_PlayShared @ 1030:0000 + City_AppendName +
 *     building icons 0x2618 mask over 0x18 bits at x 0xd6+n*0x13 +
 *     totals + City_FormatCensusRatio @ 1018:202a x3 (surplus/total,
 *     (total-surplus-decay)/total, decay/total); row y = 0x20+n*10,
 *     page at y > 0xbe via PresentRect + City_DrawCensusHeader @
 *     1018:015d; empty guard local_10 == 0 -> 1).
 *   City_OpenCensusView @ 1018:0000 (dialog 0x94 via Dlg_CreateTracked @
 *     1098:124f + fill + GfxPort_Create/SetActive @ 10d0:0000/0956 +
 *     City_DrawCensusHeader; DAT_12d8_0026/002c/002e view state).
 *   City_CloseCensusView @ 1018:0380 (PresentRect @ 10d0:0854 +
 *     Sys_WaitForInput + FUN_1100_0be5 + GfxPort_ClearActive @ 10d0:09a2
 *     + Dlg_DestroyTracked).
 *   City_DrawCensusHeader @ 1018:015d (was FUN_1018_015d: title + civ
 *     adjective 0x2da/0x1da + gov 0x132/0x139/0x141 + year via
 *     Status_AppendYearText @ 1088:1dd0; callers Open + TerrainCounts? no:
 *     Open, Report paging, 0be0, 208f, 1040:1137).
 *   City_FormatCensusRatio @ 1018:202a (num < 300 ? (num*100)/den :
 *     (num*10)/(den/10), quotient text only; already LIVE as
 *     city_format_census_ratio in game.c, untouched).
 * Format wierszy: tech grid 3-col x = (i%3)*100+8, y = 0x30+(i/3)*7
 *   (break y > 0xc0); unit/terrain rows as above; report rows y += 10.
 * Co licza: techy per civ (Tech_HasTech bits, 0x48 techs), jednostki
 *   (2 tabele count per civ*0x38: owned/desert vs terrain-extra), tereny
 *   (2b90 per civ*0x38 per terrain 0x1b). Okna (Open/Close/Header/Badge/
 *   FillBar blits) -> STUB; liczenie/format -> LIVE below, no globals. */

/* Orig City_OpenCensusView @ 1018:0000 -> city_open_census_view.
 * Dialog 0x94 + GfxPort create/set + City_DrawCensusHeader @ 1018:015d
 * tail. Win16 dialog/GDI -> SDL TBD. */
civ_error_t city_open_census_view(int title_id, int civ);
/* Orig City_CloseCensusView @ 1018:0380 -> city_close_census_view.
 * PresentRect @ 10d0:0854 + wait + GfxPort_ClearActive @ 10d0:09a2 +
 * destroy tail. Win16 -> SDL TBD. */
civ_error_t city_close_census_view(void);
/* Orig City_DrawCensusHeader @ 1018:015d -> city_draw_census_header.
 * Title + civ + year header. GDI text -> SDL TBD. */
civ_error_t city_draw_census_header(int title_id, int civ);
/* Orig Census_DrawTechBadge @ 1018:24c5 -> census_draw_tech_badge.
 * Port select + fill + Gfx_DrawTechIcon(*(icon*2+0x128)) badge. GDI -> SDL. */
civ_error_t census_draw_tech_badge(int icon, int x, int y);

/* Orig Census_ShowTechBars @ 1018:03f0 target math -> census_tech_target.
 * 1:1: t = difficulty*2 + base + 6 (base is DAT_12a8_0020); if
 * t < 0xb - extra t = 0xb - extra (extra is the 3f5c+2+civ*2 word).
 * No globals. */
int census_tech_target(int difficulty, int base, int extra);
/* Orig same bar width math -> census_bar_segment / census_bar_width /
 * census_bar_origin / census_bar_count. 1:1: seg = clamp(0x980/(val+1),
 * 1, 0x40); w = clamp(seg*val/8+8, 8, 0x130); x0 = 0xa0-w/2; count =
 * clamp(raw, 0, val) (raw is the 43b8+0x7c+civ*2 word). No globals. */
int census_bar_segment(int val);
int census_bar_width(int seg, int val);
int census_bar_origin(int width);
int census_bar_count(int raw, int val);
/* Orig same tech grid -> census_tech_cell. 1:1: x = (idx%3)*100+8,
 * y = 0x30+(idx/3)*7. idx is the drawn-tech ordinal (local_10).
 * NOENT on NULL outs, else CIV_OK. No globals. */
civ_error_t census_tech_cell(int idx, int *x_out, int *y_out);
/* Orig same owner color -> census_tech_owner_color. 1:1: ((entry&7) ==
 * civ) ? 0xf : 0xb (entry is the 2afa+6+tech*2 word). No globals. */
int census_tech_owner_color(int entry, int civ);
/* Orig Census_ShowUnitList @ 1018:067b rows -> census_unit_row_y /
 * census_unit_flag_x / census_unit_has_row. 1:1: y = 0x20+row*9;
 * flag x = (row&1)*0x16+2 at y-5; has = (a > 0 || b > 0) (a is the
 * 4121+0xd+civ*0x38+unit*2 count, b the 3f5c+0x12 count). row is the
 * drawn-row ordinal (local_6). No globals. */
int census_unit_row_y(int row);
int census_unit_flag_x(int row);
int census_unit_has_row(int a, int b);
/* Orig Census_ShowTerrainCounts @ 1018:09cc rows -> census_terrain_row_y
 * / census_terrain_cell_x. 1:1: y = 0x1c+row*7 (row is the drawn-row
 * ordinal local_a); self count at 0x68, each other civ +0x1c in civ
 * order skipping self (ord is the other-civ ordinal 0..). x =
 * 0x68+ord*0x1c. Flag x shares census_unit_flag_x. No globals. */
int census_terrain_row_y(int row);
int census_terrain_cell_x(int ord);
/* Orig City_ShowCensusReport @ 1018:1d5b rows -> census_report_row_y /
 * census_report_needs_page / census_report_next_x /
 * census_building_icon_shown / census_empty_guard. 1:1: y =
 * 0x20+idx*10 (idx drawn-city ordinal); page when y > 0xbe (0xbe < y);
 * icon x = 0xd6+n*0x13 (n filtered icons with (mask & 0x2618) != 0 over
 * 0x18 bits, mask is the 54fc city word); bit shown = (((1u<<bit) &
 * 0x2618) != 0); empty guard: total == 0 -> 1 (local_10). No globals. */
int census_report_row_y(int idx);
int census_report_needs_page(int y);
int census_report_next_x(int n);
int census_building_icon_shown(int bit);
int census_empty_guard(int total);

/* --- Chain 2026-10-16 (9): census header format (task goal 1) ---
 * Orig City_DrawCensusHeader @ 1018:015d (was FUN_1018_015d), decompiled
 * this pass: GfxPort_SetActive + FILLRECT 0x140x0x200 civ brush (color
 * ((civ_table)[param_3]), mono DAT_12d8_28fc forces param_3 = 0) + title
 * TEXTOUT at 0xa0,2 via FUN_10d8_046e + civ line at 0xa0,10 + year line
 * at 0xa0,0x12 via FUN_10d8_0528 color 0xf (civ adjective Res 0x2da/0x1da
 * + gov title 0x132/0x139/0x141 by (gov_word/2) + Status_AppendYearText
 * @ 1088:1dd0 tail). Callers (6): City_OpenCensusView @ 1018:0000 +
 * City_ShowCensusReport @ 1018:1d5b paging (y > 0xbe) +
 * Census_ShowCityProduction @ 1018:0be0 (was FUN_1018_0be0: per-city
 * trade/production census over 0x80 0x1c records with Pic_PlayShared @
 * 1030:0000 + trade sums + wonder + 0x18 production breakdown, Res
 * 0x8a,4,2 title) + Census_ShowCityStatus @ 1018:208f (was FUN_1018_208f:
 * per-city size/output census with 1260:00d6/00d8/00da + prod 0x83/0x84
 * names + shields, Res 0x8a,0,8 title) + Hof_CalcFinalScore @ 1040:1137
 * (was FUN_1040_1137: final-score census view + tech list + Gfx_FillBar
 * Rect bars; NOT the disaster — disaster is City_ProcessDisaster @
 * 1040:068e, see game.h; score callers Game_ProcessGameEnd @ 1088:0b6d +
 * Game_TurnLoopTick @ 1088:0000). Other header painters draw their own
 * titles (no shared header): HoF_Show @ 1080:0119 + Demo_ShowDemo
 * graphics @ 1188:123a + city modal (Pic_PlayShared). GDI text tails
 * stay STUB in city_draw_census_header above; pure format below is LIVE
 * 1:1 (no Win16, no globals). */
int city_census_gov_title_id(int gov_half);
int city_census_header_civ(int mono, int civ);
civ_error_t city_census_header_pos(int line, int *x_out, int *y_out);

/* --- Chain 2026-10-16 (10): census city production/status counting ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Census_ShowCityProduction @ 1018:0be0 (per-city trade/production over
 *     0x80 0x1c records @ 1420:5500, owner @ +7 == civ, valid @ +2 != -1:
 *     Pic_PlayShared @ 1030:0000 + City_AppendName + per-city outputs
 *     DAT_1260_00dc/00d2/00e2 trade line (bit0 of +2 set = disorder "399"
 *     line instead) + sums local_a += 00d2 / local_c += 00e2 + wonder
 *     0x14 (City_IsWonderOpen → local_c += local_c/2) + caravan value
 *     (diff*2+6)*civ_value*band/local_c with year band (year<0→1,
 *     <0x6d6→2, else 3) + 0x18 production breakdown local_46[] via 54fc
 *     bits * cost table 0x192c (+local_e on bit1); title Res 0x8a,4,2;
 *     paging at y > 0x9f/0xb4 via PresentRect + header).
 *   Census_ShowCityStatus @ 1018:208f (per-city size/output: size @ +3 +
 *     DAT_1260_00d6/00d8/00da triple + prod name Res 0x83 (building,
 *     +5 < 0 branch with 550a shields + cost*10 table 0x1924) vs Res 0x84
 *     (unit, +5 in 0..0x3f branch with 550a + cost*10 table 0x1a44);
 *     title Res 0x8a,0,8; paging at y > 0xbf).
 *   Trade base local_e: g_nDifficulty/2 + Tech_HasTech(civ,0x22) +
 *     Tech_HasTech(civ,0x25) (trade-route techs, see Tech_HasTech @
 *     1170:3181 + trade_calc_bonus @ 1038:0d09). Relation to
 *     City_FormatPopCount @ 11c8:03ac / City_TileYield @ 1250:0000 family:
 *     outputs 1260:00d6/d8/da/00dc/00d2/00e2 are the per-city tile-yield
 *     rollup (food/shields/trade + corruption split); this chain only sums
 *     the rollup, yield math stays in city_tile_base_yield/adjust LIVE.
 *   GDI census-view tails (Open/Header/Close/Badge/FillBar/Present) stay
 *     STUB in city_open/close/draw_census_header + census_draw_tech_badge
 *     (see above + video.h badge/bar); counting below is LIVE 1:1. */

/* Orig Census_ShowCityProduction @ 1018:0be0 trade base -> census_trade_base.
 * 1:1 local_e = difficulty/2 + (has_t31?1:0) + (has_t26?1:0). Negative
 * difficulty clamps divisor path to 0 base (orig DIV on s16; guarded). */
int census_trade_base(int difficulty, int has_t31, int has_t26);
/* Orig same wonder bonus -> census_trade_wonder_total. 1:1 when
 * wonder_open != 0: trade + trade/2 (City_IsWonderOpen(g_nPlayerCiv,0x14)
 * branch, x86 IDIV truncates toward zero; negative trade stays). */
int census_trade_wonder_total(int trade, int wonder_open);
/* Orig same accumulators -> census_prod_accum_trade/shields. 1:1 local_a
 * += DAT_1260_00d2, local_c += DAT_1260_00e2 per ordered city. */
int census_prod_accum_trade(int acc, int city_trade);
int census_prod_accum_shields(int acc, int city_shields);
/* Orig same year band -> census_prod_year_band. 1:1 year<0→1, <0x6d6→2,
 * else 3 (caravan-distance multiplier select). */
int census_prod_year_band(int year);
/* Orig same caravan value -> census_caravan_value. 1:1 (diff*2+6) *
 * civ_value * band / trade_total (orig IMUL chain then IDIV; trade_total
 * <= 0 or <0 inputs -> -1, no DIV fault). Truncates toward zero. */
int census_caravan_value(int difficulty, int civ_value, int band,
                         int trade_total);
/* Orig same building breakdown -> census_building_add. 1:1 bit set in the
 * 54fc city word: total = count + unit_cost (+trade_base when building
 * == 1, the local_46[1] palace/capital path); bit clear -> count
 * unchanged. building is 1..0x17, unit_cost is the 0x192c table word. */
int census_building_add(int count, int building_set, int building,
                        int unit_cost, int trade_base);
/* Orig Census_ShowCityStatus @ 1018:208f prod branch ->
 * census_status_prod_kind. 1:1 prod < 0 (signed +5 byte) → 0 building
 * (Res 0x83); 0 <= prod < 0x40 → 1 unit (Res 0x84); else 2 = none
 * (no prod line; orig falls through to the FUN_10d8_002d tail). */
int census_status_prod_kind(int prod);
/* Orig same shields line -> census_status_shields_total. 1:1 shown total
 * = 550a shields + cost_table*10 (cost via 0x1924 building table or
 * 0x1a44 unit table, caller passes cost explicitly). */
int census_status_shields_total(int shields, int cost);

/* --- Chain 2026-10-16 (19): Pic_PumpMenuCommand mapping (goal 2) ---
 * Orig Pic_PumpMenuCommand @ 1030:9aed (civ.exe, x86:LE:16:Protected
 * Mode), decompiled this pass: DAT_12d8_0030 = 0; PEEKMESSAGE (0,0,0) ->
 * gdy HIT (msg_win == 0 || == *param_1 || ISCHILD || == 12b0:0004 main)
 * to TRANSLATEMESSAGE + DISPATCHMESSAGE; gdy DAT_1420_54e8 == 0xf
 * (WM_PAINT) && DAT_1420_54e6 == *param_1 to Pic_PresentOnPaint @
 * 1030:900d (renamed Pic_PresentOnPaint: SETRECT 0x140,0x200 +
 * GfxPort_PresentRect 0x70 + FUN_1030_94d2 tail); switch DAT_12d8_0030:
 * 0 -> *param_2 = 0; 1 -> 0x96; 2 -> 0x97; 3 -> 0x98 (bez default: inne
 * wartosci zostawiaja *param_2 nietkniete); gdy DAT_1420_54e8 == 0x111
 * (WM_COMMAND) to Menu_DispatchCommand(DAT_1420_54ea,0) @ 1048:01cd.
 * Co to za mapowanie (menu kontekstowe miasta? skroty? mysz?):
 *   DAT_12d8_0030 to selekt trybu miasta z dialogu miasta (buttony miasta
 *   w Pic_PlayShared @ 1030:0000 city view: 1/2/3 = trzy przyciski trybu
 *   produkcji/podgladu; 0 = brak wyboru). 0x96/0x97/0x98 to tryby PIC
 *   (pic_mode_is_modal/sim w game.c: modal vs sim; 0x96/97/98 = tryby
 *   podgladu miasta, nie skroty klawiszowe ani menu glowne). WM_COMMAND
 *   0x111 to komendy paska menu w grze (Menu_DispatchCommand 68 wpisow,
 *   zob. mainmenu.h) — przechodza obok, nie przez ten switch. Mysz:
 *   ISCHILD + main-window gate decyduja czy DISPATCH wchodzi do dialogu
 *   miasta; WM_PAINT 0xf to repaint miasta (PresentOnPaint).
 * Zwiazek z Pic_PlayShared: sole caller (Pic_PumpMenuCommand wywolywany
 *   z petli miasta w Pic_PlayShared; modal miasta kreci sie wokol
 *   Sys_DrainInput @ 11f0:4ea0 (renamed Sys_DrainInput) + Sys_WaitForInput
 *   @ 1038:0c1d, jak inne modale census/HoF/wizard/Replay).
 * Port mapowania LIVE 1:1 na jawnych parametrach (no Win16, no globals);
 * PEEKMESSAGE/TRANSLATE/DISPATCH ogon STUB (pic_play_shared UNSUPPORTED).
 * No exit(). */

/* Orig Pic_PumpMenuCommand @ 1030:9aed switch -> pic_pump_select_to_mode.
 * 1:1: 0 -> 0, 1 -> 0x96, 2 -> 0x97, 3 -> 0x98, inaczej -1 (orig
 * zostawia *param_2 nietkniete = brak mapowania). */
int pic_pump_select_to_mode(int sel);

/* Orig same WM_COMMAND branch (DAT_1420_54e8 == 0x111) ->
 * pic_pump_msg_is_command. 1:1 msg == 0x111. */
int pic_pump_msg_is_command(int msg);

/* Orig same WM_PAINT branch (DAT_1420_54e8 == 0xf) ->
 * pic_pump_msg_is_paint. 1:1 msg == 0x0f. */
int pic_pump_msg_is_paint(int msg);

/* --- Chain 2026-10-16 (20): spaceship + drain + paint-tail (goals 1/4/5) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Ship_OpenView @ 1070:0000 (renamed; guard viewer==player or debug
 *     0x20 -> Cursor_Set(0x84) + Dlg_CreateTracked(0x94,0x41) + GfxPort
 *     0xd2 + Art_LoadFrameData(0x15cd,0x48) + 10x Gfx_ExtractClippedIcon
 *     -> DAT_1420_5246 + 20-entry starfield init (x=Rng_Range(0x174)+6,
 *     y=Rng_Range(0x133)+12, flag 0, weight per uVar6&3 from
 *     12d8:45c5/45c1/45ca/45bf or 1 when 12d8:28fc mono)).
 *   Ship_ShowView @ 1070:0327 (renamed; stats core below + Gfx/Text tails;
 *     callers: Menu_PromptNation @ 1070:1f58 "Which nation" brat,
 *     Ship_BuildPart @ 1070:16c3, Ship_EditParts @ 1070:1952,
 *     Ship_LaunchAndShow @ 1070:2015; sole modal via Ship_PumpViewInput @
 *     1070:1018 + Main_PumpUntilFlagClear @ 11e8:02a4 + Sys_DrainInput @
 *     11f0:4ea0; launch confirm via Dialog_ShowMessage @ 1220:00d2).
 *   Ship_IsPartConnected @ 1070:11f2 (renamed from FUN_1070_11f2: part
 *     byte == -1 (empty) or >2 -> 0 else 1; adjacency sum in ShowView).
 *   Ship_AnimateStarfield @ 1070:12b5 (renamed from FUN_1070_12b5:
 *     20-entry scroll/redraw via Ship_DrawStarPixel @ 1070:124e +
 *     GETPIXEL probe @ 1070:127a; Gfx -> STUB).
 *   Ship_PumpViewInput @ 1070:1018 (renamed from FUN_1070_1018: modal
 *     PEEKMESSAGE pump, result via DAT_1420_b376 1/2; Win16 -> STUB).
 *   Ship_CloseView @ 1070:02b5 (renamed; 10x Gfx_FreeIconBuffer +
 *     GfxPort_Close 0xd2 + Dlg_DestroyTracked; Gfx -> STUB).
 *   Ship_BuildPart @ 1070:16c3 (renamed; sole caller Pic_PlayShared @
 *     1030:0000 city view: slot scan + Tech_HasTech @ 1170:3181 gate;
 *     returns 0 built / 1 deferred-needs-tech / 2 ship-full).
 *   Ship_EditParts @ 1070:1952 (renamed; sole caller Pic_PlayShared @
 *     1030:0000: type dialogs + "No further"/"required" messages).
 *   Ship_LaunchAndShow @ 1070:2015 (renamed; callers Pic_PlayShared @
 *     1030:0000 + Ship_ShowView confirm: sets DAT_12d0_00b6 bit, arrival
 *     message, year stamp; non-player preview via Open/Show/Drain/Wait).
 *   Menu_PromptNation @ 1070:1f58 (renamed "Which nation" brat:
 *     DAT_12d0_00b6 & 0xfe00 -> 7-entry list -> i+1 -> Open/Show/Close).
 *   Sys_DrainInput @ 11f0:4ea0 (renamed; 22 callerow incl. ProcessUnitMoves,
 *     MenuZ_Modal, PumpMessages, WaitForInput, Pic_PlayShared, Replay, HoF,
 *     StartupSequence: zeroes 1420:1d44/1d46/1d48/1d4c/1d4e/1d50 (6 words)
 *     + two PEEKMESSAGE drain loops + DAT_12b0_004c = 0 res-queue reset;
 *     res queue is Main_EnqueueResString @ 1008:0a8d (append while <10)
 *     drained by Main_DrainMessages @ 11e8:02fb; Win16 queues -> STUB).
 *   Pic_PresentOnPaint @ 1030:900d (renamed; WM_PAINT 0xf handler:
 *     SETRECT 0x140,0x200 + GfxPort_PresentRect 0x70-port + Pic_FinishPaint
 *     tail @ 1030:94d2 (renamed from FUN_1030_94d2: frame/border decor via
 *     FUN_1000_14f8 + Pic @ 1030:96ad/982f; Gfx -> STUB)).
 * Port LIVE 1:1 na jawnych parametrach (no Win16, no globals, no exit());
 * dialog/GfxPort/PEEKMESSAGE ogony zostaja STUB (ship_open_view etc. nie
 * maja nowych symboli — metryki ponizej wystarcza do testu). */

/* Orig Ship_IsPartConnected @ 1070:11f2 -> ship_part_connected. 1:1:
 * (part == -1 || part > 2) -> 0 else 1 (part to signed byte wartosci
 * slotu 0xb4-record +0x1b2; ShowView sumuje 4-sasiadow x wieksze kafle). */
int ship_part_connected(int part);

/* Orig Ship_ShowView @ 1070:0327 support line -> ship_support_percent/frac.
 * 1:1: denom = clamp(pop,1,99) (Util_Clamp @ 11c8:0063 inline, no link);
 * percent = (support*100)/denom, frac = (support*100)%denom (x86 IDIV
 * trunc toward zero, same as C /%). support to DAT_1290_01ac, pop to
 * DAT_1290_01aa. */
int ship_support_percent(int support_raw, int pop);
int ship_support_frac(int support_raw, int pop);

/* Orig same power/mass line -> ship_power_percent/frac. 1:1: denom =
 * clamp(pop+support,1,99); percent = (power*200)/denom, frac =
 * (power*200)%denom. power to DAT_1290_01ae. */
int ship_power_percent(int power_raw, int pop, int support);
int ship_power_frac(int power_raw, int pop, int support);

/* Orig same flight line -> ship_flight_tenths/years/frac. 1:1: thrust =
 * clamp(pop10*10,0,sup10*10) (pop10 = DAT_1290_01a4, sup10 = DAT_1290_01a6);
 * tenths = (mass_total*50)/(thrust+1) (0x32 factor); years = tenths/10,
 * frac = tenths%10 ("x.y Years" + arrival = year + years). mass_total to
 * local_c (suma count*waga 0x12aa). */
int ship_flight_tenths(int mass_total, int pop10, int sup10);
int ship_flight_years(int flight_tenths);
int ship_flight_frac(int flight_tenths);

/* Orig same success line -> ship_success_percent. 1:1: e=100;
 * e=(e*clamp(support_pct,0,100))/100; e=(e*clamp(power_pct,0,100))/100;
 * if (flight_tenths > 0x96) e -= (flight-0x96)/10; clamp 0..100
 * (final Util_Clamp(local_e,0,100) + display clamp 1..100 caller-side). */
int ship_success_percent(int support_pct, int power_pct, int flight_tenths);

/* Orig same arrival stamp (*(param*2+0x752) = g_nYear + tenths/10) ->
 * ship_arrival_year. 1:1 year + flight_tenths/10. */
int ship_arrival_year(int year, int flight_tenths);

/* Orig Sys_DrainInput @ 11f0:4ea0 state half -> sys_drain_state_words.
 * 1:1: 6 slow zerowanych (1420:1d44/1d46/1d48/1d4c/1d4e/1d50). */
int sys_drain_state_words(void);

/* Orig Main_EnqueueResString @ 1008:0a8d gate (DAT_12b0_004c < 10) ->
 * sys_drain_queue_cap. 1:1 pojemnosc kolejki res-stringow (drain ja zeruje
 * w Sys_DrainInput, Main_DrainMessages @ 11e8:02fb oproznia). */
int sys_drain_queue_cap(void);

/* Orig Sys_DrainInput @ 11f0:4ea0 reset (DAT_12b0_004c = 0) ->
 * sys_drain_queue_reset. 1:1 wartosc wpisywana do head (zawsze 0). */
int sys_drain_queue_reset(void);

/* Orig Sys_DrainInput @ 11f0:4ea0 PEEKMESSAGE ranges (P3 drain format,
 * decompiled 2026-10-16; 22 callers incl. Civ_ProcessUnitMoves @ 11f0:0000,
 * Sys_PumpMessages @ 1108:0000, Sys_WaitForInput @ 1038:0c1d):
 *   head: 6 slow 1420:1d44/1d46/1d48/1d4e/1d4c/1d50 = 0 (sys_drain_state_words),
 *   drain1: PEEKMESSAGE(...,0x108,0x100,0,...) az 0 (key range),
 *   drain2: PEEKMESSAGE(...,0x209,0x200,0,...) az 0 (mouse range),
 *   tail: DAT_12b0_004c = 0 (res-string queue head, cap 10 via
 *   sys_drain_queue_cap, oprozniana przez Main_DrainMessages @ 11e8:02fb).
 * PEEKMESSAGE ogony Win16 -> STUB (sys_drain_input UNSUPPORTED); zakresy
 * ponizej LIVE 1:1 (no Win16, no globals). */
int sys_drain_peek_key_first(void);
int sys_drain_peek_key_last(void);
int sys_drain_peek_mouse_first(void);
int sys_drain_peek_mouse_last(void);

/* Orig Pic_PresentOnPaint @ 1030:900d geometry -> pic_paint_port_id/w/h.
 * 1:1: port 0x70, box 0x140 x 0x200 (SETRECT przed PresentRect; tail
 * Pic_FinishPaint @ 1030:94d2 Gfx -> STUB). */
int pic_paint_port_id(void);
int pic_paint_w(void);
int pic_paint_h(void);

/* --- Chain 2026-10-16 (21): starfield step + view-input codes + finish tail
 * (goals 1/2/4) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Ship_AnimateStarfield @ 1070:12b5: 20 wpisow (0..0x13; x @ +0x166,
 *     y @ +0x13e, flag @ +0x5232, color @ +0x116; bg DAT_1290_00d0, frame
 *     DAT_1420_522c). Krok: erase-old-bg via Ship_DrawStarPixel @ 1070:124e
 *     (renamed; SETPIXEL wrapper) gdy flag, potem scroll x += speed(layer):
 *     layer = idx&3: 0 -> +6, 1 -> +5, 2 -> +4, 3 -> +3, przy czym warstwy
 *     2/3 tylko gdy (frame&1) != 0; wrap gdy x > 0x17f: x = 2,
 *     y = Rng_Range(0x133)+0xc, flag = 0; potem probe-bg via
 *     Ship_ProbeStarPixel @ 1070:127a (renamed; GETPIXEL + FUN_10b0_168a)
 *     i draw-new color gdy pixel == bg (flag = 1; warstwy 0/1 rysuja 2x2
 *     blok 3+1 piksele, warstwy 2/3 pojedynczy piksel). Zwiazek z
 *     Ship_ShowView @ 1070:0327: wywolywany 1x po SETRECT + 1x per pump-loop
 *     gdy launched-bit (DAT_12d0_00b6) + zawsze PresentRect 0xd2; zwiazek z
 *     Anim_AdvanceFrames @ 10b0:0461: BRAK (starfield to wlasna petla Ship,
 *     nie klatki Anim; Anim to generyczny player sekwencji intro/PIC, STUB
 *     w dialog.h anim_advance_frames).
 *   Ship_PumpViewInput @ 1070:1018: modalny PEEKMESSAGE-pump Ship_ShowView
 *     (SETWINDOWLONG hook Ship_ViewHookProc @ 1070:10cb, renamed; wynik via
 *     DAT_1420_b376; loop az 1/2). Inputy z CS-tabeli @ 1070:11e2 (4 wpisy):
 *     0x0102 WM_CHAR -> b376 = 1 (klawisz = zamknij),
 *     0x0111 WM_COMMAND -> wParam 999 (0x3e7 Launch) -> b376 = 2
 *     (przycisk Launch = potwierdz; inne -> DEFDLGPROC),
 *     0x0201/0x0202 LBTNDOWN/UP -> coords do 1420:5228/522A + flag 5226 +
 *     b376 = 1 po valid part-click (klikniecie czesci; close/DEFDLGPROC
 *     dla reszty). Kody: 1 = close/cancel, 2 = launch-confirm.
 *   Pic_FinishPaint @ 1030:94d2: WM_PAINT 0xf tail Pic_PresentOnPaint @
 *     1030:900d (po PresentRect portu 0x70 box 0x140x0x200): 3 decor-passy
 *     (arrow, arrow, caption) via Pic_DrawFrameArrow @ 1030:96ad (renamed;
 *     FILLRECT+FRAMERECT+MOVETO/LINETO) x2 + Pic_DrawFrameCaption @
 *     1030:982f (renamed; FILLRECT+FRAMERECT+DRAWTEXT) x1, kazdy po
 *     FUN_1000_14f8 setup-copy; dispatch 0x96/0x97/0x98 zyje w
 *     Pic_DispatchFinishPaint @ 1030:95c2 (renamed). Ogon (restore DC?
 *     unlock? dirty-clear? StoreDC?): dekompilator pokazuje tylko GDI
 *     decor + SELECTOBJECT-restore w callees; wlasciwy restore/validate
 *     zyje w GfxPort_PresentRect @ 10d0:0854 (VALIDATERECT + StoreDC
 *     restore) i Gfx_StoreDC @ 1098:1104 (Gfx_SetCurrentDC @ 1100:09a9,
 *     29 callerow) + GfxPort_ClearActive @ 10d0:09a2 w popup-tail (patrz
 *     game.h unit_popup tail) — sam FinishPaint nie dotyka DC, wiec port
 *     to kolejnosc LIVE + wolania STUB.
 * Port LIVE 1:1 na jawnych parametrach (no Win16, no globals, no exit());
 * GDI/PEEKMESSAGE ogony zostaja STUB. */

/* Orig Ship_AnimateStarfield @ 1070:12b5 entry-count (0..0x13 loop) ->
 * ship_starfield_count. 1:1 liczba wpisow starfield (20). */
int ship_starfield_count(void);

/* Orig same scroll-branch (idx&3) -> ship_starfield_speed. 1:1: 0 -> 6,
 * 1 -> 5, 2 -> 4, 3 -> 3; OOB layer -> -1. Surowy przyrost per-tick
 * (gating warstw 2/3 co 2. frame robi ship_starfield_step_x). */
int ship_starfield_speed(int layer);

/* Orig same scroll + (frame&1) gate -> ship_starfield_step_x. 1:1:
 * warstwy 0/1 zawsze x+speed; warstwy 2/3 tylko gdy (frame&1) != 0
 * (DAT_1420_522c&1 w orig). Zwraca nowy x bez wrapu (wrap osobno). */
int ship_starfield_step_x(int x, int layer, int frame);

/* Orig same wrap-guard (0x17f < x) -> ship_starfield_needs_wrap.
 * 1:1 x > 0x17f. */
int ship_starfield_needs_wrap(int x);

/* Orig same wrap-body (x = 2) -> ship_starfield_wrap_x. 1:1 zawsze 2. */
int ship_starfield_wrap_x(void);

/* Orig same wrap-body (y = Rng_Range(0x133)+0xc) ->
 * ship_starfield_wrap_y. 1:1 rand+12 (rand to draw Rng_Range @ 11c8:003d
 * 0..0x132; ujemny draw -> samo 12). */
int ship_starfield_wrap_y(int rand133);

/* Orig Ship_ViewHookProc @ 1070:10cb CS-tabela @ 1070:11e2 wpisy ->
 * ship_view_msg_is_*. 1:1: 0x0102 WM_CHAR / 0x0111 WM_COMMAND /
 * 0x0201 LBTNDOWN / 0x0202 LBTNUP (klik czesci vs klawisz vs przycisk). */
int ship_view_msg_is_char(int msg);
int ship_view_msg_is_command(int msg);
int ship_view_msg_is_lbuttondown(int msg);
int ship_view_msg_is_lbuttonup(int msg);

/* Orig same COMMAND-branch (DI == 0x3e7) -> ship_view_launch_id.
 * 1:1 id przycisku Launch (999). */
int ship_view_launch_id(void);

/* Orig DAT_1420_b376 kody (Ship_PumpViewInput @ 1070:1018 loop az 1/2) ->
 * ship_view_result_is_close/is_launch. 1:1: 1 = close/cancel (CHAR /
 * valid part-click), 2 = launch-confirm (COMMAND 999). */
int ship_view_result_is_close(int code);
int ship_view_result_is_launch(int code);

/* Orig Ship_PumpViewInput @ 1070:1018 -> ship_pump_view_input. Modalny
 * PEEKMESSAGE-pump (SETWINDOWLONG hook Ship_ViewHookProc + TRANSLATE/
 * DISPATCH, wynik via DAT_1420_b376 1/2). Win16 queue/hook -> STUB
 * CIV_ERR_UNSUPPORTED; kody powyzej sa LIVE. No globals, no exit(). */
civ_error_t ship_pump_view_input(void);

/* Orig Pic_FinishPaint @ 1030:94d2 decor-passy -> step-count/is-caption.
 * 1:1: 3 passy (0: arrow, 1: arrow, 2: caption); is_caption = (step == 2).
 * Kolejnosc LIVE, wolania GDI (Arrow/Caption + FUN_1000_14f8 setup) STUB. */
int pic_finish_paint_step_count(void);
int pic_finish_paint_step_is_caption(int step);

/* Orig same tail -> pic_finish_paint. STUB CIV_ERR_UNSUPPORTED (restore DC
 * zyje w GfxPort_PresentRect @ 10d0:0854 VALIDATERECT + StoreDC restore i
 * Gfx_StoreDC @ 1098:1104 -> Gfx_SetCurrentDC @ 1100:09a9; dirty-clear to
 * VALIDATERECT; unlock to SELECTOBJECT-restore w Arrow/Caption callees).
 * No globals, no exit(). */
civ_error_t pic_finish_paint(void);

/* --- Chain 2026-10-16 (22): Ship hook dispatch + Pic arrow/caption geometry
 * (goals 2/3) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Ship_ViewHookProc @ 1070:10cb (SETWINDOWLONG hook for Ship_ShowView @
 *     1070:0327 modal via Ship_PumpViewInput @ 1070:1018; result via
 *     DAT_1420_b376: 1 = close, 2 = launch; loop until 1/2; unhooked via
 *     SETWINDOWLONG-restore + FREEPROCINSTANCE; else DEFDLGPROC @
 *     1608:01a8). CS table @ 1070:11e2 (4 entries, read from the image):
 *     0x0102 WM_CHAR -> Ship_ViewCharHandler @ 1070:10fd (label this pass:
 *       ES = 0x1420, DAT_1420_b376 = 1, return 1; key = close).
 *     0x0111 WM_COMMAND -> Ship_ViewCommandHandler @ 1070:110f (label:
 *       AX = DI (wParam); == 999 (0x3e7 Launch, ship_view_launch_id) ->
 *       DAT_1420_b376 = 2, return 1; else DEFDLGPROC).
 *     0x0202 WM_LBUTTONUP -> Ship_ViewButtonUpHandler @ 1070:1129 (label:
 *       CMP [0x5226],0 (armed flag from DOWN) -> JZ skip (no b376); else
 *       save coords [BP+6] -> 1420:5228/522A + child hit-test via
 *       1608:00b0/0104 on DAT_1290_0110 port + valid part-click ->
 *       DAT_1420_b376 = 1; always clears 5226 = 0 at 1070:1177).
 *     0x0201 WM_LBUTTONDOWN -> Ship_ViewButtonDownHandler @ 1070:117f
 *       (label: same coord save + hit-test; on hit sets [0x5226] = 1
 *       (armed for the UP); never sets b376 itself).
 *   Kody 1/2 (ship_view_result_is_close/launch LIVE above): 1 = close/
 *     cancel (CHAR or valid part-click UP), 2 = launch-confirm (COMMAND
 *     999). PEEKMESSAGE/TRANSLATE/DISPATCH pump stays STUB in
 *     ship_pump_view_input above.
 *   Pic_DrawFrameArrow @ 1030:96ad (FILLRECT white + FRAMERECT black +
 *     FUN_1000_14f8 pen setup + FUN_1030_994b + pen pick: greyed when
 *     (kind == 2 && DAT_1260_002e == 0) || (kind == 1 && DAT_1260_0030 ==
 *     0) else CREATEPEN 0x808080 + SELECTOBJECT + triangle rows via
 *     MOVETO/LINETO: dir = (kind == 1) ? -1 : +1, cy = bold + (y0+y1)/2,
 *     cx = bold + (x0+x1)/2 + dir*((h-6)/2), rows = h-7 (each row widens
 *     by 1, steps cx by -dir); SELECTOBJECT-restore + DELETEOBJECT when
 *     pen created; callee of Pic_FinishPaint @ 1030:94d2 x2 + dispatch
 *     modes 0x97/0x98 in Pic_DispatchFinishPaint @ 1030:95c2).
 *   Pic_DrawFrameCaption @ 1030:982f (FILLRECT + FRAMERECT + same 1000:14f8
 *     /994b setup + INFLATERECT(1,1) + OFFSETRECT(bold,bold) +
 *     OFFSETRECT((w)/4,0) where w = x1-x0 + SELECTOBJECT font 1420:54a2 +
 *     DRAWTEXT 0x6e4 + restore; callee of FinishPaint tail + dispatch mode
 *     0x96 in DispatchFinishPaint).
 *   Pic_DispatchFinishPaint @ 1030:95c2 (modes from pic_pump_select_to_mode
 *     LIVE above: sel 1/2/3 -> 0x96/97/98; 0x96 -> caption when flags&3,
 *     0x97 -> arrow(1) when flags&3, 0x98 -> arrow(2) when flags&3; each
 *     gated by (*(uint *)(p+6) & 3) != 0 + FUN_1000_14f8 setup variant).
 *   Pic_PumpMenuCommand @ 1030:9aed (sole caller Pic_PlayShared @ 1030:0000
 *     city-view loop: PEEKMESSAGE/ISCHILD gate + TRANSLATE/DISPATCH +
 *     WM_PAINT 0xf -> Pic_PresentOnPaint @ 1030:900d (SETRECT 0x140x0x200
 *     + PresentRect 0x70-port + FinishPaint tail) + sel switch above +
 *     WM_COMMAND 0x111 -> Menu_DispatchCommand @ 1048:01cd (NOT through the
 *     sel switch); the city modal spins on Sys_DrainInput @ 11f0:4ea0 +
 *     Sys_WaitForInput @ 1038:0c1d like census/HoF/wizard/Replay).
 * OS-free dispatch/geometry below is LIVE 1:1 on explicit params (no Win16,
 * no globals, read-only); GDI hit-test/blits + modal pump stay STUB with
 * TODO Orig. No exit(), C17. */

/* Orig Ship_ViewHookProc @ 1070:10cb CS table @ 1070:11e2 ->
 * ship_view_dispatch. 1:1 handler pick: 0x0102 -> 0 (char @ 10fd),
 * 0x0111 -> 1 (command @ 110f), 0x0201 -> 2 (button-down @ 117f), 0x0202 ->
 * 3 (button-up @ 1129), else -1 (DEFDLGPROC @ 1608:01a8 path). */
int ship_view_dispatch(int msg);

/* Orig Ship_ViewCommandHandler @ 1070:110f CMP DI,0x3e7 ->
 * ship_command_is_launch. 1:1 wparam == 999 (ship_view_launch_id). */
int ship_command_is_launch(int wparam);

/* Orig Ship_ViewButtonUpHandler @ 1070:1129 CMP [0x5226],0 gate ->
 * ship_lbuttonup_is_armed. 1:1 flag != 0 (armed by a prior DOWN hit;
 * 0 = skip, no b376). Flag is DAT_1420_5226 passed explicitly. */
int ship_lbuttonup_is_armed(int armed_flag);

/* Orig Ship_ViewButtonDownHandler @ 1070:117f hit -> [0x5226] = 1 ->
 * ship_hit_sets_armed. 1:1 hit != 0 (hit-test via 1608:00b0/0104 on the
 * DAT_1290_0110 port; valid part-click arms the UP). */
int ship_hit_sets_armed(int hit);

/* Orig Pic_DrawFrameArrow @ 1030:96ad dir (kind == 1 ? -1 : +1) ->
 * pic_arrow_dir. 1:1 -1 for kind 1, +1 otherwise (kind is param_6). */
int pic_arrow_dir(int kind);

/* Orig same row loop (for local_10 < h-7) -> pic_arrow_row_count. 1:1
 * h-7 floored at 0 (h is param_5-param_3 height; orig loop simply skips
 * when h <= 7). */
int pic_arrow_row_count(int h);

/* Orig same greyed-pen gate ((kind == 2 && f002e == 0) || (kind == 1 &&
 * f0030 == 0) -> stock object, else CREATEPEN 0x808080) ->
 * pic_arrow_is_greyed. 1:1 nonzero (1) when greyed. Flags are
 * DAT_1260_002e/0030 passed explicitly. */
int pic_arrow_is_greyed(int kind, int f002e, int f0030);

/* Orig Pic_DrawFrameCaption @ 1030:982f OFFSETRECT((w)/4,0) ->
 * pic_caption_text_dx. 1:1 (x1-x0)/4 with C truncation (orig (param_5-
 * param_3)/4; negative stays negative). w is the width explicitly. */
int pic_caption_text_dx(int w);

/* Orig Pic_DispatchFinishPaint @ 1030:95c2 mode gates ->
 * pic_dispatch_is_caption/is_arrow. 1:1: caption = (mode == 0x96),
 * arrow = (mode == 0x97 || mode == 0x98); the (*(p+6)&3) paint gate stays
 * caller-side (needs the PIC ctx). */
int pic_dispatch_is_caption(int mode);
int pic_dispatch_is_arrow(int mode);

/* Orig same arrow variant (0x97 -> FUN_1000_14f8(1,...), 0x98 -> (2,...)) ->
 * pic_dispatch_arrow_variant. 1:1: 0x97 -> 1, 0x98 -> 2, else -1. */
int pic_dispatch_arrow_variant(int mode);

/* Orig Pic_PumpMenuCommand @ 1030:9aed sole caller (Pic_PlayShared @
 * 1030:0000 city-view loop) -> pic_pump_owner_is_shared. 1:1 from_shared
 * != 0 (the pump never runs outside the shared city view; PEEKMESSAGE /
 * DISPATCH tail stays STUB in pic_play_shared). */
int pic_pump_owner_is_shared(int from_shared);

/* --- Chain 2026-10-16 (30): City_FoundCity kernels (task goal 2) ---
 * Orig City_FoundCity @ 1040:0000 (civ.exe, x86:LE:16:Protected Mode),
 * decompiled this pass (0x68d B; wola FUN_1078_4c1a reveal + Hut_ProcessHut
 * @ 11c0:2677 hut-case-0 + City_ChooseProduction @ 1170:05ca dla AI +
 * City_OpenProductionView @ 1170:05af dla gracza + Map_* + Tech_HasTech):
 *   slot: pierwszy 0x80 z 5502 == -1 (0xff), brak -> gracz: msg 0x7eb +
 *   Dialog_ShowMessage @ 1220:00d2, return -1.
 *   nazwa: local_e = (43b8+0x9c[civ]-1)<<4 (>6: -2), scan 0x10 nazw
 *   (29e9+0x17 free == -1, wrap +0x10->0xe0, 0x100->0/local_a=999),
 *   5512 = local_e, nazwa trim trailing ' ' (0x1f w dol).
 *   Kto wola: Hut_ProcessHut @ 11c0:2677 (hut-case-0: tile+0x50 food>4 +
 *   dist>3 -> FoundCity(x,y,1) + reveal; jedyny Bezposredni caller) +
 *   Unit_DoMove @ 1038:0000 posrednio (przejecie miasta resetuje home via
 *   Unit_FoundCityIfHome @ 11c0:16a4, nie FoundCity) + NewGame_InitCivState
 *   @ 10e8:12b5 (start: Unit_SpawnUnit + Map_SetTileOwner, nie FoundCity —
 *   pierwsze miasto gracza powstaje z hut/settle, nie z wizarda) + AI
 *   (ChooseProduction dla nowego miasta). Pelne zakladanie: slot + nazwa
 *   (5512/0x4da trim) + populacja 5503 = param_5 (size, hut daje 1) +
 *   budynki 54fc/5508/550a = 0 (palace 54fc = 1 gdy pierwsze miasto civ) +
 *   prod 5505 = 1 (4 gdy Tech 0x22/34, 5 gdy Tech 0x40/64) + 5516/5517 =
 *   0xff + owner 5507 = civ + 5513 x3 = 0xff + 5510 = 0 + coastal 5502 |= 2
 *   (sea && (val>0x100 || player) @ 1040:03f6) + farm 5502 |= 8 (tile 0xb/5)
 *   + seen 0x19/0x1d-tile diamond (x+0x50 layer, gracz vs AI) + home jednostek
 *   (FoundCityIfHome resettle w DoMove, nie tu) + replay log + okupacja.
 *   Port ponizej LIVE 1:1 (no Win16, no globals); dialog/map ogony STUB. */

/* Orig slot scan (pierwszy 5502 == -1/0xff w 0x80) -> city_found_free_slot.
 * 1:1 indeks albo -1 (pelne/NULL/n<=0). valids to bajty 5502 (0xff wolny). */
int city_found_free_slot(const int *valids, int n);

/* Orig prod init (1; 4 gdy Tech 0x22, 5 gdy Tech 0x40) ->
 * city_found_initial_prod. 1:1 5 > 4 > 1 (oba owned -> 5). */
int city_found_initial_prod(int has_t34, int has_t64);

/* Orig first-city (brak innego miasta tego civ) -> city_found_is_first.
 * 1:1 1 gdy pierwsze (palace 54fc = 1), 0 wpp. owners to 5507 bajty,
 * valids to 5502 (0xff skip); exclude to wlasny idx (ignorowany). */
int city_found_is_first(const int *owners, const int *valids, int n,
                        int civ, int exclude);

/* Orig name-trim (0x1f w dol while ' ') -> city_found_trim_len. 1:1
 * dlugosc bez koncowych ' ' (0..len). s moze byc NULL -> 0. */
int city_found_trim_len(const char *s, int len);

/* --- Chain 2026-10-16 (30): Tech_ChooseResearch kernels (task goal 4) ---
 * Orig Tech_ChooseResearch @ 1170:21b3 (0x896 B; dla OBOJGA: AI bierze
 * wazone losowanie, gracz dialog MenuZ; wola Tech_GrantTech @ 1170:2aaa po
 * wyborze + FUN_1078_4ca5 broadcast; callers: NewGame_RateStartPositions
 * @ 10e8:1d48 rating-loop (Res 0x82 append), Game_DoPollutionAndWonderRace
 * @ 1088:1e32 (AI wonder/tech?), Pic_PlayShared @ 1030:0000):
 *   eligible: 0x48 techow: !HasTech(t) && HasTech(pre1 @ 1894) &&
 *   HasTech(pre2 @ 1895) (pre -1 = unconditional via HasTech(-1)==1).
 *   Kryteria: koszt Tech_CalcCost @ 1170:31ec (base0 @ +0x90 + base1 @ +0x91
 *   * civ_factor @ 1ca4[civ]) -> roll = Rng_Range(koszt<<2), MAX roll
 *   wygrywa (local_c < roll; nie min-cost! drozsze waza wiecej — AI pcha
 *   drogie techy); wykluczenie biezacego badania gracza (12d0:0062 != t ||
 *   player != civ). Era/drzewo: tylko via pre1/pre2 lancuch (brak ery wprost;
 *   Future 0x47 to fallback gdy brak eligible: dialog "Future Technology"
 *   + 12a8:0022++). Losowosc: Rng_Range wszedzie (AI roll + gracz 50/50
 *   Tech(0x14) wise/scientists + hut Tech shuffle).
 *   Zwiazek: Tech_HasTech @ 1170:3181 (bit-predykat LIVE) + Tech_FindFirstOwner
 *   @ 1030:7ece (pierwszy owner 1..7, else discover) + Tech_CalcCost (GetTechValue/
 *   Cost: base0/base1 to koszt-tabela) + tech_names (NIE tabela: live nazwy via
 *   ResMgr_GetString @ 1100:078a entry 0x82 przez Res_GetStringCopy @ 11f8:0080).
 *   Port ponizej LIVE 1:1 (no Win16, no globals); dialog/MenuZ ogon STUB. */

/* Orig Tech_CalcCost @ 1170:31ec -> tech_calc_cost. 1:1 base0 + base1*factor
 * (factor to 1ca4[civ] tribe-factor; ujemne dozwolone, orig signed char). */
int tech_calc_cost(int base0, int base1, int civ_factor);

/* Orig eligible (!owned && pre1 && pre2) -> tech_research_eligible. 1:1. */
int tech_research_eligible(int not_owned, int pre1_owned, int pre2_owned);

/* Orig MAX-roll pick (local_c < roll && (active != t || player != civ)) ->
 * tech_research_roll_is_better. 1:1 1 gdy lepszy. */
int tech_research_roll_is_better(int roll, int best, int cur_tech,
                                 int active_tech, int civ, int player_civ);

/* Orig Future-fallback (0x47) -> tech_grant_is_future. 1:1 tech == 0x47. */
int tech_grant_is_future(int tech);

/* Orig ESCHISM bit (word tech/0x10, mask 1<<(tech%0x10)) -> tech_bit_word /
 * tech_bit_mask. 1:1 word 0..4 (0x47 Future OOR -> word 4/mask poza tabela,
 * caller gate via is_future). tech < 0 -> -1. */
int tech_bit_word(int tech);
int tech_bit_mask(int tech);

/* --- Chain 2026-10-16 (task): Hut/Tech/Eliminate/Pedia/Gov kernels ---
 * Orig (civ.exe, x86:LE:16:Protected Mode):
 *   Hut_ProcessHut @ 11c0:2677 (jedyny caller Civ_ProcessUnitMoves @
 *     11f0:0000 site 11f0:36a9; Rng_Range(4) case 0..3 + default-fallback):
 *     case 0 advanced-tribe: City_FindNearest + Map_WrapStepDist, dist > 3
 *       i (tile(x+0x50,y) & 7) > 4 -> City_FoundCity(civ,0,x,y,1) + reveal;
 *     case 1 tech-shuffle: turn != 0 && year < 0x3e9 -> do 0x3e6 prob:
 *       !HasTech(t) && HasTech(pre1 @ 1894) && HasTech(pre2 @ 1895) ->
 *       Tech_GrantTech(civ,t,0); case 2 gold: treasury(43b8+0x8c) += 0x32;
 *     case 3 barbs: dist > 3 && 3efe != 0 -> loop 1..8 step
 *       Util_Clamp(4-3efe,1,4): Map_GetUnitAt == -1 && occup & 1 == 0 &&
 *       class != 10, koszt(17d4) < 3 ? 6 : 3, Unit_SpawnUnit(0,kind);
 *     default/4 friendly: Unit_SpawnUnit(civ, Rng_Range(2) == 0 ? 6 : 3).
 *   Tech_GrantTech @ 1170:2aaa (ESCHISM+5+civ*10 bit + 0x48-tabela 14000
 *     + wonder-cancel Replay 5,2 + Great Library 0xfffe spread; wolaja:
 *     NewGame_InitCivState @ 10e8:1583, Tech_ChooseResearch @ 1170:22ee/
 *     26b3, self 1170:3172, Diplo_OpenContactDialog x3, Tech_GrantConquestTech
 *     (was FUN_1038_09bc) x2, Diplo_ExchangeTechs x2, Hut_ProcessHut @
 *     11c0:2952, Diplomat_CityAction @ 11b8:02c1).
 *   Game_EliminateCiv @ 10e8:1b97 (civ != 0 && != player, miasta -> early;
 *     msg destroyed + Game_ClearEliminatedCivFlags (was FUN_1038_083d) +
 *     Replay 0xd,2 + FoundCityIfHome resettle + seen 0x50x0x32 &= ~(1<<civ)
 *     + NewGame_InitCivState; wolaja: Pic_PlayShared, FUN_1078_0000,
 *     Unit_DoMove @ 1038:082c, Civ_ProcessUnitMoves @ 11f0:27b1).
 *   Tech_CalcCost @ 1170:31ec (base0[+0x90] + base1[+0x91] * 1ca4[civ];
 *     callerzy: Tech_ChooseResearch @ 1170:2239, Diplo_OpenContactDialog x2,
 *     Tech_GrantConquestTech x2) + Tech_ChooseResearch MAX-roll (koszt<<2).
 *   Pedia_ShowEntry @ 1068:06af switch 1..5 (1 tech +0x44c, 2 build
 *     param+1<0x19?2:3 + 0xd07/0xd08 via HasTech(0x25), 3 unit +0x4b0,
 *     4 terrain via Pedia_FindTerrainIndex (was FUN_1020_011b) +0x3f5/6,
 *     5 pusty; caller Pedia_ShowByCategory (was FUN_1020_0000: sel 0..4 ->
 *     Res 0x82..0x86)).
 *   Gov_AnnounceChange @ 10e8:32c1 (gov = 3b77+0x13+civ*2 != 0 -> Res 0x8d
 *     msg; caller Diplo_ChangeGovernment @ 1168:082b).
 * OS-free reguly ponizej LIVE 1:1 (no Win16, no globals, read-only); dialog/
 * map/city-state ogony STUB z TODO Orig. No exit(), C17. */

/* Orig Hut_ProcessHut @ 11c0:2677 case 0 -> hut_found_city_site_ok.
 * 1:1 dist > 3 && (tile_flags & 7) > 4 (tile = Map_GetTile(x+0x50,y)). */
int hut_found_city_site_ok(int dist, int tile_flags);

/* Orig Hut_ProcessHut @ 11c0:2677 case 1 gate -> hut_tech_shuffle_ok.
 * 1:1 turn != 0 && year < 0x3e9. */
int hut_tech_shuffle_ok(int turn, int year);

/* Orig Hut_ProcessHut @ 11c0:2677 case 1 pick -> hut_tech_eligible.
 * 1:1 !owned && pre1 && pre2 (pre -1 = unconditional via HasTech(-1)). */
int hut_tech_eligible(int not_owned, int pre1_owned, int pre2_owned);

/* Orig Hut_ProcessHut @ 11c0:2677 case 2 -> hut_gold_bonus. 1:1 0x32. */
int hut_gold_bonus(void);

/* Orig Hut_ProcessHut @ 11c0:2677 case 3 gate -> hut_barbs_ok.
 * 1:1 dist > 3 && city_count_3efe != 0. */
int hut_barbs_ok(int dist, int city_count);

/* Orig Hut_ProcessHut @ 11c0:2677 case 3 step -> hut_barb_step.
 * 1:1 Util_Clamp(4-count,1,4) @ 11c8:0063 inlined. */
int hut_barb_step(int city_count);

/* Orig Hut_ProcessHut @ 11c0:2677 case 3 kind -> hut_barb_unit_kind.
 * 1:1 tile_cost(17d4[class]) < 3 ? 6 : 3. */
int hut_barb_unit_kind(int tile_cost);

/* Orig Hut_ProcessHut @ 11c0:2677 default -> hut_friendly_unit_kind.
 * 1:1 Rng_Range(2) == 0 ? 6 : 3. */
int hut_friendly_unit_kind(int rng2);

/* Orig Tech_GrantTech @ 1170:2aaa Future-gate -> tech_grant_stores_bit.
 * 1:1 tech != 0x47 (Future nie stawia ESCHISM-bitu ani 0x48-wpisu). */
int tech_grant_stores_bit(int tech);

/* Orig Tech_GrantTech @ 1170:2aaa Great Library src -> tech_grant_is_library_source.
 * 1:1 src == 0xfffe. */
int tech_grant_is_library_source(int src);

/* Orig Tech_GrantTech @ 1170:2aaa spread (local_a > 1) ->
 * tech_grant_library_spread_ok. 1:1 owners > 1. */
int tech_grant_library_spread_ok(int owners);

/* Orig Tech_GrantTech @ 1170:2aaa barracks-obsolete (54fc & 0xfffffffd,
 * tech 0x22/0x25) -> tech_grant_barracks_obsolete_clear. 1:1 cur & ~2. */
int tech_grant_barracks_obsolete_clear(int mask);

/* Orig Tech_GrantTech @ 1170:2aaa acquire-tekst -> tech_grant_from_other.
 * 1:1 civ != src && src != 0 (develop vs acquire vs library). */
int tech_grant_from_other(int civ, int src);

/* Orig Tech_ChooseResearch @ 1170:21b3 roll (Rng_Range(koszt<<2)) ->
 * tech_cost_roll_bound. 1:1 cost << 2 (cost = tech_calc_cost). */
int tech_cost_roll_bound(int cost);

/* Orig Game_EliminateCiv @ 10e8:1b97 gate -> game_eliminate_needed.
 * 1:1 civ != 0 && civ != player_civ. */
int game_eliminate_needed(int civ, int player_civ);

/* Orig Game_EliminateCiv @ 10e8:1b97 alive-clear -> game_eliminate_alive_clear.
 * 1:1 mask & ~(1 << (civ & 0x1f)); civ OOR (< 0) -> -1. */
int game_eliminate_alive_clear(int mask, int civ);

/* Orig Game_EliminateCiv @ 10e8:1b97 seen-clear (0x50 x 0x32 warstwa) ->
 * game_eliminate_seen_clear. 1:1 cur & ~(1 << (civ & 0x1f)); civ < 0 -> -1. */
int game_eliminate_seen_clear(int cur, int civ);

/* Orig Pedia_ShowEntry @ 1068:06af switch -> pedia_entry_kind_valid.
 * 1:1 kind 1..5. */
int pedia_entry_kind_valid(int kind);

/* Orig Pedia_ShowByCategory @ 1020:0000 sel -> pedia_res_for_selector.
 * 1:1 sel 0..4 -> 0x82 + sel (tech/build/produce/gov/unit); else -1. */
int pedia_res_for_selector(int sel);

/* Orig Pedia_ShowEntry @ 1068:06af case 2 -> pedia_build_variant.
 * 1:1 (param + 1) < 0x19 ? 2 : 3 (local_1a). */
int pedia_build_variant(int param);

/* Orig Pedia_FindTerrainIndex @ 1020:011b scan -> pedia_terrain_find_index.
 * 1:1 pierwszy i w 0..0xb z table[i] == id, else -1 (orig 0xffff);
 * NULL/n <= 0 -> -1. */
int pedia_terrain_find_index(const int *table, int n, int id);

/* Orig Gov_AnnounceChange @ 10e8:32c1 gate -> gov_announce_needed.
 * 1:1 gov != 0 (3b77+0x13+civ*2). */
int gov_announce_needed(int gov);

/* --- Chain 2026-10-16 (p5): ConquestTech transfer + EliminatedFlags ---
 * Orig Tech_GrantConquestTech @ 1038:09bc (civ.exe, x86:LE:16:Protected
 * Mode), decompiled this pass (body 09bc-0bb9; signature
 * (winner, loser); gate winner != 0 && loser != 0):
 *   scan tech 0x47..0 step -1: !Tech_HasTech(winner,t) &&
 *     Tech_HasTech(loser,t) -> lista local_9c[n] (cap 72 = 0x48 wpisow,
 *     tech_conquest_list_cap), koszt via Tech_CalcCost @ 1170:31ec
 *     (base0[+0x90] + base1[+0x91] * 1ca4[可能是winner]; zob. tech_calc_cost)
 *     + track max (local_8 = -1 init, local_a = tech przy cost > best).
 *   Ile techow? WSZYSTKIE ofiary, ktorych zdobywca nie ma (pelna lista do
 *     wyboru, nie 1; cap tylko rozmiarem tablicy 72). Ktore? te z listy
 *     (victim-exclusive). Cap kosztu? BRAK capu — max bierze najdroższy
 *     (CalcCost), nie najtańszy.
 *   Dialog: player == winner -> petla wyboru az local_c != -1
 *     (Dialog_ShowMessage @ 1220:00d2 gdy DAT_1358_0008 == -1, inaczej
 *     Diplo_RunContactMenu @ 1160:328e) + Tech_GrantTech(winner,
 *     lista[wybor], loser); player == loser -> komunikat z najdroższym
 *     (local_a) + Tech_GrantTech(winner, local_a, loser); AI vs AI ->
 *     po cichu Tech_GrantTech(winner, local_a, loser) +
 *     City_NotifyCivCities(winner, 0xffff) (was FUN_1078_4ca5, renamed this
 *     pass: broadcast po wszystkich miastach civ; callee tez Tech_
 *     ChooseResearch @ 1170:21b3 + FUN_1078_0000).
 *   Callerzy (verified this pass): Unit_DoMove @ 1038:0000 (tail po
 *     Map_SetTileOwner: GrantConquestTech(winner,lastOwner) przy zdobyciu
 *     miasta) + Diplo_OpenContactDialog @ 1160:0000 x2 (sciezki
 *     trybutu/darowizny: CalcCost x2 + GrantTech x3 + GrantConquestTech).
 *   Zwiazek z Diplo_ExchangeTechs @ 1168:1220 (JEDEN call site w
 *     Diplo_PostContact @ 1168:0a07, "x2" = DWIE strony wymiany: GrantTech
 *     w obie strony gdy uVar3 == 3, tj. A-exclusive && B-exclusive wsrod
 *     0..0x48 via Tech_HasTech; gates: obie AI, (a+b+turn)&3 == 0 lub
 *     first-contact, brak 0x08, ShouldMakePeace + wonder-7/count; zob.
 *     diplo_should_exchange w dialog.h): Exchange = SYMETRYCZNA wymiana
 *     1-za-1 (ostatnie exclusive z kazdej strony), Conquest = ASYMETRYCZNY
 *     transfer (calosc victim-exclusive, wybor gracza lub max-cost).
 *   Zwiazek z Tech_ChooseResearch @ 1170:21b3 (callerzy verified this
 *     pass: NewGame_RateStartPositions @ 10e8:1d48 rating-loop,
 *     Game_DoPollutionAndWonderRace @ 1088:1e32, Pic_PlayShared @
 *     1030:0000): ChooseResearch WYBIERA nastepny research (MAX-roll
 *     koszt<<2, zob. tech_cost_roll_bound), Conquest PRZENOSI gotowe techy
 *     po zdobyciu (bez rolla; roll tylko w wyborze gracza via dialog).
 *   Zwiazek z Unit_DoMove @ 1038:082c (site INSIDE Unit_DoMove @
 *     1038:0000 body 0000-083c, nie osobna funkcja): tail po zdobyciu —
 *     Game_EliminateCiv(lastOwner, winner) przy pustym miescie
 *     (city-size 0) + GrantConquestTech + Map_SetTileOwner; ClearEliminated
 *     ponizej wywolywane z Unit_DoMove gdy 54fc&1 (flaga miasta) oraz z
 *     Game_EliminateCiv @ 10e8:1b97.
 * OS-free reguly ponizej LIVE 1:1 (no Win16, no globals, read-only);
 * dialog/GrantTech ogony STUB z TODO Orig. No exit(), C17. */

/* Orig gate (param_1 != 0 && param_2 != 0) -> tech_conquest_gate.
 * 1:1 1 gdy transfer (oba civ != 0; 0 = barbarians/brak). */
int tech_conquest_gate(int winner, int loser);

/* Orig Tech_GrantConquestTech @ 1038:09bc lista local_9c cap (0x48 techow)
 * -> tech_conquest_list_cap. 1:1 72. */
int tech_conquest_list_cap(void);

/* Orig Tech_GrantConquestTech @ 1038:09bc eligible (!HasTech(w) && HasTech(l))
 * -> tech_conquest_eligible. 1:1 winner_lacks != 0 && loser_has != 0. */
int tech_conquest_eligible(int winner_lacks, int loser_has);

/* Orig Tech_GrantConquestTech @ 1038:09bc max-track (local_8 < cost) ->
 * tech_conquest_cost_is_better. 1:1 cost > best (init best =
 * tech_conquest_initial_best() = -1). */
int tech_conquest_cost_is_better(int cost, int best);

/* Orig Tech_GrantConquestTech @ 1038:09bc init local_8 = -1 ->
 * tech_conquest_initial_best. 1:1 -1. */
int tech_conquest_initial_best(void);

/* Orig Tech_GrantConquestTech @ 1038:09bc "czy sa techy" (local_6 != 0) ->
 * tech_conquest_has_any. 1:1 count != 0. */
int tech_conquest_has_any(int count);

/* Orig Tech_GrantConquestTech @ 1038:09bc player == winner (dialog wyboru)
 * -> tech_conquest_player_picks. 1:1 player_civ == winner. */
int tech_conquest_player_picks(int player_civ, int winner);

/* Orig Tech_GrantConquestTech @ 1038:09bc player == loser (komunikat ofiary)
 * -> tech_conquest_player_loses. 1:1 player_civ == loser. */
int tech_conquest_player_loses(int player_civ, int loser);

/* --- Orig Game_ClearEliminatedCivFlags @ 1038:083d (was FUN_1038_083d) ---
 * Decompiled this pass (body 083d-08ff; callee Game_EliminateCiv @
 * 10e8:1b97 + Unit_DoMove @ 1038:0000 site 54fc&1):
 *   msg: tribe (loser*0x20+0x3da) + 0x746 " spaceship " + wariant:
 *     high-bit (1 << (civ+8)) SET? nie: brak msg, tylko clear.
 *     high SET + low (1 << civ) CLEAR -> 0x764 "construction cancelled.";
 *     high SET + low SET -> 0x752 "returns to Earth."; potem
 *     Dialog_ShowMessage(0x24fa, 100, 0x50).
 *   clear: DAT_12d0_00b6 &= ~(1 << civ) & ~(1 << (civ+8)) + NewGame_
 *     ResetCivData(civ) (was FUN_10e8_3368, renamed this pass: slot
 *     *(civ*2+0x752) = 0xffff + 0xf x 0xc bajtow 0xff od civ*0xb4+0x1b2;
 *     caller tez NewGame_Wizard @ 10e8:0000).
 * Ktore flagi (alive? kontakt? wojna?): ANI alive (to 12d8:42f8, zob.
 *   game_eliminate_alive_clear) ANI kontakt/wojna/pokoj (to 12d8:3a0a
 *   0x01/0x02/0x40, zob. diplo_* w dialog.h) — to PARA BITOW slowa
 *   12d0:00b6 (statek kosmiczny): low 1<<civ (status statku: SET =
 *   "returns", CLEAR = "cancelled") + high 1<<(civ+8) (pending-msg?:
 *   SET = pokaz msg). Dokladna semantyka TBD vs decompiler (TODO), ale
 *   mechanika bitow + wariant msg LIVE 1:1 ponizej.
 * Callee Game_EliminateCiv @ 10e8:1b97 (decompiled this pass): gate
 *   civ != 0 && != player + miasta? early-return; msg " civilization
 *   destroyed by " @ 1420:2b05 + "!" @ 0x2b21 + ClearEliminated + Replay
 *   0xd,2 + FoundCityIfHome-resettle + seen 0x50x0x32 &= ~(1<<civ) +
 *   NewGame_InitCivState; callerzy: Pic_PlayShared, FUN_1078_0000,
 *   Unit_DoMove @ 1038:082c-tail, Civ_ProcessUnitMoves @ 11f0:27b1. */

/* Orig Game_ClearEliminatedCivFlags @ 1038:083d maska 1 << civ ->
 * game_elimflag_low_mask. 1:1; civ < 0 -> -1. */
int game_elimflag_low_mask(int civ);
/* Orig Game_ClearEliminatedCivFlags @ 1038:083d maska 1 << (civ+8) ->
 * game_elimflag_high_mask. 1:1; civ < 0 -> -1. */
int game_elimflag_high_mask(int civ);

/* Orig Game_ClearEliminatedCivFlags @ 1038:083d msg-gate (00b6 & high) != 0
 * -> game_elimflag_msg_needed. 1:1. */
int game_elimflag_msg_needed(int flags_00b6, int civ);

/* Orig Game_ClearEliminatedCivFlags @ 1038:083d wariant
 * ((00b6 & low) == 0 ? 0x764 : 0x752) -> game_elimflag_msg_id. 1:1 0x764
 * (cancelled) gdy low CLEAR, 0x752 (returns) gdy low SET; -1 gdy civ < 0.
 * Tresci: 0x746 " spaceship ", 0x752 "returns to Earth.",
 * 0x764 "construction cancelled." (1420:0746/0752/0764, dump-verified
 * this pass). */
int game_elimflag_msg_id(int flags_00b6, int civ);

/* Orig Game_ClearEliminatedCivFlags @ 1038:083d clear (& ~low & ~high) ->
 * game_elimflag_clear. 1:1; civ < 0 -> -1. */
int game_elimflag_clear(int flags_00b6, int civ);

/* Orig City_CalcPlunderValue @ 1038:0901 (was FUN_1038_0901, renamed this
 * pass; callee Unit_DoMove @ 1038:0000): overflow-guard 32000/size:
 *   treasury < 32000/size ? (treasury*size)/(div+1)
 *                         : (treasury/(div+1))*size
 * (treasury = 43b8+0x8c[owner], size = 5503[city] char, div =
 * *(owner*2+0x3ebe)). OS-free 1:1 na jawnych parametrach; size <= 0 lub
 * div < 0 -> -1. No globals. */
int city_plunder_value(int treasury, int city_size, int divisor);

/* --- Game end / uprising / pollution / status math (2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Game_ProcessGameEnd @ 1088:0b6d (end-of-game HoF path: FUN_1188_07ad +
 *     Replay_OpenOrPlay @ 1180:0000 unless quit == 2, then Hof_CalcFinalScore
 *     @ 1040:1137 + HoF_VictoryCeremony/Load/TryInsert/Save unless
 *     DAT_12d0_00b6 & 0x100; called 2x from Game_TurnLoopTick @ 1088:0000).
 *   Game_SpawnUprisings @ 1088:0be9 (head gate (5-diff)*0x1e vs turn, then
 *     every-8th wave ((turn+1)&7)==0 && (DAT_12d8_2b74&7)==0 with tile loop
 *     Map_GetTileClass @ 1110:16c0 == 10 ocean + Map_GetUnitAt @ 1110:18ce
 *     == -1 + Map_GetOceanOrContVal @ 1110:1dbd >= 0x10, leader via
 *     Util_Clamp(turn/0x96+1,1,3) @ 11c8:0063 (<3 -> 0x11 else 0x12),
 *     Unit_SpawnUnit @ 11c0:12c8; second half is the city-plunder revolt
 *     scan with the 0x62/99 retry caps + Travellers/Barbarian/Native/
 *     Guerrilla message + Dialog_ShowMessage @ 1220:00d2 tail).
 *   Game_DoPollutionAndWonderRace @ 1088:1e32 (pollution head via
 *     Util_SignStep @ 11c8:003d + Util_Clamp @ 11c8:0063 on DAT_12d8_42f0;
 *     alarm when 42f0 == 0x0c && 42f4 > 6; spread via Map_SpreadPollution
 *     @ 1028:1afd when 0x10 < 42f0, then 42f0 = 0; wonder-race loop over
 *     rounds 0..difficulty with Rng_Range(0x15)+1 picks, see worldgen.h).
 *   Game_UpdateStatusPanel @ 1088:13d5 (progress icon via Util_Clamp @
 *     11c8:0063 of (science<<2)/((diff*2+base+6)*trade*year_factor+1) to
 *     0..3 with year_factor 1 (year<0) / 2, icon via DAT_1420_98e4; then
 *     pollution icons when DAT_12d8_42f0 != 0: variant 42f0%4 + count
 *     Util_Clamp(42f0/4,0,3); treasury row (10-lux-tax); pop milestone
 *     popup via Dialog_ShowMessage @ 1220:00d2).
 * OS-free gates below are LIVE 1:1 on explicit params (no globals, no
 * Win16, read-only); dialog/GDI/map/city-state tails stay STUB in
 * game_process_game_end / game_spawn_uprisings /
 * game_do_pollution_and_wonder_race / game_update_status_panel with TODO
 * Orig refs. No exit(), C17. */

/* Orig Game_ProcessGameEnd @ 1088:0b6d replay gate -> game_end_needs_replay.
 * 1:1 (g_nQuitFlag @ 12d8:24ee != 2) -> Replay_OpenOrPlay @ 1180:0000 runs.
 * Returns nonzero (1) when the replay teardown runs. No globals. */
int game_end_needs_replay(int quit_flag);

/* Orig Game_ProcessGameEnd @ 1088:0b6d ceremony gate -> game_end_shows_ceremony.
 * 1:1 (DAT_12d0_00b6 & 0x100) == 0 -> Hof_CalcFinalScore @ 1040:1137 +
 * HoF_VictoryCeremony/Load/TryInsert/Save runs. Returns nonzero (1) when
 * the ceremony path runs. No globals. */
int game_end_shows_ceremony(int flags_00b6);

/* Orig Game_SpawnUprisings @ 1088:0be9 tile loop -> game_uprising_tile_ok.
 * 1:1 Map_GetTileClass @ 1110:16c0 == 10 (ocean spawn tile) &&
 * Map_GetUnitAt @ 1110:18ce == -1 (empty) && Map_GetOceanOrContVal @
 * 1110:1dbd >= 0x10. Returns nonzero (1) when the tile accepts a spawn.
 * No globals. */
int game_uprising_tile_ok(int tile_class, int unit_at, int cont_val);

/* Orig Game_SpawnUprisings @ 1088:0be9 leader pick -> game_uprising_leader.
 * 1:1 Util_Clamp(g_nTurn/0x96+1,1,3) @ 11c8:0063 (inline here, no sys link
 * beyond util_clamp in sys.h); <3 -> 0x11 else 0x12 (leader unit type for
 * Unit_SpawnUnit @ 11c0:12c8). No globals. */
int game_uprising_leader(int turn);

/* Orig Game_SpawnUprisings @ 1088:0be9 wave gate -> game_uprising_wave_due.
 * 1:1 ((g_nTurn+1) & 7) == 0 (every 8th turn; the DAT_12d8_2b74 & 7
 * conjunct is game-state owned and stays caller-side). Returns nonzero
 * (1) when the wave turn fires. No globals. */
int game_uprising_wave_due(int turn);

/* Orig Game_DoPollutionAndWonderRace @ 1088:1e32 spread gate ->
 * game_pollution_spread_due. 1:1 0x10 < DAT_12d8_42f0 (then DAT_12d8_42f2++,
 * Map_SpreadPollution @ 1028:1afd, DAT_12d8_42f0 = 0). Returns nonzero (1)
 * when the spread fires. No globals. */
int game_pollution_spread_due(int counter);

/* Orig same alarm gate -> game_pollution_alarm_due. 1:1 DAT_12d8_42f0 ==
 * 0x0c && 6 < DAT_12d8_42f4 (smoke counter; "Scientists alarmed" message
 * @ 1420:1744 + Status_FormatMainRow @ 1138:0000 tail stays Win16).
 * Returns nonzero (1) when the alarm fires. No globals. */
int game_pollution_alarm_due(int counter, int smoke);

/* Orig Game_UpdateStatusPanel @ 1088:13d5 pollution variant ->
 * game_status_pollution_icon. 1:1 DAT_12d8_42f0 % 4 (drawn when 42f0 != 0
 * via Gfx_DrawTechIcon @ 11a8:0cb1; x86 IDIV truncates toward zero, same
 * as C %). Negative counters clamp to 0 (orig used non-negative game
 * values). No globals. */
int game_status_pollution_icon(int counter);

/* Orig same pollution count -> game_status_pollution_count. 1:1
 * Util_Clamp(DAT_12d8_42f0/4,0,3) @ 11c8:0063 (util_clamp in sys.h).
 * Negative counters clamp to 0 (same contract as above). No globals. */
int game_status_pollution_count(int counter);

/* Orig Game_UpdateStatusPanel @ 1088:13d5 progress icon ->
 * game_status_progress_index. 1:1 Util_Clamp((science<<2) /
 * ((difficulty*2+base+6)*trade*year_factor+1),0,3) @ 11c8:0063 where
 * year_factor = (year < 0) ? 1 : 2 (g_nYear @ 12d8:42fa sign select at
 * @ 1088:1554); science is DAT_43b8+0x7c[civ], base is DAT_12a8_0020,
 * trade is DAT_3f5c+2[civ]. Negative science/difficulty/base/trade clamp
 * to 0 (orig used non-negative game values). Returns 0..3, the index
 * into DAT_1420_98e4 for Gfx_DrawTechIcon @ 11a8:0cb1. No globals. */
int game_status_progress_index(int science, int difficulty, int base,
                               int trade, int year);

/* --- Turn-loop gates + tech-icon budget + demo rows (2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Game_TurnLoopTick @ 1088:0000 disaster gate (g_nTurn @ 12d8:42fe >
 *     0x32 -> 2x City_ProcessDisaster @ 1040:068e with Rng_Range @
 *     11c8:003d 0x80 draws) + treasury<100 memory-warning branch
 *     (Dialog_ShowMessage @ 1220:00d2 via FUN_11d0_01f7).
 *   Status_DrawTechIcons @ 1088:1a82 budget gates (count <=
 *     Util_Clamp(param2+3,3,0x24) @ 11c8:0063, height <= count/5, <= 3;
 *     999-try cap + adjacency stay caller-side in the STUB).
 *   Demo_ShowDemographics @ 1188:123a rows: Military ((3eee count*10)/pop),
 *     Productivity (output*10/pop), Family Size ((food*0x28)/pop+0x14,
 *     then x/10 + "." + x%10 + " children"), Life Expectancy
 *     (Util_Clamp(impr,0x14,99) @ 11c8:0063).
 * OS-free 1:1 on explicit params (no globals, no Win16, read-only, C17);
 * GDI/dialog/city-state tails stay STUB with TODO Orig refs. No exit(). */

/* Orig Game_TurnLoopTick @ 1088:0000 disaster gate -> game_turn_disaster_due.
 * 1:1 g_nTurn > 0x32 fires the 2x City_ProcessDisaster @ 1040:068e pass.
 * Returns nonzero (1) when disasters run. No globals. */
int game_turn_disaster_due(int turn);

/* Orig Game_TurnLoopTick @ 1088:0000 treasury branch ->
 * game_turn_treasury_warn. 1:1 treasury < 100 takes the memory-warning
 * branch (Dialog_ShowMessage @ 1220:00d2 via FUN_11d0_01f7 tail stays
 * Win16 caller-side). Returns nonzero (1) when the warning fires. */
int game_turn_treasury_warn(int treasury);

/* Orig Status_DrawTechIcons @ 1088:1a82 budget gate -> status_tech_count_ok.
 * 1:1 count <= Util_Clamp(progress+3,3,0x24) @ 11c8:0063 (inline here, no
 * link; progress is param2). Returns nonzero (1) when the count fits.
 * Negative counts never fit (-> 0). No globals. */
int status_tech_count_ok(int count, int progress);

/* Orig same height gate -> status_tech_height_ok. 1:1 height <= count/5
 * && height <= 3 (count/5 truncates toward zero, same as x86 IDIV).
 * Returns nonzero (1) when the height fits. Negative heights never fit.
 * No globals. */
int status_tech_height_ok(int height, int count);

/* Orig Demo_ShowDemographics @ 1188:123a Military row -> demo_military_rate.
 * 1:1 (count*10)/pop (count is the 3eee unit total). Returns the rate,
 * -1 on pop <= 0 (orig divided live totals; caller sums 0x80 records).
 * No globals. */
int demo_military_rate(int count, int pop);

/* Orig same Productivity row -> demo_productivity. 1:1 output*10/pop.
 * Returns the rate, -1 on pop <= 0. No globals. */
int demo_productivity(int output, int pop);

/* Orig same Family Size row -> demo_family_raw. 1:1 (food*0x28)/pop+0x14
 * (callers split x/10 + "." + x%10 + " children" for display).
 * Returns the raw value, -1 on pop <= 0. No globals. */
int demo_family_raw(int food, int pop);

/* Orig same Life Expectancy row -> demo_life_clamp. 1:1
 * Util_Clamp(impr,0x14,99) @ 11c8:0063 (inline here, no link; impr is the
 * improvement total). Always returns the clamped value. No globals. */
int demo_life_clamp(int impr);

/* --- Turn-loop frontier gates (2026-10-16) ---
 * Parents stay STUB (Win16/GDI/dialog tails); OS-free heads below are
 * LIVE 1:1 on explicit params (no globals, no Win16, no exit()).
 * Ref: unpacked/civ.exe (read-only; e.g. "00,000 citizens." @ 1420:172b
 * verified via strings, never edited). */

/* Orig City_ProcessDisaster @ 1040:068e city pick (2x per
 * Game_TurnLoopTick @ 1088:0000 pass once g_nTurn > 0x32, Rng_Range @
 * 11c8:003d 0x80 draws caller-side) -> city_disaster_city_ok. 1:1:
 * valid (@ 1420:5502, 0xff/-1 = empty slot) != empty && pop (@ 1420:3efe)
 * > 1 && size (@ 1420:5503) > 4. Returns 1 eligible / 0 skip. */
int city_disaster_city_ok(int valid, int pop, int size);

/* Orig same disaster switch (Rng_Range(10) @ 11c8:003d) building-flag
 * checks on the 0x1c city mask @ 1420:54fc (bits 0x10/0x100/0x80/0x08/
 * 0x04/0x02/0x40) -> city_disaster_has_building. 1:1: nonzero (1) when
 * (mask & bit) != 0 (bit is one of the 54fc bits above); bit == 0 never
 * matches (-> 0). Messages @ 1420:07f0/0839 + viewer Pic_PlayShared @
 * 1030:0000 + dialog Dialog_ShowMessage @ 1220:00d2 + Map_CenterOnTile @
 * 1110:03ae tails stay in city_process_disaster (STUB). */
int city_disaster_has_building(unsigned mask, unsigned bit);

/* Orig Game_UpdateStatusPanel @ 1088:13d5 milestone popup gate ->
 * game_status_milestone_due. 1:1: pop tier rose (tier > prev_tier) and
 * DAT_1420_14e6 > -2, then Dialog_ShowMessage @ 1220:00d2 with the
 * "00,000 citizens." @ 1420:172b tail (dialog tail stays Win16 in
 * game_update_status_panel, STUB). Returns 1 fire / 0 silent. */
int game_status_milestone_due(int prev_tier, int tier, int flag14e6);

/* Orig Status_DrawTechIcons @ 1088:1a82 AI-fake pick ->
 * status_lcg_pick. 1:1: (seed & 0xff) % 9 icon slot (seed iterates via
 * status_lcg_next: seed = seed*5+1 from DAT_13c0_0000*civ; 999-try cap +
 * adjacency/budget gates via status_tech_count_ok/height_ok stay
 * caller-side in status_draw_tech_icons, STUB). Returns 0..8. */
int status_lcg_pick(unsigned seed);

/* Orig Gfx_DrawTechIcon @ 11a8:0cb1 range -> gfx_tech_icon_valid. 1:1:
 * 9-icon blitter index 0..8 (caller status_draw_tech_icons @ 1088:1a82
 * advances per slot; SELECTPALETTE + BITBLT tail stays Win16 in
 * gfx_draw_tech_icon, STUB). Returns 1 valid / 0 OOR. */
int gfx_tech_icon_valid(int index);

/* Orig Unit_FoundCityIfHome @ 11c0:16a4 join gate -> unit_found_join_ok.
 * 1:1: type == 0x1a (settler, 1420:62ff) && home (+10/6307) == city_idx
 * takes the join path (consume settler + city_grow, LIVE in
 * unit_do_found_city); else the new-city path (map tile claim via
 * map_get_tile/map_set_tile @ 11e8:0dcf/0e19 + 1420:5500 alloc + Win16
 * dialog, UNSUPPORTED tail). Returns 1 join / 0 new-city. */
int unit_found_join_ok(int type, int home, int city_idx);

/* Orig City_SpotReveal @ 11c0:0000 record gate -> city_spot_valid. 1:1:
 * valid (@ 1420:5502, 0xff/-1 = empty) != empty (sibling
 * City_FindByTile @ 11c8:00c4 scans 0x80 0x1c records the same way;
 * seen-bit/diplomacy/minimap tails stay Win16 in city_spot_reveal,
 * STUB). Returns 1 city present / 0 empty. */
int city_spot_valid(int valid);

/* Orig Unit_ShowInfoPopup @ 11f0:468c occupancy tail ->
 * unit_popup_has_pollution. 1:1: (occ & 0x40) != 0 draws the Pollution
 * @ 1420:4845 line (Map_GetOccupancyFlags @ 1110:199b bits; siblings
 * 0x10 Railroad @ 481d else 0x08 @ 4828, 0x02 Irrigation @ 482f else
 * 0x04 Mining @ 483c, 0x01 city-guard badge path; GDI DrawDirect + h
 * step tails stay in unit_show_info_popup, STUB). Returns 1/0. */
int unit_popup_has_pollution(unsigned occ_flags);

/* --- Packet 2026-10-16 (game-only): unit/city/demo/diplo OS-free kernels ---
 * Parents stay STUB (Win16/GDI/dialog tails with TODO Orig in game.c);
 * predicates below are LIVE 1:1 on explicit params (no globals, no Win16,
 * no exit(), read-only, C17). Ref: unpacked/civ.exe read-only (strings
 * "(RailRoad)"/"(Irrigation)"/"(Mining)"/"(Pollution)" + "00,000 citizens."
 * verified via strings; never edited, civ1win/ untouched). */

/* Orig Unit_WaitForOrders @ 11c0:0ec2 blink clock -> unit_wait_blink_phase.
 * 1:1: Sys_GetTickScaled % 0x3c phase gate (0x1e split): 0 = sprite phase
 * (Unit_DrawSingle @ 1110:0dba path), 1 = "End of Turn/Press Return" card
 * phase (Text_DrawDirect @ 10d8:00e0 + GfxPort_PresentRect @ 10d0:0854).
 * Takes the tick explicitly; negative ticks never occur in orig (guarded
 * to phase 0 here). No globals. */
int unit_wait_blink_phase(int tick);

/* Orig Unit_MaybeCenterView @ 11f0:44d4 gate -> unit_should_center_view.
 * 1:1: Map_RevealCenter @ 1110:1461 runs only when the unit belongs to
 * the player (civ == player_civ) or is seen by the player (seen-by mask
 * @ 6305 bit 1<<player set). seen_mask is the +8 seen-by byte; player
 * outside 0..31 falls back to the civ-equality test only. Returns 1
 * center / 0 skip. No globals. */
int unit_should_center_view(int civ, int player_civ, unsigned seen_mask);

/* Orig Unit_ShowInfoPopup @ 11f0:468c occupancy bits (Map_GetOccupancyFlags
 * @ 1110:199b) -> unit_popup_has_railroad/river/irrigation/mine. 1:1 bit
 * tests: 0x10 Railroad @ 1420:481d, 0x08 river? @ 1420:4828, 0x02
 * Irrigation @ 1420:482f, 0x04 Mining @ 1420:483c ("(RailRoad)"/
 * "(Irrigation)"/"(Mining)" strings verified in unpacked/civ.exe). The
 * else-pairing (railroad else river, irrigation else mining) + DrawDirect
 * + h-step tails stay caller-side in unit_show_info_popup (STUB).
 * Returns 1 present / 0 absent. No globals. */
int unit_popup_has_railroad(unsigned occ_flags);
int unit_popup_has_river(unsigned occ_flags);
int unit_popup_has_irrigation(unsigned occ_flags);
int unit_popup_has_mine(unsigned occ_flags);

/* Orig Diplomat_CityAction @ 11b8:0000 case 2 steal gate ->
 * diplomat_steal_blocked. 1:1: city valid word @ 1420:5502 & 0x20 means
 * tech already stolen from this city -> sabotage/steal path blocked.
 * Returns 1 blocked / 0 allowed. No globals. */
int diplomat_steal_blocked(unsigned city_flags);

/* Orig City_DoRevolt @ 11b8:0f26 auto-pick scan -> city_revolt_score. 1:1:
 * score = (v00de - v00e0) * 0x20 / dist (orig (00de-00e0)*0x20/dist max;
 * -1 (auto) picks the best foreign city by this score). Caller runs the
 * argmax + seen-bit flips (x*0x32+y+10 + 0x14 diamond) + Unit_DoMove tail
 * (STUB in city_do_revolt). Returns the score, -1 on dist <= 0 (orig used
 * positive dist; guard is a Linux-side addition). No globals. */
int city_revolt_score(int v00de, int v00e0, int dist);

/* Orig Demo_ShowDemographics @ 1188:123a Pollution row ->
 * demo_pollution_index. 1:1: smoke - smoke*10/pop (smoke is the per-civ
 * smoke total, pop the live City_CalcPopTotal @ 11c8:0334 sum; AI rows are
 * live sums, not LCG fakes). Returns the index, -1 on pop <= 0 (orig
 * divided live totals >= 1). No globals. */
int demo_pollution_index(int smoke, int pop);

/* Orig Unit_UpdateVisibility @ 11c0:0395 seen-bit OR (+ City_DoRevolt @
 * 11b8:0f26 flips the same x*0x32+y+10 bytes) -> unit_reveal_seen_or.
 * 1:1: cur | (1 << civ) (complements unit_seen_bit / unit_seen_index;
 * minimap value via unit_reveal_minimap_value, far gate via
 * unit_sight_far_allowed). Returns the new seen value, -1 on civ OOR
 * 0..31 or cur outside 0..0xffffffff range guard (orig used byte game
 * values). No globals. */
int unit_reveal_seen_or(int cur, int civ);

/* --- Packet 2026-10-16 (game-only top-up): turn gates + combat odds +
 * revolt/diplo halves + popup moves/build tails + revolt auto-pick ---
 * Parents stay STUB/PARTIAL (Win16/GDI/dialog tails with TODO Orig in
 * game.c); predicates below are LIVE 1:1 on explicit params (no globals,
 * no Win16, no exit(), read-only, C17). Ref: unpacked/civ.exe read-only
 * (never edited, civ1win/ untouched). */

/* Orig Game_TurnLoopTick @ 1088:0000 year fixups -> game_turn_fix_year.
 * 1:1: year == 0 -> 1 (0-year doubling tail sets treasuries caller-side,
 * then year = 1); year == 0x15 (21) -> 0x14 (20). Takes the stepped year
 * (game_turn_year_step output) explicitly. No globals. */
int game_turn_fix_year(int year);

/* Orig same 0x32 scoring branch quiz sub-gate -> game_turn_quiz_due.
 * 1:1: ((opt_flags & 1) == 0) && year < 0 fires Quiz_RunQuizPair @
 * 1178:0000 (vs Map_RefreshAllViews @ 1110:03e9 when gated off). Caller
 * runs game_turn_should_score first (turn % 0x32 == 0 && player != 0xffff).
 * No globals. */
int game_turn_quiz_due(int opt_flags, int year);

/* Orig same 0x32 scoring branch save/refresh gate -> game_turn_save_due.
 * 1:1: (opt_flags & 2) != 0 && year != difficulty * -0x14 + 0x834 fires
 * Save_SaveGame @ 1198:00a3 + RefreshViews (vs Replay_AppendLog @ 11c0:3beb
 * 0xb only). opt_flags is g_nOptionFlags, difficulty g_nDifficulty.
 * No globals. */
int game_turn_save_due(int opt_flags, int year, int difficulty);

/* Orig Game_TurnLoopTick @ 1088:0000 retirement gate (1088:08e2 site) ->
 * game_turn_retirement_due. 1:1: difficulty * -0x14 + 0x820 == year fires
 * the retirement message tail (Util_StrCopy/Append @ 1000:36b2/363e +
 * Status_FormatMainRow @ 1138:0000; GDI/dialog tail stays Win16
 * caller-side). Integer math in orig order. No globals. */
int game_turn_retirement_due(int year, int difficulty);

/* Orig Game_TurnLoopTick @ 1088:0000 chronicle/end gate (1088:0994 site,
 * same 0x834 want as game_turn_save_due) -> game_turn_chronicle_due.
 * 1:1: difficulty * -0x14 + 0x834 == year fires the dynasty-ends tail
 * (Util_StrCopy/Append + Status_FormatMainRow @ 1138:0000 +
 * Sound_PlayId @ 1150:00c9 + Game_ProcessGameEnd @ 1088:0b6d HoF path;
 * tails stay Win16 caller-side). No globals. */
int game_turn_chronicle_due(int year, int difficulty);

/* Orig Combat_ResolveAttack @ 1050:0000 + Combat_CalcOdds @ 11c0:1bc3
 * win predicate -> combat_attacker_wins. 1:1: roll < odds (roll is the
 * Rng_Range @ 11c8:003d 0..99 draw via game_uprising_rng_scale, odds the
 * combat_calc_odds 0..100 value; odds 100 always wins, 0 never wins).
 * Complements unit_attack_resolve (odds + roll) and combat_roll_winner
 * (two-roll city-holder form). Returns 1 attacker wins / 0 defender wins.
 * No globals. */
int combat_attacker_wins(int roll, int odds);

/* Orig Diplomat_CityAction @ 11b8:0000 case 4 half gate ->
 * diplomat_revolt_half_due. 1:1: (city_flags & 1) != 0 halves the
 * diplomat_revolt_cost price (city flag 1420:5502&1 temple/courthouse-class
 * dampener, exact building TBD). city_flags is the 5502 word. No globals. */
int diplomat_revolt_half_due(unsigned city_flags);

/* Orig Diplomat_BribeUnit @ 11b8:07c2 half gate ->
 * diplomat_bribe_half_due. 1:1: type != 0 halves the diplomat_bribe_cost
 * price (type 0 capital-class pays full, exact class split TBD; unit_cost
 * is the 0x1a44 shield-cost entry). No globals. */
int diplomat_bribe_half_due(int type);

/* Orig Unit_ShowInfoPopup @ 11f0:468c moves line fraction gate ->
 * unit_popup_moves_needs_frac. 1:1: moves_raw % 3 != 0 draws the "..." @
 * 0x4813 + dec(rem) tail (moves @ 6300, full = moves/3 via
 * unit_popup_moves_full, frac via unit_popup_moves_frac). No globals. */
int unit_popup_moves_needs_frac(int moves_raw);

/* Orig same moves line build tail gate -> unit_popup_build_tail_due.
 * 1:1: prod_flag != 0 draws the "..." @ 0x4815 + dec(full+rate) + "..." @
 * 0x4817 tail (prod_flag is *(type*0x16+0x1a3e) != 0, rate = *(6301) *
 * 1420:1a3c[type*0xb]). No globals. */
int unit_popup_build_tail_due(int prod_flag);

/* Orig City_DoRevolt @ 11b8:0f26 auto-pick (param_2 == -1) ->
 * city_revolt_best_foreign. 1:1 argmax of city_revolt_score
 * ((v00de-v00e0)*0x20/dist) over n foreign cities (v00de/v00e0/dists
 * parallel read-only arrays, dists > 0 required like city_revolt_score).
 * Returns the winning index 0..n-1, -1 on NULL/n <= 0/none valid
 * (ties break to the lower index, like the orig strict-> scan). Seen-bit
 * flips + Unit_DoMove tail stay in city_do_revolt (STUB). No globals. */
int city_revolt_best_foreign(const int *v00de, const int *v00e0,
                             const int *dists, int n);

/* --- Pure game-math cores (2026-10-16) ---
 * OS-free 1:1 LIVE on explicit params (no globals, no Win16, no exit(),
 * C17; out params for the no-globals parallel rewrite; CIV_OK only on a
 * real effect per Fala H). Each composes already-LIVE helpers; device /
 * dialog / Gfx tails stay STUB in the parent with TODO Orig (see game.c).
 * Ref: unpacked/civ.exe read-only (civ1win/ untouched). */

/* Orig City_TileYield @ 1030:7c28 base+adjust compose ->
 * city_tile_yield_combine. 1:1: base = city_tile_base_yield(tile, irrigated,
 * kind) (1420:17d6 vs 0x1836 tables, stride 8, kind 0/1/2 food/shield/trade
 * at +0/+1/+2); then city_tile_yield_adjust(base, kind, occ_flags, pen0, pen4,
 * land_bonus, trade_bonus, gov_penalty, pollution_halve) (Map_GetOccupancyFlags
 * @ 1110:199b trims + owner(3)/Tech(3)/gov/pollution modifiers + clamp <1 -> 0).
 * Map bounds/improved/occupancy reads (Map_CheckTileBounds @ 1110:168f +
 * Map_CheckTileImproved @ 1110:1c72 + Map_GetOccupancyFlags @ 1110:199b + gov
 * state) stay caller-side; the map/device tail stays STUB in city_tile_yield.
 * Returns CIV_OK + *out (real effect), CIV_ERR_NOENT on NULL out,
 * CIV_ERR_RANGE when the base table reports OOR (tile/kind). No globals. */
civ_error_t city_tile_yield_combine(int tile, int irrigated, int kind,
                                    unsigned occ_flags, int pen0, int pen4,
                                    int land_bonus, int trade_bonus,
                                    int gov_penalty, int pollution_halve,
                                    int *out);

/* Orig City_ChooseProduction @ 1170:05ca scoring-pick core ->
 * city_choose_production_pick. 1:1 argmin over the caller-scored eligible list
 * via city_production_best_pick (strict-< minima, ties to the lower index, like
 * the local_c/local_28/local_26 loops over local_e8: positive = unit 0x00..0x1b,
 * negative = -building/-wonder). Callers score units via city_prod_unit_score
 * (supply/local_120 + continent/sea/defense/tech mults) vs buildings/wonders via
 * city_prod_building_score, gated by city_prod_unit_gate + tech_has_tech @
 * 1170:3181 + continent/sea gates; the human dialog (Dlg 0x1000 + SENDMESSAGE
 * list + City_PumpProductionDialog @ 1170:1f84, 0x66 accept / 0x65 cancel) stays
 * STUB in city_choose_production. Returns CIV_OK + *pick_out (real effect),
 * CIV_ERR_NOENT on NULL scores/pick_out, CIV_ERR_RANGE on n <= 0 or no pick.
 * No globals. */
civ_error_t city_choose_production_pick(const int *scores, int n,
                                        int *pick_out);

/* Orig Game_SpawnUprisings @ 1088:0be9 OS-free core ->
 * game_spawn_uprisings_core. 1:1 gate chain on explicit params (no globals):
 * game_uprising_due(difficulty, turn) head ((5-diff)*0x1e vs turn) +
 * game_uprising_wave_due(turn) every-8th wave ((turn+1)&7)==0 (DAT_12d8_2b74&7
 * conjunct stays caller-side) + game_uprising_tile_ok(tile_class, unit_at,
 * cont_val) (Map_GetTileClass @ 1110:16c0 == 10 ocean + Map_GetUnitAt @
 * 1110:18ce == -1 + Map_GetOceanOrContVal @ 1110:1dbd >= 0x10); on fire writes
 * *leader_out via game_uprising_leader(turn) (Util_Clamp(turn/0x96+1,1,3) <3 ->
 * 0x11 else 0x12 for Unit_SpawnUnit @ 11c0:12c8). Rng_Range @ 11c8:003d draws
 * (0x50/0x2c+3 tile loop via game_uprising_rng_scale) + Unit_SpawnUnit place +
 * Travellers/Barbarian/Native/Guerrilla message + Dialog_ShowMessage @ 1220:00d2
 * tails stay STUB in game_spawn_uprisings. Returns CIV_OK + leader (real
 * effect) when all gates fire, CIV_ERR_NOENT on NULL leader_out, else
 * CIV_ERR_UNSUPPORTED (gated off, no write). No globals. */
civ_error_t game_spawn_uprisings_core(int difficulty, int turn, int tile_class,
                                      int unit_at, int cont_val,
                                      int *leader_out);

/* Orig City_ProcessDisaster @ 1040:068e OS-free pick core -> city_disaster_pick.
 * 1:1 eligibility on explicit params (no globals): game_turn_disaster_due(turn)
 * (g_nTurn > 0x32, 2x per Game_TurnLoopTick @ 1088:0000 pass) +
 * city_disaster_city_ok(valid, pop, size) (1420:5502 != -1/0xff + 1420:3efe > 1
 * + 1420:5503 > 4; Rng_Range @ 11c8:003d 0x80 draws caller-side); on fire writes
 * *kind_out = roll (the caller Rng_Range(10) draw 0..9; building-flag checks on
 * the 0x1c mask @ 1420:54fc via city_disaster_has_building + messages @
 * 1420:07f0/0839 + viewer Pic_PlayShared @ 1030:0000 + dialog Dialog_ShowMessage
 * @ 1220:00d2 + Map_CenterOnTile @ 1110:03ae tails stay STUB in
 * city_process_disaster). Returns CIV_OK + kind (real effect) when eligible,
 * CIV_ERR_NOENT on NULL kind_out, CIV_ERR_RANGE on roll outside 0..9, else
 * CIV_ERR_UNSUPPORTED (gated off, no write). No globals. */
civ_error_t city_disaster_pick(int turn, int valid, int pop, int size, int roll,
                               int *kind_out);

/* Orig Trade_DeliverCaravanGoods @ 1038:0d09 treasury apply ->
 * trade_apply_bonus. 1:1 treasury += bonus (caller bonus via trade_calc_bonus:
 * (dist+10)*(sizes)/0x18, same-continent /2 via Map_GetContinentId @ 1110:1d99,
 * domestic /2, tech 0x31/0x26 each -= /3; route slots @ 1420:5513 via
 * trade_route_value). Route-table best-3 update + Res 0x8e/City_AppendName +
 * Dialog_ShowMessage @ 1220:00d2 + Game_UpdateStatusPanel @ 1088:13d5 +
 * Unit_FoundCityIfHome-consume @ 11c0:16a4 tails stay STUB in
 * trade_deliver_caravan_goods. Returns CIV_OK + *new_treasury_out (real effect),
 * CIV_ERR_NOENT on NULL, CIV_ERR_RANGE on treasury < 0 or bonus < 0 (orig used
 * non-negative game values). No globals. */
civ_error_t trade_apply_bonus(int treasury, int bonus, int *new_treasury_out);

/* Orig Diplomat_CityAction @ 11b8:0000 case 4 + Diplomat_BribeUnit @ 11b8:07c2
 * affordability gate -> diplomat_affords_cost. 1:1 treasury >= cost (cost via
 * diplomat_revolt_cost (((treas+1000)/(dist+3))*size, /2 on 1420:5502&1) or
 * diplomat_bribe_cost (((treas+0x2ee)/(dist+2))*shield-cost, /2 on type != 0);
 * dist via Diplo_NearestPalaceDist @ 1168:12c1). Treasury deduct + consume /
 * respawn + Map_CenterOnTile @ 1110:03ae + dialog tails stay STUB in
 * diplomat_city_action / diplomat_bribe_unit / city_do_revolt. Returns 1 afford,
 * 0 short, -1 on treasury < 0 or cost < 0. No globals. */
int diplomat_affords_cost(int treasury, int cost);

/* --- Batch 2026-10-17 (B+D): shared player 1030 + city/ship (P0) ---
 * Renamed this pass:
 *   Map_SetIrrigationFlag @ 1030:7f1e, City_CountSpecialistType @
 *   1030:7ffe, City_NormalizeLaborCounts @ 1030:802c,
 *   City_DrawLaborRowRaw @ 1030:817e, City_DrawLaborRowIcons @ 1030:830c,
 *   City_DrawDiscoveryIcon @ 1030:8529, Map_DrawMinimapDots @ 1030:8586,
 *   Map_PlotMinimapDot @ 1030:8847, Pic_ShowInfoMessage @ 1030:892a,
 *   Pic_DrawFramedPanel @ 1030:895c, City_ShowFindAndCenterDialog @
 *   1068:004c, Ship_UpdateConnectionBits @ 1070:1cfa. Fala H triage in
 *   game.c. */

/* Orig Map_SetIrrigationFlag @ 1030:7f1e (occ & 0x40 gate + SetBits +
 * Mirror + DAT_12d8_42f4++). LIVE pure occupancy-bit kernel on explicit
 * byte (hidden tile array -> explicit param, Orig kept). NULL -> NOENT. */
civ_error_t map_set_irrigation_flag(uint8_t *occ);

/* Orig City_CountSpecialistType @ 1030:7ffe (8x City_GetSpecialist @
 * 1030:7fc1 count). LIVE pure on explicit 8-entry specs (hidden city
 * table -> explicit param). NULL -> 0. */
int city_count_specialist_type(const int8_t specs[8], int type);

/* Orig City_NormalizeLaborCounts @ 1030:802c (labor split @ 1260:00de /
 * 00ae / 00e0 vs citySize @ 1420:5503 via Util_Clamp @ 11c8:0063).
 * LIVE pure arithmetic on explicit outs (hidden DGROUP -> params).
 * NULL any out -> NOENT. */
civ_error_t city_normalize_labor_counts(int size, int reserve, int *de_out,
                                        int *ae_out, int *eo_out);

/* Orig City_DrawLaborRowRaw @ 1030:817e (4 labor runs via raw masked blit
 * FUN_11a8_15ce + City_GetSpecialist). Effect is GDI blits to DC/port ->
 * SDL bridge TBD. STUB UNSUPPORTED (no guessed decode). */
civ_error_t city_draw_labor_row_raw(int city_idx, int x, int y, int max_w);

/* Orig City_DrawLaborRowIcons @ 1030:830c (same 4 runs via
 * Gfx_DrawTechIcon @ 11a8:0cb1; callers City_ShowInfoDialog @ 1030:8c02 +
 * Census @ 1018:1e9b + TopFive @ 1080:0c5a). STUB UNSUPPORTED. */
civ_error_t city_draw_labor_row_icons(int city_idx, int x, int y, int max_w);

/* Orig City_DrawDiscoveryIcon @ 1030:8529 (tech/wonder id -> icon table
 * 1420:98f4, skips 0x16-0x18, via Gfx_DrawTechIcon). STUB UNSUPPORTED. */
civ_error_t city_draw_discovery_icon(int id, int x, int y);

/* Orig Map_DrawMinimapDots @ 1030:8586 (framed panel via Pic_DrawFramed
 * Panel below + 80x50 visibility sweep + 0x80 units + 3 trade routes via
 * Map_PlotMinimapDot). Needs render-target + palette facade. STUB. */
civ_error_t map_draw_minimap_dots(void);

/* Orig Map_PlotMinimapDot @ 1030:8847 (Map_WrapX + SETRECT/OFFSETRECT +
 * Text_SetColor + FILLRECT 3x dot). Needs render-target facade (hidden
 * brush/DC -> explicit port/rect/pal param refactor allowed). STUB. */
civ_error_t map_plot_minimap_dot(int x, int y, int color);

/* Orig Pic_ShowInfoMessage @ 1030:892a (Util_StrCopy 12d8:0671->24fa +
 * Dialog_ShowMessage @ 1220:00d2 modal MenuZ). Needs dialog SDL facade.
 * STUB UNSUPPORTED. */
civ_error_t pic_show_info_message(void);

/* Orig Pic_DrawFramedPanel @ 1030:895c (SetActive + SETRECT + MakeBrush /
 * FILLRECT + raw fill + DELETEOBJECT + FRAMERECT x2 + INFLATERECT +
 * GETSTOCKOBJECT; 9 callers incl. Pic_PlayShared + city/info/tech).
 * Needs render-target + brush facade. STUB. */
civ_error_t pic_draw_framed_panel(int h, int w);

/* Orig City_ShowFindAndCenterDialog @ 1068:004c (128-city owner filter +
 * sort via FUN_1000_3396 + modal list 0x83 via FUN_1098_1909 + re-scan +
 * Map_CenterOnTile @ 1110:03ae). Needs arena + SDL modal + map facade.
 * STUB UNSUPPORTED. */
civ_error_t city_show_find_and_center_dialog(void);

/* Orig Ship_UpdateConnectionBits @ 1070:1cfa (4-neighbour N/E/S/W fix via
 * deltas 12c8:[0,1,0,-1]/12d0:[-1,0,1,0] on SEG 1290 tile byte). LIVE
 * pure on explicit tiles (hidden SEG base -> params, Orig kept). NULL
 * tiles -> NOENT guard via early return (orig faulted). */
void ship_update_connection_bits(uint8_t *tiles, int s1, int s2, int p1,
                                 int p2, int p3);

/* --- Batch 2026-10-17 fala 2 (F+G): 1030 ogon + 1050 + 1018 ---
 * Renamed this pass:
 *   Pic_DrawSunkenPanel @ 1030:8a60, Pic_DrawLabeledBar @ 1030:8c11,
 *   Tech_BuildDiscoveryList @ 1030:8d33, Tech_FindDiscoveryPair @
 *   1030:9061, City_BlitTileSlot @ 1030:90a8, Pic_PresentProductionView @
 *   1030:9203, Pic_BlitMaskedIcon @ 1030:925b, Pic_CreateInfoWindow @
 *   1030:944c, Pic_DestroyInfoWindow @ 1030:94ba, Pic_GetSharedTextBuf @
 *   1030:9588, Pic_NoopTail @ 1030:9598, Pic_ShowWindowRestored @
 *   1030:95a8, Pic_SetArrowEnable @ 1030:9915, Pic_DrawBevelEdge @
 *   1030:994b, Unit_UnlinkStack @ 1050:11ad, Combat_BlastReveal @
 *   1050:158a, Census_ShowForeignAffairs @ 1018:1221, Census_ShowCivDetail
 *   @ 1018:1739, Census_ShowCivDetailOverlay @ 1018:176c,
 *   Census_DrawCivDetail @ 1018:17db. Fala H triage in game.c. */

/* Orig Pic_DrawSunkenPanel @ 1030:8a60 (inset twin of Pic_DrawFramedPanel
 * @ 1030:895c: SetActive + SETRECT + MakeBrush + FILLRECT + raw fill +
 * FRAMERECT + INFLATERECT + GETSTOCKOBJECT frame; callers Pic_PlayShared
 * x2). Needs render-target + brush facade -> STUB UNSUPPORTED. */
civ_error_t pic_draw_sunken_panel(int x0, int y0, int x1, int y1);

/* Orig Pic_DrawLabeledBar @ 1030:8c11 (3-D bar via Gfx_FillBarRect @
 * 11e8:0504 + 4 edges via FUN_10d8_0000 + centered label via
 * Text_DrawCentered @ 10d8:0528; caller Census path FUN_1018_1221).
 * Needs bar/line/font facade -> STUB UNSUPPORTED. */
civ_error_t pic_draw_labeled_bar(int x0, int y0, int x1, int y1,
                                 const char *text, int color);

/* Orig Tech_BuildDiscoveryList @ 1030:8d33 (paged discover 0x15 + tech
 * 0x18 list fill via FramedPanel + SetIconScale + Res 0x83 + fit/draw +
 * DiscoveryIcon/TechIcon; callers Pic_PlayShared x2). Needs dialog font /
 * icon / panel facade -> STUB UNSUPPORTED. */
civ_error_t tech_build_discovery_list(int civ, int *top_io, int *count_io);

/* Orig Tech_FindDiscoveryPair @ 1030:9061 (21-entry DAT_1258_0000 /
 * DAT_1260_0000 scan for (p1,p2), -1 when absent; caller Pic_PlayShared).
 * LIVE pure on explicit tables (hidden SEG -> params). */
int tech_find_discovery_pair(const int16_t a[21], const int16_t b[21],
                             int p1, int p2);

/* Orig City_BlitTileSlot @ 1030:90a8 (worked-tile mini-view: 0x18 box +
 * ScrollPortCopy @ 10d0:03fa + Refresh @ 1110:0448 / Reveal @ 1110:1461 +
 * SetColor/FILLRECT fallback; callers City_RenderWorkedTile @ 1030:79ce +
 * Pic_PlayShared x3). Needs port/DC/scale facade -> STUB. */
civ_error_t city_blit_tile_slot(int tx, int ty, int mode);

/* Orig Pic_PresentProductionView @ 1030:9203 (SETRECT 0x140x0x200 +
 * PresentRect @ 10d0:0854 + Pic_FinishPaint @ 1030:94d2; caller
 * City_ChooseProduction @ 1170:05ca). Needs present facade -> STUB. */
civ_error_t pic_present_production_view(void);

/* Orig Pic_BlitMaskedIcon @ 1030:925b (PIC-path color masked blit triptych
 * 0x46/0x66 + 0xc6/0x88 + 0x46/0x66 via BuildIconMask @ 11a8:0d6e mask
 * 0xf5; caller Pic_PlayShared). Needs DC/bitmap/mask/BITBLT facade ->
 * STUB UNSUPPORTED (no guessed RLE). */
civ_error_t pic_blit_masked_icon(void);

/* Orig Pic_CreateInfoWindow @ 1030:944c (CREATEWINDOW style 0x5000000b
 * id 0x6dd w/h from rect; callers Pic_PlayShared x3). Win16 window, no
 * SDL twin in-func -> STUB UNSUPPORTED (call shape kept as win16.*). */
civ_error_t pic_create_info_window(int *hwnd_out);

/* Orig Pic_DestroyInfoWindow @ 1030:94ba (DESTROYWINDOW; callers
 * Pic_PlayShared x3 + Ship_ShowView @ 1070:0327). Real Win16 effect ->
 * STUB UNSUPPORTED (never WONTFIX, no fake-OK). */
civ_error_t pic_destroy_info_window(void);

/* Orig Pic_GetSharedTextBuf @ 1030:9588 (16 B, no callers/callees; twin
 * of Pic_NoopTail @ 1030:9598; decompiler string return is a stale-DS
 * artifact per disasm). Provably empty -> WONTFIX-Win16 NOOP CIV_OK. */
civ_error_t pic_get_shared_text_buf(void);

/* Orig Pic_NoopTail @ 1030:9598 (same 16 B empty shape as 9588).
 * WONTFIX-Win16 NOOP CIV_OK. */
civ_error_t pic_noop_tail(void);

/* Orig Pic_ShowWindowRestored @ 1030:95a8 (SHOWWINDOW @ 1608:00d0 with 9
 * == SW_RESTORE shape; orphan helper). USER window effect, no SDL facade
 * in-func -> STUB UNSUPPORTED. */
civ_error_t pic_show_window_restored(int hwnd);

/* Orig Pic_SetArrowEnable @ 1030:9915 (mode 0x97 -> DAT_1260_0030 = val;
 * mode 0x98 -> DAT_1260_002e = val; callers Pic_PlayShared x4; feeds the
 * pic_arrow_is_greyed gate). LIVE 1:1 on explicit outs (hidden ES:0x1260
 * -> params). */
int pic_arrow_enable_slot(int mode);
civ_error_t pic_set_arrow_enable(int mode, int val, int *f002e, int *f0030);

/* Orig Pic_DrawBevelEdge @ 1030:994b (GDI bevel: CREATEPEN white
 * 0xffffff / grey 0x808080 + SELECTOBJECT + MOVETO/LINETO + DELETEOBJECT;
 * callers DrawFrameArrow @ 1030:96ad x2 + DrawFrameCaption @ 1030:982f
 * x2). Pen/DC bridge TBD -> STUB; geometry LIVE below. */
civ_error_t pic_draw_bevel_edge(int y0, int x0, int y1, int x1, int style);
int pic_bevel_inner_h(int y1, int y0);
int pic_bevel_is_raised(int style);

/* Orig Unit_UnlinkStack @ 1050:11ad (singly-linked 0x6306 next-chain
 * detach, 0xff none, 0x20-step cap; stride civ*0x600+idx*0xc; sole caller
 * FUN_1110_17bc tile-clear gate; sibling of Unit_LinkStack @ 1050:1105).
 * LIVE 1:1 on explicit next[] (hidden segment -> param). */
int unit_unlink_stack(uint8_t *next, int n, int start, int victim);

/* Orig Combat_BlastReveal @ 1050:158a (9-tile burst: WrapX + occupancy&1
 * gate; land Rng_Range(3)!=0 && TileClass!=10 -> SetIrrigationFlag; city
 * pop -= pop/2; always RevealCenter; port select first; callers
 * Combat_ScoreAdjacent @ 1050:13af tail + Pic_PlayShared @ 1030:74d5).
 * Port/reveal bridge TBD -> STUB; kernels LIVE below. */
civ_error_t combat_blast_reveal(int x, int y);
int combat_blast_count(void);
int combat_should_irrigate(unsigned rng3, int tile_class);
int combat_halve_pop(int pop);

/* Orig Census_ShowForeignAffairs @ 1018:1221 (foreign roster: civs 1..7,
 * alive mask, embassy 0x40 gate, 0x80-city capital scan, treaty bits,
 * 0x119 buttons, drill to 176c, keys 1..7, click (y-0x1c)/0x18; caller
 * Tech_GrantTech @ 1170:2aaa). Dialog/GDI/modal bridge TBD -> STUB;
 * index kernels LIVE below. */
civ_error_t census_show_foreign_affairs(void);
int census_foreign_is_known(int player, int civ, int peace_flags);
int census_foreign_key_to_idx(int key);
int census_foreign_y_to_idx(int y);

/* Orig Census_ShowCivDetail @ 1018:1739 (Res 0x8a/2 + Open + 17db body +
 * Close; callers Diplo_PromptContact @ 1160:20c0 + Diplomat_CityAction @
 * 11b8:0000). Dialog bridge TBD -> STUB UNSUPPORTED. */
civ_error_t census_show_civ_detail(int civ);

/* Orig Census_ShowCivDetailOverlay @ 1018:176c (like 1739 but preserves
 * DAT_1420_5156 + 62 B screen stash via Util_MemCopyFar @ 1000:137f;
 * sole caller 1221 drill). Mem part facade-ready (util_mem_copy_far
 * LIVE), dialog tails TBD -> STUB UNSUPPORTED. */
civ_error_t census_show_civ_detail_overlay(int civ);

/* Orig Census_DrawCivDetail @ 1018:17db (single-civ intel body: gov /
 * wonders / capital / treasury / relations / techs 0x47..0 y+=8 stop at
 * 0xbf; callers 1739 + 176c). Render bridge TBD -> STUB; scan kernels
 * LIVE below. */
civ_error_t census_draw_civ_detail(int civ);
int census_detail_find_capital(const int *owners, const int *valids, int n,
                               int civ);
int census_detail_tech_y(int row);

/* --- Batch 2026-10-17 fala 3 (K): ship place + launch poll ---
 * Renamed this pass:
 *   Ship_ValidatePlacement @ 1070:1dfb, Ship_CanPlaceAt @ 1070:1e77,
 *   Ship_PollLaunchConfirm @ 1010:00d6. */

/* Orig Ship_CanPlaceAt @ 1070:1e77 (cell conn + 3 neighbour dim gates
 * < 0x11; sole caller ValidatePlacement; distinct from IsPartConnected
 * @ 1070:11f2). LIVE pure kernel on pre-resolved small-flags (hidden
 * 1290 grid + 1420:12b0/12b2 tables -> params; table images stay
 * caller-side with TODO). conn/flags: 1 = placeable-side. */
int ship_can_place_at(int conn_is_free, int na_small, int nb_small,
                      int nc_small);

/* Orig Ship_ValidatePlacement @ 1070:1dfb (part w/h /0x10 cell loop over
 * CanPlaceAt + 0x0E/0x0B bounds; orphan; same class as
 * Ship_UpdateConnectionBits @ 1070:1cfa LIVE). Bounds half LIVE pure on
 * explicit cell counts (hidden 12b0/12b2 dims -> params); per-cell grid
 * loop stays caller-side TODO. 1 fits, 0 not. */
int ship_validate_placement(int w_cells, int h_cells, int x0, int y0);

/* Orig Ship_PollLaunchConfirm @ 1010:00d6 (tick % 0x1E gate + PEEKMESSAGE
 * pump via FUN_11e0_095f + Cursor_Set 0x84 + DAT_13E0_001E = 1 + string
 * 1420:13D2+0x0E; orphan; prototype/return conflated in decompiler).
 * Pump + cursor + semantics unconfirmed -> STUB UNSUPPORTED (*out
 * untouched). */
civ_error_t ship_poll_launch_confirm(long tick, const char **out);

#endif /* CIVLIN_GAME_H */
