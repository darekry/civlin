/* Title / intro stubs. Frame source TBD (NE resource vs RSC frame). */
#include "title.h"

civ_error_t title_play_intro(void)
{
    /* TODO: confirm frame player (candidates: FUN_1030_0000 anim loop,
     * FUN_1018_* callers, gr_pic.c LoadGifPicture/GR_PicRead), then
     * Sound_PlayId(3)=OPENING.wav + frame loop with title_poll_skip(). */
    return CIV_ERR_UNSUPPORTED;
}

int title_poll_skip(void)
{
    /* TODO: SDL3 SDL_PollEvent for keydown/mousedown (no window owned here;
     * caller pumps). Return 0 until wired. */
    return 0;
}
