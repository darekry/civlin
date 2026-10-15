/* CivLin RSC container probe (v1 implementation). See rsc_loader.h for the
 * documented format. Read-only: never writes to civ1win/ or unpacked/. */
#include "rsc_loader.h"
#include "civ_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RSC_MIN_SIZE 64u
#define RSC_SCAN_LEN 1024u
#define RSC_MAX_RECORDS 256u

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

/* Orig mult table @ 0x3d00 is {1, 0x100, ...}; plain shifts are the
 * 64-bit-Linux equivalent (no __far segment carry). */
uint16_t rsc_read_u16le(rsc_cursor_t *cur)
{
    uint16_t v = 0;

    /* ResMgr_ReadU16 @ 1190:1953 (+ twin @ 1190:19c9): two ascending
     * bytes, mult 1 then 0x100. */
    if (!cur || !cur->ptr || cur->ptr + 2 > cur->end)
        return 0;
    v = (uint16_t)cur->ptr[0] | ((uint16_t)cur->ptr[1] << 8);
    cur->ptr += 2;
    return v;
}

uint32_t rsc_read_u32le(rsc_cursor_t *cur)
{
    uint32_t v = 0;

    /* ResMgr_ReadU32 @ 1190:1a3f: four ascending bytes. */
    if (!cur || !cur->ptr || cur->ptr + 4 > cur->end)
        return 0;
    v = rd_u32le(cur->ptr);
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

rsc_error_t rsc_record_data(const rsc_file_t *f, size_t i,
                            const uint8_t **data, size_t *size)
{
    (void)f;
    (void)i;
    (void)data;
    (void)size;
    /* TODO: full type-dir walk per ResMgr_LoadRsc @ 1190:0000 (header
     * u32+u32 then u16 count via rsc_read_u16le, per-type 4-char tag +
     * counts, per-record 0x32-B header with file-off @ +4). The integer
     * readers above are already 1:1; the list manager (file list @
     * 1420:b2c0, type list @ 1420:b2c2, GLOBALALLOC nodes) and lazy
     * ResMgr_LoadToMem @ 1190:0fda (_LLSEEK/_HREAD) need the Linux file
     * + malloc equivalents before carving is safe. */
    return RSC_ERR_UNSUPPORTED;
}

size_t rsc_image_count(const rsc_file_t *f)
{
    (void)f;
    /* No record passes strict DIB validation in v1 (see header comment). */
    return 0;
}

rsc_error_t rsc_get_indexed(const rsc_file_t *f, size_t i, rsc_indexed_t *out)
{
    (void)f;
    (void)i;
    (void)out;
    return RSC_ERR_UNSUPPORTED;
}
