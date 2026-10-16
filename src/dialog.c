/* Dialog stubs (+ MenuZ/custom-control surface). */
#include "dialog.h"

/* AL2: INT_MAX for help_hlp_lzss_try len cap (OS-free, no SDL). */
#include <limits.h>

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

int help_hlp_lzss_try(const uint8_t *src, size_t src_len,
                      uint8_t *dst, size_t dst_cap)
{
    /* AL2 HLP LZSS-EDI trial (see dialog.h). OS-free 1:1 duplicate of
     * gfx_try_lzss_edi @ 10f8:040a family (tools/unpack_edi.py edi_unpack,
     * read via Read, not edited): flags LSB-first, 1 = literal 1 B, 0 =
     * match 2 B with matchpos = ((x1 & 0xF0) << 4) | x0 and matchlen =
     * (x1 & 0x0F) + 3; 4 K ring prefilled 0x20, curpos 4096-18 = 4078
     * (same as GFX_LZSS_RING_SIZE/RING_FILL/START_POS, duplicated here, no
     * gfx pull). Duplicated (not calling gfx) to avoid a gfx pull:
     * gfx_indexed.h brings SDL; dialog HLP stays SDL-free. Stack ring only
     * (no cross-module globals); overlap-safe forward match copy. Raw LZSS
     * code-stream framing (no 25-B EDI header / unpacked_len prefix, no HLP
     * decomp_size word here — framing lives in help_hlp_decompress): decode
     * runs until src bytes are consumed, so a trailing incomplete flag/code
     * at src end is a miss (-1, not silent success). Bounds: never reads
     * past src_len, never writes past dst_cap. No SDL, no exit, no globals,
     * C17. Returns decompressed bytes (>= 0, fits int) or -1 on NULL
     * src/dst, empty src/dst cap, decode miss (TRUNC_SRC/TAG/DST overrun
     * shape), or len > INT_MAX (no fake bytes as success; dst undefined on
     * miss, caller ignores it). */
    uint8_t ring[4096];
    size_t curpos = 4078u;
    size_t cpos = 0;
    unsigned bitbuf = 0;
    unsigned bits_left = 0;
    size_t op = 0;
    size_t i;

    if (!src || !dst)
        return -1;
    if (src_len == 0 || dst_cap == 0)
        return -1;
    for (i = 0; i < (size_t)4096; i++)
        ring[i] = (uint8_t)0x20;
    while (cpos < src_len || bits_left > 0u) {
        int bit;

        if (bits_left == 0u) {
            if (cpos >= src_len)
                break;
            bitbuf = (unsigned)src[cpos++];
            bits_left = 8u;
        }
        bit = (int)(bitbuf & 1u);
        bitbuf >>= 1;
        bits_left--;
        if (bit == 1) {
            uint8_t b;

            if (cpos >= src_len)
                return -1;
            if (op >= dst_cap)
                return -1;
            b = src[cpos++];
            dst[op++] = b;
            ring[curpos] = b;
            curpos = (curpos + 1u) & (size_t)4095u;
        } else {
            unsigned x0;
            unsigned x1;
            size_t matchpos;
            unsigned matchlen;
            unsigned k;

            if (cpos + (size_t)2u > src_len)
                return -1;
            x0 = (unsigned)src[cpos];
            x1 = (unsigned)src[cpos + (size_t)1u];
            cpos += (size_t)2u;
            matchpos = (size_t)(((x1 & 0xF0u) << 4) | x0);
            matchpos &= (size_t)4095u;
            matchlen = (x1 & 0x0Fu) + 3u;
            for (k = 0; k < matchlen; k++) {
                uint8_t b;

                if (op >= dst_cap)
                    return -1;
                b = ring[matchpos];
                dst[op++] = b;
                ring[curpos] = b;
                curpos = (curpos + 1u) & (size_t)4095u;
                matchpos = (matchpos + 1u) & (size_t)4095u;
            }
        }
    }
    if (op > (size_t)INT_MAX)
        return -1;
    return (int)op;
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
     * civhelp.hlp; no SDL, no globals, no exit(), no fake, dst untouched
     * on miss). HLP 3.x LZ vs EDI: EDI LZSS2 (4K ring 0x20 curpos 4078
     * LSB-first 1=literal/0=match+3, see gfx_try_lzss_edi @ 10f8:040a
     * family in gfx_indexed.h, read via Read, not edited) is installer
     * framing, not proven same as the HLP 3.x topic-body variant
     * (decomp_size word + window/bias TBD from the 72787 B dump), so no
     * gfx alias here (AL2 wires the local duplicate help_hlp_lzss_try
     * instead, no gfx/SDL pull). Header validated via
     * help_hlp_decompressed_size above; codec TODO. */
    civ_error_t rc;
    size_t need = 0;
    int trial;

    if (out_len)
        *out_len = 0;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    if (src_len < 4u || dst_cap == 0u)
        return CIV_ERR_TRUNCATED;
    rc = help_hlp_decompressed_size(src, src_len, &need);
    if (rc != CIV_OK)
        return rc;
    /* AL2: delegate body (skip decomp_size word at src+0x00, stream at
     * src+0x04 per AK2 framing) to help_hlp_lzss_try above (local EDI
     * duplicate, no gfx/SDL pull; parity read-only, not edited). Trial
     * hit sets *out_len and returns CIV_OK (need is advisory, NOT gated:
     * decomp_size mismatch still OK, trial len authoritative; see
     * help_hlp_decompressed_size doc). Trial miss (incl. DST overrun on
     * small dst) stays UNSUPPORTED (no fake bytes as success; dst
     * undefined on miss, *out_len stays 0). */
    (void)need;
    if (src_len <= 4u)
        return CIV_ERR_UNSUPPORTED;
    trial = help_hlp_lzss_try(src + (size_t)4u, src_len - (size_t)4u, dst,
                              dst_cap);
    if (trial < 0) {
        /* TODO: rsc_dump-style read-only dump of a compressed topic body
         * vs raw to confirm window/bias/decomp_size semantics; unknown
         * bodies stay UNSUPPORTED, never faked. */
        return CIV_ERR_UNSUPPORTED;
    }
    if (out_len)
        *out_len = (size_t)trial;
    return CIV_OK;
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
     * probes help_hlp_lzss_try then reports UNSUPPORTED (no fake; caller
     * scratch + codec TODO). */
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
        /* AL2: LZSS-EDI trial probe for compressed records (no HELP
         * viewer, no fake text). Body slice = [topic_off+text_off,
         * topic_off+size) from hdr above (header already validated
         * overflow-safe; recomputed defensively). Framing per AK2:
         * +0x00 decomp_size word +0x04 LZ stream (codec TBD, EDI trial
         * best-effort via help_hlp_lzss_try above, local duplicate, no
         * gfx/SDL pull). Probe with small stack scratch (no caller
         * scratch yet, no globals); full decompressed-text return needs
         * caller-owned scratch via help_hlp_decompress (same trial with
         * framing) + formatting stripper + viewer (TODO). Hit or miss
         * both stay UNSUPPORTED here (no persistent buffer to return
         * text, never faked); raw path below still returns borrowed text.
         * Small probe may hit DST overrun even when the codec is right
         * (needs larger scratch) — still a miss probe, no fake. */
        size_t abs;
        size_t end;
        const uint8_t *body;
        size_t body_len;
        uint8_t probe[256];
        int trial;

        abs = (size_t)hdr.topic_off + (size_t)hdr.text_off;
        end = (size_t)hdr.topic_off + (size_t)hdr.size;
        if (abs >= len || end > len || abs >= end)
            return CIV_ERR_RANGE;
        body = data + abs;
        body_len = end - abs;
        if (body_len < (size_t)4u)
            return CIV_ERR_UNSUPPORTED;
        trial = help_hlp_lzss_try(body + (size_t)4u, body_len - (size_t)4u,
                                  probe, sizeof probe);
        (void)trial;
        /* TODO: wire help_hlp_decompress with caller-owned scratch once
         * the HLP 3.x LZ variant lands (window/bias TBD, not gfx alias)
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

int text_align_x(int left, int right, int text_w, int format)
{
    /* Orig FUN_1098_0ed1 @ 1098:0ed1 -> Text_DrawAligned horizontal math.
     * Decompiler: uVar1 = param_4 & 0xf; 1 -> *rect + ((rect[2]-*rect) -
     * text_w) / 2; 2 -> *rect; 3 -> rect[2] - text_w; else early return
     * (no draw). format is the full Orig param_4 word here; only the low
     * nibble selects (0x30 shadow bits + color word handled by the
     * Text_SetColor @ 1098:0e0b + TEXTOUT tail, see text_draw_aligned
     * STUB below). OS-free 1:1, no Win16, no globals, no exit(). */
    int slot = format & 0x0f;

    if (slot == 1)
        return left + ((right - left) - text_w) / 2;
    if (slot == 2)
        return left;
    if (slot == 3)
        return right - text_w;
    return -1;
}

int text_center_y(int top, int bottom, int text_h)
{
    /* Orig FUN_1098_0ed1 @ 1098:0ed1 -> Text_DrawAligned vertical math.
     * Decompiler: iVar3 = rect[1] + ((rect[3]-rect[1]) - tmHeight) / 2
     * (tmHeight via GETTEXTMETRICS, see Text_GetFontHeight @ 1098:0dda /
     * status_font_height). OS-free 1:1, no Win16, no globals. */
    return top + ((bottom - top) - text_h) / 2;
}

civ_error_t dialog_dlgunits_to_pixels(const dlg_rect_t *in, int base_x,
                                      int base_y, dlg_rect_t *out)
{
    /* Orig FUN_1098_0d43 @ 1098:0d43 -> Dialog_DlgUnitsToPixels.
     * Decompiler: GETDIALOGBASEUNITS x2 (low = baseX, high = baseY),
     * Util_MemCopyFar rect in, then x axes (left/right) (v << 2) / baseX
     * and y axes (top/bottom) (v << 3) / baseY, MemCopyFar rect out.
     * Caller: Dialog_GetItemRect @ 1100:03ff when param_7 != 0 (unit
     * rect -> pixel rect before MOVEWINDOW). OS-free 1:1 on explicit
     * rects (no Win16, no globals, read-only in). */
    if (!in || !out)
        return CIV_ERR_NOENT;
    if (base_x <= 0 || base_y <= 0)
        return CIV_ERR_RANGE;
    out->left = (in->left << 2) / base_x;
    out->right = (in->right << 2) / base_x;
    out->top = (in->top << 3) / base_y;
    out->bottom = (in->bottom << 3) / base_y;
    return CIV_OK;
}

civ_error_t dialog_client_rect(int scr_l, int scr_t, int scr_r, int scr_b,
                               int org_x, int org_y, dlg_rect_t *out)
{
    /* Orig FUN_1100_03ff @ 1100:03ff -> Dialog_GetItemRect client math.
     * Decompiler: GETDLGITEM + GETWINDOWRECT -> screen rect, then
     * SCREENTOCLIENT x2 (top-left pair, then bottom-right pair) against
     * the dialog origin. Pure subtract here (handles in the STUB below).
     * OS-free 1:1, no Win16, no globals. */
    if (!out)
        return CIV_ERR_NOENT;
    out->left = scr_l - org_x;
    out->top = scr_t - org_y;
    out->right = scr_r - org_x;
    out->bottom = scr_b - org_y;
    return CIV_OK;
}

civ_error_t dlg_center_rect(const dlg_rect_t *outer, const dlg_rect_t *inner,
                            dlg_rect_t *out)
{
    /* Orig FUN_11f8_0000 @ 11f8:0000 -> Dlg_CenterRect.
     * Decompiler: *inner_out = (outer[2]+outer[0] - (w)) >> 1 etc.,
     * keeping inner size (same shape as dlg_center_window, which stays
     * PARTIAL for the SETWINDOWPOS tail; this helper returns CIV_OK so
     * the pure math is testable). OS-free 1:1, no Win16, no globals. */
    int w;
    int h;

    if (!outer || !inner || !out)
        return CIV_ERR_NOENT;
    w = inner->right - inner->left;
    h = inner->bottom - inner->top;
    out->left = ((outer->right + outer->left) - w) >> 1;
    out->top = ((outer->bottom + outer->top) - h) >> 1;
    out->right = out->left + w;
    out->bottom = out->top + h;
    return CIV_OK;
}

civ_error_t text_draw_aligned(const char *text, const dlg_rect_t *rect,
                              int format, int color)
{
    (void)text;
    (void)rect;
    (void)format;
    (void)color;
    /* TODO: Orig FUN_1098_0ed1 @ 1098:0ed1 (Text_DrawAligned: SELECTOBJECT
     * font via DAT_1420_5484/DAT_12d8_0024 + GETTEXTMETRICS/GETTEXTEXTENT
     * measure + text_align_x/text_center_y above + SETBKMODE(1)/
     * SETTEXTALIGN(0) + shadow TEXTOUT x3 when DAT_12d8_28fc == 0 and
     * format & 0x30 via Text_SetColor @ 1098:0e0b + final TEXTOUT) and
     * FUN_1098_20f4 @ 1098:20f4 (Text_DrawResString: ResMgr_GetString +
     * Util_PascalToCStr @ 1100:0053 + DrawAligned; callers 1068:06af +
     * WorldGen_LoadEarth @ 1108:0243) -> SDL_ttf bridge TBD. */
    if (!text || !rect)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t dialog_get_item_rect(int dlg_id, int item_id, dlg_rect_t *out)
{
    (void)dlg_id;
    (void)item_id;
    /* TODO: Orig FUN_1100_03ff @ 1100:03ff (Dialog_GetItemRect: GETDLGITEM
     * + GETWINDOWRECT + SCREENTOCLIENT x2, see dialog_client_rect above,
     * + optional Dialog_DlgUnitsToPixels @ 1098:0d43 + MOVEWINDOW when
     * param_7 != 0; 12 callers incl. Dialog_MenuZ_Modal @ 1220:0156)
     * -> SDL3 widget geometry TBD. Math part is LIVE above. */
    if (!out)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t dialog_show_message(const char *text, int *out_choice)
{
    (void)text;
    /* TODO: Orig FUN_1220_00d2 @ 1220:00d2 (Dialog_ShowMessage: 0x88 vs
     * 0x91 dialog id via DAT_1418_0008 + Dlg_CreateTracked @ 1098:124f +
     * Dialog_MenuZ_Modal @ 1220:0156 + Dlg_DestroyTracked @ 1098:1612 +
     * Dialog_RefreshViews @ 1220:0eb8 SENDMESSAGE 0xf to 12b0:0004/001c/
     * 0034; 40+ callers incl. City_ProcessDisaster @ 1040:068e) -> SDL3
     * modal TBD. */
    if (out_choice)
        *out_choice = -1;
    if (!text || !out_choice)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

/* --- Dialog string lists (2026-10-16): OS-free cursor/queue/source math.
 * No globals, no exit(), C17. ResMgr fetch + GDI handles stay STUB. */

civ_error_t text_res_list_advance(size_t cur_off, size_t entry_len,
                                  size_t *next_off)
{
    /* Orig Text_FillResStringList @ 1098:1f4f cursor half: *param_4 =
     * pcVar2 + *param_4 (pcVar2 = LSTRLEN+1) + *param_6++ per packed
     * entry. Overflow-safe (orig used 16-bit far cursors). */
    if (!next_off)
        return CIV_ERR_NOENT;
    if (entry_len > SIZE_MAX - cur_off - 1u)
        return CIV_ERR_RANGE;
    *next_off = cur_off + entry_len + 1u;
    return CIV_OK;
}

int main_enqueue_res_string(int *queue, size_t *count, int id)
{
    /* Orig Main_EnqueueResString @ 1008:0a8d: if (DAT_12b0_004c < 10)
     * queue[count++] = id (queue @ 1420:4473+0x71, u16 slots). Full ring
     * silently drops (returns 0, count untouched). */
    if (!queue || !count)
        return -1;
    if (*count >= (size_t)CIV_RES_QUEUE_MAX)
        return 0;
    queue[*count] = id;
    (*count)++;
    return 1;
}

/* --- Res-string tables + queue (2026-10-16): OS-free spec/queue math. --- */

civ_error_t dialog_res_list_spec(int index, int *res_id, int *first,
                                 int *last)
{
    /* Orig Dialog_InitResStringTables @ 1068:0295 switch(param_1).
     * Decompiler: case 0 fills all five via Text_FillResStringList
     * (0x82,1,0x43), (0x83,2,999), (0x84,1,999), (0x85,1,0xc),
     * (0x86,1,999); cases 1..5 fill one (uVar1 = res, uVar2 = first,
     * uVar3 = last). */
    if (!res_id || !first || !last)
        return CIV_ERR_NOENT;
    switch (index) {
    case 1:
        *res_id = CIV_RES_TECH_LIST;
        *first = 1;
        *last = 0x43;
        return CIV_OK;
    case 2:
        *res_id = CIV_RES_BUILD_LIST;
        *first = 2;
        *last = 999;
        return CIV_OK;
    case 3:
        *res_id = CIV_RES_PRODUCE_LIST;
        *first = 1;
        *last = 999;
        return CIV_OK;
    case 4:
        *res_id = CIV_RES_GOV_LIST;
        *first = 1;
        *last = 0x0c;
        return CIV_OK;
    case 5:
        *res_id = CIV_RES_UNIT_LIST;
        *first = 1;
        *last = 999;
        return CIV_OK;
    default:
        return CIV_ERR_RANGE;
    }
}

int main_res_queue_full(size_t count)
{
    /* Orig Main_EnqueueResString @ 1008:0a8d gate: DAT_12b0_004c < 10
     * (CIV_RES_QUEUE_MAX). Producers Main_ShowResString @ 1008:0add +
     * Main_PumpMessage @ 1098:02d1; reset via sys_drain_input @
     * 11f0:4ea0; consumer TBD (no NE xrefs). */
    return count >= (size_t)CIV_RES_QUEUE_MAX;
}

civ_error_t dialog_init_res_string_tables(int index)
{
    /* TODO: Orig Dialog_InitResStringTables @ 1068:0295 (mem_alloc_lock
     * 0x7fff + Text_FillResStringList @ 1098:1f4f per spec in
     * dialog_res_list_spec above (index 0 = all five) into DAT_1288_*
     * + FUN_1098_1909 dialog 0x82 show). Needs heap + live ResMgr blobs
     * (see rsc_loader.h res_get_string_copy STUB). */
    (void)index;
    if (index < 0 || index > 5)
        return CIV_ERR_RANGE;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t main_show_res_string(int id)
{
    /* TODO: Orig Main_ShowResString @ 1008:0add (ResMgr_GetString @
     * 1100:078a into local_1e + Main_EnqueueResString @ 1008:0a8d via
     * main_enqueue_res_string above). Needs live ResMgr blobs. */
    (void)id;
    return CIV_ERR_UNSUPPORTED;
}

int text_dlg_string_source(int res_id, long fallback)
{
    /* Orig Text_DrawDlgItemString @ 1098:2796 source choice: param_5
     * (res_id) == 0 ? (param_6 (fallback) != 0 ? copy path : empty) :
     * ResMgr_GetString path. */
    if (res_id != 0)
        return 2;
    if (fallback != 0)
        return 1;
    return 0;
}

civ_error_t text_set_dlg_item_text(int dlg_id, int item_id, const char *text)
{
    (void)dlg_id;
    (void)item_id;
    (void)text;
    /* TODO: Orig Text_SetDlgItemPascal @ 1100:0947 (Util_PascalToCStr @
     * 1100:0053 on the local_110 Pascal buffer, LIVE via util_pascal_to_c,
     * + SETWINDOWTEXT on the GETDLGITEM handle; sole caller
     * Text_DrawDlgItemString @ 1098:2796). Handle tail -> SDL TBD. */
    if (!text)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t text_draw_dlg_item_string(int dlg_id, int item_id, int res_id,
                                      long fallback)
{
    (void)dlg_id;
    (void)item_id;
    (void)res_id;
    (void)fallback;
    /* TODO: Orig Text_DrawDlgItemString @ 1098:2796 (Dialog_GetItemRect @
     * 1100:03ff rect math is LIVE in dialog_client_rect/
     * dialog_dlgunits_to_pixels + source via text_dlg_string_source above
     * + Text_SetDlgItemPascal @ 1100:0947; sole caller FUN_10f0_0066 @
     * 10f0:0066). GDI handle tail -> SDL TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t text_fill_res_string_list(int res_id, int first, int last)
{
    (void)res_id;
    (void)first;
    (void)last;
    /* TODO: Orig Text_FillResStringList @ 1098:1f4f (ResMgr_GetString @
     * 1100:078a per id in first..last + Util_PascalToCStr @ 1100:0053 +
     * LSTRLEN empty-stop returning NULL + cursor pack via
     * text_res_list_advance above; sole caller Dialog_InitResStringTables
     * @ 1068:0295 for Res 0x82/0x83/0x84/0x85/0x86). Needs live ResMgr
     * blobs (see rsc_loader.h res_get_string_copy STUB) + caller heap. */
    if (last < first)
        return CIV_ERR_RANGE;
    return CIV_ERR_UNSUPPORTED;
}
