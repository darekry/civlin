/* Dialog stubs (+ MenuZ/custom-control surface). */
#include "dialog.h"

civ_error_t dialog_init(void)
{
    return CIV_ERR_UNSUPPORTED;
}

win16_msg_result_t dlg_proc(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t civ_dialog_proc(int msg, long wparam, long lparam)
{
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
    /* TODO: Ctrl_RegisterCustomClasses @ 1058:0b4d. */
    return CIV_ERR_UNSUPPORTED;
}

void dialog_ctrl_free(void)
{
    /* TODO: Ctrl_FreeGdiObjects @ 1058:0d71. */
}

civ_error_t dialog_ctrl_radio_set(int first_id, int last_id, int selected_id)
{
    (void)first_id;
    (void)last_id;
    (void)selected_id;
    /* TODO: Ctrl_RadioGroupSetSel @ 1058:0e7f. */
    return CIV_ERR_UNSUPPORTED;
}

win16_msg_result_t dialog_ctrl_paint_button(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    /* TODO: Ctrl_PaintCustomButton @ 1058:0000. */
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t dialog_ctrl_paint_static(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    /* TODO: Ctrl_PaintRandomStatic @ 1058:04f6. */
    return WIN16_MSG_DEFAULT;
}

win16_msg_result_t dialog_ctrl_paint_user(int msg, long wparam, long lparam)
{
    (void)msg;
    (void)wparam;
    (void)lparam;
    /* TODO: Ctrl_PaintRandomUser @ 1058:0440. */
    return WIN16_MSG_DEFAULT;
}

civ_error_t dlg_create_tracked(int dlg_id, unsigned flags)
{
    (void)dlg_id;
    (void)flags;
    /* TODO: Dlg_CreateTracked @ 1098:124f (CREATEDIALOG + MAKEPROCINSTANCE,
     * centering, GETDC, palette, push to 1420:1ea0 stack). */
    return CIV_ERR_UNSUPPORTED;
}

void dlg_destroy_tracked(void)
{
    /* TODO: Dlg_DestroyTracked @ 1098:1612 (DESTROYWINDOW +
     * FREEPROCINSTANCE + GLOBALFREE + pop). */
}

civ_error_t anim_pump_if_active(void)
{
    /* TODO: Anim_PumpIfActive @ 10b0:0000 (Anim_AdvanceFrames when a sequence
     * is active @ 1420:1f40). */
    return CIV_ERR_UNSUPPORTED;
}

int anim_advance_frames(void)
{
    /* TODO: Anim_AdvanceFrames @ 10b0:0461 (frame opcodes 1..0xe). Stub
     * reports done so future wait-loops terminate. */
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
