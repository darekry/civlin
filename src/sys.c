/* DOS / system helpers + RNG. 1:1 ports where the logic is OS-free,
 * documented adaptations where DOS/Win16 has no Linux equivalent. */
#include "sys.h"

#include <string.h>
#include <unistd.h>

#include <SDL3/SDL.h>

/* Scratch for getcwd() (replaces PATH_MAX, hidden under -std=c17). */
#define SYS_TMP_PATH 4096

/* RNG state: 1420:4ee0 (lo) + 1420:4ee2 (hi). */
static unsigned long s_rng_state;
/* g_nCurrentDrive @ 1228:0002; g_szBasePath @ 1228:0004. */
static int s_drive;
static char s_base_path[256];

void rng_seed(unsigned seed)
{
    /* Rng_Seed @ 1000:1716: DAT_1420_4ee2 = 0; DAT_1420_4ee0 = param. */
    s_rng_state = (unsigned long)(seed & 0xFFFFu);
}

unsigned rng_next_raw(void)
{
    /* Rng_NextRaw @ 1000:1729: seed = seed * 0x015A4E35 + 1 (via U32_Mul32
     * @ 1000:16fe, low 32 bits); return (seed >> 16) & 0x7FFF. */
    s_rng_state = s_rng_state * 0x015A4E35u + 1u;
    return (unsigned)((s_rng_state >> 16) & 0x7FFFu);
}

unsigned rng_range(int n)
{
    /* Rng_Range @ 11c8:003d: (rand15 * n) >> 15. Guard for n <= 0 is a
     * Linux-side addition (orig callers always pass n > 0). */
    if (n <= 0)
        return 0;
    return (rng_next_raw() * (unsigned)n) >> 15;
}

void rng_init(void)
{
    /* Rng_Init @ 11c8:0000: seed = Sys_GetTickScaled() & 0x7FFF, forced
     * nonzero, then Rng_Seed. */
    unsigned seed = (unsigned)(sys_get_tick_scaled() & 0x7FFFu);
    if (seed == 0)
        seed = 1;
    rng_seed(seed);
}

int util_clamp(int v, int lo, int hi)
{
    /* Util_Clamp @ 11c8:0063. */
    if (v < lo)
        v = lo;
    if (hi < v)
        v = hi;
    return v;
}

int dos_get_current_drive(void)
{
    /* Dos_GetCurrentDrive @ 1000:1252 (INT 21h AH=19h + 1). No drive
     * letters on Linux -> 0, so `0 < drive` guards skip DOS-only branches
     * (Sound_PlayId, Config_SavePrefs). */
    return 0;
}

int dos_get_current_drive_index(void)
{
    /* Dos_GetCurrentDriveIndex @ 1000:13e6: Dos_GetCurrentDrive() - 1. */
    return dos_get_current_drive() - 1;
}

int dos_get_current_dir(int drive, char *buf, size_t cap)
{
    /* Dos_GetCurrentDir @ 1000:139c (INT 21h AH=47h). Drive ignored on
     * Linux. Orig returns the path WITHOUT drive/leading backslash, so
     * the leading '/' is stripped to match. */
    char tmp[SYS_TMP_PATH];
    const char *rel;
    size_t n;

    (void)drive;
    if (!buf || cap == 0)
        return -1;
    if (!getcwd(tmp, sizeof(tmp)))
        return -1;
    rel = tmp[0] == '/' ? tmp + 1 : tmp;
    n = strlen(rel) + 1;
    if (n > cap)
        return -1;
    memcpy(buf, rel, n);
    return 0;
}

unsigned long sys_get_tick_scaled(void)
{
    /* Sys_GetTickScaled @ 1100:02e1: GETTICKCOUNT() * 0x3C / 1000. */
    return (unsigned long)SDL_GetTicks() * 60u / 1000u;
}

char *sys_build_current_dir_path(char *dst, size_t cap)
{
    /* Sys_BuildCurrentDirPath @ 1000:2826. Orig composes "D:\curdir" and
     * fails NULL when the length exceeds max or the buffer must be
     * allocated and allocation fails. Linux: getcwd() full path, same
     * contract (dst on success, NULL when too long / getcwd fails). */
    char tmp[SYS_TMP_PATH];
    size_t n;

    if (!dst || cap == 0)
        return NULL;
    if (!getcwd(tmp, sizeof(tmp)))
        return NULL;
    n = strlen(tmp) + 1;
    if (n > cap)
        return NULL;
    memcpy(dst, tmp, n);
    return dst;
}

civ_error_t sys_init_paths(void)
{
    /* WinMain prologue: Sys_GetCurrentDrive -> 1228:0002,
     * Sys_BuildCurrentDirPath -> 1228:0004. */
    s_drive = dos_get_current_drive();
    if (!sys_build_current_dir_path(s_base_path, sizeof(s_base_path)))
        return CIV_ERR_IO;
    return CIV_OK;
}

int sys_current_drive(void)
{
    return s_drive;
}

const char *sys_base_path(void)
{
    return s_base_path;
}

void sys_yield(void)
{
    /* Sys_Yield @ 1098:0157 (wraps Anim_PumpIfActive @ 10b0:0000).
     * SDL3 message-queue pump during long operations. */
    SDL_PumpEvents();
}

int sys_pump_messages(void)
{
    /* Sys_PumpMessages @ 1108:0000: Win16 PEEKMESSAGE loop (pumped in
     * unit_wait_for_orders @ 11c0:0ec2 until input/mode change) maps to
     * a single SDL_PumpEvents pass; v1 reports an empty queue (event
     * dispatch via SDL_PollEvent TBD with the modal-wait rewrite). */
    SDL_PumpEvents();
    return 0;
}

civ_error_t sys_get_client_rect(int *w_out, int *h_out)
{
    if (!w_out || !h_out)
        return CIV_ERR_NOENT;
    *w_out = 0;
    *h_out = 0;
    /* TODO: Orig FUN_1000_137f @ 1000:137f (GLOBALLOCK the status-port
     * handle -> client RECT for 1218:0956/0a06). Needs the SDL status
     * view + SDL_GetWindowSize mapping. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t status_measure_row(int *h_out)
{
    if (!h_out)
        return CIV_ERR_NOENT;
    *h_out = 0;
    /* TODO: Orig FUN_1138_0269 @ 1138:0269 (Status_MeasureRow:
     * GETTEXTMETRICS tmHeight variant; feeds the status-row stride
     * alongside status_font_height @ 1098:0dda; needs SDL text-metrics
     * bridge). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t sys_check_runtime_memory(void)
{
    /* TODO: Sys_CheckRuntimeMemory @ 11e8:0075 (GETFREESPACE <1000000 /
     * <300000 + GETFREESYSTEMRESOURCES <10/<8 -> MESSAGEBOX 1420:4473 /
     * 1420:44ff, once each via 1420:43b4/43b6). Needs a parent window +
     * SDL_GetSystemRAM mapping. */
    return CIV_ERR_UNSUPPORTED;
}

size_t file_write_chunk(FILE *fp, const uint8_t *src, size_t len)
{
    /* File_WriteChunk @ 1000:xxxx (packet E): _HWRITE/_LWRITE thin wrapper
     * complementing File_ReadChunk @ 1000:272a (file_read_chunk). fwrite
     * is the Linux equivalent; short counts on error/disk-full. */
    if (!fp || !src || len == 0)
        return 0;
    return fwrite(src, 1, len, fp);
}
