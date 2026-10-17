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

/* Modal choice dialog (Dialog_MenuZ_Modal @ 1220:0156). PARTIAL dispatcher:
 * OS-free gates LIVE 1:1 (count via dialog_split_menu_count @ 1220:0000,
 * preselect via dialog_menuz_preselect_ok, id math via
 * dialog_menuz_id_to_index/index_to_id/id_is_valid, disable via
 * dialog_menuz_item_disabled, timer 100 via dialog_menuz_timer_ms,
 * cancel -1 via dialog_menuz_cancel_code); the PEEKMESSAGE/ISDIALOGMESSAGE
 * modal pump stays CIV_ERR_UNSUPPORTED (never fake CIV_OK).
 * items_text is the '_' buffer (title + items, e.g. text @ 1420:435b;
 * leading '_' = empty title); title is the SDL display title; preselect is
 * DAT_1418_0002 (-1 = none, < -1 = RANGE). *out_index preset to the cancel
 * code (-1) on entry; NOENT on NULL, RANGE on empty/OOR. */
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
/* Orig: FUN_1058_0e7f @ 1058:0e7f -> Ctrl_RadioGroupSetSel count/index halves.
 * 1:1 OS-free: count = last - first + 1 (slots in the BM_SETCHECK fan-out),
 * index = selected - first (zero-based slot for SendDlgItemMessage). count
 * <= 0 on empty/inverted range; index OOR when < 0 or >= count (caller maps
 * to RANGE like dialog_ctrl_radio_set above). No Win16, no globals. */
int dialog_ctrl_radio_count(int first_id, int last_id);
int dialog_ctrl_radio_index(int first_id, int selected_id);
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

/* Tracked dialog layer (PARTIAL dispatcher + LIVE math; dialog ctx stack TBD).
 *
 * Orig: FUN_1098_124f @ 1098:124f -> Dlg_CreateTracked. CREATEDIALOG +
 *   MAKEPROCINSTANCE(DLGPROC @ 1098:16e2 / CIVDIALOGPROC @ 1098:1838) +
 *   palette copy (GETDC/SELECTPALETTE/REALIZEPALETTE, 0xec entries via
 *   Palette_CopyRange @ 1098:201d) + centering (Dlg_CenterRect @ 11f8:0000
 *   shape + MOVEWINDOW variants FUN_1098_0a0d @ 1098:0a5b / FUN_1098_0aa4 @
 *   1098:0ae0 / FUN_1098_0b33 @ 1098:0b67 below) + init ctl 0x65-0x6e
 *   (GETDLGITEM + Dialog_GetItemRect @ 1100:03ff + SENDMESSAGE 0x30; 0x8f
 *   adds 0x323-0x332) + Gfx_StoreDC @ 1098:1104 (Gfx_SetCurrentDC @
 *   1100:09a9) + push HWND/ctx onto the tracked stack @ 1420:1ea0, count @
 *   1420:1ec8 (top cached @ 1420:9948). 30+ callers (callers/xrefs verified
 *   2026-10-16: 31 create sites incl. Main_RunStartupSequence @ 11e0:0000
 *   Dlg_CreateTracked(0x94,0x41), City_OpenCensusView @ 1018:0000,
 *   Pic_PlayShared @ 1030:0000, HoF_Show @ 1080:0119, MainMenu_WndProc @
 *   11e0:0221, NewGame_Wizard @ 10e8:0000, Dialog_ShowMessage @ 1220:00d2).
 *   Win16 USER/GDI -> SDL3 window TBD; PARTIAL dispatcher (OS-free
 *   flag/preset/ctl/stack gates wired, modal tail CIV_ERR_UNSUPPORTED).
 *   Dialog 0x94 lifecycle (Main_RunStartupSequence @ 11e0:0000, decompiled
 *   this pass): Dlg_CreateTracked(0x94,0x41) + Gfx_StoreDC + Palette_
 *   AnimateRange @ 1090:0376 + Anim_OpenSequence @ 10b0:008a + Sound_PlayId
 *   @ 1150:00c9 (3,0) + FILLRECT + Cursor_Set @ 1098:096c + loop
 *   Anim_AdvanceFrames @ 10b0:0461 + Anim_CloseSequence @ 10b0:0287 +
 *   Sound (1,0) + [choice 0/2/3: drain + open 0x91e seq + rebase 1000 +
 *   WorldGen_PrepareEarth @ 1108:01bc + loop#2 + close + Sound (1,0) +
 *   Dlg_DestroyTracked + NewGame_Wizard @ 10e8:0000; else Destroy only].
 * Orig: FUN_1098_1612 @ 1098:1612 -> Dlg_DestroyTracked. DESTROYWINDOW +
 *   FREEPROCINSTANCE + GLOBALFREE of the ctx + pop from 1420:1ea0/1ec8
 *   (top restored to count-2 slot, 0 when empty; 1420:17aa = 0,
 *   1420:9960 cleared when it is the dying ctx) + Mem_DisposePtr @
 *   1100:060b + refresh via FUN_1208_14d5. 37 destroy sites (mirrors the
 *   create list + Ship_CloseView @ 1070:02b5 / Replay_CloseView @
 *   1180:0297 tails). Void NOOP with a LIVE pop-count probe (handle
 *   teardown TODO in dialog.c).
 * Orig: FUN_1098_1104 @ 1098:1104 -> Gfx_StoreDC. Thin wrapper over
 *   Gfx_SetCurrentDC @ 1100:09a9 (save-DCs before draw; restore via
 *   GfxPort_PresentRect @ 10d0:0854 + ClearActive @ 10d0:09a2). 28+ callers
 *   incl. the Anim frame-pump every 8. tick (tick & 7) == 0 + MemCopyFar @
 *   1000:137f, opcodes 5/7/8/9 direct, and Dlg_CreateTracked itself.
 *   SDL owner is gfx_store_dc (see video.h); dialog side keeps only the
 *   OS-free flag/id math below (no SDL here).
 * OS-free math below is LIVE 1:1 on explicit params (no Win16, no globals,
 * read-only); handle/stack-storage tails stay STUB with TODO Orig. */
civ_error_t dlg_create_tracked(int dlg_id, unsigned flags);
void dlg_destroy_tracked(void);

/* P0c tracked-lifecycle constants (Dlg_CreateTracked @ 1098:124f shape). */
#define CIV_DLG_INTRO_ID 0x94
#define CIV_DLG_EARTH_CUSTOM_ID 0x8f
#define CIV_DLG_CREATE_CENTER 0x01u
#define CIV_DLG_CREATE_SHOW 0x40u
#define CIV_DLG_INTRO_FLAGS 0x41u
#define CIV_DLG_TRACKED_CTL_FIRST 0x65
#define CIV_DLG_TRACKED_CTL_LAST 0x6eu
#define CIV_DLG_TRACKED_CTX_SIZE 0x18
#define CIV_DLG_TRACKED_PAL_COUNT 0xec

/* Orig Dlg_CreateTracked @ 1098:124f flag branches -> predicates. 1:1
 * (param_2 & 1) centers via FUN_1098_0b33, (param_2 & 0x40) SHOWWINDOWs.
 * No globals. */
int dlg_create_should_center(unsigned flags);
int dlg_create_should_show(unsigned flags);

/* Orig same ctl-init loop (iVar6 = 0x65; while (<= 0x6e)) -> predicate.
 * 1:1 0x65 <= id <= 0x6e (GETDLGITEM-guarded SENDMESSAGE 0x30 per item).
 * No globals. */
int dlg_ctl_is_tracked_init(int ctl_id);

/* Orig tracked-stack push/pop @ 1420:1ea0/1ec8 -> index math. 1:1 push slot
 * byte offset = count * 4 (each entry is a far HWND/ctx pair: off at +0x1ea0,
 * seg at +0x1ea2); pop count = count - 1 (0 clamps at empty, orig writes
 * DAT_1420_9948 = 0 when count - 1 == 0 else the count-2 slot). -1 on
 * count < 0. No globals (storage stays Win16 GLOBALALLOC/Mem_DisposePtr). */
int dlg_tracked_push_off(int count);
int dlg_tracked_pop_count(int count);

/* Orig FUN_1098_0a0d @ 1098:0a5b / FUN_1098_0aa4 @ 1098:0ae0 extent half ->
 * dlg_move_extent. 1:1 exact (0a0d/0b33: w/h) vs padded (0aa4: w+1/h+1 via
 * (local_e - local_12) + 1). padded != 0 adds one pixel per axis. */
int dlg_move_extent(int size, int padded);

/* Orig same default-origin gate (param_2 == 0 -> local_14, param_3 == 0 ->
 * local_12; FUN_1098_0b33 @ 1098:0b67 always uses the measured origin) ->
 * dlg_move_should_use_default. 1:1 nonzero (1) when param == 0. */
int dlg_move_should_use_default(int param);

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

/* Orig Ctrl_RegisterCustomClasses @ 1058:0b4d REGISTERCLASS x4 count ->
 * dialog_ctrl_class_count. 1:1 4 (strings "RanDoMRadio" @ 0xac6 file
 * 0xb7ac6 / "RanDoMStatic" @ 0xad2 file 0xb7ad2 / "RanDoMUser" @ 0xadf
 * file 0xb7adf / "Region" @ 0xaea file 0xb7aea in unpacked/civ.exe,
 * read-only, no copy). No Win16, no globals. */
int dialog_ctrl_class_count(void);

/* Orig same index gate (0 <= index < 4 selects one REGISTERCLASS name) ->
 * dialog_ctrl_class_valid. 1:1 nonzero (1) when 0 <= index < 4, 0
 * otherwise. No Win16, no globals. */
int dialog_ctrl_class_valid(int index);

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

/* Sequence player (PARTIAL dispatchers + LIVE OS-free halves;
 * anim_advance_frames: 0 = busy, nonzero = done). Refs verified
 * read-only 2026-10-16: strings unpacked/civ.exe ("OPENING.wav",
 * "RanDoMRadio"/"RanDoMStatic"/"RanDoMUser").
 *
 * Orig: FUN_10b0_0000 @ 10b0:0000 -> Anim_PumpIfActive. Runs
 *   Anim_AdvanceFrames while a sequence is active (flag @ 1420:1f40);
 *   wrapped by Sys_Yield @ 1098:0157 (see sys.h). Timer/frame pump ->
 *   SDL timer TBD; PARTIAL dispatcher (OS-free timed gates wired:
 *   active/phase/clamp/period/finish + StoreDC-pump/ret/slot/caption/
 *   dissolve/present/animate/sound/rebase, pump tail CIV_ERR_UNSUPPORTED).
 * Orig: FUN_10b0_0461 @ 10b0:0461 -> Anim_AdvanceFrames. Timed frame
 *   script, opcodes 1..0xe: palette fades/selects, offscreen blits,
 *   sound triggers (Sound_PlayId @ 1150:00c9 bridge), port switches
 *   (GR port layer, see video.h). Returns nonzero when the sequence is
 *   finished. Full script engine -> SDL TBD; PARTIAL dispatcher
 *   (OS-free opcode 1..0xe/tick/ret/slot/caption/dissolve/present/
 *   animate/sound + 11/14 via tick_is_due + frame counters/resolve count
 *   gates wired, reports done until the Win16 ctx bridge lands).
 * Orig: FUN_10b0_008a @ 10b0:008a -> Anim_OpenSequence. mem_alloc_lock
 *   0xa6 ctx + GETCLIENTRECT + Sys_GetTickScaled @ 1100:02e1 base +
 *   max-end scan + GfxPort_Create x2 (offs +9/+0x62) + FUN_1000_14f8 /
 *   FUN_1100_0876 setup + DAT_1420_1f40 = ctx. Win16/GfxPort -> SDL TBD;
 *   PARTIAL dispatcher (OS-free count/resolve/ctx/max + active/full
 *   phase lifecycle + tick (clamp/rebase/due/store/finish/ret) + full
 *   opcode 1..0xe families (palette/slot/store/callback/anim/frame/
 *   scroll/text/present/animate/sound + 11/14 period) + palette bodies
 *   (src/copy/fade) + slot (init/frame/step/finish/play/blend/phase/
 *   free) + caption (phase/palette/custom) + dissolve (phase/free/perm/
 *   swap/rand/col) gates wired; mem_alloc_lock/GETCLIENTRECT/GfxPort/
 *   tick-store/DAT_1420_1f40 device tails CIV_ERR_UNSUPPORTED, never
 *   faked; halves LIVE below).
 * Orig: FUN_10b0_0287 @ 10b0:0287 -> Anim_CloseSequence. Frees slots
 *   3/4 via Anim_FreeSlot @ 10a8:00b4 + dissolve 12/13 via
 *   Anim_FreeDissolve @ 10b0:172d + slot 7 via GfxPort_Close + both
 *   ports (GfxPort_Create x2 offs +9/+0x62 pair, see anim_seq_port_count/
 *   off LIVE below) + GLOBALFREE + Mem_DisposePtr + 1420:1f40 = 0.
 *   Win16 -> SDL TBD; void NOOP with LIVE port-pair + FINISH free-phase
 *   probes (anim_slot_should_free / anim_dissolve_should_free); handle
 *   teardown TODO in dialog.c.
 * Orig: FUN_10b0_0026 @ 10b0:0026 -> Anim_IsActive. Returns
 *   DAT_1420_1f40 (nonzero = sequence active, pumped by
 *   Anim_PumpIfActive). OS-free gate LIVE below. */
civ_error_t anim_pump_if_active(void);
int anim_advance_frames(void);
civ_error_t anim_open_sequence(void);
void anim_close_sequence(void);
/* Orig Anim_OpenSequence @ 10b0:008a count half (param_3 == 0 -> count
 * recs until opcode 0, stride 0xf words) -> anim_seq_count_frames.
 * 1:1 count of opcodes before the first 0 (cap bounds the scan).
 * NULL opcodes -> -1. No globals. */
int anim_seq_count_frames(const int *opcodes, size_t cap);
/* Orig same max-end half (max of rec[7] longs + 1) -> anim_seq_max_end.
 * 1:1 max(ends[]) + 1 (empty -> 1, orig 0 + 1). NULL with n > 0 -> -1. */
long anim_seq_max_end(const long *ends, size_t n);
/* Orig Anim_IsActive @ 10b0:0026 gate -> anim_seq_is_active.
 * 1:1 nonzero (1) when active != 0. */
int anim_seq_is_active(long active);

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

/* Chain 2026-10-16 (p5), goal 5: Diplo_ExchangeTechs @ 1168:1220 "x2".
 * Verified this pass: JEDEN call site (Diplo_PostContact @ 1168:0a07), a
 * "x2" to DWIE strony wymiany — GrantTech(param_1, local_6, param_2) +
 * GrantTech(param_2, local_8, param_1) (ostatnie exclusive z kazdej strony
 * wsrod 0..0x48; co wymieniaja: 1 tech za 1 tech, tylko gdy OBA kierunki
 * maja exclusive (uVar3 == 3); gate'y: obie AI (zadna != player), kontakt
 * (civA+civB+turn)&3 == 0 lub first-contact bez 0x01, brak 0x08 z obu
 * stron, ShouldMakePeace + wonder-7/count z obu stron). Helper ponizej. */

/* Orig Diplo_ExchangeTechs @ 1168:1220 grant pair -> diplo_exchange_grant_count.
 * 1:1 2 (dwa GrantTech w przeciwnych kierunkach na jedno wywolanie). */
int diplo_exchange_grant_count(void);

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

/* Orig: Dialog_MenuZ_Modal @ 1220:0156 preselect gate (DAT_1418_0002).
 * 1:1 OS-free: nonzero (1) when 0 <= preselect < count (radio 500+preselect
 * preselected via Ctrl_RadioGroupSetSel @ 1058:0e7f), 0 otherwise (none /
 * OOR skips preselect, same as the orig skip-disabled scan via local_44+1).
 * count <= 0 -> 0. No Win16, no globals. */
int dialog_menuz_preselect_ok(int preselect, int count);

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
 * PARTIAL dispatcher (OS-free align/slot/shadow/center probes LIVE in
 * text_draw_aligned: text_align_slot/x, text_center_y/centered_x,
 * text_needs_shadow/shadow_passes/shadow_pos/offset, text_measure_scaled,
 * text_resolve_color; zero-width sample, real GETTEXTEXTENT width stays
 * GDI-side). GDI text-out tail (SELECTOBJECT font + SETBKMODE/
 * SETTEXTALIGN + TEXTOUT + shadow via Text_SetColor) stays STUB.
 * SDL_ttf bridge TBD.
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

/* Orig: Dialog_ShowMessage @ 1220:00d2 — modal message PARTIAL
 * dispatcher: OS-free id/text gates LIVE 1:1 (id 0x88 vs 0x91 via
 * dialog_show_message_id/DAT_1418_0008 + preset-size exclusion via
 * dlg_id_needs_preset_size/dlg_preset_w/h + text via
 * dialog_split_title_len/is_title_empty/menu_count/is_sep + MenuZ base/
 * last/timer/cancel/id roundtrip/valid/disabled/art/cancel gates + refresh
 * count/msg via dialog_refresh_view_count/msg); milestone-popup tail of
 * Game_UpdateStatusPanel @ 1088:13d5 (fire via game_status_milestone_due +
 * "00,000 citizens." @ 1420:172b) with the Dlg_CreateTracked @ 1098:124f +
 * Dialog_MenuZ_Modal @ 1220:0156 + Dlg_DestroyTracked @ 1098:1612 +
 * Dialog_RefreshViews @ 1220:0eb8 modal tail STUB (CIV_ERR_UNSUPPORTED,
 * never fake CIV_OK). *out_choice preset to the cancel code (-1) on entry;
 * NOENT on NULL, RANGE on empty text. */
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

/* Orig Diplo_SetPeace @ 1168:0dda / Diplo_SetWar @ 1168:0e8e war-bit
 * tests -> diplo_is_war / diplo_is_seen. 1:1: war == (flags & 0x20) != 0
 * (peace set clears 0x20, war path keeps it; see diplo_apply_peace above),
 * seen == (flags & 0x10) != 0 (war-end sweep clears 0x10 both dirs in
 * Diplo_ProcessTurn @ 1168:0000 via diplo_war_end_clear). No globals. */
int diplo_is_war(int flags);
int diplo_is_seen(int flags);

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

/* --- Chain P4 render+text (2026-10-16): Text_Draw* @ 10d8 color/shadow ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), batch-decompiled this pass:
 *   Text_DrawDirect @ 10d8:00e0 (cVar1 = DAT_12d8_45fd; if
 *     (DAT_12d8_28fc == 0) cVar1 = civ_table[param_5] (s_..._45b5+9);
 *     then Text_SetColor + SELECTOBJECT font + MOVETO + Util_StrCopy +
 *     Text_PackPascal @ 1100:0000 + Text_DrawPascalAtOrigin @ 1100:017b).
 *   FUN_10d8_002d @ 10d8:002d (same mono color pick + SELECTOBJECT +
 *     Text_SetColor + StrCopy + PackPascal + DrawPascalAtOrigin).
 *   Text_DrawShadowed @ 10d8:02df (SelectForCoords gate + DispatchFont +
 *     GETTEXTMETRICS tmHeight local_126 + shadow TEXTOUT at (x + dx, y+1)
 *     with dx = (local_126 < 0x12 ? 0 : 1) + main TEXTOUT at (x,y)).
 * OS-free color/shadow math below is LIVE 1:1 on explicit params (no
 * Win16, no globals); SELECTOBJECT/MOVETO/TEXTOUT/SETBKMODE tails stay
 * STUB in text_draw_* above. */

/* Orig Text_DrawDirect @ 10d8:00e0 + FUN_10d8_002d @ 10d8:002d mono
 * pick -> text_resolve_color. 1:1 mono == 0 (DAT_12d8_28fc color/VGA)
 * ? civ_entry (civ_table[param]) : fallback (DAT_12d8_45fd). No globals. */
int text_resolve_color(int mono, int civ_entry, int fallback);

/* Orig Text_DrawShadowed @ 10d8:02df shadow pos -> text_shadow_pos. 1:1
 * sx = x + (height >= 0x12 ? 1 : 0) (decompiler local_126 < 0x12 ? x :
 * x+1; complements text_shadow_offset dx-only above), sy = y + 1 (shadow
 * row always one down). NULL outs -> NOENT. No globals. */
civ_error_t text_shadow_pos(int x, int y, int height,
                            int *sx_out, int *sy_out);

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
/* Orig Text_SelectFont @ 1100:0a33 wrapper side -> text_select_font.
 * PARTIAL: id->(slot,h) validation LIVE via text_dispatch_font (RANGE on
 * unknown id); LOGFONT install tail @ 11e8:0cb0 (DAT_1420_9960/+0x12,
 * DAT_1420_17cc/+0x28 + Text_FontPreApply @ 1098:0186 / PostApply @
 * 1098:0194, see chain-13 NOOPs above) + SELECTOBJECT -> SDL_ttf TBD
 * (CIV_ERR_UNSUPPORTED). Install timing: once at boot via
 * Font_EnumFamiliesCallback @ 1008:0e12 (see boot.h chain-13), select
 * per-draw here. NULL text -> NOENT on siblings. */
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

/* --- Chain 2026-10-16 (13): ellipsis loop + pre/post tails + measure
 * consistency (goals 3-5; civ.exe, x86:LE:16:Protected Mode, C17) ---
 * Anchors, renamed this pass (Ghidra plates carry the shape):
 *   Text_FitPascalEllipsis @ 1098:0000 (was FUN_1098_0000: Util_StrCopy @
 *     1000:36b2 local_106 + Text_PackPascal @ 1100:0000 +
 *     Text_MeasurePascal @ 1100:00a1; if max < w: bias local_6 = 4, while
 *     (len > 0 && max < measure+4) buf[0]--, while trailing ' ' buf[0]--,
 *     then caller_buf[len] = '.' + caller_buf[len+1] = 0 — single dot).
 *   Text_FitPascalEllipsisScaled @ 11d0:02e4 (was FUN_11d0_02e4: forwards
 *     with max = (w*16)/10, inverse of MeasureWidth (w*10)/0x10).
 * Kiedy ellipsis (za długi tekst w polu? jakie pola? limity szerokości?):
 *   Unit_ShowInfoPopup @ 11f0:468c: unit-name line + city-name line
 *   (City_AppendName), limit = status-panel width - 8
 *   (iVar4 = (local_1a - local_1e) - 8). Scaled wrapper pola: census
 *   production/status (1018:0be0/208f), FUN_1030_8d33, FUN_1188_0b3e,
 *   Pic_PlayShared @ 1030:0000 — limity w jednostkach po MeasureWidth,
 *   wrapper je oddskalowuje (w*16)/10 przed pętlą.
 * Text_FontPreApply @ 1098:0186 / PostApply @ 1098:0194 (ogony dispatch
 *   11e8:0cb0): OBA to puste far stub-y (sama gadanina DS + RETF, brak
 *   ciała). Callerzy poza dispatch: BRAK — jedyne xrefs to ogon
 *   11e8:0cbd (Pre, gdy DAT_1420_b384 != 0) i 11e8:0d0e (Post(0x45cb),
 *   gdy b384 != 0, po zapisach SI do DAT_1420_9960/+0x12 i
 *   DAT_1420_17cc/+0x28). Port: text_font_pre/post_apply NOOP (jak
 *   cursor_set), straże b384/9960/17cc zostają caller-side.
 * Text_MeasurePascal @ 1100:00a1 GETTEXTEXTENT (metryki vs FNT raster —
 *   spójność?): GETTEXTEXTENT mierzy ZAINSTALOWANY font GDI (slot z
 *   dispatch + CREATEFONTINDIRECT z callbacku), font_text_width (boot.h)
 *   sumuje dfCharTable-widths z FNT. Spójność: OBA liczą sumę advance
 *   widths bez trackingu/kerningu (tracking = 0 w obu portach), więc
 *   zgodne co do kształtu; różnice mogą wynikać z hintingu GDI vs
 *   surowych bitów FNT (TODO: dowód na dumpie civfonts.fon vs
 *   GETTEXTEXTENT; brak dowodu = brak claimu równości).
 * font_glyph_to_surface (LIVE w gfx_indexed.c, AJ1) — związek z
 *   render_glyph?: bierze TIGHT bits z font_render_glyph (boot.c,
 *   stride (w+7)/8, MSB-first) i expanduje każdy bit do indeksu 8-bit
 *   (0 = black, 1 = white) + 2-entry paleta w SDL_Surface. NIE bierze
 *   surowych font_fnt_bits (source stride (max+7)/8) — ciasny repack
 *   render_glyph jest jej kontraktem wejściowym; row locating
 *   font_fnt_row_ptr (boot.h) operuje na surowym stride.
 * OS-free pętla ellipsis poniżej jest LIVE 1:1 z measure-callbackiem
 * (= strona GETTEXTEXTENT, FNT-strona via font_text_width); GDI ogony
 * zostają STUB. No Win16, no globals, read-only poza buf. */

/* Orig Text_FontPreApply @ 1098:0186 -> text_font_pre_apply. Empty far
 * stub port (dispatch-tail hook, guard DAT_1420_b384 != 0 caller-side).
 * NOOP void (jak cursor_set). */
void text_font_pre_apply(void);

/* Orig Text_FontPostApply @ 1098:0194 -> text_font_post_apply. Same empty
 * stub shape (dispatch-tail hook with 0x45cb, guard caller-side). NOOP. */
void text_font_post_apply(void);

/* Orig Text_FitPascalEllipsis @ 1098:0000 -> text_fit_pascal_ellipsis.
 * 1:1 truncate loop on a C string: w = measure(buf); if (max < w):
 * while (len > 0 && max < measure+4) drop last char; strip trailing
 * spaces; append single '.' + NUL. measure = GETTEXTEXTENT side
 * (FNT side: font_text_width, see boot.h). NULL buf/measure -> NOENT;
 * cap == 0 -> TRUNCATED; fits -> CIV_OK untouched; no room for dot+NUL
 * -> TRUNCATED (orig assumed room in the far buffer). Empty input fits
 * to "." when max < measure (orig writes the dot even at len 0). */
civ_error_t text_fit_pascal_ellipsis(char *buf, size_t cap, int max_width,
                                     int (*measure)(const char *s, void *ctx),
                                     void *ctx);

/* Orig Text_FitPascalEllipsisScaled @ 11d0:02e4 width half ->
 * text_fit_scaled_width. 1:1 (w*16)/10 (inverse of text_measure_scaled).
 * No clamp. No globals. */
int text_fit_scaled_width(int w);

/* Orig same driver -> text_fit_pascal_ellipsis_scaled. Forwards with
 * max = text_fit_scaled_width(w). Same contracts as above. */
civ_error_t text_fit_pascal_ellipsis_scaled(char *buf, size_t cap, int w,
                                            int (*measure)(const char *s,
                                                           void *ctx),
                                            void *ctx);

/* --- Chain 2026-10-16 (14): popup ellipsis limit + kto jeszcze ucina
 * (goal 4; civ.exe, x86:LE:16:Protected Mode, C17, no exit, no globals) ---
 * Pola = linia nazwy unita + linia miasta w Unit_ShowInfoPopup @ 11f0:468c:
 *   decompile: iVar4 = (local_1a - local_1e) - 8 (status-panel width - 8,
 *   WdwStatus_CalcPanelRect + Status_DrawPanelFrame tail); potem 2x
 *   Text_FitPascalEllipsis @ 1098:0000 z tym limitem: raz linia nazwy unita
 *   (Util_StrCopy 24fa/12d8 z 0x3da+idx*0x20 + Fit + DrawDirect), raz linia
 *   miasta (City_AppendName na 24fa/12d8 + Fit + DrawDirect). Xrefs Fit:
 *   TYLKO scaled wrapper @ 11d0:0301 + te 2 miejsca w popupie (@ 11f0:4808
 *   unit-name, @ 11f0:4a76 city-name) — pełny NE xref scan, brak innych.
 * Kto jeszcze ucina (scaled wrapper @ 11d0:02e4, max = (w*16)/10):
 *   6 xrefs: Census_ShowCityProduction @ 1018:0d42, Census_ShowCityStatus
 *   @ 1018:23d5, FUN_1030_8d33 x2 (@ 1030:8e0b/8f6c), FUN_1188_0b3e
 *   (@ 1188:0d52), Pic_PlayShared @ 1030:0000 (@ 1030:4bea, tryb modal).
 *   Limity tam w jednostkach po Text_MeasureWidth @ 10d8:05b8 ((w*10)/0x10;
 *   wrapper je oddskalowuje (w*16)/10 przed pętlą — patrz
 *   text_fit_scaled_width LIVE). Kto NIE ucina (zweryfikowano brakiem
 *   xrefs Fit/FitScaled): menu (MenuZ @ 1220:0156 id math, brak Fit),
 *   status rows (Text_DrawStatusRow @ 11f8:0278 + formatters 1138/11b0/11b8
 *   — formatują do 12d8:24fa bez trunchacji), dialogi list (DrawDlgItemString
 *   @ 1098:2796 + FillResStringList @ 1098:1f4f — pack bez cięcia), save
 *   prompts (save_prompt_open/save @ 1130:0075/0194 + sanitize — cięcie
 *   nazw plików, nie ellipsis), MainMenu_WndProc @ 11e0:0221, HoF_Show @
 *   1080:0119, diplomacy @ 1160/1168 (Dialog_ShowMessage @ 1220:00d2 modal
 *   bez Fit). OS-free limit poniżej LIVE 1:1 (no Win16); GDI Fit ogon w
 *   text_fit_pascal_ellipsis (LIVE) — ten limit jest jego `max_width`. */

/* Orig Unit_ShowInfoPopup @ 11f0:468c limit half -> unit_popup_text_limit.
 * 1:1 (panel_right - panel_left) - 8 (decompiler iVar4 = local_1a -
 * local_1e + -8). Ujemne dozwolone (orig nie clampuje; ujemny max wymusza
 * ellipsis do "." jak pętla Fit). No globals, no exit(). */
int unit_popup_text_limit(int panel_left, int panel_right);

/* --- Chain 2026-10-16 (22): Anim opcode decoder (goal 1) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Anim_AdvanceFrames @ 10b0:0461 (timed frame script; callers
 *     Anim_PumpIfActive @ 10b0:0000 (flag @ 1420:1f40, wrapped by Sys_Yield
 *     @ 1098:0157) + Main_RunStartupSequence @ 11e0:0000 (2x loops, see
 *     boot.h); frame pump: every 8. tick Gfx_StoreDC @ 1098:1104 +
 *     Util_MemCopyFar @ 1000:137f; pause gate via 0x50 flag + 1420:1f40
 *     type 0/6/9 window; per-tick callback @ +0x51; returns nonzero = done).
 *   Anim_OpenSequence @ 10b0:008a (was FUN_10b0_008a: mem_alloc_lock 0xa6
 *     ctx + GETCLIENTRECT + Sys_GetTickScaled @ 1100:02e1 base + max-end
 *     scan + GfxPort_Create x2 (offs +9/+0x62) + FUN_1000_14f8/1100:0876
 *     setup + DAT_1420_1f40 = ctx; renamed this pass).
 *   Anim_CloseSequence @ 10b0:0287 (was FUN_10b0_0287: frees slots 3/4 via
 *     Anim_FreeSlot @ 10a8:00b4 + slot 7 via GfxPort_Close + closes both
 *     ports + ClearActive x2 + GLOBALFREE + Mem_DisposePtr + 1420:1f40 = 0;
 *     renamed this pass).
 *   Anim slots (segment 10a8, renamed this pass): Anim_AllocSlot @
 *     10a8:0000 (mem_alloc_lock 0x14 ctx, DAT_1420_17a6 gate), Anim_StepSlotA
 *     @ 10a8:00fc / Anim_StepSlotB @ 10a8:018e (timed step variants via
 *     FUN_10a8_0228), Anim_FreeSlot @ 10a8:00b4 (GLOBALFREE + Mem_DisposePtr
 *     @ 1100:060b, DAT_1420_17a6 gate).
 * Full opcode table (switch *local_8 in Anim_AdvanceFrames):
 *   1 palette-fade-zero (GLOBALLOCK palette + zero + flag 1 +
 *     Palette_AnimateRange @ 1090:0376 + GLOBALUNLOCK when port active).
 *   2 palette-set (0x81/0x82/0x85 select + GLOBALLOCK copy + GLOBALUNLOCK).
 *   3 slot-A (Anim_AllocSlot @ 10a8:0000 on start + Anim_StepSlotA @
 *     10a8:00fc per tick + Anim_FreeSlot @ 10a8:00b4 on end; GDI? no:
 *     slot ctx only, NO Sound_PlayId @ 1150:00c9 inside — sound rides
 *     caller-side in Main_RunStartupSequence via Sound_PlayId(3,0)/(1,0),
 *     see boot.h).
 *   4 slot-B (same with Anim_StepSlotB @ 10a8:018e; same no-sound note).
 *   5 port-switch (Gfx_StoreDC + Text_SelectFont @ 1100:0a33 x3 +
 *     FUN_1100_0a4a + GfxPort_SetActive @ 10d0:0956 for +0x12/+0x31).
 *   6 frame-load (0x50 = 1 + GfxPort_Close + Art_LoadFrameData @ 10b8:079d
 *     ([9] + 0x31 param) + Art_PortDataToBitmap @ 10b8:0e36 + 0x50 = 0;
 *     frames come from RSC via Art_LoadResPaletted @ 10b8:11bc or disk via
 *     Art_LoadDiskFile @ 10b8:0aa8, see video.h art chain).
 *   7 caption-text (palette apply + SetActive/CopyRange + ResMgr_GetString
 *     @ 1100:078a or Util_StrCopy @ 1000:36b2 + Util_PascalToCStr @
 *     1100:0053 + Text_DrawAligned @ 1098:0ed1; tail restores + PresentRect
 *     @ 10d0:0854 + ClearActive @ 10d0:09a2).
 *   8 fill-rect (Gfx_StoreDC + Gfx_MakeBrush @ 1098:00b5 + Text_SetBkColor
 *     @ 1098:2144 / Text_SetColor @ 1098:0e0b + FILLRECT x2 + DELETEOBJECT).
 *   9 port-swap (MemCopyFar swap +0x12<->+0x31 + SetActive + CopyRange +
 *     StoreDC + SETRECT/OFFSETRECT + PresentRect).
 *   10 callback ((*(param+0x51))(...) user hook, no default action).
 *   11 scroll-blit (timed (tick-start)%len == 0 gate + SetActive x2 +
 *     CopyRange/Present + FUN_1008_0df0 + OFFSETRECT + FUN_10d0_03fa +
 *     ClearActive x2).
 *   12/13 anim-A/B (SetActive x2 + Present/CopyRange/Animate +
 *     FUN_10b0_151b setup / Anim frame via FUN_10b0_17e5 + free via
 *     FUN_10b0_172d; 12 does the 0xc palette pre-copy branch).
 *   14 palette-rotate (GLOBALLOCK + save + shift + AnimateRange; timed
 *     (tick-start)%len == 0 gate like 11).
 * Format sekwencji: array of frame recs at +8, count at +10, each 0xf words
 *   ([0] opcode above, [3]/[4] rect-or-port fallback, [5]/[7] start/end tick,
 *   [9]/[10] palette range, [11] flags (0x100/0x200/0x400/0x800 port/text
 *   gates), [12] color, [13]/[14] slot ctx; exact word map TBD vs dump).
 * Kto woła: Anim_PumpIfActive (per-tick Sys_Yield pump) + intro loops in
 *   Main_RunStartupSequence (dialog 0x94 + Sound_PlayId intro/STOP + 0x91e
 *   second loop + NewGame_Wizard @ 10e8:0000 tail, see boot.h). OPENING.wav
 *   @ 1420:3114 + MPLOGO.BMP (NE resource, no filename string, see title.h)
 *   ride the same loops caller-side, not inside opcodes.
 * OS-free decoder below is LIVE 1:1 on explicit op/tick (no Win16, no
 * globals, read-only); the timed player stays STUB/PARTIAL (anim_pump_if_
 * active / anim_advance_frames above) until the SDL port/frame bridge lands.
 * No exit(), C17. */
#define ANIM_OPCODE_MIN 1
#define ANIM_OPCODE_MAX 0x0e

/* Orig Anim_AdvanceFrames @ 10b0:0461 switch range -> anim_opcode_is_valid.
 * 1:1 1 <= op <= 0x0e. */
int anim_opcode_is_valid(int op);

/* Orig same palette cases (1, 2, 0xe via Palette_AnimateRange @ 1090:0376 /
 * Palette_CopyRange @ 1098:201d) -> anim_opcode_is_palette. 1:1. */
int anim_opcode_is_palette(int op);

/* Orig same slot cases (3, 4 via Anim_AllocSlot/StepSlotA/B/FreeSlot @
 * 10a8:0000/00fc/018e/00b4) -> anim_opcode_is_slot. 1:1. */
int anim_opcode_is_slot(int op);

/* Orig same direct-StoreDC cases (5, 7, 8, 9 via Gfx_StoreDC @ 1098:1104;
 * 7 both branches, 5/8/9 unconditional; frame-pump every 8. tick is separate
 * in anim_tick_needs_store) -> anim_opcode_needs_store. 1:1. */
int anim_opcode_needs_store(int op);

/* Orig case 10 ((*(param+0x51))(...) user hook) -> anim_opcode_is_callback.
 * 1:1 op == 10. */
int anim_opcode_is_callback(int op);

/* Orig cases 12/13 (FUN_10b0_151b/17e5/172d frame pair) ->
 * anim_opcode_is_anim. 1:1 op == 12 || op == 13. */
int anim_opcode_is_anim(int op);

/* Orig Anim_AdvanceFrames @ 10b0:0461 case 6 (frame-load: 0x50 = 1 +
 * GfxPort_Close + Art_LoadFrameData @ 10b8:079d + Art_PortDataToBitmap @
 * 10b8:0e36 + 0x50 = 0) -> anim_opcode_is_frame_load. 1:1 op == 6. */
int anim_opcode_is_frame_load(int op);

/* Orig Anim_AdvanceFrames @ 10b0:0461 case 11 (scroll-blit: timed
 * (tick-start)%len == 0 gate + SetActive x2 + CopyRange/Present +
 * FUN_1008_0df0 + OFFSETRECT + FUN_10d0_03fa + ClearActive x2) ->
 * anim_opcode_is_scroll. 1:1 op == 11. */
int anim_opcode_is_scroll(int op);

/* Orig frame-pump gate ((tick & 7) == 0 -> Gfx_StoreDC + MemCopyFar) ->
 * anim_tick_needs_store. 1:1 nonzero (1) when (tick & 7) == 0. */
int anim_tick_needs_store(long tick);

/* Orig loop-end (cur >= end -> nonzero done) -> anim_seq_is_finished.
 * 1:1 cur >= end. */
int anim_seq_is_finished(long cur, long end);

/* --- Chain 2026-10-16 (23): Anim slot struct + sequence phases (goals 1/2) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), renamed this pass:
 *   Anim_BlendSlotFrame @ 10a8:0228 (was FUN_10a8_0228: GLOBALLOCK palette +
 *     per-channel out = src*frame/max, alpha byte kept + FUN_1090_0119
 *     apply + GLOBALFREE; sole callers Anim_StepSlotA/B below).
 *   Anim_RebaseClock @ 10b0:005b (was FUN_10b0_005b: seq+0x1a = now - param,
 *     seq+0x16 = param; Startup loop#2 passes 1000).
 *   Anim_SetupDissolve @ 10b0:151b (was FUN_10b0_151b: mem_alloc_lock 0x79c
 *     scatter ctx + random permutation via FUN_1100_03ea for opcodes 12/13).
 *   Anim_StepDissolve @ 10b0:17e5 (was FUN_10b0_17e5: per-tick random-order
 *     pixel copy for 12/13; 12 = palette-gated 0xc branch).
 *   Anim_FreeDissolve @ 10b0:172d (was FUN_10b0_172d: Mem_DisposePtr of the
 *     0x79c ctx; opcode 12/13 tail in Anim_CloseSequence @ 10b0:0287).
 * Slot answers (Anim_AllocSlot @ 10a8:0000 disasm + StepA/B decompile):
 *   ILE SLOTOW (A/B = 2?): NIE — A/B to dwa rodzaje opkodow (3 = slot-A
 *     forward, 4 = slot-B reverse), nie pula 2 slotow. Kazdy wpis timeline
 *     3/4 alokuje wlasny 0x14-kontekst przy starcie (Anim_AllocSlot) i zwalnia
 *     przy koncu (Anim_FreeSlot @ 10a8:00b4 / CloseSequence); wspolbieznie
 *     tyle slotow, ile aktywnych wpisow 3/4. To samo dla 12/13: kazdy wpis
 *     ma wlasny 0x79c scatter-ctx (Setup/Step/FreeDissolve), nie 2.
 *   FORMAT SLOTU 0x14 (z disasm 10a8:0000, pola LE): +0x00 dword art/frame
 *     handle (param_1) | +0x04 u16 geo0 (param_5) | +0x06 u16 geo1 (param_6)
 *     | +0x08 u16 geo2 (param_7) | +0x0a u16 max_frames (param_3) | +0x0c u16
 *     cur_frame (= 0 przy alloc) | +0x0e u16 ticks_per_frame (param_4) |
 *     +0x10 long start_tick (param_2). RAMKA? nie bitmapa — slot niesie
 *     licznik klatki + tempo; POZYCJA? geo0/1/2 to geometria wpisu (exact
 *     semantyka TBD vs dump timeline-rec 0xf slow); CALLBACK +0x51? NIE na
 *     slocie — callback zyje na SEKWENCJI (case 10: (*(seq+0x51))(...) per
 *     tick + rebase w Anim_RebaseClock); TIMER? start_tick + ticks_per_frame
 *     + Sys_GetTickScaled @ 1100:02e1 (frame = (now - start) / per).
 *   STEP: frame = (now - start_tick) / ticks_per_frame; frame > max -> 1
 *     (done, slot zwalniany przez caller); inaczej BlendSlotFrame(slot,
 *     A ? frame : max - frame). B to reverse-fade tego samego blenda.
 * ZWIAZEK Z Art_LoadFrameData @ 10b8:079d: BRAK bezposredniego — sloty
 *   fade'uja PALETE (BlendSlotFrame na GLOBALLOCK palette + FUN_1090_0119),
 *   Art_LoadFrameData laduje BITMAPY klatek dla opkodu 6 (RSC via
 *   Art_LoadResPaletted @ 10b8:11bc lub disk via Art_LoadDiskFile @
 *   10b8:0aa8 + Art_PortDataToBitmap @ 10b8:0e36; por. video.h art chain).
 *   Sekwencje nie pochodza z plikow RSC jako calosc: to inline tablice
 *   timeline-rec (param_4 array + param_3 count w Anim_OpenSequence @
 *   10b0:008a); intro loop#1 jedzie na dialogu 0x94 + Sound 3/STOP
 *   caller-side (OPENING.wav @ 1420:3114 + MPLOGO.BMP jako NE resource bez
 *   filename string, patrz title.h/boot.h), loop#2 na 0x91e + rebase 1000.
 * CYKL ZYCIA: open (alloc 0xa6 + GETCLIENTRECT + tick-base + max-end scan
 *   + GfxPort_Create x2 + DAT_1420_1f40 = ctx) -> pump (Anim_PumpIfActive @
 *   10b0:0000 gdy 1420:1f40 != 0, via Sys_Yield @ 1098:0157) -> advance
 *   (Anim_AdvanceFrames @ 10b0:0461 opcode switch 1..0xe) -> close
 *   (Anim_CloseSequence: free slotow 3/4 + dissolve 12/13 + GfxPort_Close x2
 *   + 1420:1f40 = 0). Fazy ponizej sa LIVE 1:1 na jawnych licznikach (no
 *   Win16, no globals); player (pump/advance) zostaje STUB/PARTIAL powyzej
 *   az mostek GfxPort/frame/SDL wyladuje. No exit(), C17. */
#define ANIM_SLOT_SIZE 0x14
#define ANIM_SLOT_OP_A 3
#define ANIM_SLOT_OP_B 4

typedef struct anim_slot {
    int art;             /* +0x00 art/frame handle (param_1) */
    int geo0;            /* +0x04 geometry word (param_5) */
    int geo1;            /* +0x06 geometry word (param_6) */
    int geo2;            /* +0x08 geometry word (param_7) */
    int max_frames;      /* +0x0a frame count (param_3) */
    int cur_frame;       /* +0x0c current frame (0 at alloc) */
    int ticks_per_frame; /* +0x0e ticks per frame (param_4) */
    long start_tick;     /* +0x10 start tick (param_2) */
} anim_slot_t;

/* Orig Anim_AllocSlot @ 10a8:0000 field init -> anim_slot_init. 1:1 wypelnia
 * pola jak wyzej (cur_frame = 0). NULL slot -> NOENT. max <= 0 lub
 * per <= 0 -> RANGE (orig dzielilby przez zero / konczyl od razu). */
civ_error_t anim_slot_init(anim_slot_t *slot, int art, int geo0, int geo1,
                           int geo2, int max_frames, int ticks_per_frame,
                           long start_tick);

/* Orig Anim_StepSlotA/B @ 10a8:00fc/018e licznik -> anim_slot_frame_at.
 * 1:1 frame = (now - start) / per (C trunc). per <= 0 -> -1. */
long anim_slot_frame_at(long start, long now, int ticks_per_frame);

/* Orig same gate (max < frame -> 1) -> anim_slot_is_finished. 1:1
 * frame > max. */
int anim_slot_is_finished(long frame, int max_frames);

/* Orig Anim_BlendSlotFrame @ 10a8:0228 kanal -> anim_slot_blend.
 * 1:1 (src * frame) / max (clamp 0..255; alpha caller-side). max <= 0,
 * frame < 0 -> -1. src OOR 0..255 -> -1. */
int anim_slot_blend(int src, long frame, int max_frames);

/* Orig StepA vs StepB wybor (frame vs max-frame) -> anim_slot_play_frame.
 * 1:1 op 3 -> frame, op 4 -> max - frame; inny op -> -1. */
long anim_slot_play_frame(int op, long frame, int max_frames);

/* Fazy sekwencji (open -> pumping -> done -> closed). */
typedef enum anim_seq_phase {
    ANIM_SEQ_CLOSED = 0,
    ANIM_SEQ_OPEN = 1,
    ANIM_SEQ_PLAYING = 2,
    ANIM_SEQ_DONE = 3
} anim_seq_phase_t;

/* Orig cykl open/pump/advance/close -> anim_seq_phase_step. 1:1: CLOSED +
 * opened -> OPEN; OPEN + pumped -> PLAYING; PLAYING + (cur >= end) -> DONE;
 * DONE + closed -> CLOSED; inaczej brak zmiany. Flagi 0/1 explicite (no
 * globals; DAT_1420_1f40 active-flag zostaje caller-side). */
anim_seq_phase_t anim_seq_phase_step(anim_seq_phase_t phase, int opened,
                                     int pumped, int finished, int closed);

/* --- Chain 2026-10-16 (24): dissolve 12/13 + clock rebase (task goals 1/2) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Anim_SetupDissolve @ 10b0:151b (mem_alloc_lock 0x79c scatter ctx; header
 *     words: [0]=opcode 12/13, [1]=param_2 byte, [2..3]=start long, [4..5]=
 *     span long (param_5-param_4), [6..]=rect copy, [7]/[9]=rect y0/y1,
 *     [8]/[6]=rect x0/x1 words, [10..13]=port far ptrs; then identity
 *     pu[i+0x1ed]=i-1 for i=w..1, per-row pu[row+0xd]=rand()%w for
 *     row=h..1, then w random swaps pu[a+0x1ee]<->pu[b+0x1ee] with
 *     a,b=rand()%w; rand via Rng_NextRaw thunk FUN_1100_03ea @ 1100:03ea
 *     (Rng_NextRaw @ 1000:1729: LCG via FUN_1000_16fe mul, & 0x7fff)
 *     modulo-shaped (raw&0x7fff)%n — NIE skala Rng_Range @ 11c8:003d
 *     (raw*n>>15); callee of Anim_AdvanceFrames @ 10b0:0461 case 12/13
 *     entry (DAT_12d8_28fc == 0 + start-tick gate)).
 *   Anim_StepDissolve @ 10b0:17e5 (per-tick: col=(start+perm[(elapsed+row)%w])
 *     %w over rows h..1, GfxPort_SetActive x2 + GLOBALLOCK bitmaps +
 *     Art_PortDataToBitmap @ 10b8:0e36 + ClearActive x2; 12 = palette-gated
 *     0xc branch (copy src->dst skipping bVar1, bVar1==0 reads dst pixel),
 *     else fill branch (dst < bVar1 -> bVar2); row stride advance via the
 *     0xf-shift carry shape).
 *   Anim_FreeDissolve @ 10b0:172d (Mem_DisposePtr of the 0x79c ctx + zero
 *     [13]/[14]; tail of case 12/13 end-tick in AdvanceFrames + CloseSequence
 *     @ 10b0:0287).
 * Rozmiar planszy (0x79c = 1948 B = ?chroni? ile pikseli?): header 0x1ee
 *   words (494 w = 988 B) + permutacja szerokosci w slow + tablica startow
 *   wierszy h slow; pojemnosc = 0x79c/2 - 0x1ee = 480 wpisow (typowo w <=
 *   320 przy dissolve prostokata + h wierszy, np. 320+200 = 520 > 480?
 *   orig liczy w = ctx[4] low-word span (NIE pelna 320) i h = ctx[9]-ctx[7]
 *   wiec w+h <= 480; pelny 320x200 blit idzie pasami wierszy, nie jednym
 *   ctx na caly ekran — stad staly 0x79c wystarcza).
 * Permutacja (Fisher-Yates? LCG? seed?): NIE czysty Fisher-Yates (ten
 *   zyje w map.c shuffle via rng_range): identity + w losowych transpozycji
 *   (a,b = modulo-draws, swap) + per-row starty modulo-draws; LCG to
 *   Rng_NextRaw @ 1000:1729 (seed DAT_1420_4ee0/4ee2, Rng_Init @ 11c8:0000
 *   seed = Sys_GetTickScaled & 0x7fff, forced nonzero; Rng_Seed @
 *   1000:1716 setter); seed dissolve = biezacy stan RNG (brak wlasnego
 *   seeda w ctx).
 * Zwiazek z opkodami 12/13 w AdvanceFrames: 12/13 to para dissolve
 *   (setup przy start-tick, step per-tick, free przy end-tick; 12 robi
 *   dodatkowo palette pre-copy 0xc branch + AnimateRange, 13 nie).
 *   GDI/bitblt ogony zostaja STUB (art_port_data_to_bitmap w video.h);
 *   permutacja/krok ponizej sa LIVE 1:1 na jawnych parametrach (bez GDI,
 *   bez globali, bez exit(), C17).
 *   Anim_RebaseClock @ 10b0:005b (seq+0x1a = now-param, seq+0x16 = param;
 *     now = Sys_GetTickScaled @ 1100:02e1 = GETTICKCOUNT*0x3c/1000; OpenSequence
 *     @ 10b0:008a init zeruje +0x16 i stawia +0x1a = now). Po co rebase
 *     (pauza? seek? loop?): SEEK/skok logicznego zegara (frame = now -
 *     base): Startup loop#2 w Main_RunStartupSequence @ 11e0:0000 rebase
 *     1000 (przeskocz pierwsze 1000 tickow intro-nakladki / czasu map-gen),
 *     MainMenu_WndProc @ 11e0:0221 rebase 100000 (menu idle) + rebase 0x48c
 *     (1164, re-sync petli menu gdy 0x8c < tick < 0x48c); NIE pauza ani
 *     loop-wrap (petla kreci AdvanceFrames, nie rebase). Kto wola poza
 *     startup: MainMenu_WndProc @ 11e0:0884 (100000) + @ 11e0:090d (0x48c);
 *     PumpIfActive @ 10b0:0000 / CloseSequence @ 10b0:0287 NIE rebase'uja
 *     (pump tylko AdvanceFrames gdy 1420:1f40 != 0, close tylko free).
 *     Port ponizej LIVE (anim_rebase_clock). */
#define ANIM_DISSOLVE_CTX_SIZE 0x79cu
#define ANIM_DISSOLVE_HDR_WORDS 0x1eeu
#define ANIM_DISSOLVE_MAX_ENTRIES 480u

/* Orig Anim_SetupDissolve @ 10b0:151b rand half -> anim_dissolve_rand_index.
 * 1:1 (raw & 0x7fff) % n (modulo-draw, NIE skala rng_range @ 11c8:003d).
 * n <= 0 -> -1. No globals. */
int anim_dissolve_rand_index(unsigned raw, int n);

/* Orig same identity half (pu[i+0x1ed]=i-1, i=w..1) -> anim_dissolve_perm_init.
 * 1:1 perm[i]=i for 0..n-1 (shifted base, ta sama permutacja). NULL perm ->
 * NOENT; n <= 0 lub n > ANIM_DISSOLVE_MAX_ENTRIES -> RANGE; else CIV_OK. */
civ_error_t anim_dissolve_perm_init(int *perm, size_t n);

/* Orig same swap half (a,b=rand()%w, swap pu[a+0x1ee]<->pu[b+0x1ee]) ->
 * anim_dissolve_swap. 1:1 jedna transpozycja perm[r1%n]<->perm[r2%n].
 * NULL perm -> NOENT; n <= 0 -> RANGE (r1/r2 dowolne unsigned). */
civ_error_t anim_dissolve_swap(int *perm, size_t n, unsigned r1, unsigned r2);

/* Orig Anim_StepDissolve @ 10b0:17e5 col half ->
 * anim_dissolve_col. 1:1 (start_col + perm_val) % w (w = ctx[4] low word;
 * caller liczy idx=(elapsed+row)%w, tu czysta kolumna). w <= 0 -> -1. */
int anim_dissolve_col(int start_col, int perm_val, int w);

/* Orig Anim_RebaseClock @ 10b0:005b -> anim_rebase_clock.
 * 1:1 *out_base = now_tick - param, *out_stored = param (seq+0x1a/+0x16).
 * NULL out -> NOENT (drugi out moze byc NULL = skip). No globals. */
civ_error_t anim_rebase_clock(long now_tick, long param, long *out_base,
                              long *out_stored);

/* Orig Anim_AdvanceFrames @ 10b0:0461 head (decompiled 2026-10-16:
 * local_10 = Sys_GetTickScaled @ 1100:02e1 - *(seq+0xd); if (end <=
 * local_10) local_10 = end; while (cur < local_10) pump frame recs) ->
 * anim_clamp_tick. 1:1 min(now - base, end) (elapsed clamp, caller feeds
 * now = sys_tick_scaled_from(tick)). No globals. */
long anim_clamp_tick(long now, long base, long end);

/* Orig Anim_AdvanceFrames @ 10b0:0461 tail (decompiled 2026-10-16: cur <
 * end -> busy (cur>>8)<<8, else done ((cur>>8)<<8)|1; low byte is the
 * while(c == 0) gate in Main_RunStartupSequence @ 11e0:0000 loop#1/loop#2)
 * -> anim_advance_ret. 1:1 exact int shape (boolean half stays in
 * anim_seq_is_finished). No globals. */
int anim_advance_ret(long cur, long end);

/* Orig Anim_StepSlotA/B @ 10a8:00fc/018e step gate (decompiled 2026-10-16:
 * pc = per * cur; if (start + pc <= now) frame = (now - start) / per, else
 * hold; A blends frame forward, B blends max - frame) ->
 * anim_slot_should_step. 1:1 nonzero (1) when start + per*cur <= now.
 * per <= 0 -> 0 (orig would divide by zero once the gate passes; here a
 * hard stop instead of the far-pointer fault). No globals. */
int anim_slot_should_step(long start, int per, int cur, long now);

/* Anim_AdvanceFrames @ 10b0:0461 opcode map (decompiled 2026-10-16, full
 * switch 1..0xe over the seq records at seq+8, each gated by
 * [start,end] local_c window; outer clock via anim_clamp_tick +
 * rebase via anim_rebase_clock + ret via anim_advance_ret above):
 *   1 palette fade-zero (GLOBALLOCK + zero RGB + Palette_AnimateRange),
 *   2 palette table copy (0x81/0x82/0x85 -> blobs, see anim_pal_src_for),
 *   3/4 slot A/B (Alloc on start, Step + Free on end),
 *   5 text/font triple (StoreDC + SelectFont + Text_SyncFontTail @
 *     1100:0a4a x3 + SetActive x2),
 *   6 frame load (Close + Art_LoadFrameData @ 10b8:079d +
 *     Art_PortDataToBitmap @ 10b8:0e36),
 *   7 caption (ApplyPalette + Store/SetActive + Get/CopyRange +
 *     ResMgr_GetString/PackPascal + DrawAligned, present on end),
 *   8 fill (StoreDC + MakeBrush + SetBk/Color + FILLRECT x2 + DelObj),
 *   9 port swap + present (memcpy swap + Palette_AnimateLockedRange @
 *     1090:0119 on 0xb),
 *   0xa callback (call seq+0x51),
 *   0xb scroll (SetActive x2 + SnapshotAndAnimate @ 1098:25b9 on start
 *     else Gfx_ScrollPortCopy @ 10d0:03fa + Anim_ScrollSyncTick @
 *     1008:0df0),
 *   0xc/0xd dissolve (Setup/Step/FreeDissolve + present),
 *   0xe palette rotate (GLOBALLOCK rotate + AnimateRange).
 * OS-free opcode predicates already LIVE (anim_opcode_is_* in dialog.c);
 * bodies below are the small pure tails (no Win16). Device tails stay
 * STUB in anim_advance_frames PARTIAL. Next: Art_LoadFrameData payload
 * carve + shared Pic_PlayShared @ 1030:0000 (~31 KB, 16 callers). */

/* Orig Anim_ScrollSyncTick @ 1008:0df0 (renamed 2026-10-16 from
 * FUN_1008_0df0; callers Anim_AdvanceFrames @ 10b0:0461 opcode 0xb +
 * FUN_11d8_0000). Returns 0 always (empty sync stub). OS-free 1:1. */
int anim_scroll_sync_tick(void);

/* Orig Text_SyncFontTail @ 1100:0a4a (renamed 2026-10-16 from
 * FUN_1100_0a4a; called 3x in Anim opcode 5 text path). Thin wrapper
 * over Font_EnumFamiliesCallbackTail @ 1008:09d3 (empty NOOP tail).
 * NOOP void like the tail. */
void text_sync_font_tail(void);

/* Orig Dlg_CenterTrackedWindow @ 1098:0b33 (renamed 2026-10-16 from
 * FUN_1098_0b33; flags&1 path of Dlg_CreateTracked @ 1098:124f).
 * OS-free 1:1 port of the GETWINDOWRECT + Dlg_CenterRect @ 11f8:0000 +
 * MOVEWINDOW shape on explicit rects: the Win16 HWND lookups (GETWINDOWRECT
 * for the parent/dialog pair) and the MOVEWINDOW application stay
 * caller-side (handles mapped to explicit dlg_rect_t offsets/sizes, no
 * __far/segments, no globals); the centering math itself is LIVE via
 * dlg_center_rect above (same kept inner size). NULL parent/dlg/out ->
 * NOENT, else CIV_OK with *out set. Refs verified read-only 1:1 vs
 * unpacked/civ.exe (NE LE, no copy). */
civ_error_t dlg_center_tracked_window(const dlg_rect_t *parent,
                                      const dlg_rect_t *dlg,
                                      dlg_rect_t *out);

/* Orig Dlg_InitPaletteWord @ 1098:1196 (renamed 2026-10-16 from
 * FUN_1098_1196; tail of Dlg_CreateTracked @ 1098:124f palette copy).
 * When color (DAT_12d8_28fc == 0) writes *(param+6) =
 * Gfx_GetZeroPaletteWord @ 1098:1184 (= 0). OS-free gate 1:1 on the
 * explicit flag: 1 when the write happens, 0 when headless. The word
 * store itself stays caller-side. */
int dlg_init_palette_word_writes(int headless_28fc);

/* Orig Gfx_GetZeroPaletteWord @ 1098:1184 (renamed 2026-10-16 from
 * FUN_1098_1184; sole writers via Dlg_InitPaletteWord above + two
 * sibling 1098:11c8/11f0 paths). Returns 0 always. OS-free 1:1. */
int gfx_get_zero_palette_word(void);

/* --- Chain 2026-10-16 (33): Anim opcode bodies — palette/slot/caption/
 * dissolve phases (critical path, goal: engine 1..0xe) ---
 * Orig Anim_AdvanceFrames @ 10b0:0461 case bodies (civ.exe,
 * x86:LE:16:Protected Mode), decompiled 2026-10-16 (GLOBALLOCK/GfxPort/
 * Art/ResMgr tails stay in the anim_advance_frames PARTIAL dispatcher
 * above; the 11/0xe
 * period half is already LIVE as anim_tick_is_due, mono caller-side).
 * OS-free halves below are LIVE 1:1 on explicit params (no Win16, no
 * globals, read-only except caller-owned palette outs, no exit(), C17).
 * Palette entries are 4 B (RGBA-ish, alpha kept/copied verbatim like
 * Anim_BlendSlotFrame @ 10a8:0228, see anim_slot_blend). */

/* Orig case 2 palette-blob select (p9 == 0x81 -> blob @ 0x12b0005a, 0x82
 * -> 0x12b00082, 0x85 -> 0x12b0006e) -> anim_pal_src_for. 1:1 0x81 -> 0,
 * 0x82 -> 1, 0x85 -> 2, else -1 (caller picks the blob bytes; blob
 * contents TBD from dump). */
int anim_pal_src_for(int code);

/* Orig case 1 fade-zero fill (for i in [p9,p10): pal[i*4+0..2] = 0,
 * pal[i*4+3] = 1, then Palette_AnimateRange) -> anim_pal_fade_zero. 1:1
 * zero-fill entries [first,last) with (0,0,0,1). NULL pal -> NOENT;
 * first < 0 / last > (int)cap_entries / first >= last -> RANGE; else
 * CIV_OK (orig unbounded far write; here capped). */
civ_error_t anim_pal_fade_zero(uint8_t *pal, size_t cap_entries, int first,
                               int last);

/* Orig case 2 palette-set copy (for i in [0,n): dst[(i+off)*4..+4] =
 * src[i*4..+4], u32 entries) -> anim_pal_copy. 1:1 n-entry copy to
 * dst_off. NULL dst/src -> NOENT; n < 0 / dst_off < 0 / dst_off + n >
 * dst_cap / n > src_cap -> RANGE; n == 0 -> CIV_OK (no-op).
 * GLOBALLOCK/AnimateRange tails stay caller-side. */
civ_error_t anim_pal_copy(uint8_t *dst, size_t dst_cap, int dst_off,
                           const uint8_t *src, size_t src_cap, int n);

/* Orig cases 3/4 slot lifecycle (start == cur -> Alloc, needs_store
 * caller-side; end == cur && slot -> Step + Free + clear; slot &&
 * middle -> Step; else nothing) -> anim_slot_phase. 1:1
 * ANIM_SLOTPH_ALLOC / FINISH / STEP / NONE below. */
#define ANIM_SLOTPH_NONE 0
#define ANIM_SLOTPH_ALLOC 1
#define ANIM_SLOTPH_STEP 2
#define ANIM_SLOTPH_FINISH 3
int anim_slot_phase(long start, long end, long cur, int has_slot);

/* Orig Anim_FreeSlot @ 10a8:00b4 free gate (slots 3/4 tail in
 * Anim_CloseSequence @ 10b0:0287 + end-tick in Anim_AdvanceFrames @
 * 10b0:0461 cases 3/4) -> anim_slot_should_free. 1:1 nonzero (1) when
 * anim_slot_phase above is ANIM_SLOTPH_FINISH (end == cur && slot, with
 * start == cur ALLOC winning ties), 0 otherwise. No Win16, no globals. */
int anim_slot_should_free(long start, long end, long cur, int has_slot);

/* Orig case 7 caption flag bits (0x100 = palette apply on start-tick,
 * 0x200 = alt-port palette path, 0x400 = force draw every tick, 0x800 =
 * alt-palette copy path). */
#define ANIM_CAPF_PALETTE_APPLY 0x100
#define ANIM_CAPF_ALT_PORT 0x200
#define ANIM_CAPF_FORCE 0x400
#define ANIM_CAPF_ALT_PAL 0x800

/* Orig case 7 draw/undraw branch ((start == cur || flags & 0x400) ->
 * draw; else end == cur -> undraw; else nothing) -> anim_caption_phase.
 * 1:1 ANIM_CAPPH_DRAW / UNDRAW / NONE below (draw wins ties). */
#define ANIM_CAPPH_NONE 0
#define ANIM_CAPPH_DRAW 1
#define ANIM_CAPPH_UNDRAW 2
int anim_caption_phase(long start, long end, long cur, int flags);

/* Orig case 7 palette-apply gate ((start == cur) && (flags & 0x100) ->
 * MainPort_ApplyPalette) -> anim_caption_needs_palette. 1:1 nonzero (1)
 * when start == cur and (flags & ANIM_CAPF_PALETTE_APPLY) != 0. */
int anim_caption_needs_palette(long start, long cur, int flags);

/* Orig case 7 text-source select (text ptr == "PicDecompress..." magic +
 * 0x19 -> Util_StrCopy path, else ResMgr_GetString @ 1100:078a) ->
 * anim_caption_is_custom. 1:1 nonzero (1) when is_magic != 0 (caller
 * compares the pointer). */
int anim_caption_is_custom(int is_magic);

/* Orig cases 12/13 dissolve lifecycle (mono == 0 && start == cur ->
 * Setup; end == cur -> Step + Present + FreeDissolve + clear; else Step
 * + Present; ClearActive tails caller-side) -> anim_dissolve_phase. 1:1
 * ANIM_DIZPH_SETUP / FINISH / STEP below (setup wins ties).
 * mono = DAT_12d8_28fc. */
#define ANIM_DIZPH_STEP 0
#define ANIM_DIZPH_SETUP 1
#define ANIM_DIZPH_FINISH 2
int anim_dissolve_phase(long start, long end, long cur, int mono);

/* Orig Anim_FreeDissolve @ 10b0:172d free gate (opcodes 12/13 tail in
 * Anim_CloseSequence @ 10b0:0287 + end-tick in Anim_AdvanceFrames @
 * 10b0:0461 cases 12/13) -> anim_dissolve_should_free. 1:1 nonzero (1)
 * when anim_dissolve_phase above is ANIM_DIZPH_FINISH (end == cur, with
 * mono == 0 && start == cur SETUP winning ties), 0 otherwise. No Win16,
 * no globals. */
int anim_dissolve_should_free(long start, long end, long cur, int mono);

/* --- Chain 2026-10-16 (25): MenuZ modes + tracked IDs (task goal 4) ---
 * Orig Dialog_MenuZ_Modal @ 1220:0156 (civ.exe, x86:LE:16:Protected Mode),
 * decompiled this pass (shared modal MainMenu + kazdy NewGame_Wizard step):
 *   Debug_LogTrace MenuZ/windows.c + DAT_1420_b3e2 = param_8 + SELECTOBJECT +
 *   GETTEXTMETRICS + MAKEPROCINSTANCE + SETWINDOWLONG hook + SETTIMER 100
 *   (0 = 'Can't initialize timer in MenuZ' @ 1420:4a41 via FUN_1098_28e4) +
 *   0x18 slots clear + Sys_DrainInput + Cursor_Set(0x81) + DAT_1418_0006 art
 *   branch: -1 = plain (iVar8 = 0, iVar9 = 0); < 3 = small pic (0x51/0x74 +
 *   SETRECT + local 0x92fc art); else = GfxPort_Create + SetActive +
 *   Census_DrawTechBadge (0x3b/0x66 + SETRECT) + Util_StrCopy menu text +
 *   Dialog_GetItemRect 0x68 + Dialog_SplitMenuText @ 1220:0000 (was
 *   FUN_1220_0000, renamed this pass: '_' split do bufora 0x12d8:0x24fa) +
 *   FUN_1068_1f7e layout + Dialog_GetItemRect 0x67 + OFFSETRECT + SHOWWINDOW
 *   + Gfx_StoreDC + Dialog_DrawFrame3D @ 1220:0d38 (gdy DAT_1418_000c) +
 *   CREATEWINDOW RanDoMStatic 0x4a6e + RanDoMRadio 500+i (DISABLE gdy
 *   DAT_12d8_24da bit; preselect DAT_1418_0002, skip disabled via local_44+1)
 *   + Ctrl_RadioGroupSetSel @ 1058:0e7f (500..0x208) + PEEKMESSAGE/
 *   ISDIALOGMESSAGE pump az DAT_12b0_00a0 != 0; exit: SETWINDOWLONG restore +
 *   FREEPROCINSTANCE + DAT_1418_0004 = (DAT_1418_0008 >= 1 && 00a0 == -1) +
 *   reset DAT_1418_0006 = -1 / 0002 = -1 / 000c = 0 / 0008 = 0 / 24dc = 0 /
 *   24da = 0 / 24e0 = 0 / 000a = 1 / 000e = 0x96 / 0000 = 0xd8 + KILLTIMER +
 *   GLOBALFREE + SELECTOBJECT restore; returns DAT_12b0_009c (index).
 * Jakie tryby (modal? jakie ID dialogow? 0x94? 0x91e?):
 *   modal TAK (blokuje PEEKMESSAGE/ISDIALOGMESSAGE az wybor; DAT_1418_0004 =
 *   back/cancel jak w wizardzie). DAT_1418_0006 art-mode: -1 plain menu
 *   (MainMenu_WndProc @ 11e0:0221: tekst @ 1420:435b '_Start a New Game_Load
 *   a Saved Game_Play on EARTH_Customize World_View Hall of Fame_Quit' +
 *   g_nMenuChoice @ 12d8:44e2; petla while choice < 0 || > 3 + dispatch
 *   1 = Save_LoadDialog @ 1130:000e, 3 = WorldGen_LoadEarth @ 1108:0243
 *   (dialog 0x8f, != 0 -> re-prompt -1), 4 = HoF_Load + HoF_Show, 5 =
 *   WinMain_Shutdown; Sound_PlayId() click po wyborze), 0..2 small-pic,
 *   >= 3 tech-badge (Census_DrawTechBadge; DAT_1418_000a cols). ID dialogow:
 *   0x94 intro/PIC host (Main_RunStartupSequence @ 11e0:0000 loop#1 +
 *   Pic_PlayShared @ 1030:0000 0x94 0x41), 0x8b mainmenu (WndProc), 0x90
 *   wizard frame (art 0x8d), 0x8f earth-custom (WorldGen_LoadEarth), 0x97
 *   wizard finalize, 0x88 vs 0x91 message (Dialog_ShowMessage @ 1220:00d2
 *   via DAT_1418_0008: 0 -> 0x88 else 0x91), 0x91e to NIE dialog tylko Anim
 *   sequence id loop#2 (Anim_OpenSequence param 0x91e). 0x94 w nazwie
 *   (Dlg_CreateTracked(0x94...)) to dialog; 0x91e to sequence.
 * Zwiazek z Dlg_CreateTracked @ 1098:124f i Dialog_MenuZ_Modal: ShowMessage
 *   = Dlg_CreateTracked(0x88/0x91) + Dialog_MenuZ_Modal + Dlg_DestroyTracked
 *   + Dialog_RefreshViews @ 1220:0eb8 (SENDMESSAGE 0xf do okien); MenuZ to
 *   srodek, Tracked to host (palette copy + stack @ 1420:1ea0/1ec8, 30+
 *   callerow). dialog_menuz (STUB) + dlg_create_tracked (STUB) + helpers
 *   id<->index (LIVE) zostaja; ponizszy dispatch to PARTIAL (OS-free galezie
 *   LIVE, Win16 pump STUB).
 * Rename galezi: Dialog_SplitMenuText @ 1220:0000 (split), art-mode via
 *   dialog_menuz_art_mode, msg-id via dialog_show_message_id, loop-gate via
 *   dialog_menuz_should_loop, cancel-gate via dialog_menuz_is_cancel.
 *   Wewnatrz korpusu Dialog_MenuZ_Modal @ 1220:0156 (body ..0d37, verified
 *   disasm this pass): +0b88 = CALLF Text_GetFontHeight @ 1098:0dda (repaint
 *   font-height measure w PEEKMESSAGE/ISDIALOGMESSAGE pump), +0889 = CALLF
 *   Text_SetColor @ 1098:0e0b (menu color setup przed CREATEWINDOW static
 *   0x4a6e; siostrzane +10c6/+118e to TEXTOUT menu items, patrz
 *   text_draw_aligned). Sygnatura dekompilera: int __cdecl16far
 *   Dialog_MenuZ_Modal(char*,u2,u2,u2,u2,u2,char,u4) -> int index
 *   (DAT_12b0_009c); petla modalna: PEEKMESSAGE + ISDIALOGMESSAGE az
 *   DAT_12b0_00a0 != 0; mapowanie id<->index: RanDoMRadio 500+i <->
 *   zero-based index (dialog_menuz_id_to_index/index_to_id, base 500).
 *   Sasiedzi w zakresie: Dialog_ShowMessage @ 1220:00d2 (caller MenuZ),
 *   Dialog_DrawFrame3D @ 1220:0d38 (caller MenuZ gdy DAT_1418_000c),
 *   Dialog_RefreshViews @ 1220:0eb8 (ogon ShowMessage, SENDMESSAGE 0xf),
 *   GfxPort_SelectForCoords @ 1220:0f2f (port-select, wlasnosc video.h).
 * OS-free dispatch ponizej LIVE 1:1 (no Win16, no globals); pump/Win16 tails
 * STUB. No exit(), C17. */

/* Orig DAT_1418_0006 art branch -> dialog_menuz_art_mode. 1:1 -1 -> 0
 * (plain), 0..2 -> 1 (small pic), >= 3 -> 2 (tech badge). */
int dialog_menuz_art_mode(int opt_0006);

/* Orig Dialog_ShowMessage @ 1220:00d2 id choice (DAT_1418_0008 == 0 ? 0x88
 * : 0x91) -> dialog_show_message_id. 1:1. */
int dialog_show_message_id(int flag_0008);

/* Orig MainMenu_WndProc @ 11e0:0221 re-prompt gate (choice < 0 || > 3) ->
 * dialog_menuz_should_loop. 1:1 nonzero (1) gdy re-prompt, 0 gdy accept
 * (g_nMenuChoice @ 12d8:44e2 0..3 wchodzi w Startup loop#2; 1/3/4/5 dispatch
 * caller-side; 4/5 nigdy w PrepareEarth — 'inne -> return 1 bez mapy'). */
int dialog_menuz_should_loop(int choice);

/* Orig MenuZ exit (DAT_1418_0008 >= 1 && DAT_12b0_00a0 == -1 -> 0004 = 1) ->
 * dialog_menuz_is_cancel. 1:1 nonzero (1) gdy cancel/back, 0 gdy pick. */
int dialog_menuz_is_cancel(int flag_0008, int done_00a0);

/* Orig Dlg_CreateTracked @ 1098:124f dialog-id preset-size branch
 * (param_1 == 0x94 || == 0x8f -> DAT_12d8_0026 = 1 + 002e = 0x200 + 002c =
 * 0x140) -> dlg_id_needs_preset_size. 1:1 nonzero (1) dla 0x94/0x8f. */
int dlg_id_needs_preset_size(int dlg_id);

/* --- Chain 2026-10-16 (26): Dialog_SplitMenuText '_' split (task goal 3) ---
 * Orig Dialog_SplitMenuText @ 1220:0000 (civ.exe, x86:LE:16:Protected Mode),
 * decompiled this pass (sole caller Dialog_MenuZ_Modal @ 1220:0156 after
 * Util_StrCopy into the 0x12d8:0x24fa buffer; callees Text_MeasureMenuWidth
 * @ 1100:00ea (was FUN_1100_00ea: SELECTOBJECT + Util_StrLen +
 * GETTEXTEXTENT width) per item):
 *   bVar3 = first-token flag; walk chars: '_' or '\0' ends a token (write
 *   '\0' over the separator in the caller's 24fa buffer). First token =
 *   title: when non-empty sets *max_w = DAT_1418_000e (default 0x96 via
 *   MenuZ exit reset; exact title-width semantics TBD vs GETTEXTEXTENT —
 *   title is NOT measured, items are). Later tokens = radio items: store
 *   far ptr (seg:off pair) into the param_2 ptr array (*out += 4 per item),
 *   measure via Text_MeasureMenuWidth, *max_w = max(*max_w, w), count++.
 *   Cap 0x18 items (return 0x18 early); '\0' ends with count return.
 * Format tekstow menu: '_' separator (title '_' item1 '_' item2 ...;
 *   puste tokeny dozwolone? orig nie filtruje pustych itemow — pusty item
 *   tez dostaje slot + width 0; cap 0x18 tnie ogon). Pozycje: title na gorze
 *   (RanDoMStatic 0x4a6e), items jako RanDoMRadio 500+i wierszami/kolumnami
 *   (DAT_1418_000a cols; OFFSETRECT per item w MenuZ). Hotkeye: BRAK
 *   dedykowanego '&' w tym splicie (menu strings 14b0 "&Save Game" uzywaja
 *   '&' w statycznych tekstach, nie '_' split; '_' to separator, nie hotkey).
 * Kto wola: TYLKO Dialog_MenuZ_Modal @ 1220:0156 (MainMenu_WndProc @
 *   11e0:0221 tekst 1420:435b '_Start a New Game_...' + kazdy
 *   NewGame_Wizard @ 10e8:0000 step (0x2a5a/0x2aa9/0x2ad2) + CustomWorldModal
 *   @ 1220:16fd lista + Dialog_ShowMessage @ 1220:00d2 modal). Zwiazek z
 *   Text_FillResStringList @ 1098:1f4f (sole caller Dialog_InitResStringTables
 *   @ 1068:0295; Res 0x82..0x86 pack z LSTRLEN empty-stop) i SplitMenu:
 *   FillResStringList buduje tabele res-string na init (ResMgr blobs),
 *   SplitMenuText tnie '_' tekst modalny na zywo (MenuZ buffer 24fa);
 *   wizard list plemion buduje recznie StrCopy/Append (NIE Fill), MenuZ tnie.
 * OS-free split ponizej LIVE 1:1 na jawnych buforach (no Win16, no globals,
 * read-only poza dst); GETTEXTEXTENT width + DAT_1418_000e/max tail zostaje
 * caller-side (Text_MeasureMenuWidth @ 1100:00ea GDI). No exit(), C17. */
#define CIV_SPLIT_MAX 0x18
#define CIV_SPLIT_DEFAULT_W 0x96

/* Orig separator test ('_' or '\0') -> dialog_split_is_sep. 1:1. */
int dialog_split_is_sep(int c);

/* Orig item count (tokens after the first/title, cap 0x18) ->
 * dialog_split_menu_count. 1:1 count of '_' after the title segment
 * (title = up to first '_' or '\0'; each later '_' or '\0' closes one item;
 * trailing '\0' always closes the last item, even empty). NULL -> -1.
 * No globals. */
int dialog_split_menu_count(const char *s);

/* Orig '_' -> '\0' copy + ptr pack -> dialog_split_menu_copy. 1:1 copies
 * src (incl. NUL) into dst (cap bytes), replacing each '_' with '\0';
 * fills entries[0..cap_entries) with pointers into dst for the item tokens
 * (tokens after the first/title; title stays dst[0] but is NOT listed).
 * *count_out = item count (same as dialog_split_menu_count, capped by
 * cap_entries). NULL src/dst/entries/count_out -> NOENT (count 0 when
 * count_out non-NULL); cap == 0 or cap_entries == 0 -> TRUNCATED; src NUL
 * beyond cap -> TRUNCATED (dst NUL-terminated, no fake tail); else CIV_OK.
 * Empty items listed (width 0 caller-side). No globals, no exit(). */
civ_error_t dialog_split_menu_copy(const char *src, char *dst, size_t cap,
                                   const char **entries, size_t cap_entries,
                                   size_t *count_out);

/* Orig Dialog_SplitMenuText @ 1220:0000 full (ptr pack + width max via
 * Text_MeasureMenuWidth @ 1100:00ea + DAT_1418_000e title default) ->
 * dialog_split_menu_text. Width/GDI tail -> SDL TBD; stub
 * CIV_ERR_UNSUPPORTED (outs zeroed when non-NULL). Pure split lives in
 * dialog_split_menu_copy above (LIVE). No globals, no exit(). */
civ_error_t dialog_split_menu_text(const char *src, int *count_out,
                                   int *max_w_out);

/* --- Chain 2026-10-16 (27): MenuWidth measure without GDI (task goal 4) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Text_MeasureMenuWidth @ 1100:00ea (was FUN_1100_00ea: SELECTOBJECT font
 *     + Util_StrLen @ 1000:3722 + GETTEXTEXTENT @ 1608:0220 width + SELECT
 *     restore; callee: none beyond GDI (SplitMenuText w opisie mylnie jako
 *     callee — SplitMenuText @ 1220:0000 jest CALLEREM, nie callee)).
 *   Kto mierzy szerokosc menu: Dialog_SplitMenuText @ 1220:0000 per item
 *     (max via ponizszy track-max; sole caller Dialog_MenuZ_Modal @
 *     1220:0156 dla MainMenu_WndProc @ 11e0:0221 tekst 1420:435b + kazdy
 *     NewGame_Wizard @ 10e8:0000 step + CustomWorldModal @ 1220:16fd + Show
 *     Message @ 1220:00d2) + Text_DrawAlignedList @ 1068:258f (was
 *     FUN_1068_258f, renamed this pass: SELECTOBJECT slot DAT_12d8_0024 +
 *     SETBKMODE(1) + GLOBALLOCK param_1 + per-row MeasureMenuWidth +
 *     align (1 = center, 2 = left, 3 = right, same as text_align_x) +
 *     TEXTOUT; callers Dialog_MenuZ_Modal @ 1220:0156 + Text_DrawAlignedList
 *     Free @ 1068:255a (was FUN_1068_255a, renamed: trace + List + GLOBALFREE)).
 *   Zwiazek z text_measure_* (LIVE): Text_MeasureWidth @ 10d8:05b8 =
 *     PackPascal @ 1100:0000 + MeasurePascal @ 1100:00a1 (GETTEXTEXTENT na
 *     buf[0]/buf+1) + (w*10)/0x10 via text_measure_scaled; MeasureMenuWidth
 *     to ten sam GETTEXTEXTENT bez pakowania (C string + StrLen).
 *     Pomiar bez GDI: font_text_width (boot.h, suma dfCharTable-widths,
 *     tracking 0) daje te sama sume advance bez DC; bounding = max + title
 *     default 0x96 (DAT_1418_000e) + cap 0x18 (CIV_SPLIT_MAX). GDI ogon
 *     (SELECTOBJECT/TEXTOUT) zostaje STUB; matematyka max/align ponizej LIVE
 *     1:1 (no Win16, no globals, read-only poza out). No exit(), C17. */

/* Orig SplitMenuText max half (*max_w = max(*max_w, w)) ->
 * dialog_menu_track_max. 1:1 max(cur, w) (ujemne dozwolone, orig nie
 * clampuje; MeasureMenuWidth >= 0 w praktyce). */
int dialog_menu_track_max(int cur_max, int w);

/* Orig SplitMenuText title half (non-empty title -> *max_w = DAT_1418_000e) ->
 * dialog_menu_title_base. 1:1 title_nonempty ? 0x96 (CIV_SPLIT_DEFAULT_W) :
 * cur_max (pusty tytul zostawia max bez zmian). */
int dialog_menu_title_base(int title_nonempty, int cur_max);

/* Orig Text_DrawAlignedList @ 1068:258f align half (1/2/3) ->
 * text_aligned_list_x. 1:1 jak text_align_x @ 1098:0ed1: 1 = center
 * (left+(w-text_w)/2), 2 = left, 3 = right (right-text_w), inne -> -1. */
int text_aligned_list_x(int left, int right, int text_w, int mode);

/* --- Chain 2026-10-16 (28): Text_DrawAlignedList layout (task goal 1) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled + renamed this pass:
 *   Text_DrawAlignedList @ 1068:258f (was FUN_1068_258f: SELECTOBJECT slot
 *     DAT_12d8_0024 + SETBKMODE(1) + GLOBALLOCK param_1 + per-row
 *     Text_MeasureMenuWidth @ 1100:00ea + align 1/2/3 (same as text_align_x
 *     @ 1098:0ed1) + TEXTOUT @ 1608:01cc; callers Dialog_MenuZ_Modal @
 *     1220:0156 WM_PAINT redraw (DAT_1420_54e8 == 0xf, handle local_38,
 *     mode 2 = left) + Text_DrawAlignedListFree @ 1068:255a).
 *   Text_DrawAlignedListFree @ 1068:255a (was FUN_1068_255a: Debug_LogTrace
 *     "TextBoxDrawAndClose ... dialogs" + List(param_1, param_2) +
 *     GLOBALFREE param_1 when != 0).
 *   Text_BuildWrappedList @ 1068:2072 (was FUN_1068_2072: word-wrap builder
 *     of the SAME handle the List draws: GLOBALALLOC 0x51a + GLOBALLOCK +
 *     GETTEXTMETRICS row_h at +0x10 + wrap on spaces/\r via GETTEXTEXTENT vs
 *     width + rows: count at +0x18, len[i] at +0x1a (i*2), off[i] at +0x9a,
 *     text base at +0x11a; returns handle; MenuZ local_38 source).
 * Jakie listy: DWA rozlaczne systemy w MenuZ, oba rysowane tym samym List:
 *   (a) SplitMenuText @ 1220:0000 '_' items (title + radio entries, ptr pack
 *     local_2da, buttons 500+, measure via dialog_menu_track_max/title_base
 *     LIVE) — to sa RADIO buttons, NIE ten handle;
 *   (b) word-wrap handle z Text_BuildWrappedList (tytul/akapit, mode 2 =
 *     left w MenuZ redraw; help/census akapity tym samym formatem).
 * Format wierszy handle: row i: off = u16 at +0x9a + i*2 (offset into text
 *   at +0x11a), len = u16 at +0x1a + i*2; Util_MemMove len bytes into 256 B
 *   stack buf local_11c + buf[len] = 0; w = MeasureMenuWidth(buf); x via
 *   text_aligned_list_x (1 center / 2 left / 3 right); y = top + i * row_h
 *   (row_h = u16 at +0x10, GETTEXTMETRICS tmHeight); TEXTOUT(x, y).
 * Zwiazek z dialog_menu_track_max/title_base (LIVE): tamte licza max width
 *   SplitMenuText items (radio buttons); ten List mierzy SZEROKOSC
 *   word-wrap wierszy tym samym MeasureMenuWidth, ale max NIE wraca do
 *   SplitMenuText (wrap width byl wejsciem buildera, nie wyjsciem List).
 * Port layoutu -> LIVE ponizej (no Win16, no globals, read-only poza out);
 *   SELECTOBJECT/SETBKMODE/GLOBALLOCK/TEXTOUT/GLOBALFREE ogony zostaja
 *   caller-side STUB (text_draw_aligned_list*). No exit(), C17. */

/* Orig List row-y half (local_18 = local_e + i * *(int *)(base+0x10)) ->
 * text_aligned_list_row_y. 1:1 top + i * row_h. No globals. */
int text_aligned_list_row_y(int top, int row_h, int i);

/* Orig List loop gate (*(int *)(base+0x18) <= i -> break) ->
 * text_aligned_list_should_stop. 1:1 nonzero (1) when i >= count (stop),
 * 0 otherwise. count <= 0 stops immediately (orig breaks at i = 0). */
int text_aligned_list_should_stop(int i, int count);

/* Orig List/Free handle gate (param_1 != 0 -> GLOBALUNLOCK/GLOBALFREE) ->
 * text_aligned_list_needs_unlock. 1:1 nonzero (1) when handle != 0. */
int text_aligned_list_needs_unlock(int handle);

/* Orig Text_BuildWrappedList @ 1068:2072 row-cap half (0x51a handle:
 * len u16 table at +0x1a..+0x99 = 0x80 B = 64 rows before the off table at
 * +0x9a; text tail +0x11a..0x519 = 0x400 chars bounds chars, not rows) ->
 * text_wrapped_list_cap. 1:1 64. */
int text_wrapped_list_cap(void);

/* Orig Text_DrawAlignedList @ 1068:258f GDI tail (SELECTOBJECT slot +
 * SETBKMODE(1) + GLOBALLOCK + per-row Measure + TEXTOUT + UNLOCK/SELECT
 * restore). SDL_ttf bridge TBD; stub CIV_ERR_UNSUPPORTED. Layout lives in
 * text_aligned_list_* above (LIVE). No globals, no exit(). */
civ_error_t text_draw_aligned_list(int handle, int mode);

/* Orig Text_DrawAlignedListFree @ 1068:255a (trace + List + GLOBALFREE).
 * Same GDI tail + trace; stub CIV_ERR_UNSUPPORTED. Free gate lives in
 * text_aligned_list_needs_unlock above (LIVE). */
civ_error_t text_draw_aligned_list_free(int handle, int mode);

/* --- Chain 2026-10-16 (29): Text_BuildWrappedList wrap rules (task goal 1) ---
 * Orig Text_BuildWrappedList @ 1068:2072 (civ.exe, x86:LE:16:Protected Mode),
 * decompiled this pass (GLOBALALLOC 0x51a + GLOBALLOCK + GETTEXTMETRICS
 * row_h at +0x10 + wrap vs width local_c = rect_w + rows count at +0x18,
 * len u16 at +0x1a, off u16 at +0x9a, text base at +0x11a; returns handle).
 * Reguly lamania (decompile):
 *   - wiodace spacje pomijane (while *p == ' ' p++);
 *   - slowa zbierane do spacji / '\r' / NUL (wewnetrzna petla do break na
 *     ' '; '\r' konczy wiersz, NUL konczy tekst);
 *   - myflniki NIE lamia (brak '-' gate; tylko ' ' + '\r');
 *   - max-width via GETTEXTEXTENT vs local_c: local_c <= measured -> overflow
 *     (text_wrapped_overflows); wiersz cofa sie do ostatniego breaka;
 *   - trailing spaces ciecie wstecz (while last == ' ' len--);
 *   - pojedyncze slowo szersze niz width i tak zajmuje wiersz (fallback
 *     local_1c = local_14 gdy overflow==0 lub NUL/'\r' lub jedno slowo);
 *   - char remap: byte -0x6b (0x95) -> -5 (0xFB) in-place przed measure
 *     (text_wrapped_char_fix; exact glyph-page TBD, never faked beyond byte);
 *   - '\r' konsumowany jako 1 bajt po wierszu (local_14++ gdy overflow==0).
 * Kto wola (14 xrefs, NE scan): Dialog_MenuZ_Modal @ 1220:050f (tytul/akapit
 *   local_27a gdy non-empty, mode 2 = left w redraw + GLOBALFREE na koncu),
 *   Text_ShowTextByName @ 1068:1dd7 (ResMgr_GetByName civilopedia/help text),
 *   NewGame_CustomHelpDialog @ 10e0:0497 (help customize), Status_FormatMainRow
 *   @ 1138:0370 (status/census akapit z 24fa), City_ChooseProduction @
 *   1170:1abc/1b8b/1de6/1eb5 (production/tech/city akapity x4),
 *   Tech_ChooseResearch @ 1170:2531 (tech dialog), Quiz_RunCivilizationQuiz @
 *   1178:01bb/065a/0731 (quiz CORRECT/INCORRECT/question x3),
 *   NewGame_Wizard @ 10e8:1201 (wizard screens), Text_ShowWrappedBuffer @
 *   11d0:00b0 (24fa buffer box). ShowMessage @ 1220:00d2 NIE wola wprost
 *   (modal 0x88/0x91 via MenuZ, wrap w MenuZ powyzej). Census akapity ida
 *   przez Status_FormatMainRow/ShowTextByName, nie osobny census-wrap.
 * Port regul -> LIVE ponizej (no Win16, no globals, read-only poza out);
 *   GETTEXTEXTENT/GETTEXTMETRICS/GLOBALALLOCpixel tails stay STUB. No exit(),
 *   C17. */

/* Orig wrap char gate (*p == ' ') -> text_wrap_is_space. 1:1. */
int text_wrap_is_space(int c);

/* Orig wrap hard break (*p == '\0' || *p == '\r') -> text_wrap_is_hard_break.
 * 1:1 1 gdy koniec wiersza/teksu, 0 wpp. */
int text_wrap_is_hard_break(int c);

/* Orig in-place byte remap (*p == -0x6b -> -5) -> text_wrapped_char_fix.
 * 1:1 ((char)-0x6b) ? -5 : c (porownanie na signed char, jak orig). */
int text_wrapped_char_fix(int c);

/* Orig width gate (local_c <= GETTEXTEXTENT -> overflow) ->
 * text_wrapped_overflows. 1:1 width <= measured (width = local_c rect_w).
 * Ujemne dozwolone (orig signed int compare). */
int text_wrapped_overflows(int width, int measured);

/* Orig trailing-space backtrack (while [local_1c-1] == ' ' local_1c--) ->
 * text_wrapped_trim_trailing. 1:1 usuwa koncowe spacje z dlugosci wiersza
 * (len sans trailing ' '; start_idx/base dla off+len jak +0x9a/+0x1a).
 * Zwraca przycieta dlugosc (0..len). NULL s -> 0. */
int text_wrapped_trim_trailing(const char *s, int len);

/* Orig leading-space skip (while *p == ' ' p++) -> text_wrapped_skip_leading.
 * 1:1 liczba wiodacych spacji od pos (0 gdy s NULL lub pos OOR). */
int text_wrapped_skip_leading(const char *s, int pos, int n);

/* Orig row store (off = cur - base, len = end - start) -> text_wrapped_row_span.
 * 1:1 off = cur_off - base_off, len via out (NOENT on NULL len_out).
 * RANGE gdy cur < base lub end < start. */
civ_error_t text_wrapped_row_span(int base_off, int cur_off, int start_off,
                                  int end_off, int *off_out, int *len_out);

/* --- Chain 2026-10-16 (30): Text_ShowTextByName + Quiz kernels (task goal 5) ---
 * Orig Text_ShowTextByName @ 1068:1dd7 (ResMgr_GetByName + GLOBALALLOC 0x42 +
 * MemCopyFar + Text_BuildWrappedList @ 1068:2072 + DrawAlignedListFree; indeks:
 * NIE numer — NAZWA zasobu (ResMgr_GetByName, cywilopedia: jaki tekst? ten
 * ktory Pedia_ShowEntry @ 1068:06af poda: tech/building/unit/terrain long-desc
 * gdy (g_nOptionFlags & 0x40) != 0; param_3 == 4 (terrain pomoc?) omija long).
 * Civilopedia? TAK — Pedia_ShowEntry to viewer civilopedii (switch param_3:
 * 1 tech (+0x44c icon), 2 building/wonder (0xd07/0xd08), 3 unit (+0x4b0), 4
 * terrain yields, 5 pusty); ShowTextByName rysuje dlugi opis pod ikona.
 * Orig Quiz_RunCivilizationQuiz @ 1178:0022 (CIVILIZATION QUIZ @ 1420:37d4 +
 * Dialog_MenuZ_Modal @ 1220:0156; kiedy? Quiz_RunQuizPair @ 1178:0000 z
 * Game_TurnLoopTick @ 1088:0000: tryb 1 (nagroda?) potem retry 0 gdy Pudlo;
 * easter egg? NIE — quiz tech-tree (nagroda: DAT_12d0_00b6 |= 1 gdy dobrze;
 * pudlo w trybie 0: DAT_13b8_0000 = 1 + FoundCityIfHome resettle wszystkich
 * jednostek gracza; koniec gry? NIE — srodek gry, tech-check). MenuZ z
 * 12 odpowiedzi (local_18 < 0xc, pre-chain via 1894/1895 jak Tech_Choose).
 * Port ponizej LIVE 1:1 (no Win16, no globals); GDI/MenuZ ogony STUB. */

/* Orig ShowTextByName layout (*param_4 = *(+0x10)* *(+0x18) + *(+2)) ->
 * text_wrapped_total_height. 1:1 y0 + row_h * n_rows. */
int text_wrapped_total_height(int row_h, int n_rows, int y0);

/* Orig Pedia long-desc gate ((opt & 0x40) != 0) -> text_show_needs_long_desc.
 * 1:1 1 gdy pokazac dlugi opis (terrain kind 4 caller-side omija). */
int text_show_needs_long_desc(int opt_flags);

/* Orig Quiz compare (local_e == local_10) -> quiz_answer_is_correct. 1:1. */
int quiz_answer_is_correct(int picked, int correct);

/* Orig Quiz pair retry (pierwszy == 0 -> drugi z 0) -> quiz_should_retry.
 * 1:1 1 gdy retry (tryb 1 pudlo). */
int quiz_should_retry(int first_result);

/* Orig Quiz tech-pick (pre1 != -1 && pre2 != -1 && pre1 != 0x7f) ->
 * quiz_tech_is_valid. 1:1 (0x7f = terminator lancucha pre, -1 = brak). */
int quiz_tech_is_valid(int pre1, int pre2);

/* --- Chain 2026-10-16 (task): Quiz reward/resettle (goal 3) ---
 * Orig Quiz_RunQuizPair @ 1178:0000 (caller Game_TurnLoopTick @ 1088:04d3:
 *   Quiz(1), pudlo -> Quiz(0)) + Quiz_RunCivilizationQuiz @ 1178:0022:
 *   dobrze (local_e == local_10) -> DAT_12d0_00b6 |= 1 (nagroda);
 *   pudlo w trybie 0 -> DAT_13b8_0000 = 1 + Unit_FoundCityIfHome resettle
 *   wszystkich jednostek gracza + DAT_13b8_0000 = 0.
 * OS-free ponizej LIVE 1:1 (no Win16, no globals); MenuZ/GDI ogony STUB. */

/* Orig Quiz_RunCivilizationQuiz @ 1178:0022 nagroda -> quiz_reward_flags.
 * 1:1 cur | 1 (DAT_12d0_00b6 |= 1). */
int quiz_reward_flags(int cur);

/* Orig Quiz_RunCivilizationQuiz @ 1178:0022 pudlo -> quiz_needs_resettle.
 * 1:1 mode == 0 && !correct (resettle + 13b8 guard). */
int quiz_needs_resettle(int mode, int correct);

/* Orig Quiz_RunQuizPair @ 1178:0000 pierwszy tryb -> quiz_pair_first_mode.
 * 1:1 1 (retry 0 w quiz_should_retry). */
int quiz_pair_first_mode(void);

/* --- Chain 2026-10-16 (p5): Pedia dispatcher shape (goal 3) ---
 * Orig Pedia_ShowByCategory @ 1020:0000 (civ.exe, x86:LE:16:Protected Mode),
 * decompiled this pass (body 0000-014e; Dlg_CreateTracked(0x82, 0x61) +
 * FRAMERECT + switch sel 0..4 -> Res 0x82..0x86 tytul + PascalToCStr +
 * Pedia_ShowEntry + Dlg_DestroyTracked):
 *   sel 0 -> Res 0x82 (tech; entry kind 1: +0x44c, Requires/Allows/units).
 *   sel 1 -> Res 0x83 (improvements/wonders; kind 2: param+1 < 0x19 ? 2 : 3,
 *     0xd07/0xd08 via Tech_HasTech(0x25)).
 *   sel 2 -> Res 0x84 (units/produce; kind 3: +0x4b0, Cost/Attack/Defense).
 *   sel 3 -> Res 0x85 (terrain, 12 wpisow 1..0xc; kind 4: index via
 *     Pedia_FindTerrainIndex @ 1020:011b scan 0..0xb, 0xffff gdy brak;
 *     +0x3f5/6, food/prod/trade + movement + defense%).
 *   sel 4 -> Res 0x86 (governments; kind 5: pusty, local_8 = 0).
 * Kategorie: tech / buildings+wonders / units / terrain / governments.
 * Zwiazek z Pedia_ShowEntry @ 1068:06af switch 1..5 (kind = sel + 1):
 *   tytul Res (0x82+sel) + tresc kind (pedia_entry_kind_valid w game.h).
 *   Res-stringi via Dialog_InitResStringTables @ 1068:0295 +
 *   Text_FillResStringList @ 1098:1f4f (0x82 tech 1..0x43 / 0x83 build
 *   2..999 / 0x84 produce 1..999 / 0x85 1..0xc / 0x86 1..999).
 * Zwiazek z Tech_GrantTech @ 1170:2aaa (player-tech -> Pedia_ShowByCategory
 *   (tech, 0) civilopedia-popup po odkryciu).
 * OS-free ksztalt ponizej LIVE 1:1 (no Win16, no globals, read-only);
 * Dlg_CreateTracked/GDI ogon STUB z TODO Orig. No exit(), C17. */

/* Orig sel 0..4 gate -> pedia_selector_valid. 1:1. */
int pedia_selector_valid(int sel);

/* Orig Pedia_ShowByCategory @ 1020:0000 kind = sel + 1 (switch 1..5) ->
 * pedia_kind_from_selector. 1:1 sel 0..4 -> 1..5, else -1. */
int pedia_kind_from_selector(int sel);

/* Orig Pedia_ShowByCategory @ 1020:0000 title Res select (sel 0..4 ->
 * Res 0x82..0x86: 0 tech, 1 build, 2 produce, 3 terrain, 4 gov) ->
 * pedia_res_from_selector. 1:1 0x82 + sel, else -1. Same Res ids as
 * dialog_res_list_spec @ 1068:0295 (0x82 tech 1..0x43 / 0x83 build 2..999 /
 * 0x84 produce 1..999 / 0x85 gov 1..0x0c / 0x86 unit 1..999). No globals. */
int pedia_res_from_selector(int sel);

/* Orig Dlg_CreateTracked @ 1020:0000 (0x82, 0x61) -> pedia_dlg_id.
 * 1:1 0x82. */
int pedia_dlg_id(void);
/* Orig Dlg_CreateTracked @ 1020:0000 (0x82, 0x61) -> pedia_dlg_flags.
 * 1:1 0x61. */
int pedia_dlg_flags(void);

/* --- Chain 2026-10-16 (31): dialog STUB-split OS-free gates (task) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), 1:1 ports of pure branches
 * documented inside the STUB bodies below (no Win16, no globals, no exit(),
 * read-only, C17). The handle/modal/GDI tails stay STUB with TODO Orig:
 *   Dialog_MenuZ_Modal @ 1220:0156 (PEEKMESSAGE/ISDIALOGMESSAGE pump +
 *     CREATEWINDOW radio fan-out -> dialog_menuz STUB),
 *   Dlg_CreateTracked @ 1098:124f (CREATEDIALOG + MAKEPROCINSTANCE(DLGPROC @
 *     1098:16e2 / CIVDIALOGPROC @ 1098:1838) + palette copy + tracked-stack
 *     push @ 1420:1ea0/1ec8 -> dlg_create_tracked STUB; flag/id math LIVE in
 *     dlg_create_should_center/show + dlg_ctl_is_tracked_init +
 *     dlg_tracked_push_off/pop + dlg_id_needs_preset_size above),
 *   Diplo_PostContact @ 1168:0a07 (contact bits @ 12d8:3a0a + dialogs ->
 *     diplomacy_post_contact STUB; bit math LIVE in diplo_* above),
 *   Diplo_OpenContactDialog @ 1160:0000 (MenuZ tribute/peace/war branches ->
 *     diplo_open_contact_dialog STUB; tribute math LIVE above),
 *   UI_DrawEndTurnPrompt @ 10d8:00e0 ("End of Turn" string in civ.exe dump
 *     + sprite blink in unit_wait_for_orders @ 11c0:0ec2 on the
 *     Sys_GetTickScaled % 0x3c 0x1e-phase clock -> ui_draw_end_turn_prompt
 *     STUB; phase gate LIVE below),
 *   Text_DrawAligned @ 1098:0ed1 (SELECTOBJECT + SETBKMODE/SETTEXTALIGN +
 *     shadow TEXTOUT x3 + TEXTOUT -> text_draw_aligned STUB; x/y math LIVE
 *     in text_align_x/text_center_y above, shadow gate LIVE below),
 *   Dialog_ShowMessage @ 1220:00d2 (Dlg_CreateTracked 0x88/0x91 +
 *     Dialog_MenuZ_Modal + Dlg_DestroyTracked + Dialog_RefreshViews @
 *     1220:0eb8 SENDMESSAGE 0xf to 12b0:0004/001c/0034 -> dialog_show_message
 *     STUB; id math LIVE in dialog_show_message_id above),
 *   Anim_AdvanceFrames @ 10b0:0461 (timed opcode switch 1..0xe ->
  *   anim_advance_frames PARTIAL dispatcher; decoder LIVE in anim_opcode_* above,
 *     caption case 7 LIVE below).
 * Refs verified read-only 2026-10-16: unpacked/civ.exe strings
 *   ("RanDoMRadio"/"RanDoMStatic"/"RanDoMUser", "RANDOMRADIOPROC"/
 *   "RANDOMSTATICPROC"/"RANDOMUSERPROC", "civhelp.hlp", "End of Turn") +
 *   unpacked/civhelp.hlp 72787 B (magic 3F 5F 03 00 + version 0x46, see
 *   help_hlp_system above). */

/* Orig Dlg_CreateTracked @ 1098:124f earth-custom extra loop (0x8f adds
 * 0x323-0x332, see dlg_create_tracked STUB) -> dlg_ctl_is_earth_extra.
 * 1:1 0x323 <= id <= 0x332 (GETDLGITEM-guarded init, sibling of
 * dlg_ctl_is_tracked_init 0x65..0x6e above). No globals. */
int dlg_ctl_is_earth_extra(int ctl_id);

/* Orig Diplo_PostContact @ 1168:0a07 AI-AI contact gate ((civA+civB+turn)&3
 * == 0 or first contact without 0x08, see diplomacy_post_contact STUB) ->
 * diplo_ai_contact_gate. 1:1 nonzero (1) when (a+b+turn)&3 == 0 (periodic
 * AI-AI evaluation slot), 0 otherwise. No globals. */
int diplo_ai_contact_gate(int civ_a, int civ_b, int turn);

/* Orig Diplo_OpenContactDialog @ 1160:0000 senate override (gov < 4 forces
 * the peace message, see chain-7 block) -> diplo_senate_forces_peace.
 * 1:1 nonzero (1) when 0 <= gov < 4, 0 otherwise. No globals. */
int diplo_senate_forces_peace(int gov);

/* Orig UI_DrawEndTurnPrompt @ 10d8:00e0 blink phase (alternates the
 * "End of Turn / Press Return" text with the unit sprite blink in
 * unit_wait_for_orders @ 11c0:0ec2 on the Sys_GetTickScaled % 0x3c
 * 0x1e-phase clock) -> ui_end_turn_phase. 1:1 nonzero (1) in the text
 * phase ((tick % 0x3c) < 0x1e), 0 in the sprite phase. No globals. */
int ui_end_turn_phase(long tick);

/* Orig Text_DrawAligned @ 1098:0ed1 shadow gate (shadow TEXTOUT x3 via
 * Text_SetColor @ 1098:0e0b when DAT_12d8_28fc == 0 and format & 0x30,
 * see text_draw_aligned STUB) -> text_needs_shadow. 1:1 nonzero (1) when
 * mono == 0 (DAT_12d8_28fc color/VGA, sibling of text_resolve_color @
 * 10d8:00e0 above) and (format & 0x30) != 0, 0 otherwise. No globals. */
int text_needs_shadow(int mono, int format);

/* Orig Dialog_RefreshViews @ 1220:0eb8 (tail of Dialog_ShowMessage @
 * 1220:00d2, see dialog_show_message STUB) -> dialog_refresh_view_count /
 * dialog_refresh_msg. 1:1 3 views (12b0:0004/001c/0034) / message 0xf. */
int dialog_refresh_view_count(void);
int dialog_refresh_msg(void);

/* Orig Anim_AdvanceFrames @ 10b0:0461 case 7 (caption-text: palette apply +
 * ResMgr_GetString @ 1100:078a / Util_StrCopy @ 1000:36b2 +
 * Text_DrawAligned @ 1098:0ed1 + PresentRect @ 10d0:0854, both branches
 * StoreDC via Gfx_StoreDC @ 1098:1104, see anim_opcode_needs_store) ->
 * anim_opcode_is_text. 1:1 op == 7. */
int anim_opcode_is_text(int op);

/* --- Chain 2026-10-16 (34): Anim timed-tail gates (P0) ---
 * Orig Anim_AdvanceFrames @ 10b0:0461 timed tails (civ.exe,
 * x86:LE:16:Protected Mode), decompiled 2026-10-16 (chain-22 switch
 * 1..0xe; GfxPort/Art/ResMgr tails stay in the anim_advance_frames
 * PARTIAL dispatcher above). OS-free gates below are LIVE 1:1 on explicit
 * op (no Win16, no globals, read-only, no exit(), C17); the timed
 * GfxPort_SetActive @ 10d0:0956 / PresentRect @ 10d0:0854 +
 * Palette_AnimateRange @ 1090:0376 + Sound_PlayId @ 1150:00c9 bridges stay
 * STUB in anim_advance_frames / anim_pump_if_active / anim_open_sequence
 * (PARTIAL dispatchers with TODO Orig 10b0:0461/008a/0000).
 * Refs verified read-only 2026-10-16: strings unpacked/civ.exe
 * ("OPENING.wav", "RanDoMRadio"/"RanDoMStatic"/"RanDoMUser").
 *   SetActive @ 10d0:0956 cases: 5 (port-switch +0x12/+0x31), 7 (caption),
 *     9 (swap), 11 (scroll x2), 12/13 (dissolve x2).
 *   PresentRect @ 10d0:0854 cases: 7 (undraw tail), 9, 11, 12, 13 (5 is
 *     switch-only, no present; 8 fills via FILLRECT; 6 loads via Art).
 *   AnimateRange @ 1090:0376 cases: 1 (fade-zero), 12 (0xc pre-copy branch
 *     + Animate), 14 (rotate + Animate). Op 2 uses Palette_CopyRange @
 *     1098:201d, not Animate (covered by anim_opcode_is_palette).
 *   Sound_PlayId @ 1150:00c9: no opcode plays sound inside (slots 3/4 run
 *     silent; OPENING.wav + (3,0)/(1,0) ride caller-side in
 *     Main_RunStartupSequence @ 11e0:0000, see boot.h). */

/* Orig timed PresentRect tail (@ 10d0:0854 in cases 7/9/11/12/13) ->
 * anim_opcode_needs_present. 1:1 nonzero (1) for 7/9/11/12/13, else 0. */
int anim_opcode_needs_present(int op);

/* Orig timed AnimateRange tail (@ 1090:0376 in cases 1/12/14) ->
 * anim_opcode_needs_animate. 1:1 nonzero (1) for 1/12/14, else 0. */
int anim_opcode_needs_animate(int op);

/* Orig sound invariant (no Sound_PlayId @ 1150:00c9 inside any opcode
 * 1..0xe; caller-side only) -> anim_opcode_needs_sound. 1:1 always 0
 * (documents the no-sound tail; caller plays (3,0)/(1,0) + OPENING.wav). */
int anim_opcode_needs_sound(int op);

/* --- Chain 2026-10-16 (32): tracked/anim/diplo/menu/text gates (task) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), 1:1 OS-free ports of pure
 * branches documented inside the STUB bodies below (no Win16, no globals,
 * no exit(), read-only, C17). Handle/modal/GDI tails stay STUB with TODO
 * Orig (dialog_menuz @ 1220:0156, dlg_create_tracked @ 1098:124f,
 * anim_open_sequence @ 10b0:008a, anim_advance_frames @ 10b0:0461,
 * diplomacy_post_contact @ 1168:0a07, text_draw_aligned @ 1098:0ed1).
 * Refs verified read-only 2026-10-16: unpacked/civ.exe strings
 * ("RanDoMRadio"/"RanDoMStatic"/"RanDoMUser", "RANDOMRADIOPROC"/
 * "RANDOMSTATICPROC"/"RANDOMUSERPROC", "Can't initialize timer in MenuZ" @
 * windows.c, "End of Turn"/"Press Return") + unpacked/civhelp.hlp 72787 B
 * (magic 3F 5F 03 00 + version 0x46, see help_hlp_system above). */
#define CIV_DLG_PRESET_W 0x200
#define CIV_DLG_PRESET_H 0x140
#define CIV_ANIM_SEQ_CTX_SIZE 0xa6

/* Orig Dlg_CreateTracked @ 1098:124f preset-size branch (param_1 == 0x94
 * || == 0x8f -> DAT_12d8_0026 = 1 + 002e = 0x200 + 002c = 0x140; see
 * dlg_id_needs_preset_size + dlg_create_tracked STUB) -> dlg_preset_w/h.
 * 1:1 preset width/height for 0x94 (intro/PIC host) / 0x8f (earth-custom),
 * -1 for any other id (MOVEWINDOW path). No globals. */
int dlg_preset_w(int dlg_id);
int dlg_preset_h(int dlg_id);

/* Orig Anim_OpenSequence @ 10b0:008a count half (param_3 == 0 -> count recs
 * until opcode 0 via anim_seq_count_frames, stride 0xf words; else param_3;
 * see anim_open_sequence STUB) -> anim_seq_resolve_count. 1:1 param_3 != 0
 * ? param_3 : counted (counted is the anim_seq_count_frames result,
 * negative propagates as-is). No globals. */
int anim_seq_resolve_count(int param_3, int counted);

/* Orig Anim_OpenSequence @ 10b0:008a ctx alloc (mem_alloc_lock 0xa6 +
 * DAT_1420_1f40 = ctx; see anim_open_sequence STUB) -> anim_seq_ctx_size.
 * 1:1 0xa6 (sibling of CIV_DLG_TRACKED_CTX_SIZE 0x18 + ANIM_SLOT_SIZE
 * 0x14 + ANIM_DISSOLVE_CTX_SIZE 0x79c above). No globals. */
int anim_seq_ctx_size(void);

/* Orig Anim_OpenSequence @ 10b0:008a GfxPort pair (GfxPort_Create x2 at
 * ctx offs +9/+0x62; see anim_open_sequence PARTIAL dispatcher) ->
 * anim_seq_port_count / anim_seq_port_off. 1:1 two ports; slot 0 -> 9,
 * slot 1 -> 0x62, else -1 (close frees the same x2 pair at +0x24/+0x62
 * per the anim_close_sequence comment; exact slot unit TBD vs dump).
 * No Win16 (handles stay in the GfxPort_Create @ 10d0:0000 SDL bridge,
 * see video.h), no globals. */
#define ANIM_SEQ_PORT_OFF_0 9
#define ANIM_SEQ_PORT_OFF_1 0x62
int anim_seq_port_count(void);
int anim_seq_port_off(int slot);

/* Orig Anim_AdvanceFrames @ 10b0:0461 timed cases 11 (scroll-blit) / 14
 * (palette-rotate): (tick - start) % len == 0 gate (SetActive/CopyRange
 * tails stay in the anim_advance_frames PARTIAL dispatcher above) ->
 * anim_tick_is_due. 1:1 nonzero (1) when due, 0 when not; len <= 0 -> -1 (orig divided; here a hard
 * stop). No globals. */
int anim_tick_is_due(long start, long tick, int len);

/* Orig Diplo_PostContact @ 1168:0a07 contacted-fresh gate (bit 0x01 with
 * the 392b+3 turn stamp; > 0xf turns since -> stale, re-contact via
 * Diplo_ShouldMakePeace @ 1168:0f47 path; fresh skips the player dialog;
 * see diplomacy_post_contact STUB) -> diplo_contact_stamp_due. 1:1 nonzero
 * (1) when turn - stamp > 0x0f (stale), 0 when fresh. No globals. */
int diplo_contact_stamp_due(int turn, int stamp);

/* Orig Dialog_MenuZ_Modal @ 1220:0156 radio disable (RanDoMRadio 500+i
 * DISABLE when the DAT_12d8_24da bit for i is set; preselect skips disabled
 * via local_44+1; see dialog_menuz STUB) -> dialog_menuz_item_disabled.
 * 1:1 nonzero (1) when bit index is set in mask, 0 when clear; index OOR
 * (< 0 or >= 32, int shift width) -> -1. No globals. */
int dialog_menuz_item_disabled(int mask, int index);

/* Orig same radio fan-out (CREATEWINDOW RanDoMRadio 500+i for i < count,
 * cap CIV_SPLIT_MAX 0x18; see dialog_menuz STUB + dialog_menuz_id_to_index)
 * -> dialog_menuz_id_is_valid. 1:1 nonzero (1) when 500 <= button_id <
 * 500 + count, 0 otherwise (count <= 0 -> 0). No globals. */
int dialog_menuz_id_is_valid(int button_id, int count);

/* Orig Text_DrawAligned @ 1098:0ed1 shadow passes (shadow TEXTOUT x3 via
 * Text_SetColor @ 1098:0e0b when text_needs_shadow above; final TEXTOUT is
 * the 4th draw; see text_draw_aligned STUB) -> text_shadow_passes. 1:1 3
 * (sibling of dialog_frame3d_layers/dialog_refresh_view_count constants).
 * No globals. */
int text_shadow_passes(void);

/* --- Chain 2026-10-16 (33): tracked/MenuZ/help/res/diplo/split consts+gates.
 * Orig (civ.exe, x86:LE:16:Protected Mode), 1:1 OS-free ports of pure
 * constants/branches documented inside the STUB bodies below (no Win16, no
 * globals, no exit(), read-only, C17). Handle/modal/GDI tails stay STUB with
 * TODO Orig (dialog_menuz @ 1220:0156, dlg_create_tracked @ 1098:124f,
 * dialog_open_help/help_open_topic @ 1160:xxxx, text_fill_res_string_list @
 * 1098:1f4f, diplo_open_contact_dialog @ 1160:0000 + Diplo_ProcessTurn @
 * 1168:0000, dialog_split_menu_text @ 1220:0000).
 * Refs verified read-only 2026-10-16: unpacked/civ.exe strings
 * ("MenuZ()  :  windows.c" + "Can't initialize timer in MenuZ",
 * "RanDoMRadio"/"RanDoMStatic"/"RanDoMUser", "civhelp.hlp",
 * "End of Turn"/"Press Return") + unpacked/civhelp.hlp 72787 B
 * (magic 3F 5F 03 00 + version 0x46, see help_hlp_system above). */
#define CIV_DLG_MENUZ_TIMER_MS 100
#define CIV_DIALOG_MENUZ_CANCEL -1
#define CIV_DIPLO_TRIBUTE_UNIT 0x32

/* Orig Dlg_CreateTracked @ 1098:124f ctx alloc (mem_alloc_lock 0x18 ctx +
 * push HWND/ctx onto the tracked stack @ 1420:1ea0, count @ 1420:1ec8; see
 * dlg_create_tracked STUB + CIV_DLG_TRACKED_CTX_SIZE 0x18) ->
 * dlg_tracked_ctx_size. 1:1 0x18 (sibling of anim_seq_ctx_size 0xa6 +
 * ANIM_SLOT_SIZE 0x14 + ANIM_DISSOLVE_CTX_SIZE 0x79c). No globals. */
int dlg_tracked_ctx_size(void);

/* Orig same palette copy (GETDC/SELECTPALETTE/REALIZEPALETTE, 0xec entries
 * via Palette_CopyRange @ 1098:201d; see dlg_create_tracked STUB +
 * CIV_DLG_TRACKED_PAL_COUNT 0xec) -> dlg_tracked_pal_count. 1:1 0xec.
 * No globals. */
int dlg_tracked_pal_count(void);

/* Orig Dialog_MenuZ_Modal @ 1220:0156 timer (SETTIMER 100; 0 = 'Can't
 * initialize timer in MenuZ' @ 1420:4a41 via FUN_1098_28e4; see
 * dialog_menuz STUB) -> dialog_menuz_timer_ms. 1:1 100 (ms tick for the
 * modal PEEKMESSAGE/ISDIALOGMESSAGE pump). No globals. */
int dialog_menuz_timer_ms(void);

/* Orig same modal cancel code (DAT_12b0_00a0 == -1 exit + DAT_1418_0004 =
 * cancel/back, sibling of dialog_menuz_is_cancel; see dialog_menuz STUB)
 * -> dialog_menuz_cancel_code. 1:1 -1. No globals. */
int dialog_menuz_cancel_code(void);

/* Orig dialog_open_help/help_open_topic @ 1160:xxxx topic gate (topic NULL
 * = contents page, non-NULL = NUL-terminated topic/context id; see
 * dialog_open_help STUB + help_open_topic wrapper) ->
 * help_topic_is_contents. 1:1 nonzero (1) when topic == NULL, 0 otherwise.
 * No globals, read-only (never writes civhelp.hlp). */
int help_topic_is_contents(const char *topic);

/* Orig Text_FillResStringList @ 1098:1f4f empty-stop (LSTRLEN empty check:
 * first empty string returns NULL, else the tail cursor; see
 * text_fill_res_string_list STUB + text_res_list_advance cursor half) ->
 * text_res_list_is_stop. 1:1 nonzero (1) when entry_len == 0 (stop), 0
 * otherwise. No globals. */
int text_res_list_is_stop(int entry_len);

/* Orig Diplo_ProcessTurn @ 1168:0000 tribute unit + Diplo_OpenContactDialog
 * @ 1160:0000 difficulty/afford/mercenary/threat branches (clamp(..)*0x32,
 * (gold/0x32)*0x32, see diplo_tribute_amount/diplo_contact_tribute_scaled/
 * diplo_contact_afford_snap/diplo_mercenary_amount/diplo_threat_gold LIVE)
 * -> diplo_tribute_unit. 1:1 0x32 (gold rounding/demand unit). No globals. */
int diplo_tribute_unit(void);

/* Orig Dialog_SplitMenuText @ 1220:0000 title default (*max_w =
 * DAT_1418_000e, default 0x96 via the MenuZ exit reset; sibling of
 * dialog_menu_title_base; see dialog_split_menu_text STUB +
 * CIV_SPLIT_DEFAULT_W 0x96) -> dialog_split_default_w. 1:1 0x96.
 * No globals. */
int dialog_split_default_w(void);

/* --- Chain 2026-10-16 (35): dialog top-STUB OS-free kernels (task) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), 1:1 OS-free ports of pure
 * branches/constants still buried in the 25 STUB bodies below (no Win16,
 * no globals, no exit(), read-only except caller-owned outs, C17).
 * Handle/modal/GDI tails stay STUB/PARTIAL with TODO Orig
 * (dialog_menuz @ 1220:0156, dlg_create_tracked @ 1098:124f,
 * dialog_open_help/help_open_topic @ 1160:xxxx, diplomacy_post_contact @
 * 1168:0a07, dialog_get_item_rect @ 1100:03ff, dialog_show_message @
 * 1220:00d2, dialog_draw_frame3d @ 1220:0d38, text_draw star @ 10d8:046e).
 * Refs verified read-only 1:1 vs unpacked/civ.exe (NE LE, no copy):
 *   "MenuZ()  :  windows.c" + "Can't initialize timer in MenuZ" @
 *   1420:4a41 (MenuZ timer 100 / cancel -1), "RanDoMRadio"/"RanDoMStatic"/
 *   "RanDoMUser" (500-based radio ids + '_' split buffer @ 0x12d8:0x24fa),
 *   "End of Turn"/"Press Return" (0x3c/0x1e blink, see ui_end_turn_phase),
 *   "civhelp.hlp" + unpacked/civhelp.hlp 72787 B magic 3F 5F 03 00 +
 *   version 0x46 (see help_hlp_system). MenuZ ID/timer/cancel + split +
 *   frame3D layers + diplo gates + text shadow families only; no fake. */

/* Orig Dialog_MenuZ_Modal @ 1220:0156 radio base (RanDoMRadio 500+i
 * CREATEWINDOW fan-out; DAT_1418_0002 preselect; DAT_12d8_24da disable
 * mask) -> dialog_menuz_base_id. 1:1 500 (CIV_DIALOG_MENUZ_BASE_ID).
 * No globals. */
int dialog_menuz_base_id(void);

/* Orig same fan-out end (i < count, cap CIV_SPLIT_MAX 0x18 via
 * dialog_split_menu_count) -> dialog_menuz_last_id. 1:1 500 + count - 1
 * (last radio id); count <= 0 -> -1. No globals. */
int dialog_menuz_last_id(int count);

/* Orig Dialog_GetItemRect @ 1100:03ff unit flag (param_7 != 0 ->
 * Dialog_DlgUnitsToPixels @ 1098:0d43 + MOVEWINDOW; sole MenuZ callers
 * Dialog_MenuZ_Modal @ 1220:0156 0x67/0x68 + Dialog_ShowMessage path) ->
 * dialog_get_item_rect_needs_scale. 1:1 nonzero (1) when flag != 0.
 * No globals. */
int dialog_get_item_rect_needs_scale(int flag);

/* Orig Dialog_SplitMenuText @ 1220:0000 title half (first '_'/'\0' ends
 * the title into the 0x12d8:0x24fa buffer; title NOT measured, items are
 * via Text_MeasureMenuWidth @ 1100:00ea) -> dialog_split_title_len.
 * 1:1 bytes before the first '_' or NUL (empty title -> 0). NULL -> -1.
 * No globals. */
int dialog_split_title_len(const char *s);

/* Orig same title-empty branch (empty first token leaves *max_w alone,
 * non-empty sets DAT_1418_000e 0x96 via dialog_menu_title_base) ->
 * dialog_split_is_title_empty. 1:1 nonzero (1) when first char is '_'
 * or '\0' (empty title), 0 otherwise; NULL -> -1. No globals. */
int dialog_split_is_title_empty(const char *s);

/* Orig Dialog_DrawFrame3D @ 1220:0d38 head (MemCopyFar rect + right/
 * bottom -1 + INFLATERECT(3,3), then 3x INFLATERECT(-1,-1) layers via
 * dialog_frame3d_layer) -> dialog_frame3d_base_inset / edge_trim. 1:1
 * 3 (inflate dx/dy) / 1 (right/bottom trim). No globals. */
int dialog_frame3d_base_inset(void);
int dialog_frame3d_edge_trim(void);

/* Orig same head rect math -> dialog_frame3d_base. 1:1 base = trim then
 * inflate: out = (l-3, t-3, (r-1)+3, (b-1)+3) = (l-3, t-3, r+2, b+2)
 * (INFLATERECT(dx,dy): l-=dx, t-=dy, r+=dx, b+=dy). NULL -> NOENT;
 * inverted/degenerate out (r <= l || b <= t) -> RANGE. No globals. */
civ_error_t dialog_frame3d_base(const dlg_rect_t *rc, dlg_rect_t *out);

/* Orig Diplo_PostContact @ 1168:0a07 first-contact gate (contact without
 * 0x08 skips ShouldMakePeace @ 1168:0f47 evaluation; AI-AI periodic slot
 * via diplo_ai_contact_gate; bits @ 12d8:3a0a u16 stride 0x10) ->
 * diplo_has_treaty. 1:1 nonzero (1) when (flags & 0x08) != 0.
 * No globals. */
int diplo_has_treaty(int flags);

/* Orig Diplo_PostContact @ 1168:0a07 / Diplomacy_EvalBribe @ 1078:455c
 * arg contract (foreign civ + contacting/diplomat unit index into the
 * 1420:62fc unit records; player-exclusion + visibility tails stay
 * caller-side) -> diplomacy_contact_args_ok. 1:1 nonzero (1) when
 * civ >= 0 && unit_idx >= 0 (no upper clamp: max civ/unit counts live
 * in game.h tables, never invented here). No globals. */
int diplomacy_contact_args_ok(int civ, int unit_idx);

/* Orig Text_DrawAligned @ 1098:0ed1 slot half (decompiler uVar1 =
 * param_4 & 0xf; 1 center / 2 left / 3 right via text_align_x; 0x30
 * shadow bits via text_needs_shadow; color via Text_SetColor @
 * 1098:0e0b) -> text_align_slot. 1:1 format & 0x0f. No globals. */
int text_align_slot(int format);

/* Orig Text_DrawShadowed @ 10d8:02df wrapper param (Text_DrawAt @
 * 10d8:01fa passes 0 = no shadow; shadow TEXTOUT at (x+dx,y+1) via
 * text_shadow_pos + dx via text_shadow_offset, gated by mono
 * DAT_12d8_28fc via text_needs_shadow/text_resolve_color) ->
 * text_shadowed_needs_shadow. 1:1 nonzero (1) when mono == 0 and
 * shadow_flag != 0. No globals. */
int text_shadowed_needs_shadow(int mono, int shadow_flag);

/* --- Chain 2026-10-16 (36): Dlg tracked preset/ctx/ctl/close/rebase tails ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), 1:1 OS-free ports of pure
 * branches/constants in Dlg_CreateTracked @ 1098:124f /
 * Dlg_DestroyTracked @ 1098:1612 (no Win16, no globals, no exit(),
 * read-only, C17). Handle/modal/GDI tails stay PARTIAL/void-NOOP with
 * TODO Orig in dlg_create_tracked / dlg_destroy_tracked below
 * (CREATEDIALOG + MAKEPROCINSTANCE(DLGPROC @ 1098:16e2 / CIVDIALOGPROC @
 * 1098:1838) + GETDC/SELECTPALETTE/REALIZEPALETTE + tracked-stack store @
 * 1420:1ea0/1ec8 + Gfx_StoreDC @ 1098:1104 + SHOWWINDOW + 0xec palette copy
 * via Palette_CopyRange @ 1098:201d + DESTROYWINDOW/FREEPROCINSTANCE/
 * GLOBALFREE + Mem_DisposePtr @ 1100:060b + refresh via FUN_1208_14d5 ->
 * SDL3 bridge TBD). Refs verified read-only 1:1 vs unpacked/civ.exe (NE
 * LE, no copy): "RanDoMRadio"/"RanDoMStatic"/"RanDoMUser" + "OPENING.wav"
 * + "MenuZ()  :  windows.c" + "Can't initialize timer in MenuZ". No fake. */

/* Orig Dlg_CreateTracked @ 1098:124f preset-size store (param_1 == 0x94
 * || == 0x8f -> DAT_12d8_0026 = 1, sibling of 002e = 0x200 via
 * dlg_preset_w + 002c = 0x140 via dlg_preset_h; see dlg_create_tracked
 * PARTIAL) -> dlg_preset_flag_value. 1:1 1 (the stored flag word).
 * No globals. */
int dlg_preset_flag_value(void);

/* Orig same ctl-init loop (iVar6 = 0x65; while (<= 0x6e), GETDLGITEM-guarded
 * SENDMESSAGE 0x30 per item; gate in dlg_ctl_is_tracked_init) ->
 * dlg_tracked_init_count. 1:1 0x6e - 0x65 + 1 = 10 slots. No globals. */
int dlg_tracked_init_count(void);

/* Orig same tracked-stack slot (far HWND/ctx pair: off at +0x1ea0, seg at
 * +0x1ea2; push byte offset = count * 4 via dlg_tracked_push_off) ->
 * dlg_tracked_entry_size. 1:1 4 bytes per slot. No globals. */
int dlg_tracked_entry_size(void);

/* Orig Dlg_CreateTracked @ 1098:124f earth-custom extra loop (0x8f adds
 * 0x323-0x332, gate in dlg_ctl_is_earth_extra) ->
 * dlg_ctl_earth_extra_count. 1:1 0x332 - 0x323 + 1 = 16 slots.
 * No globals. */
int dlg_ctl_earth_extra_count(void);

/* Orig Dlg_DestroyTracked @ 1098:1612 top restore (DAT_1420_1ec8 += -1 via
 * dlg_tracked_pop_count; DAT_1420_9948 = 0 when count - 1 == 0 else the
 * count-2 slot; see dlg_destroy_tracked void-NOOP) -> dlg_tracked_top_off.
 * 1:1 byte offset of the new top slot, -1 when the stack is empty
 * (count <= 1, orig writes 0). Else (count - 2) * 4. No globals. */
int dlg_tracked_top_off(int count);

/* Orig same empty gate (top cleared when the pop empties the stack) ->
 * dlg_destroy_clears_top. 1:1 nonzero (1) when count <= 1 (orig writes
 * DAT_1420_9948 = 0 + 1420:17aa = 0 + 1420:9960 cleared when it is the
 * dying ctx), 0 otherwise. No globals. */
int dlg_destroy_clears_top(int count);

/* Orig Main_RunStartupSequence @ 11e0:0000 loop#2 rebase param (SEEK/skok
 * logicznego zegara via Anim_RebaseClock @ 10b0:005b: base = now - param,
 * stored = param; math LIVE in anim_rebase_clock, probed in
 * dlg_create_tracked) -> dlg_intro_rebase_ticks. 1:1 1000 ticks (przeskocz
 * pierwsze 1000 tickow intro-nakladki; menu idle 100000 + re-sync 0x48c
 * stay caller-side in MainMenu_WndProc @ 11e0:0221). No globals. */
int dlg_intro_rebase_ticks(void);

#endif /* CIVLIN_DIALOG_H */
