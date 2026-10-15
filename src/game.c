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
    /* TODO: Game_TurnLoopTick @ 1088:0000 (civ iteration, year steps,
     * 0x32-turn scoring via 11c0:3beb, HoF endings setting quit_flag). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t game_process_game_end(int hof_slot)
{
    (void)hof_slot;
    /* TODO: Game_ProcessGameEnd @ 1088:0b6d (1188:07ad + 1180:0000 unless
     * quit==2, 1040:1137 score, HoF_VictoryCeremony/Load/TryInsert/Save). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t game_spawn_uprisings(void)
{
    /* TODO: Game_SpawnUprisings @ 1088:0be9 (difficulty-gated random tiles
     * via 1110:16c0/18ce/1dbd, units via 11c0:12c8, Travellers/Barbarian
     * /Native/Guerrilla messages). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t game_update_power_rankings(void)
{
    /* TODO: Game_UpdatePowerRankings @ 1088:108a (scores = cities*8 +
     * territory + treasury/0x20 + tech weights; rank table 0x3e84+0x1a;
     * 12a8:000c rebellion flag). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t game_update_status_panel(void)
{
    /* TODO: Game_UpdateStatusPanel @ 1088:13d5 (GfxPort blits, 1218:0956 /
     * 0a06 frames, 11f8:0278 text, 1088:1a82/1dd0 rows, TEXTOUT x3,
     * population milestone popup via 1220:00d2). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t game_do_pollution_and_wonder_race(void)
{
    /* TODO: Game_DoPollutionAndWonderRace @ 1088:1e32 (12d8:42f0 pollution
     * counter + Map_SpreadPollution, wonder pick via 1170:3181/tech tables,
     * "... builds ..." message, shield deduction). */
    return CIV_ERR_UNSUPPORTED;
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

void game_shutdown(civ_game_t *g)
{
    if (g)
        g->initialized = 0;
}
