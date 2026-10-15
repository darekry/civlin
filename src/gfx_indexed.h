/* CivLin indexed+palette -> SDL3 bridge (in-memory only).
 *
 * Palette fidelity is the whole point of this file: source pixels stay
 * indexed and the palette travels with them, so later palette animation
 * (Win16 SELECTPALETTE/ANIMATEPALETTE equivalents) remains possible.
 * Expansion to 32-bit RGBA happens only in RAM at load time; nothing is
 * ever written back to civ1win/ or unpacked/ (or anywhere on disk).
 *
 * Two paths:
 *   1. gfx_indexed_to_surface(): 8-bit SDL_Surface + SDL_Palette. Keeps
 *      indices intact on the surface; re-palettizing later = animation.
 *   2. gfx_indexed_to_texture(): convenience for direct rendering; expands
 *      through the palette into an RGBA texture in VRAM/RAM, indices kept
 *      in the source rsc_indexed_t for future re-uploads.
 */
#ifndef CIVLIN_GFX_INDEXED_H
#define CIVLIN_GFX_INDEXED_H

#include <SDL3/SDL.h>

#include "rsc_loader.h"

/* Packet H note: future rsc_get_indexed @ 1190:xxxx (ResMgr image payload
 * via ResMgr_LoadToMem @ 1190:0fda + GfxPort bitmap @ 10d0:0cfa chain)
 * feeds this bridge. Strict validation plan (enforced here AND at the
 * rsc_get_indexed producer): 8-bit only (bpp == 8), width > 0, height > 0,
 * pixels != NULL, width*height bytes addressable, pal_count 0..255
 * (0 = no palette attached), every pixel index < pal_count when a palette
 * is present. rsc_image_count stays 0 / rsc_get_indexed stays
 * RSC_ERR_UNSUPPORTED until that producer chain is proven against the 5
 * CivData files. */
SDL_Surface *gfx_indexed_to_surface(const rsc_indexed_t *img);
SDL_Texture *gfx_indexed_to_texture(SDL_Renderer *ren, const rsc_indexed_t *img);

#endif /* CIVLIN_GFX_INDEXED_H */
