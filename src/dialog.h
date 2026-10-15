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
/* --- generic dialog procs (packet Q 2026-10-15) ---
 * Win16 dialog dispatch -> SDL3 modal TBD; stubs return
 * WIN16_MSG_DEFAULT (DefDlgProc path). No globals, no exit(),
 * read-only. NE export ordinals TBD from the NE export table (see
 * Ghidra exports); both are MAKEPROCINSTANCE targets in
 * Dlg_CreateTracked @ 1098:124f. */

/* Orig: FUN_1098_16E2 @ 1098:16e2 -> DLGPROC (dlg_proc).
 * Generic dialog proc (NE export DLGPROC, ordinal TBD). MAKEPROCINSTANCE
 * target for generic dialogs in Dlg_CreateTracked @ 1098:124f.
 * Message table TBD; expected WM_INITDIALOG / WM_COMMAND / WM_PAINT
 * (+ palette copy GETDC/SELECTPALETTE/REALIZEPALETTE on the create
 * path, centering, tracked-stack push @ 1420:1ea0/1ec8). */
win16_msg_result_t dlg_proc(int msg, long wparam, long lparam);
/* Orig: FUN_1098_1838 @ 1098:1838 -> CIVDIALOGPROC (civ_dialog_proc).
 * Civ-specific dialog proc (NE export CIVDIALOGPROC, ordinal TBD).
 * Second MAKEPROCINSTANCE target in Dlg_CreateTracked @ 1098:124f for
 * civ dialogs (palette-aware centering path). Same WM_INITDIALOG /
 * WM_COMMAND / WM_PAINT table TBD as above. */
win16_msg_result_t civ_dialog_proc(int msg, long wparam, long lparam);
void dialog_shutdown(void);

/* Modal choice dialog (Dialog_MenuZ_Modal @ 1220:0156). items[] are the
 * '_' -separated entries; preselect is DAT_1418_0002 (-1 = none).
 * Returns CIV_OK + *out_index on pick, CIV_ERR_* on cancel/error. */
civ_error_t dialog_menuz(const char *title, const char *items_text,
                         int preselect, int *out_index);

/* Custom control layer (segment 1058; owner-drawn widgets for the
 * Customize-World editor).
 *
 * Orig: FUN_1058_0b4d @ 1058:0b4d -> Ctrl_RegisterCustomClasses.
 *   REGISTERCLASS x4: "RanDoMRadio" (string @ 0xac6), "RanDoMStatic"
 *   (@ 0xad2), "RanDoMUser" (@ 0xadf), "Region" (@ 0xaea); creates the
 *   shared GDI brushes/pens the paint procs select into the DC.
 *   Win16 USER/GDI -> SDL3 widget bridge TBD; stub CIV_ERR_UNSUPPORTED.
 * Orig: FUN_1058_0d71 @ 1058:0d71 -> Ctrl_FreeGdiObjects.
 *   Deletes the brushes/pens created above (DELETEOBJECT loop).
 *   Win16 GDI handle teardown -> SDL resource release TBD; stub (void).
 * Orig: FUN_1058_0000 @ 1058:0000 -> Ctrl_PaintCustomButton (owner-draw
 *   button renderer: frame + glyph + pressed-state brush; GDI BITBLT/
 *   FILLRECT -> SDL blit TBD; stub).
 * Orig: FUN_1058_0440 @ 1058:0440 -> Ctrl_PaintRandomUser (RanDoMUser
 *   body paint; stub, owner-draw TBD).
 * Orig: FUN_1058_04f6 @ 1058:04f6 -> Ctrl_PaintRandomStatic (RanDoMStatic
 *   label paint; stub, owner-draw TBD).
 * Orig: FUN_1058_0e7f @ 1058:0e7f -> Ctrl_RadioGroupSetSel (first/last/
 *   selected id math is OS-free and ported 1:1 as range validation;
 *   the BM_SETCHECK/SendMessage fan-out stays Win16 -> stub part). */
civ_error_t dialog_ctrl_register(void);
void dialog_ctrl_free(void);
civ_error_t dialog_ctrl_radio_set(int first_id, int last_id, int selected_id);
win16_msg_result_t dialog_ctrl_paint_button(int msg, long wparam, long lparam);
win16_msg_result_t dialog_ctrl_paint_static(int msg, long wparam, long lparam);
win16_msg_result_t dialog_ctrl_paint_user(int msg, long wparam, long lparam);

/* Custom-control WndProcs (segment 1058; window procs of the classes
 * above, used by the Customize-World editor; all Win16 USER message
 * dispatch -> SDL3 event/widget TBD; stubs returning WIN16_MSG_DEFAULT).
 * Orig: FUN_1058_06ab @ 1058:06ab -> RANDOMRADIOPROC (RanDoMRadio buttons).
 * Orig: FUN_1058_081a @ 1058:081a -> RANDOMSTATICPROC (RanDoMStatic label).
 * Orig: FUN_1058_0921 @ 1058:0921 -> RANDOMUSERPROC (RanDoMUser canvas).
 * Orig: FUN_1058_09f6 @ 1058:09f6 -> REGIONPROC (Region selector). */
win16_msg_result_t dialog_ctrl_radio_proc(int msg, long wparam, long lparam);
win16_msg_result_t dialog_ctrl_static_proc(int msg, long wparam, long lparam);
win16_msg_result_t dialog_ctrl_user_proc(int msg, long wparam, long lparam);
win16_msg_result_t dialog_ctrl_region_proc(int msg, long wparam, long lparam);

/* Tracked dialog layer (stubs; dialog ctx stack TBD).
 *
 * Orig: FUN_1098_124f @ 1098:124f -> Dlg_CreateTracked. CREATEDIALOG +
 *   MAKEPROCINSTANCE(DLGPROC/CIVDIALOGPROC) + palette copy (GETDC/
 *   SELECTPALETTE/REALIZEPALETTE) + centering + push HWND/ctx onto the
 *   tracked stack @ 1420:1ea0, count @ 1420:1ec8. 30+ callers.
 *   Win16 USER/GDI -> SDL3 window TBD; stub CIV_ERR_UNSUPPORTED.
 * Orig: FUN_1098_1612 @ 1098:1612 -> Dlg_DestroyTracked. DESTROYWINDOW +
 *   FREEPROCINSTANCE + GLOBALFREE of the ctx + pop from 1420:1ea0/1ec8.
 *   Stub (void). */
civ_error_t dlg_create_tracked(int dlg_id, unsigned flags);
void dlg_destroy_tracked(void);

/* --- dialog centering (packet R2) ---
 * Orig: FUN_10a0_xxxx @ 10a0:xxxx -> Dlg_CenterWindow. GETCLIENTRECT +
 *   center math sibling to wdwstatus_calc_upper/lower_rect @ 1218:0956/0a06
 *   (see map.h): cx = parent.left + (parent_w - dlg_w) / 2, cy =
 *   parent.top + (parent_h - dlg_h) / 2, OS-free 1:1 port on explicit
 *   rects (no Win16, no globals, read-only inputs); the SETWINDOWPOS
 *   application tail stays Win16 -> SDL3 TBD (stub part). */
typedef struct dlg_rect {
    int left;
    int top;
    int right;
    int bottom;
} dlg_rect_t;

/* Centers the dlg-sized rect inside parent; writes the moved rect to out
 * (same size as dlg) and returns CIV_ERR_UNSUPPORTED until the
 * SETWINDOWPOS tail lands. CIV_ERR_NOENT on NULL parent/dlg/out. */
civ_error_t dlg_center_window(const dlg_rect_t *parent, const dlg_rect_t *dlg,
                              dlg_rect_t *out);

/* --- packet F string anchors + help (2026-10-15) ---
 * "civhelp.hlp" -> CIV_HELP_FILE_NAME (WINHELP bridge TBD, exact Orig
 * FUN_* TBD; the call site issues WINHELP on the help file, read-only).
 * Reference-only (owned elsewhere, NOT redefined here): "civ.prf"
 * (boot.h CIV_PREFS_FILE), "OPENING.wav" (audio.h), "famehall.civ"
 * (halloffame.h CIV_HOF_FILE), "civfonts.fon" + "CivData0..4.rsc"
 * (boot.h / rsc_loader.h). MPLOGO.BMP has NO filename string (NE
 * resource or RSC frame, see title.h). */
#define CIV_HELP_FILE_NAME "civhelp.hlp"

/* WINHELP(CIV_HELP_FILE_NAME) bridge (stub; topic NULL = contents page).
 * Win16 WINHELP -> SDL3 help viewer TBD. */
civ_error_t dialog_open_help(const char *topic);

/* Orig: FUN_1160_xxxx @ 1160:xxxx -> Help_OpenTopic. WINHELP
 * (CIV_HELP_FILE_NAME) topic opener; thin wrapper wiring dialog_open_help
 * above (topic NULL = contents page). Win16 WINHELP -> SDL3 help viewer
 * TBD; stub CIV_ERR_UNSUPPORTED. No globals, no exit(), read-only. */
civ_error_t help_open_topic(const char *topic);

/* Sequence player (stubs). anim_advance_frames: 0 = busy, nonzero = done.
 *
 * Orig: FUN_10b0_0000 @ 10b0:0000 -> Anim_PumpIfActive. Runs
 *   Anim_AdvanceFrames while a sequence is active (flag @ 1420:1f40);
 *   wrapped by Sys_Yield @ 1098:0157 (see sys.h). Timer/frame pump ->
 *   SDL timer TBD; stub CIV_ERR_UNSUPPORTED.
 * Orig: FUN_10b0_0461 @ 10b0:0461 -> Anim_AdvanceFrames. Timed frame
 *   script, opcodes 1..0xe: palette fades/selects, offscreen blits,
 *   sound triggers (Sound_PlayId @ 1150:00c9 bridge), port switches
 *   (GR port layer, see video.h). Returns nonzero when the sequence is
 *   finished. Full script engine -> SDL TBD; stub (reports done). */
civ_error_t anim_pump_if_active(void);
int anim_advance_frames(void);

/* --- diplomacy + wait-prompt (packet D 2026-10-15) ---
 * Posted from unit_update_visibility @ 11c0:0395 tail (contact when a unit
 * first sees a foreign unit/city) and unit_wait_for_orders @ 11c0:0ec2
 * blink loop. All Win16 dialog/GDI -> SDL TBD; stubs. */

/* Orig: FUN_1168_0a07 @ 1168:0a07 -> Diplomacy_PostContact.
 * Posts the first-contact diplomacy dialog from unit_update_visibility @
 * 11c0:0395 (seen-bits x*0x32+y+10 flip / city-spot path). Params: foreign
 * civ + contacting unit index (unit record 1420:62fc layout, see game.h).
 * Win16 modal dialog pump -> SDL modal TBD. */
civ_error_t diplomacy_post_contact(int civ, int unit_idx);

/* Orig: FUN_1078_455c @ 1078:455c -> Diplomacy_EvalBribe.
 * Second diplomacy post from the 11c0:0395 visibility tail (bribe/diplomat
 * evaluation path; diplomat type 0x19, city bribe scan in
 * unit_choose_order @ 1078:1480 context). Same (civ, unit_idx) contract
 * as above. */
civ_error_t diplomacy_eval_bribe(int civ, int unit_idx);

/* Orig: FUN_10d8_00e0 @ 10d8:00e0 -> UI_DrawEndTurnPrompt.
 * Draws "End of Turn / Press Return" (alternates with the unit sprite blink
 * via 1110:0448/0dba + 1210:07ae in unit_wait_for_orders @ 11c0:0ec2 on the
 * Sys_GetTickScaled % 0x3c 0x1e-phase clock; blit via 10d0:0854). GDI
 * TEXTOUT + BITBLT -> SDL text/blit TBD. */
civ_error_t ui_draw_end_turn_prompt(void);

/* --- MenuZ button-id map (packet N, OS-free) ---
 * Orig: Dialog_MenuZ_Modal @ 1220:0156 builds RanDoMRadio buttons with
 * 500-based control ids (DAT_1418_0002 holds the preselect index,
 * DAT_1418_0004 flags cancel/back). MainMenu_WndProc @ 11e0:0221 loops
 * Util_StrCopy + Dialog_MenuZ_Modal while the raw choice is <0 or >3 over
 * text @ 1420:435b, then dispatches (see mainmenu.h). The helpers below own
 * only the id<->index arithmetic (no Win16, no globals, read-only):
 * raw 500-based button id <-> zero-based item index. Out-of-range maps to
 * -1 (id_to_index) so callers can re-prompt like the orig loop. */
#define CIV_DIALOG_MENUZ_BASE_ID 500

/* Orig: Dialog_MenuZ_Modal @ 1220:0156 id math — button id to item index.
 * Returns index (>=0) or -1 when button_id < 500. */
int dialog_menuz_id_to_index(int button_id);

/* Orig: Dialog_MenuZ_Modal @ 1220:0156 id math — item index to button id.
 * Returns 500 + index, or -1 when index < 0. */
int dialog_menuz_index_to_id(int index);

#endif /* CIVLIN_DIALOG_H */
