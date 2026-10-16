/* Dialog stubs (+ MenuZ/custom-control surface). */
#include "dialog.h"

/* AL2: INT_MAX for help_hlp_lzss_try len cap (OS-free, no SDL). */
#include <limits.h>

civ_error_t dialog_init(void)
{
    /* DLGPROC layer (@ 1098:124f/16e2/1838, controls @ 1058:*; MenuZ @
     * 1220:0156). WONTFIX-Win16 instance data: procs are pure dispatch
     * (handled/default, no USER handles stored); SDL modal lands per-call.
     * No-op CIV_OK. */
    return CIV_OK;
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
    /* TODO: Orig Dialog_MenuZ_Modal @ 1220:0156 (body ..0d37): int
     * __cdecl16far (char*,u2,u2,u2,u2,u2,char,u4) -> index DAT_12b0_009c;
     * '_' split via Dialog_SplitMenuText @ 1220:0000, radio 500+i
     * (dialog_menuz_id_to_index/index_to_id), PEEKMESSAGE/ISDIALOGMESSAGE
     * pump do DAT_12b0_00a0, inner +0889 Text_SetColor / +0b88
     * Text_GetFontHeight, frame via Dialog_DrawFrame3D @ 1220:0d38.
     * Win16 USER-menu pump -> SDL3 modal TBD (WONTFIX do mostka SDL). */
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
    /* Orig: FUN_1098_124f @ 1098:124f -> Dlg_CreateTracked (decompiled P0c:
     * Cursor_Set(0x81) + CREATEDIALOG + MAKEPROCINSTANCE(DLGPROC @ 1098:16e2 /
     * CIVDIALOGPROC @ 1098:1838) + preset size for 0x94/0x8f via
     * dlg_id_needs_preset_size (0026 = 1, 002e = 0x200, 002c = 0x140) +
     * center via FUN_1098_0b33 @ 1098:0b67 when (flags & 1) + mem_alloc_lock
     * 0x18 ctx + GETDC + init ctl 0x65-0x6e (0x8f adds 0x323-0x332) +
     * Gfx_StoreDC @ 1098:1104 + SHOWWINDOW when (flags & 0x40) + 0xec palette
     * copy via Palette_CopyRange @ 1098:201d + SELECT/REALIZEPALETTE + push
     * to 1420:1ea0/1ec8, top @ 1420:9948. Dialog 0x94 entry is
     * Dlg_CreateTracked(0x94,0x41) in Main_RunStartupSequence @ 11e0:0000.
     * TODO: SDL3 window bridge TBD. OS-free flag/id math lives in
     * dlg_create_should_center/show + dlg_ctl_is_tracked_init +
     * dlg_tracked_push_off below. */
    return CIV_ERR_UNSUPPORTED;
}

void dlg_destroy_tracked(void)
{
    /* Orig: FUN_1098_1612 @ 1098:1612 -> Dlg_DestroyTracked (decompiled P0c:
     * DESTROYWINDOW + pop 1420:1ea0/1ec8 via dlg_tracked_pop_count below +
     * FREEPROCINSTANCE + GLOBALFREE + Mem_DisposePtr @ 1100:060b + refresh
     * via FUN_1208_14d5; 1420:17aa = 0, 1420:9960 cleared when dying).
     * TODO: SDL3 TBD. */
}

int dlg_create_should_center(unsigned flags)
{
    /* Orig Dlg_CreateTracked @ 1098:124f: if ((param_2 & 1) != 0)
     * FUN_1098_0b33 (center + MOVEWINDOW exact). */
    return ((flags & CIV_DLG_CREATE_CENTER) != 0u) ? 1 : 0;
}

int dlg_create_should_show(unsigned flags)
{
    /* Orig same: if ((param_2 & 0x40) != 0) SHOWWINDOW (0x94 intro passes
     * 0x41 = center + show). */
    return ((flags & CIV_DLG_CREATE_SHOW) != 0u) ? 1 : 0;
}

int dlg_ctl_is_tracked_init(int ctl_id)
{
    /* Orig same loop: iVar6 = 0x65; while (<= 0x6e) GETDLGITEM-guarded
     * SENDMESSAGE 0x30 per ctl. */
    return (ctl_id >= (int)CIV_DLG_TRACKED_CTL_FIRST &&
        ctl_id <= (int)CIV_DLG_TRACKED_CTL_LAST) ? 1 : 0;
}

int dlg_tracked_push_off(int count)
{
    /* Orig same push: iVar6 = DAT_1420_1ec8 * 4 (far-pair slots at
     * 0x1ea0/0x1ea2); DAT_1420_1ec8++. No cap guard in orig. */
    if (count < 0)
        return -1;
    return count * 4;
}

int dlg_tracked_pop_count(int count)
{
    /* Orig Dlg_DestroyTracked @ 1098:1612 pop: DAT_1420_1ec8 += -1 (0 clamps
     * at empty with DAT_1420_9948 = 0, else the count-2 slot becomes top). */
    if (count <= 0)
        return 0;
    return count - 1;
}

int dlg_move_extent(int size, int padded)
{
    /* Orig FUN_1098_0a0d @ 1098:0a5b exact (local_e - local_12) vs
     * FUN_1098_0aa4 @ 1098:0ae0 padded ((local_e - local_12) + 1);
     * FUN_1098_0b33 @ 1098:0b67 is the exact fixed-origin form. */
    return (padded != 0) ? size + 1 : size;
}

int dlg_move_should_use_default(int param)
{
    /* Orig 0a0d/0aa4 gate: if (param_2 == 0) param_2 = local_14 (same for
     * param_3/local_12); 0b33 skips the gate (always measured origin). */
    return (param == 0) ? 1 : 0;
}

civ_error_t anim_pump_if_active(void)
{
    /* Orig: FUN_10b0_0000 @ 10b0:0000 -> Anim_PumpIfActive.
     * TODO: while a sequence is active (flag @ 1420:1f40) run
     * Anim_AdvanceFrames @ 10b0:0461 (opcode switch 1..0xe, slot-A/B via
     * Anim_AllocSlot @ 10a8:0000 + Anim_StepSlotA/B @ 10a8:00fc/018e +
     * Anim_BlendSlotFrame @ 10a8:0228 + Anim_FreeSlot @ 10a8:00b4, dissolve
     * 12/13 via Anim_SetupDissolve @ 10b0:151b + Anim_StepDissolve @
     * 10b0:17e5 + Anim_FreeDissolve @ 10b0:172d, frames via
     * Art_LoadFrameData @ 10b8:079d); wrapped by Sys_Yield @ 1098:0157
     * (see sys.h); fazy OS-free w anim_seq_phase_step LIVE ponizej.
     * SDL timer/frame pump TBD. */
    return CIV_ERR_UNSUPPORTED;
}

int anim_advance_frames(void)
{
    /* Orig: FUN_10b0_0461 @ 10b0:0461 -> Anim_AdvanceFrames. Timed frame
     * script, opcodes 1..0xe: palette fades/selects, offscreen blits,
     * sound triggers (via Sound_PlayId @ 1150:00c9), port switches (GR
     * port layer). Returns nonzero when the sequence is finished.
     * Chain 2026-10-16 (21) answer: STUB zostaje w dialog.c (sequence
     * player, nie boot.c/game.c); zwiazek ze starfield: BRAK —
     * Ship_AnimateStarfield @ 1070:12b5 to wlasna petla Ship (Draw/Probe
     * pixel + PresentRect 0xd2 w Ship_ShowView @ 1070:0327), nie klatki
     * Anim; klatki intro/PIC ida tędy (Pic_PlayShared @ 1030:0000 caller
     * Gfx_StoreDC @ 1098:1104 jak tutejszy frame-pump co 8. tick).
     * TODO: full script engine -> SDL TBD. Stub reports done so future
     * wait-loops terminate. */
    return 1;
}

civ_error_t anim_open_sequence(void)
{
    /* Orig: FUN_10b0_008a @ 10b0:008a -> Anim_OpenSequence (decompiled
     * this pass: mem_alloc_lock 0xa6 ctx [0xfff4 on NULL] + opcode-0
     * count when param_3 == 0 + GETCLIENTRECT + *ctx = param_1 +
     * ctx[4] = timeline + ctx[5] = count + Sys_GetTickScaled @ 1100:02e1
     * base at +0x1a minus param_2 + +0x16 = 0 + max-end scan over
     * rec[7] longs + 1 + per-rec [13]/[14] = 0 + GfxPort_Create x2 at
     * +9/+0x62 with FUN_1000_14f8/FUN_1100_0876 setup + DAT_1420_1f40 =
     * ctx; callers Main_RunStartupSequence @ 11e0:0000 + MainMenu_WndProc
     * @ 11e0:0221 sequence 0x91e path).
     * TODO: GfxPort_Create x2 + GETCLIENTRECT rect + tick-base wiring ->
     * SDL3 bridge TBD. OS-free halves live in anim_seq_count_frames /
     * anim_seq_max_end below. */
    return CIV_ERR_UNSUPPORTED;
}

void anim_close_sequence(void)
{
    /* Orig: FUN_10b0_0287 @ 10b0:0287 -> Anim_CloseSequence (decompiled
     * this pass: per-rec free of opcode 3/4 slots via Anim_FreeSlot @
     * 10a8:00b4 + opcode 7 via GfxPort_Close + Mem_DisposePtr, then
     * GfxPort_ClearActive/Close x2 at +0x24/+0x62 + GLOBALFREE palette
     * when DAT_12d8_28fc == 0 + Mem_DisposePtr ctx + DAT_1420_1f40 = 0;
     * callers Main_RunStartupSequence @ 11e0:0000 + MainMenu_WndProc @
     * 11e0:0221).
     * TODO: GfxPort/Mem handle teardown -> SDL3 TBD. Intentionally void
     * (orig void; no-op until the bridge lands). */
}

int anim_seq_count_frames(const int *opcodes, size_t cap)
{
    /* Orig Anim_OpenSequence @ 10b0:008a: while (*piVar2 != 0) param_3++
     * with +0xf stride (15-word timeline recs, [0] = opcode). */
    size_t n = 0;

    if (!opcodes)
        return -1;
    while (n < cap) {
        if (opcodes[n] == 0)
            break;
        n++;
    }
    return (int)n;
}

long anim_seq_max_end(const long *ends, size_t n)
{
    /* Orig same: max over rec[7] longs (start at 0) then + 1. */
    long m = 0L;
    size_t i;

    if (!ends && n > 0)
        return -1L;
    for (i = 0; i < n; i++) {
        if (ends[i] > m)
            m = ends[i];
    }
    return m + 1L;
}

int anim_seq_is_active(long active)
{
    /* Orig FUN_10b0_0026 @ 10b0:0026: return DAT_1420_1f40 (active ctx
     * or 0); pumped by Anim_PumpIfActive @ 10b0:0000 via Sys_Yield @
     * 1098:0157. */
    return (active != 0L) ? 1 : 0;
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
    /* TODO: Orig Diplo_PostContact @ 1168:0a07 (tail of Unit_UpdateVisibility
     * @ 11c0:0395: player dialog via FUN_1160_0000 unless contacted+fresh
     * stamp @ 392b+3, both dirs |= 0x11, AI-AI via Diplo_ShouldMakePeace @
     * 1168:0f47 -> Diplo_ExchangeTechs @ 1168:1220 + Diplo_SetPeace @
     * 1168:0dda / Diplo_SetWar @ 1168:0e8e + Dialog_ShowMessage @ 1220:00d2
     * embassy tails; needs diplo bits @ 12d8:3a0a + Win16 modal + SDL).
     * OS-free bit helpers below (diplo_* LIVE) cover the state math. */
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

/* --- Chain 2026-10-16 (5): diplomacy state bits + tribute + tech exchange
 * (segment 1168). OS-free 1:1 LIVE ports. No globals, no exit(), C17. */

int diplo_contact_flags(int cur)
{
    /* Orig Diplo_PostContact @ 1168:0a07: both dirs |= 0x11. */
    return cur | 0x11;
}

int diplo_is_peace(int flags)
{
    /* Orig peace-bit @ 1168:0a07/0dda/0e8e/0f47 tests (& 2). */
    return (flags & 0x02) != 0 ? 1 : 0;
}

int diplo_is_contact(int flags)
{
    /* Orig contacted-bit (& 1) gate with the 392b+3 turn stamp. */
    return (flags & 0x01) != 0 ? 1 : 0;
}

int diplo_has_embassy(int flags)
{
    /* Orig embassy-bit (& 0x40) message gate in PostContact/ProcessTurn. */
    return (flags & 0x40) != 0 ? 1 : 0;
}

int diplo_apply_peace(int cur)
{
    /* Orig Diplo_SetPeace @ 1168:0dda: |= param_3 (0x02) then & ~0x20. */
    cur |= 0x02;
    cur &= ~0x20;
    return cur;
}

int diplo_apply_war(int cur)
{
    /* Orig Diplo_SetWar @ 1168:0e8e: &= ~param_3 (0x02) then & ~0x04. */
    cur &= ~0x02;
    cur &= ~0x04;
    return cur;
}

int diplo_tribute_amount(int gold)
{
    /* Orig Diplo_ProcessTurn @ 1168:0000: iVar2 = Util_Clamp(gold/100,1,10)
     * * 0x32 (Util_Clamp @ 11c8:0063 inlined: lo 1, hi 10). Remainder
     * gold % 100 is kept by the demander, not paid. */
    int q;

    if (gold < 0)
        return -1;
    q = gold / 100;
    if (q < 1)
        q = 1;
    if (q > 10)
        q = 10;
    return q * 0x32;
}

int diplo_should_exchange(int a_exclusive, int b_exclusive)
{
    /* Orig Diplo_ExchangeTechs @ 1168:1220: uVar3 == 3 (both directions own
     * an exclusive tech among 0..0x48) -> swap pair via 1170:2aaa. */
    return (a_exclusive != 0 && b_exclusive != 0) ? 1 : 0;
}

/* --- Chain 2026-10-16 (6): diplo turn tick + government rules (task goals 1/2).
 * Caller/callee: Game_TurnLoopTick @ 1088:0000 -> Diplo_ProcessTurn @ 1168:0000
 * (per-civ turn) -> Diplo_ChangeGovernment @ 1168:06c1 + Diplo_SetPeace/SetWar
 * @ 1168:0dda/0e8e; PostContact @ 1168:0a07 is the per-contact-event sibling
 * (Unit_UpdateVisibility @ 11c0:0395 tail). OS-free 1:1 LIVE, no globals. */

int diplo_government_tick(int turn)
{
    /* Orig Diplo_ProcessTurn @ 1168:0000: (g_nTurn & 3) == 0. */
    return ((turn & 3) == 0) ? 1 : 0;
}

int diplo_spy_decay_step(int cur, int rng3)
{
    /* Orig Diplo_ProcessTurn @ 1168:0000: (flags & 0x100) && Rng_Range @
     * 11c8:003d (3) == 0 -> &= ~0x100, &= ~0x02, |= 0x200. */
    if ((cur & 0x100) != 0 && rng3 == 0) {
        cur &= ~0x100;
        cur &= ~0x02;
        cur |= 0x200;
    }
    return cur;
}

int diplo_war_end_tick(int turn)
{
    /* Orig Diplo_ProcessTurn @ 1168:0000: (g_nTurn & 0xf) == 0. */
    return ((turn & 0x0f) == 0) ? 1 : 0;
}

int diplo_war_end_clear(int cur)
{
    /* Orig Diplo_ProcessTurn @ 1168:0000 sweep: &= ~0x01 (contact both dirs)
     * + &= ~0x10 (seen both dirs). */
    cur &= ~0x01;
    cur &= ~0x10;
    return cur;
}

int diplo_tribute_turn(int civ, int turn)
{
    /* Orig Diplo_ProcessTurn @ 1168:0000: param_1 != 0 && param_1 << 2 ==
     * (g_nTurn & 0x1f) (player-exclusion g_nPlayerCiv != param_1 stays
     * caller-side). */
    if (civ <= 0)
        return 0;
    return (((civ << 2) == (turn & 0x1f)) ? 1 : 0);
}

int diplo_government_factor(int gov)
{
    /* Orig Diplo_ChangeGovernment @ 1168:06c1: 4 - param_2 / 2 at 39f3+7. */
    if (gov < 0)
        return -1;
    return 4 - gov / 2;
}

int diplo_government_needs_tech(int gov)
{
    /* Orig Diplo_ChangeGovernment @ 1168:06c1 gates via Tech_HasTech @
     * 1170:3181 (wonder-1 bypass stays caller-side). */
    switch (gov) {
    case 2:
        return 5;
    case 3:
        return 0x2b;
    case 4:
        return 0x2c;
    case 5:
        return 4;
    default:
        return -1;
    }
}

int diplo_government_available(int gov, int has_req_tech, int wonder_open)
{
    /* Orig same: gov 0/1 always; else wonder 1 open bypasses, else tech. */
    if (gov <= 1 && gov >= 0)
        return 1;
    if (gov < 0)
        return 0;
    if (wonder_open != 0)
        return 1;
    return (has_req_tech != 0) ? 1 : 0;
}

int diplo_peace_forced(int opt_flag)
{
    /* Orig Diplo_ShouldMakePeace @ 1168:0f47 head: (DAT_12d8_3e9c & 1) == 0
     * -> 1 (peace forced, skip rank/military/tech-trade tables). */
    return ((opt_flag & 1) == 0) ? 1 : 0;
}

/* --- Chain 2026-10-16 (7): contact dialog + tech/military tables. --- */

int diplo_tech_table_off(int civ, int tech)
{
    /* Orig Diplo_GetTechValue @ 1168:1366 / Diplo_GetTechCost @ 1168:138d:
     * BX = civ*0x20 + tech*2 past 12d8:3c9c/3d9c (ES=0x12d8 per disasm
     * MOV AX,0x12d8 / MOV ES,AX; Ghidra 1420: labels are the same offsets
     * 0x3c9c = 0x3c97+5, 0x3d9c = 0x3d88+0x14, real seg is 12d8). */
    if (civ < 0 || tech < 0 || tech >= CIV_DIPLO_TECH_COUNT)
        return -1;
    return civ * CIV_DIPLO_TECH_CIV_STRIDE + tech;
}

int diplo_tech_value_at(const short *table, int nwords, int civ, int tech)
{
    /* Orig Diplo_GetTechValue @ 1168:1366 word read on the runtime BSS. */
    int off;

    if (!table || nwords <= 0)
        return -1;
    off = diplo_tech_table_off(civ, tech);
    if (off < 0 || off >= nwords)
        return -1;
    return (int)table[off];
}

int diplo_tech_cost_at(const short *table, int nwords, int civ, int tech)
{
    /* Orig Diplo_GetTechCost @ 1168:138d word read, same stride past
     * 12d8:3d9c. */
    int off;

    if (!table || nwords <= 0)
        return -1;
    off = diplo_tech_table_off(civ, tech);
    if (off < 0 || off >= nwords)
        return -1;
    return (int)table[off];
}

int diplo_military_at(const uint8_t *row, int row_len)
{
    /* Orig Diplo_GetMilitaryScore @ 1168:13b4: ES:[civ*0x38+0x4160] word
     * (0x4160 = 0x4155+0xb); +0xb is UNALIGNED (bytes 0xb/0xc), x86 OK.
     * LE decode, no alignment assumption. */
    if (!row || row_len < CIV_DIPLO_MIL_SCORE_OFF + 2)
        return -1;
    return (int)row[CIV_DIPLO_MIL_SCORE_OFF] |
           ((int)row[CIV_DIPLO_MIL_SCORE_OFF + 1] << 8);
}

int diplo_contact_tribute_scaled(int base, int difficulty)
{
    /* Orig Diplo_OpenContactDialog @ 1160:0000: iVar6 =
     * Util_Clamp((iVar6*(diff+1))/0x20,0,0x14)*0x32 (Util_Clamp @
     * 11c8:0063 inlined, lo 0 hi 0x14). */
    long v;

    if (base < 0 || difficulty < 0)
        return -1;
    v = ((long)base * (long)(difficulty + 1)) / 0x20L;
    if (v < 0)
        v = 0;
    if (v > 0x14)
        v = 0x14;
    return (int)(v * 0x32L);
}

int diplo_contact_afford_snap(int demand, int gold)
{
    /* Orig same: gold < demand <= gold*2 && gold > 0x31 ->
     * demand = (gold/0x32)*0x32 (round down to tribute units). */
    if (gold < 0 || demand < 0)
        return -1;
    if (gold < demand && demand <= gold * 2 && gold > 0x31)
        return (gold / 0x32) * 0x32;
    return demand;
}

int diplo_mercenary_amount(int rank_target, int gold_foe)
{
    /* Orig MERCENARY branch @ 1160:0000: iVar6 = (rank/2)*(gold/100);
     * Util_Clamp(iVar6/0x32,2,9999)*0x32. */
    long v;
    long q;

    if (rank_target < 0 || gold_foe < 0)
        return -1;
    v = (long)(rank_target / 2) * (long)(gold_foe / 100);
    q = v / 0x32L;
    if (q < 2)
        q = 2;
    if (q > 9999)
        q = 9999;
    return (int)(q * 0x32L);
}

int diplo_threat_gold(int score16, int gold_foe)
{
    /* Orig THREAT/GIVEMORE @ 1160:0000: Util_Clamp(local16,0,gold/0x32)
     * *0x32 (0 when the foe holds nothing). */
    int cap;

    if (score16 < 0 || gold_foe < 0)
        return -1;
    cap = gold_foe / 0x32;
    if (score16 > cap)
        score16 = cap;
    return score16 * 0x32;
}

int diplo_peace_stamp(int turn)
{
    /* Orig PEACE accept @ 1160:0000: *(392b+3+civ*2) = g_nTurn+0x10 via
     * FUN_1160_257a/25fa pair. */
    return turn + 0x10;
}

civ_error_t diplo_open_contact_dialog(int civ, int x, int y, int mode)
{
    (void)civ;
    (void)x;
    (void)y;
    (void)mode;
    /* TODO: Orig Diplo_OpenContactDialog @ 1160:0000 (tribute scan 1..0xe via
     * Diplo_GetTechValue/Cost @ 1168:1366/138d + military gates via
     * Diplo_GetMilitaryScore @ 1168:13b4 + wonder gates via
     * City_IsWonderOpen @ 1030:7e57 + option menus EXCHANGE/TAKECIV/TRIBUTE/
     * REJECT/PROVOKE/PEACE/GIVE/GROVEL/MOBILIZE/ATTITUDE/THREAT/MERCENARY via
     * Diplo_PromptContact @ 1160:20c0 + Diplo_RunContactMenu @ 1160:328e +
     * Diplo_SetPeace/SetWar @ 1168:0dda/0e8e + FUN_1170_2aaa grants +
     * Dialog_ShowMessage @ 1220:00d2 + replay logs; treasuries 43b8+0x8c).
     * Win16 MenuZ/dialog -> SDL modal TBD. Pure tribute/tech math lives in
     * diplo_tribute_amount/diplo_contact_tribute_scaled/diplo_tech_value_at/
     * diplo_tech_cost_at/diplo_military_at above (LIVE). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t diplo_prompt_contact(int civ, int x, int y)
{
    (void)civ;
    (void)x;
    (void)y;
    /* TODO: Orig Diplo_PromptContact @ 1160:20c0 (emissary Yes/No message on
     * 24fa/12d8 + embassy Check-Intelligence tail + leader portrait dialog
     * 0x8e via Dlg_CreateTracked @ 1098:124f + FUN_1160_2a38/233c + Sound
     * @ 1150:019e). Win16 dialog -> SDL modal TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t diplo_run_contact_menu(int *out_choice)
{
    if (!out_choice)
        return CIV_ERR_NOENT;
    *out_choice = -1;
    /* TODO: Orig Diplo_RunContactMenu @ 1160:328e (MenuZ modal on 1420:31b4
     * via Dialog_MenuZ_Modal @ 1220:0156 + port save/restore + present).
     * Win16 modal -> SDL TBD. */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (10): Dialog_DrawFrame3D @ 1220:0d38 geometry.
 * OS-free 1:1 LIVE. No globals, no exit(), C17. See dialog.h. */

int dialog_frame3d_layers(void)
{
    /* Orig @ 1220:0d38: for (iVar4 = 0; iVar4 < 3; iVar4++) draw +
     * INFLATERECT(-1,-1) (after the head MemCopy + right/bottom -1 +
     * INFLATERECT(3,3)). */
    return 3;
}

int dialog_frame3d_dark(void)
{
    /* Orig same: CREATEPEN(...,0x8080,0x80,1) selected for the top/left L
     * (MOVETO bl → LINETO tl → LINETO tr per layer). */
    return 0x8080;
}

int dialog_frame3d_light(void)
{
    /* Orig same: CREATEPEN(...,0xe0e0,0xe0,1) selected for the
     * bottom/right L (MOVETO bl → LINETO br → LINETO tr per layer). */
    return 0xe0e0;
}

civ_error_t dialog_frame3d_layer(const dlg_rect_t *rc, int layer,
                                 dlg_rect_t *out)
{
    /* Orig same INFLATERECT(-1,-1) per layer on the caller base rect. */
    if (!rc || !out)
        return CIV_ERR_NOENT;
    if (layer < 0 || layer > 2)
        return CIV_ERR_RANGE;
    out->left = rc->left + layer;
    out->top = rc->top + layer;
    out->right = rc->right - layer;
    out->bottom = rc->bottom - layer;
    if (out->right <= out->left || out->bottom <= out->top)
        return CIV_ERR_RANGE;
    return CIV_OK;
}

civ_error_t dialog_draw_frame3d(const dlg_rect_t *rc)
{
    (void)rc;
    /* TODO: Orig Dialog_DrawFrame3D @ 1220:0d38 GDI tail (CREATEPEN 0x8080
     * / 0xe0e0 + SELECTOBJECT + MOVETO/LINETO L-pairs per layer from
     * dialog_frame3d_layer above + INFLATERECT x3 + SELECT restore +
     * DELETEOBJECTs; callers Census_DrawTechBadge @ 1018:24c5 + MenuZ @
     * 1220:0156) -> SDL_RenderDrawLines TBD. Inset/color math lives above
     * (LIVE). */
    if (!rc)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (11): Text_Draw* @ 10d8 + font dispatch @ 11e8:0b40.
 * OS-free 1:1 LIVE + GDI STUBs. No globals, no exit(), C17. See dialog.h. */

int text_shadow_offset(int height)
{
    /* Orig Text_DrawShadowed @ 10d8:02df: local_126 < 0x12 ? x : x+1. */
    if (height < 0)
        return 0;
    return (height < 0x12) ? 0 : 1;
}

int text_centered_x(int x, int w)
{
    /* Orig Text_DrawCentered @ 10d8:0528: Text_DrawClipped(x - w/2); same in
     * Text_DrawShadowedCentered @ 10d8:0495 for the shadow pass. */
    return x - w / 2;
}

int text_measure_scaled(int extent)
{
    /* Orig Text_MeasureWidth @ 10d8:05b8: (extent*10)/0x10 after
     * Text_MeasurePascal @ 1100:00a1. */
    if (extent < 0)
        return 0;
    return (extent * 10) / 0x10;
}

int text_pascal_len(const char *s)
{
    /* Orig Text_PackPascal @ 1100:0000 len half: buf[0] = Util_StrLen. */
    int n = 0;

    if (!s)
        return -1;
    while (s[n] != '\0') {
        n++;
        if (n >= 255)
            break;
    }
    return n;
}

/* Orig table @ 11e8:0d1d full dump (25 entries: 1..8, 200..215, 333).
 * Function-local static const view (read-only, no globals mutated).
 * Aliases resolved in the rows: 2->slot 0xf, 3->0x11, 7->0x10,
 * 207->0x10, 213->0x10 (jmp chains 0b80/0b82/0bb7/0c32/0c7b).
 * Ghidra labels Text_FontHandler_Slot*_H* mark the 20 distinct bodies. */
static const text_font_entry_t text_font_dump[] = {
    { 1, 0x0f, 0x0c }, { 2, 0x0f, 0x0c }, { 3, 0x11, 0x12 },
    { 4, 0x13, 0x1e }, { 5, 0x12, 0x18 }, { 6, 0x10, 0x0e },
    { 7, 0x10, 0x0e }, { 8, 0x11, 0x12 }, { 200, 0x00, 0x0e },
    { 201, 0x01, 0x0e }, { 202, 0x02, 0x0e }, { 203, 0x03, 0x0e },
    { 204, 0x04, 0x0e }, { 205, 0x05, 0x0e }, { 206, 0x06, 0x0e },
    { 207, 0x10, 0x0e }, { 208, 0x07, 0x0e }, { 209, 0x08, 0x0e },
    { 210, 0x09, 0x0e }, { 211, 0x0a, 0x0e }, { 212, 0x0b, 0x0e },
    { 213, 0x10, 0x0e }, { 214, 0x0c, 0x0e }, { 215, 0x0d, 0x0e },
    { 333, 0x14, 0x24 },
};

const text_font_entry_t *text_font_table(int *count_out)
{
    /* Orig table @ 11e8:0d1d dump accessor (borrowed view, no copy). */
    if (count_out)
        *count_out = (int)(sizeof(text_font_dump) / sizeof(text_font_dump[0]));
    return text_font_dump;
}

static int text_font_table_lookup(int id, int want_slot)
{
    /* Orig Text_DispatchFontHandler @ 11e8:0b40 linear scan (CX 0x19 over
     * @ 11e8:0d1d + indirect jmp via +0x32). */
    size_t i;

    for (i = 0; i < sizeof(text_font_dump) / sizeof(text_font_dump[0]); i++) {
        if (text_font_dump[i].id == id)
            return want_slot ? text_font_dump[i].slot : text_font_dump[i].h;
    }
    return -1;
}

int text_font_slot_for_id(int id)
{
    return text_font_table_lookup(id, 1);
}

int text_font_height_for_id(int id)
{
    return text_font_table_lookup(id, 0);
}

civ_error_t text_dispatch_font(int id, int *slot_out, int *h_out)
{
    /* Orig Text_DispatchFontHandler @ 11e8:0b40 combined half (slot +
     * SI height; shared tail @ 11e8:0cb0 with Text_FontPreApply @
     * 1098:0186 + Text_FontPostApply @ 1098:0194 stays in the
     * text_select_font STUB). */
    size_t i;

    if (!slot_out || !h_out)
        return CIV_ERR_NOENT;
    for (i = 0; i < sizeof(text_font_dump) / sizeof(text_font_dump[0]); i++) {
        if (text_font_dump[i].id == id) {
            *slot_out = text_font_dump[i].slot;
            *h_out = text_font_dump[i].h;
            return CIV_OK;
        }
    }
    return CIV_ERR_RANGE;
}

civ_error_t text_pack_pascal(char *buf, size_t cap)
{
    /* Orig Text_PackPascal @ 1100:0000 C->Pascal pack (len + shift right
     * by one, buf[0] = len). Same framing as Res_CopyPascal @ 11f8:00fc
     * / Res_AppendPascal @ 11f8:014a (buf[0]-is-len) in rsc_loader.h;
     * unpack side is Util_PascalToCStr @ 1100:0053 (util_pascal_to_c). */
    size_t n = 0;
    size_t i;

    if (!buf)
        return CIV_ERR_NOENT;
    if (cap == 0u)
        return CIV_ERR_TRUNCATED;
    while (n < cap && buf[n] != '\0')
        n++;
    if (n >= cap)
        return CIV_ERR_TRUNCATED;
    if (n > 255u)
        return CIV_ERR_RANGE;
    for (i = n; i > 0u; i--)
        buf[i] = buf[i - 1u];
    buf[0] = (char)(unsigned char)n;
    return CIV_OK;
}

civ_error_t text_pascal_payload(const char *buf, size_t cap,
                                size_t *pay_len_out)
{
    /* Orig Pascal len half (PackPascal @ 1100:0000 write side /
     * MeasurePascal @ 1100:00a1 read side: len = buf[0], chars = buf+1;
     * GETTEXTEXTENT width itself needs the DC and stays STUB). */
    unsigned pay;

    if (pay_len_out)
        *pay_len_out = 0u;
    if (!buf || !pay_len_out)
        return CIV_ERR_NOENT;
    if (cap == 0u)
        return CIV_ERR_TRUNCATED;
    pay = (unsigned char)buf[0];
    if ((size_t)1u + (size_t)pay > cap)
        return CIV_ERR_TRUNCATED;
    *pay_len_out = (size_t)pay;
    return CIV_OK;
}

civ_error_t text_draw_shadowed(const char *text, int x, int y, int color,
                               int shadow)
{
    (void)x;
    (void)y;
    (void)color;
    (void)shadow;
    /* TODO: Orig Text_DrawShadowed @ 10d8:02df GDI tail (GfxPort_
     * SelectForCoords @ 1220:0f2f + DispatchFont @ 11e8:0b40 + SELECTOBJECT
     * + GETTEXTMETRICS + SETBKMODE(1) + Text_SetColor @ 1098:0e0b x2 +
     * TEXTOUT shadow/main) -> SDL_ttf TBD. Measure lives in
     * text_shadow_offset/text_measure_scaled above. */
    if (!text)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t text_draw_at(const char *text, int x, int y, int color)
{
    (void)x;
    (void)y;
    (void)color;
    /* TODO: Orig Text_DrawAt @ 10d8:01fa (wrapper DrawShadowed(...,0)) ->
     * same SDL_ttf tail as text_draw_shadowed. */
    if (!text)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t text_draw_clipped(const char *text, int x, int y, int color)
{
    (void)x;
    (void)y;
    (void)color;
    /* TODO: Orig Text_DrawClipped @ 10d8:01b0 (SelectForCoords +
     * DispatchFont + DrawDirect @ 10d8:00e0) -> SDL_ttf TBD. */
    if (!text)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t text_draw_direct(const char *text, int x, int y, int color)
{
    (void)x;
    (void)y;
    (void)color;
    /* TODO: Orig Text_DrawDirect @ 10d8:00e0 (Text_SetColor + SELECTOBJECT
     * + MOVETO + PackPascal @ 1100:0000 + DrawPascalAtOrigin @ 1100:017b)
     * -> SDL_ttf TBD. */
    if (!text)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t text_draw_centered(const char *text, int x, int y, int color)
{
    (void)x;
    (void)y;
    (void)color;
    /* TODO: Orig Text_DrawCentered @ 10d8:0528 (MeasureWidth @ 10d8:05b8 +
     * x-w/2 via text_centered_x + DrawClipped) -> SDL_ttf TBD. */
    if (!text)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t text_draw_header_centered(const char *text, int x, int y,
                                      int color)
{
    (void)x;
    (void)y;
    (void)color;
    /* TODO: Orig Text_DrawHeaderCentered @ 10d8:046e (wrapper
     * DrawShadowedCentered @ 10d8:0495(...,0), tytuł 0xa0,2) -> SDL_ttf. */
    if (!text)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t text_select_font(int id)
{
    /* Orig Text_SelectFont @ 1100:0a33 (thin wrapper DispatchFontHandler @
     * 11e8:0b40) + LOGFONT install in the 0cb0 tail (DAT_1420_9960/+0x12,
     * DAT_1420_17cc/+0x28, 1098:0186/0194) -> SDL_ttf face select TBD.
     * PARTIAL: id->(slot,h) validation is LIVE via text_dispatch_font
     * (RANGE on unknown id, NOENT on NULL outs); the SELECTOBJECT/GDI
     * install tail stays CIV_ERR_UNSUPPORTED. */
    int slot = 0;
    int h = 0;
    civ_error_t err;

    err = text_dispatch_font(id, &slot, &h);
    if (err != CIV_OK)
        return err;
    (void)slot;
    (void)h;
    /* TODO: SDL_ttf face select for (slot, h) + Pre/PostApply bridge. */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (13): font pre/post tails + Pascal ellipsis (goals
 * 3-5; civ.exe, x86:LE:16:Protected Mode, C17, no exit, no globals) --- */

void text_font_pre_apply(void)
{
    /* Orig Text_FontPreApply @ 1098:0186: empty far stub (MOV AX,DS/NOP/
     * INC BP/PUSH BP/MOV BP,SP/PUSH DS/MOV DS,AX/POP DS/POP BP/DEC BP/
     * RETF — pure DS dance, no body). Called ONLY from the dispatch tail
     * @ 11e8:0cb0 when DAT_1420_b384 != 0; no direct callers. NOOP. */
}

void text_font_post_apply(void)
{
    /* Orig Text_FontPostApply @ 1098:0194: same empty far stub shape as
     * PreApply above. Called ONLY from the dispatch tail @ 11e8:0d0e as
     * PostApply(0x45cb) when DAT_1420_b384 != 0 (after the SI stores to
     * DAT_1420_9960/+0x12 and DAT_1420_17cc/+0x28); no direct callers.
     * NOOP. */
}

civ_error_t text_fit_pascal_ellipsis(char *buf, size_t cap, int max_width,
                                     int (*measure)(const char *s, void *ctx),
                                     void *ctx)
{
    /* Orig Text_FitPascalEllipsis @ 1098:0000 measure/truncate loop 1:1 on
     * a C string (orig packs a local Pascal copy, measures via
     * Text_MeasurePascal @ 1100:00a1 = GETTEXTEXTENT on buf[0]/buf+1, then
     * writes the single '.' + NUL back into the CALLER buffer at the
     * truncated length; net caller-visible effect = truncate + "." ported
     * here). Shape: w = measure(buf); if (max < w): bias local_6 = 4 for
     * the dot; while (len > 0 && max < measure(buf)+4) len--; while
     * (len > 0 && buf[len-1] == ' ') len--; buf[len] = '.', buf[len+1] = 0
     * (single dot, NOT "..."). NULL buf/measure -> NOENT; cap == 0 ->
     * TRUNCATED; empty/unfittable buf -> "." (orig writes the dot even at
     * len 0); no room for dot+NUL -> TRUNCATED (orig far buffer assumed
     * room; we bounds-check instead of faulting). measure == GETTEXTEXTENT
     * side (FNT side = font_text_width in boot.h; consistency note in
     * dialog.h chain-13). No globals, no exit(). */
    size_t len = 0;

    if (!buf || !measure)
        return CIV_ERR_NOENT;
    if (cap == 0u)
        return CIV_ERR_TRUNCATED;
    while (len < cap && buf[len] != '\0')
        len++;
    if (len >= cap)
        return CIV_ERR_TRUNCATED;
    if (max_width >= measure(buf, ctx))
        return CIV_OK;
    while (len > 0u && max_width < measure(buf, ctx) + 4) {
        len--;
        buf[len] = '\0';
    }
    while (len > 0u && buf[len - 1u] == ' ') {
        len--;
        buf[len] = '\0';
    }
    if (len + 1u >= cap)
        return CIV_ERR_TRUNCATED;
    buf[len] = '.';
    buf[len + 1u] = '\0';
    return CIV_OK;
}

int text_fit_scaled_width(int w)
{
    /* Orig Text_FitPascalEllipsisScaled @ 11d0:02e4 width half: 1:1
     * (w*16)/10 — inverse of Text_MeasureWidth @ 10d8:05b8 (w*10)/0x10
     * scale via text_measure_scaled. No clamp (negative stays negative
     * and forces the ellipsis, like orig SI). No globals. */
    return (w * 16) / 10;
}

civ_error_t text_fit_pascal_ellipsis_scaled(char *buf, size_t cap, int w,
                                            int (*measure)(const char *s,
                                                           void *ctx),
                                            void *ctx)
{
    /* Orig Text_FitPascalEllipsisScaled @ 11d0:02e4 driver: forwards to
     * Text_FitPascalEllipsis with max = (w*16)/10 (callers: census
     * 1018:0be0/208f, FUN_1030_8d33, FUN_1188_0b3e, Pic_PlayShared
     * @ 1030:0000). Same contracts as text_fit_pascal_ellipsis. */
    return text_fit_pascal_ellipsis(buf, cap, text_fit_scaled_width(w),
                                    measure, ctx);
}

/* --- Chain 2026-10-16 (14): popup ellipsis limit (goal 4). --- */

int unit_popup_text_limit(int panel_left, int panel_right)
{
    /* Orig Unit_ShowInfoPopup @ 11f0:468c: iVar4 = (local_1a - local_1e) +
     * -8 (status-panel width - 8; WdwStatus_CalcPanelRect +
     * Status_DrawPanelFrame tail). Limit dla obu linii Fit w popupie
     * (unit-name @ 11f0:4808 + city-name via City_AppendName @ 11f0:4a76;
     * scaled pola census/Pic_PlayShared używają text_fit_scaled_width).
     * OS-free 1:1, bez clamp (ujemny max wymusza "." w pętli Fit). */
    return (panel_right - panel_left) - 8;
}

/* --- Chain 2026-10-16 (22): Anim opcode decoder (goal 1). See dialog.h.
 * OS-free 1:1 LIVE (no Win16, no globals, no exit(), read-only). Player
 * (anim_pump_if_active / anim_advance_frames above) stays STUB/PARTIAL. */

int anim_opcode_is_valid(int op)
{
    /* Orig Anim_AdvanceFrames @ 10b0:0461 switch *local_8: cases 1..0xe,
     * default = skip (no action, next frame). */
    return (op >= ANIM_OPCODE_MIN && op <= ANIM_OPCODE_MAX) ? 1 : 0;
}

int anim_opcode_is_palette(int op)
{
    /* Orig same: 1 (fade-zero via Palette_AnimateRange @ 1090:0376), 2
     * (palette-set copy), 0xe (rotate-shift + AnimateRange). */
    return (op == 1 || op == 2 || op == 0x0e) ? 1 : 0;
}

int anim_opcode_is_slot(int op)
{
    /* Orig same: 3 (Alloc @ 10a8:0000 + StepA @ 10a8:00fc + Free @
     * 10a8:00b4), 4 (same with StepB @ 10a8:018e). No Sound_PlayId @
     * 1150:00c9 inside (sound rides caller-side in Main_RunStartupSequence
     * @ 11e0:0000, see boot.h). */
    return (op == 3 || op == 4) ? 1 : 0;
}

int anim_opcode_needs_store(int op)
{
    /* Orig same direct Gfx_StoreDC @ 1098:1104 cases: 5 (port-switch),
     * 7 (caption, both branches), 8 (fill), 9 (swap). The every-8-tick
     * pump StoreDC lives in anim_tick_needs_store below. */
    return (op == 5 || op == 7 || op == 8 || op == 9) ? 1 : 0;
}

int anim_opcode_is_callback(int op)
{
    /* Orig case 10: (*(param_1+0x51))(...) user hook, no default draw. */
    return (op == 10) ? 1 : 0;
}

int anim_opcode_is_anim(int op)
{
    /* Orig cases 12/13: FUN_10b0_151b setup + FUN_10b0_17e5 frame +
     * FUN_10b0_172d free pair (12 with the 0xc palette pre-copy). */
    return (op == 12 || op == 13) ? 1 : 0;
}

int anim_tick_needs_store(long tick)
{
    /* Orig frame-pump head: (*(tick & 7)) == 0 -> Gfx_StoreDC @
     * 1098:1104 + Util_MemCopyFar @ 1000:137f (double-buffer sync). */
    return ((tick & 7L) == 0) ? 1 : 0;
}

int anim_seq_is_finished(long cur, long end)
{
    /* Orig tail: cur < end -> busy (0), else done (nonzero). */
    return (cur >= end) ? 1 : 0;
}

/* --- Chain 2026-10-16 (23): Anim slot struct + sequence phases (goals 1/2).
 * See dialog.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()). */

civ_error_t anim_slot_init(anim_slot_t *slot, int art, int geo0, int geo1,
                           int geo2, int max_frames, int ticks_per_frame,
                           long start_tick)
{
    /* Orig Anim_AllocSlot @ 10a8:0000 field init (disasm stores per field;
     * DAT_1420_17a6 gate + mem_alloc_lock 0x14 + Mem_ReallocHandle handle
     * tail stay caller-side STUB). */
    if (!slot)
        return CIV_ERR_NOENT;
    if (max_frames <= 0 || ticks_per_frame <= 0)
        return CIV_ERR_RANGE;
    slot->art = art;
    slot->geo0 = geo0;
    slot->geo1 = geo1;
    slot->geo2 = geo2;
    slot->max_frames = max_frames;
    slot->cur_frame = 0;
    slot->ticks_per_frame = ticks_per_frame;
    slot->start_tick = start_tick;
    return CIV_OK;
}

long anim_slot_frame_at(long start, long now, int ticks_per_frame)
{
    /* Orig Anim_StepSlotA/B @ 10a8:00fc/018e: (now - start) / per. */
    if (ticks_per_frame <= 0)
        return -1L;
    return (now - start) / (long)ticks_per_frame;
}

int anim_slot_is_finished(long frame, int max_frames)
{
    /* Orig same: (max < frame) -> CONCAT31(...,1) done. */
    return (frame > (long)max_frames) ? 1 : 0;
}

int anim_slot_blend(int src, long frame, int max_frames)
{
    /* Orig Anim_BlendSlotFrame @ 10a8:0228 per-channel: (src * frame) / max
     * (u8 channels, alpha byte copied verbatim caller-side). */
    long v;

    if (src < 0 || src > 255)
        return -1;
    if (max_frames <= 0 || frame < 0)
        return -1;
    v = ((long)src * frame) / (long)max_frames;
    if (v < 0)
        v = 0;
    if (v > 255)
        v = 255;
    return (int)v;
}

long anim_slot_play_frame(int op, long frame, int max_frames)
{
    /* Orig StepA (op 3) forward vs StepB (op 4) reverse select. */
    if (op == ANIM_SLOT_OP_A)
        return frame;
    if (op == ANIM_SLOT_OP_B)
        return (long)max_frames - frame;
    return -1L;
}

anim_seq_phase_t anim_seq_phase_step(anim_seq_phase_t phase, int opened,
                                     int pumped, int finished, int closed)
{
    /* Orig open -> pump -> advance -> close lifecycle (Anim_OpenSequence @
     * 10b0:008a + Anim_PumpIfActive @ 10b0:0000 + Anim_AdvanceFrames @
     * 10b0:0461 + Anim_CloseSequence @ 10b0:0287; DAT_1420_1f40 active flag
     * + GfxPort_Create/Close + Art_LoadFrameData @ 10b8:079d frame loads
     * stay caller-side STUB). */
    switch (phase) {
    case ANIM_SEQ_CLOSED:
        return (opened != 0) ? ANIM_SEQ_OPEN : ANIM_SEQ_CLOSED;
    case ANIM_SEQ_OPEN:
        return (pumped != 0) ? ANIM_SEQ_PLAYING : ANIM_SEQ_OPEN;
    case ANIM_SEQ_PLAYING:
        return (finished != 0) ? ANIM_SEQ_DONE : ANIM_SEQ_PLAYING;
    case ANIM_SEQ_DONE:
        return (closed != 0) ? ANIM_SEQ_CLOSED : ANIM_SEQ_DONE;
    default:
        return ANIM_SEQ_CLOSED;
    }
}

/* --- Chain 2026-10-16 (24): dissolve 12/13 + clock rebase (goals 1/2). ---
 * See dialog.h block. OS-free 1:1 LIVE (no GDI, no globals, no exit()).
 * GDI tails (GLOBALLOCK bitmaps + Art_PortDataToBitmap @ 10b8:0e36 +
 * GfxPort_SetActive/ClearActive + palette AnimateRange/CopyRange) stay in
 * anim_pump_if_active/anim_advance_frames STUBs above. */

int anim_dissolve_rand_index(unsigned raw, int n)
{
    /* Orig Anim_SetupDissolve @ 10b0:151b: (uVar & 0x7fff) % iVar1 (Rng_NextRaw
     * @ 1000:1729 draw via FUN_1100_03ea @ 1100:03ea thunk; modulo shape,
     * unlike Rng_Range @ 11c8:003d scaling (raw*n>>15) in sys.c). */
    if (n <= 0)
        return -1;
    return (int)((raw & 0x7fffu) % (unsigned)n);
}

civ_error_t anim_dissolve_perm_init(int *perm, size_t n)
{
    /* Orig same: for (iVar5 = iVar1; iVar5 != 0; iVar5--) pu[iVar5+0x1ed] =
     * iVar5-1 (slots 1..w hold 0..w-1). Shifted base here: perm[0..n) = identity. */
    size_t i;

    if (!perm)
        return CIV_ERR_NOENT;
    if (n == 0u || n > (size_t)ANIM_DISSOLVE_MAX_ENTRIES)
        return CIV_ERR_RANGE;
    for (i = 0; i < n; i++)
        perm[i] = (int)i;
    return CIV_OK;
}

civ_error_t anim_dissolve_swap(int *perm, size_t n, unsigned r1, unsigned r2)
{
    /* Orig same shuffle loop: for (local_a = 0; local_a < iVar1; local_a++)
     * { i = rand()%w; j = rand()%w; swap(pu[i+0x1ee], pu[j+0x1ee]); } (w
     * random transpositions, not a single FY pass). One transposition here;
     * caller loops w times with fresh Rng_NextRaw draws. */
    size_t a;
    size_t b;
    int tmp;

    if (!perm)
        return CIV_ERR_NOENT;
    if (n == 0u || n > (size_t)ANIM_DISSOLVE_MAX_ENTRIES)
        return CIV_ERR_RANGE;
    a = (size_t)((r1 & 0x7fffu) % (unsigned)n);
    b = (size_t)((r2 & 0x7fffu) % (unsigned)n);
    tmp = perm[a];
    perm[a] = perm[b];
    perm[b] = tmp;
    return CIV_OK;
}

int anim_dissolve_col(int start_col, int perm_val, int w)
{
    /* Orig Anim_StepDissolve @ 10b0:17e5: (*local_2a + pu[(elapsed+row)%w +
     * 0x1ee]) % w (12-branch) / same in the 13-branch; row loop h..1 with
     * per-row starts pu[row+0xd]. Caller computes idx=(elapsed+row)%w and
     * reads perm[idx]; here the pure column fold. Negative folds like orig
     * C (%) on positive w after the add; w <= 0 -> -1. */
    long v;

    if (w <= 0)
        return -1;
    v = (long)start_col + (long)perm_val;
    v %= (long)w;
    if (v < 0)
        v += (long)w;
    return (int)v;
}

civ_error_t anim_rebase_clock(long now_tick, long param, long *out_base,
                              long *out_stored)
{
    /* Orig Anim_RebaseClock @ 10b0:005b: *(seq+0x1a) = Sys_GetTickScaled() -
     * param; *(seq+0x16) = param (now from Sys_GetTickScaled @ 1100:02e1 =
     * GETTICKCOUNT*0x3c/1000, see sys_get_tick_scaled in sys.c; OpenSequence
     * @ 10b0:008a inits +0x16 = 0). Callers: Main_RunStartupSequence @
     * 11e0:0000 loop#2 (1000) + MainMenu_WndProc @ 11e0:0221 (100000 idle,
     * 0x48c menu re-sync); PumpIfActive @ 10b0:0000 / CloseSequence @
     * 10b0:0287 never rebase. */
    if (!out_base)
        return CIV_ERR_NOENT;
    *out_base = now_tick - param;
    if (out_stored)
        *out_stored = param;
    return CIV_OK;
}

/* --- Chain 2026-10-16 (25): MenuZ modes + tracked IDs (goal 4). ---
 * See dialog.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()).
 * PEEKMESSAGE/ISDIALOGMESSAGE pump + CREATEWINDOW radio fan-out stay STUB
 * in dialog_menuz / dlg_create_tracked above. */

int dialog_menuz_art_mode(int opt_0006)
{
    /* Orig Dialog_MenuZ_Modal @ 1220:0156: if (DAT_1418_0006 == -1) plain
     * else if (DAT_1418_0006 < 3) small pic (0x51/0x74) else tech badge
     * (GfxPort_Create + Census_DrawTechBadge). */
    if (opt_0006 == -1)
        return 0;
    if (opt_0006 < 3)
        return 1;
    return 2;
}

int dialog_show_message_id(int flag_0008)
{
    /* Orig Dialog_ShowMessage @ 1220:00d2: if (DAT_1418_0008 == 0) uVar2 =
     * 0x88 else 0x91; then Dlg_CreateTracked + MenuZ + DestroyTracked +
     * Dialog_RefreshViews @ 1220:0eb8. */
    return (flag_0008 == 0) ? 0x88 : 0x91;
}

int dialog_menuz_should_loop(int choice)
{
    /* Orig MainMenu_WndProc @ 11e0:0221: do { ... MenuZ -> g_nMenuChoice @
     * 12d8:44e2 ...; dispatch 1/3/4/5 ... } while (choice < 0 || 3 < choice).
     * 0..3 wchodzi w Startup loop#2 (0/2/3 mapa, 1 load bez mapy w PrepareEarth
     * 'inne -> return 1'); 4/5 obsluzone w petli (HoF/quit) i re-prompt. */
    return (choice < 0 || choice > 3) ? 1 : 0;
}

int dialog_menuz_is_cancel(int flag_0008, int done_00a0)
{
    /* Orig MenuZ exit: if ((DAT_1418_0008 < 1) || (DAT_12b0_00a0 != -1))
     * DAT_1418_0004 = 0 else 1 (cancel/back jak w wizardzie). */
    if (flag_0008 < 1)
        return 0;
    return (done_00a0 == -1) ? 1 : 0;
}

int dlg_id_needs_preset_size(int dlg_id)
{
    /* Orig Dlg_CreateTracked @ 1098:124f: if ((param_1 == 0x94) ||
     * (param_1 == 0x8f)) { DAT_12d8_0026 = 1; 002e = 0x200; 002c = 0x140; }
     * (0x94 intro/PIC host, 0x8f earth-custom; 0x90 wizard / 0x8b menu /
     * 0x97 finalize / 0x88/0x91 message ida sciezka MOVEWINDOW). */
    return (dlg_id == 0x94 || dlg_id == 0x8f) ? 1 : 0;
}

/* --- Chain 2026-10-16 (26): Dialog_SplitMenuText '_' split (goal 3). ---
 * See dialog.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()).
 * Width tail (Text_MeasureMenuWidth @ 1100:00ea GETTEXTEXTENT +
 * DAT_1418_000e) stays in dialog_split_menu_text STUB below. */

int dialog_split_is_sep(int c)
{
    /* Orig Dialog_SplitMenuText @ 1220:0000: if ((c == '_') || (c == '\0'))
     * close token (*pcVar4 = '\0' over the separator in the 24fa buffer). */
    return (c == '_' || c == '\0') ? 1 : 0;
}

int dialog_split_menu_count(const char *s)
{
    /* Orig same walk: first '_'/'\0' ends the title (bVar3, no count++),
     * each later '_'/'\0' closes one item (count++, cap 0x18, '\0' returns).
     * Empty items count (no empty filter in decompile). */
    int seen_title_end = 0;
    int count = 0;
    const char *p;

    if (!s)
        return -1;
    for (p = s;; p++) {
        int c = (unsigned char)*p;

        if (c == '_' || c == '\0') {
            if (!seen_title_end) {
                seen_title_end = 1;
            } else {
                count++;
                if (count >= CIV_SPLIT_MAX)
                    return CIV_SPLIT_MAX;
            }
            if (c == '\0')
                return count;
        }
    }
}

civ_error_t dialog_split_menu_copy(const char *src, char *dst, size_t cap,
                                   const char **entries, size_t cap_entries,
                                   size_t *count_out)
{
    /* Orig same copy into the 0x12d8:0x24fa buffer with '_' -> '\0' + ptr
     * pack (*out_ptrs = entry, *out_ptrs += 4 per item, cf. FillResStringList
     * pack shape in text_res_list_advance). Title = dst[0] segment. */
    size_t len = 0;
    size_t n = 0;
    int seen_title_end = 0;
    size_t di = 0;
    size_t tok_start = 0;
    size_t si = 0;

    if (count_out)
        *count_out = 0;
    if (!src || !dst || !entries || !count_out)
        return CIV_ERR_NOENT;
    if (cap == 0 || cap_entries == 0)
        return CIV_ERR_TRUNCATED;
    while (src[len] != '\0')
        len++;
    if (len + 1 > cap)
        return CIV_ERR_TRUNCATED;
    for (si = 0; si <= len; si++) {
        int c = (unsigned char)src[si];
        int is_sep = (c == '_' || c == '\0') ? 1 : 0;

        if (di >= cap)
            return CIV_ERR_TRUNCATED;
        if (is_sep) {
            dst[di++] = '\0';
            if (!seen_title_end) {
                seen_title_end = 1;
            } else {
                if (n < cap_entries)
                    entries[n] = dst + tok_start;
                n++;
                if (n >= (size_t)CIV_SPLIT_MAX) {
                    /* Cap like orig (return 0x18 early): report capped. */
                    *count_out = (n < cap_entries) ? n : cap_entries;
                    return CIV_OK;
                }
            }
            tok_start = di;
        } else {
            dst[di++] = (char)c;
        }
    }
    *count_out = (n < cap_entries) ? n : cap_entries;
    return CIV_OK;
}

civ_error_t dialog_split_menu_text(const char *src, int *count_out,
                                   int *max_w_out)
{
    if (count_out)
        *count_out = 0;
    if (max_w_out)
        *max_w_out = 0;
    if (!src)
        return CIV_ERR_NOENT;
    /* TODO: Orig Dialog_SplitMenuText @ 1220:0000 full: '_' split into the
     * 0x12d8:0x24fa buffer (LIVE in dialog_split_menu_copy above) + ptr
     * pack + Text_MeasureMenuWidth @ 1100:00ea (SELECTOBJECT + GETTEXTEXTENT)
     * per item + *max_w default DAT_1418_000e (0x96) for non-empty title.
     * GDI measure -> SDL_ttf TBD. Caller Dialog_MenuZ_Modal @ 1220:0156. */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (27): MenuWidth measure without GDI (goal 4). ---
 * See dialog.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit());
 * SELECTOBJECT/GETTEXTEXTENT/TEXTOUT tails stay STUB. */

int dialog_menu_track_max(int cur_max, int w)
{
    /* Orig Dialog_SplitMenuText @ 1220:0000: if (*param_3 < iVar5)
     * *param_3 = iVar5 (iVar5 = Text_MeasureMenuWidth @ 1100:00ea). */
    return (w > cur_max) ? w : cur_max;
}

int dialog_menu_title_base(int title_nonempty, int cur_max)
{
    /* Orig same title half: if (first token non-empty) *param_3 =
     * DAT_1418_000e (default 0x96 via MenuZ exit reset; title NOT measured,
     * items are). */
    if (title_nonempty != 0)
        return CIV_SPLIT_DEFAULT_W;
    return cur_max;
}

int text_aligned_list_x(int left, int right, int text_w, int mode)
{
    /* Orig Text_DrawAlignedList @ 1068:258f: param_2 == 1 center
     * (left+((right-left)-w)/2), else left; param_2 == 3 right (right-w);
     * same mapping as text_align_x @ 1098:0ed1 (1/2/3). */
    if (mode == 1)
        return left + ((right - left) - text_w) / 2;
    if (mode == 2)
        return left;
    if (mode == 3)
        return right - text_w;
    return -1;
}

/* --- Chain 2026-10-16 (28): Text_DrawAlignedList layout (goal 1). ---
 * See dialog.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit());
 * SELECTOBJECT/SETBKMODE/GLOBALLOCK/TEXTOUT/GLOBALFREE tails stay STUB. */

int text_aligned_list_row_y(int top, int row_h, int i)
{
    /* Orig Text_DrawAlignedList @ 1068:258f: local_18 = local_e + iVar3 *
     * *(int *)(base + 0x10) (row_h = GETTEXTMETRICS tmHeight stored by
     * Text_BuildWrappedList @ 1068:2072 at +0x10). */
    return top + i * row_h;
}

int text_aligned_list_should_stop(int i, int count)
{
    /* Orig Text_DrawAlignedList @ 1068:258f loop head: if (*(int *)(base +
     * 0x18) <= iVar3) break (count = u16 at handle+0x18, rows from
     * Text_BuildWrappedList @ 1068:2072). */
    return (i >= count) ? 1 : 0;
}

int text_aligned_list_needs_unlock(int handle)
{
    /* Orig same tail + Free @ 1068:255a: if (param_1 != 0) GLOBALUNLOCK /
     * GLOBALFREE (handle 0 = empty list from failed GLOBALALLOC, skip). */
    return (handle != 0) ? 1 : 0;
}

int text_wrapped_list_cap(void)
{
    /* Orig Text_BuildWrappedList @ 1068:2072 handle 0x51a B: per-row len
     * u16 table at +0x1a and off u16 table at +0x9a (2 B per row each);
     * the len table runs +0x1a..+0x99 (0x80 = 128 B = 64 rows) before the
     * off table starts at +0x9a, so 64 rows max; text tail +0x11a..0x519
     * (0x400 = 1024 chars) bounds total chars, not rows. */
    return 64;
}

civ_error_t text_draw_aligned_list(int handle, int mode)
{
    (void)handle;
    (void)mode;
    /* TODO: Orig Text_DrawAlignedList @ 1068:258f GDI tail (SELECTOBJECT
     * slot DAT_12d8_0024 + SETBKMODE(1) + GLOBALLOCK handle + per-row
     * Util_MemMove into local_11c + buf[len] = 0 +
     * Text_MeasureMenuWidth @ 1100:00ea + text_aligned_list_x +
     * TEXTOUT @ 1608:01cc + GLOBALUNLOCK/SETBKMODE/SELECT restore) ->
     * SDL_ttf bridge TBD. Layout (row_y/should_stop/needs_unlock/x)
     * is LIVE above. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t text_draw_aligned_list_free(int handle, int mode)
{
    (void)handle;
    (void)mode;
    /* TODO: Orig Text_DrawAlignedListFree @ 1068:255a (Debug_LogTrace
     * "TextBoxDrawAndClose ... dialogs" + Text_DrawAlignedList(handle,
     * mode) + GLOBALFREE handle when != 0) -> SDL_ttf + heap-owner TBD.
     * Free gate is LIVE in text_aligned_list_needs_unlock above. */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (29): Text_BuildWrappedList wrap rules (goal 1). ---
 * See dialog.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()). */

int text_wrap_is_space(int c)
{
    /* Orig Text_BuildWrappedList @ 1068:2072 inner word gate (*p == ' ')
     * break + leading/trailing space loops (myslniki nie lamia). */
    return (c == ' ') ? 1 : 0;
}

int text_wrap_is_hard_break(int c)
{
    /* Orig Text_BuildWrappedList @ 1068:2072 hard break: *p == '\0' (koniec
     * tekstu, finalizuje wysokosc row_h*count + top + MemCopyFar) ||
     * *p == '\r' (koniec wiersza, konsumowany +1 gdy overflow == 0). */
    return (c == '\0' || c == '\r') ? 1 : 0;
}

int text_wrapped_char_fix(int c)
{
    /* Orig Text_BuildWrappedList @ 1068:2072 remap: if (*local_14 == -0x6b)
     * *local_14 = -5 (signed char 0x95 -> 0xFB przed GETTEXTEXTENT; exact
     * glyph-page TBD). */
    signed char sc = (signed char)c;

    if (sc == (signed char)(-0x6b))
        return -5;
    return c;
}

int text_wrapped_overflows(int width, int measured)
{
    /* Orig Text_BuildWrappedList @ 1068:2072 width gate: if (local_c <= iVar2)
     * local_20 = 1 (overflow; wiersz cofa sie do ostatniego breaka local_1c;
     * pojedyncze slowo i tak w wierszu via fallback local_1c = local_14). */
    return (width <= measured) ? 1 : 0;
}

int text_wrapped_trim_trailing(const char *s, int len)
{
    /* Orig Text_BuildWrappedList @ 1068:2072 backtrack: while ([local_1c-1]
     * == ' ' ) local_1c-- (iVar5-- petla). s to wiersz, len to surowa dlugosc
     * (end - start). */
    if (!s || len <= 0)
        return 0;
    while (len > 0 && s[len - 1] == ' ')
        len--;
    return len;
}

int text_wrapped_skip_leading(const char *s, int pos, int n)
{
    /* Orig Text_BuildWrappedList @ 1068:2072 leading skip: while (*local_14
     * == ' ' ) local_14++ (przed kazdym wierszem). Zwraca liczbe pomijanych
     * spacji (0 gdy s NULL / pos OOR / n <= pos). */
    int k = 0;

    if (!s || pos < 0 || n <= pos)
        return 0;
    while (pos + k < n && s[pos + k] == ' ')
        k++;
    return k;
}

civ_error_t text_wrapped_row_span(int base_off, int cur_off, int start_off,
                                  int end_off, int *off_out, int *len_out)
{
    /* Orig Text_BuildWrappedList @ 1068:2072 row store: *(base+0x9a+i*2) =
     * cur - (base+0x11a) (off), *(base+0x1a+i*2) = end - start (len); count
     * at +0x18++. */
    if (!off_out || !len_out)
        return CIV_ERR_NOENT;
    if (cur_off < base_off || end_off < start_off)
        return CIV_ERR_RANGE;
    *off_out = cur_off - base_off;
    *len_out = end_off - start_off;
    return CIV_OK;
}

/* --- Chain 2026-10-16 (30): Text_ShowTextByName + Quiz kernels (task goal 5) ---
 * See dialog.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()). */

int text_wrapped_total_height(int row_h, int n_rows, int y0)
{
    /* Orig Text_ShowTextByName @ 1068:1dd7 layout: *param_4 = *(+0x10) *
     * *(+0x18) + *(+2) (row_h * count + y0). */
    return y0 + row_h * n_rows;
}

int text_show_needs_long_desc(int opt_flags)
{
    /* Orig Pedia_ShowEntry @ 1068:06af gate: (g_nOptionFlags & 0x40) != 0 ->
     * Text_ShowTextByName @ 1068:1dd7 (dlugi opis cywilopedii). */
    return ((opt_flags & 0x40) != 0) ? 1 : 0;
}

int quiz_answer_is_correct(int picked, int correct)
{
    /* Orig Quiz_RunCivilizationQuiz @ 1178:0022 compare: local_e == local_10
     * (MenuZ pick vs wylosowany tech; dobrze -> 12d0:00b6 |= 1). */
    return (picked == correct) ? 1 : 0;
}

int quiz_should_retry(int first_result)
{
    /* Orig Quiz_RunQuizPair @ 1178:0000: Quiz(1) == 0 -> Quiz(0) (retry w
     * trybie 0 z FoundCityIfHome-resettle przy pudle). */
    return (first_result == 0) ? 1 : 0;
}

int quiz_tech_is_valid(int pre1, int pre2)
{
    /* Orig Quiz_RunCivilizationQuiz @ 1178:0022 pick: pre1(1894) != -1 &&
     * pre2(1895) != -1 && pre1 != 0x7f (lancuch pre, jak Tech_Choose). */
    if (pre1 == -1 || pre1 == 0x7f)
        return 0;
    if (pre2 == -1)
        return 0;
    return 1;
}

/* --- Chain 2026-10-16 (task): Quiz reward/resettle (goal 3) ---
 * See dialog.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()). */

int quiz_reward_flags(int cur)
{
    /* Orig Quiz_RunCivilizationQuiz @ 1178:0022: DAT_12d0_00b6 |= 1. */
    return cur | 1;
}

int quiz_needs_resettle(int mode, int correct)
{
    /* Orig Quiz_RunCivilizationQuiz @ 1178:0022: pudlo w trybie 0 ->
     * DAT_13b8_0000 = 1 + FoundCityIfHome-resettle. */
    if (mode != 0)
        return 0;
    return (!correct) ? 1 : 0;
}

int quiz_pair_first_mode(void)
{
    /* Orig Quiz_RunQuizPair @ 1178:0000: pierwszy Quiz(1). */
    return 1;
}
