/* Hall of Fame (famehall.civ) -> SDL3.
 *
 * Ghidra anchors (civ.exe, segment 1080 + score/ending helpers):
 *   HoF_Clear           Orig FUN_1080_0000 @ 1080:0000 (6 slots -> 0xffff)
 *   HoF_Load            Orig FUN_1080_0023 @ 1080:0023 (save_file_open/read/close, 1420:14e8)
 *   HoF_Save            Orig FUN_1080_0092 @ 1080:0092 (save_file_open x2/write/close, 1420:14f5)
 *   HoF_Show            Orig FUN_1080_0119 @ 1080:0119 (dialog 0x94; per-slot text build)
 *   HoF_TryInsert       Orig FUN_1080_0595 @ 1080:0595 (score*(diff+..) ordering, shift down)
 *   HoF_VictoryCeremony Orig FUN_1080_06ff @ 1080:06ff (Sound_PlayId(0x22)=WINTUNE + pyramid
 *                          frames + "Sire, your fame now..." strings)
 *   City_ShowTopFive    Orig FUN_1080_099b @ 1080:099b (1420:159c "The Top Five Cities...")
 *   Hof_CalcFinalScore  Orig FUN_1040_1137 @ 1040:1137 (pure score math; quit==2 bypass
 *                          lives in Game_ProcessGameEnd caller, see game.c:47)
 *   Hof_PlayEndAnimation Orig FUN_1188_07ad @ 1188:07ad (frames/BITBLT/ANIMATEPALETTE + MCI)
 *   Hof_ShowEndDialog   Orig FUN_1180_0000 @ 1180:0000 (MESSAGEBOX/DIALOG)
 *
 * Slot table @ DAT_1420_52a8, stride 0x27 words; per-slot fields:
 *   +0x00 score, +0x02 difficulty idx, +0x04/+0x06/+0x08 aux, +0x0c year-ish.
 * Difficulty names @ 1420:153e..1567 (" Chieftan "/" Warlord "/" Prince "/
 * 0x155f King /" Emperor ") — note original "Chieftan" typo kept verbatim.
 * Rating line @ 1420:1572 "CIVILIZATION RATING: ".
 * Famehall strings: 1420:14e8 filename, 1420:14f5 save mode, 1420:1502 "Empty",
 * 1420:159c "The Top Five Cities...".
 */
#ifndef CIVLIN_HALLOFFAME_H
#define CIVLIN_HALLOFFAME_H

#include "civ_common.h"

/* 6 slots per HoF_Clear @ 1080:0000 (was 5; corrected 2026-10-15). */
#define CIV_HOF_SLOTS 6
#define CIV_HOF_FILE "famehall.civ"

/* Raw-table Empty sentinel: HoF_Clear memsets each slot word to 0xffff. */
#define CIV_HOF_EMPTY_SCORE (-1)
#define CIV_HOF_DIFFICULTY_MIN 0
#define CIV_HOF_DIFFICULTY_MAX 4

typedef struct civ_hof_entry {
    int present;      /* 0 = "Empty" (1420:1502), 1 = filled */
    int score;        /* -1 == 0xffff Empty sentinel after HoF_Clear */
    int difficulty;   /* 0..4, names at 1420:153e..1567 */
    int year;
    char name[48];
    char civ[48];
} civ_hof_entry_t;

/* Orig: FUN_1080_0000 @ 1080:0000 -> HoF_Clear.
 * OS-free: clears 6 slots to 0xffff (raw memset DAT_1420_52a8,
 * 6 * 0x27 words). Decoded equivalent here: present=0, score/difficulty/
 * year=-1, empty strings. Returns CIV_OK, CIV_ERR_NOENT on NULL. */
civ_error_t HoF_Clear(civ_hof_entry_t slots[CIV_HOF_SLOTS]);

/* Orig: FUN_1080_0023 @ 1080:0023 -> HoF_Load.
 * Stub: famehall.civ via save_file_open (1420:14e8, OF_READ) /
 * save_file_read / save_file_close. Needs save_file_* wiring (see save.h).
 * Returns CIV_ERR_UNSUPPORTED. */
civ_error_t HoF_Load(civ_hof_entry_t slots[CIV_HOF_SLOTS]);

/* Orig: FUN_1080_0092 @ 1080:0092 -> HoF_Save.
 * Stub: save_file_open x2 + save_file_write + save_file_close
 * (mode string @ 1420:14f5). Needs save_file_* wiring. */
civ_error_t HoF_Save(const civ_hof_entry_t slots[CIV_HOF_SLOTS]);

/* Orig: FUN_1080_0119 @ 1080:0119 -> HoF_Show.
 * Stub: modal dialog 0x94 + per-slot text build ("Empty" @ 1420:1502,
 * difficulty names @ 1420:153e..1567, "CIVILIZATION RATING: " @ 1420:1572).
 * Needs dialog 0x94 bridge (see dialog.h). */
civ_error_t HoF_Show(const civ_hof_entry_t slots[CIV_HOF_SLOTS]);

/* Orig: FUN_1080_0595 @ 1080:0595 -> HoF_TryInsert.
 * Partial: OS-free ordering core is implemented (weighted score scan +
 * shift-down insert, 1:1 with decompiler shape); tail calling HoF_Show is
 * still a stub. Weight = score * (difficulty + 1); exact bias/addend TBD
 * from 1080:0595 decompile (task notes "score*(diff+..)"). Empty slots
 * (present==0 or score<0) sort last with weight -1. On success copies
 * *result into the hole and stores the index via out_index; -1 when the
 * score does not qualify. Always attempts the HoF_Show tail best-effort. */
civ_error_t HoF_TryInsert(civ_hof_entry_t slots[CIV_HOF_SLOTS],
                          const civ_hof_entry_t *result, int *out_index);

/* Orig: FUN_1080_06ff @ 1080:06ff -> HoF_VictoryCeremony.
 * Stub: Sound_PlayId(0x22)=WINTUNE (see audio.h) + pyramid frames +
 * "Sire, your fame now..." strings. Needs audio + frame blits. */
civ_error_t HoF_VictoryCeremony(int level, int score);

/* Orig: FUN_1080_099b @ 1080:099b -> City_ShowTopFive.
 * Stub: city ranking via 1420:159c "The Top Five Cities..." plus city
 * records @ 1420:5500. Needs city-table decode. */
civ_error_t City_ShowTopFive(void);

/* Orig: FUN_1040_1137 @ 1040:1137 -> Hof_CalcFinalScore.
 * Stub: pure score math; formula doc pending 1040:1137 decompile
 * (expected shape: base adjusted by difficulty multiplier, hence the
 * TryInsert weight above). The quit==2 bypass lives in the
 * Game_ProcessGameEnd @ 1088:0b6d caller (see game.c:47), not here. */
civ_error_t Hof_CalcFinalScore(int base_score, int difficulty, int *out_score);

/* Orig: FUN_1038_xxxx @ 1038:xxxx -> Hof_CalcInterimScore.
 * OS-free pure score math 1:1 v1 (implemented, no Win16, no globals,
 * read-only; game_calc_power_score @ 1088:108a shape): interim =
 * base_score * (difficulty + 1) (Chieftain x1 .. Emperor x5, matching the
 * HoF_TryInsert @ 1080:0595 weight). Exact bias/addend TBD vs 1038
 * decompile; sits between Title_PlayAnimLoop @ 1030:0000 and
 * Hof_CalcFinalScore @ 1040:1137. Returns -1 on base_score < 0 or
 * difficulty outside CIV_HOF_DIFFICULTY_MIN..MAX. */
int Hof_CalcInterimScore(int base_score, int difficulty);

/* Orig: FUN_1188_07ad @ 1188:07ad -> Hof_PlayEndAnimation.
 * Stub: end-of-game frames (BITBLT/ANIMATEPALETTE) + MCI music.
 * Called from Game_ProcessGameEnd unless quit==2. */
civ_error_t Hof_PlayEndAnimation(int ending_kind);

/* Orig: FUN_1180_0000 @ 1180:0000 -> Hof_ShowEndDialog.
 * Stub: MESSAGEBOX/DIALOG end screen (score + quit/ending kind).
 * Called from Game_ProcessGameEnd unless quit==2. */
civ_error_t Hof_ShowEndDialog(int ending_kind, int final_score);

/* --- Legacy lowercase compat (deprecated; delegate to HoF_* above) --- */
civ_error_t hof_load(civ_hof_entry_t slots[CIV_HOF_SLOTS]);
civ_error_t hof_save(const civ_hof_entry_t slots[CIV_HOF_SLOTS]);
civ_error_t hof_show(const civ_hof_entry_t slots[CIV_HOF_SLOTS]);

/* Inserts a finished-game score; returns 1 + index via out_index on success,
 * 0 when the score does not qualify. Shows the table (HoF_Show). */
civ_error_t hof_try_insert(civ_hof_entry_t slots[CIV_HOF_SLOTS],
                           const civ_hof_entry_t *result, int *out_index);

civ_error_t hof_victory_ceremony(int level, int score);
civ_error_t city_show_top_five(void);

#endif /* CIVLIN_HALLOFFAME_H */
