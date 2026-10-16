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

/* AF2: HLP header (civhelp.hlp Win 3.x help, unpacked 72787 B read-only,
 * never written; packed civ1win/CIVHELP.HLP is EDILZSS2 33323 B).
 * Dump-confirmed 2026-10-15 (unpacked/civhelp.hlp, read-only):
 *   magic u32 LE at +0x00 = 0x00035F3F (bytes 3F 5F 03 00),
 *   followed by 46 00 00 00 + FF FF FF FF + topic blocks (e.g.
 *   "Civilization" at file 0x25, "Sid Meier's Civilization!" at 0x4B7).
 * OS-free header-only port (no SDL, no globals, no exit(), borrowed bytes,
 * no copy): validates magic + bounds only; topic/B-tree decode stays a
 * TODO (no fake topics). WINHELP exact Orig FUN_* TBD (call site issues
 * WINHELP on CIV_HELP_FILE_NAME via dialog_open_help below). */
#define CIV_HLP_MAGIC0 0x3Fu
#define CIV_HLP_MAGIC1 0x5Fu
#define CIV_HLP_MAGIC2 0x03u
#define CIV_HLP_MAGIC3 0x00u
#define CIV_HLP_MIN_SIZE 16u

typedef struct help_hlp_info {
    uint32_t magic; /* LE u32 at +0 (0x00035F3F on disk) */
    uint32_t size;  /* total image bytes (72787 on disk) */
} help_hlp_info_t;

/* HLP header side of dialog_open_help/help_open_topic below. OS-free 1:1
 * magic + bounds check on borrowed bytes (caller-owned, never writes
 * civhelp.hlp): NULL data/out -> NOENT; len < 16 -> TRUNCATED; magic
 * mismatch (incl. EDILZSS2-packed "EDILZSS2" input) -> BAD_DATA.
 * TODO: dump the topic/B-tree directory (offsets/counts TBD from the
 * 72787 B image; use rsc_dump-style read-only dump, no fake) + wire the
 * topic-id -> file-offset map for help_open_topic. */
civ_error_t help_hlp_parse(const uint8_t *data, size_t len,
                           help_hlp_info_t *out);

/* AG3: HLP system header + topic lookup + custom-class names (2026-10-15).
 * HLP system header (civhelp.hlp Win 3.x help, unpacked 72787 B read-only,
 * never written): magic 3F 5F 03 00 at +0x00 (LE u32 0x00035F3F, already
 * validated by help_hlp_parse above) + version LE u32 at +0x04 (0x46 on
 * disk) + flags LE u32 at +0x08 (0xFFFFFFFF on disk) + topic_count LE u32
 * at +0x10 (0x36 = 54 on disk; exact directory/topic semantics TBD vs the
 * B-tree dump) + bounds (len >= 20). Observed but NOT enforced (mapping
 * TBD, never gates success): LE u32 at +0x0C = 0x00011C53 = 72787 (== file
 * size on disk). OS-free header-only port (no SDL, no globals, no exit(),
 * borrowed bytes, no copy); topic B-tree decode stays a TODO (no fake
 * topics, help_hlp_topic below always returns -1 until wired).
 * WINHELP exact Orig FUN_* TBD. Custom-class names from
 * Ctrl_RegisterCustomClasses @ 1058:0b4d (REGISTERCLASS x4 strings @
 * 0xac6/0xad2/0xadf/0xaea + brushes/pens; register/free/paint stubs above
 * stay untouched): OS-free static const 4-entry table below. */
#define CIV_HLP_SYSTEM_MIN_SIZE 20u

typedef struct help_hlp_system {
    uint32_t magic;       /* LE u32 at +0x00 (0x00035F3F on disk) */
    uint32_t version;     /* LE u32 at +0x04 (0x46 on disk) */
    uint32_t flags;       /* LE u32 at +0x08 (0xFFFFFFFF on disk) */
    uint32_t topic_count; /* LE u32 at +0x10 (0x36 on disk; exact dir TBD) */
    uint32_t size;        /* total image bytes (72787 on disk, == len) */
} help_hlp_system_t;

/* HLP system header side (header-only). OS-free 1:1 magic + bounds check
 * on borrowed bytes (caller-owned, never writes civhelp.hlp): NULL
 * data/out -> NOENT; len < 20 -> TRUNCATED; magic mismatch (incl.
 * EDILZSS2-packed "EDILZSS2" input) -> BAD_DATA. Fills version/flags/
 * topic_count from the header words above + size = (uint32_t)len.
 * TODO: dump the topic/B-tree directory (offsets/counts TBD from the
 * 72787 B image; rsc_dump-style read-only dump, no fake) + wire the
 * topic-id -> file-offset map for help_hlp_topic below. */
civ_error_t help_hlp_system(const uint8_t *data, size_t len,
                            help_hlp_system_t *out);

/* Topic id -> file offset via the HLP topic B-tree (stub). OS-free,
 * read-only, no globals, no exit(): NULL data or len < 16 or magic
 * mismatch -> -1 (no fake, no crash); any valid-header input also returns
 * -1 until the B-tree directory is dumped and wired (exact Orig FUN_*
 * TBD; use rsc_dump-style read-only dump of the 72787 B image, never
 * fabricate offsets). */
long help_hlp_topic(const uint8_t *data, size_t len, uint32_t topic_id);

/* Custom-class name table (OS-free static const 4-entry port of the
 * REGISTERCLASS strings in Ctrl_RegisterCustomClasses @ 1058:0b4d;
 * brushes/pens + register/free/paint stubs above stay untouched).
 * Index 0..3: 0 "RanDoMRadio" (str @ 0xac6), 1 "RanDoMStatic" (@ 0xad2),
 * 2 "RanDoMUser" (@ 0xadf), 3 "Region" (@ 0xaea). */
#define CIV_CTRL_CLASS_COUNT 4

/* Returns the class name for index 0..CIV_CTRL_CLASS_COUNT-1, NULL when
 * index is out of range. No globals (function-local static const view),
 * no exit(), read-only. */
const char *ctrl_class_names(int index);

/* AH3: HLP PAGE header + B-tree find (2026-10-15).
 * HLP PAGE (Win 3.x help B-tree page inside unpacked civhelp.hlp 72787 B
 * read-only, never written; packed civ1win/CIVHELP.HLP EDILZSS2 rejected
 * by help_hlp_parse above). Layout parsed header-only (borrowed bytes, no
 * copy, no SDL, no globals, no exit()):
 *   +0x00 wMagic  u16 LE (page magic, exact value TBD from dump)
 *   +0x02 wFlags  u16 LE (leaf/internal flags, exact bits TBD)
 *   +0x04 lFirst  i32 LE (first key/free link, exact semantics TBD)
 *   +0x08 nEntries u16 LE (index-array count, offsets TBD)
 * MIN_SIZE 10 covers the fixed header; the variable index array follows
 * (entry offsets/counts TBD from an rsc_dump-style read-only dump, no
 * fake). WINHELP exact Orig FUN_* TBD. Parity: help_hlp_parse/
 * help_hlp_system/help_hlp_topic above stay untouched (doc only). */
#define CIV_HLP_PAGE_MIN_SIZE 10u

typedef struct help_hlp_page {
    uint16_t magic; /* LE u16 at page+0x00 (wMagic, value TBD) */
    uint16_t flags; /* LE u16 at page+0x02 (wFlags, bits TBD) */
    int32_t first;  /* LE i32 at page+0x04 (lFirst, semantics TBD) */
    uint16_t count; /* LE u16 at page+0x08 (nEntries, index count TBD) */
    uint32_t size;  /* remaining bytes from page_off (len - page_off) */
} help_hlp_page_t;

/* HLP PAGE header side (header-only). OS-free 1:1 LE decode on borrowed
 * bytes (caller-owned file image, never writes civhelp.hlp): NULL
 * data/out -> NOENT; page_off OOR or len-page_off < 10 -> TRUNCATED
 * (overflow-safe); else fills magic/flags/first/count + size and returns
 * CIV_OK (wMagic value NOT gated: exact set TBD from dump). TODO: confirm
 * wMagic/wFlags/lFirst semantics + index-array entry layout from the
 * 72787 B image dump, then wire the PAGE walk for help_hlp_btree_find
 * below (no fake offsets). */
civ_error_t help_hlp_page(const uint8_t *data, size_t len, uint32_t page_off,
                          help_hlp_page_t *out);

/* Topic id -> file offset via the HLP PAGE B-tree walk (stub). OS-free,
 * read-only, no globals, no exit(): NULL data or len < 16 or magic
 * mismatch (incl. EDILZSS2-packed input) -> -1 (no fake, no crash;
 * same header gate as help_hlp_topic for parity); any valid-header input
 * also returns -1 until B-tree pages are wired (exact Orig FUN_* TBD).
 * TODO: walk PAGEs via help_hlp_page above (directory offsets/counts TBD;
 * rsc_dump-style read-only dump of the 72787 B image, never fabricate;
 * unknown ids stay -1). help_hlp_topic above stays untouched. */
long help_hlp_btree_find(const uint8_t *data, size_t len, uint32_t topic_id);

/* AI-B: HLP B-tree root/next + topic list (2026-10-15).
 * HLP B-tree walk inside unpacked civhelp.hlp (72787 B read-only, never
 * written; packed civ1win/CIVHELP.HLP EDILZSS2 rejected by
 * help_hlp_parse above). OS-free ports (no SDL, no globals, no exit(),
 * borrowed bytes, no copy, read-only); exact Orig FUN_* TBD. Parity:
 * help_hlp_parse/help_hlp_system/help_hlp_topic/help_hlp_page/
 * help_hlp_btree_find above stay untouched (doc only); diplomacy
 * (Diplomacy_PostContact @ 1168:0a07, Diplomacy_EvalBribe @ 1078:455c),
 * UI_DrawEndTurnPrompt @ 10d8:00e0, Dialog_MenuZ_Modal @ 1220:0156
 * (id math + modal), Dlg_CenterWindow @ 10a0:xxxx, Help_OpenTopic @
 * 1160:xxxx below stay untouched. */
#define CIV_HLP_BTREE_ROOT_OFF 0x14u
#define CIV_HLP_BTREE_ROOT_MIN_SIZE 24u
#define CIV_HLP_BTREE_MAX_STEPS 256u

/* Orig: FUN_xxxx_xxxx @ xxxx:xxxx TBD -> Help_HlpBtreeRoot.
 * B-tree root page offset via the system-header lFirst link (LE i32 at
 * +CIV_HLP_BTREE_ROOT_OFF, candidate root; exact semantics TBD vs the
 * 72787 B directory dump, e.g. |SYSTEM at file 16, |TTLBTREE at 70692).
 * OS-free 1:1 header-field read on borrowed bytes: NULL data -> -1;
 * len < 24 -> -1; magic mismatch (incl. EDILZSS2-packed input) -> -1;
 * root link negative/OOR/short-tail (< 10 B remain) -> -1 (no fake).
 * Else returns the root page file offset (>= 20, < len). wMagic NOT
 * gated (values TBD). No globals, no exit(). */
long help_hlp_btree_root(const uint8_t *data, size_t len);

/* Orig: FUN_xxxx_xxxx @ xxxx:xxxx TBD -> Help_HlpBtreeNext.
 * Next page in the chain via the PAGE header (wMagic/wFlags/lFirst +
 * nEntries index, see help_hlp_page_t; lFirst is the next-page link,
 * exact chain semantics TBD from the 72787 B dump). OS-free link follow
 * on borrowed bytes: NULL data or len < 16 or magic mismatch -> -1;
 * help_hlp_page failure (page_off OOR/short) -> -1; lFirst negative/
 * OOR/short-tail -> -1; self-loop (next == page_off) -> -1 (loop
 * guard; multi-hop revisit guard lives in help_topic_list via its
 * visited set + CIV_HLP_BTREE_MAX_STEPS cap). Else returns the next
 * page file offset. No globals, no exit(), no fake. */
long help_hlp_btree_next(const uint8_t *data, size_t len, uint32_t page_off);

/* Orig: FUN_xxxx_xxxx @ xxxx:xxxx TBD -> Help_TopicList.
 * All topic ids via the B-tree page-chain walk (root via
 * help_hlp_btree_root + next via help_hlp_btree_next). OS-free walk
 * with cap, bounds, no fake: data NULL or out_count NULL -> NOENT;
 * len < 16 -> TRUNCATED; magic mismatch -> BAD_DATA (out_count set to
 * 0 when non-NULL); cap > 0 with out_ids NULL -> NOENT. Valid header
 * with unusable root link -> CIV_OK + *out_count = 0 (empty walk, no
 * fake). Otherwise walks up to CIV_HLP_BTREE_MAX_STEPS pages (visited
 * set breaks multi-hop loops, self-loop/OOR/short-tail ends the
 * chain), fills out_ids[0..cap) with the first N page offsets and
 * always sets *out_count to the total pages visited (summary always,
 * detail capped, no flood). NOTE: ids are currently page file offsets
 * (one per page); per-entry topic-id decode from the nEntries index
 * array stays TODO (layout TBD, never fabricated). out_ids may be NULL
 * when cap == 0 (count-only walk). Caller-owned buffers only (never
 * writes civhelp.hlp). No globals, no exit(). */
civ_error_t help_topic_list(const uint8_t *data, size_t len,
                            uint32_t *out_ids, size_t cap,
                            size_t *out_count);

/* --- AJ2: HLP topic record decode (2026-10-15).
 * HLP topic record inside unpacked civhelp.hlp (72787 B read-only, never
 * written; packed civ1win/CIVHELP.HLP EDILZSS2 rejected by help_hlp_parse
 * above). Candidate record layout parsed header-only (borrowed bytes, no
 * copy, no SDL, no globals, no exit(), read-only); exact Orig FUN_* TBD:
 *   +0x00 rec_size u32 LE (total record bytes incl. header, >= 8)
 *   +0x04 format   u16 LE (0 = raw text, != 0 = compressed/encoded)
 *   +0x06 text_off u16 LE (record-relative offset to NUL-terminated text,
 *     >= 8 and < rec_size; absolute text start = topic_off + text_off)
 * MIN_SIZE 8 covers the fixed header; the topic body (formatted text,
 * embedded codes, bitmap refs) stays TODO. WINHELP exact Orig FUN_* TBD.
 * Parity: help_hlp_parse/help_hlp_system/help_hlp_topic/help_hlp_page/
 * help_hlp_btree_find/help_hlp_btree_root/help_hlp_btree_next/
 * help_topic_list above stay untouched (doc only); diplomacy
 * (Diplomacy_PostContact @ 1168:0a07, Diplomacy_EvalBribe @ 1078:455c),
 * UI_DrawEndTurnPrompt @ 10d8:00e0, Dialog_MenuZ_Modal @ 1220:0156
 * (id math + modal), Dlg_CenterWindow @ 10a0:xxxx stay untouched. */
#define CIV_HLP_TOPIC_MIN_SIZE 8u
#define CIV_HLP_TOPIC_FORMAT_RAW 0u

typedef struct help_hlp_topic_header {
    uint32_t size;     /* LE u32 at topic_off+0x00 (record bytes, >= 8) */
    uint16_t format;   /* LE u16 at topic_off+0x04 (0 = raw, != 0 compr.) */
    uint16_t text_off; /* LE u16 at topic_off+0x06 (record-rel. to text) */
    uint32_t topic_off; /* echo of the record file offset (for callers) */
} help_hlp_topic_header_t;

/* Orig: FUN_xxxx_xxxx @ xxxx:xxxx TBD -> Help_HlpTopicHeader.
 * Topic record header side (header-only). OS-free 1:1 LE decode on
 * borrowed bytes (caller-owned file image, never writes civhelp.hlp):
 * NULL data/out -> NOENT; len < 16 -> TRUNCATED (global HLP gate);
 * magic mismatch (incl. EDILZSS2-packed input) -> BAD_DATA; topic_off
 * OOR or short tail (< 8) -> RANGE; rec_size < 8 or
 * topic_off + rec_size OOR (> len, overflow-safe) -> RANGE; text_off <
 * 8 or >= rec_size or topic_off + text_off OOR -> RANGE. Else fills
 * size/format/text_off + topic_off echo and returns CIV_OK (format
 * value NOT gated here: compressed records still decode the header;
 * the body/text stage decides UNSUPPORTED). No globals, no exit(). */
civ_error_t help_hlp_topic_header(const uint8_t *data, size_t len,
                                  uint32_t topic_off,
                                  help_hlp_topic_header_t *out);

/* Orig: FUN_xxxx_xxxx @ xxxx:xxxx TBD -> Help_HlpTopicText.
 * Topic text via the header text offset above. OS-free text extract on
 * borrowed bytes (no copy, no decompress fake, no SDL, no globals, no
 * exit()): same header gate as help_hlp_topic_header (NULL data or
 * out_text+out_len both NULL? see below; len < 16 -> TRUNCATED; magic
 * mismatch -> BAD_DATA; header RANGE propagates as RANGE); format !=
 * CIV_HLP_TOPIC_FORMAT_RAW (compressed/encoded) -> UNSUPPORTED (no
 * fake decompress; TODO: wire the decompressor + formatting-code
 * stripper); missing NUL terminator within [topic_off+text_off,
 * topic_off+size) -> RANGE. On success sets *out_text to the borrowed
 * NUL-terminated bytes (data + topic_off + text_off) and *out_len to
 * the strlen (excludes NUL; may be 0 for empty text) and returns
 * CIV_OK. NULL data -> NOENT; NULL out_text or NULL out_len -> NOENT
 * (outputs zeroed when non-NULL: *out_text = NULL, *out_len = 0 on
 * any error). Formatting codes are NOT skipped yet (raw bytes
 * returned; TODO: strip WinHelp codes/bitmap refs vs the 72787 B dump,
 * never fabricate). */
civ_error_t help_hlp_topic_text(const uint8_t *data, size_t len,
                                uint32_t topic_off,
                                const uint8_t **out_text, size_t *out_len);

/* Orig: FUN_xxxx_xxxx @ xxxx:xxxx TBD -> Help_TopicGet.
 * Topic id -> text via the full chain (btree_find + header + text).
 * OS-free dispatcher (no Win16, no HELP viewer, no globals, no exit(),
 * borrowed bytes only): NULL data or NULL out_text or NULL out_len ->
 * NOENT; len < 16 -> TRUNCATED; magic mismatch -> BAD_DATA;
 * help_hlp_btree_find miss (< 0, incl. unwired B-tree stub) -> NOENT;
 * header/text RANGE propagates as RANGE; compressed format propagates
 * as UNSUPPORTED. On success sets *out_text / *out_len exactly like
 * help_hlp_topic_text above and returns CIV_OK (borrowed view into
 * data, caller-owned buffers only, never writes civhelp.hlp). Outputs
 * zeroed (*out_text = NULL, *out_len = 0) on any error when non-NULL.
 * TODO: wire the B-tree directory (offsets/counts TBD; unknown ids
 * stay NOENT, never faked) + decompressor/formatting stripper. */
civ_error_t help_topic_get(const uint8_t *data, size_t len,
                           uint32_t topic_id,
                           const uint8_t **out_text, size_t *out_len);

/* --- AK2: HLP body decompress + full-chain get (2026-10-16).
 * HLP topic body inside unpacked civhelp.hlp (72787 B read-only, never
 * written; packed civ1win/CIVHELP.HLP EDILZSS2 rejected by help_hlp_parse
 * above). Candidate body framing (header-only, borrowed bytes, exact
 * semantics TBD from the 72787 B dump + Ghidra WINHELP call sites):
 *   +0x00 decomp_size u32 LE (decompressed byte count, > 0)
 *   +0x04 LZ code stream (codec TBD, see help_hlp_decompress below).
 * Parity: help_hlp_parse/help_hlp_system/help_hlp_topic/help_hlp_page/
 * help_hlp_btree_find/help_hlp_btree_root/help_hlp_btree_next/
 * help_topic_list/help_hlp_topic_header/help_hlp_topic_text/help_topic_get
 * above stay untouched (doc only, no break); diplomacy
 * (Diplomacy_PostContact @ 1168:0a07, Diplomacy_EvalBribe @ 1078:455c),
 * UI_DrawEndTurnPrompt @ 10d8:00e0, Dialog_MenuZ_Modal @ 1220:0156
 * (id math + modal), Dlg_CenterWindow @ 10a0:xxxx stay untouched. No SDL,
 * no globals, no exit(), C17; SDL lives only in video font_blit_glyph
 * (see video.h). */

/* Orig: FUN_xxxx_xxxx @ xxxx:xxxx TBD -> Help_HlpDecompressedSize.
 * Decompressed-size word side (header-only). OS-free LE u32 read at
 * src+0x00 on borrowed bytes (caller-owned image slice, never writes
 * civhelp.hlp): NULL src/out -> NOENT; src_len < 4 -> TRUNCATED; else
 * fills *out_size with the LE u32 and returns CIV_OK (value NOT gated:
 * 0/huge still reported; dst-capacity check lives in help_hlp_decompress
 * below). No globals, no exit(). */
civ_error_t help_hlp_decompressed_size(const uint8_t *src, size_t src_len,
                                       size_t *out_size);

/* Orig: FUN_xxxx_xxxx @ xxxx:xxxx TBD -> Help_HlpDecompress.
 * HLP 3.x topic-body LZ side. OS-free stub with explicit src/dst bounds
 * (borrowed src vs caller-owned dst, never writes civhelp.hlp; no SDL, no
 * globals, no exit(), no fake bytes, dst never touched on miss).
 * HLP 3.x LZ vs EDI note (parity read-only via Read, not edited/reused
 * blindly): EDI LZSS2 (tools/unpack_edi.py + gfx_try_lzss_edi @ 10f8:040a
 * family, see gfx_indexed.h: 4K ring prefilled 0x20, curpos 4096-18=4078,
 * flags LSB-first 1=literal 1B / 0=match 2B matchpos=((x1&0xF0)<<4)|x0
 * matchlen=(x1&0x0F)+3) is the *installer* framing (25-B EDI header +
 * unpacked_len prefix, raw code stream); HLP 3.x topic bodies use their
 * own WinHelp 3.x LZ variant (decomp_size word + code stream with
 * window/bias TBD from the 72787 B dump, not proven same as EDI), so this
 * does NOT alias gfx_try_lzss_edi output (no fake decompress; AL2 wires
 * the local duplicate help_hlp_lzss_try below instead, no gfx/SDL pull).
 * AL2 (2026-10-16): delegates body (skip decomp_size word at src+0x00,
 * stream at src+0x04) to help_hlp_lzss_try; trial hit sets *out_len and
 * returns CIV_OK (need is advisory, NOT gated), trial miss stays
 * UNSUPPORTED (no fake). NULL src/dst -> NOENT (NULL out_len = skip,
 * zeroed when present); src_len < 4 or dst_cap == 0 -> TRUNCATED;
 * otherwise UNSUPPORTED on trial miss (TODO: rsc_dump-style read-only
 * dump of a compressed topic body vs raw to confirm window/bias/
 * decomp_size semantics; unknown bodies stay UNSUPPORTED, never faked).
 * Outputs zeroed (*out_len = 0 when non-NULL) on any error. */
civ_error_t help_hlp_decompress(const uint8_t *src, size_t src_len,
                                uint8_t *dst, size_t dst_cap,
                                size_t *out_len);

/* Orig: FUN_xxxx_xxxx @ xxxx:xxxx TBD -> Help_TopicGetFull.
 * Topic id -> decompressed text via the full chain (btree_find + header +
 * text + decompress). OS-free dispatcher (no Win16, no HELP viewer, no
 * SDL, no globals, no exit(), borrowed bytes only, never writes
 * civhelp.hlp): NULL data/out_text/out_len -> NOENT; len < 16 ->
 * TRUNCATED; magic mismatch (incl. EDILZSS2-packed input) -> BAD_DATA;
 * help_hlp_btree_find miss (< 0, incl. unwired B-tree stub) -> NOENT;
 * header/text RANGE propagates as RANGE; format !=
 * CIV_HLP_TOPIC_FORMAT_RAW (compressed/encoded) -> UNSUPPORTED after an
 * AL2 (2026-10-16) help_hlp_lzss_try probe on body+0x04 with stack
 * scratch (no fake text; full decompressed-text return still needs
 * caller-owned scratch via help_hlp_decompress + formatting-code stripper
 * + viewer, TODO once the codec lands). On success (raw only) sets
 * *out_text / *out_len exactly like help_hlp_topic_text above and returns
 * CIV_OK (borrowed view into data, caller-owned buffers only). Outputs
 * zeroed (*out_text = NULL, *out_len = 0) on any error when non-NULL.
 * Unknown ids stay NOENT, never faked. */
civ_error_t help_topic_get_full(const uint8_t *data, size_t len,
                                uint32_t topic_id,
                                const uint8_t **out_text, size_t *out_len);

/* --- AL2: HLP LZSS-EDI trial (2026-10-16).
 * HLP topic body via LZSS-EDI 4K ring 0x20 gfx_try_lzss_edi parity
 * read-only (tools/unpack_edi.py + gfx_try_lzss_edi @ 10f8:040a family,
 * see gfx_indexed.h, read via Read, not edited/reused blindly): flags
 * LSB-first 1=literal 1B / 0=match 2B matchpos=((x1&0xF0)<<4)|x0
 * matchlen=(x1&0x0F)+3; 4K ring prefilled 0x20, curpos 4096-18=4078,
 * overlap-safe forward copy; raw code-stream framing (no 25-B EDI header
 * / unpacked_len prefix, no HLP decomp_size word here — framing lives in
 * help_hlp_decompress above/below). OS-free port duplicated here to avoid
 * a gfx pull (gfx_indexed.h brings SDL; dialog HLP stays SDL-free): same
 * 4K ring/bias/shape, stack ring only, no cross-module globals. No SDL,
 * no globals, no exit(), C17; borrowed src vs caller-owned dst,
 * read-only (never writes civhelp.hlp).
 * HLP 3.x LZ vs EDI note (see help_hlp_decompress): EDI is installer
 * framing, HLP 3.x topic-body variant (decomp_size word + window/bias TBD
 * from the 72787 B dump) is not proven same, so this trial is best-effort
 * (miss stays -1, never faked); help_hlp_decompress wires it with framing,
 * help_topic_get_full probes it for compressed records. */
int help_hlp_lzss_try(const uint8_t *src, size_t src_len,
                      uint8_t *dst, size_t dst_cap);

/* WINHELP(CIV_HELP_FILE_NAME) bridge (stub; topic NULL = contents page).
 * Win16 WINHELP -> SDL3 help viewer TBD. */
civ_error_t dialog_open_help(const char *topic);

/* Orig: FUN_1160_xxxx @ 1160:xxxx -> Help_OpenTopic. WINHELP
 * (CIV_HELP_FILE_NAME) topic opener; thin wrapper wiring dialog_open_help
 * above. Topic-id contract (AF2 doc, no impl change): topic NULL = contents
 * page; non-NULL = NUL-terminated topic/context id selecting the HLP topic
 * record decoded via help_hlp_parse above (exact id string set TBD from
 * the WINHELP call sites + 72787 B topic directory dump; unknown ids stay
 * UNSUPPORTED via dialog_open_help, never faked). Win16 WINHELP -> SDL3
 * help viewer TBD; stub CIV_ERR_UNSUPPORTED. No globals, no exit(),
 * read-only: never writes civhelp.hlp. */
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

/* --- Chain 2026-10-16 (5): diplomacy state bits + tribute + tech exchange
 * (task goal 3; segment 1168 = dyplomacja) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   Diplo_PostContact @ 1168:0a07 (tail of Unit_UpdateVisibility @ 11c0:0395:
 *     player-involved contact opens the FUN_1160_0000 dialog unless peace bit
 *     0x01... wait: bit 1 contact? exact: bit 0x01 == contacted gate with the
 *     392b+3 turn stamp (> 0xf turns since), then both directions |= 0x11
 *     (contact+seen), AI-AI pairs on (civA+civB+turn)&3 == 0 or first contact
 *     without 0x08 evaluate Diplo_ShouldMakePeace @ 1168:0f47 (+ wonder-7 /
 *     count gates) -> Diplo_ExchangeTechs @ 1168:1220 + Diplo_SetPeace @
 *     1168:0dda (peace, replay log 3) else Diplo_SetWar @ 1168:0e8e (war,
 *     replay log 2); embassy-gated (0x40) messages via Dialog_ShowMessage @
 *     1220:00d2; tribute path lives in Diplo_ProcessTurn @ 1168:0000 below.
 *     Needs diplo bits @ 12d8:3a0a + Win16 dialogs -> diplomacy_post_contact
 *     stays STUB; bit helpers below are LIVE 1:1.)
 *   Diplo_SetPeace @ 1168:0dda (was FUN_1168_0dda: replay log 3 when setting
 *     0x02 fresh, both dirs |= param_3, 0x02 clears 0x20).
 *   Diplo_SetWar @ 1168:0e8e (was FUN_1168_0e8e: replay log 2 when clearing
 *     a set 0x02, both dirs &= ~param_3, 0x02 clears 0x04 too).
 *   Diplo_ShouldMakePeace @ 1168:0f47 (was FUN_1168_0f47: AI peace evaluator:
 *     3e9c&1 == 0 -> 1 (peace forced); rank/diplo gates vs player + military
 *     13b4 + ally counts + tech-trade sums via 1366/138d tables; 1 = peace).
 *   Diplo_ExchangeTechs @ 1168:1220 (was FUN_1168_1220: per tech 0..0x48 both
 *     exclusive -> swap via 1170:2aaa; 1 when a pair swapped).
 *   Diplo_NearestPalaceDist @ 1168:12c1 (was FUN_1168_12c1: min
 *     Map_WrapStepDist @ 11c8:02ec over palace cities (54fc&1, owner ==
 *     civ), seed 0x10; feeds Diplomat_CityAction revolt + Diplomat_BribeUnit
 *     cost rules in game.h).
 *   Diplo_ProcessTurn @ 1168:0000 (was FUN_1168_0000: per-civ diplomacy turn:
 *     government upkeep via Diplo_ChangeGovernment @ 1168:06c1, spy/embassy
 *     decay (0x100 + Rng_Range(3)), war-end messages every 0x10 turns,
 *     tribute demands at (civ<<2 == turn&0x1f) via 1160:20c0/328e dialogs
 *     with gold tribute math below).
 *   Diplo_ChangeGovernment @ 1168:06c1 (was FUN_1168_06c1: government change:
 *     AI sets directly + replay log 8, human opens the dialog list with
 *     wonder/tech gates via City_IsWonderOpen @ 1030:7e57 + Tech_HasTech @
 *     1170:3181; clears 0x08 treaty bits on change).
 *   Spy neighbours (segment 1170 = tech/city AI; espionage entry
 *     Diplomat_CityAction case 1 investigate @ 11b8:0000 calls FUN_1170_05af
 *     = City_OpenProductionView probe path, NOT a spy op; real spy ops are
 *     the Diplomat_CityAction cases 2/3 steal/sabotage via Rng_Range @
 *     11c8:003d + city-record masks, see game.h diplomat chain).
 * OS-free bit/tribute/exchange math below is LIVE 1:1 on explicit params
 * (no globals, no Win16, read-only); dialog/replay/city-state tails stay
 * CIV_ERR_UNSUPPORTED with TODO Orig refs. Diplo bit map @ 12d8:3a0a
 * (u16 per ordered civ pair, stride 0x10): 0x01 contacted, 0x02 peace,
 * 0x04 government-changed?, 0x08 treaty?, 0x10 seen?, 0x11 contact pair set
 * by PostContact, 0x20 war/contact-fight, 0x40 embassy, 0x88 ScoreAdjacent
 * contact/hostile, 0x100 spy?, 0x200 bribe-refused? (exact upper bits TBD). */

/* Orig Diplo_PostContact @ 1168:0a07 / Diplo_SetPeace @ 1168:0dda contact
 * pair -> diplo_contact_flags. 1:1 cur | 0x11 (both-direction contact+seen
 * set). No globals. */
int diplo_contact_flags(int cur);

/* Orig peace/war bit tests @ 1168:0a07/0dda/0e8e/0f47 -> predicates. 1:1:
 * peace == (flags & 0x02) != 0, contact == (flags & 0x01) != 0, embassy ==
 * (flags & 0x40) != 0. No globals. */
int diplo_is_peace(int flags);
int diplo_is_contact(int flags);
int diplo_has_embassy(int flags);

/* Orig Diplo_SetPeace @ 1168:0dda apply -> diplo_apply_peace. 1:1 cur |
 * 0x02 then & ~0x20 (peace set clears the 0x20 fight bit). No globals. */
int diplo_apply_peace(int cur);

/* Orig Diplo_SetWar @ 1168:0e8e apply -> diplo_apply_war. 1:1 cur & ~0x02
 * then & ~0x04 (war clears peace + the 0x04 treaty bit). No globals. */
int diplo_apply_war(int cur);

/* Orig Diplo_ProcessTurn @ 1168:0000 tribute math -> diplo_tribute_amount.
 * 1:1 iVar5 = gold % 100 (remainder kept by the demander?) + iVar2 =
 * Util_Clamp(gold / 100, 1, 10) * 0x32 (the demanded amount; see dialog
 * 1160:328e accept path adding iVar2 to both treasuries). This helper
 * returns the demanded part (clamp(gold/100,1,10)*0x32); -1 on gold < 0.
 * Util_Clamp @ 11c8:0063 gate is inlined (no sys pull). No globals. */
int diplo_tribute_amount(int gold);

/* Orig Diplo_ExchangeTechs @ 1168:1220 pair gate -> diplo_should_exchange.
 * 1:1: nonzero (1) when sahiptirA-exclusive and sahiptirB-exclusive both
 * exist (orig swaps one pair each way via 1170:2aaa when uVar3 == 3, i.e.
 * A-owns-B-lacks some tech AND B-owns-A-lacks some (possibly other) tech
 * among 0..0x48 via Tech_HasTech @ 1170:3181). Takes the two exclusivity
 * flags explicitly (caller scans via tech_has_tech in game.h). No globals. */
int diplo_should_exchange(int a_exclusive, int b_exclusive);

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

/* --- Text + dialog-rect chain (2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Text_DrawAligned @ 1098:0ed1 (centered TEXTOUT; callers HoF_Show @
 *     1080:0119 + Anim_AdvanceFrames @ 10b0:0461 + Text_DrawResString @
 *     1098:20f4; callees Text_SetColor @ 1098:0e0b + GETTEXTMETRICS/
 *     GETTEXTEXTENT + SETBKMODE/SETTEXTALIGN + TEXTOUT + SELECTOBJECT).
 *   Text_DrawResString @ 1098:20f4 (ResMgr_GetString + Util_PascalToCStr @
 *     1100:0053 + Text_DrawAligned; callers 1068:06af + WorldGen_LoadEarth
 *     @ 1108:0243).
 *   Text_SetColor @ 1098:0e0b (Gfx_LookupColor @ 1098:01b0 + SETTEXTCOLOR +
 *     CREATEPEN + Gfx_MakeBrush @ 1098:00b5).
 *   Dialog_DlgUnitsToPixels @ 1098:0d43 (GETDIALOGBASEUNITS + x*4/baseX +
 *     y*8/baseY; callee of Dialog_GetItemRect @ 1100:03ff).
 *   Dialog_GetItemRect @ 1100:03ff (GETDLGITEM + GETWINDOWRECT +
 *     SCREENTOCLIENT x2 + optional Dialog_DlgUnitsToPixels + MOVEWINDOW;
 *     12 callers incl. Dialog_MenuZ_Modal @ 1220:0156 + Dlg_CreateTracked
 *     @ 1098:124f + NewGame_Wizard @ 10e8:0000).
 *   Dialog_ShowMessage @ 1220:00d2 (0x88 vs 0x91 via DAT_1418_0008 +
 *     Dlg_CreateTracked + Dialog_MenuZ_Modal + Dlg_DestroyTracked +
 *     Dialog_RefreshViews @ 1220:0eb8; 40+ callers).
 *   Text_GetFontHeight @ 1098:0dda (GETTEXTMETRICS tmHeight; alias of
 *     status_font_height in game.h, same Orig).
 * OS-free rect/text math below is LIVE 1:1 on explicit params (no Win16,
 * no globals, read-only); GDI handle tails stay STUB with TODO Orig. */

/* Orig: Text_DrawAligned @ 1098:0ed1 horizontal math — alignment slot.
 * format low nibble: 1 = center, 2 = left, 3 = right (other -> -1).
 * Returns the text x origin: left + (w - text_w) / 2 (center), left
 * (left), right - text_w (right). No globals, no exit(). */
int text_align_x(int left, int right, int text_w, int format);

/* Orig: Text_DrawAligned @ 1098:0ed1 vertical math — centered line.
 * Returns top + (h - text_h) / 2. No globals, no exit(). */
int text_center_y(int top, int bottom, int text_h);

/* Orig: Dialog_DlgUnitsToPixels @ 1098:0d43 — dialog-unit scale.
 * x*4/base_x, y*8/base_y per axis (GETDIALOGBASEUNITS low = base_x,
 * high = base_y in orig). base_x/base_y <= 0 -> RANGE. No Win16. */
civ_error_t dialog_dlgunits_to_pixels(const dlg_rect_t *in, int base_x,
                                      int base_y, dlg_rect_t *out);

/* Orig: Dialog_GetItemRect @ 1100:03ff client-offset math — screen rect
 * minus the dialog origin (SCREENTOCLIENT x2 in orig). Pure subtract:
 * out = (scr_l - org_x, scr_t - org_y, scr_r - org_x, scr_b - org_y).
 * NULL out -> NOENT. No Win16 handles. */
civ_error_t dialog_client_rect(int scr_l, int scr_t, int scr_r, int scr_b,
                               int org_x, int org_y, dlg_rect_t *out);

/* Orig: Dlg_CenterRect @ 11f8:0000 — center inner rect inside outer,
 * keeping inner size (same math as dlg_center_window, but CIV_OK on
 * success so tests can assert it). NULL -> NOENT. No Win16.
 * Callers (7 xrefs, all rect-center placements): FUN_1068_06af @
 * 1068:0958/0b36 (city dialog layout x2), FUN_1098_0b33 @ 1098:0b67,
 * FUN_1098_0a0d @ 1098:0a5b, FUN_1098_0aa4 @ 1098:0ae0 (dialog create
 * paths), FUN_1170_21b3 @ 1170:2623 (tech/wonder dialog), FUN_1210_09b4
 * @ 1210:0e29 (small-map window). */
civ_error_t dlg_center_rect(const dlg_rect_t *outer, const dlg_rect_t *inner,
                            dlg_rect_t *out);

/* Orig: Text_DrawAligned @ 1098:0ed1 + Text_DrawResString @ 1098:20f4 —
 * GDI text-out tail (SELECTOBJECT font + SETBKMODE/SETTEXTALIGN +
 * TEXTOUT + shadow via Text_SetColor). SDL_ttf bridge TBD.
 * Text_SetColor @ 1098:0e0b callers (who sets text colors, 50+ xrefs):
 * Game_UpdateStatusPanel @ 1088:1554 (DAT_12d8_45be before the pop
 * line), Text_DrawAligned @ 1098:103f/10c3 (aligned-text shadow pass),
 * Anim_AdvanceFrames @ 10b0:137a/13e9 (opcode 7 caption + opcode 8
 * fill), NewGame_Wizard @ 10e8:0667/0771/0b0b/0bf6 (wizard screens),
 * Replay_PlayLog @ 1180:02d3 head (mono branch), Dialog_MenuZ_Modal @
 * 1220:0889/10c6/118e (menu items), HoF_VictoryCeremony @ 1080:08e9,
 * FUN_1110_04d0 city-draw @ 1110:07c3..0c61 x6, FUN_1170_05ca/21b3,
 * FUN_1178_0022, FUN_1188_0000/013f/07ad end-game screens. Full
 * Gfx_LookupColor @ 1098:01b0 + SETTEXTCOLOR + CREATEPEN + Gfx_MakeBrush
 * @ 1098:00b5 tail stays Win16 (see video.h brush/color chain for the
 * palette link + the LIVE gfx_brush_is_solid/gfx_color_table_kind
 * predicates). */
civ_error_t text_draw_aligned(const char *text, const dlg_rect_t *rect,
                              int format, int color);

/* Orig: Dialog_GetItemRect @ 1100:03ff — handle tail (GETDLGITEM +
 * GETWINDOWRECT + SCREENTOCLIENT + MOVEWINDOW). Math part lives in
 * dialog_client_rect/dialog_dlgunits_to_pixels above (LIVE). */
civ_error_t dialog_get_item_rect(int dlg_id, int item_id, dlg_rect_t *out);

/* Orig: Dialog_ShowMessage @ 1220:00d2 — modal message (0x88 vs 0x91
 * via DAT_1418_0008 + Dlg_CreateTracked + Dialog_MenuZ_Modal +
 * Dlg_DestroyTracked + Dialog_RefreshViews @ 1220:0eb8). SDL modal TBD. */
civ_error_t dialog_show_message(const char *text, int *out_choice);

/* --- Dialog string lists (2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Text_DrawDlgItemString @ 1098:2796 (Dialog_GetItemRect @ 1100:03ff
 *     into local_10 + source choice + Text_SetDlgItemPascal @ 1100:0947:
 *     param_5 == 0 ? (param_6 != 0 ? Util_StrCopy @ 1000:36b2 +
 *     FUN_1100_0000 : empty local_110) : ResMgr_GetString @ 1100:078a into
 *     local_110; sole caller FUN_10f0_0066 @ 10f0:0066).
 *   Text_SetDlgItemPascal @ 1100:0947 (Util_PascalToCStr @ 1100:0053 on
 *     the local_110 Pascal buffer + SETWINDOWTEXT on the item handle;
 *     sole caller Text_DrawDlgItemString above).
 *   Text_FillResStringList @ 1098:1f4f (ResMgr_GetString @ 1100:078a per
 *     id + Util_PascalToCStr @ 1100:0053 + LSTRLEN empty check + pointer
 *     pack: *out_ptrs = entry, *out_ptrs += 4, cursor += len+1, count++;
 *     returns NULL on the first empty string, else the tail cursor; sole
 *     caller Dialog_InitResStringTables @ 1068:0295).
 *   Dialog_InitResStringTables @ 1068:0295 (mem_alloc_lock 0x7fff +
 *     Text_FillResStringList x5 for Res 0x82 tech 1..0x43 / 0x83 build
 *     2..999 / 0x84 produce 1..999 / 0x85 1..0xc / 0x86 1..999 into
 *     DAT_1288_* tables + FUN_1098_1909 dialog 0x82 show; no callers
 *     found = init-time once).
 *   Main_ShowResString @ 1008:0add (ResMgr_GetString @ 1100:078a into
 *     local_1e + Main_EnqueueResString @ 1008:0a8d queue; no callers
 *     found = message-queue producer for the 12b0:004c ring).
 *   Main_EnqueueResString @ 1008:0a8d (if DAT_12b0_004c < 10:
 *     queue[DAT_12d8_0024-ish @ 1420:4473+0x71+count*2] = id, count++).
 * Who fills dialog lists: Dialog_InitResStringTables (wizard/menu/city
 * tables 0x82..0x86 at init) + Text_DrawDlgItemString (per-item text at
 * draw time, incl. wizard screens via NewGame_Wizard @ 10e8:0000 and
 * MainMenu via 11e0:0221 through Dialog_MenuZ_Modal @ 1220:0156).
 * OS-free cursor/queue/source math below is LIVE 1:1 on explicit params
 * (no Win16, no globals, read-only except outs); ResMgr string fetch +
 * GDI handles stay STUB with TODO Orig. */

/* Res list ids filled by Dialog_InitResStringTables @ 1068:0295. */
#define CIV_RES_TECH_LIST 0x82
#define CIV_RES_BUILD_LIST 0x83
#define CIV_RES_PRODUCE_LIST 0x84
#define CIV_RES_GOV_LIST 0x85
#define CIV_RES_UNIT_LIST 0x86
/* Main_EnqueueResString @ 1008:0a8d ring cap (DAT_12b0_004c < 10). */
#define CIV_RES_QUEUE_MAX 10

/* Orig Text_FillResStringList @ 1098:1f4f cursor half ->
 * text_res_list_advance. 1:1 pointer-pack step (*out_ptrs += 4,
 * cursor += entry_len+1 for the NUL): next_off = cur_off + entry_len + 1.
 * Returns CIV_OK, NOENT on NULL next_off, RANGE on overflow. The
 * ResMgr_GetString @ 1100:078a + Util_PascalToCStr @ 1100:0053 fetch +
 * LSTRLEN empty-stop stays in the text_fill_res_string_list STUB below
 * (needs live ResMgr blobs). No globals, no exit(). */
civ_error_t text_res_list_advance(size_t cur_off, size_t entry_len,
                                  size_t *next_off);

/* Orig Main_EnqueueResString @ 1008:0a8d -> main_enqueue_res_string.
 * 1:1 ring push (cap CIV_RES_QUEUE_MAX): when *count < 10,
 * queue[*count] = id and (*count)++. Returns 1 when queued, 0 when the
 * ring is full (orig silently drops), -1 on NULL queue/count. No Win16,
 * no globals, no exit(). */
int main_enqueue_res_string(int *queue, size_t *count, int id);

/* --- Res-string tables + queue (2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Dialog_InitResStringTables @ 1068:0295 (mem_alloc_lock 0x7fff; switch
 *     on param_1: 0 fills all five lists below in order, 1..5 fills one;
 *     each via Text_FillResStringList @ 1098:1f4f (Res 0x82 tech 1..0x43
 *     / 0x83 build 2..999 / 0x84 produce 1..999 / 0x85 gov 1..0x0c /
 *     0x86 unit 1..999) into the DAT_1288_* tables; then FUN_1098_1909
 *     dialog 0x82 show. No callers found in the NE xref scan = init-time
 *     once; caller TBD (candidate WinMain @ 1008:0000 family /
 *     Game_InitGlobalTables @ 11e8:0337 string-table step, NOT proven —
 *     never claimed as boot/wizard until the call site lands)).
 *   Main_EnqueueResString @ 1008:0a8d (if DAT_12b0_004c < 10:
 *     queue[DAT_1420_4473+0x71+count*2] = id, count++; ring cap 10,
 *     full ring silently drops).
 *   Main_ShowResString @ 1008:0add (ResMgr_GetString @ 1100:078a into
 *     local_1e + Main_EnqueueResString queue; no callers found =
 *     message-queue producer for the 12b0:004c ring).
 *   Main_PumpMessage @ 1098:02d1 (PEEKMESSAGE pump: WM_CHAR 0x102 ->
 *     Main_EnqueueResString direct; WM_KEYDOWN 0x100 via Main_KeyToResChar
 *     @ 1208:0000 (0x21..0x28 key map) -> enqueue; else
 *     TRANSLATE/ DISPATCHMESSAGE; TRANSLATEACCELERATOR + shutdown tail
 *     via WinMain_Shutdown @ 1008:0329; sole caller FUN_11e8_02fb).
 * Who consumes the queue: TBD (no NE xrefs to the 12b0:004c count or
 * the 1420:4473 queue slots beyond the two producers; NOT claimed as
 * Text_DrawDlgItemString until the read site lands). Reset path is
 * sys_drain_input @ 11f0:4ea0 (clears 12b0:004c alongside 1420:1d44).
 * OS-free spec/queue math below is LIVE 1:1 on explicit params (no
 * Win16, no globals, read-only except outs); ResMgr fetch + heap +
 * dialog handles stay STUB with TODO Orig. */

/* Orig Dialog_InitResStringTables @ 1068:0295 switch half ->
 * dialog_res_list_spec. 1:1 table select: 1 -> (0x82,1,0x43 tech),
 * 2 -> (0x83,2,999 build), 3 -> (0x84,1,999 produce), 4 -> (0x85,1,0x0c
 * gov), 5 -> (0x86,1,999 unit). NOENT on any NULL out, RANGE on index
 * outside 1..5, else CIV_OK. No globals, no exit(). */
civ_error_t dialog_res_list_spec(int index, int *res_id, int *first,
                                 int *last);

/* Orig Main_EnqueueResString @ 1008:0a8d gate half -> main_res_queue_full.
 * 1:1 of the DAT_12b0_004c < 10 gate (CIV_RES_QUEUE_MAX): nonzero (1)
 * when count >= 10 (orig silently drops), 0 otherwise. */
int main_res_queue_full(size_t count);

/* Orig Dialog_InitResStringTables @ 1068:0295 -> dialog_init_res_string_tables.
 * Table fill driver (spec via dialog_res_list_spec above + pack via
 * text_fill_res_string_list). Needs mem_alloc_lock 0x7fff heap + live
 * ResMgr blobs (see rsc_loader.h res_get_string_copy STUB) + the
 * FUN_1098_1909 dialog-show tail -> STUB. index 0 = all five tables. */
civ_error_t dialog_init_res_string_tables(int index);

/* Orig Main_ShowResString @ 1008:0add -> main_show_res_string.
 * Per-string producer (ResMgr_GetString @ 1100:078a fetch + enqueue via
 * main_enqueue_res_string above). Needs live ResMgr blobs -> STUB. */
civ_error_t main_show_res_string(int id);

/* Orig Text_DrawDlgItemString @ 1098:2796 source choice ->
 * text_dlg_string_source. 1:1 dispatch on (res_id, fallback): res_id !=
 * 0 -> 2 (ResMgr_GetString @ 1100:078a path); res_id == 0 && fallback !=
 * 0 -> 1 (Util_StrCopy + FUN_1100_0000 path); else 0 (empty local_110,
 * still via Text_SetDlgItemPascal @ 1100:0947). Returns 0/1/2. No
 * globals. */
int text_dlg_string_source(int res_id, long fallback);

/* Orig Text_SetDlgItemPascal @ 1100:0947 handle tail ->
 * text_set_dlg_item_text. Pascal->C half lives in util_pascal_to_c @
 * 1100:0053 (LIVE); SETWINDOWTEXT handle tail -> SDL TBD (STUB). */
civ_error_t text_set_dlg_item_text(int dlg_id, int item_id, const char *text);

/* Orig Text_DrawDlgItemString @ 1098:2796 -> text_draw_dlg_item_string.
 * Item-rect (Dialog_GetItemRect @ 1100:03ff, LIVE math in
 * dialog_client_rect/dialog_dlgunits_to_pixels) + source choice (above)
 * + Text_SetDlgItemPascal @ 1100:0947. GDI handle tail -> STUB. */
civ_error_t text_draw_dlg_item_string(int dlg_id, int item_id, int res_id,
                                      long fallback);

/* Orig Text_FillResStringList @ 1098:1f4f -> text_fill_res_string_list.
 * Pack loop (ResMgr_GetString per id + PascalToC + LSTRLEN empty-stop +
 * cursor pack via text_res_list_advance above). Needs live ResMgr blobs
 * (see rsc_loader.h res_get_string_copy STUB) -> STUB. */
civ_error_t text_fill_res_string_list(int res_id, int first, int last);

/* --- Chain 2026-10-16 (6): diplo turn tick + government rules (task goals 1/2) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Diplo_ProcessTurn @ 1168:0000 (caller Game_TurnLoopTick @ 1088:0000, per-civ
 *     pass after the alive-mask loop: government upkeep via Diplo_ChangeGovernment
 *     @ 1168:06c1, spy/embassy decay, war-end sweep every 0x10 turns, tribute
 *     demands at (civ<<2 == turn&0x1f) via Diplo_OpenContactDialog @ 1160:0000
 *     + dialogs @ 1160:328e/207a/11d0:01f7; callees Rng_Range @ 11c8:003d,
 *     Util_Clamp @ 11c8:0063, Util_IntToDec @ 1000:2928, Util_StrCopy/Append @
 *     1000:36b2/363e, City_IsWonderOpen @ 1030:7e57, Dialog_ShowMessage @
 *     1220:00d2, Diplo_SetPeace/SetWar @ 1168:0dda/0e8e).
 *   Diplo_ProcessTurn is the per-turn caller of Diplo_PostContact @ 1168:0a07
 *     logic: PostContact runs on contact events (Unit_UpdateVisibility @
 *     11c0:0395 tail: player dialog unless peace bit 0x01 with the 392b+3 stamp,
 *     both dirs |= 0x11, AI-AI on (a+b+turn)&3==0 or first contact without 0x08
 *     via Diplo_ShouldMakePeace @ 1168:0f47 -> Diplo_ExchangeTechs @ 1168:1220 /
 *     SetPeace/SetWar + Dialog_ShowMessage); ProcessTurn runs once per civ per
 *     turn (government + decay + tribute), PostContact runs per contact event.
 *   Diplo_ChangeGovernment @ 1168:06c1 (government change: AI sets directly +
 *     replay log 8, human opens the dialog list with wonder/tech gates via
 *     City_IsWonderOpen @ 1030:7e57 + Tech_HasTech @ 1170:3181; stores
 *     4 - gov/2 at 39f3+7 (corruption/production factor); clears 0x08 treaty
 *     bits on change; gov 0 = anarchy/no-government path).
 *   Diplo_GetTechValue @ 1168:1366 (was FUN_1168_1366: word at 3c97+5+civ*0x20+
 *     tech*2; tech value table read in ShouldMakePeace + OpenContactDialog).
 *   Diplo_GetTechCost @ 1168:138d (was FUN_1168_138d: word at 3d88+0x14+civ*0x20+
 *     tech*2; tech cost table sibling of the value above).
 *   Diplo_GetMilitaryScore @ 1168:13b4 (was FUN_1168_13b4: word at 4155+0xb+
 *     civ*0x38; military score vs ShouldMakePeace ally/power gates).
 *   Diplo_OpenContactDialog @ 1160:0000 (was FUN_1160_0000: player-involved
 *     contact dialog, caller Diplo_PostContact player branches; Win16 dialog ->
 *     STUB, tribute math lives in diplo_tribute_amount).
 * OS-free tick/factor/gate math below is LIVE 1:1 on explicit params (no
 * globals, no Win16, read-only); dialog/replay/city-state tails stay
 * CIV_ERR_UNSUPPORTED with TODO Orig refs. Diplo bit map @ 12d8:3a0a (u16 per
 * ordered civ pair, stride 0x10): 0x01 contacted, 0x02 peace, 0x04 treaty?,
 * 0x08 treaty-changed?, 0x10 seen, 0x20 war-fight, 0x40 embassy, 0x80 hostile,
 * 0x100 spy?, 0x200 bribe-refused? (upper bits TBD). */

/* Orig Diplo_ProcessTurn @ 1168:0000 government gate -> diplo_government_tick.
 * 1:1 (g_nTurn & 3) == 0 (every 4th turn government upkeep via
 * Diplo_ChangeGovernment @ 1168:06c1). Returns 1 when due, 0 otherwise. */
int diplo_government_tick(int turn);

/* Orig Diplo_ProcessTurn @ 1168:0000 spy/embassy decay -> diplo_spy_decay_step.
 * 1:1: when (cur & 0x100) != 0 and rng3 == 0 (Rng_Range @ 11c8:003d draw 3 hit):
 * next = (cur & ~0x100 & ~0x02) | 0x200 (drop spy, drop peace, mark refused).
 * Else returns cur unchanged. rng3 is the Rng_Range(3) draw (0..2). */
int diplo_spy_decay_step(int cur, int rng3);

/* Orig Diplo_ProcessTurn @ 1168:0000 war-end sweep gate -> diplo_war_end_tick.
 * 1:1 (turn & 0xf) == 0 (every 16th turn clears contact/seen bits). */
int diplo_war_end_tick(int turn);

/* Orig Diplo_ProcessTurn @ 1168:0000 war-end clear -> diplo_war_end_clear.
 * 1:1 cur & ~0x01 & ~0x10 (drops the contacted 0x01 bit both dirs + the seen
 * 0x10 bit both dirs in the 0xf sweep). No globals. */
int diplo_war_end_clear(int cur);

/* Orig Diplo_ProcessTurn @ 1168:0000 tribute slot -> diplo_tribute_turn.
 * 1:1 civ != 0 && (civ << 2) == (turn & 0x1f) && civ is not the player
 * (player check stays caller-side: g_nPlayerCiv != civ). Returns 1 when this
 * civ demands tribute this turn, 0 otherwise. */
int diplo_tribute_turn(int civ, int turn);

/* Orig Diplo_ChangeGovernment @ 1168:06c1 factor store -> diplo_government_factor.
 * 1:1 4 - gov / 2 (stored at 39f3+7 per civ; corruption/production factor:
 * anarchy 0 -> 4, despotism 1 -> 4, monarchy 2 -> 3, republic 3 -> 3,
 * democracy 4 -> 2, communism 5 -> 2). Returns factor, -1 on gov < 0. */
int diplo_government_factor(int gov);

/* Orig Diplo_ChangeGovernment @ 1168:06c1 tech gate -> diplo_government_needs_tech.
 * 1:1 required tech id per government (Tech_HasTech @ 1170:3181 gate;
 * City_IsWonderOpen @ 1030:7e57 wonder 1 bypasses all and stays caller-side):
 * 2 -> 5, 3 -> 0x2b, 4 -> 0x2c, 5 -> 4, else -1 (gov 0 anarchy / 1 always
 * available, no tech). Returns tech id or -1. */
int diplo_government_needs_tech(int gov);

/* Orig Diplo_ChangeGovernment @ 1168:06c1 availability -> diplo_government_available.
 * 1:1 nonzero (1) when gov is selectable: gov 0/1 always; else wonder_open
 * (City_IsWonderOpen param_1,1) bypasses, else has_req_tech != 0. Returns
 * 1 available, 0 locked. */
int diplo_government_available(int gov, int has_req_tech, int wonder_open);

/* Orig Diplo_ShouldMakePeace @ 1168:0f47 head -> diplo_peace_forced.
 * 1:1 (DAT_12d8_3e9c & 1) == 0 forces peace (returns 1); else 0 (full
 * rank/diplo/military/tech-trade evaluation via Diplo_GetTechValue/Cost @
 * 1168:1366/138d + Diplo_GetMilitaryScore @ 1168:13b4 stays caller-side). */
int diplo_peace_forced(int opt_flag);

/* --- Chain 2026-10-16 (7): contact dialog + tech/military tables (goals 1/2) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   Diplo_OpenContactDialog @ 1160:0000 (player-involved contact dialog;
 *     caller Diplo_PostContact @ 1168:0a07 player branches + tribute slot in
 *     Diplo_ProcessTurn @ 1168:0000 at (civ<<2 == turn&0x1f) via
 *     diplo_tribute_turn; gate Diplo_PromptContact @ 1160:20c0 below).
 *   Diplo_PromptContact @ 1160:20c0 (was FUN_1160_20c0: emissary Yes/No via
 *     Dialog_ShowMessage @ 1220:00d2 on 24fa/12d8 ["An emissary from the "+
 *     name+" wishes to speak...Will you receive him? Yes/No" + embassy
 *     " Check Intelligence" tail when 0x40] + leader portrait dialog 0x8e via
 *     Dlg_CreateTracked @ 1098:124f + FUN_1160_2a38/233c; returns
 *     DAT_1358_0008: -1 refused, else contact mode; -2 = Check Intelligence
 *     via Census_ShowTechBars @ 1018:03f0 family, see game.h).
 *   Diplo_RunContactMenu @ 1160:328e (was FUN_1160_328e: MenuZ modal on
 *     1420:31b4/31b6 via Dialog_MenuZ_Modal @ 1220:0156 + present/offset
 *     rects + Art_PortDataToBitmap; returns the picked button index).
 * Dialog options inside OpenContactDialog (Win16 MenuZ/buttons -> STUB;
 * option strings observed in decompile, exact button ids TBD):
 *   EXCHANGE (tech swap loop: Res 0x82 tech names via Res_GetStringCopy +
 *     FUN_1170_2aaa grants both ways, FUN_1038_09bc logs), TAKECIV (demand
 *   tech via 1170:2aaa), TRIBUTE (demand gold iVar6, treasuries at
 *   43b8+0x8c per civ), REJECT/PROVOKE (refuse -> Diplo_SetWar @ 1168:0e8e),
 *   PEACE (offer peace -> FUN_1160_257a/25fa + stamp turn+0x10),
 *   GIVE/GROVEL/MOBILIZE (AI gifts when losing), ATTITUDE/THREAT1/2/THREAT
 *   (demand menu: GIVEMORE gold or MERCENARY bribe-ally war), CANCEL (leave).
 * Tribute formula chain (LIVE below, 1:1 on explicit params):
 *   base iVar6 from the tech-value/cost scan (1..0xe) + wonder/military
 *   gates, then difficulty scale + affordability snap + mercenary/threat
 *   branches. Acceptance thresholds (caller-side, Win16): AI pays when
 *   Diplo_RunContactMenu returns 1; war when refused with local_10 set;
 *   peace stamp g_nTurn+0x10 on accept; senate override (< 4 gov) forces
 *   peace message. Dialog/replay/city-state tails stay STUB.
 * Tech/military tables (runtime BSS in seg 12d8, zeros at load — NOT ROM,
 * so no static const dump; accessors below are the LIVE port):
 *   Diplo_GetTechValue @ 1168:1366: ES=0x12d8 word at [BX+0x3c9c],
 *     BX = civ*0x20 + tech*2 (civ stride 16 words, tech 0..0x47).
 *   Diplo_GetTechCost @ 1168:138d: ES=0x12d8 word at [BX+0x3d9c], same BX.
 *   Diplo_GetMilitaryScore @ 1168:13b4: ES=0x12d8 word at [BX+0x4160],
 *     BX = civ*0x38 (28-word rows; byte +0xb is an UNALIGNED LE word).
 *   (Ghidra prints 1420:3c97+5 / 1420:3d88+14 / 1420:4155+0xb labels: same
 *   offsets 0x3c9c/0x3d9c/0x4160, real segment is ES=0x12d8 per disasm
 *   MOV AX,0x12d8 / MOV ES,AX, not DGROUP 1420 strings.)
 * Readers: Diplo_OpenContactDialog @ 1160:0000 (tribute scan 1..0xe +
 *   military gates) + Diplo_ShouldMakePeace @ 1168:0f47 (military + tech
 *   1..0xe trade sums); writers TBD (init-time fill, NOT proven — never
 *   claimed). Other tech readers (NOT these tables): Tech_HasTech @
 *   1170:3181 (bitmask at 3b9b+1, see game.h tech_has_tech LIVE) +
 *   tech_names (Res 0x82 strings) + City_ChooseProduction @ 1170:05ca
 *   (gates via Tech_HasTech + continent, not these word tables).
 * OS-free math below is LIVE 1:1 on explicit params (no globals, no Win16,
 * read-only except outs); dialog/replay tails stay STUB with TODO Orig. */

#define CIV_DIPLO_TECH_CIV_STRIDE 16
#define CIV_DIPLO_TECH_COUNT 0x48
#define CIV_DIPLO_MIL_ROW_BYTES 0x38
#define CIV_DIPLO_MIL_SCORE_OFF 0x0b

/* Orig Diplo_GetTechValue @ 1168:1366 index -> diplo_tech_table_off.
 * 1:1 word offset civ*16+tech (bytes civ*0x20+tech*2 past 12d8:3c9c).
 * Returns offset >= 0, -1 on civ < 0 / tech OOR [0,0x48). No globals. */
int diplo_tech_table_off(int civ, int tech);

/* Orig Diplo_GetTechValue @ 1168:1366 read -> diplo_tech_value_at.
 * 1:1 table[off] via diplo_tech_table_off above on the caller-owned base
 * (runtime BSS at 12d8:3c9c, zeros at load; caller passes the live row
 * image, never a static dump). Returns value, -1 on NULL/nwords<=0/OOR. */
int diplo_tech_value_at(const short *table, int nwords, int civ, int tech);

/* Orig Diplo_GetTechCost @ 1168:138d read -> diplo_tech_cost_at.
 * Same layout past 12d8:3d9c. Same contract as diplo_tech_value_at. */
int diplo_tech_cost_at(const short *table, int nwords, int civ, int tech);

/* Orig Diplo_GetMilitaryScore @ 1168:13b4 read -> diplo_military_at.
 * 1:1 UNALIGNED LE u16 at row[0x0b] (row is the 0x38-byte civ row past
 * 12d8:4160; odd offset spans bytes 0xb/0xc, x86 allows it). Returns
 * value 0..65535, -1 on NULL/row_len < 0x0d. No globals. */
int diplo_military_at(const uint8_t *row, int row_len);

/* Orig Diplo_OpenContactDialog @ 1160:0000 difficulty scale ->
 * diplo_contact_tribute_scaled. 1:1 iVar6 = clamp((iVar6*(diff+1))/0x20,
 * 0,0x14)*0x32 (diff = g_nDifficulty). No globals. */
int diplo_contact_tribute_scaled(int base, int difficulty);

/* Orig same affordability snap -> diplo_contact_afford_snap. 1:1 when
 * gold < demand <= gold*2 and gold > 0x31: demand = (gold/0x32)*0x32,
 * else demand unchanged. Returns snapped demand (-1 on gold < 0). */
int diplo_contact_afford_snap(int demand, int gold);

/* Orig MERCENARY branch -> diplo_mercenary_amount. 1:1 iVar6 =
 * (rank_target/2)*(gold_foe/100); clamp(iVar6/0x32,2,9999)*0x32.
 * Returns amount, -1 on rank/gold < 0. No globals. */
int diplo_mercenary_amount(int rank_target, int gold_foe);

/* Orig THREAT2/GIVEMORE branch -> diplo_threat_gold. 1:1
 * clamp(local16,0,gold_foe/0x32)*0x32 (0 = no gold to squeeze).
 * Returns gold demand (multiple of 0x32), -1 on inputs < 0. */
int diplo_threat_gold(int score16, int gold_foe);

/* Orig peace-stamp accept -> diplo_peace_stamp. 1:1 g_nTurn+0x10 stored
 * at 392b+3 on PEACE accept (FUN_1160_257a/25fa pair). No globals. */
int diplo_peace_stamp(int turn);

/* Orig Diplo_OpenContactDialog @ 1160:0000 -> diplo_open_contact_dialog.
 * Full contact dialog (tribute/tech/peace/war branches via
 * Diplo_PromptContact + Diplo_RunContactMenu + SetPeace/SetWar +
 * FUN_1170_2aaa grants + replay logs). Win16 MenuZ/dialog -> SDL modal
 * TBD; stub CIV_ERR_UNSUPPORTED. No globals, no exit(). */
civ_error_t diplo_open_contact_dialog(int civ, int x, int y, int mode);

/* Orig Diplo_PromptContact @ 1160:20c0 -> diplo_prompt_contact.
 * Emissary Yes/No + leader portrait dialog 0x8e. Win16 -> SDL TBD. */
civ_error_t diplo_prompt_contact(int civ, int x, int y);

/* Orig Diplo_RunContactMenu @ 1160:328e -> diplo_run_contact_menu.
 * MenuZ modal on 1420:31b4/31b6. Win16 -> SDL TBD. */
civ_error_t diplo_run_contact_menu(int *out_choice);

/* --- Chain 2026-10-16 (10): Dialog_DrawFrame3D @ 1220:0d38 geometry ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 * CREATEPEN 0x8080 dark + 0xe0e0 light + SELECTOBJECT + MemCopyFar rect +
 * right/bottom -1 + INFLATERECT(3,3) + 3x { dark top/left L (MOVETO bl +
 * LINETO tl + LINETO tr), light bottom/right L (MOVETO bl + LINETO br +
 * LINETO tr... wait order in decompile: dark MOVETO(local_6,local_c) →
 * LINETO(local_a,local_c) → LINETO(local_a,local_8); light
 * MOVETO(local_6,local_c) → LINETO(local_6,local_8) →
 * LINETO(local_a,local_8); then INFLATERECT(-1,-1) } + SELECT restore +
 * DELETEOBJECTs. Callers: Census_DrawTechBadge @ 1018:24c5 (badge 0x60x0x35
 * box) + Dialog_MenuZ_Modal @ 1220:0156 (menu frame). Other frame
 * painters (NOT this): Status_DrawPanelFrame @ 11f8:0278 (FILLRECT +
 * white/black 0xffff/0 pens + MOVETO/LINETO border via upper/lower rects
 * 1218:0956/0a06, see game.h) + dialog 0x94 fill in City_OpenCensusView @
 * 1018:0000. GDI pen/line tails stay STUB below; inset/color math is LIVE. */

/* Orig Dialog_DrawFrame3D @ 1220:0d38 layers -> dialog_frame3d_layers.
 * 1:1 loop iVar4 = 0; iVar4 < 3 (three INFLATERECT(-1,-1) layers). */
int dialog_frame3d_layers(void);
/* Orig same pens -> dialog_frame3d_dark/light. 1:1 CREATEPEN colors
 * 0x8080 (dark top/left) / 0xe0e0 (light bottom/right). */
int dialog_frame3d_dark(void);
int dialog_frame3d_light(void);
/* Orig same per-layer rect -> dialog_frame3d_layer. 1:1 out = rc inset by
 * layer (left+layer, top+layer, right-layer, bottom-layer; layer 0 is the
 * post-INFLATERECT(3,3)/-1 base passed by the caller, each further layer
 * is INFLATERECT(-1,-1)). RANGE on layer < 0 || > 2 or inverted rect;
 * NOENT on NULL. No globals. */
civ_error_t dialog_frame3d_layer(const dlg_rect_t *rc, int layer,
                                 dlg_rect_t *out);
/* Orig same GDI tail -> dialog_draw_frame3d. CREATEPEN/SELECTOBJECT +
 * MOVETO/LINETO per layer + DELETEOBJECT -> SDL_RenderDrawLines TBD. */
civ_error_t dialog_draw_frame3d(const dlg_rect_t *rc);

/* --- Chain 2026-10-16 (11): Text_Draw* @ 10d8 + font dispatch @ 11e8:0b40 ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   Text_DrawShadowed @ 10d8:02df (cień + TEXTOUT x2: GfxPort_SelectForCoords
 *     @ 1220:0f2f gate + Text_DispatchFontHandler @ 11e8:0b40 on
 *     *(DAT_1420_2990+0x10) + SELECTOBJECT font + GETTEXTMETRICS +
 *     Util_StrCopy + SETBKMODE(1) + Text_SetColor @ 1098:0e0b shadow/main +
 *     TEXTOUT shadow at (x+dx,y+1) then main at (x,y); dx = 0 when tmHeight
 *     < 0x12 else 1; mono DAT_12d8_28fc skips the shadow pass).
 *   Text_DrawAt @ 10d8:01fa (cień 0: thin wrapper Text_DrawShadowed(...,0)).
 *   Text_DrawClipped @ 10d8:01b0 (SelectForCoords + font + Text_DrawDirect @
 *     10d8:00e0).
 *   Text_DrawDirect @ 10d8:00e0 (MOVETO + emit: Text_SetColor + SELECTOBJECT
 *     + MOVETO + Util_StrCopy + Text_PackPascal @ 1100:0000 +
 *     Text_DrawPascalAtOrigin @ 1100:017b).
 *   Text_DrawCentered @ 10d8:0528 (pomiar Text_MeasureWidth @ 10d8:05b8 +
 *     x-w/2 + Text_DrawClipped).
 *   Text_DrawHeaderCentered @ 10d8:046e (tytuł 0xa0,2: wrapper
 *     Text_DrawShadowedCentered @ 10d8:0495(...,0)).
 *   Text_DrawShadowedCentered @ 10d8:0495 (was FUN_10d8_0495: DispatchFont +
 *     MeasureWidth + x-w/2 + DrawShadowed).
 *   Text_MeasureWidth @ 10d8:05b8 (was FUN_10d8_05b8: Util_StrCopy +
 *     Text_PackPascal + Text_MeasurePascal @ 1100:00a1 + (w*10)/0x10).
 *   Text_PackPascal @ 1100:0000 (was FUN_1100_0000: C->Pascal pack: len =
 *     Util_StrLen, shift right by 1, buf[0] = len).
 *   Text_MeasurePascal @ 1100:00a1 (was FUN_1100_00a1: GETTEXTEXTENT on the
 *     Pascal buffer: len = buf[0], chars = buf+1).
 *   Text_DrawPascalAtOrigin @ 1100:017b (was FUN_1100_017b: SETBKMODE(1) +
 *     SETTEXTALIGN(1) + Util_PascalToCStr + TEXTOUT at 0,0 + restore).
 *   Text_SelectFont @ 1100:0a33 (was FUN_1100_0a33: thin wrapper
 *     Text_DispatchFontHandler(id)).
 * Kto woła które (callers this pass):
 *   DrawAt: Census_ShowCityProduction @ 1018:0be0, Demo_RankRow @ 1188:220a,
 *     Demo_ShowDemographics @ 1188:123a, Hof_CalcFinalScore @ 1040:1137 x4,
 *     Unit_DrawStatusBadge @ 1110:0ff6 + FUN_1018_1221/17db/1158_0000.
 *   DrawClipped: 21 wołających — census (ShowCityProduction/Status @
 *     1018:0be0/208f, TechBars @ 1018:03f0, UnitList @ 1018:067b,
 *     TerrainCounts @ 1018:09cc, CensusReport @ 1018:1d5b), Demo_RankRow/
 *     ShowDemographics, HoF tech labels in Hof_CalcFinalScore, Pic_PlayShared
 *     @ 1030:0000, Replay_ShowEntry @ 1180:0e78 + Text_DrawCentered.
 *   DrawCentered: City_DrawCensusHeader @ 1018:015d, City_ShowInfoDialog @
 *     1030:8b39, City_ShowTopFive @ 1080:099b, Demo_ShowDemographics,
 *     HoF_VictoryCeremony @ 1080:06ff, NewGame_Wizard @ 10e8:0000,
 *     Pic_PlayShared + FUN_1030_8c11/1070_0327/1138_0000/1178_0022/1188_0b3e.
 *   HeaderCentered: Census_ShowTechBars, City_DrawCensusHeader,
 *     City_ShowTopFive, Replay_ShowCivDestroyed @ 1180:17e9,
 *     Replay_ShowConqueror @ 1180:152f + FUN_1158_0000/1188_07ad/11b0_0000.
 * Związek z Text_DrawAligned @ 1098:0ed1 (text_align_x/text_center_y LIVE +
 *   text_draw_aligned STUB: ten sam cień Text_SetColor @ 1098:0e0b x3 TEXTOUT
 *   + SETBKMODE/SETTEXTALIGN; 10d8 to dialog-census/HoF warstwa, 1098 to
 *   generyczny modal/Anim @ 10b0:0461/HoF_Show @ 1080:0119) i Text_SetColor
 *   @ 1098:0e0b / Text_SetBkColor @ 1098:2144 (Gfx_LookupColor +
 *   SETTEXTCOLOR/CREATEPEN/Gfx_MakeBrush vs SETBKCOLOR; mono DAT_12d8_28fc
 *   gate w obu).
 * Font dispatch @ 11e8:0b40 (jump-table po *(DAT_1420_2990+0x10), 25 wpisów
 *   @ 11e8:0d1d: id 1..8,200..215,333=0x014d; handler ustawia slot
 *   DAT_12d8_0024 + SI = wysokość LOGFONT, potem wspólny ogon 11e8:0cb0:
 *   1098:0186/0194 + DAT_1420_9960/+0x12/+0x28 + 1098:0194(0x45cb); aliasy
 *   2->1, 3->0x11, 7->slot 0x10, 207/213->slot 0x10; zob. labele
 *   Text_FontHandler_* w Ghidrze). Handlery to ROZMIARY/STYLE (slot =
 *   rodzina: 0xf h12 nagłówek, 0x13 h30 największy, 0x12 h24, 0x11 h18,
 *   0x10 h14 body, 0..0xd h14 serie 200.., 0x14 h36 tytuł 333), nie
 *   bold/italic (te w dfItalic/dfWeight FNT, zob. boot.h font_fnt_parse).
 *   Związek z civfonts.fon (Font_EnumFamiliesInit @ 1008:0e99 STUB:
 *   ENUMFONTFAMILIES + ADDFONTRESOURCE; granica formatu FON: nagłówek NE +
 *   FONTDIR/RT_FONT x21 LIVE via font_fon_parse/face_count, raster FNT
 *   PARTIAL via font_fnt_bits/render_glyph, TTF bridge STUB) i
 *   Text_GetFontHeight @ 1098:0dda (GETTEXTMETRICS tmHeight; FNT-strona to
 *   font_text_height = dfPixHeight, zob. boot.h).
 * Kto wybiera font (callerzy dispatch, 27): Text_* wrappery (Shadowed,
 *   Clipped, Centered, ShadowedCentered, SelectFont) + City_ChooseProduction
 *   @ 1170:05ca, City_DrawTileView @ 1110:04d0, Diplo_PromptContact @
 *   1160:20c0, Diplo_RunContactMenu @ 1160:328e, Game_UpdateStatusPanel @
 *   1088:13d5, HoF_Show @ 1080:0119, MainMenu_WndProc @ 11e0:0221,
 *   Map_DrawCityLabels @ 1208:1f64, NewGame_Wizard @ 10e8:0000,
 *   Unit_ShowInfoPopup @ 11f0:468c, WorldGen_LoadEarth @ 1108:0243 +
 *   FUN_1030_8d33/1060_01c1/1068_06af/1068_1f7e/1100_0a33/1138_0000/
 *   1170_21b3/1178_0022/11c0_0ec2/11d0_0010/11d8_0000/11d8_087f.
 * OS-free pomiary/pozycje poniżej są LIVE 1:1 (no Win16, no globals);
 * GDI ogony (SELECTOBJECT/TEXTOUT/MOVETO/SETBKMODE) zostają STUB. */

/* Orig Text_DrawShadowed @ 10d8:02df cień dx -> text_shadow_offset. 1:1
 * tmHeight < 0x12 ? 0 : 1 (decompiler: local_126 < 0x12 ? x : x+1).
 * height < 0 -> 0. No globals. */
int text_shadow_offset(int height);

/* Orig Text_DrawCentered @ 10d8:0528 + Text_DrawShadowedCentered @ 10d8:0495
 * pomiar+x-w/2 -> text_centered_x. 1:1 x - w/2 (C trunc toward zero).
 * No globals. */
int text_centered_x(int x, int w);

/* Orig Text_MeasureWidth @ 10d8:05b8 skala -> text_measure_scaled. 1:1
 * (extent*10)/0x10 (decompiler: (iVar2*10)/0x10 po Text_MeasurePascal).
 * extent < 0 -> 0. No globals. */
int text_measure_scaled(int extent);

/* Orig Text_PackPascal @ 1100:0000 połowa długości -> text_pascal_len.
 * 1:1 strlen cap 255 (buf[0] = len, len = Util_StrLen). NULL -> -1. */
int text_pascal_len(const char *s);

/* Orig Text_DispatchFontHandler @ 11e8:0b40 tabela @ 11e8:0d1d ->
 * text_font_slot_for_id / text_font_height_for_id. 1:1 id -> slot
 * DAT_12d8_0024 / SI wysokość (aliasy 2->1, 7->0x10, 207/213->0x10).
 * Nieznany id -> -1. No globals. */
int text_font_slot_for_id(int id);
int text_font_height_for_id(int id);

/* Orig Text_* @ 10d8 GDI ogony -> STUBs (TEXTOUT/MOVETO/SELECTOBJECT +
 * SETBKMODE + Text_SetColor @ 1098:0e0b -> SDL_ttf TBD). NULL text -> NOENT. */
civ_error_t text_draw_shadowed(const char *text, int x, int y, int color,
                               int shadow);
civ_error_t text_draw_at(const char *text, int x, int y, int color);
civ_error_t text_draw_clipped(const char *text, int x, int y, int color);
civ_error_t text_draw_direct(const char *text, int x, int y, int color);
civ_error_t text_draw_centered(const char *text, int x, int y, int color);
civ_error_t text_draw_header_centered(const char *text, int x, int y,
                                      int color);
civ_error_t text_select_font(int id);

/* --- Chain 2026-10-16 (12): font table dump + Pascal pack (goals 1/4) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Text_DispatchFontHandler @ 11e8:0b40 (25-entry id table @ 11e8:0d1d:
 *     1..8, 200..215 (0xc8..0xd7), 333 (0x014d); jump offsets at +0x32;
 *     per-id handler sets DAT_12d8_0024 slot + SI LOGFONT height, then the
 *     shared tail @ 11e8:0cb0: Text_FontPreApply @ 1098:0186 (noop when
 *     DAT_1420_b384 == 0) + DAT_1420_9960/+0x12 + DAT_1420_17cc/+0x28 +
 *     Text_FontPostApply @ 1098:0194(0x45cb); Ghidra labels
 *     Text_FontHandler_Slot*_H* mark the 20 distinct handler bodies;
 *     aliases 2->Slot0F, 3->Slot11, 7->Slot10, 207->Slot10, 213->Slot10
 *     via jmp chains @ 0b80/0b82/0bb7/0c32/0c7b; heights h12/h30/h24/h18/
 *     h14/h36). Callers (27): Text_SelectFont @ 1100:0a33 (thin wrapper) +
 *     Text_DrawShadowed @ 10d8:02df / DrawClipped @ 10d8:01b0 / DrawCentered
 *     @ 10d8:0528 / ShadowedCentered @ 10d8:0495 + City_ChooseProduction @
 *     1170:05ca, City_DrawTileView @ 1110:04d0, Diplo_PromptContact @
 *     1160:20c0, Diplo_RunContactMenu @ 1160:328e, Game_UpdateStatusPanel @
 *     1088:13d5, HoF_Show @ 1080:0119, MainMenu_WndProc @ 11e0:0221,
 *     Map_DrawCityLabels @ 1208:1f64, NewGame_Wizard @ 10e8:0000,
 *     Unit_ShowInfoPopup @ 11f0:468c, WorldGen_LoadEarth @ 1108:0243 +
 *     FUN_1030_8d33/1060_01c1/1068_06af/1068_1f7e/1138_0000/1170_21b3/
 *     1178_0022/11c0_0ec2/11d0_0010/11d8_0000/11d8_087f. Readers besides
 *     the dispatch: Text_SelectFont wrapper + Text_GetFontHeight @
 *     1098:0dda (GETTEXTMETRICS tmHeight; FNT side is font_text_height =
 *     dfPixHeight, see boot.h) + status_font_height (same Orig, game.h).
 *   Text_PackPascal @ 1100:0000 (C->Pascal: len = Util_StrLen, shift
 *     right, buf[0] = len; callers Text_DrawDirect @ 10d8:00e0,
 *     Text_MeasureWidth @ 10d8:05b8, Text_DrawDlgItemString @ 1098:2796,
 *     Text_FitPascalEllipsis @ 1098:0000, FUN_10d8_002d).
 *   Text_MeasurePascal @ 1100:00a1 (GETTEXTEXTENT on Pascal buf: len =
 *     buf[0], chars = buf+1; callers Text_MeasureWidth + FitEllipsis).
 *   Text_DrawPascalAtOrigin @ 1100:017b (SETBKMODE(1) + SETTEXTALIGN(1) +
 *     Util_PascalToCStr @ 1100:0053 + TEXTOUT at 0,0 + restore; callers
 *     Text_DrawDirect + FUN_10d8_002d).
 *   Text_MeasureWidth @ 10d8:05b8 (Util_StrCopy + PackPascal +
 *     MeasurePascal + (w*10)/0x10 via text_measure_scaled LIVE; callers
 *     Text_DrawCentered @ 10d8:0528 + ShadowedCentered @ 10d8:0495 +
 *     Demo_RankRow @ 1188:220a + FUN_1138_0000 + HoF_Show).
 *   Res_AppendPascal @ 11f8:014a (Pascal append: dst[0] += src[0] +
 *     payload copy; see rsc_loader.h res_append_pascal LIVE) +
 *     Res_CopyPascal @ 11f8:00fc (Pascal copy, res_copy_pascal LIVE) +
 *     Res_ComparePascal @ 11f8:0207 (memcmp on min(len) then len diff,
 *     res_compare_pascal LIVE): the Pascal-buffer family shares the
 *     buf[0]-is-len framing with Text_PackPascal (pack) /
 *     Util_PascalToCStr (unpack, util_pascal_to_c LIVE).
 * OS-free table/pack math below is LIVE 1:1 (no Win16, no globals,
 * read-only except the pack dst); GETTEXTEXTENT/TEXTOUT tails stay STUB. */

/* Orig table @ 11e8:0d1d full dump (25 entries, id -> slot + SI height).
 * Static const, read-only, no globals mutated, no exit(). */
typedef struct text_font_entry {
    int id;   /* Orig id word (1..8, 200..215, 333) */
    int slot; /* DAT_12d8_0024 LOGFONT slot */
    int h;    /* SI LOGFONT height (0x0c/0x1e/0x18/0x12/0x0e/0x24) */
} text_font_entry_t;

/* Returns the borrowed table view (25 entries) + count via out (may be
 * NULL for the view only). Never NULL (function-local static const). */
const text_font_entry_t *text_font_table(int *count_out);

/* Orig Text_DispatchFontHandler @ 11e8:0b40 combined half ->
 * text_dispatch_font. 1:1 id -> (slot, h) via the table above (aliases
 * resolved in the table: 2->0x0f, 3->0x11, 7->0x10, 207->0x10,
 * 213->0x10). NULL outs -> NOENT; unknown id -> RANGE (outs untouched).
 * Else CIV_OK. No Win16 (LOGFONT install via Text_FontPre/PostApply +
 * SELECTOBJECT stays in text_select_font STUB). No globals. */
civ_error_t text_dispatch_font(int id, int *slot_out, int *h_out);

/* Orig Text_PackPascal @ 1100:0000 -> text_pack_pascal. 1:1 C->Pascal
 * pack in place: len = strlen(buf) (cap 255, Util_StrLen shape), shift
 * the N chars right by one, buf[0] = (char)len (NUL dropped, same byte
 * count N+1 as the C form). NULL buf -> NOENT; cap == 0 -> TRUNCATED;
 * strlen >= cap (no NUL in cap) -> TRUNCATED; len > 255 -> RANGE
 * (buf untouched on any error). No globals, no exit(). */
civ_error_t text_pack_pascal(char *buf, size_t cap);

/* Orig Pascal framing (PackPascal @ 1100:0000 / MeasurePascal @ 1100:00a1
 * len = buf[0] half) -> text_pascal_payload. 1:1 validates the Pascal
 * buffer: pay_len = (uint8_t)buf[0], needs 1 + pay_len <= cap. NULL
 * buf/out -> NOENT; cap == 0 -> TRUNCATED; short tail -> TRUNCATED
 * (out zeroed when non-NULL on error). Else CIV_OK + *pay_len. This is
 * the OS-free half of MeasurePascal (GETTEXTEXTENT width itself needs
 * the DC and stays STUB); same framing as Res_Copy/AppendPascal in
 * rsc_loader.h. No globals. */
civ_error_t text_pascal_payload(const char *buf, size_t cap,
                                size_t *pay_len_out);

#endif /* CIVLIN_DIALOG_H */
