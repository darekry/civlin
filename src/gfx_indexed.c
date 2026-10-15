/* CivLin indexed+palette -> SDL3 bridge. See gfx_indexed.h.
 * Z2: OS-free probe helpers (gfx_probe_rsc_image + gfx_probe_head_hex,
 * no SDL) mirror rsc_decode_bitmap @ 1198:xxxx so the images_strict=0
 * backlog (dir 454/14/31/143/1) can be triaged without touching SDL. */
#include "gfx_indexed.h"

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
     * (no fake bytes); Ghidra dump steps: open civ.exe (CivLin.gpr),
     * go to DAT_1420_9966 @ 1420:9966, select 0xec*3 = 0x2c4 RGB bytes,
     * export as C array, cross-check vs godpal.c (count 0xec, order,
     * LOGPALETTE 0x300 zero tail past 0xec), then paste as static const
     * kGodpal[0xec][3] + fill *rgb from the table.
     * SDL3 consumer: SDL_SetPaletteColors via palette_apply_to_port @
     * 1200:00a8/1090:0376 / palette_animate_range @ 1090:0376 (see
     * video.h). Bounds above stay live. */
    return CIV_ERR_UNSUPPORTED;
}
