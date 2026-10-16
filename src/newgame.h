/* New-game wizard (difficulty / civ count / tribe / custom world).
 *
 * Ghidra anchors (civ.exe):
 *   NewGame_Wizard @ 10e8:0000 steps 0..5 (PUSH 0x2a5a @ 10e8:01c9 proves
 *     the site; caller: Main_RunStartupSequence @ 11e0:0000)
 *   Dialog_MenuZ_Modal base @ 1220:0156 (each wizard step; see dialog.h
 *     dialog_menuz) vs custom-world variant @ 1220:16fd (see
 *     newgame_custom_world_modal below)
 *   Tribe_GetNameList @ 11f8:0080 (Orig FUN_11f8_0080; param 0x94 at the
 *     newgame_pick_tribe call site)
 *   Ctrl_* custom controls @ 1058:xxxx (RANDOMRADIOPROC @ 1058:06ab,
 *     RANDOMSTATICPROC @ 1058:081a, RANDOMUSERPROC @ 1058:0921,
 *     REGIONPROC @ 1058:09f6; registered by Ctrl_RegisterCustomClasses
 *     @ 1058:0b4d, see dialog.h)
 *   WorldGen_CustomizeDialog @ 1060:xxxx (Orig FUN_1060_xxxx; custom editor
 *     host fed by newgame_customize_world; see worldgen.h — referenced by
 *     Orig address only, not redeclared here)
 *   Sound_PlayId @ 1150:00c9 (leader wav for the picked civ at wizard end;
 *     see audio.h — referenced by Orig address only)
 *   Map_GenerateRandomOrLoad @ 1028:0000 (map comes next after finalize;
 *     see worldgen.h — referenced by Orig address only)
 *
 * Strings @ 1420 (dump-confirmed 2026-10-15):
 *   0x2a5a "Difficulty Level..._Chieftain (easiest)_Warlord_Prince_King_"
 *          "Emperor (toughest)"
 *   0x2aa9 "Level of Competition..." (+ 0x2ac1 "_"/0x2ac3 " Civilizations")
 *   0x2ad2 "Pick your tribe..." (+ 0x2ae5 "_Custom.")
 * Game state written (segment 0x12d8 globals, orig finalize tail):
 *   g_nDifficulty (42f6) 0=Chieftain..4=Emperor
 *   g_nCivCount   (3b9a) 2..6 (from 6 - modal index)
 *   g_nCivAliveMask (42f8) = 0xff >> (6 - count)
 *   g_nPlayerCiv  (4300); g_nYear (42fa)=0xF060 at finalize
 *
 * Rules: C17, no exit(), no globals, read-only vs civ1win/unpacked.
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

/* Runs steps 0..5 of NewGame_Wizard (back/forward via cancel semantics).
 * Orig: NewGame_Wizard @ 10e8:0000 -> newgame_wizard_run.
 * Caller: Main_RunStartupSequence @ 11e0:0000. */
civ_error_t newgame_wizard_run(civ_newgame_settings_t *out);

/* Orig: NewGame_Wizard @ 10e8:0000 step 0 -> newgame_pick_difficulty.
 * Text 1420:2a5a ("Difficulty Level..._Chieftain (easiest)_Warlord_Prince_
 * King_Emperor (toughest)") via Dialog_MenuZ_Modal @ 1220:0156 (Orig
 * FUN_1220_0156); modal index 0..4 maps 1:1 to Chieftain..Emperor and is
 * stored to g_nDifficulty @ 12d8:42f6. Win16 dialog tail TBD; stub
 * returns CIV_ERR_UNSUPPORTED (NULL out -> CIV_ERR_NOENT). */
civ_error_t newgame_pick_difficulty(civ_difficulty_t *out);

/* Orig: NewGame_Wizard @ 10e8:0000 step 1 -> newgame_pick_competition.
 * Text 1420:2aa9 ("Level of Competition...") + count suffix built from
 * 1420:2ac1 ("_") / 1420:2ac3 (" Civilizations") via Dialog_MenuZ_Modal
 * @ 1220:0156 (Orig FUN_1220_0156); civ count = 6 - modal index, range
 * 2..6, stored to g_nCivCount @ 12d8:3b9a (alive mask @ 12d8:42f8 is
 * derived at finalize). Win16 dialog tail TBD; stub returns
 * CIV_ERR_UNSUPPORTED (NULL out -> CIV_ERR_NOENT). */
civ_error_t newgame_pick_competition(int *civ_count_out);

/* OS-free sub-expression of step 1 / finalize (no Win16, no globals).
 * Modal index 0..4 -> civ count 6..2 (6 - index); out-of-range -> -1.
 * Read-only pure helper; mirrors the "6 - modal index" assignment to
 * g_nCivCount @ 12d8:3b9a. */
int newgame_civ_count_from_index(int modal_index);

/* OS-free sub-expression of the finalize tail (no Win16, no globals).
 * alive_mask = 0xff >> (6 - count); count 2..6 -> 0x03..0xff.
 * Out-of-range count -> -1. Read-only pure helper; mirrors the write
 * to g_nCivAliveMask @ 12d8:42f8. */
int newgame_alive_mask(int civ_count);

/* Orig: FUN_11f8_0080 @ 11f8:0080 -> Tribe_GetNameList.
 * CHAIN ANSWER (2026-10-16): YES — this IS the ResMgr blob chain.
 * The 0x94 call-site param is the BLOB 0x2da8 entry group
 * (Res_GetStringCopy(0x94, i) + Res_GetStringCopy(0x94, i+8) two-column
 * tribe list in NewGame_Wizard @ 10e8:0000 step 2 over text 1420:2ad2 +
 * 1420:2ae5 "_Custom"; ResMgr_GetString @ 1100:078a + Res_GetStringCopy
 * @ 11f8:0080; see res_blob_entry_kind()/RES_BLOB_TRIBE in rsc_loader.h).
 * OS-free string table (segment 1420 dump TBD 2026-10-15: no tribe-name
 * block decoded yet, so no 1:1 table port possible; FIRSTMOVE/PILLAGE/
 * font-map tables also pending via boot_init_unit_font_tables @
 * 10e8:2d46): index selects the tribe, returns the NUL-terminated name,
 * NULL when out of range or not yet dumped. Packet I scope: doc-only
 * extension; stub body in newgame.c untouched (still returns NULL).
 * Read-only. */
const char *tribe_get_name_list(int index);

/* Orig: NewGame_Wizard @ 10e8:0000 step 2 -> newgame_pick_tribe.
 * Text 1420:2ad2 ("Pick your tribe...") + 1420:2ae5 ("_Custom.") plus the
 * tribe list from Tribe_GetNameList @ 11f8:0080 (Orig FUN_11f8_0080,
 * param 0x94) via Dialog_MenuZ_Modal @ 1220:0156 (Orig FUN_1220_0156);
 * selection stored to g_nPlayerCiv @ 12d8:4300. Win16 dialog tail TBD;
 * stub returns CIV_ERR_UNSUPPORTED (NULL ctx/civ_out -> CIV_ERR_NOENT). */
civ_error_t newgame_pick_tribe(const civ_newgame_settings_t *ctx, int *civ_out);

/* Orig: NewGame_Wizard @ 10e8:0000 steps 3/4 -> newgame_customize_world.
 * Custom-world path via the custom Dialog_MenuZ_Modal variant @ 1220:16fd
 * (Orig FUN_1220_16fd, separate entry from the base @ 1220:0156) driving
 * the Ctrl_* @ 1058 controls (RANDOMRADIOPROC @ 1058:06ab, RANDOMSTATICPROC
 * @ 1058:081a, RANDOMUSERPROC @ 1058:0921, REGIONPROC @ 1058:09f6); feeds
 * WorldGen_CustomizeDialog @ 1060:xxxx (Orig FUN_1060_xxxx, see worldgen.h).
 * Win16 dialog/control tails TBD; stub returns CIV_ERR_UNSUPPORTED (NULL
 * ctx -> CIV_ERR_NOENT). */
civ_error_t newgame_customize_world(civ_newgame_settings_t *ctx);

/* Orig: FUN_1220_16fd @ 1220:16fd -> newgame_custom_world_modal.
 * Custom-world modal variant (Dialog_MenuZ_Modal custom entry), separate
 * from the base Dialog_MenuZ_Modal @ 1220:0156 (Orig FUN_1220_0156) used by
 * steps 0..2 above (see dialog.h dialog_menuz for the base). Hosts the
 * Ctrl_* @ 1058 paint/input fan-out for the Customize-World editor and
 * feeds WorldGen_CustomizeDialog @ 1060:xxxx. Win16 USER/GDI -> SDL3 TBD;
 * stub returns CIV_ERR_UNSUPPORTED (NULL ctx/out_action -> CIV_ERR_NOENT).
 * No globals, no exit(), read-only inputs. */
civ_error_t newgame_custom_world_modal(const civ_newgame_settings_t *ctx,
                                       int *out_action);

/* --- Chain 2026-10-16 (19): Menu_PromptNation discriminators (goal 4) ---
 * Orig Menu_PromptNation @ 1070:1f58 (civ.exe, x86:LE:16:Protected Mode),
 * decompiled this pass (zob. tez mainmenu.h chain-19):
 *   gate (DAT_12d0_00b6 & 0xfe00) != 0; Util_StrCopy Which-nation? @
 *   1420:146d ("Which nation?"); for (i = 1; i < 8; i++) StrAppend "_"
 *   @ 1420:147b + StrAppend 12d8:03da+i*0x20 (runtime tribe table 7 x
 *   0x20, wypelniana przez Tribe_GetNameList @ 11f8:0080 / Res blob 0x94
 *   + Res_GetStringCopy; 12d8 to DGROUP runtime, nie statyczny ROM —
 *   stad zera w statycznym dumpie); DAT_12d8_24da = DAT_12d0_00b6>>9 ^
 *   0xff; i = Dialog_ShowMessage @ 1220:00d2(0x24fa,100,0x50) + 1; jezeli
 *   0 < i to DAT_1290_0114 = i + Ship_OpenView @ 1070:0000 (renamed) +
 *   Ship_ShowView(i,0xffff,1) @ 1070:0327 + Ship_CloseView @ 1070:02b5.
 * Kiedy (wybor nacji na starcie? NewGame_Wizard? debug?):
 *   NIE startowy wybor (ten to NewGame_Wizard @ 10e8:0000 step 2 Pick
 *   tribe przez newgame_pick_tribe + tribe_get_name_list STUB + text
 *   1420:2ad2/2ae5) — to podglad spaceship wybranej nacji w grze
 *   (wywoływany z menu 506/0x374 + Ship_LaunchAndShow @ 1070:2015 brat).
 * Lista nacji (7 civow? barbarians?): petla 1..7 = 7 wpisow z 12d8:03da
 *   (7 zywych civow; barbarians osobno poza petla, nie w tej liscie;
 *   maska 12d8:24da = dostepne). Zwiazek z tribe_get_name_list (STUB w
 *   newgame.c: to zrodlo nazw 12d8:03da przez Res blob 0x94) i
 *   Text_FillResStringList @ 1098:1f4f (ogolny builder list res-string;
 *   PromptNation to jego Which-nation instancja z Dialog_ShowMessage).
 * Port listy/logiki LIVE 1:1 na jawnych parametrach (no Win16, no
 * globals); Dialog_ShowMessage + Ship_* ogony STUB. No exit(). */

/* Orig Menu_PromptNation @ 1070:1f58 gate -> menu_prompt_nation_needed.
 * 1:1: (flags_00b6 & 0xfe00) != 0. */
int menu_prompt_nation_needed(int flags_00b6);

/* Orig same loop bound (i = 1; i < 8) -> menu_prompt_nation_count.
 * 1:1: 7 wpisow. */
int menu_prompt_nation_count(void);

/* Orig same DAT_12d8_24da assign -> menu_prompt_nation_avail_mask.
 * 1:1: (flags_00b6 >> 9) ^ 0xff (low 8 bitow; caller maskuje do 0xff). */
int menu_prompt_nation_avail_mask(int flags_00b6);

/* Orig same Dialog_ShowMessage+1 gate (0 < i) ->
 * menu_prompt_nation_choice_valid. 1:1: ret_plus1 > 0. */
int menu_prompt_nation_choice_valid(int ret_plus1);

/* Orig: NewGame_Wizard @ 10e8:0000 step 5 (finalize tail) -> newgame_finalize.
 * Writes g_nDifficulty @ 12d8:42f6, g_nCivCount @ 12d8:3b9a,
 * g_nCivAliveMask @ 12d8:42f8 (= 0xff >> (6 - count), see
 * newgame_alive_mask), g_nPlayerCiv @ 12d8:4300, g_nYear @ 12d8:42fa
 * (= 0xF060, CIV_NEWGAME_START_YEAR); then Sound_PlayId @ 1150:00c9
 * (leader wav for the picked civ) and Map_GenerateRandomOrLoad @ 1028:0000
 * next. OS-free mask/year math is ported 1:1 and validated here (range
 * errors -> CIV_ERR_RANGE); the dialog/sound/map tails stay stubbed, so
 * the function still returns CIV_ERR_UNSUPPORTED on valid input (NULL
 * settings -> CIV_ERR_NOENT). No globals, no exit(), read-only. */
civ_error_t newgame_finalize(const civ_newgame_settings_t *settings);

#endif /* CIVLIN_NEWGAME_H */
