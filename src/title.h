/* Title / intro movie -> SDL3.
 *
 * Ghidra anchors (civ.exe):
 *   Sound_PlayId      @ 1150:00c9  (SI=3/4 -> 0x83 OPENING.wav; see audio.h)
 *   Sound_MapIdToWav  @ 1150:019e  (jump table @ 1150:0266)
 *   Sound_MciOpen     @ 1150:001c / Sound_MciClose @ 1150:0066 (MCI 0x803/0x804)
 *   GR_PicRead / LoadGifPicture / PicDecompress : gr_pic.c (1420:2300/2372/23ce)
 *     — candidate frame player for the intro sequence (NOT yet confirmed
 *     which caller drives the pre-menu animation).
 *   Title_PlayAnimLoop @ 1030:0000 (Orig FUN_1030_0000; anim-loop candidate
 *     from title.c:6; drives frames + Sound_PlayId OPENING)
 *   Title_DecodeFrame @ 1018:xxxx (Orig FUN_1018:xxxx wildcard; frame decoder
 *     candidate; same 1018 segment as the 1018:015d caller of
 *     game_append_year_text @ 1088:1dd0 -- see game.h 5-caller chain:
 *     status panel, 1018:015d, 1138:0000, 11b0:0000, 11b8:14e5)
 *   Map_GenerateRandomOrLoad @ 1028:0000 plays Sound_PlayId(4)=OPENING at
 *     map-gen start — related but distinct from the pre-menu intro.
 *
 * Asset notes: OPENING.wav confirmed at 1420:3114. MPLOGO.BMP has NO filename
 * string in the binary — likely an NE resource or an RSC frame, NOT a file
 * load. Dumping NE resources of unpacked/civ.exe is the next step before
 * implementing frame playback here.
 */
#ifndef CIVLIN_TITLE_H
#define CIVLIN_TITLE_H

#include <stddef.h>
#include <stdint.h>

#include <SDL3/SDL.h>

#include "civ_common.h"

/* Plays the pre-menu intro (OPENING.wav + frames). Skippable on input in
 * the original. UNSUPPORTED until the frame source (NE resource vs RSC) is
 * confirmed. */
civ_error_t title_play_intro(void);

/* Orig: FUN_1030_0000 @ 1030:0000 -> title_play_anim_loop (Title_PlayAnimLoop).
 * CORRECTION 2026-10-15 (verified: size 0x79cd ~31 KB, 13 callers incl.
 * City_ShowTopFive @ 1080:099b, Unit_DoMove @ 1038:0000, City_ProcessDisaster
 * @ 1040:068e, Game score @ 1040:1137 tails, 1170/1188/11b8 families):
 * 1030:0000 is the shared PIC/modal player (Dlg_CreateTracked @ 1098:124f
 * + FILLRECT/FRAMERECT + internal 1030:79ce/7c28/... helpers), NOT the
 * pre-menu intro loop. CONFIRMED 2026-10-16: Ghidra name is Pic_PlayShared
 * @ 1030:0000 with params (city_idx, mode) — mode 1 opens the modal city
 * view, mode 0 runs the background sim tick (incl. the caravan prod-0x1b
 * Trade_DeliverCaravanGoods @ 1038:0d09 call); 13-caller fan-out re-verified
 * (City_DoRevolt @ 11b8:0f26, City_ProcessDisaster @ 1040:068e,
 * City_ShowCensusReport @ 1018:1d5b, City_ShowTopFive @ 1080:099b,
 * Demo_ShowDemographics @ 1188:123a, 1018:0be0, 1018:208f, 1040:1137,
 * 1170:0000, 1170:05af, 1170:05ca, 1188:0db8, Unit_DoMove @ 1038:0000);
 * no Art_LoadFrameData @ 10b8:079d frame path inside (city GDI tiles, not
 * RSC frames). Linux owner is pic_play_shared (see game.h/game.c); this
 * stub keeps the intro shape (frames + Sound_PlayId(CIV_SOUND_OPENING) +
 * title_poll_skip()) until the true intro entry is isolated. Win16 frame
 * present -> stub CIV_ERR_UNSUPPORTED. */
civ_error_t title_play_anim_loop(void);

/* Orig: FUN_1018_xxxx @ 1018:xxxx -> title_decode_frame (Title_DecodeFrame).
 * Frame decoder candidate (wildcard offset: exact FUN_1018 entry TBD).
 * Same 1018 segment as the 1018:015d caller of game_append_year_text @
 * 1088:1dd0 (chain: status panel, 1018:015d, 1138:0000, 11b0:0000,
 * 11b8:14e5 -- see game.h); decode context to be confirmed against that
 * caller chain. Win16/GDI blit path -> stub CIV_ERR_UNSUPPORTED. */
civ_error_t title_decode_frame(const uint8_t *src, size_t len);

/* Orig: FUN_1010_xxxx @ 1010:xxxx -> pic_decompress_chunk
 * (Pic_DecompressChunk). Sibling of gr_pic.c PicDecompress @ 1420:23ce:
 * OS-free chunk decoder (LZ/RLE byte math, save_rll_* shape). Bounds-checked
 * src->dst, no Win16 until the blit (caller owns the GfxPort blit, see
 * video.h). Tag v1 (matches save_pack @ 10f8:05da / save_unpack @ 10f8:040a):
 * <0x80 = repeat next byte N times, >=0x80 = copy N-0x80 literals. Exact
 * tag/bias TBD vs 1010 decompile; blit path stays in the caller. */
civ_error_t pic_decompress_chunk(const uint8_t *src, size_t src_len,
                                 uint8_t *dst, size_t dst_cap,
                                 size_t *out_len);

/* Orig: FUN_1020_xxxx @ 1020:xxxx -> pic_read_header (Pic_ReadHeader).
 * Sibling of GR_PicRead @ 1420:2300 / LoadGifPicture @ 1420:2372: header
 * probe for an intro frame (dims/format). Stub: needs _LOPEN/_HREAD chunked
 * file path (cf. save_file_read @ 1140:019d) + FINDRESOURCE/LOADRESOURCE
 * handle path (cf. rsc_loader.h) + GfxPort select (cf. video.h
 * gfx_port_set_active @ 10d0:0956). Returns CIV_ERR_UNSUPPORTED. */
civ_error_t pic_read_header(const uint8_t *src, size_t len,
                            unsigned *out_w, unsigned *out_h);

/* Orig: FUN_1010_xxxx @ 1010:xxxx / FUN_1020_xxxx @ 1020:xxxx spare ->
 * title_load_opening_frame (Title_LoadOpeningFrame). Frame fetch feeding
 * Title_DecodeFrame @ 1018:xxxx + Title_PlayAnimLoop @ 1030:0000 +
 * Sound_PlayId(CIV_SOUND_OPENING)=OPENING.wav (see audio.h). Stub: needs
 * RSC/NE-resource frame source (RSC entry vs NE bitmap, cf. rsc_loader.h
 * ResMgr_LoadRsc @ 1190:0000) + GfxPort_SetActive @ 10d0:0956 select
 * (cf. video.h). Returns CIV_ERR_UNSUPPORTED. */
civ_error_t title_load_opening_frame(unsigned frame_index);

/* Orig Title_PlayAnimLoop @ 1030:0000 skip poll (key/mouse/quit each frame;
 * shared PIC/modal player Pic_PlayShared @ 1030:0000 shape, see above) ->
 * title_event_is_skip. OS-free 1:1 predicate over the SDL3 event type:
 * QUIT / KEY_DOWN / MOUSE_BUTTON_DOWN -> 1 (skip), else 0. No SDL calls,
 * no globals, no exit(), C17. */
int title_event_is_skip(uint32_t ev_type);

/* 1 == user asked to skip (key/mouse), 0 == keep playing. Pure SDL3 event
 * poll helper used by the intro and the victory ceremony (drains via
 * title_event_is_skip above). */
int title_poll_skip(void);

/* SDL3 intro player (Title_PlayAnimLoop @ 1030:0000 shape): MPLOGO.BMP
 * frame (civ1win/MPLOGO.BMP, read-only; procedural emblem when missing)
 * + Sound_PlayId(CIV_SOUND_OPENING)=OPENING.wav (see audio.h; silent when
 * missing) + title_poll_skip(). Fade-in + float + sweep + starfield;
 * ends on skip, on WAV drain, or on timeout (CIVLIN_INTRO_SECONDS env
 * caps, default 20 s / 6 s silent). CIV_OK on end/skip, NOENT on NULL
 * win/ren. No exit(). */
civ_error_t title_play_intro_sdl(SDL_Window *win, SDL_Renderer *ren);

/* Orig: none (AF1 Linux-port helper; MPLOGO.BMP has NO filename string
 * in civ.exe — NE resource or RSC frame, NOT a file load — so there is
 * no Orig file-load address; the Linux port passes the path explicitly)
 * -> title_mplogo_show (Title_MplogoShow). Loads the 1-bpp BMP at path
 * (unpacked/MPLOGO.BMP or civ1win/MPLOGO.BMP, read-only, never writes)
 * via gfx_bmp_load_indexed (gfx_indexed.h: gfx_bmp_parse file header
 * 42 4D "BM" + DIB 40 + w/h/planes/bpp/compression/palette/pixels +
 * gfx_bmp_to_indexed bit unpack with 4 B row padding), bridges the
 * rsc_indexed_t via gfx_indexed_to_texture (SDL only here, never in the
 * parse funcs), renders one centered frame + present. Single frame (the
 * caller owns the anim loop / title_poll_skip; cf.
 * title_play_intro_sdl). Returns CIV_OK on the presented frame;
 * CIV_ERR_NOENT on NULL win/ren/path; otherwise the loader/bridge code
 * passthrough (IO/NOMEM/BAD_DATA/RANGE/UNSUPPORTED/TRUNCATED). No
 * exit(), no globals, C17. */
civ_error_t title_mplogo_show(SDL_Window *win, SDL_Renderer *ren,
                              const char *path);

#endif /* CIVLIN_TITLE_H */
