/* DOS / system helpers + RNG. 1:1 ports where the logic is OS-free,
 * documented adaptations where DOS/Win16 has no Linux equivalent. */
#include "sys.h"

#include "win16.h"

#include <errno.h>
#include <math.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <SDL3/SDL.h>

/* Scratch for getcwd() (replaces PATH_MAX, hidden under -std=c17). */
#define SYS_TMP_PATH 4096

/* RNG state: 1420:4ee0 (lo) + 1420:4ee2 (hi). */
static unsigned long s_rng_state;
/* g_nCurrentDrive @ 1228:0002; g_szBasePath @ 1228:0004. */
static int s_drive;
static char s_base_path[256];

void rng_seed(unsigned seed)
{
    /* Rng_Seed @ 1000:1716: DAT_1420_4ee2 = 0; DAT_1420_4ee0 = param. */
    s_rng_state = (unsigned long)(seed & 0xFFFFu);
}

unsigned rng_next_raw(void)
{
    /* Rng_NextRaw @ 1000:1729: seed = seed * 0x015A4E35 + 1 (via U32_Mul32
     * @ 1000:16fe, low 32 bits); return (seed >> 16) & 0x7FFF. */
    s_rng_state = s_rng_state * 0x015A4E35u + 1u;
    return (unsigned)((s_rng_state >> 16) & 0x7FFFu);
}

unsigned rng_range(int n)
{
    /* Rng_Range @ 11c8:003d: (rand15 * n) >> 15. Guard for n <= 0 is a
     * Linux-side addition (orig callers always pass n > 0). */
    if (n <= 0)
        return 0;
    return (rng_next_raw() * (unsigned)n) >> 15;
}

void rng_init(void)
{
    /* Rng_Init @ 11c8:0000: seed = Sys_GetTickScaled() & 0x7FFF, forced
     * nonzero, then Rng_Seed. */
    unsigned seed = (unsigned)(sys_get_tick_scaled() & 0x7FFFu);
    if (seed == 0)
        seed = 1;
    rng_seed(seed);
}

int util_clamp(int v, int lo, int hi)
{
    /* Util_Clamp @ 11c8:0063. */
    if (v < lo)
        v = lo;
    if (hi < v)
        v = hi;
    return v;
}

int util_progress(int done, int total)
{
    /* Generic progress percent (see sys.h: the seg-1000 memset wrapper
     * is Util_MemSet, not this helper). Continent-link progress-bar UI
     * scales the counter to the 0x1248 bar range; Linux reports 0..100
     * percent. No Win16, no globals, no exit(), read-only. */
    long pct;

    if (total <= 0 || done <= 0)
        return 0;
    if (done >= total)
        return 100;
    pct = (long)done * 100L / (long)total;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    return (int)pct;
}

int debug_print(const char *fmt, ...)
{
    /* Orig: FUN_1000_2c9c @ 1000:2c9c -> Debug_Print. OS-free 1:1 port of
     * the fprintf path behind ResMgr_DumpStats @ 1190:17d0: vfprintf(fmt)
     * to stdout. No globals, no exit(), never writes game assets. */
    va_list ap;
    int n;

    if (!fmt)
        return -1;
    va_start(ap, fmt);
    n = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return n;
}

int debug_print_line(const char *s)
{
    /* Orig: FUN_1000_3088 @ 1000:3088 -> Debug_PrintLine. OS-free 1:1 of
     * the puts-to-1420:4cd0 path (Util_StrLen @ 1000:3722 + write via
     * File_WriteBuffered @ 1000:2e85 + '\n' via File_PutCharBuffered @
     * 1000:2cd8): NULL seg:off returns 0, short write or newline failure
     * returns -1 (orig 0xffff), else 10. No globals, no exit(). */
    if (!s)
        return 0;
    if (fputs(s, stdout) == EOF)
        return -1;
    if (fputc('\n', stdout) == EOF)
        return -1;
    return 10;
}

civ_error_t file_write_buffered(const uint8_t *src, size_t len)
{
    /* Orig FUN_1000_2e85 @ 1000:2e85 (File_WriteBuffered: Borland buffered
     * fwrite with 0x4e4e fd flags, 1420:4cd0 FILE struct, DOS write via
     * Dos_Write @ 1000:3fc4 INT 21h, overflow via File_EmitChar @ 1000:2cba).
     * Linux LIVE via buffered stdio to stdout (fwrite; libc buffering is
     * the portable twin of the 1420:4cd0 stream, flag dispatch lives in
     * file_buffered_is_direct). No globals beyond stdout, no exit(). */
    size_t n;

    if (len == 0)
        return CIV_OK;
    if (!src)
        return CIV_ERR_NOENT;
    n = fwrite(src, 1, len, stdout);
    if (n != len)
        return CIV_ERR_IO;
    return CIV_OK;
}

civ_error_t file_put_char_buffered(int c)
{
    /* Orig FUN_1000_2cd8 @ 1000:2cd8 (File_PutCharBuffered: flag 8/0x90/
     * 0x200 dispatch, flush via File_Flush @ 1000:2152, DOS via Dos_Write
     * @ 1000:3fc4). Linux LIVE via buffered stdio to stdout (fputc; flag
     * dispatch in file_buffered_is_direct / file_is_text_mode, flush is
     * libc buffering). No globals beyond stdout, no exit(). */
    if (fputc(c, stdout) == EOF)
        return CIV_ERR_IO;
    return CIV_OK;
}

civ_error_t file_emit_char(char c)
{
    /* Orig FUN_1000_2cba @ 1000:2cba (File_EmitChar: count -1 then
     * File_PutCharBuffered @ 1000:2cd8; overflow path of File_WriteBuffered
     * @ 1000:2e85). Linux LIVE thin delegate to file_put_char_buffered
     * (same single-byte overflow shape, buffered stdio). */
    return file_put_char_buffered((int)(unsigned char)c);
}

int file_buffered_is_direct(unsigned flags)
{
    /* Orig File_WriteBuffered @ 1000:2e85 / File_PutCharBuffered @
     * 1000:2cd8 flag dispatch (flag 8 = direct path, else buffered via
     * the 1420:4cd0 stream; verified against the unpacked/civ.exe
     * decompiler comments in sys.h). */
    return ((flags & 8u) != 0u) ? 1 : 0;
}

int file_is_text_mode(unsigned flags)
{
    /* Orig File_WriteText @ 1000:3e70 / File_ReadBuffered @ 1000:33c0
     * text gate (0x4000 bit in the 0x4e4e-table word enables LF->CRLF /
     * CRLF handling; same bit tested in Dos_Read @ 1000:1750 / Dos_Write
     * @ 1000:3fc4 comments). */
    return ((flags & 0x4000u) != 0u) ? 1 : 0;
}

/* --- DOS-file primitives (INT 21h effect port via stdio/POSIX) --- */

civ_error_t dos_map_error(int dos_code)
{
    /* Orig FUN_1000_151a @ 1000:151a (Dos_MapError: errno mapper to
     * DAT_1420_0010/DAT_1420_4e7a, always returns 0xffff). Linux LIVE pure
     * mapper via errno (no DGROUP writes, explicit param only): 0 -> CIV_OK,
     * ENOENT -> NOENT, ENOMEM -> NOMEM, EINVAL/EBADF -> RANGE, else IO.
     * DOS codes are folded through dos_map_error_lookup @ 1000:151a so the
     * table @ 1420:4e7c still decides DOS-specific values. No globals. */
    int e0010 = 0;
    int e4e7a = 0;

    if (dos_code == 0)
        return CIV_OK;
    if (dos_code == ENOENT)
        return CIV_ERR_NOENT;
    if (dos_code == ENOMEM)
        return CIV_ERR_NOMEM;
    if (dos_code == EINVAL || dos_code == EBADF)
        return CIV_ERR_RANGE;
    (void)dos_map_error_lookup(dos_code, &e0010, &e4e7a);
    if (e0010 == 0)
        return CIV_OK;
    if (e0010 == ENOENT)
        return CIV_ERR_NOENT;
    if (e0010 == ENOMEM)
        return CIV_ERR_NOMEM;
    if (e0010 == EINVAL || e0010 == EBADF)
        return CIV_ERR_RANGE;
    return CIV_ERR_IO;
}

int dos_map_error_lookup(int dos_code, int *out_e0010, int *out_e4e7a)
{
    /* Orig Dos_MapError @ 1000:151a (decompiled 2026-10-16; table bytes
     * @ 1420:4e7c read from civ.exe, 0x59 entries, all < 0x80 so the
     * orig (int)*(signed char *) extend is the identity). Note the
     * <= -0x31 fallthrough also clamps to 0x57: for negative input the
     * outer else-if is skipped, so control reaches `param_1 = 0x57`. */
    static const int8_t table[0x59] = {
        0, 19, 2, 2, 4, 5, 6, 8, 8, 8, 20, 21, 5, 19, 14, 22,
        5, 17, 2, 30, 41, 44, 40, 40, 40, 40, 40, 41, 44, 40,
        40, 40, 5, 5, 41, 23, 23, 14, 14, 14, 14, 14, 14, 14,
        14, 14, 14, 14, 14, 14, 15, 44, 35, 2, 44, 15, 42, 40,
        40, 40, 19, 27, 28, 2, 2, 5, 15, 2, 23, 40, 42, 19,
        42, 14, 14, 14, 14, 14, 14, 14, 35, 14, 28, 40, 23, 35,
        37, 19, 40,
    };
    int idx;

    _Static_assert(sizeof(table) == 0x59, "dos map table is 0x59 bytes");
    if (!out_e0010 || !out_e4e7a)
        return -1;
    if (dos_code < 0 &&
        (dos_code == -0x30 || -(long)dos_code < 0x30L)) {
        *out_e0010 = -dos_code;
        *out_e4e7a = 0xFFFF;
        return 0xFFFF;
    }
    idx = dos_code;
    if (idx < 0 || idx >= 0x59)
        idx = 0x57;
    *out_e4e7a = idx;
    *out_e0010 = (int)table[idx];
    return 0xFFFF;
}

civ_error_t dos_is_char_device(int fd)
{
    (void)fd;
    /* TODO: Orig FUN_1000_1566 @ 1000:1566 (Dos_IsCharDevice: INT 21h
     * AX=0x4400, BX=fd -> DX & 0x80). No DOS ioctls on Linux.
     * Live twin: isatty() one level up (not at this layer).
     * Pure bit kernel: dos_is_char_device_bit below (LIVE). */
    return CIV_ERR_UNSUPPORTED;
}

int dos_is_char_device_bit(unsigned dx)
{
    /* Orig Dos_IsCharDevice @ 1000:1566 tail (MOV AX,DX / AND AX,0x80):
     * 1 = char device (console), 0 = block file. Routes the Dos_Read @
     * 1000:1750 / Dos_Write @ 1000:3fc4 hook vectors (1420:4fc4/4fc8). */
    return ((dx & 0x80u) != 0u) ? 1 : 0;
}

civ_error_t dos_seek(int fd, long offset, int origin)
{
    /* Orig FUN_1000_161c @ 1000:161c (Dos_Seek: INT 21h AH=0x42, AL=origin,
     * CX:DX=offset; live twin file_seek_wrapper in save.h). Linux LIVE via
     * lseek (fseek twin; DOS 0/1/2 == SEEK_SET/SEEK_CUR/SEEK_END, the
     * 0x4e4e-table bit-0x200 clear has no Linux equivalent). No globals. */
    off_t r;

    if (fd < 0)
        return CIV_ERR_RANGE;
    if (!dos_seek_origin_ok(origin))
        return CIV_ERR_RANGE;
    r = lseek(fd, (off_t)offset, origin);
    if (r == (off_t)-1)
        return dos_map_error(errno);
    return CIV_OK;
}

int dos_seek_origin_ok(int origin)
{
    /* Orig Dos_Seek @ 1000:161c AL contract (INT 21h AH=0x42: 0 SET /
     * 1 CUR / 2 END from [BP+0x0c]; clears 0x4e4e-table bit 0x200 first;
     * other AL values fail via Dos_MapError @ 1000:151a). */
    return (origin >= 0 && origin <= 2) ? 1 : 0;
}

civ_error_t dos_read(int fd, void *buf, unsigned len)
{
    /* Orig FUN_1000_1750 @ 1000:1750 (Dos_Read: INT 21h AH=0x3f; live twin
     * save_file_read @ 1140:019d chunked 0x8000). Linux LIVE via read
     * (fread twin; 0x8000 chunk shape like save_file_read, EINTR retried;
     * 0x4e4e-table bit-1 guard via dos_read_is_blocked has no table on
     * Linux and is skipped, hook 1420:4fc4 + text-mode 0x4000 live one
     * level up). No globals. */
    uint8_t *dst;
    unsigned total = 0;

    if (!buf)
        return CIV_ERR_NOENT;
    if (len == 0)
        return CIV_OK;
    if (fd < 0)
        return CIV_ERR_RANGE;
    dst = (uint8_t *)buf;
    while (total < len) {
        unsigned left = len - total;
        unsigned want = left > 0x8000u ? 0x8000u : left;
        ssize_t n = read(fd, dst + total, (size_t)want);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return dos_map_error(errno);
        }
        if (n == 0)
            break;
        total += (unsigned)n;
        if ((unsigned)n < want)
            break;
    }
    if (total == len)
        return CIV_OK;
    return CIV_ERR_TRUNCATED;
}

int dos_read_is_blocked(unsigned flags)
{
    /* Orig Dos_Read @ 1000:1750 guard head (0x4e4e-table word bit 1 set
     * -> errno 5 via Dos_MapError @ 1000:151a, before the 1420:4fc4 hook
     * + Dos_IsCharDevice @ 1000:1566 dispatch). */
    return ((flags & 2u) != 0u) ? 1 : 0;
}

civ_error_t dos_unlink(const char *path)
{
    /* Orig FUN_1000_19a6 @ 1000:19a6 (Dos_Unlink: INT 21h AH=0x41,
     * DS:DX=path). Linux LIVE via remove (unlink twin). No globals. */
    if (!path)
        return CIV_ERR_NOENT;
    if (*path == '\0')
        return CIV_ERR_NOENT;
    if (remove(path) == 0)
        return CIV_OK;
    return dos_map_error(errno);
}

civ_error_t dos_file_attr(const char *path, int func, int attrs)
{
    /* Orig FUN_1000_1f5c @ 1000:1f5c (Dos_FileAttr: INT 21h AH=0x43,
     * AL=func, CX=attrs). Linux LIVE via stat (get) / chmod (set): func
     * validated via dos_file_attr_func_ok @ 1000:1f5c; get returns CIV_OK
     * when stat succeeds (real query, no mutation); set maps DOS 0x01
     * read-only to owner-write clear/set preserving other mode bits
     * (File_Open @ 1000:2ad9 uses func 0 probe + func 1 arg 1 restore).
     * No globals. */
    struct stat st;

    if (!path)
        return CIV_ERR_NOENT;
    if (*path == '\0')
        return CIV_ERR_NOENT;
    if (!dos_file_attr_func_ok(func))
        return CIV_ERR_RANGE;
    if (func == 0) {
        if (stat(path, &st) == 0)
            return CIV_OK;
        return dos_map_error(errno);
    }
    if (stat(path, &st) != 0)
        return dos_map_error(errno);
    if ((attrs & 1) != 0)
        st.st_mode &= (mode_t) ~(S_IWUSR | S_IWGRP | S_IWOTH);
    else
        st.st_mode |= (mode_t)S_IWUSR;
    if (chmod(path, st.st_mode) == 0)
        return CIV_OK;
    return dos_map_error(errno);
}

int dos_file_attr_func_ok(int func)
{
    /* Orig Dos_FileAttr @ 1000:1f5c AL contract (0 = get attrs -> CX,
     * 1 = set attrs from CX; File_Open @ 1000:2ad9 probes with 0 and
     * restores read-only with 1). */
    return (func == 0 || func == 1) ? 1 : 0;
}

civ_error_t dos_close(int fd)
{
    /* Orig FUN_1000_1fa6 @ 1000:1fa6 (Dos_Close: INT 21h AH=0x3e +
     * 0x4e4e-table clear; live twin save_file_close in save.h). Linux
     * LIVE via close (fclose twin; no table clear on Linux). No globals. */
    if (fd < 0)
        return CIV_ERR_RANGE;
    if (close(fd) == 0)
        return CIV_OK;
    return dos_map_error(errno);
}

civ_error_t dos_is_eof(int fd, int *out_eof)
{
    /* Orig FUN_1000_201e @ 1000:201e (Dos_IsEof: 4400-device gate +
     * 4201/4202/4200 tell/end/restore probe). Linux LIVE via the same
     * lseek triple (fseek/ftell twin file_seek_wrapper; table/high-water/
     * char-device gates skipped, tail compare via dos_eof_at_end @
     * 1000:201e). No globals. */
    off_t cur;
    off_t end;

    if (!out_eof)
        return CIV_ERR_NOENT;
    *out_eof = 0;
    if (fd < 0)
        return CIV_ERR_RANGE;
    cur = lseek(fd, (off_t)0, SEEK_CUR);
    if (cur == (off_t)-1)
        return dos_map_error(errno);
    end = lseek(fd, (off_t)0, SEEK_END);
    if (end == (off_t)-1)
        return dos_map_error(errno);
    if (lseek(fd, cur, SEEK_SET) == (off_t)-1)
        return dos_map_error(errno);
    *out_eof = dos_eof_at_end((long)cur, (long)end);
    return CIV_OK;
}

int dos_eof_at_end(long cur, long end)
{
    /* Orig Dos_IsEof @ 1000:201e tail (decompiled 2026-10-16:
     * `cur_restored < end ? 0 : 1` after the 4200 restore). */
    return (end <= cur) ? 1 : 0;
}

civ_error_t dos_create(const char *path, int attrs)
{
    if (!path)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_1000_2aac @ 1000:2aac (Dos_Create: INT 21h AH=0x3c,
     * CX=attrs; live twin save_file_open O_CREAT in save.c). */
    (void)attrs;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t dos_truncate(int fd)
{
    (void)fd;
    /* TODO: Orig FUN_1000_2ac7 @ 1000:2ac7 (Dos_Truncate: INT 21h AH=0x40
     * CX=0 zero-write at cursor; sole caller File_Open @ 1000:2ad9). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t dos_open(const char *path, int mode_flags)
{
    /* Orig FUN_1000_2c4a @ 1000:2c4a (Dos_Open: INT 21h AH=0x3d, AL=mode
     * from flags; live twin save_file_open O_RDONLY in save.h). Linux
     * LIVE existence/readability probe via fopen (no create/truncate like
     * the orig; mode via dos_open_mode_map @ 1000:2c4a: base 0 -> "rb",
     * 1/2 -> "r+b"; table word via dos_open_table_word for RE-map
     * fidelity, no table write on Linux). Opens then closes immediately:
     * CIV_OK when open succeeded, else dos_map_error(errno). No globals. */
    int m;
    const char *fmode;
    FILE *fp;

    if (!path)
        return CIV_ERR_NOENT;
    if (*path == '\0')
        return CIV_ERR_NOENT;
    m = dos_open_mode_map(mode_flags);
    (void)dos_open_table_word(mode_flags);
    fmode = ((m & 3) == 0) ? "rb" : "r+b";
    fp = fopen(path, fmode);
    if (!fp)
        return dos_map_error(errno);
    (void)fclose(fp);
    return CIV_OK;
}

int dos_open_mode_map(int flags)
{
    /* Orig Dos_Open @ 1000:2c4a (disassembled 2026-10-16: AL=1;
     * TEST CX,2/JNZ take; AL=2; TEST CX,4/JNZ take; AL=0; then
     * AL |= flags_low & 0xf0 for INT 21h AH=0x3d). Bit 1 wins. */
    int base;

    if ((flags & 0x2) != 0)
        base = 1;
    else if ((flags & 0x4) != 0)
        base = 2;
    else
        base = 0;
    return base | ((unsigned)flags & 0xF0u);
}

unsigned dos_open_table_word(int flags)
{
    /* Orig Dos_Open @ 1000:2c4a table site (MOV AX,[BP+0xa] /
     * AND AX,0xb8ff / OR AX,0x8000 into the 0x4e4e-table word). */
    return ((unsigned)flags & 0xB8FFu) | 0x8000u;
}

civ_error_t dos_write(int fd, const void *buf, unsigned len)
{
    /* Orig FUN_1000_3fc4 @ 1000:3fc4 (Dos_Write: INT 21h AH=0x40; live
     * twin save_file_write @ 1140:0245 chunked 0x8000). Linux LIVE via
     * write (fwrite twin; 0x8000 chunk shape like save_file_write, EINTR
     * retried; 0x4e4e-table bit-0 guard via dos_write_is_blocked has no
     * table on Linux and is skipped, hook 1420:4fc8 + text-mode 0x4000
     * live one level up). No globals. */
    const uint8_t *src;
    unsigned total = 0;

    if (!buf)
        return CIV_ERR_NOENT;
    if (len == 0)
        return CIV_OK;
    if (fd < 0)
        return CIV_ERR_RANGE;
    src = (const uint8_t *)buf;
    while (total < len) {
        unsigned left = len - total;
        unsigned want = left > 0x8000u ? 0x8000u : left;
        ssize_t n = write(fd, src + total, (size_t)want);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return dos_map_error(errno);
        }
        if (n == 0)
            break;
        total += (unsigned)n;
        if ((unsigned)n < want)
            break;
    }
    if (total == len)
        return CIV_OK;
    return CIV_ERR_TRUNCATED;
}

int dos_write_is_blocked(unsigned flags)
{
    /* Orig Dos_Write @ 1000:3fc4 guard head (0x4e4e-table word bit 0 set
     * -> errno 5 via Dos_MapError @ 1000:151a, before the 1420:4fc8 hook
     * + Dos_IsCharDevice @ 1000:1566 dispatch; success sets bit 0x1000). */
    return ((flags & 1u) != 0u) ? 1 : 0;
}

civ_error_t dos_ioctl(int fd, int func)
{
    /* Orig FUN_1000_4252 @ 1000:4252 (Dos_Ioctl: INT 21h AH=0x44
     * subfunction dispatch with verr() gate). Linux PARTIAL via
     * isatty/fstat console/device query (no verr/DGROUP on Linux):
     * non-direct funcs keep CIV_ERR_UNSUPPORTED (no real effect, Fala H);
     * direct funcs (0/6/7/8/9/10/0xe/0xf via dos_ioctl_is_direct @
     * 1000:4252) validate fd via fstat (func 0 additionally queries
     * isatty) and return CIV_OK on a successful query. No globals. */
    struct stat st;

    if (fd < 0)
        return CIV_ERR_RANGE;
    if (!dos_ioctl_is_direct(func))
        return CIV_ERR_UNSUPPORTED;
    if (func == 0)
        (void)isatty(fd);
    if (fstat(fd, &st) != 0)
        return dos_map_error(errno);
    return CIV_OK;
}

int dos_ioctl_is_direct(int func)
{
    /* Orig Dos_Ioctl @ 1000:4252 dispatch head (direct INT 21h AH=0x44
     * funcs pass straight through; others require DAT_1420_0012 != 0 +
     * verr() or fail errno 0x13 via Dos_MapError @ 1000:151a; func 0
     * returns the high word DX:AX >> 16). */
    switch (func) {
    case 0:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 0x0E:
    case 0x0F:
        return 1;
    default:
        return 0;
    }
}

int dos_get_current_drive(void)
{
    /* Dos_GetCurrentDrive @ 1000:1252 (INT 21h AH=19h + 1). No drive
     * letters on Linux -> 0, so `0 < drive` guards skip DOS-only branches
     * (Sound_PlayId, Config_SavePrefs). */
    return 0;
}

int dos_get_current_drive_index(void)
{
    /* Dos_GetCurrentDriveIndex @ 1000:13e6: Dos_GetCurrentDrive() - 1. */
    return dos_get_current_drive() - 1;
}

int dos_get_current_dir(int drive, char *buf, size_t cap)
{
    /* Dos_GetCurrentDir @ 1000:139c (INT 21h AH=47h). Drive ignored on
     * Linux. Orig returns the path WITHOUT drive/leading backslash, so
     * the leading '/' is stripped to match. */
    char tmp[SYS_TMP_PATH];
    const char *rel;
    size_t n;

    (void)drive;
    if (!buf || cap == 0)
        return -1;
    if (!getcwd(tmp, sizeof(tmp)))
        return -1;
    rel = tmp[0] == '/' ? tmp + 1 : tmp;
    n = strlen(rel) + 1;
    if (n > cap)
        return -1;
    memcpy(buf, rel, n);
    return 0;
}

unsigned long sys_tick_scaled_from(unsigned long tick)
{
    /* Orig Sys_GetTickScaled @ 1100:02e1 pure half (GETTICKCOUNT * 0x3c /
     * 1000, decompiled this pass). 1:1 on the explicit tick. */
    return tick * 60u / 1000u;
}

unsigned long sys_get_tick_scaled(void)
{
    /* Sys_GetTickScaled @ 1100:02e1: GETTICKCOUNT() * 0x3C / 1000. */
    return sys_tick_scaled_from((unsigned long)SDL_GetTicks());
}

char *sys_build_current_dir_path(char *dst, size_t cap)
{
    /* Sys_BuildCurrentDirPath @ 1000:2826. Orig composes "D:\curdir" and
     * fails NULL when the length exceeds max or the buffer must be
     * allocated and allocation fails. Linux: getcwd() full path, same
     * contract (dst on success, NULL when too long / getcwd fails). */
    char tmp[SYS_TMP_PATH];
    size_t n;

    if (!dst || cap == 0)
        return NULL;
    if (!getcwd(tmp, sizeof(tmp)))
        return NULL;
    n = strlen(tmp) + 1;
    if (n > cap)
        return NULL;
    memcpy(dst, tmp, n);
    return dst;
}

civ_error_t sys_init_paths(void)
{
    /* WinMain prologue: Sys_GetCurrentDrive -> 1228:0002,
     * Sys_BuildCurrentDirPath -> 1228:0004. */
    s_drive = dos_get_current_drive();
    if (!sys_build_current_dir_path(s_base_path, sizeof(s_base_path)))
        return CIV_ERR_IO;
    return CIV_OK;
}

int sys_current_drive(void)
{
    return s_drive;
}

const char *sys_base_path(void)
{
    return s_base_path;
}

void sys_yield(void)
{
    /* Sys_Yield @ 1098:0157 (wraps Anim_PumpIfActive @ 10b0:0000).
     * SDL3 message-queue pump during long operations. */
    SDL_PumpEvents();
}

void sys_delay_ms(unsigned ms)
{
    /* Linux-port helper (see sys.h): thin SDL_Delay delegate behind the
     * Sys_GetTickScaled @ 1100:02e1 poll loops (title.c / main.c) and the
     * future Anim_PumpIfActive @ 10b0:0000 SDL-timer tail. Exact Win16
     * Sleep quantum TBD (TODO Orig Sys_Yield @ 1098:0157 / TIMERPROC @
     * 1008:0d68); the 1 ms cooperative backoff matches the polled loops
     * 1:1 (no busy spin). No flag touch (1420:1f40 stays caller-side). */
    SDL_Delay((Uint32)ms);
}

civ_error_t sys_wait_next_tick(unsigned long *last_tick, int *advanced_out)
{
    /* P0a SDL-timer bridge for Anim_PumpIfActive @ 10b0:0000 (see sys.h):
     * combined Sys_GetTickScaled @ 1100:02e1 + Sys_Yield @ 1098:0157 poll
     * step over explicit outs (no flag storage here: 1420:1f40 stays
     * caller-side). Frame/GfxPort tails TBD caller-side (TODO Orig @
     * 10b0:0461 / 10d0:0956 / 10d0:0854 / 1090:0376 / 1150:00c9). */
    unsigned long now;

    if (!last_tick || !advanced_out)
        return CIV_ERR_NOENT;
    now = sys_get_tick_scaled();
    if (now == *last_tick) {
        sys_delay_ms(1u);
        sys_yield();
        *advanced_out = 0;
        return CIV_OK;
    }
    *last_tick = now;
    sys_yield();
    *advanced_out = 1;
    return CIV_OK;
}

int sys_pump_messages(void)
{
    /* Sys_PumpMessages @ 1108:0000: Win16 PEEKMESSAGE loop (pumped in
     * unit_wait_for_orders @ 11c0:0ec2 until input/mode change) maps to
     * a single SDL_PumpEvents pass; v1 reports an empty queue (event
     * dispatch via SDL_PollEvent TBD with the modal-wait rewrite). */
    SDL_PumpEvents();
    return 0;
}

int sys_event_requests_quit(unsigned ev_type, int key)
{
    /* Kernel of sys_poll_quit below (same shape as title_event_is_skip in
     * title.c: QUIT / KEY_DOWN / MOUSE_BUTTON_DOWN -> skip). Only QUIT and
     * ESC request quit here (clicks belong to the future MenuZ/modal owner,
     * not to the main-loop quit half). */
    if (ev_type == (unsigned)SDL_EVENT_QUIT)
        return 1;
    if (ev_type == (unsigned)SDL_EVENT_KEY_DOWN &&
        key == (int)SDLK_ESCAPE)
        return 1;
    return 0;
}

int sys_poll_quit(void)
{
    /* WinMain @ 1008:0304-0314 quit half (CMP g_nQuitFlag @ 12d8:24ee,0 /
     * JZ loop; Main_DispatchQuit @ 1098:062f quits on WM_QUIT 0x12, see
     * boot.h main_quit_msg_is_quit). SDL3 drain: every pending event is
     * consumed, quit sticks (1) once seen. No globals, no exit(). */
    SDL_Event ev;
    int quit = 0;

    while (SDL_PollEvent(&ev)) {
        int key = 0;

        if (ev.type == SDL_EVENT_KEY_DOWN)
            key = (int)ev.key.key;
        if (sys_event_requests_quit((unsigned)ev.type, key))
            quit = 1;
    }
    return quit;
}

civ_error_t sys_wait_for_input(void)
{
    /* TODO: Orig FUN_1038_0c1d @ 1038:0c1d -> Sys_WaitForInput (see sys.h:
     * Cursor_Set(0x81) + sys_drain_input @ 11f0:4ea0, then PEEKMESSAGE
     * loop: 0x102 -> key, 0x201 click -> 1 + coords, else pump). SDL
     * SDL_WaitEvent key/mouse bridge TBD (modal for Unit_ShowInfoPopup
     * @ 11f0:468c paint + 20 census/HoF/wizard/Pic/Replay callers). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t main_pump_message(void)
{
    /* TODO: Orig Main_PumpMessage @ 1098:02d1 (Win16 MSG dispatcher behind
     * Main_DrainMessages @ 11e8:02fb; WM_CHAR 0x102 -> Main_EnqueueResString
     * @ 1008:0a8d direct, WM_KEYDOWN 0x100 via Main_KeyToResChar @ 1208:0000
     * (main_key_to_res_char below, LIVE map) else DISPATCHMESSAGE @
     * 1608:0140; quit via WinMain_Shutdown @ 1008:0329). Needs Win16 MSG +
     * accel table + 12b0:0004 window. */
    return CIV_ERR_UNSUPPORTED;
}

int main_key_to_res_char(int vkey)
{
    /* Orig Main_KeyToResChar @ 1208:0000 switch on the WM_KEYDOWN wParam:
     * 0x21..0x28 -> far ptr into the 1420:142c table; Main_PumpMessage @
     * 1098:02d1 enqueues the pointer low byte (default 1420:1400 -> 0x00
     * = skip). Table ids read from the image (base 0x142c low 0x2c). */
    switch (vkey) {
    case 0x21:
        return 0x39;
    case 0x22:
        return 0x33;
    case 0x23:
        return 0x31;
    case 0x24:
        return 0x37;
    case 0x25:
        return 0x34;
    case 0x26:
        return 0x38;
    case 0x27:
        return 0x36;
    case 0x28:
        return 0x32;
    default:
        return 0;
    }
}

civ_error_t sys_get_client_rect(int *w_out, int *h_out)
{
    /* Orig: FUN_1218_0956 @ 1218:0956 family -> Sys_GetClientRect (see
     * sys.h). 1000:137f is Util_MemCopyFar (far memcpy, see civ_common.h
     * util_mem_copy_far), only the copy step inside 1218:0956. Linux
     * equivalent TBD (SDL_GetWindowSize on the status view); NULL either
     * -> NOENT. Stub CIV_ERR_UNSUPPORTED. */
    if (!w_out || !h_out)
        return CIV_ERR_NOENT;
    *w_out = 0;
    *h_out = 0;
    /* TODO: SDL_GetWindowSize mapping for the SDL status view.
     * Pure size gate: sys_client_rect_is_valid below (LIVE). */
    return CIV_ERR_UNSUPPORTED;
}

int sys_client_rect_is_valid(int w, int h)
{
    /* Orig FUN_1218_0956 @ 1218:0956 / FUN_1218_0a06 @ 1218:0a06 size
     * gate (see sys.h): GLOBALLOCK client extent before SETRECT/
     * INFLATERECT; zero/negative extent paints nothing. Verified ref
     * unpacked/civ.exe. */
    return (w > 0 && h > 0) ? 1 : 0;
}

civ_error_t status_measure_row(int *h_out)
{
    if (!h_out)
        return CIV_ERR_NOENT;
    *h_out = 0;
    /* TODO: Orig FUN_1138_0269 @ 1138:0269 (Status_MeasureRow:
     * GETTEXTMETRICS tmHeight variant; feeds the status-row stride
     * alongside status_font_height @ 1098:0dda; needs SDL text-metrics
     * bridge). Pure stride kernel: status_row_stride below (LIVE). */
    return CIV_ERR_UNSUPPORTED;
}

int status_row_stride(int h, int lines)
{
    /* Orig FUN_1138_0269 @ 1138:0269 stride half (see sys.h): local_8
     * line stride, x2 for the civ block. Verified ref unpacked/civ.exe. */
    long r;

    if (h <= 0 || lines <= 0)
        return 0;
    r = (long)h * (long)lines;
    if (r > (long)INT_MAX)
        return INT_MAX;
    return (int)r;
}

civ_error_t sys_check_runtime_memory(void)
{
    /* Sys_CheckRuntimeMemory @ 11e8:0075 (GETFREESPACE <1000000 / <300000 +
     * GETFREESYSTEMRESOURCES <10/<8 -> MESSAGEBOX 1420:4473/1420:44ff, once
     * each via 1420:43b4/43b6). Linux delegate: win16_check_runtime_memory
     * (SDL_GetSystemRAM; warn-once flags stay caller-side, no globals here).
     * Triggers/content LIVE in sys_runtime_* + boot_runtime_needs_*; per-civ
     * call site is Game_TurnLoopTick @ 1088:0000 (check-then-move order). */
    return win16_check_runtime_memory(0, 0);
}

civ_error_t sys_check_free_memory(void)
{
    /* Orig: FUN_11e8_0000 @ 11e8:0000 -> Sys_CheckFreeMemory. Boot check:
     * GETFREESPACE against the required floor, MESSAGEBOX 1420:43b8 on
     * shortfall. Linux delegate: win16_check_free_memory (SDL_GetSystemRAM
     * vs the 2.5 MB + 0x23/0x23 floors). No globals, no exit(), read-only. */
    return win16_check_free_memory();
}

size_t file_write_chunk(FILE *fp, const uint8_t *src, size_t len)
{
    /* File_WriteChunk @ 1000:xxxx (packet E): _HWRITE/_LWRITE thin wrapper
     * complementing File_ReadChunk @ 1000:272a (file_read_chunk). fwrite
     * is the Linux equivalent; short counts on error/disk-full. */
    if (!fp || !src || len == 0)
        return 0;
    return fwrite(src, 1, len, fp);
}

int ne_entry_count(const char *path)
{
    /* Orig NE entry table (MZ e_lfanew @ 0x3C + NE magic, entry table @
     * NE+0x04/0x06, bundle walk) (AH2). OS-free file-scan read-only port
     * (fopen "rb", LE reads, fseek/ftell bounds, no writes, never writes
     * unpacked/civ.exe or civfonts.fon). No globals, no exit(), C17. */
    FILE *fp;
    long fsize;
    uint8_t mz[64];
    uint32_t ne_off;
    uint8_t ne[64];
    uint16_t ent_rel;
    uint16_t ent_len;
    long ent_start;
    long ent_end;
    long pos;
    int total;

    if (!path)
        return -1;
    fp = fopen(path, "rb");
    if (!fp)
        return -1;
    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    fsize = ftell(fp);
    if (fsize < 0x40L) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0L, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    if (fread(mz, 1, sizeof(mz), fp) != sizeof(mz)) {
        fclose(fp);
        return -1;
    }
    if (mz[0] != 0x4Du || mz[1] != 0x5Au) {
        fclose(fp);
        return -1;
    }
    ne_off = (uint32_t)mz[0x3Cu] | ((uint32_t)mz[0x3Du] << 8) |
        ((uint32_t)mz[0x3Eu] << 16) | ((uint32_t)mz[0x3Fu] << 24);
    if ((long)ne_off + 64L > fsize) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, (long)ne_off, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    if (fread(ne, 1, sizeof(ne), fp) != sizeof(ne)) {
        fclose(fp);
        return -1;
    }
    if (ne[0] != 0x4Eu || ne[1] != 0x45u) {
        fclose(fp);
        return -1;
    }
    ent_rel = (uint16_t)((uint16_t)ne[0x04] | ((uint16_t)ne[0x05] << 8));
    ent_len = (uint16_t)((uint16_t)ne[0x06] | ((uint16_t)ne[0x07] << 8));
    ent_start = (long)ne_off + (long)ent_rel;
    ent_end = ent_start + (long)ent_len;
    if (ent_start > ent_end || ent_end > fsize) {
        fclose(fp);
        return -1;
    }
    total = 0;
    pos = ent_start;
    for (;;) {
        uint8_t hdr[2];
        uint8_t cnt;
        uint8_t seg;
        long esz;
        long need;

        if (pos + 2L > ent_end) {
            fclose(fp);
            return -1;
        }
        if (fseek(fp, pos, SEEK_SET) != 0) {
            fclose(fp);
            return -1;
        }
        if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
            fclose(fp);
            return -1;
        }
        cnt = hdr[0];
        seg = hdr[1];
        if (cnt == 0u)
            break;
        esz = (seg == 0xFFu) ? 6L : 3L;
        need = 2L + (long)cnt * esz;
        if (pos + need > ent_end) {
            fclose(fp);
            return -1;
        }
        if (total > 32767 - (int)cnt) {
            fclose(fp);
            return -1;
        }
        total += (int)cnt;
        pos += need;
    }
    fclose(fp);
    return total;
}

int ne_entry_addr(const char *path, int index,
                  uint16_t *seg_out, uint16_t *off_out)
{
    /* Orig NE entry table (same MZ e_lfanew @ 0x3C / NE magic / bundle
     * anchors as ne_entry_count) (AH2). OS-free file-scan read-only port
     * (same fopen "rb" + LE reads, no writes). 0-based index -> seg:off;
     * fixed seg = bundle seg, moveable 0xFF seg = byte 3. No globals,
     * no exit(), C17, read-only; outs untouched on error/OOR. */
    FILE *fp;
    long fsize;
    uint8_t mz[64];
    uint32_t ne_off;
    uint8_t ne[64];
    uint16_t ent_rel;
    uint16_t ent_len;
    long ent_start;
    long ent_end;
    long pos;
    int cur;

    if (!path || !seg_out || !off_out || index < 0)
        return -1;
    fp = fopen(path, "rb");
    if (!fp)
        return -1;
    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    fsize = ftell(fp);
    if (fsize < 0x40L) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0L, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    if (fread(mz, 1, sizeof(mz), fp) != sizeof(mz)) {
        fclose(fp);
        return -1;
    }
    if (mz[0] != 0x4Du || mz[1] != 0x5Au) {
        fclose(fp);
        return -1;
    }
    ne_off = (uint32_t)mz[0x3Cu] | ((uint32_t)mz[0x3Du] << 8) |
        ((uint32_t)mz[0x3Eu] << 16) | ((uint32_t)mz[0x3Fu] << 24);
    if ((long)ne_off + 64L > fsize) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, (long)ne_off, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    if (fread(ne, 1, sizeof(ne), fp) != sizeof(ne)) {
        fclose(fp);
        return -1;
    }
    if (ne[0] != 0x4Eu || ne[1] != 0x45u) {
        fclose(fp);
        return -1;
    }
    ent_rel = (uint16_t)((uint16_t)ne[0x04] | ((uint16_t)ne[0x05] << 8));
    ent_len = (uint16_t)((uint16_t)ne[0x06] | ((uint16_t)ne[0x07] << 8));
    ent_start = (long)ne_off + (long)ent_rel;
    ent_end = ent_start + (long)ent_len;
    if (ent_start > ent_end || ent_end > fsize) {
        fclose(fp);
        return -1;
    }
    cur = 0;
    pos = ent_start;
    for (;;) {
        uint8_t hdr[2];
        uint8_t cnt;
        uint8_t seg;
        long esz;
        long need;

        if (pos + 2L > ent_end) {
            fclose(fp);
            return -1;
        }
        if (fseek(fp, pos, SEEK_SET) != 0) {
            fclose(fp);
            return -1;
        }
        if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
            fclose(fp);
            return -1;
        }
        cnt = hdr[0];
        seg = hdr[1];
        if (cnt == 0u)
            break;
        esz = (seg == 0xFFu) ? 6L : 3L;
        need = 2L + (long)cnt * esz;
        if (pos + need > ent_end) {
            fclose(fp);
            return -1;
        }
        if (index < cur + (int)cnt) {
            int ei = index - cur;
            long epos = pos + 2L + (long)ei * esz;
            uint8_t eb[6];

            if (fseek(fp, epos, SEEK_SET) != 0) {
                fclose(fp);
                return -1;
            }
            if (fread(eb, 1, (size_t)esz, fp) != (size_t)esz) {
                fclose(fp);
                return -1;
            }
            if (seg == 0xFFu) {
                *seg_out = (uint16_t)eb[3];
                *off_out = (uint16_t)((uint16_t)eb[4] |
                    ((uint16_t)eb[5] << 8));
            } else {
                *seg_out = (uint16_t)seg;
                *off_out = (uint16_t)((uint16_t)eb[1] |
                    ((uint16_t)eb[2] << 8));
            }
            fclose(fp);
            return 0;
        }
        cur += (int)cnt;
        pos += need;
    }
    fclose(fp);
    return -1;
}

int sys_run_init_table(sys_table_entry_t *table, size_t n)
{
    /* Sys_RunInitTable @ 1000:00de: min-prio-first drain (see sys.h).
     * Orig scans ES:BX SI..DI stride 6 for smallest byte[1] with
     * byte[0] != 0xff, marks 0xff, CALLF/CALL [BX+2], repeats. */
    size_t i;
    int run = 0;

    if (!table && n > 0)
        return -1;
    for (;;) {
        size_t best = n;
        uint8_t best_prio = 0xFFu;

        for (i = 0; i < n; i++) {
            if (table[i].flag == (int8_t)0xFF || !table[i].fn)
                continue;
            if (best == n || table[i].prio < best_prio) {
                best = i;
                best_prio = table[i].prio;
            }
        }
        if (best == n)
            break;
        table[best].flag = (int8_t)0xFF;
        table[best].fn(0x1000u);
        run++;
    }
    return run;
}

int sys_run_exit_table(sys_table_entry_t *table, size_t n)
{
    /* Sys_RunExitTable @ 1000:0122: max-prio-first drain (exit reverse).
     * Same shape as init but picks largest prio (orig JNC/bVar2 <=
     * chain). Called from 00b2 single + 127f DoExit. */
    size_t i;
    int run = 0;

    if (!table && n > 0)
        return -1;
    for (;;) {
        size_t best = n;
        uint8_t best_prio = 0;

        for (i = 0; i < n; i++) {
            if (table[i].flag == (int8_t)0xFF || !table[i].fn)
                continue;
            if (best == n || table[i].prio >= best_prio) {
                best = i;
                best_prio = table[i].prio;
            }
        }
        if (best == n)
            break;
        table[best].flag = (int8_t)0xFF;
        table[best].fn(0x1000u);
        run++;
    }
    return run;
}

civ_error_t sys_run_exit_table_single(void)
{
    /* TODO: Orig FUN_1000_00b2 @ 1000:00b2 (ES=DS, SI=0x5140 DI=0x5146
     * 1x6B DGROUP entry 00 01 A7 41 00 00 @ 1420:5140 -> Sys_RunExit
     * Table @ 1000:0122). Needs load-time seg fixup for fn ptr. */
    return CIV_ERR_UNSUPPORTED;
}

void sys_noop_exit_hook1(void)
{
    /* Sys_NoopExitHook1 @ 1000:00c4: empty RETF. */
}

void sys_noop_exit_hook2(void)
{
    /* Sys_NoopExitHook2 @ 1000:00c5: empty RETF. */
}

civ_error_t dos_exit_process(int exit_code)
{
    (void)exit_code;
    /* TODO: Orig FUN_1000_00c6 @ 1000:00c6 (AH=0x4c INT 0x21 + fallback
     * via Sys_ExitFallback @ 1000:12d6). No exit() in libs. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t sys_do_exit(int exit_code, int flags)
{
    (void)exit_code;
    (void)flags;
    /* TODO: Orig FUN_1000_127f @ 1000:127f (atexit DAT_1420_4bac drain +
     * 00b2 + 4cb0 + 00c4/00c5 + 4cb4/4cb8 + 00c6). DGROUP + INT 21h. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t sys_exit_fallback(int exit_code)
{
    (void)exit_code;
    /* TODO: Orig FUN_1000_12d6 @ 1000:12d6 (Sys_DoExit(code,0,0)
     * wrapper; entry @ 1000:0000 tail + 00c6 fallback). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t float_check_range(void)
{
    /* TODO: Orig FUN_1000_0164 @ 1000:0164 (x87 FSTCW/0x0c00/FLDCW +
     * range vs 0x43fe/0x407e + DAT_1420_0010=0x22; cf. OVERFLOW @
     * 1420:4ad6, Floating Point Trap @ 1420:50cb). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t float_store_converted(void)
{
    /* TODO: Orig FUN_1000_06cf @ 1000:06cf (SI-flag dispatch to
     * Float_CheckRange + FSTP float/double/extended). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t float_decompose_exponent(void)
{
    /* TODO: Orig FUN_1000_0754 @ 1000:0754 (sign 0x7fff mask +
     * (BX>>2)<<1 index -> JMP CS:[BX+0x784] jumptable). */
    return CIV_ERR_UNSUPPORTED;
}

void sys_noop_pad(void)
{
    /* Orig FUN_1000_03b6 @ 1000:03b6 (PUSH BP / MOV BP,SP / POP BP /
     * RET padding after Util_FormatFloat @ 1000:0221). */
}

void float_discard_st0(void)
{
    /* Orig FUN_1000_06c7 @ 1000:06c7 (WAIT / FSTP ST0: discards the x87
     * stack top). No x87 stack exists in the Linux C ABI, so this stays
     * a documented NOOP and never fakes a computation. */
}

uint8_t float_pack_bcd_chunk(uint16_t dx)
{
    /* Orig FUN_1000_096a @ 1000:096a: DX/100 high half + DX%100 low
     * half, each via AAM 0xa (AH = v / 10, AL = v % 10) + SHL AH,4 +
     * OR, STOSW 2 bytes. Single-byte primitive of that step. */
    uint8_t v = (uint8_t)(dx % 100u);
    uint8_t hi = (uint8_t)(v / 10u);
    uint8_t lo = (uint8_t)(v % 10u);

    return (uint8_t)((hi << 4) | lo);
}

void float_convert_extended_to_bcd(long double x, uint8_t bcd_out[10])
{
    /* Orig FUN_1000_0988 @ 1000:0988: FSTP extended, exponent/mantissa
     * threshold gates (0x3ffe / -0x3c / 0xde0b/0x6b3a/0x763f/0xfff0),
     * DIV 10000 (0x2710) chain with 4x Float_PackBcdChunk @ 1000:096a
     * + AAM tail + sign ([SI+9] & 0x80). Same branches/layout here via
     * the pack helper and integer /% groups. */
    uint8_t raw[10];
    uint16_t bx;
    uint16_t cx;
    uint16_t si;
    uint16_t dxw;
    uint16_t exp;
    uint16_t expb;
    int e1;
    int e2;
    int i;
    unsigned long long mag;
    uint8_t sign;

    if (!bcd_out)
        return;
    memcpy(raw, &x, sizeof(raw));
    bx = (uint16_t)((uint16_t)raw[0] | ((uint16_t)raw[1] << 8));
    cx = (uint16_t)((uint16_t)raw[2] | ((uint16_t)raw[3] << 8));
    si = (uint16_t)((uint16_t)raw[4] | ((uint16_t)raw[5] << 8));
    dxw = (uint16_t)((uint16_t)raw[6] | ((uint16_t)raw[7] << 8));
    exp = (uint16_t)((uint16_t)raw[8] | ((uint16_t)raw[9] << 8));
    sign = (uint8_t)(raw[9] & 0x80u);
    expb = (uint16_t)(exp & 0x7FFFu);
    e1 = (int)expb - 0x3FFEu;
    if (e1 < 0) {
        memset(bcd_out, 0x00, 9);
        bcd_out[9] = sign;
        return;
    }
    e2 = e1 - 0x3Cu;
    if (e2 > 0)
        goto overflow;
    if (e2 == 0) {
        if (dxw < 0xDE0Bu)
            goto convert;
        if (dxw > 0xDE0Bu)
            goto overflow;
        if (si < 0x6B3Au)
            goto convert;
        if (si > 0x6B3Au)
            goto overflow;
        if (cx < 0x763Fu)
            goto convert;
        if (cx > 0x763Fu)
            goto overflow;
        if (bx > 0xFFF0u)
            goto overflow;
    }
convert:
    if (e2 < 0 || e2 == 0) {
        /* Magnitude fits the 18-digit (9 BCD byte) field: orig DIV
         * chain extracts low 10000-groups first (STOSW order). */
        mag = (unsigned long long)fabsl(x);
        for (i = 0; i < 4; i++) {
            unsigned chunk = (unsigned)(mag % 10000ULL);

            mag /= 10000ULL;
            bcd_out[2 * i] =
                float_pack_bcd_chunk((uint16_t)(chunk % 100u));
            bcd_out[2 * i + 1] =
                float_pack_bcd_chunk((uint16_t)(chunk / 100u));
        }
        if (mag > 99ULL)
            goto overflow;
        bcd_out[8] = float_pack_bcd_chunk((uint16_t)mag);
        bcd_out[9] = sign;
        return;
    }
overflow:
    memset(bcd_out, 0x99, 9);
    bcd_out[9] = sign;
}

double float_log2(double x)
{
    /* Orig FUN_1000_0c03 @ 1000:0c03 (FYL2X, with FLD1/FSUBP/FYL2XP1
     * log2((x - 1) + 1) when exp == 0x3fff && hi == 0x8000). */
    if (x == 1.0)
        return 0.0;
    return log2(x);
}

double float_exp2(double x)
{
    /* Orig FUN_1000_0c7d @ 1000:0c7d (FLDL2E FLOAT_LOG2E * x, ABS,
     * FSCALE/FRNDINT split, F2XM1 + 1, odd-bit sqrt(2) multiply via
     * _DAT_1000_0a85). Linux uses exp2() directly. */
    return exp2(x);
}

const char *float_matherr_name(int type)
{
    /* Orig table [BX+0x4aae] @ 1420:4aae (6 far ptrs -> DOMAIN/SING/
     * OVERFLOW/UNDERFLOW/TLOSS/PLOSS @ 1420:4aca+). Switch keeps the
     * no-globals rule; entry 0 (0x8000:0x0001) is unused/invalid. */
    switch (type) {
    case 1:
        return "DOMAIN";
    case 2:
        return "SING";
    case 3:
        return "OVERFLOW";
    case 4:
        return "UNDERFLOW";
    case 5:
        return "TLOSS";
    case 6:
        return "PLOSS";
    default:
        return "UNKNOWN";
    }
}

int float_matherr_filter(float_matherr_t *e)
{
    /* Orig FUN_1000_0e2e @ 1000:0e2e -> Float_MatherrFilter: type 4
     * zeroes retval (FSTP double ES:[BX+0x16]) and suppresses (1),
     * type 5 suppresses (1), else 0 = report. */
    if (!e)
        return 0;
    if (e->type == 4) {
        e->retval = 0.0;
        return 1;
    }
    if (e->type == 5)
        return 1;
    return 0;
}

int util_sprintf(char *dst, size_t cap, const char *fmt, ...)
{
    /* Orig FUN_1000_35f6 @ 1000:35f6 -> Util_Sprintf (*dst = 0, then
     * Util_Vsprintf @ 1000:19d8 -> Util_VsprintfCore @ 1000:1a3c).
     * Linux 1:1 via vsnprintf (cap includes NUL). */
    va_list ap;
    int n;

    if (!dst || cap == 0 || !fmt)
        return -1;
    dst[0] = '\0';
    va_start(ap, fmt);
    n = vsnprintf(dst, cap, fmt, ap);
    va_end(ap);
    return n;
}

civ_error_t sys_fatal_abort(const char *msg)
{
    /* Orig Sys_FatalAbort @ 1000:4124 (decompiled 2026-10-16: caption
     * via sys_fatal_basename kernel below, then GETDESKTOPWINDOW +
     * MESSAGEBOX style 0x1010; void, never returns in practice).
     * PARTIAL: best-effort dialog via win16_message_box (caption
     * "CivLin" default); the UNSUPPORTED tail stays because a lib never
     * exits — the owner (main.c) exits. Callers (float_report_error @
     * 1000:1056) ignore the code, like orig ignoring the MESSAGEBOX rc.
     * NULL msg -> NOENT (orig faulted).
     * TODO: Orig FUN_1000_4124 @ 1000:4124 (GETDESKTOPWINDOW + MESSAGEBOX
     * style 0x1010 Win16 tail). Live twins: sys_fatal_basename caption
     * kernel above (LIVE) + win16_message_box dialog (PARTIAL). */
    if (!msg)
        return CIV_ERR_NOENT;
    (void)win16_message_box(msg, NULL);
    return CIV_ERR_UNSUPPORTED;
}

const char *sys_fatal_basename(const char *path)
{
    /* Orig Sys_FatalAbort @ 1000:4124 caption kernel: Util_StrRChr @
     * 1000:37b2 over '\\' 0x5c, +1 past it, else the whole far path.
     * Thin delegate to the string family (util_str_rchr LIVE). */
    const char *b;

    if (!path)
        return NULL;
    b = util_str_rchr(path, '\\');
    return b ? b + 1 : path;
}

double float_report_error(float_matherr_t *e, char *buf, size_t cap,
                          int *errno_out)
{
    /* Orig Float_ReportError @ 1000:1056 (matherr: NULL doubles -> 0.0
     * via FLDZ, filter, sprintf "%s: %s error\n" @ 1420:4b9e with
     * "pow" @ 1420:4b96/4b9a + name table [BX+0x4aae], errno
     * DAT_1420_0010 = 0x22 types 3/4 else 0x21, Sys_FatalAbort @
     * 1000:4124, ST0 = retval). PARTIAL: far/DGROUP dropped, errno
     * via out-param, dialog via sys_fatal_abort (STUB-tolerant:
     * its UNSUPPORTED is ignored, like orig ignoring MESSAGEBOX rc).
     * Suppressed (filter 1): no format/abort, errno/buf untouched. */
    const char *func;
    const char *err;
    int code;

    if (!e)
        return 0.0;
    if (float_matherr_filter(e))
        return e->retval;
    if (!buf || cap == 0 || !errno_out)
        return e->retval;
    func = e->name ? e->name : "pow";
    err = float_matherr_name(e->type);
    /* Orig PUSH order: err-name far ptr then func-name far ptr into
     * "%s: %s error\n", i.e. "func: err error". */
    util_sprintf(buf, cap, "%s: %s error\n", func, err);
    code = (e->type == 3 || e->type == 4) ? 0x22 : 0x21;
    *errno_out = code;
    (void)sys_fatal_abort(buf);
    return e->retval;
}

long double float_pow10(int n)
{
    /* Orig Float_Pow10 @ 1000:0d56: clamp +-0x1344, 1.0/0.0/INF edges,
     * binary decomposition over float @ 1420:4b0e, float [0x4b2e],
     * double [0x4b32], extended [0x4b3a..0x4b80], FDIVR finish.
     * Linux 1:1 observable via powl (same edges/clamps). */
    if (n < -0x1344)
        return 0.0L;
    if (n > 0x1344)
        return (long double)INFINITY;
    if (n == 0)
        return 1.0L;
    return powl(10.0L, (long double)n);
}

long double float_pow10_far(int n)
{
    /* Orig Float_Pow10Far @ 1000:0e20 (INC BP far RETF wrapper: PUSH
     * [BP+6] / CALL 1000:0d56). Thin delegate. */
    return float_pow10(n);
}

double float_exp2_checked(double x)
{
    /* Orig Float_Exp2Checked @ 1000:0e5a: extended gate (exp&0x7fff
     * vs 0x4008, mantissa vs 0xb172) -> type 3/4 + "pow" @ 1420:4b96
     * with HUGE_VAL @ 1420:4a92 (overflow) / 0.0 (underflow), else
     * Float_Exp2 @ 1000:0c7d. PARTIAL on double: same type/name
     * reports via float_report_error, double-range gate; exact
     * extended 0x4008/0xb172 thresholds TBD (TODO Orig). */
    float_matherr_t e;
    char msg[80];
    int code = 0;

    if (!isfinite(x))
        return exp2(x);
    if (x > 1023.0) {
        e.type = 3;
        e.name = "pow";
        e.arg1 = x;
        e.arg2 = 0.0;
        e.retval = HUGE_VAL;
        return float_report_error(&e, msg, sizeof(msg), &code);
    }
    if (x < -1075.0) {
        e.type = 4;
        e.name = "pow";
        e.arg1 = x;
        e.arg2 = 0.0;
        e.retval = 0.0;
        return float_report_error(&e, msg, sizeof(msg), &code);
    }
    return float_exp2(x);
}

double float_pow(double x, double y)
{
    /* Orig Float_Pow @ 1000:0ec8 (PARTIAL): 0x7ff0 exponent-class
     * tests per operand (zero/denormal vs INF/NAN -> types 1/3/4 +
     * "pow" @ 1420:4b9a), int-exp fast path (ROR 4 / SUB 0x3ff vs
     * 0x3f, <0xc bit-exactness, FCHS on negative base with odd int),
     * else y*log2(x) (Float_Log2 @ 1000:0c03) -> Float_Exp2Checked @
     * 1000:0e5a with FCHS fix. Linux mirrors the dispatch: domain
     * gates via float_report_error, main path via float_log2 +
     * float_exp2_checked, integer exponents via pow (same values as
     * the ROR loop). 1190 callers are false merges -> IGNORED.
     * TODO Orig: exact FISTP/ROR bit path + SING mapping. */
    float_matherr_t e;
    char msg[80];
    int code = 0;
    double ax;
    double ay;

    if (isnan(x) || isnan(y))
        return pow(x, y);
    if (isinf(y)) {
        if (fabs(x) == 1.0)
            return 1.0;
        if (fabs(x) < 1.0)
            return (y > 0.0) ? 0.0 : HUGE_VAL;
        return (y > 0.0) ? HUGE_VAL : 0.0;
    }
    if (isinf(x)) {
        if (y == 0.0)
            return 1.0;
        if (x > 0.0)
            return (y > 0.0) ? HUGE_VAL : 0.0;
        /* Negative INF base: parity decides, like the FCHS tail. */
        ay = floor(y);
        if (y != ay) {
            e.type = 1;
            e.name = "pow";
            e.arg1 = x;
            e.arg2 = y;
            e.retval = NAN;
            return float_report_error(&e, msg, sizeof(msg), &code);
        }
        ax = (y > 0.0) ? HUGE_VAL : 0.0;
        if (fmod(ay, 2.0) != 0.0)
            ax = -ax;
        return ax;
    }
    if (x == 0.0) {
        if (y > 0.0)
            return 0.0;
        if (y == 0.0)
            return 1.0;
        e.type = 3;
        e.name = "pow";
        e.arg1 = x;
        e.arg2 = y;
        e.retval = HUGE_VAL;
        return float_report_error(&e, msg, sizeof(msg), &code);
    }
    if (x < 0.0) {
        ay = floor(y);
        if (y != ay) {
            e.type = 1;
            e.name = "pow";
            e.arg1 = x;
            e.arg2 = y;
            e.retval = NAN;
            return float_report_error(&e, msg, sizeof(msg), &code);
        }
        ax = float_exp2_checked(y * float_log2(-x));
        if (fmod(ay, 2.0) != 0.0)
            ax = -ax;
        return ax;
    }
    if (y == 0.0)
        return 1.0;
    if (y == 1.0)
        return x;
    return float_exp2_checked(y * float_log2(x));
}

static int float_scan_is_space(int c)
{
    /* Orig ctype table [c+0x4baf] & 1 (isspace set). Manual set to
     * stay locale-independent. */
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
        c == '\f' || c == '\v';
}

double float_scan_float(float_getchar_fn get, float_ungetchar_fn unget,
                        void *ctx, int nmax, int *consumed, int *status)
{
    /* Orig Float_ScanFloat @ 1000:03bc (PARTIAL): see sys.h for the
     * token flow (ws/sign/INF/NAN/digits 0x12-cap/e-exponent 0x1344/
     * Pow10Far scale/FCHS). nmax = [BP+0xc] get/unget budget,
     * *consumed = [BP+0xe] char count, *status = [BP+0x12] (1 ok,
     * 0 no-digits/fail, 2 overflow, -1 immediate EOF). Caller @
     * 1000:0605 scales via Float_Pow10Far; INF/NAN consts @
     * 1420:4af6/4afa/4afe/4b02 (float +-INF/+-NAN). TODO Orig: exact
     * 18-digit rounding + x87 FILD/FMULP last-ulp behavior. */
    int left;
    int used = 0;
    int c = 0;
    int neg = 0;
    int ndig = 0;
    int ndot = -1;
    int expv = 0;
    int exp_over = 0;
    long double mant = 0.0L;
    long double scale;
    int any = 0;

    if (!get || !consumed || !status)
        return 0.0;
    left = nmax;
    /* Leading whitespace + sign (orig CBW/XCHG + isspace loop). */
    for (;;) {
        if (left <= 0) {
            *consumed = used;
            *status = -1;
            return 0.0;
        }
        c = get(ctx);
        left--;
        used++;
        if (c < 0) {
            *consumed = used - 1;
            *status = -1;
            return 0.0;
        }
        if (!float_scan_is_space(c))
            break;
    }
    if ((c == '+' || c == '-') && left > 0) {
        int s = c;
        int d;

        /* Peek for INF/NAN words (orig 0633/0675 tails). Need one
         * char lookahead: consume, decide, push back if numeric. */
        if (left <= 0) {
            *consumed = used;
            *status = 0;
            return 0.0;
        }
        d = get(ctx);
        left--;
        used++;
        if (d == 'I' || d == 'i' || d == 'N' || d == 'n') {
            int up = (d == 'I' || d == 'N') ? d : (d - 32);
            const char *want = (up == 'I') ? "NF" : "AN";
            int k0;
            int k1;

            if (left < 2) {
                *consumed = used;
                *status = 0;
                return 0.0;
            }
            k0 = get(ctx);
            k1 = get(ctx);
            left -= 2;
            used += 2;
            if ((k0 == want[0] || k0 == want[0] + 32) &&
                (k1 == want[1] || k1 == want[1] + 32)) {
                double v = (up == 'I') ? INFINITY : NAN;
                *consumed = used;
                *status = 1;
                return (s == '-') ? -v : v;
            }
            *consumed = used;
            *status = 0;
            return 0.0;
        }
        if (unget)
            unget(d, ctx);
        left++;
        used--;
        c = s;
    } else if (c == 'I' || c == 'i' || c == 'N' || c == 'n') {
        int up = (c == 'I' || c == 'N') ? c : (c - 32);
        const char *want = (up == 'I') ? "NF" : "AN";
        int k0;
        int k1;

        if (left < 2) {
            *consumed = used;
            *status = 0;
            return 0.0;
        }
        k0 = get(ctx);
        k1 = get(ctx);
        left -= 2;
        used += 2;
        if ((k0 == want[0] || k0 == want[0] + 32) &&
            (k1 == want[1] || k1 == want[1] + 32)) {
            double v = (up == 'I') ? INFINITY : NAN;
            *consumed = used;
            *status = 1;
            return v;
        }
        *consumed = used;
        *status = 0;
        return 0.0;
    }
    if (c == '+' || c == '-') {
        neg = (c == '-');
        if (left <= 0) {
            *consumed = used;
            *status = 0;
            return 0.0;
        }
        c = get(ctx);
        left--;
        used++;
    }
    /* Mantissa: digits + one dot, 18 significant caps (0x12). */
    for (;;) {
        if (c == '.' && ndot < 0) {
            ndot = ndig;
            if (left <= 0)
                break;
            c = get(ctx);
            left--;
            used++;
            continue;
        }
        if (c < '0' || c > '9')
            break;
        any = 1;
        if (ndig < 18) {
            mant = mant * 10.0L + (long double)(c - '0');
            ndig++;
        } else {
            /* Beyond the cap orig keeps MUL 10 chains for scale
             * only; mantissa precision TBD (TODO Orig). */
            if (ndot < 0)
                expv += 1;
        }
        if (left <= 0) {
            c = -1;
            break;
        }
        c = get(ctx);
        left--;
        used++;
    }
    if (!any) {
        if (c >= 0 && unget) {
            unget(c, ctx);
            left++;
            used--;
        }
        *consumed = used;
        *status = 0;
        return 0.0;
    }
    if (ndot < 0)
        ndot = ndig;
    /* Decimal shift from the dot: value = mant * 10^(ndot-ndig). */
    expv += ndot - ndig;
    /* Exponent part e/E with 0x1344 clamp (orig DI limit). */
    if (c == 'e' || c == 'E') {
        int ec = 0;
        int es = 1;
        int ed = 0;

        if (left <= 0) {
            if (unget)
                unget(c, ctx);
            left++;
            used--;
            c = -1;
        } else {
            c = get(ctx);
            left--;
            used++;
            if (c == '+' || c == '-') {
                es = (c == '-') ? -1 : 1;
                if (left <= 0) {
                    *consumed = used;
                    *status = 0;
                    return 0.0;
                }
                c = get(ctx);
                left--;
                used++;
            }
            for (;;) {
                if (c < '0' || c > '9')
                    break;
                ed = 1;
                ec = ec * 10 + (c - '0');
                if (ec > 0x1344) {
                    ec = 0;
                    exp_over = 1;
                }
                if (left <= 0) {
                    c = -1;
                    break;
                }
                c = get(ctx);
                left--;
                used++;
            }
            if (!ed) {
                /* No exponent digits: push back terminator like
                 * orig 0586 unget path. */
                if (c >= 0 && unget) {
                    unget(c, ctx);
                    left++;
                    used--;
                }
            } else {
                expv += es * ec;
            }
        }
    } else if (c >= 0 && unget) {
        unget(c, ctx);
        left++;
        used--;
    }
    if (exp_over) {
        *consumed = used;
        *status = 2;
        return neg ? -INFINITY : INFINITY;
    }
    scale = float_pow10_far(expv);
    mant *= scale;
    *consumed = used;
    *status = 1;
    return (double)(neg ? -mant : mant);
}

char util_emit_hex_digit(unsigned v)
{
    /* Orig Util_EmitHexDigit @ 1000:19d0 (ADD 0x90 / DAA / ADC 0x40 /
     * DAA / STOSB): low nibble to ASCII hex uppercase 1:1. */
    static const char digits[] = "0123456789ABCDEF";

    return digits[v & 15u];
}

int util_emit_hex_byte(char *out, unsigned v)
{
    /* Orig Util_EmitHexByte @ 1000:19c7 (AAM 0x10 split + 2x Digit:
     * high via CALL 19d0, low via fallthrough). Big-nibble order. */
    if (!out)
        return -1;
    out[0] = util_emit_hex_digit((v >> 4) & 15u);
    out[1] = util_emit_hex_digit(v & 15u);
    return 2;
}

int util_emit_hex_word(char *out, unsigned v)
{
    /* Orig Util_EmitHexWord @ 1000:19c0 (MOV AL,DH / CALL Byte +
     * MOV AL,DL fallthrough): DX as 4 hex, high byte first (same as
     * %p seg:off halves; caller emits ':' when far). */
    if (!out)
        return -1;
    util_emit_hex_byte(out, (v >> 8) & 255u);
    util_emit_hex_byte(out + 2, v & 255u);
    return 4;
}

size_t util_vsprintf_strlen(const char *s)
{
    /* Orig Util_VsprintfStrLen @ 1000:19f2 (SCASB.REPNE/NOT CX/DEC CX,
     * DI preserved): strlen without NUL. NULL -> 0 (orig faulted). */
    if (!s)
        return 0;
    return strlen(s);
}

int util_format_int(char *out, size_t cap, uint32_t v, int is_signed,
                    unsigned base, char digits)
{
    /* Orig Util_FormatInt @ 1000:157a (base 2..36 else empty, signed
     * negate when (int32)v < 0, DIV chain into 34 B scratch, reversed
     * with '0'-'9' or (b-10)+digits). Wrappers 15fc/2928/2974 pass
     * digits='a' (0x61); VsprintfCore %x/%X passes 'a'/'A' via c-0x17.
     * Returns chars without NUL, -1 on NULL/cap0/truncation; invalid
     * base -> empty string (0) 1:1. */
    char tmp[34];
    int n = 0;
    int neg = 0;
    uint32_t mag;
    int i;

    if (!out || cap == 0)
        return -1;
    if (base < 2u || base > 36u) {
        out[0] = '\0';
        return 0;
    }
    if (digits != 'a' && digits != 'A')
        digits = 'a';
    if (is_signed && (int32_t)v < 0) {
        neg = 1;
        mag = 0u - v;
    } else {
        mag = v;
    }
    if (mag == 0)
        tmp[n++] = '0';
    else {
        while (mag != 0) {
            unsigned d = mag % base;

            mag /= base;
            if (d < 10u)
                tmp[n++] = (char)('0' + d);
            else
                tmp[n++] = (char)(digits + (d - 10u));
        }
    }
    /* Need neg + n chars + NUL. */
    if ((size_t)(n + neg) + 1u > cap)
        return -1;
    i = 0;
    if (neg)
        out[i++] = '-';
    while (n > 0)
        out[i++] = tmp[--n];
    out[i] = '\0';
    return i;
}

int util_parse_int(const char *s)
{
    /* Orig FUN_1000_1ed2 @ 1000:1ed2 -> Util_ParseInt (see sys.h).
     * Decompiler: isspace skip via ctype [c+0x4baf] & 1, sign on the
     * first non-space byte (0x2b '+' skip, 0x2d '-' neg), digit run
     * with unsigned wrap (orig 16-bit carry fast path + 32-bit tail;
     * Linux single uint32_t accumulator, same wrap), stop at the first
     * non-digit (trailing bytes ignored, no endptr). OS-free 1:1. */
    const unsigned char *p = NULL;
    unsigned char c = 0;
    uint32_t acc = 0;
    int neg = 0;

    if (!s)
        return 0;
    p = (const unsigned char *)s;
    do {
        c = *p++;
    } while (float_scan_is_space((int)c));
    if (c == (unsigned char)'+') {
        c = *p++;
    } else if (c == (unsigned char)'-') {
        neg = 1;
        c = *p++;
    }
    for (;;) {
        if (c < (unsigned char)'0' || c > (unsigned char)'9')
            break;
        acc = acc * 10u + (uint32_t)(unsigned char)(c - (unsigned char)'0');
        c = *p++;
    }
    if (neg)
        acc = 0u - acc;
    return (int)acc;
}

const char *debug_log_trace(void)
{
    /* Orig FUN_1148_003c @ 1148:003c -> Debug_LogTrace (see sys.h).
     * Release no-op: the decompiler body is just "return static_base"
     * (the trace-file/line prefix the 40+ callers push is dropped in
     * this build). Returns a read-only empty string. */
    return "";
}

civ_error_t mem_dispose_ptr(const void *p)
{
    /* Orig FUN_1100_060b @ 1100:060b (Mem_DisposePtr:
     * Debug_LogTrace("DisposPtr", "mac_c") + GLOBALHANDLE(select) gate
     * (zero select = no-op) + GLOBALUNLOCK + GLOBALFREE; 12 callers
     * incl. WinMain_Shutdown @ 1008:0329 + Map_GenerateRandomOrLoad @
     * 1028:0000 + save_serialize @ 10f8:02bd). WONTFIX-Win16 (see
     * win16_compat.h; selectors have no Linux equivalent: memory is
     * caller-owned malloc/arena, nothing to unlock/free here) ->
     * explicit no-op CIV_OK, NULL included (orig zero-select no-op).
     * No globals, no exit(), read-only. */
    (void)p;
    return CIV_OK;
}

civ_error_t mem_dispose_handle(int handle)
{
    /* Orig FUN_1100_0652 @ 1100:0652 (Mem_DisposeHandle:
     * Debug_LogTrace("DisposeHandle", "mac_c") + GLOBALUNLOCK +
     * GLOBALFREE; 0 = no-op). Same WONTFIX no-op as above (opaque int
     * handle, nothing locked on Linux) -> CIV_OK including handle 0. */
    (void)handle;
    return CIV_OK;
}

civ_error_t mem_realloc_handle(int *handle)
{
    /* Orig FUN_1100_030f @ 1100:030f (Mem_ReallocHandle:
     * Debug_LogTrace("HandToHand", "mac_c") + GLOBALSIZE (0 -> 0xff93)
     * + GLOBALALLOC (fail -> 0xff94) + GLOBALLOCK x2 + Util_MemMove +
     * unlocks, *handle = new). Same WONTFIX no-op: no selector move on
     * Linux, *handle untouched. NULL out-param -> NOENT (orig would
     * fault writing through it). */
    if (!handle)
        return CIV_ERR_NOENT;
    return CIV_OK;
}

/* --- Chain 2026-10-16 (16): modal-wait discriminators @ 1038:0c1d ---
 * See sys.h decision (modal stays STUB; SDL_WaitEvent TBD). OS-free 1:1. */

int sys_wait_msg_is_key(int msg)
{
    /* Orig Sys_WaitForInput @ 1038:0c1d: DAT_1420_54e8 == 0x102 (WM_CHAR). */
    return (msg == 0x102) ? 1 : 0;
}

int sys_wait_msg_is_lbutton(int msg)
{
    /* Orig same: DAT_1420_54e8 == 0x201 (WM_LBUTTONDOWN). */
    return (msg == 0x201) ? 1 : 0;
}

int sys_wait_scale_coord(int v)
{
    /* Orig same click branch: (uint)((int)v * 10) >> 4 (LOWORD/HIWORD
     * of DAT_1420_54ec -> DAT_1328_0010/000e). Guard for v < 0 is a
     * Linux-side addition (orig did unsigned wrap on a pixel coord). */
    if (v < 0)
        return -1;
    return (v * 10) >> 4;
}

int sys_wait_cursor_id(void)
{
    /* Orig same prologue: Cursor_Set(0x81) before the drain + loop. */
    return 0x81;
}

/* --- Chain 2026-10-16 (17): file-table geometry (goal 4) ---
 * See sys.h block. OS-free 1:1 (no DOS, no globals, no exit()). */

int file_table_first_fd(void)
{
    /* Orig File_CloseAll @ 1000:1fc6: uVar3 = 5 (0..4 to stdin/out/err/
     * aux/prn, skan od 5 do DAT_1420_4e4c). */
    return CIV_FILE_TABLE_FIRST_FD;
}

int file_table_entry_len(void)
{
    /* Orig same: local_6 += 0x14 (20 B na wpis od 0x4d20 DGROUP). */
    return CIV_FILE_TABLE_ENTRY_LEN;
}

int file_table_fd_off(void)
{
    /* Orig same: *(char *)(local_6 + 4) to fd byte (signed char). */
    return CIV_FILE_TABLE_FD_OFF;
}

int file_entry_is_open(int fd_byte)
{
    /* Orig File_CloseEntry @ 1000:2098 gate: -1 < *(char *)((int *)p)[2]
     * (fd byte na +4; -1/0xff = pusty slot) -> File_CloseWrapper @
     * 1000:1f7c (0x4e4e-table clear + Dos_Close @ 1000:1fa6). */
    return (fd_byte >= 0) ? 1 : 0;
}

int file_table_slot_rel(int fd)
{
    /* Orig File_CloseAll @ 1000:1fc6 slot: 0x4d20 + (fd-5)*0x14 (wzgledny
     * offset, bez bazy DGROUP; DAT_1420_4e4c to high-water slotow). */
    if (fd < CIV_FILE_TABLE_FIRST_FD)
        return -1;
    return (fd - CIV_FILE_TABLE_FIRST_FD) * CIV_FILE_TABLE_ENTRY_LEN;
}

/* --- Chain 2026-10-16 (18): drive/path guards + tmp format (goals 1+3) ---
 * See sys.h block. Guards LIVE 1:1 on explicit params (no DOS, no globals);
 * raw INT 21h bodies stay STUB with TODO Orig. */

int sys_path_guard_should_set(int drive)
{
    /* Orig Config_SavePrefs @ 1008:0bbf: CMP word ptr ES:[0x2],0x0 /
     * JLE skip (g_nCurrentDrive @ 1228:0002); same JLE shape in
     * Sound_PlayId @ 1150:00c9 (0 < g_nCurrentDrive), Art_LoadDiskFile @
     * 10b8:0aa8, Art_SaveConvertedFrame @ 10b8:0c1c. Linux drive is
     * always 0 (dos_get_current_drive), so the guard skips the chdir. */
    return (drive > 0) ? 1 : 0;
}

int sys_drive_ready_ok(int rc)
{
    /* Orig same sites: OR SI,SI / JNZ skip (Dos_DriveReady @ 1000:1184
     * rc: 0 = ready, 0xffff = not). A not-ready drive still falls
     * through to the file op (only the chdir is skipped); only the
     * SetPath failure aborts (see sys_path_guard_abort). */
    return (rc == 0) ? 1 : 0;
}

int sys_path_guard_abort(int rc)
{
    /* Orig Config_SavePrefs @ 1008:0bbf: OR SI,SI / JZ continue /
     * JMP abort (Dos_SetPath @ 1000:116a rc; nonzero -> return without
     * _LCREAT). Sound_PlayId @ 1150:00c9 ignores the rc instead
     * (fire-and-forget chdir before the WAV open). */
    return (rc != 0) ? 1 : 0;
}

civ_error_t dos_chdir_raw(const char *path)
{
    if (!path)
        return CIV_ERR_NOENT;
    /* TODO: Orig FUN_1000_116a @ 1000:116a (Dos_SetPath: MOV AH,0x3b /
     * LDS DX,[BP+6] far path / INT 0x21; CF -> Dos_MapError @ 1000:151a,
     * else 0). Callers pass 1228:0004 (g_szBasePath via sys_init_paths;
     * verified PUSH 0x1228 / PUSH 0x4 in Config_SavePrefs @ 1008:0bff).
     * Needs DOS; the Linux high-level twin dos_set_path in boot.c stays
     * a CIV_OK no-op (cwd owned by the launcher). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t dos_select_drive(int drive, unsigned *out_total)
{
    if (out_total)
        *out_total = 0;
    /* TODO: Orig FUN_1000_1266 @ 1000:1266 (Dos_SelectDrive:
     * MOV DL,[BP+6] / DEC DL / MOV AH,0x0e / INT 0x21; AL = drive count
     * -> ES:[BX] out). Sole users: Dos_DriveReady @ 1000:1184 +
     * Dos_GetCurrentDrive @ 1000:1252. Needs DOS. */
    (void)drive;
    return CIV_ERR_UNSUPPORTED;
}

const char *file_temp_env_name(void)
{
    /* Orig File_BuildTempPath @ 1000:1648 default dir DS:0x4ed6 = "TMP"
     * (1420:4ed6, env var name read from the image). */
    return "TMP";
}

const char *file_temp_suffix(void)
{
    /* Orig same DS:0x4eda = ".$$$" (1420:4eda) appended via
     * Util_StrAppend @ 1000:363e. */
    return ".$$$";
}

int file_build_temp_path(char *dst, size_t cap, const char *dir,
                         const char *prefix, unsigned num)
{
    /* Orig File_BuildTempPath @ 1000:1648: FUN_1000_1e9c copy +
     * FUN_1000_15fc decimal (Util_FormatInt @ 1000:157a) +
     * Util_StrAppend @ 1000:363e ".$$$" (DS:0x4eda). Uniqueness lives one
     * level up in File_BuildUniqueTempPath @ 1000:16a5 (counter +1/+2 +
     * Dos_GetFileAttr @ 1000:1210 INT 21h AH=0x43 probe loop); the tmp
     * slot ([8] != 0, O_TEMPORARY-style via File_Open @ 1000:2ad9: save
     * temp, Art_SaveConvertedFrame @ 10b8:0c1c disk cache) is rebuilt in
     * File_CloseEntry @ 1000:2098 as (num,0,0,0,0) then Dos_Unlink @
     * 1000:19a6 (INT 21h AH=0x41). No randomness: plain incrementing
     * counter. Linux 1:1 on explicit params (no far, no DOS, no globals):
     * dir NULL/empty -> getenv("TMP") -> "." fallback; prefix NULL ->
     * ""; joined with '/' (orig '\\' on DOS). Returns chars without NUL,
     * -1 on NULL dst / cap == 0 / truncation. */
    const char *d;
    const char *p;
    int n;

    if (!dst || cap == 0)
        return -1;
    d = (dir && dir[0] != '\0') ? dir : getenv("TMP");
    if (!d || d[0] == '\0')
        d = ".";
    p = (prefix && prefix[0] != '\0') ? prefix : "";
    n = snprintf(dst, cap, "%s/%s%u.$$$", d, p, num);
    if (n < 0 || (size_t)n >= cap)
        return -1;
    return n;
}

int file_temp_next_num(int cur)
{
    /* Orig File_BuildUniqueTempPath @ 1000:16a5 (decompiled 2026-10-16:
     * *p == -1 (free slot) ? *p += 2 : *p += 1; the Dos_GetFileAttr @
     * 1000:1210 probe loop around it needs DOS, see dos_file_attr). */
    return cur + ((cur == -1) ? 2 : 1);
}

/* --- Chain 2026-10-16 (19): message-pump hierarchy (goals 1+5) ---
 * See sys.h block. OS-free 1:1 (no Win16, no globals, no exit()). */

int sys_pump_mode_select(int flag_1d4c, int flag_1d50)
{
    /* Orig Sys_PumpMessages @ 1108:0000: if (DAT_1420_1d4c == 0) {
     *   if (DAT_1420_1d50 == 0) DAT_1328_0012 = 0; else 2; } else 1. */
    if (flag_1d4c != 0)
        return 1;
    if (flag_1d50 == 0)
        return 0;
    return 2;
}

int sys_pump_scale_coord(int v)
{
    /* Orig Sys_PumpMessages @ 1108:0000 popup branch (v*10>>4) + LIVE above. */
    if (v < 0)
        return -1;
    return (v * 10) >> 4;
}

/* --- Chain 2026-10-16 (28): runtime-warning triggers/content (goal 3).
 * See sys.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()). */

int sys_runtime_warn_style_first(void)
{
    /* Orig Sys_CheckRuntimeMemory @ 11e8:0075 branch 1 MESSAGEBOX style
     * 0x30 (MB_ICONEXCLAMATION: "WARNING" @ 1420:44f7). */
    return 0x30;
}

int sys_runtime_warn_style_final(void)
{
    /* Orig same branch 2 MESSAGEBOX style 0x10 (MB_ICONHAND: "FINAL
     * WARNING" @ 1420:4578). */
    return 0x10;
}

const char *sys_runtime_warn_title_first(void)
{
    /* Orig title @ 1420:44f7. */
    return "WARNING";
}

const char *sys_runtime_warn_body_first(void)
{
    /* Orig body @ 1420:4473: getting-low text (exact newlines TBD vs dump;
     * semantic 1:1). */
    return "Your memory resources are getting low. Try saving the game, "
           "closing other applications and restarting windows to recover "
           "memory.";
}

const char *sys_runtime_warn_title_final(void)
{
    /* Orig title @ 1420:4578. */
    return "FINAL WARNING";
}

const char *sys_runtime_warn_body_final(void)
{
    /* Orig body @ 1420:44ff: dangerously-low text (exact newlines TBD;
     * semantic 1:1). */
    return "Your memory resources are now dangerously low. Save the game "
           "and restart windows to prevent loss of the current game.";
}

int sys_runtime_check_order(void)
{
    /* Orig Game_TurnLoopTick @ 1088:0000 per-civ tail: Sys_CheckRuntimeMemory
     * (); then Civ_ProcessUnitMoves(civ) (return on nonzero for UI pump).
     * 1 = check runs BEFORE the unit-move pump. */
    return 1;
}
