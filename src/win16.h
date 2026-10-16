/* Win16 + DOS INT 21h -> SDL3/stdio wrappers (CivLin platform layer).
 *
 * Ghidra anchors (civ.exe, NE Win3.x), renamed 2026-10-15/16 (Orig -> New):
 *   Sys_CheckFreeMemory      @ 11e8:0000  (GETFREESPACE + GETFREESYSTEMRESOURCES
 *                              + 1420:43b8 msg; boot gate, see sys.h)
 *   Sys_CheckRuntimeMemory   @ 11e8:0075  (same, per-turn; MESSAGEBOX 4473/44ff)
 *   Mem_AllocLock            @ 11e8:0337 tail (GLOBALALLOC 64000 -> 1420:b394/
 *                              b396 map buf; see map.h CIV_MAP_BUF_LEN)
 *   Mem_DisposePtr           @ 1100:060b  (GLOBALHANDLE gate + UNLOCK + FREE)
 *   Mem_DisposeHandle        @ 1100:0652  (UNLOCK + FREE)
 *   Mem_ReallocHandle        @ 1100:030f  (SIZE + ALLOC + LOCK x2 + move)
 *   Dos_GetCurrentDrive      @ 1000:1252  (INT 21h AH=19h; see sys.h)
 *   Dos_GetCurrentDir        @ 1000:139c  (INT 21h AH=47h)
 *   Dos_SetPath (chdir)      @ 1000:116a  (INT 21h AH=3Bh)
 *   Dos_Open                 @ 1000:2c4a  (INT 21h AH=3Dh; _LOPEN)
 *   Dos_Close                @ 1000:1fa6  (INT 21h AH=3Eh; _LCLOSE)
 *   Dos_Read                 @ 1000:1750  (INT 21h AH=3Fh; _LREAD/_HREAD)
 *   Dos_Write                @ 1000:3fc4  (INT 21h AH=40h; _LWRITE/_HWRITE)
 *   Dos_Seek                 @ 1000:161c  (INT 21h AH=42h; _LLSEEK)
 *   Dos_Unlink               @ 1000:19a6  (INT 21h AH=41h)
 *   Dos_FileAttr             @ 1000:1f5c  (INT 21h AH=43h)
 *   Dos_Ioctl                @ 1000:4252  (INT 21h AH=44h)
 *   Dos_Truncate             @ 1000:2ac7  (AH=40h CX=0; see sys.h)
 *   App_RegisterWindowClasses@ 1098:06f8  (REGISTERCLASS x4 + Ctrl_Register @
 *                              1058:0b4d + GETSYSTEMMETRICS; see dialog.h)
 *   WdwMap_Open              @ 1208:0af1  (CREATEWINDOW + scroll + SETTIMER 200ms)
 *   WdwSmMap_Open            @ 1210:025b  (CREATEWINDOW World Map)
 *   WdwStatus_Open           @ 1218:0386  (CREATEWINDOW Status)
 *   Video_CheckDriverCaps    @ 1008:059a  (GETDEVICECAPS checklist @ 1420:0af4)
 *   Palette_Init             @ 1090:0488  (CREATEPALETTE 0xec; see video.h)
 *   Palette_AnimateRange     @ 1090:0376  (ANIMATEPALETTE; see video.h)
 *   Font_EnumFamiliesInit    @ 1008:0e99  (ENUMFONTFAMILIES + ADDFONTRESOURCE
 *                              civfonts.fon @ 1420:00a4/00fe; see boot.h)
 *   Config_LoadPrefs         @ 1008:0b4a  (_LOPEN/_LREAD civ.prf @ 1420:010b)
 *   Config_SavePrefs         @ 1008:0bbf  (_LCREAT/_LWRITE civ.prf @ 1420:0113)
 *   ResMgr_LoadRsc           @ 1190:0000  (_LOPEN/_HREAD 0xE360; see rsc_loader.h)
 *   Sound_MciOpen            @ 1150:001c  (MCI_OPEN WAV; see audio.h)
 *
 * Policy (agreed 2026-10-16, odblokowanie: stan gry portujemy 1:1, tylko
 * mechanika OS zostaje STUB): do NOT reimplement Win16 mechanics faithfully.
 * Port only the effect promised to the rest of the game; Win16 plumbing has
 * no Linux equivalent and becomes a documented no-op CIV_OK (WONTFIX-Win16)
 * or a thin SDL3/stdio delegate below. Stan gry (DGROUP DAT_*, shared-static
 * DAT_13f8_0000, stos dialogow, bufory map/save/palety) to NIE mechanika OS
 * i MUSI byc portowany 1:1 na jawne konteksty albo module-static z adnotacja
 * Orig (patrz AGENTS.md + civ_common.h). Orig addresses stay in comments so
 * the RE map survives. C17, SDL3 only, no exit() in libs, read-only vs
 * civ1win/unpacked (never writes assets).
 */
#ifndef CIVLIN_WIN16_H
#define CIVLIN_WIN16_H

#include <stddef.h>
#include <stdint.h>

#include "civ_common.h"

/* --- Memory: startup arena + host check (replaces Win16 heap queries) ---
 *
 * Orig: GLOBALALLOC/GLOBALLOCK/UNLOCK/FREE (@ 1100:030f/060b/0652,
 * 11e8:0337) + GETFREESPACE/GETFREESYSTEMRESOURCES (@ 11e8:0000/0075).
 * Linux: one 16 MB arena allocated once at boot (map buf 64 KB + tile
 * cache + RSC payloads + save/HoF buffers all fit with margin); dynamic
 * payloads use plain malloc/free. No selectors, no locking, no compaction. */

#define WIN16_ARENA_SIZE (16u * 1024u * 1024u)
#define WIN16_MAP_BUF_LEN 64000u
#define WIN16_FREE_BYTES_MIN 2500000L
#define WIN16_FREE_RES_MIN 0x23
#define WIN16_RUN_BYTES_WARN 1000000L
#define WIN16_RUN_RES_WARN 10
#define WIN16_RUN_BYTES_FINAL 300000L
#define WIN16_RUN_RES_FINAL 8

civ_error_t win16_mem_init(void);
void win16_mem_shutdown(void);
/* Bump-allocates n bytes (8-aligned) from the arena; NULL when not
 * initialised or out of space. No per-block free (reset at shutdown). */
void *win16_arena_alloc(size_t n);
size_t win16_arena_used(void);
size_t win16_arena_size(void);
/* Transitional GLOBALALLOC-shape shims (plain malloc/free, no selector). */
void *win16_global_alloc(size_t n);
void win16_global_free(void *p);
/* Host resources: SDL_GetSystemRAM MB -> bytes (clamped); resources always
 * plentiful on Linux (returns 100). */
long win16_get_free_space(void);
int win16_get_free_resources(int which);
/* Boot/per-turn gates with Orig floors (CIV_OK = enough, NOMEM = low). */
civ_error_t win16_check_free_memory(void);
civ_error_t win16_check_runtime_memory(int already_warned, int warned_final);

/* --- DOS INT 21h / KERNEL file layer -> stdio ---
 *
 * Orig: AH=19h/47h/3Bh (drive/cwd/chdir) + AH=3Dh/3Eh/3Fh/40h/41h/42h/43h/44h
 * + _LOPEN/_LREAD/_LWRITE/_LLSEEK/_LCLOSE. Linux: drive always 0 (no drive
 * letters), cwd via getcwd, chdir is a no-op CIV_OK (cwd owned by launcher),
 * file ops via an fd table over fopen/fread/fwrite/fseek/fclose. */

int win16_dos_get_drive(void);
civ_error_t win16_dos_get_cwd(char *buf, size_t cap);
civ_error_t win16_dos_chdir(const char *path);
/* Minimal _L* table: fd >= 0, -1 (WIN16_HFILE_ERROR) on failure. Mode 0 =
 * read ("rb"), 1 = write/create ("wb"), 2 = read/write ("r+b"). Origin
 * 0/1/2 = SEEK_SET/CUR/END (as _LLSEEK). */
int win16_lopen(const char *path, int mode);
int win16_lread(int fd, void *buf, unsigned len);
int win16_lwrite(int fd, const void *buf, unsigned len);
long win16_llseek(int fd, long off, int origin);
int win16_lclose(int fd);

/* --- USER -> SDL3 ---
 *
 * Orig: REGISTERCLASS x4 + Ctrl_RegisterCustomClasses (@ 1098:06f8/1058:0b4d)
 * + GETSYSTEMMETRICS + CREATEWINDOW (main/map/status) + MESSAGEBOX +
 * SETTIMER/KILLTIMER (5 ms WinMain @ 1008:00cd, 200 ms WdwMap @ 1208:0e7b) +
 * PEEKMESSAGE/DISPATCHMESSAGE pump. SDL3: window/renderer are caller-owned
 * (main.c), classes do not exist, metrics are constants, timers are stored
 * intervals (real SDL_AddTimer lands with the modal rewrite), pump is
 * SDL_PumpEvents (already LIVE in sys_pump_messages/sys_yield). */

civ_error_t win16_register_classes(void);
int win16_get_system_metrics(int which);
civ_error_t win16_message_box(const char *text, const char *caption);
civ_error_t win16_set_timer(int ms);
civ_error_t win16_kill_timer(void);
int win16_timer_interval_ms(void);

/* --- GDI -> SDL3 ---
 *
 * Orig: CREATEPALETTE/SELECTPALETTE/ANIMATEPALETTE/DELETEOBJECT (0xec godpal
 * + LOGPALETTE 0x300) + CREATEFONTINDIRECT/ADDFONTRESOURCE/ENUMFONTFAMILIES +
 * BITBLT/STRETCHBLT ports + CREATESOLIDBRUSH/CREATEPATTERNBRUSH. SDL3: no
 * global palette/DC/brush objects; palette applies per-texture via
 * SDL_SetPaletteColors (see video.h palette_apply_to_port), fonts are the
 * bundled FNT raster (see boot.h font_fnt_*), blits are SDL_RenderTexture.
 * All below are no-op CIV_OK records of the discarded mechanics. */

civ_error_t win16_create_palette(void);
civ_error_t win16_animate_palette(int first, int count);
civ_error_t win16_dispose_palette(void);
civ_error_t win16_delete_object(void);

/* --- Assets: read-only search (never writes civ1win/unpacked) ---
 *
 * Probes "civfonts.fon", "CivData0..4.rsc", "*.wav" next to the checkout:
 * civ1win/, ., unpacked/. Copies the first hit path into out (cap incl.
 * NUL). NOENT when nothing found. */

civ_error_t win16_find_asset(const char *name, char *out, size_t cap);
civ_error_t win16_probe_resources(void);
civ_error_t win16_probe_fonts(void);

#endif /* CIVLIN_WIN16_H */
