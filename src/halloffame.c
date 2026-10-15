/* Hall of Fame: HoF batch (segment 1080 + score/ending helpers).
 * OS-free functions port decompiler shape 1:1; Win16 paths
 * (segments/__far/DOS/SELECTPALETTE/handles/MCI/DIALOG) stub out with
 * CIV_ERR_UNSUPPORTED + TODO Orig. No globals, no exit(), no file I/O in
 * implemented paths; stubs never touch civ1win/unpacked.
 *
 * Y2 end-to-end flow (stubs stay, flow documented, no Win16; see
 * halloffame.h Y2 block for the numbered steps):
 *   hof_load @ 1080:0023 -> hof_try_insert @ 1080:0595 (score*(diff+1) +
 *   memmove shift) -> hof_show @ 1080:0119 (dialog 0x94) -> hof_save @
 *   1080:0092 + hof_victory_ceremony @ 1080:06ff (WINTUNE 0x22 via
 *   Sound_PlayId @ 1150:00c9 / Sound_MapIdToWav @ 1150:019e + pyramid
 *   frames + Sire strings) + city_show_top_five @ 1080:099b (Top Five @
 *   1420:159c). Show/present via gfx_port_present_rect @ 10d0:0854
 *   (SDL_RenderTexture, VALIDATERECT no-op) after palette_apply_to_port
 *   @ 1200:00a8/1090:0376 range guard (see video.h). */
#include "halloffame.h"

#include <string.h>

/* Weighted-score helper for HoF_TryInsert ordering (not part of the 10;
 * kept static so `nm -g` only shows the public batch). Shape follows
 * Orig FUN_1080_0595 @ 1080:0595 ("score*(diff+..)"): weight grows with
 * difficulty. Exact bias/addend TBD from decompile; (difficulty + 1) is
 * the documented placeholder (Chieftain x1 .. Emperor x5). Empty slots
 * (score < 0) map to -1 so they always sort last. */
static long hof_weighted_score(int score, int difficulty)
{
    if (score < 0)
        return -1L;
    return (long)score * (long)(difficulty + 1);
}

/* Orig: FUN_1080_0000 @ 1080:0000 -> HoF_Clear.
 * OS-free 1:1: raw orig memsets DAT_1420_52a8 (6 slots x 0x27 words) to
 * 0xffff; decoded equivalent sets the Empty sentinel per slot. */
civ_error_t HoF_Clear(civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    int i;

    if (!slots)
        return CIV_ERR_NOENT;
    for (i = 0; i < CIV_HOF_SLOTS; i++) {
        slots[i].present = 0;
        slots[i].score = CIV_HOF_EMPTY_SCORE; /* 0xffff sentinel */
        slots[i].difficulty = -1;
        slots[i].year = -1;
        slots[i].name[0] = '\0';
        slots[i].civ[0] = '\0';
    }
    return CIV_OK;
}

/* Orig: FUN_1080_0023 @ 1080:0023 -> HoF_Load. */
civ_error_t HoF_Load(civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    if (!slots)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_1080_0023 @ 1080:0023 (famehall.civ @ 1420:14e8 via
     * save_file_open OF_READ + save_file_read 6x0x27 words +
     * save_file_close; see save.h save_file_open/read/close). Needs
     * save_file_* wiring + famehall.civ field map. */
    return CIV_ERR_UNSUPPORTED;
}

/* Orig: FUN_1080_0092 @ 1080:0092 -> HoF_Save. */
civ_error_t HoF_Save(const civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    if (!slots)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_1080_0092 @ 1080:0092 (save_file_open x2 +
     * save_file_write + save_file_close, mode @ 1420:14f5; see save.h).
     * Needs save_file_* wiring + famehall.civ field map. */
    return CIV_ERR_UNSUPPORTED;
}

/* Orig: FUN_1080_0119 @ 1080:0119 -> HoF_Show (Y2 flow step 3). */
civ_error_t HoF_Show(const civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    if (!slots)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_1080_0119 @ 1080:0119 (modal dialog 0x94 + per-slot
     * text build: "Empty" @ 1420:1502, difficulty names @ 1420:153e..1567
     * incl. "Chieftan" typo, "CIVILIZATION RATING: " @ 1420:1572, slot
     * table @ DAT_1420_52a8 stride 0x27 words). Needs dialog 0x94 bridge
     * (see dialog.h dlg_create_tracked); SDL3 show presents via
     * gfx_port_present_rect @ 10d0:0854 (SDL_RenderTexture, VALIDATERECT
     * no-op) after palette_apply_to_port @ 1200:00a8/1090:0376 (see
     * video.h Y2 HoF/show present path). */
    return CIV_ERR_UNSUPPORTED;
}

/* Orig: FUN_1080_0595 @ 1080:0595 -> HoF_TryInsert.
 * Ordering core ported 1:1 (linear weighted scan + shift down via
 * memmove + copy); tail calling HoF_Show @ 1080:0119 is still a stub and
 * is attempted best-effort without overriding the data-path result. */
civ_error_t HoF_TryInsert(civ_hof_entry_t slots[CIV_HOF_SLOTS],
                          const civ_hof_entry_t *result, int *out_index)
{
    long new_w;
    int i;
    int idx = -1;

    if (!slots || !result || !out_index)
        return CIV_ERR_NOENT;
    if (result->difficulty < CIV_HOF_DIFFICULTY_MIN ||
        result->difficulty > CIV_HOF_DIFFICULTY_MAX)
        return CIV_ERR_RANGE;
    if (result->score < 0)
        return CIV_ERR_RANGE;
    *out_index = -1;

    new_w = hof_weighted_score(result->score, result->difficulty);
    for (i = 0; i < CIV_HOF_SLOTS; i++) {
        long cur_w;

        if (slots[i].present && slots[i].score >= 0 &&
            slots[i].difficulty >= CIV_HOF_DIFFICULTY_MIN &&
            slots[i].difficulty <= CIV_HOF_DIFFICULTY_MAX)
            cur_w = hof_weighted_score(slots[i].score, slots[i].difficulty);
        else
            cur_w = -1L; /* Empty (HoF_Clear sentinel) sorts last. */
        if (new_w > cur_w) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        /* Does not qualify; still attempts the HoF_Show tail best-effort
         * (stub until dialog 0x94 is bridged). */
        (void)HoF_Show(slots);
        return CIV_OK;
    }
    /* Shift down: orig moves tail slots one step (far memmove over 0x27-word
     * stride); last entry drops off. */
    if (idx < CIV_HOF_SLOTS - 1)
        memmove(&slots[idx + 1], &slots[idx],
                (size_t)(CIV_HOF_SLOTS - 1 - idx) * sizeof(slots[0]));
    memcpy(&slots[idx], result, sizeof(slots[0]));
    *out_index = idx;
    /* Stub tail: show the updated table (dialog 0x94 TBD). */
    (void)HoF_Show(slots);
    return CIV_OK;
}

/* Orig: FUN_1080_06ff @ 1080:06ff -> HoF_VictoryCeremony (Y2 flow step 5). */
civ_error_t HoF_VictoryCeremony(int level, int score)
{
    (void)level;
    (void)score;
    /* TODO: Orig FUN_1080_06ff @ 1080:06ff (Sound_PlayId(0x22)=WINTUNE, see
     * audio.h CIV_SOUND_VICTORY + Sound_MapIdToWav @ 1150:019e, pyramid
     * build frames via BITBLT/SELECTPALETTE, "Sire, your fame now..."
     * strings). Needs audio + SDL texture blits; frames present via
     * gfx_port_present_rect @ 10d0:0854 (SDL_RenderTexture) after
     * palette_apply_to_port @ 1200:00a8/1090:0376 (see video.h Y2
     * HoF/show present path). */
    return CIV_ERR_UNSUPPORTED;
}

/* Orig: FUN_1080_099b @ 1080:099b -> City_ShowTopFive. */
civ_error_t City_ShowTopFive(void)
{
    /* TODO: Orig FUN_1080_099b @ 1080:099b ("The Top Five Cities..." @
     * 1420:159c via 1420:159c helper + city records @ 1420:5500 0x1c bytes
     * each; dialog TBD). Needs city-table decode. */
    return CIV_ERR_UNSUPPORTED;
}

/* Orig: FUN_1038_xxxx @ 1038:xxxx -> Hof_CalcInterimScore.
 * OS-free 1:1 v1 (implemented): interim = base * (difficulty + 1),
 * matching the HoF_TryInsert @ 1080:0595 weight via hof_weighted_score
 * above and the game_calc_power_score @ 1088:108a pure-kernel shape.
 * Exact bias/addend TBD vs 1038 decompile. */
int Hof_CalcInterimScore(int base_score, int difficulty)
{
    if (base_score < 0)
        return -1;
    if (difficulty < CIV_HOF_DIFFICULTY_MIN ||
        difficulty > CIV_HOF_DIFFICULTY_MAX)
        return -1;
    return base_score * (difficulty + 1);
}

/* Orig: FUN_1040_1137 @ 1040:1137 -> Hof_CalcFinalScore. */
civ_error_t Hof_CalcFinalScore(int base_score, int difficulty, int *out_score)
{
    (void)base_score;
    (void)difficulty;
    if (!out_score)
        return CIV_ERR_NOENT;
    if (difficulty < CIV_HOF_DIFFICULTY_MIN ||
        difficulty > CIV_HOF_DIFFICULTY_MAX)
        return CIV_ERR_RANGE;
    /* TODO: Orig FUN_1040_1137 @ 1040:1137 (pure score arithmetic; exact
     * formula TBD from decompile — expected shape is a difficulty-weighted
     * final from the base game score, matching the HoF_TryInsert weight).
     * quit==2 bypass lives in the Game_ProcessGameEnd @ 1088:0b6d caller
     * (see game.c:47), not here: that path skips 1188:07ad + 1180:0000. */
    return CIV_ERR_UNSUPPORTED;
}

/* Orig: FUN_1188_07ad @ 1188:07ad -> Hof_PlayEndAnimation. */
civ_error_t Hof_PlayEndAnimation(int ending_kind)
{
    (void)ending_kind;
    /* TODO: Orig FUN_1188_07ad @ 1188:07ad (end frames via BITBLT +
     * ANIMATEPALETTE + MCI music; called from Game_ProcessGameEnd @
     * 1088:0b6d unless quit==2, see game.c:47). Needs SDL frame bridge. */
    return CIV_ERR_UNSUPPORTED;
}

/* Orig: FUN_1180_0000 @ 1180:0000 -> Hof_ShowEndDialog. */
civ_error_t Hof_ShowEndDialog(int ending_kind, int final_score)
{
    (void)ending_kind;
    (void)final_score;
    /* TODO: Orig FUN_1180_0000 @ 1180:0000 (MESSAGEBOX/DIALOG end screen;
     * called from Game_ProcessGameEnd @ 1088:0b6d unless quit==2, see
     * game.c:47). Needs dialog bridge (see dialog.h). */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Legacy lowercase compat (deprecated; delegate to HoF_* above) ---
 * Y2 end-to-end flow delegates (thin LIVE wrappers; Orig addrs kept so
 * tools/gen_status.py counts them; present via gfx_port_present_rect @
 * 10d0:0854 + palette_apply_to_port @ 1200:00a8/1090:0376, see video.h). */

/* Orig: FUN_1080_0023 @ 1080:0023 -> hof_load (Y2 flow step 1: HoF_Load;
 * famehall.civ @ 1420:14e8). Thin delegate to HoF_Load. */
civ_error_t hof_load(civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    return HoF_Load(slots);
}

/* Orig: FUN_1080_0092 @ 1080:0092 -> hof_save (Y2 flow step 4: HoF_Save;
 * mode @ 1420:14f5). Thin delegate to HoF_Save. */
civ_error_t hof_save(const civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    return HoF_Save(slots);
}

/* Orig: FUN_1080_0119 @ 1080:0119 -> hof_show (Y2 flow step 3: HoF_Show;
 * dialog 0x94, "Empty" @ 1420:1502, names @ 1420:153e..1567, rating @
 * 1420:1572; presents via gfx_port_present_rect @ 10d0:0854). Thin
 * delegate to HoF_Show. */
civ_error_t hof_show(const civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    return HoF_Show(slots);
}

/* Orig: FUN_1080_0595 @ 1080:0595 -> hof_try_insert (Y2 flow step 2:
 * HoF_TryInsert; weight score*(difficulty+1) + memmove shift-down).
 * Thin delegate to HoF_TryInsert. */
civ_error_t hof_try_insert(civ_hof_entry_t slots[CIV_HOF_SLOTS],
                           const civ_hof_entry_t *result, int *out_index)
{
    return HoF_TryInsert(slots, result, out_index);
}

/* Orig: FUN_1080_06ff @ 1080:06ff -> hof_victory_ceremony (Y2 flow step 5:
 * HoF_VictoryCeremony; WINTUNE 0x22 via Sound_MapIdToWav @ 1150:019e +
 * pyramid frames + Sire strings; presents via gfx_port_present_rect @
 * 10d0:0854). Thin delegate to HoF_VictoryCeremony. */
civ_error_t hof_victory_ceremony(int level, int score)
{
    return HoF_VictoryCeremony(level, score);
}

/* Orig: FUN_1080_099b @ 1080:099b -> city_show_top_five (Y2 flow step 6:
 * City_ShowTopFive; "The Top Five Cities..." @ 1420:159c + records @
 * 1420:5500). Thin delegate to City_ShowTopFive. */
civ_error_t city_show_top_five(void)
{
    return City_ShowTopFive();
}
