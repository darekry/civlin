/* Video: SDL3 backend. Orig GDI paths in video.h; Win16 handles have no
 * Linux equivalent, so the port owns the SDL window/renderer created by
 * the caller (main.c) and validates them here (no globals, no exit). */
#include "video.h"

#include "gfx_indexed.h"

/* Concrete port backing store (Orig ~0x20-word struct @ 10d0:0000
 * family: DC + compatible DC + bitmap via 10d0:0cfa + clip rect +
 * palette refs; exact word map TBD, so the Linux shape stays minimal
 * but covers each Orig group). Backing texture comes from
 * gfx_port_alloc_bitmap (INDEX8 TARGET); renderer stays caller-owned;
 * VALIDATERECT is a no-op on SDL3. Fields:
 *   tex/w/h : bitmap bits (Orig HBITMAP via 10d0:0cfa).
 *   ren     : DC placeholder, borrowed display renderer (Orig HDC from
 *             GETDC/CREATECOMPATIBLEDC; borrow only, never owned/freed
 *             here; NULL until GfxPort_Create @ 10d0:0000 /
 *             CreateCompatible wires it).
 *   clip/has_clip : clip-rect placeholder (Orig SelectClipRgn words;
 *             applied as dst-rect intersect in blit, else ignored).
 *   pal     : palette-ref placeholder (Orig CREATEPALETTE handle;
 *             borrowed, owned by the INDEX8 texture/palette path, never
 *             freed here; see palette_apply_to_port). */
struct gfx_port {
    SDL_Texture *tex;
    int w;
    int h;
    SDL_Renderer *ren;
    SDL_Rect clip;
    int has_clip;
    SDL_Palette *pal;
};

civ_error_t video_init(SDL_Window *win, SDL_Renderer *ren)
{
    /* Orig Video init tail (GETDC/CREATEPALETTE family, see video.h).
     * Linux: caller owns win/ren; nothing to allocate yet. NULL -> NOENT
     * (orig faulted on a bad handle); otherwise CIV_OK so boot can proceed
     * to the title animation. */
    if (!win || !ren)
        return CIV_ERR_NOENT;
    return CIV_OK;
}

civ_error_t video_blit_indexed(SDL_Renderer *ren, SDL_Texture *tex, int x, int y)
{
    SDL_FRect dst;

    /* Thin SDL_RenderTexture wrapper (orig BITBLT SRCCOPY @ 10d0:0854).
     * NULL-safe; texture size query failure maps to BAD_DATA. */
    if (!ren || !tex)
        return CIV_ERR_NOENT;
    {
        float w = 0.0f;
        float h = 0.0f;
        if (!SDL_GetTextureSize(tex, &w, &h))
            return CIV_ERR_BAD_DATA;
        dst.x = (float)x;
        dst.y = (float)y;
        dst.w = w;
        dst.h = h;
        if (!SDL_RenderTexture(ren, tex, NULL, &dst))
            return CIV_ERR_IO;
    }
    return CIV_OK;
}

civ_error_t video_present(SDL_Renderer *ren)
{
    /* Orig present tail (BITBLT @ 10d0:0854 + VALIDATERECT @ 10d0:0854:
     * VALIDATERECT is a no-op on SDL3, no HWND/dirty-region to
     * validate). Error mapping (consistent across video.c): NULL ->
     * NOENT; SDL_RenderPresent fail -> IO; NOMEM only from host
     * allocation paths (SDL_CreatePalette / malloc probe), never from
     * present/blit. Z2 dump synthetic ok (doc only, no impl change):
     * rsc_dump present_frame (64x64 palette gradient via
     * gfx_indexed_to_texture + SDL_RenderTexture + this present) passes
     * incl. dummy video; 4x2 surface read-back via gfx_indexed_to_surface
     * passes. AA2: same synthetic surface/texture ok feeds the probe gate
     * (gfx_probe_rsc_image @ 1198:xxxx raw gate + gfx_decode_rle CvPc path,
     * see gfx_indexed.h); real frames stay gated there. AD2 (doc only,
     * no impl change): frames reach this present only after the
     * gfx_rle_expand_best dispatcher (RLL/INV/LZSS/RAW0/2/4/8 +
     * per-method fail log via gfx_rle_expand_best_ex +
     * gfx_try_fail_str/gfx_expand_method_str, see gfx_indexed.h). */
    if (!ren)
        return CIV_ERR_NOENT;
    if (!SDL_RenderPresent(ren))
        return CIV_ERR_IO;
    return CIV_OK;
}

void video_shutdown(void)
{
    /* Caller destroys win/ren (see boot_shutdown order in boot.c);
     * nothing owned here. */
}

civ_error_t palette_animate_range(int first, int count)
{
    /* Orig FUN_1090_0376 @ 1090:0376 -> Palette_AnimateRange. Orig
     * copies RGB triples + flag byte then ANIMATEPALETTE; opcode 1
     * (full apply from MainPort_ApplyPalette @ 1200:00a8) and timed
     * 1/0xe fades from Anim_AdvanceFrames @ 10b0:0461 map to
     * SDL_SetPaletteColors. No active 8-bit surface is owned here (no
     * globals), so this stages the range on a temp 256-entry palette;
     * the visible commit happens in palette_apply_to_port on the port
     * INDEX8 backing store. Colors feed from gfx_godpal_get
     * (0x00..0xeb) + black tail past 0xec (LOGPALETTE 0x300 zero tail);
     * propagates UNSUPPORTED until the godpal dump lands (no fake). */
    SDL_Color colors[256];
    SDL_Palette *tmp = NULL;
    int i;

    if (first < 0 || count <= 0 || first + count > 256)
        return CIV_ERR_RANGE;
    for (i = 0; i < count; i++) {
        int idx = first + i;
        if (idx < (int)GFX_GODPAL_COUNT) {
            uint8_t rgb[3];
            civ_error_t rc = gfx_godpal_get(idx, rgb);
            if (rc != CIV_OK)
                return rc;
            colors[i].r = rgb[0];
            colors[i].g = rgb[1];
            colors[i].b = rgb[2];
            colors[i].a = 255;
        } else {
            colors[i].r = 0;
            colors[i].g = 0;
            colors[i].b = 0;
            colors[i].a = 255;
        }
    }
    tmp = SDL_CreatePalette(256);
    if (!tmp)
        return CIV_ERR_NOMEM;
    if (!SDL_SetPaletteColors(tmp, colors, first, count)) {
        SDL_DestroyPalette(tmp);
        return CIV_ERR_IO;
    }
    SDL_DestroyPalette(tmp);
    return CIV_OK;
}

civ_error_t video_check_driver_caps(SDL_Renderer *ren)
{
    int w = 0;
    int h = 0;

    /* Orig FUN_1008_059a @ 1008:059a -> Video_CheckDriverCaps.
     * GETDEVICECAPS checklist @ 1420:0af4 (palette/raster caps) maps to:
     * render output must be non-zero (SDL_GetRenderOutputSize). Full
     * checklist bytes TBD (see boot_video_caps_count), so any live
     * renderer passes. */
    if (!ren)
        return CIV_ERR_NOENT;
    if (!SDL_GetRenderOutputSize(ren, &w, &h))
        return CIV_ERR_IO;
    if (w <= 0 || h <= 0)
        return CIV_ERR_BAD_DATA;
    return CIV_OK;
}

civ_error_t palette_init(void)
{
    /* Orig FUN_1090_0488 @ 1090:0488 -> Palette_Init. LOGPALETTE 0x300 +
     * 0xec RGB entries from godpal.c -> CREATEPALETTE -> DAT_1420_9966.
     * SDL3 8-bit palette is applied per-surface/texture at upload time
     * (see gfx_indexed_to_surface); no global palette to install here,
     * so init is a no-op CIV_OK (entry bytes still TBD, see
     * gfx_godpal_get). */
    return CIV_OK;
}

int gfx_brush_is_solid(int color, int mono)
{
    /* Orig Gfx_MakeBrush @ 1098:00b5 branch (see video.h): solid when
     * color < 10 or color > 0xf5 or mono (DAT_1420_17a6 == 1); else 8x8
     * pattern. OS-free 1:1, no Win16. */
    if (color < 10 || color > 0xf5 || mono != 0)
        return 1;
    return 0;
}

int gfx_color_table_kind(int color)
{
    /* Orig Gfx_LookupColor @ 1098:01b0 select (see video.h): < 10 low
     * table, 10 < color < 0xf6 palette, >= 0xf6 high table; 10 / 0xf6
     * fall through in orig -> -1 here. OS-free 1:1, no Win16. */
    if (color < 10)
        return 0;
    if (color > 10 && color < 0xf6)
        return 1;
    if (color >= 0xf6)
        return 2;
    return -1;
}

civ_error_t gfx_port_create(SDL_Renderer *ren, int w, int h, gfx_port_t **out)
{
    if (out)
        *out = NULL;
    (void)ren;
    (void)w;
    (void)h;
    if (!out)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_10d0_0000 @ 10d0:0000 -> GfxPort_Create (GR_PortOpen).
     * GETDC + CREATECOMPATIBLEDC + bitmap via 10d0:0cfa; port struct ~0x20
     * words. SDL_Texture bridge TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_port_alloc_bitmap(SDL_Renderer *ren, int w, int h,
                                  SDL_Texture **out)
{
    SDL_Texture *tex;

    /* Orig FUN_10d0_0cfa @ 10d0:0cfa -> GfxPort_AllocBitmap (bitmap
     * helper behind GfxPort_Create @ 10d0:0000 / GR_PortOpen). GETDC +
     * CREATECOMPATIBLEDC + bitmap select into the new port maps to
     * SDL_CreateTexture INDEX8 TARGET: INDEX8 keeps ANIMATEPALETTE
     * possible via SDL_SetPaletteColors, TARGET allows the Blit @
     * 10d0:08f1 / PresentRect @ 10d0:0854 render-target path. Error
     * mapping: NULL -> NOENT, w/h <= 0 -> RANGE, CreateTexture fail ->
     * IO (device TARGET alloc; host-side SDL_CreatePalette fails map
     * to NOMEM instead, see palette_animate_range). */
    if (out)
        *out = NULL;
    if (!ren || !out)
        return CIV_ERR_NOENT;
    if (w <= 0 || h <= 0)
        return CIV_ERR_RANGE;
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_INDEX8,
                            SDL_TEXTUREACCESS_TARGET, w, h);
    if (!tex)
        return CIV_ERR_IO;
    *out = tex;
    return CIV_OK;
}

civ_error_t gfx_port_set_active(gfx_port_t *port)
{
    (void)port;
    if (!port)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_10d0_0956 @ 10d0:0956 -> GfxPort_SetActive. Selects
     * the drawing port/DC; SDL render-target select TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_port_present_rect(gfx_port_t *port, SDL_Renderer *ren,
                                  const SDL_Rect *rect)
{
    /* Orig FUN_10d0_0854 @ 10d0:0854 -> GfxPort_PresentRect. BITBLT rop
     * 0xcc0020 (SRCCOPY) to the screen maps to SDL_RenderTexture (NULL
     * rect = whole texture via NULL src/dst, else rect as dst with NULL
     * src); VALIDATERECT is a no-op on SDL3 (no HWND/dirty-region to
     * validate). Y2 HoF/show path (see halloffame.h Y2 flow):
     * HoF_Show @ 1080:0119 (dialog 0x94) + HoF_VictoryCeremony @ 1080:06ff
     * (pyramid frames) present through here after palette_apply_to_port
     * @ 1200:00a8/1090:0376. Z2 dump synthetic ok (doc only, Blit @
     * 10d0:08f1 / AllocBitmap @ 10d0:0cfa impls untouched): rsc_dump
     * present_frame (64x64 gradient via gfx_indexed_to_texture +
     * SDL_RenderTexture + video_present) shares this present core and
     * passes on dummy video. AA2: synthetic surface/texture ok links to
     * the probe gate (gfx_probe_rsc_image @ 1198:xxxx raw gate +
     * gfx_decode_rle CvPc path, see gfx_indexed.h); real frames stay
     * gated there. AD2 (doc only, no impl change): frames reach this
     * present only after the gfx_rle_expand_best dispatcher (RLL/INV/
     * LZSS/RAW0/2/4/8 best-method + per-method fail log via
     * gfx_rle_expand_best_ex + gfx_try_fail_str/gfx_expand_method_str).
     * Error mapping: NULL -> NOENT, missing
     * tex/size fail -> BAD_DATA, SDL_RenderTexture fail -> IO (NOMEM
     * never from present, only from CreatePalette/Texture alloc paths). */
    SDL_FRect dst;
    const SDL_FRect *dstp = NULL;

    if (!port || !ren)
        return CIV_ERR_NOENT;
    if (!port->tex)
        return CIV_ERR_BAD_DATA;
    if (rect) {
        dst.x = (float)rect->x;
        dst.y = (float)rect->y;
        dst.w = (float)rect->w;
        dst.h = (float)rect->h;
        dstp = &dst;
    }
    if (!SDL_RenderTexture(ren, port->tex, NULL, dstp))
        return CIV_ERR_IO;
    return CIV_OK;
}

civ_error_t gfx_port_blit(gfx_port_t *dst, const gfx_port_t *src,
                          const SDL_Rect *src_rect, int dst_x, int dst_y)
{
    /* Orig FUN_10d0_08f1 @ 10d0:08f1 -> GfxPort_Blit. Port-to-port
     * BITBLT rop 0xcc0020 (SRCCOPY) via SDL_SetRenderTarget(dst tex) +
     * SDL_RenderTexture(src tex) + target restore; VALIDATERECT is a
     * no-op on SDL3 (no dirty-region to validate). AD2 (doc only, no
     * impl change): blitted frames reach here only after the
     * gfx_rle_expand_best dispatcher (RLL/INV/LZSS/RAW0/2/4/8 + fail log
     * via gfx_rle_expand_best_ex, see gfx_indexed.h). Error mapping
     * (consistent with present_rect above): NULL -> NOENT, missing tex
     * -> BAD_DATA, degenerate src_rect -> RANGE, SDL target/Render/
     * restore fail -> IO. Ports without a borrowed ren (both NULL:
     * GfxPort_Create @ 10d0:0000 / CreateCompatible still stubbed, so
     * no DC wired yet) stay CIV_ERR_UNSUPPORTED with the exact missing
     * field map in the TODO below. */
    SDL_Renderer *ren;
    SDL_Texture *prev = NULL;
    SDL_FRect ssrc;
    SDL_FRect sdst;
    const SDL_FRect *srcp = NULL;
    float sw;
    float sh;

    if (!dst || !src)
        return CIV_ERR_NOENT;
    if (!dst->tex || !src->tex)
        return CIV_ERR_BAD_DATA;
    /* TODO: Orig ~0x20-word field map still TBD (src/dst HDC +
     * HBITMAP via 10d0:0cfa + clip-region words). Until
     * gfx_port_create / gfx_port_create_compatible wire ren from
     * GETDC/CREATECOMPATIBLEDC, ports carry ren == NULL and this
     * keeps UNSUPPORTED instead of guessing a renderer. */
    ren = dst->ren ? dst->ren : (SDL_Renderer *)src->ren;
    if (!ren)
        return CIV_ERR_UNSUPPORTED;
    if (src_rect) {
        if (src_rect->w <= 0 || src_rect->h <= 0)
            return CIV_ERR_RANGE;
        ssrc.x = (float)src_rect->x;
        ssrc.y = (float)src_rect->y;
        ssrc.w = (float)src_rect->w;
        ssrc.h = (float)src_rect->h;
        srcp = &ssrc;
        sw = (float)src_rect->w;
        sh = (float)src_rect->h;
    } else {
        if (!SDL_GetTextureSize(src->tex, &sw, &sh))
            return CIV_ERR_BAD_DATA;
        srcp = NULL;
    }
    sdst.x = (float)dst_x;
    sdst.y = (float)dst_y;
    sdst.w = sw;
    sdst.h = sh;
    /* Clip-rect placeholder (Orig SelectClipRgn): intersect dst rect
     * with dst->clip when active; zero-area after clip is a no-op OK. */
    if (dst->has_clip) {
        float x2 = sdst.x + sdst.w;
        float y2 = sdst.y + sdst.h;
        float cx = dst->clip.x > (int)sdst.x ? (float)dst->clip.x : sdst.x;
        float cy = dst->clip.y > (int)sdst.y ? (float)dst->clip.y : sdst.y;
        float cx2 = (float)(dst->clip.x + dst->clip.w) < x2
            ? (float)(dst->clip.x + dst->clip.w)
            : x2;
        float cy2 = (float)(dst->clip.y + dst->clip.h) < y2
            ? (float)(dst->clip.y + dst->clip.h)
            : y2;
        if (cx2 <= cx || cy2 <= cy)
            return CIV_OK;
        sdst.x = cx;
        sdst.y = cy;
        sdst.w = cx2 - cx;
        sdst.h = cy2 - cy;
    }
    prev = SDL_GetRenderTarget(ren);
    if (!SDL_SetRenderTarget(ren, dst->tex))
        return CIV_ERR_IO;
    if (!SDL_RenderTexture(ren, src->tex, srcp, &sdst)) {
        (void)SDL_SetRenderTarget(ren, prev);
        return CIV_ERR_IO;
    }
    if (!SDL_SetRenderTarget(ren, prev))
        return CIV_ERR_IO;
    return CIV_OK;
}

civ_error_t mainport_get_or_create(SDL_Renderer *ren, gfx_port_t **out)
{
    if (out)
        *out = NULL;
    (void)ren;
    if (!ren || !out)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_1098_114A @ 1098:114a -> MainPort_GetOrCreate () ->
     * port/DC. GETDC + SELECTPALETTE/REALIZEPALETTE + GfxPort_SetActive
     * @ 10d0:0956 bridge, called before WdwMap_Open @ 1208:0af1 /
     * WdwSmMap_Open @ 1210:025b / WdwStatus_Open @ 1218:0386 in
     * boot_open_windows. SDL3 render-target select TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_port_fill_or_copy(gfx_port_t *dst, const gfx_port_t *src,
                                  const SDL_Rect *rect, int dst_x, int dst_y)
{
    (void)src;
    (void)rect;
    (void)dst_x;
    (void)dst_y;
    if (!dst)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_10c0_xxxx @ 10c0:xxxx -> GfxPort_FillOrCopy (between
     * HMEMCPY @ 10b8:079d and BITBLT @ 10d0:0854/08f1; Anim_AdvanceFrames
     * @ 10b0:0461 blit helper). NULL src -> FILLRECT, else BITBLT rop
     * 0xcc0020 (SRCCOPY); SDL_RenderFillRect vs SDL_RenderTexture TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_port_load_rsc_frame(SDL_Renderer *ren,
                                    const rsc_indexed_t *frame,
                                    gfx_port_t **out)
{
    if (out)
        *out = NULL;
    (void)ren;
    (void)frame;
    if (!ren || !frame || !out)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_1198_xxxx @ 1198:xxxx -> GfxPort_LoadRscFrame
     * (display side of Rsc_DecodeBitmap, see rsc_loader.h
     * rsc_decode_bitmap). GfxPort_Create @ 10d0:0000 via
     * GfxPort_AllocBitmap @ 10d0:0cfa + Palette_Init @ 1090:0488 /
     * Palette_AnimateRange @ 1090:0376 (CREATEPALETTE/SELECTPALETTE) +
     * GfxPort_Blit @ 10d0:08f1 / GfxPort_PresentRect @ 10d0:0854
     * (BITBLT); SDL INDEX8 texture + SDL_SetPaletteColors + present TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t util_draw_progress_bar(gfx_port_t *port, const SDL_Rect *rect,
                                   int value, int max)
{
    (void)port;
    (void)rect;
    (void)value;
    (void)max;
    if (!port || !rect)
        return CIV_ERR_NOENT;
    if (max <= 0 || value < 0 || value > max)
        return CIV_ERR_RANGE;
    /* TODO: Orig FUN_11d0_xxxx @ 11d0:xxxx -> Util_DrawProgressBar
     * (progress-UI for Util_Progress @ 1000:29ea calc over the 0x1248
     * range + Sys_Yield per row; calc stays in sys, do not edit sys.h).
     * Row rect fill via the GfxPort_Blit @ 10d0:08f1 path; SDL_RenderFillRect
     * for the filled/unfilled halves TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_port_change_color(gfx_port_t *port, int old_index,
                                  int new_index)
{
    (void)old_index;
    (void)new_index;
    if (!port)
        return CIV_ERR_NOENT;
    if (old_index < 0 || old_index > 255 || new_index < 0 || new_index > 255)
        return CIV_ERR_RANGE;
    /* TODO: Orig FUN_10d0_09ee @ 10d0:09ee -> GfxPort_ChangeColor /
     * GR_PortChangeColor tail (GETBITMAPBITS remap). Orig scans the DIB
     * bits via GETBITMAPBITS, swaps old_index -> new_index pixels, and
     * writes back via SETBITMAPBITS; follows the Create @ 10d0:0000 /
     * SetActive @ 10d0:0956 / PresentRect @ 10d0:0854 / Blit @ 10d0:08f1
     * / AllocBitmap @ 10d0:0cfa / FillOrCopy @ 10c0:xxxx / LoadRscFrame
     * @ 1198:xxxx chain. SDL3 options: (a) true pixel remap via
     * SDL_LockTexture on the port INDEX8 TARGET backing store
     * (godpal.c 0x00..0xeb range, see gfx_indexed.h), or (b) palette
     * alias via one SDL_SetPaletteColors entry (old color := new
     * color). Bitmap bits unavailable while the store is a GPU TARGET
     * texture with no locked CPU copy, so this stays
     * CIV_ERR_UNSUPPORTED (no palette-alias fake: it would visibly
     * merge two indices). Needs: CPU-side pixel buffer or
     * LockTexture read-back proof. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_port_get_dc(gfx_port_t *port, SDL_Renderer **out_dc)
{
    if (out_dc)
        *out_dc = NULL;
    (void)port;
    if (!port || !out_dc)
        return CIV_ERR_NOENT;
    /* TODO: Orig GETDC step behind FUN_1098_114a @ 1098:114a
     * (MainPort_GetOrCreate) + FUN_10d0_0000 @ 10d0:0000 (GfxPort_Create /
     * GR_PortOpen). Borrows the display DC (SELECTPALETTE/REALIZEPALETTE
     * applied by the MainPort_GetOrCreate caller); SDL3 renderer stands
     * in for the HDC, borrow only. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_port_release_dc(gfx_port_t *port, SDL_Renderer *dc)
{
    (void)port;
    (void)dc;
    if (!port || !dc)
        return CIV_ERR_NOENT;
    /* TODO: Orig RELEASEDC pair to gfx_port_get_dc above (same
     * FUN_1098_114a @ 1098:114a + FUN_10d0_0000 @ 10d0:0000 sites).
     * SDL3 renderer borrow needs no release; pairing kept for RE-map
     * fidelity. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_port_create_compatible(SDL_Renderer *ren, int w, int h,
                                       gfx_port_t **out)
{
    if (out)
        *out = NULL;
    (void)ren;
    (void)w;
    (void)h;
    if (!out)
        return CIV_ERR_NOENT;
    if (w <= 0 || h <= 0)
        return CIV_ERR_RANGE;
    /* TODO: Orig CREATECOMPATIBLEDC + FUN_10d0_0cfa @ 10d0:0cfa bitmap
     * select -> GfxPort_CreateCompatible. Compatible-port sibling of
     * GfxPort_Create @ 10d0:0000 (same ~0x20-word port struct);
     * complements gfx_port_alloc_bitmap (raw texture) by returning the
     * owning port. SDL3 TBD: target-capable texture selected into the
     * new port. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t palette_apply_to_port(gfx_port_t *port, int first, int count)
{
    /* Orig FUN_1200_00a8 @ 1200:00a8 MainPort_ApplyPalette tail +
     * FUN_1090_0376 @ 1090:0376 Palette_AnimateRange. Applies
     * [first, first+count) via SDL_SetPaletteColors on the port INDEX8
     * backing store (SDL_GetTexturePalette when present, else a temp
     * 256-entry palette attached via SDL_SetTexturePalette; orig RGB
     * triples + flag byte copy then ANIMATEPALETTE, opcodes 1/0xe).
     * Y2 HoF/show path (see halloffame.h Y2 flow): range guard
     * ([first, first+count) in [0,256)) feeds HoF_Show @ 1080:0119 +
     * HoF_VictoryCeremony @ 1080:06ff fades before gfx_port_present_rect
     * @ 10d0:0854 (SDL_RenderTexture). Colors feed from gfx_godpal_get
     * (godpal.c 0x00..0xeb, see gfx_indexed.h) + black tail past 0xec
     * (LOGPALETTE 0x300 zero tail); propagates UNSUPPORTED until the dump
     * lands (no fake). */
    SDL_Color colors[256];
    SDL_Palette *pal = NULL;
    SDL_Palette *owned = NULL;
    int i;

    if (!port)
        return CIV_ERR_NOENT;
    if (first < 0 || count <= 0 || first + count > 256)
        return CIV_ERR_RANGE;
    if (!port->tex)
        return CIV_ERR_BAD_DATA;
    for (i = 0; i < count; i++) {
        int idx = first + i;
        if (idx < (int)GFX_GODPAL_COUNT) {
            uint8_t rgb[3];
            civ_error_t rc = gfx_godpal_get(idx, rgb);
            if (rc != CIV_OK)
                return rc;
            colors[i].r = rgb[0];
            colors[i].g = rgb[1];
            colors[i].b = rgb[2];
            colors[i].a = 255;
        } else {
            colors[i].r = 0;
            colors[i].g = 0;
            colors[i].b = 0;
            colors[i].a = 255;
        }
    }
    pal = SDL_GetTexturePalette(port->tex);
    if (!pal) {
        owned = SDL_CreatePalette(256);
        if (!owned)
            return CIV_ERR_NOMEM;
        pal = owned;
    }
    if (!SDL_SetPaletteColors(pal, colors, first, count)) {
        if (owned)
            SDL_DestroyPalette(owned);
        return CIV_ERR_IO;
    }
    if (owned) {
        int attached = (int)SDL_SetTexturePalette(port->tex, owned);
        SDL_DestroyPalette(owned);
        if (!attached)
            return CIV_ERR_IO;
    }
    return CIV_OK;
}

void video_shutdown_display(void)
{
    /* Orig: FUN_1008_0329 @ 1008:0329 WinMain_Shutdown video-only tail.
     * DESTROYWINDOW for the display ports/windows plus
     * REMOVEFONTRESOURCE(CIV_FONT_FILE "civfonts.fon" @ 1420:00a4/00fe,
     * added by Font_EnumFamiliesInit @ 1008:0e99; NE DLL/font container
     * 244224 B, MZ "MZ" + e_lfanew 0x250 + NE "NE" + RT_FONTDIR x1/RT_FONT
     * x21 parsed OS-free via font_fon_parse in boot.h/boot.c, borrowed
     * bytes, no TTF raster; ENUMFONTFAMILIES/SDL_ttf bridge TBD, see
     * boot.c font_face_count/list stubs); KILLTIMER/
     * FREEPROCINSTANCE tails live in map/dialog shutdown (see boot.c
     * boot_shutdown). SDL3 window/renderer stay caller-owned, nothing to
     * free yet; no globals, no exit(). AF2 doc only: Blit @ 10d0:08f1 /
     * AllocBitmap @ 10d0:0cfa / PresentRect @ 10d0:0854 impls untouched. */
}

civ_error_t font_blit_glyph(SDL_Window *win, SDL_Renderer *ren,
                            SDL_Texture *tex, int x, int y,
                            const SDL_Surface *glyph)
{
    /* AK2 font blit @ 1008:0e99 (see video.h). SDL bridge port on the
     * GfxPort_Blit @ 10d0:08f1 / PresentRect @ 10d0:0854 SRCCOPY family:
     * temp texture from the borrowed glyph surface + render-target blit
     * onto the caller-owned tex + restore. Borrowed glyph from
     * font_glyph_to_surface (w/h + MSB-first bits, bit 7 = leftmost; never
     * freed here); caller-owned tex never owned/freed here; win/ren
     * borrowed (win ignored, may be NULL; ren never owned). No globals, no
     * exit(). Blit @ 10d0:08f1 / AllocBitmap @ 10d0:0cfa / PresentRect @
     * 10d0:0854 / ChangeColor @ 10d0:09ee impls untouched. Parity with
     * font_fnt_bits MSB-first + font_render_glyph tight stride (w+7)/8. */
    SDL_Texture *tmp = NULL;
    SDL_Texture *prev = NULL;
    SDL_FRect dst;

    (void)win;
    if (!ren || !tex || !glyph)
        return CIV_ERR_NOENT;
    if (glyph->w <= 0 || glyph->h <= 0)
        return CIV_ERR_BAD_DATA;
    tmp = SDL_CreateTextureFromSurface(ren, (SDL_Surface *)glyph);
    if (!tmp)
        return CIV_ERR_IO;
    prev = SDL_GetRenderTarget(ren);
    if (!SDL_SetRenderTarget(ren, tex)) {
        SDL_DestroyTexture(tmp);
        return CIV_ERR_IO;
    }
    dst.x = (float)x;
    dst.y = (float)y;
    dst.w = (float)glyph->w;
    dst.h = (float)glyph->h;
    if (!SDL_RenderTexture(ren, tmp, NULL, &dst)) {
        (void)SDL_SetRenderTarget(ren, prev);
        SDL_DestroyTexture(tmp);
        return CIV_ERR_IO;
    }
    if (!SDL_SetRenderTarget(ren, prev)) {
        SDL_DestroyTexture(tmp);
        return CIV_ERR_IO;
    }
    SDL_DestroyTexture(tmp);
    return CIV_OK;
}

int art_progress_fill_x(int bar_w, int done, int total)
{
    /* Orig FUN_10b8_062d @ 10b8:062d -> Art_DrawProgressFill width math.
     * Decompiler: SETRECT(right = left + (long)((rect[2] - *rect) *
     * param_5) / param_4, ...) then FILLRECT brush param_6. Guarded
     * here (orig DIV faulted on total == 0). OS-free 1:1, no Win16. */
    if (total <= 0 || done <= 0 || bar_w <= 0)
        return 0;
    if (done >= total)
        return bar_w;
    return (int)(((long)bar_w * (long)done) / (long)total);
}

civ_error_t art_load_frame(int frame_id, gfx_port_t **out)
{
    (void)frame_id;
    /* TODO: Orig FUN_10b8_0e00 @ 10b8:0e00 (Art_LoadFrame:
     * Art_LoadFrameData @ 10b8:079d with 0x14-kind param: disk path via
     * Art_LoadDiskFile @ 10b8:0aa8 (_LOPEN/_HREAD + save_unpack @
     * 10f8:040a when DAT_1420_17a8 != 0) else RSC path via
     * Art_LoadResPaletted @ 10b8:11bc (ResMgr_GetById + 0xec palette
     * expand + Art_DecodeLZW @ 10b8:158c, cf. gfx_lzw_decode @
     * 1420:2372/23ce in gfx_indexed.h, no duplicate) + optional
     * Art_SaveConvertedFrame @ 10b8:0c1c (_LCREAT/_HWRITE + save_pack @
     * 10f8:05da), then GfxPort_Create @ 10d0:0000 + Gfx_StoreDC @
     * 1098:1104; then Art_PortDataToBitmap @ 10b8:0e36
     * (GR_PortDataToBitmap: even-width direct vs odd-width pad via
     * DAT_1420_17a6 vs compatible-DC + SETBITMAPBITS + BITBLT 0xcc0020
     * rows); progress via Art_DrawProgressFill @ 10b8:062d +
     * Art_PumpConvertCancel @ 10b8:06a0 + Art_PreviewFrame @ 10b8:076a +
     * Art_CheckDiskSpace @ 10b8:04df 9 MB gate in Art_MaybeConvertArtwork
     * @ 10b8:0000, see boot.h art_maybe_convert_artwork) -> SDL3
     * INDEX8 texture bridge TBD (see gfx_port_load_rsc_frame). */
    if (out)
        *out = NULL;
    if (!out)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}
