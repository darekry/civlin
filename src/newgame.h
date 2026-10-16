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
 * DUMP 2026-10-16 (rsc_dump read-only on unpacked/civdata0.rsc, BE16 count
 * + Pascal walk = rsc_get_blob_string @ 1100:078a shape):
 *   0x94 tribe plurals (16) @ file off 6959 (0x1b2f): NONE Romans
 *     Babylonians Germans Egyptians Americans Greeks Indians NONE Russians
 *     Zulus French Aztecs Chinese English Mongols (two wizard columns are
 *     entries i and i+8, i = 1..count+1 1-based blob N).
 *   0x8c leaders (16) @ file off 6344 (0x18c8): "" Caesar Hammurabi
 *     Frederick Ramesses "Abe Lincoln" Alexander "M.Gandhi" "" Stalin Shaka
 *     Napoleon Montezuma "Mao Tse Tung" "Elizabeth I" "Genghis Khan"
 *     (empties at the two NONE slots 0/8 align with the tribe table above;
 *     wizard copies via Res_GetStringCopy(0x8c, slot) to 12d8:civ*0x20+0x1da).
 *   0x95 adjectives (16) @ file off 8851 (0x2293): NONE Roman Babylonian
 *     German Egyptian American Greek Indian NONE Russian Zulu French Aztec
 *     Chinese English Mongol (wizard Res_GetStringCopy(0x95, slot) plural
 *     tail via FUN_10d8_0528).
 * Tables below are the OS-free 1:1 port (static const, read-only, no
 * Win16, no globals mutated, no exit()); ResMgr/GLOBALLOCK fetch tail
 * stays in res_get_string_copy STUB (see rsc_loader.h ownership note).
 * tribe_get_name_list is LIVE: 0-based index into the 16-entry tribe
 * table, NULL when out of range. */
const char *tribe_get_name_list(int index);
#define CIV_TRIBE_N 16
#define CIV_LEADER_N 16
#define CIV_TRIBE_ADJ_N 16
extern const char *const civ_tribe_table[CIV_TRIBE_N];
extern const char *const civ_leader_table[CIV_LEADER_N];
extern const char *const civ_tribe_adj_table[CIV_TRIBE_ADJ_N];
/* Counts (1:1 blob BE counts @ 11f8:0080). */
int tribe_name_count(void);
int tribe_leader_count(void);
int tribe_adj_count(void);
/* 0x8c leader for the same 0-based slot ("" at NONE slots 0/8, NULL OOR).
 * Orig: Res_GetStringCopy @ 11f8:0080 (0x8c slot) in the wizard
 * tribe-pick tail @ 10e8:0000. */
const char *tribe_get_leader_name(int index);
/* 0x95 adjective for the same 0-based slot (NULL OOR).
 * Orig: Res_GetStringCopy @ 11f8:0080 (0x95 slot) in the wizard
 * tribe-pick tail @ 10e8:0000. */
const char *tribe_get_adjective(int index);
/* --- P2 2026-10-16: Init_UnitFontTables @ 10e8:2d46 fixed bytes ---
 * Orig Init_UnitFontTables @ 10e8:2d46 (civ.exe, x86:LE:16:Protected Mode),
 * decompiled this pass: GLOBALLOCK font resource + FUN_10e8_2cc1 distance
 * picks for FIRSTMOVE @ 1420:45fc / PILLAGE @ 1420:4606 / font-map loop
 * (Win16 GLOBALLOCK/UNLOCK + Gfx_ReplaceIconColor tails -> STUB in
 * boot_init_unit_font_tables, owned by the parallel agent, not redeclared
 * here) + two OS-free fixed stores ported below LIVE 1:1 (no Win16, no
 * globals, read-only):
 *   base (always): 45be=0 45c2=1 45c0=2 45c4=3 45bf=4 45c3=5 45c1=6 45c5=7
 *     45c6=0xf8 45ca=0xf9 45c8=0xfa 45cc=0xfb 45c7=0xfc 45cb=0xfd
 *     45c9=0xfe 45cd=0xff (16 B).
 *   alt (DAT_1420_17a8 != 0 branch): 45ee=0xff 45ef=0xfb 45f0=0xf9 45f1=1
 *     45f2=5 45f3=4 45f4=0xfc 45f5=6 45f6=0xfa 45f7=2 45f8=3 45f9=0xfb
 *     45fa=7 45fb=0xf8 45fc=0xf8 45fd=0 (16 B).
 * Order below follows the decompile store order. No exit(), C17. */
#define CIV_UNIT_FONT_FIXED_N 16
#define CIV_UNIT_FONT_ALT_N 16
/* Orig Init_UnitFontTables @ 10e8:2d46 base store count. */
int newgame_unit_font_fixed_count(void);
/* Orig Init_UnitFontTables @ 10e8:2d46 alt store count. */
int newgame_unit_font_alt_count(void);
/* Byte at 0-based position in the base store, -1 on OOR.
 * Orig Init_UnitFontTables @ 10e8:2d46 (base). */
int newgame_unit_font_fixed_at(int pos);
/* Byte at 0-based position in the alt store, -1 on OOR.
 * Orig Init_UnitFontTables @ 10e8:2d46 (alt branch). */
int newgame_unit_font_alt_at(int pos);

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

/* --- Chain 2026-10-16 (24): wizard steps 0-5 + year (task goal 3) ---
 * Orig NewGame_Wizard @ 10e8:0000 (civ.exe, x86:LE:16:Protected Mode),
 * decompiled this pass (caller Main_RunStartupSequence @ 11e0:0000):
 *   init Init_UnitFontTables @ 10e8:2d46 + g_nDifficulty=0 + g_nCivCount=6 +
 *   dialog 0x90 (frame 0x8d) + state iVar8=0..5 loop (Dialog_MenuZ_Modal @
 *   1220:0156 for steps 0..2, NewGame_CustomWorldModal @ 1220:16fd — was
 *   FUN_1220_16fd, renamed this pass — for steps 3/4; DAT_1418_0004 =
 *   back/cancel, iVar8+1 forward, iVar8-1 back, 3->2 collapse):
 *   0 difficulty: text 1420:2a5a ("Difficulty Level..._Chieftain (easiest)_
 *     Warlord_Prince_King_Emperor (toughest)") + preselect g_nDifficulty ->
 *     g_nDifficulty = modal_idx (0=Chieftain..4=Emperor, 12d8:42f6;
 *     newgame_pick_difficulty STUB, math in newgame_difficulty_from_index).
 *   1 competition: text 1420:2aa9 ("Level of Competition...") + per-count
 *     "7..3" + 1420:2ac1 "_" + dec + 1420:2ac3 " Civilizations" + preselect
 *     6-count -> g_nCivCount = Util_Clamp(6-idx,2,6) @ 11c8:0063 (2..6,
 *     12d8:3b9a) + g_nCivAliveMask = 0xff >> (6-count) (12d8:42f8; helpers
 *     newgame_civ_count_from_index/newgame_alive_mask LIVE +
 *     newgame_competition_resolve clamp below).
 *   2 tribe: text 1420:2ad2 ("Pick your tribe...") + 1420:2ae5 "_Custom."
 *     (pick 0) + two Res_GetStringCopy @ 11f8:0080 (was mislabeled
 *     Tribe_GetNameList: NIE osobna funkcja, to Res blob 0x94 entry group:
 *     Res_GetStringCopy(0x94,i)/Res_GetStringCopy(0x94,i+8) two-column list
 *     for i=1..count+1 over 0x2aee/0x2af0 separators; see rsc_loader.h
 *     RES_BLOB_TRIBE) -> g_nPlayerCiv (12d8:4300) via
 *     newgame_player_civ_resolve below (pick 0 = roll Rng_Range(count)+1,
 *     else wrap when count < pick-1: pick=(pick-1-count)+8, civ=pick&7,
 *     slot= civ(+8 when pick>7) at 43b8+0x9c; tribe strings via Res blob
 *     0x94 + Res 0x8c leader names + 0x95 title; tribe_get_name_list stays
 *     STUB — no static tribe table dumped yet) + Text_FillResStringList @
 *     1098:1f4f (NIE uzywany do listy plemion: to ogolny builder tabel
 *     0x82..0x86 w Dialog_InitResStringTables @ 1068:0295; wizard buduje
 *     liste recznie StrCopy/StrAppend petla, nie Fill; por. dialog.h).
 *   3 custom-a: Dialog_GetItemRect @ 1100:03ff (0x67 + offsets 0x32/0x50) +
 *     ResMgr_GetString/PascalToCStr/StrCopy + NewGame_CustomWorldModal list
 *     (first custom screen; newgame_customize_world STUB).
 *   4 custom-b: same shape as 3 (second custom screen, offsets identical;
 *     back from 4 collapses 3->2); WorldGen_CustomizeDialog @ 1060:xxxx
 *     host (see worldgen.h).
 *   5 finalize (iVar8 > 4 tail): tribe-name trims (12d8:02da -s, 03da),
 *     g_nYear = 0xF060 (CIV_NEWGAME_START_YEAR; int16 -4000 = 4000 BC, see
 *     newgame_start_year_* below), unit/city/map clear (0x50x0x32 zero,
 *     0x80 unit slots 0xff, replay/tech/wonder tables), Rng_Range(0x32)
 *     drift, per-civ FUN_10e8_12b5 + Tech_HasTech @ 1170:3181 list,
 *     Sound_PlayId @ 1150:00c9 (leader wav) + Sys_WaitForInput +
 *     Sound_PlayId(1,1) STOP, then Map_GenerateRandomOrLoad @ 1028:0000
 *     next (see worldgen.h); newgame_finalize stays PARTIAL (mask/year
 *     LIVE, dialog/sound/map tails STUB).
 * OS-free step math below is LIVE 1:1 on explicit params (no Win16, no
 * globals, read-only except outs); dialog/ResMgr/RNG-handle tails stay STUB
 * with TODO Orig. No exit(), C17. */

/* Orig step 0 (g_nDifficulty = modal_idx) -> newgame_difficulty_from_index.
 * 1:1 idx 0..4 -> idx (Chieftain..Emperor), else -1. No globals. */
int newgame_difficulty_from_index(int modal_index);

/* Orig step 1 (g_nCivCount = Util_Clamp(6-idx,2,6)) ->
 * newgame_competition_resolve. 1:1 clamp(6-idx,2,6) (Util_Clamp @
 * 11c8:0063 inlined lo 2 hi 6; sibling newgame_civ_count_from_index above
 * keeps the strict -1 form). No globals. */
int newgame_competition_resolve(int modal_index);

/* Orig step 2 (g_nPlayerCiv/slot @ 43b8+0x9c) -> newgame_player_civ_resolve.
 * 1:1 on explicit params (rng_draw = Rng_Range @ 11c8:003d draw 0..count-1
 * for the pick==0 roll; Res blob 0x94 strings + 0x8c/0x95 names stay
 * caller-side STUB): pick == 0 -> player = rng_draw+1, slot = player;
 * else { p = pick; if (count < pick-1) p = (pick-1-count)+8; player = p&7;
 * slot = player + (p > 7 ? 8 : 0); }. Returns player civ 0..7, -1 on
 * count OOR 2..6 / pick < 0 / rng_draw OOR for the roll / NULL out_slot
 * (out_slot may be NULL = skip; returns player anyway). No globals. */
int newgame_player_civ_resolve(int pick, int civ_count, int rng_draw,
                               int *out_slot);

/* Orig finalize (g_nYear @ 12d8:42fa = 0xF060) -> year helpers. 1:1:
 * raw u16 0xF060; signed int16 -4000 (= 4000 BC: ujemne = BC, dodatnie = AD
 * via Util_IntToDec @ 1000:2928 + BC/AD w game_append_year_text @
 * 1088:1dd0, see game.h). No globals. */
unsigned newgame_start_year_raw(void);
int newgame_start_year_signed(void);

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

/* --- Chain 2026-10-16 (25): CustomWorldModal validation (task goal 1) ---
 * Orig NewGame_CustomWorldModal @ 1220:16fd (civ.exe, x86:LE:16:Protected
 * Mode), decompiled this pass (steps 3/4 host in NewGame_Wizard @ 10e8:0000):
 *   MAKEPROCINSTANCE + SETWINDOWLONG hook + 0x18 radio slots clear +
 *   optional RanDoMStatic CREATEWINDOW (param_2 != 0) + GETCLIENTRECT +
 *   GETWINDOWTEXT + CREATEWINDOW 0x1c2 list + SETFOCUS + FUN_1100_0913 fill
 *   0x1c2 0..999 + SENDMESSAGE 0x30 + INVALIDATERECT + PEEKMESSAGE pump until
 *   DAT_12b0_00a0 != 0; exit restores SETWINDOWLONG + FREEPROCINSTANCE;
 *   DAT_12b0_00a0 == 1 copies the picked string (Util_StrCopy 0x30).
 *   Wizard wrapper (NewGame_Wizard steps 3/4): local_c = NewGame_CustomWorldModal;
 *   local_c == 0xffff -> DAT_1418_0004 = 1 (back/cancel) else 0 (forward);
 *   then the shared stepper: DAT_1418_0004 == 0 -> iVar8+1 else { if 0 < iVar8
 *   iVar8-1; if iVar8 == 3 iVar8 = 2 } (3->2 collapse: back from custom-a
 *   returns to tribe, back from custom-b returns to tribe too via 3->2).
 * Jakie ustawienia (temperatura/klimat z dialogu 0x8f/0x323?):
 *   NIE ten modal — temperatura/klimat (Temperature @ 1420:2e28 / Climate @
 *   1420:2e34 / 0x2e23 / 0x2e3c) zyja w WorldGen_LoadEarth @ 1108:0243
 *   (dialog 0x8f, IDs from 0x323: row = (id-0x323)/4, col = (id-0x323)%4;
 *   col < 3 picks, col == 3 opens help via NewGame_ShowCustomHelp @
 *   10e0:0000 -> NewGame_CustomHelpDialog @ 10e0:03a2; see
 *   worldgen_dialog_id_to_cell / worldgen_custom_apply LIVE in worldgen.h).
 *   Ten modal (1220:16fd) to lista custom-world krokow 3/4 (Ctrl_* @ 1058
 *   buttons: RANDOMRADIOPROC @ 1058:06ab, RANDOMSTATICPROC @ 1058:081a,
 *   RANDOMUSERPROC @ 1058:0921, REGIONPROC @ 1058:09f6) + WorldGen probe
 *   WorldGen_ProbeCustomFont @ 1060:0000 (was FUN_1060_0000, renamed this
 *   pass: CREATEFONT + SELECTOBJECT + DELETEOBJECT font probe, caller
 *   FUN_1060_01c1) + Dialog_SplitMenuText @ 1220:0000 (was FUN_1220_0000,
 *   renamed this pass: '_' split into the 0x24fa MenuZ buffer, callee of
 *   Dialog_MenuZ_Modal @ 1220:0156).
 * Zwiazek z worldgen_custom_apply (LIVE): ten modal ZWRACA wybor (0xffff =
 *   back), a worldgen_custom_apply ZAPISUJE wybor dialogu 0x8f (settings[row]
 *   = col); oba dziela back/front semantike (newgame_custom_result_is_back +
 *   newgame_wizard_step_back ponizej) + walidacje ponizej.
 * OS-free validation below is LIVE 1:1 on explicit params (no Win16, no
 * globals, read-only); the CREATEWINDOW/PEEKMESSAGE pump stays STUB in
 * newgame_custom_world_modal above. No exit(), C17. */

/* Orig wizard wrapper (local_c == 0xffff -> back) -> newgame_custom_result_is_back.
 * 1:1 nonzero (1) when modal_ret == 0xffff (DAT_1418_0004 = 1 path), else 0. */
int newgame_custom_result_is_back(int modal_ret);

/* Orig shared stepper (DAT_1418_0004 gate + 3->2 collapse) ->
 * newgame_wizard_step_back. 1:1: back == 0 -> step+1; else { if 0 < step
 * step-1; if step == 3 step = 2 }. step 0..5, back 0/1; OOR step -> -1. */
int newgame_wizard_step_back(int step, int back);

/* Orig custom-world settings shape (4 entries, preset 1 via
 * WorldGen_LoadEarth @ 1108:0243) -> newgame_custom_settings_valid.
 * 1:1 each settings[i] in 0..2 (choice cols; help col 3 never stored, see
 * worldgen_custom_apply). NULL -> NOENT; OOR entry -> RANGE; else CIV_OK. */
civ_error_t newgame_custom_settings_valid(const int *settings, int n);

/* --- Chain 2026-10-16 (26): custom help texts + font probe (task goal 4) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   NewGame_ShowCustomHelp @ 10e0:0000 (thin help-col wrapper: calls
 *     NewGame_CustomHelpDialog(0x2994, buf+10, param_1, param_2, param_1,
 *     param_2, 0xd8, 0xe, DAT_12d8_45c0, DAT_12d8_45be, DAT_12d8_45cd, ...)
 *     then DAT_1418_0004 = 0 (forward); sole caller WorldGen_LoadEarth @
 *     1108:0243 col == 3 branch).
 *   NewGame_CustomHelpDialog @ 10e0:03a2 (help body: Dlg_CreateTracked @
 *     1098:124f 0x87 + FUN_11d0_00e4/022d + FUN_10e0_06fd + Dialog_GetItemRect
 *     @ 1100:03ff 0x66/0x67 + Gfx + Text + PEEKMESSAGE until DAT_12d8_0030;
 *     params: res 0x99-ish id, colors 0xd8/0xe + DAT_12d8_45fb/45fd/45f1).
 *     Callers: ShowCustomHelp above + NewGame_ShowCivNoteEarly @ 10e0:077f
 *     (was FUN_10e0_077f, renamed this pass: gate (g_nOptionFlags & 1) != 0
 *     && g_nYear < 0 -> HelpDialog(0x29c9, "___Civilization Note___" @
 *     1420:29cd)) + NewGame_ShowCivNoteForPlayer @ 10e0:07e8 (was
 *     FUN_10e0_07e8: gate DAT_13f0_000c == g_nPlayerCiv -> HelpDialog(0x29e5,
 *     @ 1420:29e9)).
 *   WorldGen_ProbeCustomFont @ 1060:0000 (CREATEFONT h 0x12 weight 400 +
 *     SELECTOBJECT + DELETEOBJECT probe; sole caller
 *     WorldGen_ShowVideoCapsDialog @ 1060:01c1 (was FUN_1060_01c1: GETDEVICECAPS
 *     x16 dump + Dlg_CreateTracked 0x94 0x41 + ProbeCustomFont(0x12) +
 *     Util_IntToDec/StrCopy/Append display + Sys_WaitForInput); see
 *     worldgen_probe_font_* LIVE + worldgen_probe_custom_font STUB in
 *     worldgen.h).
 * Kiedy help: WorldGen_LoadEarth dialog 0x8f help col (col == 3, IDs from
 *   0x323: row = (id-0x323)/4): row 0 -> 0x2e23, 1 -> Temperature @
 *   1420:2e28, 2 -> Climate @ 1420:2e34, 3 -> 0x2e3c (switch w LoadEarth);
 *   + wczesna notka CivNoteEarly (opt&1 + year<0, F1/'?' TBD) + notka gracza
 *   CivNoteForPlayer (DAT_13f0_000c == player). Tresc: temperatura/klimat
 *   opisy via ShowCustomHelp param (0x2e23/2e28/2e34/2e3c string ids) +
 *   Civilization Note 0x29c9/0x29e5 bodies. Zwiazek z CustomWorldModal @
 *   1220:16fd (steps 3/4 lista custom-world via Ctrl_* @ 1058 + WorldGen
 *   customize host; help modal to osobny Dialog 0x87, nie 16fd).
 * OS-free row->help + gate math below LIVE 1:1 (no Win16, no globals);
 * Dlg_CreateTracked/PEEKMESSAGE tails stay STUB. No exit(), C17. */
#define CIV_HELP_ROW_LANDMASS 0x2e23
#define CIV_HELP_ROW_TEMPERATURE 0x2e28
#define CIV_HELP_ROW_CLIMATE 0x2e34
#define CIV_HELP_ROW_AGE 0x2e3c
#define CIV_HELP_DLG_ID 0x87
#define CIV_HELP_SHOW_PARAM 0x2994
#define CIV_HELP_NOTE_EARLY 0x29c9
#define CIV_HELP_NOTE_PLAYER 0x29e5

/* Orig LoadEarth help switch (row -> help id) -> newgame_custom_help_for_row.
 * 1:1 0 -> 0x2e23, 1 -> 0x2e28, 2 -> 0x2e34, 3 -> 0x2e3c, else -1. */
int newgame_custom_help_for_row(int row);

/* Orig ShowCustomHelp fixed param (0x2994) -> newgame_show_custom_help_param.
 * 1:1 0x2994. No globals. */
int newgame_show_custom_help_param(void);

/* Orig ShowCustomHelp tail (DAT_1418_0004 = 0) -> newgame_show_custom_help_next.
 * 1:1 0 (forward; wizard stepper newgame_wizard_step_back handles back). */
int newgame_show_custom_help_next(void);

/* Orig CivNoteEarly gate ((opt & 1) != 0 && year < 0) ->
 * newgame_civ_note_early_needed. 1:1 1 gdy pokazac, 0 gdy nie. */
int newgame_civ_note_early_needed(int opt_flags, int year);

/* Orig CivNoteForPlayer gate (DAT_13f0_000c == g_nPlayerCiv) ->
 * newgame_civ_note_player_needed. 1:1 (a == b) ? 1 : 0. */
int newgame_civ_note_player_needed(int tag_13f0_000c, int player_civ);

/* Orig NewGame_ShowCustomHelp @ 10e0:0000 -> newgame_show_custom_help.
 * Help-col wrapper (row validated via for_row above + CustomHelpDialog
 * Win16 tail). STUB CIV_ERR_UNSUPPORTED (RANGE on row OOR). */
civ_error_t newgame_show_custom_help(int row);

/* Orig NewGame_CustomHelpDialog @ 10e0:03a2 -> newgame_custom_help_dialog.
 * Help body (Dlg 0x87 + layout + PEEKMESSAGE). Win16 -> SDL TBD; stub. */
civ_error_t newgame_custom_help_dialog(int help_id);

/* --- Chain 2026-10-16 (30): NewGame_RateStartPositions kernels (task goal 3) ---
 * Orig NewGame_RateStartPositions @ 10e8:1d48 (civ.exe, x86:LE:16:Protected
 * Mode), decompiled this pass (caller NewGame_Wizard @ 10e8:0000 finalize;
 * placement osobno w NewGame_InitCivState @ 10e8:12b5 + Rng(0x32)):
 *   rating WSZYSTKICH startow (civ 1..7 z unit 62ff != -1): dystans do
 *   najblizszego sasiada na tym samym kontynencie (Map_GetContinentId @
 *   1110:1d99 + Map_WrapStepDist @ 11c8:02ec, min; 999 = brak sasiada):
 *   <10 close (0), <0x14 moderate (+1), ==999 isolated (+4 + bit 1<<civ w
 *   local_1c), else far (+2). Kryteria terenu (9-tile: self+8): ocean count
 *   (10 -> coastal/inland), 2-count (grass 2 -> +1 grasslands), 0xb-count
 *   (river >= 2 -> +2 river valley, else grass/plains). OceanVal @ 1110:1dbd
 *   /0x32 switch: 0 small island (+0), 1 large island (+0), 2 small cont (+1),
 *   3 medium (+1), else large (+2); remainder %0x32 nieuzywany (zob. map.h
 *   map_cont_size_class/remainder LIVE). Score iVar6 = dist-base + terrain +
 *   continent; best local_a = max; local_5c[civ] = score.
 *   Pick: drugi loop: 3 < best-score -> Unit_SpawnUnit extra settler (unit 0);
 *   tech loop (score-gap entries): Tech_ChooseResearch(civ,0) + Res 0x82
 *   append; best-tie: score == best && (pierwszy || isolated-bit) -> pick
 *   (izolowane wygrywaja remisy; zwraca best civ, nie losuje z waga — waga
 *   to tylko gap->settlers/techs). Ile pozycji: 7 civow (1..7), nie tile.
 *   Zwiazek z Map_ScoreCitySites @ 1028:0fb4 (NIEUZYWANY tu: tamten waguje
 *   tile weights @ 1420:17d6 dla miast w grze; rating startow liczy wlasny
 *   9-tile + continent-size, nie wagi) i City_FindNearest @ 11c8:0124 (LIVE
 *   city_find_nearest: InitCivState uzywa go do min-dist od miast + od
 *   wczesniejszych startow; Rate uzywa WrapStepDist wprost do sasiadow).
 *   Port ponizej LIVE 1:1 (no Win16, no globals); StrCopy/MenuZ ogony STUB. */

/* Orig dist-base (<10:0, <0x14:1, ==999:4, else 2) -> newgame_start_dist_score.
 * 1:1; dist < 0 -> 2 (far path, orig nigdy ujemny). */
int newgame_start_dist_score(int dist);

/* Orig terrain (+2 river>=2 else +1 grass>=3 else 0) -> newgame_start_terrain_bonus.
 * 1:1. */
int newgame_start_terrain_bonus(int river_cnt, int grass_cnt);

/* Orig continent (/0x32: 0/1:+0, 2/3:+1, else +2) -> newgame_start_continent_bonus.
 * 1:1 via map_cont_size_class (0..4+); val < 0 -> -1. */
int newgame_start_continent_bonus(int ocean_val);

/* Orig extra-settler (3 < best-score) -> newgame_start_needs_extra_settler.
 * 1:1 1 gdy dosypac settlera. */
int newgame_start_needs_extra_settler(int best, int score);

/* Orig best-tie (score == best && (first || isolated)) ->
 * newgame_start_should_pick. 1:1. */
int newgame_start_should_pick(int score, int best, int is_first, int is_isolated);

/* --- Chain 2026-10-16 (p5): CivNote res ids + tresc (goal 4) ---
 * Orig NewGame_ShowCivNoteEarly @ 10e0:077f + NewGame_ShowCivNoteForPlayer @
 * 10e0:07e8 (civ.exe, x86:LE:16:Protected Mode), decompiled this pass
 * (gates juz LIVE: newgame_civ_note_early_needed/_player_needed):
 *   Early: gate (g_nOptionFlags & 1) != 0 && g_nYear < 0 ->
 *     NewGame_CustomHelpDialog @ 10e0:03a2 (0x29c9, "___Civilization
 *     Note___" @ 1420:29cd, colors 0xd8/0xe + DAT_12d8_45fb/45fd/45fd).
 *   ForPlayer: gate DAT_13f0_000c == g_nPlayerCiv ->
 *     CustomHelpDialog(0x29e5, "___Civilization Note___" @ 1420:29e9,
 *     colors 0xd8/0xe + 45fb/45fd/45f1).
 * Kiedy notki (start gry? pierwsze tury? konkretny gracz?):
 *   Early = DOWOLNY civ z wlaczona opcja podpowiedzi (opt&1, tj. "instant
 *     advice"?) DOPOKI rok ujemny (era BC: g_nYear < 0, start 0xF060 =
 *     -4000 = 4000 BC) — czyli wczesna faza gry, nie tylko tura 0; wywolanie
 *     z Diplo_OpenContactDialog @ 1160:0000 (Monarchy 0x3757 site, zob.
 *     Tech_GrantTech @ 1170:2aaa tail: tech 5 + player -> ShowCivNoteEarly)
 *     + Tech_GrantTech monarchy-path. ForPlayer = tylko gdy znacznik
 *     DAT_13f0_000c == g_nPlayerCiv (konkretny gracz, nie kazdy).
 * Tresc (Res 0x29c9/0x29e5?): ciala Res 0x29c9 (early) / 0x29e5 (player);
 *   tytuly OBA "** Civilization Note **" (@ 1420:29cd / 1420:29e9,
 *   dump-verified this pass). Pelne ciala wymagaja ResMgr-dumpu 0x29c9/
 *   0x29e5 (TODO, nie dumpowane — poza zakresem; gate + id LIVE).
 *   Dialog 0x87 via NewGame_CustomHelpDialog @ 10e0:03a2 (STUB).
 * OS-free id/tytul ponizej LIVE 1:1 (no Win16, no globals, read-only). */

/* Orig Early HelpDialog id -> newgame_civ_note_early_res. 1:1 0x29c9. */
int newgame_civ_note_early_res(void);

/* Orig ForPlayer HelpDialog id -> newgame_civ_note_player_res. 1:1 0x29e5. */
int newgame_civ_note_player_res(void);

/* Orig tytuly 1420:29cd/29e9 -> newgame_civ_note_title.
 * 1:1 "** Civilization Note **" (oba). Read-only literal. */
const char *newgame_civ_note_title(void);

#endif /* CIVLIN_NEWGAME_H */
