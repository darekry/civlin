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

void *util_mem_set_core(void *dst, int c, size_t n)
{
    /* Orig: FUN_1000_29c4 @ 1000:29c4 -> Util_MemSetCore. OS-free 1:1
     * (odd head + word fill + odd tail == memset). */
    if (!dst)
        return NULL;
    if (n == 0)
        return dst;
    return memset(dst, c, n);
}

void *util_mem_set(void *dst, int c, size_t n)
{
    /* Orig: FUN_1000_29ea @ 1000:29ea -> Util_MemSet (wrapper over
     * Util_MemSetCore @ 1000:29c4). Thin delegate. */
    return util_mem_set_core(dst, c, n);
}

void *util_mem_copy(void *dst, const void *src, size_t n)
{
    /* Orig: FUN_1000_299e @ 1000:299e -> Util_MemCopy (word loop +
     * odd byte == memcpy). */
    if (!dst)
        return NULL;
    if (n == 0)
        return dst;
    if (!src)
        return NULL;
    return memcpy(dst, src, n);
}

void *util_mem_move_core(void *dst, const void *src, size_t n)
{
    /* Orig: FUN_1000_2a30 @ 1000:2a30 -> Util_MemMoveCore
     * (overlap-aware direction + word loop == memmove). */
    if (!dst)
        return NULL;
    if (n == 0)
        return dst;
    if (!src)
        return NULL;
    return memmove(dst, src, n);
}

void *util_mem_move(void *dst, const void *src, size_t n)
{
    /* Orig: FUN_1000_2a89 @ 1000:2a89 -> Util_MemMove (wrapper over
     * Util_MemMoveCore @ 1000:2a30). Thin delegate. */
    return util_mem_move_core(dst, src, n);
}

char *util_str_copy_nul(char *dst, const char *src)
{
    /* Orig: FUN_1140_040e @ 1140:040e -> Util_StrCopyNul (copies *src
     * then loops while copied byte != 0 == strcpy incl. NUL). */
    if (!dst || !src)
        return NULL;
    return strcpy(dst, src);
}

char *util_pascal_to_c(char *pstr)
{
    /* Orig: FUN_1100_0053 @ 1100:0053 -> Util_PascalToCStr: B = *pstr
     * (length byte), then pstr[i] = pstr[i+1] for i in 0..B-1,
     * pstr[B] = 0. In-place, returns the same buffer. */
    unsigned char len;
    int i;

    if (!pstr)
        return NULL;
    len = (unsigned char)pstr[0];
    for (i = 0; i < (int)len; i++)
        pstr[i] = pstr[i + 1];
    pstr[len] = '\0';
    return pstr;
}

uint16_t util_byte_swap16(uint16_t x)
{
    /* Orig: FUN_1140_04b5 @ 1000:04b5 -> Util_ByteSwap16
     * ((x>>8 & 0xff) + x*0x100 == byte swap). */
    return (uint16_t)(((uint16_t)(x >> 8) & 0x00ffu) |
        (uint16_t)((uint16_t)(x << 8) & 0xff00u));
}

int util_swap_high16(uint32_t v)
{
    /* Orig: FUN_1140_045c @ 1140:045c -> Util_SwapHigh16
     * ((byte)(v>>0x18) + ((v>>8) & 0xff00)). Exact 1:1 incl. narrowing
     * to int return. */
    return (int)(((v >> 24u) & 0xffu) + ((v >> 8u) & 0xff00u));
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
    /* Orig: FUN_1000_0221 @ 1000:0221 -> Util_FormatFloat (%E/%F/%G core).
     * Dispatch folds fmt via param3 & 0xdf ('E' 0x45/'F' 0x46/'G' 0x47),
     * prec clamps to 0x28, digits come from Float_DecomposeExponent @
     * 1000:0754, Util_StripTrailingZeros @ 1000:01fe runs on the G path,
     * NUL lands @ 03aa. Linux 1:1 on explicit out/cap via snprintf: the
     * libc %e/%f/%g cores are the same Steele/Gay digit generation the
     * Borland loop reimplements (same rounding, same exponent shape),
     * prec < 0 selects the default 6 (orig param2 == 0 default path),
     * the G path additionally runs util_strip_trailing_zeros (no-op under
     * C %g, mirrors the orig pipeline). Output case follows fmt case
     * (lowercase fmt -> lowercase marker, as Borland/printf). Non-finite
     * spelling follows libc (INF/NAN tails vs OVERFLOW @ 1420:4ad6 +
     * Floating Point Trap @ 1420:50cb literals TBD vs dump, cf. sys.h
     * float group). NULL out / cap == 0 -> CIV_ERR_NOENT (orig faulted);
     * unknown fmt -> CIV_ERR_BAD_DATA; truncation -> CIV_ERR_TRUNCATED
     * (buffer still NUL-terminated, snprintf semantics). No globals, no
     * exit(), read-only vs sources. */
    char conv[8];
    const char *mark;
    int n;

    if (!out || cap == 0)
        return CIV_ERR_NOENT;
    out[0] = '\0';
    switch (fmt) {
    case 'E':
    case 'e':
        mark = (fmt == 'e') ? "%.*e" : "%.*E";
        break;
    case 'F':
    case 'f':
        mark = (fmt == 'f') ? "%.*f" : "%.*F";
        break;
    case 'G':
    case 'g':
        mark = (fmt == 'g') ? "%.*g" : "%.*G";
        break;
    default:
        return CIV_ERR_BAD_DATA;
    }
    if (prec < 0)
        prec = 6;
    if (prec > 0x28)
        prec = 0x28;
    memcpy(conv, mark, 5);
    n = snprintf(out, cap, conv, prec, val);
    if (n < 0) {
        out[0] = '\0';
        return CIV_ERR_BAD_DATA;
    }
    if (fmt == 'G' || fmt == 'g')
        util_strip_trailing_zeros(out, strlen(out), cap, 1);
    if ((size_t)n >= cap)
        return CIV_ERR_TRUNCATED;
    return CIV_OK;
}

int util_str_case_cmp(const char *a, const char *b)
{
    /* Orig FUN_1000_36de @ 1000:36de -> Util_StrCaseCmp (see civ_common.h).
     * Wraps the decompiler loop: raw byte equality fast path, else
     * lowercase fold for a-z on each side before compare. */
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    unsigned ca = 0;
    unsigned cb = 0;

    if (!pa)
        pa = (const unsigned char *)"";
    if (!pb)
        pb = (const unsigned char *)"";
    for (;;) {
        ca = *pa;
        cb = *pb;
        if (ca == 0)
            return (int)ca - (int)cb;
        if (ca != cb) {
            unsigned fa = ca;
            unsigned fb = cb;

            if (fa > 0x60u && fa < 0x7bu)
                fa -= 0x20u;
            if (fb > 0x60u && fb < 0x7bu)
                fb -= 0x20u;
            if (fa != fb)
                return (int)fa - (int)fb;
        }
        pa++;
        pb++;
    }
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

int util_compare_bounded(const char *a, const char *b, int bound)
{
    /* Orig FUN_1000_3744 @ 1000:3744 -> Util_CompareBounded (see
     * civ_common.h): SCASB NUL search in b within bound + CMPSB len
     * bytes vs a, case-sensitive, returns a-b of the first diff (0 =
     * match). Sole caller Font_EnumFamiliesCallback @ 1008:0e12 with
     * (LOGFONT+0x12, DS:0x11b "CIVILIZATION", 3) -> "CIV" prefix gate. */
    size_t blen = 0;
    size_t len = 0;
    size_t i = 0;

    if (bound <= 0)
        return 0;
    if (!a)
        a = "";
    if (!b)
        b = "";
    while (blen < (size_t)bound && b[blen] != '\0')
        blen++;
    len = blen;
    if (len < (size_t)bound)
        len++; /* NUL found inside bound: compare it too (SCASB incl). */
    for (i = 0; i < len; i++) {
        unsigned char ca = (unsigned char)a[i];
        unsigned char cb = (unsigned char)b[i];

        if (ca != cb)
            return (int)ca - (int)cb;
    }
    return 0;
}

size_t util_str_len(const char *s)
{
    /* Orig FUN_1000_3722 @ 1000:3722 -> Util_StrLen (see civ_common.h):
     * SCASB.REPNE length without NUL. Distinct Orig site from
     * Util_VsprintfStrLen @ 1000:19f2 (util_vsprintf_strlen), same
     * contract; the menu/ellipsis path uses this entry. */
    if (!s)
        return 0;
    return strlen(s);
}

char *util_str_rchr(const char *s, int c)
{
    /* Orig FUN_1000_37b2 @ 1000:37b2 -> Util_StrRChr (see civ_common.h):
     * strrchr semantics incl. the NUL match (c == 0 -> past-end). */
    if (!s)
        return NULL;
    return (char *)strrchr(s, c);
}

int32_t float_ftol(long double x)
{
    /* Orig Float_Ftol @ 1000:110d: FSTCW/OR chop/FLDCW + FISTP, low DX:AX.
     * C cast truncates toward zero identically for in-range inputs. */
    return (int32_t)x;
}

uint32_t u32_mul32_far(uint32_t a, uint32_t b)
{
    /* Orig U32_Mul32Far @ 1000:1368 (far twin of U32_Mul32 @ 1000:16fe). */
    return a * b;
}

int32_t s32_div32(int32_t a, int32_t b)
{
    /* Orig S32_Div32 @ 1000:1423 (DIV fast path + 0x20 loop, DI signs). */
    return a / b;
}

uint32_t u32_shl32(uint32_t v, unsigned c)
{
    /* Orig U32_Shl32 @ 1000:14d4 (SHL/SHR/XCHG paths). */
    if (c >= 32u)
        return 0u;
    return v << c;
}
