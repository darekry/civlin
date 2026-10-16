/* Video: SDL3 backend. Orig GDI paths in video.h; Win16 handles have no
 * Linux equivalent, so the port owns the SDL window/renderer created by
 * the caller (main.c) and validates them here (no globals, no exit). */
#include "video.h"

#include <stdlib.h>

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
    /* Orig GfxPort_Create @ 10d0:0000 -> GR_PortOpen (see video.h P4
     * block). GETDC + CREATECOMPATIBLEDC borrow the caller renderer
     * (borrow only, never owned/freed here); bitmap via
     * GfxPort_AllocBitmap @ 10d0:0cfa even width ((w+1)/2*2). SDL3 port:
     * malloc gfx_port_t + SDL_CreateTexture INDEX8 TARGET backing store
     * (INDEX8 keeps ANIMATEPALETTE possible via SDL_SetPaletteColors;
     * TARGET allows the Blit @ 10d0:08f1 / PresentRect @ 10d0:0854
     * render-target path). Error mapping: NULL out -> NOENT; NULL ren
     * -> NOENT; w/h <= 0 -> RANGE; malloc fail -> NOMEM; bitmap fail ->
     * IO with *out NULL (no leak). */
    gfx_port_t *port = NULL;
    SDL_Texture *tex = NULL;
    civ_error_t rc;

    if (out)
        *out = NULL;
    if (!out || !ren)
        return CIV_ERR_NOENT;
    if (w <= 0 || h <= 0)
        return CIV_ERR_RANGE;
    port = (gfx_port_t *)malloc(sizeof(*port));
    if (!port)
        return CIV_ERR_NOMEM;
    port->tex = NULL;
    port->w = w;
    port->h = h;
    port->ren = ren;
    port->has_clip = 0;
    port->pal = NULL;
    port->clip.x = 0;
    port->clip.y = 0;
    port->clip.w = 0;
    port->clip.h = 0;
    rc = gfx_port_alloc_bitmap(ren, w, h, &tex);
    if (rc != CIV_OK) {
        free(port);
        return rc;
    }
    port->tex = tex;
    *out = port;
    return CIV_OK;
}

void gfx_port_destroy(gfx_port_t *port)
{
    /* Orig GfxPort_Close @ 10d0:0172 free tail (ClearActive prologue
     * caller-side via gfx_port_clear_active + DELETEDC + DELETEOBJECT +
     * GLOBALFREE x2 + zero words; GETDC/CREATECOMPATIBLEDC borrows need
     * no release, renderer stays caller-owned). SDL3: destroy the INDEX8
     * TARGET backing store + free the port (palette refs are borrowed,
     * never freed here). NULL is a no-op. No globals, no exit(). */
    if (!port)
        return;
    if (port->tex)
        SDL_DestroyTexture(port->tex);
    free(port);
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
    /* Orig GfxPort_SetActive @ 10d0:0956 (see video.h P4 block):
     * Gfx_SetCurrentDC(port+0x16) + 17cc = port always + 17ae = port
     * unless the 12d8:44ff+0x43 dialog port (slots caller-side via
     * gfx_port_clear_active, LIVE). SDL3 equivalent is an SDL
     * render-target select onto the port INDEX8 TARGET backing store
     * (rop 0xcc0020 SRCCOPY family, same as PresentRect @ 10d0:0854 /
     * Blit @ 10d0:08f1). gfx_port_create @ 10d0:0000 wires ren/tex
     * (GETDC/CREATECOMPATIBLEDC borrow + AllocBitmap @ 10d0:0cfa), so a
     * NULL ren/tex here is a corrupt port, not an unimplemented Win16
     * path: BAD_DATA (consistent with gfx_port_present_rect missing-tex
     * guard). NULL port -> NOENT; SDL target-set fail -> IO. Ref
     * unpacked/civ.exe read-only (NE Win3.x, 832512 B): anchor 10d0:0956
     * + slot words 17cc/17ae caller-side; no handle guessed, no fake. */
    if (!port)
        return CIV_ERR_NOENT;
    if (!port->tex || !port->ren)
        return CIV_ERR_BAD_DATA;
    if (!SDL_SetRenderTarget(port->ren, port->tex))
        return CIV_ERR_IO;
    return CIV_OK;
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
     * via gfx_rle_expand_best_ex, see gfx_indexed.h). gfx_port_create @
     * 10d0:0000 / gfx_port_create_compatible wire ren (GETDC/
     * CREATECOMPATIBLEDC borrow) + tex via AllocBitmap @ 10d0:0cfa, so a
     * missing ren/tex here is a corrupt port (BAD_DATA, consistent with
     * present_rect above), not an unimplemented path: no fake renderer
     * is guessed. Error mapping: NULL -> NOENT, missing tex/ren ->
     * BAD_DATA, degenerate src_rect -> RANGE, SDL target/Render/
     * restore fail -> IO. Ref unpacked/civ.exe read-only (NE Win3.x):
     * anchor 10d0:08f1, same rop family as PresentRect @ 10d0:0854. */
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
    ren = dst->ren ? dst->ren : (SDL_Renderer *)src->ren;
    if (!ren)
        return CIV_ERR_BAD_DATA;
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
    /* Orig FUN_1098_114A @ 1098:114a -> MainPort_GetOrCreate (see video.h).
     * P3 SDL_Texture probe bridge (PARTIAL, no fake): the window-sized
     * port geometry is proven here read-only (SDL_GetRenderOutputSize +
     * gfx_port_create @ 10d0:0000 INDEX8 TARGET + gfx_port_destroy probe;
     * same backing store the Anim_OpenSequence @ 10b0:008a GfxPort x2 pair
     * and the WdwMap_Open @ 1208:0af1 / WdwSmMap_Open @ 1210:025b /
     * WdwStatus_Open @ 1218:0386 views in boot_open_windows need), but
     * the port is NOT returned yet: SELECTPALETTE/REALIZEPALETTE (via
     * palette_apply_to_port @ 1200:00a8/1090:0376, blocked on the godpal
     * runtime capture, see gfx_godpal_get) + the main-screen SetActive @
     * 10d0:0956 render-target semantics stay TBD, so no fake CIV_OK.
     * Error mapping: NULL -> NOENT (*out NULL when present); output-size
     * query fail -> IO; zero size -> BAD_DATA; create fail propagates
     * (NOMEM/RANGE/IO, *out NULL, no leak); device tail -> UNSUPPORTED. */
    int w = 0;
    int h = 0;
    gfx_port_t *probe = NULL;
    civ_error_t rc;

    if (out)
        *out = NULL;
    if (!ren || !out)
        return CIV_ERR_NOENT;
    if (!SDL_GetRenderOutputSize(ren, &w, &h))
        return CIV_ERR_IO;
    if (w <= 0 || h <= 0)
        return CIV_ERR_BAD_DATA;
    rc = gfx_port_create(ren, w, h, &probe);
    if (rc != CIV_OK)
        return rc;
    /* P3 tail probe (Orig FUN_1098_114A @ 1098:114a): SELECTPALETTE /
     * REALIZEPALETTE via palette_apply_to_port @ 1200:00a8/1090:0376
     * (full godpal 0..0xec range; blocked on the godpal runtime capture,
     * see gfx_godpal_get, so no fake commit) + main-screen
     * GfxPort_SetActive @ 10d0:0956 render-target select exercised on
     * the probe (target saved/restored, no leaked state). Port still NOT
     * returned: palette commit + main-screen active semantics stay TBD. */
    {
        SDL_Texture *prev = SDL_GetRenderTarget(ren);

        (void)palette_apply_to_port(probe, 0, (int)GFX_GODPAL_COUNT);
        (void)gfx_port_set_active(probe);
        (void)SDL_SetRenderTarget(ren, prev);
    }
    gfx_port_destroy(probe);
    /* TODO: Orig FUN_1098_114A @ 1098:114a -> MainPort_GetOrCreate () ->
     * port/DC. GETDC + SELECTPALETTE/REALIZEPALETTE (via
     * palette_apply_to_port @ 1200:00a8/1090:0376, blocked on the godpal
     * runtime capture, see gfx_godpal_get) + GfxPort_SetActive @
     * 10d0:0956 bridge, called before WdwMap_Open @ 1208:0af1 /
     * WdwSmMap_Open @ 1210:025b / WdwStatus_Open @ 1218:0386 in
     * boot_open_windows. SDL3 render-target select + palette commit TBD
     * (tail exercised as a probe above with target restore); returning
     * the probed port as CIV_OK now would fake the palette half. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_port_fill_or_copy(gfx_port_t *dst, const gfx_port_t *src,
                                  const SDL_Rect *rect, int dst_x, int dst_y)
{
    if (!dst)
        return CIV_ERR_NOENT;
    if (src)
        /* Orig FUN_10c0_xxxx @ 10c0:xxxx copy half (between HMEMCPY @
         * 10b8:079d and BITBLT @ 10d0:0854/08f1; Anim_AdvanceFrames @
         * 10b0:0461 blit helper): else-branch BITBLT rop 0xcc0020
         * (SRCCOPY) delegates 1:1 to gfx_port_blit @ 10d0:08f1 (same rop
         * family, same NOENT/BAD_DATA/RANGE/IO/UNSUPPORTED mapping; NULL
         * rect = whole source, kept). No new device code here. */
        return gfx_port_blit(dst, src, rect, dst_x, dst_y);
    /* TODO: Orig same fill half (NULL src -> FILLRECT solid fill of rect
     * at dst_x/dst_y). Fill brush/color has no param in this signature
     * (orig brush via Gfx_MakeBrush @ 1098:00b5 path), so no fake color
     * is invented here; SDL_RenderFillRect on the port INDEX8 TARGET via
     * the borrowed ren stays TBD. */
    (void)rect;
    (void)dst_x;
    (void)dst_y;
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
    /* Orig CREATECOMPATIBLEDC + GfxPort_AllocBitmap @ 10d0:0cfa bitmap
     * select -> GfxPort_CreateCompatible (see video.h P4 block).
     * Compatible-port sibling of GfxPort_Create @ 10d0:0000 (same
     * ~0x20-word port struct, same even-width bitmap); complements
     * gfx_port_alloc_bitmap (raw texture) by returning the owning port.
     * SDL3: identical bridge to gfx_port_create (borrowed renderer +
     * INDEX8 TARGET backing store). Same error mapping: NULL out/ren
     * -> NOENT; w/h <= 0 -> RANGE; malloc -> NOMEM; bitmap -> IO. */
    if (out)
        *out = NULL;
    if (!out || !ren)
        return CIV_ERR_NOENT;
    if (w <= 0 || h <= 0)
        return CIV_ERR_RANGE;
    return gfx_port_create(ren, w, h, out);
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

int video_unit_frame_index(int civ, int type)
{
    /* Orig Unit_DrawSingle @ 1110:0dba icon fetch: (civ*0x20+type)*2-0x6c30
     * word (badge variant in Unit_DrawStatusBadge @ 1110:0ff6 adds +0x40 to
     * the type term; caller-side). HP is not drawn (one-roll kill). */
    if (civ < 0 || type < 0)
        return -1;
    return civ * 0x20 + type;
}

civ_error_t video_unit_tile_box(int dx, int dy, int view_x, int view_y,
                                int *x0_out, int *y0_out)
{
    /* Orig Gfx_ClipTileRect @ 1208:2061: SETRECT tile 0x20 box + clip query
     * @ 1100:0685 (0 = off-screen skip in Unit_DrawSingle). Wrap (Map_WrapX
     * @ 1128:177e) lives in map_wrap_x, see map.h; caller passes wrapped dx. */
    if (!x0_out || !y0_out)
        return CIV_ERR_NOENT;
    *x0_out = view_x * 0x20 - dx;
    *y0_out = view_y * 0x20 - dy;
    return CIV_OK;
}

civ_error_t video_draw_unit_sprite(gfx_port_t *port, int frame, int x, int y)
{
    (void)frame;
    (void)x;
    (void)y;
    /* TODO: Orig Unit_DrawSingle @ 1110:0dba + Unit_DrawStatusBadge @
     * 1110:0ff6 blit tail (GfxPort_SetActive @ 10d0:0956 +
     * Gfx_DrawTechIcon @ 11a8:0cb1 icon cell + badge + BITBLT @ 10d0:0854/
     * 08f1). Needs the icon-cell source rects + INDEX8 frames (see
     * gfx_indexed.h gfx_indexed_to_texture + gfx_port_present_rect/blit).
     * What-to-draw (video_unit_frame_index/box + game.h
     * unit_stack_draw_pick/unit_draw_single_layer LIVE) is done caller-side. */
    if (!port)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (6): clip-rect intersect (goal 3). OS-free 1:1 LIVE. */

civ_error_t video_clip_rect(const SDL_Rect *a, const SDL_Rect *b,
                            SDL_Rect *out, int *visible_out)
{
    /* Orig Gfx_IntersectRect @ 1100:0685: Util_MemCopyFar clip copy, then
     * left/top = max, right/bottom = min; empty (right <= left or bottom <=
     * top, orig zeroes the rect) -> 0 off-screen skip in Gfx_ClipTileRect @
     * 1208:2061 / Unit_DrawSingle @ 1110:0dba. SDL_Rect x/y/w/h adapted:
     * right = x + w, bottom = y + h. */
    int al, at, ar, ab;
    int bl, bt, br, bb;
    int l, t, r, bo;

    if (!a || !b || !out || !visible_out)
        return CIV_ERR_NOENT;
    al = a->x;
    at = a->y;
    ar = a->x + a->w;
    ab = a->y + a->h;
    bl = b->x;
    bt = b->y;
    br = b->x + b->w;
    bb = b->y + b->h;
    l = (al > bl) ? al : bl;
    t = (at > bt) ? at : bt;
    r = (ar < br) ? ar : br;
    bo = (ab < bb) ? ab : bb;
    if (r <= l || bo <= t) {
        out->x = 0;
        out->y = 0;
        out->w = 0;
        out->h = 0;
        *visible_out = 0;
        return CIV_OK;
    }
    out->x = l;
    out->y = t;
    out->w = r - l;
    out->h = bo - t;
    *visible_out = 1;
    return CIV_OK;
}

int unit_flag_icon_index(int civ, int type)
{
    /* Orig Unit_DrawFlagIcon @ 11c8:008a index via Unit_DrawStatusBadge @
     * 1110:0ff6 (civ*0x20+type+0x40) + Census_ShowUnitList/TerrainCounts @
     * 1018:067b/09cc (same +0x40 per row) + 0x1d fortify / 0x0c bar ids
     * stay caller-side. Vs video_unit_frame_index (map sprite, no +0x40). */
    if (civ < 0 || type < 0)
        return -1;
    return civ * 0x20 + type + 0x40;
}

civ_error_t gfx_icon_cell_box(int px, int py, int cell_w, int cell_h,
                              SDL_Rect *out)
{
    /* Orig Gfx_DrawIconClipped @ 11e8:0674 cell: SETRECT from the icon w/h
     * (GLOBALLOCK) + OFFSETRECT to (px,py); clip itself is video_clip_rect
     * (Gfx_IntersectRect @ 1100:0685) caller-side. */
    if (!out)
        return CIV_ERR_NOENT;
    if (cell_w <= 0 || cell_h <= 0)
        return CIV_ERR_RANGE;
    out->x = px;
    out->y = py;
    out->w = cell_w;
    out->h = cell_h;
    return CIV_OK;
}

civ_error_t gfx_draw_icon_clipped(int icon, int px, int py)
{
    (void)icon;
    (void)px;
    (void)py;
    /* TODO: Orig Gfx_DrawIconClipped @ 11e8:0674 (Gfx_DrawTechIcon @
     * 11a8:0cb1 icon blit + IntersectRect @ 1100:0685 clip + PresentRect @
     * 10d0:0854 when DAT_12b0_009a; FUN_1220_0f2f visibility gate).
     * SDL indexed blit TBD (see gfx_port_present_rect). Cell math lives in
     * gfx_icon_cell_box above (LIVE). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_present_tile_rect(int x, int y)
{
    (void)x;
    (void)y;
    /* TODO: Orig Gfx_PresentTileRect @ 11e8:07c3 (SETRECT 0x20 box at (x,y)
     * + IntersectRect @ 1100:0685 clip + PresentRect @ 10d0:0854 when
     * DAT_12b0_009a; FUN_1220_0f2f gate). SDL present TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t city_draw_tile_view(int tx, int ty, int px, int py)
{
    (void)tx;
    (void)ty;
    (void)px;
    (void)py;
    /* TODO: Orig City_DrawTileView @ 1110:04d0 (city tile (tx,ty)->(px,py):
     * class/occupancy/wonder icons via Gfx_DrawTechIcon @ 11a8:0cb1 +
     * owner frame + IntersectRect @ 1100:0685 clip + label PresentRect when
     * DAT_12b0_009a). GfxPort blits -> SDL TBD. Clip math lives in
     * video_clip_rect (LIVE). */
    return CIV_ERR_UNSUPPORTED;
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

int art_frame_id_ok(int frame_id)
{
    /* Orig Art_LoadFrameData @ 10b8:079d frame-id gate (see video.h):
     * non-negative ids reach the disk/RSC tails, negative fault in orig.
     * OS-free 1:1, no Win16, no globals. */
    return (frame_id >= 0) ? 1 : 0;
}

int gfx_startup_port_size_ok(int w, int h)
{
    /* Orig GfxPort_Create @ 10d0:0000 x2 backing for the 0x94 intro host
     * (see video.h): 0x200x0x140 preset via dlg_preset_w/h (CIV_DLG_PRESET_W
     * /H in dialog.h). OS-free 1:1, no Win16, no globals. */
    return (w == 0x200 && h == 0x140) ? 1 : 0;
}

/* --- Chain 2026-10-16 (8): IntersectRect blits + present/alloc (goals 4/5) ---
 * OS-free 1:1 LIVE. No globals, no exit(), C17. See video.h chain block. */

int gfx_color_remap_match(int pixel, int old_index)
{
    /* Orig GfxPort_ChangeColor @ 10d0:09ee inner: if (*bits == param_4)
     * *bits = param_5 (byte DIB compare; 0..255 indices, godpal.c
     * 0x00..0xeb + zero-tail, see gfx_indexed.h). */
    return pixel == old_index ? 1 : 0;
}

int gfx_remap_stride(int w)
{
    /* Orig same pitch: ((w+1)/2)*2 (even-width bitmap; pad skipped per
     * row after the w-byte scan). */
    if (w < 0)
        return -1;
    return ((w + 1) / 2) * 2;
}

int gfx_remap_index(uint8_t *bits, size_t n, int old_index, int new_index)
{
    /* Orig GfxPort_ChangeColor @ 10d0:09ee GETBITMAPBITS + SETBITMAPBITS
     * CPU-side rule on explicit bits (no handles, no palette alias fake:
     * palette alias would merge two indices visibly, see
     * gfx_port_change_color STUB). */
    size_t i;
    int swapped = 0;

    if (!bits)
        return -1;
    if (old_index < 0 || old_index > 255 || new_index < 0 ||
        new_index > 255)
        return -1;
    for (i = 0; i < n; i++) {
        if (bits[i] == (uint8_t)old_index) {
            bits[i] = (uint8_t)new_index;
            swapped++;
        }
    }
    return swapped;
}

civ_error_t gfx_tile_box(int x, int y, SDL_Rect *out)
{
    /* Orig Gfx_PresentTileRect @ 11e8:07c3: SETRECT 0x20 box at (x,y) +
     * Gfx_IntersectRect @ 1100:0685 clip (caller-side via video_clip_rect
     * LIVE) + PresentRect @ 10d0:0854 when DAT_12b0_009a. */
    if (!out)
        return CIV_ERR_NOENT;
    out->x = x;
    out->y = y;
    out->w = 0x20;
    out->h = 0x20;
    return CIV_OK;
}

civ_error_t gfx_bar_fill_extent(int mode, int px, int w, int hgt,
                                int icon_w, int icon_h,
                                int *x1_out, int *h_out)
{
    /* Orig Gfx_FillBarRect @ 11e8:0504 (was FUN_11e8_0504): mode == 2 ?
     * x1 = px+w*2, h = hgt<<1 : x1 = px+icon_w, h = icon_h (icon via
     * FUN_11e8_0d81/0da8). */
    if (!x1_out || !h_out)
        return CIV_ERR_NOENT;
    if (mode == 2) {
        *x1_out = px + w * 2;
        *h_out = hgt << 1;
    } else {
        *x1_out = px + icon_w;
        *h_out = icon_h;
    }
    return CIV_OK;
}

int gfx_bar_fill_cap(int x1, int px, int mono)
{
    /* Orig same: if (mono == 0 (DAT_1420_17a6) && 0x1ff < x1 && 0 < px)
     * x1 = 0x1ff (driver clip). */
    if (mono == 0 && x1 > 0x1ff && px > 0)
        return 0x1ff;
    return x1;
}

int art_port_is_even_width(int w)
{
    /* Orig Art_PortDataToBitmap @ 10b8:0e36 head: w/2*2 == w -> direct
     * GfxPort_AllocBitmap @ 10d0:0cfa path (even-width BITBLT). */
    if (w < 0)
        return -1;
    return (w / 2 * 2 == w) ? 1 : 0;
}

int art_port_padded_width(int w)
{
    /* Orig same odd-width pad: ((w+1)/2)*2 (GLOBALALLOC + HMEMCPY rows
     * when DAT_1420_17a6 == 1, else compatible-DC rows). Same pitch as
     * gfx_remap_stride. */
    if (w < 0)
        return -1;
    return ((w + 1) / 2) * 2;
}

/* --- Chain 2026-10-16 (9): census badge + bar + port slots (goals 2/3/4) ---
 * OS-free 1:1 LIVE. No globals, no exit(), C17. See video.h chain block. */

int census_badge_table_off(int icon)
{
    /* Orig Census_DrawTechBadge @ 1018:24c5: Gfx_DrawTechIcon @ 11a8:0cb1(
     * *(icon*2+0x128), ...) word fetch feeding the tech blit (via
     * Gfx_BlitTechIcon @ 11a8:0f56 core). */
    if (icon < 0)
        return -1;
    return icon * 2 + 0x128;
}

civ_error_t census_badge_size(int *w_out, int *h_out)
{
    /* Orig same: SETRECT(...,0x60,0x35,0,0) fill box, OFFSETRECT to dx/dy
     * caller-side (position shares gfx_icon_cell_box). */
    if (!w_out || !h_out)
        return CIV_ERR_NOENT;
    *w_out = 0x60;
    *h_out = 0x35;
    return CIV_OK;
}

int gfx_scale_icon_dim(int v)
{
    /* Orig Gfx_ScaleIconX @ 11e8:0d81 + Gfx_ScaleIconY @ 11e8:0da8 shared
     * core: (long)(v * 0xa0) / 100 (x86 LONG divide, trunc toward zero;
     * used for icon w/h in Gfx_FillBarRect @ 11e8:0504 + coord scale in
     * GfxPort_SelectForCoords @ 1220:0f2f). */
    return (int)(((long)v * 0xa0L) / 100L);
}

int gfx_bar_fill_color(int mono, int civ_entry, int fallback)
{
    /* Orig Gfx_FillBarRect @ 11e8:0504 head: cVar1 = DAT_12d8_45fd;
     * if (DAT_12d8_28fc == 0) cVar1 = civ_table[param_7]. */
    return (mono == 0) ? civ_entry : fallback;
}

int gfx_bar_brush_is_alt(int mono, int param_7)
{
    /* Orig same tail: (DAT_12d8_28fc == 0 || param_7 != 0xf) ? civ brush
     * : DAT_1420_17a0 brush. Returns 1 when the alt brush wins. */
    return (mono != 0 && param_7 == 0xf) ? 1 : 0;
}

int gfx_bar_range_needs_fill(int x0, int x1, int y0, int y1)
{
    /* Orig Gfx_FillBarRectRange @ 11e8:0912 gate: (x0 == x1 || y0 == y1)
     * -> fill path, else nothing (already a rect). */
    return (x0 == x1 || y0 == y1) ? 1 : 0;
}

int gfx_bar_range_width(int x0, int x1)
{
    /* Orig same: x-swap when x1 < x0, then w = (x1-x0)+1. */
    int lo = (x0 < x1) ? x0 : x1;
    int hi = (x0 < x1) ? x1 : x0;

    return (hi - lo) + 1;
}

int gfx_bar_range_height(int y0, int y1)
{
    /* Orig same: h = (y1-y0)+1 (no y swap in orig). */
    return (y1 - y0) + 1;
}

int gfx_port_select_kind(int has_port, int x, int y)
{
    /* Orig GfxPort_SelectForCoords @ 1220:0f2f branch (device writes
     * caller-side: mode 2 SetActive + 17d0, mode 1 17d0, mode 3 scale +
     * 17d0/17cc). has_port mirrors DAT_1420_9948 != 0. */
    if (has_port != 0)
        return 3;
    if (x < 0 || 199 < x) {
        if (y < 8 || x < 200)
            return 0;
        return 2;
    }
    return 1;
}

int gfx_port_dialog_coord(int v, int base, int off)
{
    /* Orig same dialog remap: (v-base)*2-off (bases 8/200, offs
     * DAT_1408_00c4/c6 passed explicitly). */
    return (v - base) * 2 - off;
}

int gfx_port_palette_word(int mono, int word_at_14)
{
    /* Orig GfxPort_GetPalette @ 10d0:0262: DAT_12d8_28fc == 0 ?
     * *(port+0x14) : 0. */
    return (mono == 0) ? word_at_14 : 0;
}

const gfx_port_t *gfx_port_slot_after_clear(const gfx_port_t *slot,
                                            const gfx_port_t *port)
{
    /* Orig GfxPort_ClearActive @ 10d0:09a2 per-slot: if (slot == port)
     * slot = 0 (far-ptr compare; here borrowed pointers). Also the
     * GfxPort_Close @ 10d0:0172 prologue before GR_PortClose free. */
    return (slot == port) ? NULL : slot;
}

civ_error_t gfx_port_clear_active(const gfx_port_t *port,
                                  const gfx_port_t **active_io,
                                  const gfx_port_t **prev_io)
{
    /* Orig same over both slots (DAT_1420_17cc active + DAT_1420_17ae
     * prev) on explicit addresses (no globals). NULL port clears NULL
     * slots (no-op otherwise). */
    if (!active_io || !prev_io)
        return CIV_ERR_NOENT;
    *active_io = gfx_port_slot_after_clear(*active_io, port);
    *prev_io = gfx_port_slot_after_clear(*prev_io, port);
    return CIV_OK;
}

/* --- Chain 2026-10-16 (10): GfxPort create params + tech blit + palette.
 * OS-free 1:1 LIVE. No globals, no exit(), C17. See video.h. */

int gfx_port_bitmap_pitch(int w)
{
    /* Orig GfxPort_Create @ 10d0:0000 → GfxPort_AllocBitmap @ 10d0:0cfa
     * even width ((w+1)/2*2); odd widths pad (see art_port_padded_width;
     * GLOBALALLOC + HMEMCPY rows when mono DAT_1420_17a6 == 1, else
     * compatible-DC rows in Art_PortDataToBitmap @ 10b8:0e36). */
    if (w < 0)
        return -1;
    return ((w + 1) / 2) * 2;
}

int gfx_port_is_mono(int flag_28fc)
{
    /* Orig color gate DAT_12d8_28fc (0 = color/VGA, != 0 = mono/EGA):
     * GfxPort_GetPalette @ 10d0:0262 returns 0 when mono, Palette_CopyRange
     * @ 1098:201d skips, Gfx_MakeBrush @ 1098:00b5 takes solid path. */
    return (flag_28fc != 0) ? 1 : 0;
}

int gfx_tech_bg_resolve(int req_bg, int res_bg)
{
    /* Orig Gfx_BlitTechIcon @ 11a8:0f56 head: if (param_5 == 0) param_5 =
     * res_bg (icon word +4 via GLOBALLOCK); -1 stays transparent (fast
     * Gfx_CopyIconBits @ 11a8:13c0 memcpy path when scale/pad allow). */
    return (req_bg == 0) ? res_bg : req_bg;
}

int gfx_tech_needs_scale(int a, int b)
{
    /* Orig same via Gfx_GetIconScale @ 11a8:0000 (DAT_13a0_0000/0002 set
     * by Gfx_SetIconScale @ 11a8:002c): bVar22 = (a != b); scales w/h as
     * (w*a)/b when set (Gfx_CopyIconBits scaled loop else direct). */
    return (a != b) ? 1 : 0;
}

int gfx_tech_palette_pick(int bg, int mono, int req)
{
    /* Orig Gfx_DrawTechIcon @ 11a8:0cb1: (param_6 == -1 || DAT_1420_17a6
     * != 1) ? CONCAT11(0x14, param_6) : 0x1a feeding Gfx_BlitTechIcon. */
    if (bg == -1 || mono != 1)
        return req;
    return 0x1a;
}

int palette_copy_enabled(int mono, int count)
{
    /* Orig Palette_CopyRange @ 1098:201d head: if (DAT_12d8_28fc == 0 &&
     * param_5 != 0) copy (color only, nonzero count). */
    return (mono == 0 && count != 0) ? 1 : 0;
}

int palette_copy_range_len(int src_off, int dst_off, int count)
{
    /* Orig same 0xec cap: if (0xec < src+count) count = 0xec-src; then
     * local_e capped by 0xec-dst (word RGBx copy loop). Overrun → 0. */
    int n = count;

    if (n < 0)
        return 0;
    if (src_off < 0 || dst_off < 0)
        return 0;
    if (src_off + n > 0xec)
        n = 0xec - src_off;
    if (dst_off + n > 0xec)
        n = 0xec - dst_off;
    if (n < 0)
        n = 0;
    return n;
}

/* --- Chain 2026-10-16 (11): icon scale/buffer @ 11a8.
 * OS-free 1:1 LIVE + device STUBs. No globals, no exit(), C17. See video.h. */

int gfx_icon_buffer_size(int w, int h)
{
    /* Orig Gfx_AllocIconBuffer @ 11a8:0054: GLOBALALLOC(w*h+6) + w/h store. */
    if (w <= 0 || h <= 0)
        return -1;
    return w * h + 6;
}

int gfx_icon_scaled_dim(int v, int num, int den)
{
    /* Orig Gfx_CopyIconBits @ 11a8:13c0: (w*num)/den; ScaleIconX/Y to
     * (v*0xa0)/100 (gfx_scale_icon_dim). x86 LONG divide truncates. */
    if (den <= 0)
        return v;
    return (v * num) / den;
}

int gfx_icon_scale_is_unit(int num, int den)
{
    /* Orig para DAT_13a0_0000/0002 via Gfx_SetIconScale @ 11a8:002c:
     * Hof_CalcFinalScore @ 1040:1137 wraca do (1,1); BlitTechIcon @
     * 11a8:0f56 skaluje tylko gdy a != b (gfx_tech_needs_scale). */
    return (num == den) ? 1 : 0;
}

int gfx_icon_copy_is_fast(int transparent, int scaled, int pad)
{
    /* Orig Gfx_CopyIconBits @ 11a8:13c0: (param_3 == -1 && param_4 == 0 &&
     * local_12 == 0) -> word memcpy + odd tail. */
    return (transparent == -1 && scaled == 0 && pad == 0) ? 1 : 0;
}

int gfx_icon_row_pad(int w, int need_align)
{
    /* Orig same: param_5 == 1 ? ((w+1)/2*2-w) : 0 (even pitch). */
    if (w < 0)
        return -1;
    if (need_align == 0)
        return 0;
    return ((w + 1) / 2) * 2 - w;
}

civ_error_t gfx_alloc_icon_buffer(int w, int h, int *size_out)
{
    /* TODO: Orig Gfx_AllocIconBuffer @ 11a8:0054 device tail (GLOBALALLOC
     * + GLOBALLOCK w/h store + GLOBALUNLOCK; callers 1110:04d0/0ff6,
     * 11a8:0f56/05c7, 11e8:0337) + Gfx_FreeIconBuffer @ 11a8:00d1 GLOBALFREE
     * -> SDL heap bridge TBD. Size math lives in gfx_icon_buffer_size. */
    if (size_out)
        *size_out = 0;
    if (w <= 0 || h <= 0)
        return CIV_ERR_RANGE;
    if (!size_out)
        return CIV_ERR_NOENT;
    *size_out = gfx_icon_buffer_size(w, h);
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_copy_icon_bits(int transparent, int scaled)
{
    /* TODO: Orig Gfx_CopyIconBits @ 11a8:13c0 blit tail (transparent skip +
     * scaled resample via GetIconScale + Gfx_CopyIconBuffer @ 11a8:0328
     * HMEMCPY / Gfx_BlitIconScaled @ 11a8:0450 BITBLT / Gfx_BlitIconToPort
     * @ 11a8:0aa8 HMEMCPY rows) -> SDL blit TBD. Gates live in
     * gfx_icon_copy_is_fast/gfx_icon_row_pad/gfx_icon_scaled_dim. */
    (void)transparent;
    (void)scaled;
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (12): icon blit geometry @ 11a8 (goals 3/5).
 * OS-free 1:1 LIVE. No globals, no exit(), C17. See video.h chain block.
 * HMEMCPY rows map to memcpy (util_mem_copy_far, see civ_common.h);
 * GDI BITBLT/GLOBALLOCK tails stay STUB above. */

int gfx_blit_clip_len(int pos, int scaled, int port_len)
{
    /* Orig Gfx_BlitIconToPort @ 11a8:0aa8 tail pair:
     * if (*(port+0x0c) < pos_x + scaled_w) scaled_w = *(port+0x0c) - pos_x
     * (same for h vs +0x0e). Scaled dims come from (w*num)/den via
     * gfx_icon_scaled_dim (GetIconScale @ 11a8:0000). */
    int max;

    if (scaled <= 0 || port_len <= 0 || pos >= port_len)
        return 0;
    max = port_len - pos;
    return (scaled < max) ? scaled : max;
}

civ_error_t gfx_copy_span(int w1, int h1, int w2, int h2,
                          int *rows_out, int *cols_out)
{
    /* Orig Gfx_CopyIconBuffer @ 11a8:0328: same-size single HMEMCPY when
     * w1 == w2 && h1 == h2, else per-row HMEMCPY of min(w1,w2) over
     * min(h1,h2) rows (loop while row < min(h)). */
    if (!rows_out || !cols_out)
        return CIV_ERR_NOENT;
    if (w1 <= 0 || h1 <= 0 || w2 <= 0 || h2 <= 0)
        return CIV_ERR_RANGE;
    *rows_out = (h1 < h2) ? h1 : h2;
    *cols_out = (w1 < w2) ? w1 : w2;
    return CIV_OK;
}

int gfx_blit_scaled_is_direct(int num, int den)
{
    /* Orig Gfx_BlitIconScaled @ 11a8:0450 head: GetIconScale pair, direct
     * BITBLT 0xcc0020 when num == den (Hof restores (1,1)), else the port
     * + GETBITMAPBITS + nearest resample path. */
    return (num == den) ? 1 : 0;
}

int gfx_nearest_src(int dst, int num, int den)
{
    /* Orig Gfx_CopyIconBits @ 11a8:13c0 scaled loop: DDA err += num with
     * err0 = num>>1 half bias, emit while err >= den. Floor map matches
     * except documented half-tie shift; never interpolates (nearest). */
    if (num <= 0 || dst < 0)
        return -1;
    if (den <= 0)
        return dst;
    return (dst * den) / num;
}

/* --- Chain 2026-10-16 (15): icon mask pipeline geometry @ 11a8 (goal 4).
 * See video.h pipeline block. OS-free 1:1 on explicit buffers (no
 * GLOBALLOCK/HMEMCPY/BITBLT/CREATEBITMAP, no globals, no exit()). */

int gfx_icon_mask_row_bytes(int w)
{
    /* Orig Gfx_BuildIconMask @ 11a8:0d6e row: 16-px words x 2 B, BE word
     * (high then low), remainder padded with 1s. Row bytes = ceil(w/16)*2
     * (NOT the 8-bit ((w+1)/2)*2 pitch of gfx_remap_stride: the mask is
     * 1-bpp). -1 on w < 0 (orig faulted); 0 when w == 0. */
    if (w < 0)
        return -1;
    return ((w + 15) / 16) * 2;
}

int gfx_icon_transparent_match(int pixel, int transparent)
{
    /* Orig mask-bit rule in Gfx_BuildIconMask @ 11a8:0d6e + skip rule in
     * Gfx_CopyIconClipped @ 11a8:076f: bit/skip when pixel == transparent
     * key (default first pixel caller-side when the orig param == 0). */
    return (pixel == transparent) ? 1 : 0;
}

civ_error_t gfx_icon_content_bounds(const uint8_t *px, int w, int h,
                                    uint8_t bg, int *l_out, int *t_out,
                                    int *r_out, int *b_out)
{
    /* Orig Gfx_GetIconContentBounds @ 11a8:00fe bbox (see video.h): min/max
     * x/y over pixels != bg, then max+1 (exclusive); empty -> 0,0,0,0
     * (orig SETRECT). Caller passes bg explicitly (orig defaults to the
     * first pixel when param_3 == 0). */
    int minx = 0;
    int miny = 0;
    int maxx = 0;
    int maxy = 0;
    int x = 0;
    int y = 0;

    if (l_out)
        *l_out = 0;
    if (t_out)
        *t_out = 0;
    if (r_out)
        *r_out = 0;
    if (b_out)
        *b_out = 0;
    if (!px || !l_out || !t_out || !r_out || !b_out)
        return CIV_ERR_NOENT;
    if (w <= 0 || h <= 0)
        return CIV_ERR_RANGE;
    minx = w;
    miny = h;
    maxx = -1;
    maxy = -1;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            if (px[(size_t)y * (size_t)w + (size_t)x] != bg) {
                if (x < minx)
                    minx = x;
                if (x > maxx)
                    maxx = x;
                if (y < miny)
                    miny = y;
                if (y > maxy)
                    maxy = y;
            }
        }
    }
    if (maxx < minx || maxy < miny) {
        *l_out = 0;
        *t_out = 0;
        *r_out = 0;
        *b_out = 0;
        return CIV_OK;
    }
    *l_out = minx;
    *t_out = miny;
    *r_out = maxx + 1;
    *b_out = maxy + 1;
    return CIV_OK;
}

civ_error_t gfx_fill_icon_rect(uint8_t *px, int w, int h,
                               int l, int t, int r, int b, uint8_t color)
{
    /* Orig Gfx_FillIconRect @ 11a8:02ad: rows t..b-1 at cols l..r-1 set
     * to color (orig GLOBALLOCK bits + row base + per-byte fill). Strict
     * bounds here (orig assumed inside; larger would overrun the locked
     * bits, so RANGE instead of a fake clamp). */
    int y = 0;
    int x = 0;

    if (!px)
        return CIV_ERR_NOENT;
    if (w <= 0 || h <= 0)
        return CIV_ERR_RANGE;
    if (l < 0 || t < 0 || r > w || b > h || l >= r || t >= b)
        return CIV_ERR_RANGE;
    for (y = t; y < b; y++) {
        for (x = l; x < r; x++)
            px[(size_t)y * (size_t)w + (size_t)x] = color;
    }
    return CIV_OK;
}

civ_error_t gfx_icon_copy_clipped(const uint8_t *src, int sw, int sh,
                                  uint8_t *dst, int dw, int dh,
                                  int dx, int dy, uint8_t transparent)
{
    /* Orig Gfx_CopyIconClipped @ 11a8:076f transparent + offset clip
     * (see video.h): same-size + zero offset takes the fast w*h skip
     * loop; else clip cw = min(sw, dw-dx), ch = min(sh, dh-dy) and skip
     * transparent per pixel. dx/dy >= 0 (orig ports never pass negative;
     * negative -> RANGE, no clamp). Off-screen (cw/ch <= 0) -> CIV_OK
     * no-op (orig returns without touching bits, like video_clip_rect
     * visible = 0). */
    int cw = 0;
    int ch = 0;
    int y = 0;
    int x = 0;

    if (!src || !dst)
        return CIV_ERR_NOENT;
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
        return CIV_ERR_RANGE;
    if (dx < 0 || dy < 0)
        return CIV_ERR_RANGE;
    cw = sw;
    ch = sh;
    if (dh < dy + sh)
        ch = dh - dy;
    if (dw < dx + sw)
        cw = dw - dx;
    if (cw <= 0 || ch <= 0)
        return CIV_OK;
    for (y = 0; y < ch; y++) {
        for (x = 0; x < cw; x++) {
            uint8_t s = src[(size_t)y * (size_t)sw + (size_t)x];

            if (s == transparent)
                continue;
            dst[(size_t)(dy + y) * (size_t)dw + (size_t)(dx + x)] = s;
        }
    }
    return CIV_OK;
}

/* --- Chain 2026-10-16 (16): ROP selectors + registry @ 11a8/1008 (goals 3/4).
 * See video.h block. OS-free 1:1 LIVE, no Win16, no globals, no exit(). */

unsigned gfx_blit_rop_direct(void)
{
    /* Orig Gfx_BlitTechIcon @ 11a8:0f56 PUSH 0xcc0020 (SRCCOPY direct). */
    return GFX_ROP_SRCCOPY;
}

unsigned gfx_blit_rop_mask_step(int step)
{
    /* Orig same PUSH 0x660046 / 0x8800C6 / 0x660046 (SRCINVERT/SRCAND). */
    switch (step) {
    case 0:
        return GFX_ROP_SRCINVERT;
    case 1:
        return GFX_ROP_SRCAND;
    case 2:
        return GFX_ROP_SRCINVERT;
    default:
        return 0u;
    }
}

int gfx_mask_needs_bitmap(void)
{
    /* Orig trio needs 1-bpp CREATEBITMAP; SDL3 uses colorkey/alpha instead. */
    return 0;
}

int gfx_icon_registry_groups(void)
{
    /* Orig Gfx_FreeAllIconBuffers @ 1008:075b: 19 loop groups. */
    return 19;
}

int gfx_icon_registry_group_count(int group)
{
    /* Orig same loop counts in decompile order (see video.h for addresses).
     * 0:42 1:8 2:16 3:11 4:192 5:33 6:32 7:60 8:4 9:36 10:9 11:320 12:11
     * 13:4 14:28 15:12 16:1 17:1 18:1 (16-18 are the 3 singletons). */
    static const int counts[19] = {
        42, 8, 16, 11, 192, 33, 32, 60, 4, 36,
        9, 320, 11, 4, 28, 12, 1, 1, 1
    };

    if (group < 0 || group >= 19)
        return -1;
    return counts[group];
}

int gfx_icon_registry_total(void)
{
    /* Orig same: sum of the 19 groups = 821 slots. */
    return 821;
}

civ_error_t gfx_icon_registry_free(int *slots, int n)
{
    /* Orig Gfx_FreeAllIconBuffers @ 1008:075b slot-state half (GLOBALFREE
     * caller-side, like DELETEOBJECT in font_free_slots @ 1008:062f). */
    int i;

    if (!slots)
        return CIV_ERR_NOENT;
    if (n < 0 || n > 821)
        return CIV_ERR_RANGE;
    for (i = 0; i < n; i++)
        slots[i] = 0;
    return CIV_OK;
}

/* --- Chain 2026-10-16 (22): Gfx_StoreDC pattern (goal 4). See video.h.
 * LIVE order/gates + PARTIAL calls (no Win16, no globals, no exit()). */

civ_error_t gfx_store_dc(void)
{
    /* Orig Gfx_StoreDC @ 1098:1104 -> Gfx_SetCurrentDC @ 1100:09a9 (thin
     * wrapper saving the current port/DC; 28 callers incl. Anim frame-pump
     * @ 10b0:0461, Pic_PlayShared @ 1030:0000, MenuZ_Modal @ 1220:0156,
     * Dlg_CreateTracked @ 1098:124f, map opens @ 1208:0af1/1210:025b).
     * Ref unpacked/civ.exe read-only (NE Win3.x): anchor 1098:1104 +
     * sole callee 1100:09a9; no handle guessed, no fake. OS-free 1:1
     * pattern probe below (no Win16, no globals): save-first order +
     * tick/modal gates must hold; mismatch is a corrupt RE map
     * (BAD_DATA), never a stub pass. SDL_Texture bridge lives in the
     * gfx_port_* LIVE pair above (INDEX8 TARGET backing via
     * gfx_port_alloc_bitmap @ 10d0:0cfa + render-target select via
     * gfx_port_set_active @ 10d0:0956, restore via gfx_port_present_rect
     * @ 10d0:0854 + gfx_port_clear_active @ 10d0:09a2); the borrowed
     * renderer-as-DC save itself needs the caller port, so the device
     * tail stays UNSUPPORTED. */
    if (gfx_store_order_is_save_first() != 1)
        return CIV_ERR_BAD_DATA;
    if (gfx_store_tick_needs_save(0L) != 1 ||
        gfx_store_tick_needs_save(1L) != 0)
        return CIV_ERR_BAD_DATA;
    if (gfx_store_modal_needs_save(1) != 1 ||
        gfx_store_modal_needs_save(0) != 0)
        return CIV_ERR_BAD_DATA;
    /* TODO: Orig FUN_1098_1104 @ 1098:1104 -> Gfx_StoreDC () ->
     * Gfx_SetCurrentDC @ 1100:09a9 bridge. Borrowed renderer as DC +
     * TARGET backing via gfx_port_create @ 10d0:0000 / set_active @
     * 10d0:0956, restore via present_rect @ 10d0:0854 + clear_active @
     * 10d0:09a2 + SELECTOBJECT-restore + VALIDATERECT no-op on SDL3.
     * Returning the probe as CIV_OK now would fake the device half. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_dc_push(void)
{
    /* Orig push half of the save -> draw -> restore pattern (see video.h
     * chain block; same 1098:1104 save behind the Anim frame-pump tick
     * gate @ 10b0:0461 + modal entries @ 1030:0000/1220:0156/1098:124f).
     * Ref unpacked/civ.exe read-only (NE Win3.x): anchor 1098:1104 save
     * half. OS-free probe as in gfx_store_dc above (order + gates);
     * SDL renderer-target push needs the caller port (see
     * gfx_port_set_active @ 10d0:0956 LIVE), so the device tail stays
     * UNSUPPORTED with no fake. */
    if (gfx_store_order_is_save_first() != 1)
        return CIV_ERR_BAD_DATA;
    if (gfx_store_tick_needs_save(0L) != 1 ||
        gfx_store_tick_needs_save(7L) != 0)
        return CIV_ERR_BAD_DATA;
    if (gfx_store_modal_needs_save(1) != 1 ||
        gfx_store_modal_needs_save(0) != 0)
        return CIV_ERR_BAD_DATA;
    /* TODO: Orig push half @ 1098:1104 -> SDL renderer-target push
     * (SDL_SetRenderTarget onto the port INDEX8 TARGET backing from
     * gfx_port_alloc_bitmap @ 10d0:0cfa, borrowed ren via gfx_port_create
     * @ 10d0:0000). No caller port in this signature, so no target is
     * guessed here. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_dc_pop(void)
{
    /* Orig pop half (PresentRect @ 10d0:0854 + ClearActive @ 10d0:09a2 +
     * SELECTOBJECT-restore + VALIDATERECT no-op). Ref unpacked/civ.exe
     * read-only (NE Win3.x): restore anchors 10d0:0854/09a2 behind the
     * 1098:1104 save. OS-free probe (save-first order must hold; the
     * restore pair itself is LIVE in gfx_port_present_rect /
     * gfx_port_clear_active above). SDL restore needs the caller port,
     * so this tail stays UNSUPPORTED with no fake. */
    if (gfx_store_order_is_save_first() != 1)
        return CIV_ERR_BAD_DATA;
    if (gfx_store_tick_needs_save(0L) != 1 ||
        gfx_store_tick_needs_save(1L) != 0)
        return CIV_ERR_BAD_DATA;
    if (gfx_store_modal_needs_save(1) != 1 ||
        gfx_store_modal_needs_save(0) != 0)
        return CIV_ERR_BAD_DATA;
    /* TODO: Orig pop half @ 1098:1104 -> SDL restore (SDL_SetRenderTarget
     * restore + PresentRect @ 10d0:0854 + ClearActive @ 10d0:09a2 slots
     * caller-side + VALIDATERECT no-op on SDL3). */
    return CIV_ERR_UNSUPPORTED;
}

int gfx_store_tick_needs_save(long tick)
{
    /* Orig Anim_AdvanceFrames @ 10b0:0461 frame-pump head ((tick & 7) == 0
     * -> StoreDC + MemCopyFar); video-owned copy (same shape as
     * anim_tick_needs_store, no dialog pull). */
    return ((tick & 7L) == 0) ? 1 : 0;
}

int gfx_store_modal_needs_save(int is_modal)
{
    /* Orig modal-entry StoreDC (Pic_PlayShared @ 1030:0000, MenuZ_Modal @
     * 1220:0156, Dlg_CreateTracked @ 1098:124f, map opens @ 1208:0af1 /
     * 1210:025b): modal paint always saves the DC first. */
    return (is_modal != 0) ? 1 : 0;
}

int gfx_store_order_is_save_first(void)
{
    /* Orig save -> draw -> restore order (StoreDC, then Fill/Blit/Present/
     * Text via 10d0:0854/08f1 + 1098:00b5/0e0b/0ed1, then PresentRect +
     * ClearActive + SELECTOBJECT-restore + VALIDATERECT no-op). */
    return 1;
}

/* --- Chain 2026-10-16 (17): palette dispose pair (goal 2) ---
 * See video.h block. LIVE-noop (no Win16, no globals, no exit()). */

void palette_dispose(void)
{
    /* Orig Palette_Dispose @ 1090:0596: Debug_LogTrace("FreeGodPalette()
     * : godpal.c") + GLOBALFREE DAT_1420_9964 (LOGPALETTE 0x3b4, gdy != 0)
     * + DELETEOBJECT DAT_1420_9966 (HPALETTE). Pair do palette_init() @
     * 1090:0488 (LIVE-noop powyzej): SDL3 nie instaluje globalnej palety
     * (per-surface/texture w palette_apply_to_port), wiec dispose tez
     * no-op; selektory/handles nie maja odpowiednika (caller-side TODO).
     * Sole caller: WinMain_Shutdown @ 1008:0329 (faza "dispose-palette").
     * Jak Debug_LogTrace @ 1148:003c: release no-op (zob. sys.h). */
}

int palette_handle_is_live(int handle)
{
    /* Orig same guard: GLOBALFREE/DELETEOBJECT tylko gdy handle != 0
     * (zera skipowane CMP w disasm 1090:0596/0488). */
    return (handle != 0) ? 1 : 0;
}

/* --- Chain P4 render+text (2026-10-16): create/present + godpal.
 * See video.h P4 block. OS-free 1:1 LIVE, no Win16, no globals. */

int palette_animate_clamp(int first, int count)
{
    /* Orig Palette_AnimateRange @ 1090:0376 cap: if (0xec < first+count)
     * count = 0xec - first (0xec = godpal entries, see gfx_indexed.h
     * GFX_GODPAL_COUNT; LOGPALETTE tail past 0xec stays zero/black).
     * Callers: opcode 1 full apply (MainPort_ApplyPalette @ 1200:00a8)
     * + timed 1/0xe fades (Anim_AdvanceFrames @ 10b0:0461) + Push @
     * 1090:015e / Pop @ 1090:02d2 / Zero @ 1090:0670 / Set @ 1090:06fd. */
    if (first < 0 || count <= 0 || first >= CIV_PALETTE_COUNT)
        return 0;
    if (first + count > CIV_PALETTE_COUNT)
        count = CIV_PALETTE_COUNT - first;
    return count;
}

int palette_flag_for_mono(int mono)
{
    /* Orig same + Palette_Init @ 1090:0488 init + ZeroGodPalette @
     * 1090:0670 (always 0) / Set @ 1090:06fd (1 when color else 0):
     * DAT_1420_17a6 == 0 (color/VGA) ? 1 : 0 is the ANIMATEPALETTE flag
     * byte per entry; mono (EGA) clears it. */
    return (mono == 0) ? 1 : 0;
}
