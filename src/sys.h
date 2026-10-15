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
/* Orig: FUN_1000_137f @ 1000:137f -> Sys_GetClientRect.
 * Client-rect helper behind the status frame calculators @ 1218:0956 /
 * 0a06 (w_out, h_out via GLOBALLOCK on the status-port handle, then rect
 * out). Linux equivalent TBD (SDL_GetWindowSize on the status view).
 * Params are the client width/height outputs; NULL either -> NOENT
 * (orig faulted on a bad far pointer). Zeroes outputs then stub
 * CIV_ERR_UNSUPPORTED until the SDL status-view mapping lands. */
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

#endif /* CIVLIN_SYS_H */
