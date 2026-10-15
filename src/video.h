/* Video: GDI (BitBlt/SelectObject/CreateBitmap/CreatePalette,
 * SelectPalette/RealizePalette/AnimatePalette) -> SDL3.
 * Palette fidelity via gfx_indexed (8-bit surface + SDL_Palette).
 */
#ifndef CIVLIN_VIDEO_H
#define CIVLIN_VIDEO_H

#include <SDL3/SDL.h>

#include "civ_common.h"

civ_error_t video_init(SDL_Window *win, SDL_Renderer *ren);
civ_error_t video_blit_indexed(SDL_Renderer *ren, SDL_Texture *tex, int x, int y);
civ_error_t video_present(SDL_Renderer *ren);
void video_shutdown(void);

#endif /* CIVLIN_VIDEO_H */
