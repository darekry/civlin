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
 * Centered frame from the status-port client spans (GLOBALLOCK +
 * Util_MemCopyFar @ 1000:137f in orig; explicit client_w/h here,
 * client-local origin 0,0): cx = (w-0x8c)/2, cy = clamp((h-0xf0)/4,0,10)
 * (head shared with wdwstatus_frame_metrics); SETRECT(cx-1, cy, w-cx,
 * cy+0x1e) + INFLATERECT(-1,-1) -> { cx, cy+1, w-cx-1, cy+0x1d }.
 * GLOBALLOCK/window-origin tails stay caller-side (no Win16 here).
 * Sole caller Game_UpdateStatusPanel @ 1088:1479; result feeds
 * FUN_11f8_0278(..., DAT_12d8_45f4) (first text row). NULL out -> NOENT.
 * LIVE 1:1, no exit(). */
civ_error_t wdwstatus_calc_upper_rect(int client_w, int client_h,
                                      civ_rect_t *out);

/* Orig: FUN_1218_0a06 @ 1218:0a06 -> wdwstatus_calc_lower_rect.
 * Same head; SETRECT(cx-1, cy*2+0x1e, w-cx, cy*2+0x46) + INFLATERECT
 * -> { cx, cy*2+0x1f, w-cx-1, cy*2+0x45 }. Sole caller
 * Game_UpdateStatusPanel @ 1088:1536; feeds FUN_11f8_0278(...,
 * DAT_12d8_45fb) (second text row). NULL out -> NOENT. LIVE 1:1. */
civ_error_t wdwstatus_calc_lower_rect(int client_w, int client_h,
                                      civ_rect_t *out);

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

/* Orig: FUN_11f8_0278 @ 11f8:0278 -> Status_DrawPanelFrame.
 * Frame painter called with the wdwstatus rects above (upper @ 1088:1479
 * + DAT_12d8_45f4, lower @ 1088:1536 + DAT_12d8_45fb; Ghidra name
 * Status_DrawPanelFrame: FILLRECT via Gfx_MakeBrush + 2x CREATEPEN white/
 * black + MOVETO/LINETO border + SELECTOBJECT restore + DELETEOBJECT x3;
 * TEXTOUT rows live in Game_UpdateStatusPanel @ 1088:13d5 itself, not
 * here). Params: NUL-terminated string (kept for the 1088:1479/1536 row
 * contract; frame path ignores text), destination frame rect, slot
 * selecting the DAT_12d8_45f4/45fb source row / font row. GDI frame +
 * TEXTOUT -> SDL bridge TBD. Ref unpacked/civ.exe read-only (NE Win3.x,
 * 832512 B): anchors 11f8:0278 + 1088:1479/1536. PARTIAL: rect/slot
 * guards LIVE below, GDI tail UNSUPPORTED. */
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

/* --- status-text rows (packet D 2026-10-15; takeover 2026-10-17) ---
 * Row/modal formatters share the 12d8:24fa line buffer (see
 * game_append_year_text @ 1088:1dd0), then blit via GDI (frame @
 * 11f8:0278 + TEXTOUT in Game_UpdateStatusPanel @ 1088:13d5 / dialogs
 * below). Ref unpacked/civ.exe read-only (NE Win3.x, 832512 B).
 * True orig shapes (decompiled 2026-10-17, names already in Ghidra):
 *   1138:0000 Status_FormatMainRow = newspaper front-page modal (dialog
 *     0x94/0x61 via Dlg_CreateTracked @ 1098:124f + GfxPort_Create/
 *     SetActive + grid CREATEPEN/LINETO + Tech_Badge @ 1018:24c5 x4 when
 *     param == -2 else Rng_Range(3) line 0x21 + Text_BuildWrappedList @
 *     1068:2072 + City_AppendName @ 1038:0bba headline switch Rng_Range(4)
 *     + 300-px short-form + FLASH + 10 cents + January year via
 *     Status_AppendYearText @ 1088:1dd0 + 11d0:00e4 line);
 *   11b0:0000 city-view modal (dialog 0x94/0x41 via Dlg_CreateTracked +
 *     Art_LoadFrameData + GfxPort_ApplyPalette + terrain/dots paint via
 *     FUN_11b0_115c/11bf/1227/1316/1653/1a5f/1fac/237c + header via
 *     City_AppendName + Status_AppendYearText + Text_DrawHeaderCentered);
 *   11b8:14e5 city-founded modal (dialog 0x94/0x41 + Art_LoadFrame/Data +
 *     Palette_CopyRange + "founded" + Status_AppendYearText + icon scroll
 *     via Gfx_ScrollPortCopy/Gfx_DrawTechIcon + tail call to 11b0:0000
 *     with 0xfffd when Main_DrainMessages == 0).
 * Orig writes the fixed 12d8:24fa buffer directly; explicit dst/cap
 * keeps the Linux side free of globals. PARTIAL: OS-free arg guards +
 * gate wiring LIVE in map.c, GDI/dialog tails UNSUPPORTED + TODO Orig. */

/* Orig: FUN_1138_0000 @ 1138:0000 -> Status_FormatMainRow (newspaper
 * modal above; caller of game_append_year_text @ 1088:1dd0 + headline
 * switch + short-form + FLASH + cents + year). */
civ_error_t status_format_main_row(char *dst, size_t cap);

/* Orig: FUN_11b0_0000 @ 11b0:0000 -> Status_FormatSecondRow (city-view
 * modal above; second game_append_year_text caller family; same
 * 12d8:24fa buffer contract). */
civ_error_t status_format_second_row(char *dst, size_t cap);

/* Orig: FUN_11b8_14e5 @ 11b8:14e5 -> Status_FormatGoldRow (city-founded
 * modal above; third game_append_year_text caller family; same buffer;
 * tail chains into FUN_11b0_0000). */
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

/* --- Chain P4 render+text (2026-10-16): repaint gate @ 1208:17da ---
 * Orig FUN_1208_17da @ 1208:17da (repaint gate around
 * Map_RepaintViewShuffled @ 1208:1866: GLOBALLOCK city-name ctx, then
 * if (*(char *)(ctx+0x2c) != 0) { DAT_13e8_0000 = 0 + Pre_DrawTilesToPort
 * trace + Map_RepaintViewShuffled(mode 1 shuffled) + Post_DrawTilesToPort
 * trace + DAT_13e8_0000 = 1 }; GLOBALUNLOCK when ctx != 0. The
 * DAT_13e8_0000 dance + traces stay caller-side; the gate below is LIVE
 * 1:1 on the explicit +0x2c cell flag byte. No Win16, no globals. */

/* Orig FUN_1208_17da @ 1208:17da gate -> map_repaint_needed. 1:1
 * nonzero (1) when cell_flag != 0 (repaint runs), 0 otherwise. */
int map_repaint_needed(int cell_flag);

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

/* --- Chain 2026-10-16 (28): Path_FindCorridor BFS kernels (task goal 2)
 * + continent-link window + ocean/cont value (goals 2/4/5) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled + renamed this pass:
 *   Path_FindCorridor @ 1128:10de (was FUN_1128_10de: BFS/Dijkstra-FIFO over
 *     the 16x16 window around the goal DAT_1338_0002/0000 (+-8 tiles):
 *     visited/cost bytes at s_Support+4 (0x100 B, index (x-x0)*0x10+(y-y0)),
 *     ring queue DAT_1420_afaa/aeaa 256 x/u8 (& 0xff), stop at empty or
 *     0xe1 iters; callers Path_CheckContinentLink @ 1128:16d0 (budget
 *     param_3 = 0x14 = 20) + Unit_StepTowardGoal @ 1128:0000 (budget 999)).
 *   Pelny BFS: start = cel (1338 pos, queue seed + visited = 1), target =
 *     pozycja jednostki (62fd/62fe scratch (0,0x7f) w link-check — 68f1/2
 *     to ALIAS bajtow tej jednostki: 62fc+0x7f*0xc = 68f0, wiec 68f1/68f2
 *     = jej y/type, a 68f3 = tryb 1 lad / 0x10 morze — NIE osobne globale;
 *     w unit-goto start = goal tile, target = unit tile, ta sama petla).
 *     Kolejka? Tak: ring 256 (head local_6, tail uVar17, & 0xff).
 *     Visited? Tak: 0x100 cost-bytes (0 = unseen, else cost; relax gdy
 *     new < old). Koszt terenu? Tak: road&8 both (FUN_1110_197a =
 *     Map_GetOccupancyFlags & 8, NIE continent!) -> +1; else fast-unit
 *     (1420:1a3c[type*0xb] == 1) ? +3 : +tile_cost(1420:17d4[class])*3.
 *     Morze vs lad? Gate: (tileClass == 10) == (unitDomain 1a3a == 2) LUB
 *     occupancy & 1 (miasto = most: city tile laczy domeny, embarka).
 *     Kto wola poza linkami: TYLKO Unit_StepTowardGoal @ 1128:0000
 *     (AI ruch / goto / trade-caravan przez unit_step_toward_goal, budget
 *     999; trade routes posrednio przez trade_deliver_caravan_goods).
 *   Path_CheckContinentLink @ 1128:16d0 gate: (|dx| < 8) || (|dy| < 8)
 *     else 0xffff; potem scratch-unit zapis + Path_FindCorridor(0,0x7f,
 *     budget) + koszt z DAT_1420_aea8 (-1 -> 0xffff). Poza kosztem: zadnego
 *     morza/owner/mostu W TEJ funkcji — morze/owner/most siedza W SRODKU
 *     FindCorridor (sea-match OR city-bit) + W CALLERZE BuildContinentLinks
 *     (same-continent local_12 == local_14 + dir-bit OR + reverse).
 *     Zwiazek z ContinentLinkAllowed (worldgen_continent_link_allowed LIVE:
 *     cost != -1 && < 0x14) = caller-side gate na wynik; OceanRunOk
 *     (worldgen_ocean_run_ok LIVE: clear_rows >= 2) = OCEAN graf, nie ten
 *     (5-step 2-wide sea-run w Map_BuildOceanLinks @ 1028:162c, bez BFS).
 *   Map_GetOceanOrContVal @ 1110:1dbd (sea -> u16 tab 1420:43b4[cont], land
 *     -> u16 tab 0x4334+cont*2; czytelnicy Game_SpawnUprisings @ 1088:0c78,
 *     10e8:212a, Unit_StepTowardGoal @ 1128:0651 ocean>4 gate, 1040:03f6).
 *   Map_BuildOceanLinks @ 1028:162c caller save_deserialize @ 10f8:01e9:
 *     po mem_copy_far map-buf (64000 B) + Save_GetLogTag wywoluje
 *     Map_BuildOceanLinks(0) = REBUILD po loadzie (.sav NIE ma linkow:
 *     rozmiary 0x1a2ba/0x1a2b8/0x18de0 to mapa+stan, linki pochodne od mapy;
 *     continent-linki NIE rebuilduje — tylko ocean; zob. save.h predicate).
 * Port BFS -> LIVE ponizej na jawnych parametrach (no Win16, no globals,
 *   kolejka caller-owned u8[256] + visited caller-owned u8[256]; read-only
 *   poza visited/queue; tile-access (Map_GetTileClass/occupancy/cost) wchodzi
 *   jako gotowe flagi/koszty — caller czyta bufor). No exit(), C17. */

/* Orig window OR-gate @ 1128:16d0 (CheckContinentLink head) ->
 * path_corridor_gate_ok. 1:1 (|dx| < 8) || (|dy| < 8) ? 1 : 0 (x86 abs via
 * (v ^ (v>>0xf)) - (v>>0xf); ujemne dozwolone). */
int path_corridor_gate_ok(int dx, int dy);

/* Orig BFS neighbour window ((x-goal) ^ s >> 0xf shape, < 8 per axis) ->
 * path_corridor_window_ok. 1:1 |dx| < 8 && |dy| < 8 (obie osie; X wrap
 * caller-side via map_wrap_x @ 1128:177e; Y never wraps). */
int path_corridor_window_ok(int dx, int dy);

/* Orig sea/land gate w FindCorridor (tileClass == 10 vs 1a3a == 2, OR
 * occupancy & 1 city-bridge) -> path_corridor_sea_ok. 1:1
 * (is_sea_tile == unit_is_sea) || (occupancy & 1). */
int path_corridor_sea_ok(int is_sea_tile, int unit_is_sea, int occupancy);

/* Orig road-cost branch (iVar10 = road&8 here, road&8 nbr via FUN_1110_197a)
 * -> path_corridor_step_cost. 1:1: both roads -> 1; else fast-unit
 * (1420:1a3c == 1) -> 3, else base_cost * 3 (base = tile byte 1420:17d4;
 * ujemne clampowane do 0 caller-side? tu czyste mnozenie). */
int path_corridor_step_cost(int road_here, int road_nbr, int unit_fast,
                            int base_cost);

/* Orig ring (& 0xff, 256 entries afaa/aeaa) -> path_corridor_queue_adv.
 * 1:1 (v + 1) & 0xff. */
int path_corridor_queue_adv(int v);

/* Orig relax (visited == 0 || new < old -> write + enqueue) ->
 * path_corridor_should_relax. 1:1 1 gdy old == 0 || new_cost < old. */
int path_corridor_should_relax(int old_cost, int new_cost);

/* Orig CheckContinentLink tail (dir == -1 -> 0xffff else DAT_aea8 cost) ->
 * path_continent_link_result. 1:1 dir == -1 ? -1 : cost (cost to BFS koszt
 * DAT_1420_aea8; caller gate via worldgen_continent_link_allowed; full
 * (x,y,continent) probe to path_check_continent_link w worldgen.h). */
int path_continent_link_result(int dir, int cost);

/* Orig Map_GetOceanOrContVal @ 1110:1dbd table select ->
 * map_get_ocean_or_cont_val. 1:1 sea ? sea_vals[cont] : land_vals[cont]
 * (tablice caller-owned: sea = 12d8:43b4, land = 12d8:4334 per disasm
 * ES=0x12d8 MOV ES:[BX+0x43b4]/[BX+0x4334]; Ghidra label DAT_1420_43b4 to ten
 * sam offset 0x43b4, real seg 12d8; cont = continent z map_get_continent_id
 * @ 1110:1d99). Wartosc = ROZMIAR (tile count) kontynentu/oceanu z
 * Map_LabelContinents @ 1028:0a93 rank sort (size @ 0x4334, top-0xF desc).
 * -1 poza 0..n-1 lub NULL tablica. */
int map_get_ocean_or_cont_val(int is_sea, int continent,
                              const int *sea_vals, const int *land_vals,
                              int n);

/* --- Chain 2026-10-16 (29): Path_FindLinkEntry kernels (task goal 2) ---
 * Orig Path_FindLinkEntry @ 1128:0ea3 (civ.exe, x86:LE:16:Protected Mode),
 * decompiled this pass (wejscie do grafu linkow: tile -> link-block):
 *   bx = x/4, by = y/4 (IDIV 4; bloki 4x4 jak BuildContinentLinks @
 *   1028:12c6 0x14 x 0x0d); fast path: link[bx*0xd+by] != 0 (land +4 /
 *   ocean +0x108 w ES=0x1248) -> dir 0; else 8 sasiadow (dx z 12c8, dy z
 *   12d0 per disasm) z link != 0 + Map_StepDist < best + 1 z 4 naroznikow
 *   (bx*4+1..+2, by*4+1..+2) w domenie (sea == param_3) + probe
 *   Path_CheckContinentLink(x4,y4,x,y,param_3,0x12) != -1 -> dir + best;
 *   na koncu 1338:0002/0000 = bx+dx[dir] / by+dy[dir] (link-block, nie tile).
 * Co to za wejscie: BRAMA/portal tile -> link-graph (nie start BFS ani
 *   wlasciwy BFS: BFS linkow zyje w Path_FindLinkRoute @ 1128:081c, ktory
 *   wola te brame 2x (unit pos + goal pos) przed ring-BFS 0x14x0x0d
 *   (dir-bity +0x4/+0x108, wrap X 0x14, koszt/cel via WrapStepDist).
 * Kto wola: TYLKO Path_FindLinkRoute @ 1128:081c (renamed this pass; wczesniej
 *   FUN_1128_081c; caller Unit_StepTowardGoal @ 1128:0000 dla AI/goto/caravan
 *   + wczesniej 999-budget probe; BuildContinentLinks @ 1028:12c6 NIE wola
 *   Entry — wola CheckContinentLink wprost z budzetem 0x14).
 * Zwiazek z FindCorridor @ 1128:10de: Entry -> CheckContinentLink (budget
 *   0x12 = 18) -> FindCorridor (16x16 okno, visited/cost, road/sea gates);
 *   Route (081c) uzywa Entry do wejsc/wyjsc, potem wlasny link-BFS.
 * Port -> LIVE ponizej na jawnych parametrach (no Win16, no globals). */

/* Orig block (x/4 IDIV, tiles >= 0) -> path_link_block. 1:1 x/4 (C trunc;
 * orig x86 IDIV; ujemne tile nie wystepuja w grafie). */
int path_link_block(int v);

/* Orig link idx (bx*0xd+by, ES=0x1248 grid 0x14 x 0x0d) -> path_link_table_idx.
 * 1:1 bx*0x0d+by, -1 na OOR (0..0x13 / 0..0x0c). +4/+0x108 caller-side. */
int path_link_table_idx(int bx, int by);

/* Orig link gate (*(link+off) != 0) -> path_link_has_link. 1:1 cell != 0. */
int path_link_has_link(int cell);

/* Orig best gate (StepDist < best, init 99) -> path_link_is_better.
 * 1:1 dist < best. */
int path_link_is_better(int dist, int best);

/* Orig 4-corner domain (1 z 4 w domenie: (tile==10)==want) ->
 * path_link_corner_ok. 1:1 1 gdy ktorys z s0..s3 (0/1 sea) == want_sea.
 * want nonzero = sea graph (param_3 != 0), 0 = land. */
int path_link_corner_ok(int s0, int s1, int s2, int s3, int want_sea);

/* Orig Entry probe budget (PUSH 0x12 do CheckContinentLink) ->
 * path_link_entry_budget. 1:1 0x12 = 18 (vs 0x14 = 20 w BuildContinentLinks
 * @ 1028:12c6; limit = budzet korytarza FindCorridor, nie liczba kontynentow
 * ktorych do 0x40 w rank sort). */
int path_link_entry_budget(void);

/* --- Chain 2026-10-16 (29): Ocean/cont value thresholds (goals 3/5) ---
 * Orig Map_GetOceanOrContVal @ 1110:1dbd wartosc = rozmiar (tile count);
 * tablice runtime BSS w seg 12d8 (zera przy load, fill w LabelContinents —
 * NIE ROM, wiec brak static dump; accessors biora caller-owned kopie).
 * Czytelnicy (NE xref scan, 4):
 *   Unit_StepTowardGoal @ 1128:0651 ocean>4-gate: sea tile wymaga val > 4
 *     (iVar8 != 10 || OceanVal > 4) — prog zeglugi: kaluze <= 4 niezeglowne
 *     (jezioro vs morze); NIE trirema vs caravel (typ statku w 1a3a domain +
 *     cost, nie tu; wszystkie sea jednostki dziela ten prog).
 *   NewGame_RateStartPositions @ 10e8:212a (w FUN_10e8_1d48 wizard-rating:
 *     val % 0x32 (remainder, nieuzywany) + val / 0x32 switch -> 0 small
 *     island / 1 large island / 2 small continent / 3 medium continent /
 *     default large continent; + score iVar6 i opis close/moderate/far/
 *     isolated + coastal/inland + river/grass; zwraca best civ, nie
 *     finalize placement — placement to FUN_10e8_12b5 per-civ + Rng 0x32).
 *   City_FoundCity @ 1040:03f6 (w FUN_1040_0000 city-founding: 8 sasiadow,
 *     sea + (val > 0x100 || player) -> flag 5502 |= 2 coastal; male zatoki
 *     <= 0x100 licza sie tylko dla gracza; NIE HoF (1080) ani disaster
 *     (City_ProcessDisaster @ 1040:068e next func); flaga = coastal trade).
 *   Game_SpawnUprisings @ 1088:0be9 (barb spawn: losowy tile musi byc type
 *     10 + empty + OceanVal >= 0x10, inaczej reroll; male oceany < 16 bez
 *     barbarzyncow).
 * Zwiazek z 43b4/4334: sea 12d8:43b4 = rozmiary oceanow, land 12d8:4334 =
 *   rozmiary kontynentow (oba u16, cont = Map_GetContinentId @ 1110:1d99).
 * Port progow -> LIVE ponizej (no globals). */

/* Orig StepTowardGoal gate (4 < OceanVal) -> map_ocean_is_navigable.
 * 1:1 val > 4 (kaluza vs morze; trirema/caravel gates gdzie indziej). */
int map_ocean_is_navigable(int v);

/* Orig SpawnUprisings gate (OceanVal < 0x10 reroll) -> map_ocean_is_spawnable.
 * 1:1 v >= 0x10. */
int map_ocean_is_spawnable(int v);

/* Orig FoundCity coastal (0x100 < val || player) -> map_ocean_is_coastal.
 * 1:1 val > 0x100 (player-override caller-side via is_player). Pelny gate
 * ponizej w map_city_coastal_should_set. */
int map_ocean_is_coastal(int v);

/* Orig FoundCity flag (sea && (coastal || player) -> 5502 |= 2) ->
 * map_city_coastal_should_set. 1:1 is_sea && (ocean_is_coastal || is_player).
 * Zwraca 1 gdy ustawic coastal-bit, 0 wpp. */
int map_city_coastal_should_set(int is_sea, int ocean_val, int is_player);

/* Orig RateStartPositions switch (val / 0x32) -> map_cont_size_class. 1:1
 * 0 small island / 1 large island / 2 small continent / 3 medium continent /
 * else (>= 4) large continent. val < 0 -> -1. */
int map_cont_size_class(int v);

/* Orig same remainder (val % 0x32, in_DX nieuzywany dalej) ->
 * map_cont_size_remainder. 1:1 v % 0x32 (C trunc; v < 0 -> -1). */
int map_cont_size_remainder(int v);

/* --- Chain 2026-10-16 (30): Path_FindLinkRoute kernels (task goal 1) ---
 * Orig Path_FindLinkRoute @ 1128:081c (civ.exe, x86:LE:16:Protected Mode),
 * decompiled this pass (caller TYLKO Unit_StepTowardGoal @ 1128:0000 —
 * AI ruch / goto gracza / karawany przez unit_step_toward_goal, budget
 * 999; trade routes posrednio przez trade_deliver_caravan_goods; zadnych
 * innych callerow):
 *   1338:0002/0000 = goal (6302/6303); 1. Path_FindLinkEntry(unit) — gdy 0
 *   (brak wejscia: tile ani 8 sasiadow bez linku/domeny/probe 0x12) ->
 *   restore goal + return 0 (fallback: brak trasy, jednostka stoi).
 *   2. Path_FindLinkEntry(goal) (drugie wejscie; inny tile, ten sam graf
 *   0x14x0x0d, dir-bity +4 land / +0x108 sea, wrap X 0x14) + MemSet 0x104
 *   (s_Support+4 visited 0x100) + ring DAT_1420_afaa/aeaa 256 x/u8 (&0xff,
 *   head local_8, tail local_6=1, seed = goal-link + cost 1).
 *   Jak laczy wpisy: BFS po link-graph (nie tile-graph): z kolejki (x,y)
 *   bierze dir-bity linku (land +4 / sea +0x108 wg domeny 1a3a==2), dla
 *   kazdego dir 1..8: nx = x+dx (wrap: 0x14->0, -1->0x13), ny = y+dy;
 *   gdy cost[nx*0xd+ny]==0 to cost = cost[cur]+1 + enqueue (bez limitu
 *   kosztu w tej petli — limit 0xe1 iters / ring-full via head==tail;
 *   koszt terenu NIE liczy sie tu: to czysty BFS-hop, koszt terenu siedzi
 *   w FindCorridor @ 1128:10de road/fast/base*3).
 *   Koszt: hop-count (cost byte, seed 1, +1 per link); cel = unit-link
 *   (uVar4/uVar3 z 1. Entry): dequeue == cel -> found=1, break.
 *   Fallback gdy brak trasy: found==0 -> 1338 = 0xffff, potem na dole
 *   (==0xffff) restore goal + return 0 (jak w 1.).
 *   Wybor kierunku: sposrod 8 sasiadow unit-link z link-dir + cost != 0
 *   wybiera MIN cost (init 'c'=99), tie-break via Map_WrapStepDist @
 *   11c8:02ec (unit-tile*4+1 vs sasiad*4+1, mniejszy dystans wygrywa);
 *   potem 1338 = (ux+dx)*4+1 wrap X via Map_WrapX @ 1128:177e,
 *   (uy+dy)*4+1, + domena-shift: gdy (class==10)!=sea to x+1 (re-check) to
 *   y+1 (embark-align na róg 4-tile linku, cf. Entry corner-ok).
 *   Port ponizej: czyste porownania/gate LIVE 1:1 (no Win16, no globals);
 *   tile/ring/BFS ogon zostaje caller-side (path_find_corridor STUB). */

/* Orig Route best-pick (cost < best || (== && dist < best_dist), init
 * best 99/'c') -> path_route_cost_is_better. 1:1 1 gdy lepszy, 0 wpp. */
int path_route_cost_is_better(int cur_cost, int best_cost, int cur_dist,
                              int best_dist);

/* Orig Route domain-shift ((class==10)!=sea -> x+1, re-check -> y+1) gate ->
 * path_route_domain_mismatch. 1:1 is_sea_tile != unit_is_sea (oba 0/1). */
int path_route_domain_mismatch(int is_sea_tile, int unit_is_sea);

/* Orig Route fallback (found==0 -> 1338=0xffff -> restore goal, return 0) ->
 * path_route_needs_fallback. 1:1 found==0 (0 = brak trasy). */
int path_route_needs_fallback(int found);

/* --- Chain 2026-10-16 (30): Status_FormatMainRow helpers (task goal 5) ---
 * Orig Status_FormatMainRow @ 1138:0000 (dialog 0x94 via Dlg_CreateTracked
 * @ 1098:124f 0x61 + GfxPort_Create/SetActive + CREATEPEN/LINETO siatka +
 * Tech_Badge @ 1018:24c5 x4 gdy param==-2 (New Cabinet) else Rng_Range(3)
 * linia 0x21 + wrap Text_BuildWrappedList @ 1068:2072 + City_AppendName @
 * 1038:0bba headline switch Rng_Range(4) (0x2fa2/Today/Times/Tribune) +
 * 300-px short-form gate + FLASH headline + 10 cents + January year via
 * Status_AppendYearText @ 1088:1dd0 + 11d0:00e4 line; glowny wiersz statusu
 * = gazeta/newspaper front page (miasto + rok + naglowek), nie pasek
 * statusu mapy (tamten to Text_DrawStatusRow @ 11f8:0278 rows).
 * Format: city + era-year + losowy headline; callers: Tech_GrantTech @
 * 1170:2aaa (wonder-cancel), Gov_AnnounceChange @ 10e8:32c1 (rzad), Unit_DoMove
 * @ 1038:0000 (zdobycie miasta), Game_TurnLoopTick @ 1088:0000,
 * Game_DoPollutionAndWonderRace @ 1088:1e32, Map_SpreadPollution @ 1028:1afd.
 * Dialog 0x94 to generyczny modal (jak Pic_PlayShared city view); wrap z
 * 1068:2072 (Text_BuildWrappedList). Pelny GDI tail STUB (status_format_*
 * w map.c); ponizsze gate LIVE. */

/* Orig headline short-form (measure > 300 -> City_AppendName + 0x2fcb) ->
 * status_headline_needs_short. 1:1 width > 300. */
int status_headline_needs_short(int width);

/* Orig city-scan (param < 0 -> 0x80 scan valid+owner+palace, -1 tez) ->
 * status_main_needs_city_scan. 1:1 param < 0. */
int status_main_needs_city_scan(int param);

/* Orig cabinet branch @ 1138:0185 (param == -2 -> New Cabinet badge x4
 * via Census_DrawTechBadge @ 1018:24c5, else Rng_Range(3) grid lines
 * 0x21) -> status_main_is_cabinet. 1:1 param == -2. Ref
 * unpacked/civ.exe. No globals. */
int status_main_is_cabinet(int param);

/* Orig headline prefix @ 1138:03fe/041d (Rng_Range(4) switch: cases 2/3
 * StrCopy prefix 0x2fb1/0x2fbd before City_AppendName @ 1038:0bba, cases
 * 0/1 direct) -> status_headline_needs_prefix. 1:1 variant == 2 ||
 * variant == 3 (variant is the Rng_Range(4) value 0..3). */
int status_headline_needs_prefix(int variant);

/* Orig FLASH branch @ 1138:04a7 (Rng_Range(2) == 0 -> "FLASH" @
 * 1420:2fd8, else 0x2fd1) -> status_flash_is_flash. 1:1 r == 0 (r is
 * the Rng_Range(2) value). */
int status_flash_is_flash(int r);

/* Orig city-pick palace gate @ 1138:00df (scan hit when (flags & 1) != 0
 * || param == -1; flags is DAT_1420_54fc[city*7] bit0 palace) ->
 * status_main_city_pick_ok. 1:1 has_palace || param == -1 (has_palace
 * nonzero = palace present). */
int status_main_city_pick_ok(int has_palace, int param);

/* Orig founded tail @ 11b8:14e5 (Main_DrainMessages @ 11e8:02fb == 0 ->
 * FUN_11b0_0000(param, 0xfffd) city-view chain, else return) ->
 * status_gold_needs_city_view. 1:1 drain == 0. */
int status_gold_needs_city_view(int drain);

/* Orig city-name checksum @ 11b0:0000 head (sum of 12d8:24fa bytes into
 * Rng_Seed @ 1000:1716 before the terrain scatter) ->
 * status_second_name_checksum. 1:1 sum of (unsigned char) bytes; NULL
 * -> 0 (orig faulted on a bad far pointer; Linux guard stays total).
 * No globals. */
unsigned status_second_name_checksum(const char *s);

/* --- Chain 2026-10-16 (map takeover): corridor index/limit + dir inverse
 * + authoritative unit scan + occupancy bits + center origin + seen bit
 * + status clip (goals path/passability/reveal/status-text) ---
 * Orig (civ.exe NE, x86:LE:16:Protected Mode), ref unpacked/civ.exe:
 *   Path_FindCorridor @ 1128:10de (16x16 window around goal +-8 tiles:
 *     visited/cost bytes at s_Support+4, 0x100 B, index
 *     (x-x0)*0x10+(y-y0) with x0 = goal_x-8 / y0 = goal_y-8; ring queue
 *     DAT_1420_afaa/aeaa 256 x/u8 (& 0xff); stop at empty or 0xe1 iters;
 *     callers Path_CheckContinentLink @ 1128:16d0 budget 0x14 +
 *     Unit_StepTowardGoal @ 1128:0000 budget 999; sea gate
 *     (tileClass == 10) == (domain 1a3a == 2) OR occupancy & 1 +
 *     road&8 cost branch live in path_corridor_* above).
 *   Map_ResolveDirect @ 1110:1c4b (player-adjacent |dx|<2,|dy|<2 resolve,
 *     dir 1..8 1=N clockwise, 0 arrived, 0xffff not adjacent; live in
 *     map_resolve_direct above; inverse below).
 *   Map_FindUnitAt @ 1110:180e (authoritative O(0x80) linear x/y scan over
 *     unit records at 1420:62fc + civ*0x600 + idx*0xc; sets 12d8:4304/4306,
 *     clears stale tile(x+0xa0,y) to 0; vs O(1) nibble hint GetUnitAt @
 *     1110:18ce live in map_unit_slot_from_tile above).
 *   Map_GetOccupancyFlags @ 1110:199b (tile(x,y+100) + tile(x,y+0x96)*0x10;
 *     &1 city / &8 road; live in map_get_occupancy_flags above; road test
 *     FUN_1110_197a = flags & 8 feeds corridor step cost; city test & 1
 *     gates Map_CheckZOC @ 11c0:23fd).
 *   Map_CenterOnTile @ 1110:03ae (centers x/y as x - w/2, y - h/2, then
 *     Map_RefreshAllViews @ 1110:03e9; SDL view sync stays in
 *     map_center_on_tile STUB).
 *   Fog gate @ 1208:1534/1866 (reveal == 0 && (seen & 1) == 0 -> blank;
 *     seen is the 1408:00cc byte; live folded in map_tile_needs_blank).
 *   Text_DrawStatusRow @ 11f8:0278 (single row, no wrap, clipped at
 *     rect->right: break when xoff >= rect_w; live SDL bridge in
 *     text_draw_with_font above).
 * OS-free 1:1 on explicit params (no Win16, no globals, no exit(),
 * read-only except outs). Win16 tails (WndProcs/Open/Refresh/blt) stay
 * CIV_ERR_UNSUPPORTED + TODO Orig (WndProcs return WIN16_MSG_DEFAULT). */

/* Orig Path_FindCorridor @ 1128:10de visited index ->
 * path_corridor_visited_idx. 1:1 ox*0x10+oy with ox/oy window offsets
 * 0..15 (ox = x-(goal_x-8), oy = y-(goal_y-8)); -1 when OOR. */
int path_corridor_visited_idx(int ox, int oy);

/* Orig same iter cap -> path_corridor_iter_limit. 1:1 0xe1 (225). */
int path_corridor_iter_limit(void);

/* Orig Map_ResolveDirect @ 1110:1c4b inverse -> map_dir_to_dxdy. 1:1
 * 1=N(0,-1) 2=NE(1,-1) 3=E(1,0) 4=SE(1,1) 5=S(0,1) 6=SW(-1,1) 7=W(-1,0)
 * 8=NW(-1,-1) (matches map_resolve_direct); 0 -> (0,0) arrived.
 * NOENT on NULL outs, RANGE on dir < 0 or > 8, else CIV_OK. */
civ_error_t map_dir_to_dxdy(int dir, int *dx_out, int *dy_out);

/* Orig Map_FindUnitAt @ 1110:180e scan -> map_find_unit_at. 1:1 linear
 * 0..n_units-1 x/y match (authoritative vs GetUnitAt hint); stale tile
 * clear + 4304/4306 sets stay caller-side. Returns index or -1 (miss;
 * NULL arrays / n_units <= 0 -> -1). */
int map_find_unit_at(int x, int y, const int *unit_xs, const int *unit_ys,
                     int n_units);

/* Orig FUN_1110_197a (occupancy & 8 road) -> map_occupancy_has_road.
 * 1:1 (occupancy & 8) != 0 (feeds path_corridor_step_cost road branch). */
int map_occupancy_has_road(int occupancy);

/* Orig Map_CheckZOC @ 11c0:23fd city gate (occupancy & 1) ->
 * map_occupancy_is_city. 1:1 (occupancy & 1) != 0 (city -> ZOC clear). */
int map_occupancy_is_city(int occupancy);

/* Orig Map_CenterOnTile @ 1110:03ae center math -> map_center_origin_for_tile.
 * 1:1 ox = x - view_w/2, oy = y - view_h/2 (C trunc; view dims > 0).
 * NOENT on NULL outs, RANGE on view_w <= 0 or view_h <= 0, else CIV_OK.
 * Refresh tail (Map_RefreshAllViews @ 1110:03e9) stays STUB. */
civ_error_t map_center_origin_for_tile(int x, int y, int view_w, int view_h,
                                       int *ox_out, int *oy_out);

/* Orig fog seen bit (1408:00cc & 1) -> map_tile_is_seen.
 * 1:1 (seen & 1) != 0 (complements map_tile_needs_blank gate). */
int map_tile_is_seen(int seen);

/* Orig Text_DrawStatusRow @ 11f8:0278 clip -> text_status_row_needs_clip.
 * 1:1 total_w > rect_w (single row, no wrap; xoff >= rect_w breaks).
 * Widths from font_text_width + rect spans (explicit, no font access). */
int text_status_row_needs_clip(int total_w, int rect_w);

/* --- Batch 2026-10-17 fala 3 (L): 1098 view-scale twins ---
 * Renamed this pass:
 *   Map_SetViewScaleAndOrigin @ 1098:296f, Map_ScaleRectInPlace @
 *   1098:29e1. Both LIVE pure (IMUL/IDIV on seg 12d8 with 4502/4500
 *   divisors; hidden globals -> explicit params/divisors). */

/* Orig Map_SetViewScaleAndOrigin @ 1098:296f (DAT_12d8_0034 = x,
 * 0032 = y, 0038 = x*4/4502, 0036 = y*8/4500). NULL out -> NOENT. */
civ_error_t map_set_view_scale_and_origin(int16_t x, int16_t y,
                                          int16_t div_w, int16_t div_h,
                                          int16_t out[4]);

/* Orig Map_ScaleRectInPlace @ 1098:29e1 (rect[0,2]*4/4502,
 * rect[1,3]*8/4500 in place). NULL -> NOENT, zero divisors -> RANGE. */
civ_error_t map_scale_rect_in_place(int16_t rect[4], int16_t div_w,
                                    int16_t div_h);

/* --- Batch 2026-10-17 fala 5 (X): 1110 occupancy/owner/site ---
 *   Map_RemoveUnitFromTile @ 1110:17bc, Map_SetUnitPresentFlag @
 *   1110:1906, Map_ClearUnitPresentFlag @ 1110:1941, Map_HasRoad @
 *   1110:197a, Map_GetMirroredOccupancyFlags @ 1110:19e0,
 *   Map_SetOccupancyBits @ 1110:1a8f, Map_IsCitySiteValid @ 1110:1cd9,
 *   Map_GetMirroredContinentId @ 1110:1d71, Map_BuildShadePattern @
 *   1110:1e17, Map_ComposeTileIcon @ 1118:0000, Map_GetTileCacheKey @
 *   1118:04b4. (City_FindByTileChecked @ 1110:1bab lives in game.h.) */

/* Orig Map_RemoveUnitFromTile @ 1110:17bc (next == -1 -> SetTile(x+0xa0,y,
 * civ) clearing 0x08; else Unit_UnlinkStack @ 1050:11ad chain, no tile
 * write; callers ChooseOrder @ 1078:1480 + FoundCityIfHome @ 11c0:16a4 +
 * 11f0 x2; inverse of PlaceUnitAt @ 1110:1767). Marker half LIVE, unlink
 * tail STUB. */
civ_error_t map_remove_unit_from_tile(int civ, int x, int y, int next);

/* Orig Map_SetUnitPresentFlag @ 1110:1906 (tile = (old&7)|8 on the owner
 * plane x+0xa0; callers City_DoRevolt @ 11b8:0f26 + FUN_11a0_0000 schism +
 * 11f0) and twin Map_ClearUnitPresentFlag @ 1110:1941 (tile = old&7;
 * orphan teardown pair). Both LIVE RMW (OOB ignored like SetTile). */
civ_error_t map_set_unit_present_flag(int x, int y);
civ_error_t map_clear_unit_present_flag(int x, int y);

/* Orig Map_HasRoad @ 1110:197a (GetOccupancyFlags & 8; callers
 * Path_FindCorridor @ 1128:10de x2 road-road +1 vs +3/base*3).
 * Bit meaning per City_TileYield @ 1030:7c28 (&1 city,&2 food,&4 shield,
 * &8 road,&0x10 holders,&0x40 pollution). LIVE. */
int map_has_road(int x, int y);

/* Orig Map_GetMirroredOccupancyFlags @ 1110:19e0 (player-view +0x50 twin
 * of GetOccupancyFlags: tile(x+0x50,y+100) + tile(x+0x50,y+0x96)*0x10;
 * callers City_DrawTileView @ 1110:04d0 + FUN_1118 x3 + FUN_1208_1dd7).
 * LIVE (OOB folds to 0). */
int map_get_mirrored_occupancy_flags(int x, int y);

/* Orig Map_SetOccupancyBits @ 1110:1a8f (mask == 0 clears both; < 0x10
 * ORs at y+100; else (mask>>4) at y+0x96; then mirror when view == player;
 * callers SetIrrigationFlag @ 1030:7f1e + DestroyCity @ 1170:0241 +
 * FoundCity @ 1040:0000). RMW LIVE, mirror gate explicit (no silent arch
 * change). */
civ_error_t map_set_occupancy_bits(int mask, int x, int y, int view_civ,
                                   int player_civ);

/* Orig Map_IsCitySiteValid @ 1110:1cd9 (class != 10, seen&1 == 0, 2 <= y
 * <= 0x2f, (x&3)*4+(y&3) == (seed+(x>>2)*0xd+(y>>2)*0xb+8)&0x1f, occup&1
 * == 0; callers InitCivState start-pos loop + FUN_1118_04b4 + ChooseOrder
 * x2 + 11f0; extended form of CheckTileImproved @ 1110:1c72 &0xf).
 * LIVE pure on explicit params. */
int map_is_city_site_valid(int tile_class, int x, int y, int seen_byte,
                           int seed, int occupancy);

/* Orig Map_GetMirroredContinentId @ 1110:1d71 (tile(x+0x50,y+0x32);
 * orphan twin of GetContinentId @ 1110:1d99 tile(x,y+0x32)). LIVE. */
int map_get_mirrored_continent_id(int x, int y);

/* Orig Map_BuildShadePattern @ 1110:1e17 (1330:0008 128x128 pattern:
 * a*0x10+b per 8x8 block + CREATECOMPATIBLEDC + AllocBitmap @ 10d0:0cfa +
 * SRCCOPY BITBLT + DELETEDC; orphan; exact use fog/shade/dither TBD vs
 * dump, no guessing). SDL texture bridge TBD -> STUB. */
civ_error_t map_build_shade_pattern(void);

/* Orig Map_ComposeTileIcon @ 1118:0000 (cache-miss renderer: class +
 * mirrored flags + key-bit ladder compositing terrain 95e2/98ae/9622/
 * 972e/96ac + overlays 9358/935c/935a/938e/938c/9370[] + FillIconRect dots
 * via CopyIconBuffer/Clipped @ 11a8:0328/076f; callers City_DrawTileView
 * @ 1110:04d0 x2 on cache miss). Icon-buffer facade TBD -> STUB. */
civ_error_t map_compose_tile_icon(int dst_handle, unsigned long key, int tx,
                                  int ty);

/* Orig Map_GetTileCacheKey @ 1118:04b4 (neighbour+flag fold incl.
 * COURTHOUSE @ 1420:087a ocean jumptable, 0xa/0xb specials, 2 == grass
 * dither, x8 and x10 shifts; sole caller City_DrawTileView @ 1110:055d).
 * Live-map ctx bridge TBD -> STUB (no decoder guessing). */
civ_error_t map_get_tile_cache_key(int tx, int ty, int *out_key);

/* --- Batch 2026-10-17 fala 6 (AA+AD): icon cache + scaled dist ---
 *   Map_CacheCivIcons @ 1158:14b7, Map_FreeCivIcons @ 1158:17ad,
 *   Map_WrapStepDistScaled @ 11c8:02a4. */

/* Orig Map_CacheCivIcons @ 1158:14b7 (local_a 0..4 mask loop x civs 1..7
 * ES:1350 0xdc match/mode + 0xc4&3 remap + Art_LoadFrameData(id,0x48) +
 * 2x SETRECT/ExtractClippedIcon -> DAT_1420_b12e + count 0xa4++; callers
 * PalaceImprove @ 1158:041a + PalaceRebuild @ 1158:09a5). Art/extract
 * bridge TBD -> STUB. */
civ_error_t map_cache_civ_icons(uint16_t civ_mask, int mode);

/* Orig Map_FreeCivIcons @ 1158:17ad (8x2 free b12e when 0xa4 != 0 +
 * clear; callers 1158:07d6 + 08e7:0ff2). Frees must go via icon bridge
 * (vacuous no-op would leak, not WONTFIX) -> STUB. */
civ_error_t map_free_civ_icons(void);

/* Orig Map_WrapStepDistScaled @ 11c8:02a4 (orphan; 2x-scaled twin of
 * Map_WrapStepDist @ 11c8:02ec / Map_StepDist @ 11c8:025d: abs via
 * (v^(v>>15))-(v>>15), 0x28/0x50 X-only wrap, 2*max+min keeps int
 * precision; CIV_MAP_WRAP_W 0x50). LIVE pure. */
int map_wrap_step_dist_scaled(int dx, int dy);

#endif /* CIVLIN_MAP_H */
