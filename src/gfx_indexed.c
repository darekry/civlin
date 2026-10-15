/* CivLin indexed+palette -> SDL3 bridge. See gfx_indexed.h.
 * Z2: OS-free probe helpers (gfx_probe_rsc_image + gfx_probe_head_hex,
 * no SDL) mirror rsc_decode_bitmap @ 1198:xxxx so the images_strict=0
 * backlog (dir 454/14/31/143/1) can be triaged without touching SDL. */
#include "gfx_indexed.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SDL_Surface *gfx_indexed_to_surface(const rsc_indexed_t *img)
{
    /* X1 8-bit + palette gate (consumer half of the rsc_get_indexed
     * producer: rsc_record_data slice + rsc_decode_bitmap @ 1198:xxxx
     * strict gate w/h > 0, bpp == 8, pal 1..255, pixel < pal_count).
     * Rejects non-8-bit, empty, or pixel-less views before touching SDL,
     * so a malformed slice cannot reach an INDEX8 surface; valid
     * producer views flow unchanged (palette via SDL_SetPaletteColors).
     * No behavior break for valid inputs. */
    SDL_Surface *surf = NULL;
    SDL_Palette *pal = NULL;
    SDL_Color colors[256];

    if (!img || !img->pixels || img->width == 0 || img->height == 0 ||
        img->bpp != 8)
        return NULL;

    surf = SDL_CreateSurface((int)img->width, (int)img->height,
                             SDL_PIXELFORMAT_INDEX8);
    if (!surf)
        return NULL;

    if (img->pal_count > 0) {
        pal = SDL_CreateSurfacePalette(surf);
        if (!pal) {
            SDL_DestroySurface(surf);
            return NULL;
        }
        for (int i = 0; i < img->pal_count; i++) {
            colors[i].r = img->pal[i][0];
            colors[i].g = img->pal[i][1];
            colors[i].b = img->pal[i][2];
            colors[i].a = 255;
        }
        if (!SDL_SetPaletteColors(pal, colors, 0, (int)img->pal_count)) {
            SDL_DestroySurface(surf);
            return NULL;
        }
    }

    /* Direct pixel access: lock only if the surface requires it
     * (SDL_MUSTLOCK). INDEX8 software surfaces typically do not. */
    if (SDL_MUSTLOCK(surf) && !SDL_LockSurface(surf)) {
        SDL_DestroySurface(surf);
        return NULL;
    }
    {
        uint8_t *dst = (uint8_t *)surf->pixels;
        int pitch = surf->pitch;
        for (uint32_t y = 0; y < img->height; y++)
            memcpy(dst + (size_t)y * (size_t)pitch,
                   img->pixels + (size_t)y * (size_t)img->width,
                   img->width);
    }
    if (SDL_MUSTLOCK(surf))
        SDL_UnlockSurface(surf);
    return surf;
}

SDL_Texture *gfx_indexed_to_texture(SDL_Renderer *ren, const rsc_indexed_t *img)
{
    SDL_Surface *surf = NULL;
    SDL_Texture *tex = NULL;

    if (!ren)
        return NULL;
    surf = gfx_indexed_to_surface(img);
    if (!surf)
        return NULL;
    tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_DestroySurface(surf);
    return tex;
}

civ_error_t gfx_probe_rsc_image(const uint8_t *data, size_t size,
                                uint32_t *w_out, uint32_t *h_out,
                                uint8_t *bpp_out, uint8_t *pal_out)
{
    /* Z2 probe @ 1190:0a29 (OS-free mirror of rsc_decode_bitmap @
     * 1198:xxxx strict gate): u16 w LE @ +0, u16 h LE @ +2, u8 bpp @ +4
     * == 8, u8 pal @ +5
     * in 1..255, 6 + 3*pal + w*h <= size, every pixel < pal). Out params
     * are filled from the header before validation (NULL = skip) so a
     * caller can log w/h/bpp/pal even on failure. Why dir 454/14/31/143/1
     * yields images_strict=0: none of those directory slices starts with
     * a raw 8-bit header (w/h zero, or bpp != 8, or pal 0, or first pixel
     * >= pal) — the payload looks compressed/RLE or type-tagged at
     * record start, not raw INDEX8, so the strict gate correctly skips it.
     * rsc_dump get_indexed[0] OOR follows: when images_strict=0 the
     * index-0 slice fails this probe (not an 8-bit image, skipped), hence
     * the indexed view reports RANGE/OOR; log the probe reason +
     * gfx_probe_head_hex(head, 16) instead of failing validation. No SDL,
     * no exit, no globals. Orig: FUN_1198_xxxx @ 1198:xxxx +
     * FUN_1190_0a29 @ 1190:0a29. */
    uint32_t w;
    uint32_t h;
    uint8_t bpp;
    uint8_t pal_count;
    size_t pal_bytes;
    size_t px_count;
    size_t headroom;
    size_t i;
    const uint8_t *px;

    if (!data)
        return CIV_ERR_NOENT;
    if (size < 6)
        return CIV_ERR_TRUNCATED;
    w = (uint32_t)data[0] | ((uint32_t)data[1] << 8);
    h = (uint32_t)data[2] | ((uint32_t)data[3] << 8);
    bpp = data[4];
    pal_count = data[5];
    if (w_out)
        *w_out = w;
    if (h_out)
        *h_out = h;
    if (bpp_out)
        *bpp_out = bpp;
    if (pal_out)
        *pal_out = pal_count;
    if (w == 0 || h == 0)
        return CIV_ERR_RANGE;
    if (bpp != 8)
        return CIV_ERR_BAD_DATA;
    if (pal_count == 0)
        return CIV_ERR_BAD_DATA;
    px_count = (size_t)w * (size_t)h;
    pal_bytes = (size_t)pal_count * 3u;
    headroom = size - 6;
    if (pal_bytes > headroom)
        return CIV_ERR_TRUNCATED;
    if (px_count > headroom - pal_bytes)
        return CIV_ERR_TRUNCATED;
    px = data + 6 + pal_bytes;
    for (i = 0; i < px_count; i++) {
        if (px[i] >= pal_count)
            return CIV_ERR_RANGE;
    }
    return CIV_OK;
}

int gfx_probe_head_hex(char *dst, size_t cap,
                       const uint8_t *data, size_t size, size_t n)
{
    /* Z2 carving helper @ 1190:0a29: read-only hex of the first n
     * payload bytes (clamped to size, capped at 32) as "XX ..." into dst
     * (cap incl. NUL, truncated safely). Returns the would-be length (0
     * when want is 0), -1 on NULL dst/data or cap == 0 (mirrors
     * util_format_message @ 1000:2928 contract). Never reads past size,
     * never writes assets; caller passes a borrowed rsc_record_data
     * slice. Next carving: log head[0..15] per CivData file via this
     * helper + the probe reason above, then Ghidra-compare vs the 1198
     * decoder before touching any decoder. No SDL, no exit, no globals.
     * Orig slice: FUN_1190_0a29 @ 1190:0a29. */
    static const char kHex[] = "0123456789ABCDEF";
    size_t want;
    size_t total;
    size_t pos = 0;
    size_t i;

    if (!dst || !data || cap == 0)
        return -1;
    want = n;
    if (want > size)
        want = size;
    if (want > 32)
        want = 32;
    total = (want == 0) ? 0 : (want * 3u - 1u);
    for (i = 0; i < want; i++) {
        if (pos + 2 >= cap)
            break;
        dst[pos++] = kHex[((unsigned)data[i] >> 4) & 0xFu];
        dst[pos++] = kHex[(unsigned)data[i] & 0xFu];
        if (i + 1 < want) {
            if (pos + 1 >= cap)
                break;
            dst[pos++] = ' ';
        }
    }
    if (pos < cap)
        dst[pos] = '\0';
    else
        dst[cap - 1] = '\0';
    return (int)total;
}

civ_error_t gfx_decode_rle(const uint8_t *src, size_t src_len,
                           uint8_t *dst, size_t dst_cap, size_t *out_len)
{
    /* AA2 CvPc RLE expander @ 10f8:040a (OS-free 1:1 port of
     * save_rll_decode in save.c, read via Read for reuse: tag < 0x80
     * repeat next byte N times, else copy N-0x80 literals; 0x7f chunk cap
     * on encode @ 10f8:05da). Vs raw gate: CvPc frames arrive RLE here,
     * raw INDEX8 (6 + 3*pal + w*h, see gfx_decode_raw / gfx_probe_rsc_image
     * / rsc_decode_bitmap @ 1198:xxxx) only applies after expansion. Src
     * borrowed, dst caller-owned; bounds-checked both sides. No SDL, no
     * exit, no globals. */
    size_t ip = 0;
    size_t op = 0;

    if (out_len)
        *out_len = 0;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    while (ip < src_len) {
        uint8_t tag = src[ip++];
        if (tag < GFX_RLE_LITERAL_FLAG) {
            uint8_t v;

            if (ip >= src_len)
                return CIV_ERR_TRUNCATED;
            if (op + (size_t)tag > dst_cap)
                return CIV_ERR_TRUNCATED;
            v = src[ip++];
            for (unsigned k = 0; k < (unsigned)tag; k++)
                dst[op++] = v;
        } else {
            unsigned n = (unsigned)(tag - GFX_RLE_LITERAL_FLAG);

            if (ip + (size_t)n > src_len || op + (size_t)n > dst_cap)
                return CIV_ERR_TRUNCATED;
            for (unsigned k = 0; k < n; k++)
                dst[op++] = src[ip++];
        }
    }
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

int gfx_rle_expand_cvpc(const uint8_t *src, size_t src_len,
                        uint8_t *dst, size_t dst_cap,
                        uint32_t *w_out, uint32_t *h_out,
                        uint8_t *bpp_out, uint8_t *pal_out)
{
    /* AB2 CvPc RLE-expand-then-probe @ 10f8:040a + 1198:xxxx (OS-free,
     * no SDL, no exit, no globals). Confirmed CvPc head BB DF 06 45...
     * is binary RLE, not raw INDEX8 (raw probe reports BAD_DATA/RANGE,
     * so the 6 + 3*pal + w*h gate skips it); expand via gfx_decode_rle
     * (tag < 0x80 repeat / >= 0x80 literal, save_rll @ 10f8:05da/040a)
     * into caller-owned dst, then re-probe expanded bytes via
     * gfx_probe_rsc_image for w/h/bpp/pal (filled even on gate failure
     * so the caller can log the reason). Src borrowed (valid while the
     * container lives), dst caller-owned; bounds-checked both sides. */
    size_t out_len = 0;
    civ_error_t dr;

    if (!src || !dst || dst_cap == 0)
        return -1;
    dr = gfx_decode_rle(src, src_len, dst, dst_cap, &out_len);
    if (dr != CIV_OK)
        return -1;
    /* Re-probe fills w/h/bpp/pal even when the expanded bytes still
     * fail the raw gate; result is for caller logging, len still valid. */
    (void)gfx_probe_rsc_image(dst, out_len, w_out, h_out, bpp_out, pal_out);
    if (out_len > (size_t)0x7fffffff)
        return -1;
    return (int)out_len;
}

civ_error_t gfx_decode_raw(const uint8_t *data, size_t size,
                           rsc_indexed_t *out)
{
    /* AA2 raw INDEX8 decoder @ 1198:xxxx (OS-free 1:1 port mirroring
     * rsc_decode_bitmap in rsc_loader.c, read via Read, not edited, +
     * gfx_probe_rsc_image gate above: u16 w LE @ +0, u16 h LE @ +2, u8 bpp
     * @ +4 == 8, u8 pal @ +5 in 1..255, 6 + 3*pal + w*h <= size, every
     * pixel < pal). CIV mapping matches the probe (BAD_DATA on bpp != 8
     * or pal == 0; RSC uses BAD_MAGIC there, else same RANGE/TRUNCATED /
     * NOENT). Borrowed view: pixels points into data, palette copied tail-
     * zeroed. RLE payloads expand via gfx_decode_rle first; this takes raw
     * bytes only. No SDL, no exit, no globals. */
    uint32_t w;
    uint32_t h;
    uint8_t bpp;
    uint8_t pal_count;
    size_t pal_bytes;
    size_t px_count;
    size_t headroom;
    const uint8_t *pal;
    const uint8_t *px;
    size_t i;

    if (!data || !out)
        return CIV_ERR_NOENT;
    if (size < 6)
        return CIV_ERR_TRUNCATED;
    w = (uint32_t)data[0] | ((uint32_t)data[1] << 8);
    h = (uint32_t)data[2] | ((uint32_t)data[3] << 8);
    bpp = data[4];
    pal_count = data[5];
    if (w == 0 || h == 0)
        return CIV_ERR_RANGE;
    if (bpp != 8)
        return CIV_ERR_BAD_DATA;
    if (pal_count == 0)
        return CIV_ERR_BAD_DATA;
    px_count = (size_t)w * (size_t)h;
    pal_bytes = (size_t)pal_count * 3u;
    headroom = size - 6;
    if (pal_bytes > headroom)
        return CIV_ERR_TRUNCATED;
    if (px_count > headroom - pal_bytes)
        return CIV_ERR_TRUNCATED;
    pal = data + 6;
    px = pal + pal_bytes;
    for (i = 0; i < px_count; i++) {
        if (px[i] >= pal_count)
            return CIV_ERR_RANGE;
    }
    out->width = w;
    out->height = h;
    out->bpp = 8;
    out->pal_count = pal_count;
    for (i = 0; i < (size_t)pal_count; i++) {
        out->pal[i][0] = pal[3 * i];
        out->pal[i][1] = pal[3 * i + 1];
        out->pal[i][2] = pal[3 * i + 2];
    }
    if ((size_t)pal_count < 256)
        memset(&out->pal[pal_count], 0,
               (256 - (size_t)pal_count) * 3);
    out->pixels = px;
    return CIV_OK;
}

size_t gfx_godpal_count(void)
{
    return (size_t)GFX_GODPAL_COUNT;
}

civ_error_t gfx_godpal_get(int index, uint8_t rgb[3])
{
    (void)index;
    (void)rgb;
    if (!rgb)
        return CIV_ERR_NOENT;
    if (index < 0 || index >= GFX_GODPAL_COUNT)
        return CIV_ERR_RANGE;
    /* TODO: Orig godpal.c 0xec RGB entries feeding FUN_1090_0488 @
     * 1090:0488 Palette_Init (LOGPALETTE 0x300 -> CREATEPALETTE ->
     * DAT_1420_9966 @ 1420:9966). Byte payload not yet dumped from
     * godpal.c / DAT_1420_9966, so no static const table is emitted here
     * (no fake bytes); bounds above stay live. AD2 wiring (still stub
     * until bytes pasted): run gfx_godpal_search("unpacked/civ.exe",
     * &off) (local-only unpacked/civ.exe, read-only, never committed);
     * on CIV_OK the LOGPALETTE anchor 00 03 EC 00 sits at off (palVersion
     * 0x0300 LE = 00 03, palNumEntries 0x00EC LE = EC 00, then 0xecx4
     * PALETTEENTRY RGBA). Dump via dd/xxd 0x2c4 RGB bytes at off+4 (or
     * 0x3B4 LOGPALETTE and drop flag bytes), cross-check vs Ghidra
     * CivLin.gpr program "civ.exe" DAT_1420_9966 @ 1420:9966 order/count
     * + "godpal.c" string neighbour + LOGPALETTE 0x300 zero tail past
     * 0xec (Ghidra 12.1.2: open CivLin.gpr, double-click "civ.exe" NE
     * Win3.x, Go To DAT_1420_9966, select 0xec*3 = 0x2c4 RGB, Copy
     * Special -> C Array or Bytes export + xxd -i), then paste as static
     * const kGodpal[0xec][3] (read-only, file-static const) + fill *rgb
     * from table and return CIV_OK. SDL3 consumer:
     * SDL_SetPaletteColors via palette_apply_to_port @ 1200:00a8/1090:0376
     * / palette_animate_range @ 1090:0376 (see video.h). */
    return CIV_ERR_UNSUPPORTED;
}

const char *gfx_try_fail_str(int fail)
{
    /* AD2 reason strings over GFX_TRY_FAIL_* (OS-free, no SDL).
     * Shared by gfx_try_rll / gfx_try_rll_inv / gfx_try_lzss_edi
     * *fail_out values (NONE on success or NOENT arg failure). String
     * literals only, no globals, no exit, C17. */
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

const char *gfx_expand_method_str(int method)
{
    /* AD2 method strings over GFX_EXPAND_* + AE1 GIF + AG1 1K/2K + AH1
     * BITS_MSB/BIAS2/BIAS4 (OS-free, no SDL). RAW0/2/4/8 log aliases are
     * RAW_SHIFTED + raw_off (see gfx_rle_expand_best_ex:
     * "RAW_SHIFTED+2", short "RAW2"); GIF winners log as GIF/GIF+<off>
     * with raw_off_out = 0/8 magic offset. String literals only, no
     * globals, no exit, C17. */
    switch (method) {
    case GFX_EXPAND_NONE:
        return "NONE";
    case GFX_EXPAND_RLL:
        return "RLL";
    case GFX_EXPAND_RLL_INV:
        return "RLL_INV";
    case GFX_EXPAND_LZSS_EDI:
        return "LZSS_EDI";
    case GFX_EXPAND_RAW_SHIFTED:
        return "RAW_SHIFTED";
    case GFX_EXPAND_GIF:
        return "GIF";
    case GFX_EXPAND_LZSS_1K:
        return "LZSS_1K";
    case GFX_EXPAND_LZSS_2K:
        return "LZSS_2K";
    case GFX_EXPAND_BITS_MSB:
        return "BITS_MSB";
    case GFX_EXPAND_BITS_BIAS2:
        return "BITS_BIAS2";
    case GFX_EXPAND_BITS_BIAS4:
        return "BITS_BIAS4";
    default:
        return "UNKNOWN";
    }
}

civ_error_t gfx_try_rll(const uint8_t *src, size_t src_len,
                        uint8_t *dst, size_t dst_cap, size_t *out_len,
                        int *fail_out)
{
    /* AC2 current-tag try @ 10f8:040a (OS-free 1:1 with gfx_decode_rle
     * above: tag < 0x80 repeat next byte N times, tag >= 0x80 copy
     * N-0x80 literals; tag 0 no-op but consumes the value byte). CvPc
     * BB DF 06 45... under this tag: BB >= 0x80 claims 59 literals, so
     * a short slice reports TAG_OVERRUN and a long run past dst_cap
     * reports DST_OVERRUN (vs a silent wrong decode). Src borrowed, dst
     * caller-owned. No SDL, no exit, no globals. */
    size_t ip = 0;
    size_t op = 0;

    if (out_len)
        *out_len = 0;
    if (fail_out)
        *fail_out = GFX_TRY_FAIL_NONE;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    while (ip < src_len) {
        uint8_t tag = src[ip++];
        if (tag < GFX_RLE_LITERAL_FLAG) {
            uint8_t v;

            if (ip >= src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TRUNC_SRC;
                return CIV_ERR_TRUNCATED;
            }
            if (op + (size_t)tag > dst_cap) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            v = src[ip++];
            for (unsigned k = 0; k < (unsigned)tag; k++)
                dst[op++] = v;
        } else {
            unsigned n = (unsigned)(tag - GFX_RLE_LITERAL_FLAG);

            if (ip + (size_t)n > src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TAG_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            if (op + (size_t)n > dst_cap) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            for (unsigned k = 0; k < n; k++)
                dst[op++] = src[ip++];
        }
    }
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

civ_error_t gfx_try_rll_inv(const uint8_t *src, size_t src_len,
                            uint8_t *dst, size_t dst_cap, size_t *out_len,
                            int *fail_out)
{
    /* AC2 inverted-tag try @ 10f8:040a hypothesis (OS-free, no SDL).
     * Flipped vs gfx_try_rll: tag < 0x80 = copy tag literal bytes, tag
     * >= 0x80 = repeat next byte (tag-0x80) times (0x80 no-op, still
     * consumes the value byte, mirror of the tag-0 rule). BB DF 06 45...
     * under this tag: BB >= 0x80 is repeat-59 of DF (1 B tail, plausible
     * in a ~500 B rsc_record_data slice) instead of literal-59 (59 B
     * tail). Same borrowed-src / caller-dst bounds and fail_out mapping
     * as gfx_try_rll. No SDL, no exit, no globals. */
    size_t ip = 0;
    size_t op = 0;

    if (out_len)
        *out_len = 0;
    if (fail_out)
        *fail_out = GFX_TRY_FAIL_NONE;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    while (ip < src_len) {
        uint8_t tag = src[ip++];
        if (tag < GFX_RLE_LITERAL_FLAG) {
            unsigned n = (unsigned)tag;

            if (ip + (size_t)n > src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TAG_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            if (op + (size_t)n > dst_cap) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            for (unsigned k = 0; k < n; k++)
                dst[op++] = src[ip++];
        } else {
            unsigned n = (unsigned)(tag - GFX_RLE_LITERAL_FLAG);
            uint8_t v;

            if (ip >= src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TRUNC_SRC;
                return CIV_ERR_TRUNCATED;
            }
            if (op + (size_t)n > dst_cap) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            v = src[ip++];
            for (unsigned k = 0; k < n; k++)
                dst[op++] = v;
        }
    }
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

civ_error_t gfx_try_lzss_edi(const uint8_t *src, size_t src_len,
                             uint8_t *dst, size_t dst_cap, size_t *out_len,
                             int *fail_out)
{
    /* AC2 EDI LZSS2 try (OS-free 1:1 port of tools/unpack_edi.py
     * edi_unpack, read via Read, not edited: flags LSB-first,
     * 1 = literal 1 B, 0 = match 2 B with matchpos =
     * ((x1 & 0xF0) << 4) | x0 and matchlen = (x1 & 0x0F) + 3; 4 K ring
     * prefilled 0x20, curpos 4096-18 = 4078). Stack ring only (no
     * globals); overlap-safe forward match copy. Expects a raw LZSS code
     * stream without the 25-B EDI header / unpacked_len prefix (installer
     * framing, not a CvPc field): decode runs until src bytes are
     * consumed, so a trailing incomplete flag/code at src end reports
     * TRUNC (not silent success); the dispatcher treats TRUNC as a miss.
     * Bounds: never reads past src_len, never writes past dst_cap. No
     * SDL, no exit, no globals. */
    uint8_t ring[GFX_LZSS_RING_SIZE];
    size_t curpos = (size_t)GFX_LZSS_START_POS;
    size_t cpos = 0;
    unsigned bitbuf = 0;
    unsigned bits_left = 0;
    size_t op = 0;
    size_t i;

    if (out_len)
        *out_len = 0;
    if (fail_out)
        *fail_out = GFX_TRY_FAIL_NONE;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    for (i = 0; i < (size_t)GFX_LZSS_RING_SIZE; i++)
        ring[i] = (uint8_t)GFX_LZSS_RING_FILL;
    while (cpos < src_len || bits_left > 0) {
        int bit;

        if (bits_left == 0) {
            if (cpos >= src_len)
                break;
            bitbuf = (unsigned)src[cpos++];
            bits_left = 8;
        }
        bit = (int)(bitbuf & 1u);
        bitbuf >>= 1;
        bits_left--;
        if (bit == 1) {
            uint8_t b;

            if (cpos >= src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TRUNC_SRC;
                return CIV_ERR_TRUNCATED;
            }
            if (op >= dst_cap) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            b = src[cpos++];
            dst[op++] = b;
            ring[curpos] = b;
            curpos = (curpos + 1u) & ((size_t)GFX_LZSS_RING_SIZE - 1u);
        } else {
            unsigned x0;
            unsigned x1;
            size_t matchpos;
            unsigned matchlen;
            unsigned k;

            if (cpos + 2u > src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TAG_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            x0 = (unsigned)src[cpos];
            x1 = (unsigned)src[cpos + 1u];
            cpos += 2u;
            matchpos = (size_t)(((x1 & 0xF0u) << 4) | x0);
            matchpos &= (size_t)GFX_LZSS_RING_SIZE - 1u;
            matchlen = (x1 & 0x0Fu) + 3u;
            for (k = 0; k < matchlen; k++) {
                uint8_t b;

                if (op >= dst_cap) {
                    if (fail_out)
                        *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                    return CIV_ERR_TRUNCATED;
                }
                b = ring[matchpos];
                dst[op++] = b;
                ring[curpos] = b;
                curpos = (curpos + 1u) & ((size_t)GFX_LZSS_RING_SIZE - 1u);
                matchpos = (matchpos + 1u) & ((size_t)GFX_LZSS_RING_SIZE - 1u);
            }
        }
    }
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

civ_error_t gfx_try_lzss_1k(const uint8_t *src, size_t src_len,
                             uint8_t *dst, size_t dst_cap, size_t *out_len,
                             int *fail_out)
{
    /* AG1 1K LZSS try (OS-free 1:1 code shape over tools/unpack_edi.py
     * edi_unpack, read via Read, not edited, smaller ring only, no
     * Win16, no SDL). Borland/early-variant hypothesis for the CvPc BB
     * DF 86k backlog (RLL/INV/LZSS-EDI/GIF/RAW all miss): flags
     * LSB-first, 1 = literal 1 B, 0 = match 2 B with matchpos =
     * (((x1 & 0xF0) << 4) | x0) & 1023 and matchlen = (x1 & 0x0F) + 3;
     * 1K ring prefilled 0x20, curpos 1024-18 = 1006. Stack ring only (no
     * globals); overlap-safe forward match copy. Same framing note as
     * gfx_try_lzss_edi: raw LZSS code stream, trailing incomplete
     * flag/code at src end reports TRUNC (dispatcher treats it as a
     * miss). Bounds: never reads past src_len, never writes past
     * dst_cap. No SDL, no exit, no globals, C17. */
    uint8_t ring[GFX_LZSS_1K_RING_SIZE];
    size_t curpos = (size_t)GFX_LZSS_1K_START_POS;
    size_t cpos = 0;
    unsigned bitbuf = 0;
    unsigned bits_left = 0;
    size_t op = 0;
    size_t i;

    if (out_len)
        *out_len = 0;
    if (fail_out)
        *fail_out = GFX_TRY_FAIL_NONE;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    for (i = 0; i < (size_t)GFX_LZSS_1K_RING_SIZE; i++)
        ring[i] = (uint8_t)GFX_LZSS_RING_FILL;
    while (cpos < src_len || bits_left > 0) {
        int bit;

        if (bits_left == 0) {
            if (cpos >= src_len)
                break;
            bitbuf = (unsigned)src[cpos++];
            bits_left = 8;
        }
        bit = (int)(bitbuf & 1u);
        bitbuf >>= 1;
        bits_left--;
        if (bit == 1) {
            uint8_t b;

            if (cpos >= src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TRUNC_SRC;
                return CIV_ERR_TRUNCATED;
            }
            if (op >= dst_cap) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            b = src[cpos++];
            dst[op++] = b;
            ring[curpos] = b;
            curpos = (curpos + 1u) & ((size_t)GFX_LZSS_1K_RING_SIZE - 1u);
        } else {
            unsigned x0;
            unsigned x1;
            size_t matchpos;
            unsigned matchlen;
            unsigned k;

            if (cpos + 2u > src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TAG_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            x0 = (unsigned)src[cpos];
            x1 = (unsigned)src[cpos + 1u];
            cpos += 2u;
            matchpos = (size_t)(((x1 & 0xF0u) << 4) | x0);
            matchpos &= (size_t)GFX_LZSS_1K_RING_SIZE - 1u;
            matchlen = (x1 & 0x0Fu) + 3u;
            for (k = 0; k < matchlen; k++) {
                uint8_t b;

                if (op >= dst_cap) {
                    if (fail_out)
                        *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                    return CIV_ERR_TRUNCATED;
                }
                b = ring[matchpos];
                dst[op++] = b;
                ring[curpos] = b;
                curpos = (curpos + 1u) & ((size_t)GFX_LZSS_1K_RING_SIZE - 1u);
                matchpos = (matchpos + 1u) & ((size_t)GFX_LZSS_1K_RING_SIZE - 1u);
            }
        }
    }
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

civ_error_t gfx_try_lzss_2k(const uint8_t *src, size_t src_len,
                             uint8_t *dst, size_t dst_cap, size_t *out_len,
                             int *fail_out)
{
    /* AG1 2K LZSS try (OS-free, same shape/backlog as gfx_try_lzss_1k
     * above, no Win16, no SDL). Flags LSB-first, 1 = literal 1 B,
     * 0 = match 2 B with matchpos = (((x1 & 0xF0) << 4) | x0) & 2047
     * and matchlen = (x1 & 0x0F) + 3; 2K ring prefilled 0x20, curpos
     * 2048-18 = 2030. Stack ring only (no globals); overlap-safe
     * forward match copy. Same TRUNC-on-trailing-fragment + bounds
     * contract as the 1K sibling. No SDL, no exit, no globals, C17. */
    uint8_t ring[GFX_LZSS_2K_RING_SIZE];
    size_t curpos = (size_t)GFX_LZSS_2K_START_POS;
    size_t cpos = 0;
    unsigned bitbuf = 0;
    unsigned bits_left = 0;
    size_t op = 0;
    size_t i;

    if (out_len)
        *out_len = 0;
    if (fail_out)
        *fail_out = GFX_TRY_FAIL_NONE;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    for (i = 0; i < (size_t)GFX_LZSS_2K_RING_SIZE; i++)
        ring[i] = (uint8_t)GFX_LZSS_RING_FILL;
    while (cpos < src_len || bits_left > 0) {
        int bit;

        if (bits_left == 0) {
            if (cpos >= src_len)
                break;
            bitbuf = (unsigned)src[cpos++];
            bits_left = 8;
        }
        bit = (int)(bitbuf & 1u);
        bitbuf >>= 1;
        bits_left--;
        if (bit == 1) {
            uint8_t b;

            if (cpos >= src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TRUNC_SRC;
                return CIV_ERR_TRUNCATED;
            }
            if (op >= dst_cap) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            b = src[cpos++];
            dst[op++] = b;
            ring[curpos] = b;
            curpos = (curpos + 1u) & ((size_t)GFX_LZSS_2K_RING_SIZE - 1u);
        } else {
            unsigned x0;
            unsigned x1;
            size_t matchpos;
            unsigned matchlen;
            unsigned k;

            if (cpos + 2u > src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TAG_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            x0 = (unsigned)src[cpos];
            x1 = (unsigned)src[cpos + 1u];
            cpos += 2u;
            matchpos = (size_t)(((x1 & 0xF0u) << 4) | x0);
            matchpos &= (size_t)GFX_LZSS_2K_RING_SIZE - 1u;
            matchlen = (x1 & 0x0Fu) + 3u;
            for (k = 0; k < matchlen; k++) {
                uint8_t b;

                if (op >= dst_cap) {
                    if (fail_out)
                        *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                    return CIV_ERR_TRUNCATED;
                }
                b = ring[matchpos];
                dst[op++] = b;
                ring[curpos] = b;
                curpos = (curpos + 1u) & ((size_t)GFX_LZSS_2K_RING_SIZE - 1u);
                matchpos = (matchpos + 1u) & ((size_t)GFX_LZSS_2K_RING_SIZE - 1u);
            }
        }
    }
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

civ_error_t gfx_try_bits_msb(const uint8_t *src, size_t src_len,
                             uint8_t *dst, size_t dst_cap, size_t *out_len,
                             int *fail_out)
{
    /* AH1 Borland MSB-first LZSS try (OS-free 1:1 code shape over
     * tools/unpack_edi.py edi_unpack, read via Read, not edited, bit
     * order flipped only, no Win16, no SDL). CvPc BB DF 86k backlog:
     * RLL/INV/LZSS-EDI (LSB-first)/1K/2K/GIF/RAW all TAG_OVERRUN, so a
     * Borland GR_Pic* MSB-first order is trialled (gr_pic.c @
     * 1420:2300/2372/23ce, see title.h). Stream: flags MSB-first,
     * 1 = literal (1 B), 0 = match (2 B: matchpos =
     * ((x1 & 0xF0) << 4) | x0, matchlen = (x1 & 0x0F) + 3); 4K ring
     * prefilled 0x20, curpos 4096-18 = 4078. Stack ring only (no
     * globals); overlap-safe forward match copy. Same raw-stream framing
     * as gfx_try_lzss_edi: trailing incomplete flag/code at src end
     * reports TRUNC (dispatcher treats it as a miss). Bounds: never reads
     * past src_len, never writes past dst_cap. No SDL, no exit, no
     * globals, C17. */
    uint8_t ring[GFX_LZSS_RING_SIZE];
    size_t curpos = (size_t)GFX_LZSS_START_POS;
    size_t cpos = 0;
    unsigned bitbuf = 0;
    unsigned bits_left = 0;
    size_t op = 0;
    size_t i;

    if (out_len)
        *out_len = 0;
    if (fail_out)
        *fail_out = GFX_TRY_FAIL_NONE;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    for (i = 0; i < (size_t)GFX_LZSS_RING_SIZE; i++)
        ring[i] = (uint8_t)GFX_LZSS_RING_FILL;
    while (cpos < src_len || bits_left > 0) {
        int bit;

        if (bits_left == 0) {
            if (cpos >= src_len)
                break;
            bitbuf = (unsigned)src[cpos++];
            bits_left = 8;
        }
        bit = (int)((bitbuf >> 7u) & 1u);
        bitbuf = (bitbuf << 1u) & 0xFFu;
        bits_left--;
        if (bit == 1) {
            uint8_t b;

            if (cpos >= src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TRUNC_SRC;
                return CIV_ERR_TRUNCATED;
            }
            if (op >= dst_cap) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            b = src[cpos++];
            dst[op++] = b;
            ring[curpos] = b;
            curpos = (curpos + 1u) & ((size_t)GFX_LZSS_RING_SIZE - 1u);
        } else {
            unsigned x0;
            unsigned x1;
            size_t matchpos;
            unsigned matchlen;
            unsigned k;

            if (cpos + 2u > src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TAG_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            x0 = (unsigned)src[cpos];
            x1 = (unsigned)src[cpos + 1u];
            cpos += 2u;
            matchpos = (size_t)(((x1 & 0xF0u) << 4) | x0);
            matchpos &= (size_t)GFX_LZSS_RING_SIZE - 1u;
            matchlen = (x1 & 0x0Fu) + 3u;
            for (k = 0; k < matchlen; k++) {
                uint8_t b;

                if (op >= dst_cap) {
                    if (fail_out)
                        *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                    return CIV_ERR_TRUNCATED;
                }
                b = ring[matchpos];
                dst[op++] = b;
                ring[curpos] = b;
                curpos = (curpos + 1u) & ((size_t)GFX_LZSS_RING_SIZE - 1u);
                matchpos = (matchpos + 1u) & ((size_t)GFX_LZSS_RING_SIZE - 1u);
            }
        }
    }
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

civ_error_t gfx_try_bits_bias2(const uint8_t *src, size_t src_len,
                               uint8_t *dst, size_t dst_cap, size_t *out_len,
                               int *fail_out)
{
    /* AH1 Borland MSB-first bias+2 LZSS try (OS-free, same shape/ring as
     * gfx_try_bits_msb above, no Win16, no SDL). Same 4K 0x20 ring,
     * curpos 4078, flags MSB-first, 1 = literal, 0 = match (2 B:
     * matchpos = ((x1 & 0xF0) << 4) | x0, matchlen = (x1 & 0x0F) + 2);
     * Borland threshold-1 variant (vs +3 threshold-2). Stack ring only;
     * overlap-safe forward copy; same TRUNC-on-trailing-fragment +
     * bounds contract. No SDL, no exit, no globals, C17. */
    uint8_t ring[GFX_LZSS_RING_SIZE];
    size_t curpos = (size_t)GFX_LZSS_START_POS;
    size_t cpos = 0;
    unsigned bitbuf = 0;
    unsigned bits_left = 0;
    size_t op = 0;
    size_t i;

    if (out_len)
        *out_len = 0;
    if (fail_out)
        *fail_out = GFX_TRY_FAIL_NONE;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    for (i = 0; i < (size_t)GFX_LZSS_RING_SIZE; i++)
        ring[i] = (uint8_t)GFX_LZSS_RING_FILL;
    while (cpos < src_len || bits_left > 0) {
        int bit;

        if (bits_left == 0) {
            if (cpos >= src_len)
                break;
            bitbuf = (unsigned)src[cpos++];
            bits_left = 8;
        }
        bit = (int)((bitbuf >> 7u) & 1u);
        bitbuf = (bitbuf << 1u) & 0xFFu;
        bits_left--;
        if (bit == 1) {
            uint8_t b;

            if (cpos >= src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TRUNC_SRC;
                return CIV_ERR_TRUNCATED;
            }
            if (op >= dst_cap) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            b = src[cpos++];
            dst[op++] = b;
            ring[curpos] = b;
            curpos = (curpos + 1u) & ((size_t)GFX_LZSS_RING_SIZE - 1u);
        } else {
            unsigned x0;
            unsigned x1;
            size_t matchpos;
            unsigned matchlen;
            unsigned k;

            if (cpos + 2u > src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TAG_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            x0 = (unsigned)src[cpos];
            x1 = (unsigned)src[cpos + 1u];
            cpos += 2u;
            matchpos = (size_t)(((x1 & 0xF0u) << 4) | x0);
            matchpos &= (size_t)GFX_LZSS_RING_SIZE - 1u;
            matchlen = (x1 & 0x0Fu) + 2u;
            for (k = 0; k < matchlen; k++) {
                uint8_t b;

                if (op >= dst_cap) {
                    if (fail_out)
                        *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                    return CIV_ERR_TRUNCATED;
                }
                b = ring[matchpos];
                dst[op++] = b;
                ring[curpos] = b;
                curpos = (curpos + 1u) & ((size_t)GFX_LZSS_RING_SIZE - 1u);
                matchpos = (matchpos + 1u) & ((size_t)GFX_LZSS_RING_SIZE - 1u);
            }
        }
    }
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

civ_error_t gfx_try_bits_bias4(const uint8_t *src, size_t src_len,
                               uint8_t *dst, size_t dst_cap, size_t *out_len,
                               int *fail_out)
{
    /* AH1 Borland MSB-first bias+4 LZSS try (OS-free, same shape/ring as
     * gfx_try_bits_msb above, no Win16, no SDL). Same 4K 0x20 ring,
     * curpos 4078, flags MSB-first, 1 = literal, 0 = match (2 B:
     * matchpos = ((x1 & 0xF0) << 4) | x0, matchlen = (x1 & 0x0F) + 4);
     * Borland threshold-3 variant. Stack ring only; overlap-safe forward
     * copy; same TRUNC-on-trailing-fragment + bounds contract. No SDL, no
     * exit, no globals, C17. */
    uint8_t ring[GFX_LZSS_RING_SIZE];
    size_t curpos = (size_t)GFX_LZSS_START_POS;
    size_t cpos = 0;
    unsigned bitbuf = 0;
    unsigned bits_left = 0;
    size_t op = 0;
    size_t i;

    if (out_len)
        *out_len = 0;
    if (fail_out)
        *fail_out = GFX_TRY_FAIL_NONE;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    for (i = 0; i < (size_t)GFX_LZSS_RING_SIZE; i++)
        ring[i] = (uint8_t)GFX_LZSS_RING_FILL;
    while (cpos < src_len || bits_left > 0) {
        int bit;

        if (bits_left == 0) {
            if (cpos >= src_len)
                break;
            bitbuf = (unsigned)src[cpos++];
            bits_left = 8;
        }
        bit = (int)((bitbuf >> 7u) & 1u);
        bitbuf = (bitbuf << 1u) & 0xFFu;
        bits_left--;
        if (bit == 1) {
            uint8_t b;

            if (cpos >= src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TRUNC_SRC;
                return CIV_ERR_TRUNCATED;
            }
            if (op >= dst_cap) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            b = src[cpos++];
            dst[op++] = b;
            ring[curpos] = b;
            curpos = (curpos + 1u) & ((size_t)GFX_LZSS_RING_SIZE - 1u);
        } else {
            unsigned x0;
            unsigned x1;
            size_t matchpos;
            unsigned matchlen;
            unsigned k;

            if (cpos + 2u > src_len) {
                if (fail_out)
                    *fail_out = GFX_TRY_FAIL_TAG_OVERRUN;
                return CIV_ERR_TRUNCATED;
            }
            x0 = (unsigned)src[cpos];
            x1 = (unsigned)src[cpos + 1u];
            cpos += 2u;
            matchpos = (size_t)(((x1 & 0xF0u) << 4) | x0);
            matchpos &= (size_t)GFX_LZSS_RING_SIZE - 1u;
            matchlen = (x1 & 0x0Fu) + 4u;
            for (k = 0; k < matchlen; k++) {
                uint8_t b;

                if (op >= dst_cap) {
                    if (fail_out)
                        *fail_out = GFX_TRY_FAIL_DST_OVERRUN;
                    return CIV_ERR_TRUNCATED;
                }
                b = ring[matchpos];
                dst[op++] = b;
                ring[curpos] = b;
                curpos = (curpos + 1u) & ((size_t)GFX_LZSS_RING_SIZE - 1u);
                matchpos = (matchpos + 1u) & ((size_t)GFX_LZSS_RING_SIZE - 1u);
            }
        }
    }
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

civ_error_t gfx_try_raw_shifted(const uint8_t *data, size_t size,
                                size_t offset, rsc_indexed_t *out)
{
    /* AC2 raw-at-offset try @ 1198:xxxx (OS-free, delegates to
     * gfx_decode_raw above for parity with rsc_decode_bitmap; header
     * fields/bounds/pixel gate identical, error mapping identical).
     * Offset skips prefixes: 0 = slice start, 2/4 = length/palette
     * prefix hypothesis, 8 = 8-byte type tag (CvD1CvMM/CvPc) with w/h at
     * +8. Borrowed view, read-only assets. No SDL, no exit, no globals. */
    if (!data || !out)
        return CIV_ERR_NOENT;
    if (offset > size)
        return CIV_ERR_TRUNCATED;
    return gfx_decode_raw(data + offset, size - offset, out);
}

civ_error_t gfx_lzw_init(gfx_lzw_dict_t *dict, uint8_t min_code_size)
{
    /* AE1 LZW table init @ 1420:23ce sibling (OS-free 1:1, no SDL).
     * clear = 1<<min, eoi = clear+1, next = clear+2, width = min+1;
     * roots 0..clear-1 map to single bytes. No globals, no exit. */
    unsigned i;
    unsigned clear;

    if (!dict)
        return CIV_ERR_NOENT;
    if (min_code_size < GFX_LZW_MIN_CODE_MIN ||
        min_code_size > GFX_LZW_MIN_CODE_MAX)
        return CIV_ERR_RANGE;
    clear = 1u << min_code_size;
    dict->clear_code = (uint16_t)clear;
    dict->eoi_code = (uint16_t)(clear + 1u);
    dict->next_code = (uint16_t)(clear + 2u);
    dict->code_width = (uint8_t)(min_code_size + 1u);
    dict->min_code_size = min_code_size;
    for (i = 0; i < (unsigned)GFX_LZW_DICT_SIZE; i++)
        dict->prefix[i] = 0xFFFFu;
    for (i = 0; i < 256u; i++)
        dict->suffix[i] = (uint8_t)i;
    return CIV_OK;
}

civ_error_t gfx_lzw_decode(const uint8_t *src, size_t src_len,
                           uint8_t min_code_size,
                           uint8_t *dst, size_t dst_cap, size_t *out_len)
{
    /* AE1 LZW core @ 1420:23ce sibling (OS-free, no SDL, no globals).
     * LSB-first codes, clear reset, eoi stop, prefix/suffix walk with a
     * 4096-B pixel stack (KwKwK code==next handled), width grows past
     * (1<<width) up to 12. Bounds-checked both sides; tolerant missing
     * eoi (clean exhaustion with op > 0 is OK for CvPc slices). */
    gfx_lzw_dict_t dict;
    uint8_t stack[4096];
    size_t bitpos = 0;
    size_t total_bits;
    size_t op = 0;
    int prev = -1;
    civ_error_t ir;

    if (out_len)
        *out_len = 0;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    if (min_code_size < GFX_LZW_MIN_CODE_MIN ||
        min_code_size > GFX_LZW_MIN_CODE_MAX)
        return CIV_ERR_RANGE;
    if (src_len == 0 || dst_cap == 0)
        return CIV_ERR_TRUNCATED;
    ir = gfx_lzw_init(&dict, min_code_size);
    if (ir != CIV_OK)
        return ir;
    if (src_len > (size_t)SIZE_MAX / 8u)
        total_bits = (size_t)SIZE_MAX;
    else
        total_bits = src_len * 8u;
    for (;;) {
        unsigned width;
        unsigned code = 0;
        unsigned b;

        width = (unsigned)dict.code_width;
        if (bitpos >= total_bits) {
            /* Clean exhaustion: OK when pixels exist (tolerant missing
             * eoi), else truncated (empty stream). */
            if (op == 0)
                return CIV_ERR_TRUNCATED;
            break;
        }
        if (width > total_bits - bitpos)
            return CIV_ERR_TRUNCATED;
        for (b = 0; b < width; b++) {
            size_t at = bitpos + (size_t)b;
            size_t byte = at >> 3;
            unsigned bit = (unsigned)(at & 7u);
            unsigned v = (unsigned)((src[byte] >> bit) & 1u);
            code |= (v << b);
        }
        bitpos += (size_t)width;
        if (code == (unsigned)dict.clear_code) {
            ir = gfx_lzw_init(&dict, min_code_size);
            if (ir != CIV_OK)
                return ir;
            prev = -1;
            continue;
        }
        if (code == (unsigned)dict.eoi_code)
            break;
        if (prev == -1) {
            /* First code after clear/start must be a literal root. */
            if (code >= (unsigned)dict.clear_code)
                return CIV_ERR_BAD_DATA;
            if (op >= dst_cap)
                return CIV_ERR_TRUNCATED;
            dst[op++] = (uint8_t)code;
            prev = (int)code;
            continue;
        }
        if (code < (unsigned)dict.next_code) {
            /* Existing entry: walk to root, emit reversed. */
            unsigned cur = code;
            size_t sp = 0;
            unsigned iter = 0;
            uint8_t first = 0;
            size_t k;

            while (cur != 0xFFFFu) {
                if (sp >= sizeof stack)
                    return CIV_ERR_BAD_DATA;
                if (cur >= (unsigned)GFX_LZW_DICT_SIZE)
                    return CIV_ERR_BAD_DATA;
                if (++iter > (unsigned)GFX_LZW_DICT_SIZE)
                    return CIV_ERR_BAD_DATA;
                stack[sp++] = dict.suffix[cur];
                if (dict.prefix[cur] == 0xFFFFu) {
                    first = dict.suffix[cur];
                    break;
                }
                cur = (unsigned)dict.prefix[cur];
            }
            if (sp == 0)
                return CIV_ERR_BAD_DATA;
            if (op + sp > dst_cap)
                return CIV_ERR_TRUNCATED;
            for (k = 0; k < sp; k++)
                dst[op++] = stack[sp - 1u - k];
            if ((unsigned)dict.next_code < (unsigned)GFX_LZW_DICT_SIZE) {
                dict.prefix[dict.next_code] = (uint16_t)prev;
                dict.suffix[dict.next_code] = first;
                dict.next_code++;
                if (dict.code_width < (uint8_t)GFX_LZW_MAX_BITS &&
                    (unsigned)dict.next_code >= (1u << dict.code_width))
                    dict.code_width++;
            }
            prev = (int)code;
        } else if (code == (unsigned)dict.next_code && prev >= 0) {
            /* KwKwK: prev string + its first byte. */
            unsigned cur = (unsigned)prev;
            size_t sp = 0;
            unsigned iter = 0;
            uint8_t first = 0;
            size_t k;

            while (cur != 0xFFFFu) {
                if (sp >= sizeof stack)
                    return CIV_ERR_BAD_DATA;
                if (cur >= (unsigned)GFX_LZW_DICT_SIZE)
                    return CIV_ERR_BAD_DATA;
                if (++iter > (unsigned)GFX_LZW_DICT_SIZE)
                    return CIV_ERR_BAD_DATA;
                stack[sp++] = dict.suffix[cur];
                if (dict.prefix[cur] == 0xFFFFu) {
                    first = dict.suffix[cur];
                    break;
                }
                cur = (unsigned)dict.prefix[cur];
            }
            if (sp == 0)
                return CIV_ERR_BAD_DATA;
            if (op + sp + 1u > dst_cap)
                return CIV_ERR_TRUNCATED;
            for (k = 0; k < sp; k++)
                dst[op++] = stack[sp - 1u - k];
            dst[op++] = first;
            if ((unsigned)dict.next_code < (unsigned)GFX_LZW_DICT_SIZE) {
                dict.prefix[dict.next_code] = (uint16_t)prev;
                dict.suffix[dict.next_code] = first;
                dict.next_code++;
                if (dict.code_width < (uint8_t)GFX_LZW_MAX_BITS &&
                    (unsigned)dict.next_code >= (1u << dict.code_width))
                    dict.code_width++;
            }
            prev = (int)code;
        } else {
            return CIV_ERR_BAD_DATA;
        }
    }
    if (op == 0)
        return CIV_ERR_TRUNCATED;
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

civ_error_t gfx_gif_parse(const uint8_t *data, size_t size,
                          uint8_t *px_buf, size_t px_cap,
                          rsc_indexed_t *out)
{
    /* AE1 GIF frame parse @ 1420:2300/2372 sibling (OS-free, no SDL).
     * GIF87a/89a + LSD + GCE-skip + 0x2C descriptor + LZW min +
     * sub-blocks; full mode decodes via gfx_lzw_decode (malloc'd flat
     * sub-block copy, no globals), header mode (px_buf NULL) sizes only.
     * w/h from the image descriptor; palette GCT unless LCT present;
     * 256-entry tables -> UNSUPPORTED (u8 pal_count cap); interlace ->
     * UNSUPPORTED. Borrowed-from-px_buf view on success. */
    uint32_t lsd_w = 0;
    uint32_t lsd_h = 0;
    uint8_t lsd_packed = 0;
    int gct_flag = 0;
    unsigned gct_n = 0;
    size_t gct_entries = 0;
    size_t pos = 0;
    size_t img_pos = 0;
    int found_image = 0;
    uint32_t iw = 0;
    uint32_t ih = 0;
    int lct_flag = 0;
    int interlace = 0;
    unsigned lct_n = 0;
    size_t lct_entries = 0;
    size_t dpos = 0;
    uint8_t lzw_min = 0;
    size_t blk_start = 0;
    size_t total_comp = 0;
    size_t p = 0;
    uint8_t pal[256][3];
    size_t pal_entries = 0;
    size_t px_count = 0;
    size_t i;

    (void)lsd_w;
    (void)lsd_h;
    if (!data || !out)
        return CIV_ERR_NOENT;
    if (size < 13)
        return CIV_ERR_TRUNCATED;
    if (memcmp(data, "GIF87a", 6) != 0 && memcmp(data, "GIF89a", 6) != 0)
        return CIV_ERR_BAD_DATA;
    lsd_w = (uint32_t)data[6] | ((uint32_t)data[7] << 8);
    lsd_h = (uint32_t)data[8] | ((uint32_t)data[9] << 8);
    lsd_packed = data[10];
    gct_flag = (lsd_packed & 0x80) != 0;
    gct_n = (unsigned)(lsd_packed & 0x07u);
    if (gct_flag)
        gct_entries = (size_t)1u << (gct_n + 1u);
    pos = 13;
    if (gct_flag) {
        size_t need;

        if (gct_entries < 2 || gct_entries > 256)
            return CIV_ERR_BAD_DATA;
        need = gct_entries * 3u;
        if (need > size - pos)
            return CIV_ERR_TRUNCATED;
        for (i = 0; i < gct_entries; i++) {
            pal[i][0] = data[pos + 3u * i];
            pal[i][1] = data[pos + 3u * i + 1u];
            pal[i][2] = data[pos + 3u * i + 2u];
        }
        pal_entries = gct_entries;
        pos += need;
    }
    for (;;) {
        uint8_t sep;

        if (pos >= size)
            return CIV_ERR_TRUNCATED;
        sep = data[pos];
        if (sep == 0x21u) {
            if (pos + 2u > size)
                return CIV_ERR_TRUNCATED;
            pos += 2u;
            for (;;) {
                uint8_t n;

                if (pos >= size)
                    return CIV_ERR_TRUNCATED;
                n = data[pos++];
                if (n == 0)
                    break;
                if ((size_t)n > size - pos)
                    return CIV_ERR_TRUNCATED;
                pos += (size_t)n;
            }
            continue;
        }
        if (sep == 0x2Cu) {
            found_image = 1;
            img_pos = pos;
            break;
        }
        if (sep == 0x3Bu)
            return CIV_ERR_BAD_DATA;
        return CIV_ERR_BAD_DATA;
    }
    if (!found_image)
        return CIV_ERR_BAD_DATA;
    if (img_pos + 10u > size)
        return CIV_ERR_TRUNCATED;
    iw = (uint32_t)data[img_pos + 5u] | ((uint32_t)data[img_pos + 6u] << 8);
    ih = (uint32_t)data[img_pos + 7u] | ((uint32_t)data[img_pos + 8u] << 8);
    if (iw == 0 || ih == 0)
        return CIV_ERR_RANGE;
    if (iw > 65535u || ih > 65535u)
        return CIV_ERR_RANGE;
    lct_flag = (data[img_pos + 9u] & 0x80) != 0;
    interlace = (data[img_pos + 9u] & 0x40) != 0;
    if (interlace)
        return CIV_ERR_UNSUPPORTED;
    lct_n = (unsigned)(data[img_pos + 9u] & 0x07u);
    if (lct_flag) {
        size_t need;
        size_t lct_pos;

        lct_entries = (size_t)1u << (lct_n + 1u);
        if (lct_entries < 2 || lct_entries > 256)
            return CIV_ERR_BAD_DATA;
        lct_pos = img_pos + 10u;
        need = lct_entries * 3u;
        if (need > size - lct_pos)
            return CIV_ERR_TRUNCATED;
        for (i = 0; i < lct_entries; i++) {
            pal[i][0] = data[lct_pos + 3u * i];
            pal[i][1] = data[lct_pos + 3u * i + 1u];
            pal[i][2] = data[lct_pos + 3u * i + 2u];
        }
        pal_entries = lct_entries;
        dpos = lct_pos + need;
    } else {
        if (!gct_flag || pal_entries == 0)
            return CIV_ERR_BAD_DATA;
        dpos = img_pos + 10u;
    }
    if (dpos >= size)
        return CIV_ERR_TRUNCATED;
    lzw_min = data[dpos++];
    if (lzw_min < GFX_LZW_MIN_CODE_MIN || lzw_min > GFX_LZW_MIN_CODE_MAX)
        return CIV_ERR_RANGE;
    blk_start = dpos;
    p = dpos;
    for (;;) {
        uint8_t n;

        if (p >= size)
            return CIV_ERR_TRUNCATED;
        n = data[p++];
        if (n == 0)
            break;
        if ((size_t)n > size - p)
            return CIV_ERR_TRUNCATED;
        if (total_comp > (size_t)SIZE_MAX - (size_t)n)
            return CIV_ERR_RANGE;
        total_comp += (size_t)n;
        p += (size_t)n;
    }
    if (total_comp == 0)
        return CIV_ERR_TRUNCATED;
    if (pal_entries == 256)
        return CIV_ERR_UNSUPPORTED;
    if (pal_entries < 2 || pal_entries > 255)
        return CIV_ERR_BAD_DATA;
    if (iw == 0 || ih == 0)
        return CIV_ERR_RANGE;
    if ((size_t)iw > (size_t)SIZE_MAX / (size_t)ih)
        return CIV_ERR_RANGE;
    px_count = (size_t)iw * (size_t)ih;
    out->width = iw;
    out->height = ih;
    out->bpp = 8;
    out->pal_count = (uint8_t)pal_entries;
    for (i = 0; i < pal_entries; i++) {
        out->pal[i][0] = pal[i][0];
        out->pal[i][1] = pal[i][1];
        out->pal[i][2] = pal[i][2];
    }
    if (pal_entries < 256)
        memset(&out->pal[pal_entries], 0, (256 - pal_entries) * 3u);
    if (!px_buf) {
        out->pixels = NULL;
        return CIV_OK;
    }
    if (px_count > px_cap)
        return CIV_ERR_TRUNCATED;
    {
        uint8_t *flat = NULL;
        size_t q = blk_start;
        size_t wp = 0;
        size_t dec_len = 0;
        civ_error_t dr;

        flat = (uint8_t *)malloc(total_comp);
        if (!flat)
            return CIV_ERR_NOMEM;
        while (q < p) {
            uint8_t n = data[q++];

            if (n == 0)
                break;
            if ((size_t)n > size - q || wp + (size_t)n > total_comp) {
                free(flat);
                return CIV_ERR_TRUNCATED;
            }
            memcpy(flat + wp, data + q, (size_t)n);
            wp += (size_t)n;
            q += (size_t)n;
        }
        if (wp != total_comp) {
            free(flat);
            return CIV_ERR_TRUNCATED;
        }
        dr = gfx_lzw_decode(flat, total_comp, lzw_min, px_buf, px_cap,
                            &dec_len);
        free(flat);
        if (dr != CIV_OK)
            return dr;
        if (dec_len < px_count)
            return CIV_ERR_TRUNCATED;
        if (dec_len != px_count)
            return CIV_ERR_BAD_DATA;
        out->pixels = (const uint8_t *)px_buf;
        return CIV_OK;
    }
}

int gfx_try_gif(const uint8_t *src, size_t src_len,
                uint8_t *dst, size_t dst_cap,
                uint32_t *w_out, uint32_t *h_out,
                uint8_t *bpp_out, uint8_t *pal_out)
{
    /* AE1 GIF dispatcher @ 1420:2372 sibling (OS-free, no SDL).
     * GIF magic 47 49 46 vs BB DF at +0 (miss), live try at +8 past the
     * 8B CvD tag; per offset header-size, full decode into dst+head,
     * header fill, re-probe. Returns raw blob len or -1, no fake. */
    static const size_t kOffs[2] = { 0, 8 };
    size_t oi;

    if (!src || !dst || dst_cap == 0)
        return -1;
    for (oi = 0; oi < 2; oi++) {
        size_t off = kOffs[oi];
        const uint8_t *base;
        size_t tail;
        rsc_indexed_t hdr;
        rsc_indexed_t full;
        uint32_t w;
        uint32_t h;
        uint8_t pal;
        size_t pal_bytes;
        size_t px_count;
        size_t head_len;
        size_t raw_len;
        civ_error_t fr;

        if (off > src_len)
            continue;
        tail = src_len - off;
        base = src + off;
        if (tail < (size_t)GFX_GIF_MAGIC_LEN)
            continue;
        if (memcmp(base, "GIF87a", 6) != 0 &&
            memcmp(base, "GIF89a", 6) != 0)
            continue;
        if (gfx_gif_parse(base, tail, NULL, 0, &hdr) != CIV_OK)
            continue;
        w = hdr.width;
        h = hdr.height;
        pal = hdr.pal_count;
        if (w == 0 || h == 0 || pal == 0)
            continue;
        pal_bytes = (size_t)pal * 3u;
        if ((size_t)w > (size_t)SIZE_MAX / (size_t)h)
            continue;
        px_count = (size_t)w * (size_t)h;
        if (pal_bytes > (size_t)SIZE_MAX - 6u)
            continue;
        head_len = 6u + pal_bytes;
        if (px_count > (size_t)SIZE_MAX - head_len)
            continue;
        raw_len = head_len + px_count;
        if (raw_len > dst_cap)
            continue;
        if (raw_len > (size_t)INT_MAX)
            continue;
        fr = gfx_gif_parse(base, tail, dst + head_len,
                           dst_cap - head_len, &full);
        if (fr != CIV_OK)
            continue;
        if (full.width != w || full.height != h ||
            full.pal_count != pal)
            continue;
        dst[0] = (uint8_t)(w & 0xFFu);
        dst[1] = (uint8_t)(((unsigned)w >> 8) & 0xFFu);
        dst[2] = (uint8_t)(h & 0xFFu);
        dst[3] = (uint8_t)(((unsigned)h >> 8) & 0xFFu);
        dst[4] = 8;
        dst[5] = pal;
        memcpy(dst + 6, full.pal, pal_bytes);
        (void)gfx_probe_rsc_image(dst, raw_len, w_out, h_out, bpp_out,
                                  pal_out);
        return (int)raw_len;
    }
    return -1;
}

int gfx_try_pk(const uint8_t *src, size_t src_len,
               uint8_t *dst, size_t dst_cap,
               uint32_t *w_out, uint32_t *h_out,
               uint8_t *bpp_out, uint8_t *pal_out)
{
    /* AE1 PK stub (no Orig; hypothesised ZIP for the BB DF backlog).
     * PK\x03\x04 = 50 4B 03 04, PK\x05\x06 empty, PK\x07\x08 spanned:
     * slice head BB DF != 50 4B at +0 and no PK magic expected at +8
     * (8B CvD tag family), so likely not PK; no inflate attempted, dst
     * never touched, outs never filled, always -1 (no fake). */
    (void)src;
    (void)src_len;
    (void)dst;
    (void)dst_cap;
    (void)w_out;
    (void)h_out;
    (void)bpp_out;
    (void)pal_out;
    return -1;
}

int gfx_try_huff_stub(const uint8_t *src, size_t src_len,
                      uint8_t *dst, size_t dst_cap,
                      uint32_t *w_out, uint32_t *h_out,
                      uint8_t *bpp_out, uint8_t *pal_out)
{
    /* AG1 Huffman/LZW-beyond-GIF stub (no Orig; CvPc BB DF 86k slice
     * misses RLL/INV/LZSS-EDI/LZSS-1K/LZSS-2K/GIF/RAW). No fake decode:
     * a Huffman decoder needs its code-length table from the payload
     * head first (per-symbol bit lengths + symbol order, or an explicit
     * tree dump); BB DF 06 45... shows no proven table at +0/+2/+4/+8
     * yet, so decoding now would emit fake bytes. TODO (carve first):
     * log head hex via gfx_probe_head_hex, hunt a length-vector/tree
     * candidate, then wire the table-driven decoder here. Same
     * int-dispatcher shape as gfx_try_gif/gfx_try_pk for a future
     * longest-wins slot: dst never touched, outs never filled, always
     * -1 (NULL src/dst and dst_cap == 0 also -1). No SDL, no exit, no
     * globals, C17. */
    (void)src;
    (void)src_len;
    (void)dst;
    (void)dst_cap;
    (void)w_out;
    (void)h_out;
    (void)bpp_out;
    (void)pal_out;
    return -1;
}

civ_error_t gfx_godpal_search(const char *path, long *offset_out)
{
    /* AD2 LOGPALETTE anchor scan (read-only, no writes, no SDL, no fake
     * table). Magic reasoning (WinGDI LOGPALETTE behind Palette_Init @
     * 1090:0488 -> CREATEPALETTE -> DAT_1420_9966 @ 1420:9966): WORD
     * palVersion 0x0300 LE = bytes 00 03, WORD palNumEntries 0x00EC LE =
     * bytes EC 00, so the LE anchor is 00 03 EC 00 (4 B), followed by
     * 0xec PALETTEENTRY structs (R,G,B,flags, 4 B each = 0x3B4 with the
     * header; godpal.c RGB source is 0xec*3 = 0x2c4, flags dropped,
     * LOGPALETTE 0x300 tail past 0xec stays zero). RGB plausibility alone
     * is useless (every byte < 256, always true), so only the anchor is
     * searched here; the 0x3B4/0x2c4 dump + Ghidra DAT_1420_9966
     * order/count + "godpal.c" string-neighbour check happen after the
     * hit, before any static const kGodpal table (never here). Scan:
     * fopen(path,"rb") on the caller path (e.g. local-only
     * unpacked/civ.exe, never committed, never written) + 4 KiB sliding
     * window with 3-B overlap (magic straddling the chunk edge is still
     * caught), fread chunks, bounds-checked. No SDL, no exit, no
     * globals, C17. */
    static const uint8_t kMagic[4] = { 0x00, 0x03, 0xEC, 0x00 };
    uint8_t buf[4096 + 3];
    FILE *fp = NULL;
    size_t carry = 0;
    long base = 0;
    size_t n;
    size_t total;
    size_t i;

    if (!path || path[0] == '\0')
        return CIV_ERR_NOENT;
    fp = fopen(path, "rb");
    if (!fp)
        return CIV_ERR_IO;
    for (;;) {
        n = fread(buf + carry, 1, 4096, fp);
        if (n == 0 && ferror(fp)) {
            fclose(fp);
            return CIV_ERR_IO;
        }
        total = carry + n;
        if (total >= (size_t)GFX_GODPAL_MAGIC_LEN) {
            for (i = 0; i + (size_t)GFX_GODPAL_MAGIC_LEN <= total; i++) {
                if (buf[i] == kMagic[0] && buf[i + 1] == kMagic[1] &&
                    buf[i + 2] == kMagic[2] && buf[i + 3] == kMagic[3]) {
                    long hit = base + (long)i;
                    if (offset_out)
                        *offset_out = hit;
                    fclose(fp);
                    return CIV_OK;
                }
            }
        }
        if (n < 4096) {
            if (ferror(fp)) {
                fclose(fp);
                return CIV_ERR_IO;
            }
            if (offset_out)
                *offset_out = -1;
            fclose(fp);
            return CIV_ERR_NOENT;
        }
        /* Full chunk, no hit: keep the last 3 B for the straddling
         * magic, slide the window forward. */
        buf[0] = buf[total - 3];
        buf[1] = buf[total - 2];
        buf[2] = buf[total - 1];
        carry = 3;
        base += (long)(total - 3);
    }
}

int gfx_rle_expand_best(const uint8_t *src, size_t src_len,
                        uint8_t *dst, size_t dst_cap,
                        int *method_out, size_t *raw_off_out,
                        uint32_t *w_out, uint32_t *h_out,
                        uint8_t *bpp_out, uint8_t *pal_out)
{
    /* AC2 best-of-variants dispatcher + AE1 GIF slot + AG1 1K/2K slots
     * + AH1 BITS_MSB/BIAS2/BIAS4 slots (no Orig; re-probe via
     * gfx_probe_rsc_image @ 1198:xxxx gate, mirror of
     * gfx_rle_expand_cvpc logging). Thin wrapper over
     * gfx_rle_expand_best_ex below (AD2 fail/log outs skipped here):
     * order RLL -> RLL_INV -> LZSS_EDI -> LZSS_1K -> LZSS_2K ->
     * BITS_MSB -> BITS_BIAS2 -> BITS_BIAS4 -> GIF {0,8} -> RAW
     * {0,2,4,8}, longest wins, ties keep the earlier method (RLL > INV >
     * LZSS_EDI > LZSS_1K > LZSS_2K > BITS_MSB > BITS_BIAS2 > BITS_BIAS4
     * > GIF > RAW). PK/HUFF stubs never trialled (BB DF != 50 4B;
     * Huffman needs its code-length table first). No SDL, no exit, no
     * globals. */
    return gfx_rle_expand_best_ex(src, src_len, dst, dst_cap,
                                  method_out, raw_off_out,
                                  w_out, h_out, bpp_out, pal_out,
                                  NULL, NULL, NULL, NULL, 0);
}

int gfx_rle_expand_best_ex(const uint8_t *src, size_t src_len,
                           uint8_t *dst, size_t dst_cap,
                           int *method_out, size_t *raw_off_out,
                           uint32_t *w_out, uint32_t *h_out,
                           uint8_t *bpp_out, uint8_t *pal_out,
                           int *fail_rll_out, int *fail_inv_out,
                           int *fail_lzss_out,
                           char *log_buf, size_t log_cap)
{
    /* AD2 extended dispatcher + AE1 GIF slot + AG1 1K/2K slots + AH1
     * BITS_MSB/BIAS2/BIAS4 slots (no Orig; OS-free, no SDL). Same trial
     * order, tie-break, refill, and re-probe as gfx_rle_expand_best
     * above: RLL -> INV -> LZSS_EDI -> LZSS_1K -> LZSS_2K -> BITS_MSB ->
     * BITS_BIAS2 -> BITS_BIAS4 -> GIF {0,8} -> RAW {0,2,4,8}; trials
     * share the caller dst (each decodes from scratch) and the winner is
     * refilled at the end (re-run RLL/LZSS/BITS/GIF, memcpy RAW slice),
     * so no side buffer or globals are needed. GIF candidate =
     * gfx_try_gif rebuilt raw blob (6+3*pal+w*h, log GIF:ok/miss; PK +
     * HUFF stubs never trialled: BB DF != 50 4B; Huffman needs its
     * code-length table from the payload head first). RAW candidate =
     * probe pass at src+off with len = src_len-off fitting dst_cap
     * (else skipped and logged as "skip"). Best-method log names are
     * RLL/INV/LZSS/LZSS1K/LZSS2K/BITS_MSB/BITS_BIAS2/BITS_BIAS4/GIF/
     * RAW0/2/4/8 (GIF log-only ok/miss; 1K/2K/BITS log-only TRY_FAIL; RAW
     * suffices + raw_off; strings via gfx_expand_method_str + "+<off>"
     * for RAW, GIF best uses method string "GIF" with raw_off_out = 0/8
     * magic off). Per-method fails: RLL/INV/LZSS_EDI as GFX_TRY_FAIL_*
     * outs + log, 1K/2K/BITS as GFX_TRY_FAIL_* log-only (signature
     * unchanged, no new fail_out); GIF as ok/miss; RAW0/2/4/8 as
     * probe-gate civ_strerror (or "skip"). Src borrowed, dst
     * caller-owned (e.g. rsc_dump 4 MiB scratch). Bounds-checked every
     * trial, on refill, and on log_buf (snprintf-truncated). No SDL, no
     * exit, no globals, C17, read-only assets. */
    const size_t kRawOffs[4] = { 0, 2, 4, 8 };
    size_t best_len = 0;
    int best_method = GFX_EXPAND_NONE;
    size_t best_off = 0;
    size_t trial_len = 0;
    int fail_rll = GFX_TRY_FAIL_NONE;
    int fail_inv = GFX_TRY_FAIL_NONE;
    int fail_lzss = GFX_TRY_FAIL_NONE;
    int fail_1k = GFX_TRY_FAIL_NONE;
    int fail_2k = GFX_TRY_FAIL_NONE;
    int fail_bmsb = GFX_TRY_FAIL_NONE;
    int fail_bb2 = GFX_TRY_FAIL_NONE;
    int fail_bb4 = GFX_TRY_FAIL_NONE;
    int gif_len_tmp = -1;
    size_t gif_len = 0;
    size_t gif_off = 0;
    int gif_ok = 0;
    civ_error_t raw_rc[4] = { CIV_ERR_NOENT, CIV_ERR_NOENT,
                              CIV_ERR_NOENT, CIV_ERR_NOENT };
    int raw_skip[4] = { 1, 1, 1, 1 };
    size_t oi;

    if (method_out)
        *method_out = GFX_EXPAND_NONE;
    if (raw_off_out)
        *raw_off_out = 0;
    if (fail_rll_out)
        *fail_rll_out = GFX_TRY_FAIL_NONE;
    if (fail_inv_out)
        *fail_inv_out = GFX_TRY_FAIL_NONE;
    if (fail_lzss_out)
        *fail_lzss_out = GFX_TRY_FAIL_NONE;
    if (!src || !dst || dst_cap == 0) {
        if (log_buf && log_cap > 0) {
            (void)snprintf(log_buf, log_cap,
                           "best=NONE len=-1 RLL:%s INV:%s LZSS:%s "
                           "LZSS1K:%s LZSS2K:%s BITS_MSB:%s BITS_BIAS2:%s "
                           "BITS_BIAS4:%s GIF:miss "
                           "RAW0:skip RAW2:skip RAW4:skip RAW8:skip",
                           gfx_try_fail_str(fail_rll),
                           gfx_try_fail_str(fail_inv),
                           gfx_try_fail_str(fail_lzss),
                           gfx_try_fail_str(fail_1k),
                           gfx_try_fail_str(fail_2k),
                           gfx_try_fail_str(fail_bmsb),
                           gfx_try_fail_str(fail_bb2),
                           gfx_try_fail_str(fail_bb4));
        }
        return -1;
    }
    trial_len = 0;
    if (gfx_try_rll(src, src_len, dst, dst_cap,
                    &trial_len, &fail_rll) == CIV_OK) {
        best_len = trial_len;
        best_method = GFX_EXPAND_RLL;
        best_off = 0;
    }
    trial_len = 0;
    if (gfx_try_rll_inv(src, src_len, dst, dst_cap,
                        &trial_len, &fail_inv) == CIV_OK) {
        if (best_method == GFX_EXPAND_NONE || trial_len > best_len) {
            best_len = trial_len;
            best_method = GFX_EXPAND_RLL_INV;
            best_off = 0;
        }
    }
    trial_len = 0;
    if (gfx_try_lzss_edi(src, src_len, dst, dst_cap,
                         &trial_len, &fail_lzss) == CIV_OK) {
        if (best_method == GFX_EXPAND_NONE || trial_len > best_len) {
            best_len = trial_len;
            best_method = GFX_EXPAND_LZSS_EDI;
            best_off = 0;
        }
    }
    trial_len = 0;
    if (gfx_try_lzss_1k(src, src_len, dst, dst_cap,
                        &trial_len, &fail_1k) == CIV_OK) {
        if (best_method == GFX_EXPAND_NONE || trial_len > best_len) {
            best_len = trial_len;
            best_method = GFX_EXPAND_LZSS_1K;
            best_off = 0;
        }
    }
    trial_len = 0;
    if (gfx_try_lzss_2k(src, src_len, dst, dst_cap,
                        &trial_len, &fail_2k) == CIV_OK) {
        if (best_method == GFX_EXPAND_NONE || trial_len > best_len) {
            best_len = trial_len;
            best_method = GFX_EXPAND_LZSS_2K;
            best_off = 0;
        }
    }
    trial_len = 0;
    if (gfx_try_bits_msb(src, src_len, dst, dst_cap,
                         &trial_len, &fail_bmsb) == CIV_OK) {
        if (best_method == GFX_EXPAND_NONE || trial_len > best_len) {
            best_len = trial_len;
            best_method = GFX_EXPAND_BITS_MSB;
            best_off = 0;
        }
    }
    trial_len = 0;
    if (gfx_try_bits_bias2(src, src_len, dst, dst_cap,
                           &trial_len, &fail_bb2) == CIV_OK) {
        if (best_method == GFX_EXPAND_NONE || trial_len > best_len) {
            best_len = trial_len;
            best_method = GFX_EXPAND_BITS_BIAS2;
            best_off = 0;
        }
    }
    trial_len = 0;
    if (gfx_try_bits_bias4(src, src_len, dst, dst_cap,
                           &trial_len, &fail_bb4) == CIV_OK) {
        if (best_method == GFX_EXPAND_NONE || trial_len > best_len) {
            best_len = trial_len;
            best_method = GFX_EXPAND_BITS_BIAS4;
            best_off = 0;
        }
    }
    gif_len_tmp = gfx_try_gif(src, src_len, dst, dst_cap,
                              NULL, NULL, NULL, NULL);
    if (gif_len_tmp >= 0) {
        static const size_t kGifOffs[2] = { 0, 8 };
        size_t go;

        gif_ok = 1;
        gif_len = (size_t)gif_len_tmp;
        gif_off = 0;
        for (go = 0; go < 2; go++) {
            size_t off = kGifOffs[go];
            rsc_indexed_t hdr;
            size_t pal_bytes;
            size_t px_count;
            size_t raw_len;

            if (off > src_len)
                continue;
            if (src_len - off < (size_t)GFX_GIF_MAGIC_LEN)
                continue;
            if (memcmp(src + off, "GIF87a", 6) != 0 &&
                memcmp(src + off, "GIF89a", 6) != 0)
                continue;
            if (gfx_gif_parse(src + off, src_len - off, NULL, 0,
                              &hdr) != CIV_OK)
                continue;
            pal_bytes = (size_t)hdr.pal_count * 3u;
            px_count = (size_t)hdr.width * (size_t)hdr.height;
            raw_len = 6u + pal_bytes + px_count;
            if (raw_len == gif_len) {
                gif_off = off;
                break;
            }
        }
        if (best_method == GFX_EXPAND_NONE || gif_len > best_len) {
            best_len = gif_len;
            best_method = GFX_EXPAND_GIF;
            best_off = gif_off;
        }
    }
    for (oi = 0; oi < 4; oi++) {
        size_t off = kRawOffs[oi];
        size_t cand;
        civ_error_t pr;

        if (off > src_len)
            continue;
        cand = src_len - off;
        if (cand > dst_cap)
            continue;
        pr = gfx_probe_rsc_image(src + off, cand, NULL, NULL, NULL, NULL);
        raw_skip[oi] = 0;
        raw_rc[oi] = pr;
        if (pr != CIV_OK)
            continue;
        if (best_method == GFX_EXPAND_NONE || cand > best_len) {
            best_len = cand;
            best_method = GFX_EXPAND_RAW_SHIFTED;
            best_off = off;
        }
    }
    if (fail_rll_out)
        *fail_rll_out = fail_rll;
    if (fail_inv_out)
        *fail_inv_out = fail_inv;
    if (fail_lzss_out)
        *fail_lzss_out = fail_lzss;
    if (best_method == GFX_EXPAND_NONE) {
        if (log_buf && log_cap > 0) {
            const char *r0 = raw_skip[0] ? "skip" : civ_strerror(raw_rc[0]);
            const char *r1 = raw_skip[1] ? "skip" : civ_strerror(raw_rc[1]);
            const char *r2 = raw_skip[2] ? "skip" : civ_strerror(raw_rc[2]);
            const char *r3 = raw_skip[3] ? "skip" : civ_strerror(raw_rc[3]);
            (void)snprintf(log_buf, log_cap,
                           "best=NONE len=-1 RLL:%s INV:%s LZSS:%s "
                           "LZSS1K:%s LZSS2K:%s BITS_MSB:%s BITS_BIAS2:%s "
                           "BITS_BIAS4:%s GIF:%s "
                           "RAW0:%s RAW2:%s RAW4:%s RAW8:%s",
                           gfx_try_fail_str(fail_rll),
                           gfx_try_fail_str(fail_inv),
                           gfx_try_fail_str(fail_lzss),
                           gfx_try_fail_str(fail_1k),
                           gfx_try_fail_str(fail_2k),
                           gfx_try_fail_str(fail_bmsb),
                           gfx_try_fail_str(fail_bb2),
                           gfx_try_fail_str(fail_bb4),
                           gif_ok ? "ok" : "miss",
                           r0, r1, r2, r3);
        }
        return -1;
    }
    if (best_len > (size_t)INT_MAX) {
        if (log_buf && log_cap > 0) {
            const char *r0 = raw_skip[0] ? "skip" : civ_strerror(raw_rc[0]);
            const char *r1 = raw_skip[1] ? "skip" : civ_strerror(raw_rc[1]);
            const char *r2 = raw_skip[2] ? "skip" : civ_strerror(raw_rc[2]);
            const char *r3 = raw_skip[3] ? "skip" : civ_strerror(raw_rc[3]);
            (void)snprintf(log_buf, log_cap,
                           "best=NONE len=-1 RLL:%s INV:%s LZSS:%s "
                           "LZSS1K:%s LZSS2K:%s BITS_MSB:%s BITS_BIAS2:%s "
                           "BITS_BIAS4:%s GIF:%s "
                           "RAW0:%s RAW2:%s RAW4:%s RAW8:%s",
                           gfx_try_fail_str(fail_rll),
                           gfx_try_fail_str(fail_inv),
                           gfx_try_fail_str(fail_lzss),
                           gfx_try_fail_str(fail_1k),
                           gfx_try_fail_str(fail_2k),
                           gfx_try_fail_str(fail_bmsb),
                           gfx_try_fail_str(fail_bb2),
                           gfx_try_fail_str(fail_bb4),
                           gif_ok ? "ok" : "miss",
                           r0, r1, r2, r3);
        }
        return -1;
    }
    if (best_method == GFX_EXPAND_RLL) {
        trial_len = 0;
        if (gfx_try_rll(src, src_len, dst, dst_cap, &trial_len, NULL) != CIV_OK ||
            trial_len != best_len)
            return -1;
    } else if (best_method == GFX_EXPAND_RLL_INV) {
        trial_len = 0;
        if (gfx_try_rll_inv(src, src_len, dst, dst_cap, &trial_len, NULL) != CIV_OK ||
            trial_len != best_len)
            return -1;
    } else if (best_method == GFX_EXPAND_LZSS_EDI) {
        trial_len = 0;
        if (gfx_try_lzss_edi(src, src_len, dst, dst_cap, &trial_len, NULL) != CIV_OK ||
            trial_len != best_len)
            return -1;
    } else if (best_method == GFX_EXPAND_LZSS_1K) {
        trial_len = 0;
        if (gfx_try_lzss_1k(src, src_len, dst, dst_cap, &trial_len, NULL) != CIV_OK ||
            trial_len != best_len)
            return -1;
    } else if (best_method == GFX_EXPAND_LZSS_2K) {
        trial_len = 0;
        if (gfx_try_lzss_2k(src, src_len, dst, dst_cap, &trial_len, NULL) != CIV_OK ||
            trial_len != best_len)
            return -1;
    } else if (best_method == GFX_EXPAND_BITS_MSB) {
        trial_len = 0;
        if (gfx_try_bits_msb(src, src_len, dst, dst_cap, &trial_len, NULL) != CIV_OK ||
            trial_len != best_len)
            return -1;
    } else if (best_method == GFX_EXPAND_BITS_BIAS2) {
        trial_len = 0;
        if (gfx_try_bits_bias2(src, src_len, dst, dst_cap, &trial_len, NULL) != CIV_OK ||
            trial_len != best_len)
            return -1;
    } else if (best_method == GFX_EXPAND_BITS_BIAS4) {
        trial_len = 0;
        if (gfx_try_bits_bias4(src, src_len, dst, dst_cap, &trial_len, NULL) != CIV_OK ||
            trial_len != best_len)
            return -1;
    } else if (best_method == GFX_EXPAND_GIF) {
        int glen = gfx_try_gif(src, src_len, dst, dst_cap,
                               NULL, NULL, NULL, NULL);

        if (glen < 0 || (size_t)glen != best_len)
            return -1;
    } else {
        if (best_off > src_len || best_len > src_len - best_off ||
            best_len > dst_cap)
            return -1;
        if (best_len > 0)
            memcpy(dst, src + best_off, best_len);
    }
    /* Re-probe fills w/h/bpp/pal even when the winning bytes still fail
     * the raw gate; result is for caller logging, len still valid. */
    (void)gfx_probe_rsc_image(dst, best_len, w_out, h_out, bpp_out, pal_out);
    if (method_out)
        *method_out = best_method;
    if (raw_off_out)
        *raw_off_out = best_off;
    if (log_buf && log_cap > 0) {
        char best_name[32];
        const char *r0 = raw_skip[0] ? "skip" : civ_strerror(raw_rc[0]);
        const char *r1 = raw_skip[1] ? "skip" : civ_strerror(raw_rc[1]);
        const char *r2 = raw_skip[2] ? "skip" : civ_strerror(raw_rc[2]);
        const char *r3 = raw_skip[3] ? "skip" : civ_strerror(raw_rc[3]);
        if (best_method == GFX_EXPAND_RAW_SHIFTED)
            (void)snprintf(best_name, sizeof best_name, "RAW%zu", best_off);
        else
            (void)snprintf(best_name, sizeof best_name, "%s",
                           gfx_expand_method_str(best_method));
        (void)snprintf(log_buf, log_cap,
                       "best=%s len=%lu RLL:%s INV:%s LZSS:%s LZSS1K:%s "
                       "LZSS2K:%s BITS_MSB:%s BITS_BIAS2:%s BITS_BIAS4:%s "
                       "GIF:%s RAW0:%s RAW2:%s RAW4:%s RAW8:%s",
                       best_name, (unsigned long)best_len,
                       gfx_try_fail_str(fail_rll),
                       gfx_try_fail_str(fail_inv),
                       gfx_try_fail_str(fail_lzss),
                       gfx_try_fail_str(fail_1k),
                       gfx_try_fail_str(fail_2k),
                       gfx_try_fail_str(fail_bmsb),
                       gfx_try_fail_str(fail_bb2),
                       gfx_try_fail_str(fail_bb4),
                       gif_ok ? "ok" : "miss",
                       r0, r1, r2, r3);
    }
    return (int)best_len;
}

civ_error_t gfx_bmp_parse(const uint8_t *data, size_t size, gfx_bmp_t *out)
{
    /* AF1 BMP parse (OS-free 1:1, no Win16, no SDL). MPLOGO.BMP has NO
     * filename string in civ.exe (NE resource or RSC frame, see title.h);
     * the Linux port passes an explicit path and this parses the borrowed
     * file bytes. LE16/32 reads are manual bytes. MPLOGO.BMP reference
     * (unpacked/MPLOGO.BMP + civ1win/MPLOGO.BMP, read-only byte dump):
     * 42 4D "BM", fsize 4718, reserved 0,0, off 62; DIB 40, w 380, h +97
     * (bottom-up), planes 1, bpp 1, comp 0 BI_RGB, imgsz 4656, 2834x2834,
     * clrused/clrimp 0 (derive 2); palette 00 00 00 00 / FF FF FF 00;
     * stride 48, 48*97=4656. No SDL, no exit, no globals. */
    uint32_t fsize;
    uint32_t pix_off;
    uint32_t dib_size;
    int32_t w_signed;
    int32_t h_signed;
    uint16_t planes;
    uint16_t bpp;
    uint32_t comp;
    uint32_t clrused;
    size_t pal_base;
    size_t pal_entries;
    size_t stride;
    size_t total;
    uint32_t w_abs;
    uint32_t h_abs;
    size_t i;

    if (!data || !out)
        return CIV_ERR_NOENT;
    if (size < 14)
        return CIV_ERR_TRUNCATED;
    if (data[0] != 0x42u || data[1] != 0x4Du)
        return CIV_ERR_BAD_DATA;
    fsize = (uint32_t)data[2] | ((uint32_t)data[3] << 8) |
        ((uint32_t)data[4] << 16) | ((uint32_t)data[5] << 24);
    pix_off = (uint32_t)data[10] | ((uint32_t)data[11] << 8) |
        ((uint32_t)data[12] << 16) | ((uint32_t)data[13] << 24);
    if (fsize > size)
        return CIV_ERR_TRUNCATED;
    if (size < 18)
        return CIV_ERR_TRUNCATED;
    dib_size = (uint32_t)data[14] | ((uint32_t)data[15] << 8) |
        ((uint32_t)data[16] << 16) | ((uint32_t)data[17] << 24);
    if (dib_size != 40u)
        return CIV_ERR_UNSUPPORTED;
    if (size < 14u + 40u)
        return CIV_ERR_TRUNCATED;
    w_signed = (int32_t)((uint32_t)data[18] | ((uint32_t)data[19] << 8) |
        ((uint32_t)data[20] << 16) | ((uint32_t)data[21] << 24));
    h_signed = (int32_t)((uint32_t)data[22] | ((uint32_t)data[23] << 8) |
        ((uint32_t)data[24] << 16) | ((uint32_t)data[25] << 24));
    planes = (uint16_t)((uint16_t)data[26] | ((uint16_t)data[27] << 8));
    bpp = (uint16_t)((uint16_t)data[28] | ((uint16_t)data[29] << 8));
    comp = (uint32_t)data[30] | ((uint32_t)data[31] << 8) |
        ((uint32_t)data[32] << 16) | ((uint32_t)data[33] << 24);
    clrused = (uint32_t)data[46] | ((uint32_t)data[47] << 8) |
        ((uint32_t)data[48] << 16) | ((uint32_t)data[49] << 24);
    if (w_signed <= 0 || h_signed == 0)
        return CIV_ERR_RANGE;
    if (planes != 1u)
        return CIV_ERR_BAD_DATA;
    if (bpp != 1u)
        return CIV_ERR_UNSUPPORTED;
    if (comp != 0u)
        return CIV_ERR_UNSUPPORTED;
    if (clrused != 0u && clrused != 2u)
        return CIV_ERR_BAD_DATA;
    w_abs = (uint32_t)w_signed;
    h_abs = (uint32_t)(h_signed < 0 ? -(int64_t)h_signed : (int64_t)h_signed);
    if (w_abs == 0 || h_abs == 0)
        return CIV_ERR_RANGE;
    if ((size_t)w_abs > (size_t)SIZE_MAX / 1u)
        return CIV_ERR_RANGE;
    /* Row stride (((w*bpp+31)/32)*4), overflow-safe for 1-bpp. */
    if ((size_t)w_abs > (size_t)SIZE_MAX - 31u)
        return CIV_ERR_RANGE;
    stride = (((size_t)w_abs * 1u + 31u) / 32u) * 4u;
    if (stride == 0)
        return CIV_ERR_RANGE;
    if ((size_t)h_abs > (size_t)SIZE_MAX / stride)
        return CIV_ERR_RANGE;
    total = stride * (size_t)h_abs;
    pal_base = 14u + (size_t)dib_size;
    pal_entries = 2u;
    if (pal_base + pal_entries * 4u > size)
        return CIV_ERR_TRUNCATED;
    if ((size_t)pix_off < pal_base + pal_entries * 4u)
        return CIV_ERR_BAD_DATA;
    if ((size_t)pix_off > size)
        return CIV_ERR_TRUNCATED;
    if (total > size - (size_t)pix_off)
        return CIV_ERR_TRUNCATED;
    out->width = w_abs;
    out->height = h_abs;
    out->bpp = bpp;
    out->pal_count = 2;
    for (i = 0; i < pal_entries; i++) {
        uint8_t b = data[pal_base + 4u * i];
        uint8_t g = data[pal_base + 4u * i + 1u];
        uint8_t r = data[pal_base + 4u * i + 2u];

        out->pal[i][0] = r;
        out->pal[i][1] = g;
        out->pal[i][2] = b;
    }
    memset(&out->pal[pal_entries], 0, (256u - pal_entries) * 3u);
    out->pixels = data + (size_t)pix_off;
    out->row_stride = stride;
    out->pixel_offset = (size_t)pix_off;
    out->top_down = (h_signed < 0) ? 1 : 0;
    return CIV_OK;
}

civ_error_t gfx_bmp_to_indexed(const gfx_bmp_t *bmp, uint8_t *px_buf,
                                size_t px_cap, rsc_indexed_t *out)
{
    /* AF1 1-bpp -> indexed (OS-free, no SDL). MSB-first bit unpack with
     * 4 B row padding skipped via row_stride; bottom-up rows flipped
     * unless top_down (MPLOGO.BMP h=+97 bottom-up). Palette copies the
     * BMP entries (confirmed black/white via byte dump) else defaults to
     * black/white; tail zeroed like rsc_decode_bitmap @ 1198:xxxx (NOT
     * edited, mirror gate only). No SDL, no exit, no globals. */
    size_t px_count;
    uint32_t y;
    uint32_t x;

    if (!bmp || !px_buf || !out)
        return CIV_ERR_NOENT;
    if (bmp->bpp != 1u)
        return CIV_ERR_UNSUPPORTED;
    if (bmp->width == 0 || bmp->height == 0 || !bmp->pixels)
        return CIV_ERR_RANGE;
    if ((size_t)bmp->width > (size_t)SIZE_MAX / (size_t)bmp->height)
        return CIV_ERR_RANGE;
    px_count = (size_t)bmp->width * (size_t)bmp->height;
    if (px_cap < px_count)
        return CIV_ERR_TRUNCATED;
    if ((size_t)bmp->height > (size_t)SIZE_MAX / bmp->row_stride)
        return CIV_ERR_TRUNCATED;
    if (bmp->row_stride * (size_t)bmp->height == 0)
        return CIV_ERR_TRUNCATED;
    for (y = 0; y < bmp->height; y++) {
        uint32_t src_y =
            bmp->top_down ? y : (bmp->height - 1u - y);
        const uint8_t *row = bmp->pixels + (size_t)src_y * bmp->row_stride;

        for (x = 0; x < bmp->width; x++) {
            uint8_t byte = row[x >> 3];
            unsigned bit = 7u - (x & 7u);
            uint8_t v = (uint8_t)((byte >> bit) & 1u);

            px_buf[(size_t)y * (size_t)bmp->width + (size_t)x] = v;
        }
    }
    out->width = bmp->width;
    out->height = bmp->height;
    out->bpp = 8;
    out->pal_count = 2;
    if (bmp->pal_count >= 2u) {
        for (size_t i = 0; i < 2u; i++) {
            out->pal[i][0] = bmp->pal[i][0];
            out->pal[i][1] = bmp->pal[i][1];
            out->pal[i][2] = bmp->pal[i][2];
        }
    } else {
        out->pal[0][0] = 0;
        out->pal[0][1] = 0;
        out->pal[0][2] = 0;
        out->pal[1][0] = 255;
        out->pal[1][1] = 255;
        out->pal[1][2] = 255;
    }
    memset(&out->pal[2], 0, (256u - 2u) * 3u);
    out->pixels = (const uint8_t *)px_buf;
    return CIV_OK;
}

civ_error_t gfx_bmp_load_indexed(const char *path, rsc_indexed_t *img_out,
                                  uint8_t **px_buf_out)
{
    /* AF1 file loader (read-only, no SDL). MPLOGO.BMP has NO filename
     * string in civ.exe (NE resource or RSC frame); the Linux port passes
     * the path explicitly (unpacked/MPLOGO.BMP or civ1win/MPLOGO.BMP).
     * fopen "rb" only, never writes. File buffer is freed before return;
     * only the unpacked indices survive (caller frees *px_buf_out). */
    FILE *fp = NULL;
    long fsize = 0;
    size_t size = 0;
    uint8_t *raw = NULL;
    size_t nread = 0;
    gfx_bmp_t bmp;
    uint8_t *px = NULL;
    size_t px_count = 0;
    civ_error_t pr;
    civ_error_t cr;

    if (px_buf_out)
        *px_buf_out = NULL;
    if (!path || path[0] == '\0' || !img_out || !px_buf_out)
        return CIV_ERR_NOENT;
    fp = fopen(path, "rb");
    if (!fp)
        return CIV_ERR_IO;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return CIV_ERR_IO;
    }
    fsize = ftell(fp);
    if (fsize < 0) {
        fclose(fp);
        return CIV_ERR_IO;
    }
    size = (size_t)fsize;
    if (size == 0) {
        fclose(fp);
        return CIV_ERR_TRUNCATED;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return CIV_ERR_IO;
    }
    raw = (uint8_t *)malloc(size);
    if (!raw) {
        fclose(fp);
        return CIV_ERR_NOMEM;
    }
    nread = fread(raw, 1, size, fp);
    fclose(fp);
    if (nread != size) {
        free(raw);
        return CIV_ERR_IO;
    }
    pr = gfx_bmp_parse(raw, size, &bmp);
    if (pr != CIV_OK) {
        free(raw);
        return pr;
    }
    if ((size_t)bmp.width > (size_t)SIZE_MAX / (size_t)bmp.height) {
        free(raw);
        return CIV_ERR_RANGE;
    }
    px_count = (size_t)bmp.width * (size_t)bmp.height;
    if (px_count == 0) {
        free(raw);
        return CIV_ERR_RANGE;
    }
    px = (uint8_t *)malloc(px_count);
    if (!px) {
        free(raw);
        return CIV_ERR_NOMEM;
    }
    cr = gfx_bmp_to_indexed(&bmp, px, px_count, img_out);
    free(raw);
    if (cr != CIV_OK) {
        free(px);
        return cr;
    }
    *px_buf_out = px;
    return CIV_OK;
}

SDL_Surface *font_glyph_to_surface(const uint8_t *bits, int w, int h)
{
    /* AJ1 font surface @ 1008:0e99 (OS-free bit expand + SDL bridge port,
     * malloc + bit expand, no Win16). FNT 1-bpp bits via font_render_glyph
     * @1008:0e99 dfBitsOffset (see boot.h font_fnt_bits / font_render_glyph,
     * read via Read, not edited): w/h + bits MSB-first stride=(w+7)/8
     * (tight rows, bit 7 = leftmost). Expands each bit to an 8-bit index
     * (0 = black, 1 = white) into a caller-malloc'd w*h buffer, wraps it as
     * rsc_indexed_t 8-bit + 2-entry palette black/white (00 00 00 /
     * FF FF FF, tail zeroed), then builds the SDL surface 8-bit via the
     * gfx_indexed_to_surface path (palette via SDL_SetPaletteColors there).
     * Bits are borrowed (never freed here); the surface is caller-owned
     * (SDL_DestroySurface). Parity with font_fnt_bits MSB-first unpack +
     * gfx_bmp_to_indexed unpack; gfx_decode_rle/raw/probe/lzw/gif/bits
     * parity kept (no break). No exit, no globals, C17; SDL only here. */
    uint8_t *px = NULL;
    rsc_indexed_t img;
    SDL_Surface *surf = NULL;
    size_t stride;
    size_t px_count;
    int x;
    int y;

    if (!bits || w <= 0 || h <= 0)
        return NULL;
    if ((size_t)w > (size_t)SIZE_MAX / (size_t)h)
        return NULL;
    px_count = (size_t)w * (size_t)h;
    if (px_count == 0)
        return NULL;
    stride = ((size_t)w + 7u) / 8u;
    if (stride == 0)
        return NULL;
    px = (uint8_t *)malloc(px_count);
    if (!px)
        return NULL;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            size_t src_idx = (size_t)y * stride + (size_t)x / 8u;
            unsigned src_shift = 7u - ((unsigned)x & 7u);
            uint8_t v = (uint8_t)(((unsigned)bits[src_idx] >> src_shift) & 1u);

            px[(size_t)y * (size_t)w + (size_t)x] = v;
        }
    }
    img.width = (uint32_t)w;
    img.height = (uint32_t)h;
    img.bpp = 8;
    img.pal_count = 2;
    img.pal[0][0] = 0;
    img.pal[0][1] = 0;
    img.pal[0][2] = 0;
    img.pal[1][0] = 255;
    img.pal[1][1] = 255;
    img.pal[1][2] = 255;
    memset(&img.pal[2], 0, (256u - 2u) * 3u);
    img.pixels = (const uint8_t *)px;
    surf = gfx_indexed_to_surface(&img);
    free(px);
    return surf;
}

int gfx_try_custom_stub(const uint8_t *src, size_t src_len,
                        uint8_t *dst, size_t dst_cap,
                        uint32_t *w_out, uint32_t *h_out,
                        uint8_t *bpp_out, uint8_t *pal_out)
{
    /* AJ1 custom-codec placeholder for the CvPc BB DF 86k backlog (no Orig;
     * custom Borland GR_Pic* beyond LZSS, see gfx_indexed.h CvPc custom
     * codec doc). Slice len 86857, head16 BB DF 06 45..., best NONE under
     * gfx_rle_expand_best_ex: all RLL/LZSS/GIF/RAW/BITS miss (RLL/INV/
     * LZSS-EDI/1K/2K/BITS_MSB/BIAS2/BIAS4 TAG_OVERRUN all, GIF miss,
     * RAW probe miss). TODO needs Ghidra decompile FUN_1198_xxxx body via
     * analyzeHeadless before any real decode; no fake decode here (dst
     * never touched, outs never filled). Keep gfx_rle_expand_best_ex order
     * (untrialled like PK/HUFF). Always -1, even on NULL src/dst or
     * dst_cap == 0. No SDL, no exit, no globals, C17. */
    (void)src;
    (void)src_len;
    (void)dst;
    (void)dst_cap;
    (void)w_out;
    (void)h_out;
    (void)bpp_out;
    (void)pal_out;
    return -1;
}
