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
