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

/* Orig: FUN_1000_2928 @ 1000:2928 -> Util_IntToDec.
 * Integer -> decimal string for game_append_year_text @ 1088:1dd0
 * (called as FUN_1000_2928(..., 10): abs(g_nYear) + "BC"/"AD" onto the
 * 12d8:24fa line buffer). OS-free base-10 conversion (repeated div/mod
 * by 10, sign only for base 10); generic radix 2..36 via the same loop.
 * Returns dst (NULL when dst is NULL). */
char *util_int_to_dec(char *dst, int value, int base);

/* Orig: FUN_11d0_xxxx @ 11d0:xxxx -> Util_FormatMessage.
 * snprintf composer sibling to Util_StrCopy/Append @ 1000:36b2/363e +
 * IntToDec @ 1000:2928; needed by game_append_year_text @ 1088:1dd0 +
 * ResMgr_DumpStats @ 1190:17d0 via Debug_Print @ 1000:2c9c (see sys.h).
 * OS-free 1:1 port on explicit dst/cap via vsnprintf: formats fmt into
 * dst (cap bytes including the NUL, truncated safely). Returns the
 * vsnprintf count (would-be length), -1 on NULL dst/fmt or cap == 0.
 * No globals, no exit(), never writes game assets. */
int util_format_message(char *dst, size_t cap, const char *fmt, ...);

/* Orig: FUN_1000_1978 @ 1000:1978 -> Util_ToLower.
 * ASCII tolower used by ResMgr_CmpName @ 1190:0e72 (case-insensitive
 * 0x1f-char compare). OS-free: 'A'..'Z' -> +32, all else unchanged. */
int util_to_lower(int c);

#endif /* CIVLIN_COMMON_H */
