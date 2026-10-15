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

void map_shutdown(void)
{
}

civ_error_t wdwstatus_calc_upper_rect(civ_rect_t *out)
{
    (void)out;
    /* TODO: FUN_1218_0956 @ 1218:0956 (client-rect -> centered frame,
     * cy+0x1e variant; INFLATERECT -1,-1; see map.h). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t wdwstatus_calc_lower_rect(civ_rect_t *out)
{
    (void)out;
    /* TODO: FUN_1218_0a06 @ 1218:0a06 (cy*2+0x46/0x1e variant). */
    return CIV_ERR_UNSUPPORTED;
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
