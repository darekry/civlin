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
    /* TODO: Palette_AnimateRange @ 1090:0376 (RGB triples + ANIMATEPALETTE;
     * SDL_Palette equivalent on the active 8-bit surface). */
    return CIV_ERR_UNSUPPORTED;
}
