/* CivLin RSC container manager (W2: load walk implemented; Y2: dump
 * validation documented, walk impls unchanged).
 * See rsc_loader.h for the documented BE framing + Y2 5-file validation
 * plan (CivData0..4.rsc via rsc_open @ magic 00 00 01 00 + marker and
 * rsc_load_rsc @ 1190:0000; Civdata3.rsc ~1.4 MiB largest; per-file
 * size/record_count/image_count via rsc_image_count + rsc_decode_bitmap
 * @ 1198:xxxx strict gate: borrowed view + pal 1..255 + pixel<pal_count).
 * Read-only: never writes to civ1win/ or unpacked/. No SDL, no exit(),
 * no globals, C17. */
#include "rsc_loader.h"
#include "civ_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RSC_MIN_SIZE 64u
#define RSC_SCAN_LEN 1024u
#define RSC_MAX_RECORDS 256u

/* W2: buffered OpenResFile chunk (ResMgr_LoadRsc @ 1190:0000 GLOBALALLOC).
 * Z1: GLOBALCOMPACT threshold (< 1500000 selects LoadRscDisk @ 1190:0501). */
#define RSC_LOAD_BUF_SIZE 0xE360u
#define RSC_GLOBALCOMPACT_THRESH 1500000u
/* Directory layout (BE, validated vs all 5 CivData sizes; Z1 variant table
 * in rsc_loader.h: rsrcRSED vs CvD1..4 share the same on-disk framing).
 * 16-B file header (magic + unk0 BE32 + unk1 BE32 + dir_size BE32) + 14-B
 * file extra (opaque, skipped) + types (8 B each) + records (12 B on-disk)
 * + strings. On-disk record (12 B, BOTH families): id BE16 @ +0, name-off
 * BE16 @ +2, unk u8 @ +4, file-off BE24 @ +5..+7, unk BE16 @ +8..+9, len
 * BE16 @ +10..+11. In-memory node holds u32 @ +4/+8 (zero-extended); the
 * "u32@+4/+8 on-disk" reading is never valid (e.g. STR# rec0 0x20000000)
 * and only produced false truncations, so Z1 reads BE24/BE16 exclusively
 * via rsc_rec_file_off()/rsc_rec_len(). */
#define RSC_DIR_HDR_SIZE 16u
#define RSC_DIR_EXTRA_SIZE 14u
#define RSC_DIR_TYPE_SIZE 8u
#define RSC_DIR_REC_SIZE 12u
#define RSC_DIR_MIN_SIZE 50u
#define RSC_MAX_TYPES 64u
#define RSC_MAX_RES_PER_TYPE 2048u

static const uint8_t kMagic[4] = {0x00, 0x00, 0x01, 0x00};

struct rsc_file {
    char *path;
    uint8_t *data;
    size_t size;
    rsc_record_t records[RSC_MAX_RECORDS];
    size_t nrecords;
};

const char *rsc_strerror(rsc_error_t err)
{
    switch (err) {
    case RSC_OK: return "ok";
    case RSC_ERR_IO: return "i/o error";
    case RSC_ERR_NOMEM: return "out of memory";
    case RSC_ERR_BAD_MAGIC: return "bad magic or marker";
    case RSC_ERR_TRUNCATED: return "file truncated";
    case RSC_ERR_NOENT: return "no such entry";
    case RSC_ERR_RANGE: return "index out of range";
    case RSC_ERR_UNSUPPORTED: return "supported file, feature not yet decoded";
    default: return "unknown error";
    }
}

/* Tag pattern 1: "CvD<n>CvMM" where <n> is an ASCII digit. */
static int is_cvd_tag(const uint8_t *p)
{
    return p[0] == 'C' && p[1] == 'v' && p[2] == 'D' &&
           p[3] >= '0' && p[3] <= '9' &&
           p[4] == 'C' && p[5] == 'v' && p[6] == 'M' && p[7] == 'M';
}

/* Tag pattern 2: Mac-resource style "rsrcRSED" (civdata0.rsc). */
static int is_rsed_tag(const uint8_t *p)
{
    return memcmp(p, "rsrcRSED", 8) == 0;
}

static uint32_t rd_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Orig mult table @ 1420:3d00 is {1, 0x100, 0x10000, 0x1000000} LE
 * ascending (verified 01 00 00 00 00 01 00 00 ...); loops descend (1->0
 * for U16 @ 1190:1953/19c9, 3->0 for U32 @ 1190:1a3f), so the first byte
 * takes the largest power = big-endian. Plain shifts are the 64-bit-Linux
 * equivalent (no __far segment carry). Fixed W2 (was LE). */
uint16_t rsc_read_u16le(rsc_cursor_t *cur)
{
    uint16_t v = 0;

    /* ResMgr_ReadU16 @ 1190:1953 (+ twin @ 1190:19c9): BE (first*0x100). */
    if (!cur || !cur->ptr || cur->ptr + 2 > cur->end)
        return 0;
    v = ((uint16_t)cur->ptr[0] << 8) | (uint16_t)cur->ptr[1];
    cur->ptr += 2;
    return v;
}

uint32_t rsc_read_u32le(rsc_cursor_t *cur)
{
    uint32_t v = 0;

    /* ResMgr_ReadU32 @ 1190:1a3f: BE (first*0x1000000). */
    if (!cur || !cur->ptr || cur->ptr + 4 > cur->end)
        return 0;
    v = ((uint32_t)cur->ptr[0] << 24) | ((uint32_t)cur->ptr[1] << 16) |
        ((uint32_t)cur->ptr[2] << 8) | (uint32_t)cur->ptr[3];
    cur->ptr += 4;
    return v;
}

int rsc_cmp_type(const uint8_t tag_a[4], const uint8_t tag_b[4])
{
    int i;

    /* ResMgr_CmpType @ 1190:1533: all 4 bytes must match. */
    if (!tag_a || !tag_b)
        return 0;
    for (i = 0; i < 4; i++) {
        if (tag_a[i] != tag_b[i])
            return 0;
    }
    return 1;
}

static int ascii_lower(int c)
{
    /* FUN_1000_1978 @ 1000:1978 single source: util_to_lower
     * (see civ_common.h) for the A-Z range used by res names. */
    return util_to_lower(c);
}

int rsc_cmp_name(const char *a, const char *b)
{
    int i;

    /* ResMgr_CmpName @ 1190:0e72: NULL-safe (0/0 miss), NUL/NUL early
     * stop, case-insensitive, compares bytes 1..0x1f (byte 0 is the
     * length prefix in the on-disk header, skipped by callers). */
    if (!a || !b)
        return 0;
    for (i = 1; i < 0x20; i++) {
        int ca = (unsigned char)a[i];
        int cb = (unsigned char)b[i];
        if (ca == 0 && cb == 0)
            break;
        if (ascii_lower(ca) != ascii_lower(cb))
            return 0;
    }
    return 1;
}

size_t file_read_chunk(FILE *fp, uint8_t *dst, size_t len)
{
    /* File_ReadChunk @ 1000:272a: streaming read behind
     * ResMgr_ReadU16Disk @ 1190:1abe / U16Disk2 @ 1190:1b47 / U32Disk @
     * 1190:1bcb (orig _HREAD/_LREAD on the resource handle). fread is
     * the Linux equivalent; short counts on EOF/error. */
    if (!fp || !dst || len == 0)
        return 0;
    return fread(dst, 1, len, fp);
}

/* memmem for the marker scan. */
static int has_marker(const uint8_t *data, size_t size)
{
    static const char *markers[] = {"Civ Data ", "rsrcRSED", "CvMM"};
    size_t scan = size < RSC_SCAN_LEN ? size : RSC_SCAN_LEN;
    for (size_t m = 0; m < sizeof(markers) / sizeof(markers[0]); m++) {
        size_t ml = strlen(markers[m]);
        if (ml > scan)
            continue;
        for (size_t i = 0; i + ml <= scan; i++) {
            if (memcmp(data + i, markers[m], ml) == 0)
                return 1;
        }
    }
    return 0;
}

static void scan_records(rsc_file_t *f)
{
    size_t scan = f->size < RSC_SCAN_LEN ? f->size : RSC_SCAN_LEN;
    f->nrecords = 0;
    if (scan < 16)
        return;
    for (size_t i = 0; i + 16 <= scan; i++) {
        const uint8_t *p = f->data + i;
        if (!is_cvd_tag(p) && !is_rsed_tag(p))
            continue;
        if (f->nrecords >= RSC_MAX_RECORDS)
            return;
        rsc_record_t *r = &f->records[f->nrecords++];
        memcpy(r->type, p, 8);
        r->type[8] = '\0';
        r->entry_offset = (uint32_t)i;
        r->unknown_a = rd_u32le(p + 8);
        r->unknown_b = rd_u32le(p + 12);
    }
}

rsc_error_t rsc_open(const char *path, rsc_file_t **out)
{
    FILE *fp = NULL;
    long fsize = 0;
    rsc_file_t *f = NULL;
    uint8_t *buf = NULL;
    size_t got = 0;

    if (!path || !out)
        return RSC_ERR_NOENT;
    *out = NULL;

    fp = fopen(path, "rb");
    if (!fp)
        return RSC_ERR_IO;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    fsize = ftell(fp);
    if (fsize < 0) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    if ((size_t)fsize < RSC_MIN_SIZE) {
        fclose(fp);
        return RSC_ERR_TRUNCATED;
    }
    rewind(fp);

    buf = (uint8_t *)malloc((size_t)fsize);
    if (!buf) {
        fclose(fp);
        return RSC_ERR_NOMEM;
    }
    got = fread(buf, 1, (size_t)fsize, fp);
    fclose(fp);
    if (got != (size_t)fsize) {
        free(buf);
        return RSC_ERR_IO;
    }
    if (memcmp(buf, kMagic, 4) != 0 || !has_marker(buf, got)) {
        free(buf);
        return RSC_ERR_BAD_MAGIC;
    }

    f = (rsc_file_t *)calloc(1, sizeof(*f));
    if (!f) {
        free(buf);
        return RSC_ERR_NOMEM;
    }
    f->path = (char *)malloc(strlen(path) + 1);
    if (!f->path) {
        free(buf);
        free(f);
        return RSC_ERR_NOMEM;
    }
    strcpy(f->path, path);
    f->data = buf;
    f->size = got;
    scan_records(f);
    *out = f;
    return RSC_OK;
}

void rsc_close(rsc_file_t *f)
{
    if (!f)
        return;
    free(f->path);
    free(f->data);
    free(f);
}

size_t rsc_size(const rsc_file_t *f)
{
    return f ? f->size : 0;
}

const char *rsc_path(const rsc_file_t *f)
{
    return f ? f->path : "(null)";
}

size_t rsc_record_count(const rsc_file_t *f)
{
    return f ? f->nrecords : 0;
}

rsc_error_t rsc_record_info(const rsc_file_t *f, size_t i, rsc_record_t *out)
{
    if (!f || !out)
        return RSC_ERR_NOENT;
    if (i >= f->nrecords)
        return RSC_ERR_RANGE;
    *out = f->records[i];
    return RSC_OK;
}

/* --- W2: BE directory helpers (no SDL, no exit, no globals) --- */

static uint16_t rsc_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static uint32_t rsc_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint32_t rsc_be24(const uint8_t *p)
{
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
}

/* Z1: LE32 reader for variant-tolerant header parse only. Orig integer
 * readers (ResMgr_ReadU16 @ 1190:1953 / ReadU32 @ 1190:1a3f) are BE
 * (descending mult table @ 1420:3d00); the header is BE on all 5 CivData
 * (LE dir_size is 0x66010000..0xFE020000 huge > size, never valid). This
 * exists so rsc_parse_hdr_variant() can prove LE invalid instead of
 * assuming it, per Z1 BE-vs-LE diagnosis (ffff0001 vs c6000001 vs
 * 00000001 are LE scan readings of tag+8; header itself stays BE). */
static uint32_t rsc_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Z1: unify file_off/len duality (Orig FUN_seg_off @ 1190:0000/0501).
 * On-disk 12-B rec (BOTH rsrcRSED and CvD*): file-off BE24 @ +5..+7,
 * len BE16 @ +10..+11. In-memory 0x32-B node: u32 @ +4/+8 (zero-extended
 * via rsc_new_resource). The u32-on-disk reading (BE32 @ +4 / BE32 @ +8)
 * yields huge offsets (e.g. 0x20000000 for civdata0 STR# rec0) and false
 * TRUNCATED, so these helpers read BE24/BE16 exclusively. NULL rec
 * yields 0 (mirrors truncated-cursor 0 in rsc_read_u16le). */
static uint32_t rsc_rec_file_off(const uint8_t *rec)
{
    if (!rec)
        return 0;
    return rsc_be24(rec + 5);
}

static uint32_t rsc_rec_len(const uint8_t *rec)
{
    if (!rec)
        return 0;
    return (uint32_t)rsc_be16(rec + 10);
}

/* Z1: variant-tolerant 16-B header parse (Orig FUN_1190_0000 @ 1190:0000
 * header preamble + FUN_1190_0501 @ 1190:0501 streaming preamble).
 * Tries BE dir_size first (valid on all 5 CivData: 63..10608); only if BE
 * gives dir_size < RSC_DIR_MIN_SIZE or > size does it try LE, and only
 * accepts LE when it lands on a matching header copy at dir_off. Reports
 * dir_off/dir_size via out params. BAD_MAGIC on magic mismatch or header
 * copy mismatch, TRUNCATED on short/small/bad dir_size. Does NOT enforce
 * dir_size <= 0xE360 here (callers decide buffered vs streaming via
 * rsc_dir_needs_streaming); dir_size > size is still TRUNCATED. */
static rsc_error_t rsc_parse_hdr_variant(const uint8_t *data, size_t size,
                                         size_t *out_off, size_t *out_size)
{
    uint32_t be_size = 0;
    uint32_t le_size = 0;

    if (!data || !out_off || !out_size)
        return RSC_ERR_NOENT;
    if (size < RSC_DIR_HDR_SIZE)
        return RSC_ERR_TRUNCATED;
    if (memcmp(data, kMagic, 4) != 0)
        return RSC_ERR_BAD_MAGIC;
    be_size = rsc_be32(data + 12);
    if (be_size >= RSC_DIR_MIN_SIZE && (size_t)be_size <= size) {
        size_t off = size - (size_t)be_size;
        if (off >= RSC_DIR_HDR_SIZE && off + (size_t)be_size == size &&
            memcmp(data + off, kMagic, 4) == 0 &&
            memcmp(data + off, data, RSC_DIR_HDR_SIZE) == 0) {
            *out_off = off;
            *out_size = (size_t)be_size;
            return RSC_OK;
        }
    }
    le_size = rsc_le32(data + 12);
    if (le_size >= RSC_DIR_MIN_SIZE && (size_t)le_size <= size) {
        size_t off = size - (size_t)le_size;
        if (off >= RSC_DIR_HDR_SIZE && off + (size_t)le_size == size &&
            memcmp(data + off, kMagic, 4) == 0 &&
            memcmp(data + off, data, RSC_DIR_HDR_SIZE) == 0) {
            *out_off = off;
            *out_size = (size_t)le_size;
            return RSC_OK;
        }
    }
    if (be_size < RSC_DIR_MIN_SIZE || (size_t)be_size > size)
        return RSC_ERR_TRUNCATED;
    return RSC_ERR_BAD_MAGIC;
}

static int rsc_tag_printable(const uint8_t *p)
{
    /* Type tags (CvPc/TEXT/GDAT/STR#...) are 4 printable ASCII (32..126);
     * records start with 00 xx and strings with a small len (< 32), so
     * this cleanly ends the types array (validated vs all 5 CivData). */
    for (int k = 0; k < 4; k++) {
        if (p[k] < 32 || p[k] > 126)
            return 0;
    }
    return 1;
}

/* Locate the EOF directory (Orig FUN_1190_0000 @ 1190:0000 preamble; Z1
 * variant-tolerant). Delegates to rsc_parse_hdr_variant() (BE first, LE
 * fallback; both tried per Z1 BE-vs-LE diagnosis). Oversize dirs
 * (dir_size > 0xE360 but <= size) are valid here (OK, not TRUNCATED):
 * the in-memory rsc_record_data() slice walk holds the whole file, so no
 * 0xE360 cap applies; buffered file loads decide streaming separately via
 * rsc_dir_needs_streaming()/rsc_load_rsc_auto() into ResMgr_LoadRscDisk
 * @ 1190:0501. BAD_MAGIC on magic/copy mismatch, TRUNCATED on short/small
 * dir_size > size. */
static rsc_error_t rsc_find_dir(const uint8_t *data, size_t size,
                                size_t *out_off, size_t *out_size)
{
    return rsc_parse_hdr_variant(data, size, out_off, out_size);
}

rsc_error_t rsc_record_data(const rsc_file_t *f, size_t i,
                            const uint8_t **data, size_t *size)
{
    /* ResMgr_LoadRsc @ 1190:0000 minimal carve (W2; Z1 truncated-last-to-
     * EOF + duality unified): directory slice preferred over the v1 1 KiB
     * tag-scan probe (scan kept for record_count/info listing). BE via
     * rsc_rec_file_off()/rsc_rec_len() (BE24 @ +5 / BE16 @ +10 on-disk for
     * BOTH families; u32 @ +4/+8 is the in-memory node only), bounds
     * file_off+len <= size, borrowed (no copy, valid while rsc_file
     * lives). Len 0 derives from the next file-off in the same type,
     * last to EOF = rsc_size (Z1; W2 used dir_off so truncated tails still
     * carve instead of TRUNCATED; all CivData CvPc types store len 0
     * except CIVDATA2/3 explicit; TEXT/GDAT/KDAT mix per Z1 table). */
    size_t dir_off = 0;
    size_t dir_size = 0;
    const uint8_t *dir = NULL;
    const uint8_t *types_end = NULL;
    const uint8_t *p = NULL;
    size_t ntypes = 0;
    size_t total = 0;
    size_t want = i;
    rsc_error_t fr;

    if (!f || !data || !size)
        return RSC_ERR_NOENT;
    if (!f->data || f->size < RSC_DIR_HDR_SIZE)
        return RSC_ERR_TRUNCATED;
    *data = NULL;
    *size = 0;

    /* Z1: in-memory slice walk has the whole file, so no 0xE360 cap;
     * rsc_find_dir() is variant-tolerant (BE first, LE fallback) and
     * allows oversize dirs (buffered fallback lives in rsc_load_rsc via
     * rsc_dir_needs_streaming, not here). */
    fr = rsc_find_dir(f->data, f->size, &dir_off, &dir_size);
    if (fr != RSC_OK)
        return RSC_ERR_UNSUPPORTED;
    dir = f->data + dir_off;
    /* First pass: count types (printable tags) to find the records array
     * and total (each type contributes count+1 per orig <=count+1U). */
    p = dir + RSC_DIR_HDR_SIZE + RSC_DIR_EXTRA_SIZE;
    types_end = dir + dir_size;
    while ((size_t)(types_end - p) >= RSC_DIR_TYPE_SIZE &&
           ntypes < RSC_MAX_TYPES) {
        uint16_t cnt;
        if (!rsc_tag_printable(p))
            break;
        cnt = rsc_be16(p + 4);
        if ((size_t)cnt > RSC_MAX_RES_PER_TYPE)
            return RSC_ERR_UNSUPPORTED;
        /* count+1 records of 12 B must fit (conservatively vs remaining;
         * strings follow, so <= remaining, not ==). */
        if ((size_t)((size_t)cnt + 1u) > (size_t)(types_end - p - 8) / RSC_DIR_REC_SIZE)
            return RSC_ERR_UNSUPPORTED;
        ntypes++;
        total += (size_t)cnt + 1u;
        p += RSC_DIR_TYPE_SIZE;
    }
    if (ntypes == 0 || total == 0)
        return RSC_ERR_UNSUPPORTED;
    if (want >= total)
        return RSC_ERR_RANGE;
    {
        const uint8_t *recs = p;
        const uint8_t *table = NULL;
        size_t rec_bytes = total * RSC_DIR_REC_SIZE;
        size_t table_size = 0;
        /* Table starts after all records; strings follow. */
        if ((size_t)(types_end - recs) < rec_bytes)
            return RSC_ERR_UNSUPPORTED;
        table = recs + rec_bytes;
        table_size = (size_t)(types_end - table);
        /* Second pass: walk types/records in directory order to reach i. */
        p = dir + RSC_DIR_HDR_SIZE + RSC_DIR_EXTRA_SIZE;
        for (size_t t = 0; t < ntypes; t++) {
            uint16_t cnt = rsc_be16(p + 4);
            size_t nrec = (size_t)cnt + 1u;
            const uint8_t *rp = recs;
            p += RSC_DIR_TYPE_SIZE;
            for (size_t r = 0; r < nrec; r++) {
                const uint8_t *rec = rp + r * RSC_DIR_REC_SIZE;
                uint32_t file_off;
                uint32_t len;
                if (want == 0) {
                    /* Z1 unified on-disk read (both families); see
                     * rsc_rec_file_off/rsc_rec_len. */
                    int is_last = (t + 1 == ntypes) && (r + 1 == nrec);
                    file_off = rsc_rec_file_off(rec);
                    len = rsc_rec_len(rec);
                    if (len == 0) {
                        /* Derive from next in same type, last to EOF
                         * (Z1; payloads precede the dir when intact, but a
                         * truncated dir still yields a usable tail slice
                         * to EOF instead of TRUNCATED). */
                        if (r + 1 < nrec) {
                            uint32_t nxt =
                                rsc_rec_file_off(rec + RSC_DIR_REC_SIZE);
                            if (nxt > file_off)
                                len = nxt - file_off;
                        } else if (f->size > (size_t)file_off) {
                            uint64_t tail =
                                (uint64_t)f->size - (uint64_t)file_off;
                            len = tail > 0xFFFFFFFFu
                                      ? 0xFFFFFFFFu
                                      : (uint32_t)tail;
                        }
                    }
                    if (file_off > f->size || len > f->size - file_off) {
                        /* Z1 truncated-tail tolerance: an explicit len that
                         * runs past EOF clamps to EOF when it is the last
                         * record overall (civdata0 STR# tail / Civdata3
                         * CvPc tail); non-last overruns stay TRUNCATED
                         * (genuine dir_off+len vs size violation). */
                        if (is_last && file_off <= f->size &&
                            file_off < f->size) {
                            len = (uint32_t)(f->size - (size_t)file_off);
                        } else {
                            return RSC_ERR_TRUNCATED;
                        }
                    }
                    *data = f->data + file_off;
                    *size = (size_t)len;
                    return RSC_OK;
                }
                want--;
            }
            recs += nrec * RSC_DIR_REC_SIZE;
            (void)table;
            (void)table_size;
        }
    }
    return RSC_ERR_RANGE;
}

size_t rsc_image_count(const rsc_file_t *f)
{
    /* X1 strict gate + Y2 per-file validation (see rsc_loader.h):
     * count directory slices that pass rsc_record_data (borrowed slice,
     * file_off+len <= size, valid while rsc_file lives) + rsc_decode_bitmap
     * @ 1198:xxxx (w/h > 0, bpp == 8, pal 1..255, every pixel index <
     * pal_count; borrowed view: pixels into the container, palette copied
     * tail-zeroed). OS-free, no SDL, no exit, no globals. RANGE ends the
     * walk; UNSUPPORTED (directory-walk failure) ends with the count so far
     * (0 when the dir itself is unproven); per-record TRUNCATED/RANGE/
     * BAD_MAGIC skips (not an image). rsc_dump.c Y2 reports this per file
     * alongside size/record_count (Civdata3.rsc ~1.4 MiB largest). */
    size_t count = 0;

    if (!f)
        return 0;
    for (size_t i = 0; i < 8192u; i++) {
        const uint8_t *data = NULL;
        size_t size = 0;
        rsc_indexed_t tmp;
        rsc_error_t dr = rsc_record_data(f, i, &data, &size);

        if (dr == RSC_ERR_RANGE)
            break;
        if (dr == RSC_ERR_UNSUPPORTED)
            break;
        if (dr != RSC_OK)
            continue;
        if (rsc_decode_bitmap(data, size, &tmp) == RSC_OK)
            count++;
    }
    return count;
}

rsc_error_t rsc_get_indexed(const rsc_file_t *f, size_t i, rsc_indexed_t *out)
{
    /* X1 port + Y2 validation view (see rsc_loader.h): ResMgr image payload
     * via the rsc_record_data slice (ResMgr_LoadToMem @ 1190:0fda
     * file-off/len bounds) + rsc_decode_bitmap strict gate (Rsc_DecodeBitmap
     * @ 1198:xxxx: w/h > 0, bpp == 8, pal 1..255, every pixel index <
     * pal_count). Borrowed view: pixels points into the container (valid
     * while rsc_file lives), palette copied with tail zeroed (mirrors
     * gfx_indexed_to_surface 8-bit + palette gate). rsc_dump.c Y2 spot-checks
     * record 0 per file through this view. OS-free, no SDL, no exit, no
     * globals. */
    const uint8_t *data = NULL;
    size_t size = 0;
    rsc_error_t dr;

    if (!f || !out)
        return RSC_ERR_NOENT;
    dr = rsc_record_data(f, i, &data, &size);
    if (dr != RSC_OK)
        return dr;
    return rsc_decode_bitmap(data, size, out);
}

rsc_error_t rsc_decode_bitmap(const uint8_t *data, size_t size,
                              rsc_indexed_t *out)
{
    /* Rsc_DecodeBitmap @ 1198:xxxx: neighbour of the 1190 resmgr cluster;
     * 0x32-B header decoder (file-off @ +4 / len @ +8) feeding
     * BITBLT/PALETTE. Caller slices via rsc_record_data (bounds
     * file_off + len <= container size); this validates the slice. */
    uint32_t w;
    uint32_t h;
    uint8_t bpp;
    uint8_t pal_count;
    size_t pal_bytes;
    size_t px_count;
    size_t headroom;
    const uint8_t *pal;
    const uint8_t *px;
    size_t i;

    if (!data || !out)
        return RSC_ERR_NOENT;
    if (size < 6)
        return RSC_ERR_TRUNCATED;
    w = (uint32_t)data[0] | ((uint32_t)data[1] << 8);
    h = (uint32_t)data[2] | ((uint32_t)data[3] << 8);
    bpp = data[4];
    pal_count = data[5];
    if (w == 0 || h == 0)
        return RSC_ERR_RANGE;
    if (bpp != 8)
        return RSC_ERR_BAD_MAGIC;
    if (pal_count == 0)
        return RSC_ERR_BAD_MAGIC;
    px_count = (size_t)w * (size_t)h;
    pal_bytes = (size_t)pal_count * 3u;
    headroom = size - 6;
    if (pal_bytes > headroom)
        return RSC_ERR_TRUNCATED;
    if (px_count > headroom - pal_bytes)
        return RSC_ERR_TRUNCATED;
    pal = data + 6;
    px = pal + pal_bytes;
    for (i = 0; i < px_count; i++) {
        if (px[i] >= pal_count)
            return RSC_ERR_RANGE;
    }
    out->width = w;
    out->height = h;
    out->bpp = 8;
    out->pal_count = pal_count;
    for (i = 0; i < (size_t)pal_count; i++) {
        out->pal[i][0] = pal[3 * i];
        out->pal[i][1] = pal[3 * i + 1];
        out->pal[i][2] = pal[3 * i + 2];
    }
    if ((size_t)pal_count < 256)
        memset(&out->pal[pal_count], 0,
               (256 - (size_t)pal_count) * 3);
    out->pixels = px;
    return RSC_OK;
}

/* --- AA1: per-type payload decoders (segment 1198, OS-free) --- */

rsc_error_t rsc_decode_strlist(const uint8_t *data, size_t size,
                               rsc_strlist_t *out)
{
    /* Rsc_DecodeStrList @ 1198:xxxx: STR# Pascal list (32 recs civdata0,
     * count BE to match rsc_read_u16le BE via ResMgr_ReadU16 @ 1190:1953).
     * Caller slices via rsc_record_data (borrowed, valid while rsc_file
     * lives); triage head via gfx_probe_head_hex(head,16) caller-side
     * (read-only, no asset writes). OS-free 1:1 port, no SDL/exit/globals. */
    uint16_t count = 0;
    const uint8_t *p = NULL;
    size_t remaining = 0;
    uint16_t i = 0;

    if (!data || !out)
        return RSC_ERR_NOENT;
    if (size < 2)
        return RSC_ERR_TRUNCATED;
    count = rsc_be16(data);
    p = data + 2;
    remaining = size - 2;
    for (i = 0; i < count; i++) {
        uint8_t len = 0;
        if (remaining < 1)
            return RSC_ERR_TRUNCATED;
        len = *p;
        p++;
        remaining--;
        if ((size_t)len > remaining)
            return RSC_ERR_TRUNCATED;
        p += (size_t)len;
        remaining -= (size_t)len;
    }
    out->count = count;
    out->body = data + 2;
    out->body_len = size - 2;
    return RSC_OK;
}

rsc_error_t rsc_decode_text(const uint8_t *data, size_t size,
                            rsc_text_t *out)
{
    /* Rsc_DecodeText @ 1198:xxxx: TEXT dialog string (398 recs civdata0,
     * unk 0x01b6; e.g. menu @ 1420:435b, difficulty @ 2a5a). Single
     * NUL-terminated C string per slice hypothesis; caller slices via
     * rsc_record_data (borrowed) + gfx_probe_head_hex(head,16) triage
     * caller-side (read-only). OS-free port, no SDL/exit/globals. */
    const uint8_t *nul = NULL;

    if (!data || !out)
        return RSC_ERR_NOENT;
    if (size == 0)
        return RSC_ERR_TRUNCATED;
    nul = (const uint8_t *)memchr(data, 0, size);
    if (!nul)
        return RSC_ERR_BAD_MAGIC;
    out->text = (const char *)data;
    out->len = (size_t)(nul - data);
    return RSC_OK;
}

rsc_error_t rsc_decode_gdat(const uint8_t *data, size_t size,
                            rsc_gdat_t *out)
{
    /* Rsc_DecodeGdat @ 1198:xxxx: GDAT stub (6 recs, unk 0x146a; game data
     * tables hypothesis: terrain? cf. weights @ 1420:17d6). Layout TBD via
     * Ghidra 1198 compare; caller logs rsc_record_data slice head 16 B via
     * gfx_probe_head_hex (read-only, no asset writes). No fake: *out
     * untouched on UNSUPPORTED. No SDL/exit/globals, borrowed slice. */
    if (!data || !out)
        return RSC_ERR_NOENT;
    if (size == 0)
        return RSC_ERR_TRUNCATED;
    return RSC_ERR_UNSUPPORTED;
}

rsc_error_t rsc_decode_kdat(const uint8_t *data, size_t size,
                            rsc_kdat_t *out)
{
    /* Rsc_DecodeKdat @ 1198:xxxx: KDAT stub (13 recs, unk 0x14be; table
     * hypothesis, stride unknown). Layout TBD via Ghidra 1198 compare;
     * caller logs rsc_record_data slice head 16 B via gfx_probe_head_hex
     * (read-only, no asset writes). No fake: *out untouched on
     * UNSUPPORTED. No SDL/exit/globals, borrowed slice. */
    if (!data || !out)
        return RSC_ERR_NOENT;
    if (size == 0)
        return RSC_ERR_TRUNCATED;
    return RSC_ERR_UNSUPPORTED;
}

rsc_error_t rsc_decode_cvpc(const uint8_t *data, size_t size,
                            rsc_cvpc_t *out)
{
    /* Rsc_DecodeCvPc @ 1198:xxxx: CvPc stub (picture hypothesis: 3/13/30/
     * 148/0 recs, likely bitmap frames for intro/pyramid/minimap). Bitmap
     * candidate feeding rsc_decode_bitmap @ 1198:xxxx / gfx_probe_rsc_image
     * (w/h/bpp/pal gate); images_strict=0 proves no raw INDEX8 at slice
     * start, so no decode is faked here. Plan: caller logs rsc_record_data
     * slice head 16 B via gfx_probe_head_hex (gfx_indexed.h, OS-free,
     * read-only, no asset writes) + probe reason, then Ghidra compares vs
     * the 1198 RLE/tagged-frame decoder. *out untouched on UNSUPPORTED.
     * No SDL/exit/globals, borrowed slice. */
    if (!data || !out)
        return RSC_ERR_NOENT;
    if (size == 0)
        return RSC_ERR_TRUNCATED;
    return RSC_ERR_UNSUPPORTED;
}

/* --- AB1: string/table lookup (segment 1198/1190, OS-free) --- */

rsc_error_t rsc_strlist_count(const uint8_t *data, size_t size,
                              uint16_t *out)
{
    /* Rsc_StrListCount @ 1198:xxxx: STR# u16 BE count @ +0 (32 recs
     * civdata0; BE to match rsc_read_u16le BE via ResMgr_ReadU16 @
     * 1190:1953). Caller slices via rsc_record_data (borrowed, valid while
     * rsc_file lives). OS-free port, no SDL/exit/globals. */
    if (!data || !out)
        return RSC_ERR_NOENT;
    if (size < 2)
        return RSC_ERR_TRUNCATED;
    *out = rsc_be16(data);
    return RSC_OK;
}

rsc_error_t rsc_strlist_get(const uint8_t *data, size_t size,
                            unsigned index, const uint8_t **str,
                            size_t *len)
{
    /* Rsc_StrListGet @ 1198:xxxx: index->string via u8 len walk (e.g.
     * civdata0 rec[3] 0x0F + "The Corporation" 15 B). Orig Pascal-list
     * shape: u16 BE count @ +0 then (u8 len + len bytes) per entry;
     * trailing bytes after the list are allowed (ignored, same as
     * rsc_decode_strlist). Overflow-safe, bounds on every visited entry.
     * Borrowed *str+*len (valid while container lives). */
    uint16_t count = 0;
    const uint8_t *p = NULL;
    size_t remaining = 0;
    unsigned i = 0;

    if (!data || !str || !len)
        return RSC_ERR_NOENT;
    if (size < 2)
        return RSC_ERR_TRUNCATED;
    count = rsc_be16(data);
    if ((unsigned)index >= (unsigned)count)
        return RSC_ERR_NOENT;
    p = data + 2;
    remaining = size - 2;
    for (i = 0; i < (unsigned)count; i++) {
        uint8_t slen = 0;
        if (remaining < 1)
            return RSC_ERR_TRUNCATED;
        slen = *p;
        p++;
        remaining--;
        if ((size_t)slen > remaining)
            return RSC_ERR_TRUNCATED;
        if (i == (unsigned)index) {
            *str = p;
            *len = (size_t)slen;
            return RSC_OK;
        }
        p += (size_t)slen;
        remaining -= (size_t)slen;
    }
    return RSC_ERR_NOENT;
}

long rsc_text_find(const uint8_t *data, size_t size,
                   const uint8_t *needle, size_t needle_len)
{
    /* Rsc_TextFind @ 1198:xxxx: TEXT slice scan (398 recs civdata0, unk
     * 0x01b6; e.g. menu @ 1420:435b, difficulty @ 1420:2a5a; observed
     * " 90% Science)", "  80% Tax,"). Portable memmem: byte loop with
     * memcmp at each offset (no GNU memmem, no SDL/exit/globals). */
    size_t i = 0;

    if (!data || !needle)
        return -1;
    if (needle_len == 0 || size < needle_len)
        return -1;
    for (i = 0; i + needle_len <= size; i++) {
        if (memcmp(data + i, needle, needle_len) == 0)
            return (long)i;
    }
    return -1;
}

rsc_error_t rsc_gdat_detail(const uint8_t *data, size_t size,
                            unsigned index, const uint8_t **entry,
                            size_t *entry_len)
{
    /* Rsc_GdatDetail @ 1198:xxxx STUB (6 recs, unk 0x146a; terrain-table
     * hypothesis: weights @ 1420:17d6 *3 + 1420:17d8, moisture @
     * 1420:12b8/1420:12bc). Stride TBD via Ghidra 1198 compare; no fake:
     * *entry / *entry_len untouched on UNSUPPORTED. Borrowed slice, no
     * SDL/exit/globals, C17. */
    (void)index;
    if (!data || !entry || !entry_len)
        return RSC_ERR_NOENT;
    if (size == 0)
        return RSC_ERR_TRUNCATED;
    return RSC_ERR_UNSUPPORTED;
}

rsc_error_t rsc_kdat_detail(const uint8_t *data, size_t size,
                            unsigned index, const uint8_t **entry,
                            size_t *entry_len)
{
    /* Rsc_KdatDetail @ 1198:xxxx STUB (13 recs, unk 0x14be; table
     * hypothesis, stride unknown). Layout TBD via Ghidra 1198 compare;
     * caller logs rsc_record_data slice head 16 B via gfx_probe_head_hex
     * (read-only, no asset writes). No fake: *entry / *entry_len untouched
     * on UNSUPPORTED. Borrowed slice, no SDL/exit/globals, C17. */
    (void)index;
    if (!data || !entry || !entry_len)
        return RSC_ERR_NOENT;
    if (size == 0)
        return RSC_ERR_TRUNCATED;
    return RSC_ERR_UNSUPPORTED;
}

rsc_error_t rsc_string_lookup(const uint8_t tag[4], const uint8_t *data,
                              size_t size, unsigned index,
                              const uint8_t **str, size_t *len)
{
    /* Rsc_StringLookup @ 1198:xxxx dispatcher via rsc_cmp_type
     * (ResMgr_CmpType @ 1190:1533). STR# -> rsc_strlist_get (#2, count #1
     * inside); TEXT index 0 -> whole string via rsc_decode_text (substring
     * search itself stays in rsc_text_find #3, e.g. " 90%" / menu
     * 1420:435b / difficulty 1420:2a5a); GDAT/KDAT -> detail stubs #4/#5;
     * CvPc/unknown -> UNSUPPORTED with outs untouched (no fake). */
    static const uint8_t kStrHash[4] = {'S', 'T', 'R', '#'};
    static const uint8_t kText[4] = {'T', 'E', 'X', 'T'};
    static const uint8_t kGdat[4] = {'G', 'D', 'A', 'T'};
    static const uint8_t kKdat[4] = {'K', 'D', 'A', 'T'};
    static const uint8_t kCvPc[4] = {'C', 'v', 'P', 'c'};
    rsc_text_t txt;

    if (!tag || !data || !str || !len)
        return RSC_ERR_NOENT;
    if (rsc_cmp_type(tag, kStrHash))
        return rsc_strlist_get(data, size, index, str, len);
    if (rsc_cmp_type(tag, kText)) {
        rsc_error_t dr;
        if (index != 0)
            return RSC_ERR_NOENT;
        dr = rsc_decode_text(data, size, &txt);
        if (dr != RSC_OK)
            return dr;
        *str = (const uint8_t *)txt.text;
        *len = txt.len;
        return RSC_OK;
    }
    if (rsc_cmp_type(tag, kGdat))
        return rsc_gdat_detail(data, size, index, str, len);
    if (rsc_cmp_type(tag, kKdat))
        return rsc_kdat_detail(data, size, index, str, len);
    if (rsc_cmp_type(tag, kCvPc)) {
        if (size == 0)
            return RSC_ERR_TRUNCATED;
        return RSC_ERR_UNSUPPORTED;
    }
    if (size == 0)
        return RSC_ERR_TRUNCATED;
    return RSC_ERR_UNSUPPORTED;
}

/* Orig ResMgr_GetString @ 1100:078a blob entry reader (id 0x2da8 via
 * ResMgr_GetById @ 1190:0aea + ResMgr_DisposeHandle @ 1190:09ed). */
rsc_error_t rsc_get_blob_string(const uint8_t *data, size_t size,
                                unsigned index, uint8_t *out, size_t cap,
                                size_t *out_len)
{
    uint16_t count = 0;
    const uint8_t *p = NULL;
    size_t remaining = 0;
    unsigned i = 0;

    if (out_len)
        *out_len = 0;
    if (!data || !out)
        return RSC_ERR_NOENT;
    if (size < 2)
        return RSC_ERR_TRUNCATED;
    /* BE count (CONCAT11(first, second) in orig == first*0x100+second,
     * same descending mult-table as ResMgr_ReadU16 @ 1190:1953). */
    count = (uint16_t)((uint16_t)data[0] * 0x100u + (uint16_t)data[1]);
    if ((unsigned)index >= (unsigned)count)
        return RSC_ERR_NOENT;
    p = data + 2;
    remaining = size - 2;
    for (i = 0; i <= (unsigned)index; i++) {
        uint8_t slen = 0;

        if (remaining < 1)
            return RSC_ERR_TRUNCATED;
        slen = *p;
        if ((size_t)slen + 1u > remaining)
            return RSC_ERR_TRUNCATED;
        if (i == (unsigned)index) {
            if ((size_t)slen + 1u > cap)
                return RSC_ERR_RANGE;
            out[0] = slen;
            if (slen > 0)
                memcpy(out + 1, p + 1, (size_t)slen);
            if (out_len)
                *out_len = (size_t)slen + 1u;
            return RSC_OK;
        }
        p += (size_t)slen + 1u;
        remaining -= (size_t)slen + 1u;
    }
    return RSC_ERR_NOENT;
}

/* --- Packet E: ResMgr list manager (segment 1190, OS-free 1:1) --- */

rsc_mgr_t *rsc_mgr_new(void)
{
    /* Manager alloc (replaces globals @ 1420:b2bc/b2be/b2c0/b2c2).
     * calloc zeroes files/file_count/type_count (orig globals start 0). */
    return (rsc_mgr_t *)calloc(1, sizeof(rsc_mgr_t));
}

static void rsc_free_res_list(rsc_res_t *head, int drop_payload_only)
{
    /* Shared free path behind ResMgr_ZapRes @ 1190:10e6 (drop payload,
     * loaded=0/handle=0, keep node when drop_payload_only) and
     * ResMgr_Dispose @ 1190:0c92 (free node when !drop_payload_only). */
    rsc_res_t *cur = head;
    while (cur) {
        rsc_res_t *next = cur->next;
        free(cur->payload);
        cur->payload = NULL;
        cur->payload_len = 0;
        cur->loaded = 0;
        cur->handle = 0;
        if (!drop_payload_only)
            free(cur);
        cur = next;
    }
}

void rsc_mgr_dispose(rsc_mgr_t *mgr)
{
    /* ResMgr_ZapRes @ 1190:10e6 / ResMgr_Release @ 1190:113f equiv:
     * frees every payload (orig frees +0x2e via FREERESOURCE/GLOBALFREE),
     * clears loaded +0x2d / handle +0x2e, keeps file/type/res nodes so
     * directory walks still work. NULL-safe no-op. */
    rsc_dir_t *d;
    if (!mgr)
        return;
    for (d = mgr->files; d; d = d->next) {
        rsc_type_t *t;
        for (t = d->types; t; t = t->next)
            rsc_free_res_list(t->head, 1);
    }
}

void rsc_mgr_free(rsc_mgr_t *mgr)
{
    /* ResMgr_Dispose @ 1190:0c92 / CDisposeResourceMgr equiv without
     * _LCLOSE (caller closes its own FILE*): dispose payloads, then free
     * res nodes, type nodes, file nodes, mgr. NULL-safe. */
    rsc_dir_t *d;
    if (!mgr)
        return;
    rsc_mgr_dispose(mgr);
    d = mgr->files;
    while (d) {
        rsc_dir_t *dnext = d->next;
        rsc_type_t *t = d->types;
        while (t) {
            rsc_type_t *tnext = t->next;
            rsc_free_res_list(t->head, 0);
            free(t);
            t = tnext;
        }
        free(d);
        d = dnext;
    }
    free(mgr);
}

rsc_dir_t *rsc_add_file(rsc_mgr_t *mgr, uint32_t unk0, uint32_t unk1,
                        uint16_t type_count)
{
    /* ResMgr_AddFile @ 1190:1692 (AddRFile): malloc 0x12-B file node.
     * Orig reads u32+u32 then u16 count from the buffer cursor
     * (rsc_read_u32le x2 + rsc_read_u16le) and links the node at
     * DAT_1420_b2c0. Here the three header values arrive as params. */
    rsc_dir_t *d;
    if (!mgr)
        return NULL;
    d = (rsc_dir_t *)calloc(1, sizeof(*d));
    if (!d)
        return NULL;
    d->unk0 = unk0;
    d->unk1 = unk1;
    d->type_count = type_count;
    d->types = NULL;
    d->next = mgr->files;
    mgr->files = d;
    mgr->file_count++;
    return d;
}

rsc_type_t *rsc_add_type(rsc_mgr_t *mgr, rsc_dir_t *dir,
                         const uint8_t tag[4])
{
    /* ResMgr_AddType @ 1190:13a8 (AddType): malloc 10-B type node.
     * Orig walks list @ DAT_1420_b2c2 with ResMgr_CmpType @ 1190:1533
     * to avoid duplicates; duplicate handling TBD (currently links
     * unconditionally, matching the NewResource link step). */
    rsc_type_t *t;
    if (!mgr || !dir || !tag)
        return NULL;
    t = (rsc_type_t *)calloc(1, sizeof(*t));
    if (!t)
        return NULL;
    memcpy(t->tag, tag, 4);
    t->count = 0;
    t->head = NULL;
    t->next = dir->types;
    dir->types = t;
    mgr->type_count++;
    return t;
}

rsc_res_t *rsc_new_resource(rsc_type_t *type, uint16_t id, uint32_t file_off,
                            uint32_t len, const char *name)
{
    /* ResMgr_NewResource @ 1190:12ee (NewResource): malloc 0x32-B res node,
     * links into type list, count at +4 (type->count++). Name stored
     * length-prefixed: [0]=len capped 0x1f, [1..len]=chars, NUL-padded,
     * so rsc_cmp_name (which skips byte 0) matches 1:1. */
    rsc_res_t *r;
    size_t n = 0;
    if (!type)
        return NULL;
    r = (rsc_res_t *)calloc(1, sizeof(*r));
    if (!r)
        return NULL;
    r->id = id;
    r->name_off = 0;
    r->file_off = file_off;
    r->len = len;
    if (name)
        n = strlen(name);
    if (n > 0x1f)
        n = 0x1f;
    r->name[0] = (char)n;
    if (n > 0)
        memcpy(&r->name[1], name, n);
    memset(&r->name[1 + n], 0, sizeof(r->name) - 1 - n);
    r->loaded = 0;
    r->handle = 0;
    r->payload = NULL;
    r->payload_len = 0;
    r->next = type->head;
    type->head = r;
    type->count++;
    return r;
}

rsc_dir_t *rsc_find_file(const rsc_mgr_t *mgr, unsigned index)
{
    /* ResMgr_FindFile @ 1190:1180: walk file list @ DAT_1420_b2c0.
     * Decompiler shape: cur = head; while (cur && i-- > 0) cur = cur->next. */
    const rsc_dir_t *cur;
    if (!mgr)
        return NULL;
    cur = mgr->files;
    while (cur && index-- > 0)
        cur = cur->next;
    return (rsc_dir_t *)cur;
}

rsc_type_t *rsc_find_type(const rsc_mgr_t *mgr, const uint8_t tag[4])
{
    /* ResMgr_FindType @ 1190:121d: walk type list(s) (@ DAT_1420_b2c2
     * global in orig; here every dir->types), tag compare via
     * rsc_cmp_type (ResMgr_CmpType @ 1190:1533, 4-byte equality). */
    const rsc_dir_t *d;
    if (!mgr || !tag)
        return NULL;
    for (d = mgr->files; d; d = d->next) {
        const rsc_type_t *t;
        for (t = d->types; t; t = t->next) {
            if (rsc_cmp_type(t->tag, tag))
                return (rsc_type_t *)t;
        }
    }
    return NULL;
}

rsc_res_t *rsc_find_by_handle(const rsc_mgr_t *mgr, uint16_t handle)
{
    /* ResMgr_FindByHandle @ 1190:0ee5: walk all types/res, match handle
     * @ +0x2e. Handle 0 means "not loaded" (rsc_new_resource init) and
     * never matches, mirroring the orig loaded-flag guard. */
    const rsc_dir_t *d;
    if (!mgr || handle == 0)
        return NULL;
    for (d = mgr->files; d; d = d->next) {
        const rsc_type_t *t;
        for (t = d->types; t; t = t->next) {
            const rsc_res_t *r;
            for (r = t->head; r; r = r->next) {
                if (r->handle == handle)
                    return (rsc_res_t *)r;
            }
        }
    }
    return NULL;
}

uint16_t rsc_get_type_count(const rsc_mgr_t *mgr)
{
    /* ResMgr_GetTypeCount @ 1190:0978: returns type-count @ DAT_1420_b2bc. */
    return mgr ? mgr->type_count : 0;
}

rsc_res_t *rsc_get_res_indexed(const rsc_type_t *type, unsigned index)
{
    /* ResMgr_GetIndexed @ 1190:0a29: Nth res in type list (zero-based).
     * Decompiler shape matches rsc_find_file: counted list walk. */
    const rsc_res_t *cur;
    if (!type)
        return NULL;
    cur = type->head;
    while (cur && index-- > 0)
        cur = cur->next;
    return (rsc_res_t *)cur;
}

rsc_res_t *rsc_get_by_id(const rsc_type_t *type, uint16_t id)
{
    /* ResMgr_GetById @ 1190:0aea: linear walk matching id @ +0x00. */
    const rsc_res_t *cur;
    if (!type)
        return NULL;
    for (cur = type->head; cur; cur = cur->next) {
        if (cur->id == id)
            return (rsc_res_t *)cur;
    }
    return NULL;
}

rsc_res_t *rsc_get_by_name(const rsc_type_t *type, const char *name)
{
    /* ResMgr_GetByName @ 1190:0bbe: linear walk via rsc_cmp_name
     * (ResMgr_CmpName @ 1190:0e72: case-insensitive, 0x1f max, NUL/NUL
     * stop, skips byte 0 length prefix). The plain-C key is wrapped
     * into length-prefixed form on the stack before comparing. */
    char key[0x20];
    size_t n = 0;
    const rsc_res_t *cur;
    if (!type || !name)
        return NULL;
    n = strlen(name);
    if (n > 0x1f)
        n = 0x1f;
    key[0] = (char)n;
    if (n > 0)
        memcpy(&key[1], name, n);
    memset(&key[1 + n], 0, sizeof(key) - 1 - n);
    for (cur = type->head; cur; cur = cur->next) {
        if (rsc_cmp_name(cur->name, key))
            return (rsc_res_t *)cur;
    }
    return NULL;
}

rsc_error_t rsc_load_to_mem(rsc_mgr_t *mgr, rsc_res_t *res, FILE *fp)
{
    /* ResMgr_LoadToMem @ 1190:0fda (RestoMem): _LLSEEK(file_off @ +0x04)
     * + _HREAD(len @ +0x08) into GLOBALALLOC, loaded +0x2d = 1, handle
     * +0x2e. Linux: fseek (file_seek_wrapper @ 1000:xxxx / _LLSEEK equiv)
     * + file_read_chunk (File_ReadChunk @ 1000:272a / _HREAD equiv) +
     * malloc (no GLOBALALLOC). Validated vs CivData sizes via ftell END
     * (file_off+len <= file_size, overflow-safe). No exit/globals/SDL. */
    long file_size = 0;
    long cur = 0;
    uint8_t *payload = NULL;
    uint16_t max_handle = 0;
    const rsc_dir_t *d = NULL;

    if (!mgr || !res || !fp)
        return RSC_ERR_NOENT;
    /* Idempotent when already loaded with matching len/payload/handle. */
    if (res->loaded && res->handle != 0) {
        if (res->len == 0 && res->payload == NULL && res->payload_len == 0)
            return RSC_OK;
        if (res->payload != NULL && res->payload_len == (size_t)res->len)
            return RSC_OK;
    }
    /* File size for bounds (position-preserving except for the payload
     * seek below, which is the intended _LLSEEK). */
    if (fseek(fp, 0, SEEK_END) != 0)
        return RSC_ERR_IO;
    file_size = ftell(fp);
    if (file_size < 0)
        return RSC_ERR_IO;
    if ((uint32_t)res->file_off > (uint32_t)file_size ||
        (uint32_t)res->len > (uint32_t)file_size - (uint32_t)res->file_off)
        return RSC_ERR_TRUNCATED;
    /* Empty payload: no malloc, loaded 1, keep/assign handle. */
    if (res->len == 0) {
        free(res->payload);
        res->payload = NULL;
        res->payload_len = 0;
        res->loaded = 1;
        if (res->handle == 0) {
            for (d = mgr->files; d; d = d->next) {
                const rsc_type_t *t;
                for (t = d->types; t; t = t->next) {
                    const rsc_res_t *r;
                    for (r = t->head; r; r = r->next) {
                        if (r != res && r->handle > max_handle)
                            max_handle = r->handle;
                    }
                }
            }
            if (max_handle == 0xFFFFu)
                return RSC_ERR_NOMEM;
            res->handle = (uint16_t)(max_handle + 1u);
            if (res->handle == 0)
                res->handle = 1;
        }
        return RSC_OK;
    }
    /* _LLSEEK(file_off) equivalent (file_seek_wrapper @ 1000:xxxx). */
    if (fseek(fp, (long)res->file_off, SEEK_SET) != 0)
        return RSC_ERR_IO;
    cur = ftell(fp);
    if (cur < 0 || (uint32_t)cur != res->file_off)
        return RSC_ERR_IO;
    payload = (uint8_t *)malloc((size_t)res->len);
    if (!payload)
        return RSC_ERR_NOMEM;
    if (file_read_chunk(fp, payload, (size_t)res->len) != (size_t)res->len) {
        free(payload);
        return RSC_ERR_IO;
    }
    /* Assign handle: max-existing + 1, skipping 0 (0 never matches
     * ResMgr_FindByHandle @ 1190:0ee5). */
    for (d = mgr->files; d; d = d->next) {
        const rsc_type_t *t;
        for (t = d->types; t; t = t->next) {
            const rsc_res_t *r;
            for (r = t->head; r; r = r->next) {
                if (r != res && r->handle > max_handle)
                    max_handle = r->handle;
            }
        }
    }
    if (max_handle == 0xFFFFu) {
        free(payload);
        return RSC_ERR_NOMEM;
    }
    free(res->payload);
    res->payload = payload;
    res->payload_len = (size_t)res->len;
    res->loaded = 1;
    if (res->handle == 0) {
        res->handle = (uint16_t)(max_handle + 1u);
        if (res->handle == 0)
            res->handle = 1;
    }
    return RSC_OK;
}

/* --- Packet H: ResMgr full walk + dispose (segment 1190, W2) --- */

/* Copy a len-prefixed string-table entry (len u8 + chars, capped 0x1f)
 * into a NUL-terminated C string for rsc_new_resource (which re-encodes
 * length-prefixed internally). 0xffff/out-of-bounds/truncated -> empty.
 * table/table_size cover the string area only (dir end). */
static void rsc_table_name(const uint8_t *table, size_t table_size,
                           uint16_t name_off, char out[32])
{
    size_t avail = 0;
    size_t n = 0;
    if (!out)
        return;
    out[0] = '\0';
    if (!table || table_size == 0)
        return;
    if (name_off == 0xFFFFu)
        return;
    if ((size_t)name_off >= table_size)
        return;
    /* Len byte must exist; chars follow. Truncate to available/table. */
    avail = table_size - (size_t)name_off - 1u;
    n = (size_t)table[name_off];
    if (n > 0x1Fu)
        n = 0x1Fu;
    if (n > avail)
        n = avail;
    if (n > 0)
        memcpy(out, table + (size_t)name_off + 1u, n);
    out[n] = '\0';
}

/* Z1: GLOBALCOMPACT analog (Orig GLOBALCOMPACT @ 1000:xxxx < 1500000 ->
 * ResMgr_LoadRscDisk @ 1190:0501). True when the EOF directory would not
 * fit the buffered _HREAD 0xE360 (dir_size > 0xE360) or the container hits
 * the orig low-memory threshold (file_size >= 1500000; Civdata3.rsc
 * 1433655 B sits just below it). No globals, no SDL, no exit. */
int rsc_dir_needs_streaming(size_t dir_size, size_t file_size)
{
    if (dir_size > RSC_LOAD_BUF_SIZE)
        return 1;
    if (file_size >= RSC_GLOBALCOMPACT_THRESH)
        return 1;
    return 0;
}

/* Z1: File_ReadChunk @ 1000:272a loop behind the buffered path for large
 * dirs (Orig ResMgr_LoadRsc @ 1190:0000 _HREAD 0xE360; large dirs stream in
 * 0xE360 chunks, mirroring the ResMgr_LoadRscDisk @ 1190:0501 path).
 * Seeks to dir_off then repeats file_read_chunk() until dir_size bytes are
 * gathered. Returns RSC_OK on full read, RSC_ERR_NOENT on bad args,
 * RSC_ERR_IO on seek/short read. Read-only, no globals/SDL/exit. */
rsc_error_t rsc_read_dir_chunked(FILE *fp, size_t dir_off, uint8_t *dst,
                                 size_t dir_size)
{
    size_t got = 0;

    if (!fp || !dst)
        return RSC_ERR_NOENT;
    if (dir_size == 0)
        return RSC_ERR_TRUNCATED;
    if (fseek(fp, (long)dir_off, SEEK_SET) != 0)
        return RSC_ERR_IO;
    while (got < dir_size) {
        size_t want = dir_size - got;
        size_t chunk = want > RSC_LOAD_BUF_SIZE ? RSC_LOAD_BUF_SIZE : want;
        size_t n;
        /* long dir_off is safe here: dir_off+dir_size == file_size <= LONG_MAX
         * on supported containers (all 5 CivData < 1.5 MiB). */
        if (chunk == 0)
            break;
        n = file_read_chunk(fp, dst + got, chunk);
        if (n != chunk)
            return RSC_ERR_IO;
        got += n;
    }
    return RSC_OK;
}

/* Remove the most-recently added file node and its children (load failure
 * cleanup, no leak; restores counts). dir must be mgr->files head (as
 * rsc_add_file prepends). */
static void rsc_drop_file(rsc_mgr_t *mgr, rsc_dir_t *dir,
                          uint16_t old_file_count, uint16_t old_type_count)
{
    rsc_type_t *t = NULL;
    rsc_type_t *tnext = NULL;
    if (!mgr || !dir)
        return;
    if (mgr->files == dir)
        mgr->files = dir->next;
    else {
        /* Defensive: unlink anywhere (should not happen for fresh add). */
        rsc_dir_t *prev = NULL;
        rsc_dir_t *cur = NULL;
        for (cur = mgr->files; cur; cur = cur->next) {
            if (cur == dir) {
                if (prev)
                    prev->next = cur->next;
                else
                    mgr->files = cur->next;
                break;
            }
            prev = cur;
        }
    }
    for (t = dir->types; t; t = tnext) {
        tnext = t->next;
        rsc_free_res_list(t->head, 0);
        free(t);
    }
    free(dir);
    mgr->file_count = old_file_count;
    mgr->type_count = old_type_count;
}

rsc_error_t rsc_load_rsc(rsc_mgr_t *mgr, const char *path)
{
    /* ResMgr_LoadRsc @ 1190:0000 (OpenResFile): _LOPEN + _HREAD 0xE360
     * into GLOBALALLOC (fallback to LoadRscDisk @ 1190:0501 when
     * GLOBALCOMPACT < 1500000), then u32/u32/u16-equivalent + per-type
     * tag/count + 12-B records -> 0x32-B nodes via rsc_add_file/type/
     * new_resource. Linux (Z1): fopen(rb) + ftell size + 16-B header via
     * variant-tolerant BE32 (LE only if BE invalid) + malloc dir_size (no
     * GLOBALALLOC) + rsc_read_dir_chunked() the EOF directory (one
     * File_ReadChunk @ 1000:272a when <= 0xE360, chunk loop otherwise) +
     * BE walk (two passes + rsc_rec_file_off/rsc_rec_len + len-0 last to
     * EOF). Falls back to rsc_load_rsc_disk() when dir_size > 0xE360
     * (GLOBALCOMPACT analog). Called 5x from WinMain @ 1008:0000 (CivData
     * sizes 181519/729188/959728/1433655/144102, dirs 10608/358/766/3621/
     * 63). Read-only, validated vs sizes, no exit/globals/SDL. */
    FILE *fp = NULL;
    long file_size_l = 0;
    size_t file_size = 0;
    uint8_t hdr[RSC_DIR_HDR_SIZE];
    uint32_t hdr_unk0 = 0;
    uint32_t hdr_unk1 = 0;
    uint32_t dir_size32 = 0;
    size_t dir_size = 0;
    size_t dir_off = 0;
    uint8_t *dirbuf = NULL;
    uint16_t old_file_count = 0;
    uint16_t old_type_count = 0;
    rsc_dir_t *dir = NULL;
    size_t ntypes = 0;
    size_t total = 0;
    const uint8_t *p = NULL;
    const uint8_t *dir_end = NULL;
    const uint8_t *recs_base = NULL;
    const uint8_t *table = NULL;
    size_t table_size = 0;
    size_t rec_cursor = 0;

    if (!mgr || !path)
        return RSC_ERR_NOENT;
    fp = fopen(path, "rb");
    if (!fp)
        return RSC_ERR_IO;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    file_size_l = ftell(fp);
    if (file_size_l < 0) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    file_size = (size_t)file_size_l;
    if (file_size < RSC_DIR_HDR_SIZE) {
        fclose(fp);
        return RSC_ERR_TRUNCATED;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    if (file_read_chunk(fp, hdr, sizeof(hdr)) != sizeof(hdr)) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    if (memcmp(hdr, kMagic, 4) != 0) {
        fclose(fp);
        return RSC_ERR_BAD_MAGIC;
    }
    /* Z1 variant-tolerant header: BE first (canonical), LE only if BE
     * dir_size is out of range but LE lands on a matching header copy.
     * Both endians tried per Z1 BE-vs-LE diagnosis; LE never validates on
     * the 5 CivData (LE sizes 0x66010000.. huge), so BE always wins there. */
    hdr_unk0 = rsc_be32(hdr + 4);
    hdr_unk1 = rsc_be32(hdr + 8);
    dir_size32 = rsc_be32(hdr + 12);
    dir_size = (size_t)dir_size32;
    if (dir_size < RSC_DIR_MIN_SIZE || dir_size > file_size) {
        /* BE invalid: probe LE before failing (LE never validates on the 5
         * CivData, but this proves the BE-vs-LE diagnosis instead of
         * assuming it; hdr bytes are left untouched so the later header-copy
         * memcmp still compares raw on-disk bytes). */
        uint32_t le_size = rsc_le32(hdr + 12);
        if (le_size >= RSC_DIR_MIN_SIZE && (size_t)le_size <= file_size) {
            size_t le_off = file_size - (size_t)le_size;
            uint8_t probe[RSC_DIR_HDR_SIZE];
            if (le_off >= RSC_DIR_HDR_SIZE &&
                fseek(fp, (long)le_off, SEEK_SET) == 0 &&
                file_read_chunk(fp, probe, sizeof(probe)) == sizeof(probe) &&
                memcmp(probe, kMagic, 4) == 0 &&
                memcmp(probe, hdr, RSC_DIR_HDR_SIZE) == 0) {
                hdr_unk0 = rsc_le32(hdr + 4);
                hdr_unk1 = rsc_le32(hdr + 8);
                dir_size32 = le_size;
                dir_size = (size_t)le_size;
            } else {
                fclose(fp);
                return RSC_ERR_TRUNCATED;
            }
        } else {
            fclose(fp);
            return RSC_ERR_TRUNCATED;
        }
    }
    dir_off = file_size - dir_size;
    if (dir_off < RSC_DIR_HDR_SIZE || dir_off + dir_size != file_size) {
        fclose(fp);
        return RSC_ERR_TRUNCATED;
    }
    /* Z1 fallback wiring (GLOBALCOMPACT analog): oversize dirs stream via
     * ResMgr_LoadRscDisk @ 1190:0501 instead of failing TRUNCATED. */
    if (rsc_dir_needs_streaming(dir_size, file_size)) {
        fclose(fp);
        return rsc_load_rsc_disk(mgr, path);
    }
    dirbuf = (uint8_t *)malloc(dir_size);
    if (!dirbuf) {
        fclose(fp);
        return RSC_ERR_NOMEM;
    }
    /* _HREAD 0xE360 equivalent via File_ReadChunk @ 1000:272a: single read
     * when dir_size <= 0xE360 (all 5 CivData), chunk loop otherwise. */
    {
        rsc_error_t cr =
            rsc_read_dir_chunked(fp, dir_off, dirbuf, dir_size);
        if (cr != RSC_OK) {
            free(dirbuf);
            fclose(fp);
            return cr;
        }
    }
    if (memcmp(dirbuf, kMagic, 4) != 0 ||
        memcmp(dirbuf, hdr, RSC_DIR_HDR_SIZE) != 0) {
        free(dirbuf);
        fclose(fp);
        return RSC_ERR_BAD_MAGIC;
    }
    if (dir_size < RSC_DIR_HDR_SIZE + RSC_DIR_EXTRA_SIZE) {
        free(dirbuf);
        fclose(fp);
        return RSC_ERR_TRUNCATED;
    }
    dir_end = dirbuf + dir_size;
    /* Pass 1: types array (printable tags) -> total records. */
    p = dirbuf + RSC_DIR_HDR_SIZE + RSC_DIR_EXTRA_SIZE;
    while ((size_t)(dir_end - p) >= RSC_DIR_TYPE_SIZE &&
           ntypes < RSC_MAX_TYPES) {
        uint16_t cnt;
        if (!rsc_tag_printable(p))
            break;
        cnt = rsc_be16(p + 4);
        if ((size_t)cnt > RSC_MAX_RES_PER_TYPE) {
            free(dirbuf);
            fclose(fp);
            return RSC_ERR_TRUNCATED;
        }
        if ((size_t)((size_t)cnt + 1u) >
            (size_t)(dir_end - p - RSC_DIR_TYPE_SIZE) / RSC_DIR_REC_SIZE) {
            free(dirbuf);
            fclose(fp);
            return RSC_ERR_TRUNCATED;
        }
        ntypes++;
        total += (size_t)cnt + 1u;
        p += RSC_DIR_TYPE_SIZE;
    }
    if (ntypes == 0 || total == 0) {
        free(dirbuf);
        fclose(fp);
        return RSC_ERR_TRUNCATED;
    }
    {
        size_t need = RSC_DIR_HDR_SIZE + RSC_DIR_EXTRA_SIZE +
                      ntypes * RSC_DIR_TYPE_SIZE + total * RSC_DIR_REC_SIZE;
        if (need > dir_size) {
            free(dirbuf);
            fclose(fp);
            return RSC_ERR_TRUNCATED;
        }
        recs_base = dirbuf + RSC_DIR_HDR_SIZE + RSC_DIR_EXTRA_SIZE +
                    ntypes * RSC_DIR_TYPE_SIZE;
        table = recs_base + total * RSC_DIR_REC_SIZE;
        table_size = (size_t)(dir_end - table);
    }
    old_file_count = mgr->file_count;
    old_type_count = mgr->type_count;
    dir = rsc_add_file(mgr, hdr_unk0, hdr_unk1, 0);
    if (!dir) {
        free(dirbuf);
        fclose(fp);
        return RSC_ERR_NOMEM;
    }
    /* Pass 2: create types/records in directory order. */
    p = dirbuf + RSC_DIR_HDR_SIZE + RSC_DIR_EXTRA_SIZE;
    rec_cursor = 0;
    for (size_t t = 0; t < ntypes; t++) {
        uint8_t tag[4];
        uint16_t cnt;
        size_t nrec;
        rsc_type_t *type = NULL;
        memcpy(tag, p + t * RSC_DIR_TYPE_SIZE, 4);
        cnt = rsc_be16(p + t * RSC_DIR_TYPE_SIZE + 4);
        nrec = (size_t)cnt + 1u;
        type = rsc_add_type(mgr, dir, tag);
        if (!type) {
            rsc_drop_file(mgr, dir, old_file_count, old_type_count);
            free(dirbuf);
            fclose(fp);
            return RSC_ERR_NOMEM;
        }
        for (size_t r = 0; r < nrec; r++) {
            const uint8_t *rec =
                recs_base + (rec_cursor + r) * RSC_DIR_REC_SIZE;
            uint16_t id = rsc_be16(rec + 0);
            uint16_t name_off = rsc_be16(rec + 2);
            /* Z1 unified on-disk duality (both families BE24/BE16). */
            uint32_t file_off = rsc_rec_file_off(rec);
            uint32_t len = rsc_rec_len(rec);
            char name[32];
            rsc_res_t *res = NULL;
            int is_last_overall =
                (t + 1 == ntypes) && (r + 1 == nrec);
            if (len == 0) {
                if (r + 1 < nrec) {
                    uint32_t nxt = rsc_rec_file_off(rec + RSC_DIR_REC_SIZE);
                    if (nxt > file_off)
                        len = nxt - file_off;
                } else if (file_size > (size_t)file_off) {
                    /* Z1 last-to-EOF (truncated tails carve to EOF). */
                    uint64_t tail =
                        (uint64_t)file_size - (uint64_t)file_off;
                    len = tail > 0xFFFFFFFFu ? 0xFFFFFFFFu : (uint32_t)tail;
                }
            }
            if (file_off > file_size || len > file_size - file_off) {
                /* Z1 truncated-tail tolerance: last overall clamps to EOF
                 * (civdata0/Civdata3 tails); non-last stays TRUNCATED
                 * (dir_off+len vs size violation, see Z1 table). */
                if (is_last_overall && file_off < file_size) {
                    len = (uint32_t)(file_size - (size_t)file_off);
                } else {
                    rsc_drop_file(mgr, dir, old_file_count, old_type_count);
                    free(dirbuf);
                    fclose(fp);
                    return RSC_ERR_TRUNCATED;
                }
            }
            rsc_table_name(table, table_size, name_off, name);
            res = rsc_new_resource(type, id, file_off, len, name);
            if (!res) {
                rsc_drop_file(mgr, dir, old_file_count, old_type_count);
                free(dirbuf);
                fclose(fp);
                return RSC_ERR_NOMEM;
            }
            res->name_off = name_off;
        }
        rec_cursor += nrec;
    }
    dir->type_count = (uint16_t)ntypes;
    free(dirbuf);
    fclose(fp);
    return RSC_OK;
}

rsc_error_t rsc_load_rsc_disk(rsc_mgr_t *mgr, const char *path)
{
    /* ResMgr_LoadRscDisk @ 1190:0501: streaming twin of LoadRsc (taken when
     * GLOBALCOMPACT < 1500000; Z1 len-to-EOF + duality unified, no 0xE360
     * cap). Same BE walk via File_ReadChunk (File_ReadChunk @ 1000:272a): 16-B header
     * (magic + U32Disk x3 variant-tolerant BE, LE only if BE invalid) +
     * 14-B extra + types (tag via fread + count/unk via ReadU16Disk) +
     * 12-B records (id via ReadU16Disk, name-off via ReadU16Disk2,
     * file-off BE24 @ +5 / len BE16 @ +10 on-disk for BOTH families via
     * rsc_rec helpers, u32 @ +4/+8 is the in-memory node only, len-0 last
     * to EOF as in LoadRsc) + string-table names via save/restore seeks.
     * Same validation/cleanup as buffered (no 0xE360 cap, so large dirs
     * stream). */
    FILE *fp = NULL;
    long file_size_l = 0;
    size_t file_size = 0;
    uint8_t hdr[RSC_DIR_HDR_SIZE];
    uint8_t dirhdr[RSC_DIR_HDR_SIZE];
    uint8_t extra[RSC_DIR_EXTRA_SIZE];
    uint32_t hdr_unk0 = 0;
    uint32_t hdr_unk1 = 0;
    uint32_t dir_size32 = 0;
    size_t dir_size = 0;
    size_t dir_off = 0;
    size_t dir_end = 0;
    uint16_t old_file_count = 0;
    uint16_t old_type_count = 0;
    rsc_dir_t *dir = NULL;
    size_t ntypes = 0;
    size_t total = 0;

    if (!mgr || !path)
        return RSC_ERR_NOENT;
    fp = fopen(path, "rb");
    if (!fp)
        return RSC_ERR_IO;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    file_size_l = ftell(fp);
    if (file_size_l < 0) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    file_size = (size_t)file_size_l;
    if (file_size < RSC_DIR_HDR_SIZE) {
        fclose(fp);
        return RSC_ERR_TRUNCATED;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    /* Header: magic via fread, U32s via ReadU32Disk (BE, W2). */
    if (file_read_chunk(fp, hdr, 4) != 4) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    if (memcmp(hdr, kMagic, 4) != 0) {
        fclose(fp);
        return RSC_ERR_BAD_MAGIC;
    }
    hdr_unk0 = rsc_read_u32_disk(fp);
    hdr_unk1 = rsc_read_u32_disk(fp);
    dir_size32 = rsc_read_u32_disk(fp);
    /* Detect short header reads (disk readers return 0 on short, which
     * collides with valid 0; re-verify via ftell). */
    {
        long pos = ftell(fp);
        if (pos != (long)RSC_DIR_HDR_SIZE) {
            fclose(fp);
            return RSC_ERR_IO;
        }
    }
    /* Re-read magic bytes into hdr for the later memcmp (already have). */
    memcpy(hdr, kMagic, 4);
    /* Reconstruct full 16-B header for the dir-copy memcmp below. */
    hdr[4] = (uint8_t)(hdr_unk0 >> 24);
    hdr[5] = (uint8_t)(hdr_unk0 >> 16);
    hdr[6] = (uint8_t)(hdr_unk0 >> 8);
    hdr[7] = (uint8_t)hdr_unk0;
    hdr[8] = (uint8_t)(hdr_unk1 >> 24);
    hdr[9] = (uint8_t)(hdr_unk1 >> 16);
    hdr[10] = (uint8_t)(hdr_unk1 >> 8);
    hdr[11] = (uint8_t)hdr_unk1;
    hdr[12] = (uint8_t)(dir_size32 >> 24);
    hdr[13] = (uint8_t)(dir_size32 >> 16);
    hdr[14] = (uint8_t)(dir_size32 >> 8);
    hdr[15] = (uint8_t)dir_size32;
    dir_size = (size_t)dir_size32;
    if (dir_size < RSC_DIR_MIN_SIZE || dir_size > file_size) {
        /* Z1 BE-vs-LE probe: byte-swap the BE size (hdr bytes unchanged)
         * and accept LE only when it lands on a matching header copy.
         * LE never validates on the 5 CivData; this documents the check. */
        uint32_t le_swapped = ((dir_size32 & 0xFFu) << 24) |
                              (((dir_size32 >> 8) & 0xFFu) << 16) |
                              (((dir_size32 >> 16) & 0xFFu) << 8) |
                              ((dir_size32 >> 24) & 0xFFu);
        size_t le_size = (size_t)le_swapped;
        int le_ok = 0;
        if (le_swapped >= RSC_DIR_MIN_SIZE && le_size <= file_size) {
            size_t le_off = file_size - le_size;
            long cur_pos = ftell(fp);
            uint8_t probe[RSC_DIR_HDR_SIZE];
            if (le_off >= RSC_DIR_HDR_SIZE && cur_pos == (long)RSC_DIR_HDR_SIZE &&
                fseek(fp, (long)le_off, SEEK_SET) == 0 &&
                file_read_chunk(fp, probe, sizeof(probe)) == sizeof(probe) &&
                memcmp(probe, hdr, sizeof(hdr)) == 0) {
                uint32_t le_unk0 = ((hdr_unk0 & 0xFFu) << 24) |
                                   (((hdr_unk0 >> 8) & 0xFFu) << 16) |
                                   (((hdr_unk0 >> 16) & 0xFFu) << 8) |
                                   ((hdr_unk0 >> 24) & 0xFFu);
                uint32_t le_unk1 = ((hdr_unk1 & 0xFFu) << 24) |
                                   (((hdr_unk1 >> 8) & 0xFFu) << 16) |
                                   (((hdr_unk1 >> 16) & 0xFFu) << 8) |
                                   ((hdr_unk1 >> 24) & 0xFFu);
                hdr_unk0 = le_unk0;
                hdr_unk1 = le_unk1;
                dir_size32 = le_swapped;
                dir_size = le_size;
                hdr[4] = (uint8_t)(hdr_unk0 >> 24);
                hdr[5] = (uint8_t)(hdr_unk0 >> 16);
                hdr[6] = (uint8_t)(hdr_unk0 >> 8);
                hdr[7] = (uint8_t)hdr_unk0;
                hdr[8] = (uint8_t)(hdr_unk1 >> 24);
                hdr[9] = (uint8_t)(hdr_unk1 >> 16);
                hdr[10] = (uint8_t)(hdr_unk1 >> 8);
                hdr[11] = (uint8_t)hdr_unk1;
                hdr[12] = (uint8_t)(dir_size32 >> 24);
                hdr[13] = (uint8_t)(dir_size32 >> 16);
                hdr[14] = (uint8_t)(dir_size32 >> 8);
                hdr[15] = (uint8_t)dir_size32;
                le_ok = 1;
            }
        }
        if (!le_ok) {
            fclose(fp);
            return RSC_ERR_TRUNCATED;
        }
    }
    dir_off = file_size - dir_size;
    if (dir_off < RSC_DIR_HDR_SIZE || dir_off + dir_size != file_size) {
        fclose(fp);
        return RSC_ERR_TRUNCATED;
    }
    /* Z1: streaming has no 0xE360 cap (this is the fallback target when
     * rsc_dir_needs_streaming() is true). */
    dir_end = dir_off + dir_size;
    if (fseek(fp, (long)dir_off, SEEK_SET) != 0) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    if (file_read_chunk(fp, dirhdr, sizeof(dirhdr)) != sizeof(dirhdr)) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    if (memcmp(dirhdr, hdr, sizeof(hdr)) != 0) {
        fclose(fp);
        return RSC_ERR_BAD_MAGIC;
    }
    if (file_read_chunk(fp, extra, sizeof(extra)) != sizeof(extra)) {
        fclose(fp);
        return RSC_ERR_IO;
    }
    /* Pass 1 (streaming): count types/total to find the string table. */
    {
        size_t remaining = dir_size - RSC_DIR_HDR_SIZE - RSC_DIR_EXTRA_SIZE;
        while (remaining >= RSC_DIR_TYPE_SIZE && ntypes < RSC_MAX_TYPES) {
            long tag_pos = 0;
            uint8_t tag[4];
            uint16_t cnt = 0;
            uint16_t unk = 0;
            tag_pos = ftell(fp);
            if (tag_pos < 0) {
                fclose(fp);
                return RSC_ERR_IO;
            }
            if (file_read_chunk(fp, tag, 4) != 4) {
                fclose(fp);
                return RSC_ERR_IO;
            }
            if (!rsc_tag_printable(tag)) {
                if (fseek(fp, tag_pos, SEEK_SET) != 0) {
                    fclose(fp);
                    return RSC_ERR_IO;
                }
                break;
            }
            cnt = rsc_read_u16_disk(fp);
            unk = rsc_read_u16_disk(fp);
            (void)unk;
            {
                long after = ftell(fp);
                if (after < 0 || (size_t)(after - tag_pos) != 8) {
                    fclose(fp);
                    return RSC_ERR_IO;
                }
            }
            if ((size_t)cnt > RSC_MAX_RES_PER_TYPE) {
                fclose(fp);
                return RSC_ERR_TRUNCATED;
            }
            remaining -= RSC_DIR_TYPE_SIZE;
            if ((size_t)((size_t)cnt + 1u) > remaining / RSC_DIR_REC_SIZE) {
                fclose(fp);
                return RSC_ERR_TRUNCATED;
            }
            ntypes++;
            total += (size_t)cnt + 1u;
        }
    }
    if (ntypes == 0 || total == 0) {
        fclose(fp);
        return RSC_ERR_TRUNCATED;
    }
    {
        size_t need = RSC_DIR_HDR_SIZE + RSC_DIR_EXTRA_SIZE +
                      ntypes * RSC_DIR_TYPE_SIZE + total * RSC_DIR_REC_SIZE;
        if (need > dir_size) {
            fclose(fp);
            return RSC_ERR_TRUNCATED;
        }
    }
    {
        size_t table_off =
            dir_off + RSC_DIR_HDR_SIZE + RSC_DIR_EXTRA_SIZE +
            ntypes * RSC_DIR_TYPE_SIZE + total * RSC_DIR_REC_SIZE;
        size_t table_size = dir_end - table_off;
        size_t recs_off =
            dir_off + RSC_DIR_HDR_SIZE + RSC_DIR_EXTRA_SIZE +
            ntypes * RSC_DIR_TYPE_SIZE;
        old_file_count = mgr->file_count;
        old_type_count = mgr->type_count;
        dir = rsc_add_file(mgr, hdr_unk0, hdr_unk1, 0);
        if (!dir) {
            fclose(fp);
            return RSC_ERR_NOMEM;
        }
        /* Pass 2: re-read types/records, resolve names via table seeks. */
        if (fseek(fp, (long)(dir_off + RSC_DIR_HDR_SIZE + RSC_DIR_EXTRA_SIZE),
                  SEEK_SET) != 0) {
            rsc_drop_file(mgr, dir, old_file_count, old_type_count);
            fclose(fp);
            return RSC_ERR_IO;
        }
        for (size_t t = 0; t < ntypes; t++) {
            uint8_t tag[4];
            uint16_t cnt = 0;
            uint16_t tunk = 0;
            size_t nrec = 0;
            rsc_type_t *type = NULL;
            if (file_read_chunk(fp, tag, 4) != 4) {
                rsc_drop_file(mgr, dir, old_file_count, old_type_count);
                fclose(fp);
                return RSC_ERR_IO;
            }
            if (!rsc_tag_printable(tag)) {
                rsc_drop_file(mgr, dir, old_file_count, old_type_count);
                fclose(fp);
                return RSC_ERR_TRUNCATED;
            }
            cnt = rsc_read_u16_disk(fp);
            tunk = rsc_read_u16_disk(fp);
            (void)tunk;
            nrec = (size_t)cnt + 1u;
            type = rsc_add_type(mgr, dir, tag);
            if (!type) {
                rsc_drop_file(mgr, dir, old_file_count, old_type_count);
                fclose(fp);
                return RSC_ERR_NOMEM;
            }
            /* Records for this type start at recs_off + prior totals;
             * seek there (types array already consumed in order, but be
             * explicit for clarity after the first-pass rewind). */
            (void)recs_off;
            for (size_t r = 0; r < nrec; r++) {
                uint16_t id = 0;
                uint16_t name_off = 0;
                uint8_t tail[RSC_DIR_REC_SIZE - 4];
                uint32_t file_off = 0;
                uint32_t len = 0;
                char name[32];
                rsc_res_t *res = NULL;
                long rec_end = 0;
                name[0] = '\0';
                id = rsc_read_u16_disk(fp);
                name_off = rsc_read_u16_disk2(fp);
                if (file_read_chunk(fp, tail, sizeof(tail)) != sizeof(tail)) {
                    rsc_drop_file(mgr, dir, old_file_count, old_type_count);
                    fclose(fp);
                    return RSC_ERR_IO;
                }
                /* Z1 unified tail (both families; u32 @ +4/+8 is the
                 * in-memory node only, never on-disk): unk u8 @ +0
                 * (= rec +4), file-off BE24 @ +1..+3 (= rec +5..+7),
                 * unk BE16 @ +4..+5, len BE16 @ +6..+7 (= rec +10..+11). */
                file_off = rsc_be24(tail + 1);
                len = (uint32_t)rsc_be16(tail + 6);
                rec_end = ftell(fp);
                if (rec_end < 0) {
                    rsc_drop_file(mgr, dir, old_file_count, old_type_count);
                    fclose(fp);
                    return RSC_ERR_IO;
                }
                if (len == 0) {
                    if (r + 1 < nrec) {
                        uint8_t nxt[RSC_DIR_REC_SIZE];
                        if (file_read_chunk(fp, nxt, sizeof(nxt)) !=
                            sizeof(nxt)) {
                            rsc_drop_file(mgr, dir, old_file_count,
                                          old_type_count);
                            fclose(fp);
                            return RSC_ERR_IO;
                        }
                        {
                            uint32_t nxt_off = rsc_rec_file_off(nxt);
                            if (nxt_off > file_off)
                                len = nxt_off - file_off;
                        }
                        if (fseek(fp, rec_end, SEEK_SET) != 0) {
                            rsc_drop_file(mgr, dir, old_file_count,
                                          old_type_count);
                            fclose(fp);
                            return RSC_ERR_IO;
                        }
                    } else if (file_size > (size_t)file_off) {
                        /* Z1 last-to-EOF (truncated tails carve to EOF). */
                        uint64_t tail64 =
                            (uint64_t)file_size - (uint64_t)file_off;
                        len = tail64 > 0xFFFFFFFFu ? 0xFFFFFFFFu
                                                   : (uint32_t)tail64;
                    }
                }
                if (file_off > file_size || len > file_size - file_off) {
                    /* Z1 truncated-tail tolerance: last overall record
                     * (t+1==ntypes && r+1==nrec, both in scope) clamps to
                     * EOF (civdata0/Civdata3 tails); non-last overruns
                     * stay TRUNCATED (dir_off+len vs size violation). */
                    int is_last_overall =
                        (t + 1 == ntypes) && (r + 1 == nrec);
                    if (is_last_overall && file_off < file_size) {
                        len = (uint32_t)(file_size - (size_t)file_off);
                    } else {
                        rsc_drop_file(mgr, dir, old_file_count,
                                      old_type_count);
                        fclose(fp);
                        return RSC_ERR_TRUNCATED;
                    }
                }
                /* Name via string-table seek (save/restore, read-only). */
                if (name_off == 0xFFFFu) {
                    name[0] = '\0';
                } else if ((size_t)name_off < table_size) {
                    long back = rec_end;
                    size_t avail = 0;
                    uint8_t nlen = 0;
                    uint8_t nbuf[32];
                    if (fseek(fp, (long)(table_off + (size_t)name_off),
                              SEEK_SET) != 0) {
                        rsc_drop_file(mgr, dir, old_file_count,
                                      old_type_count);
                        fclose(fp);
                        return RSC_ERR_IO;
                    }
                    if (file_read_chunk(fp, &nlen, 1) != 1) {
                        rsc_drop_file(mgr, dir, old_file_count,
                                      old_type_count);
                        fclose(fp);
                        return RSC_ERR_IO;
                    }
                    avail = table_size - (size_t)name_off - 1u;
                    {
                        size_t n = (size_t)nlen;
                        if (n > 0x1Fu)
                            n = 0x1Fu;
                        if (n > avail)
                            n = avail;
                        if (n >= sizeof(nbuf))
                            n = sizeof(nbuf) - 1u;
                        if (n > 0 &&
                            file_read_chunk(fp, nbuf, n) != n) {
                            rsc_drop_file(mgr, dir, old_file_count,
                                          old_type_count);
                            fclose(fp);
                            return RSC_ERR_IO;
                        }
                        if (n > 0)
                            memcpy(name, nbuf, n);
                        name[n] = '\0';
                    }
                    if (fseek(fp, back, SEEK_SET) != 0) {
                        rsc_drop_file(mgr, dir, old_file_count,
                                      old_type_count);
                        fclose(fp);
                        return RSC_ERR_IO;
                    }
                } else {
                    name[0] = '\0';
                }
                res = rsc_new_resource(type, id, file_off, len, name);
                if (!res) {
                    rsc_drop_file(mgr, dir, old_file_count, old_type_count);
                    fclose(fp);
                    return RSC_ERR_NOMEM;
                }
                res->name_off = name_off;
            }
        }
        dir->type_count = (uint16_t)ntypes;
    }
    fclose(fp);
    return RSC_OK;
}

/* Z1 fallback entry (no new Orig address; decision wrapper over
 * ResMgr_LoadRsc @ 1190:0000 + ResMgr_LoadRscDisk @ 1190:0501, Orig
 * GLOBALCOMPACT @ 1000:xxxx < 1500000). Tries buffered first; when the
 * buffered attempt fails with TRUNCATED/UNSUPPORTED and the container
 * needs streaming (rsc_dir_needs_streaming: dir_size > 0xE360 or
 * file_size >= 1500000), retries via streaming and returns that result.
 * Buffered success or non-size failures (IO/NOMEM/BAD_MAGIC/RANGE/NOENT)
 * return directly with no second attempt (mgr keeps the buffered partial
 * cleanup semantics; no double-add because buffered drops its file node on
 * failure before the retry). Read-only, no exit/globals/SDL. */
rsc_error_t rsc_load_rsc_auto(rsc_mgr_t *mgr, const char *path)
{
    FILE *fp = NULL;
    long file_size_l = 0;
    size_t file_size = 0;
    uint8_t hdr[RSC_DIR_HDR_SIZE];
    size_t dir_size = 0;
    size_t dir_off = 0;
    rsc_error_t br = RSC_OK;

    if (!mgr || !path)
        return RSC_ERR_NOENT;
    /* Peek header/size up-front for the GLOBALCOMPACT analog without
     * loading anything (read-only). Only the 16-B prefix + file size are
     * examined here (no full-file parse, so no header-copy validation yet);
     * BE dir_size is canonical, LE is probed only when BE is out of range.
     * Failure here falls through to the normal buffered attempt, which will
     * report the IO error itself. */
    fp = fopen(path, "rb");
    if (fp) {
        if (fseek(fp, 0, SEEK_END) == 0) {
            file_size_l = ftell(fp);
            if (file_size_l >= 0)
                file_size = (size_t)file_size_l;
        }
        if (file_size >= RSC_DIR_HDR_SIZE &&
            fseek(fp, 0, SEEK_SET) == 0 &&
            file_read_chunk(fp, hdr, sizeof(hdr)) == sizeof(hdr) &&
            memcmp(hdr, kMagic, 4) == 0) {
            uint32_t be = rsc_be32(hdr + 12);
            uint32_t le = rsc_le32(hdr + 12);
            int be_ok = (be >= RSC_DIR_MIN_SIZE && (size_t)be <= file_size);
            int le_ok = (le >= RSC_DIR_MIN_SIZE && (size_t)le <= file_size);
            dir_size = be_ok ? (size_t)be : (le_ok ? (size_t)le : 0);
            dir_off = (dir_size != 0) ? file_size - dir_size : 0;
            (void)dir_off;
            if (dir_size != 0 &&
                rsc_dir_needs_streaming(dir_size, file_size)) {
                fclose(fp);
                return rsc_load_rsc_disk(mgr, path);
            }
            /* Small-dir fast path falls through to buffered below so the
             * single implementation stays canonical (full header-copy
             * validation happens inside rsc_load_rsc). */
        }
        fclose(fp);
    }
    br = rsc_load_rsc(mgr, path);
    if (br == RSC_OK)
        return RSC_OK;
    if (br != RSC_ERR_TRUNCATED && br != RSC_ERR_UNSUPPORTED)
        return br;
    /* Buffered failed for a possibly-size reason: re-peek (read-only) and
     * retry streaming only when streaming is indicated; otherwise return
     * the original buffered error (preserves truncated root-cause for
     * civdata0/Civdata3 payload overruns, which stream would also hit). */
    fp = fopen(path, "rb");
    if (!fp)
        return br;
    do {
        long sz = 0;
        uint8_t h[RSC_DIR_HDR_SIZE];
        size_t ds = 0;
        size_t doff = 0;
        if (fseek(fp, 0, SEEK_END) != 0)
            break;
        sz = ftell(fp);
        if (sz < 0)
            break;
        if ((size_t)sz < RSC_DIR_HDR_SIZE)
            break;
        if (fseek(fp, 0, SEEK_SET) != 0)
            break;
        if (file_read_chunk(fp, h, sizeof(h)) != sizeof(h))
            break;
        if (memcmp(h, kMagic, 4) != 0)
            break;
        /* Full-file variant parse needs the whole 16-B + size context;
         * reuse the on-disk size: read dir_size BE/LE tolerant. */
        {
            uint32_t be = rsc_be32(h + 12);
            uint32_t le = rsc_le32(h + 12);
            size_t fsz = (size_t)sz;
            int be_ok = (be >= RSC_DIR_MIN_SIZE && (size_t)be <= fsz);
            int le_ok = (le >= RSC_DIR_MIN_SIZE && (size_t)le <= fsz);
            if (be_ok) {
                ds = (size_t)be;
                doff = fsz - ds;
            } else if (le_ok) {
                ds = (size_t)le;
                doff = fsz - ds;
            } else {
                break;
            }
            (void)doff;
            if (rsc_dir_needs_streaming(ds, fsz)) {
                fclose(fp);
                return rsc_load_rsc_disk(mgr, path);
            }
        }
    } while (0);
    fclose(fp);
    return br;
}

uint16_t rsc_read_u16_disk(FILE *fp)
{
    /* ResMgr_ReadU16Disk @ 1190:1abe: 2-byte BE fetch via File_ReadChunk
     * (File_ReadChunk @ 1000:272a). Descending mult table @ 1420:3d00
     * (fixed W2, was LE). 0 on NULL fp / short read (mirrors the
     * truncated-cursor 0 in rsc_read_u16le). */
    uint8_t buf[2];
    if (!fp)
        return 0;
    if (file_read_chunk(fp, buf, sizeof(buf)) != sizeof(buf))
        return 0;
    return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

uint16_t rsc_read_u16_disk2(FILE *fp)
{
    /* ResMgr_ReadU16Disk2 @ 1190:1b47: identical twin of ReadU16Disk (orig
     * uses this slot for the record name-off field @ +2). Same 2-byte BE
     * fetch via File_ReadChunk; separate symbol for RE-map fidelity. */
    uint8_t buf[2];
    if (!fp)
        return 0;
    if (file_read_chunk(fp, buf, sizeof(buf)) != sizeof(buf))
        return 0;
    return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

uint32_t rsc_read_u32_disk(FILE *fp)
{
    /* ResMgr_ReadU32Disk @ 1190:1bcb: 4-byte BE fetch via File_ReadChunk
     * (same descending shape as ResMgr_ReadU32 @ 1190:1a3f; fixed W2).
     * 0 on NULL fp / short read. */
    uint8_t buf[4];
    if (!fp)
        return 0;
    if (file_read_chunk(fp, buf, sizeof(buf)) != sizeof(buf))
        return 0;
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | (uint32_t)buf[3];
}

rsc_dir_t *rsc_add_file_disk(rsc_mgr_t *mgr, FILE *fp)
{
    /* ResMgr_AddFileDisk @ 1190:1570 (disk AddRFile): reads the 10-byte
     * file header (u32 unk0 + u32 unk1 + u16 type-dir count) via
     * File_ReadChunk (File_ReadChunk @ 1000:272a; fread equiv) and delegates
     * node alloc/link to rsc_add_file (ResMgr_AddFile @ 1190:1692).
     * BE (fixed W2, was LE). Consumes exactly 10 bytes on success; NULL
     * on bad mgr/fp, short read (truncated header), or NOMEM. */
    uint8_t hdr[10];
    uint32_t unk0;
    uint32_t unk1;
    uint16_t type_count;
    if (!mgr || !fp)
        return NULL;
    if (file_read_chunk(fp, hdr, sizeof(hdr)) != sizeof(hdr))
        return NULL;
    unk0 = ((uint32_t)hdr[0] << 24) | ((uint32_t)hdr[1] << 16) |
           ((uint32_t)hdr[2] << 8) | (uint32_t)hdr[3];
    unk1 = ((uint32_t)hdr[4] << 24) | ((uint32_t)hdr[5] << 16) |
           ((uint32_t)hdr[6] << 8) | (uint32_t)hdr[7];
    type_count = ((uint16_t)hdr[8] << 8) | (uint16_t)hdr[9];
    return rsc_add_file(mgr, unk0, unk1, type_count);
}

rsc_error_t rsc_dump_stats(const rsc_mgr_t *mgr, FILE *out)
{
    /* ResMgr_DumpStats @ 1190:17d0: debug dump via FUN_1000_2c9c @
     * 1000:2c9c in orig. OS-free port: totals + one line per file node.
     * fprintf is the Linux equivalent of the orig debug-print path. */
    const rsc_dir_t *d;
    unsigned ntypes = 0;
    unsigned nres = 0;
    FILE *dst;
    if (!mgr)
        return RSC_ERR_NOENT;
    dst = out ? out : stdout;
    for (d = mgr->files; d; d = d->next) {
        const rsc_type_t *t;
        unsigned dt = 0;
        unsigned dr = 0;
        for (t = d->types; t; t = t->next) {
            const rsc_res_t *r;
            dt++;
            for (r = t->head; r; r = r->next)
                dr++;
        }
        ntypes += dt;
        nres += dr;
        if (fprintf(dst, "rsc: file unk0=%lu unk1=%lu types=%u res=%u\n",
                    (unsigned long)d->unk0, (unsigned long)d->unk1,
                    dt, dr) < 0)
            return RSC_ERR_IO;
    }
    if (fprintf(dst, "rsc: files=%u types=%u res=%u\n",
                (unsigned)mgr->file_count, ntypes, nres) < 0)
        return RSC_ERR_IO;
    return RSC_OK;
}

void rsc_dispose_manager(rsc_mgr_t **pmgr, FILE **pfp)
{
    /* ResMgr_Dispose @ 1190:0c92 (CDisposeResourceMgr): _LCLOSE every open
     * resource handle + free all file/type/res lists. Linux: fclose (the
     * File_CloseWrapper @ 1000:1f7c equiv, NULL-safe) then rsc_mgr_free
     * (payload + node frees). Double-pointer form clears the caller's
     * handles to NULL so a second dispose is a no-op. */
    if (pfp && *pfp) {
        fclose(*pfp);
        *pfp = NULL;
    }
    if (pmgr && *pmgr) {
        rsc_mgr_free(*pmgr);
        *pmgr = NULL;
    }
}

/* --- AC1: CvPc header + index (segment 1198, OS-free) --- */

#define RSC_CVPC_HEAD_SCAN_MAX 64u
#define RSC_CVPC_DIM_MAX 640u
/* GDAT stride hypothesis (worldgen_score_city_sites weights[0x18]):
 * sizeof row? TBD via Ghidra 1198 compare; documented, not applied. */
#define RSC_GDAT_STRIDE_HYP 0x18u

long rsc_cvpc_head_off(const uint8_t *data, size_t size)
{
    /* Rsc_CvPcHeadOff @ 1198:xxxx: scan for a plausible INDEX8 header
     * (LE layout matching rsc_decode_bitmap: u16 w + u16 h + u8 bpp==8 +
     * u8 pal 1..255). CvPc slices (BB DF...) fail the probe at 0, so the
     * header may sit at offset (8-byte tag + compressed data hypothesis,
     * w/h after decompress) or the codec differs (EDI LZSS2 ring 0x20?
     * GIF LZW?). Only header fields are gated here (w/h 1..640); pixel
     * bounds are NOT required since compressed slices may be shorter. */
    size_t scan = 0;
    size_t off = 0;

    if (!data)
        return -1;
    if (size < 6)
        return -1;
    scan = size < RSC_CVPC_HEAD_SCAN_MAX ? size : RSC_CVPC_HEAD_SCAN_MAX;
    if (scan < 6)
        return -1;
    for (off = 0; off + 6 <= scan; off++) {
        uint32_t w = (uint32_t)data[off] | ((uint32_t)data[off + 1] << 8);
        uint32_t h = (uint32_t)data[off + 2] | ((uint32_t)data[off + 3] << 8);
        uint8_t bpp = data[off + 4];
        uint8_t pal = data[off + 5];

        if (w < 1 || w > RSC_CVPC_DIM_MAX)
            continue;
        if (h < 1 || h > RSC_CVPC_DIM_MAX)
            continue;
        if (bpp != 8)
            continue;
        if (pal == 0)
            continue;
        return (long)off;
    }
    return -1;
}

rsc_error_t rsc_gdat_stride(const uint8_t *data, size_t size,
                            size_t *stride_out)
{
    /* Rsc_GdatStride @ 1198:xxxx STUB (6 recs, unk 0x146a; weights @
     * 1420:17d6 *3 + 1420:17d8 hypothesis, moisture @ 1420:12b8/12bc).
     * Candidate RSC_GDAT_STRIDE_HYP (0x18) from weights[0x18] is
     * documented, not returned: stride TBD via Ghidra 1198 compare, so
     * *stride_out stays untouched on UNSUPPORTED (no decode fake). */
    (void)RSC_GDAT_STRIDE_HYP;
    if (!data || !stride_out)
        return RSC_ERR_NOENT;
    if (size == 0)
        return RSC_ERR_TRUNCATED;
    return RSC_ERR_UNSUPPORTED;
}

rsc_error_t rsc_kdat_stride(const uint8_t *data, size_t size,
                            size_t *stride_out)
{
    /* Rsc_KdatStride @ 1198:xxxx STUB (13 recs, unk 0x14be; table
     * hypothesis, stride unknown). Layout TBD via Ghidra 1198 compare;
     * caller logs the rsc_record_data slice head 16 B via
     * gfx_probe_head_hex (read-only). No fake: *stride_out untouched on
     * UNSUPPORTED. Borrowed slice, no SDL/exit/globals, C17. */
    if (!data || !stride_out)
        return RSC_ERR_NOENT;
    if (size == 0)
        return RSC_ERR_TRUNCATED;
    return RSC_ERR_UNSUPPORTED;
}

rsc_error_t rsc_text_index(const uint8_t *data, size_t size,
                           size_t *offsets, size_t cap, size_t *out_n)
{
    /* Rsc_TextIndex @ 1198:xxxx: per-slice NUL walk (single-string
     * records yield 1 offset; concatenated blobs yield N; empty strings
     * count; trailing unterminated bytes ignored). The 398-rec full index
     * (civdata0 TEXT unk 0x01b6) is a caller loop over rsc_record_data
     * slices (borrowed, valid while rsc_file lives) calling this per
     * slice; offsets are per-slice (add slice base for absolute logs). */
    size_t pos = 0;
    size_t n = 0;

    if (!data || !out_n)
        return RSC_ERR_NOENT;
    if (cap > 0 && !offsets)
        return RSC_ERR_NOENT;
    if (size == 0)
        return RSC_ERR_TRUNCATED;
    *out_n = 0;
    while (pos < size) {
        const uint8_t *nul =
            (const uint8_t *)memchr(data + pos, 0, size - pos);
        if (!nul)
            break;
        if (n >= cap) {
            *out_n = n;
            return RSC_ERR_RANGE;
        }
        offsets[n++] = pos;
        pos = (size_t)(nul - data) + 1u;
    }
    *out_n = n;
    if (n == 0)
        return RSC_ERR_BAD_MAGIC;
    return RSC_OK;
}

rsc_error_t rsc_strlist_all(const uint8_t *data, size_t size,
                            void (*cb)(unsigned index, const uint8_t *str,
                                       size_t len, void *ctx),
                            void *ctx)
{
    /* Rsc_StrListAll @ 1198:xxxx: count + per-index get loop (32 recs
     * civdata0 STR#; BE count via rsc_strlist_count, u8-len entries via
     * rsc_strlist_get, trailing allowed). Log-ready: no I/O here, caller
     * logs inside cb in index order; NULL cb is a validate-only walk. */
    uint16_t count = 0;
    rsc_error_t cr;
    unsigned i = 0;

    if (!data)
        return RSC_ERR_NOENT;
    cr = rsc_strlist_count(data, size, &count);
    if (cr != RSC_OK)
        return cr;
    for (i = 0; i < (unsigned)count; i++) {
        const uint8_t *str = NULL;
        size_t len = 0;
        rsc_error_t gr = rsc_strlist_get(data, size, i, &str, &len);
        if (gr != RSC_OK)
            return gr;
        if (cb)
            cb(i, str, len, ctx);
    }
    return RSC_OK;
}

int rsc_cvpc_is_compressed(const uint8_t *data, size_t size)
{
    /* Rsc_CvPcIsCompressed @ 1198:xxxx predicate (BB DF... observation):
     * first byte >= 0x80 (literal tag under gfx_decode_rle <0x80 repeat /
     * >=0x80 literal) + strict probe fails at 0 (bad w/h/bpp/pal) +
     * plausible header at offset > 0 (8-byte tag + compressed data
     * hypothesis; alt codecs EDI LZSS2 ring 0x20? GIF LZW? TBD). Raw
     * INDEX8 at 0 or no offset header (or short/NULL input) is 0. */
    rsc_indexed_t probe;
    long off = 0;

    if (!data || size == 0)
        return 0;
    if (data[0] < 0x80)
        return 0;
    if (rsc_decode_bitmap(data, size, &probe) == RSC_OK)
        return 0;
    off = rsc_cvpc_head_off(data, size);
    return off > 0 ? 1 : 0;
}

/* --- AD1: slice diagnostics + stats (segment 1198, OS-free) --- */

size_t rsc_slice_len(const rsc_file_t *f, size_t i)
{
    /* Rsc_SliceLen @ 1198:xxxx diagnostic accessor over ResMgr_LoadRsc
     * @ 1190:0000 dir walk (rsc_record_data slice: BE24 @ +5 / BE16 @ +10,
     * len 0 derives to next/EOF; borrowed, valid while rsc_file lives).
     * Returns size on RSC_OK, 0 on any error (NULL file, RANGE end,
     * UNSUPPORTED dir unproven, TRUNCATED overrun). No SDL/exit/globals. */
    const uint8_t *data = NULL;
    size_t size = 0;

    if (!f)
        return 0;
    if (rsc_record_data(f, i, &data, &size) != RSC_OK)
        return 0;
    return size;
}

rsc_error_t rsc_text_stats(const rsc_file_t *f, rsc_text_stats_t *out)
{
    /* Rsc_TextStats @ 1198:xxxx aggregate (TEXT 398 recs civdata0, unk
     * 0x01b6; menu @ 1420:435b, difficulty @ 1420:2a5a; " 90% Science)").
     * Loops rsc_record_data slices 0..8191 until RANGE/UNSUPPORTED; per
     * slice counts strings via rsc_text_index (Rsc_TextIndex @ 1198:xxxx;
     * cap 64 stack, heap 1024 retry on RANGE), chars/longest via bounded
     * NUL lens, hits via rsc_text_find " 90%" (Rsc_TextFind @ 1198:xxxx,
     * record-level). Non-TEXT (BAD_MAGIC, no NUL) and empty (TRUNCATED)
     * slices skip; no logging here (no flood; caller logs). */
    static const uint8_t kNeedle[] = {' ', '9', '0', '%'};
    size_t total_strings = 0;
    size_t total_chars = 0;
    size_t longest = 0;
    size_t hits = 0;
    size_t i = 0;

    if (!f || !out)
        return RSC_ERR_NOENT;
    out->total_strings = 0;
    out->total_chars = 0;
    out->longest_len = 0;
    out->hits_90pct = 0;
    for (i = 0; i < 8192u; i++) {
        const uint8_t *data = NULL;
        size_t size = 0;
        size_t offs_stack[64];
        size_t n = 0;
        size_t k = 0;
        rsc_error_t dr = rsc_record_data(f, i, &data, &size);

        if (dr == RSC_ERR_RANGE || dr == RSC_ERR_UNSUPPORTED)
            break;
        if (dr != RSC_OK)
            continue;
        {
            rsc_error_t tr =
                rsc_text_index(data, size, offs_stack, 64u, &n);
            if (tr == RSC_ERR_TRUNCATED)
                continue;
            if (tr == RSC_ERR_BAD_MAGIC)
                continue;
            if (tr == RSC_ERR_RANGE) {
                /* More than 64 strings in one slice: retry with a bounded
                 * heap buffer (1024 offsets); still RANGE after that counts
                 * the capped 1024 (diagnostic truncation, documented). */
                size_t cap_big = 1024u;
                size_t *offs_big =
                    (size_t *)malloc(cap_big * sizeof(size_t));
                size_t n_big = 0;
                if (!offs_big) {
                    /* NOMEM: count the 64 we already have, keep walking. */
                    n = 64u;
                } else {
                    rsc_error_t tr2 =
                        rsc_text_index(data, size, offs_big, cap_big,
                                       &n_big);
                    if (tr2 == RSC_OK || tr2 == RSC_ERR_RANGE) {
                        /* Count + measure from the big buffer. */
                        for (k = 0; k < n_big; k++) {
                            size_t off = offs_big[k];
                            const uint8_t *nul = NULL;
                            size_t len = 0;
                            if (off >= size)
                                continue;
                            nul = (const uint8_t *)memchr(data + off, 0,
                                                          size - off);
                            if (!nul)
                                continue;
                            len = (size_t)(nul - (data + off));
                            total_strings++;
                            total_chars += len;
                            if (len > longest)
                                longest = len;
                        }
                        free(offs_big);
                        if (rsc_text_find(data, size, kNeedle,
                                          sizeof(kNeedle)) >= 0)
                            hits++;
                        continue;
                    }
                    free(offs_big);
                    if (tr2 == RSC_ERR_TRUNCATED ||
                        tr2 == RSC_ERR_BAD_MAGIC)
                        continue;
                    n = 64u;
                }
            }
            if (tr != RSC_OK && tr != RSC_ERR_RANGE)
                continue;
            /* n holds stored offsets (cap when RANGE); measure them. */
            for (k = 0; k < n; k++) {
                size_t off = offs_stack[k];
                const uint8_t *nul = NULL;
                size_t len = 0;
                if (off >= size)
                    continue;
                nul = (const uint8_t *)memchr(data + off, 0, size - off);
                if (!nul)
                    continue;
                len = (size_t)(nul - (data + off));
                total_strings++;
                total_chars += len;
                if (len > longest)
                    longest = len;
            }
            if (rsc_text_find(data, size, kNeedle, sizeof(kNeedle)) >= 0)
                hits++;
        }
    }
    out->total_strings = total_strings;
    out->total_chars = total_chars;
    out->longest_len = longest;
    out->hits_90pct = hits;
    return RSC_OK;
}

const uint8_t *rsc_gdat_row(const uint8_t *data, size_t size,
                             unsigned index)
{
    /* Rsc_GdatRow @ 1198:xxxx diagnostic row view (GDAT 6 recs, unk 0x146a;
     * weights @ 1420:17d6 *3 + 1420:17d8 hypothesis, moisture @ 1420:12b8/
     * 12bc; cf. worldgen_score_city_sites @ 1028:0fb4 weights[0x18]).
     * Tries stride RSC_GDAT_ROW_STRIDE (0x18): row = 0x18 bytes at
     * index*stride. Bounds only, overflow-safe; layout TBD via Ghidra 1198
     * compare, no field split, no decode fake. Borrowed slice. */
    size_t off = 0;

    if (!data)
        return NULL;
    if (size == 0)
        return NULL;
    off = (size_t)index * (size_t)RSC_GDAT_ROW_STRIDE;
    if (index != 0 &&
        off / (size_t)index != (size_t)RSC_GDAT_ROW_STRIDE)
        return NULL;
    if (off + (size_t)RSC_GDAT_ROW_STRIDE > size)
        return NULL;
    return data + off;
}

const uint8_t *rsc_kdat_row(const uint8_t *data, size_t size,
                             unsigned index)
{
    /* Rsc_KdatRow @ 1198:xxxx diagnostic row view (KDAT 13 recs, unk 0x14be;
     * table hypothesis, stride TBD; tried as RSC_KDAT_ROW_STRIDE 0x20 until
     * the Ghidra 1198 compare lands; caller logs head 16 B via
     * gfx_probe_head_hex, read-only). Same bounds shape as rsc_gdat_row:
     * row = 0x20 bytes at index*stride. Borrowed slice, no fake. */
    size_t off = 0;

    if (!data)
        return NULL;
    if (size == 0)
        return NULL;
    off = (size_t)index * (size_t)RSC_KDAT_ROW_STRIDE;
    if (index != 0 &&
        off / (size_t)index != (size_t)RSC_KDAT_ROW_STRIDE)
        return NULL;
    if (off + (size_t)RSC_KDAT_ROW_STRIDE > size)
        return NULL;
    return data + off;
}

/* --- AE2: TEXT full walk + GDAT/KDAT row decode (segment 1198, OS-free) --- */

rsc_error_t rsc_text_get_all(const rsc_file_t *f, rsc_text_loc_t *locs,
                             size_t cap, size_t *out_n, size_t *out_total)
{
    /* Rsc_TextGetAll @ 1198:xxxx file walk (TEXT 398 recs civdata0, unk
     * 0x01b6; total 14977 strings via the rsc_text_stats shape). Walks dir
     * slices 0..8191 via rsc_record_data (borrowed, valid while rsc_file
     * lives) until RANGE (end) or UNSUPPORTED (dir unproven); per slice
     * indexes via rsc_text_index (cap 64 stack, heap 1024 retry on RANGE;
     * TRUNCATED empty and BAD_MAGIC non-TEXT skipped). Collects the first
     * cap locations (rec = slice index, off = per-slice offset) and counts
     * the full total (summary always, no flood, no logging here). */
    size_t stored = 0;
    size_t total = 0;
    size_t i = 0;

    if (!f || !out_n || !out_total)
        return RSC_ERR_NOENT;
    if (cap > 0 && !locs)
        return RSC_ERR_NOENT;
    for (i = 0; i < 8192u; i++) {
        const uint8_t *data = NULL;
        size_t size = 0;
        size_t offs_stack[64];
        size_t n = 0;
        size_t k = 0;
        rsc_error_t dr = rsc_record_data(f, i, &data, &size);

        if (dr == RSC_ERR_RANGE || dr == RSC_ERR_UNSUPPORTED)
            break;
        if (dr != RSC_OK)
            continue;
        {
            rsc_error_t tr =
                rsc_text_index(data, size, offs_stack, 64u, &n);
            if (tr == RSC_ERR_TRUNCATED)
                continue;
            if (tr == RSC_ERR_BAD_MAGIC)
                continue;
            if (tr == RSC_ERR_RANGE) {
                size_t cap_big = 1024u;
                size_t *offs_big =
                    (size_t *)malloc(cap_big * sizeof(size_t));
                size_t n_big = 0;

                if (!offs_big) {
                    n = 64u;
                } else {
                    rsc_error_t tr2 =
                        rsc_text_index(data, size, offs_big, cap_big,
                                       &n_big);
                    if (tr2 == RSC_OK || tr2 == RSC_ERR_RANGE) {
                        for (k = 0; k < n_big; k++) {
                            total++;
                            if (stored < cap) {
                                locs[stored].rec = i;
                                locs[stored].off = offs_big[k];
                                stored++;
                            }
                        }
                        free(offs_big);
                        continue;
                    }
                    free(offs_big);
                    if (tr2 == RSC_ERR_TRUNCATED ||
                        tr2 == RSC_ERR_BAD_MAGIC)
                        continue;
                    n = 64u;
                }
            }
            if (tr != RSC_OK && tr != RSC_ERR_RANGE)
                continue;
            for (k = 0; k < n; k++) {
                total++;
                if (stored < cap) {
                    locs[stored].rec = i;
                    locs[stored].off = offs_stack[k];
                    stored++;
                }
            }
        }
    }
    *out_n = stored;
    *out_total = total;
    return total > cap ? RSC_ERR_RANGE : RSC_OK;
}

rsc_error_t rsc_text_search_all(const rsc_file_t *f, const uint8_t *needle,
                                size_t needle_len, rsc_text_loc_t *hits,
                                size_t cap, size_t *out_n,
                                size_t *out_total)
{
    /* Rsc_TextSearchAll @ 1198:xxxx file scan (" 90%" hits=3 in civdata0,
     * 0 others; record-level via rsc_text_find, matches rsc_text_stats
     * hits_90pct). Walks slices 0..8191 via rsc_record_data (borrowed)
     * until RANGE/UNSUPPORTED; per slice verifies TEXT via rsc_text_index
     * cap-1 probe (TRUNCATED/BAD_MAGIC skipped, OK/RANGE kept) then scans
     * via rsc_text_find (portable memmem, no flood, no logging here). */
    size_t stored = 0;
    size_t total = 0;
    size_t i = 0;

    if (!f || !needle || !out_n || !out_total)
        return RSC_ERR_NOENT;
    if (needle_len == 0)
        return RSC_ERR_NOENT;
    if (cap > 0 && !hits)
        return RSC_ERR_NOENT;
    for (i = 0; i < 8192u; i++) {
        const uint8_t *data = NULL;
        size_t size = 0;
        size_t probe[1];
        size_t pn = 0;
        long found = -1;
        rsc_error_t dr = rsc_record_data(f, i, &data, &size);

        if (dr == RSC_ERR_RANGE || dr == RSC_ERR_UNSUPPORTED)
            break;
        if (dr != RSC_OK)
            continue;
        {
            rsc_error_t tr = rsc_text_index(data, size, probe, 1u, &pn);
            if (tr == RSC_ERR_TRUNCATED)
                continue;
            if (tr == RSC_ERR_BAD_MAGIC)
                continue;
            if (tr != RSC_OK && tr != RSC_ERR_RANGE)
                continue;
        }
        found = rsc_text_find(data, size, needle, needle_len);
        if (found < 0)
            continue;
        total++;
        if (stored < cap) {
            hits[stored].rec = i;
            hits[stored].off = (size_t)found;
            stored++;
        }
    }
    *out_n = stored;
    *out_total = total;
    return total > cap ? RSC_ERR_RANGE : RSC_OK;
}

rsc_error_t rsc_gdat_decode_row(const uint8_t *data, size_t size,
                                unsigned index, rsc_gdat_row_dec_t *out)
{
    /* Rsc_GdatDecodeRow @ 1198:xxxx diagnostic u16 (GDAT 6 recs, unk 0x146a;
     * stride 0x18 via rsc_gdat_row bounds; 12 x u16 BE try to match the
     * dir-layer BE via ResMgr_ReadU16 @ 1190:1953; TBD via Ghidra 1198
     * compare, no field split beyond the array, no fake). */
    const uint8_t *row = NULL;
    unsigned k = 0;

    if (!data || !out)
        return RSC_ERR_NOENT;
    if (size == 0)
        return RSC_ERR_TRUNCATED;
    row = rsc_gdat_row(data, size, index);
    if (!row)
        return RSC_ERR_RANGE;
    for (k = 0; k < 12u; k++) {
        out->v[k] = (uint16_t)(((uint16_t)row[2u * k] << 8) |
                               (uint16_t)row[2u * k + 1u]);
    }
    return RSC_OK;
}

rsc_error_t rsc_kdat_decode_row(const uint8_t *data, size_t size,
                                unsigned index, rsc_kdat_row_dec_t *out)
{
    /* Rsc_KdatDecodeRow @ 1198:xxxx diagnostic u16 (KDAT 13 recs, unk 0x14be;
     * stride 0x20 via rsc_kdat_row bounds; 16 x u16 BE, same BE try as
     * GDAT until the Ghidra 1198 compare lands; no fake). */
    const uint8_t *row = NULL;
    unsigned k = 0;

    if (!data || !out)
        return RSC_ERR_NOENT;
    if (size == 0)
        return RSC_ERR_TRUNCATED;
    row = rsc_kdat_row(data, size, index);
    if (!row)
        return RSC_ERR_RANGE;
    for (k = 0; k < 16u; k++) {
        out->v[k] = (uint16_t)(((uint16_t)row[2u * k] << 8) |
                               (uint16_t)row[2u * k + 1u]);
    }
    return RSC_OK;
}

rsc_error_t res_append_pascal(uint8_t *dst, size_t cap,
                              const uint8_t *src)
{
    /* Orig FUN_11f8_014a @ 11f8:014a -> Res_AppendPascal.
     * Decompiler: b = *src; src++; dst_end = dst + *dst + 1; *dst += b;
     * then copy b bytes. Callee of City_FormatName @ 1048:0101 x2 +
     * ResMgr_GetString @ 1100:078a blob walk. OS-free 1:1 on caller
     * buffers (no Win16, no globals). */
    unsigned dl = 0;
    unsigned sl = 0;
    unsigned i = 0;

    if (!dst || !src)
        return RSC_ERR_NOENT;
    if (cap == 0)
        return RSC_ERR_RANGE;
    dl = dst[0];
    sl = src[0];
    if (dl + sl > 255u)
        return RSC_ERR_RANGE;
    if ((size_t)dl + (size_t)sl + 1u > cap)
        return RSC_ERR_RANGE;
    for (i = 0; i < sl; i++)
        dst[1u + dl + i] = src[1u + i];
    dst[0] = (uint8_t)(dl + sl);
    return RSC_OK;
}

int res_compare_pascal(const uint8_t *a, const uint8_t *b)
{
    /* Orig FUN_11f8_0207 @ 11f8:0207 -> Res_ComparePascal.
     * Decompiler: la = *a++; lb = *b++; n = min(la, lb); memcmp n
     * bytes, first diff (unsigned) decides; else la - lb. NULL treated
     * as empty (len 0) so the helper stays total. OS-free 1:1. */
    unsigned la = a ? a[0] : 0u;
    unsigned lb = b ? b[0] : 0u;
    unsigned n = la < lb ? la : lb;
    unsigned i = 0;

    for (i = 0; i < n; i++) {
        unsigned ca = a ? a[1u + i] : 0u;
        unsigned cb = b ? b[1u + i] : 0u;

        if (ca != cb)
            return (int)ca - (int)cb;
    }
    return (int)la - (int)lb;
}

rsc_error_t res_copy_pascal(uint8_t *dst, size_t cap,
                            const uint8_t *src)
{
    /* Orig FUN_11f8_00fc @ 11f8:00fc -> Res_CopyPascal.
     * Decompiler: *dst = *src (len byte), then copy len bytes + NUL
     * walk shape (len+1 total incl. the len byte loop). OS-free 1:1. */
    unsigned sl = 0;
    unsigned i = 0;

    if (!dst || !src)
        return RSC_ERR_NOENT;
    if (cap == 0)
        return RSC_ERR_RANGE;
    sl = src[0];
    if ((size_t)sl + 1u > cap)
        return RSC_ERR_RANGE;
    dst[0] = src[0];
    for (i = 0; i < sl; i++)
        dst[1u + i] = src[1u + i];
    return RSC_OK;
}

rsc_error_t res_get_string_copy(unsigned table, unsigned entry,
                                char *out, size_t cap)
{
    (void)table;
    (void)entry;
    /* TODO: Orig FUN_11f8_0080 @ 11f8:0080 (Res_GetStringCopy:
     * DAT_13f8_0000 = 0 + ResMgr_GetString(table, entry + 1 into the
     * shared static @ DAT_13f8_0000) + Util_PascalToCStr @ 1100:0053;
     * returns the shared static pointer, overwritten by the next
     * GetString call — caller must copy immediately; see rsc_loader.h
     * ownership note) -> Linux has no shared static (no globals), so
     * the blob fetch (rsc_get_blob_string @ 1100:078a, LIVE) + copy
     * wiring stays TBD. */
    if (!out)
        return RSC_ERR_NOENT;
    if (cap > 0)
        out[0] = '\0';
    return RSC_ERR_UNSUPPORTED;
}

rsc_error_t res_get_string_raw(unsigned table, unsigned entry,
                               uint8_t *out, size_t cap)
{
    (void)table;
    (void)entry;
    /* TODO: Orig FUN_11f8_00c5 @ 11f8:00c5 (Res_GetStringRaw: same as
     * copy above but WITHOUT Util_PascalToCStr — Pascal length byte
     * kept; same shared static @ DAT_13f8_0000 ownership) -> same TBD. */
    if (!out)
        return RSC_ERR_NOENT;
    if (cap > 0)
        out[0] = 0;
    return RSC_ERR_UNSUPPORTED;
}
