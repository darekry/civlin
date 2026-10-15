/* CivLin indexed+palette -> SDL3 bridge. See gfx_indexed.h. */
#include "gfx_indexed.h"

#include <string.h>

SDL_Surface *gfx_indexed_to_surface(const rsc_indexed_t *img)
{
    /* Strict 8-bit + palette gate for the packet H rsc_get_indexed plan
     * (see gfx_indexed.h): rejects non-8-bit, empty, or pixel-less views
     * before touching SDL, so a future ResMgr producer cannot smuggle a
     * malformed slice into an INDEX8 surface. */
    SDL_Surface *surf = NULL;
    SDL_Palette *pal = NULL;
    SDL_Color colors[256];

    if (!img || !img->pixels || img->width == 0 || img->height == 0 ||
        img->bpp != 8)
        return NULL;

    surf = SDL_CreateSurface((int)img->width, (int)img->height,
                             SDL_PIXELFORMAT_INDEX8);
    if (!surf)
        return NULL;

    if (img->pal_count > 0) {
        pal = SDL_CreateSurfacePalette(surf);
        if (!pal) {
            SDL_DestroySurface(surf);
            return NULL;
        }
        for (int i = 0; i < img->pal_count; i++) {
            colors[i].r = img->pal[i][0];
            colors[i].g = img->pal[i][1];
            colors[i].b = img->pal[i][2];
            colors[i].a = 255;
        }
        if (!SDL_SetPaletteColors(pal, colors, 0, (int)img->pal_count)) {
            SDL_DestroySurface(surf);
            return NULL;
        }
    }

    /* Direct pixel access: lock only if the surface requires it
     * (SDL_MUSTLOCK). INDEX8 software surfaces typically do not. */
    if (SDL_MUSTLOCK(surf) && !SDL_LockSurface(surf)) {
        SDL_DestroySurface(surf);
        return NULL;
    }
    {
        uint8_t *dst = (uint8_t *)surf->pixels;
        int pitch = surf->pitch;
        for (uint32_t y = 0; y < img->height; y++)
            memcpy(dst + (size_t)y * (size_t)pitch,
                   img->pixels + (size_t)y * (size_t)img->width,
                   img->width);
    }
    if (SDL_MUSTLOCK(surf))
        SDL_UnlockSurface(surf);
    return surf;
}

SDL_Texture *gfx_indexed_to_texture(SDL_Renderer *ren, const rsc_indexed_t *img)
{
    SDL_Surface *surf = NULL;
    SDL_Texture *tex = NULL;

    if (!ren)
        return NULL;
    surf = gfx_indexed_to_surface(img);
    if (!surf)
        return NULL;
    tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_DestroySurface(surf);
    return tex;
}
