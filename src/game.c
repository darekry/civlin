/* Game state. Turn logic mirrors Game_TurnLoopTick @ 1088:0000.
 * Y1 boot loop + tick: game_tick is the TIMERPROC @ 1008:0d68 body pumped
 * from while(!g_nQuitFlag) in WinMain @ 1008:0000; SDL pump via
 * Sys_PumpMessages @ 1108:0000 + Sys_Yield @ 1098:0157 (see sys.h). */
#include "game.h"

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
    /* Orig TIMERPROC @ 1008:0d68 body, pumped from the while(!g_nQuitFlag)
     * loop in WinMain @ 1008:0000 (200 ms SETTIMER installed by WdwMap_Open
     * @ 1208:0af1, KILLTIMER first in WinMain_Shutdown @ 1008:0329). Tick
     * -> game_turn_tick @ 1088:0000 linkage below: per-frame SDL pump is
     * live, turn pass stays stubbed -> overall CIV_ERR_UNSUPPORTED. No
     * globals, no exit(), read-only (pump only). */
    if (g->quit_flag)
        return CIV_OK;
    /* Live pump: Sys_PumpMessages @ 1108:0000 (SDL_PumpEvents, 1 = message,
     * 0 = empty) + Sys_Yield @ 1098:0157 (Anim pump @ 10b0:0000). */
    (void)sys_pump_messages();
    sys_yield();
    /* TODO: TIMERPROC body -> Game_TurnLoopTick @ 1088:0000
     * (game_turn_tick below; Civ_ProcessUnitMoves @ 11f0:0000, year steps,
     * scoring every 0x32 via game_calc_progress_threshold, HoF endings).
     * Exercised for linkage; result ignored until the per-civ loop lands. */
    (void)game_turn_tick(g);
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

civ_error_t game_append_year_text(void)
{
    /* TODO: FUN_1088_1dd0 @ 1088:1dd0 (abs(year) + BC/AD onto 12d8:24fa). */
    return CIV_ERR_UNSUPPORTED;
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
