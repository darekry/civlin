/* CivLin RSC resource manager (resmgr.c port).
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
 *   - Header per file node: u32 unk, u32 unk, u16 type-dir count, then per
 *     type: 4-char tag + u16 res-count + u16 unk, per record 0x32-B header
 *     (id u16 @ +0, name-off u16, file-off u32 @ +4, len u32, name len u8
 *     capped 0x1f + name chars @ +0xc, ...). All LE via mult table.
 *   - Runtime: file list @ 1420:b2c0, type list @ 1420:b2c2, cursor pair
 *     @ 1420:b2b4/b2b8 (+seg @ b2b6/b2ba), file-count @ 1420:b2be,
 *     type-count @ 1420:b2bc. Lazy payload load (LoadToMem) + Zap/Release.
 *
 * What this module does now:
 *   - DOES (OS-free, ported 1:1): rsc_read_u16le / rsc_read_u32le /
 *     rsc_cmp_type / rsc_cmp_name (exact logic of ResMgr_ReadU16/U32,
 *     CmpType/CmpName, cursor-advancing, no Win16).
 *   - DOES (v1 probe, kept): rsc_open() validates magic 00 00 01 00 +
 *     marker, scans the first 1 KiB for 8-byte tags (bounded, read-only).
 *   - DOES NOT yet: full type-dir walk (needs the u32/u32/u16 header
 *     decode against all 5 files -> rsc_record_data() still UNSUPPORTED),
 *     GLOBAL* list manager, GfxPort BITBLT bridge, palette animation
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

/* Payload carving: STUB (packet H documents, does not yet carve).
 * Exact offsets needed for a safe bounds-checked slice (see Packet H file
 * header above): file header u32 unk0 + u32 unk1 + u16 type-dir count;
 * per-type 4-char tag + u16 res-count + u16 unk; per-record 0x32-B header
 * (id u16 @ +0, name-off u16 @ +2, file-off u32 @ +4, len u32 @ +8, name
 * len u8 @ +0xc capped 0x1f + chars @ +0xd, loaded @ +0x2d, handle @
 * +0x2e). Minimal version will be: validate i < nrecords, read file-off /
 * len from the 0x32-B header, bounds-check file-off + len <= rsc_size,
 * return borrowed slice (no copy, valid while rsc_file lives). Until the
 * full type-dir walk is validated against all 5 CivData files, returns
 * RSC_ERR_UNSUPPORTED (RSC_ERR_NOENT/RANGE on bad args). */
rsc_error_t rsc_record_data(const rsc_file_t *f, size_t i,
                            const uint8_t **data, size_t *size);

/* Indexed images: STUB (packet H documents strict validation plan).
 * Plan: rsc_record_data slice must first succeed; then validate an 8-bit
 * image + palette (width > 0, height > 0, width*height <= slice len, bpp
 * == 8, pal_count in 1..255 with 3*pal_count bytes present, max pixel
 * index < pal_count when a palette is attached). rsc_image_count stays 0
 * and rsc_get_indexed returns RSC_ERR_UNSUPPORTED until that chain is
 * proven; gfx_indexed_to_surface already enforces the same 8-bit +
 * palette checks on its input (see gfx_indexed.h). */
size_t rsc_image_count(const rsc_file_t *f);
rsc_error_t rsc_get_indexed(const rsc_file_t *f, size_t i, rsc_indexed_t *out);

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
 *   FUN_1190_0fda @ 1190:0fda ResMgr_LoadToMem     -> rsc_load_to_mem (stub)
 *   FUN_1190_10e6 @ 1190:10e6 ResMgr_ZapRes        -> rsc_mgr_dispose covers
 *     free path (payload free + loaded=0/handle=0, no _LCLOSE here)
 *   FUN_1190_0c92 @ 1190:0c92 ResMgr_Dispose       -> rsc_mgr_dispose/free
 * All find/get/add are OS-free linked-list walks (implemented 1:1);
 * only LoadToMem needs file offsets + _LLSEEK/_HREAD (stub). */

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
 * Win16-sensitive STUB: orig does _LLSEEK(file_off) + _HREAD(len) payload,
 * sets loaded flag +0x2d = 1 and handle +0x2e. Needs container offsets +
 * file handle plumbing (see ResMgr_LoadRsc @ 1190:0000 /
 * ResMgr_LoadRscDisk @ 1190:0501); Linux payload malloc + file_seek_wrapper
 * + file_read_chunk wiring TBD. Always RSC_ERR_UNSUPPORTED (NULL-safe:
 * RSC_ERR_NOENT on bad args). */
rsc_error_t rsc_load_to_mem(rsc_mgr_t *mgr, rsc_res_t *res, FILE *fp);

/* --- Packet H: ResMgr full walk + GfxPort bitmap (segment 1190/10d0) ---
 * Container framing (from ResMgr_AddFile/AddType/NewResource + header
 * comment above; all LE via rsc_read_u16le/rsc_read_u32le):
 *   file header: u32 unk0, u32 unk1, u16 type-dir count, then per type:
 *     4-char tag + u16 res-count + u16 unk, then per record a 0x32-B
 *     header: id u16 @ +0, name-off u16 @ +2, file-off u32 @ +4, len u32
 *     @ +8, name len u8 @ +0xc capped 0x1f + name chars @ +0xd (NUL-padded
 *     to +0x2c), loaded flag u8 @ +0x2d, handle u16 @ +0x2e.
 * Name mapping (Orig FUN_seg_off @ seg:off -> C name):
 *   FUN_1190_0000 @ 1190:0000 ResMgr_LoadRsc      -> rsc_load_rsc (stub)
 *   FUN_1190_0501 @ 1190:0501 ResMgr_LoadRscDisk  -> rsc_load_rsc_disk (stub)
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
 * Win16-sensitive STUB: orig does _LOPEN(path) + _HREAD 0xE360 bytes into
 * a GLOBALALLOC buffer, then parses the type dir + per-record 0x32-B
 * headers above into file/type/res lists (rsc_add_file / rsc_add_type /
 * rsc_new_resource). Called 5x from WinMain @ 1008:0000 with
 * 1420:00b1/00be/00cb/00d8/00e5 (CivData0..4.rsc). Linux wiring TBD:
 * file_open_wrapper + file_read_chunk + the same node constructors.
 * Always RSC_ERR_UNSUPPORTED (RSC_ERR_NOENT on bad mgr/path). */
rsc_error_t rsc_load_rsc(rsc_mgr_t *mgr, const char *path);

/* Orig: FUN_1190_0501 @ 1190:0501 -> ResMgr_LoadRscDisk.
 * Win16-sensitive STUB: same directory walk as ResMgr_LoadRsc @ 1190:0000
 * but streams via the File_ReadChunk path (FUN_1000_272a @ 1000:272a,
 * _HREAD/_LREAD on the resource handle) instead of one 0xE360 buffered
 * read; taken when GLOBALCOMPACT reports < 1500000 bytes free. Linux
 * wiring TBD (fopen + rsc_read_u16_disk/u32_disk streaming into the same
 * node constructors). Always RSC_ERR_UNSUPPORTED (RSC_ERR_NOENT on bad
 * mgr/path). */
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
 * bytes, assemble LE (byte0 * 1 + byte1 * 0x100, same mult table @ 0x3d00
 * as ResMgr_ReadU16 @ 1190:1953). Returns 0 on NULL fp or short read. */
uint16_t rsc_read_u16_disk(FILE *fp);
/* Orig: FUN_1190_1b47 @ 1190:1b47 -> ResMgr_ReadU16Disk2.
 * IMPLEMENTED twin of ResMgr_ReadU16Disk (identical 2-byte LE fetch; orig
 * uses the second slot for the record name-off field). Same contract:
 * 0 on NULL fp or short read. Kept as a separate symbol for RE-map
 * fidelity. */
uint16_t rsc_read_u16_disk2(FILE *fp);
/* Orig: FUN_1190_1bcb @ 1190:1bcb -> ResMgr_ReadU32Disk.
 * IMPLEMENTED via File_ReadChunk: fread 4 bytes, assemble LE (shifts
 * 0/8/16/24, same as ResMgr_ReadU32 @ 1190:1a3f). Returns 0 on NULL fp
 * or short read. */
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
