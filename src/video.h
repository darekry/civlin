/* Video: GDI (BitBlt/SelectObject/CreateBitmap/CreatePalette,
 * SelectPalette/RealizePalette/AnimatePalette) -> SDL3.
 * Palette fidelity via gfx_indexed (8-bit surface + SDL_Palette).
 *
 * Ghidra anchors (civ.exe), renamed 2026-10-15:
 *   Palette_Init         @ 1090:0488 (CREATEPALETTE, godpal.c; see boot.h)
 *   Palette_AnimateRange @ 1090:0376 (copies RGB triples + flag byte, then
 *                            ANIMATEPALETTE; called from MainPort_ApplyPalette
 *                            @ 1200:00a8, Anim_AdvanceFrames @ 10b0:0461
 *                            opcodes 1/0xe, title fades, HoF screens)
 */
#ifndef CIVLIN_VIDEO_H
#define CIVLIN_VIDEO_H

#include <SDL3/SDL.h>

#include "civ_common.h"

civ_error_t video_init(SDL_Window *win, SDL_Renderer *ren);
civ_error_t video_blit_indexed(SDL_Renderer *ren, SDL_Texture *tex, int x, int y);
civ_error_t video_present(SDL_Renderer *ren);
void video_shutdown(void);

/* Palette_AnimateRange @ 1090:0376 (stub; range repaint on the active
 * 8-bit surface; exact source-lock semantics TBD, see Ghidra). */
civ_error_t palette_animate_range(int first, int count);

#endif /* CIVLIN_VIDEO_H */
