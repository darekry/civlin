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
 * v1 1 KiB tag-scan probe; scan kept for record_count/info listing).
 * Walks the EOF directory in f->data (BE, same framing as rsc_load_rsc):
 * file header 16 B (magic + unk0 BE32 + unk1 BE32 + dir_size BE32),
 * 14-B file extra (skipped), per-type 8 B (tag + count BE16 + unk BE16,
 * count+1 records per orig <=count+1U loops), per-record 12-B on-disk
 * (id BE16 @ +0, name-off BE16 @ +2, unk u8 @ +4, file-off BE24 @
 * +5..+7 zero-extended to u32 @ +4 in the node, unk BE16 @ +8..+9, len
 * BE16 @ +10..+11 zero-extended to u32 @ +8; len 0 derives from the next
 * file-off in the same type, last to dir_off). Validates i < dir_total
 * (RANGE), file_off+len <= rsc_size overflow-safe (TRUNCATED), returns
 * the borrowed slice f->data+file_off (valid while rsc_file lives, no
 * copy, empty slice with size 0 when len is 0). Directory-walk failures
 * (bad magic/dir_size/dir_off) return UNSUPPORTED (scan listing still
 * available); NULL args return NOENT. Read-only, no exit/globals/SDL. */
rsc_error_t rsc_record_data(const rsc_file_t *f, size_t i,
                            const uint8_t **data, size_t *size);

/* Indexed images: IMPLEMENTED (X1, strict gate, OS-free, no SDL).
 * Producer chain: rsc_record_data slice must first succeed (directory
 * file-off/len, bounds file_off+len <= size, borrowed, valid while
 * rsc_file lives; len 0 derives from next file-off, last to dir_off);
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
 * IMPLEMENTED (W2, buffered 0xE360 walk, read-only): orig does _LOPEN(path)
 * + _HREAD 0xE360 bytes into a GLOBALALLOC buffer (falls back to
 * ResMgr_LoadRscDisk @ 1190:0501 when GLOBALCOMPACT < 1500000), then parses
 * the type dir + per-record headers into file/type/res lists (rsc_add_file
 * @ 1190:1692 / rsc_add_type @ 1190:13a8 / rsc_new_resource @ 1190:12ee).
 * Called 5x from WinMain @ 1008:0000 with 1420:00b1/00be/00cb/00d8/00e5
 * (CivData0..4.rsc; observed sizes 181519/729188/959728/1433655/144102,
 * dirs 10608/358/766/3621/63, all < 0xE360). Linux: fopen(rb) + ftell size
 * + fread 16-B file header (magic + unk0 BE32 + unk1 BE32 + dir_size BE32)
 * + malloc dir_size (<= 0xE360, no GLOBALALLOC) + file_read_chunk the EOF
 * directory (dir_off = size-dir_size) + BE cursor walk (u32/u32/u16
 * equivalent + per-type tag/count + 12-B records -> 0x32-B nodes, len 0
 * derives from next file-off in the same type, last to dir_off; names via
 * string-table name-off, 0xffff = empty, capped 0x1f). Validates magic,
 * dir_size/dir_off/type/record/string bounds vs sizes (BAD_MAGIC/
 * TRUNCATED/RANGE/NOMEM/IO); partial file node removed on failure (no
 * leak). Read-only, no exit/globals/SDL. RSC_ERR_NOENT on bad mgr/path. */
rsc_error_t rsc_load_rsc(rsc_mgr_t *mgr, const char *path);

/* Orig: FUN_1190_0501 @ 1190:0501 -> ResMgr_LoadRscDisk.
 * IMPLEMENTED (W2, streaming twin of ResMgr_LoadRsc @ 1190:0000):
 * same directory walk but streams via the File_ReadChunk path
 * (FUN_1000_272a @ 1000:272a, _HREAD/_LREAD on the resource handle)
 * instead of one 0xE360 buffered read; taken when GLOBALCOMPACT reports
 * < 1500000 bytes free. Linux: fopen(rb) + ftell size + 16-B header via
 * file_read_chunk + fseek to dir_off + streaming reads via
 * rsc_read_u16_disk @ 1190:1abe / rsc_read_u16_disk2 @ 1190:1b47 (name-off
 * slot) / rsc_read_u32_disk @ 1190:1bcb (all BE, fixed W2) + the same
 * node constructors (12-B records -> 0x32-B nodes, same len-0 derivation
 * and string-table names as rsc_load_rsc). Same validation/cleanup as
 * buffered (BAD_MAGIC/TRUNCATED/RANGE/NOMEM/IO; partial node removed).
 * Read-only, no exit/globals/SDL. RSC_ERR_NOENT on bad mgr/path. */
rsc_error_t rsc_load_rsc_disk(rsc_mgr_t *mgr, const char *path);

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

#endif /* CIVLIN_RSC_LOADER_H */
