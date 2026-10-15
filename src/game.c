/* Game state. Turn logic mirrors Game_TurnLoopTick @ 1088:0000. */
#include "game.h"

civ_error_t game_init(civ_game_t *g)
{
    if (!g)
        return CIV_ERR_NOENT;
    g->initialized = 0;
    g->quit_flag = 0;
    g->turn = 0;
    g->difficulty = 0;
    g->player_civ = 0;
    g->alive_mask = 0;
    g->year = 0;
    g->menu_choice = -1;
    g->civ_count = 0;
    /* TODO: DGROUP 1420 + segment 12d8 layout mapping. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t game_new(civ_game_t *g)
{
    (void)g;
    /* TODO: NewGame_Wizard (newgame.h) -> worldgen -> state fill. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t game_tick(civ_game_t *g)
{
    (void)g;
    /* TODO: per-frame pump (message queue + TIMERPROC @ 1008:0d68). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t game_turn_tick(civ_game_t *g)
{
    if (!g)
        return CIV_ERR_NOENT;
    /* TODO: Game_TurnLoopTick @ 1088:0000 (civ iteration via
     * Civ_ProcessUnitMoves @ 11f0:0000, year steps, scoring branch every
     * 0x32 turns via Game_CalcProgressThreshold @ 1088:1d99
     * (game_calc_progress_threshold, implemented: sum i*(7-diff)/2+1),
     * HoF endings setting quit_flag). Scoring branch covered here once
     * the per-civ loop lands; threshold math itself is done. */
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

civ_error_t game_append_year_text(void)
{
    /* TODO: FUN_1088_1dd0 @ 1088:1dd0 (abs(year) + BC/AD onto 12d8:24fa). */
    return CIV_ERR_UNSUPPORTED;
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
    /* TODO: FUN_11f0_4ea0 @ 11f0:4ea0 (clear 1d44.. + PEEKMESSAGE drain). */
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
    /* TODO: Orig Combat_ResolveAttack @ TBD (near unit_choose_order @
     * 1078:1480 combat-odds tail with 11c0:1bc3; exact seg:off TBD).
     * Decode atk/def * terrain * veteran multipliers, then scale via
     * combat_calc_odds @ 11c0:1bc3 (atk*100/(atk+def) v1). */
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

civ_error_t unit_do_found_city(int civ, int unit_idx)
{
    (void)civ;
    (void)unit_idx;
    /* TODO: Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x62 case
     * (jump tables 1420:43b8+0x8f x0x0e, +0x4b x0x11, 1420:435b+0x28
     * x0x20). Found-city/irrigate?: type 0x1a settler founds via
     * Unit_FoundCityIfHome @ 11c0:16a4 (unit_found_city_if_home) when
     * home (+10/6307); needs 1420:5500 city records + map tile claim. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t unit_do_fortify(int civ, int unit_idx)
{
    (void)civ;
    (void)unit_idx;
    /* TODO: Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x66 case
     * (jump tables 1420:43b8+0x8f x0x0e, +0x4b x0x11, 1420:435b+0x28
     * x0x20). Wait-fortify?: set fortified flag on the 1420:62fc record
     * so the unit holds; needs flag-bit decode + status redraw. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t unit_do_sentry(int civ, int unit_idx)
{
    (void)civ;
    (void)unit_idx;
    /* TODO: Orig Civ_ProcessUnitMoves @ 11f0:0000 order 0x68 case
     * (jump tables 1420:43b8+0x8f x0x0e, +0x4b x0x11, 1420:435b+0x28
     * x0x20). Sentry/1-move?: park until unit_update_visibility @
     * 11c0:0395 contact or one move elapses; exact wake TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t unit_do_move_attack(int civ, int unit_idx, int order)
{
    (void)civ;
    (void)unit_idx;
    (void)order;
    /* TODO: Orig Civ_ProcessUnitMoves @ 11f0:0000 orders
     * 0x69/0x6d/0x6f/0x70/0x72/0x75 cases (opaque keys into the three
     * jump tables 1420:43b8+0x8f x0x0e, +0x4b x0x11, 1420:435b+0x28
     * x0x20; 0x6d road?, 0x70 pollution? per frontier). Steps via
     * unit_step_toward_goal @ 1128:0000, odds via Combat_ResolveAttack +
     * 11c0:1bc3; needs map/combat state. */
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

void game_shutdown(civ_game_t *g)
{
    if (g)
        g->initialized = 0;
}
