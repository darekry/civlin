/* CivLin RSC resource manager (resmgr.c port, W2: load walk implemented).
 *
 * Ghidra anchors (civ.exe, NE Win3.x), renamed 2026-10-15/16:
 *   ResMgr_LoadRsc      @ 1190:0000  (OpenResFile; _LOPEN + _HREAD 0xE360
 *                         into GLOBALALLOC buffer; parses type dir +
 *                         per-record headers; 5x from WinMain @ 1008:0000
 *                         with 1420:00b1/00be/00cb/00d8/00e5 = CivData0..4)
 *   ResMgr_LoadRscDisk  @ 1190:0501  (same via FUN_1000_* streaming file
 *                         API; fallback when GLOBALCOMPACT < 1500000)
 *   ResMgr_AddFile      @ 1190:1692  (AddRFile; 0x12-B file node -> list
 *                         @ DAT_1420_b2c0; reads u32+u32 then u16 count)
 *   ResMgr_AddFileDisk  @ 1190:1570  (disk variant of AddRFile)
 *   ResMgr_AddType      @ 1190:13a8  (AddType; 10-B type node -> list
 *                         @ DAT_1420_b2c2; 4-char tag compare)
 *   ResMgr_NewResource  @ 1190:12ee  (NewResource; 0x32-B res node, links
 *                         into type list, count at +4)
 *   ResMgr_ReadU16      @ 1190:1953  (buffer cursor u16 LE; mult table
 *                         @ 0x3d00: 1, 0x100)
 *   ResMgr_ReadU16b     @ 1190:19c9  (identical twin, name-offset slot)
 *   ResMgr_ReadU32      @ 1190:1a3f  (buffer cursor u32 LE; 1,100,10000..)
 *   ResMgr_ReadU16Disk  @ 1190:1abe / ResMgr_ReadU16Disk2 @ 1190:1b47 /
 *   ResMgr_ReadU32Disk  @ 1190:1bcb  (disk variants via FUN_1000_272a)
 *   ResMgr_CmpType      @ 1190:1533  (4-byte tag equality)
 *   ResMgr_CmpName      @ 1190:0e72  (case-insensitive via 1000:1978,
 *                         0x1f chars max + NUL/NUL stop)
 *   ResMgr_FindType     @ 1190:121d / ResMgr_FindFile @ 1190:1180 /
 *   ResMgr_FindByHandle @ 1190:0ee5  (list walks)
 *   ResMgr_GetTypeCount @ 1190:0978 / ResMgr_GetIndexed @ 1190:0a29 /
 *   ResMgr_GetById      @ 1190:0aea / ResMgr_GetByName @ 1190:0bbe
 *   ResMgr_LoadToMem    @ 1190:0fda  (RestoMem; _LLSEEK + _HREAD payload,
 *                         sets loaded flag +0x2d, handle +0x2e)
 *   ResMgr_ZapRes       @ 1190:10e6  (ZapRes; frees +0x2e)
 *   ResMgr_Release      @ 1190:113f / ResMgr_Acquire @ 1190:09cd /
 *   ResMgr_DisposeHandle@ 1190:09ed / ResMgr_Dispose @ 1190:0c92
 *                         (CDisposeResourceMgr; _LCLOSE + free all lists)
 *   ResMgr_DumpStats    @ 1190:17d0  (debug dump via 1000:2c9c)
 *
 * Display path (who consumes resources):
 *   GfxPort_Create      @ 10d0:0000  (GR_PortOpen; GETDC + CREATECOMPATIBLEDC
 *                         + bitmap via 10d0:0cfa; port struct ~0x20 words)
 *   GfxPort_SetActive   @ 10d0:0956 / GfxPort_PresentRect @ 10d0:0854
 *                         (BITBLT 0xcc0020 + VALIDATERECT) /
 *   GfxPort_Blit        @ 10d0:08f1 / GfxPort_ChangeColor @ 10d0:09ee
 *                         (GR_PortChangeColor; GET/SETBITMAPBITS color remap)
 *   Palette_Init        @ 1090:0488  (LOGPALETTE 0x300, 0xec entries ->
 *                         CREATEPALETTE @ DAT_1420_9966)
 *   Palette_AnimateRange@ 1090:0376  (-> ANIMATEPALETTE; see video.h)
 *   Sound_MapIdToWav    @ 1150:019e / Sound_PlayId @ 1150:00c9 (MCI +
 *                         SNDPLAYSOUND; see audio.h)
 *   Font_EnumFamiliesInit@1008:0e99 + ADDFONTRESOURCE(civfonts.fon @
 *                         1420:00a4/00fe); WINHELP(civhelp.hlp); civ.prf
 *                         via Config_LoadPrefs @ 1008:0b4a (see boot.h).
 *
 * Framing learned from the loader (replaces the v1 probe guesses):
 *   - Custom resmgr container, NOT Mac HFS resources, NOT NE resources
 *     (file(1) "HFS resource fork" hit is a false positive; "rsrcRSED"
 *     in civdata0 is just another 4-char type tag family).
 *   - File header 16 B: magic 00 00 01 00 @ +0, u32 unk0 BE @ +4, u32
 *     unk1 BE @ +8, u32 dir_size BE @ +12 (dir_size = file_size -
 *     dir_off; observed: 358/766/3621/63/10608 for CivData1/2/3/4/0,
 *     all < 0xE360). Directory at EOF (last magic) starts with the same
 *     16-B header copy, then 14-B file-node extra (opaque, skipped),
 *     then per type: 4-char tag + u16 res-count BE + u16 unk BE (8 B),
 *     then per record 12-B on-disk: id BE16 @ +0, name-off BE16 @ +2,
 *     unk u8 @ +4, file-off BE24 @ +5..+7 (zero-extended to u32),
 *     unk BE16 @ +8..+9, len BE16 @ +10..+11 (zero-extended to u32).
 *     In-memory res node is 0x32 B via rsc_new_resource (id @ +0,
 *     name-off @ +2, file-off u32 @ +4, len u32 @ +8, name len u8 @
 *     +0xc capped 0x1f + chars @ +0xd, loaded @ +0x2d, handle @ +0x2e).
 *     All integers BE (orig descending mult-table loop over ascending
 *     LE table @ 1420:3d00 {1,0x100,0x10000,0x1000000}: first byte *
 *     largest power = BE; previous "LE" docs were misread, fixed W2).
 *   - String table at dir end (after all types/records): len-prefixed
 *     names (len u8 + chars, len capped 0x1f); record name-off is the
 *     byte offset into the table (0xffff = no name -> empty).
 *     Payloads before the directory (file_off+len <= dir_off <= size).
 *   - Runtime: file list @ 1420:b2c0, type list @ 1420:b2c2, cursor pair
 *     @ 1420:b2b4/b2b8 (+seg @ b2b6/b2ba), file-count @ 1420:b2be,
 *     type-count @ 1420:b2bc. Lazy payload load (LoadToMem) + Zap/Release.
 *
 * What this module does now (W2):
 *   - DOES (OS-free, ported 1:1, BE fixed W2): rsc_read_u16le /
 *     rsc_read_u32le / rsc_cmp_type / rsc_cmp_name (ResMgr_ReadU16/U32,
 *     CmpType/CmpName, cursor-advancing, no Win16; _le suffix retained
 *     for RE-map fidelity, actually BE per descending loop).
 *   - DOES (v1 probe, kept for listing): rsc_open() validates magic
 *     00 00 01 00 + marker, scans the first 1 KiB for 8-byte tags
 *     (bounded, read-only); record_count/info still reflect the scan.
 *   - DOES (W2, buffered 0xE360): rsc_load_rsc() reads the EOF directory
 *     (dir_size <= 0xE360, malloc, no GLOBALALLOC) via File_ReadChunk
 *     and walks u32/u32/u16-equivalent + per-type tag/count + 12-B
 *     records into rsc_add_file/type/new_resource (read-only, validated
 *     vs all 5 CivData sizes, no exit/globals/SDL).
 *   - DOES (W2, streaming twin): rsc_load_rsc_disk() same walk streamed
 *     via rsc_read_u16_disk/disk2/u32_disk (taken when GLOBALCOMPACT <
 *     1500000 in orig).
 *   - DOES (W2): rsc_load_to_mem() via fseek (file_seek_wrapper/_LLSEEK
 *     equiv) + file_read_chunk + malloc, sets loaded @ +0x2d = 1 and
 *     handle @ +0x2e (non-zero unique, 0 never matches find_by_handle).
 *   - DOES (W2, minimal slice): rsc_record_data() prefers the directory
 *     slice (file-off/len from the 12-B header -> u32 @ +4/+8 in the
 *     node, bounds file_off+len <= size, borrowed, valid while rsc_file
 *     lives; len 0 derives from next file-off in the same type, last to
 *     dir_off); scan kept as listing fallback, slice failures return
 *     UNSUPPORTED/TRUNCATED/RANGE (no copy, no exit/globals/SDL).
 *   - DOES NOT yet: GfxPort BITBLT bridge, palette animation
 *     (ANIMATEPALETTE -> SDL_SetPaletteColors in video.c), sound/font
 *     wiring. Those stay stubs with TODO Orig addresses.
 *
 * Y2 dump validation (read-only, OS-free, no load-walk changes):
 *   - rsc_dump.c opens each of the 5 CivData0..4.rsc via rsc_open()
 *     (magic 00 00 01 00 @ +0 + marker "Civ Data "/"rsrcRSED"/"CvMM")
 *     plus rsc_load_rsc() (ResMgr_LoadRsc @ 1190:0000 buffered 0xE360
 *     EOF-dir walk); largest observed is Civdata3.rsc ~1.4 MiB.
 *   - Per-file report: size via rsc_size(), record_count via
 *     rsc_record_count() (v1 scan listing), image_count via
 *     rsc_image_count() (X1 strict gate below) plus dir_records/slices/
 *     images_strict from the rsc_record_data() + rsc_decode_bitmap()
 *     walk and type/res totals via rsc_dump_stats().
 *   - Strict gate (already ported, reused here, not reimplemented):
 *     borrowed view (pixels points into the container, valid while
 *     rsc_file lives; palette copied, tail zeroed) + pal_count in
 *     1..255 + every pixel index < pal_count (see rsc_decode_bitmap @
 *     1198:xxxx and rsc_get_indexed @ 1190:0a29). Non-images skip
 *     (TRUNCATED/RANGE/BAD_MAGIC), never fail validation.
 *
 * Z1 truncated root-cause (read-only diagnosis, 2026-10-15, unpacked/):
 *   rsc_dump slices OK vs total: civdata0 181519 B dir 10608 B 457 total
 *     -> 454 slices OK (3 TRUNCATED: STR# idx 9/15/25 file_off+len > size);
 *     Civdata3 1433655 B dir 3621 B 149 total -> 143 OK (6 TRUNCATED:
 *     CvPc idx 129/136/145/146/147/148, ends up to +31996 past size);
 *     CIVDATA1 729188 B dir 358 B 14/14 OK; CIVDATA2 959728 B dir 766 B
 *     31/31 OK; CIVDATA4 144102 B dir 63 B 1/1 OK. Invariant violated is
 *     file_off+len <= size (payload ends past EOF), not dir_size > 0xE360
 *     (all dirs 63..10608 < 0xE360=58272) and not BE/LE header misread
 *     (LE dir_size is 0x66010000..0xFE020000 huge > size, always invalid;
 *     BE is the only valid header endian). Last-to-dir_off vs last-to-EOF:
 *     len==0 last-in-type derives to dir_off in W2; Z1 extends truncated
 *     last-in-type/file to EOF (size) so a short dir still yields a usable
 *     tail slice instead of TRUNCATED.
 *   Variant table (Orig FUN_seg_off @ seg:off kept; _le suffix = BE impl):
 *     family     | probe tag  | unknown_a LE   | unknown_a BE   | unknown_b LE
 *     rsrcRSED   | rsrcRSED   | 0xffff0001     | 0x0100ffff     | 0x0002ffff/0x0000ffff
 *       (civdata0.rsc 181519 B; 5 types STR#(32,unk 0x002a)/TEXT(398,0x01b6)/
 *        GDAT(6,0x146a)/KDAT(13,0x14be)/CvPc(3,0x1566); 12-B rec on-disk,
 *        same BE24@+5/BE16@+10 as CvD*, u32@+4/+8 in-memory node only)
 *     CvD1CvMM   | CvD1CvMM   | 0xc6000001     | 0x010000c6     | 0x00008406
 *       (CIVDATA1.RSC 729188 B; 1 type CvPc cnt13 unk 0x000a; 14/14 OK)
 *     CvD2CvMM   | CvD2CvMM   | 0xc6000001     | 0x010000c6     | 0x0000b008
 *       (CIVDATA2.RSC 959728 B; 1 type CvPc cnt30 unk 0x000a; 31/31 OK)
 *     CvD3CvMM   | CvD3CvMM   | 0x00000001     | 0x01000000     | 0x00000000
 *       (Civdata3.rsc 1433655 B; 1 type CvPc cnt148 unk 0x000a; 143/149 OK)
 *     CvD4CvMM   | CvD4CvMM   | 0xc6000001     | 0x010000c6     | 0x0000dc0a
 *       (CIVDATA4.RSC 144102 B; 1 type CvPc cnt0 unk 0x000a; 1/1 OK)
 *   On-disk vs in-memory duality (unified Z1, no variant split on-disk):
 *     on-disk 12-B rec (BOTH families): id BE16 @ +0, name-off BE16 @ +2,
 *       unk u8 @ +4, file-off BE24 @ +5..+7, unk BE16 @ +8..+9,
 *       len BE16 @ +10..+11 (helpers rsc_rec_file_off/rsc_rec_len).
 *     in-memory 0x32-B node: file-off u32 @ +4, len u32 @ +8 (zero-extended;
 *       see rsc_new_resource). The "u32@+4/+8" form never appears on-disk;
 *       readers that tried u32 on-disk got huge offsets (e.g. STR# rec0
 *       0x20000000) and false truncations. Z1 helpers read BE24/BE16 only.
 *   Per-type 8-B: 4-char tag + res-count BE16 + unk BE16. rsrcRSED unks
 *     (42/438/5226/5310/5478) are larger than CvD* (10) but still just
 *     opaque per-type words (not counts); res-count vs unk check passes
 *     (count+1 records fit in remaining dir) for both families, so no
 *     per-type size split is needed.
 *   Buffer/fallback (Orig GLOBALCOMPACT @ 1000:xxxx < 1500000):
 *     buffered ResMgr_LoadRsc @ 1190:0000 uses one 0xE360 read; streaming
 *     ResMgr_LoadRscDisk @ 1190:0501 uses File_ReadChunk @ 1000:272a loop.
 *     Z1: dir_size > 0xE360 (or file_size >= 1500000) selects streaming via
 *     rsc_dir_needs_streaming()/rsc_load_rsc_auto(); large dirs are read by
 *     rsc_read_dir_chunked() in 0xE360 chunks, not one fread.
 *
 * Rules: read-only against civ1win//unpacked (never writes there), no
 * exit() in the library, no globals, C17, SDL3 only in gfx_indexed (this
 * file has zero SDL dependency).
 */
#ifndef CIVLIN_RSC_LOADER_H
#define CIVLIN_RSC_LOADER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum rsc_error {
    RSC_OK = 0,
    RSC_ERR_IO = 1,          /* fopen/fread/stat failed */
    RSC_ERR_NOMEM = 2,       /* malloc failed */
    RSC_ERR_BAD_MAGIC = 3,   /* first 4 bytes != 00 00 01 00, or no marker */
    RSC_ERR_TRUNCATED = 4,   /* file smaller than minimum header */
    RSC_ERR_NOENT = 5,       /* bad path / null out pointer */
    RSC_ERR_RANGE = 6,       /* record index out of range */
    RSC_ERR_UNSUPPORTED = 7  /* known file, feature not yet decoded */
} rsc_error_t;

/* One directory entry found by the v1 tag scan. unknown_a/unknown_b are the
 * raw little-endian u32 pair following the 8-byte tag, stored verbatim. */
typedef struct rsc_record {
    char type[9];            /* NUL-terminated 8-byte tag, e.g. "CvD1CvMM" */
    uint32_t entry_offset;   /* file offset where the tag was found */
    uint32_t unknown_a;
    uint32_t unknown_b;
} rsc_record_t;

/* Indexed image view (borrowed pixels+palette, valid while rsc_file lives).
 * pal entries are R,G,B bytes (no alpha in source format). */
typedef struct rsc_indexed {
    uint32_t width;
    uint32_t height;
    uint8_t bpp;             /* always 8 for now */
    uint8_t pal_count;       /* 0..255have; 0 means "no palette attached" */
    uint8_t pal[256][3];
    const uint8_t *pixels;   /* width*height indices */
} rsc_indexed_t;

/* Buffer cursor for the OS-free integer readers below. Mirrors the
 * DAT_1420_b2b8 advance in ResMgr_ReadU16/U32 (segment arithmetic
 * dropped; plain linear cursor on Linux). */
typedef struct rsc_cursor {
    const uint8_t *ptr;
    const uint8_t *end;
} rsc_cursor_t;

typedef struct rsc_file rsc_file_t;

const char *rsc_strerror(rsc_error_t err);

/* OS-free ports (1:1 from Ghidra, no Win16):
 *   rsc_read_u16le -> ResMgr_ReadU16 @ 1190:1953 (and twin 1190:19c9)
 *   rsc_read_u32le -> ResMgr_ReadU32 @ 1190:1a3f
 *   rsc_cmp_type   -> ResMgr_CmpType @ 1190:1533 (4-byte tag equality)
 *   rsc_cmp_name   -> ResMgr_CmpName @ 1190:0e72 (case-insensitive,
 *                     0x1f chars max, NUL/NUL stops; tolower via
 *                     FUN_1000_1978 in orig, plain ASCII here)
 * Endian (fixed W2): orig loops descend (1->0 for U16, 3->0 for U32)
 * over the ascending LE table @ 1420:3d00 {1,0x100,0x10000,0x1000000}
 * (verified via 1420:3d00 bytes 01 00 00 00 00 01 00 00 ...), so the
 * first byte takes the largest power = big-endian. The _le suffix is
 * retained for RE-map fidelity; the implementation is BE to validate
 * against all 5 CivData files (LE gave huge file-offs > sizes).
 * Readers return 0 on truncated cursor (orig callers never hit this;
 * _HREAD always fills 0xE360). */
uint16_t rsc_read_u16le(rsc_cursor_t *cur);
uint32_t rsc_read_u32le(rsc_cursor_t *cur);
int rsc_cmp_type(const uint8_t tag_a[4], const uint8_t tag_b[4]);
int rsc_cmp_name(const char *a, const char *b);

/* Orig: FUN_1000_272a @ 1000:272a -> File_ReadChunk.
 * Disk-streaming read behind ResMgr_ReadU16Disk @ 1190:1abe,
 * ResMgr_ReadU16Disk2 @ 1190:1b47 and ResMgr_ReadU32Disk @ 1190:1bcb
 * (orig via _HREAD/_LREAD on the resource file handle). Linux
 * equivalent: fread(fp, dst, len); returns bytes actually read (short
 * on EOF/error, 0 on NULL fp/dst or len == 0). */
size_t file_read_chunk(FILE *fp, uint8_t *dst, size_t len);

/* Loads the whole file into memory (largest observed: Civdata3.rsc,
 * ~1.4 MiB — fine). Returns RSC_ERR_*; *out is NULL on error. */
rsc_error_t rsc_open(const char *path, rsc_file_t **out);
void rsc_close(rsc_file_t *f);

/* Whole-file info. */
size_t rsc_size(const rsc_file_t *f);
const char *rsc_path(const rsc_file_t *f);

/* Directory entries found by the v1 scan. */
size_t rsc_record_count(const rsc_file_t *f);
rsc_error_t rsc_record_info(const rsc_file_t *f, size_t i, rsc_record_t *out);

/* Payload carving: IMPLEMENTED minimal slice (W2, prefers slice over the
 * v1 1 KiB tag-scan probe; scan kept for record_count/info listing; Z1
 * truncated-last-to-EOF + duality unified). Walks the EOF directory in
 * f->data (BE, same framing as rsc_load_rsc; header BE32 variant-tolerant,
 * LE only if BE dir_size invalid): file header 16 B (magic + unk0 BE32 +
 * unk1 BE32 + dir_size BE32), 14-B file extra (skipped), per-type 8 B (tag
 * + count BE16 + unk BE16, count+1 records per orig <=count+1U loops),
 * per-record 12-B on-disk via rsc_rec_file_off/rsc_rec_len (id BE16 @ +0,
 * name-off BE16 @ +2, unk u8 @ +4, file-off BE24 @ +5..+7 zero-extended to
 * u32 @ +4 in the node, unk BE16 @ +8..+9, len BE16 @ +10..+11
 * zero-extended to u32 @ +8 for BOTH families; u32 @ +4/+8 is the
 * in-memory node only, never on-disk; len 0 derives from the next
 * file-off in the same type, last to EOF = rsc_size (Z1; W2 used dir_off
 * so truncated tails still carve to EOF instead of TRUNCATED). Validates
 * i < dir_total (RANGE), file_off+len <= rsc_size overflow-safe
 * (TRUNCATED; truncated non-last records stay TRUNCATED, truncated last
 * clamps to EOF), returns the borrowed slice f->data+file_off (valid while
 * rsc_file lives, no copy, empty slice with size 0 when len is 0 and no
 * next/EOF extends it). Directory-walk failures (bad magic/dir_size/
 * dir_off) return UNSUPPORTED (scan listing still available); NULL args
 * return NOENT. Read-only, no exit/globals/SDL. */
rsc_error_t rsc_record_data(const rsc_file_t *f, size_t i,
                            const uint8_t **data, size_t *size);

/* Indexed images: IMPLEMENTED (X1, strict gate, OS-free, no SDL).
 * Producer chain: rsc_record_data slice must first succeed (directory
 * file-off/len, bounds file_off+len <= size, borrowed, valid while
 * rsc_file lives; len 0 derives from next file-off, last to EOF/Z1);
 * then rsc_decode_bitmap validates the 8-bit image + palette (width > 0,
 * height > 0, bpp == 8, pal_count in 1..255 with 3*pal_count bytes
 * present, width*height indices present, every pixel index < pal_count).
 * rsc_image_count counts slices that pass that strict gate (0 on NULL
 * file or directory-walk failure); rsc_get_indexed returns the borrowed
 * view for record i (pixels points into the container, palette copied
 * with tail zeroed). gfx_indexed_to_surface enforces the same 8-bit +
 * palette checks on its input (see gfx_indexed.h). */
size_t rsc_image_count(const rsc_file_t *f);
rsc_error_t rsc_get_indexed(const rsc_file_t *f, size_t i, rsc_indexed_t *out);

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_DecodeBitmap.
 * Neighbour of the 1190 resmgr cluster; decodes one 0x32-B-header payload
 * slice (file-off @ +4 / len @ +8, see rsc_record_data plan) into an
 * rsc_indexed view feeding BITBLT/PALETTE (display side is
 * GfxPort_LoadRscFrame in video.h). Payload layout (all LE): u16 width
 * @ +0, u16 height @ +2, u8 bpp @ +4 (must be 8), u8 pal_count @ +5
 * (1..255), pal_count*3 RGB bytes @ +6, then width*height 8-bit indices.
 * OS-free 1:1 port (no Win16, no SDL, no exit, no globals): validates
 * bounds (6 + 3*pal_count + w*h <= size, overflow-safe), 8-bpp +
 * pal 1..255, and every pixel < pal_count; on success fills *out with a
 * borrowed view (pixels points into data, valid while the container
 * lives; palette copied, tail zeroed). Returns RSC_ERR_NOENT on NULL
 * data/out, RSC_ERR_TRUNCATED when the slice is short, RSC_ERR_BAD_MAGIC
 * on bad bpp/pal/dims, RSC_ERR_RANGE on zero dims / pixel out of range. */
rsc_error_t rsc_decode_bitmap(const uint8_t *data, size_t size,
                              rsc_indexed_t *out);

/* --- AA1: per-type payload decoders (segment 1198, OS-free) ---
 * Context (read-only, no asset writes): civdata0 rsrcRSED 5 types
 *   STR#(32,unk 0x002a)/TEXT(398,unk 0x01b6)/GDAT(6,unk 0x146a)/
 *   KDAT(13,unk 0x14be)/CvPc(3,unk 0x1566); CIVDATA1/2/3/4 CvD* CvPc
 *   cnt 13/30/148/0. Dir_records 454/14/31/144/1 all slices OK via
 *   rsc_record_data (borrowed slice, valid while rsc_file lives);
 *   images_strict=0 (no raw INDEX8 at slice start via rsc_decode_bitmap
 *   @ 1198:xxxx strict gate). Payloads look type-tagged/RLE, not raw.
 * Caller plan (read-only, caller-side e.g. rsc_dump.c so this file stays
 *   SDL-free): slice via rsc_record_data(f,i,&data,&size), then log first
 *   16 B via gfx_probe_head_hex(head,16) (gfx_indexed.h, OS-free, no SDL,
 *   borrowed slice, never writes civ1win//unpacked/), then dispatch below
 *   by 4-char tag via rsc_cmp_type (ResMgr_CmpType @ 1190:1533). Rules:
 *   C17, zero SDL, no exit(), no globals, read-only borrowed slices. */

/* STR# string-list view (borrowed, valid while the container lives).
 * body points at the first len byte after the u16 count (data+2),
 * body_len is size-2. Caller walks: len = body[p]; bytes = body[p+1..p+len]. */
typedef struct rsc_strlist {
    uint16_t count;
    const uint8_t *body;
    size_t body_len;
} rsc_strlist_t;

/* TEXT string view (borrowed, valid while the container lives).
 * text points into data; text[len] is the terminating NUL inside the slice. */
typedef struct rsc_text {
    const char *text;
    size_t len;
} rsc_text_t;

/* GDAT table view (reserved for the 1198 table layout; stub fills nothing). */
typedef struct rsc_gdat {
    const uint8_t *body;
    size_t body_len;
} rsc_gdat_t;

/* KDAT table view (reserved for the 1198 table layout; stub fills nothing). */
typedef struct rsc_kdat {
    const uint8_t *body;
    size_t body_len;
} rsc_kdat_t;

/* CvPc frame view (reserved for the 1198 RLE/tagged frame; stub fills nothing). */
typedef struct rsc_cvpc {
    const uint8_t *body;
    size_t body_len;
} rsc_cvpc_t;

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_DecodeStrList.
 * STR# payload layout (Pascal strings, count BE to match rsc_read_u16le BE
 * via ResMgr_ReadU16 @ 1190:1953): u16 count BE @ +0, then count entries of
 * (u8 len + len bytes). OS-free 1:1 port (no Win16, no SDL, no exit, no
 * globals): overflow-safe walk, trailing bytes after the list are allowed
 * (ignored). Borrowed view (body into data, valid while container lives).
 * Returns RSC_ERR_NOENT on NULL data/out, RSC_ERR_TRUNCATED when size < 2
 * or any entry overruns the slice, RSC_OK otherwise. */
rsc_error_t rsc_decode_strlist(const uint8_t *data, size_t size,
                               rsc_strlist_t *out);

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_DecodeText.
 * TEXT payload layout (one dialog string per slice hypothesis; 398 recs,
 * unk 0x01b6; e.g. menu @ 1420:435b, difficulty @ 2a5a): single
 * NUL-terminated C string (len = NUL offset within size). OS-free port
 * (no Win16, no SDL, no exit, no globals): memchr for the NUL inside the
 * borrowed slice; multi-string records (embedded NULs) still return the
 * first string, caller may rescan text[len+1..]. Returns RSC_ERR_NOENT on
 * NULL data/out, RSC_ERR_TRUNCATED when size == 0, RSC_ERR_BAD_MAGIC when
 * no NUL is present within size, RSC_OK otherwise (borrowed, valid while
 * container lives). */
rsc_error_t rsc_decode_text(const uint8_t *data, size_t size,
                            rsc_text_t *out);

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_DecodeGdat.
 * GDAT payload layout TBD (6 recs, unk 0x146a; game data tables hypothesis:
 * terrain tables? cf. weights @ 1420:17d6). Needs Ghidra compare vs the
 * 1198 decoder before any field split (endian/stride unknown; BE count
 * assumed only for the dir layer, not the payload). Stub: validates args
 * (NOENT on NULL data/out, TRUNCATED when size == 0) then returns
 * RSC_ERR_UNSUPPORTED with *out untouched (no decode fake). Plan:
 * caller logs the rsc_record_data slice head 16 B via gfx_probe_head_hex
 * (read-only, no asset writes) and compares vs 1198. No SDL, no exit,
 * no globals, C17, borrowed read-only slice. */
rsc_error_t rsc_decode_gdat(const uint8_t *data, size_t size,
                            rsc_gdat_t *out);

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_DecodeKdat.
 * KDAT payload layout TBD (13 recs, unk 0x14be; table hypothesis, stride
 * unknown). Needs Ghidra compare vs the 1198 decoder before any field
 * split. Stub: validates args (NOENT on NULL data/out, TRUNCATED when
 * size == 0) then returns RSC_ERR_UNSUPPORTED with *out untouched (no
 * decode fake). Plan: caller logs the rsc_record_data slice head 16 B
 * via gfx_probe_head_hex (read-only, no asset writes) and compares vs
 * 1198. No SDL, no exit, no globals, C17, borrowed read-only slice. */
rsc_error_t rsc_decode_kdat(const uint8_t *data, size_t size,
                            rsc_kdat_t *out);

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_DecodeCvPc.
 * CvPc payload layout TBD (picture hypothesis: 3/13/30/148/0 recs across
 * CivData0..4, likely bitmap frames for intro/pyramid/minimap). Bitmap
 * candidate feeding rsc_decode_bitmap @ 1198:xxxx / gfx_probe_rsc_image
 * (w/h/bpp/pal gate: u16 w + u16 h + u8 bpp==8 + u8 pal 1..255 + RGB +
 * indices); images_strict=0 proves no slice starts with raw INDEX8, so
 * this stub never fakes a bitmap decode. Stub: validates args (NOENT on
 * NULL data/out, TRUNCATED when size == 0) then returns
 * RSC_ERR_UNSUPPORTED with *out untouched. Plan: caller logs the
 * rsc_record_data slice head 16 B via gfx_probe_head_hex (gfx_indexed.h,
 * OS-free, read-only, no asset writes) + probe reason, then Ghidra
 * compares the head vs the 1198 RLE/tagged-frame decoder. No SDL,
 * no exit, no globals, C17, borrowed read-only slice. */
rsc_error_t rsc_decode_cvpc(const uint8_t *data, size_t size,
                            rsc_cvpc_t *out);

/* --- AB1: string/table lookup (segment 1198/1190, OS-free) ---
 * Confirmed from dump head16 (read-only, no asset writes): civdata0
 * rec[3] `0F 54 68 65...` = 0x0F + "The Corporation" 15 B STR# u8 len;
 * rec[1] " 90% Science)" ASCII TEXT; rec[2] "  80% Tax," ASCII TEXT.
 * Container: civdata0 rsrcRSED STR#(32,unk 0x002a)/TEXT(398,unk 0x01b6)/
 * GDAT(6,unk 0x146a)/KDAT(13,unk 0x14be)/CvPc(3,unk 0x1566). Slices are
 * borrowed via rsc_record_data (valid while rsc_file lives). Rules: C17,
 * zero SDL, no exit(), no globals, borrowed slices. */

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_StrListCount.
 * STR# count accessor (32 recs civdata0): u16 BE count @ +0 (BE to match
 * rsc_read_u16le BE via ResMgr_ReadU16 @ 1190:1953). OS-free port: bounds
 * only, no walk. NOENT on NULL data/out, TRUNCATED when size < 2,
 * otherwise *out = BE16(data+0). Borrowed slice, no SDL/exit/globals. */
rsc_error_t rsc_strlist_count(const uint8_t *data, size_t size,
                              uint16_t *out);
/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_StrListGet.
 * STR# index->string via u8 len walk (e.g. rec[3] 0x0F +
 * "The Corporation"): u16 BE count @ +0, then count entries of (u8 len +
 * len bytes); trailing bytes after the list are allowed (ignored, same as
 * rsc_decode_strlist). OS-free port 1:1: overflow-safe walk, bounds on
 * every entry. NOENT on NULL data/str/len and on index OOR
 * (index >= count), TRUNCATED when size < 2 or any visited entry overruns
 * the slice, otherwise borrowed *str (first char, NOT the len byte) +
 * *len (valid while container lives). No SDL/exit/globals. */
rsc_error_t rsc_strlist_get(const uint8_t *data, size_t size,
                            unsigned index, const uint8_t **str,
                            size_t *len);
/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_TextFind.
 * TEXT substring scan (398 recs civdata0, unk 0x01b6; e.g. menu @
 * 1420:435b, difficulty @ 1420:2a5a; observed " 90% Science)",
 * "  80% Tax,"). Portable memmem over the borrowed slice: first offset at
 * which needle occurs, or -1. Returns (long)offset on hit, -1 on miss and
 * on bad args (NULL data/needle, needle_len == 0, needle_len > size).
 * Empty haystack (size == 0) is a miss (-1). No SDL/exit/globals. */
long rsc_text_find(const uint8_t *data, size_t size,
                   const uint8_t *needle, size_t needle_len);
/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_GdatDetail (STUB).
 * GDAT detail (6 recs, unk 0x146a; terrain-table hypothesis: weights @
 * 1420:17d6 *3 + 1420:17d8, moisture @ 1420:12b8/1420:12bc). Field stride
 * TBD: needs Ghidra compare vs the 1198 table decoder before any field
 * split (endian/stride unknown; no fake). Stub: NOENT on NULL
 * data/entry/entry_len, TRUNCATED when size == 0, otherwise
 * RSC_ERR_UNSUPPORTED with *entry / *entry_len untouched. Borrowed slice,
 * no SDL/exit/globals, C17. */
rsc_error_t rsc_gdat_detail(const uint8_t *data, size_t size,
                            unsigned index, const uint8_t **entry,
                            size_t *entry_len);
/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_KdatDetail (STUB).
 * KDAT detail (13 recs, unk 0x14be; table hypothesis, stride unknown).
 * Layout TBD via Ghidra 1198 compare; caller logs the rsc_record_data
 * slice head 16 B via gfx_probe_head_hex (read-only, no asset writes).
 * Stub: NOENT on NULL data/entry/entry_len, TRUNCATED when size == 0,
 * otherwise RSC_ERR_UNSUPPORTED with *entry / *entry_len untouched (no
 * decode fake). Borrowed slice, no SDL/exit/globals, C17. */
rsc_error_t rsc_kdat_detail(const uint8_t *data, size_t size,
                            unsigned index, const uint8_t **entry,
                            size_t *entry_len);
/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_StringLookup (dispatcher).
 * Generic string lookup by 4-char type tag via rsc_cmp_type
 * (ResMgr_CmpType @ 1190:1533). OS-free dispatcher port, borrowed slices:
 * STR# dispatches to rsc_strlist_get (#2, count #1 inside the walk;
 * trailing allowed; NOENT OOR / TRUNCATED passthrough); TEXT with
 * index == 0 returns the whole NUL-terminated string via rsc_decode_text
 * (len = NUL offset; substring search itself stays in rsc_text_find #3,
 * e.g. " 90%" / menu 1420:435b / difficulty 1420:2a5a) and index != 0 is
 * NOENT (single string per slice hypothesis); GDAT dispatches to
 * rsc_gdat_detail #4 (stub UNSUPPORTED); KDAT to rsc_kdat_detail #5
 * (stub UNSUPPORTED); CvPc/unknown tags validate (NOENT on NULL
 * tag/data/str/len, TRUNCATED when size == 0) then UNSUPPORTED with
 * *str / *len untouched (CvPc: no fake, images_strict=0 proves no raw
 * INDEX8 at slice start). No SDL/exit/globals, C17. */
rsc_error_t rsc_string_lookup(const uint8_t tag[4], const uint8_t *data,
                              size_t size, unsigned index,
                              const uint8_t **str, size_t *len);

/* --- Packet E: ResMgr list manager (segment 1190, OS-free 1:1) ---
 * Win16 imports behind this layer (KERNEL):
 *   _LOPEN/_LCLOSE  -> file_open_wrapper/file_close_wrapper (save.h)
 *   _LREAD/_HREAD   -> file_read_chunk (above, FUN_1000_272a @ 1000:272a)
 *   _LWRITE/_HWRITE -> file_write_chunk (sys.h, fwrite equiv)
 *   _LLSEEK         -> file_seek_wrapper (save.h, fseek/ftell equiv)
 *   LOADRESOURCE/FINDRESOURCE/LOCKRESOURCE/UNLOCKRESOURCE/FREERESOURCE
 *     -> rsc_find_type/rsc_get_by_id/rsc_get_by_name/rsc_load_to_mem below
 *     (orig GLOBAL handles; Linux uses malloc'd nodes + payload pointers).
 *
 * Orig node sizes (packed Win16, far ptrs = 4 B):
 *   file node 0x12 B (AddRFile; list @ DAT_1420_b2c0; u32+u32 then u16 count)
 *   type node 0x10 B (AddType; list @ DAT_1420_b2c2; 4-char tag + counts)
 *   res  node 0x32 B (NewResource; id u16 @ +0, name-off u16 @ +2,
 *     file-off u32 @ +4, len u32 @ +8, name-len u8 + chars @ +0xc capped
 *     0x1f, loaded flag @ +0x2d, handle @ +0x2e).
 * Linux structs use native pointers (larger); field ORDER and on-disk
 * offsets (+0x2d/+0x2e etc.) are preserved in comments. No globals:
 * caller owns an rsc_mgr_t (zero-initialised or via rsc_mgr_new) and
 * passes it explicitly (orig globals @ 1420:b2b4/b2b8/b2b6/b2ba/b2bc/
 * b2be/b2c0/b2c2 dropped).
 *
 * Name mapping (Orig FUN_seg_off @ seg:off -> C name; Ghidra display
 * name is Module_VerbNoun Pascal, C name is snake_case per save.h note):
 *   FUN_1190_1692 @ 1190:1692 ResMgr_AddFile      -> rsc_add_file
 *   FUN_1190_13a8 @ 1190:13a8 ResMgr_AddType       -> rsc_add_type
 *   FUN_1190_12ee @ 1190:12ee ResMgr_NewResource   -> rsc_new_resource
 *   FUN_1190_1180 @ 1190:1180 ResMgr_FindFile      -> rsc_find_file
 *   FUN_1190_121d @ 1190:121d ResMgr_FindType      -> rsc_find_type
 *   FUN_1190_0ee5 @ 1190:0ee5 ResMgr_FindByHandle  -> rsc_find_by_handle
 *   FUN_1190_0978 @ 1190:0978 ResMgr_GetTypeCount  -> rsc_get_type_count
 *   FUN_1190_0a29 @ 1190:0a29 ResMgr_GetIndexed    -> rsc_get_res_indexed
 *     (renamed with _res_ infix: rsc_get_indexed above is the v1 image view)
 *   FUN_1190_0aea @ 1190:0aea ResMgr_GetById       -> rsc_get_by_id
 *   FUN_1190_0bbe @ 1190:0bbe ResMgr_GetByName     -> rsc_get_by_name
 *   FUN_1190_0fda @ 1190:0fda ResMgr_LoadToMem     -> rsc_load_to_mem (W2)
 *   FUN_1190_10e6 @ 1190:10e6 ResMgr_ZapRes        -> rsc_mgr_dispose covers
 *     free path (payload free + loaded=0/handle=0, no _LCLOSE here)
 *   FUN_1190_0c92 @ 1190:0c92 ResMgr_Dispose       -> rsc_mgr_dispose/free
 * All find/get/add are OS-free linked-list walks (implemented 1:1);
 * LoadToMem is file offsets + fseek/file_read_chunk/malloc (W2). */

typedef struct rsc_res rsc_res_t;
typedef struct rsc_type rsc_type_t;
typedef struct rsc_dir rsc_dir_t;
typedef struct rsc_mgr rsc_mgr_t;

/* Res node (orig 0x32 B). name[0] is the length prefix (u8, capped 0x1f)
 * so rsc_cmp_name(a->name, key) works directly (it skips byte 0 and
 * compares 1..0x1f case-insensitively, NUL/NUL stop). Linux-only
 * payload/payload_len/next are appended after the orig tail. */
struct rsc_res {
    uint16_t id;               /* +0x00 */
    uint16_t name_off;         /* +0x02 string-area offset */
    uint32_t file_off;         /* +0x04 payload offset in container */
    uint32_t len;              /* +0x08 payload length */
    char name[0x20];           /* +0x0c: [0]=len, [1..len]=chars, NUL-padded */
    uint8_t loaded;            /* +0x2d */
    uint16_t handle;           /* +0x2e (0 == not loaded) */
    uint8_t *payload;          /* Linux only (malloc'd by load path) */
    size_t payload_len;        /* Linux only */
    rsc_res_t *next;
};

/* Type node (orig 0x10 B: tag[4] + count + far ptrs). */
struct rsc_type {
    uint8_t tag[4];
    uint16_t count;            /* res count in this type (orig +4) */
    rsc_res_t *head;
    rsc_type_t *next;
};

/* File node (orig 0x12 B: u32 unk0 + u32 unk1 + u16 type-dir count). */
struct rsc_dir {
    uint32_t unk0;
    uint32_t unk1;
    uint16_t type_count;       /* header u16 (expected types) */
    rsc_type_t *types;
    rsc_dir_t *next;
};

/* Manager (replaces globals @ 1420:b2bc/b2be/b2c0/b2c2; no globals). */
struct rsc_mgr {
    rsc_dir_t *files;
    uint16_t file_count;       /* DAT_1420_b2be equiv */
    uint16_t type_count;       /* DAT_1420_b2bc equiv (total linked types) */
};

/* Lifecycle (OS-free malloc/free; NULL-safe; no exit). */
rsc_mgr_t *rsc_mgr_new(void);
void rsc_mgr_free(rsc_mgr_t *mgr);      /* ResMgr_Dispose @ 1190:0c92 equiv
                                         * (frees nodes + payloads; caller
                                         * closes its own FILE*, no _LCLOSE) */
void rsc_mgr_dispose(rsc_mgr_t *mgr);   /* Alias of free-path behind
                                         * ResMgr_ZapRes @ 1190:10e6 /
                                         * ResMgr_Release @ 1190:113f:
                                         * drops payloads, clears
                                         * loaded/handle, keeps nodes. */

/* Orig: FUN_1190_1692 @ 1190:1692 -> ResMgr_AddFile (AddRFile).
 * OS-free node alloc/link: malloc 0x12-B file node, stores u32+u32+u16,
 * prepends to mgr->files, bumps file_count. Returns node or NULL (NOMEM
 * or bad mgr). */
rsc_dir_t *rsc_add_file(rsc_mgr_t *mgr, uint32_t unk0, uint32_t unk1,
                        uint16_t type_count);
/* Orig: FUN_1190_13a8 @ 1190:13a8 -> ResMgr_AddType (AddType).
 * OS-free: malloc 10-B type node, copies 4-char tag, links into
 * dir->types, bumps mgr->type_count. NULL on bad args/NOMEM. */
rsc_type_t *rsc_add_type(rsc_mgr_t *mgr, rsc_dir_t *dir,
                         const uint8_t tag[4]);
/* Orig: FUN_1190_12ee @ 1190:12ee -> ResMgr_NewResource (NewResource).
 * OS-free: malloc 0x32-B res node, fills id/file_off/len + length-prefixed
 * name (truncated to 0x1f, NUL-padded), loaded=0/handle=0/payload=NULL,
 * links into type->head, bumps type->count. NULL on bad args/NOMEM. */
rsc_res_t *rsc_new_resource(rsc_type_t *type, uint16_t id, uint32_t file_off,
                            uint32_t len, const char *name);

/* Orig: FUN_1190_1180 @ 1190:1180 -> ResMgr_FindFile.
 * OS-free list walk: zero-based index into mgr->files (decompiler shows
 * sequential traversal with counter; orig key appears ordinal). NULL on
 * bad mgr or out-of-range. */
rsc_dir_t *rsc_find_file(const rsc_mgr_t *mgr, unsigned index);
/* Orig: FUN_1190_121d @ 1190:121d -> ResMgr_FindType.
 * OS-free: walks every dir->types, tag compare via rsc_cmp_type
 * (ResMgr_CmpType @ 1190:1533). NULL on bad args/miss. */
rsc_type_t *rsc_find_type(const rsc_mgr_t *mgr, const uint8_t tag[4]);
/* Orig: FUN_1190_0ee5 @ 1190:0ee5 -> ResMgr_FindByHandle.
 * OS-free: walks all types/res, matches handle @ +0x2e. Handle 0 never
 * matches (means "not loaded"). NULL on bad mgr/miss. */
rsc_res_t *rsc_find_by_handle(const rsc_mgr_t *mgr, uint16_t handle);

/* Orig: FUN_1190_0978 @ 1190:0978 -> ResMgr_GetTypeCount.
 * OS-free getter: mgr->type_count (DAT_1420_b2bc equiv); 0 on NULL mgr. */
uint16_t rsc_get_type_count(const rsc_mgr_t *mgr);
/* Orig: FUN_1190_0a29 @ 1190:0a29 -> ResMgr_GetIndexed.
 * OS-free: Nth res in type->head (zero-based). NULL on bad type/OOR. */
rsc_res_t *rsc_get_res_indexed(const rsc_type_t *type, unsigned index);
/* Orig: FUN_1190_0aea @ 1190:0aea -> ResMgr_GetById.
 * OS-free: linear walk matching id @ +0x00. NULL on bad type/miss. */
rsc_res_t *rsc_get_by_id(const rsc_type_t *type, uint16_t id);
/* Orig: FUN_1190_0bbe @ 1190:0bbe -> ResMgr_GetByName.
 * OS-free: linear walk via rsc_cmp_name (ResMgr_CmpName @ 1190:0e72,
 * case-insensitive 0x1f, NUL/NUL stop). Key is a plain C string; it is
 * wrapped into length-prefixed form internally. NULL on bad args/miss. */
rsc_res_t *rsc_get_by_name(const rsc_type_t *type, const char *name);

/* Orig: FUN_1190_0fda @ 1190:0fda -> ResMgr_LoadToMem (RestoMem).
 * IMPLEMENTED (W2, OS-free 1:1 without Win16): orig does _LLSEEK(file_off
 * @ +0x04) + _HREAD(len @ +0x08) into a GLOBALALLOC buffer, sets loaded
 * flag +0x2d = 1 and handle +0x2e (0 == not loaded, never matches
 * ResMgr_FindByHandle @ 1190:0ee5). Linux: fseek(fp, file_off, SEEK_SET)
 * (file_seek_wrapper @ 1000:xxxx / _LLSEEK equiv, 0/1/2 == SEEK_SET/CUR/END)
 * + file_read_chunk (File_ReadChunk @ 1000:272a / _HREAD equiv) + malloc
 * (no GLOBALALLOC), then payload/payload_len assignment. Validates
 * file_off+len <= file_size overflow-safe via ftell END (TRUNCATED),
 * fseek failure is IO; len 0 loads empty (payload NULL, loaded 1).
 * Idempotent when already loaded (same len/handle/payload). Handle is
 * max-existing+1 skipping 0 (NOMEM on 0xFFFF exhaustion). Read-only
 * against the container (never writes), no exit/globals/SDL. NULL-safe:
 * RSC_ERR_NOENT on bad mgr/res/fp. */
rsc_error_t rsc_load_to_mem(rsc_mgr_t *mgr, rsc_res_t *res, FILE *fp);

/* --- Packet H: ResMgr full walk + GfxPort bitmap (segment 1190/10d0) ---
 * Container framing (from ResMgr_AddFile/AddType/NewResource + header
 * comment above; all BE via rsc_read_u16le/rsc_read_u32le, fixed W2):
 *   file header: u32 unk0, u32 unk1, u16 type-dir count, then per type:
 *     4-char tag + u16 res-count + u16 unk, then per record a 0x32-B
 *     header: id u16 @ +0, name-off u16 @ +2, file-off u32 @ +4, len u32
 *     @ +8, name len u8 @ +0xc capped 0x1f + name chars @ +0xd (NUL-padded
 *     to +0x2c), loaded flag u8 @ +0x2d, handle u16 @ +0x2e.
 * Name mapping (Orig FUN_seg_off @ seg:off -> C name):
 *   FUN_1190_0000 @ 1190:0000 ResMgr_LoadRsc      -> rsc_load_rsc (W2)
 *   FUN_1190_0501 @ 1190:0501 ResMgr_LoadRscDisk  -> rsc_load_rsc_disk (W2)
 *   FUN_1190_1570 @ 1190:1570 ResMgr_AddFileDisk  -> rsc_add_file_disk
 *   FUN_1190_1abe @ 1190:1abe ResMgr_ReadU16Disk  -> rsc_read_u16_disk
 *   FUN_1190_1b47 @ 1190:1b47 ResMgr_ReadU16Disk2 -> rsc_read_u16_disk2
 *   FUN_1190_1bcb @ 1190:1bcb ResMgr_ReadU32Disk  -> rsc_read_u32_disk
 *   FUN_1190_17d0 @ 1190:17d0 ResMgr_DumpStats    -> rsc_dump_stats
 *   FUN_1190_0c92 @ 1190:0c92 ResMgr_Dispose       -> rsc_dispose_manager
 *     (CDisposeResourceMgr; _LCLOSE + free all lists).
 * GfxPort bitmap helper lives in video.h:
 *   FUN_10d0_0cfa @ 10d0:0cfa GfxPort_AllocBitmap -> gfx_port_alloc_bitmap. */

/* Orig: FUN_1190_0000 @ 1190:0000 -> ResMgr_LoadRsc (OpenResFile).
 * IMPLEMENTED (W2, buffered 0xE360 walk, read-only; Z1 fallback+chunked):
 * orig does _LOPEN(path) + _HREAD 0xE360 bytes into a GLOBALALLOC buffer
 * (falls back to ResMgr_LoadRscDisk @ 1190:0501 when GLOBALCOMPACT <
 * 1500000), then parses the type dir + per-record headers into
 * file/type/res lists (rsc_add_file @ 1190:1692 / rsc_add_type @ 1190:13a8
 * / rsc_new_resource @ 1190:12ee). Called 5x from WinMain @ 1008:0000 with
 * 1420:00b1/00be/00cb/00d8/00e5 (CivData0..4.rsc; observed sizes
 * 181519/729188/959728/1433655/144102, dirs 10608/358/766/3621/63, all <
 * 0xE360). Linux: fopen(rb) + ftell size + 16-B file header via
 * variant-tolerant BE32 (LE only if BE dir_size invalid; see Z1 table
 * above) + malloc dir_size (no GLOBALALLOC) + rsc_read_dir_chunked() the
 * EOF directory (dir_off = size-dir_size; one File_ReadChunk @ 1000:272a
 * when dir_size <= 0xE360, 0xE360-chunk loop otherwise, mirroring the
 * LoadRscDisk streaming path) + BE walk (u32/u32/u16 equivalent +
 * per-type tag/count + 12-B records via rsc_rec_file_off/rsc_rec_len:
 * BE24 @ +5 / BE16 @ +10 on-disk for BOTH families, u32 @ +4/+8 is the
 * in-memory node only -> 0x32-B nodes, len 0 derives from next file-off in
 * the same type, last to EOF (size) for truncated tails (Z1; W2 used
 * dir_off); names via string-table name-off, 0xffff = empty, capped 0x1f).
 * When dir_size > 0xE360 this buffered entry falls back to
 * rsc_load_rsc_disk() (GLOBALCOMPACT analog, see rsc_load_rsc_auto).
 * Validates magic, dir_size/dir_off/type/record/string bounds vs sizes
 * (BAD_MAGIC/TRUNCATED/RANGE/NOMEM/IO); partial file node removed on
 * failure (no leak). Read-only, no exit/globals/SDL. NOENT on bad mgr/path. */
rsc_error_t rsc_load_rsc(rsc_mgr_t *mgr, const char *path);

/* Orig: FUN_1190_0501 @ 1190:0501 -> ResMgr_LoadRscDisk.
 * IMPLEMENTED (W2, streaming twin of ResMgr_LoadRsc @ 1190:0000; Z1
 * len-to-EOF + duality unified): same directory walk but streams via the
 * File_ReadChunk path (FUN_1000_272a @ 1000:272a, _HREAD/_LREAD on the
 * resource handle) instead of one 0xE360 buffered read; taken when
 * GLOBALCOMPACT reports < 1500000 bytes free. Linux: fopen(rb) + ftell
 * size + 16-B header via file_read_chunk (variant-tolerant BE32, LE only
 * if BE dir_size invalid) + fseek to dir_off + streaming reads via
 * rsc_read_u16_disk @ 1190:1abe / rsc_read_u16_disk2 @ 1190:1b47 (name-off
 * slot) / rsc_read_u32_disk @ 1190:1bcb (all BE, fixed W2) + the same node
 * constructors (12-B records via rsc_rec_file_off/rsc_rec_len -> 0x32-B
 * nodes, same len-0 to-EOF derivation and string-table names as
 * rsc_load_rsc; no dir_size <= 0xE360 cap here, so large dirs stream).
 * Same validation/cleanup as buffered (BAD_MAGIC/TRUNCATED/RANGE/NOMEM/IO;
 * partial node removed). Read-only, no exit/globals/SDL. NOENT on bad
 * mgr/path. */
rsc_error_t rsc_load_rsc_disk(rsc_mgr_t *mgr, const char *path);

/* Z1 fallback wiring (Orig GLOBALCOMPACT @ 1000:xxxx < 1500000 ->
 * ResMgr_LoadRscDisk @ 1190:0501; no new Orig address, decision wrapper).
 * rsc_dir_needs_streaming() is the OS-free GLOBALCOMPACT analog: true when
 * dir_size > 0xE360 (buffered _HREAD would not fit) or file_size >= 1500000
 * (orig low-memory threshold; Civdata3.rsc 1433655 B sits just below it).
 * rsc_read_dir_chunked() is the File_ReadChunk @ 1000:272a loop behind the
 * buffered path for large dirs: fseek(dir_off) + repeated file_read_chunk
 * in 0xE360 chunks (short read = IO). rsc_load_rsc_auto() tries buffered
 * rsc_load_rsc() first and falls back to rsc_load_rsc_disk() when the
 * buffered attempt fails with TRUNCATED/UNSUPPORTED caused by oversize dir
 * (or when rsc_dir_needs_streaming() is true up-front). All read-only, no
 * exit/globals/SDL. */
int rsc_dir_needs_streaming(size_t dir_size, size_t file_size);
rsc_error_t rsc_read_dir_chunked(FILE *fp, size_t dir_off, uint8_t *dst,
                                 size_t dir_size);
rsc_error_t rsc_load_rsc_auto(rsc_mgr_t *mgr, const char *path);

/* Orig: FUN_1190_1570 @ 1190:1570 -> ResMgr_AddFileDisk (disk AddRFile).
 * IMPLEMENTED via File_ReadChunk: reads u32 unk0 + u32 unk1 (each via
 * rsc_read_u32_disk) then u16 type-dir count (via rsc_read_u16_disk) from
 * fp and delegates node alloc/link to rsc_add_file (ResMgr_AddFile @
 * 1190:1692). Returns the node, or NULL on bad mgr/fp, short read
 * (truncated header), or NOMEM. Consumes exactly 10 bytes on success. */
rsc_dir_t *rsc_add_file_disk(rsc_mgr_t *mgr, FILE *fp);

/* Orig: FUN_1190_1abe @ 1190:1abe -> ResMgr_ReadU16Disk.
 * IMPLEMENTED via File_ReadChunk (FUN_1000_272a @ 1000:272a): fread 2
 * bytes, assemble BE (byte0 * 0x100 + byte1, descending mult table @
 * 1420:3d00 as ResMgr_ReadU16 @ 1190:1953; fixed W2, was LE). Returns 0
 * on NULL fp or short read. */
uint16_t rsc_read_u16_disk(FILE *fp);
/* Orig: FUN_1190_1b47 @ 1190:1b47 -> ResMgr_ReadU16Disk2.
 * IMPLEMENTED twin of ResMgr_ReadU16Disk (identical 2-byte BE fetch; orig
 * uses the second slot for the record name-off field). Same contract:
 * 0 on NULL fp or short read. Kept as a separate symbol for RE-map
 * fidelity. */
uint16_t rsc_read_u16_disk2(FILE *fp);
/* Orig: FUN_1190_1bcb @ 1190:1bcb -> ResMgr_ReadU32Disk.
 * IMPLEMENTED via File_ReadChunk: fread 4 bytes, assemble BE (shifts
 * 24/16/8/0, same descending table as ResMgr_ReadU32 @ 1190:1a3f; fixed
 * W2, was LE). Returns 0 on NULL fp or short read. */
uint32_t rsc_read_u32_disk(FILE *fp);

/* Orig: FUN_1190_17d0 @ 1190:17d0 -> ResMgr_DumpStats (debug dump via
 * FUN_1000_2c9c @ 1000:2c9c).
 * IMPLEMENTED OS-free port: walks mgr->files/types/res and fprintf's one
 * line per file node (unk0/unk1/type_count) plus totals (files, types,
 * resources). out == NULL selects stdout. RSC_ERR_NOENT on NULL mgr;
 * RSC_ERR_IO on fprintf failure; RSC_OK otherwise. */
rsc_error_t rsc_dump_stats(const rsc_mgr_t *mgr, FILE *out);

/* Orig: FUN_1190_0c92 @ 1190:0c92 -> ResMgr_Dispose (CDisposeResourceMgr).
 * IMPLEMENTED via rsc_mgr_free + File_CloseWrapper (FUN_1000_1f7c @
 * 1000:1f7c, _LCLOSE equiv): if pfp && *pfp, fclose + clear to NULL;
 * if pmgr && *pmgr, rsc_mgr_free + clear to NULL. Fully NULL-safe
 * (NULL pmgr/pfp, NULL *pmgr / *pfp are no-ops). Payload free + node free
 * order matches rsc_mgr_free; no exit, no globals. */
void rsc_dispose_manager(rsc_mgr_t **pmgr, FILE **pfp);

/* --- AC1: CvPc header + index (segment 1198, OS-free) ---
 * Context (read-only diagnosis, no asset writes): CvPc slices start with
 *   BB DF... and fail the strict INDEX8 probe at offset 0 via
 *   rsc_decode_bitmap @ 1198:xxxx (w/h/bpp/pal gate: u16 w LE + u16 h LE +
 *   u8 bpp==8 + u8 pal 1..255 + RGB + indices). gfx_decode_rle tag
 *   hypothesis (<0x80 repeat / >=0x80 literal, see gfx_indexed.h) explains
 *   BB (>=0x80 literal) as compressed-stream start, not a bitmap header.
 *   Two hypotheses for the real header: (a) header at offset (8-byte tag +
 *   compressed data, w/h after decompress) so rsc_cvpc_head_off scans for a
 *   plausible header within the first 64 B; (b) different codec entirely
 *   (EDI LZSS2 ring 0x20? GIF LZW? TBD via Ghidra 1198 compare). Caller
 *   plan (caller-side, e.g. rsc_dump.c, so this file stays SDL-free):
 *   slice via rsc_record_data (borrowed, valid while rsc_file lives),
 *   log head 16 B via gfx_probe_head_hex, then dispatch below. Rules:
 *   C17, zero SDL, no exit(), no globals, borrowed slices. */

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_CvPcHeadOff.
 * Scan a CvPc slice for a plausible INDEX8 header (same LE layout as
 * rsc_decode_bitmap: u16 w LE @ +0, u16 h LE @ +2, u8 bpp @ +4, u8 pal @
 * +5). Plausible means w in 1..640, h in 1..640, bpp == 8, pal in 1..255
 * (u8 pal != 0). Scans offsets 0..min(64,size)-6, returns the first match
 * or -1. OS-free port (scan only, no decode fake): -1 on NULL data,
 * size < 6, or no plausible header. Full pixel-bounds (6+3*pal+w*h) are
 * NOT required here: under hypothesis (a) w/h describe the decompressed
 * frame, so the compressed slice may be shorter. No SDL/exit/globals. */
long rsc_cvpc_head_off(const uint8_t *data, size_t size);
/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_GdatStride (STUB).
 * GDAT stride (6 recs, unk 0x146a; terrain-table hypothesis: weights @
 * 1420:17d6 *3 + 1420:17d8, moisture @ 1420:12b8/1420:12bc). Candidate
 * stride 0x18 (weights[0x18] from worldgen_score_city_sites: sizeof row?
 * TBD via Ghidra 1198 table-decoder compare before any field split;
 * endian/stride unknown, no fake). Stub: NOENT on NULL data/stride_out,
 * TRUNCATED when size == 0, otherwise RSC_ERR_UNSUPPORTED with
 * *stride_out untouched. Borrowed slice, no SDL/exit/globals, C17. */
rsc_error_t rsc_gdat_stride(const uint8_t *data, size_t size,
                            size_t *stride_out);
/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_KdatStride (STUB).
 * KDAT stride (13 recs, unk 0x14be; table hypothesis, stride unknown;
 * TBD via Ghidra 1198 compare; caller logs the rsc_record_data slice head
 * 16 B via gfx_probe_head_hex, read-only). Stub: NOENT on NULL
 * data/stride_out, TRUNCATED when size == 0, otherwise
 * RSC_ERR_UNSUPPORTED with *stride_out untouched (no decode fake).
 * Borrowed slice, no SDL/exit/globals, C17. */
rsc_error_t rsc_kdat_stride(const uint8_t *data, size_t size,
                            size_t *stride_out);
/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_TextIndex.
 * TEXT full index within ONE borrowed slice (covers both hypotheses:
 * per-record single string -> exactly 1 offset (0); concatenated TEXT
 * blobs -> N offsets, one per NUL-terminated string). Walks NULs via
 * memchr: each stored offset is the start of a NUL-terminated string;
 * empty strings (consecutive NULs) count; trailing unterminated bytes
 * are ignored (same trailing-tolerant policy as rsc_decode_strlist).
 * Full 398-rec index (civdata0 TEXT, unk 0x01b6) is a caller loop over
 * rsc_record_data slices calling this per slice (offsets are per-slice;
 * add the slice base for file-absolute logging). OS-free port, bounds
 * only: NOENT on NULL data/out_n (NULL offsets allowed only when cap ==
 * 0 as a count query), TRUNCATED when size == 0, BAD_MAGIC when no NUL
 * is present within size (no strings), RANGE when cap holds fewer than
 * the strings present (*out_n = number stored, caller grows and
 * retries). No SDL/exit/globals, C17. */
rsc_error_t rsc_text_index(const uint8_t *data, size_t size,
                           size_t *offsets, size_t cap, size_t *out_n);
/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_StrListAll.
 * STR# full walk (32 recs civdata0, unk 0x002a) via the rsc_strlist_count
 * + rsc_strlist_get loop (same BE count / u8-len walk, trailing allowed).
 * Log-ready: this function performs no I/O itself; the caller logs inside
 * cb (e.g. head bytes or "%.*s"). cb may be NULL for a validate-only walk
 * (still walks every entry, still returns TRUNCATED/NOENT on bad data);
 * ctx is passed through untouched (may be NULL). Returns RSC_ERR_NOENT
 * on NULL data (or OOR inside the walk, passthrough from strlist_get),
 * RSC_ERR_TRUNCATED on short slice (passthrough), RSC_OK after all count
 * entries are visited (cb called in index order 0..count-1). Borrowed
 * slices, no SDL/exit/globals, C17. */
rsc_error_t rsc_strlist_all(const uint8_t *data, size_t size,
                            void (*cb)(unsigned index, const uint8_t *str,
                                       size_t len, void *ctx),
                            void *ctx);
/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_CvPcIsCompressed (predicate).
 * Compressed-stream predicate for CvPc slices (BB DF... observation):
 * true (1) iff first byte >= 0x80 (literal tag under the gfx_decode_rle
 * <0x80 repeat / >=0x80 literal hypothesis) AND the strict INDEX8 probe
 * fails at offset 0 (rsc_decode_bitmap != RSC_OK: bad w/h/bpp/pal at slice
 * start) AND a plausible header exists at offset > 0 via
 * rsc_cvpc_head_off (w/h 1..640 + bpp==8 + pal 1..255 within first 64 B).
 * Otherwise false (0), including NULL data, size == 0, raw INDEX8 at 0,
 * or no offset header. OS-free port (bool as int, no error code): never
 * fails, never logs. No SDL/exit/globals, C17. */
int rsc_cvpc_is_compressed(const uint8_t *data, size_t size);

/* --- AD1: slice diagnostics + stats (segment 1198, OS-free) ---
 * Context (read-only, no asset writes): civdata0 rsrcRSED STR#(32)/
 * TEXT(398,unk 0x01b6)/GDAT(6,unk 0x146a)/KDAT(13,unk 0x14be)/CvPc(3);
 * slices are borrowed via rsc_record_data (valid while rsc_file lives).
 * Caller plan (caller-side e.g. rsc_dump.c, so this file stays SDL-free):
 * slice via rsc_record_data, then dispatch below. Rules: C17, zero SDL,
 * no exit(), no globals, borrowed read-only slices. */

/* GDAT row stride hypothesis (Orig FUN_1198_xxxx @ 1198:xxxx ->
 * Rsc_GdatRow stride doc): 0x18 bytes per row from weights[0x18] in
 * worldgen_score_city_sites @ 1028:0fb4 (weights @ 1420:17d6*3+17d8,
 * moisture @ 1420:12b8/1420:12bc; 6 GDAT recs, unk 0x146a). TBD via
 * Ghidra 1198 table-decoder compare before any field split; documented,
 * applied as a diagnostic row view only (no field decode). */
#define RSC_GDAT_ROW_STRIDE 0x18u
/* KDAT row stride hypothesis (Orig FUN_1198_xxxx @ 1198:xxxx ->
 * Rsc_KdatRow stride doc): 0x20 bytes per row, TBD. KDAT 13 recs, unk
 * 0x14be; table hypothesis, stride unknown; 0x20 is the diagnostic try
 * (power-of-two neighbour of 0x18) until the Ghidra 1198 compare lands.
 * Documented, applied as a diagnostic row view only (no field decode). */
#define RSC_KDAT_ROW_STRIDE 0x20u

/* Orig: FUN_1190_0000 @ 1190:0000 -> Rsc_SliceLen (diagnostic accessor).
 * Record_data slice len accessor: returns the borrowed slice size for
 * record i via rsc_record_data (ResMgr_LoadRsc @ 1190:0000 BE EOF-dir
 * walk; file-off BE24 @ +5 / len BE16 @ +10, len 0 derives to next/EOF).
 * OS-free port (bounds only, no walk change): returns size on RSC_OK,
 * 0 on any error (NULL file, RANGE end, UNSUPPORTED dir unproven,
 * TRUNCATED overrun) and on NULL file. No SDL/exit/globals, C17,
 * read-only borrowed slice (no copy). */
size_t rsc_slice_len(const rsc_file_t *f, size_t i);

/* TEXT aggregate stats (Orig FUN_1198_xxxx @ 1198:xxxx -> Rsc_TextStats).
 * TEXT 398 recs civdata0, unk 0x01b6; e.g. menu @ 1420:435b, difficulty
 * @ 1420:2a5a; observed " 90% Science)", "  80% Tax,". total_strings
 * counts NUL-terminated strings via rsc_text_index per slice (Rsc_TextIndex
 * @ 1198:xxxx; empty strings count, trailing unterminated bytes ignored);
 * total_chars sums each indexed string len (NUL offset, bounds-checked);
 * longest_len is the max string len; hits_90pct counts slices containing
 * " 90%" (leading space, 4 B) via rsc_text_find (Rsc_TextFind @ 1198:xxxx,
 * record-level, matches dump_text_all). Non-TEXT slices (no NUL ->
 * BAD_MAGIC) and empty slices (TRUNCATED) are skipped, never fail. */
typedef struct rsc_text_stats {
    size_t total_strings;
    size_t total_chars;
    size_t longest_len;
    size_t hits_90pct;
} rsc_text_stats_t;

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_TextStats (aggregate).
 * OS-free port (loop rsc_record_data + rsc_decode_text shape via
 * rsc_text_index + rsc_text_find, bounds, no flood): walks dir slices
 * 0..8191 until RANGE (end) or UNSUPPORTED (dir unproven); per slice
 * indexes via rsc_text_index (cap 64 stack, heap 1024 retry on RANGE),
 * chars/longest via bounded NUL lens, hits via rsc_text_find " 90%".
 * No logging here (no flood; caller logs, e.g. dump_text_stats in
 * rsc_dump.c). NOENT on NULL f/out (*out untouched), otherwise RSC_OK
 * with zeros when no TEXT slices (e.g. CvD1/2/3/4 CvPc-only). Borrowed
 * slices, no SDL/exit/globals, C17. */
rsc_error_t rsc_text_stats(const rsc_file_t *f, rsc_text_stats_t *out);

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_GdatRow (diagnostic row view).
 * GDAT 6 recs, unk 0x146a; terrain-table hypothesis (weights @ 1420:17d6
 * *3 + 1420:17d8, moisture @ 1420:12b8/1420:12bc; cf.
 * worldgen_score_city_sites @ 1028:0fb4 weights[0x18]). Tries stride
 * RSC_GDAT_ROW_STRIDE (0x18): row = 0x18 bytes at index*stride.
 * OS-free port (bounds only, no field split): returns pointer into the
 * borrowed slice (valid while container lives) or NULL on NULL data,
 * size == 0, overflow, or index*stride+stride > size. No SDL/exit/
 * globals, C17. Layout TBD via Ghidra 1198 compare; no decode fake. */
const uint8_t *rsc_gdat_row(const uint8_t *data, size_t size,
                             unsigned index);

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_KdatRow (diagnostic row view).
 * KDAT 13 recs, unk 0x14be; table hypothesis, stride TBD (tried as
 * RSC_KDAT_ROW_STRIDE 0x20 until the Ghidra 1198 compare lands; caller
 * logs the rsc_record_data slice head 16 B via gfx_probe_head_hex,
 * read-only). Same shape as rsc_gdat_row: row = 0x20 bytes at
 * index*stride. Returns pointer into the borrowed slice or NULL on
 * NULL data, size == 0, overflow, or overrun. No SDL/exit/globals,
 * C17, no decode fake. */
const uint8_t *rsc_kdat_row(const uint8_t *data, size_t size,
                             unsigned index);

/* --- AE2: TEXT full walk + GDAT/KDAT row decode (segment 1198, OS-free) ---
 * Context (read-only, no asset writes): civdata0 rsrcRSED TEXT 398 recs
 *   (unk 0x01b6) totals 14977 strings via rsc_text_stats (Rsc_TextStats @
 *   1198:xxxx: NUL walk via rsc_text_index per slice + chars/longest +
 *   hits for " 90%" via rsc_text_find; observed hits=3 in civdata0, 0 in
 *   CvD1/2/3/4 CvPc-only); GDAT 6 recs stride 0x18 (unk 0x146a, weights
 *   @ 1420:17d6 *3 + 1420:17d8 hypothesis, moisture @ 1420:12b8/12bc;
 *   cf. worldgen_score_city_sites @ 1028:0fb4 weights[0x18]) + KDAT 13
 *   recs stride 0x20 (unk 0x14be). Slices borrowed via rsc_record_data
 *   (valid while rsc_file lives). Caller plan (caller-side e.g. rsc_dump.c
 *   dump_text_full / dump_godpal_search_run, SDL-free): walk via
 *   rsc_text_get_all / rsc_text_search_all (capped detail, summary always,
 *   no flood, no logging here) + row decode via rsc_gdat/kdat_decode_row
 *   (u16 BE arrays, bounds, no fake) + godpal run via gfx_godpal_search
 *   (read-only, no writes, no fake table). Rules: C17, zero SDL, no exit(),
 *   no globals, borrowed read-only slices. */

/* TEXT string location (file-level walk position).
 * rec is the directory slice index (rsc_record_data i), off is the
 * per-slice byte offset from rsc_text_index (start of a NUL-terminated
 * string within that slice). */
typedef struct rsc_text_loc {
    size_t rec;
    size_t off;
} rsc_text_loc_t;

/* GDAT decoded row (diagnostic u16 view).
 * 0x18 bytes per row (RSC_GDAT_ROW_STRIDE) = 12 x u16 BE (weights
 * hypothesis @ 1420:17d6; endian TBD via Ghidra 1198 compare, BE tried
 * to match the dir-layer BE via ResMgr_ReadU16 @ 1190:1953). */
typedef struct rsc_gdat_row_dec {
    uint16_t v[12];
} rsc_gdat_row_dec_t;

/* KDAT decoded row (diagnostic u16 view).
 * 0x20 bytes per row (RSC_KDAT_ROW_STRIDE) = 16 x u16 BE (same BE try
 * as GDAT until the Ghidra 1198 table-decoder compare lands). */
typedef struct rsc_kdat_row_dec {
    uint16_t v[16];
} rsc_kdat_row_dec_t;

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_TextGetAll (file walk).
 * TEXT full walk (398 recs civdata0, unk 0x01b6; total 14977 strings via
 * rsc_text_stats shape): walks dir slices 0..8191 via rsc_record_data
 * (borrowed) until RANGE (end) or UNSUPPORTED (dir unproven); per slice
 * indexes via rsc_text_index (Rsc_TextIndex @ 1198:xxxx; cap 64 stack,
 * heap 1024 retry on RANGE, same as rsc_text_stats; empty TRUNCATED and
 * non-TEXT BAD_MAGIC skipped, never fail). Collects file-level locations
 * (rec = slice index, off = per-slice offset) for the first cap strings;
 * counts the full total regardless of cap (summary always, no flood, no
 * logging here; caller logs capped detail + summary, e.g. dump_text_full).
 * OS-free port, bounds only: NOENT on NULL f/out_n/out_total (NULL locs
 * allowed only when cap == 0 as a count query; *out_n / *out_total untouched
 * on NOENT), otherwise fills *out_n = stored (min(total,cap)) and
 * *out_total = total strings; returns RSC_OK when total <= cap, RANGE when
 * total > cap (truncated detail, full summary). Zeros when no TEXT slices
 * (e.g. CvD1/2/3/4). Borrowed slices, no SDL/exit/globals, C17. */
rsc_error_t rsc_text_get_all(const rsc_file_t *f, rsc_text_loc_t *locs,
                             size_t cap, size_t *out_n, size_t *out_total);

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_TextSearchAll (file scan).
 * TEXT needle scan (" 90%" hits=3 in civdata0, 0 others; record-level via
 * rsc_text_find @ 1198:xxxx, matches rsc_text_stats hits_90pct): walks dir
 * slices 0..8191 via rsc_record_data (borrowed) until RANGE/UNSUPPORTED;
 * per slice verifies TEXT via rsc_text_index (cap 1 probe; TRUNCATED empty
 * and BAD_MAGIC non-TEXT skipped) then scans via rsc_text_find
 * (Rsc_TextFind @ 1198:xxxx; portable memmem). Collects file-level hits
 * (rec = slice index, off = needle offset within that slice) for the first
 * cap hits; counts the full total regardless of cap (summary always, no
 * flood, no logging here; caller logs capped 32-char contexts, e.g.
 * dump_text_full). NOENT on NULL f/needle/out_n/out_total or needle_len
 * == 0 (NULL hits allowed only when cap == 0 as a count query;
 * *out_n / *out_total untouched on NOENT), otherwise fills *out_n = stored
 * and *out_total = total hits; returns RSC_OK when total <= cap, RANGE when
 * total > cap. Zeros when no hits. Borrowed slices, no SDL/exit/globals. */
rsc_error_t rsc_text_search_all(const rsc_file_t *f, const uint8_t *needle,
                                size_t needle_len, rsc_text_loc_t *hits,
                                size_t cap, size_t *out_n,
                                size_t *out_total);

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_GdatDecodeRow (diagnostic u16).
 * GDAT 6 recs, unk 0x146a; stride RSC_GDAT_ROW_STRIDE (0x18) from
 * weights[0x18] in worldgen_score_city_sites @ 1028:0fb4 (weights @
 * 1420:17d6 *3 + 1420:17d8, moisture @ 1420:12b8/1420:12bc). Decodes one
 * row as 12 x u16 BE (row = 0x18 bytes at index*stride via rsc_gdat_row
 * bounds; BE tried to match the dir-layer BE, TBD via Ghidra 1198
 * compare; no field split beyond the u16 array, no decode fake).
 * OS-free port (bounds + copy): NOENT on NULL data/out (*out untouched),
 * TRUNCATED when size == 0, RANGE on overflow or index*stride+stride >
 * size, otherwise fills *out (12 BE16s) and returns RSC_OK. Borrowed
 * slice, no SDL/exit/globals, C17. */
rsc_error_t rsc_gdat_decode_row(const uint8_t *data, size_t size,
                                unsigned index, rsc_gdat_row_dec_t *out);

/* Orig: FUN_1198_xxxx @ 1198:xxxx -> Rsc_KdatDecodeRow (diagnostic u16).
 * KDAT 13 recs, unk 0x14be; stride RSC_KDAT_ROW_STRIDE (0x20, diagnostic
 * try until the Ghidra 1198 compare lands; caller logs head 16 B via
 * gfx_probe_head_hex, read-only). Same shape as rsc_gdat_decode_row:
 * row = 0x20 bytes at index*stride via rsc_kdat_row bounds, decoded as
 * 16 x u16 BE. NOENT on NULL data/out (*out untouched), TRUNCATED when
 * size == 0, RANGE on overflow/overrun, otherwise fills *out and returns
 * RSC_OK. Borrowed slice, no SDL/exit/globals, C17, no decode fake. */
rsc_error_t rsc_kdat_decode_row(const uint8_t *data, size_t size,
                                unsigned index, rsc_kdat_row_dec_t *out);

#endif /* CIVLIN_RSC_LOADER_H */
