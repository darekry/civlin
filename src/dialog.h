/* Dialogs + modal choice (MenuZ) + custom controls.
 *
 * Ghidra anchors (civ.exe):
 *   DLGPROC         @ 1098:16e2, CIVDIALOGPROC @ 1098:1838 (exports)
 *   Dialog_MenuZ_Modal @ 1220:0156 (MenuZ : windows.c; "Can't initialize
 *     timer in MenuZ" @ 1420:4a41). Shared modal for MainMenu_WndProc AND
 *     every NewGame_Wizard step: builds RanDoMRadio buttons + RanDoMStatic
 *     label, pumps PEEKMESSAGE/ISDIALOGMESSAGE, returns button index.
 *     DAT_1418_0004=1 on cancel/back; DAT_1418_0002 preselect.
 *   Ctrl_RegisterCustomClasses @ 1058:0b4d (registers RanDoMRadio 0xac6,
 *     RanDoMStatic 0xad2, RanDoMUser 0xadf, Region 0xaea + brushes/pens)
 *   Ctrl_FreeGdiObjects @ 1058:0d71
 *   Ctrl_PaintCustomButton @ 1058:0000, Ctrl_PaintRandomUser @ 1058:0440,
 *   Ctrl_PaintRandomStatic @ 1058:04f6, Ctrl_PaintUnused049b @ 1058:049b
 *     (no callers; dead duplicate)
 *   Ctrl_RadioGroupSetSel @ 1058:0e7f
 * Menu strings: 14b0:0010 "&Save Game\tCtrl+S", 14b0:0092 "&Auto Save",
 *   14d0:0028 "Save First".
 * Tracked dialogs (renamed 2026-10-15):
 *   Dlg_CreateTracked  @ 1098:124f (CREATEDIALOG + MAKEPROCINSTANCE +
 *     palette copy + push to 1420:1ea0 stack, count 1420:1ec8; 30+ callers)
 *   Dlg_DestroyTracked @ 1098:1612 (DESTROYWINDOW + FREEPROCINSTANCE +
 *     GLOBALFREE + pop)
 * Intro/sequence player (renamed 2026-10-15):
 *   Anim_PumpIfActive  @ 10b0:0000 (runs Anim_AdvanceFrames when a sequence
 *     is active @ 1420:1f40; wrapped by Sys_Yield @ 1098:0157, see sys.h)
 *   Anim_AdvanceFrames @ 10b0:0461 (timed frame opcodes 1..0xe: palette,
 *     blits, sound, port switches; nonzero = sequence finished)
 */
#ifndef CIVLIN_DIALOG_H
#define CIVLIN_DIALOG_H

#include "civ_common.h"
#include "win16_compat.h"

civ_error_t dialog_init(void);
win16_msg_result_t dlg_proc(int msg, long wparam, long lparam);
win16_msg_result_t civ_dialog_proc(int msg, long wparam, long lparam);
void dialog_shutdown(void);

/* Modal choice dialog (Dialog_MenuZ_Modal @ 1220:0156). items[] are the
 * '_' -separated entries; preselect is DAT_1418_0002 (-1 = none).
 * Returns CIV_OK + *out_index on pick, CIV_ERR_* on cancel/error. */
civ_error_t dialog_menuz(const char *title, const char *items_text,
                         int preselect, int *out_index);

/* Custom control layer (segment 1058; owner-drawn widgets for the
 * Customize-World editor). */
civ_error_t dialog_ctrl_register(void);
void dialog_ctrl_free(void);
civ_error_t dialog_ctrl_radio_set(int first_id, int last_id, int selected_id);
win16_msg_result_t dialog_ctrl_paint_button(int msg, long wparam, long lparam);
win16_msg_result_t dialog_ctrl_paint_static(int msg, long wparam, long lparam);
win16_msg_result_t dialog_ctrl_paint_user(int msg, long wparam, long lparam);

/* Tracked dialog layer (stubs; dialog ctx stack TBD). */
civ_error_t dlg_create_tracked(int dlg_id, unsigned flags);
void dlg_destroy_tracked(void);

/* Sequence player (stubs). anim_advance_frames: 0 = busy, nonzero = done. */
civ_error_t anim_pump_if_active(void);
int anim_advance_frames(void);

#endif /* CIVLIN_DIALOG_H */
