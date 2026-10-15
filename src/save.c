/* CivLin SAVE/LOAD. See save.h for Orig FUN_* -> new name mapping.
 * RLL codec (save_pack/save_unpack, Orig FUN_10f8_05da/040a) is decoded
 * and implemented natively below. Marshal/xfer need live Win16 segments,
 * so they stay UNSUPPORTED stubs. No I/O happens yet. */
#include "save.h"

#include <string.h>

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
    (void)path;
    (void)write_mode;
    /* TODO: map to fopen(); semantics from Ghidra save_file_open
     * (Orig FUN_1140_0139 @ 1140:0139). */
    return WIN16_HFILE_ERROR;
}

long save_file_size(win16_handle h)
{
    (void)h;
    /* TODO: fseek/ftell; semantics from Ghidra save_file_size
     * (Orig FUN_1140_0295 @ 1140:0295). */
    return -1L;
}

int save_file_read(win16_handle h, uint8_t *dst, unsigned long len,
                   unsigned long *out_read)
{
    (void)h;
    (void)dst;
    (void)len;
    if (out_read)
        *out_read = 0;
    /* TODO: chunked 0x8000 reads; see Ghidra save_file_read
     * (Orig FUN_1140_019d @ 1140:019d). */
    return 1;
}

int save_file_write(win16_handle h, const uint8_t *src, unsigned long len,
                    unsigned long *out_written)
{
    (void)h;
    (void)src;
    (void)len;
    if (out_written)
        *out_written = 0;
    /* TODO: chunked writes; see Ghidra save_file_write
     * (Orig FUN_1140_0245 @ 1140:0245). */
    return 1;
}

void save_file_close(win16_handle h)
{
    (void)h;
    /* TODO: fclose(); see Ghidra save_file_close
     * (Orig FUN_1140_037c @ 1140:037c). */
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
