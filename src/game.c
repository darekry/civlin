/* Game state. Turn logic mirrors Game_TurnLoopTick @ 1088:0000.
 * U1 boot loop + tick: game_tick feeds the while(!g_nQuitFlag @ 12d8:24ee)
 * loop in WinMain @ 1008:0304 (direct turn calls, not a timer callback;
 * TIMERPROC @ 1008:0d68 only ticks the 24f0 countdown, see boot.h
 * chain-21); SDL pump via Sys_PumpMessages @ 1108:0000 + Sys_Yield @
 * 1098:0157 (see sys.h). */
#include "game.h"

#include "map.h"
#include "sys.h"

civ_error_t game_init(civ_game_t *g)
{
    if (!g)
        return CIV_ERR_NOENT;
    g->initialized = 1;
    g->quit_flag = 0;
    g->turn = 0;
    g->difficulty = 0;
    g->player_civ = 0;
    g->alive_mask = 0;
    g->year = 0;
    g->menu_choice = -1;
    g->civ_count = 0;
    /* OS-free 1:1 live (caller-owned image of g_nQuitFlag @ 12d8:24ee,
     * g_nTurn @ 12d8:42fe, g_nDifficulty @ 12d8:42f6, g_nPlayerCiv @
     * 12d8:4300, g_nCivAliveMask @ 12d8:42f8, g_nYear @ 12d8:42fa,
     * g_nMenuChoice @ 12d8:44e2, g_nCivCount @ 12d8:3b9a).
     * TODO: DGROUP 1420 full layout mapping (unit/city records) lands with
     * game_new/worldgen. */
    return CIV_OK;
}

civ_error_t game_new(civ_game_t *g)
{
    if (!g)
        return CIV_ERR_NOENT;
    /* TODO: Orig NewGame_Wizard @ 10e8:0000 (newgame_wizard_run, see
     * newgame.h) -> Map_GenerateRandomOrLoad @ 1028:0000 (see worldgen.h)
     * -> start-of-game field fill (g_nDifficulty/CivCount/AliveMask/
     * PlayerCiv/Year). Win16 dialog/map tails TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t game_tick(civ_game_t *g)
{
    if (!g)
        return CIV_ERR_NOENT;
    /* Orig WinMain @ 1008:0304 loop body: while(!g_nQuitFlag @ 12d8:24ee)
     * Game_TurnLoopTick @ 1088:0000 (direct calls, NOT a timer callback:
     * TIMERPROC @ 1008:0d68 only ticks the 24f0 countdown, see boot.h
     * chain-21; the 200 ms map timer is a NULL-proc WM_TIMER, see map.h
     * map_timer_interval_ms). Tick -> game_turn_tick @ 1088:0000 linkage
     * below: per-frame SDL pump is live, turn pass stays stubbed ->
     * overall CIV_ERR_UNSUPPORTED. No globals, no exit(), read-only
     * (pump only). */
    if (g->quit_flag)
        return CIV_OK;
    /* Live pump: Sys_PumpMessages @ 1108:0000 (SDL_PumpEvents, 1 = message,
     * 0 = empty) + Sys_Yield @ 1098:0157 (Anim pump @ 10b0:0000). */
    (void)sys_pump_messages();
    sys_yield();
    /* TODO: WinMain @ 1008:0304 loop tail -> Game_TurnLoopTick @ 1088:0000
     * (game_turn_tick below; Civ_ProcessUnitMoves @ 11f0:0000, year steps,
     * scoring every 0x32 via game_calc_progress_threshold, HoF endings).
     * Exercised for linkage; result ignored until the per-civ loop lands. */
    (void)game_turn_tick(g);
    return CIV_ERR_UNSUPPORTED;
}

int game_turn_year_step(int year, int opt_flags)
{
    /* Orig Game_TurnLoopTick @ 1088:0000 year-step tail (g_nYear @ 12d8:42fa,
     * gate DAT_12d0_00b6 & 0xfe). OS-free 1:1, no globals. */
    if ((opt_flags & 0xfe) != 0)
        return year;
    if (year < 1000)
        return year + 0x14;
    if (year < 0x5dc)
        return year + 10;
    if (year < 0x6d6)
        return year + 5;
    if (year < 0x73a)
        return year + 2;
    return year + 1;
}

int game_turn_should_score(int turn, int player_civ)
{
    /* Orig same 0x32 branch (g_nTurn % 0x32 == 0, g_nPlayerCiv != 0xffff). */
    if (player_civ == 0xffff)
        return 0;
    return (turn % 0x32 == 0) ? 1 : 0;
}

int game_uprising_due(int difficulty, int turn)
{
    /* Orig Game_SpawnUprisings @ 1088:0be9 head: iVar8 = (5-diff)*0x1e. */
    int limit = (5 - difficulty) * 0x1e;

    if (limit - turn == 0)
        return 1;
    return (limit < turn) ? 1 : 0;
}

civ_error_t game_turn_tick(civ_game_t *g)
{
    if (!g)
        return CIV_ERR_NOENT;
    /* P3 skeleton of Game_TurnLoopTick @ 1088:0000 (sole caller WinMain @
     * 1008:0000; callees: Diplo_ProcessTurn @ 1168:0000, FUN_1078_0000/448f,
     * FUN_1158_0000, FUN_1170_0000, Civ_ProcessUnitMoves @ 11f0:0000 (=1 pump
     * -> early return), Sys_CheckRuntimeMemory @ 11e8:0075 per-civ, then
     * Game_SpawnUprisings @ 1088:0be9, Game_DoPollutionAndWonderRace @
     * 1088:1e32, 2x City_ProcessDisaster @ 1040:068e (turn > 0x32),
     * Game_UpdatePowerRankings @ 1088:108a, treasury<100 memory-warning branch
     * (Dialog_ShowMessage @ 1220:00d2 via FUN_11d0_01f7), turn++/year-step,
     * 0-year doubling + 0x15->0x14 fixups, 0x32 scoring (Quiz @ 1178:0000 +
     * Map_RefreshAllViews @ 1110:03e9, Save @ 1198:00a3 + RefreshViews @
     * 1220:0eb8, Replay_AppendLog @ 11c0:3beb 0xb, threshold @ 1088:1d99),
     * endings via Game_ProcessGameEnd @ 1088:0b6d (HoF path, quit_flag).
     * Live here on caller-owned state: turn++ + year-step + score/uprising
     * predicates (helpers above; threshold/power/pollution/uprising-scale
     * already LIVE). Per-civ/diplo/map/dialog/quiz/save/HoF tails need
     * DGROUP + Win16 -> CIV_ERR_UNSUPPORTED.
     * TODO: Orig per-civ loop + disaster draws (Rng_Range @ 11c8:003d 0x80) +
     *  Win16 message/view tails. */
    g->turn += 1;
    g->year = game_turn_year_step(g->year, 0);
    if (g->year == 0)
        g->year = 1;
    if (g->year == 0x15)
        g->year = 0x14;
    /* Exercise linkage for the frontier (results ignored until the per-civ
     * loop lands); keeps nm -g symbols wired without globals. */
    (void)game_turn_should_score(g->turn, g->player_civ);
    (void)game_uprising_due(g->difficulty, g->turn);
    (void)game_spawn_uprisings();
    (void)game_do_pollution_and_wonder_race();
    (void)game_update_power_rankings(NULL, 0, NULL, NULL);
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t game_process_game_end(int hof_slot)
{
    (void)hof_slot;
    /* TODO: Orig FUN_1088_0b6d @ 1088:0b6d (unless quit==2 run 1188:07ad +
     * 1180:0000 teardown, score via 1040:1137, then HoF_VictoryCeremony /
     * HoF_Load / HoF_TryInsert / HoF_Save; see game.h). Win16 dialogs +
     * ceremony frames + famehall.civ I/O (halloffame.h, parallel-owned). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t game_spawn_uprisings(void)
{
    /* TODO: Orig FUN_1088_0be9 @ 1088:0be9. OS-free head (Rng_Range @
     * 11c8:003d draws, see game_uprising_rng_scale) + tile gates 1110:16c0
     * /18ce/1dbd + Unit_RevealArea @ 11c0:12c8 are documented in game.h;
     * Travellers/Barbarian/Native/Guerrilla message + dialog tail needs
     * Win16 dialogs + map buffer writes (map.h, parallel-owned). */
    return CIV_ERR_UNSUPPORTED;
}

unsigned game_uprising_rng_scale(unsigned rand15, int n)
{
    /* Rng_Range @ 11c8:003d scaling used by Game_SpawnUprisings @
     * 1088:0be9 head: (rand15 * n) >> 15. n <= 0 guard is a Linux-side
     * addition (orig callers pass n > 0); rand15 is the caller's
     * rng_next_raw() draw masked to 0x7FFF. */
    if (n <= 0)
        return 0;
    return (rand15 * (unsigned)n) >> 15;
}

civ_error_t city_process_disaster(int city_idx)
{
    (void)city_idx;
    /* TODO: Orig FUN_1040_068e @ 1040:068e -> City_ProcessDisaster (see
     * game.h). Random disaster switch Rng_Range(10) on the 0x1c city
     * records @ 1420:5500/54fc + messages 1420:07f0/0839 + viewer
     * 1030:0000 + dialog 1220:00d2 + Map_CenterOnTile @ 1110:03ae.
     * Needs city-record decode + Win16 dialog/message pump. */
    return CIV_ERR_UNSUPPORTED;
}

int game_calc_power_score(int cities, int territory, int treasury, int tech)
{
    /* Game_UpdatePowerRankings @ 1088:108a kernel: cities*8 + territory +
     * treasury/0x20 + tech. */
    return cities * 8 + territory + treasury / 0x20 + tech;
}

civ_error_t game_update_power_rankings(const civ_power_state_t *states,
                                       int n_civs, int *scores_out,
                                       int *rank_out)
{
    /* Orig FUN_1088_108a @ 1088:108a (scores + rank table 0x3e84+0x1a;
     * 12a8:000c rebellion flag is Win16/game-state owned, not done here).
     * Decompiler shape: linear score pass, then nested-loop ranking. */
    int i;
    int j;

    if (!states || !scores_out || !rank_out)
        return CIV_ERR_NOENT;
    if (n_civs <= 0)
        return CIV_ERR_RANGE;
    for (i = 0; i < n_civs; i++)
        scores_out[i] = game_calc_power_score(states[i].cities,
                                              states[i].territory,
                                              states[i].treasury,
                                              states[i].tech);
    for (i = 0; i < n_civs; i++) {
        int rank = 0;

        for (j = 0; j < n_civs; j++) {
            if (scores_out[j] > scores_out[i] ||
                (scores_out[j] == scores_out[i] && j < i))
                rank++;
        }
        rank_out[i] = rank;
    }
    return CIV_OK;
}

civ_error_t game_update_status_panel(void)
{
    /* TODO: Game_UpdateStatusPanel @ 1088:13d5 (GfxPort blits, 1218:0956 /
     * 0a06 frames, 11f8:0278 text, 1088:1a82/1dd0 rows, TEXTOUT x3,
     * population milestone popup via 1220:00d2). */
    return CIV_ERR_UNSUPPORTED;
}

unsigned game_accum_pollution(unsigned counter, unsigned produced)
{
    /* Game_DoPollutionAndWonderRace @ 1088:1e32 head (12d8:42f0 word add).
     * Orig wraps at 16 bits; unsigned addition preserves the wrap. */
    return counter + produced;
}

civ_error_t game_do_pollution_and_wonder_race(void)
{
    /* TODO: Orig FUN_1088_1e32 @ 1088:1e32. OS-free head (12d8:42f0 via
     * game_accum_pollution + Map_SpreadPollution @ 1028:1afd, see
     * worldgen.h parallel-owned) + wonder pick via Game_PickWonderForAI @
     * 1170:3181 (game_pick_wonder_for_ai below, implemented) are in game.h;
     * "... builds ..." message + shield deduction + dialog tail needs Win16
     * UI + city state. */
    return CIV_ERR_UNSUPPORTED;
}

int game_pick_wonder_for_ai(const int *tech_owned, int n_tech,
                            const int *wonder_prereq,
                            const int *wonder_done, int n_wonders,
                            unsigned roll)
{
    /* Orig FUN_1170_3181 @ 1170:3181, callee of Game_DoPollutionAndWonderRace
     * @ 1088:1e32. Decompiler shape: linear tech-table scan for eligible
     * wonders, then indexed pick among them. Two passes avoid any heap. */
    int eligible = 0;
    int want;
    int i;
    int seen;

    if (!tech_owned || !wonder_prereq || !wonder_done)
        return -1;
    if (n_tech <= 0 || n_wonders <= 0)
        return -1;
    for (i = 0; i < n_wonders; i++) {
        int req = wonder_prereq[i];

        if (wonder_done[i])
            continue;
        if (req < 0)
            eligible++;
        else if (req < n_tech && tech_owned[req] != 0)
            eligible++;
    }
    if (eligible == 0)
        return -1;
    want = (int)(roll % (unsigned)eligible);
    seen = 0;
    for (i = 0; i < n_wonders; i++) {
        int req = wonder_prereq[i];
        int ok;

        if (wonder_done[i])
            continue;
        if (req < 0)
            ok = 1;
        else if (req < n_tech && tech_owned[req] != 0)
            ok = 1;
        else
            ok = 0;
        if (!ok)
            continue;
        if (seen == want)
            return i;
        seen++;
    }
    return -1;
}

int civ_process_unit_moves(int civ)
{
    (void)civ;
    /* TODO: Civ_ProcessUnitMoves @ 11f0:0000 (0x80 units at 1420:62fc;
     * 1 = pump UI and resume later, 0 = civ done). Full dispatch: per-unit
     * visibility 11c0:0395, Map_CenterOnTile, order via 1078:1480 /
     * 1128:0000 jump tables, city prod 11c0:16a4, Unit_ShowInfoPopup,
     * 11c0:0ec2 wait loop, 11f0:44d4/4ea0 center/drain (see game.h). */
    return 0;
}

civ_error_t status_draw_tech_icons(int civ, int progress, int x, int y)
{
    (void)civ;
    (void)progress;
    (void)x;
    (void)y;
    /* TODO: FUN_1088_1a82 @ 1088:1a82 (rule + 9 icons via 11a8:0cb1;
     * player copies live rows, AI fakes via LCG; see game.h). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t game_append_year_text(int year, char *out, size_t cap)
{
    /* Orig FUN_1088_1dd0 @ 1088:1dd0 (game year <-> text; the true chain,
     * not the 1000:386e Date_Convert / 1000:3ba0 Float_ParseFormat /
     * 1000:3d90 Date_CheckValid C-runtime date chain via Sys_GetDate @
     * 1000:18d4): abs(year) via Util_IntToDec @ 1000:2928 (...,10, sign
     * only for base 10) + " BC" (year < 0, 1420:173c) / " AD" (else,
     * 1420:1740) appended to the line buffer (orig 12d8:24fa). OS-free
     * 1:1 on explicit out/cap with the orig leading space before the era
     * (year_suffix below returns the spaceless literal). NOENT on NULL
     * out, RANGE when cap == 0 or the text does not fit. No globals. */
    long abs_year = year < 0 ? -(long)year : (long)year;
    const char *era = year < 0 ? " BC" : " AD";
    char num[12];
    int nlen = 0;
    size_t need = 0;

    if (!out)
        return CIV_ERR_NOENT;
    if (cap == 0)
        return CIV_ERR_RANGE;
    if (abs_year == 0) {
        num[0] = '0';
        nlen = 1;
    } else {
        char rev[12];
        int rn = 0;

        while (abs_year > 0) {
            rev[rn++] = (char)('0' + abs_year % 10);
            abs_year /= 10;
        }
        nlen = rn;
        for (int k = 0; k < rn; k++)
            num[k] = rev[rn - 1 - k];
    }
    need = (size_t)nlen + 3u + 1u; /* digits + " BC"/" AD" + NUL */
    if (need > cap)
        return CIV_ERR_RANGE;
    for (int k = 0; k < nlen; k++)
        out[k] = num[k];
    out[nlen] = era[0];
    out[nlen + 1] = era[1];
    out[nlen + 2] = era[2];
    out[nlen + 3] = '\0';
    return CIV_OK;
}

const char *year_suffix(int year)
{
    /* AG2: 1420:173c "BC" when year<0 / 1420:1740 "AD". OS-free 1:1;
     * complements game_append_year_text @ 1088:1dd0 (abs via Util_IntToDec
     * @ 1000:2928 + this suffix onto 12d8:24fa). Read-only literal. */
    return year < 0 ? "BC" : "AD";
}

civ_error_t gfx_draw_tech_icon(int index, int x, int y, int palette)
{
    (void)index;
    (void)x;
    (void)y;
    (void)palette;
    /* TODO: Orig FUN_11a8_0cb1 @ 11a8:0cb1 (9-icon GDI blitter: index
     * 0..8, x/y origin, palette select; BITBLT the icon cell, then the
     * status_draw_tech_icons caller advances to the next slot). Needs
     * the icon-cell source rects + SDL texture bridge. */
    return CIV_ERR_UNSUPPORTED;
}

int status_font_height(void)
{
    /* TODO: FUN_1098_0dda @ 1098:0dda (GETTEXTMETRICS tmHeight). */
    return -1;
}

int unit_choose_order(int civ, int unit_idx)
{
    (void)civ;
    (void)unit_idx;
    /* TODO: FUN_1078_1480 @ 1078:1480 (AI order brain; order codes in
     * game.h). Largest unit function, 0x300e bytes. */
    return 0x20;
}

int unit_step_toward_goal(int civ, int unit_idx)
{
    (void)civ;
    (void)unit_idx;
    /* TODO: FUN_1128_0000 @ 1128:0000 (dir 1..8 / 0 / 0xffff; 10de/081c
     * pathfind + 8-neighbour cost model; see game.h). */
    return 0xffff;
}

civ_error_t unit_update_visibility(int x, int y, int civ, int unit_idx,
                                   int redraw)
{
    (void)x;
    (void)y;
    (void)civ;
    (void)unit_idx;
    (void)redraw;
    /* TODO: FUN_11c0_0395 @ 11c0:0395 (seen-bits, city spot, diplomacy,
     * minimap x+0xf0; see game.h). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t unit_wait_for_orders(int civ, int unit_idx)
{
    (void)civ;
    (void)unit_idx;
    /* TODO: FUN_11c0_0ec2 @ 11c0:0ec2 (blink-and-pump modal; see game.h). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t unit_maybe_center_view(int civ, int unit_idx)
{
    (void)civ;
    (void)unit_idx;
    /* TODO: FUN_11f0_44d4 @ 11f0:44d4 (1110:1461 when player/seen). */
    return CIV_ERR_UNSUPPORTED;
}

int unit_ai_player_turn(int civ, int unit_idx)
{
    (void)civ;
    (void)unit_idx;
    /* TODO: Orig FUN_1078_4f0f @ 1078:4f0f (civ==0 delegate from
     * unit_choose_order @ 1078:1480; drives unit_step_toward_goal @
     * 1128:0000 at 1078:55d3, then maps the step/dir state to an order
     * code from the game.h dispatch list). Game-logic stub: idle. */
    return 0x20;
}

civ_error_t unit_reveal_area(int x, int y, int civ)
{
    (void)x;
    (void)y;
    (void)civ;
    /* TODO: Orig FUN_11c0_12c8 @ 11c0:12c8 (area reveal; fans out to
     * unit_update_visibility @ 11c0:0395 per tile/unit, then posts
     * diplomacy/minimap updates from the 0395 tail). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t sys_drain_input(void)
{
    /* TODO: Orig Sys_DrainInput @ 11f0:4ea0 (P3 drain format, 22 callers incl.
     * Civ_ProcessUnitMoves @ 11f0:0000, Sys_PumpMessages @ 1108:0000,
     * Sys_WaitForInput @ 1038:0c1d): DAT_1420_1d44/1d46/1d48/1d4e/1d4c/1d50
     * = 0 (6 slow, see sys_drain_state_words) + PEEKMESSAGE(...,0x108,0x100)
     * drain + PEEKMESSAGE(...,0x209,0x200) drain + DAT_12b0_004c = 0 (cap 10,
     * see sys_drain_queue_cap/reset; emptied by Main_DrainMessages @
     * 11e8:02fb). Win16 PEEKMESSAGE queues have no SDL equivalent here
     * (SDL pump lives in sys_pump_messages @ 1108:0000, LIVE) -> stub. */
    return CIV_ERR_UNSUPPORTED;
}

unsigned game_calc_progress_threshold(int n, int difficulty)
{
    /* Game_CalcProgressThreshold @ 1088:1d99: sum_{i=0..n} i*(7-diff),
     * halved, plus 1. */
    unsigned acc = 0;
    int i;

    for (i = 0; i <= n; i++)
        acc += (unsigned)i * (unsigned)(7 - difficulty);
    return acc / 2u + 1u;
}

civ_error_t unit_show_info_popup(int civ, int unit_idx)
{
    (void)civ;
    (void)unit_idx;
    /* TODO: Orig FUN_11f0_468c @ 11f0:468c (Unit_ShowInfoPopup: turn==0 &&
     * difficulty==0 popup; font via status_font_height @ 1098:0dda at
     * 11f0:4754, modal via Dialog_MenuZ_Modal @ 1220:0156 family; needs
     * SDL dialog + text bridge). */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (15): Unit_ShowInfoPopup line predicates (goal 3).
 * See game.h layout block. OS-free 1:1, no Win16, no globals. */

int unit_popup_is_end_of_turn(int unit_idx)
{
    /* Orig Unit_ShowInfoPopup @ 11f0:468c branch: param_2 == 0x80 takes
     * the 3-line end-of-turn card ("End of Turn"/"Press Return"/
     * "to continue"), else the normal unit card. */
    return (unit_idx == 0x80) ? 1 : 0;
}

int unit_popup_has_veteran(int flags)
{
    /* Orig same: (*(62fc) & 0x20) != 0 draws "Veteran" @ 1420:4803. */
    return ((flags & 0x20) != 0) ? 1 : 0;
}

int unit_popup_moves_full(int moves_raw)
{
    /* Orig same: Util_Clamp(*(6300)/3,0,99) full moves before the
     * fraction tail (Util_Clamp @ 11c8:0063; clamp inline here, no link). */
    int full = moves_raw / 3;

    if (full < 0)
        full = 0;
    if (full > 99)
        full = 99;
    return full;
}

int unit_popup_moves_frac(int moves_raw)
{
    /* Orig same: *(6300) % 3 fraction (drawn with 0x4813 tail when != 0;
     * x86 IDIV remainder sign follows the dividend, same as C %). */
    return moves_raw % 3;
}

int unit_popup_badge_needs_wrap(int x, int panel_right)
{
    /* Orig same badge-stack tail: if (local_1a - 0x24 <= local_6):
     * wrap to (local_1e+2, y+0x20). local_1a = panel right, local_6 = x. */
    return ((panel_right - 0x24 <= x) ? 1 : 0);
}

civ_error_t combat_resolve_attack(int atk_strength, int def_strength,
                                  int terrain_bonus, int vet_bonus,
                                  int *odds_out)
{
    if (!odds_out)
        return CIV_ERR_NOENT;
    (void)atk_strength;
    (void)def_strength;
    (void)terrain_bonus;
    (void)vet_bonus;
    /* TODO: Orig Combat_ResolveAttack @ 1050:0000 full flow (sole caller
     * Unit_ChooseOrder @ 1078:1480): effective kernels (combat_effective_
     * attack/defense LIVE) + difficulty (combat_difficulty_scale LIVE) +
     * preview (combat_preview_score LIVE, param_5 == 0) else rolls
     * (combat_roll_winner LIVE via rng_range @ 11c8:003d + city-holder
     * reroll) + animation/sound/Gfx tails (Unit_EnsureOnScreen @ 11c0:231d
     * + Map_RefreshTileView @ 1110:0448 + Sound_PlayId @ 1150:00c9 +
     * Gfx_DrawTechIcon @ 11a8:0cb1 + GfxPort @ 10d0:0854/0956) + diplo tails
     * (Combat_CheckPeaceBlock @ 1050:12be + Diplo_SetWar @ 1168:0e8e) +
     * outcomes (diplomat 0x19 -> Combat_ScoreAdjacent @ 1050:13af;
     * defender wins -> 396a++ + Unit_FoundCityIfHome @ 11c0:16a4 +
     * combat_should_promote LIVE; attacker wins -> pillage/capture/city
     * tails + 396a += CalcOdds + IterateStack + messages). Odds scale via
     * combat_calc_odds @ 11c0:1bc3 (LIVE v1). Needs game state + Win16 UI. */
    return CIV_ERR_UNSUPPORTED;
}

int combat_calc_odds(int atk_eff, int def_eff)
{
    /* Orig FUN_11c0_1bc3 @ 11c0:1bc3 -> Combat_CalcOdds. OS-free 1:1 v1:
     * percent odds from effective strengths; veteran/terrain pre-scale
     * + rounding TBD vs decompiler. */
    if (atk_eff < 0 || def_eff < 0)
        return -1;
    if (atk_eff == 0 && def_eff == 0)
        return -1;
    return atk_eff * 100 / (atk_eff + def_eff);
}

civ_error_t unit_found_city_if_home(int civ, int unit_idx)
{
    (void)civ;
    (void)unit_idx;
    /* TODO: Orig FUN_11c0_16a4 @ 11c0:16a4 (type 0x1a settler founds
     * when home-city +10/6307 matches; needs city records 0x1c bytes
     * at 1420:5500 + map tile claim). */
    return CIV_ERR_UNSUPPORTED;
}

int map_check_zoc(const int *enemy_map, int w, int h, int x, int y)
{
    /* Orig FUN_11c0_23fd @ 11c0:23fd -> Map_CheckZOC. ZOC gate for
     * unit_step_toward_goal @ 1128:0000: 8-neighbour enemy scan. */
    int dyy;
    int dxx;

    if (!enemy_map)
        return -1;
    if (w <= 0 || h <= 0)
        return -1;
    if (x < 0 || x >= w || y < 0 || y >= h)
        return -1;
    for (dyy = -1; dyy <= 1; dyy++) {
        for (dxx = -1; dxx <= 1; dxx++) {
            int nx;
            int ny;

            if (dxx == 0 && dyy == 0)
                continue;
            nx = x + dxx;
            ny = y + dyy;
            if (nx < 0 || nx >= w || ny < 0 || ny >= h)
                continue;
            if (enemy_map[ny * w + nx] != 0)
                return 1;
        }
    }
    return 0;
}

civ_error_t city_spot_reveal(int x, int y, int civ)
{
    (void)x;
    (void)y;
    (void)civ;
    /* TODO: Orig FUN_11c0_0000 @ 11c0:0000 (city spot via 1110:180e/16ee
     * in the unit_update_visibility @ 11c0:0395 tail; sibling
     * city_update_visibility @ 11c8:00c4; needs 1420:5500 + seen-bits). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t city_update_visibility(int x, int y, int civ)
{
    (void)x;
    (void)y;
    (void)civ;
    /* TODO: Orig FUN_11c8_00c4 @ 11c8:00c4 (second spot helper in the
     * same 0395 tail; sibling city_spot_reveal @ 11c0:0000). */
    return CIV_ERR_UNSUPPORTED;
}

int unit_find_diplomat_target(int ux, int uy, const int *dx, const int *dy,
                              const int *visible, int n)
{
    /* Orig FUN_11c8_02ec @ 11c8:02ec -> Unit_FindDiplomatTarget. Type
     * 0x0e nearest visible player diplomat for unit_choose_order @
     * 1078:1480: linear Chebyshev-minimum scan. */
    int i;
    int best = -1;
    int best_d = 0;

    if (!dx || !dy || !visible)
        return -1;
    if (n <= 0)
        return -1;
    for (i = 0; i < n; i++) {
        int adx;
        int ady;
        int d;

        if (visible[i] == 0)
            continue;
        adx = dx[i] - ux;
        if (adx < 0)
            adx = -adx;
        ady = dy[i] - uy;
        if (ady < 0)
            ady = -ady;
        d = adx > ady ? adx : ady;
        if (best < 0 || d < best_d) {
            best = i;
            best_d = d;
        }
    }
    return best;
}

civ_error_t unit_do_found_city(uint8_t *units, int unit_idx, int n_units,
                               uint8_t *cities, int city_idx, int n_cities)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x62 case (jump tables
     * 1420:43b8+0x8f x0x0e, +0x4b x0x11, 1420:435b+0x28 x0x20) via
     * Unit_FoundCityIfHome @ 11c0:16a4: type 0x1a settler founds when
     * home-city +10/6307 matches the city index. Join path is OS-free
     * (consume + city_grow); new-city needs map tile claim + record
     * alloc + Win16 dialog (see map_get_tile/map_set_tile @ 11e8:0dcf/
     * 0e19, map.h, parallel-owned). No globals, no exit(). */
    uint8_t *urec;
    int utype;
    int home;
    civ_error_t gr;

    if (units == NULL || cities == NULL)
        return CIV_ERR_NOENT;
    if (n_units <= 0 || n_cities <= 0)
        return CIV_ERR_RANGE;
    if (unit_idx < 0 || unit_idx >= n_units)
        return CIV_ERR_RANGE;
    if (city_idx < 0 || city_idx >= n_cities)
        return CIV_ERR_RANGE;
    urec = units + (size_t)unit_idx * (size_t)CIV_UNIT_REC_LEN;
    utype = urec[CIV_UNIT_OFF_TYPE];
    if (utype != CIV_UNIT_TYPE_SETTLER)
        return CIV_ERR_RANGE;
    home = urec[CIV_UNIT_OFF_HOME];
    if (home != city_idx) {
        /* TODO: Orig 11c0:16a4 new-city tail + 1420:5500 alloc + map tile
         * claim (map_get_tile/map_set_tile, see map.h) + Win16 dialog
         * pump. Join (home == city_idx) is done below; founding on a
         * fresh tile stays CIV_ERR_UNSUPPORTED. */
        return CIV_ERR_UNSUPPORTED;
    }
    if (city_get_valid(cities, city_idx, n_cities) == 0)
        return CIV_ERR_RANGE;
    gr = city_grow(cities, city_idx, n_cities);
    if (gr != CIV_OK)
        return gr;
    /* Consume the settler: empty slot + no moves left. */
    urec[CIV_UNIT_OFF_TYPE] = (uint8_t)CIV_UNIT_TYPE_EMPTY;
    urec[CIV_UNIT_OFF_MOVES] = 0;
    return CIV_OK;
}

civ_error_t unit_do_fortify(uint8_t *units, int unit_idx, int n_units)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x66 case (jump tables
     * 1420:43b8+0x8f x0x0e, +0x4b x0x11, 1420:435b+0x28 x0x20):
     * wait-fortify sets +11 flags busy (ORs 0x08, see game.h AJ3 block)
     * + zeroes moves (+3) so the unit holds; combat-tail bonus not here.
     * OS-free 1:1 on explicit base. No globals, no exit(). */
    uint8_t *rec;

    if (units == NULL)
        return CIV_ERR_NOENT;
    if (n_units <= 0)
        return CIV_ERR_RANGE;
    if (unit_idx < 0 || unit_idx >= n_units)
        return CIV_ERR_RANGE;
    rec = units + (size_t)unit_idx * (size_t)CIV_UNIT_REC_LEN;
    rec[CIV_UNIT_OFF_MOVES] = 0;
    rec[CIV_UNIT_OFF_FLAGS] |= (uint8_t)CIV_UNIT_FLAG_FORTIFIED;
    return CIV_OK;
}

civ_error_t unit_do_sentry(uint8_t *units, int unit_idx, int n_units)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x68 case (same jump
     * tables): sentry parks until unit_update_visibility @ 11c0:0395
     * contact or one move elapses; Linux port ORs 0x09 (0x08 busy | 0x01
     * sentry-wait, see game.h AJ3 block) on +11 + zeroes moves (+3).
     * Exact wake predicate + 0x08 vs 0x09 split TBD. OS-free 1:1 on
     * explicit base. No globals, no exit(). */
    uint8_t *rec;

    if (units == NULL)
        return CIV_ERR_NOENT;
    if (n_units <= 0)
        return CIV_ERR_RANGE;
    if (unit_idx < 0 || unit_idx >= n_units)
        return CIV_ERR_RANGE;
    rec = units + (size_t)unit_idx * (size_t)CIV_UNIT_REC_LEN;
    rec[CIV_UNIT_OFF_MOVES] = 0;
    rec[CIV_UNIT_OFF_FLAGS] |= (uint8_t)CIV_UNIT_FLAG_SENTRY;
    return CIV_OK;
}

civ_error_t unit_do_move_attack(int civ, int unit_idx, int order)
{
    int odds = -1;
    int win;
    int step;

    /* AL1 wiring: explicit kernels with placeholder board (full 1420:62fc
     * units / 64 KB tile buffer / 1420:17d4 cost images TBD, so NULL here
     * -> step returns -1, no write; odds uses 1v1 dummy to exercise the
     * combat_calc_odds @ 11c0:1bc3 + Rng_Range @ 11c8:003d path). Real
     * per-key dispatch (0x69/0x6d/0x6f/0x70/0x72/0x75; 0x6d road?, 0x70
     * pollution?) + UI pump tail via unit_wait_for_orders @ 11c0:0ec2
     * (blink-and-pump modal, Win16) stay TBD. */
    win = unit_attack_resolve(1, 1, 0, 0, 0u, &odds);
    (void)win;
    (void)odds;
    step = unit_move_attack_step(NULL, 0, 0, NULL, 0, 0, NULL, 0, NULL, 0,
                                 0, NULL, NULL, 0, NULL, NULL, 0, 0);
    (void)step;
    (void)civ;
    (void)unit_idx;
    (void)order;
    /* TODO Orig per-key handlers + unit_wait_for_orders @ 11c0:0ec2 tail. */
    return CIV_ERR_UNSUPPORTED;
}

int unit_order_is_idle(int order)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x00: idle/none. */
    return order == 0x00;
}

int unit_order_is_sleep(int order)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x20: force-sleep/reset. */
    return order == 0x20;
}

int unit_order_is_fail(int order)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x50: settler-site fail. */
    return order == 0x50;
}

int unit_order_is_no_path(int order)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0xffff + unit_step_toward_goal
     * @ 1128:0000 no-path tail (clears 6302/6304): pure code test. The
     * 16-bit 0xffff arrives as -1 when callers use signed ints. */
    return order == 0xffff || order == -1;
}

civ_error_t unit_notify_wonder(int civ, int wonder)
{
    (void)civ;
    (void)wonder;
    /* TODO: Orig Game_DoPollutionAndWonderRace @ 1088:1e32 tail ("...
     * builds ..." message + shield deduction + dialog pump). Pick is done
     * via Game_PickWonderForAI @ 1170:3181 (game_pick_wonder_for_ai,
     * implemented, tech-table gated); needs city shields + Win16 dialog. */
    return CIV_ERR_UNSUPPORTED;
}

/* AI-C unit table: dump-confirmed subset only (see game.h). Index
 * 0x00..0x1a; unlisted slots are NULL -> "?" (no fake names). 0xff
 * ("empty") is handled in unit_type_name, not in this table. */
static const char *const s_unit_type_names[CIV_UNIT_TYPE_MAX_CONFIRMED + 1] = {
    [0x0e] = "Caravan?",
    [0x19] = "Diplomat",
    [0x1a] = "Settler",
};

const char *unit_type_name(int type)
{
    const char *s;

    /* Orig unit record +2 type @ 1420:62ff: 0xff = empty slot. */
    if (type == CIV_UNIT_TYPE_EMPTY)
        return "empty";
    if (type < 0 || type > CIV_UNIT_TYPE_MAX_CONFIRMED)
        return "?";
    s = s_unit_type_names[type];
    /* 0x0f and every undecoded slot stay "?" (TODO vs dump). */
    return s != NULL ? s : "?";
}

int unit_type_count(void)
{
    /* Max confirmed 0x1a + 1; 28th slot (0x1b) TBD vs dump (see game.h). */
    return CIV_UNIT_TYPE_MAX_CONFIRMED + 1;
}

/* Shared city-field fetch: byte at base + idx*0x1c + off, -1 OOR. */
static int city_get_field(const uint8_t *base, int idx, int n_cities,
                          int off)
{
    if (base == NULL)
        return -1;
    if (n_cities <= 0)
        return -1;
    if (idx < 0 || idx >= n_cities)
        return -1;
    return base[(size_t)idx * CIV_CITY_REC_LEN + (size_t)off];
}

int city_get_x(const uint8_t *base, int idx, int n_cities)
{
    /* Orig +0x5500 x (record +0). */
    return city_get_field(base, idx, n_cities, CIV_CITY_OFF_X);
}

int city_get_y(const uint8_t *base, int idx, int n_cities)
{
    /* Orig +0x5501 y (record +1). */
    return city_get_field(base, idx, n_cities, CIV_CITY_OFF_Y);
}

int city_get_valid(const uint8_t *base, int idx, int n_cities)
{
    /* Orig +0x5502 valid (record +2). */
    return city_get_field(base, idx, n_cities, CIV_CITY_OFF_VALID);
}

int city_get_size(const uint8_t *base, int idx, int n_cities)
{
    /* Orig +0x5503 size/def (record +3). */
    return city_get_field(base, idx, n_cities, CIV_CITY_OFF_SIZE);
}

int city_get_owner(const uint8_t *base, int idx, int n_cities)
{
    /* Orig +0x5507 owner (record +7). */
    return city_get_field(base, idx, n_cities, CIV_CITY_OFF_OWNER);
}

civ_error_t city_grow(uint8_t *cities, int city_idx, int n_cities)
{
    /* Orig city size+1 helper (1420:5500 0x1c stride, +3 size/def):
     * byte at cities + city_idx*0x1c + 3. Bounds 1..0xff (see game.h
     * AJ3 block: 0 = invalid/empty, 0xff full; gameplay cap TBD).
     * OS-free 1:1 on explicit base via city_get_size. No globals,
     * no exit(), no map/dialog I/O. */
    uint8_t *rec;
    int size;

    if (cities == NULL)
        return CIV_ERR_NOENT;
    if (n_cities <= 0)
        return CIV_ERR_RANGE;
    if (city_idx < 0 || city_idx >= n_cities)
        return CIV_ERR_RANGE;
    size = city_get_size(cities, city_idx, n_cities);
    if (size < CIV_CITY_SIZE_MIN || size >= CIV_CITY_SIZE_MAX)
        return CIV_ERR_RANGE;
    rec = cities + (size_t)city_idx * (size_t)CIV_CITY_REC_LEN;
    rec[CIV_CITY_OFF_SIZE] = (uint8_t)(size + 1);
    return CIV_OK;
}

/* --- AK1 move + accessors (2026-10-15): unit_step_toward_goal @
 * 1128:0000 scoring (Chebyshev-remaining + terrain cost gated on
 * sea/land, ZOC, occupancy, ocean-clear). OS-free, no globals, no
 * exit(), read-only (except dx/dy outs). */

int unit_move_cost(int tile, const uint8_t *cost_table, int cost_n,
                   int move_dir, int last_dir, int is_river)
{
    /* Terrain-cost tail: 1420:17d4*3-or-3 base (same formula as
     * tile_move_cost in map.h, inlined to keep game/map decoupled) +
     * river 1 + anti-reverse 0x6304 (last-dir +7) penalty. Zero-entry /
     * OOR fallback + penalty value TBD vs decompiler. */
    int base;
    int opp;

    if (cost_table == NULL || cost_n <= 0)
        return -1;
    if (tile < 0 || tile >= cost_n) {
        base = 3;
    } else {
        int v = cost_table[tile];

        base = v != 0 ? v * 3 : 3;
    }
    if (is_river != 0)
        base += CIV_UNIT_RIVER_COST;
    if (move_dir >= 1 && move_dir <= 8 && last_dir >= 1 && last_dir <= 8) {
        opp = ((last_dir - 1 + 4) % 8) + 1;
        if (move_dir == opp)
            base += CIV_UNIT_REVERSE_PENALTY;
    }
    return base;
}

int unit_can_enter(int is_sea_unit, int is_sea_tile,
                   const int *enemy_map, int ew, int eh, int x, int y,
                   const int *unit_xs, const int *unit_ys, int n_units,
                   const int *city_xs, const int *city_ys, int n_cities,
                   int ocean_depth)
{
    /* Entry gates: 0x1a3a vs Map_IsSeaLand @ 1110:16c0 == 10, ZOC @
     * 11c0:23fd (inlined 8-neighbour scan), occupancy @ 1110:18ce/16ee
     * (inlined linear scan), city-blocked @ 1110:199b (inlined linear
     * scan), ocean @ 1110:1dbd > 4 (explicit depth param). 1 = enter,
     * 0 = blocked. NULL/empty maps mean clear for that gate. */
    int sea_unit = is_sea_unit != 0;
    int sea_tile = is_sea_tile != 0;
    int dyy;
    int dxx;
    int i;

    if (sea_unit != sea_tile)
        return 0;
    if (enemy_map != NULL && ew > 0 && eh > 0) {
        if (x < 0 || x >= ew || y < 0 || y >= eh)
            return 0;
        for (dyy = -1; dyy <= 1; dyy++) {
            for (dxx = -1; dxx <= 1; dxx++) {
                int nx;
                int ny;

                if (dxx == 0 && dyy == 0)
                    continue;
                nx = x + dxx;
                ny = y + dyy;
                if (nx < 0 || nx >= ew || ny < 0 || ny >= eh)
                    continue;
                if (enemy_map[ny * ew + nx] != 0)
                    return 0;
            }
        }
    }
    if (unit_xs != NULL && unit_ys != NULL && n_units > 0) {
        for (i = 0; i < n_units; i++) {
            if (unit_xs[i] == x && unit_ys[i] == y)
                return 0;
        }
    }
    if (city_xs != NULL && city_ys != NULL && n_cities > 0) {
        for (i = 0; i < n_cities; i++) {
            if (city_xs[i] == x && city_ys[i] == y)
                return 0;
        }
    }
    if (sea_tile != 0 && ocean_depth <= 4)
        return 0;
    return 1;
}

civ_error_t unit_dir_to_dxdy(int dir, int *dx_out, int *dy_out)
{
    /* Dir 1..8 -> dx/dy (1 = N clockwise, same as map_resolve_direct @
     * 1110:1c4b; exact orig numbering TBD). Index 0 = arrived (0,0). */
    static const int dx[9] = { 0, 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int dy[9] = { 0, -1, -1, 0, 1, 1, 1, 0, -1 };

    if (dx_out == NULL || dy_out == NULL)
        return CIV_ERR_NOENT;
    if (dir == CIV_UNIT_DIR_NO_PATH || dir == -1)
        return CIV_ERR_RANGE;
    if (dir < 0 || dir > 8)
        return CIV_ERR_RANGE;
    *dx_out = dx[dir];
    *dy_out = dy[dir];
    return CIV_OK;
}

int unit_home(const uint8_t *units, int unit_idx, int n_units)
{
    /* Unit +10 home-city (1420:62fc stride 0xc): 0xff = none -> -1. */
    int v;

    if (units == NULL || n_units <= 0 || unit_idx < 0 || unit_idx >= n_units)
        return -1;
    v = units[(size_t)unit_idx * (size_t)CIV_UNIT_REC_LEN +
              (size_t)CIV_UNIT_OFF_HOME];
    if (v == 0xff)
        return -1;
    return v;
}

int unit_moves_left(const uint8_t *units, int unit_idx, int n_units)
{
    /* Unit +3 moves/counter. */
    if (units == NULL || n_units <= 0 || unit_idx < 0 || unit_idx >= n_units)
        return -1;
    return units[(size_t)unit_idx * (size_t)CIV_UNIT_REC_LEN +
                 (size_t)CIV_UNIT_OFF_MOVES];
}

int unit_seen_by(const uint8_t *units, int unit_idx, int n_units)
{
    /* Unit +8 seen-by mask. */
    if (units == NULL || n_units <= 0 || unit_idx < 0 || unit_idx >= n_units)
        return -1;
    return units[(size_t)unit_idx * (size_t)CIV_UNIT_REC_LEN +
                 (size_t)CIV_UNIT_OFF_SEEN];
}

int city_get_prod(const uint8_t *base, int idx, int n_cities)
{
    /* TODO: Orig city production/shields offset TBD from the 0x1c
     * records at 1420:5500 (known: +0 x, +1 y, +2 valid, +3 size/def,
     * +7 owner). No fake offset defined; always -1 until decoded. */
    if (base == NULL)
        return -1;
    if (n_cities <= 0)
        return -1;
    if (idx < 0 || idx >= n_cities)
        return -1;
    return -1;
}

/* --- AL1 move_attack + tables (2026-10-15): unit_step_toward_goal @
 * 1128:0000 single-step core + combat odds/roll + tech/wonder/cost
 * tables. OS-free, no globals, no exit(). See game.h AL1 block for
 * parent/parity/TBD notes. */

int unit_move_attack_step(uint8_t *units, int unit_idx, int n_units,
                          const uint8_t *tiles, int bw, int bh,
                          const uint8_t *cost_table, int cost_n,
                          const int *enemy_map, int ew, int eh,
                          const int *unit_xs, const int *unit_ys,
                          int n_occ_units,
                          const int *city_xs, const int *city_ys, int n_cities,
                          int is_sea_unit)
{
    uint8_t *rec;
    int ux;
    int uy;
    int moves;
    int gx;
    int gy;
    int last_dir;
    int dx;
    int dy;
    int adx;
    int ady;
    int dir;
    int best_dir;
    int best_score;
    int best_nx;
    int best_ny;

    if (units == NULL || n_units <= 0 || unit_idx < 0 || unit_idx >= n_units)
        return -1;
    if (tiles == NULL || bw <= 0 || bh <= 0)
        return -1;
    if (cost_table == NULL || cost_n <= 0)
        return -1;
    rec = units + (size_t)unit_idx * (size_t)CIV_UNIT_REC_LEN;
    ux = rec[CIV_UNIT_OFF_X];
    uy = rec[CIV_UNIT_OFF_Y];
    moves = rec[CIV_UNIT_OFF_MOVES];
    gx = rec[CIV_UNIT_OFF_GOAL_X];
    gy = rec[CIV_UNIT_OFF_GOAL_Y];
    last_dir = rec[CIV_UNIT_OFF_LAST_DIR];
    /* Goal none (0xff, see header top) -> nothing to do. */
    if (gx == 0xff || gy == 0xff)
        return CIV_UNIT_DIR_ARRIVED;
    /* Arrived / exhausted -> 0, no write (goal-clear policy TBD). */
    if (ux == gx && uy == gy)
        return CIV_UNIT_DIR_ARRIVED;
    if (moves <= 0)
        return CIV_UNIT_DIR_ARRIVED;
    /* Direct (|dx|<2,|dy|<2, cf. map_resolve_direct @ 1110:1c4b): try the
     * goal-adjacent step first, gated by unit_can_enter. Falls through
     * to full scoring when not adjacent or blocked (TBD vs decompiler
     * whether blocked direct goes straight to 0xffff). */
    dx = gx - ux;
    dy = gy - uy;
    adx = dx < 0 ? -dx : dx;
    ady = dy < 0 ? -dy : dy;
    if (adx < 2 && ady < 2) {
        int ddx;
        int ddy;
        int nx;
        int ny;
        int tile;
        int is_sea;
        int is_river;
        int depth;
        int cx;
        int cy;
        int i;

        dir = -1;
        /* 1=N clockwise match for (dx,dy), same table as
         * unit_dir_to_dxdy (exact orig numbering TBD). */
        if (dx == 0 && dy == -1)
            dir = 1;
        else if (dx == 1 && dy == -1)
            dir = 2;
        else if (dx == 1 && dy == 0)
            dir = 3;
        else if (dx == 1 && dy == 1)
            dir = 4;
        else if (dx == 0 && dy == 1)
            dir = 5;
        else if (dx == -1 && dy == 1)
            dir = 6;
        else if (dx == -1 && dy == 0)
            dir = 7;
        else if (dx == -1 && dy == -1)
            dir = 8;
        if (dir >= 1 && dir <= 8) {
            if (unit_dir_to_dxdy(dir, &ddx, &ddy) == CIV_OK) {
                nx = ux + ddx;
                ny = uy + ddy;
                if (nx >= 0 && nx < bw && ny >= 0 && ny < bh) {
                    tile = tiles[(size_t)ny * (size_t)bw + (size_t)nx];
                    is_sea = (tile == 10) ? 1 : 0;
                    is_river = (tile == 0x0c) ? 1 : 0;
                    (void)is_river;
                    /* Ocean depth from target along step, capped at 5
                     * (cf. map_ocean_clear_depth @ 1110:1dbd >4). */
                    depth = 0;
                    cx = nx;
                    cy = ny;
                    for (i = 0; i < 5; i++) {
                        int t2;

                        if (cx < 0 || cx >= bw || cy < 0 || cy >= bh)
                            break;
                        t2 = tiles[(size_t)cy * (size_t)bw + (size_t)cx];
                        if (t2 != 10)
                            break;
                        depth++;
                        cx += ddx;
                        cy += ddy;
                    }
                    if (unit_can_enter(is_sea_unit, is_sea, enemy_map, ew,
                                       eh, nx, ny, unit_xs, unit_ys,
                                       n_occ_units, city_xs, city_ys,
                                       n_cities, depth) != 0) {
                        rec[CIV_UNIT_OFF_X] = (uint8_t)nx;
                        rec[CIV_UNIT_OFF_Y] = (uint8_t)ny;
                        rec[CIV_UNIT_OFF_LAST_DIR] = (uint8_t)dir;
                        rec[CIV_UNIT_OFF_MOVES] =
                            (uint8_t)(moves > 0 ? moves - 1 : 0);
                        return dir;
                    }
                }
            }
        }
    }
    /* Full 8-neighbour best-score: Chebyshev-remaining + move_cost. */
    best_dir = CIV_UNIT_DIR_NO_PATH;
    best_score = 0;
    best_nx = ux;
    best_ny = uy;
    for (dir = 1; dir <= 8; dir++) {
        int ddx;
        int ddy;
        int nx;
        int ny;
        int tile;
        int is_sea;
        int is_river;
        int depth;
        int cx;
        int cy;
        int i;
        int cost;
        int rdx;
        int rdy;
        int rem;
        int score;

        if (unit_dir_to_dxdy(dir, &ddx, &ddy) != CIV_OK)
            continue;
        nx = ux + ddx;
        ny = uy + ddy;
        if (nx < 0 || nx >= bw || ny < 0 || ny >= bh)
            continue;
        tile = tiles[(size_t)ny * (size_t)bw + (size_t)nx];
        is_sea = (tile == 10) ? 1 : 0;
        is_river = (tile == 0x0c) ? 1 : 0;
        depth = 0;
        cx = nx;
        cy = ny;
        for (i = 0; i < 5; i++) {
            int t2;

            if (cx < 0 || cx >= bw || cy < 0 || cy >= bh)
                break;
            t2 = tiles[(size_t)cy * (size_t)bw + (size_t)cx];
            if (t2 != 10)
                break;
            depth++;
            cx += ddx;
            cy += ddy;
        }
        if (unit_can_enter(is_sea_unit, is_sea, enemy_map, ew, eh, nx, ny,
                           unit_xs, unit_ys, n_occ_units, city_xs, city_ys,
                           n_cities, depth) == 0)
            continue;
        cost = unit_move_cost(tile, cost_table, cost_n, dir, last_dir,
                              is_river);
        if (cost < 0)
            continue;
        rdx = gx - nx;
        rdx = rdx < 0 ? -rdx : rdx;
        rdy = gy - ny;
        rdy = rdy < 0 ? -rdy : rdy;
        rem = rdx > rdy ? rdx : rdy;
        score = rem + cost;
        if (best_dir == CIV_UNIT_DIR_NO_PATH || score < best_score) {
            best_dir = dir;
            best_score = score;
            best_nx = nx;
            best_ny = ny;
        }
    }
    if (best_dir == CIV_UNIT_DIR_NO_PATH) {
        /* No path: clear goal-x/goal-y/last-dir to 0xff (exact 6302/6304
         * mask TBD, 6303? included here; see unit_order_is_no_path). */
        rec[CIV_UNIT_OFF_GOAL_X] = 0xff;
        rec[CIV_UNIT_OFF_GOAL_Y] = 0xff;
        rec[CIV_UNIT_OFF_LAST_DIR] = 0xff;
        return CIV_UNIT_DIR_NO_PATH;
    }
    rec[CIV_UNIT_OFF_X] = (uint8_t)best_nx;
    rec[CIV_UNIT_OFF_Y] = (uint8_t)best_ny;
    rec[CIV_UNIT_OFF_LAST_DIR] = (uint8_t)best_dir;
    rec[CIV_UNIT_OFF_MOVES] = (uint8_t)(moves > 0 ? moves - 1 : 0);
    return best_dir;
}

int unit_attack_resolve(int atk_strength, int def_strength,
                        int atk_flags, int def_flags,
                        unsigned rand15, int *odds_out)
{
    int atk_eff;
    int def_eff;
    int odds;
    int roll;

    /* Veteran 0x20 (unit +11, see game.h header top) pre-scale TBD vs
     * combat_resolve_attack stub (no fake multipliers); pass-through. */
    (void)atk_flags;
    (void)def_flags;
    atk_eff = atk_strength;
    def_eff = def_strength;
    odds = combat_calc_odds(atk_eff, def_eff);
    if (odds_out != NULL)
        *odds_out = odds;
    if (odds < 0)
        return -1;
    /* Rng_Range @ 11c8:003d via game_uprising_rng_scale (0..99). Caller
     * passes the raw rand15 draw (0..0x7FFF) or a test vector. */
    roll = (int)game_uprising_rng_scale(rand15, 100);
    return roll < odds ? 1 : 0;
}

const char *tech_names(int tech)
{
    (void)tech;
    /* TODO: Orig tech table for Game_UpdatePowerRankings @ 1088:108a
     * (tech contribution, table TBD) + Game_PickWonderForAI @ 1170:3181
     * (tech-gated via game_pick_wonder_for_ai prereq indices). Checked
     * game.c: only prereq indices, no string literals, so no
     * dump-confirmed names yet; Alphabet/.../Nuclear hypothesis stays
     * TODO (no fake). Confirmed subset empty + "?" unknown. */
    return "?";
}

const char *wonder_names(int wonder)
{
    (void)wonder;
    /* TODO: Orig wonder table for Game_PickWonderForAI @ 1170:3181
     * (eligible[w] via game_pick_wonder_for_ai) + "... builds ..." tail
     * via unit_notify_wonder @ 1088:1e32. Checked game.c: only wonder
     * indices, no string literals, so no confirmed names; 7-wonders
     * hypothesis (Pyramids/Hanging/Oracle/etc.) stays TODO (no fake). */
    return "?";
}

int terrain_cost_table(int tile)
{
    (void)tile;
    /* TODO: Orig 1420:17d4 terrain base-cost table for unit_move_cost /
     * tile_move_cost (*3-or-3). Entries/stride TBD (0x?? entries?),
     * so no image yet (no fake entries); consumers take the explicit
     * cost_table image until the fixed image is decoded. Always -1. */
    return -1;
}

void game_shutdown(civ_game_t *g)
{
    if (g)
        g->initialized = 0;
}

civ_error_t city_format_pop_count(int pop, const char *sep_thousands,
                                  const char *pad_zero, const char *suffix,
                                  char *out, size_t cap)
{
    /* Orig FUN_11c8_03ac @ 11c8:03ac -> City_FormatPopCount.
     * Decompiler: if (99 < pop) { q = pop / 100; Util_IntToDec(q) +
     * StrAppend; StrAppend(0x42ec); if (pop % 100 < 10)
     * StrAppend(0x42ee); } r = pop % 100; Util_IntToDec(r) + StrAppend;
     * StrAppend(0x42f0). Separators live in DGROUP (12d8:42ec/42ee/42f0,
     * image-zero, runtime-filled; "," / "0" / suffix hypothesis from the
     * "00,000 citizens." @ 1420:172b neighbour, exact bytes TBD), so
     * they arrive as caller params here (no globals). Negative pop is
     * treated as 0 (orig compared signed 99 < pop, then %/div; game
     * totals are non-negative). OS-free 1:1, no Win16, no exit(). */
    char num[16];
    size_t pos = 0;
    size_t k = 0;
    int r = 0;

    if (!out || !sep_thousands || !pad_zero || !suffix)
        return CIV_ERR_NOENT;
    if (cap == 0)
        return CIV_ERR_RANGE;
    out[0] = '\0';
    if (pop < 0)
        pop = 0;
    if (pop > 99) {
        int q = pop / 100;

        (void)util_int_to_dec(num, q, 10);
        for (k = 0; num[k] != '\0'; k++) {
            if (pos + 1u >= cap)
                return CIV_ERR_RANGE;
            out[pos++] = num[k];
        }
        for (k = 0; sep_thousands[k] != '\0'; k++) {
            if (pos + 1u >= cap)
                return CIV_ERR_RANGE;
            out[pos++] = sep_thousands[k];
        }
        if (pop % 100 < 10) {
            for (k = 0; pad_zero[k] != '\0'; k++) {
                if (pos + 1u >= cap)
                    return CIV_ERR_RANGE;
                out[pos++] = pad_zero[k];
            }
        }
    }
    r = pop % 100;
    (void)util_int_to_dec(num, r, 10);
    for (k = 0; num[k] != '\0'; k++) {
        if (pos + 1u >= cap)
            return CIV_ERR_RANGE;
        out[pos++] = num[k];
    }
    for (k = 0; suffix[k] != '\0'; k++) {
        if (pos + 1u >= cap)
            return CIV_ERR_RANGE;
        out[pos++] = suffix[k];
    }
    out[pos] = '\0';
    return CIV_OK;
}

int city_calc_pop_total(const int *owner_of, const int *valid_of,
                        const int *size_of, int n, int owner)
{
    /* Orig FUN_11c8_0334 @ 11c8:0334 -> City_CalcPopTotal.
     * Decompiler: for i in 0..0x80 with city[i].owner (+7 @ 1420:5507)
     * == civ and city[i].valid (+2 @ 1420:5502) != -1, for s in
     * 1..city[i].size (+3 @ 1420:5503) total += s; then
     * City_FormatPopCount(total) (format tail lives in
     * city_format_pop_count above; this helper returns the raw total).
     * Size <= 0 contributes 0 (inner loop skipped). OS-free 1:1 on
     * explicit arrays (no globals, no 0x80 hardcode beyond the caller
     * n). */
    int total = 0;
    int i = 0;
    int s = 0;

    if (!owner_of || !valid_of || !size_of || n <= 0)
        return -1;
    for (i = 0; i < n; i++) {
        if (owner_of[i] == owner && valid_of[i] != -1) {
            for (s = 1; s <= size_of[i]; s++)
                total += s;
        }
    }
    return total;
}

civ_error_t city_format_name(const uint8_t *p1, const uint8_t *p2,
                             uint8_t *dst, size_t cap)
{
    /* Orig FUN_1048_0101 @ 1048:0101 -> City_FormatName join tail +
     * FUN_11f8_014a @ 11f8:014a -> Res_AppendPascal (see rsc_loader.h
     * res_append_pascal). Decompiler: *name = 0, then
     * Res_AppendPascal(name, res_string_0x88_4) + Res_AppendPascal(name,
     * local_10c copy) with the 0x88,5 conditional via the FUN_1008_0d93
     * + GLOBALLOCK gate (handle tail stays Win16). Pure join here:
     * dst[0] = 0 then append p1/p2 length-prefixed (u8 len + bytes).
     * OS-free 1:1, no Win16, no globals. */
    size_t len = 0;
    size_t n = 0;
    size_t i = 0;

    if (!dst || cap == 0)
        return CIV_ERR_NOENT;
    dst[0] = 0;
    len = 0;
    const uint8_t *parts[2] = { p1, p2 };

    for (n = 0; n < 2u; n++) {
        const uint8_t *src = parts[n];
        unsigned sl = 0;

        if (!src)
            continue;
        sl = src[0];
        if (len + sl > 255u)
            return CIV_ERR_RANGE;
        if (len + sl + 1u > cap)
            return CIV_ERR_RANGE;
        for (i = 0; i < sl; i++)
            dst[1u + len + i] = src[1u + i];
        len += sl;
        dst[0] = (uint8_t)len;
    }
    return CIV_OK;
}

/* --- Census / demographics / status-content (2026-10-16): OS-free 1:1.
 * No globals, no exit(), C17. GDI/dialog tails stay STUB below. */

civ_error_t city_format_census_ratio(int num, int den, char *out, size_t cap)
{
    /* Orig City_FormatCensusRatio @ 1018:202a. Decompiler: num < 300 ?
     * q = (num*100)/den, r = (num*100)%den : q = (num*10)/(den/10),
     * r = (num*10)%(den/10); then IntToDec(q) + StrAppend(24fa). The
     * remainder r is computed but never appended (4th StrAppend arg is
     * the far-stack slot, ignored): output is the quotient text only. */
    int q;
    char tmp[16];
    size_t k = 0;

    if (!out)
        return CIV_ERR_NOENT;
    if (cap == 0)
        return CIV_ERR_RANGE;
    if (den <= 0)
        return CIV_ERR_RANGE;
    if (num < 300) {
        q = (num * 100) / den;
    } else {
        int d10 = den / 10;

        if (d10 == 0)
            return CIV_ERR_RANGE;
        q = (num * 10) / d10;
    }
    (void)util_int_to_dec(tmp, q, 10);
    for (k = 0; tmp[k] != '\0'; k++) {
        if (k + 1u >= cap)
            return CIV_ERR_RANGE;
        out[k] = tmp[k];
    }
    if (k + 1u > cap)
        return CIV_ERR_RANGE;
    out[k] = '\0';
    return CIV_OK;
}

int demo_approval(int happy, int pop)
{
    /* Orig Demo_ShowDemographics @ 1188:123a approval row:
     * local_1a[civ] = (local_3a[civ] * 0x32) / local_2a[civ]. */
    if (pop <= 0)
        return -1;
    return (happy * 0x32) / pop;
}

int demo_literacy(int schools, int pop, const int *tech_owned, int n_tech)
{
    /* Orig Demo literacy row: base = ((local_6a + pop*2)*2)/pop, then
     * x2 per Game_PickWonderForAI tech @ 1170:3181 among
     * (0, 0x16, 0x1e, 0x0d), then Util_Clamp(v,0,100) @ 11c8:0063. The
     * exact tech ids are wonder-prereq indices (see
     * game_pick_wonder_for_ai); callers pass flags in the same order. */
    static const int kDoubleTechs[4] = { 0, 0x16, 0x1e, 0x0d };
    int v;
    int t;

    if (pop <= 0)
        return -1;
    v = ((schools + pop * 2) * 2) / pop;
    if (tech_owned != NULL && n_tech > 0) {
        for (t = 0; t < 4; t++) {
            int id = kDoubleTechs[t];

            if (id >= 0 && id < n_tech && tech_owned[id] != 0)
                v *= 2;
        }
    }
    if (v < 0)
        v = 0;
    if (v > 100)
        v = 100;
    return v;
}

int demo_disease_rate(int pop, int impr, int has_medicine)
{
    /* Orig Demo disease row: v = (pop*0x32)/(pop+impr); medicine tech
     * 10 @ 1170:3181 halves it. */
    int denom;
    int v;

    if (pop < 0)
        return -1;
    denom = pop + impr;
    if (denom <= 0)
        return -1;
    v = (pop * 0x32) / denom;
    if (has_medicine != 0)
        v /= 2;
    return v;
}

int demo_life_years(int v)
{
    /* Orig disease-row tail: local_5a[civ] = 0x5dc/(v+0x14). */
    if (v < 0)
        return -1;
    return 0x5dc / (v + 0x14);
}

int demo_rank_player(const int *vals, int n, int player, unsigned alive_mask)
{
    /* Orig Demo_RankRow @ 1188:220a ranking half: local_8 = 1, then for
     * each alive civ 1..7 with vals[civ] > vals[player] local_8++. Dead
     * civs (mask bit clear) are skipped. vals indexes 0..n-1 with civ ids
     * 1..7 (index 0 unused, like local_1a[8] in decompiler). */
    int rank;
    int c;

    if (!vals)
        return -1;
    if (player < 1 || player > 7)
        return -1;
    if (n <= player)
        return -1;
    rank = 1;
    for (c = 1; c <= 7 && c < n; c++) {
        if (c == player)
            continue;
        if ((alive_mask & (1u << ((unsigned)c & 31u))) == 0)
            continue;
        if (vals[c] > vals[player])
            rank++;
    }
    return rank;
}

unsigned status_lcg_next(unsigned seed)
{
    /* Orig Status_DrawTechIcons @ 1088:1a82 AI-fake step: local_e =
     * local_e*5+1 (32-bit wrap), pick = (local_e & 0xff) % 9 is done by
     * the caller (needs the %9 for the icon slot, cap 999 tries,
     * adjacency/budget gates in the caller STUB). */
    return seed * 5u + 1u;
}

civ_error_t demo_show_demographics(int civ)
{
    (void)civ;
    /* TODO: Orig Demo_ShowDemographics @ 1188:123a (FUN_1188_0000 open +
     * live city-record sums over 0x80 0x1c records @ 1420:5500 + tile scan
     * via Map_IsSeaLand @ 1110:16c0/Map 1110:16ee + Demo_RankRow @
     * 1188:220a per row + City_CloseCensusView @ 1018:0380 close).
     * Ranking/math kernels above are LIVE; GDI/dialog tails -> SDL. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t demo_rank_row(const int *vals, int n, int player)
{
    (void)vals;
    (void)n;
    (void)player;
    /* TODO: Orig Demo_RankRow @ 1188:220a bar/text tail (FUN_10d8_0000
     * bars + FUN_10d8_01fa rank text "1st/2nd/3rd/Nth" @ 1420:3ced/3cf0/
     * 3cf3/3cf6 + leader line when the best beats the player). Ranking
     * half lives in demo_rank_player above (LIVE); GDI tail -> SDL. */
    if (!vals)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t city_show_census_report(int civ)
{
    (void)civ;
    /* TODO: Orig City_ShowCensusReport @ 1018:1d5b (City_OpenCensusView @
     * 1018:0000 dialog + shared PIC viewer Pic_PlayShared @ 1030:0000 +
     * building icons 0x2618 mask + City_FormatCensusRatio @ 1018:202a x3 +
     * City_CloseCensusView @ 1018:0380). Win16 dialog/GDI -> SDL. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t census_show_tech_bars(int civ)
{
    (void)civ;
    /* TODO: Orig Census_ShowTechBars @ 1018:03f0 (Res 0x8a/5 title +
     * City_OpenCensusView @ 1018:0000 + 0x0c flag-bar segments via
     * Unit_DrawFlagIcon @ 11c8:008a + Res 0x82 tech list + Close @
     * 1018:0380). Win16 dialog/GDI -> SDL TBD. Index math lives in
     * unit_flag_icon_index (video.h, LIVE). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t census_show_unit_list(int civ)
{
    (void)civ;
    /* TODO: Orig Census_ShowUnitList @ 1018:067b (per-unit rows + flag icons
     * civ*0x20+unit+0x40 via Unit_DrawFlagIcon @ 11c8:008a + Res 0x84 names +
     * stats at 1a40/1a42/1a3c). Win16 -> SDL TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t census_show_terrain_counts(int civ)
{
    (void)civ;
    /* TODO: Orig Census_ShowTerrainCounts @ 1018:09cc (per-terrain rows +
     * same flag icons + Res 0x84 + per-civ counts at 2b90). Win16 -> SDL. */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Trade / diplomat / revolt / shared-PIC chain (2026-10-16): OS-free
 * 1:1 LIVE kernels + Win16 STUBs. No globals, no exit(), C17. See game.h
 * chain block for callers/callees/addrs. */

int trade_calc_bonus(int dist, int size_a, int size_b, int same_cont,
                     int domestic, int has_t31, int has_t26)
{
    /* Orig Trade_DeliverCaravanGoods @ 1038:0d09: base =
     * (dist+10)*(sizes)/0x18; same-continent /2 (Map_GetContinentId @
     * 1110:1d99 equality via Map_GetTile @ 11e8:0dcf(x, y+0x32));
     * domestic /2 (owner(dst) == civ); tech 0x31/0x26 each -= /3 (via
     * Game_PickWonderForAI @ 1170:3181 nonzero). Integer division in
     * orig order. */
    int base;

    if (dist < 0)
        dist = 0;
    if (size_a < 0)
        size_a = 0;
    if (size_b < 0)
        size_b = 0;
    base = (dist + 10) * (size_a + size_b) / 0x18;
    if (same_cont != 0)
        base /= 2;
    if (domestic != 0)
        base /= 2;
    if (has_t31 != 0)
        base -= base / 3;
    if (has_t26 != 0)
        base -= base / 3;
    return base;
}

int trade_route_value(int size, int foreign)
{
    /* Orig Trade_DeliverCaravanGoods @ 1038:0d09 route slots @ 1420:5513:
     * local_8 = size, foreign owner -> *2; best-3 kept by this value. */
    if (size < 0)
        return -1;
    if (foreign != 0)
        return size * 2;
    return size;
}

civ_error_t trade_deliver_caravan_goods(int civ, int unit_idx, int city_idx)
{
    (void)civ;
    (void)unit_idx;
    (void)city_idx;
    /* TODO: Orig Trade_DeliverCaravanGoods @ 1038:0d09 (bonus kernel above
     * is LIVE in trade_calc_bonus; route value in trade_route_value).
     * Tail needs city records @ 1420:5500 (x/y/5506/5507/5513 slots) +
     * treasury cells + Res 0x8e/City_AppendName/Dialog_ShowMessage @
     * 1220:00d2 + Game_UpdateStatusPanel @ 1088:13d5 + consume via
     * Unit_FoundCityIfHome @ 11c0:16a4. */
    return CIV_ERR_UNSUPPORTED;
}

int diplomat_revolt_cost(int treasury, int dist, int size, int half)
{
    /* Orig Diplomat_CityAction @ 11b8:0000 case 4: ((treas+1000)/(dist+3))
     * *size (long math in orig), /2 when 1420:5502&1. dist is the
     * 1168:12c1 value. */
    long price;

    if (treasury < 0 || dist < 0 || size < 0)
        return -1;
    price = (long)(treasury + 1000) / (long)(dist + 3) * (long)size;
    if (half != 0)
        price /= 2;
    return (int)price;
}

int diplomat_bribe_cost(int treasury, int dist, int unit_cost, int type)
{
    /* Orig Diplomat_BribeUnit @ 11b8:07c2: ((treas+0x2ee)/(dist+2)) *
     * shield-cost (0x1a44 table by unit type), /2 when type != 0. dist
     * is the 1168:12c1 value. */
    long price;

    if (treasury < 0 || unit_cost < 0 || dist < -1)
        return -1;
    price = (long)(treasury + 0x2ee) / (long)(dist + 2) * (long)unit_cost;
    if (type != 0)
        price /= 2;
    return (int)price;
}

civ_error_t diplomat_city_action(int city_idx, int civ, int unit_idx)
{
    (void)city_idx;
    (void)civ;
    (void)unit_idx;
    /* TODO: Orig Diplomat_CityAction @ 11b8:0000 cases 0..5 (cost kernels
     * above LIVE: revolt via diplomat_revolt_cost; steal via Rng 0x48 +
     * Res 0x82; sabotage via Rng 2/0x18 + Res 0x83/0x84; embassy bit 0x40
     * @ 12d8:3a0a + FUN_1018_1739; investigate via FUN_1170_05af).
     * Needs diplo bits + city records + Rng_Range @ 11c8:003d + Win16
     * dialog (Dialog_ShowMessage @ 1220:00d2, DAT_1418_0002/0006). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t diplomat_bribe_unit(int civ, int unit_idx, int by_civ)
{
    (void)civ;
    (void)unit_idx;
    (void)by_civ;
    /* TODO: Orig Diplomat_BribeUnit @ 11b8:07c2 (price kernel above LIVE
     * in diplomat_bribe_cost; 11c0:1bc3 dist<2 gate + Res 0x84 name +
     * Map_CenterOnTile @ 1110:03ae + consume @ 11c0:16a4 + respawn @
     * 11c0:12c8 + seen-bit flip + treasury deduct + Game_UpdateStatusPanel
     * @ 1088:13d5 + FUN_11e8_02a4(0x1e)). Needs unit records + treasury. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t city_do_revolt(int city_idx, int new_owner)
{
    (void)city_idx;
    (void)new_owner;
    /* TODO: Orig City_DoRevolt @ 11b8:0f26 (-1 auto-pick via
     * (00de-00e0)*0x20/dist scan; seen-bit flips x*0x32+y+10 + 0x14
     * diamond; near-unit moves via 1110:171f/1906 + 11c0:12c8; message
     * 1420:4168/4182 + Dialog_ShowMessage @ 1220:00d2; Unit_DoMove @
     * 1038:0000(new_owner, city, 1) tail). Needs city/unit records. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t pic_play_shared(int city_idx, int mode)
{
    (void)city_idx;
    (void)mode;
    /* TODO: Orig Pic_PlayShared @ 1030:0000 (mode 1 modal via
     * Dlg_CreateTracked @ 1098:124f + tile helpers City_RenderWorkedTile
     * @ 1030:79ce / City_TileYield @ 1030:7c28 / City_IsWonderOpen @
     * 1030:7e57 / Tech_FindFirstOwner @ 1030:7ece / City_SetSpecialist
     * @ 1030:7f65 / City_GetSpecialist @ 1030:7fc1; mode 0 background
     * sim tick incl. caravan 0x1b via Trade_DeliverCaravanGoods @
     * 1038:0d09; NOT the intro loop, see title.h). Needs city records
     * + GDI modal. Mode split itself is LIVE in pic_mode_is_modal /
     * pic_mode_is_sim below. */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16: Pic modes + 0x1b caravan + unit loop + tech ---
 * OS-free 1:1 LIVE kernels + Win16 STUBs. No globals, no exit(), C17.
 * See game.h chain block for callers/callees/addrs. */

int tech_has_tech(int civ, int tech, const uint16_t *bits, int n_civs)
{
    /* Orig Tech_HasTech @ 1170:3181: (tech == -1) || (tech < 0x47 &&
     * civ != 0 && bit set in ESCHISM+5+civ*10 row, word tech/0x10, bit
     * tech%0x10). Decompiler: *(ESCHISM+5+civ*10+(tech/0x10)*2) &
     * (1 << (tech%0x10)). */
    unsigned word;
    unsigned bit;

    if (tech == -1)
        return 1;
    if (tech < 0 || tech >= 0x47)
        return 0;
    if (civ == 0)
        return 0;
    if (bits == NULL || n_civs <= 0)
        return 0;
    if (civ < 0 || civ >= n_civs)
        return 0;
    word = (unsigned)tech / 0x10u;
    bit = (unsigned)tech % 0x10u;
    if (word >= 5u)
        return 0;
    return (bits[(size_t)civ * 5u + word] & (uint16_t)(1u << bit)) != 0;
}

int pic_mode_is_modal(int mode)
{
    /* Orig Pic_PlayShared @ 1030:0000: param_2 == 1 opens the modal city
     * view (Dlg_CreateTracked @ 1098:124f dialog 0x94). */
    return mode == 1;
}

int pic_mode_is_sim(int mode)
{
    /* Orig Pic_PlayShared @ 1030:0000: param_2 == 0 runs the background
     * sim tick (growth/prod/trade incl. caravan prod 0x1b). */
    return mode == 0;
}

int city_get_specialist(const uint8_t *base, int city_idx, int n_cities,
                         int slot)
{
    /* Orig City_GetSpecialist @ 1030:7fc1: 2-bit value at slot*2 in the
     * city specialist word (1420:5510 = rec+0x10); slot >= 8 -> 1. LE
     * word: base[off] | base[off+1]<<8. */
    size_t off;
    unsigned w;

    if (base == NULL || n_cities <= 0 || city_idx < 0 || city_idx >= n_cities)
        return -1;
    if (slot < 0)
        return -1;
    if (slot >= 8)
        return 1;
    off = (size_t)city_idx * (size_t)CIV_CITY_REC_LEN + 0x10u;
    w = (unsigned)base[off] | ((unsigned)base[off + 1u] << 8);
    return (int)((w >> ((unsigned)slot * 2u)) & 3u);
}

civ_error_t city_set_specialist(uint8_t *base, int city_idx, int n_cities,
                                int slot, int value)
{
    /* Orig City_SetSpecialist @ 1030:7f65: clear + set 2 bits at slot*2
     * (orig shifted raw value; mask keeps neighbours intact). */
    size_t off;
    unsigned w;

    if (base == NULL)
        return CIV_ERR_NOENT;
    if (n_cities <= 0 || city_idx < 0 || city_idx >= n_cities)
        return CIV_ERR_RANGE;
    if (slot < 0)
        return CIV_ERR_RANGE;
    if (slot >= 8)
        return CIV_OK;
    off = (size_t)city_idx * (size_t)CIV_CITY_REC_LEN + 0x10u;
    w = (unsigned)base[off] | ((unsigned)base[off + 1u] << 8);
    w &= ~(3u << ((unsigned)slot * 2u));
    w |= ((unsigned)(value & 3) << ((unsigned)slot * 2u));
    base[off] = (uint8_t)(w & 0xffu);
    base[off + 1u] = (uint8_t)((w >> 8) & 0xffu);
    return CIV_OK;
}

int unit_is_settler(int type)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 CMP 62ff,0x1a @ 11f0:1ecd:
     * JGE takes the settler/city path (11c8:00c4 lookup), JL jumps to
     * normal move @ 11f0:228a; second CMP @ 11f0:1f24 JZ continues the
     * settler path, else JMP 1fff diplomat test. */
    return type == 0x1a;
}

int unit_is_caravan(int type)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 CMP 62ff,0x1b @ 11f0:2012:
     * JZ enters the 0x1b caravan branch @ 11f0:201d (home-city 6307 x/y
     * + Map_WrapStepDist @ 11c8:02ec + 5505/0xa/continent gates), else
     * JMP 2228 normal path. */
    return type == 0x1b;
}

int caravan_dist_ok(int dist)
{
    /* Orig 11f0:20b0 CMP dist,0xa / JGE 212b: dist >= 10 skips the bonus
     * message path (falls to 2228 via the continent gate). */
    return dist < 10;
}

int caravan_prod_ok(int prod)
{
    /* Orig 11f0:20a8 CMP 5505,0xe8 / JL 212b (signed): prod byte < -24
     * skips the bonus. 0xe8 as i8 is -24 (wonder/building prod is
     * negative; caravan goods need prod > -24). */
    return prod >= -24;
}

int caravan_foreign_bonus(int cont_home, int cont_dst)
{
    /* Orig 11f0:211b/2126 Map_GetContinentId @ 1110:1d99 x2 + CMP/JNZ:
     * equal continents -> JMP 2228 (skip, same_cont halve in
     * trade_calc_bonus), differ -> 212b bonus message. */
    return cont_home != cont_dst;
}

int city_is_wonder_open(int civ, const int *has_tech, int n,
                         int discover_owner)
{
    /* Orig City_IsWonderOpen @ 1030:7e57: for civ 1..7 if any owns tech
     * (Tech_HasTech @ 1170:3181) return 0; else return
     * (discover[tech] owner == civ). has_tech[c] is the precomputed
     * Tech_HasTech bit per civ (index 0 unused). */
    int c;

    if (has_tech == NULL || n <= 0 || civ < 0 || civ >= n)
        return -1;
    for (c = 1; c < 8 && c < n; c++) {
        if (has_tech[c] != 0)
            return 0;
    }
    return discover_owner == civ ? 1 : 0;
}

int tech_find_first_owner(const int *has_tech, int n,
                           int discover_fallback)
{
    /* Orig Tech_FindFirstOwner @ 1030:7ece: first civ 1..7 with
     * Tech_HasTech set, else the discover-table value. */
    int c;

    if (has_tech == NULL)
        return -1;
    for (c = 1; c < 8 && c < n; c++) {
        if (has_tech[c] != 0)
            return c;
    }
    return discover_fallback;
}

int city_prod_is_unit(int prod)
{
    /* Orig City_ChooseProduction @ 1170:05ca eligible loop local_6 <
     * 0x1c (units 0x00..0x1b, incl. 0x19/0x1a/0x1b specials). */
    return prod >= 0x00 && prod <= 0x1b;
}

int city_prod_is_building(int prod)
{
    /* Orig same function: buildings loop local_6 1..0x18 with
     * local_e8 = -local_6 (negative prod ids). */
    return prod <= -1 && prod >= -0x18;
}

int city_prod_is_wonder(int prod)
{
    /* Orig same function: wonders loop local_6 0x19..0x2e with
     * local_e8 = -local_6. */
    return prod <= -0x19 && prod >= -0x2e;
}

civ_error_t city_choose_production(int civ, int city_idx)
{
    (void)civ;
    (void)city_idx;
    /* TODO: Orig City_ChooseProduction @ 1170:05ca (Pic_PlayShared probe
     * 0xffff + eligible unit/building/wonder lists via Tech_HasTech @
     * 1170:3181 + continent/sea gates via Map_GetContinentId @ 1110:1d99
     * + unit scoring (supply/local_120/continent/sea/defense/tech mults)
     * vs building/wonder fixed table + human dialog Dlg 0x1000 +
     * SENDMESSAGE list + 1170:1f84 pump, 0x66 accept / 0x65 cancel).
     * Needs city records @ 1420:5500 + Rng_Range @ 11c8:003d + Win16
     * dialog. Classifiers above (city_prod_is_unit/building/wonder) +
     * tech gates (tech_has_tech) are LIVE. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t city_tile_yield(int x, int y, int kind, int *out)
{
    (void)x;
    (void)y;
    (void)kind;
    (void)out;
    /* TODO: Orig City_TileYield @ 1030:7c28 (kind 0/1/2 food/shield/trade;
     * Map 1110:168f gate + tile class @ 1110:16c0 + irrigated/road @
     * 1110:1c72 (1420:17d6 vs 0x1836) + occupancy @ 1110:199b trims +
     * owner(3) trade +1 + pollution halve + gov penalties + trade
     * doubling + wonder halve). Needs map tables + gov state. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t city_render_worked_tile(int city_idx, int slot, int draw)
{
    (void)city_idx;
    (void)slot;
    (void)draw;
    /* TODO: Orig City_RenderWorkedTile @ 1030:79ce (city slot claim/draw;
     * draw == 1 centers 1330:4010/400e + paints via 1030:90a8 tile blit +
     * 11a8:15ce yield icons with stride local_e, else Map_SetTile claim
     * x+0x50 + DAT_1420_00d6 food/shield/trade accumulators). Needs GDI
     * + map buffer. */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (2): production shortlist/scoring + TileYield tables
 * + Unit_DoMove dispatch + unit-loop order ---
 * OS-free 1:1 LIVE kernels + Win16 STUBs. No globals, no exit(), C17.
 * See game.h chain block for callers/callees/addrs. */

int city_prod_unit_gate(int unit, int prereq_owned, int obsolete_owned,
                        int is_sea_unit, int coastal_ok, int special_ok)
{
    /* Orig City_ChooseProduction @ 1170:05ca eligible unit loop
     * (local_6 < 0x1c): Tech(0x1a4c) owned + Tech(0x1a38) NOT owned +
     * (5502&2 || domain 0x1a3a != 2); 0x19 needs DAT_3960 != -1 +
     * Tech(0x1b). */
    if (unit < 0x00 || unit > 0x1b)
        return 0;
    if (!prereq_owned)
        return 0;
    if (obsolete_owned)
        return 0;
    if (is_sea_unit && !coastal_ok)
        return 0;
    if (unit == 0x19 && !special_ok)
        return 0;
    return 1;
}

int city_prod_unit_score(int supply, int upkeep, int cost_plus2,
                         int atk_plus_def, int moves_plus1,
                         int sea_mult, int gov_mult)
{
    /* Orig City_ChooseProduction @ 1170:05ca unit score:
     * (supply*upkeep*(1a44+2))/(((1a40)+(1a42))*((1a3c)+1)) then
     * continent-sea-defense-tech x2/x4 branches (callers fold into
     * sea_mult/gov_mult here; exact branch TBD). */
    long base;

    if (atk_plus_def <= 0 || moves_plus1 <= 0)
        return -1;
    base = (long)supply * (long)upkeep * (long)cost_plus2;
    base /= (long)atk_plus_def * (long)moves_plus1;
    base *= (long)sea_mult * (long)gov_mult;
    if (base > 0x7fffffffL)
        return 0x7fffffff;
    if (base < (long)-0x80000000L)
        return -0x80000000;
    return (int)base;
}

int city_prod_building_score(int base_value, int sea_factor, int cost_plus3)
{
    /* Orig City_ChooseProduction @ 1170:05ca building/wonder score:
     * (base*sea*3)/(cost+3) (54fc&2 quarter bonus stays in caller). */
    long v;

    if (cost_plus3 <= 0)
        return -1;
    v = (long)base_value * (long)sea_factor * 3L;
    v /= (long)cost_plus3;
    if (v > 0x7fffffffL)
        return 0x7fffffff;
    if (v < (long)-0x80000000L)
        return -0x80000000;
    return (int)v;
}

civ_error_t city_open_production_view(int city_idx, int prod)
{
    (void)city_idx;
    (void)prod;
    /* TODO: Orig City_OpenProductionView @ 1170:05af (Pic_PlayShared(city,1)
     * modal tail via Dlg_CreateTracked @ 1098:124f 0x94 + tile helpers
     * City_RenderWorkedTile @ 1030:79ce / City_TileYield @ 1030:7c28).
     * Win16 dialog + GDI tiles TBD. */
    return CIV_ERR_UNSUPPORTED;
}

/* Orig 1420:17d6 base (17 tiles x8) + 1420:1836 irrigated (11 tiles x8,
 * tiles 0..10 ocean-inclusive; beyond falls back to base). Stride 8,
 * kind 0/1/2 food/shield/trade at +0/+1/+2; bytes +3..+7 are
 * next-terrain/cost data, not yields. Dumps 2026-10-16 via
 * ghidra_read_memory (civ.exe). */
static const uint8_t s_city_yield_base[17u * 8u] = {
    0x00, 0x01, 0x00, 0x01, 0x0e, 0x00, 0x01, 0x02,
    0x01, 0x01, 0x00, 0x01, 0x06, 0x00, 0x01, 0x02,
    0x02, 0x01, 0x00, 0x01, 0x0a, 0x00, 0x02, 0x03,
    0x01, 0x02, 0x00, 0x02, 0x02, 0x00, 0x02, 0x04,
    0x01, 0x00, 0x00, 0x02, 0x0c, 0x00, 0x03, 0x06,
    0x00, 0x01, 0x00, 0x03, 0x0d, 0x00, 0x01, 0x02,
    0x01, 0x00, 0x00, 0x00, 0x07, 0x00, 0x02, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x02, 0x03,
    0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x02, 0x03,
    0x01, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x01, 0x02,
    0x01, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01, 0x03,
    0x02, 0x01, 0x01, 0x02, 0x09, 0x00, 0x01, 0x02,
    0x03, 0x01, 0x00, 0x01, 0x0e, 0x00, 0x01, 0x02,
    0x01, 0x03, 0x00, 0x01, 0x06, 0x00, 0x01, 0x02,
    0x02, 0x01, 0x00, 0x01, 0x0a, 0x00, 0x02, 0x03,
    0x03, 0x02, 0x00, 0x02, 0x02, 0x00, 0x02, 0x04,
    0x01, 0x02, 0x00, 0x02, 0x0c, 0x00, 0x03, 0x06
};

static const uint8_t s_city_yield_irr[11u * 8u] = {
    0x03, 0x01, 0x00, 0x01, 0x0e, 0x00, 0x01, 0x02,
    0x01, 0x03, 0x00, 0x01, 0x06, 0x00, 0x01, 0x02,
    0x02, 0x01, 0x00, 0x01, 0x0a, 0x00, 0x02, 0x03,
    0x03, 0x02, 0x00, 0x02, 0x02, 0x00, 0x02, 0x04,
    0x01, 0x02, 0x00, 0x02, 0x0c, 0x00, 0x03, 0x06,
    0x00, 0x01, 0x06, 0x03, 0x0d, 0x00, 0x01, 0x02,
    0x03, 0x00, 0x00, 0x00, 0x07, 0x00, 0x02, 0x02,
    0x02, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x02, 0x03,
    0x01, 0x04, 0x00, 0x00, 0x03, 0x00, 0x02, 0x03,
    0x01, 0x00, 0x04, 0x00, 0x0b, 0x00, 0x01, 0x02,
    0x03, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01, 0x03
};

int city_tile_base_yield(int tile, int irrigated, int kind)
{
    /* Orig City_TileYield @ 1030:7c28 base: Map_CheckTileImproved @
     * 1110:1c72 selects 0x1836 else 1420:17d6; yield = table[tile*8+kind].
     * Map_GetTile @ 11e8:0dcf class + Map_CheckTileBounds @ 1110:168f gate
     * stay in caller (OOR tile -> -1 here). */
    size_t off;

    if (kind < 0 || kind > 2)
        return -1;
    if (tile < 0 || tile > 16)
        return -1;
    if (irrigated && tile >= 0 && tile <= 10) {
        off = (size_t)tile * 8u + (size_t)kind;
        return (int)s_city_yield_irr[off];
    }
    off = (size_t)tile * 8u + (size_t)kind;
    return (int)s_city_yield_base[off];
}

int city_tile_yield_adjust(int base, int kind, unsigned occ_flags,
                           int pen0, int pen4, int land_bonus,
                           int trade_bonus, int gov_penalty,
                           int pollution_halve)
{
    /* Orig City_TileYield @ 1030:7c28 modifier chain (1110:199b occ via
     * Map_GetOccupancyFlags @ 1110:199b; tile*0xc pens; owner(3) trade;
     * Tech(3) trade; gov despotism/republic; 0x40 pollution halve):
     * food/shield trims, forest/land trade +1, trade bonuses, gov -1,
     * halve, clamp <1 -> 0. Exact tile*0xc ids + 1250:0000 -2 TBD. */
    int v = base;

    if (kind == 0 && (occ_flags & 2u))
        v += -1 - pen0;
    if (kind == 1 && (occ_flags & 4u))
        v += -1 - pen4;
    if (kind == 2 && (occ_flags & 8u) && land_bonus)
        v += 1;
    v += trade_bonus;
    v -= gov_penalty;
    if (pollution_halve)
        v = (v + 1) / 2;
    if (v < 1)
        v = 0;
    return v;
}

int unit_is_diplomat(int type)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:1e80..21b6 city-entry diplomat path
     * (@ 11f0:1fff via Diplomat_CityAction @ 11b8:0000; frontier type 0x19
     * diplomat vs 0x1a settler / 0x1b caravan / 0x0e caravan? hypothesis). */
    return type == 0x19;
}

int unit_type_is_valid(int type)
{
    /* Orig City_ChooseProduction @ 1170:05ca eligible loops local_6 < 0x1c:
     * full unit range 0x00..0x1b (28 types; 0xff = empty slot). */
    return type >= 0x00 && type <= 0x1b;
}

int unit_dispatch_class(int type)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:1e80..21b6 dispatch (CMP 62ff,0x1a
     * @ 11f0:1ecd JGE settler/city path else JMP 228a normal; CMP 0x1a @
     * 11f0:1f24 JZ settler else JMP 1fff diplomat test; CMP 0x1b @
     * 11f0:2012 JZ caravan branch): 0 settler, 1 caravan, 2 diplomat,
     * 3 normal, -1 invalid/empty. */
    if (type == 0x1a)
        return 0;
    if (type == 0x1b)
        return 1;
    if (type == 0x19)
        return 2;
    if (type >= 0x00 && type <= 0x1b)
        return 3;
    return -1;
}

int unit_order_is_fortify(int order)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x66 case -> Unit_DoFortify
     * (LIVE in game.c; busy 0x08 + moves 0). */
    return order == 0x66;
}

int unit_order_is_sentry(int order)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x68 case -> Unit_DoSentry
     * (LIVE in game.c; 0x09 sentry-wait + moves 0). */
    return order == 0x68;
}

int unit_order_is_found(int order)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x62 case -> Unit_DoFoundCity
     * (PARTIAL in game.c; join consumes settler + city_grow, new-city TBD). */
    return order == 0x62;
}

int unit_order_is_move(int order)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 orders 0x69/0x6d/0x6f/0x70/
     * 0x72/0x75 cases -> Unit_DoMoveAttack variants (PARTIAL stepper +
     * combat odds LIVE, UI pump STUB). 0x6d road? 0x70 pollution? TBD. */
    return order == 0x69 || order == 0x6d || order == 0x6f ||
           order == 0x70 || order == 0x72 || order == 0x75;
}

int city_transfer_treasury(int treas, int amount, int is_winner)
{
    /* Orig Unit_DoMove @ 1038:0000 treasury pair (1420:43b8+0x8c per civ:
     * loser -= iVar7 via Util_Clamp @ 11a0 range, winner += iVar7).
     * Caller passes the clamped amount; negative clamp to 0+. */
    int v;

    if (amount < 0)
        amount = 0;
    v = is_winner ? treas + amount : treas - amount;
    if (v < 0)
        v = 0;
    return v;
}

civ_error_t civ_unit_turn_step(int civ, int unit_idx, int order)
{
    /* Orig Civ_ProcessUnitMoves @ 11f0:0000 loop order (per-civ 0x80 units
     * @ 1420:62fc stride 0xc outer, per-pass do-while inner with bVar5
     * dirty + local_38 wrap + DAT_1310_0002 gate; per unit: visibility
     * Unit_RevealArea @ 11c0:12c8 / update @ 11c0:0395 -> centering
     * Unit_MaybeCenterView @ 11f0:44d4 (Map 1110:1461 when player/seen) ->
     * choose Unit_ChooseOrder @ 1078:1480 / step @ 1128:0000 -> three
     * jump tables -> reveal via unit_reveal_area / dialog via
     * unit_wait_for_orders @ 11c0:0ec2 + city-entry @ 11c0:2c32 +
     * Unit_EnsureOnScreen @ 11c0:231d + Unit_PlayMoveAnimation @ 11c0:2c32).
     * PARTIAL: IDLE (0x00) is a no-op CIV_OK; every other order reaches a
     * Win16/map/city/dialog tail. */
    if (civ < 0 || unit_idx < 0)
        return CIV_ERR_RANGE;
    if (unit_order_is_idle(order))
        return CIV_OK;
    if (unit_order_is_sleep(order) || unit_order_is_fail(order) ||
        unit_order_is_no_path(order) || unit_order_is_fortify(order) ||
        unit_order_is_sentry(order) || unit_order_is_found(order) ||
        unit_order_is_move(order))
        return CIV_ERR_UNSUPPORTED;
    return CIV_ERR_RANGE;
}

/* --- Chain 2026-10-16 (3): production pump + AI + reveal kernels ---
 * OS-free 1:1 LIVE ports + Win16 STUBs. No globals, no exit(), C17.
 * See game.h chain block for callers/callees/addrs. */

int city_pump_result_is_accept(int code)
{
    /* Orig City_ChooseProduction @ 1170:05ca dialog tail loop
     * (do { City_PumpProductionDialog @ 1170:1f84; ... } while
     * (local_2e != 0x66 && local_2e != 0x65)): 0x66 accepts (returns
     * local_e8[0] current prod). */
    return code == 0x66;
}

int city_pump_result_is_cancel(int code)
{
    /* Same loop: 0x65 cancels (returns the newly picked iVar4). */
    return code == 0x65;
}

int city_production_best_pick(const int *scores, int n)
{
    /* Orig City_ChooseProduction @ 1170:05ca minima (local_c/local_28/
     * local_26 over local_e8): strict-< argmin, ties to lower index.
     * The advisor pair (local_24 best unit, local_22 best building/wonder)
     * and overall best (local_10/local_c) share this shape; the human
     * dialog shows them as advisor text, the AI takes the minimum. */
    int i;
    int best;
    int best_v;

    if (scores == NULL || n <= 0)
        return -1;
    best = 0;
    best_v = scores[0];
    for (i = 1; i < n; i++) {
        if (scores[i] < best_v) {
            best = i;
            best_v = scores[i];
        }
    }
    return best;
}

civ_error_t city_pump_production_dialog(int *result_out)
{
    /* TODO: Orig City_PumpProductionDialog @ 1170:1f84 (MAKEPROCINSTANCE +
     * SETWINDOWLONG hook + PEEKMESSAGE/ISDIALOGMESSAGE loop on DAT_12d8_0030
     * + restore + FREEPROCINSTANCE; *param_2 = DAT_12d8_0030). Needs Win16
     * message state + Dlg_CreateTracked @ 1098:124f dialog. Codes via
     * city_pump_result_is_accept/cancel above (0x66/0x65). */
    if (result_out == NULL)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

int unit_choose_delegates_to_player(int civ)
{
    /* Orig Unit_ChooseOrder @ 1078:1480 head (if (param_1 == 0) return
     * Unit_AIPlayerTurn(0, param_2)): civ 0 is the human-civ order path
     * via Unit_AIPlayerTurn @ 1078:4f0f. */
    return civ == 0;
}

int unit_needs_sentry_idle(int type, int timer)
{
    /* Orig Unit_ChooseOrder @ 1078:1480 head (type 0x0f with 6301 timer
     * < 1 -> 0x68 sentry/1-move). */
    return type == 0x0f && timer < 1;
}

int unit_diplomat_needs_goal(int type, int goal)
{
    /* Orig Unit_ChooseOrder @ 1078:1480 head (type 0x0e with 6302 goal
     * == -1 hunts the nearest visible player diplomat via
     * unit_find_diplomat_target @ 11c8:02ec or a staging city, else
     * 0x20). Goal 0xff reads as -1 signed. */
    return type == 0x0e && (goal == 0xff || goal == -1);
}

int unit_ai_best_dir(const int *scores, int n)
{
    /* Orig Unit_AIPlayerTurn @ 1078:4f0f + Unit_StepTowardGoal @ 1128:0000
     * neighbour best (strict-> argmax over dirs 1..8; ties to lower dir).
     * n covers the scored dirs (orig loops local_14 1..8). */
    int i;
    int best;
    int best_v;

    if (scores == NULL || n <= 0)
        return -1;
    best = 0;
    best_v = scores[0];
    for (i = 1; i < n; i++) {
        if (scores[i] > best_v) {
            best = i;
            best_v = scores[i];
        }
    }
    return best;
}

int unit_ai_reverse_penalty(int move_dir, int last_dir)
{
    /* Orig Unit_StepTowardGoal @ 1128:0000 anti-reverse block (6304
     * last-dir): dist = |move-last| folded (>4 -> 8-dist), penalty =
     * dist*dist. 0xff init / 0 / 0xffff never penalise (orig
     * *(6304) != -1 gate); OOR dirs return 0. */
    int d;

    if (move_dir < 1 || move_dir > 8)
        return 0;
    if (last_dir == 0xff || last_dir == 0 || last_dir == 0xffff ||
        last_dir == -1)
        return 0;
    if (last_dir < 1 || last_dir > 8)
        return 0;
    d = move_dir - last_dir;
    if (d < 0)
        d = -d;
    if (d > 4)
        d = 8 - d;
    return d * d;
}

int unit_seen_index(int x, int y)
{
    /* Orig Unit_UpdateVisibility @ 11c0:0395 seen-byte (x*0x32+y+10, the
     * fog byte ORed with 1<<civ; City_DoRevolt @ 11b8:0f26 flips the same
     * bytes on revolt). 0x32 = 0x50 game-map width. */
    return x * 0x32 + y + 10;
}

int unit_seen_bit(int civ)
{
    /* Orig same function (1 << (civ & 0x1f) ORed into the seen byte).
     * Linux guards 0..31 (civ 0..5 game + spare bits); OOR -> -1. */
    if (civ < 0 || civ > 31)
        return -1;
    return 1 << civ;
}

int unit_reveal_minimap_value(int tile)
{
    /* Orig Unit_UpdateVisibility @ 11c0:0395 redraw tail
     * (Map_SetTile(x+0xf0, y, tile == 1 ? 1 : 2) via the 1210:09b4 minimap
     * repaint from Map_RefreshAllViews @ 1110:03e9). */
    return tile == 1 ? 1 : 2;
}

int unit_sight_far_allowed(int range_flags, int domain, int tile_is_sea)
{
    /* Orig Unit_UpdateVisibility @ 11c0:0395 far-ring gate ((unit 0x1a46 &
     * 2) != 0 && (domain 0x1a3a != 2 || Map_GetTileClass @ 1110:16c0 ==
     * 10 sea)). Player diamond far tiles 9.. and the AI 0x10 far ring
     * share this predicate. Nonzero = allowed. */
    return ((range_flags & 2) != 0) && (domain != 2 || tile_is_sea != 0);
}

int unit_sight_count(int is_player, int range_flags, int domain,
                     int tile_is_sea)
{
    /* Orig same function: player scans the 0x19-tile diamond (offsets via
     * Map_WrapX @ 1128:177e); others scan 8 near + 0x10 far when the gate
     * above holds, else 8 near only. */
    if (is_player)
        return 0x19;
    return unit_sight_far_allowed(range_flags, domain, tile_is_sea) ? 0x18
                                                                    : 8;
}

int unit_find_free_slot(const int *types, int n)
{
    /* Orig Unit_SpawnUnit @ 11c0:12c8 head (scan 0x80 1420:62ff slots for
     * type -1; full (past 0x7e) shows the FIRSTUNIT message and returns
     * -1). Types use -1/0xff empty (see unit_type_name). */
    int i;

    if (types == NULL || n <= 0)
        return -1;
    for (i = 0; i < n; i++) {
        if (types[i] == -1 || types[i] == 0xff)
            return i;
    }
    return -1;
}

int city_find_by_tile(const int *xs, const int *ys, const int *valids, int n,
                      int x, int y)
{
    /* Orig City_FindByTile @ 11c8:00c4 (scan 0x80 0x1c city records for
     * valid != -1 && x && y match; -1 when none). Backs City_SpotReveal @
     * 11c0:0000 + City_FindNearest @ 11c8:0124 callers in the 0395 tail. */
    int i;

    if (xs == NULL || ys == NULL || valids == NULL || n <= 0)
        return -1;
    for (i = 0; i < n; i++) {
        if (valids[i] != -1 && xs[i] == x && ys[i] == y)
            return i;
    }
    return -1;
}

int city_find_nearest(int ux, int uy, const int *xs, const int *ys,
                      const int *valids, int n, int *dist_out)
{
    /* Orig City_FindNearest @ 11c8:0124 (argmin Map_WrapStepDist @
     * 11c8:02ec over valid? NO — orig scans all 0x80 slots with valid !=
     * -1; DAT_1420_b386 = winning dist). Caller arrays are explicit here;
     * xs/ys/valids parallel (valids[i] != -1 required). dist_out may be
     * NULL. Returns city index, -1 when none/NULL/n <= 0. */
    int i;
    int best = -1;
    int best_d = 0;

    if (xs == NULL || ys == NULL || n <= 0)
        return -1;
    for (i = 0; i < n; i++) {
        int d;

        if (valids != NULL && valids[i] == -1)
            continue;
        d = map_wrap_step_dist(ux, xs[i], uy, ys[i]);
        if (best < 0 || d < best_d) {
            best = i;
            best_d = d;
        }
    }
    if (dist_out != NULL)
        *dist_out = best < 0 ? -1 : best_d;
    return best;
}

/* --- Chain 2026-10-16 (4): combat/ZOC/spawn/queue/visibility ---
 * OS-free 1:1 LIVE ports. No globals, no exit(), C17. See game.h chain
 * block for callers/callees/addrs + TODO tails. */

int combat_effective_attack(int base, int veteran, int moves_left)
{
    /* Orig Combat_ResolveAttack @ 1050:0000 atk kernel: local_10 =
     * 1a40[type]<<3; vet 0x20 -> += /2; moves<3 -> *moves/3. Difficulty
     * (civ==0 vs player) + city 3efe==1 -> 0 stay in caller. */
    int atk;

    if (base < 0 || moves_left < 0)
        return -1;
    atk = base << 3;
    if (veteran)
        atk += atk / 2;
    if (moves_left < 3)
        atk = atk * moves_left / 3;
    return atk;
}

int combat_effective_defense(int base, int terrain_mult, int fortified,
                             int city_mult, int flat, int veteran)
{
    /* Orig Combat_ResolveAttack @ 1050:0000 def kernel: air/sea (1a3a 1/2)
     * -> base<<3; else base*4*terrain (17d5) with fortified *3/2, city *2,
     * walls *3; vet 0x20 -> += /2. city_mult 1/2/3 picked by caller. */
    int def;

    if (base < 0 || terrain_mult < 0 || city_mult < 1)
        return -1;
    if (flat != 0 && flat != 1)
        return -1;
    if (fortified != 0 && fortified != 1)
        return -1;
    if (veteran != 0 && veteran != 1)
        return -1;
    if (flat) {
        def = base << 3;
    } else {
        def = base * 4 * terrain_mult;
        if (fortified)
            def = def * 3 / 2;
        def *= city_mult;
    }
    if (veteran)
        def += def / 2;
    return def;
}

int combat_preview_score(int atk_eff, int def_eff)
{
    /* Orig Combat_ResolveAttack @ 1050:0000 preview (param_5 == 0):
     * (local_10<<3)/(local_12+1). Rolls (Rng_Range @ 11c8:003d pair +
     * city-holder reroll) live in unit_attack_resolve (odds + roll). */
    if (atk_eff < 0 || def_eff < 0)
        return -1;
    return (atk_eff << 3) / (def_eff + 1);
}

int unit_count_stack(const uint8_t *next, int n, int start)
{
    /* Orig Unit_IterateStack @ 11c0:1ee1 count shape: follow 0x6306 until
     * 0xff, loop-back to start, OOR, or >9 extra steps (local_8 > 9 cap).
     * Callback (param_3: Requires tally in CalcOdds/CountDefenders) stays
     * in caller; this is the pure length. */
    int count = 0;
    int cur = start;
    int steps = 0;

    if (!next || n <= 0 || start < 0 || start >= n)
        return -1;
    for (;;) {
        int nxt;

        if (cur < 0 || cur >= n)
            break;
        count++;
        nxt = next[cur];
        if (nxt == 0xff || nxt == -1)
            break;
        if (nxt < 0 || nxt >= n)
            break;
        if (nxt == start)
            break;
        cur = nxt;
        steps++;
        if (steps > 9)
            break;
    }
    return count;
}

civ_error_t unit_stack_insert(uint8_t *next, int n, int head, int fresh)
{
    /* Orig Unit_LinkStack @ 1050:1105: link fresh after head
     * (next[fresh] = next[head], next[head] = fresh; 0xff tail kept).
     * Exact civ/new/head order TBD vs decompiler; explicit chain here. */
    if (!next)
        return CIV_ERR_NOENT;
    if (n <= 0 || head < 0 || head >= n || fresh < 0 || fresh >= n)
        return CIV_ERR_RANGE;
    if (head == fresh)
        return CIV_ERR_RANGE;
    next[fresh] = next[head];
    next[head] = (uint8_t)fresh;
    return CIV_OK;
}

int unit_find_nearest_at_tile(const int *xs, const int *ys,
                               const int *valids, int n, int exclude,
                               int x, int y, int *dist_out)
{
    /* Orig Unit_FindAtTile @ 11c8:01ab: 0x80 62ff active (type != -1),
     * skip exclude, argmin Map_WrapStepDist @ 11c8:02ec, DAT_1420_b386 =
     * winning dist. valids[i] != -1 required (NULL = all live). Ties to
     * lower index (strict-< loop). */
    int i;
    int best = -1;
    int best_d = 0;

    if (!xs || !ys || n <= 0)
        return -1;
    for (i = 0; i < n; i++) {
        int d;

        if (valids && valids[i] == -1)
            continue;
        if (i == exclude)
            continue;
        d = map_wrap_step_dist(x, xs[i], y, ys[i]);
        if (best < 0 || d < best_d) {
            best = i;
            best_d = d;
        }
    }
    if (dist_out)
        *dist_out = best < 0 ? -1 : best_d;
    return best;
}

int unit_spawn_unit(uint8_t *units, int n_units, int type, int x, int y,
                    int home, const uint8_t *timer_table, int timer_n,
                    int seen_bit)
{
    /* Orig Unit_SpawnUnit @ 11c0:12c8 alloc/init (factory, NOT a reveal
     * wrapper): scan 0x80 for type 0xff else -1 (FIRSTUNIT message tail
     * Win16); x/y/type/seen/goal 0xff/timer 0x1a3e/home via City_FindNearest
     * @ 11c8:0124; Map 1110:171f/1767 place + Unit_UpdateVisibility @
     * 11c0:0395 tails stay in caller (map.h tile markers LIVE there). */
    int i;
    int slot = -1;
    uint8_t *rec;
    int timer = 0;

    if (!units || n_units <= 0)
        return -1;
    if (type < 0 || x < 0 || y < 0)
        return -1;
    for (i = 0; i < n_units; i++) {
        if (units[(size_t)i * CIV_UNIT_REC_LEN + CIV_UNIT_OFF_TYPE] == 0xff) {
            slot = i;
            break;
        }
    }
    if (slot < 0)
        return -1;
    if (timer_table && type < timer_n && type >= 0)
        timer = timer_table[type];
    if (timer != 0)
        timer--;
    rec = units + (size_t)slot * CIV_UNIT_REC_LEN;
    rec[CIV_UNIT_OFF_X] = (uint8_t)x;
    rec[CIV_UNIT_OFF_Y] = (uint8_t)y;
    rec[CIV_UNIT_OFF_TYPE] = (uint8_t)type;
    rec[CIV_UNIT_OFF_MOVES] = 0;
    rec[CIV_UNIT_OFF_TIMER] = (uint8_t)timer;
    rec[CIV_UNIT_OFF_GOAL_X] = 0xff;
    rec[CIV_UNIT_OFF_GOAL_Y] = 0xff;
    rec[CIV_UNIT_OFF_LAST_DIR] = 0xff;
    rec[CIV_UNIT_OFF_SEEN] = (uint8_t)(seen_bit & 0xff);
    rec[CIV_UNIT_OFF_NEXT] = 0xff;
    rec[CIV_UNIT_OFF_HOME] = (uint8_t)(home & 0xff);
    return slot;
}

int unit_queue_is_duplicate(const uint8_t *qx, const uint8_t *qy,
                             const uint8_t *qk, const uint8_t *qp, int n,
                             int x, int y, int kind, int prio)
{
    /* Orig Unit_QueueOrder @ 1078:455c dup gate: same x/y/kind with stored
     * prio <= new -> drop (return before the AI intercept + slot pick).
     * Kinds 1..4 contact orders (exact semantics TBD). */
    int i;

    if (!qx || !qy || !qk || !qp || n <= 0)
        return 0;
    for (i = 0; i < n; i++) {
        if (qx[i] == (uint8_t)x && qy[i] == (uint8_t)y &&
            qk[i] == (uint8_t)kind && qp[i] <= (uint8_t)prio)
            return 1;
    }
    return 0;
}

int unit_queue_pick_slot(const int8_t *kinds, const int8_t *prios, int n,
                         int prio)
{
    /* Orig Unit_QueueOrder @ 1078:455c slot pick: first kind == -1 (0xff
     * empty) or prio < new (lower evicted after Unit_QueueShift @
     * 1078:4837). Full 0x20 -> caller drops (return, no write). */
    int i;

    if (!kinds || !prios || n <= 0)
        return -1;
    for (i = 0; i < n; i++) {
        if (kinds[i] == -1 || prios[i] < (int8_t)prio)
            return i;
    }
    return -1;
}

civ_error_t unit_queue_shift(uint8_t *qx, uint8_t *qy, uint8_t *qk,
                             uint8_t *qp, int n, int at)
{
    /* Orig Unit_QueueShift @ 1078:4837: rows [at..n-2] -> [at+1..n-1] when
     * qp[row] != 0 (orig loops 0x1e..at down, copying 0x612/0x512/0x412/
     * 0x312 bytes +1). Frees [at] for the new x/y/kind/prio store. */
    int r;

    if (!qx || !qy || !qk || !qp)
        return CIV_ERR_NOENT;
    if (n <= 0 || at < 0 || at >= n)
        return CIV_ERR_RANGE;
    for (r = n - 2; r >= at; r--) {
        if (qp[r] == 0)
            continue;
        qx[r + 1] = qx[r];
        qy[r + 1] = qy[r];
        qk[r + 1] = qk[r];
        qp[r + 1] = qp[r];
    }
    return CIV_OK;
}

/* --- Chain 2026-10-16 (5): combat rolls/promotion/difficulty + stack walk/
 * defender pick + draw-layer pick + adjacent tally (task goals 1/2/4/5).
 * OS-free 1:1 LIVE ports. No globals, no exit(), C17. Caller of
 * Combat_ResolveAttack @ 1050:0000 is Unit_ChooseOrder @ 1078:1480 (sole
 * xref); rolls via rng_range @ 11c8:003d in sys.c (callers pass draws). */

int combat_roll_winner(int atk_eff, int def_eff, unsigned atk_roll,
                       unsigned def_roll, unsigned atk_roll2,
                       unsigned def_roll2, int city_holder)
{
    /* Orig Combat_ResolveAttack @ 1050:0000: iVar5 = Rng_Range(local_10),
     * iVar6 = Rng_Range(local_12); local_1e = 0x10 when iVar6 < iVar5
     * (attacker wins); city-holder (local_22, palace 54fc&1 + 3efe path)
     * with attacker win rerolls once, defender wins when iVar5 < iVar6 on
     * the second pair. Caller passes Rng_Range draws (rng_range in sys.c);
     * eff values only validate (< 0 bad; orig used non-negative game vals). */
    int atk_win;

    if (atk_eff < 0 || def_eff < 0)
        return -1;
    atk_win = (def_roll < atk_roll) ? 1 : 0;
    if (city_holder != 0 && atk_win)
        atk_win = (atk_roll2 < def_roll2) ? 0 : 1;
    return atk_win;
}

int combat_should_promote(unsigned rand2)
{
    /* Orig both outcome tails: iVar5 = Rng_Range(2); if != 0 unit |= 0x20. */
    return rand2 != 0u ? 1 : 0;
}

int combat_difficulty_scale(int atk, int atk_civ, int def_civ, int player_civ,
                            int difficulty, int real_fight)
{
    /* Orig two sites in Combat_ResolveAttack @ 1050:0000. Setup site
     * (param_1 == 0, real_fight == 0): player defends -> atk*(diff+1)/4,
     * else atk/2 (vs-player handicap / AI-vs-AI halve). Real-fight site
     * (param_1 != 0, real_fight != 0): diff < 2 + player == def -> /2;
     * diff == 0 + player == atk -> *2 (Chieftain player bonus). */
    if (atk < 0 || difficulty < 0 || difficulty > 4)
        return -1;
    if (real_fight == 0) {
        if (atk_civ != 0)
            return atk;
        if (player_civ == def_civ)
            return atk * (difficulty + 1) / 4;
        return atk / 2;
    }
    if (atk_civ == 0)
        return atk;
    if (difficulty < 2 && player_civ == def_civ)
        atk /= 2;
    if (difficulty == 0 && player_civ == atk_civ)
        atk *= 2;
    return atk;
}

int unit_iterate_stack(const uint8_t *next, int n, int start,
                       unit_stack_visit_fn cb, void *ctx)
{
    /* Orig Unit_IterateStack @ 11c0:1ee1: iVar1 = start; loop { iVar1 =
     * next[iVar1]; cb(); if -1 return; if == start return; local_8++ > 9
     * return; }. Note the callback runs for the FOLLOW node (first visit
     * is next[start], not start itself); this port visits start first like
     * unit_count_stack (pure length twin) so cb sees every node incl. start.
     * Loop-back/OOR/>9-extra-steps stop like the count twin. */
    int cur = start;
    int steps = 0;
    int visits = 0;

    if (!next || n <= 0 || start < 0 || start >= n)
        return -1;
    for (;;) {
        int nxt;

        if (cur < 0 || cur >= n)
            break;
        if (cb) {
            int stop = cb(cur, ctx);

            if (stop != 0)
                break;
        }
        visits++;
        nxt = next[cur];
        if (nxt == 0xff || nxt == -1)
            break;
        if (nxt < 0 || nxt >= n)
            break;
        if (nxt == start)
            break;
        cur = nxt;
        steps++;
        if (steps > 9)
            break;
    }
    return visits;
}

int unit_stack_defense_score(int base_def, int terrain_mult, int fortified,
                             int veteran, int tile_is_sea,
                             int domain_is_sea)
{
    /* Orig Unit_ScoreStackDefender @ 11c0:1a81: sea tile (GetTileClass @
     * 1110:16c0 == 10) skips unless domain @ 1a3a == 2; domain 0 (land):
     * def*8*terrain*(fort 0x08 ? 3 : 2) [orig *(2|3) folded]; sea/air:
     * def<<4; vet 0x20 -> += /2. base = 1a42[type], terrain = 17d5 entry. */
    int score;

    if (base_def < 0 || terrain_mult < 0)
        return -1;
    if ((fortified != 0 && fortified != 1) ||
        (veteran != 0 && veteran != 1))
        return -1;
    if (tile_is_sea != 0 && domain_is_sea == 0)
        return -1;
    if (domain_is_sea == 0)
        score = base_def * 8 * terrain_mult * (fortified ? 3 : 2);
    else
        score = base_def << 4;
    if (veteran)
        score += score / 2;
    return score;
}

int unit_stack_best(const int *scores, int n)
{
    /* Orig Unit_PickStackTop @ 11c0:1a2c seed b37e = 0xffff + strict < in
     * Unit_ScoreStackDefender: negatives never beat the seed -> -1 when
     * all negative. Ties break to the lower index. */
    int i;
    int best = -1;
    int best_s = 0;

    if (!scores || n <= 0)
        return -1;
    for (i = 0; i < n; i++) {
        if (scores[i] < 0)
            continue;
        if (best < 0 || scores[i] > best_s) {
            best = i;
            best_s = scores[i];
        }
    }
    return best;
}

int unit_stack_draw_pick(int tile_is_sea, int head_domain_is_sea,
                         const int *domains, int n, int head, int has_stack)
{
    /* Orig Unit_DrawStacked @ 1110:1516: land/single -> PickStackTop (head
     * here; caller scores via unit_stack_best first); sea + stack + head
     * sea-domain -> PickStackTop; sea + stack + head land -> first sea node
     * in chain order, else head. domains[] is chain order head-first. */
    int i;

    if (!domains || n <= 0 || head < 0 || head >= n)
        return -1;
    if (tile_is_sea == 0 || has_stack == 0)
        return head;
    if (head_domain_is_sea != 0)
        return head;
    for (i = 0; i < n; i++) {
        if (domains[i] != 0)
            return i;
    }
    return head;
}

int unit_draw_single_layer(int clipped, int tile_is_sea, int occupant_flag,
                           int domain_is_sea, int has_stack, int *badge_out)
{
    /* Orig Unit_DrawSingle @ 1110:0dba: Gfx_ClipTileRect @ 1208:2061 == 0
     * (off-screen) -> 0; sea + (62fc & 1, embarked/occupant) + domain != 2
     * -> 0 hidden; else stacked (next 0x6306 != -1) badge via
     * Gfx_DrawTechIcon @ 11a8:0cb1 + Unit_DrawStatusBadge @ 1110:0ff6. */
    if (clipped != 0)
        return 0;
    if (tile_is_sea != 0 && occupant_flag != 0 && domain_is_sea == 0)
        return 0;
    if (badge_out)
        *badge_out = (has_stack != 0) ? 1 : 0;
    return 1;
}

int combat_adjacent_add(int score, int odds)
{
    /* Orig Combat_ScoreAdjacent @ 1050:13af: *piVar1 += Combat_CalcOdds(...)
     * into the 396a power table per adjacent enemy stack (threat assess for
     * the Unit_ChooseOrder @ 1078:1480 diplomat 0x19 bribe-target scan). */
    if (odds < 0)
        odds = 0;
    return score + odds;
}

int combat_adjacent_diplo_flag(int owner, int atk)
{
    /* Orig same loop tail: owner != atk -> diplo |= 0x88 (contact bits @
     * 3a0a marking the adjacent civs as contacted/hostile). */
    return owner != atk ? 0x88 : 0x00;
}

int diplo_nearest_palace_dist(int civ, const int *city_xs,
                               const int *city_ys, const int *city_owners,
                               const int *city_palace, int n_cities, int x,
                               int y)
{
    /* Orig Diplo_NearestPalaceDist @ 1168:12c1: seed 0x10, min
     * Map_WrapStepDist @ 11c8:02ec over cities with valid (caller filters
     * 5502 != -1) + owner == civ + palace (54fc & 1). Feeds the revolt
     * (game.h diplomat_revolt_cost dist) + bribe (diplomat_bribe_cost dist)
     * price rules. NULL arrays / n <= 0 -> seed (no fake). */
    int i;
    int best = 0x10;

    if (!city_xs || !city_ys || !city_owners || !city_palace || n_cities <= 0)
        return 0x10;
    for (i = 0; i < n_cities; i++) {
        int d;

        if (city_owners[i] != civ)
            continue;
        if (city_palace[i] == 0)
            continue;
        d = map_wrap_step_dist(city_xs[i], x, city_ys[i], y);
        if (d < best)
            best = d;
    }
    return best;
}

/* --- Chain 2026-10-16 (6): peace-block gate + stacked sea-search (goals 4/5).
 * OS-free 1:1 LIVE, no globals. */

int combat_check_peace_block(int peace_flags, int gov, int choice)
{
    /* Orig Combat_CheckPeaceBlock @ 1050:12be: (player-vs-target @ 12d8:3a0a
     * & 2) == 0 -> 0 free; else Dialog_ShowMessage @ 1220:00d2 (0xa24/0xa4c)
     * choice == 1 + government @ 3b77+0x13 < 4 -> 1 senate block, else second
     * message (0xa6b) + 0xffff allowed-after-warning. Dialog tails stay
     * caller-side; this is the pure gate. */
    if ((peace_flags & 0x02) == 0)
        return 0;
    if (choice == 1 && gov < 4)
        return 1;
    return 0xffff;
}

int unit_stack_draw_needs_sea_search(int tile_is_sea, int has_stack,
                                     int head_domain_is_sea)
{
    /* Orig Unit_DrawStacked @ 1110:1516: sea tile + stack + land head ->
     * scan the 0x6306 chain for the first domain-2 (1a3a == 2) node before
     * Unit_DrawSingle @ 1110:0dba; else Unit_PickStackTop @ 11c0:1a2c
     * scoring (unit_stack_best) straight. Layer order: terrain (map) ->
     * city underlay -> unit icon (Gfx_DrawTechIcon @ 11a8:0cb1) -> badge
     * (Unit_DrawStatusBadge @ 1110:0ff6). */
    return (tile_is_sea != 0 && has_stack != 0 && head_domain_is_sea == 0) ? 1 : 0;
}

/* --- Chain 2026-10-16 (8): census counting kernels (task goal 1) ---
 * OS-free 1:1 LIVE + Win16 STUBs. No globals, no exit(), C17. See game.h
 * chain block for callers/callees/addrs + row formats. */

civ_error_t city_open_census_view(int title_id, int civ)
{
    (void)title_id;
    (void)civ;
    /* TODO: Orig City_OpenCensusView @ 1018:0000 (SETRECT 0x140x0x200 +
     * DAT_12d8_0026/002c/002e view state + Dlg_CreateTracked @ 1098:124f
     * dialog 0x94 + Text_SetColor @ 1098:0e0b + FILLRECT + GfxPort_Create
     * @ 10d0:0000 + GfxPort_SetActive @ 10d0:0956 + Cursor_Set 0x84 +
     * City_DrawCensusHeader @ 1018:015d). Win16 dialog/GDI -> SDL TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t city_close_census_view(void)
{
    /* TODO: Orig City_CloseCensusView @ 1018:0380 (SETRECT 0x140x0x200 +
     * GfxPort_PresentRect @ 10d0:0854 + Sys_WaitForInput + FUN_1100_0be5
     * + GfxPort_ClearActive @ 10d0:09a2 + Dlg_DestroyTracked). Win16. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t city_draw_census_header(int title_id, int civ)
{
    (void)title_id;
    (void)civ;
    /* TODO: Orig City_DrawCensusHeader @ 1018:015d (was FUN_1018_015d:
     * GfxPort_SetActive + fill + title TEXTOUT at 0xa0,2 + civ adjective
     * 0x2da/0x1da + gov 0x132/0x139/0x141 + year via Status_AppendYearText
     * @ 1088:1dd0 at 0xa0,10/0x12; DAT_12d8_28fc mono gate forces color
     * 0). Win16 GDI text -> SDL TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t census_draw_tech_badge(int icon, int x, int y)
{
    (void)icon;
    (void)x;
    (void)y;
    /* TODO: Orig Census_DrawTechBadge @ 1018:24c5 (was FUN_1018_24c5:
     * port select 0x48x0x50 + fill 0x60x0x35 OFFSETRECT + Text_SetColor
     * 44fc + Gfx_DrawTechIcon(*(icon*2+0x128)) badge). Win16 GDI -> SDL. */
    return CIV_ERR_UNSUPPORTED;
}

int census_tech_target(int difficulty, int base, int extra)
{
    /* Orig Census_ShowTechBars @ 1018:03f0: local_14 = diff*2 + base + 6
     * (base is DAT_12a8_0020); if (local_14 < 0xb - extra) local_14 =
     * 0xb - extra (extra is the 3f5c+2+civ*2 word). */
    int t = difficulty * 2 + base + 6;

    if (t < 0xb - extra)
        t = 0xb - extra;
    return t;
}

int census_bar_segment(int val)
{
    /* Orig same: Util_Clamp @ 11c8:0063(0x980/(val+1), 1, 0x40). Guard
     * val < 0 as 0 (orig callers pass val >= 0: local_14 * extra *
     * year-factor). */
    int v = val;

    if (v < 0)
        v = 0;
    return util_clamp(0x980 / (v + 1), 1, 0x40);
}

int census_bar_width(int seg, int val)
{
    /* Orig same: Util_Clamp(seg*val/8+8, 8, 0x130). */
    return util_clamp(seg * val / 8 + 8, 8, 0x130);
}

int census_bar_origin(int width)
{
    /* Orig same: 0xa0 - w/2 (x86 IDIV trunc toward zero). */
    return 0xa0 - width / 2;
}

int census_bar_count(int raw, int val)
{
    /* Orig same: Util_Clamp(raw, 0, val) loop bound (raw is the
     * 43b8+0x7c+civ*2 word, val the iVar2 product). */
    return util_clamp(raw, 0, val);
}

civ_error_t census_tech_cell(int idx, int *x_out, int *y_out)
{
    /* Orig Census_ShowTechBars @ 1018:03f0 grid: x = (idx%3)*100+8,
     * y = 0x30+(idx/3)*7 (idx is local_10 drawn ordinal); caller breaks
     * when 0xc0 < y. */
    if (!x_out || !y_out)
        return CIV_ERR_NOENT;
    if (idx < 0)
        return CIV_ERR_RANGE;
    *x_out = (idx % 3) * 100 + 8;
    *y_out = 0x30 + (idx / 3) * 7;
    return CIV_OK;
}

int census_tech_owner_color(int entry, int civ)
{
    /* Orig same: ((entry & 7) == civ) ? 0xf : 0xb (entry is the
     * 2afa+6+tech*2 word). */
    return ((entry & 7) == civ) ? 0xf : 0xb;
}

int census_unit_row_y(int row)
{
    /* Orig Census_ShowUnitList @ 1018:067b: iVar4 starts 0x20, +9 per
     * drawn row. */
    return 0x20 + row * 9;
}

int census_unit_flag_x(int row)
{
    /* Orig same + Census_ShowTerrainCounts @ 1018:09cc: (drawn&1)*0x16+2
     * at y-5 (local_6 & 1 alternates the flag column). */
    return (row & 1) * 0x16 + 2;
}

int census_unit_has_row(int a, int b)
{
    /* Orig Census_ShowUnitList @ 1018:067b gate: 0 < a (4121+0xd count)
     * || 0 < b (3f5c+0x12 count) draws the 0x1c row. */
    return (a > 0 || b > 0) ? 1 : 0;
}

int census_terrain_row_y(int row)
{
    /* Orig Census_ShowTerrainCounts @ 1018:09cc: local_8 starts 0x1c, +7
     * per drawn terrain (0x1b loop, any-civ nonzero at 2b90 gate). */
    return 0x1c + row * 7;
}

int census_terrain_cell_x(int ord)
{
    /* Orig same: self count at 0x68, each other civ +0x1c in civ order
     * skipping self (ord is the other-civ ordinal). */
    return 0x68 + ord * 0x1c;
}

int census_report_row_y(int idx)
{
    /* Orig City_ShowCensusReport @ 1018:1d5b: iVar5 starts 0x20, +10 per
     * drawn city (0x80 loop, owner == civ && valid != -1). */
    return 0x20 + idx * 10;
}

int census_report_needs_page(int y)
{
    /* Orig same: if (0xbe < y) present + wait + City_DrawCensusHeader @
     * 1018:015d + reset y = 0x20. */
    return y > 0xbe ? 1 : 0;
}

int census_report_next_x(int n)
{
    /* Orig same: local_6 starts 0xd6, +0x13 per shown building icon
     * ((mask & 0x2618) != 0 over 0x18 bits, mask is 54fc word). */
    return 0xd6 + n * 0x13;
}

int census_building_icon_shown(int bit)
{
    /* Orig same per-bit gate: (mask & 0x2618) != 0 with mask = 1u<<bit
     * (caller passes the 54fc word bit). bit OOR -> 0. */
    unsigned m;

    if (bit < 0 || bit >= 32)
        return 0;
    m = 1u << ((unsigned)bit & 31u);
    return (m & 0x2618u) != 0u ? 1 : 0;
}

int census_empty_guard(int total)
{
    /* Orig City_ShowCensusReport @ 1018:1d5b tail: if (local_10 == 0)
     * local_10 = 1 (total size sum; avoids /0 in the three
     * City_FormatCensusRatio @ 1018:202a calls). */
    return total == 0 ? 1 : total;
}

/* --- Chain 2026-10-16 (9): census header format (goal 1). OS-free 1:1
 * LIVE. No globals, no exit(), C17. See game.h chain block. */

int city_census_gov_title_id(int gov_half)
{
    /* Orig City_DrawCensusHeader @ 1018:015d: iVar4 = gov_word/2; 0 ->
     * 0x132, 1 -> 0x139, 2 -> 0x141, else no title (falls to Appends).
     * Returns Res id or -1 when no gov title. */
    if (gov_half == 0)
        return 0x132;
    if (gov_half == 1)
        return 0x139;
    if (gov_half == 2)
        return 0x141;
    return -1;
}

int city_census_header_civ(int mono, int civ)
{
    /* Orig same head: if (DAT_12d8_28fc != 0) param_3 = 0 (mono forces
     * color 0 for the 0x140x0x200 FILLRECT civ brush). */
    return (mono != 0) ? 0 : civ;
}

civ_error_t city_census_header_pos(int line, int *x_out, int *y_out)
{
    /* Orig same TEXTOUT positions: line 0 title via FUN_10d8_046e at
     * 0xa0,2; line 1 civ adjective+gov at 0xa0,10; line 2 year via
     * Status_AppendYearText @ 1088:1dd0 at 0xa0,0x12 (all color 0xf via
     * FUN_10d8_0528 except the title path). */
    if (!x_out || !y_out)
        return CIV_ERR_NOENT;
    if (line == 0) {
        *x_out = 0xa0;
        *y_out = 2;
        return CIV_OK;
    }
    if (line == 1) {
        *x_out = 0xa0;
        *y_out = 10;
        return CIV_OK;
    }
    if (line == 2) {
        *x_out = 0xa0;
        *y_out = 0x12;
        return CIV_OK;
    }
    return CIV_ERR_RANGE;
}

/* --- Chain 2026-10-16 (10): census city production/status counting.
 * OS-free 1:1 LIVE. No globals, no exit(), C17. See game.h. */

int census_trade_base(int difficulty, int has_t31, int has_t26)
{
    /* Orig Census_ShowCityProduction @ 1018:0be0 head: local_e =
     * g_nDifficulty/2 (+1 Tech_HasTech 0x22, +1 Tech_HasTech 0x25).
     * x86 IDIV truncates toward zero (C matches). */
    int base = difficulty / 2;

    if (has_t31 != 0)
        base++;
    if (has_t26 != 0)
        base++;
    return base;
}

int census_trade_wonder_total(int trade, int wonder_open)
{
    /* Orig same: if (City_IsWonderOpen(g_nPlayerCiv,0x14)) local_c =
     * local_c + local_c/2 (iVar5 unused divide form: local_c +=
     * local_c/2 via (local_c>>0xf) sign adjust; C / truncates same). */
    if (wonder_open == 0)
        return trade;
    return trade + trade / 2;
}

int census_prod_accum_trade(int acc, int city_trade)
{
    /* Orig same per-city: local_a += DAT_1260_00d2 (trade rollup). */
    return acc + city_trade;
}

int census_prod_accum_shields(int acc, int city_shields)
{
    /* Orig same per-city: local_c += DAT_1260_00e2 (shield rollup). */
    return acc + city_shields;
}

int census_prod_year_band(int year)
{
    /* Orig same caravan band: g_nYear<0 → 1; <0x6d6 (1750) → 2; else 3. */
    if (year < 0)
        return 1;
    if (year < 0x6d6)
        return 2;
    return 3;
}

int census_caravan_value(int difficulty, int civ_value, int band,
                         int trade_total)
{
    /* Orig same: iVar5 = (g_nDifficulty*2+6) * civ_value * band;
     * local_8 = iVar5 / local_c (only when local_c != 0 and y < 0xb4).
     * civ_value is the (s__No_Data...+2+g_nPlayerCiv*2) word. */
    long v;

    if (trade_total <= 0 || band <= 0 || civ_value < 0 || difficulty < 0)
        return -1;
    v = (long)(difficulty * 2 + 6) * (long)civ_value * (long)band;
    return (int)(v / (long)trade_total);
}

int census_building_add(int count, int building_set, int building,
                        int unit_cost, int trade_base)
{
    /* Orig same 1..0x17 loop: bit (54fc word & 1<<bit) set →
     * local_46[bit] += *(bit*6+0x192c); bit == 1 additionally +=
     * local_e (trade base, palace/capital path). */
    if (building < 1 || building >= 0x18)
        return count;
    if (building_set == 0)
        return count;
    count += unit_cost;
    if (building == 1)
        count += trade_base;
    return count;
}

int census_status_prod_kind(int prod)
{
    /* Orig Census_ShowCityStatus @ 1018:208f: (char)+5 < 0 → Res 0x83
     * building branch (550a + 0x1924*10); < 0x40 → Res 0x84 unit branch
     * (550a + 0x1a44*10); else no prod line (falls to 10d8:002d tail). */
    if (prod < 0)
        return 0;
    if (prod < 0x40)
        return 1;
    return 2;
}

int census_status_shields_total(int shields, int cost)
{
    /* Orig same shields line: shown = 550a + cost_table*10 (cost via the
     * Res 0x83 0x1924 building table or Res 0x84 0x1a44 unit table). */
    return shields + cost * 10;
}

/* --- Chain 2026-10-16 (19): Pic_PumpMenuCommand mapping (goal 2) ---
 * See game.h block. OS-free 1:1 (no Win16, no globals, no exit()). */

int pic_pump_select_to_mode(int sel)
{
    /* Orig Pic_PumpMenuCommand @ 1030:9aed switch(DAT_12d8_0030):
     * 0->0, 1->0x96, 2->0x97, 3->0x98 (city-mode buttons in the
     * Pic_PlayShared @ 1030:0000 city view; WM_COMMAND 0x111 goes to
     * Menu_DispatchCommand @ 1048:01cd beside this switch, not through
     * it). Other values leave *param_2 untouched -> -1 here. */
    switch (sel) {
    case 0:
        return 0;
    case 1:
        return 0x96;
    case 2:
        return 0x97;
    case 3:
        return 0x98;
    default:
        return -1;
    }
}

int pic_pump_msg_is_command(int msg)
{
    /* Orig same: DAT_1420_54e8 == 0x111 (WM_COMMAND) ->
     * Menu_DispatchCommand(DAT_1420_54ea,0). */
    return (msg == 0x111) ? 1 : 0;
}

int pic_pump_msg_is_paint(int msg)
{
    /* Orig same: DAT_1420_54e8 == 0xf (WM_PAINT) && DAT_1420_54e6 ==
     * *param_1 -> Pic_PresentOnPaint @ 1030:900d (renamed). */
    return (msg == 0x0f) ? 1 : 0;
}

/* --- Chain 2026-10-16 (20): spaceship + drain + paint-tail (goals 1/4/5).
 * See game.h block. OS-free 1:1 (no Win16, no globals, no exit()). */

int ship_part_connected(int part)
{
    /* Orig Ship_IsPartConnected @ 1070:11f2: (byte == -1 || byte > 2) ->
     * 0 else 1 (ShowView sumuje 4 sasiadow per kafel wiekszy niz 1x1). */
    if (part == -1 || part > 2)
        return 0;
    return 1;
}

int ship_support_percent(int support_raw, int pop)
{
    /* Orig Ship_ShowView @ 1070:0327: denom = Util_Clamp(pop,1,99);
     * pct = (support*100)/denom (DAT_1290_01ac/DAT_1290_01aa). */
    int denom = pop;

    if (denom < 1)
        denom = 1;
    if (denom > 99)
        denom = 99;
    return (support_raw * 100) / denom;
}

int ship_support_frac(int support_raw, int pop)
{
    /* Orig same: rem = (support*100)%denom (x86 IDIV == C /% trunc). */
    int denom = pop;

    if (denom < 1)
        denom = 1;
    if (denom > 99)
        denom = 99;
    return (support_raw * 100) % denom;
}

int ship_power_percent(int power_raw, int pop, int support)
{
    /* Orig same: denom = Util_Clamp(pop+support,1,99);
     * pct = (power*200)/denom (DAT_1290_01ae). */
    int denom = pop + support;

    if (denom < 1)
        denom = 1;
    if (denom > 99)
        denom = 99;
    return (power_raw * 200) / denom;
}

int ship_power_frac(int power_raw, int pop, int support)
{
    /* Orig same: rem = (power*200)%denom. */
    int denom = pop + support;

    if (denom < 1)
        denom = 1;
    if (denom > 99)
        denom = 99;
    return (power_raw * 200) % denom;
}

int ship_flight_tenths(int mass_total, int pop10, int sup10)
{
    /* Orig same: thrust = Util_Clamp(pop10*10,0,sup10*10) (DAT_1290_01a4/
     * DAT_1290_01a6); tenths = (mass*0x32)/(thrust+1). mass_total to
     * local_c (suma count*waga). */
    int thrust = pop10 * 10;
    int cap = sup10 * 10;

    if (thrust < 0)
        thrust = 0;
    if (thrust > cap)
        thrust = cap;
    return (mass_total * 50) / (thrust + 1);
}

int ship_flight_years(int flight_tenths)
{
    /* Orig same display/arrival: years = tenths/10. */
    return flight_tenths / 10;
}

int ship_flight_frac(int flight_tenths)
{
    /* Orig same: frac = tenths%10 ("x.y Years"). */
    return flight_tenths % 10;
}

int ship_success_percent(int support_pct, int power_pct, int flight_tenths)
{
    /* Orig same: e=100; e=(e*clamp(sup,0,100))/100;
     * e=(e*clamp(pow,0,100))/100; if (flight > 0x96) e -= (flight-0x96)/10;
     * clamp 0..100. */
    int e = 100;
    int s = support_pct;
    int p = power_pct;

    if (s < 0)
        s = 0;
    if (s > 100)
        s = 100;
    if (p < 0)
        p = 0;
    if (p > 100)
        p = 100;
    e = (e * s) / 100;
    e = (e * p) / 100;
    if (flight_tenths > 0x96)
        e -= (flight_tenths - 0x96) / 10;
    if (e < 0)
        e = 0;
    if (e > 100)
        e = 100;
    return e;
}

int ship_arrival_year(int year, int flight_tenths)
{
    /* Orig same stamp: *(param*2+0x752) = g_nYear + tenths/10. */
    return year + flight_tenths / 10;
}

int sys_drain_state_words(void)
{
    /* Orig Sys_DrainInput @ 11f0:4ea0 head: DAT_1420_1d44/1d46/1d48/1d4e/
     * 1d4c/1d50 = 0 (6 slow; potem dwa PEEKMESSAGE-drainy + 12b0:004c). */
    return 6;
}

int sys_drain_queue_cap(void)
{
    /* Orig Main_EnqueueResString @ 1008:0a8d: if (DAT_12b0_004c < 10)
     * append (Sys_DrainInput zeruje head, Main_DrainMessages oproznia). */
    return 10;
}

int sys_drain_queue_reset(void)
{
    /* Orig Sys_DrainInput @ 11f0:4ea0 tail: DAT_12b0_004c = 0. */
    return 0;
}

int sys_drain_peek_key_first(void)
{
    /* Orig same drain1: PEEKMESSAGE wMsgFilterMin 0x108. */
    return 0x108;
}

int sys_drain_peek_key_last(void)
{
    /* Orig same drain1: wMsgFilterMax 0x100 (min > max = current-message
     * drain idiom; exact Win16 filter quirk TBD, value kept verbatim). */
    return 0x100;
}

int sys_drain_peek_mouse_first(void)
{
    /* Orig same drain2: wMsgFilterMin 0x209. */
    return 0x209;
}

int sys_drain_peek_mouse_last(void)
{
    /* Orig same drain2: wMsgFilterMax 0x200 (same idiom as above). */
    return 0x200;
}

int pic_paint_port_id(void)
{
    /* Orig Pic_PresentOnPaint @ 1030:900d: GfxPort_PresentRect(0x70,...). */
    return 0x70;
}

int pic_paint_w(void)
{
    /* Orig same: SETRECT(...,0x140,0x200,...) box przed PresentRect. */
    return 0x140;
}

int pic_paint_h(void)
{
    /* Orig same: h = 0x200. */
    return 0x200;
}

/* --- Chain 2026-10-16 (21): starfield step + view-input codes + finish tail
 * (goals 1/2/4). See game.h block. OS-free 1:1 (no Win16, no globals,
 * no exit()). */

int ship_starfield_count(void)
{
    /* Orig Ship_AnimateStarfield @ 1070:12b5: loop 0..0x13 (20 wpisow;
     * x @ +0x166, y @ +0x13e, flag @ +0x5232, color @ +0x116). */
    return 20;
}

int ship_starfield_speed(int layer)
{
    /* Orig same scroll-switch (idx&3): 0:+6, 1:+5, 2:+4, 3:+3. */
    switch (layer & 3) {
    case 0:
        return 6;
    case 1:
        return 5;
    case 2:
        return 4;
    case 3:
        return 3;
    default:
        return -1;
    }
}

int ship_starfield_step_x(int x, int layer, int frame)
{
    /* Orig same: warstwy 2/3 tylko gdy (DAT_1420_522c&1) != 0. */
    int l = layer & 3;
    int speed = ship_starfield_speed(l);

    if (speed < 0)
        return x;
    if (l >= 2 && (frame & 1) == 0)
        return x;
    return x + speed;
}

int ship_starfield_needs_wrap(int x)
{
    /* Orig same: if (0x17f < x) { x = 2; y = Rng+0xc; flag = 0; }. */
    return (x > 0x17f) ? 1 : 0;
}

int ship_starfield_wrap_x(void)
{
    /* Orig same wrap-body: x = 2. */
    return 2;
}

int ship_starfield_wrap_y(int rand133)
{
    /* Orig same: y = Rng_Range(0x133)+0xc (draw 0..0x132). */
    if (rand133 < 0)
        return 12;
    return rand133 + 12;
}

int ship_view_msg_is_char(int msg)
{
    /* Orig Ship_ViewHookProc @ 1070:10cb CS-tabela @ 1070:11e2: 0x0102. */
    return (msg == 0x0102) ? 1 : 0;
}

int ship_view_msg_is_command(int msg)
{
    /* Orig same: 0x0111 WM_COMMAND (Launch 999 branch). */
    return (msg == 0x0111) ? 1 : 0;
}

int ship_view_msg_is_lbuttondown(int msg)
{
    /* Orig same: 0x0201 (coords -> 1420:5228/522A, flag 5226). */
    return (msg == 0x0201) ? 1 : 0;
}

int ship_view_msg_is_lbuttonup(int msg)
{
    /* Orig same: 0x0202 (valid part-click -> b376 = 1). */
    return (msg == 0x0202) ? 1 : 0;
}

int ship_view_launch_id(void)
{
    /* Orig same COMMAND-branch: DI == 0x3e7 (999 Launch). */
    return 999;
}

int ship_view_result_is_close(int code)
{
    /* Orig DAT_1420_b376: 1 = close/cancel (CHAR / valid part-click). */
    return (code == 1) ? 1 : 0;
}

int ship_view_result_is_launch(int code)
{
    /* Orig same: 2 = launch-confirm (COMMAND 999). */
    return (code == 2) ? 1 : 0;
}

civ_error_t ship_pump_view_input(void)
{
    /* TODO: Orig Ship_PumpViewInput @ 1070:1018 (SETWINDOWLONG hook
     * Ship_ViewHookProc @ 1070:10cb + PEEKMESSAGE + TRANSLATE/DISPATCH,
     * wynik via DAT_1420_b376 1/2; caller Ship_ShowView @ 1070:0327).
     * Win16 queue/hook -> SDL modal bridge TBD. */
    return CIV_ERR_UNSUPPORTED;
}

int pic_finish_paint_step_count(void)
{
    /* Orig Pic_FinishPaint @ 1030:94d2: 3 decor-passy (arrow, arrow,
     * caption) via Pic_DrawFrameArrow @ 1030:96ad x2 +
     * Pic_DrawFrameCaption @ 1030:982f x1; dispatch 0x96/0x97/0x98 zyje
     * w Pic_DispatchFinishPaint @ 1030:95c2. */
    return 3;
}

int pic_finish_paint_step_is_caption(int step)
{
    /* Orig same: tylko ostatni pass to caption (DRAWTEXT). */
    return (step == 2) ? 1 : 0;
}

civ_error_t pic_finish_paint(void)
{
    /* TODO: Orig Pic_FinishPaint @ 1030:94d2 tail (frame/border decor via
     * FUN_1000_14f8 + Pic_DrawFrameArrow @ 1030:96ad / Pic_DrawFrameCaption
     * @ 1030:982f; WM_PAINT 0xf tail Pic_PresentOnPaint @ 1030:900d po
     * PresentRect portu 0x70 box 0x140x0x200). Kolejnosc powyzej LIVE;
     * restore DC zyje w GfxPort_PresentRect @ 10d0:0854 (VALIDATERECT +
     * StoreDC restore) i Gfx_StoreDC @ 1098:1104 -> Gfx_SetCurrentDC @
     * 1100:09a9 (29 callerow: Anim_AdvanceFrames @ 10b0:0461 frame-pump,
     * Pic_PlayShared @ 1030:0000, Dialog_MenuZ_Modal @ 1220:0156,
     * Dlg_CreateTracked @ 1098:124f, map opens @ 1208:0af1/1210:025b,
     * Game_UpdateStatusPanel @ 1088:13d5, Unit_ShowInfoPopup @ 11f0:468c;
     * Ship_ShowView @ 1070:0327 nie uzywa StoreDC — SetActive+PresentRect
     * wprost) + GfxPort_ClearActive @ 10d0:09a2; unlock to
     * SELECTOBJECT-restore w Arrow/Caption; dirty-clear to VALIDATERECT.
     * GDI -> SDL bridge TBD. */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (22): Ship hook dispatch + Pic arrow/caption geometry
 * (goals 2/3). See game.h. OS-free 1:1 LIVE (no Win16, no globals). */

int ship_view_dispatch(int msg)
{
    /* Orig Ship_ViewHookProc @ 1070:10cb CS table @ 1070:11e2: 0x0102 ->
     * 0x10FD (char), 0x0111 -> 0x110F (command), 0x0201 -> 0x117F (down),
     * 0x0202 -> 0x1129 (up); loop over 4 entries, else DEFDLGPROC. */
    switch (msg) {
    case 0x0102:
        return 0;
    case 0x0111:
        return 1;
    case 0x0201:
        return 2;
    case 0x0202:
        return 3;
    default:
        return -1;
    }
}

int ship_command_is_launch(int wparam)
{
    /* Orig Ship_ViewCommandHandler @ 1070:110f: AX = DI (wParam low),
     * CMP AX,0x3e7 -> JZ b376 = 2 (Launch), else DEFDLGPROC. */
    return (wparam == 999) ? 1 : 0;
}

int ship_lbuttonup_is_armed(int armed_flag)
{
    /* Orig Ship_ViewButtonUpHandler @ 1070:1129: CMP [0x5226],0 -> JZ
     * 0x1177 skip (disarm path clears 5226, no b376); armed path saves
     * coords + hit-tests + maybe b376 = 1. */
    return (armed_flag != 0) ? 1 : 0;
}

int ship_hit_sets_armed(int hit)
{
    /* Orig Ship_ViewButtonDownHandler @ 1070:117f: hit-test via
     * 1608:00b0/0104 on DAT_1290_0110; hit -> [0x5226] = 1 (arms UP),
     * miss -> flag untouched; never touches b376 itself. */
    return (hit != 0) ? 1 : 0;
}

int pic_arrow_dir(int kind)
{
    /* Orig Pic_DrawFrameArrow @ 1030:96ad: kind (param_6) == 1 -> iVar3 =
     * -1 else +1 (triangle points left vs right). */
    return (kind == 1) ? -1 : 1;
}

int pic_arrow_row_count(int h)
{
    /* Orig same: for (local_10 = 0; local_10 < h-7; ...) MOVETO/LINETO
     * rows; h = param_5-param_3. Non-positive loop = 0 rows. */
    int n = h - 7;
    return (n > 0) ? n : 0;
}

int pic_arrow_is_greyed(int kind, int f002e, int f0030)
{
    /* Orig same pen pick: ((kind == 2 && DAT_1260_002e == 0) ||
     * (kind == 1 && DAT_1260_0030 == 0)) -> GETSTOCKOBJECT greyed, else
     * CREATEPEN(0x1000,0x808080,1). */
    if ((kind == 2 && f002e == 0) || (kind == 1 && f0030 == 0))
        return 1;
    return 0;
}

int pic_caption_text_dx(int w)
{
    /* Orig Pic_DrawFrameCaption @ 1030:982f: OFFSETRECT((param_5-param_3)
     * /4, 0) after INFLATERECT(1,1) + OFFSETRECT(bold,bold). */
    return w / 4;
}

int pic_dispatch_is_caption(int mode)
{
    /* Orig Pic_DispatchFinishPaint @ 1030:95c2: mode 0x96 -> caption
     * (Pic_DrawFrameCaption @ 1030:982f) when (*(p+6)&3) != 0. */
    return (mode == 0x96) ? 1 : 0;
}

int pic_dispatch_is_arrow(int mode)
{
    /* Orig same: 0x97/0x98 -> arrow (Pic_DrawFrameArrow @ 1030:96ad)
     * when (*(p+6)&3) != 0 (0x97 with setup 1, 0x98 with setup 2). */
    return (mode == 0x97 || mode == 0x98) ? 1 : 0;
}

int pic_dispatch_arrow_variant(int mode)
{
    /* Orig same FUN_1000_14f8 setup variant: 0x97 -> 1, 0x98 -> 2. */
    if (mode == 0x97)
        return 1;
    if (mode == 0x98)
        return 2;
    return -1;
}

int pic_pump_owner_is_shared(int from_shared)
{
    /* Orig Pic_PumpMenuCommand @ 1030:9aed sole caller Pic_PlayShared @
     * 1030:0000 (city-view modal loop; WM_PAINT 0xf -> Pic_PresentOnPaint
     * @ 1030:900d + sel 1/2/3 -> 0x96/97/98 via pic_pump_select_to_mode +
     * WM_COMMAND 0x111 -> Menu_DispatchCommand @ 1048:01cd). */
    return (from_shared != 0) ? 1 : 0;
}

/* --- Chain 2026-10-16 (30): City_FoundCity kernels (task goal 2) ---
 * See game.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()). */

int city_found_free_slot(const int *valids, int n)
{
    /* Orig City_FoundCity @ 1040:0000 slot: while (iVar8 <= 0x7f) if
     * (5502[i] == -1) break; 0x7f < i -> msg + -1. valids[i] == -1/0xff. */
    int i;

    if (!valids || n <= 0)
        return -1;
    for (i = 0; i < n && i < 0x80; i++) {
        if (valids[i] == -1 || (valids[i] & 0xff) == 0xff)
            return i;
    }
    return -1;
}

int city_found_initial_prod(int has_t34, int has_t64)
{
    /* Orig City_FoundCity @ 1040:0000 prod: 5505 = 1; Tech(0x22) -> 4;
     * Tech(0x40) -> 5 (nadpisuje 4). */
    int prod = 1;

    if (has_t34 != 0)
        prod = 4;
    if (has_t64 != 0)
        prod = 5;
    return prod;
}

int city_found_is_first(const int *owners, const int *valids, int n,
                        int civ, int exclude)
{
    /* Orig City_FoundCity @ 1040:0000 first: bVar9 = true; for 0x80: owner ==
     * civ && i != nowe -> false; palace 54fc = 1 gdy true. */
    int i;

    if (!owners || !valids || n <= 0)
        return 0;
    for (i = 0; i < n && i < 0x80; i++) {
        if (i == exclude)
            continue;
        if (valids[i] == -1 || (valids[i] & 0xff) == 0xff)
            continue;
        if (owners[i] == civ)
            return 0;
    }
    return 1;
}

int city_found_trim_len(const char *s, int len)
{
    /* Orig City_FoundCity @ 1040:0000 trim: iVar4 = 0x1f; while (0 < iVar4 &&
     * name[iVar4] == ' ') name[iVar4--] = 0. Zwraca dlugosc bez ' '. */
    int n;

    if (!s || len <= 0)
        return 0;
    n = len;
    while (n > 0 && s[n - 1] == ' ')
        n--;
    return n;
}

/* --- Chain 2026-10-16 (30): Tech_ChooseResearch kernels (task goal 4) ---
 * See game.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()). */

int tech_calc_cost(int base0, int base1, int civ_factor)
{
    /* Orig Tech_CalcCost @ 1170:31ec: *(tech*2+0x90) + *(tech*2+0x91) *
     * 1ca4[civ] (signed char x s16). */
    return base0 + base1 * civ_factor;
}

int tech_research_eligible(int not_owned, int pre1_owned, int pre2_owned)
{
    /* Orig Tech_ChooseResearch @ 1170:21b3 eligible: HasTech(t) == 0 &&
     * HasTech(pre1 @ 1894) != 0 && HasTech(pre2 @ 1895) != 0. */
    if (!not_owned)
        return 0;
    if (!pre1_owned)
        return 0;
    if (!pre2_owned)
        return 0;
    return 1;
}

int tech_research_roll_is_better(int roll, int best, int cur_tech,
                                 int active_tech, int civ, int player_civ)
{
    /* Orig Tech_ChooseResearch @ 1170:21b3 MAX-roll: (local_c < roll) &&
     * (12d0:0062 != tech || player != civ). */
    if (roll <= best)
        return 0;
    if (cur_tech == active_tech && civ == player_civ)
        return 0;
    return 1;
}

int tech_grant_is_future(int tech)
{
    /* Orig Tech_GrantTech @ 1170:2aaa Future: tech == 0x47 (Future Technology,
     * 12a8:0022++ licznik, bez ESCHISM-bitu). */
    return (tech == 0x47) ? 1 : 0;
}

int tech_bit_word(int tech)
{
    /* Orig Tech_HasTech @ 1170:3181 word: tech/0x10 (0..4 dla 0x47). */
    if (tech < 0)
        return -1;
    return tech / 0x10;
}

int tech_bit_mask(int tech)
{
    /* Orig Tech_HasTech @ 1170:3181 mask: 1 << (tech%0x10). */
    int r;

    if (tech < 0)
        return -1;
    r = tech % 0x10;
    if (r < 0)
        r += 0x10;
    return 1 << r;
}

/* --- Chain 2026-10-16 (task): Hut/Tech/Eliminate/Pedia/Gov kernels ---
 * See game.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()). */

int hut_found_city_site_ok(int dist, int tile_flags)
{
    /* Orig Hut_ProcessHut @ 11c0:2677 case 0: 3 < dist + 4 < (tile & 7). */
    if (dist <= 3)
        return 0;
    return ((tile_flags & 7) > 4) ? 1 : 0;
}

int hut_tech_shuffle_ok(int turn, int year)
{
    /* Orig Hut_ProcessHut @ 11c0:2677 case 1: turn != 0 && year < 0x3e9. */
    if (turn == 0)
        return 0;
    return (year < 0x3e9) ? 1 : 0;
}

int hut_tech_eligible(int not_owned, int pre1_owned, int pre2_owned)
{
    /* Orig Hut_ProcessHut @ 11c0:2677 case 1 pick (jak Tech_ChooseResearch). */
    if (!not_owned)
        return 0;
    if (!pre1_owned)
        return 0;
    if (!pre2_owned)
        return 0;
    return 1;
}

int hut_gold_bonus(void)
{
    /* Orig Hut_ProcessHut @ 11c0:2677 case 2: treasury += 0x32. */
    return 0x32;
}

int hut_barbs_ok(int dist, int city_count)
{
    /* Orig Hut_ProcessHut @ 11c0:2677 case 3: 3 < dist && 3efe != 0. */
    if (dist <= 3)
        return 0;
    return (city_count != 0) ? 1 : 0;
}

int hut_barb_step(int city_count)
{
    /* Orig Hut_ProcessHut @ 11c0:2677 case 3: Util_Clamp(4-count,1,4). */
    int v = 4 - city_count;

    if (v < 1)
        v = 1;
    if (v > 4)
        v = 4;
    return v;
}

int hut_barb_unit_kind(int tile_cost)
{
    /* Orig Hut_ProcessHut @ 11c0:2677 case 3: 17d4[class] < 3 ? 6 : 3. */
    return (tile_cost < 3) ? 6 : 3;
}

int hut_friendly_unit_kind(int rng2)
{
    /* Orig Hut_ProcessHut @ 11c0:2677 default: Rng_Range(2) == 0 ? 6 : 3. */
    return (rng2 == 0) ? 6 : 3;
}

int tech_grant_stores_bit(int tech)
{
    /* Orig Tech_GrantTech @ 1170:2aaa: tech != 0x47 stawia bit + 0x48-wpis. */
    return (tech != 0x47) ? 1 : 0;
}

int tech_grant_is_library_source(int src)
{
    /* Orig Tech_GrantTech @ 1170:2aaa: param_3 == 0xfffe = Great Library. */
    return (src == 0xfffe) ? 1 : 0;
}

int tech_grant_library_spread_ok(int owners)
{
    /* Orig Tech_GrantTech @ 1170:2aaa: 1 < local_a (wlascicieli > 1). */
    return (owners > 1) ? 1 : 0;
}

int tech_grant_barracks_obsolete_clear(int mask)
{
    /* Orig Tech_GrantTech @ 1170:2aaa (0x22/0x25): 54fc & 0xfffffffd. */
    return mask & ~2;
}

int tech_grant_from_other(int civ, int src)
{
    /* Orig Tech_GrantTech @ 1170:2aaa acquire: civ != src && src != 0. */
    if (civ == src)
        return 0;
    if (src == 0)
        return 0;
    return 1;
}

int tech_cost_roll_bound(int cost)
{
    /* Orig Tech_ChooseResearch @ 1170:21b3: Rng_Range(koszt << 2). */
    return cost << 2;
}

int game_eliminate_needed(int civ, int player_civ)
{
    /* Orig Game_EliminateCiv @ 10e8:1b97: civ != 0 && civ != player. */
    if (civ == 0)
        return 0;
    if (civ == player_civ)
        return 0;
    return 1;
}

int game_eliminate_alive_clear(int mask, int civ)
{
    /* Orig Game_EliminateCiv @ 10e8:1b97: mask & ~(1 << (civ & 0x1f)). */
    if (civ < 0)
        return -1;
    return mask & ~(1 << (civ & 0x1f));
}

int game_eliminate_seen_clear(int cur, int civ)
{
    /* Orig Game_EliminateCiv @ 10e8:1b97 seen 0x50x0x32: cur &= ~(1<<civ). */
    if (civ < 0)
        return -1;
    return cur & ~(1 << (civ & 0x1f));
}

int pedia_entry_kind_valid(int kind)
{
    /* Orig Pedia_ShowEntry @ 1068:06af switch: case 1..5. */
    return (kind >= 1 && kind <= 5) ? 1 : 0;
}

int pedia_res_for_selector(int sel)
{
    /* Orig Pedia_ShowByCategory @ 1020:0000: sel 0..4 -> 0x82 + sel. */
    if (sel < 0 || sel > 4)
        return -1;
    return 0x82 + sel;
}

int pedia_build_variant(int param)
{
    /* Orig Pedia_ShowEntry @ 1068:06af case 2: param+1 < 0x19 ? 2 : 3. */
    return (param + 1 < 0x19) ? 2 : 3;
}

int pedia_terrain_find_index(const int *table, int n, int id)
{
    /* Orig Pedia_FindTerrainIndex @ 1020:011b: scan 0..0xb, else 0xffff. */
    int i;

    if (!table || n <= 0)
        return -1;
    for (i = 0; i < n; i++) {
        if (table[i] == id)
            return i;
    }
    return -1;
}

int gov_announce_needed(int gov)
{
    /* Orig Gov_AnnounceChange @ 10e8:32c1: gov(3b77+0x13) != 0. */
    return (gov != 0) ? 1 : 0;
}

/* --- Chain 2026-10-16 (p5): ConquestTech transfer + EliminatedFlags ---
 * See game.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()). */

int tech_conquest_gate(int winner, int loser)
{
    /* Orig Tech_GrantConquestTech @ 1038:09bc: param_1 != 0 && param_2 != 0. */
    if (winner == 0)
        return 0;
    if (loser == 0)
        return 0;
    return 1;
}

int tech_conquest_list_cap(void)
{
    /* Orig same: local_9c[72] dla techow 0..0x47. */
    return 0x48;
}

int tech_conquest_eligible(int winner_lacks, int loser_has)
{
    /* Orig same scan: HasTech(w,t) == 0 && HasTech(l,t) != 0. */
    if (!winner_lacks)
        return 0;
    if (!loser_has)
        return 0;
    return 1;
}

int tech_conquest_cost_is_better(int cost, int best)
{
    /* Orig same max-track: local_8 < Tech_CalcCost(...) -> update. */
    return (cost > best) ? 1 : 0;
}

int tech_conquest_initial_best(void)
{
    /* Orig same: local_8 = -1 przed scanem. */
    return -1;
}

int tech_conquest_has_any(int count)
{
    /* Orig same: local_6 != 0 -> dialog/grant; 0 = nic do transferu. */
    return (count != 0) ? 1 : 0;
}

int tech_conquest_player_picks(int player_civ, int winner)
{
    /* Orig same: g_nPlayerCiv == param_1 -> petla wyboru z listy. */
    return (player_civ == winner) ? 1 : 0;
}

int tech_conquest_player_loses(int player_civ, int loser)
{
    /* Orig same: g_nPlayerCiv == param_2 -> komunikat ofiary z max-tech. */
    return (player_civ == loser) ? 1 : 0;
}

int game_elimflag_low_mask(int civ)
{
    /* Orig Game_ClearEliminatedCivFlags @ 1038:083d: 1 << (civ & 0x1f). */
    if (civ < 0)
        return -1;
    return 1 << (civ & 0x1f);
}

int game_elimflag_high_mask(int civ)
{
    /* Orig same: 1 << ((civ + 8) & 0x1f). */
    if (civ < 0)
        return -1;
    return 1 << ((civ + 8) & 0x1f);
}

int game_elimflag_msg_needed(int flags_00b6, int civ)
{
    /* Orig same: (DAT_12d0_00b6 & high) != 0 -> ShowMessage. */
    int high = game_elimflag_high_mask(civ);

    if (high < 0)
        return 0;
    return ((flags_00b6 & high) != 0) ? 1 : 0;
}

int game_elimflag_msg_id(int flags_00b6, int civ)
{
    /* Orig same: low CLEAR -> 0x764 cancelled, low SET -> 0x752 returns. */
    int low = game_elimflag_low_mask(civ);

    if (low < 0)
        return -1;
    return ((flags_00b6 & low) == 0) ? 0x764 : 0x752;
}

int game_elimflag_clear(int flags_00b6, int civ)
{
    /* Orig same: 00b6 & ~low & ~high (+ NewGame_ResetCivData caller-side). */
    int low = game_elimflag_low_mask(civ);
    int high = game_elimflag_high_mask(civ);

    if (low < 0 || high < 0)
        return -1;
    return flags_00b6 & ~low & ~high;
}

int city_plunder_value(int treasury, int city_size, int divisor)
{
    /* Orig City_CalcPlunderValue @ 1038:0901 overflow-guard 32000/size. */
    if (city_size <= 0)
        return -1;
    if (divisor < 0)
        return -1;
    if (treasury < 32000 / city_size)
        return (treasury * city_size) / (divisor + 1);
    return (treasury / (divisor + 1)) * city_size;
}

/* --- Game end / uprising / pollution / status math (2026-10-16) ---
 * OS-free 1:1 LIVE gates. No globals, no Win16, no exit(), C17. See game.h
 * block for callers/callees/addrs + STUB tails. */

int game_end_needs_replay(int quit_flag)
{
    /* Orig Game_ProcessGameEnd @ 1088:0b6d: if (g_nQuitFlag != 2)
     * Replay_OpenOrPlay @ 1180:0000 (g_nQuitFlag @ 12d8:24ee: 2 skips the
     * teardown; HoF path follows via the ceremony gate below). */
    return (quit_flag != 2) ? 1 : 0;
}

int game_end_shows_ceremony(int flags_00b6)
{
    /* Orig Game_ProcessGameEnd @ 1088:0b6d: if ((DAT_12d0_00b6 & 0x100)
     * == 0) Hof_CalcFinalScore @ 1040:1137 + HoF_VictoryCeremony/Load/
     * TryInsert/Save (see halloffame.h, parallel-owned). */
    return ((flags_00b6 & 0x100) == 0) ? 1 : 0;
}

int game_uprising_tile_ok(int tile_class, int unit_at, int cont_val)
{
    /* Orig Game_SpawnUprisings @ 1088:0be9 tile loop: do { Rng_Range @
     * 11c8:003d (0x50/0x2c+3) draws } while (Map_GetTileClass @ 1110:16c0
     * != 10); } while (Map_GetUnitAt @ 1110:18ce != -1 ||
     * Map_GetOceanOrContVal @ 1110:1dbd < 0x10). Draws stay caller-side. */
    if (tile_class != 10)
        return 0;
    if (unit_at != -1)
        return 0;
    return (cont_val >= 0x10) ? 1 : 0;
}

int game_uprising_leader(int turn)
{
    /* Orig Game_SpawnUprisings @ 1088:0be9: iVar3 = Util_Clamp(g_nTurn /
     * 0x96 + 1, 1, 3) @ 11c8:0063 (util_clamp in sys.h); leader 0x11 when
     * iVar3 < 3 else 0x12 for Unit_SpawnUnit @ 11c0:12c8. Negative turns
     * clamp via util_clamp (orig used non-negative g_nTurn). */
    int v = util_clamp(turn / 0x96 + 1, 1, 3);

    return (v < 3) ? 0x11 : 0x12;
}

int game_uprising_wave_due(int turn)
{
    /* Orig Game_SpawnUprisings @ 1088:0be9 wave gate: ((g_nTurn + 1U & 7)
     * == 0) && ((DAT_12d8_2b74 & 7) == 0). The DAT_12d8_2b74 conjunct is
     * game-state owned and stays caller-side; this is the pure turn half. */
    return (((turn + 1) & 7) == 0) ? 1 : 0;
}

int game_pollution_spread_due(int counter)
{
    /* Orig Game_DoPollutionAndWonderRace @ 1088:1e32: if (0x10 <
     * DAT_12d8_42f0) { DAT_12d8_42f2++; Map_SpreadPollution @ 1028:1afd;
     * DAT_12d8_42f0 = 0; } (counter is DAT_12d8_42f0). */
    return (0x10 < counter) ? 1 : 0;
}

int game_pollution_alarm_due(int counter, int smoke)
{
    /* Orig same: if ((DAT_12d8_42f0 == 0x0c) && (6 < DAT_12d8_42f4))
     * "Scientists alarmed" @ 1420:1744 + Status_FormatMainRow @ 1138:0000
     * (smoke is DAT_12d8_42f4; message tail stays Win16). */
    return (counter == 0x0c && 6 < smoke) ? 1 : 0;
}

int game_status_pollution_icon(int counter)
{
    /* Orig Game_UpdateStatusPanel @ 1088:13d5: if (DAT_12d8_42f0 != 0)
     * Gfx_DrawTechIcon @ 11a8:0cb1 variant (DAT_12d8_42f0 % 4) + count
     * below (x86 IDIV truncates toward zero, same as C %). */
    if (counter < 0)
        counter = 0;
    return counter % 4;
}

int game_status_pollution_count(int counter)
{
    /* Orig same: Util_Clamp(DAT_12d8_42f0 / 4, 0, 3) @ 11c8:0063. */
    if (counter < 0)
        counter = 0;
    return util_clamp(counter / 4, 0, 3);
}

int game_status_progress_index(int science, int difficulty, int base,
                               int trade, int year)
{
    /* Orig Game_UpdateStatusPanel @ 1088:13d5 progress icon: year < 0 ?
     * 1 : 2 factor (g_nYear @ 12d8:42fa sign select), then Util_Clamp @
     * 11c8:0063((science << 2) / ((difficulty * 2 + base + 6) * trade *
     * factor + 1), 0, 3); icon via DAT_1420_98e4 + Gfx_DrawTechIcon @
     * 11a8:0cb1. science is DAT_43b8+0x7c[civ], base DAT_12a8_0020,
     * trade DAT_3f5c+2[civ]. Negative game inputs clamp to 0 (orig used
     * non-negative values). */
    int factor;
    long denom;
    long v;

    if (science < 0)
        science = 0;
    if (difficulty < 0)
        difficulty = 0;
    if (base < 0)
        base = 0;
    if (trade < 0)
        trade = 0;
    factor = (year < 0) ? 1 : 2;
    denom = (long)(difficulty * 2 + base + 6) * (long)trade * (long)factor + 1L;
    v = ((long)science << 2) / denom;
    return util_clamp((int)v, 0, 3);
}

/* --- Turn-loop gates + tech-icon budget + demo rows (2026-10-16) ---
 * See game.h block. OS-free 1:1 LIVE (no globals, no Win16, no exit()). */

int game_turn_disaster_due(int turn)
{
    /* Orig Game_TurnLoopTick @ 1088:0000 (g_nTurn @ 12d8:42fe): 2x
     * City_ProcessDisaster @ 1040:068e once g_nTurn > 0x32 (Rng_Range @
     * 11c8:003d 0x80 draws stay caller-side). */
    return (turn > 0x32) ? 1 : 0;
}

int game_turn_treasury_warn(int treasury)
{
    /* Orig Game_TurnLoopTick @ 1088:0000 treasury<100 memory-warning
     * branch (Dialog_ShowMessage @ 1220:00d2 via FUN_11d0_01f7 tail stays
     * Win16 caller-side). */
    return (treasury < 100) ? 1 : 0;
}

int status_tech_count_ok(int count, int progress)
{
    /* Orig Status_DrawTechIcons @ 1088:1a82 budget (count <=
     * Util_Clamp(param2+3,3,0x24) @ 11c8:0063; Util_Clamp inline here, no
     * link, like unit_popup_moves_full). */
    int budget = progress + 3;

    if (budget < 3)
        budget = 3;
    if (budget > 0x24)
        budget = 0x24;
    if (count < 0)
        return 0;
    return (count <= budget) ? 1 : 0;
}

int status_tech_height_ok(int height, int count)
{
    /* Orig Status_DrawTechIcons @ 1088:1a82 (height <= count/5, <= 3;
     * x86 IDIV truncates toward zero, same as C /). */
    if (height < 0)
        return 0;
    if (height > 3)
        return 0;
    return (height <= count / 5) ? 1 : 0;
}

int demo_military_rate(int count, int pop)
{
    /* Orig Demo_ShowDemographics @ 1188:123a Military row: (3eee count *
     * 10) / pop. Caller sums live 0x80 city records per civ (no LCG
     * fake, unlike the status tech-icon AI rows). */
    if (pop <= 0)
        return -1;
    return (count * 10) / pop;
}

int demo_productivity(int output, int pop)
{
    /* Orig Demo_ShowDemographics @ 1188:123a Productivity row:
     * output*10/pop. */
    if (pop <= 0)
        return -1;
    return output * 10 / pop;
}

int demo_family_raw(int food, int pop)
{
    /* Orig Demo_ShowDemographics @ 1188:123a Family Size row:
     * (food*0x28)/pop+0x14 (callers render x/10 + "." + x%10 +
     * " children", rank /10). */
    if (pop <= 0)
        return -1;
    return (food * 0x28) / pop + 0x14;
}

int demo_life_clamp(int impr)
{
    /* Orig Demo_ShowDemographics @ 1188:123a Life Expectancy row:
     * Util_Clamp(impr,0x14,99) @ 11c8:0063 (inline here, no link). */
    int v = impr;

    if (v < 0x14)
        v = 0x14;
    if (v > 99)
        v = 99;
    return v;
}

/* --- Turn-loop frontier gates (2026-10-16): OS-free 1:1 LIVE heads for
 * STUB parents (Win16/GDI/dialog tails stay CIV_ERR_UNSUPPORTED in the
 * parents with TODO Orig). No globals, no Win16, no exit(), C17. Ref:
 * unpacked/civ.exe read-only (never edited; civ1win/ untouched). */

int city_disaster_city_ok(int valid, int pop, int size)
{
    /* Orig City_ProcessDisaster @ 1040:068e pick (called 2x per
     * Game_TurnLoopTick @ 1088:0000 pass once g_nTurn > 0x32): valid @
     * 1420:5502 != -1 (0xff empty slot, cf. city_found_free_slot) + pop
     * @ 1420:3efe > 1 + size @ 1420:5503 > 4 (Rng_Range @ 11c8:003d 0x80
     * draws + messages @ 1420:07f0/0839 + viewer Pic_PlayShared @
     * 1030:0000 + dialog @ 1220:00d2 + Map_CenterOnTile @ 1110:03ae stay
     * caller-side in city_process_disaster, STUB). */
    if (((unsigned)valid & 0xffu) == 0xffu)
        return 0;
    if (pop <= 1)
        return 0;
    if (size <= 4)
        return 0;
    return 1;
}

int city_disaster_has_building(unsigned mask, unsigned bit)
{
    /* Orig same switch Rng_Range(10): building-flag checks on the 0x1c
     * city mask @ 1420:54fc (bits 0x10/0x100/0x80/0x08/0x04/0x02/0x40
     * gate the GRANARY/TEMPLE/COURTHOUSE/MARKETPLACE/CATHEDRAL demands +
     * 0x860/0x873/0x87a/0x885/0x891/0x89b tails). */
    if (bit == 0u)
        return 0;
    return (mask & bit) != 0u ? 1 : 0;
}

int game_status_milestone_due(int prev_tier, int tier, int flag14e6)
{
    /* Orig Game_UpdateStatusPanel @ 1088:13d5 tail: Dialog_ShowMessage @
     * 1220:00d2 fires when the pop tier rises and DAT_1420_14e6 > -2
     * ("00,000 citizens." @ 1420:172b tail; string verified present in
     * unpacked/civ.exe via strings, read-only). Dialog tail stays Win16
     * in game_update_status_panel (STUB). */
    if (tier <= prev_tier)
        return 0;
    return (flag14e6 > -2) ? 1 : 0;
}

int status_lcg_pick(unsigned seed)
{
    /* Orig Status_DrawTechIcons @ 1088:1a82 AI-fake step (other civs;
     * player copies live rows 0xdc/0xc4): pick (seed & 0xff) % 9, seed
     * iterated by status_lcg_next (seed*5+1 from DAT_13c0_0000*civ)
     * under adjacency + budget (status_tech_count_ok/height_ok) + 999-try
     * cap gates in status_draw_tech_icons (STUB). */
    return (int)((seed & 0xffu) % 9u);
}

int gfx_tech_icon_valid(int index)
{
    /* Orig Gfx_DrawTechIcon @ 11a8:0cb1 (9-icon blitter called from
     * status_draw_tech_icons @ 1088:1a82; SELECTPALETTE + BITBLT tail
     * stays Win16 in gfx_draw_tech_icon, STUB). */
    return (index >= 0 && index <= 8) ? 1 : 0;
}

int unit_found_join_ok(int type, int home, int city_idx)
{
    /* Orig Unit_FoundCityIfHome @ 11c0:16a4 gate, mirrors the join branch
     * LIVE in unit_do_found_city (PARTIAL): type 0x1a settler (1420:62ff)
     * with home-city +10/6307 == city_idx consumes (type -> 0xff, moves
     * -> 0) + city_grow; else the new-city path (map claim via
     * map_get_tile/map_set_tile @ 11e8:0dcf/0e19 + 1420:5500 alloc +
     * Win16 dialog, UNSUPPORTED tail). */
    if (type != CIV_UNIT_TYPE_SETTLER)
        return 0;
    return (home == city_idx) ? 1 : 0;
}

int city_spot_valid(int valid)
{
    /* Orig City_SpotReveal @ 11c0:0000 gate in the Unit_UpdateVisibility
     * @ 11c0:0395 tail (via Map_FindUnitAt @ 1110:180e + Map_GetTileOwner
     * @ 1110:16ee; sibling City_UpdateVisibility @ 11c8:00c4 + City_FindBy
     * Tile @ 11c8:00c4 scan 0x80 0x1c records @ 1420:5500 the same way).
     * Seen-bit/diplomacy/minimap tails stay in city_spot_reveal (STUB). */
    return (((unsigned)valid & 0xffu) == 0xffu) ? 0 : 1;
}

int unit_popup_has_pollution(unsigned occ_flags)
{
    /* Orig Unit_ShowInfoPopup @ 11f0:468c occupancy tail
     * (Map_GetOccupancyFlags @ 1110:199b): 0x40 Pollution @ 1420:4845
     * line (each DrawDirect + h step; GDI tail stays in
     * unit_show_info_popup, STUB). Siblings: 0x10 Railroad @ 481d else
     * 0x08 @ 4828; 0x02 Irrigation @ 482f else 0x04 Mining @ 483c. */
    return (occ_flags & 0x40u) != 0u ? 1 : 0;
}

/* --- Packet 2026-10-16 (game-only): unit/city/demo/diplo OS-free kernels.
 * See game.h packet block. LIVE 1:1 on explicit params (no globals, no
 * Win16, no exit(), C17). Ref: unpacked/civ.exe read-only. */

int unit_wait_blink_phase(int tick)
{
    /* Orig Unit_WaitForOrders @ 11f0:0ec2 modal blink-and-pump: alternates
     * the unit sprite (1110:0448/0dba + 1210:07ae) vs "End of Turn/Press
     * Return" (10d8:00e0 + 10d0:0854) on a Sys_GetTickScaled % 0x3c
     * 0x1e-phase clock, pumping FUN_1108_0000 until 11e8:02fb input or
     * 1328:0012 mode change (modal tail stays STUB in
     * unit_wait_for_orders). */
    unsigned phase;

    if (tick < 0)
        return 0;
    phase = (unsigned)tick % 0x3cu;
    return (phase < 0x1eu) ? 0 : 1;
}

int unit_should_center_view(int civ, int player_civ, unsigned seen_mask)
{
    /* Orig Unit_MaybeCenterView @ 11f0:44d4: calls 1110:1461 (reveal/center
     * helper) only when the unit belongs to the player or is seen by the
     * player (6305 bit). Caller: Civ_ProcessUnitMoves @ 11f0:030c. */
    if (civ == player_civ)
        return 1;
    if (player_civ < 0 || player_civ > 31)
        return 0;
    return (seen_mask & (1u << ((unsigned)player_civ & 31u))) != 0u ? 1 : 0;
}

int unit_popup_has_railroad(unsigned occ_flags)
{
    /* Orig Unit_ShowInfoPopup @ 11f0:468c: 0x10 Railroad @ 1420:481d line
     * (Map_GetOccupancyFlags @ 1110:199b; "(RailRoad)" verified in
     * unpacked/civ.exe). Else-branch with 0x08 river stays caller-side. */
    return (occ_flags & 0x10u) != 0u ? 1 : 0;
}

int unit_popup_has_river(unsigned occ_flags)
{
    /* Orig same: 0x08 river? @ 1420:4828 line (drawn when 0x10 clear). */
    return (occ_flags & 0x08u) != 0u ? 1 : 0;
}

int unit_popup_has_irrigation(unsigned occ_flags)
{
    /* Orig same: 0x02 Irrigation @ 1420:482f line ("(Irrigation)" verified
     * in unpacked/civ.exe). Else-branch with 0x04 mining stays
     * caller-side. */
    return (occ_flags & 0x02u) != 0u ? 1 : 0;
}

int unit_popup_has_mine(unsigned occ_flags)
{
    /* Orig same: 0x04 Mining @ 1420:483c line ("(Mining)"/" with Mining)"
     * verified in unpacked/civ.exe; drawn when 0x02 clear). */
    return (occ_flags & 0x04u) != 0u ? 1 : 0;
}

int diplomat_steal_blocked(unsigned city_flags)
{
    /* Orig Diplomat_CityAction @ 11b8:0000 case 2 steal-tech: Rng_Range @
     * 11c8:003d 0x48 scan with the 5502 & 0x20 already-stolen gate (Res
     * 0x82 names via Res_GetStringCopy @ 11f8:0080); dialog tail stays
     * STUB in diplomat_city_action. */
    return (city_flags & 0x20u) != 0u ? 1 : 0;
}

int city_revolt_score(int v00de, int v00e0, int dist)
{
    /* Orig City_DoRevolt @ 11b8:0f26: param_2 == -1 auto-picks the best
     * foreign city by ((00de - 00e0) * 0x20 / dist) max; then seen-bit
     * flips at x*0x32+y+10 + 0x14 diamond neighbours, near-unit moves via
     * 1110:171f/1906 + 11c0:12c8, then Unit_DoMove(new_owner, city, 1) +
     * message 1420:4168/4182 + Dialog_ShowMessage @ 1220:00d2 (tails stay
     * STUB in city_do_revolt). */
    long score;

    if (dist <= 0)
        return -1;
    score = (long)(v00de - v00e0) * 0x20L;
    score /= (long)dist;
    if (score > 0x7fffffffL)
        return 0x7fffffff;
    if (score < (long)-0x80000000L)
        return -0x80000000;
    return (int)score;
}

int demo_pollution_index(int smoke, int pop)
{
    /* Orig Demo_ShowDemographics @ 1188:123a Pollution row: smoke totals
     * minus smoke*10/pop (live city-record sums per civ over the 0x80 0x1c
     * records @ 1420:5500, no LCG fake; GDI bar/text tail stays STUB in
     * demo_show_demographics via Demo_RankRow @ 1188:220a). */
    if (pop <= 0)
        return -1;
    return smoke - smoke * 10 / pop;
}

int unit_reveal_seen_or(int cur, int civ)
{
    /* Orig Unit_UpdateVisibility @ 11c0:0395 seen-byte OR (fog byte at
     * x*0x32+y+10 via unit_seen_index |= 1 << civ via unit_seen_bit; same
     * bytes City_DoRevolt @ 11b8:0f26 flips on revolt; minimap via
     * Map_SetTile(x+0xf0, 1/2) via unit_reveal_minimap_value). */
    int bit;

    if (civ < 0 || civ > 31)
        return -1;
    bit = unit_seen_bit(civ);
    if (bit < 0)
        return -1;
    return cur | bit;
}
