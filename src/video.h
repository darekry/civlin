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
 * Ported via SDL_SetRenderTarget round-trip on the minimal gfx_port_t
 * (see video.c: tex/w/h + borrowed ren as the DC placeholder + clip +
 * palette placeholders covering the ~0x20-word Orig struct): save
 * SDL_GetRenderTarget, SDL_SetRenderTarget(dst tex),
 * SDL_RenderTexture(src tex, src_rect -> dst_x/dst_y rect), restore
 * target. Same 0xcc0020 SRCCOPY rop family as PresentRect @ 10d0:0854;
 * VALIDATERECT is a no-op on SDL3. AD2 (doc only, no impl change):
 * blitted frames reach here only after the gfx_rle_expand_best
 * dispatcher (RLL/INV/LZSS/RAW0/2/4/8 + fail log via
 * gfx_rle_expand_best_ex, see gfx_indexed.h). Exact Orig field map still
 * TBD (src/dst HDC + HBITMAP via 10d0:0cfa + clip-region words), so ports
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
 * gfx_indexed.h for the LZW core). */
civ_error_t art_load_frame(int frame_id, gfx_port_t **out);

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

#endif /* CIVLIN_VIDEO_H */
