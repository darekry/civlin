/* CivLin common types + Ghidra address annotations.
 *
 * Rules (2026-10-16 odblokowanie): C17, no exit() in libs, read-only vs
 * civ1win/unpacked. Stan gry DOZWOLONY i WYMAGANY 1:1: DGROUP (DAT_1420_*,
 * DAT_12d8_*, DAT_13f8_0000 shared-static, stos dialogow, bufory map/save,
 * palety) przenosimy na jawne konteksty (struct + out/cap) albo — gdy
 * oryginal uzywal proces-global/static o semantyce nadpisywania — na
 * module-static z ta sama semantyke + adnotacja Orig adresu. NIE stubujemy
 * tylko dlatego, ze oryginal uzyl globala/__far/segmentu. STUB/PARTIAL
 * tylko dla mechaniki OS (USER/GDI/KERNEL plumbing, szczegoly w win16.h).
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

/* Orig: FUN_1000_01fe @ 1000:01fe -> Util_StripTrailingZeros.
 * Trailing-zero/dot strip behind Util_FormatFloat @ 1000:0221 %E/%F/%G:
 * when is_g_format != 0 ((DL & 0x5f) == 0x47 'G'/'g'), strips trailing
 * '0' (ES:[BX-1] == 0x30 while BX > CX bound); then (both paths, unless
 * the zero loop hit the bound) strips one trailing '.' (0x2e). Orig
 * communicates the new end via BX (caller NUL-terminates later @
 * 0221:03aa). Linux 1:1 on explicit buf/len (no ES:, no registers):
 * in-place strip + NUL, returns the new length. NULL buf -> 0 (orig
 * faulted); len > cap clamps to cap. No globals, no exit(). */
size_t util_strip_trailing_zeros(char *buf, size_t len, size_t cap,
                                 int is_g_format);

/* Orig: FUN_1000_0221 @ 1000:0221 -> Util_FormatFloat (%E/%F/%G core).
 * Borland sprintf float core (prec clamp 0x28, 'E' 0x45/'F' 0x46/'G'
 * 0x47 via param3 & 0xdf, sign, Float_DecomposeExponent @ 1000:0754
 * digits, Util_StripTrailingZeros @ 1000:01fe on the G path, NUL @
 * 03aa; special 0x7fff/0x7ffe tails reference nearby symbols as reloc
 * base, exact INF/NAN literals TBD vs dump, cf. OVERFLOW @ 1420:4ad6 +
 * Floating Point Trap @ 1420:50cb, sys.h float group). OS-free 1:1 port
 * on explicit out/cap via snprintf (same digit generation/rounding;
 * prec < 0 selects default 6; output case follows fmt case; G path runs
 * util_strip_trailing_zeros): CIV_OK on fit, CIV_ERR_TRUNCATED on
 * truncation (still NUL-terminated), CIV_ERR_NOENT on NULL out/cap 0,
 * CIV_ERR_BAD_DATA on unknown fmt. No globals, no exit(). */
civ_error_t util_format_float(char *out, size_t cap, double val,
                              char fmt, int prec);

/* Orig: FUN_1000_137f @ 1000:137f -> Util_MemCopyFar.
 * Far memcpy (Win16 HMEMCPY shape: LDS SI [BP+6] src, LES DI [BP+0x0a]
 * dst, CX byte count; CLD + SHR CX,1 + REP MOVSW + ADC CX,CX + REP
 * MOVSB; RETF 8). Used 116x (HoF_TryInsert @ 1080:0607, Config_SavePrefs
 * @ 1008:0bbf tails, dialog unit scaling @ 1098:0d43, status rect fetch
 * via GLOBALLOCK @ 1218:0956, etc.) as the struct/buffer copier.
 * OS-free 1:1 port: memcpy(dst, src, n). NULL dst -> NULL (orig faulted
 * on a bad far pointer); NULL src with n > 0 -> NULL; n == 0 returns
 * dst (no touch, even when src is NULL). No Win16, no globals, no
 * exit(), read-only vs sources. */
void *util_mem_copy_far(void *dst, const void *src, size_t n);

/* Orig: FUN_1000_29c4 @ 1000:29c4, now Util_MemSetCore in Ghidra.
 * Memset core (odd-address head byte, then CONCAT11 fill word loop via
 * SHR CX,1 REP MOVSW, then odd tail byte). OS-free 1:1 port: memset
 * semantics (fill byte c over n bytes). NULL dst -> NULL (orig faulted);
 * n == 0 returns dst untouched. No Win16, no globals, no exit(). */
void *util_mem_set_core(void *dst, int c, size_t n);
/* Orig: FUN_1000_29ea @ 1000:29ea, now Util_MemSet in Ghidra.
 * Memset wrapper (far dst + len + fill passthrough to Util_MemSetCore @
 * 1000:29c4, returns dst). Zeroes the OPENFILENAME struct in
 * save_prompt_open/save @ 1130:0075/0194 and 12d8:4334 (0x100 B) in
 * MainMenu_WndProc @ 11e0:0221. OS-free 1:1 port: same contract as the
 * core (thin delegate). No Win16, no globals, no exit(). */
void *util_mem_set(void *dst, int c, size_t n);
/* Orig: FUN_1000_299e @ 1000:299e, now Util_MemCopy in Ghidra.
 * Memcpy word loop (SHR CX,1 REP MOVSW + odd MOVSB; complement to
 * Util_MemCopyFar @ 1000:137f). Used by File_WriteBuffered @ 1000:2e85.
 * OS-free 1:1 port: memcpy semantics with the same NULL/n==0 contract
 * as util_mem_copy_far. No Win16, no globals, no exit(). */
void *util_mem_copy(void *dst, const void *src, size_t n);
/* Orig: FUN_1000_2a30 @ 1000:2a30, now Util_MemMoveCore in Ghidra.
 * Memmove core (overlap-aware direction pick dst<src, then word loop
 * forward/backward + odd byte). OS-free 1:1 port: memmove semantics.
 * Same NULL/n==0 contract. No Win16, no globals, no exit(). */
void *util_mem_move_core(void *dst, const void *src, size_t n);
/* Orig: FUN_1000_2a89 @ 1000:2a89, now Util_MemMove in Ghidra.
 * Memmove wrapper over Util_MemMoveCore @ 1000:2a30 (returns dst).
 * Same contract. No Win16, no globals, no exit(). */
void *util_mem_move(void *dst, const void *src, size_t n);
/* Orig: FUN_1140_040e @ 1140:040e, now Util_StrCopyNul in Ghidra.
 * Strcpy incl. NUL (copies *src then loops while the copied byte != 0;
 * no callers left in this binary — orphan helper). OS-free 1:1 port:
 * strcpy semantics, returns dst, NULL either -> NULL (orig faulted).
 * No Win16, no globals, no exit(). */
char *util_str_copy_nul(char *dst, const char *src);
/* Orig: FUN_1100_0053 @ 1100:0053, now Util_PascalToCStr in Ghidra.
 * Pascal->C string in place behind Save_SaveGame @ 1198:00a3 (follows
 * ResMgr_GetString @ 1100:078a fetching the default name from BLOB
 * 0x2da8): shifts bytes [1..len] down by one and NUL-terminates
 * (len = original byte 0). OS-free 1:1 port on the caller's buffer;
 * returns dst, NULL on NULL (orig faulted). No Win16, no globals,
 * no exit(). */
char *util_pascal_to_c(char *pstr);
/* Orig: FUN_1140_04b5 @ 1140:04b5, now Util_ByteSwap16 in Ghidra.
 * 16-bit byte swap ((x>>8 & 0xff) + x*0x100). Orphan helper (no xrefs).
 * OS-free 1:1 port. No Win16, no globals, no exit(). */
uint16_t util_byte_swap16(uint16_t x);
/* Orig: FUN_1140_045c @ 1140:045c, now Util_SwapHigh16 in Ghidra.
 * High-word byte fold ((v>>0x18 & 0xff) + ((v>>8) & 0xff00); i.e. swap
 * of the top two bytes (0xAABBCCDD -> 0xBBAA). Orphan helper (no xrefs).
 * OS-free 1:1 port preserving the exact expression incl. 32->int
 * truncation. No Win16, no globals, no exit(). */
int util_swap_high16(uint32_t v);

/* Orig: FUN_1000_36de @ 1000:36de -> Util_StrCaseCmp.
 * Case-insensitive C-string compare behind Res_CompareStringsCI @
 * 11f8:01af (far-ptr wrapper). Decompiler: byte loop while *a != 0 and
 * *a == *b (raw), else fold a-z (0x60 < c < 0x7b) via -0x20 each and
 * compare folded; returns unfolded/folded diff (u - v). NULL either ->
 * treated as empty ("") so the helper stays total; both NULL -> 0.
 * OS-free 1:1, no Win16, no globals, no exit(). */
int util_str_case_cmp(const char *a, const char *b);

/* Orig: FUN_1000_3744 @ 1000:3744 -> Util_CompareBounded.
 * Bounded case-SENSITIVE prefix compare (SCASB NUL search within bound +
 * CMPSB; 0 = match). Disasm: LES DI,[BP+0xa] (filter far ptr) + CX =
 * word [BP+0xe] (bound) + SCASB.REPNE (AL=0) + LDS SI,[BP+6] (candidate
 * far ptr) + CMPSB.REPE, returns last-byte diff (SI - DI). Len = bound
 * minus SCASB remainder = min(bound, strlen(filter)+1) incl. the NUL when
 * found inside bound; then that many bytes compared (same result as
 * strncmp(a,b,bound) with NULL-as-"" totality; bound <= 0 -> 0 via JCXZ,
 * no touch). NO case folding (unlike Util_StrCaseCmp @ 1000:36de above:
 * 0x60 < c < 0x7b fold there, raw bytes here).
 * Sole caller: Font_EnumFamiliesCallback @ 1008:0e12 (ENUMFUNC via
 * MAKEPROCINSTANCE from Font_EnumFamiliesInit @ 1008:0e99): pushes 0x3,
 * DS, 0x11b, LOGFONT seg, LOGFONT+0x12 off (ADD SP,0xa = 5 words), so
 * [BP+6] = LOGFONT+0x12 candidate (lfFaceName), [BP+0xa/+0xc] = DS:0x11b
 * filter far ptr (NOT a 0x11b bound: 0x11b is the DGROUP offset; DS:0x11b
 * reads "CIVILIZATION" @ 1420:011b, 12 B + NUL), [BP+0xe] = 3 = bound.
 * SCASB scans the filter for NUL within 3 (none: "CIV"), len = 3, CMPSB
 * checks the "CIV" prefix case-sensitively -> keeps only CIV* faces (all
 * 21 RT_FONT faces qualify; GDI order may differ from RT_FONT order, see
 * boot.h chain-13/14). "Mode 3" = bound 3 (prefix length), not a case
 * mode; no other mode value occurs (single call site).
 * Who else should call (match nazw? search?): NOBODY now — full NE xref
 * scan shows no other callers; the sibling string families use different
 * helpers by design: Res_CompareStringsCI @ 11f8:01af (case-insensitive
 * unbounded via Util_StrCaseCmp, a-z fold) + Res_ComparePascal @
 * 11f8:0207 (Pascal min-len memcmp + len diff, see rsc_loader.h
 * res_compare_pascal LIVE) + Res_AppendPascal @ 11f8:014a (Pascal join)
 * + Util_StrCopyN @ 1000:377e (bounded COPY with NUL pad, same SCASB
 * shape as here but copy, not compare). A future facename search reuses
 * this helper (same "CIV" prefix rule), it does NOT reuse the CI/Pascal
 * family. OS-free 1:1, no Win16, no globals, no exit(). */
int util_compare_bounded(const char *a, const char *b, int bound);

/* Orig: FUN_1000_3722 @ 1000:3722 -> Util_StrLen.
 * NUL-terminated byte length (SCASB.REPNE shape, same as
 * Util_VsprintfStrLen @ 1000:19f2 but the general string-family entry;
 * callers: Debug_PrintLine @ 1000:3088 length step before
 * File_WriteBuffered @ 1000:2e85, save path tail @ 1140:xxxx spec+8,
 * Text_PackPascal @ 1100:0000 len half via buf[0] = len, dialog extent
 * @ 1100:00ea width step via GETTEXTEXTENT @ 1608:0220). Verified ref
 * unpacked/civ.exe (NE, not packed civ1win/CIV.EXE). OS-free 1:1 port:
 * strlen semantics; NULL -> 0 (orig faulted on a bad far pointer).
 * No Win16, no globals, no exit(). Procedural menu text path (width
 * sum + ellipsis cap) uses this, never the Vsprintf-internal twin. */
size_t util_str_len(const char *s);

/* Orig: FUN_1000_37b2 @ 1000:37b2 -> Util_StrRChr.
 * Reverse byte search (SCASB-back shape; strrchr semantics): last
 * occurrence of (char)c in s, NUL counted as a match position (returns
 * past-end when c == 0), else NULL when absent. Sole in-tree user:
 * Sys_FatalAbort @ 1000:4124 caption kernel over '\\' 0x5c (see sys.h
 * sys_fatal_basename, now a thin delegate here). Verified ref
 * unpacked/civ.exe. OS-free 1:1 port; NULL s -> NULL (orig faulted).
 * No Win16, no globals, no exit(). */
char *util_str_rchr(const char *s, int c);

/* Orig: FUN_1000_110d @ 1000:110d -> Float_Ftol.
 * x87 chop (FSTCW/OR 0x0c/FLDCW + FISTP qword, low DX:AX). OS-free 1:1
 * via C truncation toward zero (callers feed in-range Pow results).
 * No Win16, no globals, no exit(). */
int32_t float_ftol(long double x);

/* Orig: FUN_1000_1368 @ 1000:1368 -> U32_Mul32Far.
 * Far twin of U32_Mul32 @ 1000:16fe (same MUL/JCXZ chain, RETF vs RET).
 * Low-32 DX:AX*CX:BX, sign-agnostic. OS-free 1:1. No globals. */
uint32_t u32_mul32_far(uint32_t a, uint32_t b);

/* Orig: FUN_1000_1423 @ 1000:1423 -> S32_Div32.
 * Signed 32/32->32 (DIV fast path + 0x20 shift/sub loop, DI sign flags).
 * OS-free 1:1 via int32_t /. Divisor 0 faults in orig (#DE), same UB
 * here (callers pass nonzero elemSize @ 1420:b4aa). No globals. */
int32_t s32_div32(int32_t a, int32_t b);

/* Orig: FUN_1000_14d4 @ 1000:14d4 -> U32_Shl32.
 * 32-bit SHL DX:AX by CL (<0x10 carry-via-BX, else XCHG path).
 * OS-free 1:1: v << c, 0 when c >= 32. No globals. */
uint32_t u32_shl32(uint32_t v, unsigned c);

/* --- Batch 2026-10-17 fala 2 (H+I): 1000 core/file pure kernels ---
 * Renamed this pass:
 *   Util_ItoaDec @ 1000:15fc, U32_Mul32 @ 1000:16fe,
 *   Util_ToLowerAlias @ 1000:194a (twin/dupe of util_to_lower: same
 *     ctype-table +0x20 shape as 194a; header previously misattributed
 *     Orig 1978 -> util_to_lower, correct tolower Orig is 194a),
 *   Util_ToUpper @ 1000:1978, Util_Stpcpy @ 1000:1e9c,
 *   Util_GetEnv @ 1000:28b6, Util_LongToDec @ 1000:2974.
 * All LIVE OS-free 1:1 (no Win16, no globals, no exit()). */

/* Orig Util_ItoaDec @ 1000:15fc (Util_FormatInt @ 1000:157a wrapper with
 * 0x61/signed=0/base=10; caller File_BuildTempPath @ 1000:1648
 * "<prefix><num>.$$$" step). Appends unsigned decimal to dst. NULL dst
 * or cap == 0 -> NOENT (orig faulted). */
civ_error_t util_itoa_dec(char *dst, size_t cap, unsigned value);

/* Orig U32_Mul32 @ 1000:16fe (near twin of U32_Mul32Far @ 1000:1368 far;
 * same MUL/JCXZ chain, RET vs RETF; low-32 DX:AX*CX:BX; callers
 * Rng_NextRaw @ 1000:1729 + Date_Convert @ 1000:386e +
 * Float_ParseFormat @ 1000:3ba0). */
uint32_t u32_mul32(uint32_t a, uint32_t b);

/* Orig Util_ToLowerAlias @ 1000:194a (ctype @ 1420:4baf bit4==A-Z =>
 * +0x20; callers FUN_1158_0000 menu map + Pic_PlayShared @ 1030:0000).
 * Behavior-identical twin of util_to_lower (correct tolower Orig is
 * 194a, not 1978); thin delegate kept so the RE map records both sites. */
int util_to_lower_alias(int c);

/* Orig Util_ToUpper @ 1000:1978 (twin of 194a with bit8==a-z => -0x20;
 * callers ResMgr_CmpName @ 1190:0e72 + strupr loops @ 1100:0b9d /
 * 11e8:09f7). 0xFFFF passthrough preserved. */
int util_to_upper(int c);

/* Orig Util_Stpcpy @ 1000:1e9c (Util_StrLen @ 1000:3722 + Util_MemCopy @
 * 1000:299e, returns NUL position for File_BuildTempPath @ 1000:1648
 * chaining). NULL either -> NULL (orig faulted). */
char *util_stpcpy(char *dst, const char *src);

/* Orig Util_GetEnv @ 1000:28b6 (SCASB strlen + CMPSB environ scan of
 * DAT_1420_4fc0 for "name="; caller Float_ParseFormat @ 1000:3ba0 with
 * "TZ"). NULL either / empty name -> NULL. */
const char *util_get_env(const char *name, const char *const *envp);

/* Orig Util_LongToDec @ 1000:2974 (Util_FormatInt @ 1000:157a long twin
 * of Util_IntToDec @ 1000:2928; generic radix 2..36, '-' iff base == 10
 * && val < 0; caller FUN_11e8:012a MESSAGEBOX builder). Bad base empties
 * dst. NULL dst -> NULL. */
char *util_long_to_dec(char *dst, long val, int base);

/* --- Batch 2026-10-17 fala 3 (M+N): sort + string + far-mem kernels ---
 * Renamed this pass:
 *   Sort_SwapElems @ 1000:30e2, Sort_QSortCore @ 1000:3110, Sort_QSort @
 *   1000:3396, Util_StrCmp @ 1000:3680, Util_StrStr @ 1000:37fe,
 *   Util_MemCopyFarDstFirst @ 1000:40aa, Util_MemSetFarCore @ 1000:40d0,
 *   Util_MemSetFar @ 1000:40f6. All LIVE OS-free 1:1. */

/* Orig Sort_SwapElems @ 1000:30e2 (swap *a<->*b, n bytes via SHR CX,1
 * word loop + odd MOVSB; n = DAT_1420_b4aa image; 6 callers in
 * Sort_QSortCore @ 1000:3110). Hidden size global -> explicit n. */
void sort_swap_elems(void *a, void *b, size_t n);

/* Orig Sort_QSortCore @ 1000:3110 (recursive quicksort core: n==2/3 fast
 * paths, median-of-3, partition, S32_Div32 @ 1000:1423 index math, tail
 * recursion on the smaller side; callers self x2 + Sort_QSort @
 * 1000:3396). Effect-identical port via libc qsort on explicit
 * base/n/size/cmp (hidden b4aa/b4ac/b4ae -> params + module-static
 * images kept for RE fidelity, overwritten next call). */
void sort_qsort_core(void *base, int n, int elem_size,
                     int (*cmp)(const void *, const void *));

/* Orig Sort_QSort @ 1000:3396 (entry: b4aa = size, b4ae/b4ac = cmp far;
 * size == 0 returns; callers Pal_KdBuild @ 10c0:023e size 2 +
 * City_ShowFindAndCenterDialog @ 1068:004c). Same libc-qsort effect on
 * explicit params. */
void sort_qsort(void *base, int n, int elem_size,
                int (*cmp)(const void *, const void *));

/* DAT_1420_b4aa/b4ac images: last Sort_QSort staging, valid until the
 * next call (same pattern as dos_errno_e0010/4e7a in sys.h). */
int sort_elem_size_image(void);
int sort_has_cmp_image(void);

/* Orig Util_StrCmp @ 1000:3680 (SCASB strlen(s2)+1 incl NUL + CMPSB REPE,
 * returns last-byte diff; callers City_ShowFind @ 1068:004c rescan +
 * Quiz @ 1178:0022 ordering; distinct from CompareBounded @ 1000:3744
 * bound/mode-3 and StrCaseCmp @ 1000:36de folding). NULL either -> "".
 */
int util_str_cmp(const char *s1, const char *s2);

/* Orig Util_StrStr @ 1000:37fe (empty needle -> hay; SCASB search +
 * CMPSB verify; sole caller FUN_11d0_022d launch-confirm loop). NULL
 * either -> NULL (orig faulted). */
char *util_str_str(const char *haystack, const char *needle);

/* Orig Util_MemCopyFarDstFirst @ 1000:40aa (far memcpy, dst-first:
 * LES DI,[BP+6] dst, LDS SI,[BP+0xa] src, REP MOVSW/MOVSB; twin of
 * Util_MemCopyFar @ 1000:137f src-first; 9 callers incl. Dlg templates
 * @ 1098:2a85/2c36/2def + Pal_FindNearest @ 10c0:0413). Same contract
 * as util_mem_copy_far. */
void *util_mem_copy_far_dst_first(void *dst, const void *src, size_t n);

/* Orig Util_MemSetFarCore @ 1000:40d0 (odd-head + CONCAT11 word loop;
 * twin of Util_MemSetCore @ 1000:29c4; sole caller 40f6). Same contract
 * as util_mem_set_core. */
void *util_mem_set_far_core(void *dst, int c, size_t n);

/* Orig Util_MemSetFar @ 1000:40f6 (far wrapper reordering (dst,fill,len)
 * -> core (dst,len,fill); callers Pal_QuantInit @ 10c0:013d x3). Thin
 * delegate, same contract. */
void *util_mem_set_far(void *dst, int c, size_t n);

#endif /* CIVLIN_COMMON_H */
