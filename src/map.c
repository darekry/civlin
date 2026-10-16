/* Map tile buffer (1:1) + window stubs. */
#include "map.h"

/* AL2: SDL/video bridge for text_draw_with_font only (read-only parity
 * via Read, not edited; no Win16 here). Other funcs stay SDL-free. */
#include <SDL3/SDL.h>

#include "video.h"
#include "gfx_indexed.h"
/* boot.h already via map.h (font_render_glyph/text_width/height +
 * font_fnt_info_t); explicit for parity clarity. */
#include "boot.h"

/* 64 KB tile buffer: g_segMapBuf:g_offMapBuf @ 1420:b396/b394. */
static uint8_t s_map[CIV_MAP_BUF_LEN];

uint8_t *map_buffer(void)
{
    return s_map;
}

size_t map_buffer_len(void)
{
    return sizeof(s_map);
}

static int map_in_bounds(int x, int y)
{
    /* Map_GetTile @ 11e8:0dcf / Map_SetTile @ 11e8:0e19 accept
     * 0 <= x < 0x140, 0 <= y < 200. */
    return x >= 0 && x < (int)CIV_MAP_BUF_W && y >= 0 && y < (int)CIV_MAP_BUF_H;
}

int map_get_tile(int x, int y)
{
    /* Map_GetTile @ 11e8:0dcf: *(base + y * 0x140 + x), OOB -> sentinel.
     * The original returns a sentinel byte on OOB; -1 keeps the contract
     * explicit on Linux. */
    if (!map_in_bounds(x, y))
        return -1;
    return s_map[(size_t)y * CIV_MAP_BUF_W + (size_t)x];
}

void map_set_tile(int x, int y, uint8_t tile)
{
    /* Map_SetTile @ 11e8:0e19: OOB writes silently ignored. */
    if (!map_in_bounds(x, y))
        return;
    s_map[(size_t)y * CIV_MAP_BUF_W + (size_t)x] = tile;
}

void map_fill_rect(int x, int y, unsigned w, unsigned h, uint8_t tile)
{
    /* Map_FillRect @ 1028:1d15 (x, y, w, h, tile): row-wise fill with
     * stride wrap. The original has a fast dword path for w % 4 == 0;
     * memset per row is equivalent. OOB rows/cols fall out via
     * map_set_tile. */
    unsigned r, c;

    for (r = 0; r < h; r++) {
        for (c = 0; c < w; c++)
            map_set_tile(x + (int)c, y + (int)r, tile);
    }
}

void map_draw_rect_border(int x, int y, int w, int h, uint8_t tile)
{
    /* Map_DrawRectBorder @ 1028:1ca7: top row, bottom row, left col,
     * right col. */
    map_fill_rect(x, y, (unsigned)w, 1, tile);
    map_fill_rect(x, y + h, (unsigned)w, 1, tile);
    map_fill_rect(x, y, 1, (unsigned)h, tile);
    map_fill_rect(x + w, y, 1, (unsigned)h, tile);
}

int map_try_place_isolated_tile(int x, int y)
{
    /* Map_TryPlaceIsolatedTile @ 1028:05e7: when none of the 4 diagonal
     * neighbours holds 1, write 1 and return 1; otherwise return 0. */
    if (map_get_tile(x - 1, y - 1) == 1 || map_get_tile(x - 1, y + 1) == 1 ||
        map_get_tile(x + 1, y - 1) == 1 || map_get_tile(x + 1, y + 1) == 1)
        return 0;
    map_set_tile(x, y, 1);
    return 1;
}

civ_error_t map_init(void)
{
    return CIV_ERR_UNSUPPORTED;
}

win16_msg_result_t map_wndproc_main(int msg, long wparam, long lparam)
{
    /* Orig FUN_1208_0054 @ 1208:0054 -> WDWMAPPROC. Main map WndProc:
     * (msg, wparam, lparam) -> WIN16_MSG_HANDLED when consumed else
     * WIN16_MSG_DEFAULT (DefWindowProc). Message table TBD (expected
     * WM_PAINT / WM_HSCROLL / WM_VSCROLL / WM_TIMER, see map.h); Win16
     * USER/GDI -> SDL3 TBD. No globals, no exit(), read-only. */
    (void)msg;
    (void)wparam;
    (void)lparam;
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t map_wndproc_small(int msg, long wparam, long lparam)
{
    /* Orig FUN_1210_0000 @ 1210:0000 -> WDWSMMAPPROC. World-map child
     * WndProc: same (msg, wparam, lparam) -> handled/default contract
     * as above. Message table TBD (expected WM_PAINT + click-center via
     * Map_CenterOnTile @ 1110:03ae, see map.h); Win16 -> SDL3 TBD. No
     * globals, no exit(), read-only. */
    (void)msg;
    (void)wparam;
    (void)lparam;
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t map_wndproc_status(int msg, long wparam, long lparam)
{
    /* Orig FUN_1218_0000 @ 1218:0000 -> WDWSTATUSPROC. Status child
     * WndProc: same handled/default contract. Message table TBD
     * (expected WM_PAINT of the 1218:0956/0a06 frames + 11f8:0278 rows,
     * see map.h); Win16 -> SDL3 TBD. No globals, no exit(), read-only. */
    (void)msg;
    (void)wparam;
    (void)lparam;
    return WIN16_MSG_DEFAULT;
}

civ_error_t map_timer_proc(void)
{
    /* Orig FUN_1008_0D68 @ 1008:0d68 -> TIMERPROC. 200 ms SETTIMER tick
     * from WdwMap_Open @ 1208:0af1, pumped by WinMain @ 1008:0000;
     * tick -> game_tick (see game.h) -> Game_TurnLoopTick @ 1088:0000 /
     * game_turn_tick. TODO: SDL timer bridge (SDL_AddTimer) + frame
     * pump; KILLTIMER teardown in WinMain_Shutdown @ 1008:0329. No
     * globals, no exit(), read-only. */
    return CIV_ERR_UNSUPPORTED;
}

int map_timer_interval_ms(void)
{
    /* Orig WdwMap_Open @ 1208:0af1 SETTIMER(..., 1, 0xC8, NULL) interval
     * (1208:0e7b; 0xC8 = 200 ms WM_TIMER do map_wndproc_main @ 1208:0054).
     * SDL_AddTimer: dokumentacja, bez implementacji (patrz boot.h). */
    return 200;
}

void map_shutdown(void)
{
}

civ_error_t wdwstatus_calc_upper_rect(civ_rect_t *out)
{
    (void)out;
    /* TODO: FUN_1218_0956 @ 1218:0956 (client-rect -> centered frame,
     * cy+0x1e variant; INFLATERECT -1,-1; see map.h). OS-free cx/cy head
     * is LIVE in wdwstatus_frame_metrics below; the SETRECT field order
     * + GLOBALLOCK rect fetch stay Win16 until decoded. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t wdwstatus_calc_lower_rect(civ_rect_t *out)
{
    (void)out;
    /* TODO: FUN_1218_0a06 @ 1218:0a06 (cy*2+0x46/0x1e variant; same head
     * as above, LIVE in wdwstatus_frame_metrics). */
    return CIV_ERR_UNSUPPORTED;
}

/* --- status dirty-flag + frame metrics (2026-10-16): OS-free 1:1. --- */

int wdwstatus_is_dirty(int flag)
{
    /* Orig WdwStatus_Refresh @ 1218:0801 head: *(handle+0x2c) == 0 ->
     * early out (GLOBALUNLOCK only), else full repaint. Same gate guards
     * WdwStatus_RequestRefresh @ 1218:063e (DAT_1420_4964 != 0). */
    return flag != 0;
}

civ_error_t wdwstatus_frame_metrics(int client_w, int client_h,
                                    int *cx_out, int *cy_out)
{
    /* Orig 1218:0956/0a06/0abf shared head. Decompiler:
     * iVar1 = ((local_c - local_10) + -0x8c) / 2 (width span - 0x8c);
     * iVar2 = ((local_a - local_e) + -0xf0) / 4 (height span - 0xf0);
     * if (10 < iVar2) iVar2 = 10; if (iVar2 < 0) iVar2 = 0.
     * C17 /2 and /4 on the caller spans match the orig signed division
     * for non-negative spans (rect spans are >= 0 on the status port). */
    int cy;

    if (!cx_out || !cy_out)
        return CIV_ERR_NOENT;
    *cx_out = (client_w - 0x8c) / 2;
    cy = (client_h - 0xf0) / 4;
    if (cy > 10)
        cy = 10;
    if (cy < 0)
        cy = 0;
    *cy_out = cy;
    return CIV_OK;
}

int path_find_corridor(int civ, int unit_idx, int max_steps)
{
    (void)civ;
    (void)unit_idx;
    (void)max_steps;
    /* TODO: Orig FUN_1128_10de @ 1128:10de (corridor grid search with a
     * max_steps budget, 999 from unit_step_toward_goal @ 1128:0000;
     * writes the corridor + falls back to 0xffff/no-path clearing
     * 6302/6304). Pure tile-buffer logic, no Win16; decode pending. */
    return 0xffff;
}

int path_is_passable(int civ, int x, int y)
{
    /* Path_IsPassable @ 1128:081c (v1 pure port): bounds gate only.
     * The full sea/land/ZOC/occupancy gating lives in the caller
     * (unit_step_toward_goal @ 1128:0000: 0x1a3a vs 1110:16c0 sea match,
     * 11c0:23fd ZOC, 1110:18ce/16ee occupancy); TBD here. */
    (void)civ;
    return map_get_tile(x, y) != -1;
}

int map_is_sea_land(int x, int y)
{
    /* Orig FUN_1110_16C0 @ 1110:16c0 -> Map_IsSeaLand (sea/land match
     * gate: 0x1a3a vs 1110:16c0==10). Folds the ==10 ocean test:
     * tile 10 (see worldgen.h) is sea. OOB map_get_tile -> -1 != 10
     * -> 0 (land), so callers still gate OOB via path_is_passable. */
    return map_get_tile(x, y) == 10;
}

int map_is_occupied(int x, int y, const int *unit_xs, const int *unit_ys,
                    int n_units)
{
    /* Orig FUN_1110_18CE @ 1110:18ce -> Map_IsOccupied (occupancy gate
     * for unit_step_toward_goal @ 1128:0000). Orig scans the 1420:62fc
     * unit stack with 0x1a3a side gating (exact predicate TBD); here
     * the stack is passed explicitly so map.c keeps no globals. */
    int i;

    if (unit_xs == NULL || unit_ys == NULL || n_units <= 0)
        return 0;
    for (i = 0; i < n_units; i++) {
        if (unit_xs[i] == x && unit_ys[i] == y)
            return 1;
    }
    return 0;
}

int map_is_city_blocked(int x, int y, const int *city_xs,
                        const int *city_ys, int n_cities)
{
    /* Orig FUN_1110_199B @ 1110:199b -> Map_IsCityBlocked (second
     * occupancy/city gate for unit_step_toward_goal @ 1128:0000;
     * city records 0x1c bytes at 1420:5500, exact predicate TBD).
     * Explicit arrays keep no globals; NULL/empty -> 0. */
    int i;

    if (city_xs == NULL || city_ys == NULL || n_cities <= 0)
        return 0;
    for (i = 0; i < n_cities; i++) {
        if (city_xs[i] == x && city_ys[i] == y)
            return 1;
    }
    return 0;
}

int map_ocean_clear_depth(int x, int y, int dx, int dy)
{
    /* Orig FUN_1110_1DBD @ 1110:1dbd -> Map_OceanClearDepth (ocean >4
     * clear check for unit_step_toward_goal @ 1128:0000: caller tests
     * result >4). Run check straight from the tile buffer via
     * map_get_tile, capped at 5. */
    int depth = 0;
    int k;

    if (dx < -1 || dx > 1 || dy < -1 || dy > 1)
        return 0;
    if (dx == 0 && dy == 0)
        return 0;
    for (k = 0; k < 5; k++) {
        if (map_get_tile(x + dx * k, y + dy * k) != 10)
            break;
        depth++;
    }
    return depth;
}

int map_resolve_direct(int ux, int uy, int gx, int gy)
{
    /* Orig FUN_1110_1C4B @ 1110:1c4b -> Map_ResolveDirect
     * (player-adjacent |dx|<2,|dy|<2 direct resolve for
     * unit_step_toward_goal @ 1128:0000). Direction numbering 1=N
     * clockwise is the Linux convention; exact orig table TBD. */
    int dx = gx - ux;
    int dy = gy - uy;

    if (dx == 0 && dy == 0)
        return 0;
    if (dx < -1 || dx > 1 || dy < -1 || dy > 1)
        return 0xffff;
    if (dx == 0 && dy == -1)
        return 1;
    if (dx == 1 && dy == -1)
        return 2;
    if (dx == 1 && dy == 0)
        return 3;
    if (dx == 1 && dy == 1)
        return 4;
    if (dx == 0 && dy == 1)
        return 5;
    if (dx == -1 && dy == 1)
        return 6;
    if (dx == -1 && dy == 0)
        return 7;
    return 8;
}

civ_error_t map_reveal_center(int civ, int x, int y)
{
    /* Orig FUN_1110_1461 @ 1110:1461 -> Map_RevealCenter
     * (reveal/center helper, called from unit_maybe_center_view @
     * 11f0:44d4 when the 6305 seen bit says visible). Needs the SDL
     * view/scroll state; stub CIV_ERR_UNSUPPORTED. */
    (void)civ;
    (void)x;
    (void)y;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t map_spot_city(int civ, int x, int y)
{
    /* Orig pair FUN_1110_180E @ 1110:180e + FUN_1110_16EE @ 1110:16ee
     * -> Map_SpotCity (city-spotting pair feeding 11c0:0000, via
     * unit_update_visibility @ 11c0:0395). Needs city records +
     * seen-bits + minimap layer; stub CIV_ERR_UNSUPPORTED. */
    (void)civ;
    (void)x;
    (void)y;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t text_draw_status_row(const char *str, const civ_rect_t *rect,
                                 int slot)
{
    (void)str;
    (void)rect;
    (void)slot;
    /* TODO: Orig FUN_11f8_0278 @ 11f8:0278 (GDI TEXTOUT of the status
     * row string into the 1218:0956/0a06 frame; slot selects the
     * DAT_12d8_45f4/45fb source row + font). Needs the font/text
     * SDL bridge. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t text_draw_with_font(SDL_Window *win, SDL_Renderer *ren,
                                SDL_Texture *tex, const char *str,
                                const civ_rect_t *rect, int slot,
                                const uint8_t *font_data, size_t font_len,
                                const font_fnt_info_t *font_info)
{
    /* AL2 Text_DrawStatusRow @ 11f8:0278 via font_blit_glyph @ 1008:0e99
     * + font_text_width/height (see map.h). OS-free metrics + per-char
     * SDL blit loop, no Win16: str + rect (civ_rect_t from
     * wdwstatus_calc_upper/lower @ 1218:0956/0a06) + slot DAT_12d8_45f4/
     * 45fb (0 = upper/45f4, 1 = lower/45fb; other non-negative = same
     * origin, exact row mapping TBD) -> font blit per char at
     * (rect->left + xoff, rect->top), xoff += glyph width (tracking 0).
     * Metrics parity read-only (not edited): font_text_width (sum of
     * per-char entry widths, OOR skipped) + font_text_height
     * (dfPixHeight line stride). SDL bridge via video.h font_blit_glyph
     * (SRCCOPY 0xcc0020 family) + gfx_indexed.h font_glyph_to_surface
     * (FNT 1-bpp MSB-first tight (w+7)/8 -> 8-bit black/white surface).
     * Single row, no wrap, clipped at rect->right (break when xoff >=
     * rect_w); vertical overflow still blits top-aligned (no fake scale).
     * Borrowed font image (never writes civfonts.fon) vs caller-owned SDL
     * dest (win borrowed, may be NULL like font_blit_glyph; ren/tex
     * borrowed, never owned/freed here; glyph surfaces temp, destroyed
     * per char). No globals, no exit(), C17. */
    int rect_w;
    int line_h;
    int total_w;
    int xoff;
    size_t i;

    if (!str || !rect || !font_data || !font_info)
        return CIV_ERR_NOENT;
    if (!ren || !tex)
        return CIV_ERR_NOENT;
    if (font_len < (size_t)CIV_FNT_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    if (rect->right <= rect->left || rect->bottom <= rect->top)
        return CIV_ERR_RANGE;
    if (slot < 0)
        return CIV_ERR_RANGE;
    /* Metrics via font_text_width/height (parity, no SDL). total_w is the
     * GetTextExtent shape without tracking (OOR chars contribute 0, see
     * boot.h); used only for documentation/clipping parity, success never
     * gated on it (empty/undecodable text still walks per char below). */
    total_w = font_text_width(font_data, font_len, font_info, str);
    line_h = font_text_height(font_info);
    (void)total_w;
    if (line_h <= 0)
        return CIV_ERR_BAD_DATA;
    rect_w = rect->right - rect->left;
    /* Slot selects the DAT_12d8_45f4/45fb source row + font row; the rect
     * already selects the upper/lower frame, so no y offset is faked here
     * (exact row mapping TBD, see map.h). win is passed through to
     * font_blit_glyph (ignored there, may be NULL). */
    xoff = 0;
    for (i = 0; str[i] != '\0'; i++) {
        int c = (unsigned char)str[i];
        uint8_t bits[1024];
        int gw = 0;
        int gh = 0;
        civ_error_t r;
        SDL_Surface *surf = NULL;
        civ_error_t b;

        if (xoff >= rect_w)
            break;
        /* Tight repack (w+7)/8 via font_render_glyph (dfBitsOffset +
         * dfCharTable, MSB-first). Skip OOR/undecodable chars with 0
         * advance (parity with font_text_width, never faked). 1024 B
         * covers all 21 faces (max stride*h ~5*38=190 B for TIMES36; see
         * boot.h font_fnt_bits stride (dfMaxWidth+7)/8). */
        r = font_render_glyph(font_data, font_len, font_info, c, bits,
                              sizeof bits, &gw, &gh);
        if (r != CIV_OK)
            continue;
        if (gw <= 0 || gh <= 0)
            continue;
        surf = font_glyph_to_surface(bits, gw, gh);
        if (!surf)
            return CIV_ERR_IO;
        b = font_blit_glyph(win, ren, tex, rect->left + xoff, rect->top,
                            surf);
        SDL_DestroySurface(surf);
        if (b != CIV_OK)
            return b;
        xoff += gw;
    }
    return CIV_OK;
}

civ_error_t status_format_main_row(char *dst, size_t cap)
{
    (void)dst;
    (void)cap;
    /* TODO: Orig FUN_1138_0000 @ 1138:0000 (Status_FormatMainRow: formats
     * the main status line into 12d8:24fa via game_append_year_text @
     * 1088:1dd0, then Text_DrawStatusRow @ 11f8:0278; needs GDI font /
     * TEXTOUT SDL bridge). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t status_format_second_row(char *dst, size_t cap)
{
    (void)dst;
    (void)cap;
    /* TODO: Orig FUN_11b0_0000 @ 11b0:0000 (Status_FormatSecondRow: second
     * status line, second game_append_year_text caller; same 12d8:24fa +
     * 11f8:0278 path as above). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t status_format_gold_row(char *dst, size_t cap)
{
    (void)dst;
    (void)cap;
    /* TODO: Orig FUN_11b8_14e5 @ 11b8:14e5 (Status_FormatGoldRow: gold/city
     * row; same 12d8:24fa + 11f8:0278 path; field layout TBD). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t map_open_main(void)
{
    /* TODO: Orig FUN_1208_0AF1 @ 1208:0af1 -> WdwMap_Open () -> handle.
     * CREATEWINDOW "CIVILIZATION" + scroll ranges + GfxPort select +
     * SETTIMER 200 ms, logs "WdwMapOpen ...wdwmap.c". Win16 USER/GDI ->
     * SDL3 window TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t map_open_small(void)
{
    /* TODO: Orig FUN_1210_025B @ 1210:025b -> WdwSmMap_Open (child
     * "World Map"). CREATEWINDOW child; Win16 -> SDL3 TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t map_open_status(void)
{
    /* TODO: Orig FUN_1218_0386 @ 1218:0386 -> WdwStatus_Open (child
     * "Status"); feeds WdwStatus_Refresh @ 1218:0801 +
     * Game_UpdateStatusPanel @ 1088:13d5. CREATEWINDOW child; Win16 ->
     * SDL3 TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t map_refresh_status(void)
{
    /* Orig: FUN_1218_0801 @ 1218:0801 -> WdwStatus_Refresh. Dirty flag
     * +0x2c + Game_UpdateStatusPanel @ 1088:13d5 + Unit_ShowInfoPopup @
     * 11f0:468c + blits 10d0:0854/09a2. Win16 USER/GDI -> SDL3 TBD; stub
     * CIV_ERR_UNSUPPORTED. No globals, no exit(), read-only. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t map_center_on_tile(int civ, int x, int y)
{
    /* Orig: FUN_1110_03ae @ 1110:03ae -> Map_CenterOnTile. Centers x/y
     * (x - w/2, y - h/2) then Map_RefreshAllViews @ 1110:03e9. Needs the
     * SDL view/scroll state; stub CIV_ERR_UNSUPPORTED. */
    (void)civ;
    (void)x;
    (void)y;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t map_refresh_all_views(int civ)
{
    /* Orig: FUN_1110_03e9 @ 1110:03e9 -> Map_RefreshAllViews. Civ bit ->
     * 1408:00cc + Game_UpdateStatusPanel @ 1088:13d5 + GETFREESPACE guard
     * + 1208:17da + 1210:09b4. Win16 windowing -> SDL3 TBD; stub
     * CIV_ERR_UNSUPPORTED. */
    (void)civ;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t mainport_apply_palette(int first, int count)
{
    /* Orig: FUN_1200_00a8 @ 1200:00a8 -> MainPort_ApplyPalette.
     * GLOBALLOCK -> Palette_AnimateRange @ 1090:0376 (see video.h). Win16
     * palette -> SDL3 TBD; stub CIV_ERR_UNSUPPORTED. */
    (void)first;
    (void)count;
    return CIV_ERR_UNSUPPORTED;
}

/* AI-C tile table: dump-confirmed subset only (see map.h). Index
 * 0..CIV_TILE_TYPE_MAX; unlisted slots are NULL -> "?" (no fake names).
 * NOTE: ocean is decimal 10 (0x0A, see Map_IsSeaLand @ 1110:16c0); 0x10
 * (16) is undecoded -> "?". */
static const char *const s_tile_type_names[CIV_TILE_TYPE_MAX + 1] = {
    [0] = "cleared",
    [1] = "land",
    [2] = "land",
    [10] = "ocean",
    [0x0b] = "land",
    [0x0c] = "river seed",
    [0x0f] = "border",
};

const char *tile_type_name(int tile)
{
    const char *s;

    if (tile < 0 || tile > CIV_TILE_TYPE_MAX)
        return "?";
    s = s_tile_type_names[tile];
    return s != NULL ? s : "?";
}

int tile_move_cost(int tile, const uint8_t *cost_table, int cost_n)
{
    /* Orig 1420:17d4 base-cost table (*3-or-3) for unit_step_toward_goal
     * @ 1128:0000 (map-side twin of unit_move_cost in game.h; river 1 +
     * anti-reverse 0x6304 live in the game-side wrapper). Zero-entry /
     * OOR fallback TBD vs decompiler. OS-free, no globals, read-only. */
    int v;

    if (cost_table == NULL || cost_n <= 0)
        return -1;
    if (tile < 0 || tile >= cost_n)
        return 3;
    v = cost_table[tile];
    return v != 0 ? v * 3 : 3;
}

/* --- Map step distance (2026-10-16): OS-free 1:1. No globals. */

int map_step_dist(int dx, int dy)
{
    /* Orig Map_StepDist @ 11c8:025d. Decompiler: ax = |dx|, bx = |dy|
     * (via (v ^ (v>>0xf)) - (v>>0xf)); hi = max, lo = min; lo /= 2;
     * returns hi + lo. */
    int ax = dx < 0 ? -dx : dx;
    int by = dy < 0 ? -dy : dy;
    int hi = ax > by ? ax : by;
    int lo = ax < by ? ax : by;

    return hi + lo / 2;
}

int map_wrap_step_dist(int x1, int x2, int y1, int y2)
{
    /* Orig Map_WrapStepDist @ 11c8:02ec. Decompiler: dx = |x1-x2| (same
     * fold); if 0x28 < dx dx = 0x50-dx (game-map width wrap, cf.
     * CIV_MAP_WRAP_W); dy = |y1-y2| unwrapped; then Map_StepDist @
     * 11c8:025d. Used by Replay_FindNearestSlot @ 1180:1096. */
    int dx = x1 - x2;
    int dy = y1 - y2;

    dx = dx < 0 ? -dx : dx;
    if (dx > 0x28)
        dx = CIV_MAP_WRAP_W - dx;
    dy = dy < 0 ? -dy : dy;
    return map_step_dist(dx, dy);
}

/* --- Tile access family (2026-10-16): OS-free 1:1. No globals. */

int map_check_tile_bounds(int x, int y)
{
    /* Orig Map_CheckTileBounds @ 1110:168f: (x<0 || 0x4f<x || y<0 ||
     * 0x31<y) ? 0 : 1. Game-map window 0x50 x 0x32 (cf. the 0x140 x 200
     * Map_GetTile @ 11e8:0dcf buffer gated by map_in_bounds above). */
    if (x < 0 || x > 0x4f || y < 0 || y > 0x31)
        return 0;
    return 1;
}

int map_check_tile_improved(int tile, int x, int y, int seed)
{
    /* Orig Map_CheckTileImproved @ 1110:1c72: (y<2 || 0x2f<y ||
     * (x&3)*4+(y&3) != (seed+(x>>2)*0xd+(y>>2)*0xb & 0xf)) ? 0 : 1.
     * tile (Map_GetTileClass @ 1110:16c0 kind) is kept for parity but
     * unused in the predicate (orig ignores param_1, see decompile).
     * seed is DAT_13c0_0000. Selects the 0x1836 irrigated table in
     * City_TileYield @ 1030:7c28 (see city_tile_base_yield). */
    int want;
    int have;

    (void)tile;
    if (y < 2 || y > 0x2f)
        return 0;
    want = (x & 3) * 4 + (y & 3);
    have = (seed + (x >> 2) * 0xd + (y >> 2) * 0xb) & 0xf;
    return want == have ? 1 : 0;
}

int map_get_occupancy_flags(int x, int y)
{
    /* Orig Map_GetOccupancyFlags @ 1110:199b: lo = Map_GetTile(x,y+100),
     * hi = Map_GetTile(x,y+0x96); return lo + hi*0x10. Bit contract per
     * City_TileYield @ 1030:7c28: &1 city/occupied, &2 food, &4 shield,
     * &8 road, &0x10 holders, &0x40 pollution (upper layer). OOB (-1)
     * folds to 0 here (orig reads the 0x2e sentinel byte, exact value
     * TBD vs dump, never faked as real occupancy). */
    int lo = map_get_tile(x, y + 100);
    int hi = map_get_tile(x, y + 0x96);

    if (lo < 0)
        lo = 0;
    if (hi < 0)
        hi = 0;
    return (lo & 0xff) + (hi & 0xff) * 0x10;
}

int map_get_continent_id(int x, int y)
{
    /* Orig Map_GetContinentId @ 1110:1d99: Map_GetTile(x,y+0x32) (the
     * continent layer; game map is 0x50 x 0x32 so +0x32 rows lands on
     * the id plane). OOB -> -1 (orig sentinel TBD). Feeds
     * City_ChooseProduction @ 1170:05ca gates + Map_GetOceanOrContVal @
     * 1110:1dbd. */
    return map_get_tile(x, y + 0x32);
}

int map_sign_step(int d)
{
    /* Orig Util_SignStep @ 1110:1c4b: d<1 ? (d<0 ? -1 : 0) : 1. Used by
     * Unit_StepTowardGoal @ 1128:0000 (player-adjacent resolve) and
     * Unit_AIPlayerTurn @ 1078:4f0f (home-delta steering via
     * Map_GetContinentId-matched city deltas). */
    if (d < 1)
        return d < 0 ? -1 : 0;
    return 1;
}

civ_error_t map_clear_occupancy_bits(int x, int y, unsigned mask)
{
    /* Orig Map_ClearOccupancyBits @ 1110:1b36: if (mask&0xf)
     * Map_SetTile(x,y+100, tile&~(mask&0xf)); if (mask&0xf0)
     * Map_SetTile(x,y+0x96, tile&~((mask>>4)&0xf)). Read-modify-write
     * via map_get_tile/map_set_tile; OOB silently ignored (same as
     * map_set_tile @ 11e8:0e19). */
    int lo;
    int hi;

    if ((mask & 0xfu) != 0u) {
        lo = map_get_tile(x, y + 100);
        if (lo >= 0)
            map_set_tile(x, y + 100,
                         (uint8_t)((unsigned)lo & ~(mask & 0xfu)));
    }
    if ((mask & 0xf0u) != 0u) {
        hi = map_get_tile(x, y + 0x96);
        if (hi >= 0)
            map_set_tile(x, y + 0x96,
                         (uint8_t)((unsigned)hi & ~((mask >> 4) & 0xfu)));
    }
    return CIV_OK;
}

/* --- Chain 2026-10-16 (4): wrap/lookup/ZOC-detail/mirror/owner ---
 * OS-free 1:1 LIVE ports. No globals, no exit(), C17. See map.h chain
 * block for callers/callees/addrs. */

int map_wrap_x(int x)
{
    /* Orig Map_WrapX @ 1128:177e: single-step X wrap on the 0x50 game-map
     * width (Y never wraps; NOT a modulo: +-0x100 stays OOB by design).
     * Callers: ScanZOCDetail @ 11c0:2437 + UpdateVisibility @ 11c0:0395
     * neighbour tables, ResolveAttack animation wrap, DrawSingle @
     * 1110:0dba. */
    if (x < 0)
        x += CIV_MAP_WRAP_W;
    else if (x > 0x4f)
        x -= CIV_MAP_WRAP_W;
    return x;
}

int map_unit_slot_from_tile(int tile)
{
    /* Orig Map_GetUnitAt @ 1110:18ce: tile(x+0xa0,y) hint (bit3 set -> civ
     * slot bits0-2, else -1). Stale hints (unit moved without tile clear)
     * must be verified via the Find path (0x80 x/y scan, clears to 0). */
    if (tile < 0)
        return -1;
    if ((tile & 8) == 0)
        return -1;
    return tile & 7;
}

int map_tile_owner_from_tile(int tile)
{
    /* Orig Map_GetTileOwner @ 1110:16ee: tile(x+0xa0,y)&7 map-layer owner
     * (fast path for the 0395 tail City_FindByTile @ 11c8:00c4 gate); city
     * @ 1420:5507 + unit slot civ authoritative. */
    if (tile < 0)
        return -1;
    return tile & 7;
}

int map_scan_zoc_detail(int self_civ, int self_is_sea,
                        const int *nbr_is_sea, const int *nbr_has_unit,
                        const int *nbr_civ, int n)
{
    /* Orig Map_ScanZOCDetail @ 11c0:2437 enemy rule (occupancy &1 city gate
     * stays in Map_CheckZOC @ 11c0:23fd caller, LIVE in game.h): 8-neighbour
     * scan with sea match (GetTileClass @ 1110:16c0 == 10) + GetUnitAt @
     * 1110:18ce != -1 + owner != self -> 1. No diplomat/caravan/sea
     * exceptions beyond the sea match; ZOC/occupancy/ocean gates stay in
     * Unit_StepTowardGoal @ 1128:0000. Player reveal side effects
     * (seen-bit OR + Map_RevealCenter @ 1110:1461) stay Win16. */
    int i;
    int self_sea;

    if (!nbr_is_sea || !nbr_has_unit || !nbr_civ || n != 8)
        return -1;
    self_sea = self_is_sea != 0;
    for (i = 0; i < 8; i++) {
        int sea;

        if (!nbr_has_unit[i])
            continue;
        sea = nbr_is_sea[i] != 0;
        if (sea != self_sea)
            continue;
        if (nbr_civ[i] == self_civ)
            continue;
        return 1;
    }
    return 0;
}

civ_error_t map_mirror_occupancy(int x, int y)
{
    /* Orig Map_MirrorOccupancy @ 1110:1a2f: tile(x+0x50,y+100) =
     * tile(x,y+100), tile(x+0x50,y+0x96) = tile(x,y+0x96). OOB sides fold
     * via map_get_tile (-1 skipped) / map_set_tile (ignored). */
    int lo = map_get_tile(x, y + 100);
    int hi = map_get_tile(x, y + 0x96);

    if (lo >= 0)
        map_set_tile(x + 0x50, y + 100, (uint8_t)lo);
    if (hi >= 0)
        map_set_tile(x + 0x50, y + 0x96, (uint8_t)hi);
    return CIV_OK;
}

civ_error_t map_set_tile_owner(int x, int y, int civ)
{
    /* Orig Map_SetTileOwner @ 1110:171f: tile(x+0xa0,y) = (old&8)|civ.
     * Backs Unit_SpawnUnit place + City_DoRevolt moves. OOB read folds to
     * civ alone (sentinel TBD); OOB write ignored like Map_SetTile. */
    int old;

    if (civ < 0 || civ > 7)
        return CIV_ERR_RANGE;
    old = map_get_tile(x + 0xa0, y);
    if (old < 0)
        old = 0;
    map_set_tile(x + 0xa0, y, (uint8_t)((old & 8) | civ));
    return CIV_OK;
}

civ_error_t map_place_unit_marker(int x, int y, int civ)
{
    /* Orig Map_PlaceUnitAt @ 1110:1767 marker half: tile(x+0xa0,y) =
     * (civ|8) (FindUnitAt verify + Unit_LinkStack @ 1050:1105 link need
     * unit records -> game.h unit_stack_insert). OOB ignored. */
    if (civ < 0 || civ > 7)
        return CIV_ERR_RANGE;
    map_set_tile(x + 0xa0, y, (uint8_t)(civ | 8));
    return CIV_OK;
}

/* --- Chain 2026-10-16 (7): view origin + scroll clamp. --- */

civ_error_t map_set_view_origin(int px, int py, int *tile_x_out,
                                int *tile_y_out, int *off_x_out,
                                int *off_y_out)
{
    /* Orig Map_SetViewOrigin @ 1208:20b1: DAT_1330_4010 = px/0x20,
     * DAT_1330_400e = py/0x20, DAT_1408_00c6 = px%0x20, DAT_1408_00c4 =
     * py%0x20 (x86 IDIV trunc toward zero; C / and % match). Clamp tail
     * Map_ClampViewScroll @ 1208:213b lives below. */
    if (!tile_x_out || !tile_y_out || !off_x_out || !off_y_out)
        return CIV_ERR_NOENT;
    *tile_x_out = px / 0x20;
    *tile_y_out = py / 0x20;
    *off_x_out = px % 0x20;
    *off_y_out = py % 0x20;
    return CIV_OK;
}

int map_clamp_view_x(int tx)
{
    /* Orig Map_ClampViewScroll @ 1208:213b X: < 0 -> +0x50, > 0x4f ->
     * -0x50 (single-step, NOT modulo; Y never wraps). Same as map_wrap_x
     * @ 1128:177e (kept separate for RE-map fidelity). */
    if (tx < 0)
        return tx + 0x50;
    if (tx > 0x4f)
        return tx - 0x50;
    return tx;
}

int map_clamp_view_y_plain(int ty)
{
    /* Orig same without Tech 8 (Tech_HasTech @ 1170:3181(pciv,8) == 0):
     * < -0x19 -> -0x19, > 0x4b -> 0x4b. */
    if (ty < -0x19)
        return -0x19;
    if (ty > 0x4b)
        return 0x4b;
    return ty;
}

civ_error_t map_clamp_view_y_scroll(int ty, int pix_off, int scroll_max,
                                    int *ty_out, int *off_out)
{
    /* Orig same with Tech 8: GETSCROLLRANGE max + SETSCROLLPOS sync
     * (Win16 handles stay caller-side); pure clamp: ty < 0 -> 0, then
     * ty*0x20+off > max -> ty = max/0x20, off = max%0x20. */
    long pos;

    if (!ty_out || !off_out)
        return CIV_ERR_NOENT;
    if (scroll_max < 0)
        return CIV_ERR_RANGE;
    if (ty < 0)
        ty = 0;
    pos = (long)ty * 0x20L + (long)pix_off;
    if (pos > (long)scroll_max) {
        ty = scroll_max / 0x20;
        pix_off = scroll_max % 0x20;
    }
    *ty_out = ty;
    *off_out = pix_off;
    return CIV_OK;
}

civ_error_t map_clamp_view_scroll(int tx, int ty)
{
    (void)tx;
    (void)ty;
    /* TODO: Orig Map_ClampViewScroll @ 1208:213b (Menu_CheckCityNameId +
     * GLOBALLOCK port rect + GETSCROLLRANGE/SETSCROLLPOS + Tech 8 branch +
     * Util_MemCopyFar @ 1000:137f). Win16 scroll handles -> SDL view TBD.
     * Pure X/Y halves live in map_clamp_view_x/y_* above (LIVE). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t map_paint_tiles_clipped(int left, int top, int right, int bottom)
{
    (void)left;
    (void)top;
    (void)right;
    (void)bottom;
    /* TODO: Orig Map_PaintTilesClipped @ 1208:1534 (tile-range paint from
     * 1330:4010/400e + 1408:00c6/c4: per-tile SETRECT 0x20 + clip @
     * 1100:0685 + OOB/fog blank vs Map_RevealCenter @ 1110:1461 +
     * Map_DrawCityLabels @ 1208:1f64 + PresentRect @ 10d0:0854). GfxPort
     * blits -> SDL TBD. Clip math lives in video_clip_rect (LIVE). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t map_repaint_view_shuffled(int mode)
{
    (void)mode;
    /* TODO: Orig Map_RepaintViewShuffled @ 1208:1866 (mode 2 present / 1
     * shuffled via mem_alloc_lock + Rng_Range @ 11c8:003d + 0 clip @
     * 1100:0685 + RevealCenter/fill + scroll sync). GfxPort -> SDL TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t map_draw_city_labels(void)
{
    /* TODO: Orig Map_DrawCityLabels @ 1208:1f64 (labels over DAT_1408_0000
     * via IntersectRect @ 1100:0685 + Text_DrawAligned @ 1098:0ed1).
     * GDI text -> SDL_ttf TBD. Box math lives in map_city_label_box +
     * map_city_label_res_id below (LIVE). */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (8): paint order + city-label format (goals 2/3) ---
 * OS-free 1:1 LIVE. No globals, no exit(), C17. See map.h chain block. */

int map_paint_start(int origin, int off)
{
    /* Orig Map_PaintTilesClipped @ 1208:1534: local_c = 1330:4010 +
     * off/0x20 (x86 IDIV trunc toward zero). */
    return origin + off / 0x20;
}

int map_paint_end(int start, int span)
{
    /* Orig same: end = start + span/0x20 + 1 (+1 when span%0x20 != 0).
     * span is (right-left) / (bottom-top) of the paint rect. */
    int end = start + span / 0x20 + 1;

    if (span % 0x20 != 0)
        end++;
    return end;
}

int map_paint_tile_px(int tile, int origin)
{
    /* Orig same: px = (tile - origin)*0x20 (local_8/local_a accumulators
     * advancing +0x20 per tile). */
    return (tile - origin) * 0x20;
}

int map_tile_needs_blank(int ty, int reveal, int seen)
{
    /* Orig Map_PaintTilesClipped @ 1208:1534 + Map_RepaintViewShuffled @
     * 1208:1866 fog gate: (ty < 0 || 0x31 < ty) || (reveal == 0 &&
     * (seen & 1) == 0) -> blank via FUN_1000_14f8/1100:0876 (mode 1) or
     * FILLRECT stock (mode 0), else Map_RevealCenter @ 1110:1461 (via
     * Map_WrapX @ 1128:177e X-only + Map_RefreshTileView @ 1110:0448 ->
     * City_DrawTileView @ 1110:04d0). seen is the 1408:00cc byte. */
    if (ty < 0 || ty > 0x31)
        return 1;
    if (reveal == 0 && (seen & 1) == 0)
        return 1;
    return 0;
}

int map_repaint_mode_is_present(int mode)
{
    /* Orig Map_RepaintViewShuffled @ 1208:1866: param_3 == 2 -> direct
     * GfxPort_PresentRect @ 10d0:0854 + GfxPort_ClearActive @ 10d0:09a2. */
    return mode == 2;
}

int map_repaint_mode_is_shuffled(int mode)
{
    /* Orig same: param_3 == 1 -> Fisher-Yates shuffled tile order (order
     * table via mem_alloc_lock + Rng_Range @ 11c8:003d swaps). */
    return mode == 1;
}

int map_repaint_mode_is_rowmajor(int mode)
{
    /* Orig same: param_3 == 0 -> row-major loops (outer X from -off_x,
     * inner Y from -off_y, +0x20 px each) + Map_DrawCityLabels @
     * 1208:1f64 + PresentRect. */
    return mode == 0;
}

int map_shuffle_grid_count(int w_tiles, int h_tiles)
{
    /* Orig Map_RepaintViewShuffled @ 1208:1866 shuffled head: n =
     * (local_14-local_10) * local_8 (rows * cols). */
    if (w_tiles <= 0 || h_tiles <= 0)
        return -1;
    return w_tiles * h_tiles;
}

civ_error_t map_shuffle_tile_at(int order_val, int w_tiles,
                                int *gx_out, int *gy_out)
{
    /* Orig same pop: gx = order[n] % w, gy = order[n] / w (then tile =
     * (gx+local_e, gy+local_10), px = gx*0x20-off). */
    if (!gx_out || !gy_out)
        return CIV_ERR_NOENT;
    if (w_tiles <= 0 || order_val < 0)
        return CIV_ERR_RANGE;
    *gx_out = order_val % w_tiles;
    *gy_out = order_val / w_tiles;
    return CIV_OK;
}

int map_shuffle_px(int g, int off)
{
    /* Orig same: px = g*0x20 - off (off is 1408:00c6/c4). */
    return g * 0x20 - off;
}

civ_error_t map_refresh_tile_px(int tx, int origin, int off, int *px_out)
{
    /* Orig Map_RefreshTileView @ 1110:0448: wx = Map_WrapX(tx -
     * 1330:4010)*0x20 - 1408:00c6 (X caller wraps via map_wrap_x @
     * 1128:177e; Y same without wrap). */
    if (!px_out)
        return CIV_ERR_NOENT;
    *px_out = (tx - origin) * 0x20 - off;
    return CIV_OK;
}

civ_error_t map_city_label_box(int x, int y, civ_rect_t *out)
{
    /* Orig Map_DrawCityLabels @ 1208:1f64: SETRECT(x+0x34, y+0xa6,
     * x+0x20, y-0x86, local_e) -> left x+0x20, top y-0x86, right x+0x34,
     * bottom y+0xa6 (x/y are the DAT_1408_0000 entry words). */
    if (!out)
        return CIV_ERR_NOENT;
    out->left = x + 0x20;
    out->top = y - 0x86;
    out->right = x + 0x34;
    out->bottom = y + 0xa6;
    return CIV_OK;
}

int map_city_label_res_id(int owner)
{
    /* Orig same text: Util_StrCopy @ 1000:36b2(owner*0x20+0x4da) before
     * FUN_11e8_0b40(8) + Text_DrawAligned @ 1098:0ed1 (civ adjective, not
     * City_FormatName @ 1048:0101 Pascal-join). */
    if (owner < 0)
        return -1;
    return owner * 0x20 + 0x4da;
}

/* --- Chain 2026-10-16 (17): tile icon cache (goal 3) ---
 * See map.h block. OS-free 1:1 (no Win16, no globals, no exit()). */

int map_tile_cache_capacity(void)
{
    /* Orig Map_DisposeTileCache @ 1110:0127 loop + Map_AllocTileCacheEntry
     * @ 1110:01a3 append bound: 0xfa = 250 slotow. */
    return CIV_TILE_CACHE_N;
}

int map_tile_cache_slot_len(void)
{
    /* Orig 12-B stride (undefined2 i*6 / undefined4 i*3 w disasm). */
    return CIV_TILE_CACHE_SLOT_LEN;
}

int map_tile_cache_match(const map_tile_slot_t *slot, int32_t key, int sub)
{
    /* Orig Map_FindTileCache @ 1110:030c: (*(long *)(slot+1) == param_1)
     * && ((char)slot[5] == param_2) (2-B int: +2 B key, +10 B sub). */
    if (!slot)
        return 0;
    return (slot->key == key && slot->sub == sub) ? 1 : 0;
}

int map_tile_cache_is_empty(const map_tile_slot_t *slot)
{
    /* Orig Map_DisposeTileCache @ 1110:0127 + Alloc victim: DELETEOBJECT
     * tylko gdy handle != 0 (puste skipowane). */
    if (!slot)
        return 1;
    return (slot->handle == 0) ? 1 : 0;
}

int map_tile_cache_find(const map_tile_slot_t *slots, int n, int used,
                        int32_t key, int sub)
{
    /* Orig Map_FindTileCache @ 1110:030c scan 0..DAT_1330_0002-1 (hit ->
     * lru refresh + 2e40++, miss -> 2e44++ + 0; liczniki caller-side). */
    int lim;
    int i;

    if (!slots || n <= 0 || used <= 0)
        return -1;
    lim = (used < n) ? used : n;
    for (i = 0; i < lim; i++) {
        if (slots[i].key == key && slots[i].sub == sub)
            return i;
    }
    return -1;
}

int map_tile_cache_victim(const map_tile_slot_t *slots, int n, int used)
{
    /* Orig Map_AllocTileCacheEntry @ 1110:01a3: used < 0xfa -> slot[used]
     * (append + 2e42++), else min-lru victim (init 0x7fffffff; DELETEOBJECT
     * starego handle caller-side). */
    int lim;
    int i;
    int best;
    int32_t best_lru;

    if (!slots || n <= 0 || used < 0)
        return -1;
    if (used < n && used < CIV_TILE_CACHE_N)
        return used;
    lim = (used < n) ? used : n;
    if (lim <= 0)
        return -1;
    best = 0;
    best_lru = slots[0].lru;
    for (i = 1; i < lim; i++) {
        if (slots[i].lru < best_lru) {
            best_lru = slots[i].lru;
            best = i;
        }
    }
    return best;
}

civ_error_t map_tile_cache_dispose(map_tile_slot_t *slots, int n,
                                   int *used_io, int *tick_io,
                                   int *total_io)
{
    /* Orig Map_DisposeTileCache @ 1110:0127: 2e42 = 0 + per-slot handle =
     * 0 + key (a2ec) = 0 + lru (a2f0) = 0 + 1330:0004 = 0 + 1330:0002 = 0
     * (DELETEOBJECT per-slot caller-side, jak font_free_slots). */
    int i;

    if (!slots) {
        if (n > 0)
            return CIV_ERR_NOENT;
        n = 0;
    }
    if (n < 0 || n > CIV_TILE_CACHE_N)
        return CIV_ERR_RANGE;
    for (i = 0; i < n; i++) {
        slots[i].handle = 0;
        slots[i].key = 0;
        slots[i].lru = 0;
        slots[i].sub = 0;
    }
    if (used_io)
        *used_io = 0;
    if (tick_io)
        *tick_io = 0;
    if (total_io)
        *total_io = 0;
    return CIV_OK;
}

int map_tile_cache_next_tick(int tick)
{
    /* Orig DAT_1330_0004++ w Alloc @ 1110:01a3 i Find-hit @ 1110:030c. */
    return tick + 1;
}

/* --- Chain 2026-10-16 (20): SmMap click-to-center (goal 2). See map.h.
 * OS-free 1:1 (no Win16, no globals, no exit()). */

int smmap_click_tile_delta(int rect_lo, int rect_hi, int click, int scale)
{
    /* Orig SmMap_CenterOnClick @ 1210:056e: ((b3cc+b3d0)/2 - clickX) /
     * DAT_1410_0000 (Y analogicznie b3ce/b3d2). */
    if (scale == 0)
        return 0;
    return ((rect_lo + rect_hi) / 2 - click) / scale;
}

int smmap_center_target(int view, int delta)
{
    /* Orig same: Map_RefreshAllViews(player, 1330:4010-dx, 1330:400e-dy). */
    return view - delta;
}

int smmap_click_scale(void)
{
    /* Orig DAT_1410_0000 image (inspect 2 B: 02 00). */
    return 2;
}

/* --- Chain 2026-10-16 (28): Path_FindCorridor BFS kernels (goal 2) ---
 * See map.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()).
 * Caller owns queue u8[256] + visited u8[256]; tile reads (class/road/
 * occupancy/cost/continent) wchodza jako gotowe flagi. */

static int corridor_abs(int v)
{
    /* Orig x86 abs shape (v ^ (v >> 0xf)) - (v >> 0xf) dla 16-bit far int. */
    return v < 0 ? -v : v;
}

int path_corridor_gate_ok(int dx, int dy)
{
    /* Orig Path_CheckContinentLink @ 1128:16d0 head: if ((|dx| < 8) ||
     * (|dy| < 8)) proceed (68f1/2 scratch + FindCorridor), else 0xffff. */
    if (corridor_abs(dx) < 8)
        return 1;
    if (corridor_abs(dy) < 8)
        return 1;
    return 0;
}

int path_corridor_window_ok(int dx, int dy)
{
    /* Orig Path_FindCorridor @ 1128:10de neighbour filter: per-axis
     * (d ^ (d >> 0xf)) - (d >> 0xf) < 8 (X wrap via Map_WrapX @ 1128:177e
     * caller-side, Y never wraps). */
    if (corridor_abs(dx) >= 8)
        return 0;
    if (corridor_abs(dy) >= 8)
        return 0;
    return 1;
}

int path_corridor_sea_ok(int is_sea_tile, int unit_is_sea, int occupancy)
{
    /* Orig Path_FindCorridor @ 1128:10de expander: (tileClass == 10) ==
     * (1a3a == 2) || occ & 1 (city tile = most laczacy domeny; FUN_1110_197a
     * to road&8, nie continent — continent gate siedzi w callerze
     * BuildContinentLinks @ 1028:12c6). */
    int tile_sea = (is_sea_tile != 0) ? 1 : 0;
    int unit_sea = (unit_is_sea != 0) ? 1 : 0;

    if (tile_sea == unit_sea)
        return 1;
    if ((occupancy & 1) != 0)
        return 1;
    return 0;
}

int path_corridor_step_cost(int road_here, int road_nbr, int unit_fast,
                            int base_cost)
{
    /* Orig: iVar10 = road&8 here (FUN_1110_197a = Map_GetOccupancyFlags &
     * 8); if ((iVar10 == 0) || (nbr_road == 0)) fast ? +3 : +base*3
     * (base = sbyte 1420:17d4[class]) else +1 (road-to-road). */
    int here = (road_here != 0) ? 1 : 0;
    int nbr = (road_nbr != 0) ? 1 : 0;

    if (here != 0 && nbr != 0)
        return 1;
    if (unit_fast != 0)
        return 3;
    return base_cost * 3;
}

int path_corridor_queue_adv(int v)
{
    /* Orig Path_FindCorridor @ 1128:10de ring DAT_1420_afaa/aeaa:
     * local_6 = (local_6+1) & 0xff (256 entries, head + tail, stop at
     * empty or 0xe1 iters). */
    return (v + 1) & 0xff;
}

int path_corridor_should_relax(int old_cost, int new_cost)
{
    /* Orig Path_FindCorridor @ 1128:10de relax: if ((visited == 0) ||
     * (new < visited)) { visited = new; enqueue; } (visited 0 = unseen;
     * cost bytes, budget = param_3). */
    if (old_cost == 0)
        return 1;
    return (new_cost < old_cost) ? 1 : 0;
}

int path_continent_link_result(int dir, int cost)
{
    /* Orig Path_CheckContinentLink @ 1128:16d0 tail: iVar2 =
     * Path_FindCorridor(0, 0x7f, budget) (scratch unit alias 68f1/2);
     * DAT_68f3 = 0xff; cost = DAT_1420_aea8; if (dir == -1) cost = 0xffff.
     * Caller gate: worldgen_continent_link_allowed (cost != -1 && < 0x14).
     * Pelny probe (x, y, continent) to path_check_continent_link w
     * worldgen.h (STUB 0x14); to jest czysty ogon dir/cost. */
    if (dir == -1)
        return -1;
    return cost;
}

int map_get_ocean_or_cont_val(int is_sea, int continent,
                              const int *sea_vals, const int *land_vals,
                              int n)
{
    /* Orig Map_GetOceanOrContVal @ 1110:1dbd: class = Map_GetTileClass @
     * 1110:16c0; sea (== 10) -> u16 1420:43b4[cont] (cont = Map_GetContinentId
     * @ 1110:1d99), land -> u16 0x4334 + cont*2. OOR/NULL -> -1. */
    if (!sea_vals || !land_vals)
        return -1;
    if (continent < 0 || continent >= n)
        return -1;
    if (is_sea != 0)
        return sea_vals[continent];
    return land_vals[continent];
}
