/* Title / intro: SDL3 MPLOGO.BMP + OPENING.wav player.
 *
 * Orig anchors in title.h (Pic_PlayShared @ 1030:0000 = Orig FUN_1030_0000,
 * shared modal player, NOT the intro loop; Main_RunStartupSequence @
 * 11e0:0000 loop#1 = the true intro driver via Anim_OpenSequence @
 * 10b0:008a / Anim_AdvanceFrames @ 10b0:0461 / Anim_CloseSequence @
 * 10b0:0287 + Dlg_CreateTracked @ 1098:124f + Gfx_StoreDC @ 1098:1104;
 * Title_DecodeFrame @ 1018:xxxx, gr_pic.c LoadGifPicture/GR_PicRead @
 * 1420:2300/2372/23ce, Sound_PlayId @ 1150:00c9 SI=3 -> 0x83 OPENING.wav).
 * Frame source note: MPLOGO.BMP has NO filename string in the binary (NE
 * resource or RSC frame); until the NE/RSC dump lands, the Linux port
 * loads civ1win/MPLOGO.BMP directly (read-only) as the intro frame.
 * No exit(), read-only vs civ1win/.
 */
#include "title.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "audio.h"
#include "gfx_indexed.h"
#include "sys.h"

civ_error_t title_play_intro(void)
{
    /* TODO: confirm frame player (Main_RunStartupSequence @ 11e0:0000 loop#1:
     * Dlg_CreateTracked(0x94,0x41) @ 1098:124f host + Anim_OpenSequence @
     * 10b0:008a + Anim_AdvanceFrames @ 10b0:0461 pump + Anim_CloseSequence @
     * 10b0:0287 + Gfx_StoreDC @ 1098:1104 save-first; Pic_PlayShared @
     * 1030:0000 is the separate shared modal player, NOT this intro —
     * see title_play_anim_loop above; FUN_1018_* callers / gr_pic.c
     * LoadGifPicture/GR_PicRead are frame-decode candidates), then
     * Sound_PlayId(3)=OPENING.wav + frame loop with title_poll_skip().
     * See title_play_anim_loop / title_decode_frame below (packet C).
     * SDL3 path is title_play_intro_sdl() below (needs win/ren, so the
     * arg-less RE-map entry stays a stub). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t title_play_anim_loop(void)
{
    /* Orig FUN_1030_0000 @ 1030:0000 = Pic_PlayShared (shared PIC/modal
     * player, NOT the intro loop — see title.h map correction; Linux owner
     * pic_play_shared in game.h/game.c with the LIVE mode split in
     * pic_mode_is_modal/pic_mode_is_sim). PARTIAL: OS-free intro gates
     * below are LIVE 1:1 (ref unpacked/civ.exe Main_RunStartupSequence @
     * 11e0:0000 + Anim_OpenSequence @ 10b0:008a / Anim_AdvanceFrames @
     * 10b0:0461 decompiles); the Win16 modal tails stay stubbed.
     * TODO: Orig Pic_PlayShared @ 1030:0000 modal tails (Dlg_CreateTracked
     * @ 1098:124f dialog 0x94 + Dlg_DestroyTracked @ 1098:1612 +
     * Gfx_StoreDC @ 1098:1104 save-first + Anim_CloseSequence @ 10b0:0287
     * + Anim_RebaseClock @ 10b0:005b owners in dialog.c/video.c + GDI
     * FILLRECT/FRAMERECT + 1030:79ce/7c28/... tile helpers) -> SDL3 bridge
     * TBD. Frame present (Win16) -> CIV_ERR_UNSUPPORTED. SDL3 intro loop
     * lives in title_play_intro_sdl() below. */
    int is_intro_host;
    int is_earth_seq;
    int keep_on_busy;

    is_intro_host = title_dialog_is_intro(0x94);
    is_earth_seq = title_seq_is_earth(0x91e);
    keep_on_busy = title_intro_should_continue(0);
    if (is_intro_host == 0 || is_earth_seq == 0 || keep_on_busy == 0)
        return CIV_ERR_BAD_DATA;
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
     * @ 1018:xxxx + Main_RunStartupSequence @ 11e0:0000 loop#1 (Anim pump;
     * Pic_PlayShared @ 1030:0000 is the separate shared modal player) +
     * Sound_PlayId
     * (CIV_SOUND_OPENING)=OPENING.wav (see audio.h). Needs RSC/NE-resource
     * frame source (cf. rsc_loader.h ResMgr_LoadRsc @ 1190:0000) +
     * GfxPort_SetActive @ 10d0:0956 select (see video.h). MPLOGO.BMP has
     * no filename string (NE resource or RSC frame, see title.h). */
    return CIV_ERR_UNSUPPORTED;
}

int title_event_is_skip(uint32_t ev_type)
{
    /* Orig Main_RunStartupSequence @ 11e0:0000 intro-loop per-frame skip
     * check (key/mouse/quit each Anim_AdvanceFrames @ 10b0:0461 tick; same
     * shape as the Pic_PlayShared @ 1030:0000 modal pump, see title.h).
     * OS-free 1:1:
     * QUIT / KEY_DOWN / MOUSE_BUTTON_DOWN -> skip. No SDL calls. */
    if (ev_type == SDL_EVENT_QUIT)
        return 1;
    if (ev_type == SDL_EVENT_KEY_DOWN)
        return 1;
    if (ev_type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        return 1;
    return 0;
}

int title_poll_skip(void)
{
    /* Skippable-intro poll (orig Main_RunStartupSequence @ 11e0:0000 loop#1
     * checks key/mouse each Anim_AdvanceFrames @ 10b0:0461 tick). 1 == user
     * asked to skip (keydown/mousedown/quit), 0 == keep
     * playing. Consumes the polled events (intro owns the loop). Drains
     * via title_event_is_skip above (kernel stays testable without SDL
     * pump). */
    SDL_Event ev;

    while (SDL_PollEvent(&ev)) {
        if (title_event_is_skip(ev.type))
            return 1;
    }
    return 0;
}

/* Resolve a civ1win asset (MPLOGO.BMP): as-is, civ1win/<name>,
 * case-insensitive scan. Returns out or NULL. */
static char *title_resolve_asset(const char *name, char *out, size_t cap)
{
    char probe[512];
    FILE *fp = NULL;
    DIR *dir = NULL;
    struct dirent *ent = NULL;

    if (!name || !out || cap == 0)
        return NULL;
    out[0] = '\0';
    fp = fopen(name, "rb");
    if (fp) {
        fclose(fp);
        snprintf(out, cap, "%s", name);
        out[cap - 1] = '\0';
        return out;
    }
    snprintf(probe, sizeof(probe), "civ1win/%s", name);
    fp = fopen(probe, "rb");
    if (fp) {
        fclose(fp);
        snprintf(out, cap, "%s", probe);
        out[cap - 1] = '\0';
        return out;
    }
    dir = opendir("civ1win");
    if (!dir)
        return NULL;
    while ((ent = readdir(dir)) != NULL) {
        if (strcasecmp(ent->d_name, name) == 0) {
            closedir(dir);
            snprintf(out, cap, "civ1win/%s", ent->d_name);
            out[cap - 1] = '\0';
            fp = fopen(out, "rb");
            if (fp) {
                fclose(fp);
                return out;
            }
            return NULL;
        }
    }
    closedir(dir);
    return NULL;
}

/* Max intro wall time (ms). OPENING.wav is ~17.6 s; hard cap 20 s keeps
 * dummy-audio builds terminating. CIVLIN_INTRO_SECONDS env overrides
 * (used by headless smoke tests). */
static unsigned title_max_ms(void)
{
    const char *env = getenv("CIVLIN_INTRO_SECONDS");
    if (env && env[0] != '\0') {
        long v = strtol(env, NULL, 10);
        if (v > 0 && v < 120)
            return (unsigned)v * 1000u;
    }
    return 20000u;
}

civ_error_t title_play_intro_sdl(SDL_Window *win, SDL_Renderer *ren)
{
    /* SDL3 intro (Main_RunStartupSequence @ 11e0:0000 loop#1 shape: dialog
     * 0x94 host + Anim_OpenSequence @ 10b0:008a / AdvanceFrames @ 10b0:0461
     * pump; NOT the Pic_PlayShared @ 1030:0000 modal shape): MPLOGO.BMP frame
     * + Sound_PlayId(CIV_SOUND_OPENING)=OPENING.wav + title_poll_skip().
     * Animation = fade-in + gentle float + gold sweep + twinkling
     * starfield + blinking prompt; skippable any time. Returns CIV_OK on
     * natural end or skip (missing BMP still animates procedurally;
     * missing/silent audio still animates silently). No exit(). */
    char bmp_path[512];
    SDL_Surface *surf = NULL;
    SDL_Texture *logo = NULL;
    float logo_w = 0.0f;
    float logo_h = 0.0f;
    int have_logo = 0;
    int out_w = 640;
    int out_h = 480;
    Uint64 t0 = 0;
    unsigned max_ms = 0;
    unsigned frame = 0;
    unsigned long last_tick = 0;
    civ_error_t audio_rc = CIV_ERR_UNSUPPORTED;
    int audio_started = 0;
    int skipped = 0;

    if (!win || !ren)
        return CIV_ERR_NOENT;

    if (title_resolve_asset("MPLOGO.BMP", bmp_path, sizeof(bmp_path))) {
        surf = SDL_LoadBMP(bmp_path);
        if (surf) {
            logo = SDL_CreateTextureFromSurface(ren, surf);
            SDL_DestroySurface(surf);
            surf = NULL;
        }
        if (logo) {
            if (SDL_GetTextureSize(logo, &logo_w, &logo_h)) {
                have_logo = 1;
                SDL_SetTextureAlphaMod(logo, 0);
                SDL_Log("title: logo %s (%.0fx%.0f)", bmp_path, logo_w,
                        logo_h);
            } else {
                SDL_DestroyTexture(logo);
                logo = NULL;
            }
        } else {
            SDL_Log("title: LoadBMP/texture failed %s: %s", bmp_path,
                    SDL_GetError());
        }
    } else {
        SDL_Log("title: MPLOGO.BMP not found, procedural emblem");
    }

    /* Sound_PlayId(3)=OPENING.wav (SI 3/4 -> 0x83; id 4 normalizes to 3).
     * Failure (dummy device, missing file) is non-fatal: silent animation
     * still proves the frame loop. */
    audio_rc = audio_play_id(CIV_SOUND_OPENING);
    audio_started = (audio_rc == CIV_OK);
    if (!audio_started)
        SDL_Log("title: opening audio silent (%d)", (int)audio_rc);

    if (!SDL_GetRenderOutputSize(ren, &out_w, &out_h)) {
        out_w = 640;
        out_h = 480;
    }

    max_ms = title_max_ms();
    t0 = SDL_GetTicks();
    frame = 0;
    /* U1: frame clock = oryginalne ticki gry (Sys_GetTickScaled @ 1100:02e1,
     * GETTICKCOUNT * 60 / 1000; ta sama jednostka co okresy opkodow Anim @
     * 10b0:0461 i seed Rng_Init @ 11c8:0000). Jeden tick = jedna klatka;
     * wall-clock (SDL_GetTicks) zostaje tylko na koniec audio/limity.
     * Tick-wait delegowany do sys_wait_next_tick (Sys_GetTickScaled @
     * 1100:02e1 + Sys_Yield @ 1098:0157 bridge, ref unpacked/civ.exe
     * Main_RunStartupSequence @ 11e0:0000 loop#1 pump; ten sam krok co
     * petla main @ 1008:0304 w main.c (wlasciciel: main.c, tylko read) i
     * timer-half anim_pump_if_active @ 10b0:0000 w dialog.c, nadal
     * PARTIAL — ogony klatek/GfxPort tam, nie tu). */
    last_tick = sys_get_tick_scaled();
    for (;;) {
        Uint64 now = 0;
        unsigned elapsed = 0;
        int advanced = 0;
        civ_error_t wrc = CIV_OK;
        float cx = 0.0f;
        float cy = 0.0f;
        int tri = 0;
        float float_y = 0.0f;
        Uint8 alpha = 255;
        int i = 0;

        if (title_poll_skip()) {
            skipped = 1;
            break;
        }
        wrc = sys_wait_next_tick(&last_tick, &advanced);
        if (wrc != CIV_OK)
            return wrc;
        if (!advanced)
            continue;
        now = SDL_GetTicks();
        elapsed = (unsigned)(now - t0);
        if (audio_started) {
            /* Natural end: WAV drained after a 2 s minimum (so the logo
             * is visible even for short/odd files). Hard cap above. */
            if (elapsed > 2000u && !audio_is_playing())
                break;
            if (elapsed >= max_ms)
                break;
        } else {
            unsigned silent_ms = max_ms;
            if (silent_ms > 6000u)
                silent_ms = 6000u;
            if (elapsed >= silent_ms)
                break;
        }

        /* --- frame --- */
        SDL_SetRenderDrawColor(ren, 4, 8, 24, 255);
        SDL_RenderClear(ren);

        /* Starfield: 90 deterministic twinkling dots (integer math only,
         * no libm dependency). */
        for (i = 0; i < 90; i++) {
            int sx = (i * 97 + (int)(frame * 3)) % (out_w > 0 ? out_w : 640);
            int sy = (i * 57 + (int)(frame * 1)) % (out_h > 0 ? out_h : 480);
            Uint8 tw = (Uint8)(90 + ((i * 31 + (int)frame * 7) % 160));
            SDL_SetRenderDrawColor(ren, tw, tw, (Uint8)(tw > 200 ? 255 : tw),
                                   255);
            SDL_RenderPoint(ren, (float)sx, (float)sy);
        }

        /* Bottom gold band (palette homage) with a travelling highlight. */
        SDL_SetRenderDrawColor(ren, 96, 72, 16, 255);
        {
            SDL_FRect band;
            band.x = 0.0f;
            band.y = (float)out_h - 28.0f;
            band.w = (float)out_w;
            band.h = 6.0f;
            SDL_RenderFillRect(ren, &band);
        }
        {
            float hx = (float)((frame * 9) % (unsigned)(out_w + 80)) - 40.0f;
            SDL_SetRenderDrawColor(ren, 255, 220, 120, 255);
            SDL_RenderLine(ren, hx, (float)out_h - 30.0f, hx + 24.0f,
                           (float)out_h - 30.0f);
        }

        if (have_logo) {
            /* Fade-in over ~64 frames, then gentle float (+/-10 px via
             * integer triangle wave, period 120 frames). */
            alpha = frame < 64 ? (Uint8)(frame * 4) : 255;
            SDL_SetTextureAlphaMod(logo, alpha);
            tri = (int)(frame % 120u);
            if (tri >= 60)
                tri = 120 - tri;
            float_y = ((float)tri - 30.0f) / 3.0f;
            cx = ((float)out_w - logo_w) / 2.0f;
            cy = ((float)out_h - logo_h) / 2.0f - 30.0f + float_y;
            {
                SDL_FRect dst;
                dst.x = cx;
                dst.y = cy;
                dst.w = logo_w;
                dst.h = logo_h;
                SDL_RenderTexture(ren, logo, NULL, &dst);
            }
            /* Gold sweep across the logo band every ~3 s. */
            {
                float sweep =
                    (float)((frame * 7) % (unsigned)(logo_w + 200)) - 100.0f;
                SDL_SetRenderDrawColor(ren, 255, 220, 120, 255);
                SDL_RenderLine(ren, cx + sweep, cy, cx + sweep,
                               cy + logo_h);
            }
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDebugText(ren, 8.0f, 8.0f,
                                "CivLin intro - OPENING.wav + MPLOGO.BMP");
            if (elapsed > 2000u && (frame / 30u) % 2u == 0)
                SDL_RenderDebugText(ren, 8.0f, (float)out_h - 20.0f,
                                    "Press any key to skip");
        } else {
            /* Procedural emblem fallback: bordered plates + pulsing fill. */
            float bw = 380.0f;
            float bh = 97.0f;
            float bx = ((float)out_w - bw) / 2.0f;
            float by = ((float)out_h - bh) / 2.0f - 30.0f;
            Uint8 pulse = (Uint8)(120 + (frame * 5u) % 120u);
            SDL_FRect outer;
            SDL_FRect inner;
            outer.x = bx - 8.0f;
            outer.y = by - 8.0f;
            outer.w = bw + 16.0f;
            outer.h = bh + 16.0f;
            inner.x = bx;
            inner.y = by;
            inner.w = bw;
            inner.h = bh;
            SDL_SetRenderDrawColor(ren, 200, 170, 90, 255);
            SDL_RenderRect(ren, &outer);
            SDL_SetRenderDrawColor(ren, pulse, (Uint8)(pulse / 2u), 20, 255);
            SDL_RenderFillRect(ren, &inner);
            SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
            SDL_RenderDebugText(ren, bx + 60.0f, by + 40.0f,
                                "M P L O G O (procedural)");
            SDL_RenderDebugText(ren, 8.0f, 8.0f,
                                "CivLin intro - procedural (no BMP)");
            if ((frame / 30u) % 2u == 0)
                SDL_RenderDebugText(ren, 8.0f, (float)out_h - 20.0f,
                                    "Press any key to skip");
        }

        SDL_RenderPresent(ren);
        frame++;
    }

    if (logo)
        SDL_DestroyTexture(logo);
    if (skipped && audio_started)
        (void)audio_play_id(CIV_SOUND_STOP);
    SDL_Log("title: intro done (frames=%u skipped=%d)", frame, skipped);
    return CIV_OK;
}

civ_error_t title_mplogo_show(SDL_Window *win, SDL_Renderer *ren,
                              const char *path)
{
    /* AF1 Linux-port MPLOGO display (no Orig file-load address: MPLOGO.BMP
     * has NO filename string in civ.exe — NE resource or RSC frame, NOT a
     * file load; the Linux port passes the path explicitly, read-only).
     * Loads via gfx_bmp_load_indexed (OS-free parse + 1-bpp unpack, fopen
     * "rb" only inside the loader, never writes), bridges via
     * gfx_indexed_to_texture (SDL only here), renders one centered frame
     * + present. Single frame; the caller owns the loop/skip. */
    rsc_indexed_t img;
    uint8_t *px = NULL;
    SDL_Texture *tex = NULL;
    float tw = 0.0f;
    float th = 0.0f;
    int out_w = 640;
    int out_h = 480;
    civ_error_t lr;

    if (!win || !ren || !path || path[0] == '\0')
        return CIV_ERR_NOENT;
    memset(&img, 0, sizeof(img));
    lr = gfx_bmp_load_indexed(path, &img, &px);
    if (lr != CIV_OK)
        return lr;
    tex = gfx_indexed_to_texture(ren, &img);
    if (!tex) {
        free(px);
        return CIV_ERR_UNSUPPORTED;
    }
    if (!SDL_GetTextureSize(tex, &tw, &th)) {
        SDL_DestroyTexture(tex);
        free(px);
        return CIV_ERR_UNSUPPORTED;
    }
    if (!SDL_GetRenderOutputSize(ren, &out_w, &out_h)) {
        out_w = 640;
        out_h = 480;
    }
    SDL_SetRenderDrawColor(ren, 4, 8, 24, 255);
    SDL_RenderClear(ren);
    {
        SDL_FRect dst;

        dst.x = ((float)out_w - tw) / 2.0f;
        dst.y = ((float)out_h - th) / 2.0f;
        dst.w = tw;
        dst.h = th;
        SDL_RenderTexture(ren, tex, NULL, &dst);
    }
    SDL_RenderPresent(ren);
    SDL_DestroyTexture(tex);
    free(px);
    return CIV_OK;
}

/* --- Chain 2026-10-16 (28): intro predicates + loop gate (goal 5). See
 * title.h. OS-free 1:1 LIVE (no Win16, no globals, no exit()). Ref
 * unpacked/civ.exe Main_RunStartupSequence @ 11e0:0000 + Anim_OpenSequence
 * @ 10b0:008a / Anim_AdvanceFrames @ 10b0:0461 decompiles. */

int title_dialog_is_intro(int dlg_id)
{
    /* Orig Main_RunStartupSequence @ 11e0:0000 loop#1 host test
     * (Dlg_CreateTracked @ 1098:124f param_1 == 0x94). */
    return (dlg_id == 0x94) ? 1 : 0;
}

int title_seq_is_earth(int seq_id)
{
    /* Orig same loop#2 seq test (Anim_OpenSequence @ 10b0:008a seq ==
     * 0x91e; sequence id, NOT a dialog id). */
    return (seq_id == 0x91e) ? 1 : 0;
}

int title_intro_should_continue(int adv_ret)
{
    /* Orig same loop#1/loop#2 pump gate (do { c = Anim_AdvanceFrames @
     * 10b0:0461 } while (c == 0); loop#2 DAT_13e0_001e break stays in
     * main_startup_should_break in boot.h). */
    return (adv_ret == 0) ? 1 : 0;
}
