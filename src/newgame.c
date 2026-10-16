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
     * Count 2..6 -> 0x0f,0x1f,0x3f,0x7f,0xff; out-of-range -> -1. */
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

/* --- Chain 2026-10-16 (24): wizard steps 0-5 + year (goal 3). ---
 * See newgame.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()).
 * Dialog/ResMgr/RNG-handle tails stay in the pick/customize STUBs. */

int newgame_difficulty_from_index(int modal_index)
{
    /* Orig NewGame_Wizard @ 10e8:0000 step 0 -> case 1: g_nDifficulty @
     * 12d8:42f6 = local_c (modal 0..4 Chieftain..Emperor, text 1420:2a5a). */
    if (modal_index < 0 || modal_index > 4)
        return -1;
    return modal_index;
}

int newgame_competition_resolve(int modal_index)
{
    /* Orig step 1 -> case 2: g_nCivCount @ 12d8:3b9a = Util_Clamp @ 11c8:0063
     * (6 - local_c, 2, 6); texts 1420:2aa9/2ac1/2ac3. Clamp, never -1. */
    int v = CIV_NEWGAME_MAX_CIVS - modal_index;

    if (v < CIV_NEWGAME_MIN_CIVS)
        v = CIV_NEWGAME_MIN_CIVS;
    if (v > CIV_NEWGAME_MAX_CIVS)
        v = CIV_NEWGAME_MAX_CIVS;
    return v;
}

int newgame_player_civ_resolve(int pick, int civ_count, int rng_draw,
                               int *out_slot)
{
    /* Orig step 2 -> case 3: local_c == 0 (Custom) rolls Rng_Range @ 11c8:003d
     * (g_nCivCount) + 1; else direct pick with the count wrap + slot at
     * 43b8+0x9c (Res_GetStringCopy @ 11f8:0080 blob 0x94 tribe strings +
     * 0x8c leader / 0x95 title stay caller-side). */
    int p;
    int player;
    int slot;

    if (civ_count < CIV_NEWGAME_MIN_CIVS ||
        civ_count > CIV_NEWGAME_MAX_CIVS)
        return -1;
    if (pick < 0)
        return -1;
    if (pick == 0) {
        if (rng_draw < 0 || rng_draw >= civ_count)
            return -1;
        player = rng_draw + 1;
        slot = player;
    } else {
        p = pick;
        if (civ_count < pick - 1)
            p = (pick - 1 - civ_count) + 8;
        player = p & 7;
        slot = player + ((p > 7) ? 8 : 0);
    }
    if (out_slot)
        *out_slot = slot;
    return player;
}

unsigned newgame_start_year_raw(void)
{
    /* Orig finalize: g_nYear @ 12d8:42fa = 0xF060. */
    return CIV_NEWGAME_START_YEAR;
}

int newgame_start_year_signed(void)
{
    /* Orig same word as int16: 0xF060 = -4000 = 4000 BC. */
    return (int)(int16_t)CIV_NEWGAME_START_YEAR;
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

/* --- Chain 2026-10-16 (25): CustomWorldModal validation (goal 1). ---
 * See newgame.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()).
 * CREATEWINDOW/PEEKMESSAGE pump stays in newgame_custom_world_modal STUB. */

int newgame_custom_result_is_back(int modal_ret)
{
    /* Orig NewGame_Wizard @ 10e8:0000 steps 3/4 wrapper: local_c =
     * NewGame_CustomWorldModal @ 1220:16fd (...); if (local_c == 0xffff)
     * DAT_1418_0004 = 1 else 0. */
    return (modal_ret == 0xffff) ? 1 : 0;
}

int newgame_wizard_step_back(int step, int back)
{
    /* Orig shared stepper: if (DAT_1418_0004 == 0) iVar8+1 else { if 0 <
     * iVar8 iVar8-1; if iVar8 == 3 iVar8 = 2 } (3->2 collapse). */
    if (step < 0 || step > 5)
        return -1;
    if (back != 0 && back != 1)
        return -1;
    if (back == 0)
        return step + 1;
    if (step > 0)
        step--;
    if (step == 3)
        step = 2;
    return step;
}

civ_error_t newgame_custom_settings_valid(const int *settings, int n)
{
    /* Orig WorldGen_LoadEarth @ 1108:0243 shape: 4 entries (landmass Mass?
     * /temperature/climate/age hypothesis) preset to 1, each choice col
     * 0..2 (help col 3 never stored, see worldgen_custom_apply LIVE). */
    int i;

    if (!settings)
        return CIV_ERR_NOENT;
    if (n != 4)
        return CIV_ERR_RANGE;
    for (i = 0; i < n; i++) {
        if (settings[i] < 0 || settings[i] > 2)
            return CIV_ERR_RANGE;
    }
    return CIV_OK;
}

/* --- Chain 2026-10-16 (26): custom help texts + font probe (goal 4). ---
 * See newgame.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()).
 * Dlg_CreateTracked/PEEKMESSAGE tails stay STUB below. */

int newgame_custom_help_for_row(int row)
{
    /* Orig WorldGen_LoadEarth @ 1108:0243 help switch: case 0 -> 0x2e23,
     * case 1 -> Temperature @ 1420:2e28, case 2 -> Climate @ 1420:2e34,
     * case 3 -> 0x2e3c (col == 3 help via NewGame_ShowCustomHelp @
     * 10e0:0000). */
    switch (row) {
    case 0:
        return CIV_HELP_ROW_LANDMASS;
    case 1:
        return CIV_HELP_ROW_TEMPERATURE;
    case 2:
        return CIV_HELP_ROW_CLIMATE;
    case 3:
        return CIV_HELP_ROW_AGE;
    default:
        return -1;
    }
}

int newgame_show_custom_help_param(void)
{
    /* Orig NewGame_ShowCustomHelp @ 10e0:0000 head: NewGame_CustomHelpDialog
     * (0x2994, ...) first param. */
    return CIV_HELP_SHOW_PARAM;
}

int newgame_show_custom_help_next(void)
{
    /* Orig same tail: DAT_1418_0004 = 0 (forward; back handled by
     * newgame_custom_result_is_back / newgame_wizard_step_back). */
    return 0;
}

int newgame_civ_note_early_needed(int opt_flags, int year)
{
    /* Orig NewGame_ShowCivNoteEarly @ 10e0:077f: if (((g_nOptionFlags & 1)
     * != 0) && (g_nYear < 0)) HelpDialog(0x29c9, 0x29cd note). */
    if (((opt_flags & 1) != 0) && (year < 0))
        return 1;
    return 0;
}

int newgame_civ_note_player_needed(int tag_13f0_000c, int player_civ)
{
    /* Orig NewGame_ShowCivNoteForPlayer @ 10e0:07e8: if (DAT_13f0_000c ==
     * g_nPlayerCiv) HelpDialog(0x29e5, 0x29e9 note). */
    return (tag_13f0_000c == player_civ) ? 1 : 0;
}

civ_error_t newgame_show_custom_help(int row)
{
    /* Orig NewGame_ShowCustomHelp @ 10e0:0000 wrapper validated here; body
     * needs NewGame_CustomHelpDialog @ 10e0:03a2 Win16 below. */
    if (newgame_custom_help_for_row(row) < 0)
        return CIV_ERR_RANGE;
    /* TODO: NewGame_CustomHelpDialog(0x2994, buf, help row string @
     * 0x2e23/2e28/2e34/2e3c, colors 0xd8/0xe + DAT_12d8_45c0/45be/45cd) +
     * DAT_1418_0004 = 0 via Dlg_CreateTracked @ 1098:124f 0x87. Win16 -> SDL. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t newgame_custom_help_dialog(int help_id)
{
    (void)help_id;
    /* TODO: Orig NewGame_CustomHelpDialog @ 10e0:03a2 body: Dlg_CreateTracked
     * @ 1098:124f (0x87) + FUN_11d0_00e4/022d + FUN_10e0_06fd +
     * Dialog_GetItemRect @ 1100:03ff (0x66/0x67) + Gfx/Text layout +
     * PEEKMESSAGE until DAT_12d8_0030 (+ CivNoteEarly 0x29c9 / CivNotePlayer
     * 0x29e5 callers). Win16 USER/GDI -> SDL3 TBD. */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (30): NewGame_RateStartPositions kernels (task goal 3) ---
 * See newgame.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()). */

int newgame_start_dist_score(int dist)
{
    /* Orig NewGame_RateStartPositions @ 10e8:1d48 dist-base: <10 close 0,
     * <0x14 moderate 1, ==999 isolated 4, else far 2. */
    if (dist == 999)
        return 4;
    if (dist < 10)
        return 0;
    if (dist < 0x14)
        return 1;
    return 2;
}

int newgame_start_terrain_bonus(int river_cnt, int grass_cnt)
{
    /* Orig NewGame_RateStartPositions @ 10e8:1d48 terrain: river(0xb) >= 2
     * -> +2 river valley; else grass(2) >= 3 -> +1 grasslands; else +0. */
    if (river_cnt >= 2)
        return 2;
    if (grass_cnt >= 3)
        return 1;
    return 0;
}

int newgame_start_continent_bonus(int ocean_val)
{
    /* Orig NewGame_RateStartPositions @ 10e8:212a switch (val/0x32): 0/1 -> +0
     * (small/large island), 2/3 -> +1 (small/medium continent), else +2. */
    int q;

    if (ocean_val < 0)
        return -1;
    q = ocean_val / 0x32;
    if (q <= 1)
        return 0;
    if (q <= 3)
        return 1;
    return 2;
}

int newgame_start_needs_extra_settler(int best, int score)
{
    /* Orig NewGame_RateStartPositions @ 10e8:1d48 settler-gap: 3 < best-score
     * -> Unit_SpawnUnit extra settler (unit 0). */
    return ((best - score) > 3) ? 1 : 0;
}

int newgame_start_should_pick(int score, int best, int is_first, int is_isolated)
{
    /* Orig NewGame_RateStartPositions @ 10e8:1d48 best-tie: score == best &&
     * (local_1a == -1 || isolated-bit) -> pick (izolowane wygrywaja remisy). */
    if (score != best)
        return 0;
    if (is_first != 0)
        return 1;
    return (is_isolated != 0) ? 1 : 0;
}

/* --- Chain 2026-10-16 (p5): CivNote res ids + tresc (goal 4) ---
 * See newgame.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()). */

int newgame_civ_note_early_res(void)
{
    /* Orig NewGame_ShowCivNoteEarly @ 10e0:077f HelpDialog id. */
    return CIV_HELP_NOTE_EARLY;
}

int newgame_civ_note_player_res(void)
{
    /* Orig NewGame_ShowCivNoteForPlayer @ 10e0:07e8 HelpDialog id. */
    return CIV_HELP_NOTE_PLAYER;
}

const char *newgame_civ_note_title(void)
{
    /* Orig tytuly @ 1420:29cd/1420:29e9 (oba "** Civilization Note **"). */
    return "** Civilization Note **";
}
