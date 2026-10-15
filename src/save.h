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
 *       wraps _LOPEN via FUN_1000_2ad9
 *     Orig: FUN_1140_0245 @ 1140:0245 -> save_file_write
 *       chunked via thunk _HWRITE/_LWRITE
 *     Orig: FUN_1140_0295 @ 1140:0295 -> save_file_size
 *       wraps _LLSEEK
 *     Orig: FUN_1140_019d @ 1140:019d -> save_file_read
 *       chunked 0x8000 via _HREAD/_LREAD
 *     Orig: FUN_1140_037c @ 1140:037c -> save_file_close
 *       wraps _LCLOSE via FUN_1000_1f7c
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

/* Orig: FUN_1000_2ad9 @ 1000:2ad9 -> File_OpenWrapper.
 * Thin _LOPEN wrapper used by save_file_open @ 1140:0139. Linux
 * equivalent: fopen(path, write_mode ? "wb" : "rb"); write_mode 0 =
 * OF_READ, nonzero = OF_WRITE/OFCREATE (exact OF_ flag mapping TBD, see
 * Ghidra). Returns the stream, NULL on bad args or fopen failure. */
FILE *file_open_wrapper(const char *path, int write_mode);
/* Orig: FUN_1000_1f7c @ 1000:1f7c -> File_CloseWrapper.
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
 * Packet N: marshal + RLL + file wiring (stubs until live ctx mapped).
 * save_serialize @ 10f8:02bd flow: alloc 0x1a2ba image (LocalAlloc,
 *   err 1420:2cac "Could not allocate buffer for saving") -> marshal via
 *   save_marshal_fields @ 10f8:090c (direction save) -> RLL-encode via
 *   save_pack @ 10f8:05da (save_rll_encode core) -> save_file_open O_WRITE
 *   @ 1140:0139 -> save_file_write @ 1140:0245 -> save_file_close @
 *   1140:037c. save_deserialize @ 10f8:0000 flow: GLOBALALLOC 0xa2ba
 *   scratch -> save_file_open O_READ -> save_file_size @ 1140:0295 (accept
 *   0x1a2ba / 0x1a2b8 / 0x18de0, else err 1420:2c74 "Could not load saved
 *   game. Corrupted or out of memory.") -> save_file_read @ 1140:019d ->
 *   RLL-decode via save_unpack @ 10f8:040a (save_rll_decode core) ->
 *   marshal via save_marshal_fields (direction load) -> close. Both are
 *   UNSUPPORTED stubs: RLL codec + file I/O below are ported, live-ctx
 *   marshal wiring is TBD, no globals taken here. */

/* Orig: FUN_10f8_02bd @ 10f8:02bd, now save_serialize — pack + write .sav.
 * Stub: validates path (NOENT on NULL/empty), else UNSUPPORTED; read-only
 * (no file touched until wiring lands). */
civ_error_t save_serialize(const char *path);
/* Orig: FUN_10f8_0000 @ 10f8:0000, now save_deserialize — read + unpack .sav.
 * Stub: validates path (NOENT on NULL/empty), else UNSUPPORTED; read-only.
 * Size gate lives here when wired (not in save_file_size): 0x1a2ba
 * canonical, 0x1a2b8 alt (cond word), 0x18de0 small-map. */
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

/* --- RLL codec, native form (new API, implemented next) --- */

civ_error_t save_rll_encode(const uint8_t *src, size_t src_len,
                            uint8_t *dst, size_t dst_cap, size_t *out_len);
civ_error_t save_rll_decode(const uint8_t *src, size_t src_len,
                            uint8_t *dst, size_t dst_cap, size_t *out_len);

/* --- field marshaller (game ctx <-> flat image) ---
 * Packet N: save_marshal_fields @ 10f8:090c is the ordered xfer driver. Stub:
 *   direction via g_nSaveDirection @ 1310:0008 (0 = save: live -> image,
 *   1 = load: image -> live), small/large variant via g_nSaveVariant @
 *   1310:0000 (pre-word at image +0x0, live 1310:0000), image cursor via
 *   g_nSaveCursorOff @ 1420:a2d6 with base @ 1420:a2da. Ordered calls below
 *   (image offsets CIV_SAVE_OFF_*): words/block/array/strided then the
 *   0xfa00 map block at +0xa8ba. No globals taken here (caller-owned image
 *   + cap); live segments 12d8/12d0/1248/1290/1298/1308/1350/1388/1390/
 *   13c0/13e8/1420 have no Linux mapping yet -> UNSUPPORTED. */
/* Orig: FUN_10f8_090c @ 10f8:090c, now save_marshal_fields.
 * Direction by Ghidra g_nSaveDirection @ 1310:0008 (0=save,1=load);
 * variant by g_nSaveVariant @ 1310:0000. Cursor g_nSaveCursorOff @
 * 1420:a2d6, base g_nSaveBaseOff @ 1420:a2da.
 * Stub: NOENT on NULL image, TRUNCATED when cap < CIV_SAVE_BUF_MIN
 * (smallest accepted wire size), else UNSUPPORTED; read-only (no write
 * on stub path). */
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

/* Livectx segments (game state scatter). 12d8 is the main ctx. */
#define CIV_SEG_GAME_MAIN 0x12d8u
#define CIV_SEG_GAME_SYS 0x12d0u
#define CIV_SEG_GAME_AUX 0x1290u

#endif /* CIVLIN_SAVE_H */
