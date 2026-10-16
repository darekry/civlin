/* CivLin SAVE/LOAD (.sav) — stubs with Ghidra anchors.
 *
 * Observed in Ghidra (civ.exe, NE Win3.x), renamed 2026-10-15:
 *   strings (seg 1420):
 *     1420:2eea "Game Files (*.sav)"   (open filter)
 *     1420:2f4a "Game Files (*.sav)"   (save filter)
 *     1420:2eb8 "GetOpenFileName returned Error #"
 *     1420:2f18 "GetSaveFileName returned Error #"
 *     1420:2cac "Could not allocate buffer for saving"
 *     1420:2c74 "Could not load saved game.  Corrupted or out of memory."
 *     1420:2c3c "CivLoadGame ... load c" (load entry log)
 *   code (Orig -> New, plate comment in Ghidra holds the same mapping):
 *     Orig: FUN_1130_0075 @ 1130:0075 -> save_prompt_open
 *       open dialog (GETOPENFILENAME + 1420:2eea)
 *     Orig: FUN_1130_0194 @ 1130:0194 -> save_prompt_save
 *       save dialog (GETSAVEFILENAME + 1420:2f4a)
 *     Orig: FUN_10f8_02bd @ 10f8:02bd -> save_serialize
 *       alloc 0x1a2ba, then save_pack + save_file_open/write/close
 *     Orig: FUN_10f8_0000 @ 10f8:0000 -> save_deserialize
 *       GLOBALALLOC 0xa2ba scratch, + save_file_* in + save_unpack
 *     Orig: FUN_1140_0139 @ 1140:0139 -> save_file_open
 *       wraps _LOPEN via File_Open @ 1000:2ad9
 *     Orig: FUN_1140_0245 @ 1140:0245 -> save_file_write
 *       chunked via thunk _HWRITE/_LWRITE
 *     Orig: FUN_1140_0295 @ 1140:0295 -> save_file_size
 *       wraps _LLSEEK
 *     Orig: FUN_1140_019d @ 1140:019d -> save_file_read
 *       chunked 0x8000 via _HREAD/_LREAD
 *     Orig: FUN_1140_037c @ 1140:037c -> save_file_close
 *       wraps _LCLOSE via File_CloseWrapper @ 1000:1f7c
 *     Orig: FUN_10f8_05da @ 10f8:05da -> save_pack (called from save_serialize)
 *     Orig: FUN_10f8_040a @ 10f8:040a -> save_unpack (called from save_deserialize)
 *
 * NOTE: Ghidra warns snake_case is not PascalCase — kept intentionally
 * to stay 1:1 with src/ C names.
 *
 * Xref caveat: Ghidra MCP reports no xrefs to 1420:xxxx / EXTERNAL imports
 * for this NE binary, so anchors were found by operand scan
 * (PUSH 0x2eea -> 1130:009c etc.), not by get_xrefs_to. Re-confirm after
 * function-creation pass in Ghidra.
 *
 * Sizes seen in decompile:
 *   SAVE_BUF_SIZE  0x1a2ba (107194) — canonical save image
 *   SAVE_BUF_ALT   0x1a2b8          — accepted variant (load check)
 *   SAVE_BUF_MIN   0x18de0          — accepted variant (load check)
 *   SAVE_SCRATCH   0x0a2ba (41658)  — GLOBALALLOC scratch in FUN_10f8_0000
 */
#ifndef CIVLIN_SAVE_H
#define CIVLIN_SAVE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "civ_common.h"
#include "win16_compat.h"

#define CIV_SAVE_BUF_SIZE 0x1a2bau
#define CIV_SAVE_BUF_ALT 0x1a2b8u
#define CIV_SAVE_BUF_MIN 0x18de0u
#define CIV_SAVE_SCRATCH 0x0a2bau
#define CIV_SAVE_READ_CHUNK 0x8000u

#define CIV_SAVE_FILTER "Game Files (*.sav)"

/* Dialog outcome: mirrors GETOPENFILENAME/GETSAVEFILENAME BOOL + path. */
typedef struct civ_save_dialog {
    int confirmed;      /* 1 == user picked a file, 0 == cancelled/error */
    char path[320];     /* Win16 MAXPATH-ish; 0x1340 seen in decompile is len */
    long ext_error;     /* COMMDLGEXTENDEDERROR value on failure, 0 if none */
} civ_save_dialog_t;

/* --- dialog layer (USER/COMMDLG -> SDL3 file picker stub) --- */

/* Orig: FUN_1130_0075 @ 1130:0075, now save_prompt_open in Ghidra */
civ_error_t save_prompt_open(civ_save_dialog_t *out);
/* Orig: FUN_1130_0194 @ 1130:0194, now save_prompt_save in Ghidra */
civ_error_t save_prompt_save(civ_save_dialog_t *out);

/* Orig: FUN_1140_0095 @ 1140:0095, now Save_InitFileSpec in Ghidra.
 * File-spec init behind HoF_Load @ 1080:0023 ("famehall.civ" @
 * 1420:14e8) + HoF_Save @ 1080:0092 (@ 1420:14f5). Orig: Util_StrCopy @
 * 1000:36b2 the filename at spec+8 then Util_StrLen @ 1000:3722 into
 * spec+6, returns 0. OS-free 1:1 port on explicit caller buffers (no
 * Win16 far, no DGROUP): copies src into dst (cap incl. NUL) and
 * reports the strlen via out_len (NULL out_len skips the store).
 * NOENT on NULL src/dst, RANGE when cap == 0 or src does not fit,
 * else CIV_OK. No globals, no exit(). */
civ_error_t save_init_filespec(const char *src, char *dst, size_t cap,
                               size_t *out_len);
/* Orig: FUN_1130_000e @ 1130:000e, now Save_LoadDialog in Ghidra.
 * Modal load behind MainMenu_WndProc @ 11e0:0221 choice 1: save_prompt
 * _open @ 1130:0075 then save_deserialize @ 10f8:0000 (+ Anim/Cursor).
 * Needs COMMDLG + live ctx -> stub CIV_ERR_UNSUPPORTED. NULL out_path
 * -> NOENT (orig faulted). No globals, no exit(). */
civ_error_t save_load_dialog(char *out_path, size_t cap);
/* Orig: FUN_1098_26d4 @ 1098:26d4, now Save_SaveDialog in Ghidra.
 * Modal save wrapper over save_prompt_save @ 1130:0194 (callers: replay
 * path @ 1180:0000 + Save_SaveGame @ 1198:00a3). Needs COMMDLG ->
 * stub CIV_ERR_UNSUPPORTED. NULL out_path -> NOENT. */
civ_error_t save_save_dialog(char *out_path, size_t cap);
/* Orig: FUN_1198_00a3 @ 1198:00a3, now Save_SaveGame in Ghidra.
 * Builds "civname+year" filename (Util_StrAppend @ 1000:363e +
 * Util_IntToDec @ 1000:2928 + suffix), prompts via Save_SaveDialog @
 * 1098:26d4, writes via save_serialize @ 10f8:02bd. Needs dialogs +
 * live ctx -> stub CIV_ERR_UNSUPPORTED. NULL path -> NOENT. */
civ_error_t save_save_game(const char *path);
/* Orig: FUN_1198_0000 @ 1198:0000, now Save_SanitizeFileName in Ghidra.
 * Filename sanitizer behind Save_SaveGame @ 1198:00a3 (also called
 * from the replay path @ 1180:0000): keeps [0-9A-Za-z] only, drops all
 * other bytes, copies the filtered form back over the input (orig via
 * a 16 B stack buffer + Util_StrCopy @ 1000:36b2). OS-free 1:1 port on
 * the caller's NUL-terminated buffer (in-place, no length cap — orig
 * would overflow past 15 chars, Linux does not). NOENT on NULL, else
 * CIV_OK. No Win16, no globals, no exit(). */
civ_error_t save_sanitize_filename(char *path);

/* --- low-level file layer (KERNEL _LOPEN/_LREAD/_LWRITE/_LLSEEK/_LCLOSE) ---
 * Packet L: Ghidra display (PascalCase) <-> C name (snake_case, canonical):
 *   Save_FileOpen  @ 1140:0139 <-> save_file_open
 *   Save_FileSize  @ 1140:0295 <-> save_file_size
 *   Save_FileRead  @ 1140:019d <-> save_file_read
 *   Save_FileWrite @ 1140:0245 <-> save_file_write
 *   Save_FileClose @ 1140:037c <-> save_file_close
 * Callers: save_serialize @ 10f8:02bd, save_deserialize @ 10f8:0000,
 * HoF_Load @ 1080:0023, HoF_Save @ 1080:0092. */

/* Orig: FUN_1140_0139 @ 1140:0139, Ghidra display Save_FileOpen -> save_file_open.
 * Wraps _LOPEN via File_OpenWrapper @ 1000:2ad9 (fopen "rb"/"wb" twin);
 * write_mode 0 = OF_READ, nonzero = OF_WRITE/OFCREATE. Returns handle
 * or WIN16_HFILE_ERROR. */
win16_handle save_file_open(const char *path, int write_mode);
/* Orig: FUN_1140_0295 @ 1140:0295, Ghidra display Save_FileSize -> save_file_size.
 * Via _LLSEEK (fseek/ftell twin file_seek_wrapper); position-preserving;
 * -1 on error. Accepted image sizes (checked by save_deserialize
 * @ 10f8:0000, not here): 0x1a2ba / 0x1a2b8 / 0x18de0. */
long save_file_size(win16_handle h);
/* Orig: FUN_1140_019d @ 1140:019d, Ghidra display Save_FileRead -> save_file_read.
 * Chunked 0x8000 via _HREAD/_LREAD (= File_ReadChunk fread twin);
 * 0 ok / 1 eof-short. */
int save_file_read(win16_handle h, uint8_t *dst, unsigned long len,
                   unsigned long *out_read);
/* Orig: FUN_1140_0245 @ 1140:0245, Ghidra display Save_FileWrite -> save_file_write.
 * Chunked 0x8000 via _HWRITE/_LWRITE (= File_WriteChunk fwrite twin);
 * 0 ok / 1 short-write. */
int save_file_write(win16_handle h, const uint8_t *src, unsigned long len,
                    unsigned long *out_written);
/* Orig: FUN_1140_037c @ 1140:037c, Ghidra display Save_FileClose -> save_file_close.
 * Wraps _LCLOSE via File_CloseWrapper @ 1000:1f7c (fclose NULL-safe twin). */
void save_file_close(win16_handle h);

/* Orig: FUN_1000_2ad9 @ 1000:2ad9, now File_Open in Ghidra -> File_OpenWrapper.
 * Thin _LOPEN wrapper used by save_file_open @ 1140:0139. Linux
 * equivalent: fopen(path, write_mode ? "wb" : "rb"); write_mode 0 =
 * OF_READ, nonzero = OF_WRITE/OFCREATE (exact OF_ flag mapping TBD, see
 * Ghidra). Returns the stream, NULL on bad args or fopen failure. */
FILE *file_open_wrapper(const char *path, int write_mode);
/* Orig: FUN_1000_1f7c @ 1000:1f7c, now File_CloseWrapper in Ghidra.
 * Thin _LCLOSE wrapper used by save_file_close @ 1140:037c. Linux
 * equivalent: fclose(fp); NULL-safe no-op. */
void file_close_wrapper(FILE *fp);
/* Orig: FUN_1000_xxxx @ 1000:xxxx -> File_SeekWrapper (packet E).
 * Thin _LLSEEK wrapper complementing File_OpenWrapper @ 1000:2ad9,
 * File_CloseWrapper @ 1000:1f7c and File_ReadChunk @ 1000:272a
 * (rsc_loader.h). Used by save_file_size @ 1140:0295 and the future
 * ResMgr_LoadToMem @ 1190:0fda payload path (rsc_load_to_mem). Linux
 * equivalent: fseek(fp, offset, origin) + ftell; origin maps 1:1 to
 * SEEK_SET/SEEK_CUR/SEEK_END (orig _LLSEEK 0/1/2 match DOS). Returns the
 * new file offset, or -1 on bad args / seek / tell failure (mirrors
 * _LLSEEK HFILE_ERROR). */
long file_seek_wrapper(FILE *fp, long offset, int origin);
/* Win16 file-import map (KERNEL, packet E): _LOPEN -> file_open_wrapper,
 * _LCLOSE -> file_close_wrapper, _LLSEEK -> file_seek_wrapper (above),
 * _LREAD/_HREAD -> file_read_chunk (rsc_loader.h) / save_file_read
 * (chunked 0x8000 @ 1140:019d), _LWRITE/_HWRITE -> file_write_chunk
 * (sys.h) / save_file_write (chunked @ 1140:0245). */

/* --- serialize layer (game state <-> flat 0x1a2ba image) ---
 * W1 (packet N): marshal + RLL + file wiring, partial ports (live ctx
 * still zeroed, see below).
 * save_serialize @ 10f8:02bd flow: alloc 0x1a2ba image (LocalAlloc,
 *   err 1420:2cac "Could not allocate buffer for saving" -> NOMEM) ->
 *   marshal via save_marshal_fields @ 10f8:090c (direction save,
 *   g_nSaveDirection @ 1310:0008 == 0) -> RLL-encode via save_pack @
 *   10f8:05da core (save_rll_encode) -> save_file_open O_WRITE @ 1140:0139
 *   -> save_file_write @ 1140:0245 -> save_file_close @ 1140:037c.
 * save_deserialize @ 10f8:0000 flow: heap wire buffer (GLOBALALLOC 0xa2ba
 *   scratch equiv) -> save_file_open O_READ -> save_file_size @ 1140:0295
 *   (read bound only) -> save_file_read @ 1140:019d -> RLL-decode via
 *   save_unpack @ 10f8:040a core (save_rll_decode) -> decoded-size gate
 *   0x1a2ba / 0x1a2b8 / 0x18de0 (else BAD_DATA, orig err 1420:2c74 "Could
 *   not load saved game. Corrupted or out of memory.") -> marshal via
 *   save_marshal_fields (direction load, == 1) -> close. RLL codec + file
 *   I/O + marshal driver below are ported; live-ctx scatter (segments
 *   12d8/...) stays zeroed (TODO), no globals taken here. */

/* Orig: FUN_10f8_02bd @ 10f8:02bd, now save_serialize — pack + write .sav.
 * Partial port (W1): calloc 0x1a2ba -> save_marshal_fields (save) ->
 * save_rll_encode -> save_file_open/write/close. NOENT on NULL/empty
 * path, NOMEM on alloc failure, IO on open/write failure, else CIV_OK.
 * Image is zeros until live ctx maps (TODO). */
civ_error_t save_serialize(const char *path);
/* Orig: FUN_10f8_0000 @ 10f8:0000, now save_deserialize — read + unpack .sav.
 * Partial port (W1): open -> size -> read -> save_rll_decode -> decoded
 * gate 0x1a2ba canonical / 0x1a2b8 alt / 0x18de0 small-map (BAD_DATA on
 * mismatch) -> save_marshal_fields (load) -> close. NOENT on NULL/empty
 * path, IO on open/seek failure, TRUNCATED on empty/short read or RLL
 * overrun, NOMEM on alloc failure. Size gate lives here (not in
 * save_file_size); wire length varies with RLL so the gate is on the
 * decoded flat length. */
civ_error_t save_deserialize(const char *path);

/* Pack/unpack are RLL codecs (not game pack). Log strings:
 *   save_pack    "RLLEncode ... load c"   Orig FUN_10f8_05da @ 10f8:05da
 *   save_unpack  "RLLDecode ... load c"   Orig FUN_10f8_040a @ 10f8:040a
 * Tag byte: <0x80 = repeat next byte N times; >=0x80 = copy N-0x80 literals.
 * Wire sizes: canonical 0x1a2ba, alt 0x1a2b8 (cond word), small-map 0x18de0.
 * Packet N: save_pack/save_unpack are thin legacy wrappers over the ported
 *   save_rll_encode/save_rll_decode below (same tag scheme, same 0x7f chunk
 *   cap). Legacy signatures are kept for the RE map (out_image/out_len vs
 *   image/len); they carry no caller buffer for the native src/dst/cap
 *   triple, so no trivial delegate is possible until the marshal flat image
 *   + heap owner are wired — stubs document the wrapper relationship. */
/* Orig: FUN_10f8_05da @ 10f8:05da, now save_pack (RLL encode, called from save_serialize).
 * Stub: NOENT on NULL out params, else UNSUPPORTED; read-only. */
civ_error_t save_pack(const uint8_t **out_image, size_t *out_len);
/* Orig: FUN_10f8_040a @ 10f8:040a, now save_unpack (RLL decode, called from save_deserialize).
 * Stub: NOENT on NULL image, else UNSUPPORTED; read-only. */
civ_error_t save_unpack(const uint8_t *image, size_t len);

/* Native RLL buffer twins (heading to full LIVE, Orig FUN_10f8_05da @
 * 10f8:05da / FUN_10f8_040a @ 10f8:040a cores via save_rll_encode/decode).
 * Unlike the legacy save_pack/save_unpack wrappers above (which carry no
 * caller dst/cap), these take the explicit native src/dst/cap triple, so
 * save_serialize @ 10f8:02bd / save_deserialize @ 10f8:0000 can delegate
 * 1:1. NOENT on NULL src/dst, TRUNCATED on empty wire (unpack) or dst
 * overrun, else CIV_OK with *out_len (NULL out_len skips the store).
 * OS-free, no Win16, no globals, no exit(). */
civ_error_t save_pack_buffer(const uint8_t *src, size_t src_len,
                             uint8_t *dst, size_t dst_cap, size_t *out_len);
civ_error_t save_unpack_buffer(const uint8_t *src, size_t src_len,
                               uint8_t *dst, size_t dst_cap,
                               size_t *out_len);

/* Orig: FUN_1198_00a3 @ 1198:00a3, now Save_SaveGame (filename builder).
 * OS-free 1:1 of the "civname+year" filename head: filters civname to
 * [0-9A-Za-z] (same keep-alnum rule as save_sanitize_filename @
 * 1198:0000, orig via a 16 B stack buffer + Util_StrCopy @ 1000:36b2),
 * appends abs(year) decimal (Util_IntToDec @ 1000:2928) + "BC" when
 * year < 0 else "AD" (Save_SaveGame suffix 0x3efe/0x3f00; HoF_Show @
 * 1080:0119 uses " BC"/" AD" @ 1420:173c/1740 with a leading space for
 * the status line, filenames use the spaceless form). NOENT on NULL
 * civname/out, RANGE when cap == 0 or the result (incl. NUL) does not
 * fit, else CIV_OK. No Win16, no globals, no exit(). */
civ_error_t save_build_filename(const char *civname, int year, char *out,
                                size_t cap);

/* --- RLL codec, native form (new API, implemented next) ---
 * Orig: FUN_10f8_05da @ 10f8:05da (save_pack RLL core, called from
 *   save_serialize @ 10f8:02bd) -> save_rll_encode; Orig: FUN_10f8_040a
 *   @ 10f8:040a (save_unpack RLL core, called from save_deserialize @
 *   10f8:0000) -> save_rll_decode. Tag scheme verified ref
 *   unpacked/civ.exe: <0x80 repeat next byte N times, >=0x80 copy
 *   N-0x80 literals, 0x7f chunk cap. */

civ_error_t save_rll_encode(const uint8_t *src, size_t src_len,
                            uint8_t *dst, size_t dst_cap, size_t *out_len);
civ_error_t save_rll_decode(const uint8_t *src, size_t src_len,
                            uint8_t *dst, size_t dst_cap, size_t *out_len);

/* --- field marshaller (game ctx <-> flat image) ---
 * W1: save_marshal_fields @ 10f8:090c is the ordered xfer driver (ported).
 *   direction via g_nSaveDirection @ 1310:0008 (0 = save: live -> image,
 *   1 = load: image -> live; reservation is direction-symmetric),
 *   small/large variant via g_nSaveVariant @ 1310:0000 (pre-word at image
 *   +0x0, live 1310:0000; full-size path ported, small-map shrink TODO),
 *   image cursor via g_nSaveCursorOff @ 1420:a2d6 with base @ 1420:a2da
 *   (local cursor here, no globals). Ordered calls below (image offsets
 *   CIV_SAVE_OFF_*): pre word + 8 words + 3 arrays 8x0x20 + blks
 *   0x4b0/0x4b0/0xe00 + strided 28x22B + blk 0x3000 + blk 0xfa0 + array
 *   0x100x0x20 + blk 0x1000 + blk 0x240 + blk 0x104 + blk 0x5a0 + last
 *   word + map 0xfa00 at +0xa8ba. Gaps between CIV_SAVE_OFF_* are padding
 *   left as caller left it (zeros from save_serialize calloc). No globals
 *   taken here (caller-owned image + cap); live segments 12d8/12d0/1248/
 *   1290/1298/1308/1350/1388/1390/13c0/13e8/1420 have no Linux mapping yet,
 *   so the live memcpy half stays TODO (OS-free reservation only, no
 *   Win16 far pointers). */
/* Orig: FUN_10f8_090c @ 10f8:090c, now save_marshal_fields.
 * Direction by Ghidra g_nSaveDirection @ 1310:0008 (0=save,1=load);
 * variant by g_nSaveVariant @ 1310:0000. Cursor g_nSaveCursorOff @
 * 1420:a2d6, base g_nSaveBaseOff @ 1420:a2da (locals here).
 * Ported (W1): NOENT on NULL image, TRUNCATED when cap < CIV_SAVE_BUF_MIN
 * (smallest accepted flat size) or when the full-size walk overruns cap,
 * BAD_DATA on layout overlap/final-size mismatch, CIV_OK when the full
 * ordered sequence fits (cursor ends at CIV_SAVE_IMAGE_SIZE). Image bytes
 * preserved (no write on the reservation path). */
civ_error_t save_marshal_fields(uint8_t *image, size_t cap);
/* Orig: FUN_10f8_0f40 @ 10f8:0f40, now save_xfer_words — count WORDS (x2 bytes).
 * OS-free image-cursor half ported 1:1: len = words*2 with overflow guard,
 * bounds-check *cursor + len <= cap (TRUNCATED on overrun), advance *cursor
 * by len, return CIV_OK. seg:off is the live Win16 far anchor (directional
 * memcpy live<->image window by g_nSaveDirection, memcpy on Linux not
 * HMEMCPY/mem_copy_far); no selector dereference here — retained for the RE
 * map, live wiring lands with save_marshal_fields. NOENT on NULL
 * image/cursor. */
civ_error_t save_xfer_words(uint8_t *image, size_t cap, size_t *cursor,
                            uint16_t seg, uint16_t off, unsigned words);
/* Orig: FUN_10f8_0fcf @ 10f8:0fcf, now save_xfer_block — byte block via mem_copy_far.
 * Same contract as words with len in bytes: bounds-check + advance, CIV_OK.
 * NOENT on NULL image/cursor. */
civ_error_t save_xfer_block(uint8_t *image, size_t cap, size_t *cursor,
                            uint16_t seg, uint16_t off, unsigned len);
/* Orig: FUN_10f8_1035 @ 10f8:1035, now save_xfer_array — n elems, stride 0x20.
 * Orig loop: n iterations, live stride 0x20 per elem, elem_len image bytes
 * each (full 0x20; small-map 0x0e/0x0c/0x0b/0x0d per layout). Ported as
 * per-elem bounds-checked advance (TRUNCATED mid-loop leaves *cursor at the
 * first failing elem start); total = n*elem_len with overflow guard.
 * CIV_OK when all n fit. NOENT on NULL image/cursor; RANGE on n*elem_len
 * overflow. seg/base are live anchors (see words). */
civ_error_t save_xfer_array(uint8_t *image, size_t cap, size_t *cursor,
                            uint16_t seg, uint16_t base, unsigned n,
                            unsigned elem_len);
/* Orig: FUN_10f8_10bd @ 10f8:10bd, now save_xfer_strided — skip 0xc + 0xb words.
 * Orig loop: n iterations, each live elem skips 0xc then moves 0xb words
 * (22B) to/from the image (packed 22B per elem in image, e.g. 28 elems at
 * CIV_SAVE_OFF_STRIDED_1420_1A38). Ported as per-elem 22B bounds-checked
 * advance; TRUNCATED mid-loop leaves *cursor at failing elem start.
 * CIV_OK when all n fit. NOENT on NULL image/cursor. seg/base live anchors.
 * Stride constants below. */
civ_error_t save_xfer_strided(uint8_t *image, size_t cap, size_t *cursor,
                              uint16_t seg, uint16_t base, unsigned n);

/* --- flat .sav image layout (uncompressed, 0x1a2ba) ---
 * Derived from save_marshal_fields @ 10f8:090c disassembly + save_serialize
 * @ 10f8:02bd (pre-word + map block at +0xa8ba, len 0xfa00).
 * [0x00000..0x00002) pre word: live 1310:0000 (g_nSaveVariant)
 * [0x00002..0x0a8ba) ordered fields below (offsets are image-relative)
 * [0x0a8ba..0x1a2ba) map block 64000B to/from 1420:b394/b396 ptr
 * Live game ctx = segments 12d8 (main), 12d0/1248/1290/1298/1308/1350/
 * 1388/1390/13c0/13e8/1420. Hottest: 12d8:42fe (82 xrefs), 12d8:4300
 * (607 xrefs) — lead game-ctx candidates.
 * Small-map variant (0x18de0): arrays shrink 8x0x20->8x0x0e/0x0c/0x0b and
 * 0x100x0x20->0x100x0x0d (diff 5338B incl. cond word).
 */
#define CIV_SAVE_IMAGE_SIZE 0x1a2bau
#define CIV_SAVE_IMAGE_HDR_LEN 0xa8bau
#define CIV_SAVE_IMAGE_MAP_OFF 0xa8bau
#define CIV_SAVE_IMAGE_MAP_LEN 0xfa00u

/* Image-relative offsets (v1, full-size path). Pre-word at +0x0. */
#define CIV_SAVE_OFF_PRE 0x00000u
#define CIV_SAVE_OFF_F_12D8_42FE 0x00002u
#define CIV_SAVE_OFF_F_12D8_4300 0x00004u
#define CIV_SAVE_OFF_F_12D8_42FC 0x00006u
#define CIV_SAVE_OFF_F_13C0_0000 0x00008u
#define CIV_SAVE_OFF_F_12D8_42FA 0x0000au
#define CIV_SAVE_OFF_F_12D8_42F6 0x0000cu
#define CIV_SAVE_OFF_F_12D8_42F8 0x0000eu
#define CIV_SAVE_OFF_F_12D0_0062 0x00010u
#define CIV_SAVE_OFF_A_12D8_01DA 0x00012u /* 8x0x20 */
#define CIV_SAVE_OFF_A_12D8_02DA 0x00112u /* 8x0x20 */
#define CIV_SAVE_OFF_A_12D8_03DA 0x00212u /* 8x0x20 (small: 8x0x0b) */
#define CIV_SAVE_OFF_BLK_12D8_3200 0x00d82u   /* 0x4b0 */
#define CIV_SAVE_OFF_BLK_12D8_2D50 0x01232u   /* 0x4b0 */
#define CIV_SAVE_OFF_BLK_1420_54FC 0x016e2u   /* 0xe00 */
#define CIV_SAVE_OFF_STRIDED_1420_1A38 0x024e2u /* 28 x (0xc skip + 22B) */
#define CIV_SAVE_OFF_BLK_1420_62FC 0x0289au   /* 0x3000 */
#define CIV_SAVE_OFF_BLK_13E8_000A 0x0589au   /* 0xfa0 */
#define CIV_SAVE_OFF_A_12D8_04DA 0x06b4au     /* 0x100 x 0x20 (small: 0x0d) */
#define CIV_SAVE_OFF_BLK_1390_03AF 0x08b4cu   /* 0x1000 */
#define CIV_SAVE_OFF_BLK_12D8_36B0 0x09d38u   /* 0x240 */
#define CIV_SAVE_OFF_BLK_1248_0004 0x09f80u   /* 0x104 */
#define CIV_SAVE_OFF_BLK_1290_01B2 0x0a0acu   /* 0x5a0 */
#define CIV_SAVE_OFF_F_1308_0020 0x0a8b8u     /* last word; +2 == 0xa8ba */

/* RLL tag: <0x80 repeat, >=0x80 literal run. */
#define CIV_SAVE_RLL_REPEAT_MAX 0x7fu
#define CIV_SAVE_RLL_LITERAL_FLAG 0x80u

/* Xfer strides (packet N, Orig FUN_10f8_1035 @ 10f8:1035 / FUN_10f8_10bd @
 * 10f8:10bd). Live stride is fixed; image elem_len varies by variant
 * (full 0x20 vs small-map 0x0e/0x0c/0x0b/0x0d). Strided: per elem skip 0xc
 * live bytes, then 0xb words (22B) packed in image. */
#define CIV_SAVE_ARRAY_LIVE_STRIDE 0x20u
#define CIV_SAVE_STRIDED_SKIP 0x0cu
#define CIV_SAVE_STRIDED_WORDS 0x0bu
#define CIV_SAVE_STRIDED_ELEM_LEN (0x0bu * 2u)

/* Live-ctx segments (game state scatter, X1). 12d8 is the main ctx.
 * Full scatter from save_marshal_fields @ 10f8:090c walk + frontier:
 *   12d8 main (words @ 42fe/4300/42fc/42fa/42f6/42f8, arrays 01da/02da/03da
 *     8x0x20, blks 3200/2d50 0x4b0, array 04da 0x100x0x20, blk 36b0 0x240)
 *   12d0 sys (word @ 0062), 1248 aux (blk @ 0004 0x104),
 *   1290 aux (blk @ 01b2 0x5a0), 1298/1350/1388 reserved (no walk yet,
 *     unmapped -> reservation-only), 1308 ctrl (word @ 0020),
 *   1390 (blk @ 03af 0x1000), 13c0 (word @ 0000), 13e8 (blk @ 000a 0xfa0),
 *   1420 far (blk 54fc 0xe00, strided 1a38 28x22B, blk 62fc 0x3000,
 *     map 0xfa00 @ b394 via ptr @ b394/b396), 1310 ctrl (pre-word @ 0000
 *     g_nSaveVariant, direction @ 1310:0008 g_nSaveDirection).
 * Map block is indirect in orig (far ptr at 1420:b394/b396); on Linux the
 * caller registers a linear window for seg 0x1420 large enough for
 * off+len (no 64K wrap) or leaves it unmapped. No globals. */
#define CIV_SEG_GAME_MAIN 0x12d8u
#define CIV_SEG_GAME_SYS 0x12d0u
#define CIV_SEG_GAME_AUX 0x1290u
#define CIV_SEG_GAME_AUX2 0x1248u
#define CIV_SEG_GAME_R1298 0x1298u
#define CIV_SEG_GAME_CTRL 0x1308u
#define CIV_SEG_GAME_R1350 0x1350u
#define CIV_SEG_GAME_R1388 0x1388u
#define CIV_SEG_GAME_R1390 0x1390u
#define CIV_SEG_GAME_R13C0 0x13c0u
#define CIV_SEG_GAME_R13E8 0x13e8u
#define CIV_SEG_GAME_FAR 0x1420u
#define CIV_SEG_SAVE_CTRL 0x1310u

/* Direction mirrors g_nSaveDirection @ 1310:0008 (0 save, 1 load). */
#define CIV_SAVE_DIR_SAVE 0
#define CIV_SAVE_DIR_LOAD 1

/* Explicit live ctx (X1, OS-free, no globals). Caller owns every window;
 * NULL base or missing seg = unmapped -> reservation-only (zeros behavior
 * preserved). seg/off anchors are kept on every xfer call for the RE map.
 * Map block via 1420:b394/b396 ptr in orig: register a linear 1420 window
 * covering off+len on Linux. */
typedef struct civ_save_seg_win {
    uint16_t seg;      /* Win16 seg anchor, e.g. 0x12d8 */
    uint8_t *base;     /* caller-owned live bytes (NULL = unmapped) */
    size_t len;        /* window size */
} civ_save_seg_win_t;

#define CIV_SAVE_CTX_MAX_SEGS 16u

typedef struct civ_save_ctx {
    int direction;     /* CIV_SAVE_DIR_SAVE (live->image) or LOAD (image->live) */
    civ_save_seg_win_t wins[CIV_SAVE_CTX_MAX_SEGS];
    size_t nwins;
} civ_save_ctx_t;

/* OS-free ctx lifecycle (no globals, no exit): init zeroes wins/nwins and
 * stores direction (nonzero -> LOAD); add registers one seg window
 * (NOENT on NULL ctx/base, RANGE when full/len==0, OK on insert or
 * in-place update of an existing seg). */
void civ_save_ctx_init(civ_save_ctx_t *ctx, int direction);
civ_error_t civ_save_ctx_add(civ_save_ctx_t *ctx, uint16_t seg,
                             uint8_t *base, size_t len);

/* Live twins of the xfer family (X1, OS-free, no globals, no Win16):
 * same seg/off anchors + image-cursor reservation as the plain variants,
 * plus a directional bounds-checked memcpy when ctx maps the seg
 * (SAVE: live -> image window at *cursor; LOAD: image window -> live).
 * ctx NULL or seg unmapped = reservation-only (same as plain). Live
 * overrun (off+len beyond the registered window) reports RANGE;
 * image-cursor overrun reports TRUNCATED with *cursor at the failing
 * elem start (matches orig stop-short). Array live stride is fixed
 * 0x20 (copies elem_len bytes per elem, prefix for small-map);
 * strided live stride is SKIP+ELEM_LEN per elem (skips 0xc live bytes,
 * packs 22B per elem in the image). */
civ_error_t save_xfer_words_live(uint8_t *image, size_t cap, size_t *cursor,
                                 uint16_t seg, uint16_t off, unsigned words,
                                 const civ_save_ctx_t *ctx);
civ_error_t save_xfer_block_live(uint8_t *image, size_t cap, size_t *cursor,
                                 uint16_t seg, uint16_t off, unsigned len,
                                 const civ_save_ctx_t *ctx);
civ_error_t save_xfer_array_live(uint8_t *image, size_t cap, size_t *cursor,
                                 uint16_t seg, uint16_t base, unsigned n,
                                 unsigned elem_len,
                                 const civ_save_ctx_t *ctx);
civ_error_t save_xfer_strided_live(uint8_t *image, size_t cap, size_t *cursor,
                                   uint16_t seg, uint16_t base, unsigned n,
                                   const civ_save_ctx_t *ctx);
/* Orig FUN_10f8_090c @ 10f8:090c live driver: same ordered CIV_SAVE_OFF_*
 * walk as save_marshal_fields but through the *_live xfers with ctx
 * (NULL ctx == reservation-only, same as plain). Direction comes from
 * ctx->direction (SAVE live->image, else image->live); unmapped segs
 * (1298/1350/1388 frontier, or any caller hole) stay reservation-only. */
civ_error_t save_marshal_fields_live(uint8_t *image, size_t cap,
                                      const civ_save_ctx_t *ctx);

/* Strided live stride: per elem skip 0xc live bytes + 22B packed. */
#define CIV_SAVE_STRIDED_LIVE_STRIDE \
    (CIV_SAVE_STRIDED_SKIP + CIV_SAVE_STRIDED_ELEM_LEN)

/* --- Replay log (segment 1180, 2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Replay_PlayLog @ 1180:02d3 (reader: year via Replay_YearFromTurn @
 *     1180:0f7c + line buffer @ 12d8:24fa + cases below + Replay_ShowEntry
 *     @ 1180:0e78 per entry; pump via Sys_PumpMessages @ 1108:0000 +
 *     Sys_DrainInput @ 11f0:4ea0; writer is Replay_AppendLog @ 11c0:3beb).
 *   Log store: count @ 1388:0000 (u16-ish, cap 0x1000) + bytes @
 *     1390:03af. Head byte: kind*0x10 + (turn>>8); next byte: turn low;
 *     then payload (see replay_entry_len: 1/9 found+destroyed/capture 6B,
 *     2 war / 3 peace 3B, 5 discover / 6 produce / 8 form / 10 build 4B,
 *     0xb census 5B, 0xc election variable, 0xd destroyed 4B).
 *   Cases (Replay_PlayLog switch on head>>4): 1/9 found(0x88 city name via
 *     Replay_FindEmptySlot @ 1180:0fe5 + Replay_RefreshMapTile @ 1180:110c)
 *     vs capture (Replay_UpdateSlotOwner @ 1180:1044); 2 war ("declare war
 *     on"); 3 peace ("make peace with"); 5 discover (Res 0x82 tech via
 *     Res_GetStringCopy @ 11f8:0080); 6 produce-first (Res 0x84); 8 form
 *     (Res 0x8d); 10 build (Res 0x83 + 0x3967 when < 8); 0xb census
 *     (City_FormatPopCount @ 11c8:03ac + "population"); 0xc election
 *     (4 rows of high/low nibble civ picks); 0xd destroyed
 *     (Replay_ShowCivDestroyed @ 1180:17e9 when mode==4 and victim==player).
 *   Modes DAT_1390_009c: 1 = quiet (no Replay_ShowEntry), 2 = election
 *     tail (+ FUN_1038_0c1d), 3 = record-to-file (handle @ 0xb1a6 via
 *     Replay_OpenOrPlay @ 1180:0000 + Replay_PromptMode @ 1180:017a +
 *     Replay_CloseView @ 1180:0297 + Replay_DumpMapText @ 1180:137f map
 *     text), 4 = conqueror demo (Replay_ShowConqueror @ 1180:152f, slots
 *     0x3a1 preset 0xff x0xe + "CONQUEROR" titles).
 *   Civ names: Replay_LoadCivNames @ 1180:14be (Res 0x94/0x95 into
 *     12d8:03da leader / 12d8:02da civ display).
 *   Map tile refresh: Replay_RefreshMapTile @ 1180:110c (11x? clamp
 *     x+-5/0x50 + y+-5/2..0x31 window, nearest via Replay_FindNearestSlot
 *     @ 1180:1096 over Map_WrapStepDist @ 11c8:02ec, brush by
 *     DAT_1420:b1a4 mode + Gfx_MakeBrush @ 1098:00b5 + PresentRect).
 *   Who writes: Replay_AppendLog @ 11c0:3beb (11 callers: Unit_DoMove @
 *     1038:0000, Game_TurnLoopTick @ 1088:0000, Game_DoPollutionAndWonderRace
 *     @ 1088:1e32, NewGame wizard @ 10e8:1b97, diplomacy @ 1168:06c1/0dda/
 *     0e8e, tech/wonder @ 1170:0241/2aaa, city/founding @ 1040:0000 +
 *     1030:0000 shared PIC player). Who reads: Replay_PlayLog (above) +
 *     Save_SaveDialog @ 1098:26d4 path via Replay_OpenOrPlay mode 3.
 * OS-free format/count math below is LIVE 1:1 on explicit params (no
 * Win16, no globals, read-only except the append cursor); GDI/file tails
 * stay STUB with TODO Orig. */

/* Replay log caps (Orig Replay_AppendLog @ 11c0:3beb + Replay_PlayLog). */
#define CIV_REPLAY_MAX_BYTES 0x1000u
#define CIV_REPLAY_SLOTS 0x80u
#define CIV_REPLAY_EMPTY 0xffff

/* Orig Replay_YearFromTurn @ 1180:0f7c -> replay_year_from_turn.
 * OS-free 1:1 turn->year ladder (no globals): turn < 0xfb:
 * turn*0x14 - 4000; turn-0xfa < 0x33: (turn-0xfa)*10 + 1000; turn-300 <
 * 0x33: (turn-300)*5 + 0x5dc; turn-0x15e < 0x33: (turn-0x15e)*2 + 0x6d6;
 * else turn + 0x5aa. Negative turn clamps the first arm (orig compared
 * signed). */
int replay_year_from_turn(int turn);

/* Orig Replay_AppendLog @ 11c0:3beb -> replay_append_log.
 * OS-free 1:1 on explicit buf/count (buf is the 1390:03af image, cap
 * bytes; *count is the 1388:0000 cursor): needs 2 + payload_len bytes and
 * *count + 2 + payload_len < CIV_REPLAY_MAX_BYTES (orig
 * DAT_1388_0000 + param_2 + 2 < 0x1000 gate, param_2 = payload_len incl.
 * kind/turn framing? here payload_len excludes the 2 framing bytes).
 * Writes head = kind*0x10 + ((turn>>8) & 0x0f) at buf[*count], turn low at
 * buf[*count+1], then payload bytes; advances *count by 2+payload_len.
 * kind is 0..15 (switch cases 1..0xd, exact set see above); turn is
 * g_nTurn (signed shown as u16 split). Returns CIV_OK, NOENT on NULL
 * buf/count (NULL payload ok when payload_len == 0), RANGE on bad kind
 * or count OOR, TRUNCATED when the cap gate fails (no partial write).
 * No globals, no exit(). */
civ_error_t replay_append_log(uint8_t *buf, size_t cap, size_t *count,
                              int kind, int turn, const uint8_t *payload,
                              unsigned payload_len);

/* Orig Replay_FindEmptySlot @ 1180:0fe5 scan half -> replay_find_empty_slot.
 * OS-free 1:1: first i in 0..n-1 with owners[i] == CIV_REPLAY_EMPTY
 * (0x80 slots @ 1390:00a0 in orig, -1 sentinel). Returns the index, -1
 * when full or on NULL/n <= 0. No globals, no exit(), read-only. */
int replay_find_empty_slot(const int *owners, int n);

/* Orig Replay_UpdateSlotOwner @ 1180:1044 scan half -> replay_update_slot_owner.
 * OS-free 1:1: first i with xs[i]==x && ys[i]==y (orig 0x2a0/0x1a0 word
 * tables vs param_2/param_3 bytes); writes owners[i] = owner (orig
 * 0x00a0 table). Returns the index, -1 when no match or on NULL/n <= 0.
 * owners NULL -> -1 (no write). No globals, no exit(). */
int replay_update_slot_owner(int *owners, const int *xs, const int *ys, int n,
                             int x, int y, int owner);

/* --- Replay map-tile (2026-10-16) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Replay_FindNearestSlot @ 1180:1096 (0x80 slots @ 1390:00a0, skip -1;
 *     score via Map_WrapStepDist @ 11c8:02ec over the 0x2a0/0x1a0 word
 *     tables; best into DAT_1420_b1a4 init 999, strict < keeps ties at
 *     the lower index; sole caller Replay_RefreshMapTile @ 1180:110c).
 *   Replay_RefreshMapTile @ 1180:110c (11x? clamp window x+-5/0x50 +
 *     y+-5/2..0x31 via Util_Clamp @ 11c8:0063; per tile Map_IsSeaLand @
 *     1110:16c0 vs 10 + Map 1110:1d99 owner compare vs the nearest slot
 *     (mismatch or DAT_1420_b1a4 >= 6 -> gray via DAT_12d8_45c6 brush,
 *     else owner brush 7/0x0f/0x10+owner); 6x6 px rects at (x*6+0x10,
 *     y*6+0x10) + GfxPort_PresentRect @ 10d0:0854; sole caller
 *     Replay_PlayLog @ 1180:02d3 cases 1/9 with local_16 = 1).
 *   Replay_DumpMapText @ 1180:137f (record path: rows y 0..0x31, per row
 *     0x50 cells via Map_IsSeaLand @ 1110:16c0 (10 -> 0x20 ' ', else
 *     0x2e '.'), city overlay via the 0x100 scan (x @ +0x17, y @ +3) +
 *     Map 1110:199b-combined low bit (&1 == 0 -> 0x2b '+', else 0x2a
 *     '*') + city-name bytes (@ 0x4da) appended at 0x24fb, CR 0x0d at
 *     12d8:254a, 0x51 B save_file_write @ 1140:0245 per row; sole caller
 *     Replay_OpenOrPlay @ 1180:0000 mode 3).
 * How replay replays the map: it does NOT move units (no Unit_DoMove /
 * Map claim) — found/capture log entries (kinds 1/9) allocate or retarget
 * a city slot (Replay_FindEmptySlot @ 1180:0fe5 / Replay_UpdateSlotOwner
 * @ 1180:1044), then Replay_RefreshMapTile repaints the 11-wide window
 * around the city with the nearest-slot owner colors; the file record
 * path instead dumps the ASCII map text above. GDI/file tails stay STUB;
 * the OS-free window/score/cell math below is LIVE 1:1 on explicit
 * params (no Win16, no globals, read-only except outs). */

/* Orig Replay_FindNearestSlot @ 1180:1096 -> replay_find_nearest_slot.
 * 1:1 scan (skip owners[i] == CIV_REPLAY_EMPTY; score
 * map_wrap_step_dist(x, xs[i], y, ys[i]) per Map_WrapStepDist @ 11c8:02ec,
 * see map.h; strict < keeps ties at the lower index, init 999 == orig
 * DAT_1420_b1a4). Returns the index, -1 when none qualifies or on
 * NULL/n <= 0; *dist_out gets the winning distance (999 when none, orig
 * sentinel; NULL dist_out skips the store). No globals, no exit(). */
int replay_find_nearest_slot(const int *owners, const int *xs,
                             const int *ys, int n, int x, int y,
                             int *dist_out);

/* Orig Replay_RefreshMapTile @ 1180:110c clamp half -> replay_tile_window.
 * 1:1 of the Util_Clamp @ 11c8:0063 calls (see sys.h util_clamp):
 * x0 = clamp(x-5,0,0x50), x1 = clamp(x+5,0,0x50), y0 = clamp(y-5,2,0x31),
 * y1 = clamp(y+5,2,0x31) (game map 0x50 x 0x32 at rows 2..0x31; the tile
 * == 10 vs owner-brush + 6x6 PresentRect paint tail stays STUB).
 * NOENT on any NULL out, else CIV_OK. No globals, no exit(). */
civ_error_t replay_tile_window(int x, int y, int *x0, int *x1, int *y0,
                               int *y1);

/* Orig Replay_DumpMapText @ 1180:137f cell half -> replay_map_text_cell.
 * 1:1: ocean (Map_IsSeaLand @ 1110:16c0 == 10) -> ' ' (0x20), else '.'
 * (0x2e); a city at the cell (0x100 scan hit) overrides to '*' (0x2a),
 * or '+' (0x2b) when the Map 1110:199b-combined low bit is clear
 * ((combined & 1) == 0). Params are pre-decoded predicates (tile_is_ocean
 * != 0, has_city != 0, combined_lo_bit & 1); the city-name append (@
 * 0x4da into 0x24fb) + CR row framing + save_file_write tail stay STUB.
 * Returns the cell char (never '?'). No globals, no exit(). */
char replay_map_text_cell(int tile_is_ocean, int has_city,
                          int combined_lo_bit);

/* --- Replay modes (2026-10-16) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode):
 *   Replay_OpenOrPlay @ 1180:0000 (loop: Replay_PromptMode @ 1180:017a;
 *     mode < 1 -> return; mode == 3 -> record-to-file: civ-name filename
 *     via Util_StrAppend @ 1000:363e + Save_SanitizeFileName @ 1198:0000
 *     + Save_SaveDialog @ 1098:26d4 + save_file_open @ 1140:0139 mode
 *     0x8004 + Replay_DumpMapText @ 1180:137f map text, then
 *     Replay_PlayLog @ 1180:02d3 + save_file_close @ 1140:037c; else
 *     play: Replay_PlayLog + Sound_PlayId @ 1150:00c9 + FUN_11f0_4ea0 +
 *     FUN_1038_0c1d (now Sys_WaitForInput); then Replay_CloseView @
 *     1180:0297 teardown).
 *   Replay_PromptMode @ 1180:017a (DAT_1390_0098 == 0 -> mode =
 *     Dialog_ShowMessage @ 1220:00d2 "See Replay? ..." (0 = cancel, else
 *     play/record pick); else mode = 4; mode != 0 -> Dlg_CreateTracked @
 *     1098:124f view + GfxPort_Create @ 10d0:0000 0x140x200).
 *   Replay_CloseView @ 1180:0297 (Dlg_DestroyTracked @ 1098:1612 +
 *     FUN_10d0_0172 port teardown; always runs after the log).
 * Mode gates below are LIVE 1:1 on explicit params (dialog/file/GDI
 * tails stay in the replay_open_or_play STUB). */

/* Orig Replay_OpenOrPlay @ 1180:0000 dispatch half -> replay_mode_is_record.
 * 1:1: nonzero (1) when mode == 3 (record-to-file), 0 otherwise. */
int replay_mode_is_record(int mode);

/* Orig Replay_PromptMode @ 1180:017a gate half -> replay_mode_needs_view.
 * 1:1: nonzero (1) when mode != 0 (Dlg_CreateTracked view path), 0 when
 * mode == 0 (cancel, orig returns before the view). */
int replay_mode_needs_view(int mode);

/* Orig Replay_PlayLog @ 1180:02d3 entry framing -> replay_entry_len.
 * Fixed payload framing per kind (excludes the 2 kind/turn bytes):
 * 1/9: 4, 2/3: 1, 5/6/8/10/0xd: 2, 0xb: 3, 0xc/others: -1 (variable or
 * unknown; 0xc walks 4 rows of nibble picks). Returns the count or -1.
 * No globals, no exit(). */
int replay_entry_len(int kind);

/* Orig Replay_ShowEntry @ 1180:0e78 -> replay_show_entry.
 * Per-entry display tail (Text_SetColor when entry civ == player +
 * file-write when mode == 3 via handle @ 0xb1a6 + map blit via
 * FUN_11e8_0504/FUN_10d8_01b0 + PresentRect). GDI/file tail -> STUB. */
civ_error_t replay_show_entry(int entry_civ, int player_civ, int mode);

/* Orig Replay_OpenOrPlay @ 1180:0000 -> replay_open_or_play.
 * Record/play dispatcher (Replay_PromptMode @ 1180:017a dialog + mode 3
 * file open/close via save_file_open @ 1140:0139 + Replay_PlayLog +
 * Replay_CloseView @ 1180:0297 teardown). COMMDLG/file/dialog tail -> STUB. */
civ_error_t replay_open_or_play(void);

/* Orig Replay_ShowCivDestroyed @ 1180:17e9 -> replay_show_civ_destroyed.
 * Civ-destroyed animation (Art frames + Res 0x39d5 + Gfx_DrawTechIcon +
 * Sound_PlayId + year/civ text via 10d8:046e + PresentRect). GDI/audio
 * tail -> STUB. Param civ is the destroyed civ index. */
civ_error_t replay_show_civ_destroyed(int civ);

/* --- Chain 2026-10-16 (28): ocean-link rebuild after load (goal 5) ---
 * Orig save_deserialize @ 10f8:0000 tail (decompiled this pass):
 *   mem_copy_far(wire+0x93e0/0xa8ba -> g_segMapBuf:g_offMapBuf, 64000 B) +
 *   Save_GetLogTag + Map_BuildOceanLinks(0) @ 1028:162c + DAT_1310_0002 = 2
 *   + DAT_1420_14e6 = 0xfffd. Po co linki z save: .sav NIE ma linkow —
 *   rozmiary 0x1a2ba/0x1a2b8/0x18de0 to mapa (64000 B) + stan, a linki
 *   (0x104 B ocean w 1248:0108 + 0x104 B continent w 1248:0004) sa POCHODNE
 *   mapy (ten sam grid 0x14 x 0x0d co worldgen_build_*), wiec po loadzie
 *   ocean REBUILD (continent NIE — zostaje z worldgenu? orig wola tylko
 *   OceanLinks; continent links w save-sciezce nie odbudowane, TBD czy
 *   sa w .sav czy po prostu nieuzywane po loadzie). Predykat ponizej LIVE
 *   1:1 (no Win16, no globals); sama odbudowa (worldgen_build_ocean_links
 *   STUB + map scatter) caller-side. No exit(), C17. */

/* Orig save_deserialize @ 10f8:0000 rebuild half ->
 * save_needs_ocean_rebuild. 1:1 1 = zawsze rebuilduj ocean-linki po loadzie
 * (orig wola bezwarunkowo po mem_copy_far; brak gate na rozmiar/typ). */
int save_needs_ocean_rebuild(void);

#endif /* CIVLIN_SAVE_H */
