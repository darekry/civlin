/* CivLin SAVE/LOAD. See save.h for Orig FUN_* -> new name mapping.
 * RLL codec (save_pack/save_unpack, Orig FUN_10f8_05da/040a) is decoded
 * and implemented natively below. Marshal/xfer need live Win16 segments,
 * so they stay UNSUPPORTED stubs. Packet L file I/O (save_file_* @ 1140)
 * is implemented below on POSIX fds — the HFILE-native twin of the FILE*
 * File_* wrappers (same Win16 imports, same chunk/contract semantics). */
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
    (void)path;
    /* TODO: save_pack + save_file_open/write/close.
     * See Ghidra save_serialize (Orig FUN_10f8_02bd @ 10f8:02bd,
     * alloc 0x1a2ba, error 1420:2cac). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_deserialize(const char *path)
{
    (void)path;
    /* TODO: save_file_open/size/read + save_unpack.
     * See Ghidra save_deserialize (Orig FUN_10f8_0000 @ 10f8:0000,
     * error 1420:2c74, sizes 0x1a2ba/0x1a2b8/0x18de0). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_pack(const uint8_t **out_image, size_t *out_len)
{
    (void)out_image;
    (void)out_len;
    /* Legacy wrapper: real codec is save_rll_encode below.
     * Orig Ghidra save_pack (FUN_10f8_05da @ 10f8:05da). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_unpack(const uint8_t *image, size_t len)
{
    (void)image;
    (void)len;
    /* Legacy wrapper: real codec is save_rll_decode below.
     * Orig Ghidra save_unpack (FUN_10f8_040a @ 10f8:040a). */
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
    (void)image;
    (void)cap;
    /* TODO: ordered xfers per Ghidra save_marshal_fields (Orig FUN_10f8_090c).
     * Needs live seg 12d8/12d0/... — no Win16 memory on Linux yet. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_xfer_words(uint8_t *image, size_t cap, size_t *cursor,
                            uint16_t seg, uint16_t off, unsigned words)
{
    (void)image;
    (void)cap;
    (void)cursor;
    (void)seg;
    (void)off;
    (void)words;
    /* Orig save_xfer_words (FUN_10f8_0f40): count WORDS x2 bytes, dir by
     * g_nSaveDirection. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_xfer_block(uint8_t *image, size_t cap, size_t *cursor,
                            uint16_t seg, uint16_t off, unsigned len)
{
    (void)image;
    (void)cap;
    (void)cursor;
    (void)seg;
    (void)off;
    (void)len;
    /* Orig save_xfer_block (FUN_10f8_0fcf) via mem_copy_far. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_xfer_array(uint8_t *image, size_t cap, size_t *cursor,
                            uint16_t seg, uint16_t base, unsigned n,
                            unsigned elem_len)
{
    (void)image;
    (void)cap;
    (void)cursor;
    (void)seg;
    (void)base;
    (void)n;
    (void)elem_len;
    /* Orig save_xfer_array (FUN_10f8_1035), stride 0x20. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_xfer_strided(uint8_t *image, size_t cap, size_t *cursor,
                              uint16_t seg, uint16_t base, unsigned n)
{
    (void)image;
    (void)cap;
    (void)cursor;
    (void)seg;
    (void)base;
    (void)n;
    /* Orig save_xfer_strided (FUN_10f8_10bd): skip 0xc + 0xb words. */
    return CIV_ERR_UNSUPPORTED;
}
