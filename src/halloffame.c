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

#include <stdio.h>
#include <string.h>

#include "save.h"

/* LE u16 helpers for the famehall.civ image (explicit shifts, no
 * alignment or aliasing assumptions). */
static uint16_t hof_get_u16le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void hof_put_u16le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xffu);
    p[1] = (uint8_t)((v >> 8) & 0xffu);
}

/* Bounded strlen for the 48 B decoded name fields (never trusts the
 * caller's NUL; C17 has no strnlen in strict mode). */
static size_t hof_name_len(const char *s)
{
    size_t n = 0;

    while (n < sizeof(((civ_hof_entry_t *)0)->name) && s[n] != '\0')
        n++;
    if (n > CIV_HOF_NAME_RAW - 1u)
        n = CIV_HOF_NAME_RAW - 1u;
    return n;
}

/* Weighted-score helper for HoF_TryInsert ordering (not part of the 10;
 * kept static so `nm -g` only shows the public batch). Shape verified
 * 2026-10-16 @ 1080:05b0-05df: signed IMUL after ADD DX,2 on BOTH the
 * slot side ([BX+52aa]) and the new-score side (12d8:42f6), i.e. weight
 * = score * (difficulty + 2). Empty slots (score < 0) map to -1 so
 * they always sort last. */
static long hof_weighted_score(int score, int difficulty)
{
    if (score < 0)
        return -1L;
    return (long)score * (long)(difficulty + 2);
}

/* AG3 HoF difficulty names: OS-free static const 5-entry port of
 * 1420:153e..1567 (Orig "Chieftan" typo kept verbatim) + label macros in
 * halloffame.h (CIV_HOF_EMPTY_TEXT @ 1420:1502, CIV_HOF_RATING_LABEL @
 * 1420:1572, CIV_HOF_TOP_FIVE_TITLE @ 1420:159c for HoF_Show @ 1080:0119
 * + City_ShowTopFive @ 1080:099b). Other HoF impls untouched. No globals
 * (function-local read-only view), no exit(), read-only. */
const char *hof_diff_names(int difficulty)
{
    static const char *const names[CIV_HOF_DIFF_COUNT] = {
        " Chieftan ",
        " Warlord ",
        " Prince ",
        "King",
        "Emperor"
    };

    if (difficulty < CIV_HOF_DIFFICULTY_MIN ||
        difficulty > CIV_HOF_DIFFICULTY_MAX)
        return NULL;
    return names[difficulty];
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
    /* HoF_Clear first (orig CALLs it), then famehall.civ @ 1420:14e8 via
     * save_file_open O_READ + save_file_read 468 B + save_file_close.
     * Missing file falls through with the cleared table (CIV_OK), as in
     * orig (fresh install has no famehall.civ yet). */
    win16_handle h;
    uint8_t raw[CIV_HOF_RAW_SIZE];
    unsigned long got = 0;

    if (!slots)
        return CIV_ERR_NOENT;
    HoF_Clear(slots);
    h = save_file_open(CIV_HOF_FILE, 0);
    if (h == WIN16_HFILE_ERROR)
        return CIV_OK;
    if (save_file_read(h, raw, sizeof(raw), &got) != 0 ||
        got != sizeof(raw)) {
        save_file_close(h);
        return CIV_ERR_IO;
    }
    save_file_close(h);
    return hof_decode(raw, slots);
}

/* Orig: FUN_1080_0092 @ 1080:0092 -> HoF_Save. */
civ_error_t HoF_Save(const civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    /* save_file_open O_WRITE|O_CREAT|O_TRUNC covers orig's double-open
     * (0x8004 then 0x8102), then save_file_write 468 B + save_file_close
     * (mode string @ 1420:14f5). */
    win16_handle h;
    uint8_t raw[CIV_HOF_RAW_SIZE];
    unsigned long wrote = 0;
    civ_error_t rc;

    if (!slots)
        return CIV_ERR_NOENT;
    rc = hof_encode(slots, raw);
    if (rc != CIV_OK)
        return rc;
    h = save_file_open(CIV_HOF_FILE, 1);
    if (h == WIN16_HFILE_ERROR)
        return CIV_ERR_IO;
    if (save_file_write(h, raw, sizeof(raw), &wrote) != 0 ||
        wrote != sizeof(raw)) {
        save_file_close(h);
        return CIV_ERR_IO;
    }
    save_file_close(h);
    return CIV_OK;
}

/* Orig: FUN_1080_0119 @ 1080:0119 -> HoF_Show (Y2 flow step 3). */
civ_error_t HoF_Show(const civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    if (!slots)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_1080_0119 @ 1080:0119 (modal dialog 0x94 + per-slot
     * text build: "Empty" @ 1420:1502, difficulty names @ 1420:153e..1567
     * incl. "Chieftan" typo, "CIVILIZATION RATING: " @ 1420:1572, slot
     * table @ DAT_1420_52a8 stride 0x27 words; text drawn via
     * Text_DrawAligned @ 1098:0ed1 + Text_DrawResString @ 1098:20f4,
     * population via City_FormatPopCount @ 11c8:03ac / City_CalcPopTotal
     * @ 11c8:0334, item rects via Dialog_GetItemRect @ 1100:03ff). The
     * OS-free string half lives in hof_format_slot below; SDL3 show
     * presents via gfx_port_present_rect @ 10d0:0854 (SDL_RenderTexture,
     * VALIDATERECT no-op) after palette_apply_to_port @ 1200:00a8/1090:0376
     * (see video.h Y2 HoF/show present path). */
    return CIV_ERR_UNSUPPORTED;
}

/* Orig HoF_Show @ 1080:0119 text-build half (OS-free, no dialog/GDI). */
civ_error_t hof_format_slot(const civ_hof_entry_t *e, int index, char *out,
                            size_t cap)
{
    const char *diff = NULL;
    const char *era = NULL;
    long abs_year = 0;
    long rating = 0;
    int n = 0;

    if (!e || !out)
        return CIV_ERR_NOENT;
    if (cap == 0)
        return CIV_ERR_RANGE;
    if (index < 0 || index >= CIV_HOF_SLOTS)
        return CIV_ERR_RANGE;
    if (!e->present || e->score < 0) {
        if (sizeof(CIV_HOF_EMPTY_TEXT) > cap)
            return CIV_ERR_RANGE;
        memcpy(out, CIV_HOF_EMPTY_TEXT, sizeof(CIV_HOF_EMPTY_TEXT));
        return CIV_OK;
    }
    if (e->difficulty < CIV_HOF_DIFFICULTY_MIN ||
        e->difficulty > CIV_HOF_DIFFICULTY_MAX)
        return CIV_ERR_RANGE;
    diff = hof_diff_names(e->difficulty);
    if (!diff)
        return CIV_ERR_RANGE;
    era = e->year < 0 ? "BC" : "AD";
    abs_year = e->year < 0 ? -(long)e->year : (long)e->year;
    /* HoF_Show rating: score * (difficulty + 1) / 50 + "%" (DIV 0x32 @
     * 1080:0450; "%" @ 1420:1589). TryInsert ordering weighs score *
     * (difficulty + 2) instead (verified @ 1080:05b0-05df). */
    rating = (long)e->score * (long)(e->difficulty + 1) / 50L;
    n = snprintf(out, cap, "%d. %s of the %s (%s) - %ld %s - Score %d - %s%ld%%",
                 index + 1, e->name, e->civ, diff, abs_year, era, e->score,
                 CIV_HOF_RATING_LABEL, rating);
    if (n < 0 || (size_t)n + 1u > cap)
        return CIV_ERR_RANGE;
    return CIV_OK;
}

/* Orig: FUN_1080_0595 @ 1080:0595 -> HoF_TryInsert.
 * Ordering core ported 1:1 (linear weighted scan + shift down via
 * memmove + copy); tail calling HoF_Show @ 1080:0119 is still a stub and
 * is attempted best-effort without overriding the data-path result.
 * Like orig (@ 1080:05e3-05e6: CMP SI,5 / JZ insert), the last slot
 * always takes when reached, so out_index is always 0..5. */
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
        /* Orig @ 1080:05df-05e6: JL insert, else SI==5 inserts anyway. */
        if (new_w > cur_w || i == CIV_HOF_SLOTS - 1) {
            idx = i;
            break;
        }
    }
    /* Shift down: orig moves tail slots one step (far memmove over 0x27-word
     * stride via Util_MemCopyFar @ 1000:137f: dst DI*0x4e+0x52a8 =
     * src + 0x4e); last entry drops off. */
    if (idx < CIV_HOF_SLOTS - 1)
        memmove(&slots[idx + 1], &slots[idx],
                (size_t)(CIV_HOF_SLOTS - 1 - idx) * sizeof(slots[0]));
    memcpy(&slots[idx], result, sizeof(slots[0]));
    *out_index = idx;
    /* Stub tail: show the updated table (dialog 0x94 TBD). */
    (void)HoF_Show(slots);
    return CIV_OK;
}

/* Raw famehall.civ codec (field map in halloffame.h). Unknown header
 * words (w2/w4/w5/w6) have no decoded home yet: encode writes 0,
 * decode ignores them. */

civ_error_t hof_encode(const civ_hof_entry_t slots[CIV_HOF_SLOTS],
                        uint8_t out[CIV_HOF_RAW_SIZE])
{
    int i;

    if (!slots || !out)
        return CIV_ERR_NOENT;
    for (i = 0; i < CIV_HOF_SLOTS; i++) {
        uint8_t *p = out + (size_t)i * CIV_HOF_SLOT_RAW;
        size_t n;

        memset(p, 0, CIV_HOF_SLOT_RAW);
        if (!slots[i].present || slots[i].score < 0) {
            /* HoF_Clear state: score word 0xffff, rest zero. */
            hof_put_u16le(p, 0xffffu);
            continue;
        }
        if (slots[i].score > 0xffff)
            return CIV_ERR_RANGE;
        hof_put_u16le(p, (uint16_t)slots[i].score);
        hof_put_u16le(p + 2, (uint16_t)slots[i].difficulty);
        /* +4 aux, +8/+10 unk, +12 pop/civ: 0 (TBD, see header). */
        hof_put_u16le(p + 6, (uint16_t)(int16_t)slots[i].year);
        n = hof_name_len(slots[i].name);
        memcpy(p + 14, slots[i].name, n);
        n = hof_name_len(slots[i].civ);
        memcpy(p + 46, slots[i].civ, n);
    }
    return CIV_OK;
}

civ_error_t hof_decode(const uint8_t raw[CIV_HOF_RAW_SIZE],
                        civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    int i;
    size_t n;

    if (!raw || !slots)
        return CIV_ERR_NOENT;
    for (i = 0; i < CIV_HOF_SLOTS; i++) {
        const uint8_t *p = raw + (size_t)i * CIV_HOF_SLOT_RAW;
        uint16_t score = hof_get_u16le(p);

        if (score == 0xffffu) {
            slots[i].present = 0;
            slots[i].score = CIV_HOF_EMPTY_SCORE;
            slots[i].difficulty = -1;
            slots[i].year = -1;
            slots[i].name[0] = '\0';
            slots[i].civ[0] = '\0';
            continue;
        }
        slots[i].present = 1;
        slots[i].score = (int)score;
        slots[i].difficulty = (int)hof_get_u16le(p + 2);
        slots[i].year = (int)(int16_t)hof_get_u16le(p + 6);
        /* Raw names are NUL-terminated C strings (Util_StrCopy in orig);
         * clamp into the 48 B decoded fields. */
        n = 0;
        while (n < CIV_HOF_NAME_RAW && p[14 + n] != '\0')
            n++;
        if (n > sizeof(slots[i].name) - 1u)
            n = sizeof(slots[i].name) - 1u;
        memcpy(slots[i].name, p + 14, n);
        slots[i].name[n] = '\0';
        n = 0;
        while (n < CIV_HOF_NAME_RAW && p[46 + n] != '\0')
            n++;
        if (n > sizeof(slots[i].civ) - 1u)
            n = sizeof(slots[i].civ) - 1u;
        memcpy(slots[i].civ, p + 46, n);
        slots[i].civ[n] = '\0';
    }
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
