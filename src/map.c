/* Map tile buffer (1:1) + window stubs. */
#include "map.h"

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
    (void)msg;
    (void)wparam;
    (void)lparam;
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t map_wndproc_small(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t map_wndproc_status(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    return WIN16_MSG_DEFAULT;
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
    /* TODO: WdwStatus_Refresh @ 1218:0801 (dirty +0x2c, Game_UpdateStatusPanel
     * + Unit_ShowInfoPopup, blit 10d0:0854/09a2). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t map_center_on_tile(int civ, int x, int y)
{
    (void)civ;
    (void)x;
    (void)y;
    /* TODO: Map_CenterOnTile @ 1110:03ae (x - w/2, y - h/2, then
     * Map_RefreshAllViews). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t map_refresh_all_views(int civ)
{
    (void)civ;
    /* TODO: Map_RefreshAllViews @ 1110:03e9 (civ bit -> 1408:00cc,
     * Game_UpdateStatusPanel, GETFREESPACE, 1208:17da + 1210:09b4). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t mainport_apply_palette(int first, int count)
{
    (void)first;
    (void)count;
    /* TODO: MainPort_ApplyPalette @ 1200:00a8 (GLOBALLOCK ->
     * Palette_AnimateRange). */
    return CIV_ERR_UNSUPPORTED;
}
