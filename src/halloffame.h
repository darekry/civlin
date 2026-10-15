/* Hall of Fame (famehall.civ) -> SDL3.
 *
 * Ghidra anchors (civ.exe, segment 1080):
 *   HoF_Clear           @ 1080:0000 (6 slots -> 0xffff)
 *   HoF_Load            @ 1080:0023 (save_file_open/read/close, 1420:14e8)
 *   HoF_Save            @ 1080:0092 (save_file_open x2/write/close, 1420:14f5)
 *   HoF_Show            @ 1080:0119 (dialog 0x94; per-slot text build)
 *   HoF_TryInsert       @ 1080:0595 (score*(diff+..) ordering, shift down)
 *   HoF_VictoryCeremony @ 1080:06ff (Sound_PlayId(0x22)=WINTUNE + pyramid
 *                          frames + "Sire, your fame now..." strings)
 *   City_ShowTopFive    @ 1080:099b (1420:159c "The Top Five Cities...")
 *
 * Slot table @ DAT_1420_52a8, stride 0x27 words; per-slot fields:
 *   +0x00 score, +0x02 difficulty idx, +0x04/+0x06/+0x08 aux, +0x0c year-ish.
 * Difficulty names @ 1420:153e..1567 (" Chieftan "/" Warlord "/" Prince "/
 * 0x155f King /" Emperor ") — note original "Chieftan" typo kept verbatim.
 * Rating line @ 1420:1572 "CIVILIZATION RATING: ".
 */
#ifndef CIVLIN_HALLOFFAME_H
#define CIVLIN_HALLOFFAME_H

#include "civ_common.h"

#define CIV_HOF_SLOTS 5
#define CIV_HOF_FILE "famehall.civ"

typedef struct civ_hof_entry {
    int present;      /* 0 = "Empty" (1420:1502), 1 = filled */
    int score;
    int difficulty;   /* 0..4, names at 1420:153e..1567 */
    int year;
    char name[48];
    char civ[48];
} civ_hof_entry_t;

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
