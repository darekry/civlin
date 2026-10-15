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
win16_msg_result_t map_wndproc_main(int msg, long wparam, long lparam);
win16_msg_result_t map_wndproc_small(int msg, long wparam, long lparam);
win16_msg_result_t map_wndproc_status(int msg, long wparam, long lparam);
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

/* --- window opens / view refresh (stubs; Win16 windowing TBD) --- */

/* WdwMap_Open @ 1208:0af1. */
civ_error_t map_open_main(void);
/* WdwSmMap_Open @ 1210:025b. */
civ_error_t map_open_small(void);
/* WdwStatus_Open @ 1218:0386. */
civ_error_t map_open_status(void);
/* WdwStatus_Refresh @ 1218:0801. */
civ_error_t map_refresh_status(void);
/* Map_CenterOnTile @ 1110:03ae. */
civ_error_t map_center_on_tile(int civ, int x, int y);
/* Map_RefreshAllViews @ 1110:03e9. */
civ_error_t map_refresh_all_views(int civ);
/* MainPort_ApplyPalette @ 1200:00a8 (range repaint; exact locked-handle
 * arg order TBD, see Ghidra). */
civ_error_t mainport_apply_palette(int first, int count);

#endif /* CIVLIN_MAP_H */
