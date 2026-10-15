/* Title / intro stubs. Frame source TBD (NE resource vs RSC frame). */
#include "title.h"

civ_error_t title_play_intro(void)
{
    /* TODO: confirm frame player (candidates: FUN_1030_0000 anim loop,
     * FUN_1018_* callers, gr_pic.c LoadGifPicture/GR_PicRead), then
     * Sound_PlayId(3)=OPENING.wav + frame loop with title_poll_skip().
     * See title_play_anim_loop / title_decode_frame below (packet C). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t title_play_anim_loop(void)
{
    /* TODO: Orig FUN_1030_0000 @ 1030:0000 (Title_PlayAnimLoop): frame loop
     * with Sound_PlayId(CIV_SOUND_OPENING)=OPENING.wav + title_poll_skip().
     * Frame present (Win16) -> stub. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t title_decode_frame(const uint8_t *src, size_t len)
{
    (void)src;
    (void)len;
    /* TODO: Orig FUN_1018:xxxx wildcard (Title_DecodeFrame): frame decoder
     * candidate; exact offset TBD. Same 1018 segment as the 1018:015d
     * caller of game_append_year_text @ 1088:1dd0 (see game.h chain).
     * Win16/GDI path -> stub. */
    return CIV_ERR_UNSUPPORTED;
}

int title_poll_skip(void)
{
    /* TODO: SDL3 SDL_PollEvent for keydown/mousedown (no window owned here;
     * caller pumps). Return 0 until wired. */
    return 0;
}
