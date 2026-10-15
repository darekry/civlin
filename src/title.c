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

civ_error_t pic_decompress_chunk(const uint8_t *src, size_t src_len,
                                 uint8_t *dst, size_t dst_cap,
                                 size_t *out_len)
{
    /* Orig FUN_1010_xxxx @ 1010:xxxx (Pic_DecompressChunk), sibling of
     * gr_pic.c PicDecompress @ 1420:23ce. OS-free 1:1 v1 (no Win16, no
     * globals, read-only): RLL chunk decode with the save_pack @ 10f8:05da
     * tag shape (<0x80 repeat / 0x80|n literals, 0x7f chunk cap implied).
     * Exact tag/bias TBD vs 1010 decompile; blit stays in the caller. */
    size_t ip = 0;
    size_t op = 0;

    if (out_len)
        *out_len = 0;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    while (ip < src_len) {
        uint8_t tag = src[ip++];
        if (tag < 0x80u) {
            uint8_t v;

            if (ip >= src_len)
                return CIV_ERR_TRUNCATED;
            if (op + tag > dst_cap)
                return CIV_ERR_TRUNCATED;
            v = src[ip++];
            for (unsigned k = 0; k < tag; k++)
                dst[op++] = v;
        } else {
            unsigned n = (unsigned)(tag - 0x80u);

            if (ip + n > src_len || op + n > dst_cap)
                return CIV_ERR_TRUNCATED;
            for (unsigned k = 0; k < n; k++)
                dst[op++] = src[ip++];
        }
    }
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

civ_error_t pic_read_header(const uint8_t *src, size_t len,
                            unsigned *out_w, unsigned *out_h)
{
    (void)src;
    (void)len;
    if (!out_w || !out_h)
        return CIV_ERR_NOENT;
    *out_w = 0;
    *out_h = 0;
    /* TODO: Orig FUN_1020_xxxx @ 1020:xxxx (Pic_ReadHeader), sibling of
     * GR_PicRead @ 1420:2300 / LoadGifPicture @ 1420:2372. Needs _LOPEN /
     * _HREAD chunked file path (cf. save_file_read @ 1140:019d) +
     * FINDRESOURCE/LOADRESOURCE handle path (cf. rsc_loader.h) +
     * GfxPort select (gfx_port_set_active @ 10d0:0956, see video.h).
     * Header layout TBD from 1020 decompile. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t title_load_opening_frame(unsigned frame_index)
{
    (void)frame_index;
    /* TODO: Orig FUN_1010_xxxx @ 1010:xxxx / FUN_1020_xxxx @ 1020:xxxx
     * spare (Title_LoadOpeningFrame): frame fetch feeding Title_DecodeFrame
     * @ 1018:xxxx + Title_PlayAnimLoop @ 1030:0000 + Sound_PlayId
     * (CIV_SOUND_OPENING)=OPENING.wav (see audio.h). Needs RSC/NE-resource
     * frame source (cf. rsc_loader.h ResMgr_LoadRsc @ 1190:0000) +
     * GfxPort_SetActive @ 10d0:0956 select (see video.h). MPLOGO.BMP has
     * no filename string (NE resource or RSC frame, see title.h). */
    return CIV_ERR_UNSUPPORTED;
}

int title_poll_skip(void)
{
    /* TODO: SDL3 SDL_PollEvent for keydown/mousedown (no window owned here;
     * caller pumps). Return 0 until wired. */
    return 0;
}
