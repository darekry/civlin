/* Video: SDL3 backend. Orig GDI paths in video.h; Win16 handles have no
 * Linux equivalent, so the port owns the SDL window/renderer created by
 * the caller (main.c) and validates them here (no globals, no exit). */
#include "video.h"

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
    (void)first;
    (void)count;
    /* TODO: Orig FUN_1090_0376 @ 1090:0376 -> Palette_AnimateRange (RGB
     * triples + flag byte, then ANIMATEPALETTE; callers: opcode 1 + 0xe
     * from MainPort_ApplyPalette @ 1200:00a8 and Anim_AdvanceFrames @
     * 10b0:0461; SDL_Palette equivalent on the active 8-bit surface). */
    return CIV_ERR_UNSUPPORTED;
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
    if (out)
        *out = NULL;
    (void)ren;
    (void)w;
    (void)h;
    if (!out)
        return CIV_ERR_NOENT;
    if (w <= 0 || h <= 0)
        return CIV_ERR_RANGE;
    /* TODO: Orig FUN_10d0_0cfa @ 10d0:0cfa -> GfxPort_AllocBitmap (bitmap
     * helper behind GfxPort_Create @ 10d0:0000 / GR_PortOpen). GETDC +
     * CREATECOMPATIBLEDC + bitmap select into the new port (~0x20-word
     * struct: DC + compatible DC + bitmap + clip rect + palette refs).
     * SDL3 TBD: SDL_CreateTexture streaming/target + select as the port
     * backing store (8-bit INDEX8 keeps palette animation possible). */
    return CIV_ERR_UNSUPPORTED;
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
    (void)port;
    (void)ren;
    (void)rect;
    if (!port || !ren)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_10d0_0854 @ 10d0:0854 -> GfxPort_PresentRect. BITBLT
     * rop 0xcc0020 (SRCCOPY) + VALIDATERECT; SDL_RenderTexture rect TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t gfx_port_blit(gfx_port_t *dst, const gfx_port_t *src,
                          const SDL_Rect *src_rect, int dst_x, int dst_y)
{
    (void)dst;
    (void)src;
    (void)src_rect;
    (void)dst_x;
    (void)dst_y;
    if (!dst || !src)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_10d0_08f1 @ 10d0:08f1 -> GfxPort_Blit. Port-to-port
     * BITBLT (0xcc0020 rop family); SDL blit TBD. */
    return CIV_ERR_UNSUPPORTED;
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
     * GR_PortChangeColor. GETBITMAPBITS scan + SETBITMAPBITS write-back
     * remapping old_index -> new_index inside the port bitmap; follows
     * the Create @ 10d0:0000 / SetActive @ 10d0:0956 / PresentRect @
     * 10d0:0854 / Blit @ 10d0:08f1 / AllocBitmap @ 10d0:0cfa / FillOrCopy
     * @ 10c0:xxxx / LoadRscFrame @ 1198:xxxx chain. SDL3 TBD: INDEX8
     * pixel remap or palette alias on the port backing store (godpal.c
     * 0x00..0xeb range, see gfx_indexed.h). */
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
    (void)first;
    (void)count;
    if (!port)
        return CIV_ERR_NOENT;
    if (first < 0 || count <= 0 || first + count > 256)
        return CIV_ERR_RANGE;
    /* TODO: Orig FUN_1200_00a8 @ 1200:00a8 MainPort_ApplyPalette tail +
     * FUN_1090_0376 @ 1090:0376 Palette_AnimateRange -> Palette_ApplyToPort
     * (video-side helper; map-side mainport_apply_palette stays the
     * caller). RGB triples + flag byte copy then ANIMATEPALETTE (opcodes
     * 1/0xe via MainPort_ApplyPalette and Anim_AdvanceFrames @ 10b0:0461);
     * SDL_SetPaletteColors on the port backing store TBD (godpal.c
     * 0xec entries via Palette_Init @ 1090:0488 -> DAT_1420_9966, see
     * gfx_indexed.h). */
    return CIV_ERR_UNSUPPORTED;
}

void video_shutdown_display(void)
{
    /* Orig: FUN_1008_0329 @ 1008:0329 WinMain_Shutdown video-only tail.
     * DESTROYWINDOW for the display ports/windows plus
     * REMOVEFONTRESOURCE(CIV_FONT_FILE "civfonts.fon" @ 1420:00a4/00fe,
     * added by Font_EnumFamiliesInit @ 1008:0e99); KILLTIMER/
     * FREEPROCINSTANCE tails live in map/dialog shutdown (see boot.c
     * boot_shutdown). SDL3 window/renderer stay caller-owned, nothing to
     * free yet; no globals, no exit(). */
}
