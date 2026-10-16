/* Video: GDI (BitBlt/SelectObject/CreateBitmap/CreatePalette,
 * SelectPalette/RealizePalette/AnimatePalette, GetDeviceCaps) -> SDL3.
 * Palette fidelity via gfx_indexed (8-bit surface + SDL_Palette).
 *
 * Y2 HoF/show present path (ported, referenced by halloffame.h Y2 flow):
 *   HoF_Show @ 1080:0119 (dialog 0x94) + HoF_VictoryCeremony @ 1080:06ff
 *   (pyramid frames) present via gfx_port_present_rect @ 10d0:0854
 *   (BITBLT rop 0xcc0020 SRCCOPY -> SDL_RenderTexture; VALIDATERECT is a
 *   no-op on SDL3) after palette_apply_to_port @ 1200:00a8/1090:0376
 *   range guard ([first, first+count) inside [0,256), INDEX8 backing
 *   store, SDL_SetPaletteColors). Both already ported; HoF stubs stay
 *   UNSUPPORTED until the dialog/frame bridges land (no Win16 here).
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
/* Orig present tail @ 10d0:0854 (BITBLT rop 0xcc0020 SRCCOPY +
 * VALIDATERECT no-op on SDL3). Z2 dump synthetic ok (no assets, see
 * rsc_dump.c + gfx_indexed.h): 64x64 palette gradient via
 * gfx_indexed_to_texture + SDL_RenderTexture + SDL_RenderPresent passes
 * (1 frame, incl. SDL_VIDEODRIVER=dummy = no crash); 4x2 surface path
 * via gfx_indexed_to_surface verified by format/dims/pixel/palette
 * read-back. AA2: same synthetic surface/texture ok feeds the probe gate
 * (gfx_probe_rsc_image @ 1198:xxxx strict raw INDEX8 gate + gfx_decode_rle
 * CvPc RLE path, see gfx_indexed.h); real RSC frames stay gated there,
 * this present shares the SDL_RenderTexture core with
 * gfx_port_present_rect @ 10d0:0854 (no impl change here). AD2: frame
 * bytes reach this present only after the gfx_rle_expand_best dispatcher
 * (RLL/INV/LZSS/RAW0/2/4/8 + per-method fail log via
 * gfx_rle_expand_best_ex + gfx_try_fail_str/gfx_expand_method_str, see
 * gfx_indexed.h); VALIDATERECT stays a no-op, synthetic ok unchanged.
 * NULL ren -> NOENT; SDL_RenderPresent fail -> IO. */
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

/* --- Gfx brush/color chain (segment 1098, 2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Gfx_MakeBrush @ 1098:00b5 (color < 10 or > 0xf5 or DAT_1420_17a6 == 1
 *     -> Gfx_LookupColor + CREATESOLIDBRUSH; else 8x8 pattern bitmap via
 *     GfxPort_AllocBitmap @ 10d0:0cfa filled with the color +
 *     CREATEPATTERNBRUSH + DELETEOBJECT; 20 callers incl. the PIC player
 *     @ 1030:0000, Text_DrawAligned paths, Anim_AdvanceFrames @ 10b0:0461).
 *   Gfx_LookupColor @ 1098:01b0 (color < 10 -> static table @ +0x66;
 *     10 < color < 0xf6 -> 0xec palette via GLOBALLOCK(DAT_1420_9964) + 4
 *     at entry (param-10)*4+4, GLOBALUNLOCK after; else (>= 0xf6) static
 *     high table @ -0x34a; 8 callers incl. Gfx_MakeBrush, Text_SetColor @
 *     1098:0e0b, Text_SetBkColor @ 1098:2144).
 *   Text_SetColor @ 1098:0e0b (Gfx_GetCurrentDC + Gfx_LookupColor +
 *     SETTEXTCOLOR + CREATEPEN(1) + Gfx_MakeBrush; DAT_1420_5480/547e/547c
 *     caches; 50+ callers: Game_UpdateStatusPanel @ 1088:1554, PIC player,
 *     dialogs, NewGame_Wizard @ 10e8:0667/0771/0b0b/0bf6, MenuZ @ 1220).
 *   Text_SetBkColor @ 1098:2144 (DAT_12d8_28fc gate + Gfx_LookupColor +
 *     SETBKCOLOR; mono vs color branch).
 * Relationship to gfx_indexed.c: the 0xec palette IS the godpal run
 * (gfx_godpal_count/get/search — LOGPALETTE 0x300 zero tail + 0xec RGB
 * entries feeding Palette_Init @ 1090:0488 -> CREATEPALETTE; Linux
 * applies via SDL_SetPaletteColors in palette_apply_to_port, see above;
 * static low/high tables have no dump yet and stay STUB). GDI handles
 * (HBRUSH/HPEN/CREATEPATTERNBRUSH) have no Linux equivalent -> the full
 * MakeBrush/LookupColor/SetColor tails stay STUB with TODO Orig; the
 * OS-free branch predicates below are LIVE 1:1 (no Win16, no globals). */

/* Orig: Gfx_MakeBrush @ 1098:00b5 branch — solid vs pattern brush.
 * 1 when the color takes the CREATESOLIDBRUSH path (color < 10 or
 * color > 0xf5 or mono != 0, i.e. DAT_1420_17a6 == 1), 0 for the 8x8
 * pattern path. OS-free 1:1, no Win16, no globals, no exit(). */
int gfx_brush_is_solid(int color, int mono);

/* Orig: Gfx_LookupColor @ 1098:01b0 table select — which table serves
 * the color. 0 = static low table (< 10, @ +0x66), 1 = 0xec palette
 * (10 < color < 0xf6 via DAT_1420_9964), 2 = static high table
 * (>= 0xf6 @ -0x34a), -1 = boundary (10 / 0xf6 fall through in orig).
 * OS-free 1:1, no Win16, no globals, no exit(). */
int gfx_color_table_kind(int color);

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
 * GR_PortOpen). LIVE SDL_Texture bridge (see video.c): malloc gfx_port_t +
 * gfx_port_alloc_bitmap @ 10d0:0cfa INDEX8 TARGET backing store (even
 * pitch ((w+1)/2*2) via gfx_port_bitmap_pitch; INDEX8 keeps
 * ANIMATEPALETTE possible via SDL_SetPaletteColors, TARGET allows the
 * Blit @ 10d0:08f1 / PresentRect @ 10d0:0854 render-target path) + borrowed
 * ren/w/h wiring (GETDC/CREATECOMPATIBLEDC borrow the caller renderer,
 * never owned/freed here). Backs the Anim_OpenSequence @ 10b0:008a GfxPort
 * x2 pair (ctx offs +9/+0x62) + the WdwMap_Open @ 1208:0af1 / WdwSmMap_Open @ 1210:025b /
 * WdwStatus_Open @ 1218:0386 views in boot_open_windows (ports are
 * textures, window/renderer stay caller-owned). NULL out/ren reports
 * NOENT (*out NULL when present); w/h <= 0 reports RANGE; malloc fail
 * reports NOMEM; bitmap fail reports IO (*out NULL, no leak). */
civ_error_t gfx_port_create(SDL_Renderer *ren, int w, int h, gfx_port_t **out);

/* Orig: FUN_10d0_0956 @ 10d0:0956 -> gfx_port_set_active (GfxPort_SetActive).
 * LIVE SDL render-target bridge (see video.c): Gfx_SetCurrentDC(port+0x16)
 * + 17cc = port always + 17ae = port unless the 12d8:44ff+0x43 dialog port
 * (slots caller-side via gfx_port_clear_active, LIVE) maps to
 * SDL_SetRenderTarget(port ren, port tex) onto the INDEX8 TARGET backing
 * store from gfx_port_alloc_bitmap @ 10d0:0cfa (same 0xcc0020 SRCCOPY family
 * as PresentRect @ 10d0:0854 / Blit @ 10d0:08f1). gfx_port_create @
 * 10d0:0000 wires ren/tex, so NULL ren/tex is a corrupt port (BAD_DATA,
 * consistent with present_rect), not a stub. Backs the Anim_OpenSequence @
 * 10b0:008a GfxPort x2 pair + WdwMap_Open @ 1208:0af1 / WdwSmMap_Open @
 * 1210:025b / WdwStatus_Open @ 1218:0386 views in boot_open_windows.
 * NULL port -> NOENT; SDL target-set fail -> IO. Ref unpacked/civ.exe
 * read-only (anchor 10d0:0956). */
civ_error_t gfx_port_set_active(gfx_port_t *port);

/* Orig: FUN_10d0_0854 @ 10d0:0854 -> gfx_port_present_rect
 * (GfxPort_PresentRect). Ported: BITBLT rop 0xcc0020 (SRCCOPY) to the
 * screen maps to SDL_RenderTexture (whole texture when rect is NULL,
 * else rect as dst with NULL src); VALIDATERECT is a no-op on SDL3 (no
 * dirty-region validation needed, no HWND to validate). Y2 HoF/show path:
 * HoF_Show @ 1080:0119 (dialog 0x94) + HoF_VictoryCeremony @ 1080:06ff
 * (pyramid frames) present through here after palette_apply_to_port
 * (see halloffame.h Y2 flow). Z2 dump synthetic ok (doc only, no impl
 * change; Blit @ 10d0:08f1 / AllocBitmap @ 10d0:0cfa impls untouched):
 * rsc_dump present_frame (64x64 palette gradient via
 * gfx_indexed_to_texture + SDL_RenderTexture + video_present) passes on
 * dummy video; this port path shares the same SDL_RenderTexture present
 * core. AA2: synthetic surface (4x2 via gfx_indexed_to_surface) + texture
 * (64x64 via gfx_indexed_to_texture) ok links to the probe gate
 * (gfx_probe_rsc_image @ 1198:xxxx raw gate + gfx_decode_rle CvPc path,
 * see gfx_indexed.h); real frames stay gated there. AD2 (doc only, no
 * impl change): frame bytes reach this present only after the
 * gfx_rle_expand_best dispatcher (RLL/INV/LZSS/RAW0/2/4/8 best-method +
 * per-method fail log via gfx_rle_expand_best_ex +
 * gfx_try_fail_str/gfx_expand_method_str, see gfx_indexed.h);
 * VALIDATERECT stays a no-op, synthetic ok unchanged. Error mapping (consistent across video.c):
 * NULL port/renderer -> NOENT (NULL rect = full port, kept); missing
 * backing texture / SDL_GetTextureSize fail -> BAD_DATA;
 * SDL_RenderTexture fail -> IO (SDL_GetError logged by caller; NOMEM only
 * from allocation paths: SDL_CreatePalette/Texture, never from present). */
civ_error_t gfx_port_present_rect(gfx_port_t *port, SDL_Renderer *ren,
                                  const SDL_Rect *rect);

/* Orig: FUN_10d0_08f1 @ 10d0:08f1 -> gfx_port_blit (GfxPort_Blit).
 * LIVE via SDL_SetRenderTarget round-trip on the minimal gfx_port_t
 * (see video.c: tex/w/h + borrowed ren as the DC placeholder + clip +
 * palette placeholders covering the ~0x20-word Orig struct): save
 * SDL_GetRenderTarget, SDL_SetRenderTarget(dst tex),
 * SDL_RenderTexture(src tex, src_rect -> dst_x/dst_y rect), restore
 * target. Same 0xcc0020 SRCCOPY rop family as PresentRect @ 10d0:0854;
 * VALIDATERECT is a no-op on SDL3. AD2 (doc only, no impl change):
 * blitted frames reach here only after the gfx_rle_expand_best
 * dispatcher (RLL/INV/LZSS/RAW0/2/4/8 + fail log via
 * gfx_rle_expand_best_ex, see gfx_indexed.h). gfx_port_create @ 10d0:0000 /
 * gfx_port_create_compatible wire ren (GETDC/CREATECOMPATIBLEDC borrow) +
 * tex via AllocBitmap @ 10d0:0cfa, so missing ren/tex is a corrupt port
 * (BAD_DATA), not a stub — no fake renderer guessed. Backs the
 * Anim_OpenSequence @ 10b0:008a GfxPort x2 pair + WdwMap_Open @ 1208:0af1 /
 * WdwSmMap_Open @ 1210:025b / WdwStatus_Open @ 1218:0386 views in
 * boot_open_windows. Error mapping: NULL ports -> NOENT (NULL src_rect =
 * whole source, kept); missing backing tex/ren -> BAD_DATA; src_rect w/h
 * <= 0 -> RANGE; SDL target-set / RenderTexture / target-restore fail ->
 * IO. Ref unpacked/civ.exe read-only (anchor 10d0:08f1). */
civ_error_t gfx_port_blit(gfx_port_t *dst, const gfx_port_t *src,
                          const SDL_Rect *src_rect, int dst_x, int dst_y);

/* Orig: FUN_1098_114A @ 1098:114a -> mainport_get_or_create
 * (MainPort_GetOrCreate). PARTIAL SDL_Texture probe bridge (see video.c):
 * window-sized port geometry is proven via SDL_GetRenderOutputSize +
 * gfx_port_create @ 10d0:0000 INDEX8 TARGET + tail probe (palette_apply_to_port
 * @ 1200:00a8/1090:0376 full 0..0xec range + gfx_port_set_active @ 10d0:0956
 * with render-target save/restore, results ignored) + destroy probe (same
 * backing store the Anim_OpenSequence @ 10b0:008a GfxPort x2 pair and the map
 * views need), but the port is NOT returned yet: SELECTPALETTE/REALIZEPALETTE
 * (via palette_apply_to_port @ 1200:00a8/1090:0376, blocked on the godpal
 * runtime capture) + main-screen GfxPort_SetActive @ 10d0:0956
 * render-target semantics stay TBD, so the tail keeps UNSUPPORTED (no fake
 * CIV_OK). Called before WdwMap_Open @ 1208:0af1 / WdwSmMap_Open @
 * 1210:025b / WdwStatus_Open @ 1218:0386 in boot_open_windows (see boot.h).
 * NULL renderer/out reports NOENT; size-query fail reports IO; zero size
 * reports BAD_DATA; create fail propagates (NOMEM/RANGE/IO). */
civ_error_t mainport_get_or_create(SDL_Renderer *ren, gfx_port_t **out);

/* Orig: FUN_10c0_xxxx @ 10c0:xxxx -> gfx_port_fill_or_copy
 * (GfxPort_FillOrCopy). Sits between HMEMCPY @ 10b8:079d and BITBLT @
 * 10d0:0854/08f1; blit helper behind Anim_AdvanceFrames @ 10b0:0461.
 * Orig path: NULL src -> FILLRECT (solid fill of rect at dst_x/dst_y),
 * else BITBLT rop 0xcc0020 (SRCCOPY) from src. SDL3: copy half is LIVE
 * as a thin delegate to gfx_port_blit @ 10d0:08f1 (same rop, same
 * mapping, NULL rect = whole source); fill half stays UNSUPPORTED (fill
 * brush/color has no param here — orig via Gfx_MakeBrush @ 1098:00b5 —
 * so no fake color; SDL_RenderFillRect on the port TARGET TBD). NULL
 * dst reports NOENT (NULL src = fill path, kept; NULL rect =
 * full/origin, kept). */
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

/* AK2: font glyph blit (2026-10-16).
 * Orig Font_EnumFamiliesInit @ 1008:0e99 FNT glyph-bits side via
 * font_render_glyph @ 1008:0e99 dfBitsOffset (see boot.h font_fnt_bits /
 * font_render_glyph + gfx_indexed.h font_glyph_to_surface, read via Read,
 * not edited) -> font_blit_glyph (SDL bridge port, no Win16).
 * Glyph is the borrowed 8-bit surface from font_glyph_to_surface (w/h +
 * MSB-first bits, bit 7 = leftmost; valid while the FNT image /
 * font_render_glyph dst lives, never freed here); tex is the caller-owned
 * dest texture (INDEX8 TARGET port backing store or screen texture, never
 * owned/freed here); win/ren are borrowed (win kept for WinMain-order
 * call-site symmetry, ignored, may be NULL; ren is the DC placeholder,
 * borrowed, never owned). SDL path: SDL_CreateTextureFromSurface(ren,
 * glyph) temp + SDL_SetRenderTarget(ren, tex) + SDL_RenderTexture(tmp ->
 * x/y rect) + target restore + temp destroy (same SRCCOPY 0xcc0020 family
 * as GfxPort_Blit @ 10d0:08f1 / GfxPort_PresentRect @ 10d0:0854;
 * VALIDATERECT is a no-op on SDL3). No globals, no exit(), C17; SDL only
 * here (dialog HLP stays SDL-free). Error mapping (consistent with
 * video.c): NULL ren/tex/glyph -> NOENT (NULL win = ignored, kept);
 * glyph w/h <= 0 -> BAD_DATA; SDL temp-create / target-set / Render /
 * target-restore fail -> IO. Parity: bit order matches font_fnt_bits
 * MSB-first + font_render_glyph tight stride (w+7)/8 +
 * font_glyph_to_surface expand (see gfx_indexed.h);
 * font_render_glyph/text_width/height + help_hlp_* untouched (no break). */
civ_error_t font_blit_glyph(SDL_Window *win, SDL_Renderer *ren,
                            SDL_Texture *tex, int x, int y,
                            const SDL_Surface *glyph);

/* Orig: FUN_1200_00a8 @ 1200:00a8 MainPort_ApplyPalette tail +
 * FUN_1090_0376 @ 1090:0376 Palette_AnimateRange -> palette_apply_to_port
 * (Palette_ApplyToPort; video-side helper, map-side
 * mainport_apply_palette in map.h stays the caller). Ported: applies
 * [first, first+count) via SDL_SetPaletteColors on the port INDEX8
 * backing store (SDL_GetTexturePalette when present, else a temp
 * 256-entry palette attached via SDL_SetTexturePalette; orig: RGB
 * triples + flag byte copy then ANIMATEPALETTE, opcode 1 full apply +
 * 0xe fade from MainPort_ApplyPalette, timed 1/0xe from
 * Anim_AdvanceFrames @ 10b0:0461). Y2 HoF/show path: range guard feeds
 * HoF_Show @ 1080:0119 + HoF_VictoryCeremony @ 1080:06ff fades before
 * gfx_port_present_rect @ 10d0:0854 (see halloffame.h Y2 flow). Colors
 * feed from gfx_godpal_get (godpal.c 0x00..0xeb) + black tail past 0xec;
 * propagates UNSUPPORTED until the dump lands (no fake bytes). NULL port
 * reports NOENT; first < 0 or count <= 0 or first+count > 256 reports
 * RANGE; missing backing texture reports BAD_DATA; SDL failure reports
 * IO/NOMEM. */
civ_error_t palette_apply_to_port(gfx_port_t *port, int first, int count);

/* --- Artwork frame chain (segment 10b8, 2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Art_LoadFrame @ 10b8:0e00 (Art_LoadFrameData @ 10b8:079d with
 *     0x14-param + Art_PortDataToBitmap @ 10b8:0e36; 10 callers incl.
 *     HoF_Show @ 1080:0119 + NewGame_Wizard @ 10e8:0000 +
 *     WorldGen_LoadEarth @ 1108:0243 + FUN_11d8_0000/087f).
 *   Art_LoadFrameData @ 10b8:079d (Art_LoadDiskFile @ 10b8:0aa8 disk
 *     path via _LOPEN/_HREAD + save_unpack when DAT_1420_17a8 != 0,
 *     else Art_LoadResPaletted @ 10b8:11bc RSC path via ResMgr_GetById
 *     + palette expand to 0xec + Art_DecodeLZW @ 10b8:158c, then
 *     Art_SaveConvertedFrame @ 10b8:0c1c when GOV != 1, then
 *     GfxPort_Create @ 10d0:0000 + Gfx_StoreDC @ 1098:1104).
 *   Art_PortDataToBitmap @ 10b8:0e36 (GR_PortDataToBitmap/log string:
 *     even-width BITBLT path vs odd-width pad via DAT_1420_17a6 vs
 *     CREATECOMPATIBLEDC + SETBITMAPBITS + BITBLT 0xcc0020 rows, then
 *     SELECTOBJECT/DELETEOBJECT + SETRECT clip; driver-dependent).
 *   Art_DecodeLZW @ 10b8:158c (GIF-style LZW: 2 << bits clear/eoi,
 *     dict via GLOBALALLOC 0x2000/0x4000 tables; sibling of
 *     gfx_lzw_decode @ 1420:2372/23ce, see gfx_indexed.h — decode
 *     core stays there, no duplicate here).
 *   Art_DrawProgressFill @ 10b8:062d (SETRECT fill-width math + FILLRECT;
 *     callee of the Art_MaybeConvertArtwork @ 10b8:0000 convert loop
 *     via Art_PumpConvertCancel @ 10b8:06a0 + Art_PreviewFrame @
 *     10b8:076a + Art_CheckDiskSpace @ 10b8:04df 9 MB gate).
 * OS-free fill-width math below is LIVE 1:1 (no Win16, no globals);
 * bitmap/driver conversion stays STUB with TODO Orig. */

/* Orig: Art_DrawProgressFill @ 10b8:062d fill-width math — progress bar
 * segment width. 1:1: fill_w = (bar_w * done) / total (orig SETRECT
 * right = left + (long)(w * done) / total, then FILLRECT). Returns the
 * fill width, 0 when total <= 0 or done <= 0 (orig DIV would fault;
 * guarded here). No globals, no exit(). */
int art_progress_fill_x(int bar_w, int done, int total);

/* Orig: Art_LoadFrame @ 10b8:0e00 + Art_LoadFrameData @ 10b8:079d +
 * Art_PortDataToBitmap @ 10b8:0e36 — frame load + driver convert
 * (GLOBALALLOC/LOCK + ResMgr/palette/LZW decode + GfxPort_Create @
 * 10d0:0000 + BITBLT 0xcc0020 present). SDL3 indexed-texture bridge
 * TBD (see gfx_port_load_rsc_frame above + gfx_lzw_decode in
 * gfx_indexed.h for the LZW core). Startup bridge (Main_RunStartupSequence
 * @ 11e0:0000, see boot.h/boot.c): the 0x94 intro host backing store is the
 * 0x200x0x140 preset (gfx_startup_port_size_ok below) for the GfxPort_Create
 * @ 10d0:0000 x2 pair (anim_seq_port_count/off in dialog.h); the frame id
 * gate is art_frame_id_ok below (exact intro frame id TBD from the dump).
 * Ref unpacked/civ.exe (read-only): "RanDoMRadio"/"RanDoMStatic" host
 * strings + "OPENING" sound reach this path. */
civ_error_t art_load_frame(int frame_id, gfx_port_t **out);

/* Orig Art_LoadFrameData @ 10b8:079d frame-id gate -> art_frame_id_ok.
 * OS-free 1:1: 1 when frame_id >= 0 (disk via Art_LoadDiskFile @ 10b8:0aa8
 * else RSC via Art_LoadResPaletted @ 10b8:11bc both take non-negative ids;
 * negative ids fault in orig), 0 otherwise. Exact intro frame id for the
 * Main_RunStartupSequence @ 11e0:0000 host TBD from the dump (bridge probe
 * uses 0; EARTH preset 0x89 is the documented sibling, see worldgen.h), so
 * this stays a range gate, never a table. No globals, no exit(). */
int art_frame_id_ok(int frame_id);

/* Orig GfxPort_Create @ 10d0:0000 x2 backing size for the 0x94 intro host
 * (Main_RunStartupSequence @ 11e0:0000 via Anim_OpenSequence @ 10b8:008a
 * ctx offs +9/+0x62, see anim_seq_port_count/off in dialog.h) ->
 * gfx_startup_port_size_ok. OS-free 1:1: 1 when w == 0x200 && h == 0x140
 * (the Dlg_CreateTracked @ 1098:124f preset = CIV_DLG_PRESET_W/H via
 * dlg_preset_w/h in dialog.h; pitch ((w+1)/2*2) via gfx_port_bitmap_pitch
 * stays 0x200), 0 otherwise. Actual SDL_Texture pair needs a borrowed
 * renderer (see gfx_port_create / boot_open_windows) -> size gate only.
 * No globals, no exit(). */
int gfx_startup_port_size_ok(int w, int h);

/* Orig: FUN_1008_0329 @ 1008:0329 WinMain_Shutdown video-only tail ->
 * video_shutdown_display. DESTROYWINDOW for the display ports/windows
 * plus REMOVEFONTRESOURCE(CIV_FONT_FILE "civfonts.fon" @ 1420:00a4/00fe,
 * added by Font_EnumFamiliesInit @ 1008:0e99; NE DLL/font container
 * 244224 B, MZ "MZ" + e_lfanew 0x250 + NE "NE" + RT_FONTDIR x1/RT_FONT x21
 * parsed OS-free via font_fon_parse/font_face_count/font_face_list in
 * boot.h/boot.c, borrowed bytes, no TTF raster here; ENUMFONTFAMILIES
 * family list + SDL_ttf face bridge TBD);
 * KILLTIMER/FREEPROCINSTANCE tails live in map/dialog shutdown (see
 * map.h/boot.c). SDL3 window/renderer stay caller-owned (see
 * boot_shutdown), so this installs/removes nothing yet. Void stub: no
 * globals, no exit(). AF2 doc only: Blit @ 10d0:08f1 / AllocBitmap @
 * 10d0:0cfa / PresentRect @ 10d0:0854 impls untouched. */
void video_shutdown_display(void);

/* --- Chain 2026-10-16 (5): unit sprite present (task goal 4) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Unit_DrawStacked @ 1110:1516 (stack pick, see game.h
 *     unit_stack_draw_pick LIVE) + Unit_DrawSingle @ 1110:0dba (single
 *     layer, see game.h unit_draw_single_layer LIVE: type icon + civ flag
 *     color + stacked badge when next != -1 + status letters; HP NOT drawn:
 *     one-roll kill, veteran 0x20 is the only combat state).
 *   Gfx_ClipTileRect @ 1208:2061 (was FUN_1208_2061: SETRECT tile 0x20 box +
 *     clip query @ 1100:0685; 0 = off-screen, caller skips).
 *   Gfx_DrawTechIcon @ 11a8:0cb1 (badge blit, see game.h gfx_draw_tech_icon
 *     STUB) + Unit_DrawStatusBadge @ 1110:0ff6 (flag/status badge, see
 *     game.h badge layer).
 *   Blitter link (gfx_indexed.c / video.c): indexed frames via
 *   gfx_indexed_to_texture @ 10d0:0854-family + SDL_RenderTexture present
 *   (see gfx_port_present_rect @ 10d0:0854 / gfx_port_blit @ 10d0:08f1).
 * What-to-draw enumeration (type/frame/pos) below is LIVE 1:1 (icon frame =
 * (civ*0x20+type) table index, tile 0x20 px box at view origin); the SDL
 * blit stays STUB with TODO Orig until the frame table lands. No globals,
 * no exit(), C17. */

/* Orig Unit_DrawSingle @ 1110:0dba icon frame -> video_unit_frame_index.
 * 1:1 table index civ*0x20+type (orig (civ*0x20+type)*2-0x6c30 word fetch;
 * badge variant +0x40 in Unit_DrawStatusBadge @ 1110:0ff6 stays caller-side).
 * Returns index >= 0, -1 on civ/type < 0. No globals. */
int video_unit_frame_index(int civ, int type);

/* Orig Gfx_ClipTileRect @ 1208:2061 tile box -> video_unit_tile_box. 1:1 the
 * 0x20 px tile box at the scrolled view origin: out = (x0, y0, x0+0x20,
 * y0+0x20) with x0 = (view_x)*0x20 - dx, y0 = (view_y)*0x20 - dy (orig
 * Map_WrapX(x - DAT_1330_4010)*0x20 - DAT_1408_00c6 per axis; wrap lives in
 * map_wrap_x, see map.h, caller passes the wrapped delta). NOENT on NULL
 * out. Else CIV_OK. No globals. */
civ_error_t video_unit_tile_box(int dx, int dy, int view_x, int view_y,
                                int *x0_out, int *y0_out);

/* Orig Unit_DrawSingle @ 1110:0dba + Unit_DrawStatusBadge @ 1110:0ff6 blit
 * tail -> video_draw_unit_sprite. GfxPort blit (SELECTPALETTE + BITBLT via
 * 10d0:0854/08f1 + Gfx_DrawTechIcon @ 11a8:0cb1 icon cell + badge) -> SDL
 * texture blit TBD (needs the icon-cell source rects + INDEX8 frames, see
 * gfx_indexed.h). NULL port -> NOENT; otherwise CIV_ERR_UNSUPPORTED. */
civ_error_t video_draw_unit_sprite(gfx_port_t *port, int frame, int x, int y);

/* --- Chain 2026-10-16 (6): clip-rect intersect (task goal 3) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Gfx_ClipTileRect @ 1208:2061 (SETRECT tile 0x20 box at (x,y) + clip query
 *     Gfx_IntersectRect @ 1100:0685; 0 = off-screen, caller Unit_DrawSingle @
 *     1110:0dba skips; callers Unit_DrawSingle + FUN_11f0_4e26).
 *   Gfx_IntersectRect @ 1100:0685 (was FUN_1100_0685: rect intersect via
 *     Util_MemCopyFar @ 1000:137f of the clip rect then max(left,top) +
 *     min(right,bottom) + empty -> 0; 8 callers incl. Gfx_ClipTileRect,
 *     GfxPort_ChangeColor @ 10d0:09ee, FUN_1110_04d0/11e8:0674/07c3,
 *     FUN_1208_1534/1866/1f64).
 *   Gfx_DrawTechIcon @ 11a8:0cb1 (badge blit: param_1 == 0 skips, else
 *     FUN_11a8_0f56 with palette 0x14/param_6 or 0x1a when DAT_1420_17a8 == 1;
 *     20+ callers incl. Unit_DrawSingle, Status_DrawTechIcons @ 1088:1a82,
 *     Combat_ResolveAttack @ 1050:0000 animation; blit stays STUB in
 *     gfx_draw_tech_icon, see game.h).
 *   GfxPort_PresentRect @ 10d0:0854 (BITBLT rop 0xcc0020 SRCCOPY present of
 *     the clipped rect; VALIDATERECT no-op; STUB tail stays in
 *     gfx_port_present_rect above).
 * Viewport/zoom: no zoom in Civ1 (fixed 0x20 px tiles, see
 * video_unit_tile_box: x0 = view*0x20 - dx; Map_SetViewOrigin @ 1208:20b1
 * sets DAT_1330_4010/400e tile origin + DAT_1408_00c6/c4 pixel offsets, then
 * FUN_1208_213b scroll-clamps; wrap on X via map_wrap_x). The box below is
 * the 0x20 tile rect at the scrolled origin; the intersect below is the
 * viewport clip (map window client rect). OS-free 1:1 LIVE, no Win16. */

/* Orig Gfx_IntersectRect @ 1100:0685 + Gfx_ClipTileRect @ 1208:2061 clip ->
 * video_clip_rect. 1:1 max(a.left,b.left), max(a.top,b.top),
 * min(a.right,b.right), min(a.bottom,b.bottom); empty (right <= left or
 * bottom <= top) -> *visible_out = 0 with *out zeroed, else *out holds the
 * intersection and *visible_out = 1. Returns CIV_OK; NULL a/b/out/visible ->
 * NOENT. No globals, no exit(). */
civ_error_t video_clip_rect(const SDL_Rect *a, const SDL_Rect *b,
                            SDL_Rect *out, int *visible_out);

/* --- Chain 2026-10-16 (7): flag icons + clip callers (goals 4/5) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   Unit_DrawFlagIcon @ 11c8:008a (Gfx_DrawIconClipped @ 11e8:0674 tail with
 *     the icon handle DAT_1420_9350[param_1]; param_1 is the flag index).
 *   City_DrawTileView @ 1110:04d0 (was FUN_1110_04d0: city tile (x,y)->(px,py)
 *     view: tile class/occupancy/wonder icons + owner frame + IntersectRect
 *     clip + PresentRect when DAT_12b0_009a; GfxPort blits -> STUB).
 *   Gfx_DrawIconClipped @ 11e8:0674 (was FUN_11e8_0674: Gfx_DrawTechIcon then
 *     IntersectRect-clipped PresentRect of the icon cell when DAT_12b0_009a;
 *     icon present path -> STUB, cell math LIVE below).
 *   Gfx_PresentTileRect @ 11e8:07c3 (was FUN_11e8_07c3: IntersectRect-clipped
 *     PresentRect of the 0x20 tile box at (x,y); same STUB split).
 * Flag-icon index (LIVE below, 1:1): badge variant in Unit_DrawStatusBadge @
 *   1110:0ff6 is civ*0x20+type+0x40 (vs map icon civ*0x20+type in
 *   Unit_DrawSingle @ 1110:0dba via video_unit_frame_index above); the
 *   DAT_1420_9350[param_1] handle is the icon-cell source (blit stays STUB).
 * What the icons ARE: civ-colored unit flags (census lists + map badges):
 *   callers Unit_DrawStatusBadge @ 1110:0ff6 x2 (map badge: normal icon +
 *   0x1d fortify icon when flag & 8) + Census_ShowTechBars @ 1018:03f0
 *   (0x0c progress-bar segments), Census_ShowUnitList @ 1018:067b +
 *   Census_ShowTerrainCounts @ 1018:09cc (civ*0x20+unit+0x40 per row; see
 *   game.h census STUBs). Relation to video_unit_frame_index: same
 *   civ*0x20+type base, +0x40 selects the flag sheet (badge/census) vs the
 *   map sprite sheet. No globals, no exit(), C17.
 * What IntersectRect clips per caller (Gfx_IntersectRect @ 1100:0685 is the
 *   shared clip query, LIVE as video_clip_rect above; each site clips its
 *   own rect vs the port clip rect, 0 = off-screen skip):
 *   GfxPort_ChangeColor @ 10d0:09ee: port bitmap rect (w/h at +0xc/+0xe)
 *     before the GETBITMAPBITS index-remap scan (old->new in 0..255).
 *   City_DrawTileView @ 1110:04d0: 0x20 city-tile box at (px,py) before the
 *     tile/city blits + label PresentRect when DAT_12b0_009a != 0.
 *   Gfx_DrawIconClipped @ 11e8:0674: icon cell box (w/h from the icon
 *     resource via GLOBALLOCK) at (px,py) before PresentRect.
 *   Gfx_PresentTileRect @ 11e8:07c3: 0x20 tile box at (x,y) before
 *     PresentRect (unconditional repaint helper).
 *   Map_PaintTilesClipped @ 1208:1534: per-tile 0x20 boxes over the paint
 *     range (view origin + offsets) before RevealCenter/blank.
 *   Map_RepaintViewShuffled @ 1208:1866 x3: same per-tile boxes in the
 *     shuffled (mode 1) + row-major (mode 0) loops + the port-rect sync at
 *     the head (mode-agnostic clip).
 *   Map_DrawCityLabels @ 1208:1f64: per-label rects over DAT_1408_0000
 *     entries before Text_DrawAligned.
 *   Gfx_ClipTileRect @ 1208:2061: the 0x20 tile box at the scrolled origin
 *     (video_unit_tile_box + clip; 0 = Unit_DrawSingle skips).
 * OS-free index/cell math below is LIVE 1:1; blit/present tails stay STUB. */

/* Orig Unit_DrawFlagIcon @ 11c8:008a index + Unit_DrawStatusBadge @ 1110:0ff6
 * badge (+0x40) + Census_ShowUnitList/TerrainCounts @ 1018:067b/09cc
 * (civ*0x20+unit+0x40) -> unit_flag_icon_index. 1:1 civ*0x20+type+0x40.
 * Returns index >= 0, -1 on civ/type < 0. No globals. */
int unit_flag_icon_index(int civ, int type);

/* Orig Gfx_DrawIconClipped @ 11e8:0674 cell -> gfx_icon_cell_box. 1:1 the
 * icon-cell rect at (px,py): out = (px, py, px+cell_w, py+cell_h) (orig
 * SETRECT from the GLOBALLOCK'd icon w/h + OFFSETRECT to px/py; caller
 * passes cell_w/h explicitly, no handles). NOENT on NULL out; RANGE on
 * cell_w/h <= 0; else CIV_OK. The IntersectRect clip itself stays in
 * video_clip_rect above. No globals. */
civ_error_t gfx_icon_cell_box(int px, int py, int cell_w, int cell_h,
                              SDL_Rect *out);

/* Orig Gfx_DrawIconClipped @ 11e8:0674 -> gfx_draw_icon_clipped.
 * Icon blit + clipped present (Gfx_DrawTechIcon @ 11a8:0cb1 + PresentRect
 * @ 10d0:0854 when DAT_12b0_009a). SDL bridge TBD. */
civ_error_t gfx_draw_icon_clipped(int icon, int px, int py);

/* Orig Gfx_PresentTileRect @ 11e8:07c3 -> gfx_present_tile_rect.
 * Clipped present of the 0x20 tile box. SDL bridge TBD. */
civ_error_t gfx_present_tile_rect(int x, int y);

/* Orig City_DrawTileView @ 1110:04d0 -> city_draw_tile_view.
 * City tile (tx,ty)->(px,py) view (class/occupancy/wonder/owner + clip +
 * label present). GfxPort blits -> SDL TBD. */
civ_error_t city_draw_tile_view(int tx, int ty, int px, int py);

/* --- Chain 2026-10-16 (8): IntersectRect blits + present/alloc (goals 4/5) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   GfxPort_ChangeColor @ 10d0:09ee (remap bitmapy portu: SETRECT port
 *     w/h box + Gfx_IntersectRect @ 1100:0685 clip (0 = skip) +
 *     CREATECOMPATIBLEDC + even-width ((w+1)/2*2) bitmap via
 *     GfxPort_AllocBitmap @ 10d0:0cfa + BITBLT 0xcc0020 SRCCOPY +
 *     GETBITMAPBITS scan (old_index -> new_index per byte, stride pad
 *     ((w+1)/2*2-w) skipped per row) + SETBITMAPBITS write-back + BITBLT
 *     back + SELECT/DELETEOBJECT + DELETEDC; GLOBALALLOC fail ->
 *     FUN_1098_28e4 error).
 *   Gfx_DrawIconClipped @ 11e8:0674 (cell ikony: Gfx_DrawTechIcon @
 *     11a8:0cb1 blit, then IntersectRect-clipped PresentRect @ 10d0:0854
 *     of the icon cell (w/h from GLOBALLOCK'd icon resource via
 *     OFFSETRECT to px/py) when DAT_12b0_009a; FUN_1220_0f2f gate).
 *   Gfx_PresentTileRect @ 11e8:07c3 (box 0x20: IntersectRect-clipped
 *     PresentRect of the 0x20 tile box at (x,y), same DAT_12b0_009a +
 *     FUN_1220_0f2f gates; unconditional repaint helper).
 *   Co remapuja: ChangeColor remapuje INDEKSY palety (8-bit DIB bytes,
 *     old_index -> new_index, 0..255; mono? nie: mono branch zyje w
 *     Gfx_MakeBrush @ 1098:00b5 solid-vs-pattern (gfx_brush_is_solid
 *     LIVE), tu zawsze bajtowy remap bitmapy portu; paleta (godpal)
 *     nietknieta, tylko pixele). Zwiazek z gfx_indexed.c: port bitmap
 *     to INDEX8 (jak rsc_indexed_t pixels + gfx_godpal 0x00..0xeb, patrz
 *     gfx_indexed.h GFX_GODPAL_COUNT; indeksy 0xec..0xff to LOGPALETTE
 *     0x300 zero-tail / wolne sloty, remap je tez obejmuje w 0..255).
 *   Gfx_FillBarRect @ 11e8:0504 (was FUN_11e8_0504: bar fill: mode 2 ->
 *     x = px+w*2, h = hgt<<1 else x = px+icon_w, h = icon_h via
 *     FUN_11e8_0d81/0da8; y = py+h; cap 0x1ff when mono == 0 (DAT_1420_
 *     17a6) && x1 > 0x1ff && px > 0; FILLRECT brush civ color (0xf when
 *     mono && param_7 == 0xf via DAT_1420_17a0 else civ color); callers
 *     Census_ShowTechBars @ 1018:03f0 + City_ShowCensusReport @ 1018:1d5b
 *     + status rows).
 *   GfxPort_PresentRect @ 10d0:0854 (prezentacja wycietego rectu: BITBLT
 *     rop 0xcc0020 SRCCOPY w = right-left, h = bottom-top + VALIDATERECT
 *     no-op; juz LIVE jako gfx_port_present_rect -> SDL_RenderTexture
 *     refl; VALIDATERECT bez odpowiednika w SDL3, patrz video.h).
 *   GfxPort_AllocBitmap @ 10d0:0cfa (alokacja bitmap device-dependent:
 *     w Art_PortDataToBitmap @ 10b8:0e36: even-width direct vs odd-width
 *     pad via DAT_1420_17a6 vs compatible-DC + SETBITMAPBITS + BITBLT
 *     rows; juz LIVE jako gfx_port_alloc_bitmap -> SDL_CreateTexture
 *     INDEX8 TARGET, patrz video.h).
 * Port regul remap/geometrii -> LIVE below (no Win16, no globals,
 * read-only except the explicit remap dst); blit/present/device tails
 * stay STUB (gfx_port_change_color / gfx_draw_icon_clipped /
 * gfx_present_tile_rect / city_draw_tile_view). */

/* Orig GfxPort_ChangeColor @ 10d0:09ee scan core -> gfx_color_remap_match.
 * 1:1: nonzero (1) when pixel == old_index (byte compare before the
 * *dst = new write). Takes bytes explicitly. No globals. */
int gfx_color_remap_match(int pixel, int old_index);
/* Orig same row stride -> gfx_remap_stride. 1:1: ((w+1)/2)*2 (even-width
 * bitmap pitch; pad ((w+1)/2*2-w) skipped per row). Returns pitch, -1 on
 * w < 0. No globals. */
int gfx_remap_stride(int w);
/* Orig same CPU-side rule -> gfx_remap_index. 1:1 GETBITMAPBITS +
 * SETBITMAPBITS pair on explicit bits (no handles): for i in 0..n-1
 * bits[i] == (uint8_t)old_index -> bits[i] = (uint8_t)new_index.
 * Returns swapped count, -1 on NULL bits. Indices 0..255 (godpal.c
 * 0x00..0xeb live + zero-tail, see gfx_indexed.h). No globals. */
int gfx_remap_index(uint8_t *bits, size_t n, int old_index, int new_index);
/* Orig Gfx_PresentTileRect @ 11e8:07c3 box -> gfx_tile_box. 1:1 the 0x20
 * tile box at (x,y): out = (x, y, x+0x20, y+0x20) (orig SETRECT +
 * IntersectRect @ 1100:0685 clip caller-side via video_clip_rect LIVE).
 * NOENT on NULL out. No globals. */
civ_error_t gfx_tile_box(int x, int y, SDL_Rect *out);
/* Orig Gfx_FillBarRect @ 11e8:0504 extent -> gfx_bar_fill_extent. 1:1:
 * mode == 2 (local_6) ? x1 = px + w*2, h = hgt<<1 : x1 = px + icon_w,
 * h = icon_h (icon_w/h via FUN_11e8_0d81/0da8 icon resource; caller
 * passes them explicitly, no handles). NOENT on NULL outs. No globals. */
civ_error_t gfx_bar_fill_extent(int mode, int px, int w, int hgt,
                                int icon_w, int icon_h,
                                int *x1_out, int *h_out);
/* Orig same cap -> gfx_bar_fill_cap. 1:1: mono == 0 (DAT_1420_17a6) &&
 * x1 > 0x1ff && px > 0 -> 0x1ff, else x1. No globals. */
int gfx_bar_fill_cap(int x1, int px, int mono);
/* Orig Art_PortDataToBitmap @ 10b8:0e36 width branch ->
 * art_port_is_even_width / art_port_padded_width. 1:1: even == (w/2*2
 * == w) takes the direct BITBLT path; odd with mono (DAT_1420_17a6 ==
 * 1) pads via GLOBALALLOC + HMEMCPY rows, odd without mono goes the
 * compatible-DC + SETBITMAPBITS + BITBLT-rows path; padded = ((w+1)/2)
 * *2 (same pitch as gfx_remap_stride). Returns 1/0 or pitch (-1 on
 * w < 0). No globals. */
int art_port_is_even_width(int w);
int art_port_padded_width(int w);

/* --- Chain 2026-10-16 (9): census header/badge + bar + port slots ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   City_DrawCensusHeader @ 1018:015d (was FUN_1018_015d: GfxPort_SetActive
 *     + FILLRECT 0x140x0x200 civ brush + title TEXTOUT at 0xa0,2 via
 *     FUN_10d8_046e + civ line at 0xa0,10 + year line at 0xa0,0x12 via
 *     FUN_10d8_0528 color 0xf; civ adjective Res 0x2da/0x1da + gov title
 *     0x132/0x139/0x141 by (gov_word/2) + year via Status_AppendYearText
 *     @ 1088:1dd0; mono DAT_12d8_28fc forces civ 0; callers
 *     City_OpenCensusView @ 1018:0000 + City_ShowCensusReport @ 1018:1d5b
 *     paging (y > 0xbe) + Census_ShowCityProduction @ 1018:0be0 (was
 *     FUN_1018_0be0: per-city trade/production census, Res 0x8a,4,2) +
 *     Census_ShowCityStatus @ 1018:208f (was FUN_1018_208f: per-city
 *     size/output census, Res 0x8a,0,8) + Hof_CalcFinalScore @ 1040:1137
 *     (was FUN_1040_1137: final-score census view + Gfx_FillBarRect bars;
 *     NOT the disaster: disaster is City_ProcessDisaster @ 1040:068e,
 *     see game.h; score callers Game_ProcessGameEnd @ 1088:0b6d +
 *     Game_TurnLoopTick @ 1088:0000). Other header painters: HoF_Show @
 *     1080:0119 + Demo_ShowDemographics @ 1188:123a draw their own titles
 *     (no shared header; see halloffame.h/game.h). GDI text tails stay
 *     STUB in city_draw_census_header (see game.h); pure header math
 *     lives there (gov title / mono civ / line positions).
 *   Census_DrawTechBadge @ 1018:24c5 (badge: GfxPort_GetPalette @
 *     10d0:0262 port+0x14 word (0 when mono DAT_12d8_28fc) + Palette_
 *     CopyRange @ 1098:201d 0xec-capped GLOBALLOCK copy + GfxPort_
 *     ApplyPalette @ 1200:0116 MainPort_ApplyPalette wrapper with
 *     port+0x14 + FILLRECT 0x60x0x35 OFFSETRECT badge box + Text_SetColor
 *     44fc brush + Dialog_DrawFrame3D @ 1220:0d38 raised frame (sole
 *     other caller Dialog_MenuZ_Modal @ 1220:0156) + Gfx_DrawTechIcon @
 *     11a8:0cb1(*(icon*2+0x128)) via Gfx_BlitTechIcon @ 11a8:0f56 core
 *     (sole caller; palette 0x14/req or 0x1a when mono DAT_1420_17a8).
 *     Which port: the census dialog port (City_OpenCensusView @
 *     1018:0000 GfxPort_Create @ 10d0:0000 + GfxPort_SetActive @
 *     10d0:0956 selection; badge params 0x48x0x50 are the palette-copy
 *     window, NOT MainPort). Which icons: tech icons (Census_ShowTechBars
 *     @ 1018:03f0 0x0c segments + 0x48 tech list; badge icon is the
 *     tech-table word). Relation to icon_cell_box: the badge box IS an
 *     icon cell (0x60x0x35 fill + frame + tech blit; cell math shares
 *     gfx_icon_cell_box).
 *     Callers of the badge: Census_ShowTechBars + Census_ShowUnitList @
 *     1018:067b + Census_ShowCityProduction + Dialog_MenuZ_Modal +
 *     FUN_1138_0000 (status). GDI/palette-device tails stay STUB;
 *     OS-free table/size/scale math below is LIVE.
 *   Gfx_FillBarRect @ 11e8:0504 (bar fill: GfxPort_SelectForCoords @
 *     1220:0f2f mode gate 0..3 (0 = skip) + Text_SetColor/Gfx_MakeBrush
 *     civ color (mono DAT_12d8_28fc ? DAT_12d8_45fd fallback :
 *     civ_table[param_7]) + extent (mode == 2 ? x1 = px+w*2, h = hgt<<1
 *     : x1 = px+icon_w, h = icon_h via Gfx_ScaleIconX/Y @ 11e8:0d81/0da8
 *     (v*0xa0)/100) + y = py+h + cap 0x1ff when mono == 0 && x1 > 0x1ff
 *     && px > 0 + FILLRECT brush (mono && param_7 == 0xf ?
 *     DAT_1420_17a0 : civ brush). Already LIVE: gfx_bar_fill_extent +
 *     gfx_bar_fill_cap; extended below with color/brush/range/scale.
 *     Callers (18): Census_ShowTechBars + City_ShowCensusReport +
 *     Hof_CalcFinalScore + Demo rank bars @ 1188:0b3e/0db8 + status rows
 *     @ 11b0:0000/11b8:14e5 + dialog/replay bars (1070:0000/0327,
 *     10d8:0612, 1158:08e7, 11d8:0000/087f, 1030:8c11, Gfx_FillBarRectRange
 *     @ 11e8:0912, Replay_PromptMode @ 1180:017a, Replay_ShowConqueror @
 *     1180:152f, Replay_ShowEntry @ 1180:0e78). No HP bars (one-roll
 *     kill, see game.h) and no production-progress bars here (city prod
 *     is icon lists, not bars).
 *   Gfx_FillBarRectRange @ 11e8:0912 (was FUN_11e8_0912: degenerate-range
 *     wrapper: (x0 == x1 || y0 == y1) -> x-swap when x1 < x0, then
 *     Gfx_FillBarRect with w = (x1-x0)+1, h = (y1-y0)+1).
 *   Gfx_ScaleIconX @ 11e8:0d81 / Gfx_ScaleIconY @ 11e8:0da8 (was
 *     FUN_11e8_0d81/0da8: shared core (v*0xa0)/100 driver scale for icon
 *     w/h in FillBarRect + coord scale in SelectForCoords).
 *   GfxPort_SelectForCoords @ 1220:0f2f (was FUN_1220_0f2f: port select
 *     by coords: no port (DAT_1420_9948 == 0) ? x<0||x>199 ?
 *     (y<8||x<200 ? 0 outside : 2 dialog + SetActive + 17d0=12b0:0004) :
 *     1 main (17d0=12b0:0034) : 3 port (scale or dialog-offset + 17d0 =
 *     port + 17cc=17ae); dialog offset (v-base)*2-off with bases 8/200
 *     and DAT_1408_00c4/c6).
 *   GfxPort_GetPalette @ 10d0:0262 (was FUN_10d0_0262: port+0x14 word
 *     when color (DAT_12d8_28fc == 0) else 0; feeds Palette_CopyRange +
 *     GfxPort_ApplyPalette in the badge).
 *   Palette_CopyRange @ 1098:201d (was FUN_1098_201d: 0xec-capped
 *     GLOBALLOCK palette copy; callers badge + Anim_AdvanceFrames @
 *     10b0:0461 + UI palettes).
 *   GfxPort_ApplyPalette @ 1200:0116 (was FUN_1200_0116: MainPort_
 *     ApplyPalette @ 1200:00a8 wrapper with port+0x14 + 0x14 params;
 *     callers badge + HoF_Show + NewGame_Wizard + WorldGen_LoadEarth +
 *     status/dialog/replay UI).
 *   Dialog_DrawFrame3D @ 1220:0d38 (was FUN_1220_0d38: raised 3D frame:
 *     CREATEPEN 0x8080/0xe0e0 + MOVETO/LINETO + INFLATERECT x3; callers
 *     badge + MenuZ).
 *   Gfx_BlitTechIcon @ 11a8:0f56 (was FUN_11a8_0f56: tech-icon blit core
 *     behind Gfx_DrawTechIcon @ 11a8:0cb1, sole caller; mono DAT_1420_17a6
 *     vs color paths with GfxPort_AllocBitmap @ 10d0:0cfa even-width
 *     bitmaps + BITBLT rops).
 *   Port lifecycle (slots DAT_1420_17cc active / DAT_1420_17ae prev):
 *     GfxPort_Create @ 10d0:0000 (GR_PortOpen: GETDC + CREATECOMPATIBLEDC
 *     + GfxPort_AllocBitmap @ 10d0:0cfa INDEX8 TARGET bitmap + clip/pal
 *     words; 33 creators: City_OpenCensusView, Pic_PlayShared @ 1030:0000,
 *     Dialog_MenuZ_Modal, Art_LoadFrameData @ 10b8:079d, WdwMap_Open @
 *     1208:0af1 / WdwSmMap_Open @ 1210:025b / WdwStatus_Open @ 1218:0386,
 *     MainMenu_WndProc @ 11e0:0221, NewGame_Wizard @ 10e8:0000, HoF/Demo/
 *     Replay/WorldGen UI + WinMain @ 1008:0000) -> GfxPort_SetActive @
 *     10d0:0956 (17cc = port always; 17ae = port unless the 12d8:44ff+0x43
 *     dialog port) -> GfxPort_PresentRect @ 10d0:0854 (BITBLT 0xcc0020 +
 *     VALIDATERECT no-op; LIVE) / GfxPort_Blit @ 10d0:08f1 (LIVE round-
 *     trip) -> GfxPort_Close @ 10d0:0172 (was FUN_10d0_0172: GR_PortClose:
 *     ClearActive prologue + DELETEDC + DELETEOBJECT + GLOBALFREE x2 +
 *     zero words) vs GfxPort_ClearActive @ 10d0:09a2 alone (slot clear
 *     only, no free; 12 switchers: Anim_AdvanceFrames, Game_UpdateStatus
 *     Panel @ 1088:13d5, Map_PaintTilesClipped @ 1208:1534, Map_Repaint
 *     ViewShuffled @ 1208:1866, Unit_ShowInfoPopup @ 11f0:468c, WdwStatus_
 *     Refresh @ 1218:0801, 1098:21b8, 10b0:0287/17e5, 10e8:231f,
 *     1210:09b4, 1218:06db; City_CloseCensusView @ 1018:0380 closes via
 *     Present + wait + 1100:0be5 + Close/ClearActive + destroy). Create/
 *     SetActive/Close device tails stay STUB (see gfx_port_create etc.);
 *     AllocBitmap + PresentRect already LIVE; slot-clear math below is
 *     LIVE on explicit slots (no globals).
 * OS-free 1:1 LIVE below (no Win16, no globals, C17). */

/* Orig Census_DrawTechBadge @ 1018:24c5 table -> census_badge_table_off.
 * 1:1 the *(icon*2+0x128) word-fetch offset feeding Gfx_DrawTechIcon @
 * 11a8:0cb1 (icon is the tech/badge index). Returns offset >= 0, -1 on
 * icon < 0. No globals. */
int census_badge_table_off(int icon);
/* Orig same badge box -> census_badge_size. 1:1 the 0x60x0x35 fill box
 * (SETRECT + OFFSETRECT to dx/dy caller-side; position shares
 * gfx_icon_cell_box). Writes w/h, NOENT on NULL. No globals. */
civ_error_t census_badge_size(int *w_out, int *h_out);
/* Orig Gfx_ScaleIconX @ 11e8:0d81 + Gfx_ScaleIconY @ 11e8:0da8 shared core
 * -> gfx_scale_icon_dim. 1:1 (v*0xa0)/100 driver scale (icon w/h in
 * Gfx_FillBarRect @ 11e8:0504 + coord scale in GfxPort_SelectForCoords @
 * 1220:0f2f; x86 LONG divide truncates toward zero). No globals. */
int gfx_scale_icon_dim(int v);
/* Orig Gfx_FillBarRect @ 11e8:0504 color -> gfx_bar_fill_color. 1:1:
 * mono == 0 (DAT_12d8_28fc) ? civ_entry : fallback (civ_entry is
 * civ_table[param_7], fallback is DAT_12d8_45fd). No globals. */
int gfx_bar_fill_color(int mono, int civ_entry, int fallback);
/* Orig same brush -> gfx_bar_brush_is_alt. 1:1 nonzero (1) when the
 * DAT_1420_17a0 brush wins: mono != 0 && param_7 == 0xf, else 0 (civ
 * brush). No globals. */
int gfx_bar_brush_is_alt(int mono, int param_7);
/* Orig Gfx_FillBarRectRange @ 11e8:0912 gate/span ->
 * gfx_bar_range_needs_fill / gfx_bar_range_width / gfx_bar_range_height.
 * 1:1: needs = (x0 == x1 || y0 == y1); width = (max(x0,x1)-min(x0,x1))+1
 * (orig swaps x only); height = (y1-y0)+1 (no y swap in orig). Width -1
 * on degenerate? no: always >= 1 for int inputs (overflow caller-side).
 * No globals. */
int gfx_bar_range_needs_fill(int x0, int x1, int y0, int y1);
int gfx_bar_range_width(int x0, int x1);
int gfx_bar_range_height(int y0, int y1);
/* Orig GfxPort_SelectForCoords @ 1220:0f2f branch ->
 * gfx_port_select_kind. 1:1 port-mode pick without device state:
 * has_port == 0 ? (x<0||199<x ? (y<8||x<200 ? 0 : 2) : 1) : 3 (0 outside,
 * 1 main, 2 dialog, 3 port). Mode 2 also GfxPort_SetActive's the dialog
 * port + 17d0=12b0:0004; mode 1 sets 17d0=12b0:0034; mode 3 scales (see
 * gfx_scale_icon_dim / gfx_port_dialog_coord) + 17d0=port + 17cc=17ae.
 * has_port/x/y are explicit (DAT_1420_9948/1260:002c stay caller-side).
 * No globals. */
int gfx_port_select_kind(int has_port, int x, int y);
/* Orig same dialog offset -> gfx_port_dialog_coord. 1:1 (v-base)*2-off
 * (bases 8/200 with DAT_1408_00c4/c6 offs passed explicitly). No globals. */
int gfx_port_dialog_coord(int v, int base, int off);
/* Orig GfxPort_GetPalette @ 10d0:0262 gate -> gfx_port_palette_word.
 * 1:1: mono == 0 ? word_at_14 : 0 (word_at_14 is the port+0x14 palette
 * word; mono is DAT_12d8_28fc != 0). Caller passes the word explicitly
 * (no handles). No globals. */
int gfx_port_palette_word(int mono, int word_at_14);
/* Orig GfxPort_ClearActive @ 10d0:09a2 + GfxPort_Close @ 10d0:0172
 * prologue -> gfx_port_slot_after_clear / gfx_port_clear_active. 1:1
 * slot clear: slot == port -> NULL else slot (far-ptr compare in orig;
 * here borrowed gfx_port_t pointers). The clearer takes explicit slot
 * addresses (no DAT_1420_17cc/17ae globals): NULL slot addrs -> NOENT;
 * NULL port clears NULL slots (no-op) and never fails. No globals. */
const gfx_port_t *gfx_port_slot_after_clear(const gfx_port_t *slot,
                                            const gfx_port_t *port);
civ_error_t gfx_port_clear_active(const gfx_port_t *port,
                                  const gfx_port_t **active_io,
                                  const gfx_port_t **prev_io);

/* --- Chain 2026-10-16 (10): GfxPort create params + tech blit + palette ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   GfxPort_Create @ 10d0:0000 (GR_PortOpen: Debug_LogTrace "GR PortOpen" +
 *     GETDC(0) + CREATECOMPATIBLEDC + RELEASEDC + Util_MemCopyFar rect +
 *     OFFSETRECT + port ~0x20 words ([4..7] w/h clip, [8] w, [10] color
 *     GLOBALALLOC or FUN_1098_28e4 error, [0x17]/[0xd] bitmap select,
 *     [0xe] palette param_4, [0xf] color handle, [0xc] compat DC) +
 *     GETDEVICECAPS 0xc/0xe + GfxPort_AllocBitmap @ 10d0:0cfa even width
 *     ((w+1)/2*2) + SELECTOBJECT; 33 creators: City_OpenCensusView @
 *     1018:0000, Pic_PlayShared @ 1030:0000, Dialog_MenuZ_Modal @ 1220:0156,
 *     Art_LoadFrameData @ 10b8:079d, WdwMap_Open @ 1208:0af1 /
 *     WdwSmMap_Open @ 1210:025b / WdwStatus_Open @ 1218:0386, MainMenu_WndProc
 *     @ 11e0:0221, NewGame_Wizard @ 10e8:0000, HoF/Demo/Replay/WorldGen UI +
 *     WinMain @ 1008:0000; relation to MainPort (MainPort_GetOrCreate @
 *     1098:114a: GETDC + SELECTPALETTE/REALIZEPALETTE +
 *     GfxPort_SetActive @ 10d0:0956; see boot.h boot_open_windows).
 *     GfxPort_SetActive @ 10d0:0956: Gfx_SetCurrentDC(port+0x16) + 17cc =
 *     port always + 17ae = port unless the 12d8:44ff+0x43 dialog port.
 *   Gfx_BlitTechIcon @ 11a8:0f56 (sole caller Gfx_DrawTechIcon @ 11a8:0cb1,
 *     which skips param_1 == 0 and picks palette 0x14/param_6 or 0x1a when
 *     mono DAT_1420_17a6 == 1): GLOBALLOCK icon (w/h at +0/+2, bg at +4) +
 *     bg resolve (param_5 == 0 → res bg) + Gfx_GetIconScale @ 11a8:0000
 *     scale when a != b + mono DAT_1420_17a6 vs color paths (mono:
 *     CREATECOMPATIBLEDC + even-width AllocBitmap + BITBLT 0xcc0020 +
 *     GET/SETBITMAPBITS via Gfx_AllocIconBuffer @ 11a8:0054 +
 *     Gfx_CopyIconBits @ 11a8:13c0 transparent/scaled copy; color: mask DC
 *     + text-color 0 + BITBLT triptych 0x46/0x66 + 0xc6/0x88 + rop) +
 *     GLOBALUNLOCK when param_1 != 0. Masks: transparent index (-1 =
 *     copy-all fast path, else skip-bg transparent copy) + mono mask
 *     bitmap (1-bpp via AllocBitmap 0,1) + color text-mask trio; bg is the
 *     fill index for the padded/odd-width tail (see art_port_padded_width).
 *   Palette_CopyRange @ 1098:201d (GLOBALLOCK src/dst palettes + 0xec cap
 *     on both ends + word copy loop + GLOBALUNLOCKs; gated on color
 *     DAT_12d8_28fc == 0 && param_5 != 0; callers Census_DrawTechBadge @
 *     1018:24c5 + Anim_AdvanceFrames @ 10b0:0461 + UI palettes).
 *   Device tails (GETDC/CREATECOMPATIBLEDC/GETDEVICECAPS/CREATEBITMAP/
 *     SELECTOBJECT/BITBLT/GET+SETBITMAPBITS/GLOBALALLOC) stay STUB in
 *     gfx_port_create/set_active/alloc_bitmap/blit above; param math below
 *     is LIVE 1:1 (no Win16, no globals, C17). */

/* Orig GfxPort_Create @ 10d0:0000 even width -> gfx_port_bitmap_pitch.
 * 1:1 ((w+1)/2)*2 (same pitch as gfx_remap_stride/art_port_padded_width;
 * duplicated here so the create path owns its copy). -1 on w < 0. */
int gfx_port_bitmap_pitch(int w);
/* Orig same mono gate -> gfx_port_is_mono. 1:1 DAT_12d8_28fc != 0 (1 =
 * mono/EGA, 0 = color/VGA; feeds GetPalette/CopyRange/MakeBrush). */
int gfx_port_is_mono(int flag_28fc);
/* Orig Gfx_BlitTechIcon @ 11a8:0f56 bg resolve -> gfx_tech_bg_resolve.
 * 1:1 param_5 == 0 → res_bg (icon word +4), else param_5. No globals. */
int gfx_tech_bg_resolve(int req_bg, int res_bg);
/* Orig same scale gate (via Gfx_GetIconScale @ 11a8:0000) ->
 * gfx_tech_needs_scale. 1:1 a != b (DAT_13a0_0000 vs 0002 set by
 * Gfx_SetIconScale @ 11a8:002c). No globals. */
int gfx_tech_needs_scale(int a, int b);
/* Orig Gfx_DrawTechIcon @ 11a8:0cb1 palette pick -> gfx_tech_palette_pick.
 * 1:1 (bg == -1 || mono != 1) ? req (0x14/param_6) : 0x1a. No globals. */
int gfx_tech_palette_pick(int bg, int mono, int req);
/* Orig Palette_CopyRange @ 1098:201d gate -> palette_copy_enabled. 1:1
 * (mono == 0 && count != 0) with mono = DAT_12d8_28fc != 0. No globals. */
int palette_copy_enabled(int mono, int count);
/* Orig same 0xec cap -> palette_copy_range_len. 1:1 count capped by
 * 0xec - src_off and 0xec - dst_off (negative/overrun → 0; count < 0 →
 * 0). 0xec = palette entries (godpal 0x00..0xeb, see gfx_indexed.h). */
int palette_copy_range_len(int src_off, int dst_off, int count);

/* --- Chain 2026-10-16 (11): icon scale/buffer @ 11a8 ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   Gfx_SetIconScale @ 11a8:002c (DAT_13a0_0000/0002 = num/den para).
 *   Gfx_GetIconScale @ 11a8:0000 (czyta parę do callerów).
 *   Gfx_AllocIconBuffer @ 11a8:0054 (GLOBALALLOC w*h+6 + w/h w buforze;
 *     callers City_DrawTileView @ 1110:04d0, Gfx_BlitTechIcon @ 11a8:0f56,
 *     Unit_DrawStatusBadge @ 1110:0ff6, Game_InitGlobalTables @ 11e8:0337
 *     (0x20x0x20), Gfx_AllocIconBuffer @ 11a8:05c7).
 *   Gfx_CopyIconBits @ 11a8:13c0 (transparent/skala w BlitTechIcon:
 *     param_3 == 0 ? res bg; fast memcpy gdy transparent == -1 && scale ==
 *     0 && pad == 0; else transparent loop; else scaled loop z
 *     Gfx_GetIconScale; callers Gfx_BlitIconToPort @ 11a8:0aa8 +
 *     Gfx_BlitTechIcon).
 *   Gfx_CopyIconBuffer @ 11a8:0328 (was FUN_11a8_0328: same-size HMEMCPY
 *     vs per-row min(w) HMEMCPY).
 *   Gfx_FreeIconBuffer @ 11a8:00d1 (was FUN_11a8_00d1: GLOBALFREE + trace).
 *   Gfx_BlitIconScaled @ 11a8:0450 (was FUN_11a8_0450: BITBLT direct gdy
 *     skala 1:1, else port + GETBITMAPBITS + scaled blit).
 *   Gfx_BlitIconToPort @ 11a8:0aa8 (was FUN_11a8_0aa8: port clip SETRECT +
 *     CopyIconBits + HMEMCPY rows).
 * Kiedy skala != 1 (nie mipmapy ani zoom minimapy — fixed 0x20 px tiles):
 *   Hof_CalcFinalScore @ 1040:1137 dopasowuje wiersze techów: DAT_1420_07dc
 *   < 0xb -> SetIconScale(3,4), < 9 -> (1,2), na końcu zawsze (1,1); poza
 *   tym BlitTechIcon skaluje gdy Get != siebie (a != b). Rozmiary buforów:
 *   Alloc = w*h+6 (2x u16 w/h + piksele; even pitch ((w+1)/2)*2 gdy mono,
 *   zob. gfx_port_bitmap_pitch/art_port_padded_width). Port reguł poniżej
 *   jest LIVE 1:1 (no Win16, no globals); GLOBALALLOC/BITBLT ogony STUB. */

/* Orig Gfx_AllocIconBuffer @ 11a8:0054 rozmiar -> gfx_icon_buffer_size.
 * 1:1 w*h+6 (nagłówek w/h + piksele). -1 gdy w/h <= 0. No globals. */
int gfx_icon_buffer_size(int w, int h);

/* Orig Gfx_CopyIconBits @ 11a8:13c0 + Gfx_ScaleIconX/Y @ 11e8:0d81/0da8
 * skalowany wymiar -> gfx_icon_scaled_dim. 1:1 (v*num)/den (Gfx_ScaleIcon
 * to (v*0xa0)/100). den <= 0 -> v. No globals. */
int gfx_icon_scaled_dim(int v, int num, int den);

/* Orig sama skala 1:1 -> gfx_icon_scale_is_unit. 1:1 num == den (para
 * 13a0:0000/0002 z Gfx_SetIconScale; Hof wraca do (1,1)). No globals. */
int gfx_icon_scale_is_unit(int num, int den);

/* Orig Gfx_CopyIconBits @ 11a8:13c0 fast gate -> gfx_icon_copy_is_fast.
 * 1:1 transparent == -1 && scaled == 0 && pad == 0 (memcpy połówek +
 * nieparzysty ogon). No globals. */
int gfx_icon_copy_is_fast(int transparent, int scaled, int pad);

/* Orig ten sam pad wiersza -> gfx_icon_row_pad. 1:1 need_align != 0 ?
 * ((w+1)/2*2-w) : 0 (even pitch, ten sam co gfx_port_bitmap_pitch).
 * -1 gdy w < 0. No globals. */
int gfx_icon_row_pad(int w, int need_align);

/* Orig Gfx_AllocIconBuffer/GlobalFree BITBLT ogony -> STUBs. */
civ_error_t gfx_alloc_icon_buffer(int w, int h, int *size_out);
civ_error_t gfx_copy_icon_bits(int transparent, int scaled);

/* --- Chain 2026-10-16 (12): icon blit geometry @ 11a8 (goals 3/5) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   Gfx_BlitIconToPort @ 11a8:0aa8 (port clip SETRECT + scaled dims via
 *     Gfx_GetIconScale @ 11a8:0000 ((w*num)/den) + Gfx_ExtractClippedIcon
 *     @ 11a8:05c7 (clip extract to a fresh icon buffer) +
 *     Gfx_CopyIconBits @ 11a8:13c0 + HMEMCPY rows clipped vs port w/h at
 *     +0x0c/+0x0e; empty port (+0x12 == 0) -> FUN_1098_28e4 error).
 *   Gfx_CopyIconBuffer @ 11a8:0328 (same-size single HMEMCPY vs per-row
 *     min(w) HMEMCPY over min(h) rows; HMEMCPY shape = Util_MemCopyFar @
 *     1000:137f, see civ_common.h util_mem_copy_far for the memcpy core).
 *   Gfx_DrawIconClipped @ 11e8:0674 (Gfx_DrawTechIcon @ 11a8:0cb1 blit +
 *     Gfx_IntersectRect @ 1100:0685 clip (video_clip_rect LIVE) +
 *     GfxPort_PresentRect @ 10d0:0854 BITBLT 0xcc0020 when DAT_12b0_009a;
 *     cell box LIVE as gfx_icon_cell_box).
 *   Gfx_BlitIconScaled @ 11a8:0450 (BITBLT direct when GetIconScale 1:1,
 *     else port + GETBITMAPBITS + scaled resample via Gfx_CopyIconBits:
 *     nearest-neighbour (point) resample, NOT bilinear — the scaled loop
 *     accumulates (err += num; while err >= den: emit, err -= den) with a
 *     half-step bias (err starts at num>>1), replicating/skipping source
 *     bytes without interpolation; see gfx_nearest_src below).
 *   Gfx_FreeIconBuffer @ 11a8:00d1 (GLOBALFREE + Debug_LogTrace @
 *     1148:003c "TileDispose()  :  shape.c" trace; free tail stays STUB
 *     in gfx_alloc_icon_buffer above, trace needs no port).
 *   Helpers renamed this pass: Gfx_ExtractClippedIcon @ 11a8:05c7 (clip
 *     extract), Gfx_DrawTechIconDefault @ 11a8:0c89 (0xff wrapper),
 *     Gfx_BlitIconWithBg @ 11a8:0a76 / Gfx_BlitIconDefault @ 11a8:0a4e
 *     (0x14-bg wrappers), Gfx_CreateIconMaskBitmap @ 11a8:0cfd +
 *     Gfx_BuildIconMask @ 11a8:0d6e (1-bpp mask + CREATEBITMAP),
 *     Gfx_CopyIconClipped @ 11a8:076f (transparent + offset clip),
 *     Gfx_GetIconContentBounds @ 11a8:00fe (non-bg bbox),
 *     Gfx_ReplaceIconColor @ 11a8:022f, Gfx_FillIconRect @ 11a8:02ad.
 * Dims/clip/palette split: scaled dims + clip spans + palette pick below
 * are LIVE 1:1 (no Win16, no globals, read-only); HMEMCPY rows map to
 * memcpy (see util_mem_copy_far) and GDI BITBLT/GLOBALLOCK tails stay
 * STUB. Palette: icon bytes are indices into the 0xec godpal run
 * (gfx_godpal_get, see gfx_indexed.h; mono branch in Gfx_MakeBrush @
 * 1098:00b5 via gfx_brush_is_solid LIVE); this blit never remaps indices
 * (remap lives in GfxPort_ChangeColor @ 10d0:09ee via gfx_remap_index). */

/* Orig Gfx_BlitIconToPort @ 11a8:0aa8 clip half -> gfx_blit_clip_len.
 * 1:1 clipped span at pos against the port extent: 0 when scaled <= 0
 * or pos >= port_len or port_len <= 0, else min(scaled, port_len - pos)
 * (orig pair: w vs +0x0c, h vs +0x0e). Negative pos is caller-side
 * (orig ports never pass it; returns scaled here, no clamp). */
int gfx_blit_clip_len(int pos, int scaled, int port_len);

/* Orig Gfx_CopyIconBuffer @ 11a8:0328 span half -> gfx_copy_span. 1:1
 * rows = min(h1, h2), cols = min(w1, w2) (same-size HMEMCPY is the
 * rows*h1 == rows*h2 && cols == w1 == w2 fast case, caller-side).
 * NULL outs -> NOENT; w/h <= 0 -> RANGE. Else CIV_OK. No globals. */
civ_error_t gfx_copy_span(int w1, int h1, int w2, int h2,
                          int *rows_out, int *cols_out);

/* Orig Gfx_BlitIconScaled @ 11a8:0450 direct gate ->
 * gfx_blit_scaled_is_direct. 1:1 num == den (GetIconScale pair from
 * Gfx_SetIconScale @ 11a8:002c; Hof_CalcFinalScore @ 1040:1137 restores
 * (1,1)) takes the BITBLT-direct path, else the nearest resample path
 * (see gfx_nearest_src). No globals. */
int gfx_blit_scaled_is_direct(int num, int den);

/* Orig Gfx_CopyIconBits @ 11a8:13c0 scaled loop (nearest DDA with the
 * half-step bias err0 = num>>1) -> gfx_nearest_src. 1:1 floor map
 * (dst*den)/num for output pixel dst (orig bias shifts ties by half a
 * step; ties still land on the same neighbour here, no interpolation
 * ever). -1 on num <= 0 or dst < 0. No globals. */
int gfx_nearest_src(int dst, int num, int den);

/* --- Chain 2026-10-16 (15): icon mask pipeline @ 11a8 (goal 4) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Gfx_CreateIconMaskBitmap @ 11a8:0cfd (wrapper: GLOBALLOCK icon +
 *     Gfx_BuildIconMask + GLOBALUNLOCK when handle != 0; sole caller
 *     Gfx_BlitTechIcon @ 11a8:0f56 mask branch when param_5 != -1, i.e.
 *     transparent key present; param_5 == -1 takes the direct BITBLT
 *     0x20/0xcc path without a mask).
 *   Gfx_BuildIconMask @ 11a8:0d6e (GLOBALALLOC ((w+1)/2*2 * h) 8-bit
 *     scratch + GLOBALLOCK, then per-row 16-px words: bit 1 <<
 *     (0xf - x & 0x1f) set when pixel == transparent (param_4), stored
 *     high byte then low byte (BE word), remainder row padded with 1s
 *     (transparent), then CREATEBITMAP(w?, h, 1, 1, bits) 1-bpp +
 *     UNLOCK/FREE. Callers: Gfx_CreateIconMaskBitmap above +
 *     FUN_1030_925b @ 1030:925b (PIC path).
 *   Gfx_CopyIconClipped @ 11a8:076f (transparent + offset clip: same
 *     size + zero offset -> fast w*h loop skipping transparent (default
 *     first pixel when param_5 == 0); else clip h = min(sh, dh-dy), w =
 *     min(sw, dw-dx), per-pixel skip; callers FUN_1118_0000 @ 1118:0000).
 *   Gfx_GetIconContentBounds @ 11a8:00fe (non-bg bbox: GLOBALLOCK icon,
 *     default bg = first pixel when param_3 == 0, scan min/max x/y of
 *     pixels != bg, then +1 on max (exclusive), empty (max < min) ->
 *     SETRECT 0,0,0,0; callers FUN_1068_06af @ 1068:06af +
 *     Unit_DrawStatusBadge @ 1110:0ff6).
 *   Gfx_ReplaceIconColor @ 11a8:022f (w*h byte scan old -> new; callers
 *     City_DrawTileView @ 1110:04d0 + Init_UnitFontTables @ 10e8:2d46 +
 *     Unit_DrawStatusBadge @ 1110:0ff6: civ flag recolor).
 *   Gfx_FillIconRect @ 11a8:02ad (rows [t,b) at cols [l,r) fill color;
 *     caller FUN_1118_0000 @ 1118:0000).
 * Po co 1-bpp? Win16 transparent blit needs a monochrome mask for the
 *   AND/PAINT rop trio (Gfx_BlitTechIcon mask branch: SELECT mask +
 *   SETTEXTCOLOR 0/SETBKCOLOR + BITBLT 0x46/0x66/0xc688 + restore; the
 *   -1 branch skips the mask with direct 0x20/0xcc). 1 plane × 1 bpp is
 *   the CREATEBITMAP mask shape (opaque vs transparent only, no color).
 * Transparent blit? Yes: mask bit = 1 where source == transparent key
 *   (param_4, default first pixel), 0 elsewhere; the rop trio punches
 *   the transparent pixels out of the dest before painting the icon.
 * Tło flagi civ? The replaced/filled key is the per-civ flag background
 *   (owner color cell): ReplaceIconColor swaps the flag index (civ color
 *   via GfxPort_ChangeColor @ 10d0:09ee family, see gfx_remap_index) and
 *   FillIconRect paints the badge/flag box; bbox trims the flag box to
 *   the non-bg content. Relation to gfx_indexed.c: icon bytes are INDEX8
 *   (like rsc_indexed_t pixels + gfx_godpal 0x00..0xeb, see gfx_indexed.h
 *   GFX_GODPAL_COUNT; remap itself lives in gfx_remap_index, not here).
 * OS-free geometry below is LIVE 1:1 on explicit caller buffers (no
 * GLOBALLOCK/HMEMCPY/BITBLT/CREATEBITMAP handles, no globals); device
 * tails stay STUB (gfx_port_change_color / gfx_draw_icon_clipped /
 * gfx_present_tile_rect / city_draw_tile_view). */
int gfx_icon_mask_row_bytes(int w);
int gfx_icon_transparent_match(int pixel, int transparent);
civ_error_t gfx_icon_content_bounds(const uint8_t *px, int w, int h,
                                    uint8_t bg, int *l_out, int *t_out,
                                    int *r_out, int *b_out);
civ_error_t gfx_fill_icon_rect(uint8_t *px, int w, int h,
                               int l, int t, int r, int b, uint8_t color);
civ_error_t gfx_icon_copy_clipped(const uint8_t *src, int sw, int sh,
                                   uint8_t *dst, int dw, int dh,
                                   int dx, int dy, uint8_t transparent);

/* --- Chain 2026-10-16 (16): blit ROP selectors + icon-buffer registry
 * (goals 3/4; civ.exe, x86:LE:16:Protected Mode, C17, no exit, no globals) ---
 * Anchors (decompiled this pass, disasm-verified PUSH immediates):
 *   Gfx_BlitTechIcon @ 11a8:0f56 (was FUN_11a8_0f56): color branch builds the
 *     mask via Gfx_CreateIconMaskBitmap @ 11a8:0cfd (was FUN_11a8_0cfd:
 *     GLOBALLOCK icon + Gfx_BuildIconMask + GLOBALUNLOCK; sole caller here)
 *     + Gfx_BuildIconMask @ 11a8:0d6e (was FUN_11a8_0d6e: GLOBALALLOC
 *     ((w+1)/2*2*h) scratch + per-row 16-px words, bit 1<<(0xf-x&0x1f) when
 *     pixel == transparent key, BE word high-then-low, tail rows padded
 *     with 1s, CREATEBITMAP(w?,h,1,1,bits) 1-bpp + UNLOCK/FREE; second caller
 *     FUN_1030_925b PIC path) + mask DC (CREATECOMPATIBLEDC x2) + trio:
 *     Orig PUSH 0x660046 / 0x8800C6 / 0x660046 @ 11a8:11bb/11dc/11fd (mask
 *     branch) vs direct PUSH 0xcc0020 @ 11a8:1147 (param_5 == -1 fast path)
 *     + mono path PUSH 0xcc0020 x2 @ 11a8:12eb/1375 (DAT_1420_17a6 == 1 via
 *     Gfx_AllocIconBuffer @ 11a8:0054 + Gfx_CopyIconBits @ 11a8:13c0 +
 *     GET/SETBITMAPBITS). SETTEXTCOLOR 0 + SETBKCOLOR 0xffffff frame the
 *     trio (mask select + text/bk colors + restore + DELETEOBJECT mask).
 *   Dokladne raster-opy: 0x00CC0020 = SRCCOPY (direct copy: -1 fast path +
 *     mono GET/SETBITMAPBITS path + GfxPort_PresentRect @ 10d0:0854 /
 *     GfxPort_Blit @ 10d0:08f1 family, already LIVE as gfx_port_blit /
 *     gfx_port_present_rect); 0x00660046 = SRCINVERT (mask steps 0 i 2:
 *     dest ^= src pattern passes around the AND); 0x008800C6 = SRCAND
 *     (mask step 1: dest &= mask punches the transparent pixels out before
 *     the icon paint). Decompiler split them as 0x46/0x66 + 0xc6/0x88 byte
 *     pairs — pelne ROP-y to 0x00660046 / 0x008800C6 (PUSH immediates
 *     w disasm, nie zgadywane). MERGPAINT (0x00BB0226) / SRCPAINT
 *     (0x00EE0086) nie wystepuja w tym triptyku.
 *   Zwiazek z SDL: Win16 potrzebuje 1-bpp maski bo trio AND/PAINT operuje
 *     na monochromatycznej masce (CREATEBITMAP 1 plane x 1 bpp: opaque vs
 *     transparent only, no color); SDL3 nie potrzebuje maski: transparent
 *     blit to SDL_SetSurfaceColorKey (colorkey == transparent key, default
 *     first pixel caller-side jak CopyIconClipped @ 11a8:076f) lub
 *     SDL_BLENDMODE_BLEND alpha — wiec Gfx_BuildIconMask /
 *     Gfx_CreateIconMaskBitmap device tails zostaja STUB (GLOBALALLOC +
 *     CREATEBITMAP nie maja odpowiednika), a selektory ROP ponizej sa LIVE
 *     1:1 (wybor sciezki direct-vs-mask + numer kroku tria, no Win16).
 *   Gfx_FreeAllIconBuffers @ 1008:075b (sole caller WinMain_Shutdown @
 *     1008:0329): 19 grup / 821 slotow (loop counts z dekompilatu: 42 + 8
 *     + 16 + 11 + 192(16x12) + 33 + 33?nie: 33 + 32 + 60(3x20) + 4 + 36(9x4)
 *     + 9 + 320(0x140) + 11 + 4 + 28(14x2) + 12(4x3) + 3 single = 821;
 *     adresy DAT_1420_98f4/98e4/98c4/98ae/972e/96ec/96ac/9632/962a/95e2/95d0/
 *     9350/933a/+0x128/b168/b150/b1a0/aea2/96aa). Ktore tablice: dokladny
 *     content-map TBD z dumpu (kandydaci: tech icons @ 11a8:0cb1 via
 *     *(icon*2+0x128) + unit sprites @ 1110:0dba/0ff6 + tile/flag cells @
 *     1420:9350 + port/scratch bufory; rozmiary per-slot via
 *     gfx_icon_buffer_size w*h+6, patrz Gfx_AllocIconBuffer @ 11a8:0054).
 *     Rejestr ponizej jest LIVE bez GLOBALALLOC (caller-owned int slots +
 *     counts, zero jak font_free_slots @ 1008:062f; GLOBALFREE caller-side).
 * OS-free 1:1 LIVE (no Win16 handles, no globals, read-only except outs). */
#define GFX_ROP_SRCCOPY 0x00CC0020u
#define GFX_ROP_SRCINVERT 0x00660046u
#define GFX_ROP_SRCAND 0x008800C6u

/* Orig Gfx_BlitTechIcon @ 11a8:0f56 direct path (PUSH 0xcc0020 @ 11a8:1147
 * + mono 0xcc0020 x2 @ 11a8:12eb/1375) -> gfx_blit_rop_direct. 1:1 SRCCOPY.
 * No globals. */
unsigned gfx_blit_rop_direct(void);

/* Orig same mask triptych (PUSH 0x660046 @ 11a8:11bb + 0x8800c6 @ 11a8:11dc
 * + 0x660046 @ 11a8:11fd) -> gfx_blit_rop_mask_step. 1:1 step 0 -> SRCINVERT,
 * 1 -> SRCAND, 2 -> SRCINVERT; OOB step -> 0 (orig never passes it). */
unsigned gfx_blit_rop_mask_step(int step);

/* Orig mask-vs-colorkey decision (SDL bridge) -> gfx_mask_needs_bitmap.
 * 1 on Win16 (1-bpp CREATEBITMAP trio above), 0 on SDL3 (colorkey/alpha).
 * Takes no handles; documents why Build/CreateIconMask stay STUB. */
int gfx_mask_needs_bitmap(void);

/* Orig Gfx_FreeAllIconBuffers @ 1008:075b registry shape ->
 * gfx_icon_registry_groups / group_count / total. 1:1 loop counts (19 grup,
 * 821 total j.w.; group ids 0..18 w kolejnosci dekompilatu). OOB group -> -1.
 * No globals. */
int gfx_icon_registry_groups(void);
int gfx_icon_registry_group_count(int group);
int gfx_icon_registry_total(void);

/* Orig same slot-state half (jak font_free_slots @ 1008:062f) ->
 * gfx_icon_registry_free. Zeruje caller-owned slots[0..n) + CIV_OK (zera
 * zostaja zerami jak CMP-skip w orig; GDI GLOBALFREE caller-side).
 * NULL slots -> NOENT; n < 0 lub n > total (821) -> RANGE. No globals. */
civ_error_t gfx_icon_registry_free(int *slots, int n);

/* --- Chain 2026-10-16 (17): palette init/dispose pair (goal 2) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Palette_Init @ 1090:0488 (GLOBALALLOC 0x3b4 LOGPALETTE -> DAT_1420_9964
 *     + GLOBALLOCK + *p = 0x300 (palVersion) + p[1] = 0xec (palNumEntries,
 *     godpal 236 kolorow, zob. gfx_indexed.h gfx_godpal_count/get) + 0xec
 *     RGB triples + flag byte (1 gdy DAT_1420_17a6 == 0, else 0) +
 *     CREATEPALETTE -> DAT_1420_9966 (stary 9966 kasowany DELETEOBJECT);
 *     juz LIVE-noop jako palette_init, bo SDL3 nie ma globalnej palety).
 *   Palette_Dispose @ 1090:0596 (Debug_LogTrace "FreeGodPalette()  :
 *     godpal.c" @ 1420:1e3a + GLOBALFREE DAT_1420_9964 (gdy != 0) +
 *     DELETEOBJECT DAT_1420_9966; sole caller WinMain_Shutdown @ 1008:0329,
 *     faza "dispose-palette" w boot_shutdown_step_name).
 * Co to za obiekty: 9964 = GLOBALALLOC handle na LOGPALETTE (0x3b4 B =
 *   4 naglowek + 0xec * 4 wpisy RGB+flags), 9966 = HPALETTE z
 *   CREATEPALETTE (device palette). Nie pedzel ani bitmapa (pedzle zyja
 *   w Gfx_MakeBrush @ 1098:00b5, bitmapy w GfxPort_AllocBitmap @
 *   10d0:0cfa; kasuje je Gfx_FreeDriverObjects @ 1008:066d, nie ten ogon).
 * Kto jeszcze uzywa: MainPort_ApplyPalette @ 1200:00a8 (GLOBALLOCK portu
 *   -> Palette_AnimateRange @ 1090:0376, opcode 1 full + 0xe fade, potem
 *   *(port+6) = DAT_1420_9966) + Gfx_LookupColor @ 1098:01b0 (GLOBALLOCK
 *   9964 + 4 przy wpisie (color-10)*4+4, 10 < color < 0xf6) +
 *   GfxPort_ChangeColor @ 10d0:09ee (NIE rusza palety: remapuje INDEKSY
 *   bitmapy portu old->new, zob. gfx_remap_index) + Anim_AdvanceFrames @
 *   10b0:0461 (timed 1/0xe fades) + title/HoF fades (palette_animate_range
 *   LIVE). Pelna lista xrefs 9966: 1018:00f8, 1210:02c0/04e4, 1208:0b61/
 *   0dfc/0e4d, 10e8:00f4, 1218:03dd, 1188:00b1, 1098:1140, 11a8:04fb,
 *   1200:00f8, 1098:1514, 10b8:0a49, 1210:0a54, 1090:044b, 1208:2261+,
 *   11e0:0578, 1208:107a, 1090:06fd+, 1098:2f8f, 1090:0670.
 * Port pary init/dispose: init juz LIVE-noop (palette_init), dispose
 *   ponizej LIVE-noop (GLOBALFREE/DELETEOBJECT nie maja odpowiednika —
 *   SDL3 trzyma palete per-surface/texture, zob. palette_apply_to_port);
 *   ksztalt LOGPALETTE + guard nonzero LIVE jako stale/predykat (no
 *   Win16, no globals, read-only). */

/* Orig Palette_Init @ 1090:0488 LOGPALETTE shape (godpal 0xec). */
#define CIV_PALETTE_COUNT 0xec
#define CIV_PALETTE_LOGPAL_SIZE 0x3b4
#define CIV_PALETTE_LOGPAL_VER 0x300

/* Orig Palette_Dispose @ 1090:0596 -> palette_dispose. LIVE-noop pair do
 * palette_init (void jak orig; GLOBALFREE 9964 + DELETEOBJECT 9966 to
 * Win16 handles, caller-side TODO; SDL3 nie instaluje nic globalnie).
 * No globals, no exit(). */
void palette_dispose(void);

/* Orig same guard (9964/9966 != 0 przed free) -> palette_handle_is_live.
 * 1:1: 1 gdy handle != 0 (free), 0 wpp (skip). No globals. */
int palette_handle_is_live(int handle);

/* --- Chain 2026-10-16 (22): Gfx_StoreDC pattern (goal 4) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Gfx_StoreDC @ 1098:1104 (thin wrapper: Gfx_SetCurrentDC @ 1100:09a9
 *     with the current port/DC; sole callee SetCurrentDC; 28 callers incl.
 *     Anim_AdvanceFrames @ 10b0:0461 frame-pump, Pic_PlayShared @
 *     1030:0000, Dialog_MenuZ_Modal @ 1220:0156, Dlg_CreateTracked @
 *     1098:124f, WdwMap_Open @ 1208:0af1 / WdwSmMap_Open @ 1210:025b,
 *     Game_UpdateStatusPanel @ 1088:13d5, Unit_ShowInfoPopup @ 11f0:468c,
 *     Main_RunStartupSequence @ 11e0:0000, NewGame_Wizard @ 10e8:0000,
 *     Sys_PumpMessages @ 1108:0000, WorldGen_LoadEarth @ 1108:0243).
 * Po co każdy save'uje DC:
 *   Anim frame-pump (co 8. tick): double-buffer sync (StoreDC + MemCopyFar
 *     przed blitami opkodów 5/7/8/9; patrz dialog.h anim_tick_needs_store).
 *   Pic_PlayShared / MenuZ_Modal / Dlg_CreateTracked / map-opens:
 *     tło pod modal (StoreDC przed FILLRECT/BITBLT dialogu, restore via
 *     PresentRect @ 10d0:0854 + ClearActive @ 10d0:09a2 po modalu).
 *   UpdateStatusPanel / ShowInfoPopup: clip/save przed status/popup paint
 *     (StoreDC + SetActive, restore via PresentRect + ClearActive; unlock
 *     to SELECTOBJECT-restore, dirty-clear to VALIDATERECT no-op na SDL3).
 *   Ship_ShowView @ 1070:0327: NIE używa StoreDC (SetActive + PresentRect
 *     wprost + starfield PresentRect 0xd2).
 * Czy wzorzec da się uogólnić: TAK — save (StoreDC) -> draw (Fill/Blit/
 *   Present/Text) -> restore (SetActive/PresentRect/ClearActive +
 *   SELECTOBJECT-restore + VALIDATERECT no-op). Poniżej LIVE kolejność
 *   (save-first) + tick/modal gates (no Win16, no globals); same wołania
 *   (StoreDC/push/pop) zostają STUB z TODO Orig aż mostek SDL (borrowed
 *   renderer jako DC, TARGET texture jako backing) wyląduje w
 *   gfx_port_create/set_active/present_rect/blit powyżej.
 * No exit(), C17. */

/* Orig Gfx_StoreDC @ 1098:1104 -> gfx_store_dc. Thin wrapper over
 * Gfx_SetCurrentDC @ 1100:09a9 (borrowed renderer/DC save). SDL bridge
 * TBD (renderer borrow + TARGET backing, see gfx_port_* above). */
civ_error_t gfx_store_dc(void);

/* Orig push/pop generalization (save -> draw -> restore) -> gfx_dc_push /
 * gfx_dc_pop. STUB pair keeping the pairing for RE-map fidelity (SDL3
 * renderer borrow needs no real push; the order itself is LIVE in
 * gfx_store_order_is_save_first below). */
civ_error_t gfx_dc_push(void);
civ_error_t gfx_dc_pop(void);

/* Orig frame-pump gate (same shape as anim_tick_needs_store @ 10b0:0461:
 * (tick & 7) == 0 -> StoreDC + MemCopyFar; video-owned copy so video.c
 * needs no dialog pull) -> gfx_store_tick_needs_save. 1:1. */
int gfx_store_tick_needs_save(long tick);

/* Orig modal-entry gate (StoreDC before FILLRECT/BITBLT dialogu) ->
 * gfx_store_modal_needs_save. 1:1 is_modal != 0 (modal/dialog paint
 * always saves; modeless sim tick in Pic_PlayShared mode 0 skips). */
int gfx_store_modal_needs_save(int is_modal);

/* Orig save -> draw -> restore order -> gfx_store_order_is_save_first.
 * 1:1 always 1 (save first, then draw, then PresentRect/ClearActive +
 * SELECTOBJECT-restore + VALIDATERECT no-op restore). */
int gfx_store_order_is_save_first(void);

/* --- Chain P4 render+text (2026-10-16): create/present/repaint + godpal ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), batch-decompiled this pass:
 *   GfxPort_Create @ 10d0:0000 (GR_PortOpen: Debug_LogTrace "GR PortOpen" +
 *     GETDC(0) + CREATECOMPATIBLEDC + RELEASEDC + Util_MemCopyFar rect +
 *     OFFSETRECT + port ~0x20 words + GLOBALALLOC color handle (or
 *     FUN_1098_28e4 error) + GETDEVICECAPS 0xc/0xe + GfxPort_AllocBitmap @
 *     10d0:0cfa even width ((w+1)/2*2) + SELECTOBJECT; 15+ creators incl.
 *     WinMain @ 1008:01f5, City_OpenCensusView @ 1018:0108, Pic_PlayShared
 *     @ 1030:019b/01c3, Art_LoadFrameData @ 10b8:0a59).
 *   GfxPort_PresentRect @ 10d0:0854 (BITBLT rop 0xcc0020 SRCCOPY w =
 *     right-left, h = bottom-top + VALIDATERECT no-op; already LIVE as
 *     gfx_port_present_rect -> SDL_RenderTexture above).
 *   GfxPort_ClearActive @ 10d0:09a2 (slot clear DAT_1420_17cc/17ae; already
 *     LIVE as gfx_port_slot_after_clear/gfx_port_clear_active above).
 *   GfxPort_AllocBitmap @ 10d0:0cfa (even-width CREATEBITMAP vs odd
 *     FUN_10a0_0000 path; already LIVE as gfx_port_alloc_bitmap ->
 *     SDL_CreateTexture INDEX8 TARGET above).
 *   GfxPort_Close @ 10d0:0172 (GR_PortClose: ClearActive prologue +
 *     DELETEDC + DELETEOBJECT + GLOBALFREE x2 + zero words; free tail
 *     LIVE as gfx_port_destroy below, DC/device pairing stays borrowed).
 *   GfxPort_SetActive @ 10d0:0956 (Gfx_SetCurrentDC(port+0x16) + 17cc =
 *     port always + 17ae = port unless the 12d8:44ff+0x43 dialog port;
 *     SDL3 equivalent is an SDL render-target select, LIVE below).
 *   Palette_AnimateRange @ 1090:0376 (0xec cap + RGB triples + flag byte
 *     then ANIMATEPALETTE; callers opcode 1/0xe incl. MainPort_ApplyPalette
 *     @ 1200:00a8 + Anim_AdvanceFrames @ 10b0:0461 + PushPalette @
 *     1090:015e/PopPalette @ 1090:02d2 + Zero @ 1090:0670/Set @ 1090:06fd).
 *   Palette_Init @ 1090:0488 (LOGPALETTE 0x300 + 0xec zeroed entries +
 *     CREATEPALETTE -> DAT_1420_9966; already LIVE-noop as palette_init).
 *   MainPort_ApplyPalette @ 1200:00a8 (GLOBALLOCK port -> AnimateRange +
 *     *(port+6) = DAT_1420_9966; already LIVE as palette_apply_to_port).
 *   DAT_1420_9966 @ 1420:9966 (HPALETTE handle word, BSS zero at load;
 *     read 16 B zeros in Ghidra, not a static RGB table: the 0xec RGB
 *     payload is built at runtime via AnimateRange from art/anim sources,
 *     so no static LOGPALETTE anchor 00 03 EC 00 exists in unpacked/civ.exe
 *     — gfx_godpal_get stays UNSUPPORTED with no fake bytes, range/flag
 *     math below is LIVE).
 *   Map_RepaintViewShuffled @ 1208:1866 (mode 2 direct PresentRect / 1
 *     shuffled Fisher-Yates via mem_alloc_lock + Rng_Range @ 11c8:003d /
 *     0 row-major; per-tile SETRECT 0x20 + IntersectRect clip + OOB/fog
 *     blank vs RevealCenter; geometry already LIVE in map.h
 *     (map_paint helpers, map_shuffle helpers, map_repaint_mode helpers)).
 *   Map_PaintTilesClipped @ 1208:1534 (tile-range variant of the same
 *     per-tile loop; geometry already LIVE in map.h).
 *   Map_DrawCityLabels @ 1208:1f64 (per-label rects + Text_DrawAligned;
 *     box already LIVE as map_city_label_box in map.h).
 *   FUN_1208_17da @ 1208:17da (repaint gate: *(u+0x2c) != 0 drives the
 *     Pre/Post_DrawTilesToPort pair around Map_RepaintViewShuffled; LIVE
 *     as map_repaint_needed in map.h).
 *   Text_DrawDirect @ 10d8:00e0 (mono color pick + SELECTOBJECT/MOVETO +
 *     PackPascal + DrawPascalAtOrigin; color pick LIVE as
 *     text_resolve_color in dialog.h).
 *   Text_DrawShadowed @ 10d8:02df (SelectForCoords gate + DispatchFont +
 *     shadow at (x + (h>=0x12), y+1) + main at (x,y); pos LIVE as
 *     text_shadow_pos in dialog.h, dx-only already LIVE as
 *     text_shadow_offset).
 *   Text_DrawAt @ 10d8:01fa (DrawShadowed(...,0) wrapper; STUB).
 *   Text_DrawClipped @ 10d8:01b0 (SelectForCoords + DispatchFont +
 *     DrawDirect; STUB).
 * OS-free 1:1 LIVE below (no Win16, no globals, C17); GDI device tails
 * stay STUB with TODO Orig. */

/* Orig GfxPort_Create @ 10d0:0000 malloc + bitmap bridge ->
 * gfx_port_create is now LIVE (see video.c): malloc gfx_port_t +
 * gfx_port_alloc_bitmap INDEX8 TARGET + wire ren/w/h (GETDC/
 * CREATECOMPATIBLEDC borrow the caller renderer; even width
 * ((w+1)/2*2) via gfx_port_bitmap_pitch). NULL out -> NOENT (nothing
 * stored); NULL ren -> NOENT; w/h <= 0 -> RANGE; malloc fail -> NOMEM;
 * bitmap fail -> IO (freed, *out NULL). */
void gfx_port_destroy(gfx_port_t *port);

/* Orig Palette_AnimateRange @ 1090:0376 0xec cap ->
 * palette_animate_clamp. 1:1: first < 0 || count <= 0 || first >= 0xec
 * -> 0; else min(count, 0xec - first) (decompiler: if (0xec <
 * first+count) count = 0xec - first). No globals. */
int palette_animate_clamp(int first, int count);

/* Orig same + ZeroGodPalette @ 1090:0670 / Set @ 1090:06fd flag byte ->
 * palette_flag_for_mono. 1:1 DAT_1420_17a6 == 0 ? 1 : 0 (color sets the
 * LOGPALETTE flag, mono clears; Zero always clears). No globals. */
int palette_flag_for_mono(int mono);

#endif /* CIVLIN_VIDEO_H */
