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
 * success so tests can assert it). NULL -> NOENT. No Win16. */
civ_error_t dlg_center_rect(const dlg_rect_t *outer, const dlg_rect_t *inner,
                            dlg_rect_t *out);

/* Orig: Text_DrawAligned @ 1098:0ed1 + Text_DrawResString @ 1098:20f4 —
 * GDI text-out tail (SELECTOBJECT font + SETBKMODE/SETTEXTALIGN +
 * TEXTOUT + shadow via Text_SetColor). SDL_ttf bridge TBD. */
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

#endif /* CIVLIN_DIALOG_H */
