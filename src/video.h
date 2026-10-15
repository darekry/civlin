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

/* Palette_AnimateRange @ 1090:0376 (stub; range repaint on the active
 * 8-bit surface; exact source-lock semantics TBD, see Ghidra). Palette
 * opcode path: MainPort_ApplyPalette @ 1200:00a8 issues opcode 1 (full
 * apply) and Anim_AdvanceFrames @ 10b0:0461 issues opcodes 1/0xe (timed
 * fades); SDL_Palette equivalent on the active 8-bit surface TBD. */
civ_error_t palette_animate_range(int first, int count);

/* GfxPort_* @ 10d0:0000/0956/0854/08f1 + bitmap @ 10d0:0cfa (stubs).
 * Opaque drawing port: the original port struct holds ~0x20 words (DC +
 * compatible DC + bitmap via 10d0:0cfa + clip rect + palette refs); field
 * map not yet decoded, so the struct stays opaque until the 10d0:0cfa
 * allocator is RE'd.	SDL_Texture + SDL render-target bridge TBD. */
typedef struct gfx_port gfx_port_t;

/* Orig: FUN_10d0_0cfa @ 10d0:0cfa -> gfx_port_alloc_bitmap
 * (GfxPort_AllocBitmap; bitmap helper behind GfxPort_Create @ 10d0:0000 /
 * GR_PortOpen). Orig path: GETDC + CREATECOMPATIBLEDC + bitmap select
 * into the fresh port (dimensions from the port open request). SDL3 TBD:
 * streaming INDEX8 or target-capable RGBA texture selected into the port
 * (palette re-applied via SDL_SetPaletteColors on present). NULL out
 * reports NOENT; w/h <= 0 reports RANGE; otherwise UNSUPPORTED. */
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
 * (GfxPort_PresentRect). BITBLT rop 0xcc0020 (SRCCOPY) to the screen +
 * VALIDATERECT; SDL3 equivalent is an SDL_RenderTexture rect present.
 * NULL port/renderer reports NOENT (NULL rect = full port, kept). */
civ_error_t gfx_port_present_rect(gfx_port_t *port, SDL_Renderer *ren,
                                  const SDL_Rect *rect);

/* Orig: FUN_10d0_08f1 @ 10d0:08f1 -> gfx_port_blit (GfxPort_Blit).
 * Port-to-port BITBLT (same 0xcc0020 rop family); SDL blit TBD. NULL
 * ports report NOENT (NULL src_rect = whole source, kept). */
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

#endif /* CIVLIN_VIDEO_H */
