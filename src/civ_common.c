/* Shared error strings. */
#include "civ_common.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char *civ_strerror(civ_error_t err)
{
    switch (err) {
    case CIV_OK: return "ok";
    case CIV_ERR_IO: return "i/o error";
    case CIV_ERR_NOMEM: return "out of memory";
    case CIV_ERR_BAD_DATA: return "bad data";
    case CIV_ERR_TRUNCATED: return "truncated";
    case CIV_ERR_NOENT: return "no such entry";
    case CIV_ERR_RANGE: return "out of range";
    case CIV_ERR_UNSUPPORTED: return "supported path, not yet decoded";
    default: return "unknown error";
    }
}

char *util_str_copy(char *dst, const char *src)
{
    /* Util_StrCopy @ 1000:36b2. */
    return strcpy(dst, src);
}

char *util_str_append(char *dst, const char *src)
{
    /* Util_StrAppend @ 1000:363e (orig uses word copies; strcat equiv). */
    return strcat(dst, src);
}

char *util_int_to_dec(char *dst, int value, int base)
{
    /* Util_IntToDec @ 1000:2928. Decompiler shape: repeated div/mod by
     * the radix with a reversed digit buffer, sign prefix only for
     * base 10, NUL-terminated. Base-10 is the game path (year text);
     * other radix 2..36 share the same loop. */
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char tmp[34];
    unsigned u;
    int neg = 0;
    size_t n = 0;
    size_t i;

    if (!dst)
        return NULL;
    if (base < 2 || base > 36)
        base = 10;
    if (value < 0 && base == 10) {
        neg = 1;
        /* Avoids INT_MIN negation overflow: -(v+1)+1 in unsigned. */
        u = (unsigned)(-(value + 1)) + 1u;
    } else {
        u = (unsigned)value;
    }
    if (u == 0) {
        tmp[n++] = '0';
    } else {
        while (u > 0) {
            tmp[n++] = digits[u % (unsigned)base];
            u /= (unsigned)base;
        }
    }
    i = 0;
    if (neg)
        dst[i++] = '-';
    while (n > 0)
        dst[i++] = tmp[--n];
    dst[i] = '\0';
    return dst;
}

int util_to_lower(int c)
{
    /* Util_ToLower @ 1000:1978: if 'A' <= c <= 'Z' return c + 32. */
    if (c >= 'A' && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

void *util_mem_copy_far(void *dst, const void *src, size_t n)
{
    /* Util_MemCopyFar @ 1000:137f: far REP MOVSW/MOVSB copy, CX bytes
     * src -> dst (see civ_common.h). Linux: memcpy with NULL guards
     * (orig faulted on a bad far pointer). */
    if (!dst)
        return NULL;
    if (n == 0)
        return dst;
    if (!src)
        return NULL;
    return memcpy(dst, src, n);
}

size_t util_strip_trailing_zeros(char *buf, size_t len, size_t cap,
                                 int is_g_format)
{
    /* Util_StripTrailingZeros @ 1000:01fe (see civ_common.h): G-path
     * strips '0's while BX > CX bound, then one '.'; non-G only the
     * dot. Linux: bound is the buffer start (never underflow) + cap. */
    if (!buf || cap == 0)
        return 0;
    if (len > cap)
        len = cap;
    if (is_g_format) {
        while (len > 0 && buf[len - 1] == '0')
            len--;
    }
    if (len > 0 && buf[len - 1] == '.')
        len--;
    if (len < cap)
        buf[len] = '\0';
    else if (cap > 0)
        buf[cap - 1] = '\0';
    return len;
}

civ_error_t util_format_float(char *out, size_t cap, double val,
                              char fmt, int prec)
{
    (void)val;
    (void)fmt;
    (void)prec;
    /* TODO: Orig FUN_1000_0221 @ 1000:0221 (Borland %E/%F/%G core via
     * Float_DecomposeExponent @ 1000:0754 + Util_StripTrailingZeros @
     * 1000:01fe; prec clamp 0x28; INF/NAN tails TBD vs dump, cf.
     * sys.h float group). Needs digit loop + jumptable decode. */
    if (out && cap > 0)
        out[0] = '\0';
    return CIV_ERR_UNSUPPORTED;
}

int util_format_message(char *dst, size_t cap, const char *fmt, ...)
{
    /* Util_FormatMessage @ 11d0:xxxx. snprintf composer sibling to
     * Util_StrCopy/Append @ 1000:36b2/363e + IntToDec @ 1000:2928;
     * feeds game_append_year_text @ 1088:1dd0 + ResMgr_DumpStats @
     * 1190:17d0 via Debug_Print @ 1000:2c9c. OS-free 1:1 port on
     * explicit dst/cap via vsnprintf (truncated safely, always NUL
     * terminated when cap > 0). No globals, no exit(). */
    va_list ap;
    int n;

    if (!dst || cap == 0 || !fmt)
        return -1;
    va_start(ap, fmt);
    n = vsnprintf(dst, cap, fmt, ap);
    va_end(ap);
    return n;
}
