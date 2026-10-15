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

civ_error_t dlg_center_window(const dlg_rect_t *parent, const dlg_rect_t *dlg,
                              dlg_rect_t *out)
{
    /* Orig FUN_10a0_xxxx @ 10a0:xxxx -> Dlg_CenterWindow. GETCLIENTRECT +
     * centering math sibling to wdwstatus_calc_upper/lower_rect @
     * 1218:0956/0a06 (see map.h): centered origin = parent origin +
     * (parent size - dlg size) / 2. OS-free 1:1 port on explicit rects;
     * the SETWINDOWPOS move application stays Win16 -> SDL3 TBD.
     * No globals, no exit(), read-only inputs. */
    int parent_w;
    int parent_h;
    int dlg_w;
    int dlg_h;
    int x;
    int y;

    if (!parent || !dlg || !out)
        return CIV_ERR_NOENT;
    parent_w = parent->right - parent->left;
    parent_h = parent->bottom - parent->top;
    dlg_w = dlg->right - dlg->left;
    dlg_h = dlg->bottom - dlg->top;
    x = parent->left + (parent_w - dlg_w) / 2;
    y = parent->top + (parent_h - dlg_h) / 2;
    out->left = x;
    out->top = y;
    out->right = x + dlg_w;
    out->bottom = y + dlg_h;
    /* TODO: SETWINDOWPOS(x, y) application -> SDL3 window positioning TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t dialog_open_help(const char *topic)
{
    (void)topic;
    /* TODO: WINHELP(CIV_HELP_FILE_NAME "civhelp.hlp") bridge (exact Orig
     * FUN_* TBD). Win16 WINHELP -> SDL3 help viewer TBD. Read-only: never
     * writes civhelp.hlp. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t help_hlp_parse(const uint8_t *data, size_t len,
                           help_hlp_info_t *out)
{
    /* AF2 HLP header side (see dialog.h help_hlp_info_t). OS-free 1:1
     * magic + bounds check on borrowed bytes (caller-owned, no copy,
     * never writes civhelp.hlp; no SDL, no globals, no exit()).
     * Dump-confirmed (unpacked/civhelp.hlp 72787 B, read-only): magic
     * bytes 3F 5F 03 00 at +0 (LE u32 0x00035F3F). Packed
     * civ1win/CIVHELP.HLP ("EDILZSS2", 33323 B) is rejected with BAD_DATA
     * (unpack first, cf. tools/unpack_edi.py). TODO: dump the topic/B-tree
     * directory (offsets/counts TBD; rsc_dump-style read-only dump, no
     * fake) + wire topic-id -> offset for help_open_topic (topic NULL =
     * contents page, unchanged stub below). */
    uint32_t magic;

    if (!data || !out)
        return CIV_ERR_NOENT;
    if (len < CIV_HLP_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    if (data[0] != CIV_HLP_MAGIC0 || data[1] != CIV_HLP_MAGIC1 ||
        data[2] != CIV_HLP_MAGIC2 || data[3] != CIV_HLP_MAGIC3)
        return CIV_ERR_BAD_DATA;
    magic = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    out->magic = magic;
    out->size = (uint32_t)len;
    return CIV_OK;
}

civ_error_t help_hlp_system(const uint8_t *data, size_t len,
                            help_hlp_system_t *out)
{
    /* AG3 HLP system header side (see dialog.h help_hlp_system_t).
     * OS-free header-only 1:1 magic + bounds check on borrowed bytes
     * (caller-owned, no copy, never writes civhelp.hlp; no SDL, no
     * globals, no exit()). Dump-confirmed (unpacked/civhelp.hlp 72787 B,
     * read-only): magic 3F 5F 03 00 at +0 (LE u32 0x00035F3F), version
     * LE u32 at +0x04 (0x46 on disk), flags LE u32 at +0x08 (0xFFFFFFFF
     * on disk), topic_count LE u32 at +0x10 (0x36 on disk; exact
     * directory/topic semantics TBD). LE u32 at +0x0C is 0x00011C53 ==
     * 72787 (== file size) on disk: observed, mapping TBD, never gates
     * success. Packed civ1win/CIVHELP.HLP ("EDILZSS2", 33323 B) is
     * rejected with BAD_DATA (unpack first, cf. tools/unpack_edi.py).
     * TODO: dump the topic/B-tree directory (offsets/counts TBD from the
     * 72787 B image; rsc_dump-style read-only dump, no fake) + wire the
     * topic-id -> offset map for help_hlp_topic below. */
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t topic_count;

    if (!data || !out)
        return CIV_ERR_NOENT;
    if (len < CIV_HLP_SYSTEM_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    if (data[0] != CIV_HLP_MAGIC0 || data[1] != CIV_HLP_MAGIC1 ||
        data[2] != CIV_HLP_MAGIC2 || data[3] != CIV_HLP_MAGIC3)
        return CIV_ERR_BAD_DATA;
    magic = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    version = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
        ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
    flags = (uint32_t)data[8] | ((uint32_t)data[9] << 8) |
        ((uint32_t)data[10] << 16) | ((uint32_t)data[11] << 24);
    topic_count = (uint32_t)data[16] | ((uint32_t)data[17] << 8) |
        ((uint32_t)data[18] << 16) | ((uint32_t)data[19] << 24);
    out->magic = magic;
    out->version = version;
    out->flags = flags;
    out->topic_count = topic_count;
    out->size = (uint32_t)len;
    return CIV_OK;
}

long help_hlp_topic(const uint8_t *data, size_t len, uint32_t topic_id)
{
    /* AG3 topic id -> file offset via the HLP topic B-tree (stub, no
     * fake). OS-free, read-only, no globals, no exit(): validates the
     * header exactly like help_hlp_parse/help_hlp_system above (NULL ->
     * -1; len < 16 -> -1; magic mismatch incl. EDILZSS2-packed input ->
     * -1), then returns -1 until the B-tree is wired. Exact Orig FUN_*
     * TBD. TODO: dump the topic/B-tree directory from the 72787 B image
     * (offsets/counts TBD; rsc_dump-style read-only dump, e.g.
     * "Civilization" at file 0x25) + map topic_id -> offset; never
     * fabricate offsets (unknown ids stay -1). */
    (void)topic_id;
    if (!data)
        return -1L;
    if (len < CIV_HLP_MIN_SIZE)
        return -1L;
    if (data[0] != CIV_HLP_MAGIC0 || data[1] != CIV_HLP_MAGIC1 ||
        data[2] != CIV_HLP_MAGIC2 || data[3] != CIV_HLP_MAGIC3)
        return -1L;
    /* TODO: B-tree walk (directory offsets/counts TBD) -> file offset;
     * Win16 WINHELP -> SDL3 help viewer TBD. Stub returns -1, no fake. */
    return -1L;
}

const char *ctrl_class_names(int index)
{
    /* AG3 custom-class name table: OS-free static const 4-entry port of
     * the REGISTERCLASS strings in Ctrl_RegisterCustomClasses @
     * 1058:0b4d (0 "RanDoMRadio" str @ 0xac6, 1 "RanDoMStatic" @ 0xad2,
     * 2 "RanDoMUser" @ 0xadf, 3 "Region" @ 0xaea). The register/free/
     * paint/proc stubs above stay untouched (Win16 USER/GDI -> SDL3
     * widget bridge TBD). No globals (function-local read-only view),
     * no exit(), read-only. */
    static const char *const names[CIV_CTRL_CLASS_COUNT] = {
        "RanDoMRadio",
        "RanDoMStatic",
        "RanDoMUser",
        "Region"
    };

    if (index < 0 || index >= CIV_CTRL_CLASS_COUNT)
        return NULL;
    return names[index];
}

civ_error_t help_hlp_page(const uint8_t *data, size_t len, uint32_t page_off,
                          help_hlp_page_t *out)
{
    /* AH3 HLP PAGE header side (see dialog.h help_hlp_page_t). OS-free
     * header-only LE decode on borrowed bytes (caller-owned file image, no
     * copy, never writes civhelp.hlp; no SDL, no globals, no exit()).
     * Bounds are overflow-safe (page_off OOR or short tail -> TRUNCATED).
     * wMagic is parsed but NOT gated (exact value TBD from the 72787 B
     * image dump). TODO: confirm wMagic/wFlags/lFirst + index-array entry
     * layout via rsc_dump-style read-only dump, then wire the PAGE walk
     * for help_hlp_btree_find below (no fake). Exact Orig FUN_* TBD. */
    uint16_t magic;
    uint16_t flags;
    int32_t first;
    uint16_t count;

    if (!data || !out)
        return CIV_ERR_NOENT;
    if ((size_t)page_off >= len || len - (size_t)page_off < CIV_HLP_PAGE_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    magic = (uint16_t)data[page_off] | ((uint16_t)data[page_off + 1] << 8);
    flags = (uint16_t)data[page_off + 2] | ((uint16_t)data[page_off + 3] << 8);
    first = (int32_t)((uint32_t)data[page_off + 4] |
        ((uint32_t)data[page_off + 5] << 8) |
        ((uint32_t)data[page_off + 6] << 16) |
        ((uint32_t)data[page_off + 7] << 24));
    count = (uint16_t)data[page_off + 8] | ((uint16_t)data[page_off + 9] << 8);
    out->magic = magic;
    out->flags = flags;
    out->first = first;
    out->count = count;
    out->size = (uint32_t)(len - (size_t)page_off);
    /* TODO: B-tree PAGE walk (index-array offsets/counts TBD) -> file
     * offset; Win16 WINHELP -> SDL3 help viewer TBD. Header-only for now. */
    return CIV_OK;
}

long help_hlp_btree_find(const uint8_t *data, size_t len, uint32_t topic_id)
{
    /* AH3 topic id -> file offset via the HLP PAGE B-tree walk (stub, no
     * fake). OS-free, read-only, no globals, no exit(): same header gate
     * as help_hlp_topic above for parity (NULL -> -1; len < 16 -> -1;
     * magic mismatch incl. EDILZSS2-packed input -> -1), then -1 until
     * the B-tree is wired. Exact Orig FUN_* TBD. TODO: PAGE walk via
     * help_hlp_page above (directory offsets/counts TBD from the 72787 B
     * image dump, e.g. "Civilization" at file 0x25; rsc_dump-style
     * read-only dump, never fabricate offsets; unknown ids stay -1). */
    (void)topic_id;
    if (!data)
        return -1L;
    if (len < CIV_HLP_MIN_SIZE)
        return -1L;
    if (data[0] != CIV_HLP_MAGIC0 || data[1] != CIV_HLP_MAGIC1 ||
        data[2] != CIV_HLP_MAGIC2 || data[3] != CIV_HLP_MAGIC3)
        return -1L;
    /* TODO: B-tree PAGE walk (needs B-tree pages) -> file offset; Win16
     * WINHELP -> SDL3 help viewer TBD. Stub returns -1, no fake. */
    return -1L;
}

long help_hlp_btree_root(const uint8_t *data, size_t len)
{
    /* AI-B root link (see dialog.h). OS-free 1:1 LE i32 read at
     * +CIV_HLP_BTREE_ROOT_OFF (candidate system-header lFirst/root;
     * exact semantics TBD vs the 72787 B directory dump) on borrowed
     * bytes (caller-owned, no copy, never writes civhelp.hlp; no SDL,
     * no globals, no exit()). Bounds only, no fake; wMagic NOT gated.
     * Exact Orig FUN_* TBD. */
    uint32_t u;
    int32_t s;

    if (!data)
        return -1L;
    if (len < CIV_HLP_BTREE_ROOT_MIN_SIZE)
        return -1L;
    if (len < CIV_HLP_MIN_SIZE)
        return -1L;
    if (data[0] != CIV_HLP_MAGIC0 || data[1] != CIV_HLP_MAGIC1 ||
        data[2] != CIV_HLP_MAGIC2 || data[3] != CIV_HLP_MAGIC3)
        return -1L;
    if ((size_t)CIV_HLP_BTREE_ROOT_OFF + (size_t)4 > len)
        return -1L;
    u = (uint32_t)data[CIV_HLP_BTREE_ROOT_OFF] |
        ((uint32_t)data[CIV_HLP_BTREE_ROOT_OFF + 1u] << 8) |
        ((uint32_t)data[CIV_HLP_BTREE_ROOT_OFF + 2u] << 16) |
        ((uint32_t)data[CIV_HLP_BTREE_ROOT_OFF + 3u] << 24);
    s = (int32_t)u;
    if (s < 0)
        return -1L;
    if ((size_t)s >= len)
        return -1L;
    if (len - (size_t)s < CIV_HLP_PAGE_MIN_SIZE)
        return -1L;
    return (long)s;
}

long help_hlp_btree_next(const uint8_t *data, size_t len, uint32_t page_off)
{
    /* AI-B next link (see dialog.h). OS-free lFirst follow via
     * help_hlp_page on borrowed bytes (no copy, never writes
     * civhelp.hlp; no SDL, no globals, no exit()). Self-loop guarded
     * here; multi-hop loops are guarded by help_topic_list below
     * (visited set + CIV_HLP_BTREE_MAX_STEPS). Exact Orig FUN_* TBD. */
    help_hlp_page_t pg;
    civ_error_t rc;
    long nxt;

    if (!data)
        return -1L;
    if (len < CIV_HLP_MIN_SIZE)
        return -1L;
    if (data[0] != CIV_HLP_MAGIC0 || data[1] != CIV_HLP_MAGIC1 ||
        data[2] != CIV_HLP_MAGIC2 || data[3] != CIV_HLP_MAGIC3)
        return -1L;
    rc = help_hlp_page(data, len, page_off, &pg);
    if (rc != CIV_OK)
        return -1L;
    if (pg.first < 0)
        return -1L;
    nxt = (long)pg.first;
    if (nxt < 0)
        return -1L;
    if ((size_t)nxt >= len)
        return -1L;
    if (len - (size_t)nxt < CIV_HLP_PAGE_MIN_SIZE)
        return -1L;
    if ((uint32_t)nxt == page_off)
        return -1L;
    return nxt;
}

civ_error_t help_topic_list(const uint8_t *data, size_t len,
                            uint32_t *out_ids, size_t cap,
                            size_t *out_count)
{
    /* AI-B topic walk (see dialog.h). OS-free page-chain walk from the
     * root link via help_hlp_btree_root/next on borrowed bytes (no
     * copy, never writes civhelp.hlp; no SDL, no globals, no exit()).
     * Detail capped (first cap ids), summary always (*out_count =
     * total visited, no flood). Ids are page file offsets (one per
     * page); per-entry topic-id decode stays TODO (no fake).
     * Exact Orig FUN_* TBD. */
    long root;
    long cur;
    long nxt;
    uint32_t visited[CIV_HLP_BTREE_MAX_STEPS];
    size_t total;
    size_t steps;
    size_t i;

    if (out_count)
        *out_count = 0;
    if (!data || !out_count)
        return CIV_ERR_NOENT;
    if (len < CIV_HLP_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    if (data[0] != CIV_HLP_MAGIC0 || data[1] != CIV_HLP_MAGIC1 ||
        data[2] != CIV_HLP_MAGIC2 || data[3] != CIV_HLP_MAGIC3)
        return CIV_ERR_BAD_DATA;
    if (cap > 0 && !out_ids)
        return CIV_ERR_NOENT;
    root = help_hlp_btree_root(data, len);
    if (root < 0) {
        *out_count = 0;
        return CIV_OK;
    }
    total = 0;
    steps = 0;
    cur = root;
    while (cur >= 0 && steps < CIV_HLP_BTREE_MAX_STEPS) {
        uint32_t cur_u;
        int seen;

        if ((size_t)cur >= len)
            break;
        if (len - (size_t)cur < CIV_HLP_PAGE_MIN_SIZE)
            break;
        cur_u = (uint32_t)cur;
        seen = 0;
        for (i = 0; i < total; ++i) {
            if (visited[i] == cur_u) {
                seen = 1;
                break;
            }
        }
        if (seen)
            break;
        if (total >= CIV_HLP_BTREE_MAX_STEPS)
            break;
        visited[total] = cur_u;
        if (total < cap && out_ids)
            out_ids[total] = cur_u;
        total++;
        steps++;
        nxt = help_hlp_btree_next(data, len, cur_u);
        if (nxt < 0)
            break;
        cur = nxt;
    }
    *out_count = total;
    return CIV_OK;
}

civ_error_t help_hlp_topic_header(const uint8_t *data, size_t len,
                                          uint32_t topic_off,
                                          help_hlp_topic_header_t *out)
{
    /* AJ2 topic header side (see dialog.h help_hlp_topic_header_t).
     * OS-free header-only LE decode on borrowed bytes (caller-owned,
     * no copy, never writes civhelp.hlp; no SDL, no globals, no exit()).
     * Overflow-safe bounds; format NOT gated (compressed still decodes).
     * Exact Orig FUN_* TBD. */
    uint32_t size;
    uint16_t format;
    uint16_t text_off;

    if (!data || !out)
        return CIV_ERR_NOENT;
    if (len < CIV_HLP_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    if (data[0] != CIV_HLP_MAGIC0 || data[1] != CIV_HLP_MAGIC1 ||
        data[2] != CIV_HLP_MAGIC2 || data[3] != CIV_HLP_MAGIC3)
        return CIV_ERR_BAD_DATA;
    if ((size_t)topic_off >= len)
        return CIV_ERR_RANGE;
    if (len - (size_t)topic_off < CIV_HLP_TOPIC_MIN_SIZE)
        return CIV_ERR_RANGE;
    size = (uint32_t)data[topic_off] |
        ((uint32_t)data[topic_off + 1u] << 8) |
        ((uint32_t)data[topic_off + 2u] << 16) |
        ((uint32_t)data[topic_off + 3u] << 24);
    format = (uint16_t)data[topic_off + 4u] |
        ((uint16_t)data[topic_off + 5u] << 8);
    text_off = (uint16_t)data[topic_off + 6u] |
        ((uint16_t)data[topic_off + 7u] << 8);
    if (size < CIV_HLP_TOPIC_MIN_SIZE)
        return CIV_ERR_RANGE;
    if ((size_t)size > len - (size_t)topic_off)
        return CIV_ERR_RANGE;
    if ((size_t)text_off < CIV_HLP_TOPIC_MIN_SIZE)
        return CIV_ERR_RANGE;
    if ((size_t)text_off >= (size_t)size)
        return CIV_ERR_RANGE;
    if ((size_t)text_off > len - (size_t)topic_off)
        return CIV_ERR_RANGE;
    out->size = size;
    out->format = format;
    out->text_off = text_off;
    out->topic_off = topic_off;
    /* TODO: decode the topic body (formatted text, embedded codes,
     * bitmap refs; layout TBD from the 72787 B image dump, no fake) +
     * confirm rec_size/format/text_off semantics vs the B-tree walk. */
    return CIV_OK;
}

civ_error_t help_hlp_topic_text(const uint8_t *data, size_t len,
                                uint32_t topic_off,
                                const uint8_t **out_text, size_t *out_len)
{
    /* AJ2 topic text side (see dialog.h). OS-free NUL-terminated text
     * extract via help_hlp_topic_header above on borrowed bytes (no copy,
     * no decompress fake; no SDL, no globals, no exit()). Raw bytes
     * returned (formatting codes NOT stripped yet). Exact Orig FUN_* TBD. */
    help_hlp_topic_header_t hdr;
    civ_error_t rc;
    size_t abs;
    size_t end;
    size_t pos;

    if (out_text)
        *out_text = NULL;
    if (out_len)
        *out_len = 0;
    if (!data || !out_text || !out_len)
        return CIV_ERR_NOENT;
    rc = help_hlp_topic_header(data, len, topic_off, &hdr);
    if (rc != CIV_OK)
        return rc;
    if (hdr.format != CIV_HLP_TOPIC_FORMAT_RAW) {
        /* TODO: wire the topic decompressor for format != 0 (exact
         * codec TBD from the 72787 B image dump; never fake plaintext)
         * + strip WinHelp formatting codes/bitmap refs. */
        return CIV_ERR_UNSUPPORTED;
    }
    abs = (size_t)hdr.topic_off + (size_t)hdr.text_off;
    end = (size_t)hdr.topic_off + (size_t)hdr.size;
    if (abs >= len || end > len || abs >= end)
        return CIV_ERR_RANGE;
    pos = abs;
    while (pos < end && data[pos] != 0u)
        pos++;
    if (pos >= end)
        return CIV_ERR_RANGE;
    *out_text = data + abs;
    *out_len = pos - abs;
    /* TODO: skip/strip WinHelp formatting codes (exact code set TBD;
     * raw view returned for now, no fake). */
    return CIV_OK;
}

civ_error_t help_topic_get(const uint8_t *data, size_t len,
                           uint32_t topic_id,
                           const uint8_t **out_text, size_t *out_len)
{
    /* AJ2 full-chain dispatcher (see dialog.h). OS-free topic id -> text
     * via help_hlp_btree_find + help_hlp_topic_header/text on borrowed
     * bytes (no Win16, no HELP viewer, no copy, never writes civhelp.hlp;
     * no SDL, no globals, no exit()). Exact Orig FUN_* TBD. */
    long off;

    if (out_text)
        *out_text = NULL;
    if (out_len)
        *out_len = 0;
    if (!data || !out_text || !out_len)
        return CIV_ERR_NOENT;
    if (len < CIV_HLP_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    if (data[0] != CIV_HLP_MAGIC0 || data[1] != CIV_HLP_MAGIC1 ||
        data[2] != CIV_HLP_MAGIC2 || data[3] != CIV_HLP_MAGIC3)
        return CIV_ERR_BAD_DATA;
    off = help_hlp_btree_find(data, len, topic_id);
    if (off < 0)
        return CIV_ERR_NOENT;
    if ((size_t)off >= len)
        return CIV_ERR_RANGE;
    /* TODO: wire the B-tree directory (offsets/counts TBD from the 72787 B
     * image dump; unknown ids stay NOENT, never faked) + topic
     * decompressor/formatting stripper (UNSUPPORTED path above). */
    return help_hlp_topic_text(data, len, (uint32_t)off, out_text, out_len);
}

civ_error_t help_hlp_decompressed_size(const uint8_t *src, size_t src_len,
                                               size_t *out_size)
{
    /* AK2 decomp_size word side (see dialog.h). OS-free LE u32 at src+0x00
     * on borrowed bytes (caller-owned slice, never writes civhelp.hlp; no
     * SDL, no globals, no exit()). Value NOT gated (0/huge still
     * reported); capacity check lives in help_hlp_decompress. */
    uint32_t v;

    if (!src || !out_size)
        return CIV_ERR_NOENT;
    if (src_len < 4u)
        return CIV_ERR_TRUNCATED;
    v = (uint32_t)src[0] | ((uint32_t)src[1] << 8) |
        ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
    *out_size = (size_t)v;
    return CIV_OK;
}

civ_error_t help_hlp_decompress(const uint8_t *src, size_t src_len,
                                uint8_t *dst, size_t dst_cap,
                                size_t *out_len)
{
    /* AK2 HLP 3.x LZ body side (see dialog.h). OS-free stub with explicit
     * src/dst bounds (borrowed src vs caller-owned dst, never writes
     * civhelp.hlp; no SDL, no globals, no exit(), no fake, dst untouched).
     * HLP 3.x LZ vs EDI: EDI LZSS2 (4K ring 0x20 curpos 4078 LSB-first
     * 1=literal/0=match+3, see gfx_try_lzss_edi @ 10f8:040a family in
     * gfx_indexed.h, read via Read, not edited) is installer framing, not
     * proven same as the HLP 3.x topic-body variant (decomp_size word +
     * window/bias TBD from the 72787 B dump), so no alias here. Header
     * validated via help_hlp_decompressed_size above; codec TODO. */
    civ_error_t rc;
    size_t need = 0;

    if (out_len)
        *out_len = 0;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    if (src_len < 4u || dst_cap == 0u)
        return CIV_ERR_TRUNCATED;
    rc = help_hlp_decompressed_size(src, src_len, &need);
    if (rc != CIV_OK)
        return rc;
    (void)need;
    /* TODO: rsc_dump-style read-only dump of a compressed topic body vs
     * raw to confirm window/bias/decomp_size semantics, then wire a real
     * LZSS trial (not gfx_try_lzss_edi alias until proven same); unknown
     * bodies stay UNSUPPORTED, never faked; dst never touched here. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t help_topic_get_full(const uint8_t *data, size_t len,
                                uint32_t topic_id,
                                const uint8_t **out_text, size_t *out_len)
{
    /* AK2 full-chain dispatcher (see dialog.h). OS-free topic id ->
     * decompressed text via help_hlp_btree_find + help_hlp_topic_header +
     * help_hlp_topic_text + help_hlp_decompress decision on borrowed bytes
     * (no Win16, no HELP viewer, no SDL, no globals, no exit(), never
     * writes civhelp.hlp). Raw format borrows the view; compressed format
     * reports UNSUPPORTED (no fake; caller scratch + codec TODO). */
    long off;
    help_hlp_topic_header_t hdr;
    civ_error_t rc;

    if (out_text)
        *out_text = NULL;
    if (out_len)
        *out_len = 0;
    if (!data || !out_text || !out_len)
        return CIV_ERR_NOENT;
    if (len < CIV_HLP_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    if (data[0] != CIV_HLP_MAGIC0 || data[1] != CIV_HLP_MAGIC1 ||
        data[2] != CIV_HLP_MAGIC2 || data[3] != CIV_HLP_MAGIC3)
        return CIV_ERR_BAD_DATA;
    off = help_hlp_btree_find(data, len, topic_id);
    if (off < 0)
        return CIV_ERR_NOENT;
    if ((size_t)off >= len)
        return CIV_ERR_RANGE;
    rc = help_hlp_topic_header(data, len, (uint32_t)off, &hdr);
    if (rc != CIV_OK)
        return rc;
    if (hdr.format != CIV_HLP_TOPIC_FORMAT_RAW) {
        /* TODO: wire help_hlp_decompress with caller-owned scratch once
         * the HLP 3.x LZ variant lands (window/bias TBD, not EDI alias)
         * + strip WinHelp formatting codes/bitmap refs; never fake. */
        return CIV_ERR_UNSUPPORTED;
    }
    return help_hlp_topic_text(data, len, (uint32_t)off, out_text, out_len);
}

civ_error_t help_open_topic(const char *topic)
{
    /* Orig FUN_1160_xxxx @ 1160:xxxx -> Help_OpenTopic. WINHELP
     * (CIV_HELP_FILE_NAME "civhelp.hlp") topic opener wiring
     * dialog_open_help(topic) above (topic NULL = contents page).
     * Win16 WINHELP -> SDL3 help viewer TBD. No globals, no exit(),
     * read-only: never writes civhelp.hlp. */
    return dialog_open_help(topic);
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
