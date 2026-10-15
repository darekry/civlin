/* Video stubs. */
#include "video.h"

civ_error_t video_init(SDL_Window *win, SDL_Renderer *ren)
{
    (void)win;
    (void)ren;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t video_blit_indexed(SDL_Renderer *ren, SDL_Texture *tex, int x, int y)
{
    (void)ren;
    (void)tex;
    (void)x;
    (void)y;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t video_present(SDL_Renderer *ren)
{
    (void)ren;
    return CIV_ERR_UNSUPPORTED;
}

void video_shutdown(void)
{
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
    if (!ren)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_1008_059a @ 1008:059a -> Video_CheckDriverCaps.
     * GETDEVICECAPS checklist @ 1420:0af4 (palette/raster caps) maps to
     * SDL3 renderer caps (output size + pixel format + target-texture
     * support checks TBD). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t palette_init(void)
{
    /* TODO: Orig FUN_1090_0488 @ 1090:0488 -> Palette_Init. LOGPALETTE
     * 0x300 + 0xec RGB entries from godpal.c -> CREATEPALETTE, handle to
     * DAT_1420_9966 (SELECTPALETTE/REALIZEPALETTE at each port present).
     * SDL3: SDL_SetPaletteColors on the 8-bit surfaces TBD. */
    return CIV_ERR_UNSUPPORTED;
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
