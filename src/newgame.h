/* New-game wizard (difficulty / civ count / tribe / custom world).
 *
 * Ghidra anchors (civ.exe):
 *   NewGame_Wizard @ 10e8:0000 (PUSH 0x2a5a @ 10e8:01c9 proves the site;
 *     caller: Main_RunStartupSequence @ 11e0:0000)
 *   Dialog_MenuZ_Modal @ 1220:0156 (each step) / @ 1220:16fd (custom world)
 *   Ctrl_* custom controls @ 1058:xxxx (RANDOMRADIOPROC 06ab,
 *     RANDOMSTATICPROC 081a, RANDOMUSERPROC 0921, REGIONPROC 09f6)
 *   Sound_PlayId (leader wav for the picked civ at wizard end)
 *   Map_GenerateRandomOrLoad @ 1028:0000 (map comes next)
 *
 * Strings @ 1420 (dump-confirmed 2026-10-15):
 *   0x2a5a "Difficulty Level..._Chieftain (easiest)_Warlord_Prince_King_"
 *          "Emperor (toughest)"
 *   0x2aa9 "Level of Competition..." (+ 0x2ac1 "_"/0x2ac3 " Civilizations")
 *   0x2ad2 "Pick your tribe..." (+ 0x2ae5 "_Custom.")
 * Game state written (segment 0x12d8 globals):
 *   g_nDifficulty (42f6) 0=Chieftain..4=Emperor
 *   g_nCivCount   (3b9a) 2..6 (from 6 - modal index)
 *   g_nCivAliveMask (42f8) = 0xff >> (6 - count)
 *   g_nPlayerCiv  (4300); g_nYear (42fa)=0xF060 at finalize
 */
#ifndef CIVLIN_NEWGAME_H
#define CIVLIN_NEWGAME_H

#include "civ_common.h"

typedef enum civ_difficulty {
    CIV_DIFF_CHIEFTAIN = 0,
    CIV_DIFF_WARLORD = 1,
    CIV_DIFF_PRINCE = 2,
    CIV_DIFF_KING = 3,
    CIV_DIFF_EMPEROR = 4
} civ_difficulty_t;

#define CIV_NEWGAME_TEXT_DIFFICULTY \
    "Difficulty Level..._Chieftain (easiest)_Warlord_Prince_King_Emperor (toughest)"
#define CIV_NEWGAME_TEXT_COMPETITION "Level of Competition..."
#define CIV_NEWGAME_TEXT_CIVS " Civilizations"
#define CIV_NEWGAME_TEXT_TRIBE "Pick your tribe..."
#define CIV_NEWGAME_TEXT_CUSTOM "_Custom."

#define CIV_NEWGAME_MIN_CIVS 2
#define CIV_NEWGAME_MAX_CIVS 6
#define CIV_NEWGAME_START_YEAR 0xf060u

typedef struct civ_newgame_settings {
    civ_difficulty_t difficulty;
    int civ_count;   /* CIV_NEWGAME_MIN_CIVS..CIV_NEWGAME_MAX_CIVS */
    int player_civ;  /* 0..7 index; 0 from random path means "roll" */
    int custom_world;/* 0 = random map, 1 = customized via 1058 controls */
} civ_newgame_settings_t;

/* Runs steps 0..5 of NewGame_Wizard (back/forward via cancel semantics). */
civ_error_t newgame_wizard_run(civ_newgame_settings_t *out);

/* Single steps (each = text build + Dialog_MenuZ_Modal). */
civ_error_t newgame_pick_difficulty(civ_difficulty_t *out);
civ_error_t newgame_pick_competition(int *civ_count_out);
civ_error_t newgame_pick_tribe(const civ_newgame_settings_t *ctx, int *civ_out);
civ_error_t newgame_customize_world(civ_newgame_settings_t *ctx);
civ_error_t newgame_finalize(const civ_newgame_settings_t *settings);

#endif /* CIVLIN_NEWGAME_H */
