/* CivLin common types + Ghidra address annotations.
 *
 * Rules: C17, no exit() in libs, no globals, read-only vs civ1win/unpacked.
 * Ghidra target: civ.exe (NE Win3.x, Borland C++ 1991), program "civ.exe".
 */
#ifndef CIVLIN_COMMON_H
#define CIVLIN_COMMON_H

#include <stddef.h>
#include <stdint.h>

/* Ghidra segment helper: annotation only, e.g. GHIDRA_ADDR("1130:0075"). */
#define CIVLIN_GHIDRA(addr) /* Ghidra: addr */

typedef enum civ_error {
    CIV_OK = 0,
    CIV_ERR_IO = 1,
    CIV_ERR_NOMEM = 2,
    CIV_ERR_BAD_DATA = 3,
    CIV_ERR_TRUNCATED = 4,
    CIV_ERR_NOENT = 5,
    CIV_ERR_RANGE = 6,
    CIV_ERR_UNSUPPORTED = 7 /* known path, not yet decoded -> stub */
} civ_error_t;

const char *civ_strerror(civ_error_t err);

/* String builders used by every startup screen (dialog text buffers).
 * Ghidra: Util_StrCopy @ 1000:36b2 (strcpy semantics, returns dst),
 * Util_StrAppend @ 1000:363e (strcat semantics incl. dst return). */
char *util_str_copy(char *dst, const char *src);
char *util_str_append(char *dst, const char *src);

#endif /* CIVLIN_COMMON_H */
