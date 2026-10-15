/* CivLin RSC container probe (v1).
 *
 * Observed format (spike RE 2026-10-15, all 5 files in unpacked/):
 *
 *   Offset  Content
 *   ------  ---------------------------------------------------------------
 *   0x00    Magic 4 bytes: 00 00 01 00 (identical in all 5 files).
 *            NOTE: file(1) misidentifies these files as "Apple HFS/HFS+
 *            resource fork". That detection is a false positive as far as
 *            this project is concerned: the payload framing below is
 *            Civ-specific, so we parse it ourselves and never rely on the
 *            HFS interpretation.
 *   0x04..  unknown_u32_a, unknown_u32_b (per-file varying; e.g. CIVDATA1
 *            has 00 0b 1e fe / 00 0b 1d fe). Stored verbatim, NOT interpreted.
 *   0x10..  First payload bytes. CIVDATA1 starts with a MIDI header here
 *            ("MThd" at 0x19, "MTrk" at 0x27); CIVDATA2 has an embedded
 *            Photoshop resource ("be Photoshop 2.0 ... 8BIM" near 0xE0);
 *            CIVDATA3 starts with "FW User Dictionary"; civdata0 has game
 *            text ("Civ Data 1", "NewCiv0.rsrc", tax-rate strings).
 *   ~0x30   Human-readable marker, e.g. "\nCiv Data 1\x00" (CIVDATA1/2/4,
 *            Civdata3) or "Civ Data 1" at 0x15 + "NewCiv0.rsrc" (civdata0).
 *   dir     Directory entries, 8-byte type tag + unknown fields:
 *             CIVDATA1/2/3/4: "CvD<n>CvMM" (e.g. "CvD1CvMM"), twice in a row,
 *               each followed by bytes like 01 00 00 c6 06 84 00 00 00 00.
 *               Captured verbatim as unknown_a/unknown_b; their meaning
 *               (counts? offsets? codec ids?) is NOT yet decoded.
 *             civdata0.rsc: Mac-resource style "rsrcRSED" entries, each
 *               followed by 01 00 ff ff ff ff (01 00 = unknown_u16,
 *               ff ff ff ff = unknown_i32, plausibly "no name / any id").
 *
 * Correlation with Ghidra (civ.exe, NE Win 3.x):
 *   - Data segment 0x1420 holds the runtime filenames "civfonts.fon",
 *     "CivData0..4.rsc", "civhelp.hlp", "civ.prf" (0x1420:00a4..00e5) plus
 *     the message "DO NOT delete any *.rsc files". This proves the game
 *     loads these files by name at runtime via file I/O -> a runtime parser
 *     in src/ is the correct approach (no converter pipeline).
 *   - Ghidra did NOT recover data xrefs to those string addresses
 *     (16-bit NE, functions not auto-created), so the exact loader
 *     function(s) calling _LOPEN/_LREAD on them are TBD in the function-
 *     creation pass. Record framing above must be confirmed against that
 *     code before record_data() can carve payloads.
 *
 * What v1 does and does NOT do:
 *   - DOES: validate magic + minimum size, locate the marker, enumerate
 *     directory entries by the two observed tag patterns (bounded scan of
 *     the first 1 KiB), expose them read-only. This is sufficient to prove
 *     the container probe works on all 5 files.
 *   - DOES NOT: carve record payloads (framing unknown -> rsc_record_data()
 *     returns RSC_ERR_UNSUPPORTED), decode pixels (no plain DIB headers
 *     found; the few "28 00 00 00" hits are pixel-data coincidences with
 *     absurd w/h) -> rsc_image_count() is 0 and rsc_get_indexed() returns
 *     RSC_ERR_UNSUPPORTED. The indexed+palette structs and the SDL bridge
 *     (gfx_indexed) exist already so palette fidelity is preserved when
 *     decoding lands.
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

typedef struct rsc_file rsc_file_t;

const char *rsc_strerror(rsc_error_t err);

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
