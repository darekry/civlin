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
 *
 * Y2 end-to-end flow (stubs stay CIV_ERR_UNSUPPORTED, flow documented here,
 * no Win16; present via video.h gfx_port_present_rect @ 10d0:0854 +
 * palette_apply_to_port @ 1200:00a8/1090:0376):
 *   1. HoF_Load @ 1080:0023 (hof_load): famehall.civ @ 1420:14e8 via
 *      save_file_open/read/close into 6 slots (HoF_Clear @ 1080:0000
 *      0xffff sentinel first).
 *   2. HoF_TryInsert @ 1080:0595 (hof_try_insert): weight =
 *      score * (difficulty + 1) via hof_weighted_score, linear scan,
 *      memmove shift-down, copy into the hole, out_index (-1 = no qualify).
 *   3. HoF_Show @ 1080:0119 (hof_show): modal dialog 0x94 + per-slot text
 *      ("Empty" @ 1420:1502, difficulty names @ 1420:153e..1567, rating @
 *      1420:1572); SDL3 present via gfx_port_present_rect (BITBLT rop
 *      0xcc0020 -> SDL_RenderTexture, VALIDATERECT no-op) after the
 *      palette_apply_to_port [first, first+count) range guard.
 *   4. HoF_Save @ 1080:0092 (hof_save): save_file_open x2/write/close
 *      (mode @ 1420:14f5).
 *   5. HoF_VictoryCeremony @ 1080:06ff (hof_victory_ceremony):
 *      Sound_PlayId(0x22) = WINTUNE (Sound_MapIdToWav @ 1150:019e) +
 *      pyramid frames + "Sire, your fame now..." strings, same present path.
 *   6. City_ShowTopFive @ 1080:099b (city_show_top_five): "The Top Five
 *      Cities..." @ 1420:159c + city records @ 1420:5500.
 */
#ifndef CIVLIN_HALLOFFAME_H
#define CIVLIN_HALLOFFAME_H

#include "civ_common.h"

/* 6 slots per HoF_Clear @ 1080:0000 (was 5; corrected 2026-10-15). */
#define CIV_HOF_SLOTS 6
#define CIV_HOF_FILE "famehall.civ"

/* famehall.civ raw format (decomposed 2026-10-16 from HoF_Load @
 * 1080:0023 / HoF_Save @ 1080:0092 disassembly: save_file_read/write
 * 0x1d4 = 468 B at DAT_1420_52a8; HoF_TryInsert @ 1080:0595 stride
 * 0x27 words = 0x4e B per slot):
 *   file = 6 slots x 78 B, little-endian, no header, no checksum.
 *   per slot (all u16 LE unless noted):
 *     +0x00 score      (0xffff = Empty sentinel, set by HoF_Clear;
 *                        note Clear writes ONLY this word per slot)
 *     +0x02 difficulty (0..4, names @ 1420:153e..1567)
 *     +0x04 aux        (DAT_12d8_42ee copy; semantics TBD)
 *     +0x06 year       (i16; HoF_Show sign-tests it for BC/AD;
 *                        source DAT_12d8_42fa)
 *     +0x08 unk        (DAT_1420_516c copy; semantics TBD)
 *     +0x0a unk        (DAT_1420_516a copy; semantics TBD)
 *     +0x0c pop/civ    (FUN_11c8_0334(g_nPlayerCiv @ 12d8:4300);
 *                        HoF_Show formats it via FUN_11c8_03ac; TBD)
 *     +0x0e leader name, 32 B NUL-terminated (from 12d8:civ*0x20+0x1da)
 *     +0x2e civ name,    32 B NUL-terminated (from 12d8:civ*0x20+0x2da)
 *   TryInsert weight (verified @ 1080:05b0-05df, signed IMUL after
 *   ADD DX,2 on BOTH sides): weight = score * (difficulty + 2).
 *   Insert when slot_w < new_w; slot 5 always takes (replaces last),
 *   so the -1 "no qualify" path is unreachable in orig. Shift-down
 *   copies slot[DI-1] -> slot[DI] (DI = 5..idx+1, 0x4e B via
 *   Util_MemCopyFar @ 1000:137f, dst DI*0x4e+0x52a8 = src + 0x4e).
 * Codec below maps the known fields; unknown header words (w2/w4/w5/w6)
 * encode as 0 and are ignored on decode (semantics TBD, see above). */
#define CIV_HOF_RAW_SIZE 468u
#define CIV_HOF_SLOT_RAW 78u
#define CIV_HOF_NAME_RAW 32u

/* Raw-table Empty sentinel: HoF_Clear memsets each slot word to 0xffff. */
#define CIV_HOF_EMPTY_SCORE (-1)
#define CIV_HOF_DIFFICULTY_MIN 0
#define CIV_HOF_DIFFICULTY_MAX 4

/* AG3: HoF difficulty + label strings (OS-free static const port,
 * 2026-10-15; other HoF impls untouched). Difficulty names @ 1420:153e..
 * 1567 (5 entries, original "Chieftan" typo kept verbatim, never fixed):
 * 0 " Chieftan ", 1 " Warlord ", 2 " Prince ", 3 "King", 4 "Emperor".
 * Rating line @ 1420:1572 "CIVILIZATION RATING: " (trailing space kept) +
 * 1420:159c "The Top Five Cities..." (City_ShowTopFive @ 1080:099b title) +
 * 1420:1502 "Empty" (HoF_Clear 0xffff slot text in HoF_Show @ 1080:0119).
 * No SDL, no globals, no exit(), read-only. */
#define CIV_HOF_DIFF_COUNT 5
#define CIV_HOF_EMPTY_TEXT "Empty"
#define CIV_HOF_RATING_LABEL "CIVILIZATION RATING: "
#define CIV_HOF_TOP_FIVE_TITLE "The Top Five Cities..."

/* Difficulty index -> display name (0..4 via CIV_HOF_DIFFICULTY_MIN/MAX
 * above; "Chieftan" typo kept verbatim). Returns NULL when difficulty is
 * out of range. No globals (function-local read-only view), no exit(),
 * read-only. */
const char *hof_diff_names(int difficulty);

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
 * LIVE: HoF_Clear first (as in orig), then save_file_open(CIV_HOF_FILE,
 * O_READ) + save_file_read 468 B + save_file_close. Missing file =
 * empty table (CIV_OK, orig falls through the same way); short read =
 * CIV_ERR_IO (table left cleared); bad image = CIV_ERR_BAD_DATA.
 * Decode via hof_decode below. No Win16, no globals, no exit(). */
civ_error_t HoF_Load(civ_hof_entry_t slots[CIV_HOF_SLOTS]);

/* Orig: FUN_1080_0092 @ 1080:0092 -> HoF_Save.
 * LIVE: hof_encode to 468 B, then save_file_open(CIV_HOF_FILE,
 * O_WRITE|O_CREAT|O_TRUNC — covers orig's double-open 0x8004/0x8102) +
 * save_file_write + save_file_close. CIV_OK or CIV_ERR_IO. No Win16,
 * no globals, no exit(). */
civ_error_t HoF_Save(const civ_hof_entry_t slots[CIV_HOF_SLOTS]);

/* Orig: FUN_1080_0119 @ 1080:0119 -> HoF_Show.
 * Stub: modal dialog 0x94 + per-slot text build ("Empty" @ 1420:1502,
 * difficulty names @ 1420:153e..1567, "CIVILIZATION RATING: " @ 1420:1572).
 * Needs dialog 0x94 bridge (see dialog.h). */
civ_error_t HoF_Show(const civ_hof_entry_t slots[CIV_HOF_SLOTS]);

/* Orig: FUN_1080_0119 @ 1080:0119 text-build half (HoF_Show per-slot line,
 * callees Text_DrawAligned @ 1098:0ed1 + City_FormatPopCount @ 11c8:03ac
 * + Text_DrawResString @ 1098:20f4 + Dialog_GetItemRect @ 1100:03ff).
 * OS-free 1:1 of the string assembly (no dialog/GDI): Empty slots render
 * CIV_HOF_EMPTY_TEXT ("Empty" @ 1420:1502); filled slots render "N. <name>
 * of the <civ> (<difficulty>) - <year> BC/AD - Score <score> -
 * CIVILIZATION RATING: <rating>%" where <difficulty> is hof_diff_names
 * (names @ 1420:153e..1567, "Chieftan" typo kept), <year> uses the HoF
 * sign-test (year < 0 -> "BC" @ 1420:1522-ish, else "AD"), and <rating> =
 * score * (difficulty + 1) / 50 + "%" (HoF_Show @ 1080:0435-0460: IMUL
 * then DIV 0x32, "%" @ 1420:1589; note TryInsert weighs score *
 * (difficulty + 2), verified @ 1080:05b0-05df). NOENT on NULL e/out,
 * RANGE when cap == 0, index OOR, or the text (incl. NUL) does not fit.
 * No Win16, no globals, no exit(). */
civ_error_t hof_format_slot(const civ_hof_entry_t *e, int index, char *out,
                            size_t cap);

/* Orig: FUN_1080_0595 @ 1080:0595 -> HoF_TryInsert.
 * Ordering core ported 1:1 (linear weighted scan + shift down via
 * memmove + copy, then the HoF_Show tail best-effort). Weight =
 * score * (difficulty + 2) on BOTH sides (verified @ 1080:05b0-05df:
 * ADD DX,2 before each IMUL); empty slots weigh -1 and sort last.
 * Like orig, slot 5 always takes when reached (replaces the worst),
 * so out_index is always 0..5 here (orig's -1 is unreachable).
 * NOENT on NULL slots/result/out_index, RANGE on out-of-range
 * difficulty or negative score. */
civ_error_t HoF_TryInsert(civ_hof_entry_t slots[CIV_HOF_SLOTS],
                          const civ_hof_entry_t *result, int *out_index);

/* Raw famehall.civ codec (OS-free, 1:1 with the format block above).
 * hof_encode packs slots into 468 B LE (unknown header words as 0,
 * names NUL-padded to 32 B); hof_decode unpacks (0xffff score word =
 * Empty slot). NOENT on NULL args, RANGE when a present slot's score
 * does not fit u16, else CIV_OK. No Win16, no globals, no exit(). */
civ_error_t hof_encode(const civ_hof_entry_t slots[CIV_HOF_SLOTS],
                        uint8_t out[CIV_HOF_RAW_SIZE]);
civ_error_t hof_decode(const uint8_t raw[CIV_HOF_RAW_SIZE],
                        civ_hof_entry_t slots[CIV_HOF_SLOTS]);

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
 * base_score * (difficulty + 1) (Chieftain x1 .. Emperor x5).
 * NOTE: HoF_TryInsert @ 1080:0595 weighs score*(difficulty+2) (verified
 * @ 1080:05b0-05df); whether interim shares the +2 bias is TBD vs the
 * 1038 decompile. Sits between Title_PlayAnimLoop @ 1030:0000 and
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

/* --- Legacy lowercase compat (deprecated; delegate to HoF_* above) ---
 * Y2 flow delegates (Orig addrs kept for the RE map; present via
 * gfx_port_present_rect @ 10d0:0854 + palette_apply_to_port @
 * 1200:00a8/1090:0376, see video.h). */
/* Orig: FUN_1080_0023 @ 1080:0023 -> hof_load (Y2 step 1). */
civ_error_t hof_load(civ_hof_entry_t slots[CIV_HOF_SLOTS]);
/* Orig: FUN_1080_0092 @ 1080:0092 -> hof_save (Y2 step 4). */
civ_error_t hof_save(const civ_hof_entry_t slots[CIV_HOF_SLOTS]);
/* Orig: FUN_1080_0119 @ 1080:0119 -> hof_show (Y2 step 3, dialog 0x94). */
civ_error_t hof_show(const civ_hof_entry_t slots[CIV_HOF_SLOTS]);

/* Orig: FUN_1080_0595 @ 1080:0595 -> hof_try_insert (Y2 step 2).
 * Inserts a finished-game score; returns 1 + index via out_index on success,
 * 0 when the score does not qualify. Shows the table (HoF_Show). */
civ_error_t hof_try_insert(civ_hof_entry_t slots[CIV_HOF_SLOTS],
                           const civ_hof_entry_t *result, int *out_index);

/* Orig: FUN_1080_06ff @ 1080:06ff -> hof_victory_ceremony (Y2 step 5,
 * WINTUNE 0x22 + pyramid frames + Sire strings). */
civ_error_t hof_victory_ceremony(int level, int score);
/* Orig: FUN_1080_099b @ 1080:099b -> city_show_top_five (Y2 step 6,
 * Top Five @ 1420:159c). */
civ_error_t city_show_top_five(void);

#endif /* CIVLIN_HALLOFFAME_H */
