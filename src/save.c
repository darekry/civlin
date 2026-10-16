/* CivLin SAVE/LOAD. See save.h for Orig FUN_* -> new name mapping.
 * RLL codec (save_rll_encode/save_rll_decode, Orig FUN_10f8_05da/040a cores)
 * is decoded and implemented natively below. W1: save_marshal_fields @
 * 10f8:090c is the ordered xfer driver over CIV_SAVE_OFF_* (local cursor
 * mirrors g_nSaveCursorOff @ 1420:a2d6 over base @ 1420:a2da; direction
 * g_nSaveDirection @ 1310:0008 and variant g_nSaveVariant @ 1310:0000 are
 * caller-documented, reservation is direction-symmetric); save_serialize @
 * 10f8:02bd and save_deserialize @ 10f8:0000 are partial ports on top of
 * it + RLL + POSIX save_file_* (live Win16 segments 12d8/... have no Linux
 * mapping yet, so the marshalled image is caller-owned zeros with a TODO;
 * no Win16, no globals, no exit()). xfer words/block/array/strided (@
 * 10f8:0f40/0fcf/1035/10bd) own only the OS-free image-cursor half
 * (bounds-checked reservation, no Win16, no globals). Packet L file I/O
 * (save_file_* @ 1140) is implemented below on POSIX fds — the HFILE-native
 * twin of the FILE* File_* wrappers (same Win16 imports, same
 * chunk/contract semantics). */
#define _POSIX_C_SOURCE 200809L
#include "save.h"

#include <fcntl.h>
#include <stdlib.h>
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

civ_error_t save_init_filespec(const char *src, char *dst, size_t cap,
                               size_t *out_len)
{
    /* Orig: FUN_1140_0095 @ 1140:0095 -> Save_InitFileSpec. OS-free 1:1
     * of the StrCopy @ 1000:36b2 + StrLen @ 1000:3722 pair (spec+8 path,
     * spec+6 len, returns 0). Linux on explicit buffers: copy + strlen.
     * NOENT on NULL src/dst, RANGE when cap == 0 or src (incl. NUL)
     * does not fit, else CIV_OK. No Win16, no globals, no exit(). */
    size_t n;

    if (out_len)
        *out_len = 0;
    if (!src || !dst)
        return CIV_ERR_NOENT;
    if (cap == 0)
        return CIV_ERR_RANGE;
    n = strlen(src) + 1u;
    if (n > cap)
        return CIV_ERR_RANGE;
    memcpy(dst, src, n);
    if (out_len)
        *out_len = n - 1u;
    return CIV_OK;
}

civ_error_t save_load_dialog(char *out_path, size_t cap)
{
    if (!out_path)
        return CIV_ERR_NOENT;
    if (cap > 0)
        out_path[0] = '\0';
    /* TODO: Orig FUN_1130_000e @ 1130:000e (Save_LoadDialog: modal load
     * via save_prompt_open @ 1130:0075 + save_deserialize @ 10f8:0000 +
     * Anim_PumpIfActive @ 10b0:0000 / Cursor_Set 0x84; caller
     * MainMenu_WndProc @ 11e0:0221 choice 1). Needs COMMDLG + live ctx. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_save_dialog(char *out_path, size_t cap)
{
    if (!out_path)
        return CIV_ERR_NOENT;
    if (cap > 0)
        out_path[0] = '\0';
    /* TODO: Orig FUN_1098_26d4 @ 1098:26d4 (Save_SaveDialog: modal save
     * wrapper over save_prompt_save @ 1130:0194; callers replay @
     * 1180:0000 + Save_SaveGame @ 1198:00a3). Needs COMMDLG. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_save_game(const char *path)
{
    if (!path)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_1198_00a3 @ 1198:00a3 (Save_SaveGame: "civname+year"
     * filename via Util_StrAppend @ 1000:363e + Util_IntToDec @
     * 1000:2928, prompt via Save_SaveDialog @ 1098:26d4, write via
     * save_serialize @ 10f8:02bd). Needs dialogs + live ctx. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t save_sanitize_filename(char *path)
{
    /* Orig: FUN_1198_0000 @ 1198:0000 -> Save_SanitizeFileName. OS-free
     * 1:1 of the keep-alnum filter (0x30-0x39 / 0x41-0x5a / 0x61-0x7a)
     * + copy-back: orig accumulates into a 16 B stack buffer then
     * Util_StrCopy @ 1000:36b2 over the input; Linux filters in place
     * (read cursor != write cursor), so overlong names stay safe.
     * NOENT on NULL (orig faulted). No Win16, no globals, no exit(). */
    size_t r = 0;
    size_t w = 0;

    if (!path)
        return CIV_ERR_NOENT;
    while (path[r] != '\0') {
        char c = path[r];

        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z'))
            path[w++] = c;
        r++;
    }
    path[w] = '\0';
    return CIV_OK;
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
     * "&Save Game" menu path). Partial port (W1):
     *   1. alloc flat image CIV_SAVE_BUF_SIZE (0x1a2ba; orig LocalAlloc,
     *      err string @ 1420:2cac "Could not allocate buffer for saving"
     *      -> CIV_ERR_NOMEM here);
     *   2. save_marshal_fields @ 10f8:090c with direction save
     *      (g_nSaveDirection @ 1310:0008 == 0) to validate/fill the image;
     *   3. save_pack @ 10f8:05da core via save_rll_encode into a heap
     *      RLL buffer (worst-case flat + flat/127 + 16);
     *   4. save_file_open @ 1140:0139 O_WRITE, save_file_write @ 1140:0245,
     *      save_file_close @ 1140:037c.
     * Live game ctx (segments 12d8/12d0/1248/1290/1298/1308/1350/1388/
     * 1390/13c0/13e8/1420) has no Linux mapping yet, so the image stays
     * caller-owned zeros (TODO: scatter live ctx via memcpy once mapped).
     * No globals, no exit(). */
    uint8_t *image = NULL;
    uint8_t *enc = NULL;
    size_t enc_cap = 0;
    size_t enc_len = 0;
    win16_handle h = WIN16_HFILE_ERROR;
    unsigned long written = 0;
    civ_error_t rc;

    if (!path || !*path)
        return CIV_ERR_NOENT;
    image = (uint8_t *)calloc(1, (size_t)CIV_SAVE_BUF_SIZE);
    if (!image)
        return CIV_ERR_NOMEM;
    rc = save_marshal_fields(image, (size_t)CIV_SAVE_BUF_SIZE);
    if (rc != CIV_OK) {
        free(image);
        return rc;
    }
    /* RLL worst case: every 0x7f literals cost one tag byte. */
    enc_cap = (size_t)CIV_SAVE_BUF_SIZE +
        (size_t)CIV_SAVE_BUF_SIZE / 127u + 16u;
    enc = (uint8_t *)malloc(enc_cap);
    if (!enc) {
        free(image);
        return CIV_ERR_NOMEM;
    }
    rc = save_rll_encode(image, (size_t)CIV_SAVE_BUF_SIZE, enc, enc_cap,
                         &enc_len);
    free(image);
    image = NULL;
    if (rc != CIV_OK) {
        free(enc);
        return rc;
    }
    h = save_file_open(path, 1);
    if (h == WIN16_HFILE_ERROR) {
        free(enc);
        return CIV_ERR_IO;
    }
    rc = CIV_OK;
    if (save_file_write(h, enc, (unsigned long)enc_len, &written) != 0 ||
        (size_t)written != enc_len)
        rc = CIV_ERR_IO;
    save_file_close(h);
    free(enc);
    return rc;
}

civ_error_t save_deserialize(const char *path)
{
    /* Save_Deserialize, Orig FUN_10f8_0000 @ 10f8:0000 (called from the
     * Load path + worldgen_build_ocean_links @ 1028:162c context). Partial
     * port (W1):
     *   1. GLOBALALLOC 0xa2ba (CIV_SAVE_SCRATCH) scratch equiv: heap wire
     *      buffer sized by save_file_size @ 1140:0295 (orig RLL window);
     *   2. save_file_open @ 1140:0139 O_READ + save_file_read @ 1140:019d
     *      (0x8000 chunks) to fetch the wire bytes;
     *   3. save_unpack @ 10f8:040a core via save_rll_decode into a flat
     *      CIV_SAVE_BUF_SIZE image;
     *   4. size gate on the decoded image: CIV_SAVE_BUF_SIZE (0x1a2ba)
     *      canonical / CIV_SAVE_BUF_ALT (0x1a2b8) cond-word alt /
     *      CIV_SAVE_BUF_MIN (0x18de0) small-map, else CIV_ERR_BAD_DATA
     *      (orig err @ 1420:2c74 "Could not load saved game. Corrupted or
     *      out of memory.");
     *   5. save_marshal_fields @ 10f8:090c with direction load
     *      (g_nSaveDirection @ 1310:0008 == 1) to validate/scatter;
     *   6. save_file_close @ 1140:037c.
     * Wire length varies with RLL compression, so the 0x1a2ba/0x1a2b8/
     * 0x18de0 gate applies to the decoded flat length, not the file size
     * (file size is only a read bound + empty/short check). Live scatter
     * into segments 12d8/... is TODO (image validated only). No globals,
     * no exit(). */
    win16_handle h = WIN16_HFILE_ERROR;
    long fsize = -1L;
    uint8_t *wire = NULL;
    uint8_t *flat = NULL;
    size_t flat_len = 0;
    unsigned long got = 0;
    civ_error_t rc;

    if (!path || !*path)
        return CIV_ERR_NOENT;
    h = save_file_open(path, 0);
    if (h == WIN16_HFILE_ERROR)
        return CIV_ERR_IO;
    fsize = save_file_size(h);
    if (fsize < 0) {
        save_file_close(h);
        return CIV_ERR_IO;
    }
    if (fsize == 0) {
        save_file_close(h);
        return CIV_ERR_TRUNCATED;
    }
    /* Upper bound: RLL worst case is flat + flat/127 + 16; anything far
     * larger cannot decode into CIV_SAVE_BUF_SIZE (corrupted). */
    if ((unsigned long)fsize >
        (unsigned long)CIV_SAVE_BUF_SIZE +
            (unsigned long)CIV_SAVE_BUF_SIZE / 127u + 4096u) {
        save_file_close(h);
        return CIV_ERR_BAD_DATA;
    }
    wire = (uint8_t *)malloc((size_t)fsize);
    if (!wire) {
        save_file_close(h);
        return CIV_ERR_NOMEM;
    }
    if (save_file_read(h, wire, (unsigned long)fsize, &got) != 0 ||
        got != (unsigned long)fsize) {
        save_file_close(h);
        free(wire);
        return CIV_ERR_TRUNCATED;
    }
    save_file_close(h);
    flat = (uint8_t *)malloc((size_t)CIV_SAVE_BUF_SIZE);
    if (!flat) {
        free(wire);
        return CIV_ERR_NOMEM;
    }
    rc = save_rll_decode(wire, (size_t)fsize, flat,
                         (size_t)CIV_SAVE_BUF_SIZE, &flat_len);
    free(wire);
    wire = NULL;
    if (rc != CIV_OK) {
        free(flat);
        return rc;
    }
    if (flat_len != (size_t)CIV_SAVE_BUF_SIZE &&
        flat_len != (size_t)CIV_SAVE_BUF_ALT &&
        flat_len != (size_t)CIV_SAVE_BUF_MIN) {
        free(flat);
        return CIV_ERR_BAD_DATA;
    }
    rc = save_marshal_fields(flat, (size_t)CIV_SAVE_BUF_SIZE);
    free(flat);
    return rc;
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

civ_error_t save_pack_buffer(const uint8_t *src, size_t src_len,
                             uint8_t *dst, size_t dst_cap, size_t *out_len)
{
    /* Orig FUN_10f8_05da @ 10f8:05da (save_pack RLL core, called from
     * save_serialize @ 10f8:02bd via Debug_LogTrace "RLLEncode ... load c"
     * + GLOBALALLOC wire sizing len+len/0x80+5). Native 1:1 delegate to
     * save_rll_encode (<0x80 repeat / 0x80|n literal, 0x7f chunk cap).
     * NOENT on NULL src/dst (out_len NULL skips the store, same as the
     * codec below). No Win16, no globals, no exit(). */
    if (!src || !dst)
        return CIV_ERR_NOENT;
    return save_rll_encode(src, src_len, dst, dst_cap, out_len);
}

civ_error_t save_unpack_buffer(const uint8_t *src, size_t src_len,
                               uint8_t *dst, size_t dst_cap,
                               size_t *out_len)
{
    /* Orig FUN_10f8_040a @ 10f8:040a (save_unpack RLL core, called from
     * save_deserialize @ 10f8:0000 via Debug_LogTrace "RLLDecode ... load
     * c"). Native 1:1 delegate to save_rll_decode. TRUNCATED on empty
     * wire (matches the legacy save_unpack gate), NOENT on NULL src/dst.
     * No Win16, no globals, no exit(). */
    if (!src || !dst)
        return CIV_ERR_NOENT;
    if (src_len == 0) {
        if (out_len)
            *out_len = 0;
        return CIV_ERR_TRUNCATED;
    }
    return save_rll_decode(src, src_len, dst, dst_cap, out_len);
}

civ_error_t save_build_filename(const char *civname, int year, char *out,
                                size_t cap)
{
    /* Orig Save_SaveGame @ 1198:00a3 filename head ("civname+year" via
     * Util_StrAppend @ 1000:363e + Util_IntToDec @ 1000:2928 + suffix
     * 0x3efe/0x3f00 by year sign, sanitized via Save_SanitizeFileName @
     * 1198:0000). OS-free 1:1 on explicit buffers: keep [0-9A-Za-z] from
     * civname, append abs(year) decimal, append "BC" (year < 0) else "AD"
     * (spaceless filename form; status lines use " BC"/" AD" @ 1420:173c /
     * 1420:1740 with a leading space, see game_append_year_text @
     * 1088:1dd0). NOENT on NULL civname/out, RANGE when cap == 0 or the
     * result (incl. NUL) does not fit. No Win16, no globals, no exit(). */
    size_t w = 0;
    size_t i = 0;
    long abs_year;
    char num[12];
    int nlen = 0;
    const char *suffix;

    if (!civname || !out)
        return CIV_ERR_NOENT;
    if (cap == 0)
        return CIV_ERR_RANGE;
    while (civname[i] != '\0') {
        char c = civname[i];

        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z')) {
            if (w + 1 >= cap)
                return CIV_ERR_RANGE;
            out[w++] = c;
        }
        i++;
    }
    abs_year = year < 0 ? -(long)year : (long)year;
    if (abs_year == 0) {
        num[0] = '0';
        nlen = 1;
    } else {
        char rev[12];
        int rn = 0;

        while (abs_year > 0) {
            rev[rn++] = (char)('0' + abs_year % 10);
            abs_year /= 10;
        }
        nlen = rn;
        for (int k = 0; k < rn; k++)
            num[k] = rev[rn - 1 - k];
    }
    suffix = year < 0 ? "BC" : "AD";
    if (w + (size_t)nlen + 2u + 1u > cap)
        return CIV_ERR_RANGE;
    for (int k = 0; k < nlen; k++)
        out[w++] = num[k];
    out[w++] = suffix[0];
    out[w++] = suffix[1];
    out[w] = '\0';
    return CIV_OK;
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

/* Local cursor seek for the marshal driver below. Mirrors the orig
 * g_nSaveCursorOff @ 1420:a2d6 over base @ 1420:a2da: the driver walks
 * CIV_SAVE_OFF_* in order, skipping forward over padding gaps (cursor < off
 * -> cursor = off). Backward motion would mean overlapping fields ->
 * BAD_DATA. Off beyond cap -> TRUNCATED. No globals, no Win16. */
static civ_error_t marshal_seek(size_t *cursor, size_t cap, size_t off)
{
    if (!cursor)
        return CIV_ERR_NOENT;
    if (off > cap)
        return CIV_ERR_TRUNCATED;
    if (*cursor > off)
        return CIV_ERR_BAD_DATA;
    *cursor = off;
    return CIV_OK;
}

civ_error_t save_marshal_fields(uint8_t *image, size_t cap)
{
    /* Save_MarshalFields, Orig FUN_10f8_090c @ 10f8:090c. Ordered xfer
     * driver over the CIV_SAVE_OFF_* layout (save.h, full-size path):
     * pre-word (live 1310:0000 g_nSaveVariant) + 8 words + 3 arrays
     * 8x0x20 + blks 0x4b0/0x4b0/0xe00 + strided 28x22B + blk 0x3000 + blk
     * 0xfa0 + array 0x100x0x20 + blk 0x1000 + blk 0x240 + blk 0x104 + blk
     * 0x5a0 + last word + map 0xfa00 at +0xa8ba (live 1420:b394/b396).
     * Direction switch is g_nSaveDirection @ 1310:0008 (0 = save
     * live->image, 1 = load image->live); reservation is
     * direction-symmetric so the same walk validates both. Small-map
     * variant (g_nSaveVariant @ 1310:0000) shrinks the 8x0x20 / 0x100x0x20
     * arrays -> TODO (this walk covers the full-size path; a small cap in
     * [MIN,FULL) TRUNCATEs at the first field that no longer fits).
     * Cursor mirrors g_nSaveCursorOff @ 1420:a2d6 over base @ 1420:a2da
     * (local here, no globals). Each step reuses the ported
     * save_xfer_words/block/array/strided (which own the xfer_reserve
     * bounds check). OS-free: image bytes are caller-owned (serialize
     * calloc-zeros them); no Win16 far-pointer dereference, no HMEMCPY —
     * the live seg:off anchors below are retained for the RE map and the
     * future bounds-checked memcpy once live ctx is mapped. */
    size_t cursor = 0;
    civ_error_t rc;

#define SAVE_MARSHAL_SEEK(off)                                                 \
    do {                                                                       \
        rc = marshal_seek(&cursor, cap, (size_t)(off));                         \
        if (rc != CIV_OK)                                                      \
            return rc;                                                         \
    } while (0)
#define SAVE_MARSHAL_CHECK(call)                                               \
    do {                                                                       \
        rc = (call);                                                            \
        if (rc != CIV_OK)                                                      \
            return rc;                                                         \
    } while (0)

    if (!image)
        return CIV_ERR_NOENT;
    if (cap < (size_t)CIV_SAVE_BUF_MIN)
        return CIV_ERR_TRUNCATED;

    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_PRE);
    SAVE_MARSHAL_CHECK(save_xfer_words(image, cap, &cursor, 0x1310u, 0x0000u, 1u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_F_12D8_42FE);
    SAVE_MARSHAL_CHECK(save_xfer_words(image, cap, &cursor, 0x12d8u, 0x42feu, 1u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_F_12D8_4300);
    SAVE_MARSHAL_CHECK(save_xfer_words(image, cap, &cursor, 0x12d8u, 0x4300u, 1u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_F_12D8_42FC);
    SAVE_MARSHAL_CHECK(save_xfer_words(image, cap, &cursor, 0x12d8u, 0x42fcu, 1u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_F_13C0_0000);
    SAVE_MARSHAL_CHECK(save_xfer_words(image, cap, &cursor, 0x13c0u, 0x0000u, 1u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_F_12D8_42FA);
    SAVE_MARSHAL_CHECK(save_xfer_words(image, cap, &cursor, 0x12d8u, 0x42fau, 1u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_F_12D8_42F6);
    SAVE_MARSHAL_CHECK(save_xfer_words(image, cap, &cursor, 0x12d8u, 0x42f6u, 1u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_F_12D8_42F8);
    SAVE_MARSHAL_CHECK(save_xfer_words(image, cap, &cursor, 0x12d8u, 0x42f8u, 1u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_F_12D0_0062);
    SAVE_MARSHAL_CHECK(save_xfer_words(image, cap, &cursor, 0x12d0u, 0x0062u, 1u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_A_12D8_01DA);
    SAVE_MARSHAL_CHECK(save_xfer_array(image, cap, &cursor, 0x12d8u, 0x01dau, 8u, 0x20u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_A_12D8_02DA);
    SAVE_MARSHAL_CHECK(save_xfer_array(image, cap, &cursor, 0x12d8u, 0x02dau, 8u, 0x20u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_A_12D8_03DA);
    SAVE_MARSHAL_CHECK(save_xfer_array(image, cap, &cursor, 0x12d8u, 0x03dau, 8u, 0x20u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_BLK_12D8_3200);
    SAVE_MARSHAL_CHECK(save_xfer_block(image, cap, &cursor, 0x12d8u, 0x3200u, 0x4b0u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_BLK_12D8_2D50);
    SAVE_MARSHAL_CHECK(save_xfer_block(image, cap, &cursor, 0x12d8u, 0x2d50u, 0x4b0u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_BLK_1420_54FC);
    SAVE_MARSHAL_CHECK(save_xfer_block(image, cap, &cursor, 0x1420u, 0x54fcu, 0xe00u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_STRIDED_1420_1A38);
    SAVE_MARSHAL_CHECK(save_xfer_strided(image, cap, &cursor, 0x1420u, 0x1a38u, 28u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_BLK_1420_62FC);
    SAVE_MARSHAL_CHECK(save_xfer_block(image, cap, &cursor, 0x1420u, 0x62fcu, 0x3000u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_BLK_13E8_000A);
    SAVE_MARSHAL_CHECK(save_xfer_block(image, cap, &cursor, 0x13e8u, 0x000au, 0xfa0u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_A_12D8_04DA);
    SAVE_MARSHAL_CHECK(save_xfer_array(image, cap, &cursor, 0x12d8u, 0x04dau, 0x100u, 0x20u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_BLK_1390_03AF);
    SAVE_MARSHAL_CHECK(save_xfer_block(image, cap, &cursor, 0x1390u, 0x03afu, 0x1000u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_BLK_12D8_36B0);
    SAVE_MARSHAL_CHECK(save_xfer_block(image, cap, &cursor, 0x12d8u, 0x36b0u, 0x240u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_BLK_1248_0004);
    SAVE_MARSHAL_CHECK(save_xfer_block(image, cap, &cursor, 0x1248u, 0x0004u, 0x104u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_BLK_1290_01B2);
    SAVE_MARSHAL_CHECK(save_xfer_block(image, cap, &cursor, 0x1290u, 0x01b2u, 0x5a0u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_OFF_F_1308_0020);
    SAVE_MARSHAL_CHECK(save_xfer_words(image, cap, &cursor, 0x1308u, 0x0020u, 1u));
    SAVE_MARSHAL_SEEK(CIV_SAVE_IMAGE_MAP_OFF);
    SAVE_MARSHAL_CHECK(save_xfer_block(image, cap, &cursor, 0x1420u, 0xb394u,
                                       CIV_SAVE_IMAGE_MAP_LEN));

#undef SAVE_MARSHAL_SEEK
#undef SAVE_MARSHAL_CHECK

    if (cursor != (size_t)CIV_SAVE_IMAGE_SIZE)
        return CIV_ERR_BAD_DATA;
    return CIV_OK;
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
     * seg/base are live anchors (base + i*(0xc+22) in orig). Live copy
     * lands in save_xfer_strided_live below (same anchors + ctx). */
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

/* --- X1 live ctx (explicit, no globals, OS-free, no Win16, no exit) --- */

void civ_save_ctx_init(civ_save_ctx_t *ctx, int direction)
{
    size_t k;

    if (!ctx)
        return;
    ctx->direction = direction ? CIV_SAVE_DIR_LOAD : CIV_SAVE_DIR_SAVE;
    ctx->nwins = 0;
    for (k = 0; k < (size_t)CIV_SAVE_CTX_MAX_SEGS; k++) {
        ctx->wins[k].seg = 0;
        ctx->wins[k].base = NULL;
        ctx->wins[k].len = 0;
    }
}

civ_error_t civ_save_ctx_add(civ_save_ctx_t *ctx, uint16_t seg,
                             uint8_t *base, size_t len)
{
    size_t k;

    if (!ctx || !base)
        return CIV_ERR_NOENT;
    if (len == 0)
        return CIV_ERR_RANGE;
    for (k = 0; k < ctx->nwins; k++) {
        if (ctx->wins[k].seg == seg) {
            ctx->wins[k].base = base;
            ctx->wins[k].len = len;
            return CIV_OK;
        }
    }
    if (ctx->nwins >= (size_t)CIV_SAVE_CTX_MAX_SEGS)
        return CIV_ERR_RANGE;
    ctx->wins[ctx->nwins].seg = seg;
    ctx->wins[ctx->nwins].base = base;
    ctx->wins[ctx->nwins].len = len;
    ctx->nwins++;
    return CIV_OK;
}

/* Find a mapped live window for seg (NULL when ctx NULL, unmapped, or
 * empty). Caller-owned; no globals. */
static const civ_save_seg_win_t *save_ctx_find(const civ_save_ctx_t *ctx,
                                                uint16_t seg)
{
    size_t k;

    if (!ctx)
        return NULL;
    for (k = 0; k < ctx->nwins; k++) {
        if (ctx->wins[k].seg == seg && ctx->wins[k].base != NULL &&
            ctx->wins[k].len > 0)
            return &ctx->wins[k];
    }
    return NULL;
}

/* Live-window bounds: off+len must sit inside win->len (overflow-safe).
 * RANGE on live overrun (caller buffer too small). */
static civ_error_t save_live_check(const civ_save_seg_win_t *win,
                                   uint16_t off, size_t len)
{
    size_t o;

    if (!win || !win->base)
        return CIV_ERR_NOENT;
    o = (size_t)off;
    if (len > win->len)
        return CIV_ERR_RANGE;
    if (o > win->len - len)
        return CIV_ERR_RANGE;
    return CIV_OK;
}

/* One contiguous live copy (words/block core): reserves len image bytes,
 * then directional memcpy between win->base+off and image+cursor_before.
 * ctx NULL / seg unmapped = reservation-only (zeros path preserved).
 * TRUNCATED leaves *cursor at the failing start (xfer_reserve shape). */
static civ_error_t save_live_copy_one(uint8_t *image, size_t cap,
                                       size_t *cursor, uint16_t seg,
                                       uint16_t off, size_t len,
                                       const civ_save_ctx_t *ctx)
{
    const civ_save_seg_win_t *win;
    size_t img_off;
    civ_error_t rc;

    if (!image || !cursor)
        return CIV_ERR_NOENT;
    win = save_ctx_find(ctx, seg);
    if (!win)
        return xfer_reserve(cursor, cap, len);
    rc = save_live_check(win, off, len);
    if (rc != CIV_OK)
        return rc;
    img_off = *cursor;
    rc = xfer_reserve(cursor, cap, len);
    if (rc != CIV_OK)
        return rc;
    if (len == 0)
        return CIV_OK;
    if (ctx->direction == CIV_SAVE_DIR_SAVE)
        memcpy(image + img_off, win->base + (size_t)off, len);
    else
        memcpy(win->base + (size_t)off, image + img_off, len);
    return CIV_OK;
}

civ_error_t save_xfer_words_live(uint8_t *image, size_t cap, size_t *cursor,
                                 uint16_t seg, uint16_t off, unsigned words,
                                 const civ_save_ctx_t *ctx)
{
    /* Orig FUN_10f8_0f40 @ 10f8:0f40 live half: words*2 bytes between
     * MK_FP(seg, off) and image+*cursor (direction by ctx->direction,
     * mirroring g_nSaveDirection @ 1310:0008). seg/off kept as anchors. */
    size_t len;

    if (!image || !cursor)
        return CIV_ERR_NOENT;
    if ((size_t)words > SIZE_MAX / 2u)
        return CIV_ERR_RANGE;
    len = (size_t)words * 2u;
    return save_live_copy_one(image, cap, cursor, seg, off, len, ctx);
}

civ_error_t save_xfer_block_live(uint8_t *image, size_t cap, size_t *cursor,
                                 uint16_t seg, uint16_t off, unsigned len,
                                 const civ_save_ctx_t *ctx)
{
    /* Orig FUN_10f8_0fcf @ 10f8:0fcf live half: len bytes via mem_copy_far.
     * Same contract as words_live, byte count. */
    if (!image || !cursor)
        return CIV_ERR_NOENT;
    return save_live_copy_one(image, cap, cursor, seg, off, (size_t)len,
                              ctx);
}

civ_error_t save_xfer_array_live(uint8_t *image, size_t cap, size_t *cursor,
                                 uint16_t seg, uint16_t base, unsigned n,
                                 unsigned elem_len,
                                 const civ_save_ctx_t *ctx)
{
    /* Orig FUN_10f8_1035 @ 10f8:1035 live half: n elems, live stride
     * CIV_SAVE_ARRAY_LIVE_STRIDE (0x20), elem_len image bytes each
     * (full 0x20; small-map 0x0e/0x0c/0x0b/0x0d prefix). Per-elem
     * reservation + directional copy; TRUNCATED (image) leaves *cursor at
     * the failing elem start, RANGE on live-window overrun. Unmapped seg
     * falls back to the reservation-only walk (same as save_xfer_array).
     * No Win16, no globals. */
    const civ_save_seg_win_t *win;
    unsigned i;

    if (!image || !cursor)
        return CIV_ERR_NOENT;
    if (n == 0 || elem_len == 0)
        return CIV_OK;
    if ((size_t)n > SIZE_MAX / (size_t)elem_len)
        return CIV_ERR_RANGE;
    if ((size_t)n > SIZE_MAX / (size_t)CIV_SAVE_ARRAY_LIVE_STRIDE)
        return CIV_ERR_RANGE;
    win = save_ctx_find(ctx, seg);
    if (!win) {
        for (i = 0; i < n; i++) {
            civ_error_t rc =
                xfer_reserve(cursor, cap, (size_t)elem_len);

            if (rc != CIV_OK)
                return rc;
        }
        return CIV_OK;
    }
    for (i = 0; i < n; i++) {
        size_t live_off;
        size_t img_off;
        civ_error_t rc;

        if ((size_t)i > (SIZE_MAX - (size_t)base) /
                (size_t)CIV_SAVE_ARRAY_LIVE_STRIDE)
            return CIV_ERR_RANGE;
        live_off = (size_t)base +
            (size_t)i * (size_t)CIV_SAVE_ARRAY_LIVE_STRIDE;
        /* Live elem must fit win (elem_len prefix of the 0x20 stride). */
        if (elem_len > win->len || live_off > win->len - (size_t)elem_len)
            return CIV_ERR_RANGE;
        img_off = *cursor;
        rc = xfer_reserve(cursor, cap, (size_t)elem_len);
        if (rc != CIV_OK)
            return rc;
        if (ctx->direction == CIV_SAVE_DIR_SAVE)
            memcpy(image + img_off, win->base + live_off, (size_t)elem_len);
        else
            memcpy(win->base + live_off, image + img_off, (size_t)elem_len);
    }
    return CIV_OK;
}

civ_error_t save_xfer_strided_live(uint8_t *image, size_t cap,
                                   size_t *cursor, uint16_t seg,
                                   uint16_t base, unsigned n,
                                   const civ_save_ctx_t *ctx)
{
    /* Orig FUN_10f8_10bd @ 10f8:10bd live half: n elems, each skips
     * CIV_SAVE_STRIDED_SKIP (0xc) live bytes then moves
     * CIV_SAVE_STRIDED_WORDS (0xb) words = CIV_SAVE_STRIDED_ELEM_LEN (22B)
     * packed per elem in the image (28 elems at
     * CIV_SAVE_OFF_STRIDED_1420_1A38). Live stride per elem is
     * CIV_SAVE_STRIDED_LIVE_STRIDE (0xc+22). Per-elem reservation +
     * directional copy; overrun leaves *cursor at the failing elem start.
     * Unmapped seg = reservation-only. seg/base anchors kept. */
    const civ_save_seg_win_t *win;
    unsigned i;

    if (!image || !cursor)
        return CIV_ERR_NOENT;
    if (n == 0)
        return CIV_OK;
    win = save_ctx_find(ctx, seg);
    if (!win) {
        for (i = 0; i < n; i++) {
            civ_error_t rc = xfer_reserve(
                cursor, cap, (size_t)CIV_SAVE_STRIDED_ELEM_LEN);

            if (rc != CIV_OK)
                return rc;
        }
        return CIV_OK;
    }
    for (i = 0; i < n; i++) {
        size_t live_off;
        size_t img_off;
        civ_error_t rc;

        if ((size_t)i > (SIZE_MAX - (size_t)base) /
                (size_t)CIV_SAVE_STRIDED_LIVE_STRIDE)
            return CIV_ERR_RANGE;
        live_off = (size_t)base +
            (size_t)i * (size_t)CIV_SAVE_STRIDED_LIVE_STRIDE;
        if ((size_t)CIV_SAVE_STRIDED_ELEM_LEN > win->len ||
            live_off > win->len - (size_t)CIV_SAVE_STRIDED_ELEM_LEN)
            return CIV_ERR_RANGE;
        img_off = *cursor;
        rc = xfer_reserve(cursor, cap, (size_t)CIV_SAVE_STRIDED_ELEM_LEN);
        if (rc != CIV_OK)
            return rc;
        if (ctx->direction == CIV_SAVE_DIR_SAVE)
            memcpy(image + img_off, win->base + live_off,
                   (size_t)CIV_SAVE_STRIDED_ELEM_LEN);
        else
            memcpy(win->base + live_off, image + img_off,
                   (size_t)CIV_SAVE_STRIDED_ELEM_LEN);
    }
    return CIV_OK;
}

civ_error_t save_marshal_fields_live(uint8_t *image, size_t cap,
                                      const civ_save_ctx_t *ctx)
{
    /* Save_MarshalFields live driver, Orig FUN_10f8_090c @ 10f8:090c:
     * same ordered CIV_SAVE_OFF_* walk as save_marshal_fields (full-size
     * path; small-map shrink TODO) but through the *_live xfers with ctx.
     * ctx NULL == reservation-only (identical to save_marshal_fields).
     * Direction from ctx->direction (SAVE live->image, else image->live);
     * unmapped segs (1298/1350/1388 frontier, caller holes, indirect map
     * when 1420 window is missing) stay reservation-only. Cursor mirrors
     * g_nSaveCursorOff @ 1420:a2d6 over base @ 1420:a2da (local, no
     * globals). OS-free, no Win16 far dereference. */
    size_t cursor = 0;
    civ_error_t rc;

#define SAVE_MARSHAL_LIVE_SEEK(off)                                             \
    do {                                                                       \
        rc = marshal_seek(&cursor, cap, (size_t)(off));                         \
        if (rc != CIV_OK)                                                      \
            return rc;                                                         \
    } while (0)
#define SAVE_MARSHAL_LIVE_CHECK(call)                                           \
    do {                                                                       \
        rc = (call);                                                            \
        if (rc != CIV_OK)                                                      \
            return rc;                                                         \
    } while (0)

    if (!image)
        return CIV_ERR_NOENT;
    if (cap < (size_t)CIV_SAVE_BUF_MIN)
        return CIV_ERR_TRUNCATED;

    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_PRE);
    SAVE_MARSHAL_LIVE_CHECK(
        save_xfer_words_live(image, cap, &cursor, 0x1310u, 0x0000u, 1u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_F_12D8_42FE);
    SAVE_MARSHAL_LIVE_CHECK(
        save_xfer_words_live(image, cap, &cursor, 0x12d8u, 0x42feu, 1u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_F_12D8_4300);
    SAVE_MARSHAL_LIVE_CHECK(
        save_xfer_words_live(image, cap, &cursor, 0x12d8u, 0x4300u, 1u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_F_12D8_42FC);
    SAVE_MARSHAL_LIVE_CHECK(
        save_xfer_words_live(image, cap, &cursor, 0x12d8u, 0x42fcu, 1u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_F_13C0_0000);
    SAVE_MARSHAL_LIVE_CHECK(
        save_xfer_words_live(image, cap, &cursor, 0x13c0u, 0x0000u, 1u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_F_12D8_42FA);
    SAVE_MARSHAL_LIVE_CHECK(
        save_xfer_words_live(image, cap, &cursor, 0x12d8u, 0x42fau, 1u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_F_12D8_42F6);
    SAVE_MARSHAL_LIVE_CHECK(
        save_xfer_words_live(image, cap, &cursor, 0x12d8u, 0x42f6u, 1u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_F_12D8_42F8);
    SAVE_MARSHAL_LIVE_CHECK(
        save_xfer_words_live(image, cap, &cursor, 0x12d8u, 0x42f8u, 1u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_F_12D0_0062);
    SAVE_MARSHAL_LIVE_CHECK(
        save_xfer_words_live(image, cap, &cursor, 0x12d0u, 0x0062u, 1u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_A_12D8_01DA);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_array_live(image, cap, &cursor, 0x12d8u,
                                                 0x01dau, 8u, 0x20u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_A_12D8_02DA);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_array_live(image, cap, &cursor, 0x12d8u,
                                                 0x02dau, 8u, 0x20u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_A_12D8_03DA);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_array_live(image, cap, &cursor, 0x12d8u,
                                                 0x03dau, 8u, 0x20u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_BLK_12D8_3200);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_block_live(image, cap, &cursor, 0x12d8u,
                                                 0x3200u, 0x4b0u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_BLK_12D8_2D50);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_block_live(image, cap, &cursor, 0x12d8u,
                                                 0x2d50u, 0x4b0u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_BLK_1420_54FC);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_block_live(image, cap, &cursor, 0x1420u,
                                                 0x54fcu, 0xe00u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_STRIDED_1420_1A38);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_strided_live(image, cap, &cursor,
                                                   0x1420u, 0x1a38u, 28u,
                                                   ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_BLK_1420_62FC);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_block_live(image, cap, &cursor, 0x1420u,
                                                 0x62fcu, 0x3000u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_BLK_13E8_000A);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_block_live(image, cap, &cursor, 0x13e8u,
                                                 0x000au, 0xfa0u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_A_12D8_04DA);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_array_live(image, cap, &cursor, 0x12d8u,
                                                 0x04dau, 0x100u, 0x20u,
                                                 ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_BLK_1390_03AF);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_block_live(image, cap, &cursor, 0x1390u,
                                                 0x03afu, 0x1000u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_BLK_12D8_36B0);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_block_live(image, cap, &cursor, 0x12d8u,
                                                 0x36b0u, 0x240u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_BLK_1248_0004);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_block_live(image, cap, &cursor, 0x1248u,
                                                 0x0004u, 0x104u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_BLK_1290_01B2);
    SAVE_MARSHAL_LIVE_CHECK(save_xfer_block_live(image, cap, &cursor, 0x1290u,
                                                 0x01b2u, 0x5a0u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_OFF_F_1308_0020);
    SAVE_MARSHAL_LIVE_CHECK(
        save_xfer_words_live(image, cap, &cursor, 0x1308u, 0x0020u, 1u, ctx));
    SAVE_MARSHAL_LIVE_SEEK(CIV_SAVE_IMAGE_MAP_OFF);
    SAVE_MARSHAL_LIVE_CHECK(
        save_xfer_block_live(image, cap, &cursor, 0x1420u, 0xb394u,
                             CIV_SAVE_IMAGE_MAP_LEN, ctx));

#undef SAVE_MARSHAL_LIVE_SEEK
#undef SAVE_MARSHAL_LIVE_CHECK

    if (cursor != (size_t)CIV_SAVE_IMAGE_SIZE)
        return CIV_ERR_BAD_DATA;
    return CIV_OK;
}
