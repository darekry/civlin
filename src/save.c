/* CivLin SAVE/LOAD. See save.h for Orig FUN_* -> new name mapping.
 * RLL codec (save_rll_encode/save_rll_decode, Orig FUN_10f8_05da/040a cores)
 * is decoded and implemented natively below. Packet N: serialize/
 * deserialize/marshal stay UNSUPPORTED stubs (live Win16 segments 12d8/...
 * have no Linux mapping yet; RLL + file I/O cores they would call are
 * ported); xfer words/block/array/strided (@ 10f8:0f40/0fcf/1035/10bd) own
 * only the OS-free image-cursor half (bounds-checked reservation, no Win16,
 * no globals). Packet L file I/O (save_file_* @ 1140) is implemented below
 * on POSIX fds — the HFILE-native twin of the FILE* File_* wrappers (same
 * Win16 imports, same chunk/contract semantics). */
#define _POSIX_C_SOURCE 200809L
#include "save.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

civ_error_t save_prompt_open(civ_save_dialog_t *out)
{
    if (!out)
        return CIV_ERR_NOENT;
    memset(out, 0, sizeof(*out));
    /* TODO: SDL3 file dialog with CIV_SAVE_FILTER; compare with
     * Ghidra save_prompt_open (Orig FUN_1130_0075 @ 1130:0075,
     * GETOPENFILENAME flow). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_prompt_save(civ_save_dialog_t *out)
{
    if (!out)
        return CIV_ERR_NOENT;
    memset(out, 0, sizeof(*out));
    /* TODO: SDL3 save dialog; compare with Ghidra save_prompt_save
     * (Orig FUN_1130_0194 @ 1130:0194, GETSAVEFILENAME +
     * COMMDLGEXTENDEDERROR flow). */
    return CIV_ERR_UNSUPPORTED;
}

win16_handle save_file_open(const char *path, int write_mode)
{
    /* Save_FileOpen, Orig FUN_1140_0139 @ 1140:0139: wraps _LOPEN via
     * File_OpenWrapper @ 1000:2ad9. Linux twin: open() with the same
     * contract as file_open_wrapper (fopen "rb"/"wb"): NULL/empty path
     * fails; write_mode 0 = OF_READ, nonzero = OF_WRITE|OFCREATE|TRUNC.
     * HFILE transport (int fd) for save_serialize @ 10f8:02bd (write),
     * save_deserialize @ 10f8:0000 (read), HoF_Load @ 1080:0023 (read),
     * HoF_Save @ 1080:0092 (write). Returns fd or WIN16_HFILE_ERROR. */
    int fd;

    if (!path || !*path)
        return WIN16_HFILE_ERROR;
    if (write_mode)
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    else
        fd = open(path, O_RDONLY);
    return fd < 0 ? WIN16_HFILE_ERROR : fd;
}

long save_file_size(win16_handle h)
{
    /* Save_FileSize, Orig FUN_1140_0295 @ 1140:0295: via _LLSEEK (fseek/
     * ftell twin file_seek_wrapper). Position-preserving size probe:
     * save SEEK_CUR, seek SEEK_END for size, restore cursor — mirrors
     * _LLSEEK(0,1) / _LLSEEK(0,2) / _LLSEEK(saved,0). Returns size or -1
     * on bad handle/seek failure (mirrors _LLSEEK HFILE_ERROR). Accepted
     * .sav image sizes are checked by the caller save_deserialize
     * @ 10f8:0000, not here: 0x1a2ba canonical, 0x1a2b8 alt,
     * 0x18de0 small-map. */
    off_t cur;
    off_t end;

    if (h < 0)
        return -1L;
    cur = lseek(h, 0, SEEK_CUR);
    if (cur == (off_t)-1)
        return -1L;
    end = lseek(h, 0, SEEK_END);
    if (end == (off_t)-1)
        return -1L;
    if (lseek(h, cur, SEEK_SET) == (off_t)-1)
        return -1L;
    return (long)end;
}

int save_file_read(win16_handle h, uint8_t *dst, unsigned long len,
                   unsigned long *out_read)
{
    /* Save_FileRead, Orig FUN_1140_019d @ 1140:019d: chunked 0x8000 via
     * _HREAD/_LREAD (= File_ReadChunk fread twin file_read_chunk in
     * rsc_loader.h). Loop 1:1 with orig: 0x8000-byte chunks until len
     * satisfied or short read (EOF/error). Returns 0 when total == len,
     * 1 on short/EOF/bad args. Used by save_deserialize @ 10f8:0000 and
     * HoF_Load @ 1080:0023. */
    unsigned long total = 0;

    if (out_read)
        *out_read = 0;
    if (len == 0)
        return 0;
    if (h < 0 || !dst)
        return 1;
    while (total < len) {
        unsigned long left = len - total;
        unsigned long want =
            left > CIV_SAVE_READ_CHUNK ? CIV_SAVE_READ_CHUNK : left;
        ssize_t n = read(h, dst + total, (size_t)want);

        if (n <= 0)
            break;
        total += (unsigned long)n;
        if ((unsigned long)n < want)
            break;
    }
    if (out_read)
        *out_read = total;
    return total == len ? 0 : 1;
}

int save_file_write(win16_handle h, const uint8_t *src, unsigned long len,
                    unsigned long *out_written)
{
    /* Save_FileWrite, Orig FUN_1140_0245 @ 1140:0245: chunked 0x8000 via
     * _HWRITE/_LWRITE (= File_WriteChunk fwrite twin file_write_chunk in
     * sys.h). Same 0x8000 segment-bound chunking as the read path above;
     * partial writes accumulate until len satisfied or error. Returns 0
     * when total == len, 1 on short/error/bad args. Used by
     * save_serialize @ 10f8:02bd and HoF_Save @ 1080:0092. */
    unsigned long total = 0;

    if (out_written)
        *out_written = 0;
    if (len == 0)
        return 0;
    if (h < 0 || !src)
        return 1;
    while (total < len) {
        unsigned long left = len - total;
        unsigned long want =
            left > CIV_SAVE_READ_CHUNK ? CIV_SAVE_READ_CHUNK : left;
        ssize_t n = write(h, src + total, (size_t)want);

        if (n <= 0)
            break;
        total += (unsigned long)n;
        if ((unsigned long)n < want)
            break;
    }
    if (out_written)
        *out_written = total;
    return total == len ? 0 : 1;
}

void save_file_close(win16_handle h)
{
    /* Save_FileClose, Orig FUN_1140_037c @ 1140:037c: wraps _LCLOSE via
     * File_CloseWrapper @ 1000:1f7c (fclose NULL-safe twin). HFILE
     * transport: close() on valid fd, no-op on WIN16_HFILE_ERROR /
     * negative (mirrors file_close_wrapper NULL-safe no-op). Errors
     * ignored (void return, as in orig and file_close_wrapper). */
    if (h >= 0)
        close(h);
}

FILE *file_open_wrapper(const char *path, int write_mode)
{
    /* File_OpenWrapper @ 1000:2ad9: _LOPEN thin wrapper used by
     * save_file_open @ 1140:0139. fopen is the Linux equivalent
     * (write_mode ? "wb" : "rb"); NULL on bad args or open failure. */
    if (!path || !*path)
        return NULL;
    return fopen(path, write_mode ? "wb" : "rb");
}

void file_close_wrapper(FILE *fp)
{
    /* File_CloseWrapper @ 1000:1f7c: _LCLOSE thin wrapper used by
     * save_file_close @ 1140:037c. fclose; NULL-safe no-op. */
    if (fp)
        fclose(fp);
}

long file_seek_wrapper(FILE *fp, long offset, int origin)
{
    /* File_SeekWrapper @ 1000:xxxx (packet E): _LLSEEK thin wrapper used
     * by save_file_size @ 1140:0295 and the ResMgr_LoadToMem @ 1190:0fda
     * payload path. fseek + ftell is the Linux equivalent; origin passes
     * through to SEEK_SET/SEEK_CUR/SEEK_END (DOS _LLSEEK 0/1/2 match).
     * Returns new offset, -1 on NULL fp, bad origin, or seek/tell error. */
    long pos;
    if (!fp)
        return -1L;
    if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END)
        return -1L;
    if (fseek(fp, offset, origin) != 0)
        return -1L;
    pos = ftell(fp);
    return pos < 0 ? -1L : pos;
}

civ_error_t save_serialize(const char *path)
{
    /* Save_Serialize, Orig FUN_10f8_02bd @ 10f8:02bd (called from the
     * "&Save Game" menu path). Orig flow, stubbed here because the live
     * game ctx (segments 12d8/12d0/...) has no Linux mapping yet:
     *   1. alloc flat image 0x1a2ba (CIV_SAVE_BUF_SIZE; err string @
     *      1420:2cac "Could not allocate buffer for saving" on failure);
     *   2. save_marshal_fields @ 10f8:090c with direction save
     *      (g_nSaveDirection @ 1310:0008 == 0) to fill the image;
     *   3. save_pack @ 10f8:05da (RLL-encode via save_rll_encode core);
     *   4. save_file_open @ 1140:0139 O_WRITE, save_file_write @ 1140:0245,
     *      save_file_close @ 1140:037c.
     * Steps 3-4 cores are ported below; steps 1-2 wiring is TBD. No heap
     * taken, no file touched, no globals: validates path only. */
    if (!path || !*path)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_deserialize(const char *path)
{
    /* Save_Deserialize, Orig FUN_10f8_0000 @ 10f8:0000 (called from the
     * Load path + worldgen_build_ocean_links @ 1028:162c context). Orig
     * flow, stubbed for the same live-ctx reason:
     *   1. GLOBALALLOC 0xa2ba (CIV_SAVE_SCRATCH) scratch for the RLL window;
     *   2. save_file_open @ 1140:0139 O_READ, save_file_size @ 1140:0295 to
     *      accept 0x1a2ba canonical / 0x1a2b8 alt / 0x18de0 small-map (else
     *      err string @ 1420:2c74 "Could not load saved game. Corrupted or
     *      out of memory.");
     *   3. save_file_read @ 1140:019d (0x8000 chunks) + save_unpack @
     *      10f8:040a (RLL-decode via save_rll_decode core);
     *   4. save_marshal_fields @ 10f8:090c with direction load
     *      (g_nSaveDirection == 1) to scatter into live ctx;
     *   5. save_file_close @ 1140:037c.
     * Size gate belongs here when wired, not in save_file_size. No heap
     * taken, no file touched, no globals: validates path only. */
    if (!path || !*path)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_pack(const uint8_t **out_image, size_t *out_len)
{
    /* Save_Pack, Orig FUN_10f8_05da @ 10f8:05da (RLLEncode, called from
     * save_serialize @ 10f8:02bd). Thin legacy wrapper over the ported
     * save_rll_encode below (same <0x80 repeat / 0x80|n literal tags, same
     * 0x7f chunk cap): once save_marshal_fields yields the flat 0x1a2ba
     * image, packing is save_rll_encode(image, 0x1a2ba, dst, dst_cap,
     * out_len). Legacy (out_image, out_len) carries no caller dst/cap, so
     * no trivial delegate fits this signature — heap owner + dst wiring
     * land with save_serialize. Validates params only; read-only. */
    if (!out_image || !out_len)
        return CIV_ERR_NOENT;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_unpack(const uint8_t *image, size_t len)
{
    /* Save_Unpack, Orig FUN_10f8_040a @ 10f8:040a (RLLDecode, called from
     * save_deserialize @ 10f8:0000; 0x8000-chunked HMEMCPY in orig, plain
     * bounds-checked copy here). Thin legacy wrapper over the ported
     * save_rll_decode below: once the wire bytes are read, decoding is
     * save_rll_decode(image, len, dst, CIV_SAVE_BUF_SIZE, &flat_len) with
     * the 0x1a2ba/0x1a2b8/0x18de0 size gate in save_deserialize. Legacy
     * (image, len) carries no caller dst, so no trivial delegate fits —
     * dst wiring lands with save_deserialize. Validates params only. */
    if (!image)
        return CIV_ERR_NOENT;
    if (len == 0)
        return CIV_ERR_TRUNCATED;
    return CIV_ERR_UNSUPPORTED;
}

/* RLL as in save_pack @ 10f8:05da: runs >=3 -> (count, value) with
 * count byte <0x80 (chunked at 0x7f); else literals -> (0x80|count, bytes).
 * Mirrors the two inner scans (run detect, then literal-until-triple). */
civ_error_t save_rll_encode(const uint8_t *src, size_t src_len,
                            uint8_t *dst, size_t dst_cap, size_t *out_len)
{
    size_t ip = 0, op = 0;

    if (out_len)
        *out_len = 0;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    if (src_len == 0) {
        if (out_len)
            *out_len = 0;
        return CIV_OK;
    }

    while (ip < src_len) {
        size_t run = 1;
        while (ip + run < src_len && src[ip + run] == src[ip])
            run++;
        if (run >= 3) {
            size_t left = run;
            while (left > 0) {
                size_t chunk = left > CIV_SAVE_RLL_REPEAT_MAX
                    ? CIV_SAVE_RLL_REPEAT_MAX
                    : left;
                if (op + 2 > dst_cap)
                    return CIV_ERR_TRUNCATED;
                dst[op++] = (uint8_t)chunk;
                dst[op++] = src[ip];
                left -= chunk;
            }
            ip += run;
            continue;
        }
        /* Literal run: until triple-repeat ahead or end. */
        size_t lit_start = ip;
        size_t lit = 0;
        while (ip < src_len) {
            size_t r = 1;
            while (ip + r < src_len && src[ip + r] == src[ip])
                r++;
            if (r >= 3)
                break;
            ip++;
            lit++;
            /* Cap literals at 0x7f per tag; emit and continue. */
            if (lit == CIV_SAVE_RLL_REPEAT_MAX) {
                if (op + 1 + lit > dst_cap)
                    return CIV_ERR_TRUNCATED;
                dst[op++] = (uint8_t)(CIV_SAVE_RLL_LITERAL_FLAG | lit);
                for (size_t k = 0; k < lit; k++)
                    dst[op++] = src[lit_start + k];
                lit_start += lit;
                lit = 0;
            }
        }
        while (lit > 0) {
            size_t chunk = lit > CIV_SAVE_RLL_REPEAT_MAX
                ? CIV_SAVE_RLL_REPEAT_MAX
                : lit;
            if (op + 1 + chunk > dst_cap)
                return CIV_ERR_TRUNCATED;
            dst[op++] = (uint8_t)(CIV_SAVE_RLL_LITERAL_FLAG | chunk);
            for (size_t k = 0; k < chunk; k++)
                dst[op++] = src[lit_start + k];
            lit_start += chunk;
            lit -= chunk;
        }
        (void)lit_start;
    }
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

/* Inverse of above, as in save_unpack @ 10f8:040a (0x8000-chunked HMEMCPY
 * in orig; here plain bounds-checked). */
civ_error_t save_rll_decode(const uint8_t *src, size_t src_len,
                            uint8_t *dst, size_t dst_cap, size_t *out_len)
{
    size_t ip = 0, op = 0;

    if (out_len)
        *out_len = 0;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    while (ip < src_len) {
        uint8_t tag = src[ip++];
        if (tag < CIV_SAVE_RLL_LITERAL_FLAG) {
            if (ip >= src_len)
                return CIV_ERR_TRUNCATED;
            if (op + tag > dst_cap)
                return CIV_ERR_TRUNCATED;
            uint8_t v = src[ip++];
            for (unsigned k = 0; k < tag; k++)
                dst[op++] = v;
        } else {
            unsigned n = (unsigned)(tag - CIV_SAVE_RLL_LITERAL_FLAG);
            if (ip + n > src_len || op + n > dst_cap)
                return CIV_ERR_TRUNCATED;
            for (unsigned k = 0; k < n; k++)
                dst[op++] = src[ip++];
        }
    }
    if (out_len)
        *out_len = op;
    return CIV_OK;
}

civ_error_t save_marshal_fields(uint8_t *image, size_t cap)
{
    /* Save_MarshalFields, Orig FUN_10f8_090c @ 10f8:090c. Ordered xfer
     * driver over the CIV_SAVE_OFF_* layout (save.h): pre-word (live
     * 1310:0000 g_nSaveVariant) then words/block/array/strided calls below
     * and the 0xfa00 map block at +0xa8ba (live 1420:b394/b396). Direction
     * switch is g_nSaveDirection @ 1310:0008 (0 = save live->image, 1 =
     * load image->live); small-map variant shrinks the 8x0x20 / 0x100x0x20
     * arrays (see layout note). Cursor mirrors g_nSaveCursorOff @
     * 1420:a2d6 over base @ 1420:a2da. Stub: live segments have no Linux
     * mapping yet, so no xfer runs; validates the caller-owned image only.
     * Read-only on stub path (no byte written). */
    if (!image)
        return CIV_ERR_NOENT;
    if (cap < CIV_SAVE_BUF_MIN)
        return CIV_ERR_TRUNCATED;
    return CIV_ERR_UNSUPPORTED;
}

/* Image-cursor reservation shared by the xfer family: the OS-free half of
 * the orig mem_copy_far/HMEMCPY loops (no Win16, no globals). Checks
 * *cursor <= cap and len <= cap - *cursor, then advances *cursor by len.
 * Live far-pointer copy (seg:off <-> image window, direction by
 * g_nSaveDirection) is caller-owned via save_marshal_fields once the live
 * ctx is mapped; on Linux that copy will be bounds-checked memcpy. */
static civ_error_t xfer_reserve(size_t *cursor, size_t cap, size_t len)
{
    if (!cursor)
        return CIV_ERR_NOENT;
    if (*cursor > cap)
        return CIV_ERR_TRUNCATED;
    if (len > cap - *cursor)
        return CIV_ERR_TRUNCATED;
    *cursor += len;
    return CIV_OK;
}

civ_error_t save_xfer_words(uint8_t *image, size_t cap, size_t *cursor,
                            uint16_t seg, uint16_t off, unsigned words)
{
    /* Orig save_xfer_words (FUN_10f8_0f40 @ 10f8:0f40): move words*2 bytes
     * between MK_FP(seg, off) and image+*cursor (direction by
     * g_nSaveDirection @ 1310:0008). Ported image-cursor half 1:1 with
     * overflow + bounds checks; seg/off retained as the RE anchor (unused
     * at runtime until live ctx maps). No Win16, no globals. */
    size_t len;

    (void)seg;
    (void)off;
    if (!image || !cursor)
        return CIV_ERR_NOENT;
    if ((size_t)words > SIZE_MAX / 2u)
        return CIV_ERR_RANGE;
    len = (size_t)words * 2u;
    return xfer_reserve(cursor, cap, len);
}

civ_error_t save_xfer_block(uint8_t *image, size_t cap, size_t *cursor,
                            uint16_t seg, uint16_t off, unsigned len)
{
    /* Orig save_xfer_block (FUN_10f8_0fcf @ 10f8:0fcf): len bytes via
     * mem_copy_far between MK_FP(seg, off) and image+*cursor. Same
     * reservation contract as words, byte count. */
    (void)seg;
    (void)off;
    if (!image || !cursor)
        return CIV_ERR_NOENT;
    return xfer_reserve(cursor, cap, (size_t)len);
}

civ_error_t save_xfer_array(uint8_t *image, size_t cap, size_t *cursor,
                            uint16_t seg, uint16_t base, unsigned n,
                            unsigned elem_len)
{
    /* Orig save_xfer_array (FUN_10f8_1035 @ 10f8:1035): n elems, live
     * stride CIV_SAVE_ARRAY_LIVE_STRIDE (0x20), elem_len image bytes each
     * (full 0x20; small-map 0x0e/0x0c/0x0b/0x0d). Orig walks per-elem
     * mem_copy_far; ported as a per-elem reservation loop so a mid-image
     * overrun leaves *cursor at the failing elem start (matches orig
     * stop-short shape). seg/base are live anchors (base + i*0x20 in orig).
     * No Win16, no globals. */
    size_t total;
    unsigned i;

    (void)seg;
    (void)base;
    if (!image || !cursor)
        return CIV_ERR_NOENT;
    if (elem_len == 0 || n == 0)
        return CIV_OK;
    if ((size_t)n > SIZE_MAX / (size_t)elem_len)
        return CIV_ERR_RANGE;
    total = (size_t)n * (size_t)elem_len;
    if (*cursor > cap || total > cap - *cursor) {
        /* Per-elem advance to the failing elem for 1:1 stop-short. */
        for (i = 0; i < n; i++) {
            if (xfer_reserve(cursor, cap, (size_t)elem_len) != CIV_OK)
                return CIV_ERR_TRUNCATED;
        }
        return CIV_ERR_TRUNCATED;
    }
    for (i = 0; i < n; i++) {
        civ_error_t rc = xfer_reserve(cursor, cap, (size_t)elem_len);

        if (rc != CIV_OK)
            return rc;
    }
    return CIV_OK;
}

civ_error_t save_xfer_strided(uint8_t *image, size_t cap, size_t *cursor,
                              uint16_t seg, uint16_t base, unsigned n)
{
    /* Orig save_xfer_strided (FUN_10f8_10bd @ 10f8:10bd): n elems, each
     * skips CIV_SAVE_STRIDED_SKIP (0xc) live bytes then moves
     * CIV_SAVE_STRIDED_WORDS (0xb) words = CIV_SAVE_STRIDED_ELEM_LEN (22B)
     * packed per elem in the image (e.g. 28 elems at
     * CIV_SAVE_OFF_STRIDED_1420_1A38). Ported as a per-elem 22B
     * reservation loop; overrun leaves *cursor at the failing elem start.
     * seg/base are live anchors (base + i*(0xc+22) in orig). */
    unsigned i;

    (void)seg;
    (void)base;
    if (!image || !cursor)
        return CIV_ERR_NOENT;
    if (n == 0)
        return CIV_OK;
    for (i = 0; i < n; i++) {
        civ_error_t rc =
            xfer_reserve(cursor, cap, (size_t)CIV_SAVE_STRIDED_ELEM_LEN);

        if (rc != CIV_OK)
            return rc;
    }
    return CIV_OK;
}
