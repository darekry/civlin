/* Video: GDI (BitBlt/SelectObject/CreateBitmap/CreatePalette,
 * SelectPalette/RealizePalette/AnimatePalette, GetDeviceCaps) -> SDL3.
 * Palette fidelity via gfx_indexed (8-bit surface + SDL_Palette).
 *
 * Ghidra anchors (civ.exe), renamed 2026-10-15/16 (packet F):
 *   Video_CheckDriverCaps  @ 1008:059a  (GETDEVICECAPS checklist @ 1420:0af4;
 *                            SDL3 renderer-caps mapping TBD)
 *   Palette_Init           @ 1090:0488  (LOGPALETTE 0x300, 0xec entries from
 *                            godpal.c -> CREATEPALETTE -> DAT_1420_9966)
 *   Palette_AnimateRange   @ 1090:0376  (copies RGB triples + flag byte, then
 *                            ANIMATEPALETTE; callers: opcode 1 + 0xe from
 *                            MainPort_ApplyPalette @ 1200:00a8 and
 *                            Anim_AdvanceFrames @ 10b0:0461; title fades,
 *                            HoF screens)
 *   GfxPort_Create         @ 10d0:0000  (GR_PortOpen : gr_port.c; GETDC +
 *                            CREATECOMPATIBLEDC + bitmap via 10d0:0cfa;
 *                            port struct ~0x20 words, SDL_Texture TBD)
 *   GfxPort_PresentRect    @ 10d0:0854  (BITBLT rop 0xcc0020 SRCCOPY +
 *                            VALIDATERECT; SDL_RenderTexture rect TBD)
 *   GfxPort_Blit           @ 10d0:08f1  (port-to-port BITBLT; SDL blit TBD)
 *   GfxPort_SetActive      @ 10d0:0956  (selects the drawing port/DC;
 *                            SDL render-target select TBD)
 */
#ifndef CIVLIN_VIDEO_H
#define CIVLIN_VIDEO_H

#include <SDL3/SDL.h>

#include "civ_common.h"
#include "rsc_loader.h"

civ_error_t video_init(SDL_Window *win, SDL_Renderer *ren);
civ_error_t video_blit_indexed(SDL_Renderer *ren, SDL_Texture *tex, int x, int y);
civ_error_t video_present(SDL_Renderer *ren);
void video_shutdown(void);

/* Video_CheckDriverCaps @ 1008:059a (stub; GETDEVICECAPS @ 1420:0af4
 * checklist maps to SDL3 renderer caps: output size + pixel format +
 * target-texture support checks TBD). NULL renderer reports NOENT. */
civ_error_t video_check_driver_caps(SDL_Renderer *ren);

/* Palette_Init @ 1090:0488 (stub; LOGPALETTE 0x300 + 0xec RGB entries from
 * godpal.c -> CREATEPALETTE, handle to DAT_1420_9966; SDL_SetPaletteColors
 * on the 8-bit surfaces TBD). */
civ_error_t palette_init(void);

/* Palette_AnimateRange @ 1090:0376 (ported; range repaint via
 * SDL_SetPaletteColors staging on a temp 256-entry palette, no globals.
 * Orig copies RGB triples + flag byte then ANIMATEPALETTE; callers issue
 * opcode 1 (full apply from MainPort_ApplyPalette @ 1200:00a8) and timed
 * 1/0xe fades from Anim_AdvanceFrames @ 10b0:0461 (title/HoF fades).
 * Colors feed from gfx_godpal_get (0x00..0xeb) + black tail past 0xec
 * (LOGPALETTE 0x300 zero tail); propagates UNSUPPORTED until the godpal
 * dump lands (no fake bytes). Range outside [0,256) reports RANGE. */
civ_error_t palette_animate_range(int first, int count);

/* GfxPort_* @ 10d0:0000/0956/0854/08f1 + bitmap @ 10d0:0cfa.
 * Drawing port: the original port struct holds ~0x20 words (DC +
 * compatible DC + bitmap via 10d0:0cfa + clip rect + palette refs); field
 * map not yet decoded, so the Linux shape stays minimal but concrete
 * (see video.c): tex/w/h backing store + DC placeholder (borrowed
 * SDL_Renderer from GETDC) + clip rect placeholder (SelectClipRgn) +
 * palette ref placeholder (CREATEPALETTE). SDL_Texture TARGET +
 * SDL_SetRenderTarget round-trip bridge for Blit/PresentRect. */
typedef struct gfx_port gfx_port_t;

/* Orig: FUN_10d0_0cfa @ 10d0:0cfa -> gfx_port_alloc_bitmap
 * (GfxPort_AllocBitmap; bitmap helper behind GfxPort_Create @ 10d0:0000 /
 * GR_PortOpen). Orig path: GETDC + CREATECOMPATIBLEDC + bitmap select
 * into the fresh port (dimensions from the port open request). SDL3 port:
 * SDL_CreateTexture INDEX8 TARGET selected as the port backing store
 * (INDEX8 keeps ANIMATEPALETTE possible via SDL_SetPaletteColors;
 * TARGET allows the Blit/PresentRect render-target path). NULL ren/out
 * reports NOENT; w/h <= 0 reports RANGE; SDL failure reports IO. */
civ_error_t gfx_port_alloc_bitmap(SDL_Renderer *ren, int w, int h,
                                  SDL_Texture **out);

/* Orig: FUN_10d0_0000 @ 10d0:0000 -> gfx_port_create (GfxPort_Create /
 * GR_PortOpen). GETDC + CREATECOMPATIBLEDC + bitmap via 10d0:0cfa. NULL
 * out reports NOENT; otherwise UNSUPPORTED. */
civ_error_t gfx_port_create(SDL_Renderer *ren, int w, int h, gfx_port_t **out);

/* Orig: FUN_10d0_0956 @ 10d0:0956 -> gfx_port_set_active (GfxPort_SetActive).
 * Selects the drawing port/DC for subsequent GDI calls; SDL3 equivalent is
 * an SDL render-target select. NULL port reports NOENT. */
civ_error_t gfx_port_set_active(gfx_port_t *port);

/* Orig: FUN_10d0_0854 @ 10d0:0854 -> gfx_port_present_rect
 * (GfxPort_PresentRect). Ported: BITBLT rop 0xcc0020 (SRCCOPY) to the
 * screen maps to SDL_RenderTexture (whole texture when rect is NULL,
 * else rect as dst with NULL src); VALIDATERECT is a no-op on SDL3 (no
 * dirty-region validation needed, no HWND to validate). Error mapping
 * (consistent across video.c): NULL port/renderer -> NOENT (NULL rect =
 * full port, kept); missing backing texture / SDL_GetTextureSize fail ->
 * BAD_DATA; SDL_RenderTexture fail -> IO (SDL_GetError logged by
 * caller; NOMEM only from allocation paths: SDL_CreatePalette/Texture,
 * never from present). */
civ_error_t gfx_port_present_rect(gfx_port_t *port, SDL_Renderer *ren,
                                  const SDL_Rect *rect);

/* Orig: FUN_10d0_08f1 @ 10d0:08f1 -> gfx_port_blit (GfxPort_Blit).
 * Ported via SDL_SetRenderTarget round-trip on the minimal gfx_port_t
 * (see video.c: tex/w/h + borrowed ren as the DC placeholder + clip +
 * palette placeholders covering the ~0x20-word Orig struct): save
 * SDL_GetRenderTarget, SDL_SetRenderTarget(dst tex),
 * SDL_RenderTexture(src tex, src_rect -> dst_x/dst_y rect), restore
 * target. Same 0xcc0020 SRCCOPY rop family as PresentRect @ 10d0:0854;
 * VALIDATERECT is a no-op on SDL3. Exact Orig field map still TBD
 * (src/dst HDC + HBITMAP via 10d0:0cfa + clip-region words), so ports
 * without a borrowed ren (both ren NULL: GfxPort_Create @ 10d0:0000 /
 * CreateCompatible still stubbed) report UNSUPPORTED. Error mapping:
 * NULL ports -> NOENT (NULL src_rect = whole source, kept); missing
 * backing tex -> BAD_DATA; src_rect w/h <= 0 -> RANGE; SDL target-set /
 * RenderTexture / target-restore fail -> IO. */
civ_error_t gfx_port_blit(gfx_port_t *dst, const gfx_port_t *src,
                          const SDL_Rect *src_rect, int dst_x, int dst_y);

/* Orig: FUN_1098_114A @ 1098:114a -> mainport_get_or_create
 * (MainPort_GetOrCreate). Returns the main port/DC: GETDC +
 * SELECTPALETTE/REALIZEPALETTE + GfxPort_SetActive @ 10d0:0956 bridge.
 * Called before WdwMap_Open @ 1208:0af1 / WdwSmMap_Open @ 1210:025b /
 * WdwStatus_Open @ 1218:0386 in boot_open_windows (see boot.h). SDL3
 * render-target select TBD; stub returns CIV_ERR_UNSUPPORTED. NULL
 * renderer/out reports NOENT. */
civ_error_t mainport_get_or_create(SDL_Renderer *ren, gfx_port_t **out);

/* Orig: FUN_10c0_xxxx @ 10c0:xxxx -> gfx_port_fill_or_copy
 * (GfxPort_FillOrCopy). Sits between HMEMCPY @ 10b8:079d and BITBLT @
 * 10d0:0854/08f1; blit helper behind Anim_AdvanceFrames @ 10b0:0461.
 * Orig path: NULL src -> FILLRECT (solid fill of rect at dst_x/dst_y),
 * else BITBLT rop 0xcc0020 (SRCCOPY) from src. SDL3 TBD:
 * SDL_RenderFillRect vs SDL_RenderTexture. NULL dst reports NOENT
 * (NULL src = fill path, kept; NULL rect = full/origin, kept);
 * otherwise UNSUPPORTED. */
civ_error_t gfx_port_fill_or_copy(gfx_port_t *dst, const gfx_port_t *src,
                                  const SDL_Rect *rect, int dst_x, int dst_y);

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> gfx_port_load_rsc_frame
 * (GfxPort_LoadRscFrame; display side of Rsc_DecodeBitmap, see
 * rsc_loader.h). Orig path: GfxPort_Create @ 10d0:0000 via
 * GfxPort_AllocBitmap @ 10d0:0cfa + Palette_Init @ 1090:0488 /
 * Palette_AnimateRange @ 1090:0376 (CREATEPALETTE/SELECTPALETTE) +
 * GfxPort_Blit @ 10d0:08f1 / GfxPort_PresentRect @ 10d0:0854 (BITBLT).
 * SDL3 TBD: INDEX8 texture + SDL_SetPaletteColors + render present.
 * NULL ren/frame/out reports NOENT; otherwise UNSUPPORTED. */
civ_error_t gfx_port_load_rsc_frame(SDL_Renderer *ren,
                                    const rsc_indexed_t *frame,
                                    gfx_port_t **out);

/* Orig: FUN_11d0_xxxx @ 11d0:xxxx -> util_draw_progress_bar
 * (Util_DrawProgressBar; progress-UI). Pairs with the Util_Progress calc
 * @ 1000:29ea (fraction over the 0x1248 range; calc stays in sys, do not
 * edit sys.h): this draws the bar (orig: row loop with Sys_Yield per row,
 * rect fill via the GfxPort_Blit @ 10d0:08f1 path). SDL3 TBD:
 * SDL_RenderFillRect for the filled/unfilled halves. NULL port/rect
 * reports NOENT; max <= 0 or value outside [0, max] reports RANGE;
 * otherwise UNSUPPORTED. */
civ_error_t util_draw_progress_bar(gfx_port_t *port, const SDL_Rect *rect,
                                   int value, int max);

/* Orig: FUN_10d0_09ee @ 10d0:09ee -> gfx_port_change_color
 * (GfxPort_ChangeColor / GR_PortChangeColor; GET/SETBITMAPBITS color
 * remap tail). Follows the Create @ 10d0:0000 / SetActive @ 10d0:0956 /
 * PresentRect @ 10d0:0854 / Blit @ 10d0:08f1 / AllocBitmap @ 10d0:0cfa /
 * FillOrCopy @ 10c0:xxxx / LoadRscFrame @ 1198:xxxx chain: remaps one
 * palette index to another inside the port bitmap (orig: GETBITMAPBITS
 * scan of the DIB bits + SETBITMAPBITS write-back of replaced pixels).
 * SDL3 plan: (a) true pixel remap needs CPU bits via
 * SDL_LockTexture/UnlockTexture on the port INDEX8 TARGET backing
 * store, then index swap + Unlock; (b) cheap alias is a single-entry
 * SDL_SetPaletteColors copy (old color := new color) on the port
 * palette (see palette_apply_to_port). Bitmap bits unavailable while
 * the backing store stays a GPU TARGET texture with no locked copy, so
 * this stays CIV_ERR_UNSUPPORTED (do not fake via palette alias: that
 * would merge two indices visibly). NULL port -> NOENT; indices outside
 * 0..255 -> RANGE (godpal.c uses 0x00..0xeb, see gfx_indexed.h). */
civ_error_t gfx_port_change_color(gfx_port_t *port, int old_index,
                                  int new_index);

/* Orig: GETDC step behind FUN_1098_114a @ 1098:114a (MainPort_GetOrCreate)
 * + FUN_10d0_0000 @ 10d0:0000 (GfxPort_Create / GR_PortOpen) ->
 * gfx_port_get_dc (GfxPort_GetDC). Borrows the display DC/renderer for
 * the port (orig: GETDC on the main window; SELECTPALETTE/REALIZEPALETTE
 * applied by the MainPort_GetOrCreate @ 1098:114a caller). SDL3: the
 * renderer stands in for the HDC (no separate DC object; borrow only,
 * no ownership transfer). NULL port/out reports NOENT; otherwise
 * UNSUPPORTED. */
civ_error_t gfx_port_get_dc(gfx_port_t *port, SDL_Renderer **out_dc);

/* Orig: RELEASEDC pair to the GETDC above (same FUN_1098_114a @
 * 1098:114a + FUN_10d0_0000 @ 10d0:0000 sites) -> gfx_port_release_dc
 * (GfxPort_ReleaseDC). Releases a DC borrowed via gfx_port_get_dc
 * (orig: RELEASEDC). SDL3: renderer borrow needs no release; stub keeps
 * the pairing for RE-map fidelity. NULL port/dc reports NOENT;
 * otherwise UNSUPPORTED. */
civ_error_t gfx_port_release_dc(gfx_port_t *port, SDL_Renderer *dc);

/* Orig: CREATECOMPATIBLEDC + FUN_10d0_0cfa @ 10d0:0cfa bitmap select ->
 * gfx_port_create_compatible (GfxPort_CreateCompatible). Compatible-port
 * sibling of GfxPort_Create @ 10d0:0000: compatible DC off the display
 * DC plus bitmap via the 10d0:0cfa allocator (same ~0x20-word port
 * struct); complements gfx_port_alloc_bitmap (which returns the raw
 * SDL_Texture backing store, this returns the owning port). SDL3 TBD:
 * target-capable texture selected into the new port. NULL out reports
 * NOENT; w/h <= 0 reports RANGE; otherwise UNSUPPORTED. */
civ_error_t gfx_port_create_compatible(SDL_Renderer *ren, int w, int h,
                                       gfx_port_t **out);

/* Orig: FUN_1200_00a8 @ 1200:00a8 MainPort_ApplyPalette tail +
 * FUN_1090_0376 @ 1090:0376 Palette_AnimateRange -> palette_apply_to_port
 * (Palette_ApplyToPort; video-side helper, map-side
 * mainport_apply_palette in map.h stays the caller). Ported: applies
 * [first, first+count) via SDL_SetPaletteColors on the port INDEX8
 * backing store (SDL_GetTexturePalette when present, else a temp
 * 256-entry palette attached via SDL_SetTexturePalette; orig: RGB
 * triples + flag byte copy then ANIMATEPALETTE, opcode 1 full apply +
 * 0xe fade from MainPort_ApplyPalette, timed 1/0xe from
 * Anim_AdvanceFrames @ 10b0:0461). Colors feed from gfx_godpal_get
 * (godpal.c 0x00..0xeb) + black tail past 0xec; propagates UNSUPPORTED
 * until the dump lands (no fake bytes). NULL port reports NOENT; first
 * < 0 or count <= 0 or first+count > 256 reports RANGE; missing backing
 * texture reports BAD_DATA; SDL failure reports IO/NOMEM. */
civ_error_t palette_apply_to_port(gfx_port_t *port, int first, int count);

/* Orig: FUN_1008_0329 @ 1008:0329 WinMain_Shutdown video-only tail ->
 * video_shutdown_display. DESTROYWINDOW for the display ports/windows
 * plus REMOVEFONTRESOURCE(CIV_FONT_FILE "civfonts.fon" @ 1420:00a4/00fe,
 * added by Font_EnumFamiliesInit @ 1008:0e99; NE DLL/font container
 * 244224 B, FON header + font-directory table offsets TBD, SDL_ttf
 * parse plan documented at font_enum_families_init in boot.h/boot.c);
 * KILLTIMER/FREEPROCINSTANCE tails live in map/dialog shutdown (see
 * map.h/boot.c). SDL3 window/renderer stay caller-owned (see
 * boot_shutdown), so this installs/removes nothing yet. Void stub: no
 * globals, no exit(). */
void video_shutdown_display(void);

#endif /* CIVLIN_VIDEO_H */
