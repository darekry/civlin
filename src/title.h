/* Title / intro movie -> SDL3.
 *
 * Ghidra anchors (civ.exe):
 *   Sound_PlayId      @ 1150:00c9  (SI=3/4 -> 0x83 OPENING.wav; see audio.h)
 *   Sound_MapIdToWav  @ 1150:019e  (jump table @ 1150:0266)
 *   Sound_MciOpen     @ 1150:001c / Sound_MciClose @ 1150:0066 (MCI 0x803/0x804)
 *   GR_PicRead / LoadGifPicture / PicDecompress : gr_pic.c (1420:2300/2372/23ce)
 *     — candidate frame player for the intro sequence (NOT yet confirmed
 *     which caller drives the pre-menu animation).
 *   Pic_PlayShared @ 1030:0000 (Orig FUN_1030_0000; shared PIC/modal player,
 *     NOT the pre-menu intro loop — see title_play_anim_loop below; Linux
 *     owner is pic_play_shared in game.h/game.c)
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

/* Orig Main_RunStartupSequence @ 11e0:0000 loop#1 intro host ->
 * title_play_intro. PARTIAL dispatcher: OS-free gates LIVE 1:1 (ref
 * unpacked/civ.exe: title_dialog_is_intro 0x94 + title_seq_is_earth 0x91e +
 * title_intro_should_continue adv==0 + Dlg_CreateTracked @ 1098:124f
 * (0x94,0x41) preset/ctx/pal/ctl gates + Dialog_MenuZ_Modal @ 1220:0156
 * timer/cancel/id gates + Anim_AdvanceFrames @ 10b0:0461 opcode 1..0xe map
 * for the title_play_intro_sdl SDL phases below); the Win16 modal tails
 * (Dlg_CreateTracked/DestroyTracked host + Gfx_StoreDC @ 1098:1104 +
 * Anim_OpenSequence @ 10b0:008a / Anim_CloseSequence @ 10b0:0287 +
 * Anim_RebaseClock @ 10b0:005b owners in dialog.c/video.c + Title_DecodeFrame
 * @ 1018:xxxx frame path) stay TODO -> CIV_ERR_UNSUPPORTED. No globals, no
 * exit(), read-only. */
civ_error_t title_play_intro(void);

/* Orig: FUN_1030_0000 @ 1030:0000 -> Pic_PlayShared (NOT Title_PlayAnimLoop).
 * MAP CORRECTION (verified: size 0x79cd ~31 KB, 16 wolajacych per task;
 * 13 verified incl. City_ShowTopFive @ 1080:099b, Unit_DoMove @ 1038:0000,
 * City_ProcessDisaster @ 1040:068e, Game score @ 1040:1137 tails,
 * 1170/1188/11b8 families): 1030:0000 is the shared PIC/modal player
 * (Dlg_CreateTracked @ 1098:124f + FILLRECT/FRAMERECT + internal
 * 1030:79ce/7c28/... helpers), NOT the pre-menu intro loop. CONFIRMED
 * 2026-10-16: Ghidra name is Pic_PlayShared @ 1030:0000 with params
 * (city_idx, mode) — mode 1 opens the modal city view (Dlg_CreateTracked @
 * 1098:124f dialog 0x94 + Gfx_StoreDC @ 1098:1104 save first), mode 0 runs
 * the background sim tick (incl. the caravan prod-0x1b
 * Trade_DeliverCaravanGoods @ 1038:0d09 call); 13-caller fan-out
 * re-verified (City_DoRevolt @ 11b8:0f26, City_ProcessDisaster @ 1040:068e,
 * City_ShowCensusReport @ 1018:1d5b, City_ShowTopFive @ 1080:099b,
 * Demo_ShowDemographics @ 1188:123a, 1018:0be0, 1018:208f, 1040:1137,
 * 1170:0000, 1170:05af, 1170:05ca, 1188:0db8, Unit_DoMove @ 1038:0000);
 * no Art_LoadFrameData @ 10b8:079d frame path inside (city GDI tiles, not
 * RSC frames). Linux owner is pic_play_shared (see game.h/game.c); the
 * mode split itself is LIVE there (pic_mode_is_modal/pic_mode_is_sim).
 * This title_play_anim_loop symbol keeps the intro shape (frames +
 * Sound_PlayId(CIV_SOUND_OPENING) + title_poll_skip()) until the true
 * intro entry is isolated — it is NOT the Orig 1030:0000 mapping. PARTIAL:
 * OS-free intro gates below are LIVE 1:1 (title_dialog_is_intro 0x94 +
 * title_seq_is_earth 0x91e + title_intro_should_continue adv==0, ref
 * unpacked/civ.exe Main_RunStartupSequence @ 11e0:0000 + Anim_OpenSequence
 * @ 10b0:008a / Anim_AdvanceFrames @ 10b0:0461); the Win16 modal tails
 * (Dlg_CreateTracked @ 1098:124f / Dlg_DestroyTracked @ 1098:1612 +
 * Gfx_StoreDC @ 1098:1104 + Anim_CloseSequence @ 10b0:0287 +
 * Anim_RebaseClock @ 10b0:005b owners in dialog.c/video.c) stay TODO ->
 * CIV_ERR_UNSUPPORTED. */
civ_error_t title_play_anim_loop(void);

/* Orig: FUN_1018_xxxx @ 1018:xxxx -> title_decode_frame (Title_DecodeFrame).
 * Frame decoder candidate (wildcard offset: exact FUN_1018 entry TBD).
 * Same 1018 segment as the 1018:015d caller of game_append_year_text @
 * 1088:1dd0 (chain: status panel, 1018:015d, 1138:0000, 11b0:0000,
 * 11b8:14e5 -- see game.h); decode context to be confirmed against that
 * caller chain. Faithful bitmap path (STUB until the 1198 compare lands):
 * rsc_get_res_indexed @ 1190:0a29 (Nth res) + rsc_record_data slice (BE) +
 * rsc_decode_bitmap @ 1198:xxxx strict gate (w/h>0, bpp==8, pal 1..255,
 * px<pal; mirror gfx_decode_raw/probe) + gfx_decode_rle expand. Ref
 * unpacked/civ.exe + rsc_dump: dir 454/14/31/144/1 slices OK,
 * images_strict=0, CIVDATA1 CvPc BB DF len 86857 best NONE — no codec
 * proven, no fake decode. Win16/GDI blit path -> stub CIV_ERR_UNSUPPORTED,
 * NOENT on NULL src, TRUNCATED on len==0. TODO: Orig FUN_1198_xxxx compare
 * vs the 1198 RLE/tagged-frame decoder (ref unpacked/civ.exe). */
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
 * Title_DecodeFrame @ 1018:xxxx + Main_RunStartupSequence @ 11e0:0000
 * loop#1 (Anim pump; Pic_PlayShared @ 1030:0000 is separate) +
 * Sound_PlayId(CIV_SOUND_OPENING)=OPENING.wav (see audio.h). Stub: needs
 * RSC/NE-resource frame source (RSC entry vs NE bitmap, cf. rsc_loader.h
 * ResMgr_LoadRsc @ 1190:0000) + GfxPort_SetActive @ 10d0:0956 select
 * (cf. video.h). Returns CIV_ERR_UNSUPPORTED. */
civ_error_t title_load_opening_frame(unsigned frame_index);

/* Orig Main_RunStartupSequence @ 11e0:0000 intro-loop skip poll (key/mouse/
 * quit each Anim_AdvanceFrames @ 10b0:0461 tick; same per-frame shape as
 * the Pic_PlayShared @ 1030:0000 modal pump, see above) ->
 * title_event_is_skip. OS-free 1:1 predicate over the SDL3 event type:
 * QUIT / KEY_DOWN / MOUSE_BUTTON_DOWN -> 1 (skip), else 0. No SDL calls,
 * no globals, no exit(), C17. */
int title_event_is_skip(uint32_t ev_type);

/* 1 == user asked to skip (key/mouse), 0 == keep playing. Pure SDL3 event
 * poll helper used by the intro and the victory ceremony (drains via
 * title_event_is_skip above). */
int title_poll_skip(void);

/* SDL3 intro player (Main_RunStartupSequence @ 11e0:0000 loop#1 shape:
 * Dlg_CreateTracked(0x94,0x41) host + Anim_OpenSequence @ 10b0:008a +
 * Anim_AdvanceFrames @ 10b0:0461 pump + Anim_CloseSequence @ 10b0:0287;
 * distinct from the Pic_PlayShared @ 1030:0000 modal shape): MPLOGO.BMP
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

/* --- Chain 2026-10-16 (28): intro predicates + loop gate (goal 5) ---
 * Orig Main_RunStartupSequence @ 11e0:0000 intro side (civ.exe,
 * x86:LE:16:Protected Mode), decompiled this pass (ref unpacked/civ.exe):
 *   loop#1 Dlg_CreateTracked(0x94,0x41) host + Anim_OpenSequence @ 10b0:008a
 *   + Sound_PlayId(3,0) + loop Anim_AdvanceFrames @ 10b0:0461 while(c == 0)
 *   + Close + Sound(1,0); loop#2 seq 0x91e + Anim_RebaseClock @ 10b0:005b
 *   (1000) + WorldGen_PrepareEarth @ 1108:01bc + Advance (until DAT_13e0_001e)
 *   + Close + Sound(1,0). IDs owned by boot.h (main_startup_intro_dialog_id
 *   0x94 / flags 0x41, earth_seq_id 0x91e, rebase_ticks 1000); predicates
 *   below are the title-owned (intro player) LIVE 1:1 halves on explicit
 *   params (no Win16, no globals, read-only). Win16 frame present/player
 *   tails stay in title_play_intro / title_play_anim_loop / title_decode_
 *   frame / title_load_opening_frame / pic_read_header STUBs below with
 *   TODO Orig. No exit(), C17. */

/* Orig same loop#1 host test (Dlg_CreateTracked param_1 == 0x94) ->
 * title_dialog_is_intro. 1:1 nonzero (1) when dlg_id == 0x94 (earth-custom
 * 0x8f / wizard 0x90 / menu 0x8b are not intro). */
int title_dialog_is_intro(int dlg_id);

/* Orig same loop#2 seq test (Anim_OpenSequence seq == 0x91e) ->
 * title_seq_is_earth. 1:1 nonzero (1) when seq_id == 0x91e (sequence id,
 * NOT a dialog id). */
int title_seq_is_earth(int seq_id);

/* Orig same loop#1/loop#2 pump gate (do { c = Anim_AdvanceFrames } while
 * (c == 0); loop#2 also breaks on DAT_13e0_001e via main_startup_should_
 * break in boot.h) -> title_intro_should_continue. 1:1 nonzero (1) when
 * adv_ret == 0 (busy, keep pumping); 0 when done. */
int title_intro_should_continue(int adv_ret);

#endif /* CIVLIN_TITLE_H */
