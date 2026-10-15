/* New-game wizard stubs. Step order mirrors NewGame_Wizard @ 10e8:0000. */
#include "newgame.h"

civ_error_t newgame_pick_difficulty(civ_difficulty_t *out)
{
    if (!out)
        return CIV_ERR_NOENT;
    *out = CIV_DIFF_CHIEFTAIN;
    /* TODO: Util_StrCopy(CIV_NEWGAME_TEXT_DIFFICULTY) + Dialog_MenuZ_Modal()
     * -> g_nDifficulty (12d8:42f6). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t newgame_pick_competition(int *civ_count_out)
{
    if (!civ_count_out)
        return CIV_ERR_NOENT;
    *civ_count_out = CIV_NEWGAME_MAX_CIVS;
    /* TODO: CIV_NEWGAME_TEXT_COMPETITION + counts -> g_nCivCount (12d8:3b9a),
     * mask g_nCivAliveMask (12d8:42f8). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t newgame_pick_tribe(const civ_newgame_settings_t *ctx, int *civ_out)
{
    if (!ctx || !civ_out)
        return CIV_ERR_NOENT;
    *civ_out = 0;
    /* TODO: CIV_NEWGAME_TEXT_TRIBE + _Custom + tribe list (FUN_11f8_0080
     * 0x94) -> g_nPlayerCiv (12d8:4300). */
    return CIV_ERR_UNSUPPORTED;
}

const char *tribe_get_name_list(int index)
{
    (void)index;
    /* TODO: Orig FUN_11f8_0080 @ 11f8:0080 (tribe-name table, param 0x94
     * at the newgame_pick_tribe call site; strings live in segment 1420
     * — dump pending, then return &table[index]). */
    return NULL;
}

civ_error_t newgame_customize_world(civ_newgame_settings_t *ctx)
{
    if (!ctx)
        return CIV_ERR_NOENT;
    /* TODO: steps 3/4 via Dialog 1220:16fd + Ctrl_* (1058) paint/input. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t newgame_finalize(const civ_newgame_settings_t *settings)
{
    if (!settings)
        return CIV_ERR_NOENT;
    /* TODO: clear units/cities/map, g_nYear=CIV_NEWGAME_START_YEAR,
     * Sound_PlayId(leader), Map_GenerateRandomOrLoad. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t newgame_wizard_run(civ_newgame_settings_t *out)
{
    civ_error_t err;

    if (!out)
        return CIV_ERR_NOENT;
    out->difficulty = CIV_DIFF_CHIEFTAIN;
    out->civ_count = CIV_NEWGAME_MAX_CIVS;
    out->player_civ = 0;
    out->custom_world = 0;
    err = newgame_pick_difficulty(&out->difficulty);
    if (err != CIV_OK)
        return err;
    err = newgame_pick_competition(&out->civ_count);
    if (err != CIV_OK)
        return err;
    err = newgame_pick_tribe(out, &out->player_civ);
    if (err != CIV_OK)
        return err;
    err = newgame_customize_world(out);
    if (err != CIV_OK)
        return err;
    return newgame_finalize(out);
}
