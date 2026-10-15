/* DOS / system helpers + RNG (SDL3/Linux port).
 *
 * Ghidra anchors (civ.exe), renamed 2026-10-15 (Orig FUN_* -> New):
 *   Dos_GetCurrentDrive      @ 1000:1252 (INT 21h AH=19h; MOV AH,0x19 / INT
 *                              0x21 / INC AX; 1-based drive number)
 *   Sys_GetCurrentDrive      @ 1000:11bb (wrapper; WinMain stores result to
 *                              g_nCurrentDrive @ 1228:0002)
 *   Dos_GetCurrentDir        @ 1000:139c (INT 21h AH=47h; DL=drive, DS:SI=buf;
 *                              0 ok / errno-mapped nonzero on carry)
 *   Dos_GetCurrentDriveIndex @ 1000:13e6 (0-based; Sys_GetCurrentDrive - 1)
 *   Sys_BuildCurrentDirPath  @ 1000:2826 (builds "D:\curdir" from drive letter
 *                              + Dos_GetCurrentDir into g_szBasePath @
 *                              1228:0004, max 0x100; NULL + 1420:0010=8/0x22
 *                              on alloc/overflow failure)
 *   Rng_Seed                 @ 1000:1716 (state hi=0, lo=param @ 1420:4ee0)
 *   Rng_NextRaw              @ 1000:1729 (Borland LCG: CX=[4ee2], BX=[4ee0],
 *                              DX:AX=0x015A4E35 x seed + 1; returns hi&0x7FFF)
 *   U32_Mul32                @ 1000:16fe (Borland 32-bit multiply helper;
 *                              low 32 bits of DX:AX * CX:BX; inlined below)
 *   Rng_Init                 @ 11c8:0000 (seed = Sys_GetTickScaled & 0x7FFF,
 *                              forced nonzero, via Rng_Seed)
 *   Rng_Range                @ 11c8:003d (rand15 * n >> 15)
 *   Util_Clamp               @ 11c8:0063 (3-arg clamp)
 *   Sys_GetTickScaled        @ 1100:02e1 (GETTICKCOUNT * 0x3C / 1000)
 *   Sys_Yield                @ 1098:0157 (wraps Anim_PumpIfActive @ 10b0:0000;
 *                              called in every long gen/AI loop)
 *   Sys_CheckRuntimeMemory   @ 11e8:0075 (per-civ warning in Game_TurnLoopTick:
 *                              GETFREESPACE <1000000/300000 + system resources
 *                              <10/8 -> MESSAGEBOX 1420:4473/44ff, once each)
 *   Sys_CheckFreeMemory      @ 11e8:0000 (boot check; see boot.h)
 *
 * Linux adaptations: no drive letters, so Dos_GetCurrentDrive returns 0.
 * That makes every `0 < g_nCurrentDrive` guard skip its DOS-only branch,
 * which is the correct Linux behaviour (verified guard sites: Sound_PlayId
 * @ 1150:00c9, Config_SavePrefs @ 1008:0bbf, both via Dos_DriveReady @
 * 1000:1184 + Dos_SetPath @ 1000:116a). Path building uses getcwd().
 */
#ifndef CIVLIN_SYS_H
#define CIVLIN_SYS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "civ_common.h"

/* --- RNG (Borland LCG; bit-exact with the original) --- */

/* Rng_Seed @ 1000:1716. */
void rng_seed(unsigned seed);
/* Rng_NextRaw @ 1000:1729. Advances state, returns (state>>16)&0x7FFF. */
unsigned rng_next_raw(void);
/* Rng_Range @ 11c8:003d. (rand15 * n) >> 15; 0 for n <= 0. */
unsigned rng_range(int n);
/* Rng_Init @ 11c8:0000. Seeds from sys_get_tick_scaled(), never zero. */
void rng_init(void);

/* --- small utils --- */

/* Util_Clamp @ 11c8:0063. */
int util_clamp(int v, int lo, int hi);

/* Orig: FUN_1000_29ea @ 1000:29ea -> Util_Progress.
 * Continent-link progress percent (orig scales the counter to the 0x1248
 * progress-bar range). OS-free 1:1 port: done * 100 / total, clamped to
 * 0..100; 0 when total <= 0 or done <= 0, 100 when done >= total. Pure
 * counter/total calc, no Win16, no globals, no exit(). */
int util_progress(int done, int total);

/* Orig: FUN_1000_2c9c @ 1000:2c9c -> Debug_Print.
 * Debug-dump print behind ResMgr_DumpStats @ 1190:17d0. OS-free 1:1 port:
 * fprintf wrapper (vfprintf of fmt to stdout, stderr when fmt is NULL-safe
 * guarded). Returns bytes written, -1 on NULL fmt or output error. No
 * globals, no exit(), never writes game assets. */
int debug_print(const char *fmt, ...);

/* --- DOS drive/dir (adapted; see header note) --- */

/* Dos_GetCurrentDrive @ 1000:1252. Always 0 on Linux (no drives). */
int dos_get_current_drive(void);
/* Dos_GetCurrentDriveIndex @ 1000:13e6. Always -1 on Linux. */
int dos_get_current_drive_index(void);
/* Dos_GetCurrentDir @ 1000:139c. getcwd() without the leading '/',
 * drive ignored. 0 on success, -1 on error (buf too small etc.). */
int dos_get_current_dir(int drive, char *buf, size_t cap);

/* --- system --- */

/* Sys_GetTickScaled @ 1100:02e1. SDL_GetTicks() * 60 / 1000. */
unsigned long sys_get_tick_scaled(void);
/* Sys_BuildCurrentDirPath @ 1000:2826. getcwd() into dst (cap-checked).
 * Returns dst on success, NULL on failure. */
char *sys_build_current_dir_path(char *dst, size_t cap);
/* Path/drive setup (WinMain prologue). Fills the statics below. */
civ_error_t sys_init_paths(void);
/* g_nCurrentDrive @ 1228:0002 (0 on Linux). */
int sys_current_drive(void);
/* g_szBasePath @ 1228:0004 (256 B). Valid after sys_init_paths(). */
const char *sys_base_path(void);
/* Sys_Yield @ 1098:0157. Pumps the OS message queue (SDL_PumpEvents). */
void sys_yield(void);
/* Orig: FUN_1108_0000 @ 1108:0000 -> Sys_PumpMessages.
 * Win16 PEEKMESSAGE pump called from unit_wait_for_orders @ 11c0:0ec2
 * (blink-and-pump modal until 11e8:02fb input or 1328:0012 mode change)
 * and alongside the 28 sys_drain_input call sites. Linux equivalent:
 * SDL_PumpEvents. Returns 1 when a message was dispatched, 0 when the
 * queue was empty. Distinct from sys_yield (Anim pump @ 10b0:0000) and
 * sys_drain_input (state clear + queue drain, see game.h). */
int sys_pump_messages(void);
/* Orig: FUN_1218_0956 @ 1218:0956 / FUN_1218_0a06 @ 1218:0a06 family ->
 * Sys_GetClientRect (see map.h wdwstatus_calc_upper/lower_rect).
 * Client-rect helper behind the status frame calculators (w_out, h_out
 * via GLOBALLOCK on the status-port handle + Util_MemCopyFar @ 1000:137f
 * struct copy, see civ_common.h util_mem_copy_far, then SETRECT/
 * INFLATERECT/GLOBALUNLOCK out). NOTE: 1000:137f is NOT the rect helper
 * (verified 2026-10-15: LDS SI/LES DI + REP MOVSW/MOVSB far memcpy, 116
 * xrefs) — it only serves the copy step inside 1218:0956. Linux
 * equivalent TBD (SDL_GetWindowSize on the status view). Params are the
 * client width/height outputs; NULL either -> NOENT (orig faulted on a
 * bad far pointer). Zeroes outputs then stub CIV_ERR_UNSUPPORTED until
 * the SDL status-view mapping lands. */
civ_error_t sys_get_client_rect(int *w_out, int *h_out);
/* Orig: FUN_1138_0269 @ 1138:0269 -> Status_MeasureRow.
 * GETTEXTMETRICS variant in the status_font_height @ 1098:0dda chain
 * (derives the status-row line stride kept as local_8, x2 for the civ
 * block; callers: 1138 row formatters + status panel @ 1088:1460 +
 * Unit_ShowInfoPopup @ 11f0:4754 + Dialog_MenuZ_Modal @ 1220:0b88 via
 * status_font_height). Linux equivalent TBD (SDL_ttf measurement;
 * SDL_GetText stub). Returns CIV_OK + *h_out on success. */
civ_error_t status_measure_row(int *h_out);
/* Sys_CheckRuntimeMemory @ 11e8:0075. UNSUPPORTED (needs window + RAM
 * thresholds; SDL_GetSystemRAM equivalent TBD). */
civ_error_t sys_check_runtime_memory(void);
/* Orig: FUN_11e8_0000 @ 11e8:0000 -> Sys_CheckFreeMemory.
 * Boot-time free-memory check (GETFREESPACE against the required floor,
 * MESSAGEBOX with the 1420:43b8 text on shortfall). Linux equivalent TBD
 * (SDL_GetSystemRAM mapping, no Win16 selector). Stub
 * CIV_ERR_UNSUPPORTED; owns the check, boot_check_memory (see boot.h)
 * delegates to it. No globals, no exit(). */
civ_error_t sys_check_free_memory(void);

/* --- Packet E: Win16 file/resource imports (KERNEL) ---
 * File map (with save.h + rsc_loader.h):
 *   _LOPEN  -> file_open_wrapper  (save.h, FUN_1000_2ad9 @ 1000:2ad9)
 *   _LCLOSE -> file_close_wrapper (save.h, FUN_1000_1f7c @ 1000:1f7c)
 *   _LLSEEK -> file_seek_wrapper  (save.h, FUN_1000_xxxx @ 1000:xxxx)
 *   _LREAD/_HREAD   -> file_read_chunk  (rsc_loader.h, FUN_1000_272a
 *     @ 1000:272a; streaming read behind ResMgr_ReadU16Disk @ 1190:1abe /
 *     U16Disk2 @ 1190:1b47 / U32Disk @ 1190:1bcb)
 *   _LWRITE/_HWRITE -> file_write_chunk (below; streaming write behind
 *     save_file_write @ 1140:0245 chunked path)
 * Resource map (KERNEL resource funcs -> rsc_loader.h ResMgr layer):
 *   FINDRESOURCE  -> rsc_find_type / rsc_get_by_id / rsc_get_by_name
 *   LOADRESOURCE  -> rsc_load_to_mem (stub @ 1190:0fda; _LLSEEK+_HREAD)
 *   LOCKRESOURCE  -> res->payload after load (no selector on Linux)
 *   UNLOCKRESOURCE/FREERESOURCE -> rsc_mgr_dispose/free (payload free +
 *     loaded/handle clear; see ResMgr_ZapRes @ 1190:10e6, ResMgr_Dispose
 *     @ 1190:0c92, ResMgr_Release @ 1190:113f). */

/* Orig: FUN_1000_xxxx @ 1000:xxxx -> File_WriteChunk (packet E).
 * Thin _HWRITE/_LWRITE wrapper complementing File_ReadChunk @ 1000:272a
 * (file_read_chunk in rsc_loader.h). Linux equivalent: fwrite(src, 1,
 * len, fp); returns bytes actually written (short on error/disk-full,
 * 0 on NULL fp/src or len == 0). */
size_t file_write_chunk(FILE *fp, const uint8_t *src, size_t len);

/* Orig NE entry table (MZ e_lfanew @ 0x3C + NE magic @ ne_off, entry table
 * @ NE+0x04/0x06, bundle walk) -> ne_entry_count (AH2). OS-free file-scan
 * read-only port (fopen "rb", LE16/LE32 reads, fseek/ftell bounds, no
 * writes, never writes unpacked/civ.exe or civfonts.fon): sums bundle counts
 * until cnt == 0 terminator (fixed 3 B vs moveable 6 B with seg 0xFF =
 * flags + CD 3F + seg + off LE, e.g. unpacked/civ.exe 11 moveable @ 0x8b4
 * len 70, unpacked/civfonts.fon 0 @ 0x3c7 len 2). Returns total entries
 * (0 for font-only NE) or -1 on NULL path / open / short / bad MZ/NE /
 * table overrun. No globals, no exit(), C17. */
int ne_entry_count(const char *path);

/* Orig NE entry table (same MZ e_lfanew @ 0x3C / NE magic / bundle anchors
 * as ne_entry_count) -> ne_entry_addr (AH2). OS-free file-scan read-only
 * port (same fopen "rb" + LE reads, no writes): 0-based index -> seg:off
 * via bundle walk (fixed: seg = bundle seg, off = u16 LE @ entry+1;
 * moveable 0xFF: seg = byte 3, off = u16 LE @ +4, e.g. civ.exe idx 0 ->
 * seg 2 off 0x0e12). NULL path/outs or index < 0 -> -1; header/table errors
 * -> -1 (outs untouched); index >= count -> -1 OOR (outs untouched); else
 * sets *seg_out and *off_out and returns 0. No globals, no exit(), C17,
 * read-only. */
int ne_entry_addr(const char *path, int index,
                  uint16_t *seg_out, uint16_t *off_out);

/* --- Sequential 1000:00b2-0221 CRT exit + FPU (2026-10-15, po kolei) ---
 * Entry @ 1000:0000 calls Sys_RunInitTable (min-first) for init, then
 * WinMain @ 1008:0000, then Sys_ExitFallback -> Sys_DoExit -> Dos exit.
 * Float group (FPU x87) feeds Util_FormatFloat %E/%F/%G via
 * Float_DecomposeExponent + Util_StripTrailingZeros. OS-free parts
 * (table scan, zero-strip, noops) are LIVE 1:1 with explicit params (no
 * ES:, no far, no INT 21h, no FPU CW); DOS/FPU tails stay STUB
 * CIV_ERR_UNSUPPORTED with TODO Orig. No globals, no exit(), C17. */

/* 6-B table entry behind Sys_RunInitTable/ExitTable (orig ES:BX stride 6:
 * flag i8 @ +0 (0xff = consumed/skip), prio u8 @ +1, fn far ptr @ +2).
 * Linux uses a native fn pointer (far/near CALLF/CALL distinction
 * dropped; both become fn()); code_seg (0x1000 in orig when flag != 0)
 * is passed through for fidelity but may be ignored by the hook. */
typedef void (*sys_table_fn_t)(unsigned code_seg);

typedef struct sys_table_entry {
    int8_t flag; /* -1 (0xff) = consumed/skip, else pending (0 = far path) */
    uint8_t prio; /* priority byte @ +1 (min-first init, max-first exit) */
    sys_table_fn_t fn; /* far fn @ +2 (NULL = skip) */
} sys_table_entry_t;

/* Orig: FUN_1000_00de @ 1000:00de -> Sys_RunInitTable (min-first).
 * OS-free 1:1 (LIVE): repeatedly scans table[0..n) for the pending
 * (flag != -1, fn != NULL) entry with the smallest prio, marks flag =
 * -1, calls fn(0x1000), until none remains. Entry @ 1000:0000 calls it
 * for init. Returns run count, -1 on NULL table with n > 0. Empty
 * (n == 0) returns 0. Table is caller-owned (may be mutated: flags). */
int sys_run_init_table(sys_table_entry_t *table, size_t n);

/* Orig: FUN_1000_0122 @ 1000:0122 -> Sys_RunExitTable (max-first).
 * OS-free 1:1 (LIVE): same as above but picks the largest prio first
 * (exit reverse order). Called from Sys_RunExitTableSingle @ 1000:00b2
 * (fixed DGROUP 0x5140-0x5146, 1x6B entry flag 0/prio 1) and Sys_DoExit
 * @ 1000:127f. Same return contract. */
int sys_run_exit_table(sys_table_entry_t *table, size_t n);

/* Orig: FUN_1000_00b2 @ 1000:00b2 -> Sys_RunExitTableSingle.
 * Fixed-table wrapper: ES=DS, SI=0x5140, DI=0x5146 (DGROUP, 1x6B entry:
 * 00 01 A7 41 00 00 = flag 0/prio 1/fn reloc @ 1420:5140) -> Sys_RunExit
 * Table @ 1000:0122. Table relocation (seg fixup at load) TBD -> stub
 * CIV_ERR_UNSUPPORTED. No globals, no exit(). */
civ_error_t sys_run_exit_table_single(void);

/* Orig: FUN_1000_00c4 @ 1000:00c4 -> Sys_NoopExitHook1 (empty RETF).
 * Orig: FUN_1000_00c5 @ 1000:00c5 -> Sys_NoopExitHook2 (empty RETF).
 * Empty hooks called from Sys_DoExit @ 1000:127f between the atexit
 * drain and Dos_ExitProcess. Intentionally empty (void, NOOP). */
void sys_noop_exit_hook1(void);
void sys_noop_exit_hook2(void);

/* Orig: FUN_1000_00c6 @ 1000:00c6 -> Dos_ExitProcess (AL=[BP+4],
 * AH=0x4c INT 0x21; fallback PUSH 0xff -> Sys_ExitFallback @ 1000:12d6
 * -> second INT 0x21). DOS process exit -> no exit() in libs per rules;
 * stub CIV_ERR_UNSUPPORTED. Param is the DOS exit code. */
civ_error_t dos_exit_process(int exit_code);

/* Orig: FUN_1000_127f @ 1000:127f -> Sys_DoExit (exit dispatcher).
 * Drains atexit stack @ DAT_1420_4bac (x4-0x4be8 indirect calls) while
 * nonzero, then Sys_RunExitTableSingle @ 1000:00b2 + indirect
 * DAT_1420_4cb0, then Sys_NoopExitHook1/2 @ 1000:00c4/00c5, then
 * (param2 == 0, param3 == 0) indirect DAT_1420_4cb4 and DAT_1420_4cb8,
 * then Dos_ExitProcess @ 1000:00c6. Touches DGROUP globals + INT 21h
 * -> stub CIV_ERR_UNSUPPORTED. Params mirror the decompiler
 * (exit_code, flags); exact flag semantics TBD. */
civ_error_t sys_do_exit(int exit_code, int flags);

/* Orig: FUN_1000_12d6 @ 1000:12d6 -> Sys_ExitFallback (wrapper).
 * Calls Sys_DoExit(exit_code, 0, 0). Called from Dos_ExitProcess
 * fallback and entry @ 1000:0000 tail. Same stub contract. */
civ_error_t sys_exit_fallback(int exit_code);

/* Orig: FUN_1000_0164 @ 1000:0164 -> Float_CheckRange (FPU range).
 * x87 helper behind Float_StoreConverted @ 1000:06cf (2 call sites):
 * FSTCW/OR 0x0c00(chop)/FLDCW, FLD extended [BP+0xc], range compare vs
 * AX 0x43fe/0x407e + BX 0x3bcd/0x3f6a + DX sign handling (SHL/RCL/SHR),
 * sets DAT_1420_0010 = 0x22 on overflow/underflow (cf. OVERFLOW @
 * 1420:4ad6, Floating Point Trap @ 1420:50cb). x87 CW + DGROUP error
 * word have no Linux equivalent here -> stub CIV_ERR_UNSUPPORTED. */
civ_error_t float_check_range(void);

/* Orig: FUN_1000_06cf @ 1000:06cf -> Float_StoreConverted.
 * FPU store dispatcher (SI flags @ [BP+8]): bit 2 set -> Float_Check
 * Range(DAT_1420_4a92, ST0, 1) + FSTP double; bit 3 set -> direct FSTP
 * extended; else Float_CheckRange(DAT_1420_4b06, ST0, 0) + FSTP float.
 * x87 stack args -> stub CIV_ERR_UNSUPPORTED. */
civ_error_t float_store_converted(void);

/* Orig: FUN_1000_0754 @ 1000:0754 -> Float_DecomposeExponent.
 * Float->digits exponent dispatch behind Util_FormatFloat @ 1000:0221:
 * LES DI [BP+0x10] out, sign save/clear via 0x7fff mask on ES:[BX+DI],
 * index = (BX>>2)<<1, JMP CS:[BX+0x784] jumptable (too many branches
 * for the decompiler). Gap 1000:0784-0967 is the LINEAR continuation
 * (verified 2026-10-15: no FUN_1000_08* exists, single RET 0x10 @ 0967):
 * 0784-07a7 FLD float/double/extended loads, 07a7-0803 FXAM/FSTSW
 * class dispatch (DX=1/7ffe/7fff), 080a-0844 log10 approx
 * ((exp-0x3fff)*0x4d10 + mantissa*0x4d>>...), 0846-0871 scale loop via
 * Float_Pow10Far @ 1000:0e20 (clamp 0x1344, FDIVP/FMULP, labels
 * Decompose_ScaleLoop/Pow10Call1-3), 0871-08be FIDIV/FIMUL refine,
 * 08ce CALL Float_ConvertExtendedToBcd @ 1000:0988 (label
 * Decompose_BcdConvertCall), 08d3-0952 BCD->ASCII digits. Exponent
 * table TBD -> stub CIV_ERR_UNSUPPORTED. */
civ_error_t float_decompose_exponent(void);

/* Orig: FUN_1000_03b6 @ 1000:03b6 -> Sys_NoopPad (PUSH BP / MOV BP,SP /
 * POP BP / RET; padding after Util_FormatFloat @ 1000:0221). Empty
 * near RET like Sys_NoopExitHook1/2 @ 1000:00c4/00c5. Intentionally
 * empty (void, NOOP). No globals, no exit(). */
void sys_noop_pad(void);

/* Orig: FUN_1000_06c7 @ 1000:06c7 -> Float_DiscardSt0 (PUSH BP / MOV
 * BP,SP / WAIT / FSTP ST0 / POP BP / RET: discards the x87 stack top).
 * Linux C ABI has no x87 register stack, so there is no counterpart;
 * documented NOOP void (never fakes a computation). No globals,
 * no exit(). */
void float_discard_st0(void);

/* Orig: FUN_1000_096a @ 1000:096a -> Float_PackBcdChunk (DX in 0..9999,
 * AX/CX scratch saved via PUSH/POP, ES:DI out via STOSW; DIV DL by 100
 * (AL=0x64), AAM 0xa + SHL AH,4 + OR per half, XCHG, second AAM pack,
 * MOV AH,DH, STOSW 2 BCD bytes, DI += 2). Linux single-byte primitive
 * of the same AAM-pack step: packs 0..99 into one BCD byte
 * ((v / 10) << 4 | (v % 10)); input reduced mod 100 first. The orig
 * 2-byte STOSW is two such packs (DX / 100 high, DX % 100 low);
 * Float_ConvertExtendedToBcd @ 1000:0988 calls it 4x for 8 bytes plus
 * a 1-byte AAM tail. OS-free 1:1, no globals, no exit(). */
uint8_t float_pack_bcd_chunk(uint16_t dx);

/* Orig: FUN_1000_0988 @ 1000:0988 -> Float_ConvertExtendedToBcd (FSTP
 * extended ES:[DI], exponent AX=[DI+8] & 0x7fff vs 0x3ffe then -0x3c,
 * mantissa BX=[DI] CX=[DI+2] SI=[DI+4] DX=[DI+6] vs magics
 * 0xde0b/0x6b3a/0x763f/0xfff0 (CMP BX,-0x10 JA = overflow when
 * BX > 0xfff0), small -> 9x0x00 fill, large -> 9x0x99 fill, else
 * normalize shifts + DIV SI by 10000 (SI=0x2710) chain with 4x CALL
 * Float_PackBcdChunk @ 1000:096a (8 BCD bytes) + AAM tail STOSB (byte
 * 8) + sign byte ([SI+9] & 0x80) STOSB (byte 9). Linux 1:1 on
 * explicit (long double x, uint8_t bcd_out[10]): same threshold
 * branches (e1 = exp - 0x3ffe, e2 = e1 - 0x3c, magic compare), same
 * 9 + sign layout via the pack helper + integer /% 10000 groups.
 * NULL bcd_out is a no-op (orig faulted). No globals, no exit(). */
void float_convert_extended_to_bcd(long double x, uint8_t bcd_out[10]);

/* Orig: FUN_1000_0c03 @ 1000:0c03 -> Float_Log2 (x87 FYL2X core with
 * FLDLN2/FXCH/FXAM/FSTSW/SAHF dispatch: x == 0 or x < 0 falls through
 * to the NaN/INF constant loads @ CS:0xa79/0xa7d, x == 1.0
 * (exp == 0x3fff && hi mantissa == 0x8000) takes log2((x - 1) + 1) via
 * FLD1/FSUBP/FYL2XP1 to avoid precision loss, else FYL2X). Linux 1:1
 * via log2(): x == 1.0 returns 0.0 directly (same FYL2XP1 value
 * without the x87 round trip); x <= 0 outside the orig contract
 * returns -HUGE_VAL (0) / NAN (negative) exactly like log2(). No
 * globals, no exit(). */
double float_log2(double x);

/* Orig: FUN_1000_0c7d @ 1000:0c7d -> Float_Exp2 (x87 2^x core: FLDL2E
 * log2(e) = 1.4426950408889634 @ 1000:0c7e times x, ABS, 0.5 compare
 * vs float CS:0xa81, FSCALE/FRNDINT/FIST split, F2XM1 + 1, odd-bit
 * multiply by _DAT_1000_0a85 = sqrt(2) 85 64 DE F9 33 F3 04 B5 FF 3F,
 * FILD/FSCALE reassemble; zero/INF/NaN paths via CS:0xa8f/0xa79).
 * Linux 1:1 via exp2() (same 2^x, same INF/overflow semantics).
 * FLOAT_LOG2E below keeps the orig FLDL2E constant for fidelity. No
 * globals, no exit(). */
double float_exp2(double x);

/* Orig FLDL2E constant @ 1000:0c7e (log2(e) used by Float_Exp2 @
 * 1000:0c7d: x * log2(e) before the ABS/FSCALE/F2XM1 split). */
#define FLOAT_LOG2E 1.4426950408889634

/* --- Batch FPU-error: matherr + pow/scan (2026-10-16) ---
 * Borland matherr chain (civ.exe, seg 1000): Float_ReportError @ 1000:1056
 * builds a Borland `struct exception` on the stack (type i16 @ +0, func
 * name far ptr @ +2, arg1/arg2/retval doubles @ +6/+14/+22 == 0x16) with
 * NULL doubles -> 0.0 (FLDZ), then Float_MatherrFilter @ 1000:0e2e, then
 * Util_Sprintf @ 1000:35f6 ("%s: %s error\n" @ 1420:4b9e, func "pow" @
 * 1420:4b96/4b9a, err names [BX+0x4aae] DOMAIN/SING/OVERFLOW/UNDERFLOW/
 * TLOSS/PLOSS), errno DAT_1420_0010 = 0x22 (ERANGE) for types 3/4 else
 * 0x21 (EDOM), then Sys_FatalAbort @ 1000:4124 (MESSAGEBOX), returning
 * retval in ST0. OS-free parts are LIVE 1:1 with explicit params (no
 * far, no DGROUP, no Win16); Win16 tails stay STUB CIV_ERR_UNSUPPORTED
 * with TODO Orig. No globals (name table is a switch, not storage),
 * no exit(), C17. <math.h> only in sys.c.
 *
 * Chain notes (verified 2026-10-16):
 * - 1000:0e27 is NOT a function: it is the inner `CALL 1000:0d56`
 *   inside Float_Pow10Far @ 1000:0e20 (sole xref of Float_Pow10).
 * - 1000:0605 is the `CALL 1000:0e20` inside Float_ScanFloat @
 *   1000:03bc; 1000:0857/0871/089e are three more `CALL 1000:0e20`
 *   in the orphan gap 1000:0784-0969 (DecomposeExponent jumptable
 *   targets: exponent clamp 0x1344, FIDIV/FIMUL scaling, then CALL
 *   Float_ConvertExtendedToBcd @ 1000:0988 at 1000:08ce). No new
 *   functions created there.
 * - 1190 callers of Float_Pow are false merges (resource-manager
 *   segment); IGNORED here, noted so nobody re-triages them. */

/* Borland `struct exception` (stack-built in Float_ReportError @
 * 1000:1056): type 1..6 (DOMAIN/SING/OVERFLOW/UNDERFLOW/TLOSS/PLOSS),
 * name = func name ("pow"), arg1/arg2/retval doubles. */
typedef struct float_matherr {
    int type;
    const char *name;
    double arg1;
    double arg2;
    double retval;
} float_matherr_t;

/* ScanFloat callbacks (orig near CALL [BP+4] get / [BP+6] unget with
 * two context words [BP+8]/[BP+0xa]; Linux merges them into ctx).
 * get returns next char or -1 on EOF; unget pushes one char back. */
typedef int (*float_getchar_fn)(void *ctx);
typedef void (*float_ungetchar_fn)(int c, void *ctx);

/* Orig: FUN_1000:0e2e @ 1000:0e2e -> Float_MatherrFilter (LIVE).
 * Returns 1 = suppress (skip message): type 4 zeroes e->retval first
 * (FSTP double ES:[BX+0x16]), type 5 as-is; else 0 = report. NULL e
 * -> 0 (orig faulted). */
int float_matherr_filter(float_matherr_t *e);

/* Orig error-name table [BX+0x4aae] (6 far ptrs -> DOMAIN/SING/
 * OVERFLOW/UNDERFLOW/TLOSS/PLOSS @ 1420:4aca+). Switch (no globals);
 * out-of-range type -> "UNKNOWN". */
const char *float_matherr_name(int type);

/* Orig: FUN_1000:35f6 @ 1000:35f6 -> Util_Sprintf (PARTIAL).
 * sprintf wrapper (*dst = 0, then Util_Vsprintf @ 1000:19d8 ->
 * Util_VsprintfCore @ 1000:1a3c). Core flags via table @ 1420:4eeb:
 * ' '(00)/'+'(00)/'#'(01)/'*'(02)/'-'(03)/'.'(04)/'0'(09)/'1'-'9'(05)/
 * 'l'(06)/'L'(07, long-double)/'h'/'H'(08)/'F'(17, far)/'N'(16, near);
 * conv d/i(0A, signed)/o(0B)/u(0C)/x/X(0D, digit 'a'/'A' via c-0x17)/
 * p(0E, XXXX:XXXX via Util_EmitHexWord)/e/E/f/g/G(0F via
 * Util_FormatFloatThunk @ 1000:42d3 -> Util_FormatFloat @ 1000:0221)/
 * c(10)/s(11, far/near)/n(12). %F-upper is far, not float; %O/%U/%D
 * upper unsupported (only x/X/E/G upper). Callers: Float_ReportError
 * @ 1000:1056 + save_prompt_open/save @ 1130:0075/0194 (+ Debug_Print
 * @ 1000:2c9c via Util_Vsprintf). Linux via vsnprintf into dst[0..cap):
 * standard %d/%o/%u/%x/%X/%c/%s/%e/%f/%g faithful (float path is thin
 * LIVE via system fp formatting, same delegation as orig %f->float);
 * Borland %Fs/%Ns far, %p colon form, %n count, %L TBD -> PARTIAL.
 * Returns vsnprintf count, -1 on NULL dst/fmt or cap == 0.
 * TODO Orig: far/near %p/%n/%L exactness @ 1a3c/19c0/19f2/1a08. */
int util_sprintf(char *dst, size_t cap, const char *fmt, ...);

/* --- Vsprintf helpers (2026-10-15, Util_VsprintfCore @ 1000:1a3c) ---
 * OS-free 1:1 ports with explicit params (no ES:, no far, no DGROUP).
 * No globals, no exit(), C17. */

/* Orig: FUN_1000_157a @ 1000:157a -> Util_FormatInt (LIVE).
 * Base 2..36 itoa core (else empty string): signed when is_signed and
 * (int32)v < 0 ('-' + 0u - v), else magnitude; DIV chain into 34 B
 * scratch, reversed with '0'-'9' or (b-10)+digits ('a' 0x61 lower for
 * %x, 'A' 0x41 upper for %X). Wrappers: 15fc/2928/2974 (itoa). */
int util_format_int(char *out, size_t cap, uint32_t v, int is_signed,
                    unsigned base, char digits);
/* Orig: FUN_1000_19d0 @ 1000:19d0 -> Util_EmitHexDigit (LIVE).
 * ADD AL,0x90 / DAA / ADC AL,0x40 / DAA / STOSB: low nibble to ASCII
 * hex uppercase. Returns the char, '?' never (v masked to 4 bits). */
char util_emit_hex_digit(unsigned v);
/* Orig: FUN_1000_19c7 @ 1000:19c7 -> Util_EmitHexByte (LIVE).
 * AAM 0x10 split + 2x EmitHexDigit (high via CALL, low via fallthrough
 * into 19d0). Writes 2 chars, returns 2, -1 on NULL out. */
int util_emit_hex_byte(char *out, unsigned v);
/* Orig: FUN_1000_19c0 @ 1000:19c0 -> Util_EmitHexWord (LIVE).
 * MOV AL,DH / CALL Byte + MOV AL,DL fallthrough: DX as 4 hex
 * (big-endian, high byte first). Used by %p seg:off (colon emitted by
 * caller when far). Writes 4 chars, returns 4, -1 on NULL out. */
int util_emit_hex_word(char *out, unsigned v);
/* Orig: FUN_1000_19f2 @ 1000:19f2 -> Util_VsprintfStrLen (LIVE).
 * PUSH DI / CX=FFFF / SCASB.REPNE / NOT CX / DEC CX / POP DI: strlen
 * preserving DI, returns len without NUL. NULL -> 0 (orig faulted). */
size_t util_vsprintf_strlen(const char *s);

/* Orig: FUN_1000:4124 @ 1000:4124 -> Sys_FatalAbort (STUB).
 * Formats via Util_StrRChr @ 1000:37b2 (strrchr '\\' 0x5c on
 * DAT_1420_4f9a/4f9c) then Win16 GETDESKTOPWINDOW + MESSAGEBOX
 * (CALLF 1608:01a4/0080, style 0x1010). Needs window + DGROUP ->
 * stub CIV_ERR_UNSUPPORTED. NULL msg -> NOENT (orig faulted). */
civ_error_t sys_fatal_abort(const char *msg);

/* Orig: Float_ReportError @ 1000:1056 (PARTIAL/LIVE).
 * Borland matherr body: NULL doubles -> 0.0, filter, else snprintf
 * "%s: %s error\n" (func name, err name) via Util_Sprintf into buf,
 * errno 0x22/0x21 (ERANGE for types 3/4, EDOM otherwise),
 * Sys_FatalAbort(buf). Returns e->retval (orig ST0 = [BP-0x8]).
 * Suppressed (filter 1) skips format/abort but still sets errno?
 * No: orig skips the whole block, errno untouched -> *errno_out
 * untouched, buf untouched, returns retval. NULL e/buf/cap==0 or
 * NULL errno_out -> retval (or 0.0 when e is NULL) without writing. */
double float_report_error(float_matherr_t *e, char *buf, size_t cap,
                          int *errno_out);

/* Orig: FUN_1000:0d56 @ 1000:0d56 -> Float_Pow10 (LIVE).
 * 10^AX on x87: AX < -0x1344 -> 0.0 (FLDZ); AX > +0x1344 -> +INF
 * (FLD float [0x4b8a] = 0x7f800000); AX == 0 -> 1.0 (FLD1); else
 * |AX| binary decomposition: low 3 bits index float table @
 * 1420:4b0e (1,10,..,1e7), then FMUL float [0x4b2e] (1e8), double
 * [0x4b32] (1e16), extended [0x4b3a/44/4e/58/62/6c/76/80]
 * (1e32..1e512...), negative n finishes FDIVR [0x4b0e] (1.0/x).
 * Linux 1:1 observable via powl (same clamp/edges, same INF). */
long double float_pow10(int n);

/* Orig: FUN_1000:0e20 @ 1000:0e20 -> Float_Pow10Far (LIVE).
 * Far RETF wrapper (INC BP ... PUSH [BP+6] / CALL 1000:0d56):
 * thin delegate to float_pow10. */
long double float_pow10_far(int n);

/* Orig: FUN_1000:0e5a @ 1000:0e5a -> Float_Exp2Checked (PARTIAL).
 * 2^x with extended range gate: (exp & 0x7fff) vs 0x4008, else
 * mantissa vs 0xb172; overflow (x>0, type 3) loads HUGE_VAL @
 * 1420:4a92, underflow (x<0, type 4) loads 0.0, both via
 * Float_ReportError(type, "pow" @ 1420:4b96); else Float_Exp2 @
 * 1000:0c7d. Linux on double: finite gate approx (|x| small ->
 * exp2), extremes via float_report_error with the same type/name
 * codes; exact 0x4008/0xb172 extended thresholds TBD (TODO Orig).
 * Non-finite x delegates to exp2 (INF/NAN preserved). */
double float_exp2_checked(double x);

/* Orig: FUN_1000:0ec8 @ 1000:0ec8 -> Float_Pow (PARTIAL).
 * pow(x,y): 0x7ff0 exponent tests (zero/denormal vs INF/NAN per
 * operand -> types 1/3/4 + "pow" @ 1420:4b9a), int-exp fast path
 * (ROR 4 / SUB 0x3ff vs 0x3f, <0xc bit-exactness via SHL/SAR, FCHS
 * for negative base with odd int), else y*log2(x) (Float_Log2 @
 * 1000:0c03) -> Float_Exp2Checked @ 1000:0e5a with FCHS sign fix.
 * Linux on double: same special-type reports via float_report_error
 * (DOMAIN=1/SING?/OVERFLOW=3/UNDERFLOW=4), main path via
 * float_exp2_checked(y * float_log2(x)); 1190 callers are false
 * merges and are IGNORED. Exact FISTP/ROR bit path TBD. */
double float_pow(double x, double y);

/* Orig: FUN_1000:03bc @ 1000:03bc -> Float_ScanFloat (PARTIAL).
 * ~780 B strtod core with get/unget callbacks: isspace skip via
 * ctype @ [c+0x4baf] & 1, sign, INF (sign? "INF" @ 0633: FLD +/-
 * INF @ 1420:4af6/4afa) / NAN ("NAN" @ 0675: FLD +/- NAN @
 * 1420:4afe/4b02), digits (18-cap 0x12, extra via MUL 10 chains),
 * e/E exp (limit 0x1344, overflow flag), scale FILD + Float_Pow10Far
 * FMULP/FDIVP, FCHS sign. Outs: *consumed += chars used ([BP+0xe]),
 * *status ([BP+0x12]): 1 ok, 0 no-digits/fail, 2 overflow (-> INF),
 * -1 (0xffff) immediate EOF. nmax ([BP+0xc]) bounds get/unget.
 * Linux: same token flow via callbacks; x87 FILD/FMULP last-ulp
 * exactness + 18-digit rounding TBD (TODO Orig @ 0440-04f3: DI<=5
 * 16-bit MUL @ 04a6-04b7 vs DI<=18 64-bit MUL x4 @ 04bf-04f0 with
 * PUSH DX carry @ 04d2/04dc, DI>18 skip @ 04f3; @ 05f5 FILD qword
 * [BP-0x18] + @ 0605 CALL Pow10Far + @ 060e FDIVP / @ 0613 FMULP +
 * @ 061c FCHS; final ST0 is extended, Linux returns double -> last-ulp
 * on 18-digit values + powl-vs-table 1e32.. TBD). NULL get or
 * consumed/status -> 0.0 without writing outs. Stays PARTIAL. */
double float_scan_float(float_getchar_fn get, float_ungetchar_fn unget,
                        void *ctx, int nmax, int *consumed, int *status);

#endif /* CIVLIN_SYS_H */
