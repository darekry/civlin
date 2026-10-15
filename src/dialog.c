/* Dialog stubs (+ MenuZ/custom-control surface). */
#include "dialog.h"

civ_error_t dialog_init(void)
{
    return CIV_ERR_UNSUPPORTED;
}

win16_msg_result_t dlg_proc(int msg, long wparam, long lparam)
{
    /* Orig FUN_1098_16E2 @ 1098:16e2 -> DLGPROC. Generic dialog proc
     * (NE export DLGPROC, ordinal TBD from NE export table;
     * MAKEPROCINSTANCE target in Dlg_CreateTracked @ 1098:124f).
     * (msg, wparam, lparam) -> WIN16_MSG_HANDLED when consumed else
     * WIN16_MSG_DEFAULT (DefDlgProc). Message table TBD (expected
     * WM_INITDIALOG / WM_COMMAND / WM_PAINT, see dialog.h); Win16 USER
     * -> SDL3 modal TBD. No globals, no exit(), read-only. */
    (void)msg;
    (void)wparam;
    (void)lparam;
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t civ_dialog_proc(int msg, long wparam, long lparam)
{
    /* Orig FUN_1098_1838 @ 1098:1838 -> CIVDIALOGPROC. Civ-specific
     * dialog proc (NE export CIVDIALOGPROC, ordinal TBD; second
     * MAKEPROCINSTANCE target in Dlg_CreateTracked @ 1098:124f).
     * Same handled/default contract as DLGPROC above; palette-aware
     * centering path (GETDC/SELECTPALETTE/REALIZEPALETTE in the create
     * path). Win16 -> SDL3 TBD. No globals, no exit(), read-only. */
    (void)msg;
    (void)wparam;
    (void)lparam;
    return WIN16_MSG_DEFAULT;
}

void dialog_shutdown(void)
{
}

civ_error_t dialog_menuz(const char *title, const char *items_text,
                         int preselect, int *out_index)
{
    (void)title;
    (void)items_text;
    (void)preselect;
    if (out_index)
        *out_index = -1;
    if (!title || !items_text || !out_index)
        return CIV_ERR_NOENT;
    /* TODO: Dialog_MenuZ_Modal @ 1220:0156 (SDL3 modal + radio buttons). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t dialog_ctrl_register(void)
{
    /* Orig: FUN_1058_0b4d @ 1058:0b4d -> Ctrl_RegisterCustomClasses.
     * TODO: REGISTERCLASS "RanDoMRadio" (str @ 0xac6), "RanDoMStatic"
     * (@ 0xad2), "RanDoMUser" (@ 0xadf), "Region" (@ 0xaea) + shared
     * brushes/pens. Win16 USER/GDI -> SDL3 widget bridge TBD. */
    return CIV_ERR_UNSUPPORTED;
}

void dialog_ctrl_free(void)
{
    /* Orig: FUN_1058_0d71 @ 1058:0d71 -> Ctrl_FreeGdiObjects.
     * TODO: DELETEOBJECT the brushes/pens from Ctrl_RegisterCustomClasses;
     * SDL resource release TBD. */
}

civ_error_t dialog_ctrl_radio_set(int first_id, int last_id, int selected_id)
{
    /* Orig: FUN_1058_0e7f @ 1058:0e7f -> Ctrl_RadioGroupSetSel.
     * OS-free selection-index math ported 1:1 (empty/inverted range and
     * out-of-range selection rejected); the BM_SETCHECK fan-out over
     * first..last via SendDlgItemMessage stays Win16 -> stub. */
    if (last_id < first_id)
        return CIV_ERR_RANGE;
    if (selected_id < first_id || selected_id > last_id)
        return CIV_ERR_RANGE;
    /* TODO: SendDlgItemMessage(hDlg, id in first..last, BM_SETCHECK,
     * selected ? 1 : 0) -> SDL3 radio-group bridge TBD. */
    return CIV_ERR_UNSUPPORTED;
}

win16_msg_result_t dialog_ctrl_paint_button(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    /* Orig: FUN_1058_0000 @ 1058:0000 -> Ctrl_PaintCustomButton.
     * TODO (owner-draw): frame + glyph + pressed-state brush via
     * BITBLT/FILLRECT/SELECTOBJECT -> SDL3 blit TBD. */
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t dialog_ctrl_paint_static(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    /* Orig: FUN_1058_04f6 @ 1058:04f6 -> Ctrl_PaintRandomStatic.
     * TODO (owner-draw RanDoMStatic label): TEXTOUT + frame -> SDL3 TBD. */
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t dialog_ctrl_paint_user(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    /* Orig: FUN_1058_0440 @ 1058:0440 -> Ctrl_PaintRandomUser.
     * TODO (owner-draw RanDoMUser canvas): custom blits -> SDL3 TBD. */
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t dialog_ctrl_radio_proc(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    /* Orig: FUN_1058_06ab @ 1058:06ab -> RANDOMRADIOPROC (WndProc for the
     * RanDoMRadio class; Customize-World editor buttons). TODO: Win16
     * USER message dispatch (WM_PAINT via Ctrl_PaintCustomButton, click/
     * focus -> Ctrl_RadioGroupSetSel) -> SDL3 widget TBD. */
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t dialog_ctrl_static_proc(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    /* Orig: FUN_1058_081a @ 1058:081a -> RANDOMSTATICPROC (WndProc for
     * RanDoMStatic). TODO: label paint via Ctrl_PaintRandomStatic ->
     * SDL3 TBD. */
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t dialog_ctrl_user_proc(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    /* Orig: FUN_1058_0921 @ 1058:0921 -> RANDOMUSERPROC (WndProc for
     * RanDoMUser). TODO: canvas paint via Ctrl_PaintRandomUser ->
     * SDL3 TBD. */
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t dialog_ctrl_region_proc(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    /* Orig: FUN_1058_09f6 @ 1058:09f6 -> REGIONPROC (WndProc for the
     * Region class). TODO: Win16 USER dispatch -> SDL3 TBD. */
    return WIN16_MSG_DEFAULT;
}

civ_error_t dlg_create_tracked(int dlg_id, unsigned flags)
{
    (void)dlg_id;
    (void)flags;
    /* Orig: FUN_1098_124f @ 1098:124f -> Dlg_CreateTracked.
     * TODO: CREATEDIALOG + MAKEPROCINSTANCE(DLGPROC/CIVDIALOGPROC) +
     * centering + GETDC/SELECTPALETTE/REALIZEPALETTE copy + push HWND/ctx
     * onto the tracked stack @ 1420:1ea0, count @ 1420:1ec8 (30+ callers)
     * -> SDL3 window TBD. */
    return CIV_ERR_UNSUPPORTED;
}

void dlg_destroy_tracked(void)
{
    /* Orig: FUN_1098_1612 @ 1098:1612 -> Dlg_DestroyTracked.
     * TODO: DESTROYWINDOW + FREEPROCINSTANCE + GLOBALFREE of the ctx +
     * pop from 1420:1ea0/1ec8 -> SDL3 TBD. */
}

civ_error_t anim_pump_if_active(void)
{
    /* Orig: FUN_10b0_0000 @ 10b0:0000 -> Anim_PumpIfActive.
     * TODO: while a sequence is active (flag @ 1420:1f40) run
     * Anim_AdvanceFrames; wrapped by Sys_Yield @ 1098:0157 (see sys.h).
     * SDL timer/frame pump TBD. */
    return CIV_ERR_UNSUPPORTED;
}

int anim_advance_frames(void)
{
    /* Orig: FUN_10b0_0461 @ 10b0:0461 -> Anim_AdvanceFrames. Timed frame
     * script, opcodes 1..0xe: palette fades/selects, offscreen blits,
     * sound triggers (via Sound_PlayId @ 1150:00c9), port switches (GR
     * port layer). Returns nonzero when the sequence is finished.
     * TODO: full script engine -> SDL TBD. Stub reports done so future
     * wait-loops terminate. */
    return 1;
}

civ_error_t dialog_open_help(const char *topic)
{
    (void)topic;
    /* TODO: WINHELP(CIV_HELP_FILE_NAME "civhelp.hlp") bridge (exact Orig
     * FUN_* TBD). Win16 WINHELP -> SDL3 help viewer TBD. Read-only: never
     * writes civhelp.hlp. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t diplomacy_post_contact(int civ, int unit_idx)
{
    (void)civ;
    (void)unit_idx;
    /* TODO: Orig FUN_1168_0a07 @ 1168:0a07 (Diplomacy_PostContact: posts
     * first-contact diplomacy from unit_update_visibility @ 11c0:0395;
     * needs Win16 modal dialog + SDL bridge). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t diplomacy_eval_bribe(int civ, int unit_idx)
{
    (void)civ;
    (void)unit_idx;
    /* TODO: Orig FUN_1078_455c @ 1078:455c (Diplomacy_EvalBribe: second
     * diplomacy post from the 11c0:0395 visibility tail; diplomat 0x19 /
     * bribe path; needs Win16 dialog + SDL bridge). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t ui_draw_end_turn_prompt(void)
{
    /* TODO: Orig FUN_10d8_00e0 @ 10d8:00e0 (UI_DrawEndTurnPrompt: "End of
     * Turn / Press Return" vs sprite blink in unit_wait_for_orders @
     * 11c0:0ec2 + 10d0:0854 blit; needs GDI TEXTOUT/BITBLT SDL bridge). */
    return CIV_ERR_UNSUPPORTED;
}

int dialog_menuz_id_to_index(int button_id)
{
    /* Orig: Dialog_MenuZ_Modal @ 1220:0156 button-id math (500-based
     * RanDoMRadio ids; DAT_1418_0002 preselect / DAT_1418_0004 cancel).
     * OS-free: id - 500, -1 when below base. No Win16, no globals. */
    if (button_id < CIV_DIALOG_MENUZ_BASE_ID)
        return -1;
    return button_id - CIV_DIALOG_MENUZ_BASE_ID;
}

int dialog_menuz_index_to_id(int index)
{
    /* Orig: Dialog_MenuZ_Modal @ 1220:0156 button-id math, inverse of above
     * (500 + index for the radio-button control id). -1 on negative index. */
    if (index < 0)
        return -1;
    return CIV_DIALOG_MENUZ_BASE_ID + index;
}
