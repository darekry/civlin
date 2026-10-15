/* Map windows: WDWMAPPROC / WDWSMMAPPROC / WDWSTATUSPROC.
 *
 * Ghidra: exports @ 1208:0054, 1210:0000, 1218:0000.
 * Win16 REGISTERCLASS + SETWINDOWWORD/GETWINDOWWORD -> SDL3 window/renderer.
 */
#ifndef CIVLIN_MAP_H
#define CIVLIN_MAP_H

#include "civ_common.h"
#include "win16_compat.h"

civ_error_t map_init(void);
win16_msg_result_t map_wndproc_main(int msg, long wparam, long lparam);
win16_msg_result_t map_wndproc_small(int msg, long wparam, long lparam);
win16_msg_result_t map_wndproc_status(int msg, long wparam, long lparam);
void map_shutdown(void);

#endif /* CIVLIN_MAP_H */
