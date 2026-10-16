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
/* AL2: FNT face + SDL types for text_draw_with_font below (SDL only
 * there, via video bridge; no Win16). boot.h brings SDL + font_fnt_info_t
 * (read-only parity, not edited). */
#include "boot.h"

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
/* --- Chain 2026-10-16 (21): map WM_TIMER interval (goal 3) ---
 * Orig WdwMap_Open @ 1208:0af1 SETTIMER(hwnd main, id 1, 0xC8, NULL;
 * 1208:0e7b) — 200 ms NULL-proc timer = WM_TIMER do map_wndproc_main @
 * 1208:0054 (nie mylic z 5 ms TIMERPROC-proc z WinMain @ 1008:0000, patrz
 * boot.h boot_timer_proc_interval_ms). KILLTIMER w WinMain_Shutdown @
 * 1008:0329. SDL_AddTimer: dokumentacja, bez implementacji (patrz boot.h
 * decyzja). 1:1 200 (ms). No globals, no exit(). */
int map_timer_interval_ms(void);
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

/* --- status dirty-flag + frame metrics (2026-10-16) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode):
 *   WdwStatus_Refresh @ 1218:0801 head (dirty flag at +0x2c of the
 *     GLOBALLOCK @ 1608:000c handle on ctx DAT_1420_4964: flag == 0 ->
 *     early out with GLOBALUNLOCK @ 1608:0010, else full repaint via
 *     GfxPort_SetActive @ 10d0:0956 + WdwStatus_CalcPanelRect @ 1218:0abf
 *     + Status_DrawPanelFrame @ 11f8:0278 + Game_UpdateStatusPanel @
 *     1088:13d5 + Unit_ShowInfoPopup @ 11f0:468c + PresentRect @
 *     10d0:0854 / FUN_10d0_09a2).
 *   WdwStatus_RequestRefresh @ 1218:063e (guard DAT_1420_4964 != 0 +
 *     GETWINDOWRECT/GETCLIENTRECT pair, then WdwStatus_Refresh with
 *     0x34/DC; caller of Refresh alongside WinMain @ 1008:0000).
 *   WdwStatus_CalcUpperRect @ 1218:0956 / WdwStatus_CalcLowerRect @
 *     1218:0a06 / WdwStatus_CalcPanelRect @ 1218:0abf shared head
 *     (GLOBALLOCK + Util_MemCopyFar @ 1000:137f of the status-port client
 *     rect, then the cx/cy derivation below; the three SETRECT shapes
 *     differ per frame: upper cy+0x1e / lower cy*2+0x46,0x1e / panel
 *     local_a-iVar2,iVar2*3+0x46, each INFLATERECT -1,-1).
 * Relation to WdwMap/SmMap windows (map_open_* STUBs in map.c):
 *   the status port is the third child (WdwStatus_Open @ 1218:0386)
 *   beside WdwMap_Open @ 1208:0af1 + WdwSmMap_Open @ 1210:025b; every
 *   view refresh funnels through Map_RefreshAllViews @ 1110:03e9 ->
 *   Game_UpdateStatusPanel @ 1088:13d5, while WdwStatus_Refresh adds the
 *   dirty-flag gate + Unit_ShowInfoPopup on top. The GLOBALLOCK rect
 *   fetch + GDI blit tails stay Win16 -> STUB (map_refresh_status);
 *   the arithmetic below is OS-free 1:1 on explicit params (no Win16,
 *   no globals, no exit(), read-only). */

/* Orig WdwStatus_Refresh @ 1218:0801 dirty gate (+0x2c flag) ->
 * wdwstatus_is_dirty. 1:1 predicate: nonzero (1) when flag != 0 (repaint),
 * 0 when flag == 0 (early out). Takes the flag byte explicitly. */
int wdwstatus_is_dirty(int flag);

/* Orig 1218:0956/0a06/0abf shared cx/cy head -> wdwstatus_frame_metrics.
 * 1:1: cx = (client_w - 0x8c) / 2; cy = (client_h - 0xf0) / 4 clamped to
 * 0..10 (orig Util_Clamp shape: >10 -> 10, <0 -> 0). client_w/h are the
 * (right-left)/(bottom-top) spans of the GLOBALLOCK'd client rect.
 * NOENT on NULL outs, else CIV_OK. */
civ_error_t wdwstatus_frame_metrics(int client_w, int client_h,
                                    int *cx_out, int *cy_out);

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

/* --- AL2 status text blit (2026-10-16) ---
 * Orig: FUN_11f8_0278 @ 11f8:0278 -> Text_DrawStatusRow via
 *   font_blit_glyph @ 1008:0e99 + font_text_width/height.
 * Params: NUL-terminated str + destination frame rect (civ_rect_t from
 *   wdwstatus_calc_upper/lower @ 1218:0956/0a06) + slot selecting the
 *   DAT_12d8_45f4/45fb source row / font row (0 = upper/45f4, 1 =
 *   lower/45fb; other non-negative = same origin, exact row mapping TBD).
 * OS-free metrics via font_text_width/height + per-char SDL blit loop via
 *   font_blit_glyph (video bridge, no Win16): for each char,
 *   font_render_glyph @ 1008:0e99 dfBitsOffset tight repack (w+7)/8 ->
 *   font_glyph_to_surface (8-bit black/white) -> font_blit_glyph at
 *   (rect->left + xoff, rect->top); xoff += glyph width (tracking 0,
 *   GetTextExtent shape); single row, no wrap, clipped at rect->right.
 * SDL lives only here (via video bridge headers in map.c; no direct
 *   Win16). No globals, no exit(), C17; read-only font image (borrowed,
 *   never writes civfonts.fon) vs caller-owned SDL dest (win borrowed,
 *   may be NULL like font_blit_glyph; ren/tex borrowed, never owned).
 * text_draw_status_row above stays the SDL-free stub (no handles without
 *   globals); this is the SDL bridge port. */
civ_error_t text_draw_with_font(SDL_Window *win, SDL_Renderer *ren,
                                SDL_Texture *tex, const char *str,
                                const civ_rect_t *rect, int slot,
                                const uint8_t *font_data, size_t font_len,
                                const font_fnt_info_t *font_info);

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

/* --- AI-C tile table (2026-10-15) ---
 * Tile codes from the worldgen.h frontier (Map_GenerateRandomOrLoad @
 * 1028:0000 pipeline + Map_LabelContinents @ 1028:0a93 tails): 10
 * (decimal, 0x0A) ocean, 1/2/0x0b scored land, 0x0c river seed, 0x0f
 * border, 0 cleared. OS-free static const port: no globals, no exit(),
 * read-only. Undecoded slots stay "?" (TODO, no fake terrain names
 * beyond confirmed). Table covers 0..16 (max observed 0x0f, one past
 * for growth to 0x10); anything else is "?" via the same fallback. */
#define CIV_TILE_TYPE_MAX 16

/* Dump-observed tile type name (Orig tile byte in the 64 KB buffer, see
 * map_get_tile @ 11e8:0dcf). OS-free static const port (implemented):
 * 0 "cleared", 1/2/0x0b "land", 10 (decimal) "ocean" (see Map_IsSeaLand
 * @ 1110:16c0 == 10; note decimal 10 = 0x0A, not 0x10), 0x0c
 * "river seed", 0x0f "border"; every other code returns "?" (TODO).
 * Returns a read-only literal, never NULL. No globals, no exit(). */
const char *tile_type_name(int tile);

/* --- AK1 tile/move cost (2026-10-15) ---
 * Map-side twin of unit_move_cost in game.h (same 1420:17d4 formula, no
 * cross-link to keep game/map decoupled for the parallel rewrite).
 * Parent: unit_step_toward_goal @ 1128:0000 neighbour scoring
 * (Chebyshev-remaining + terrain cost: 1420:17d4*3 or 3, river 1,
 * anti-reverse 0x6304 penalty; river + reverse live in unit_move_cost,
 * this helper does the base only). OS-free, no globals, no exit(),
 * read-only. */

/* Orig 1420:17d4 base-cost table -> tile_move_cost. OS-free 1:1 v1
 * (implemented): base = cost_table[tile] * 3 when 0 <= tile < cost_n
 * and entry != 0, else 3 (the "or 3" fallback; OOR/zero handling TBD vs
 * decompiler). Returns base cost (>= 3), -1 on NULL table or
 * cost_n <= 0. No globals, no exit(), read-only. */
int tile_move_cost(int tile, const uint8_t *cost_table, int cost_n);

/* --- Map step distance (2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Map_StepDist @ 11c8:025d (abs both deltas, hi = max, lo = min,
 *     returns hi + lo/2; 2-arg Chebyshev-weighted step cost used by the
 *     Wrap variant below).
 *   Map_WrapStepDist @ 11c8:02ec (x delta wrapped at the 0x50 map width:
 *     dx = |x1-x2|, if 0x28 < dx dx = 0x50-dx; dy = |y1-y2| unwrapped;
 *     then Map_StepDist(dx,dy); sole in-tree caller Replay_FindNearestSlot
 *     @ 1180:1096 over the 0x80 replay slots; the old game.h note tying
 *     11c8:02ec to Unit_FindDiplomatTarget was a misread: that helper is
 *     the explicit-params Chebyshev-min scan unit_find_diplomat_target
 *     (no Orig change), while 11c8:02ec is this wrap distance).
 * OS-free 1:1 on explicit coords (no globals, no exit()). Negative
 * inputs use absolute values (orig took signed far ints and folded via
 * (v ^ (v>>0xf)) - (v>>0xf)). */

/* Orig Map_StepDist @ 11c8:025d -> map_step_dist.
 * Returns max(|dx|,|dy|) + min(|dx|,|dy|)/2. No globals. */
int map_step_dist(int dx, int dy);

/* Orig Map_WrapStepDist @ 11c8:02ec -> map_wrap_step_dist.
 * Wraps the x delta at CIV_MAP_WRAP_W (0x50 game-map width, cf. the
 * 0x50 x 0x32 game map vs the 0x140 x 200 tile buffer at the top of this
 * header); y is unwrapped. Then map_step_dist. No globals. */
#define CIV_MAP_WRAP_W 0x50
int map_wrap_step_dist(int x1, int x2, int y1, int y2);

/* --- Tile access family (2026-10-16) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode):
 *   Map_CheckTileBounds @ 1110:168f (game-map bounds 0..0x4f x 0..0x31;
 *     narrower than the 0x140 x 200 Map_GetTile @ 11e8:0dcf buffer above).
 *   Map_CheckTileImproved @ 1110:1c72 (irrigated/road-improved predicate:
 *     2 <= y <= 0x2f and (x&3)*4+(y&3) == seed+(x>>2)*0xd+(y>>2)*0xb & 0xf;
 *     seed is DAT_13c0_0000; selects the 0x1836 table in City_TileYield @
 *     1030:7c28, see game.h city_tile_base_yield).
 *   Map_GetOccupancyFlags @ 1110:199b (low = tile(x,y+100), high =
 *     tile(x,y+0x96)*0x10; callers test &1 city / &2 food / &4 shield /
 *     &8 road / &0x10 holders / &0x40 pollution in City_TileYield).
 *   Map_GetContinentId @ 1110:1d99 (continent byte at tile(x,y+0x32);
 *     feeds City_ChooseProduction @ 1170:05ca continent gates + ocean
 *     Map_GetOceanOrContVal @ 1110:1dbd).
 *   Util_SignStep @ 1110:1c4b (sign: <1 ? (<0 ? -1 : 0) : 1; used by
 *     Unit_StepTowardGoal @ 1128:0000 player-adjacent resolve and
 *     Unit_AIPlayerTurn @ 1078:4f0f home-delta steering).
 *   Map_ClearOccupancyBits @ 1110:1b36 (clears (mask&0xf) at y+100 and
 *     (mask>>4) at y+0x96 via Map_SetTile; settler/road teardown tail).
 * Readers of the tile tables (per call graph): City_TileYield @ 1030:7c28
 *   (class + improved + occupancy), Unit_ChooseOrder @ 1078:1480 (hut
 *   tile+0x50, snipe tile+0x50+0x32), Unit_StepTowardGoal @ 1128:0000
 *   (class/occupancy/ocean gates), Unit_UpdateVisibility @ 11c0:0395
 *   (occupancy + minimap x+0xf0), Map_ScanZOCDetail @ 11c0:2437.
 * OS-free 1:1 on explicit params (no globals, no exit(), read-only
 * except the clear helper which writes via map_set_tile). OOB reads via
 * map_get_tile return -1; occupancy/continent fold OOB to 0/-1 (exact
 * 0x2e sentinel byte TBD vs dump, never faked as real data). */

/* Orig Map_CheckTileBounds @ 1110:168f: 1 when 0<=x<=0x4f and 0<=y<=0x31,
 * else 0. Game-map window (0x50 x 0x32), not the 0x140 x 200 buffer. */
int map_check_tile_bounds(int x, int y);

/* Orig Map_CheckTileImproved @ 1110:1c72: improved (irrigated/road) test.
 * tile is the Map_GetTileClass @ 1110:16c0 kind (kept for parity, unused
 * in the predicate — orig ignores it, see decompile); x/y are game-map
 * coords; seed is DAT_13c0_0000. Returns 1 when improved, 0 otherwise. */
int map_check_tile_improved(int tile, int x, int y, int seed);

/* Orig Map_GetOccupancyFlags @ 1110:199b: tile(x,y+100) +
 * tile(x,y+0x96)*0x10, each OOB folded to 0 (sentinel TBD). 0..255. */
int map_get_occupancy_flags(int x, int y);

/* Orig Map_GetContinentId @ 1110:1d99: tile(x,y+0x32), OOB -> -1. */
int map_get_continent_id(int x, int y);

/* Orig Util_SignStep @ 1110:1c4b: -1 when d<0, 0 when d==0, 1 when d>0. */
int map_sign_step(int d);

/* Orig Map_ClearOccupancyBits @ 1110:1b36: clears (mask&0xf) at (x,y+100)
 * and ((mask>>4)&0xf) at (x,y+0x96) via map_set_tile (read-modify-write;
 * OOB silently ignored like map_set_tile). No return value in orig;
 * Linux returns CIV_OK (NOENT never: no pointers). */
civ_error_t map_clear_occupancy_bits(int x, int y, unsigned mask);

/* --- Chain 2026-10-16 (4): wrap/lookup/ZOC-detail/mirror/owner ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), renamed this pass (see game.h
 * for the full caller/callee fan-out):
 *   Map_WrapX @ 1128:177e (single-step X wrap: <0 -> +0x50, >0x4f -> -0x50;
 *     Y never wraps; NOT a modulo: +-0x100 stays OOB, exact orig shape).
 *   Map_GetUnitAt @ 1110:18ce (civ slot from tile(x+0xa0,y): bit3 clear ->
 *     -1 else bits0-2; stale slots possible -> verify via FindUnitAt).
 *   Map_FindUnitAt @ 1110:180e (owner verify: GetUnitAt slot -> linear 0x80
 *     scan for x/y match, sets 12d8:4304/4306, clears stale tile(x+0xa0,y)
 *     to 0; else -1. Difference vs GetUnitAt: Get is the O(1) tile-nibble
 *     hint, Find is the O(0x80) authoritative owner+index check).
 *   Map_GetTileOwner @ 1110:16ee (tile(x+0xa0,y)&7 map-layer owner nibble;
 *     vs city owner @ 1420:5507 record byte vs unit owner slot civ: nibble
 *     is the fast path used by the 0395 tail, records authoritative).
 *   Map_CheckZOC @ 11c0:23fd (occupancy gate: Map_GetOccupancyFlags @
 *     1110:199b &1 city -> 0 clear, else Map_ScanZOCDetail @ 11c0:2437).
 *   Map_ScanZOCDetail @ 11c0:2437 (8-neighbour scan via the local_6*2 offset
 *     table + Map_WrapX, Y unwrapped: sea match (GetTileClass @ 1110:16c0
 *     == 10 both sides) + GetUnitAt != -1 + owner != self -> 1 blocked;
 *     player side effects: seen-bit OR + Map_RevealCenter @ 1110:1461 +
 *     city 5504=5503 + Map_MirrorOccupancy @ 1110:1a2f; Win16).
 *   Map_MirrorOccupancy @ 1110:1a2f (copy (x,y+100)->(x+0x50,y+100) +
 *     (x,y+0x96)->(x+0x50,y+0x96) via Map_GetTile @ 11e8:0dcf / Map_SetTile
 *     @ 11e8:0e19; minimap/occupancy mirror for the x+0x50 view).
 *   Map_SetTileOwner @ 1110:171f (tile(x+0xa0,y) = (old&8)|civ owner write;
 *     backs Unit_SpawnUnit place + City_DoRevolt moves).
 *   Map_PlaceUnitAt @ 1110:1767 (FindUnitAt + Unit_LinkStack @ 1050:1105 +
 *     SetTile(civ|8) marker; stack-link tail needs unit records -> the tile
 *     marker half below is LIVE, the link stays in game.h unit_stack_insert).
 * OS-free kernels below are LIVE 1:1 on explicit params (no globals, no
 * Win16, read-only except stated tile writes via map_get/set_tile); view
 * (Map_RevealCenter @ 1110:1461 -> Map_RefreshTileView @ 1110:0448 GfxPort)
 * stays STUB (needs SDL view state). */

/* Orig Map_WrapX @ 1128:177e -> map_wrap_x. 1:1 single-step wrap (NOT
 * modulo): x < 0 -> x+0x50, x > 0x4f -> x-0x50, else x. Y never wraps
 * (caller passes Y through). No globals. */
int map_wrap_x(int x);

/* Orig Map_GetUnitAt @ 1110:18ce core -> map_unit_slot_from_tile.
 * 1:1 nibble hint: (tile&8) == 0 -> -1 (no unit), else tile&7 (owner civ
 * slot 0..7). tile is the Map_GetTile(x+0xa0,y) byte (OOR callers pass -1
 * -> -1 here). Stale hints must be verified via the Find path (linear
 * 0x80 x/y scan); see map.h block. No globals. */
int map_unit_slot_from_tile(int tile);

/* Orig Map_GetTileOwner @ 1110:16ee core -> map_tile_owner_from_tile.
 * 1:1 tile(x+0xa0,y)&7 map-layer owner (0..7; vs city @ 1420:5507 record
 * vs unit slot civ). OOR (-1) -> -1. No globals. */
int map_tile_owner_from_tile(int tile);

/* Orig Map_ScanZOCDetail @ 11c0:2437 enemy rule -> map_scan_zoc_detail.
 * 1:1 pure scan over the 8 neighbours (n == 8): blocked (1) when any i has
 * nbr_has_unit[i] != 0 with sea match ((nbr_is_sea[i] != 0) ==
 * (self_is_sea != 0), cf. GetTileClass @ 1110:16c0 == 10) and
 * nbr_civ[i] != self_civ (GetUnitAt owner, cf. 1110:18ce). Occupancy gate
 * (&1 city -> clear) stays in the caller (Map_CheckZOC @ 11c0:23fd, already
 * LIVE in game.h map_check_zoc); player reveal side effects (seen-bit OR +
 * Map_RevealCenter @ 1110:1461) stay Win16. Returns 1 blocked, 0 clear,
 * -1 on NULL arrays or n != 8. No globals, read-only. */
int map_scan_zoc_detail(int self_civ, int self_is_sea,
                        const int *nbr_is_sea, const int *nbr_has_unit,
                        const int *nbr_civ, int n);

/* Orig Map_MirrorOccupancy @ 1110:1a2f -> map_mirror_occupancy.
 * 1:1 via map_get/set_tile: tile(x+0x50,y+100) = tile(x,y+100) and
 * tile(x+0x50,y+0x96) = tile(x,y+0x96) (OOB sides ignored like
 * map_set_tile @ 11e8:0e19). Returns CIV_OK (NOENT never: no pointers). */
civ_error_t map_mirror_occupancy(int x, int y);

/* Orig Map_SetTileOwner @ 1110:171f -> map_set_tile_owner.
 * 1:1 owner write: tile(x+0xa0,y) = (old&8)|civ (preserves the unit bit).
 * OOB reads (-1) fold to civ alone (exact orig sentinel TBD, never faked
 * as data); OOB writes ignored like map_set_tile. Returns CIV_OK, RANGE
 * on civ < 0 or > 7. No globals. */
civ_error_t map_set_tile_owner(int x, int y, int civ);

/* Orig Map_PlaceUnitAt @ 1110:1767 marker half -> map_place_unit_marker.
 * 1:1 tile marker: tile(x+0xa0,y) = (civ&7)|8 (FindUnitAt verify +
 * Unit_LinkStack @ 1050:1105 chain link need unit records -> handled via
 * game.h unit_stack_insert, not here). OOB ignored like map_set_tile.
 * Returns CIV_OK, RANGE on civ < 0 or > 7. No globals. */
civ_error_t map_place_unit_marker(int x, int y, int civ);

/* --- Chain 2026-10-16 (7): view origin + scroll clamp (goals 3/5) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   Map_SetViewOrigin @ 1208:20b1 (pixel -> tile origin: 1330:4010 = px/0x20,
 *     1330:400e = py/0x20, 1408:00c6 = px%0x20, 1408:00c4 = py%0x20; then
 *     Map_ClampViewScroll @ 1208:213b).
 *   Map_ClampViewScroll @ 1208:213b (was FUN_1208_213b: Y clamp + X wrap +
 *     scroll-pos sync; callers: only SetViewOrigin above; no other xrefs).
 *   Map_PaintTilesClipped @ 1208:1534 (was FUN_1208_1534: tile-range paint
 *     from the 1330:4010/400e origin + 1408:00c6/c4 offsets: per-tile
 *     SETRECT 0x20 box + Gfx_IntersectRect @ 1100:0685 clip (0 = skip) +
 *     OOB/fog (Map_WrapX @ 1128:177e + 1408:00cc seen bit) -> blank via
 *     FUN_1000_14f8/1100:0876 else Map_RevealCenter @ 1110:1461 tile blit;
 *     then Map_DrawCityLabels @ 1208:1f64 + GfxPort_PresentRect @ 10d0:0854).
 *   Map_RepaintViewShuffled @ 1208:1866 (was FUN_1208_1866: full-view repaint
 *     with param_3 mode: 2 = direct PresentRect, 1 = shuffled tile order via
 *     mem_alloc_lock + Rng_Range @ 11c8:003d Fisher-Yates, 0 = row-major;
 *     per-tile SETRECT + IntersectRect clip + OOB/fog fill vs RevealCenter;
 *     scroll sync via SETSCROLLPOS).
 *   Map_DrawCityLabels @ 1208:1f64 (was FUN_1208_1f64: city-name labels over
 *     DAT_1408_0000 entries via Text_DrawAligned @ 1098:0ed1 after an
 *     IntersectRect clip per label; called from both paint paths above).
 * Viewport model: fixed 0x20 px tiles, NO zoom (see video_unit_tile_box in
 * video.h: x0 = view*0x20 - dx). Wrap on X only (single-step +-0x50 via
 * map_wrap_x @ 1128:177e, NOT modulo: +-0x100 stays OOB); Y never wraps.
 * WdwMap/SmMap relation: WdwMap_Open @ 1208:0af1 (map_open_main STUB) owns
 * the scrolled view (origin above + 200 ms TIMERPROC); WdwSmMap_Open @
 * 1210:025b (map_open_small STUB) is the click-to-center minimap (tile
 * x+0xf0 layer, Map_CenterOnTile @ 1110:03ae STUB); status is the third
 * child (wdwstatus_* in map.h). Refresh fan-out Map_RefreshAllViews @
 * 1110:03e9 (STUB) repaints all three.
 * OS-free origin/clamp math below is LIVE 1:1 on explicit params (no Win16,
 * no globals, read-only except outs); GfxPort/scroll-handle tails stay
 * STUB with TODO Orig. */

/* Orig Map_SetViewOrigin @ 1208:20b1 split -> map_set_view_origin. 1:1
 * tile = px/0x20 (x86 IDIV trunc toward zero), off = px%0x20 (sign of px).
 * NOENT on any NULL out, else CIV_OK. Clamp lives in map_clamp_view_*
 * below (Map_ClampViewScroll tail). No globals. */
civ_error_t map_set_view_origin(int px, int py, int *tile_x_out,
                                int *tile_y_out, int *off_x_out,
                                int *off_y_out);

/* Orig Map_ClampViewScroll @ 1208:213b X half -> map_clamp_view_x. 1:1
 * single-step wrap (same as map_wrap_x @ 1128:177e): < 0 -> +0x50,
 * > 0x4f -> -0x50, else unchanged. Y never wraps here. No globals. */
int map_clamp_view_x(int tx);

/* Orig Map_ClampViewScroll @ 1208:213b Y half without Tech 8 (no
 * railroad movement bonus) -> map_clamp_view_y_plain. 1:1: < -0x19 ->
 * -0x19, > 0x4b -> 0x4b, else unchanged. No globals. */
int map_clamp_view_y_plain(int ty);

/* Orig Map_ClampViewScroll @ 1208:213b Y half with Tech 8 (Tech_HasTech @
 * 1170:3181(g_nPlayerCiv,8) != 0, GETSCROLLRANGE max) ->
 * map_clamp_view_y_scroll. 1:1: ty < 0 -> 0 with off kept; when
 * ty*0x20+pix_off > scroll_max: ty = scroll_max/0x20, *off_y =
 * scroll_max%0x20 (SETSCROLLPOS sync stays Win16). NOENT on NULL outs,
 * RANGE on scroll_max < 0, else CIV_OK. pix_off is the 1408:00c4 value
 * (0..0x1f expected; negative folds via the same compare). */
civ_error_t map_clamp_view_y_scroll(int ty, int pix_off, int scroll_max,
                                    int *ty_out, int *off_out);

/* Orig Map_ClampViewScroll @ 1208:213b -> map_clamp_view_scroll.
 * Full scroll sync (GETSCROLLRANGE/SETSCROLLPOS + GLOBALLOCK port rect +
 * Util_MemCopyFar @ 1000:137f). Win16 scroll handles -> SDL view TBD. */
civ_error_t map_clamp_view_scroll(int tx, int ty);

/* Orig Map_PaintTilesClipped @ 1208:1534 -> map_paint_tiles_clipped.
 * Clipped tile-range paint (origin + offsets + IntersectRect per 0x20 box
 * + fog/OOB blank vs Map_RevealCenter @ 1110:1461 + city labels +
 * PresentRect @ 10d0:0854). GfxPort blits -> SDL TBD. */
civ_error_t map_paint_tiles_clipped(int left, int top, int right, int bottom);

/* Orig Map_RepaintViewShuffled @ 1208:1866 -> map_repaint_view_shuffled.
 * Full-view repaint (mode 2 present / 1 shuffled / 0 row-major via
 * Rng_Range @ 11c8:003d shuffle + IntersectRect clip). GfxPort -> SDL. */
civ_error_t map_repaint_view_shuffled(int mode);

/* Orig Map_DrawCityLabels @ 1208:1f64 -> map_draw_city_labels.
 * City-name labels over DAT_1408_0000 entries (IntersectRect clip +
 * Text_DrawAligned @ 1098:0ed1). GDI text -> SDL_ttf TBD. */
civ_error_t map_draw_city_labels(void);

/* --- Chain 2026-10-16 (8): paint order + city-label format (goals 2/3) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Map_PaintTilesClipped @ 1208:1534 (tile-range paint from the
 *     1330:4010/400e origin + 1408:00c6/c4 offsets: per-tile SETRECT
 *     0x20 box + Gfx_IntersectRect @ 1100:0685 clip (0 = skip) +
 *     OOB/fog (Map_WrapX @ 1128:177e + 1408:00cc seen bit) -> blank via
 *     FUN_1000_14f8/1100:0876 else Map_RevealCenter @ 1110:1461 tile blit
 *     via Map_RefreshTileView @ 1110:0448 -> City_DrawTileView @ 1110:04d0;
 *     then Map_DrawCityLabels @ 1208:1f64 + GfxPort_PresentRect @
 *     10d0:0854 + GfxPort_ClearActive @ 10d0:09a2).
 *   Map_RepaintViewShuffled @ 1208:1866 (full-view repaint, param_3 mode:
 *     2 = direct PresentRect, 1 = shuffled tile order via mem_alloc_lock
 *     + Rng_Range @ 11c8:003d Fisher-Yates (two draws per i swap), 0 =
 *     row-major; per-tile SETRECT + IntersectRect clip + OOB/fog blank vs
 *     RevealCenter; scroll sync via SETSCROLLPOS; DAT_12b0_009a gate).
 *   Kolejnosc malowania: PaintClipped leci wierszami (outer X tiles,
 *     inner Y tiles, +0x20 px each); Shuffled-1 tasuje kolejnosc kafelkow
 *     (Fisher-Yates nad order[i] = i, losowa kolejnosc ukrywa kierunek
 *     odslaniania mgly wojny / artefakty rysowania; potem pojedynczy
 *     PresentRect per tile + Map_DrawCityLabels), RowMajor-0 leci
 *     wierszami jak PaintClipped (outer X, inner Y od -off). Shuffled
 *     dlaczego: rozbija widoczne fale odmalowania przy duzych refreshach
 *     (brak kierunkowego smuzenia), nie zmienia tresci kafelkow.
 *   Map_RefreshTileView @ 1110:0448 (tile (x,y) -> px view: wx =
 *     Map_WrapX(x - 1330:4010)*0x20 - 1408:00c6, wy = (y -
 *     1330:400e)*0x20 - 1408:00c4, then City_DrawTileView @ 1110:04d0).
 *   City_DrawTileView @ 1110:04d0 (city tile view: class/occupancy/wonder
 *     icons + owner frame + IntersectRect clip + label PresentRect when
 *     DAT_12b0_009a; GfxPort blits -> STUB in video.h city_draw_tile_view).
 *   Map_DrawCityLabels @ 1208:1f64 (labele miast per entry over
 *     DAT_1408_0000: box left = x+0x20, top = y-0x86, right = x+0x34,
 *     bottom = y+0xa6 (w 0x14, h 0x12c) + IntersectRect clip (0 = skip) +
 *     text = civ adjective (owner*0x20+0x4da) via Util_StrCopy @
 *     1000:36b2 + font select FUN_11e8_0b40(8) + Text_DrawAligned @
 *     1098:0ed1; format: sama nazwa/adjective civ, bez rozmiaru ani flagi
 *     wprost w labelu (rozmiar/flaga sa w City_DrawTileView @ 1110:04d0
 *     TEXTOUT/Gfx_DrawTechIcon, nie w tym labelu); zwiazek z
 *     City_FormatName @ 1048:0101: tamten Pascal-join (Res 0x88,4/5 +
 *     Res_AppendPascal @ 11f8:014a x2) buduje nazwe modalna miasta, ten
 *     label bierze gotowy adjective civ z tabeli 0x4da (nie wolanie
 *     FormatName, inny zrodlo stringow)).
 * Port kolejnosci/geometry -> LIVE below (no Win16, no globals); blit/
 * present/label-draw tails stay STUB (map_paint_tiles_clipped /
 * map_repaint_view_shuffled / map_draw_city_labels). */

/* Orig Map_PaintTilesClipped @ 1208:1534 range -> map_paint_start /
 * map_paint_end / map_paint_tile_px. 1:1: start = origin + off/0x20
 * (x86 IDIV trunc); end = start + span/0x20 + 1 (+1 when span%0x20 !=
 * 0); px = (tile - origin)*0x20 (local_8/local_a). No globals. */
int map_paint_start(int origin, int off);
int map_paint_end(int start, int span);
int map_paint_tile_px(int tile, int origin);
/* Orig same fog gate -> map_tile_needs_blank. 1:1: blank (1) when
 * ty < 0 || ty > 0x31 (Y never wraps, cf. map_wrap_x @ 1128:177e X-only)
 * or (reveal == 0 && (seen & 1) == 0) (reveal is DAT_1330_0000, seen is
 * the 1408:00cc byte at Map_WrapX(x)*0x32+y+10); else 0 (RevealCenter).
 * X wrap lives in map_wrap_x caller-side. No globals. */
int map_tile_needs_blank(int ty, int reveal, int seen);
/* Orig Map_RepaintViewShuffled @ 1208:1866 mode split (param_3). 1:1:
 * present == (mode == 2) (direct PresentRect), shuffled == (mode == 1)
 * (Fisher-Yates via Rng_Range @ 11c8:003d), rowmajor == (mode == 0).
 * No globals. */
int map_repaint_mode_is_present(int mode);
int map_repaint_mode_is_shuffled(int mode);
int map_repaint_mode_is_rowmajor(int mode);
/* Orig same shuffled grid -> map_shuffle_grid_count / map_shuffle_tile_at
 * / map_shuffle_px. 1:1: n = w*h (w = local_12-local_e tile cols, h =
 * local_14-local_10 rows); tile = order[n]: gx = val % w, gy = val / w
 * (val is the Fisher-Yates order entry); px = g*0x20 - off (off is
 * 1408:00c6/c4); tile = (gx+origin_x, gy+origin_y) via caller. NOENT on
 * NULL outs, RANGE on w <= 0, else CIV_OK. No globals. */
int map_shuffle_grid_count(int w_tiles, int h_tiles);
civ_error_t map_shuffle_tile_at(int order_val, int w_tiles,
                                int *gx_out, int *gy_out);
int map_shuffle_px(int g, int off);
/* Orig Map_RefreshTileView @ 1110:0448 -> map_refresh_tile_px. 1:1:
 * px = (tx - origin)*0x20 - off (per axis; X caller wraps via
 * map_wrap_x @ 1128:177e before passing tx). NOENT on NULL out. */
civ_error_t map_refresh_tile_px(int tx, int origin, int off, int *px_out);
/* Orig Map_DrawCityLabels @ 1208:1f64 box -> map_city_label_box. 1:1:
 * left = x+0x20, top = y-0x86, right = x+0x34, bottom = y+0xa6
 * (x/y are the DAT_1408_0000 entry words at +0x44/+0x84; w 0x14,
 * h 0x12c). Clip itself is video_clip_rect (Gfx_IntersectRect @
 * 1100:0685) caller-side. NOENT on NULL out. */
civ_error_t map_city_label_box(int x, int y, civ_rect_t *out);
/* Orig same label text id -> map_city_label_res_id. 1:1: owner*0x20 +
 * 0x4da (owner is the 1420:5507/5512 city-owner byte; table id feeds
 * Util_StrCopy @ 1000:36b2 before Text_DrawAligned @ 1098:0ed1).
 * Returns id, -1 on owner < 0. No globals. */
int map_city_label_res_id(int owner);

/* --- Chain 2026-10-16 (17): tile icon cache (goal 3) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Map_DisposeTileCache @ 1110:0127 (DAT_1420_2e42 = 0, potem petla
 *     0xfa: DELETEOBJECT handle gdy != 0 + zerowanie slotu, na koncu
 *     DAT_1330_0004 = 0 + DAT_1330_0002 = 0; sole caller WinMain_Shutdown
 *     @ 1008:0329, faza "dispose-tile-cache").
 *   Map_AllocTileCacheEntry @ 1110:01a3 (was FUN_1110_01a3: gdy
 *     DAT_1330_0002 < 0xfa bierze slot[used++] + DAT_1420_2e42++, else
 *     LRU-victim (min *(long *)(slot+3), init 0x7fffffff) + DELETEOBJECT
 *     starego handle; potem GLOBALLOCK(param_3) + key dword ->
 *     slot+1 (2-B int arytmetyka: +2 B) + sub byte -> slot+5 (+10 B) +
 *     GfxPort_AllocBitmap @ 10d0:0cfa (even ((w+1)/2*2)) + SETBITMAP-
 *     DIMENSION + lru = DAT_1330_0004++).
 *   Map_FindTileCache @ 1110:030c (was FUN_1110_030c: liniowy scan
 *     0..DAT_1330_0002-1 po (key dword @ slot+1 == param_1) &&
 *     ((char)slot[5] == param_2): hit z handle != 0 -> lru refresh
 *     (DAT_1330_0004++) + DAT_1420_2e40++ + return handle; handle == 0 ->
 *     wyzeruj slot + miss; brak matcha -> DAT_1420_2e44++ + return 0).
 * Co to za cache: 0xfa = 250 slotow po 12 B (6 slow 2-B int):
 *   [+0] HBITMAP handle (DELETEOBJECT; 0 = pusty),
 *   [+2..+5] key dword (param_1: ramka/ikona),
 *   [+6..+9] lru long (DAT_1330_0004 tick: alloc/hit refresh),
 *   [+10] sub byte (param_2: wariant), [+11] pad.
 *   (W 32-bit dekompilacie slot+1/+3/+5 to +2/+6/+10 B, bo int = 2 B.)
 *   Liczniki: DAT_1330_0002 used, DAT_1330_0004 lru-tick, DAT_1420_2e42
 *   alloc-total, DAT_1420_2e40 hits, DAT_1420_2e44 misses.
 * Kto napelnia: Map_AllocTileCacheEntry (evict-LRU + GfxPort_AllocBitmap
 *   bitmapa ikony; wolany ze sciezki paint, nie z Art_LoadFrameData @
 *   10b8:079d — tamten laduje ramki disk/RSC + LZW, tutaj gotowe ikony).
 * Kto uzywa: Map_FindTileCache (hit/miss) + Map_RefreshTileView @
 *   1110:0448 (Map_WrapX(x - 1330:4010)*0x20 - 1408:00c6, potem
 *   City_DrawTileView @ 1110:04d0, zob. video.h city_draw_tile_view STUB)
 *   + Map_PaintTilesClipped @ 1208:1534 / Map_RepaintViewShuffled @
 *   1208:1866 (per-tile paint, zob. map_paint_* LIVE).
 * Zwiazek ze slotami 17cc/17ae: ZADEN bezposredni — DAT_1420_17cc (active)
 *   / DAT_1420_17ae (prev) to sloty GfxPort (GfxPort_SetActive @ 10d0:0956
 *   + gfx_port_clear_active LIVE w video.h), nie tile-cache; cache ma
 *   wlasne used/tick w 1330:0002/0004.
 * Port struktury cache: slot + LRU/find/dispose LIVE 1:1 na jawnych
 *   parametrach (no Win16, no globals); DELETEOBJECT/GLOBALLOCK/
 *   GfxPort_AllocBitmap ogony zostaja caller-side STUB z TODO Orig. */

#define CIV_TILE_CACHE_N 0xfa
#define CIV_TILE_CACHE_SLOT_LEN 12

/* Orig slot @ DAT_1420_a2ea (12 B j.w.). Logiczny obraz (bez packingu). */
typedef struct map_tile_slot {
    int handle;   /* [+0] HBITMAP (0 = pusty) */
    int32_t key;  /* [+2] param_1 dword */
    int32_t lru;  /* [+6] DAT_1330_0004 tick */
    int sub;      /* [+10] param_2 byte */
} map_tile_slot_t;

/* Orig 0xfa bound (Dispose loop + Alloc append) -> pojemnosc. */
int map_tile_cache_capacity(void);
/* Orig 12-B stride (i*6 slow / i*3 dword) -> dlugosc slotu. */
int map_tile_cache_slot_len(void);
/* Orig Map_FindTileCache @ 1110:030c match (key dword + sub byte).
 * 1:1: 1 gdy slot != NULL i slot->key == key i slot->sub == sub, 0 wpp
 * (NULL -> 0; handle osobno w is_empty). No globals. */
int map_tile_cache_match(const map_tile_slot_t *slot, int32_t key, int sub);
/* Orig Map_DisposeTileCache @ 1110:0127 + Alloc victim guard (handle != 0
 * przed DELETEOBJECT) -> is_empty. 1:1: 1 gdy slot == NULL lub handle
 * == 0, 0 wpp. No globals. */
int map_tile_cache_is_empty(const map_tile_slot_t *slot);
/* Orig Map_FindTileCache @ 1110:030c scan -> find. 1:1 liniowy scan
 * 0..used-1 (used clampowane do [0, n]): indeks pierwszego matcha
 * (match j.w.) albo -1 (miss: orig DAT_1420_2e44++ + return 0). NULL
 * slots / n <= 0 / used <= 0 -> -1. Read-only. No globals. */
int map_tile_cache_find(const map_tile_slot_t *slots, int n, int used,
                        int32_t key, int sub);
/* Orig Map_AllocTileCacheEntry @ 1110:01a3 victim half -> victim. 1:1:
 * used < n i used < 0xfa -> used (append); else indeks min-lru ze
 * scanu 0..min(used,n)-1 (orig init 0x7fffffff). NULL slots / n <= 0 /
 * used < 0 -> -1; pusty zakres scanu -> -1. Read-only. No globals. */
int map_tile_cache_victim(const map_tile_slot_t *slots, int n, int used);
/* Orig Map_DisposeTileCache @ 1110:0127 slot-state half -> dispose. 1:1:
 * zeruje sloty[0..n) (handle/key/lru/sub) + *used_io = 0 (1330:0002) +
 * *tick_io = 0 (1330:0004) + *total_io = 0 (1420:2e42); DELETEOBJECT
 * caller-side (jak font_free_slots / gfx_icon_registry_free). NULL slots
 * przy n > 0 -> NOENT; n < 0 lub n > 0xfa -> RANGE; NULL liczniki =
 * skip (orig zawsze je ma, tu opcjonalne). No globals. */
civ_error_t map_tile_cache_dispose(map_tile_slot_t *slots, int n,
                                   int *used_io, int *tick_io,
                                   int *total_io);
/* Orig DAT_1330_0004++ (Alloc + Find-hit) -> next_tick. 1:1 tick + 1. */
int map_tile_cache_next_tick(int tick);

/* --- Chain 2026-10-16 (20): SmMap click-to-center (goal 2) ---
 * Orig SmMap_CenterOnClick @ 1210:056e (civ.exe, x86:LE:16:Protected Mode),
 * decompiled this pass (sole caller Sys_PumpMessages @ 1108:0000, galaz
 * DAT_12b0_001c == DAT_1420_1d48): Gfx_StoreDC + Menu_CheckCityNameId(0x1c,
 * DAT_1420_12b0) + GLOBALLOCK handle; gdy byte +0x2c == 0 -> early out
 * (unlock gdy handle != 0); inaczej dx = ((b3cc+b3d0)/2 - clickX)/scale,
 * dy = ((b3ce+b3d2)/2 - clickY)/scale (scale = DAT_1410_0000, dump 2),
 * potem Map_RefreshAllViews(g_nPlayerCiv, 1330:4010-dx, 1330:400e-dy)
 * (view-origin w 1330:4010/400e, minimap rect w 1420:b3cc..b3d2).
 * Zwiazek z WdwSmMap_Open @ 1210:025b (map_open_small STUB: CREATEWINDOW
 * "World Map", rect/tile-layer x+0xf0) i Menu_ToggleView1210 @ 1210:1134
 * (menu_toggle_show_cmd/view_window: SHOWWINDOW-toggle 12b0:001c; zob.
 * mainmenu.h): minimapa modeless, klik centruje widok glowny (clamp w
 * Map_CenterOnTile @ 1110:03ae STUB / map_clamp_view_* LIVE).
 * Port matematyki LIVE 1:1 na jawnych parametrach (no Win16, no globals,
 * no exit()); GLOBALLOCK/Gfx/Map_RefreshAllViews ogony STUB. */

/* Orig SmMap_CenterOnClick @ 1210:056e delta -> smmap_click_tile_delta.
 * 1:1 ((lo+hi)/2 - click)/scale (x86 IDIV trunc toward zero, same as C
 * /). lo/hi to 1420:b3cc/b3d0 (X) lub b3ce/b3d2 (Y); scale to
 * DAT_1410_0000 (dump 2). scale == 0 -> 0 (Linux guard; orig faulted
 * DIV0). */
int smmap_click_tile_delta(int rect_lo, int rect_hi, int click, int scale);

/* Orig same recenter -> smmap_center_target. 1:1 view - delta (view to
 * DAT_1330_4010 (X) lub 1330:400e (Y); wynik wchodzi do
 * Map_RefreshAllViews @ 1110:03e9 STUB). */
int smmap_center_target(int view, int delta);

/* Orig DAT_1410_0000 (dump: 02 00) -> smmap_click_scale. Zwraca skale
 * px-na-kafel uzywana w delcie powyzej (2). */
int smmap_click_scale(void);

#endif /* CIVLIN_MAP_H */
