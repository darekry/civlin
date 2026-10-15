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
