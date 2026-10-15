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

void game_shutdown(civ_game_t *g)
{
    if (g)
        g->initialized = 0;
}
