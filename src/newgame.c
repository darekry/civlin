/* New-game wizard stubs. Step order mirrors NewGame_Wizard @ 10e8:0000
 * (caller: Main_RunStartupSequence @ 11e0:0000).
 *
 * OS-free math (mask/year/count) is ported 1:1 as pure helpers; every
 * Win16 dialog/sound/map tail stays a CIV_ERR_UNSUPPORTED stub. No
 * globals, no exit(), read-only (only caller-supplied out-params are
 * written; game assets are never modified).
 */
#include "newgame.h"

/* Orig: NewGame_Wizard @ 10e8:0000 step 0 -> newgame_pick_difficulty.
 * Text 1420:2a5a via Dialog_MenuZ_Modal @ 1220:0156 (Orig FUN_1220_0156)
 * -> g_nDifficulty @ 12d8:42f6 (0=Chieftain..4=Emperor). */
civ_error_t newgame_pick_difficulty(civ_difficulty_t *out)
{
    if (!out)
        return CIV_ERR_NOENT;
    *out = CIV_DIFF_CHIEFTAIN;
    /* TODO: Util_StrCopy(CIV_NEWGAME_TEXT_DIFFICULTY) + Dialog_MenuZ_Modal
     * @ 1220:0156 -> g_nDifficulty (12d8:42f6). */
    return CIV_ERR_UNSUPPORTED;
}

/* Orig: NewGame_Wizard @ 10e8:0000 step 1 -> newgame_pick_competition.
 * Text 1420:2aa9 + 1420:2ac1/2ac3 via Dialog_MenuZ_Modal @ 1220:0156
 * (Orig FUN_1220_0156); count = 6 - modal index -> g_nCivCount @ 12d8:3b9a
 * (2..6). See newgame_civ_count_from_index for the OS-free mapping. */
civ_error_t newgame_pick_competition(int *civ_count_out)
{
    if (!civ_count_out)
        return CIV_ERR_NOENT;
    *civ_count_out = CIV_NEWGAME_MAX_CIVS;
    /* TODO: CIV_NEWGAME_TEXT_COMPETITION + counts -> g_nCivCount (12d8:3b9a),
     * mask g_nCivAliveMask (12d8:42f8) derived at finalize. */
    return CIV_ERR_UNSUPPORTED;
}

int newgame_civ_count_from_index(int modal_index)
{
    /* Orig sub-expression: g_nCivCount @ 12d8:3b9a = 6 - modal index.
     * Modal 0..4 -> count 6..2; out-of-range -> -1. */
    if (modal_index < 0 || modal_index > 4)
        return -1;
    return CIV_NEWGAME_MAX_CIVS - modal_index;
}

int newgame_alive_mask(int civ_count)
{
    /* Orig sub-expression: g_nCivAliveMask @ 12d8:42f8 = 0xff >> (6-count).
     * Count 2..6 -> 0x03,0x07,0x0f,0x3f,0xff; out-of-range -> -1. */
    if (civ_count < CIV_NEWGAME_MIN_CIVS || civ_count > CIV_NEWGAME_MAX_CIVS)
        return -1;
    return 0xff >> (CIV_NEWGAME_MAX_CIVS - civ_count);
}

/* Orig: NewGame_Wizard @ 10e8:0000 step 2 -> newgame_pick_tribe.
 * Text 1420:2ad2 + 1420:2ae5 plus Tribe_GetNameList @ 11f8:0080 (Orig
 * FUN_11f8_0080, param 0x94) via Dialog_MenuZ_Modal @ 1220:0156 (Orig
 * FUN_1220_0156) -> g_nPlayerCiv (12d8:4300). */
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
    /* Orig: FUN_11f8_0080 @ 11f8:0080 -> Tribe_GetNameList. Packet I:
     * seg 1420 dump has no decoded tribe-name block yet (FIRSTMOVE /
     * PILLAGE / font-map tables also pending via boot_init_unit_font_
     * tables @ 10e8:2d46), and the 0x94 call-site param meaning is still
     * TBD (string-table id vs MenuZ buffer length), so no 1:1 table port
     * is possible. Keeps the NULL stub (also NULL for out-of-range
     * index); doc extended in newgame.h without breaking this stub.
     * Read-only; no globals; no exit(). */
    return NULL;
}

/* Orig: NewGame_Wizard @ 10e8:0000 steps 3/4 -> newgame_customize_world.
 * Custom path via Dialog_MenuZ_Modal variant @ 1220:16fd (Orig
 * FUN_1220_16fd) + Ctrl_* @ 1058 (RANDOMRADIOPROC @ 1058:06ab,
 * RANDOMSTATICPROC @ 1058:081a, RANDOMUSERPROC @ 1058:0921, REGIONPROC
 * @ 1058:09f6); feeds WorldGen_CustomizeDialog @ 1060:xxxx (Orig
 * FUN_1060_xxxx). */
civ_error_t newgame_customize_world(civ_newgame_settings_t *ctx)
{
    if (!ctx)
        return CIV_ERR_NOENT;
    /* TODO: steps 3/4 via Dialog 1220:16fd + Ctrl_* (1058) paint/input,
     * then WorldGen_CustomizeDialog @ 1060. */
    return CIV_ERR_UNSUPPORTED;
}

/* Orig: FUN_1220_16fd @ 1220:16fd -> newgame_custom_world_modal.
 * Separate entry from the base Dialog_MenuZ_Modal @ 1220:0156 (Orig
 * FUN_1220_0156); hosts the Ctrl_* @ 1058 fan-out for the editor. */
civ_error_t newgame_custom_world_modal(const civ_newgame_settings_t *ctx,
                                       int *out_action)
{
    if (!ctx || !out_action)
        return CIV_ERR_NOENT;
    *out_action = 0;
    /* TODO: custom modal @ 1220:16fd + Ctrl_* @ 1058:06ab/081a/0921/09f6
     * paint/input -> WorldGen_CustomizeDialog @ 1060:xxxx. Base modal
     * Dialog_MenuZ_Modal @ 1220:0156 lives in dialog.h (dialog_menuz);
     * this variant is intentionally separate. */
    return CIV_ERR_UNSUPPORTED;
}

/* Orig: NewGame_Wizard @ 10e8:0000 step 5 (finalize tail) -> newgame_finalize.
 * Writes g_nDifficulty @ 12d8:42f6, g_nCivCount @ 12d8:3b9a,
 * g_nCivAliveMask @ 12d8:42f8, g_nPlayerCiv @ 12d8:4300, g_nYear @ 12d8:42fa
 * (= 0xF060); then Sound_PlayId @ 1150:00c9 + Map_GenerateRandomOrLoad
 * @ 1028:0000. */
civ_error_t newgame_finalize(const civ_newgame_settings_t *settings)
{
    int alive_mask;
    unsigned year;

    if (!settings)
        return CIV_ERR_NOENT;
    if (settings->difficulty < CIV_DIFF_CHIEFTAIN ||
        settings->difficulty > CIV_DIFF_EMPEROR)
        return CIV_ERR_RANGE;
    if (settings->civ_count < CIV_NEWGAME_MIN_CIVS ||
        settings->civ_count > CIV_NEWGAME_MAX_CIVS)
        return CIV_ERR_RANGE;
    /* OS-free 1:1 port of the orig tail math (no Win16, no globals):
     * alive_mask = 0xff >> (6 - count), year = 0xF060. */
    alive_mask = newgame_alive_mask(settings->civ_count);
    if (alive_mask < 0)
        return CIV_ERR_RANGE;
    year = CIV_NEWGAME_START_YEAR; /* 0xF060 -> g_nYear @ 12d8:42fa */
    (void)alive_mask;
    (void)year;
    /* TODO (stubbed tails, Win16 -> SDL3 TBD): write g_nDifficulty (42f6),
     * g_nCivCount (3b9a), g_nCivAliveMask (42f8), g_nPlayerCiv (4300),
     * g_nYear (42fa)=0xF060; Sound_PlayId @ 1150:00c9 (leader wav);
     * Map_GenerateRandomOrLoad @ 1028:0000 next. */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (19): Menu_PromptNation discriminators (goal 4) ---
 * See newgame.h block. OS-free 1:1 (no Win16, no globals, no exit()). */

int menu_prompt_nation_needed(int flags_00b6)
{
    /* Orig Menu_PromptNation @ 1070:1f58 gate:
     * if ((DAT_12d0_00b6 & 0xfe00) != 0) (launch-bits; 0 = brak ship
     * do podgladu). */
    return ((flags_00b6 & 0xfe00) != 0) ? 1 : 0;
}

int menu_prompt_nation_count(void)
{
    /* Orig same: for (iVar1 = 1; iVar1 < 8; iVar1++) 7 wpisow z
     * 12d8:03da+i*0x20 (runtime tribe table via Tribe_GetNameList @
     * 11f8:0080 / Res blob 0x94). */
    return 7;
}

int menu_prompt_nation_avail_mask(int flags_00b6)
{
    /* Orig same: DAT_12d8_24da = (int)DAT_12d0_00b6 >> 9 ^ 0xff. */
    return ((flags_00b6 >> 9) ^ 0xff) & 0xff;
}

int menu_prompt_nation_choice_valid(int ret_plus1)
{
    /* Orig same: iVar1 = Dialog_ShowMessage @ 1220:00d2 + 1;
     * if (0 < iVar1) { DAT_1290_0114 = iVar1; Ship_* } . */
    return (ret_plus1 > 0) ? 1 : 0;
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
