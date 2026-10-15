/* CivLin RSC dump validation Y2 (local-only, never committed output).
 *
 * Usage: rsc_dump [file.rsc ...]
 *   Default (no args): the 5 known files under unpacked/ (CivData0..4.rsc,
 *   Orig ResMgr_LoadRsc @ 1190:0000 x5 from WinMain @ 1008:0000).
 *
 * Y2 validation per file, in order (read-only, never writes to
 * civ1win/ or unpacked/ or anywhere on disk; OS-free loader calls):
 *   1. rsc_open(): magic 00 00 01 00 @ +0 + marker ("Civ Data "/"rsrcRSED"/
 *      "CvMM") + size (largest observed: Civdata3.rsc ~1.4 MiB); report
 *      size/record_count (v1 scan listing)/image_count (strict gate).
 *   2. Directory slice walk: rsc_record_data() per index (BE EOF dir,
 *      bounds file_off+len <= size, borrowed, len 0 derives from next
 *      file-off, last to dir_off) + rsc_decode_bitmap() strict gate
 *      (w/h > 0, bpp == 8, pal_count 1..255, every pixel < pal_count);
 *      report dir_records/slices/images_strict. Failures are reported,
 *      not hidden (TRUNCATED/RANGE/BAD_MAGIC skip, not images).
 *   3. Buffered manager walk: rsc_mgr_new() + rsc_load_rsc() (ResMgr_LoadRsc
 *      @ 1190:0000, 0xE360 buffered EOF dir, BE, bounds-checked) +
 *      rsc_dump_stats() totals + rsc_mgr_free(); proves the same 12-B
 *      records -> 0x32-B nodes path the game uses, without touching
 *      load-walk impls here.
 *   4. Synthetic palette-path test (no assets): 4x2 indexed image with a
 *      4-entry palette through gfx_indexed_to_surface(), verified by
 *      reading format/dimensions/pixels/palette back.
 *   5. One frame on screen: synthetic 64x64 palette gradient via
 *      gfx_indexed_to_texture() + SDL_RenderTexture, ~60 frames or until
 *      QUIT/key. Works under SDL_VIDEODRIVER=dummy (no crash = pass).
 *
 * Exit 0 iff all files validate AND the synthetic tests pass. Return value
 * is the process exit code only (no exit() in libs). No globals: only
 * kDefaultFiles[] (const table) + stack/heap locals.
 */
#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx_indexed.h"
#include "rsc_loader.h"

static const char *kDefaultFiles[] = {
    "unpacked/civdata0.rsc",
    "unpacked/CIVDATA1.RSC",
    "unpacked/CIVDATA2.RSC",
    "unpacked/Civdata3.rsc",
    "unpacked/CIVDATA4.RSC",
};

/* Y2 dir-slice + strict-gate walk (read-only, OS-free loader calls).
 * Counts directory records via rsc_record_data() until RANGE (end) or
 * UNSUPPORTED (dir walk unproven), then gates each slice via
 * rsc_decode_bitmap() (borrowed view: pixels points into the container,
 * palette copied tail-zeroed; requires w/h > 0, bpp == 8, pal 1..255,
 * every pixel < pal_count). Prints one summary line per file.
 * AA2 probe (read-only, OS-free, no asset writes): logs first-16B head_hex
 * via gfx_probe_head_hex + probe reason w/h/bpp/pal via gfx_probe_rsc_image
 * for the first up to 4 slices per file (called once per file, so all 5
 * files are covered; civdata0 carries 5 types so first-4 samples the type
 * variety without flooding 454-record logs). Head bytes distinguish CvPc
 * RLE frames (expand via gfx_decode_rle, then re-probe raw via
 * gfx_decode_raw) from raw INDEX8 (6 + 3*pal + w*h, see gfx_indexed.h). */
static int dump_validate_images(const rsc_file_t *f, const char *path)
{
    size_t dir_total = 0;
    size_t slices_ok = 0;
    size_t images = 0;

    for (size_t i = 0; i < 8192u; i++) {
        const uint8_t *data = NULL;
        size_t size = 0;
        rsc_indexed_t tmp;
        rsc_error_t dr = rsc_record_data(f, i, &data, &size);

        if (dr == RSC_ERR_RANGE)
            break;
        if (dr == RSC_ERR_UNSUPPORTED)
            break;
        if (dr != RSC_OK)
            continue;
        dir_total++;
        slices_ok++;
        if (rsc_decode_bitmap(data, size, &tmp) == RSC_OK)
            images++;
    }
    printf("  dir_records=%zu slices=%zu images_strict=%zu (gate: borrowed + pal 1..255 + px<pal)\n",
           dir_total, slices_ok, images);
    /* AA2 per-file probe: head16 + raw-gate reason (read-only, OS-free). */
    {
        size_t nlog = dir_total < 4u ? dir_total : 4u;

        for (size_t k = 0; k < nlog; k++) {
            const uint8_t *data = NULL;
            size_t size = 0;
            char head[64];
            uint32_t w = 0;
            uint32_t h = 0;
            uint8_t bpp = 0;
            uint8_t pal = 0;
            civ_error_t pr;

            if (rsc_record_data(f, k, &data, &size) != RSC_OK)
                continue;
            gfx_probe_head_hex(head, sizeof(head), data, size, 16);
            pr = gfx_probe_rsc_image(data, size, &w, &h, &bpp, &pal);
            printf("  rec[%zu] head16=%s probe=%s w=%u h=%u bpp=%u pal=%u\n",
                   k, head, civ_strerror(pr), w, h, bpp, pal);
        }
    }
    /* Spot-check the borrowed indexed view on record 0 when present. */
    if (dir_total > 0) {
        rsc_indexed_t img;
        rsc_error_t gr = rsc_get_indexed(f, 0, &img);

        if (gr == RSC_OK)
            printf("  get_indexed[0]: ok %ux%u bpp=%u pal=%u (borrowed view)\n",
                   img.width, img.height, img.bpp, img.pal_count);
        else
            printf("  get_indexed[0]: %s (not an 8-bit image, skipped)\n",
                   rsc_strerror(gr));
    }
    (void)path;
    return 0;
}

/* AB2 STR# caller-side entry fetch (rsc_strlist_get plan, OS-free,
 * read-only, no SDL, no exit, no globals). Orig: FUN_1198_xxxx @
 * 1198:xxxx Rsc_DecodeStrList entry slot (Pascal u8 len + bytes, count BE
 * via ResMgr_ReadU16 @ 1190:1953). sl is a borrowed view from
 * rsc_decode_strlist (valid while the container lives); idx is
 * zero-based. Returns RSC_OK with *s_out / *len_out on the idx-th entry,
 * RSC_ERR_NOENT on NULL sl/s_out (NULL len_out = skip), RSC_ERR_RANGE
 * on idx >= count or a truncated walk. */
static rsc_error_t dump_strlist_get(const rsc_strlist_t *sl, unsigned idx,
                                    const uint8_t **s_out, size_t *len_out)
{
    const uint8_t *p = NULL;
    size_t remaining = 0;
    unsigned k = 0;

    if (!sl || !s_out)
        return RSC_ERR_NOENT;
    if ((size_t)idx >= (size_t)sl->count)
        return RSC_ERR_RANGE;
    p = sl->body;
    remaining = sl->body_len;
    for (k = 0; k < idx; k++) {
        uint8_t len = 0;

        if (remaining < 1)
            return RSC_ERR_RANGE;
        len = *p;
        p++;
        remaining--;
        if ((size_t)len > remaining)
            return RSC_ERR_RANGE;
        p += (size_t)len;
        remaining -= (size_t)len;
    }
    if (remaining < 1)
        return RSC_ERR_RANGE;
    {
        uint8_t len = *p;

        if ((size_t)len > remaining - 1u)
            return RSC_ERR_RANGE;
        *s_out = p + 1;
        if (len_out)
            *len_out = (size_t)len;
    }
    return RSC_OK;
}

/* AB2 STR# full walk (read-only, OS-free, no SDL, no exit, no globals).
 * Orig: FUN_1198_xxxx @ 1198:xxxx Rsc_DecodeStrList (u16 count BE @ +0,
 * then count x u8 len + bytes). Walks every directory slice via
 * rsc_record_data (borrowed, valid while rsc_file lives) and decodes via
 * rsc_decode_strlist; non-STR# slices fail decode and are skipped (not
 * images, never fail validation). On civdata0 this hits all 32 STR#
 * recs (5-type variety: STR#/TEXT/GDAT/KDAT/CvPc); on CvD1/2/3/4 there
 * are no STR# slices so nothing logs. Each string logs as
 * strlist rec[i][k] len=N "text" (%.*s, Pascal len <= 255 fits int).
 * Never writes civ1win//unpacked/ (borrowed slices only). Returns 0. */
static int dump_strlist_all(const rsc_file_t *f)
{
    size_t decoded = 0;

    if (!f)
        return 0;
    for (size_t i = 0; i < 8192u; i++) {
        const uint8_t *data = NULL;
        size_t size = 0;
        rsc_strlist_t sl;
        rsc_error_t dr = rsc_record_data(f, i, &data, &size);

        if (dr == RSC_ERR_RANGE || dr == RSC_ERR_UNSUPPORTED)
            break;
        if (dr != RSC_OK)
            continue;
        if (rsc_decode_strlist(data, size, &sl) != RSC_OK)
            continue;
        decoded++;
        printf("  strlist rec[%zu]: count=%u\n", i, (unsigned)sl.count);
        for (unsigned k = 0; k < (unsigned)sl.count; k++) {
            const uint8_t *s = NULL;
            size_t slen = 0;

            if (dump_strlist_get(&sl, k, &s, &slen) != RSC_OK)
                continue;
            printf("    strlist rec[%zu][%u] len=%zu \"%.*s\"\n", i, k,
                   slen, (int)slen, (const char *)s);
        }
    }
    if (decoded == 0)
        printf("  strlist: no STR# slices\n");
    return 0;
}

/* AB2 TEXT caller-side needle search (rsc_text_find plan, OS-free,
 * read-only, no SDL, no exit, no globals). Orig: FUN_1198_xxxx @
 * 1198:xxxx Rsc_DecodeText slot (single NUL-terminated C string per
 * slice hypothesis; 398 TEXT recs civdata0 unk 0x01b6). t is a borrowed
 * view from rsc_decode_text (text[len] is the NUL inside the slice);
 * needle is a plain C string. Returns pointer into t->text on hit, NULL
 * on NULL args or miss (strstr on the NUL-terminated borrowed text). */
static const char *dump_text_find(const rsc_text_t *t, const char *needle)
{
    if (!t || !t->text || !needle)
        return NULL;
    return strstr(t->text, needle);
}

/* AB2 TEXT full walk (read-only, OS-free, no SDL, no exit, no globals).
 * Orig: FUN_1198_xxxx @ 1198:xxxx Rsc_DecodeText (one dialog string per
 * slice; e.g. menu @ 1420:435b, difficulty @ 2a5a). Walks every slice
 * via rsc_record_data (borrowed) + rsc_decode_text; non-TEXT slices fail
 * decode and are skipped. Logs only " 90%" hits via dump_text_find
 * (rsc_text_find plan) to avoid the 398-line flood: at most 10 hit lines,
 * then a hits/logged summary. Never writes assets. Returns 0. */
static int dump_text_all(const rsc_file_t *f)
{
    size_t hits = 0;
    size_t logged = 0;

    if (!f)
        return 0;
    for (size_t i = 0; i < 8192u; i++) {
        const uint8_t *data = NULL;
        size_t size = 0;
        rsc_text_t txt;
        rsc_error_t dr = rsc_record_data(f, i, &data, &size);

        if (dr == RSC_ERR_RANGE || dr == RSC_ERR_UNSUPPORTED)
            break;
        if (dr != RSC_OK)
            continue;
        if (rsc_decode_text(data, size, &txt) != RSC_OK)
            continue;
        if (!dump_text_find(&txt, " 90%"))
            continue;
        hits++;
        if (logged < 10u) {
            printf("  text rec[%zu]: \"%s\"\n", i, txt.text);
            logged++;
        }
    }
    if (hits > 0)
        printf("  text \" 90%%\" hits=%zu (logged %zu, cap 10)\n", hits,
               logged);
    else
        printf("  text \" 90%%\" hits=0\n");
    return 0;
}

/* AD1 try-fail reason strings (read-only, OS-free, no SDL, no exit,
 * no globals). Maps GFX_TRY_FAIL_* (gfx_indexed.h, read via Read, not
 * edited: NONE/TRUNC_SRC/TAG_OVERRUN/DST_OVERRUN shared by gfx_try_rll /
 * gfx_try_rll_inv / gfx_try_lzss_edi @ 10f8:040a + EDI LZSS2) to log
 * names. Returns "UNKNOWN" for out-of-range codes. Borrowed string
 * literals, never writes assets. */
static const char *dump_try_fail_str(int fail)
{
    switch (fail) {
    case GFX_TRY_FAIL_NONE:
        return "NONE";
    case GFX_TRY_FAIL_TRUNC_SRC:
        return "TRUNC_SRC";
    case GFX_TRY_FAIL_TAG_OVERRUN:
        return "TAG_OVERRUN";
    case GFX_TRY_FAIL_DST_OVERRUN:
        return "DST_OVERRUN";
    default:
        return "UNKNOWN";
    }
}

/* AB2 CvPc RLE probe, AD1 extended, AF1 best log (read-only, OS-free,
 * no SDL, no exit, no globals).
 * Orig: FUN_10f8_040a @ 10f8:040a RLLDecode (via gfx_decode_rle tag
 * < 0x80 repeat / >= 0x80 literal) + FUN_1198_xxxx @ 1198:xxxx raw gate
 * (via gfx_probe_rsc_image) + AC2 try variants (via gfx_try_rll /
 * gfx_try_rll_inv / gfx_try_lzss_edi with fail_out @ 10f8:040a/EDI).
 * Confirmed CvPc head BB DF 06 45... is binary RLE, not raw INDEX8 (raw
 * probe reports BAD_DATA/RANGE). Walks directory slices via
 * rsc_record_data (borrowed), filters to BB DF heads (size >= 2,
 * data[0] == 0xBB, data[1] == 0xDF), caps at 4 slices/file (same cap as
 * the head16 probe above) to avoid flooding Civdata3 (148 CvPc). AD1:
 * logs slice len via rsc_slice_len (Rsc_SliceLen @ 1198:xxxx diagnostic;
 * should equal the rsc_record_data size) + head16 via
 * gfx_probe_head_hex + raw probe reason w/h/bpp/pal via
 * gfx_probe_rsc_image, then per-try fail reasons (TRUNC_SRC/TAG_OVERRUN/
 * DST_OVERRUN via gfx_try_* fail_out, read-only reason strings above),
 * then RLE-expand-then-probe via gfx_rle_expand_cvpc into a caller-owned
 * 4 MiB malloc scratch (no globals, freed before return) with expand len
 * + expanded probe w/h/bpp/pal. AF1: additionally calls
 * gfx_rle_expand_best_ex on the same slice reusing the same scratch
 * (trials share the caller dst, winner refilled; order RLL > INV > LZSS
 * > GIF > RAW) with a 256 B stack log_buf, then logs the best line
 * (best=RLL/RLL_INV/LZSS_EDI/GIF/RAW0/2/4/8/NONE + len + slice len +
 * per-method RLL:/INV:/LZSS:/GIF:/RAW0/2/4/8 reasons via
 * gfx_try_fail_str + gfx_expand_method_str, GIF as ok/miss, RAW as probe
 * gate civ_strerror or skip). Never writes assets. */
static int dump_cvpc_probe(const rsc_file_t *f)
{
    const size_t kExpandCap = 4u * 1024u * 1024u;
    uint8_t *expand = NULL;
    size_t logged = 0;

    if (!f)
        return 0;
    expand = (uint8_t *)malloc(kExpandCap);
    if (!expand) {
        printf("  cvpc: out of memory\n");
        return 0;
    }
    for (size_t i = 0; i < 8192u && logged < 4u; i++) {
        const uint8_t *data = NULL;
        size_t size = 0;
        size_t slen = 0;
        char head[64];
        uint32_t w = 0;
        uint32_t h = 0;
        uint8_t bpp = 0;
        uint8_t pal = 0;
        civ_error_t pr;
        size_t rll_len = 0;
        size_t inv_len = 0;
        size_t lzss_len = 0;
        int rll_fail = GFX_TRY_FAIL_NONE;
        int inv_fail = GFX_TRY_FAIL_NONE;
        int lzss_fail = GFX_TRY_FAIL_NONE;
        civ_error_t rll_rc;
        civ_error_t inv_rc;
        civ_error_t lzss_rc;
        int exp_len = -1;
        uint32_t ew = 0;
        uint32_t eh = 0;
        uint8_t ebpp = 0;
        uint8_t epal = 0;
        civ_error_t epr;
        rsc_error_t dr = rsc_record_data(f, i, &data, &size);

        if (dr == RSC_ERR_RANGE || dr == RSC_ERR_UNSUPPORTED)
            break;
        if (dr != RSC_OK)
            continue;
        if (size < 2 || data[0] != 0xBBu || data[1] != 0xDFu)
            continue;
        slen = rsc_slice_len(f, i);
        gfx_probe_head_hex(head, sizeof(head), data, size, 16);
        pr = gfx_probe_rsc_image(data, size, &w, &h, &bpp, &pal);
        rll_rc =
            gfx_try_rll(data, size, expand, kExpandCap, &rll_len,
                        &rll_fail);
        inv_rc =
            gfx_try_rll_inv(data, size, expand, kExpandCap, &inv_len,
                            &inv_fail);
        lzss_rc =
            gfx_try_lzss_edi(data, size, expand, kExpandCap, &lzss_len,
                             &lzss_fail);
        exp_len =
            gfx_rle_expand_cvpc(data, size, expand, kExpandCap, &ew, &eh,
                                &ebpp, &epal);
        if (exp_len < 0) {
            printf("  cvpc rec[%zu] len=%zu head16=%s probe=%s w=%u h=%u bpp=%u pal=%u rll=%s/%s len=%zu rll_inv=%s/%s len=%zu lzss=%s/%s len=%zu expand=FAIL\n",
                   i, slen, head, civ_strerror(pr), w, h, bpp, pal,
                   civ_strerror(rll_rc), dump_try_fail_str(rll_fail),
                   rll_len, civ_strerror(inv_rc),
                   dump_try_fail_str(inv_fail), inv_len,
                   civ_strerror(lzss_rc), dump_try_fail_str(lzss_fail),
                   lzss_len);
        } else {
            epr = gfx_probe_rsc_image(expand, (size_t)exp_len, &ew, &eh,
                                      &ebpp, &epal);
            printf("  cvpc rec[%zu] len=%zu head16=%s probe=%s w=%u h=%u bpp=%u pal=%u rll=%s/%s len=%zu rll_inv=%s/%s len=%zu lzss=%s/%s len=%zu expand=%d exp_probe=%s ew=%u eh=%u ebpp=%u epal=%u\n",
                   i, slen, head, civ_strerror(pr), w, h, bpp, pal,
                   civ_strerror(rll_rc), dump_try_fail_str(rll_fail),
                   rll_len, civ_strerror(inv_rc),
                   dump_try_fail_str(inv_fail), inv_len,
                   civ_strerror(lzss_rc), dump_try_fail_str(lzss_fail),
                   lzss_len, exp_len, civ_strerror(epr), ew, eh, ebpp,
                   epal);
        }
        /* AF1 best log (OS-free, read-only, reuses the scratch above:
         * trials share the caller dst, winner refilled at the end, so no
         * second malloc and no globals; log_buf is a 256 B stack line).
         * Logs slice len + best method id via gfx_expand_method_str +
         * winning raw/gif offset + winning w/h/bpp/pal + the one-line
         * summary (best= + len + RLL:/INV:/LZSS:/GIF:/RAW0/2/4/8 via
         * gfx_try_fail_str inside gfx_rle_expand_best_ex). */
        {
            char best_log[256];
            int best_method = GFX_EXPAND_NONE;
            size_t best_off = 0;
            uint32_t bw = 0;
            uint32_t bh = 0;
            uint8_t bbpp = 0;
            uint8_t bpal = 0;
            int bfail_rll = GFX_TRY_FAIL_NONE;
            int bfail_inv = GFX_TRY_FAIL_NONE;
            int bfail_lzss = GFX_TRY_FAIL_NONE;
            int best_len = gfx_rle_expand_best_ex(
                data, size, expand, kExpandCap, &best_method, &best_off,
                &bw, &bh, &bbpp, &bpal, &bfail_rll, &bfail_inv,
                &bfail_lzss, best_log, sizeof(best_log));

            best_log[sizeof(best_log) - 1] = '\0';
            printf("  cvpc rec[%zu] best slice_len=%zu best_len=%d method=%s raw_off=%zu w=%u h=%u bpp=%u pal=%u fails=RLL:%s/INV:%s/LZSS:%s %s\n",
                   i, slen, best_len,
                   gfx_expand_method_str(best_method), best_off, bw, bh,
                   bbpp, bpal, gfx_try_fail_str(bfail_rll),
                   gfx_try_fail_str(bfail_inv),
                   gfx_try_fail_str(bfail_lzss), best_log);
        }
        logged++;
    }
    if (logged == 0)
        printf("  cvpc: no BB DF slices\n");
    free(expand);
    return 0;
}

/* AD1 TEXT stats dump (read-only, OS-free, no SDL, no exit, no globals).
 * Orig: FUN_1198_xxxx @ 1198:xxxx Rsc_TextStats aggregate (TEXT 398 recs
 * civdata0, unk 0x01b6; menu @ 1420:435b, difficulty @ 1420:2a5a) via
 * rsc_text_stats (Rsc_TextStats @ 1198:xxxx: total strings via
 * rsc_text_index per record + total chars + longest len + hits for
 * " 90%" via rsc_text_find). Walks slices via rsc_record_data
 * (borrowed, valid while rsc_file lives) + rsc_decode_text; caps detail
 * lines at 4 (same cap as the head16 probe; avoids the 398-line flood;
 * each preview capped at 32 chars) and always prints the summary line
 * (even when no TEXT slices, e.g. CvD1/2/3/4 CvPc-only). Detail shows
 * rec index + NUL len + preview; summary shows strings/chars/longest/
 * hits_90pct from rsc_text_stats. Never writes assets. Returns 0. */
static int dump_text_stats(const rsc_file_t *f)
{
    rsc_text_stats_t st;
    rsc_error_t sr;
    size_t logged = 0;

    if (!f)
        return 0;
    sr = rsc_text_stats(f, &st);
    if (sr != RSC_OK) {
        printf("  text stats: %s\n", rsc_strerror(sr));
        return 0;
    }
    for (size_t i = 0; i < 8192u && logged < 4u; i++) {
        const uint8_t *data = NULL;
        size_t size = 0;
        rsc_text_t txt;
        size_t preview = 0;
        rsc_error_t dr = rsc_record_data(f, i, &data, &size);

        if (dr == RSC_ERR_RANGE || dr == RSC_ERR_UNSUPPORTED)
            break;
        if (dr != RSC_OK)
            continue;
        if (rsc_decode_text(data, size, &txt) != RSC_OK)
            continue;
        preview = txt.len < 32u ? txt.len : 32u;
        printf("  textstat rec[%zu] len=%zu \"%.*s\"\n", i, txt.len,
               (int)preview, txt.text);
        logged++;
    }
    printf("  text stats: strings=%zu chars=%zu longest=%zu hits_90pct=%zu\n",
           st.total_strings, st.total_chars, st.longest_len,
           st.hits_90pct);
    return 0;
}

/* AE2 TEXT full dump (read-only, OS-free, no SDL, no exit, no globals).
 * Orig: FUN_1198_xxxx @ 1198:xxxx Rsc_TextStats aggregate (TEXT 398 recs
 * civdata0, unk 0x01b6; total 14977 strings; menu @ 1420:435b, difficulty
 * @ 1420:2a5a) via rsc_text_stats (total/longest/hits) + Rsc_TextGetAll @
 * 1198:xxxx via rsc_text_get_all (first-N locs, bounds, capped detail) +
 * Rsc_TextSearchAll @ 1198:xxxx via rsc_text_search_all (" 90%" hits=3 in
 * civdata0, 0 others; offsets[3], no flood). Walks borrowed slices via
 * rsc_record_data (valid while rsc_file lives); caps detail at 3 hit
 * lines (same no-flood policy as dump_text_all cap 10 / dump_text_stats
 * cap 4; each context preview capped at 32 chars) and always prints the
 * summary line (even when no TEXT slices, e.g. CvD1/2/3/4 CvPc-only, or
 * when the search reports RANGE truncation). Detail shows rec index +
 * slice offset + 32-char context from the hit; summary shows
 * strings/chars/longest/hits_90pct from rsc_text_stats plus
 * search_total/logged from rsc_text_search_all. Wires rsc_text_get_all
 * (cap 4 count query, cross-checks total vs stats total, no extra lines)
 * so the first-N-offsets path is exercised read-only. Never writes
 * civ1win//unpacked/ (borrowed slices only). Returns 0. */
static int dump_text_full(const rsc_file_t *f)
{
    static const uint8_t kNeedle[] = { ' ', '9', '0', '%' };
    rsc_text_stats_t st;
    rsc_text_loc_t get_locs[4];
    size_t get_n = 0;
    size_t get_total = 0;
    rsc_text_loc_t hits[3];
    size_t hit_n = 0;
    size_t hit_total = 0;
    rsc_error_t sr;
    rsc_error_t gr;
    rsc_error_t qr;
    size_t logged = 0;

    if (!f)
        return 0;
    sr = rsc_text_stats(f, &st);
    if (sr != RSC_OK) {
        printf("  text full: %s\n", rsc_strerror(sr));
        return 0;
    }
    /* Wire rsc_text_get_all read-only (first-N offsets, bounds): cap 4,
     * summary in get_total must match stats total on TEXT files; RANGE
     * means truncated detail with full summary (expected when total > 4).
     * No extra detail lines here (no flood); mismatch is noted only. */
    gr = rsc_text_get_all(f, get_locs, 4u, &get_n, &get_total);
    if (gr != RSC_OK && gr != RSC_ERR_RANGE) {
        printf("  text full get_all: %s\n", rsc_strerror(gr));
    } else if (get_total != st.total_strings) {
        printf("  text full get_all: total=%zu vs stats=%zu (mismatch note)\n",
               get_total, st.total_strings);
    }
    (void)get_n;
    (void)get_locs;
    qr = rsc_text_search_all(f, kNeedle, sizeof(kNeedle), hits, 3u,
                             &hit_n, &hit_total);
    if (qr != RSC_OK && qr != RSC_ERR_RANGE) {
        printf("  text full search: %s\n", rsc_strerror(qr));
    } else {
        for (size_t k = 0; k < hit_n; k++) {
            const uint8_t *data = NULL;
            size_t size = 0;
            size_t preview = 0;

            if (rsc_record_data(f, hits[k].rec, &data, &size) != RSC_OK)
                continue;
            if (hits[k].off >= size)
                continue;
            preview = size - hits[k].off;
            if (preview > 32u)
                preview = 32u;
            printf("  text full hit[%zu] rec[%zu]+%zu \"%.*s\"\n", k,
                   hits[k].rec, hits[k].off, (int)preview,
                   (const char *)(data + hits[k].off));
            logged++;
        }
    }
    printf("  text full: strings=%zu chars=%zu longest=%zu hits_90pct=%zu (search_total=%zu logged %zu, cap 3)\n",
           st.total_strings, st.total_chars, st.longest_len,
           st.hits_90pct, hit_total, logged);
    return 0;
}

/* AE2 godpal search run (read-only, OS-free, no SDL, no exit, no globals).
 * Orig: godpal.c 0xec RGB entries feeding Palette_Init @ 1090:0488
 * (LOGPALETTE 0x300 -> CREATEPALETTE -> DAT_1420_9966 @ 1420:9966; see
 * gfx_indexed.h) via gfx_godpal_search (AD2 implemented, 00 03 EC 00
 * anchor, 4 KiB sliding window, 3-B overlap). Calls
 * gfx_godpal_search("unpacked/civ.exe", &off) read-only (unpacked/
 * local-only, never committed, never written; caller passes the path).
 * Logs the hit offset (hex + dec) or the miss plus the next dd/xxd steps
 * (no writes here, no fake table: the 0x2c4/0x3B4 dump + Ghidra
 * DAT_1420_9966 order/count + "godpal.c" neighbour check happen after the
 * hit, before any static const kGodpal table is emitted, never here).
 * Diagnostic only: always returns 0 (never fails validation). */
static int dump_godpal_search_run(void)
{
    static const char *kExe = "unpacked/civ.exe";
    long off = 0;
    civ_error_t rc = gfx_godpal_search(kExe, &off);

    if (rc == CIV_OK) {
        printf("  godpal: hit off=0x%lx (%ld) anchor 00 03 EC 00\n",
               (unsigned long)off, off);
        printf("  godpal next: dd if=unpacked/civ.exe bs=1 skip=%ld count=708 | xxd -i  # 0x2c4 RGB (flags dropped)\n",
               off + 4L);
        printf("  godpal next: dd if=unpacked/civ.exe bs=1 skip=%ld count=948 | xxd  # 0x3B4 LOGPALETTE with header\n",
               off);
        printf("  godpal next: Ghidra CivLin.gpr civ.exe DAT_1420_9966 @1420:9966 order/count 0xec + godpal.c neighbour + LOGPALETTE 0x300 tail\n");
    } else if (rc == CIV_ERR_NOENT) {
        if (off == -1L) {
            printf("  godpal: miss (no 00 03 EC 00 anchor, scanned to EOF)\n");
            printf("  godpal next: Ghidra CivLin.gpr civ.exe DAT_1420_9966 @1420:9966 order/count 0xec + godpal.c neighbour\n");
        } else {
            printf("  godpal: %s (bad path)\n", civ_strerror(rc));
        }
    } else {
        printf("  godpal: %s (unpacked/civ.exe local-only, never committed)\n",
               civ_strerror(rc));
    }
    return 0;
}

/* AF1 BMP probe (read-only, OS-free parse via gfx_bmp_parse /
 * gfx_bmp_load_indexed, no SDL, no exit, no globals). MPLOGO.BMP has NO
 * filename string in civ.exe (NE resource or RSC frame, NOT a file load;
 * see title.h) so the Linux port probes explicit paths in order:
 * unpacked/MPLOGO.BMP then civ1win/MPLOGO.BMP (both read-only, never
 * written; fopen "rb" only inside gfx_bmp_load_indexed). Logs the BMP
 * header values (w/h/bpp from the DIB; expected 380x97x1 PC bitmap per
 * file(1): 380 x 97 x 1, image size 4656, cbSize 4718, bits offset 62;
 * byte dump: 42 4D "BM", fsize 4718, off 62, DIB 40, planes 1, comp 0
 * BI_RGB, palette black 00 00 00 / white FF FF FF, stride 48) + the
 * indexed view dims/palette + first-bytes head via gfx_probe_head_hex.
 * Diagnostic only: always returns 0 (never fails validation, so the
 * synthetic surface/texture ok lines below still gate the PASS). */
static int dump_bmp_probe(void)
{
    static const char *kPaths[2] = {
        "unpacked/MPLOGO.BMP",
        "civ1win/MPLOGO.BMP",
    };
    size_t pi;

    for (pi = 0; pi < 2u; pi++) {
        const char *path = kPaths[pi];
        FILE *fp = fopen(path, "rb");
        uint8_t *raw = NULL;
        long fsize = 0;
        size_t size = 0;
        size_t nread = 0;
        gfx_bmp_t bmp;
        civ_error_t pr;
        rsc_indexed_t img;
        uint8_t *px = NULL;
        civ_error_t lr;
        char head[64];

        if (!fp)
            continue;
        if (fseek(fp, 0, SEEK_END) != 0) {
            fclose(fp);
            continue;
        }
        fsize = ftell(fp);
        if (fsize < 0) {
            fclose(fp);
            continue;
        }
        size = (size_t)fsize;
        if (size == 0) {
            fclose(fp);
            continue;
        }
        if (fseek(fp, 0, SEEK_SET) != 0) {
            fclose(fp);
            continue;
        }
        raw = (uint8_t *)malloc(size);
        if (!raw) {
            fclose(fp);
            printf("  bmp %s: out of memory\n", path);
            return 0;
        }
        nread = fread(raw, 1, size, fp);
        fclose(fp);
        if (nread != size) {
            free(raw);
            printf("  bmp %s: short read\n", path);
            return 0;
        }
        gfx_probe_head_hex(head, sizeof(head), raw, size, 16);
        memset(&bmp, 0, sizeof(bmp));
        pr = gfx_bmp_parse(raw, size, &bmp);
        if (pr != CIV_OK) {
            printf("  bmp %s: size=%zu head16=%s parse=%s\n", path, size,
                   head, civ_strerror(pr));
            free(raw);
            return 0;
        }
        printf("  bmp %s: size=%zu head16=%s parse=ok w=%u h=%u bpp=%u pal=%u stride=%zu off=%zu top_down=%d pal0=%02X%02X%02X pal1=%02X%02X%02X\n",
               path, size, head, bmp.width, bmp.height, bmp.bpp,
               bmp.pal_count, bmp.row_stride, bmp.pixel_offset,
               bmp.top_down, bmp.pal[0][0], bmp.pal[0][1], bmp.pal[0][2],
               bmp.pal[1][0], bmp.pal[1][1], bmp.pal[1][2]);
        memset(&img, 0, sizeof(img));
        lr = gfx_bmp_load_indexed(path, &img, &px);
        if (lr != CIV_OK) {
            printf("  bmp %s: load=%s\n", path, civ_strerror(lr));
            free(raw);
            return 0;
        }
        printf("  bmp %s: indexed %ux%u bpp=%u pal=%u (1-bpp bit unpack, 4B pad rows)\n",
               path, img.width, img.height, img.bpp, img.pal_count);
        free(px);
        free(raw);
        return 0;
    }
    printf("  bmp: MPLOGO.BMP not found (unpacked/ + civ1win/ missing)\n");
    return 0;
}

/* Y2 buffered manager walk (read-only): ResMgr_LoadRsc @ 1190:0000 via
 * rsc_load_rsc() on the same path (no writes, no globals, no exit).
 * Prints type/resource totals; failure here fails validation. */
static int dump_validate_mgr(const char *path)
{
    rsc_mgr_t *mgr = rsc_mgr_new();
    rsc_error_t lr;
    int rc = 0;

    if (!mgr) {
        printf("  load_rsc: out of memory\n");
        return 1;
    }
    lr = rsc_load_rsc(mgr, path);
    if (lr != RSC_OK) {
        printf("  load_rsc: %s\n", rsc_strerror(lr));
        rc = 1;
    } else {
        printf("  load_rsc: ok files=1 types=%u\n",
               (unsigned)rsc_get_type_count(mgr));
        if (rsc_dump_stats(mgr, stdout) != RSC_OK)
            rc = 1;
    }
    rsc_mgr_free(mgr);
    return rc;
}

static int dump_one(const char *path)
{
    rsc_file_t *f = NULL;
    rsc_error_t rc = rsc_open(path, &f);
    int failures = 0;

    if (rc != RSC_OK) {
        printf("%s: open failed: %s\n", path, rsc_strerror(rc));
        return 1;
    }
    /* size/record_count (v1 scan listing)/image_count (X1 strict gate:
     * borrowed view + pal 1..255 + pixel < pal_count). */
    printf("%s: size=%zu records=%zu images=%zu\n", path, rsc_size(f),
           rsc_record_count(f), rsc_image_count(f));
    for (size_t i = 0; i < rsc_record_count(f); i++) {
        rsc_record_t rec;
        if (rsc_record_info(f, i, &rec) == RSC_OK)
            printf("  rec[%zu] type=%s entry=0x%x unknown_a=0x%08x unknown_b=0x%08x\n",
                   i, rec.type, rec.entry_offset, rec.unknown_a,
                   rec.unknown_b);
    }
    failures |= dump_validate_images(f, path);
    /* AB2 per-file dumps (read-only, OS-free; civdata0 covers the 5-type
     * variety STR#/TEXT/GDAT/KDAT/CvPc; synthetic ok below still passes
     * as these return 0 and never fail validation on skips). */
    failures |= dump_strlist_all(f);
    failures |= dump_text_all(f);
    failures |= dump_text_stats(f);
    failures |= dump_text_full(f);
    failures |= dump_cvpc_probe(f);
    rsc_close(f);
    failures |= dump_validate_mgr(path);
    return failures ? 1 : 0;
}

/* 4x2 synthetic indexed image: indices 0..3, palette R/G/B/white. */
static int test_surface_bridge(void)
{
    static const uint8_t kPix[8] = {0, 1, 2, 3, 3, 2, 1, 0};
    rsc_indexed_t img;
    SDL_Surface *surf = NULL;
    int failures = 0;

    memset(&img, 0, sizeof(img));
    img.width = 4;
    img.height = 2;
    img.bpp = 8;
    img.pal_count = 4;
    img.pal[0][0] = 255;
    img.pal[1][1] = 255;
    img.pal[2][2] = 255;
    img.pal[3][0] = img.pal[3][1] = img.pal[3][2] = 255;
    img.pixels = kPix;

    surf = gfx_indexed_to_surface(&img);
    if (!surf) {
        printf("synthetic surface: FAILED (%s)\n", SDL_GetError());
        return 1;
    }
    if (surf->format != SDL_PIXELFORMAT_INDEX8 || surf->w != 4 || surf->h != 2) {
        printf("synthetic surface: FAILED (format/w/h mismatch)\n");
        failures++;
    }
    {
        const uint8_t *px = (const uint8_t *)surf->pixels;
        if (memcmp(px, kPix, sizeof(kPix)) != 0) {
            printf("synthetic surface: FAILED (pixel mismatch)\n");
            failures++;
        }
    }
    {
        SDL_Palette *pal = SDL_GetSurfacePalette(surf);
        if (!pal || pal->ncolors < 4 || pal->colors[0].r != 255 ||
            pal->colors[1].g != 255 || pal->colors[2].b != 255 ||
            pal->colors[3].r != 255) {
            printf("synthetic surface: FAILED (palette mismatch)\n");
            failures++;
        }
    }
    SDL_DestroySurface(surf);
    if (!failures)
        printf("synthetic surface: ok (4x2 indexed + 4-entry palette)\n");
    return failures ? 1 : 0;
}

/* One frame: 64x64 palette gradient through the texture path. */
static int present_frame(SDL_Renderer *ren)
{
    static uint8_t kPix[64 * 64];
    rsc_indexed_t img;
    SDL_Texture *tex = NULL;

    memset(&img, 0, sizeof(img));
    img.width = 64;
    img.height = 64;
    img.bpp = 8;
    img.pal_count = 0; /* filled below */
    for (int i = 0; i < 64; i++) {
        img.pal[i][0] = (uint8_t)(i * 4);
        img.pal[i][1] = (uint8_t)(255 - i * 4);
        img.pal[i][2] = 128;
    }
    img.pal_count = 64;
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++)
            kPix[y * 64 + x] = (uint8_t)((x + y) % 64);
    img.pixels = kPix;

    tex = gfx_indexed_to_texture(ren, &img);
    if (!tex) {
        printf("synthetic texture: FAILED (%s)\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
    SDL_DestroyTexture(tex);
    printf("synthetic texture: ok (64x64 palette gradient, 1 frame)\n");
    return 0;
}

int main(int argc, char *argv[])
{
    int rc = 0;
    int frames = 0;

    const char **files = kDefaultFiles;
    int nfiles = (int)(sizeof(kDefaultFiles) / sizeof(kDefaultFiles[0]));
    if (argc > 1) {
        files = (const char **)(argv + 1);
        nfiles = argc - 1;
    }

    for (int i = 0; i < nfiles; i++)
        rc |= dump_one(files[i]);

    /* AE2 godpal run (read-only, no writes, no fake table; diagnostic, never
     * fails validation). Single run (not per file) to avoid log flood. */
    (void)dump_godpal_search_run();

    /* AF1 BMP probe (read-only, diagnostic, never fails validation; keeps
     * the synthetic ok lines below as the PASS gate). */
    (void)dump_bmp_probe();

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    rc |= test_surface_bridge();
    {
        SDL_Window *win = SDL_CreateWindow("CivLin - rsc smoke", 320, 200, 0);
        SDL_Renderer *ren = NULL;
        if (!win) {
            printf("CreateWindow failed: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }
        ren = SDL_CreateRenderer(win, NULL);
        if (!ren) {
            printf("CreateRenderer failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(win);
            SDL_Quit();
            return 1;
        }
        rc |= present_frame(ren);
        /* Keep the frame up briefly; quit early on input. */
        while (frames < 60) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_EVENT_QUIT ||
                    ev.type == SDL_EVENT_KEY_DOWN)
                    frames = 60;
            }
            SDL_Delay(16);
            frames++;
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
    }
    SDL_Quit();
    printf(rc == 0 ? "rsc_dump: PASS\n" : "rsc_dump: FAIL\n");
    return rc;
}
