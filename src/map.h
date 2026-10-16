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

#endif /* CIVLIN_MAP_H */
