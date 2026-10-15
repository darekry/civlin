/* Map windows + tile buffer: WDWMAPPROC / WDWSMMAPPROC / WDWSTATUSPROC.
 *
 * Ghidra anchors (civ.exe), renamed 2026-10-15 (Orig FUN_* -> New):
 *   WdwMap_Open      @ 1208:0af1 (CREATEWINDOW CIVILIZATION + scroll ranges
 *                        + GfxPort; log "WdwMapOpen ...wdwmap.c")
 *   WdwSmMap_Open    @ 1210:025b (CREATEWINDOW World Map; "WdwSmMapOpen ...")
 *   WdwStatus_Open   @ 1218:0386 (CREATEWINDOW Status; "WdwStatusOpen ...")
 *   WdwStatus_Refresh@ 1218:0801 (dirty flag +0x2c; calls Game_UpdateStatusPanel
 *                        + Unit_ShowInfoPopup; blits via 10d0:0854/09a2)
 *   Map_CenterOnTile @ 1110:03ae (centers x/y, then Map_RefreshAllViews)
 *   Map_RefreshAllViews @ 1110:03e9 (civ bit -> 1408:00cc; 1088:13d5 refresh +
 *                        GETFREESPACE + 1208:17da + 1210:09b4)
 *   MainPort_ApplyPalette @ 1200:00a8 (GLOBALLOCK -> Palette_AnimateRange)
 *   MainPort_GetOrCreate @ 1098:114a (see boot.h)
 *   exports: WDWMAPPROC @ 1208:0054, WDWSMMAPPROC @ 1210:0000,
 *            WDWSTATUSPROC @ 1218:0000, TIMERPROC @ 1008:0d68
 * Win16 REGISTERCLASS + SETWINDOWWORD/GETWINDOWWORD -> SDL3 window/renderer.
 *
 * Tile buffer: 64 KB far block g_segMapBuf:g_offMapBuf @ 1420:b396/b394
 * (alloc mem_alloc_lock(64000) in Game_InitGlobalTables), stride 0x140,
 * 0x140 x 200 cells. Game map proper is 0x50 x 0x32; the rest is scratch
 * (island copy at +0xa0, labels at +10/+0x32, see worldgen.h).
 */
#ifndef CIVLIN_MAP_H
#define CIVLIN_MAP_H

#include <stddef.h>
#include <stdint.h>

#include "civ_common.h"
#include "win16_compat.h"

#define CIV_MAP_BUF_W 0x140u
#define CIV_MAP_BUF_H 200u
#define CIV_MAP_BUF_LEN 64000u

civ_error_t map_init(void);
/* --- WndProcs + timer (packet Q 2026-10-15) ---
 * Win16 REGISTERCLASS dispatch -> SDL3 event/widget TBD; stubs return
 * WIN16_MSG_DEFAULT (DefWindowProc path). No globals, no exit(),
 * read-only (no tile-buffer writes; paint/scroll only invalidate). */

/* Orig: FUN_1208_0054 @ 1208:0054 -> WDWMAPPROC (map_wndproc_main).
 * Main "CIVILIZATION" window proc (class registered by
 * App_RegisterWindowClasses @ 1098:06f8, opened by map_open_main @
 * 1208:0af1). Message table TBD from decompile; expected WM_CREATE /
 * WM_PAINT (blits via 10d0:0854/09a2 on the Map_RefreshAllViews @
 * 1110:03e9 path) + WM_HSCROLL / WM_VSCROLL (scroll ranges set in
 * WdwMap_Open) + WM_TIMER (200 ms SETTIMER from WdwMap_Open, forwards
 * to TIMERPROC @ 1008:0d68 / map_timer_proc below) + WM_DESTROY
 * (KILLTIMER in WinMain_Shutdown @ 1008:0329). */
win16_msg_result_t map_wndproc_main(int msg, long wparam, long lparam);
/* Orig: FUN_1210_0000 @ 1210:0000 -> WDWSMMAPPROC (map_wndproc_small).
 * "World Map" child proc (opened by map_open_small @ 1210:025b).
 * Message table TBD; expected WM_PAINT (minimap layer Map_SetTile
 * x+0xf0 via the 1210:09b4 repaint from Map_RefreshAllViews @
 * 1110:03e9) + click-to-center (Map_CenterOnTile @ 1110:03ae) +
 * WM_TIMER passthrough. */
win16_msg_result_t map_wndproc_small(int msg, long wparam, long lparam);
/* Orig: FUN_1218_0000 @ 1218:0000 -> WDWSTATUSPROC (map_wndproc_status).
 * "Status" child proc (opened by map_open_status @ 1218:0386).
 * Message table TBD; expected WM_PAINT (frames wdwstatus_calc_upper /
 * lower_rect @ 1218:0956/0a06 + Text_DrawStatusRow @ 11f8:0278 rows via
 * Game_UpdateStatusPanel @ 1088:13d5 / map_refresh_status @ 1218:0801). */
win16_msg_result_t map_wndproc_status(int msg, long wparam, long lparam);
/* Orig: FUN_1008_0D68 @ 1008:0d68 -> TIMERPROC (map_timer_proc).
 * 200 ms SETTIMER callback installed by WdwMap_Open @ 1208:0af1, torn
 * down by KILLTIMER in WinMain_Shutdown @ 1008:0329, pumped from the
 * WinMain @ 1008:0000 message loop. Tick drives the per-frame pump
 * game_tick (see game.h) -> Game_TurnLoopTick @ 1088:0000 /
 * game_turn_tick. SDL timer bridge TBD; stub CIV_ERR_UNSUPPORTED.
 * No globals, no exit(), read-only. */
civ_error_t map_timer_proc(void);
void map_shutdown(void);

/* --- tile buffer (implemented 1:1; bounds match the originals) --- */

/* Raw buffer for save/worldgen (64000 B). */
uint8_t *map_buffer(void);
size_t map_buffer_len(void);
/* Map_GetTile @ 11e8:0dcf. Tile value 0..255, -1 when OOB. */
int map_get_tile(int x, int y);
/* Map_SetTile @ 11e8:0e19. OOB writes are ignored (as in the original). */
void map_set_tile(int x, int y, uint8_t tile);
/* Map_FillRect @ 1028:1d15. w*h fill with stride wrap. */
void map_fill_rect(int x, int y, unsigned w, unsigned h, uint8_t tile);
/* Map_DrawRectBorder @ 1028:1ca7. 4x Map_FillRect (top/bottom rows,
 * left/right cols). */
void map_draw_rect_border(int x, int y, int w, int h, uint8_t tile);
/* Map_TryPlaceIsolatedTile @ 1028:05e7. Sets tile 1 when none of the 4
 * diagonal neighbours is 1. Returns 1 when placed, 0 otherwise. */
int map_try_place_isolated_tile(int x, int y);

/* --- status-panel frame rects (Game_UpdateStatusPanel callees) --- */

/* Win16 RECT equivalent for the 1218:0956/0a06 calculators. */
typedef struct civ_rect {
    int left;
    int top;
    int right;
    int bottom;
} civ_rect_t;

/* Orig: FUN_1218_0956 @ 1218:0956 -> wdwstatus_calc_upper_rect.
 * Derives a centered frame from the status-port client rect
 * (GLOBALLOCK + FUN_1000_137f): cx = ((w)-0x8c)/2, cy =
 * clamp(((h)-0xf0)/4,0,10); SETRECT(cy+0x1e, ...) then INFLATERECT(-1,-1).
 * Sole caller Game_UpdateStatusPanel @ 1088:1479; result feeds
 * FUN_11f8_0278(..., DAT_12d8_45f4) (first text row). */
civ_error_t wdwstatus_calc_upper_rect(civ_rect_t *out);

/* Orig: FUN_1218_0a06 @ 1218:0a06 -> wdwstatus_calc_lower_rect.
 * Same derivation, second frame at cy*2+0x46/0x1e. Sole caller
 * Game_UpdateStatusPanel @ 1088:1536; feeds FUN_11f8_0278(...,
 * DAT_12d8_45fb) (second text row). */
civ_error_t wdwstatus_calc_lower_rect(civ_rect_t *out);

/* Orig: FUN_1128_10de @ 1128:10de -> Path_FindCorridor.
 * Corridor pather called from unit_step_toward_goal @ 1128:0000 with a
 * 999-step budget. Params: owning civ, unit index (record at
 * 1420:62fc + civ*0x600 + idx*0xc, goal at +5/+6), max_steps search
 * budget. Pure grid search over the 64 KB tile buffer (no Win16).
 * Returns a direction 1..8, 0 (arrived/exhausted), or 0xffff (no
 * path); exact corridor-writeback layout TBD, see Ghidra. */
int path_find_corridor(int civ, int unit_idx, int max_steps);

/* Orig: FUN_1128_081c @ 1128:081c -> Path_IsPassable.
 * Passability check used by unit_step_toward_goal @ 1128:0000 after the
 * corridor probe. Params: owning civ plus tile coordinates. OS-free:
 * 1 when the tile can be entered, 0 otherwise (OOB is 0). */
int path_is_passable(int civ, int x, int y);

/* --- tile queries (packet P 2026-10-15) ---
 * Callees of unit_step_toward_goal @ 1128:0000 + worldgen links +
 * visibility. OS-free queries are ported 1:1 on the tile buffer via
 * map_get_tile; view/unit-state tails stay CIV_ERR_UNSUPPORTED stubs.
 * No globals, no exit(), read-only (const arrays, no buffer writes). */

/* Orig: FUN_1110_16C0 @ 1110:16c0 -> Map_IsSeaLand.
 * Sea/land match gate for unit_step_toward_goal @ 1128:0000: orig
 * returns the tile kind and the caller tests ==10 (ocean) vs the
 * 0x1a3a sea/land domain. This helper folds the ==10: 1 when
 * map_get_tile(x,y)==10 (sea), 0 otherwise (land or OOB, since
 * map_get_tile returns -1 OOB). OS-free 1:1 via map_get_tile. */
int map_is_sea_land(int x, int y);

/* Orig: FUN_1110_18CE @ 1110:18ce -> Map_IsOccupied.
 * Occupancy gate for unit_step_toward_goal @ 1128:0000. Orig scans
 * the unit stack (records at 1420:62fc + civ*0x600 + idx*0xc, see
 * game.h) with 0x1a3a side gating (exact predicate TBD, see Ghidra).
 * Linux takes the stack explicitly (no globals): xs/ys are n_units
 * entries, NULL/empty means none. Returns 1 when any entry is at
 * (x,y), 0 otherwise. OS-free, read-only. */
int map_is_occupied(int x, int y, const int *unit_xs, const int *unit_ys,
                    int n_units);

/* Orig: FUN_1110_199B @ 1110:199b -> Map_IsCityBlocked.
 * Second occupancy/city gate for unit_step_toward_goal @ 1128:0000
 * (city-record variant of the 1110:18ce gate; city records 0x1c bytes
 * at 1420:5500 + idx*0x1c, see game.h; exact predicate TBD).
 * Linux takes cities explicitly (no globals): 1 when any entry is at
 * (x,y), 0 otherwise (NULL/empty -> 0). OS-free, read-only. */
int map_is_city_blocked(int x, int y, const int *city_xs,
                        const int *city_ys, int n_cities);

/* Orig: FUN_1110_1DBD @ 1110:1dbd -> Map_OceanClearDepth.
 * Ocean >4 clear check for the unit_step_toward_goal @ 1128:0000
 * pather (caller tests result >4). Counts consecutive ocean (10)
 * tiles from (x,y) along the unit step (dx,dy), capped at 5.
 * dx/dy must each be -1..1 and not both 0 (else 0). OOB (-1) breaks
 * the run. OS-free 1:1 via map_get_tile. Returns 0..5. */
int map_ocean_clear_depth(int x, int y, int dx, int dy);

/* Orig: FUN_1110_1C4B @ 1110:1c4b -> Map_ResolveDirect.
 * Player-adjacent direct resolve for unit_step_toward_goal @
 * 1128:0000: when |dx|<2 and |dy|<2 the step resolves without the
 * corridor probe. Params: unit (ux,uy) + goal (gx,gy). Returns dir
 * 1..8 (1=N clockwise: 1=N(0,-1) 2=NE 3=E 4=SE 5=S 6=SW 7=W 8=NW;
 * exact orig numbering TBD, see Ghidra), 0 when arrived (dx==0 &&
 * dy==0), 0xffff when not adjacent. OS-free 1:1, no tile access. */
int map_resolve_direct(int ux, int uy, int gx, int gy);

/* Orig: FUN_1110_1461 @ 1110:1461 -> Map_RevealCenter.
 * Reveal/center helper called from unit_maybe_center_view @
 * 11f0:44d4 only when the unit belongs to the player or is seen
 * (6305 bit). Needs the SDL view/scroll state; stub returns
 * CIV_ERR_UNSUPPORTED. No globals, no exit(), read-only. */
civ_error_t map_reveal_center(int civ, int x, int y);

/* Orig pair: FUN_1110_180E @ 1110:180e + FUN_1110_16EE @ 1110:16ee
 * -> Map_SpotCity. City-spotting pair feeding unit_update_visibility
 * @ 11c0:0395 -> 11c0:0000/11c8:00c4 (see game.h unit_update_visibility;
 * spots cities, flips goals on contact). Needs city records
 * (1420:5500) + seen-bits + minimap layer; stub returns
 * CIV_ERR_UNSUPPORTED. No globals, no exit(), read-only. */
civ_error_t map_spot_city(int civ, int x, int y);

/* Orig: FUN_11f8_0278 @ 11f8:0278 -> Text_DrawStatusRow.
 * Status-panel text row called with the wdwstatus frame rects above
 * (upper @ 1088:1479 + DAT_12d8_45f4, lower @ 1088:1536 + DAT_12d8_45fb).
 * Params: NUL-terminated string, destination frame rect, slot selecting
 * the text row (which DAT_12d8_* source / font row). GDI TEXTOUT ->
 * SDL text render TBD. */
civ_error_t text_draw_status_row(const char *str, const civ_rect_t *rect,
                                 int slot);

/* --- status-text rows (packet D 2026-10-15) ---
 * Row formatters write the NUL-terminated status line into the shared
 * 12d8:24fa line buffer (see game_append_year_text @ 1088:1dd0), then the
 * caller blits via Text_DrawStatusRow @ 11f8:0278 into the 1218:0956/0a06
 * frames. All three need the GDI font selection + TEXTOUT -> SDL text
 * bridge; stubs. */

/* Orig: FUN_1138_0000 @ 1138:0000 -> Status_FormatMainRow.
 * Formats the main status line (caller of game_append_year_text @
 * 1088:1dd0 per game.h:104; year + progress text into 12d8:24fa).
 * Orig writes the fixed 12d8:24fa buffer directly; explicit dst/cap
 * keeps the Linux side free of globals. */
civ_error_t status_format_main_row(char *dst, size_t cap);

/* Orig: FUN_11b0_0000 @ 11b0:0000 -> Status_FormatSecondRow.
 * Second status line (second game_append_year_text caller; same 12d8:24fa
 * buffer contract as above). Exact field layout TBD, see Ghidra. */
civ_error_t status_format_second_row(char *dst, size_t cap);

/* Orig: FUN_11b8_14e5 @ 11b8:14e5 -> Status_FormatGoldRow.
 * Gold/city row (third game_append_year_text caller family; same buffer).
 * Exact gold/city field layout TBD, see Ghidra. */
civ_error_t status_format_gold_row(char *dst, size_t cap);

/* --- window opens / view refresh (stubs; Win16 windowing TBD) --- */

/* Orig: FUN_1208_0AF1 @ 1208:0af1 -> map_open_main (WdwMap_Open).
 * Creates the main map window (CREATEWINDOW "CIVILIZATION"), sets scroll
 * ranges, selects the GfxPort/DC, starts the 200 ms SETTIMER, logs
 * "WdwMapOpen ...wdwmap.c". Win16 USER/GDI -> SDL3 window TBD; stub
 * returns CIV_ERR_UNSUPPORTED. */
civ_error_t map_open_main(void);
/* Orig: FUN_1210_025B @ 1210:025b -> map_open_small (WdwSmMap_Open).
 * Creates the "World Map" child window. Win16 -> SDL3 TBD; stub returns
 * CIV_ERR_UNSUPPORTED. */
civ_error_t map_open_small(void);
/* Orig: FUN_1218_0386 @ 1218:0386 -> map_open_status (WdwStatus_Open).
 * Creates the "Status" child window; feeds WdwStatus_Refresh @ 1218:0801
 * + Game_UpdateStatusPanel @ 1088:13d5. Win16 -> SDL3 TBD; stub returns
 * CIV_ERR_UNSUPPORTED. */
civ_error_t map_open_status(void);
/* Orig: FUN_1218_0801 @ 1218:0801 -> WdwStatus_Refresh.
 * Marks the status window dirty (flag at +0x2c), then
 * Game_UpdateStatusPanel @ 1088:13d5 + Unit_ShowInfoPopup @ 11f0:468c
 * and blits via 10d0:0854/09a2. Win16 USER/GDI -> SDL3 TBD; stub
 * CIV_ERR_UNSUPPORTED. No globals, no exit(), read-only. */
civ_error_t map_refresh_status(void);
/* Orig: FUN_1110_03ae @ 1110:03ae -> Map_CenterOnTile.
 * Centers the map view on tile (x, y) (x - w/2, y - h/2), then
 * Map_RefreshAllViews @ 1110:03e9 (below). Needs the SDL view/scroll
 * state; stub CIV_ERR_UNSUPPORTED. No globals, no exit(), read-only. */
civ_error_t map_center_on_tile(int civ, int x, int y);
/* Orig: FUN_1110_03e9 @ 1110:03e9 -> Map_RefreshAllViews.
 * Sets the civ view bit (-> 1408:00cc), then Game_UpdateStatusPanel @
 * 1088:13d5 + GETFREESPACE guard + 1208:17da + 1210:09b4 view repaints.
 * Win16 windowing -> SDL3 TBD; stub CIV_ERR_UNSUPPORTED. No globals, no
 * exit(), read-only. */
civ_error_t map_refresh_all_views(int civ);
/* Orig: FUN_1200_00a8 @ 1200:00a8 -> MainPort_ApplyPalette.
 * GLOBALLOCK the main-port handle -> Palette_AnimateRange @ 1090:0376
 * (see video.h) range repaint (first, count). Exact locked-handle arg
 * order TBD, see Ghidra. Win16 palette -> SDL3 TBD; stub
 * CIV_ERR_UNSUPPORTED. No globals, no exit(), read-only. */
civ_error_t mainport_apply_palette(int first, int count);

#endif /* CIVLIN_MAP_H */
