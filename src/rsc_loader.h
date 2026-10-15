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

/* Payload carving: UNSUPPORTED in v1 (framing not yet decoded). */
rsc_error_t rsc_record_data(const rsc_file_t *f, size_t i,
                            const uint8_t **data, size_t *size);

/* Indexed images with strict validation: 0 / UNSUPPORTED in v1. */
size_t rsc_image_count(const rsc_file_t *f);
rsc_error_t rsc_get_indexed(const rsc_file_t *f, size_t i, rsc_indexed_t *out);

#endif /* CIVLIN_RSC_LOADER_H */
