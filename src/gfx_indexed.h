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

#include "civ_common.h"
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

/* Orig: godpal.c 0xec RGB entries feeding FUN_1090_0488 @ 1090:0488
 * Palette_Init (LOGPALETTE 0x300 -> CREATEPALETTE -> DAT_1420_9966,
 * SELECTPALETTE/REALIZEPALETTE at each port present; ANIMATEPALETTE via
 * Palette_AnimateRange @ 1090:0376, see video.h). OS-free table port:
 * 0xec entries x 3 bytes (R,G,B, no alpha; LOGPALETTE tail past 0xec
 * stays zero/black). Byte payload not yet dumped from godpal.c /
 * DAT_1420_9966, so only the size is live (TODO dump); entry fetch stays
 * CIV_ERR_UNSUPPORTED until the dump lands. No globals, no exit(). */
#define GFX_GODPAL_COUNT 0xec

/* Returns GFX_GODPAL_COUNT (0xec). OS-free 1:1 size port. */
size_t gfx_godpal_count(void);

/* Fetches one godpal entry (index 0..0xeb) into rgb[3]. NULL rgb reports
 * NOENT; index outside 0..0xeb reports RANGE; otherwise UNSUPPORTED
 * until the godpal.c / DAT_1420_9966 byte dump lands. */
civ_error_t gfx_godpal_get(int index, uint8_t rgb[3]);

#endif /* CIVLIN_GFX_INDEXED_H */
