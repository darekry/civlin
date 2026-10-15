/* Shared error strings. */
#include "civ_common.h"

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
