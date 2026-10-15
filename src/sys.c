/* DOS / system helpers + RNG. 1:1 ports where the logic is OS-free,
 * documented adaptations where DOS/Win16 has no Linux equivalent. */
#include "sys.h"

#include <stdarg.h>
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

int util_progress(int done, int total)
{
    /* Orig: FUN_1000_29ea @ 1000:29ea -> Util_Progress. OS-free 1:1 port
     * of the counter/total percent behind the continent-link progress
     * (orig bar range 0x1248; Linux reports 0..100 percent). No Win16, no
     * globals, no exit(), read-only. */
    long pct;

    if (total <= 0 || done <= 0)
        return 0;
    if (done >= total)
        return 100;
    pct = (long)done * 100L / (long)total;
    if (pct < 0)
        pct = 0;
    if (pct > 100)
        pct = 100;
    return (int)pct;
}

int debug_print(const char *fmt, ...)
{
    /* Orig: FUN_1000_2c9c @ 1000:2c9c -> Debug_Print. OS-free 1:1 port of
     * the fprintf path behind ResMgr_DumpStats @ 1190:17d0: vfprintf(fmt)
     * to stdout. No globals, no exit(), never writes game assets. */
    va_list ap;
    int n;

    if (!fmt)
        return -1;
    va_start(ap, fmt);
    n = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return n;
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
    /* Orig: FUN_1218_0956 @ 1218:0956 family -> Sys_GetClientRect (see
     * sys.h). 1000:137f is Util_MemCopyFar (far memcpy, see civ_common.h
     * util_mem_copy_far), only the copy step inside 1218:0956. Linux
     * equivalent TBD (SDL_GetWindowSize on the status view); NULL either
     * -> NOENT. Stub CIV_ERR_UNSUPPORTED. */
    if (!w_out || !h_out)
        return CIV_ERR_NOENT;
    *w_out = 0;
    *h_out = 0;
    /* TODO: SDL_GetWindowSize mapping for the SDL status view. */
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

civ_error_t sys_check_free_memory(void)
{
    /* Orig: FUN_11e8_0000 @ 11e8:0000 -> Sys_CheckFreeMemory. Boot check:
     * GETFREESPACE against the required floor, MESSAGEBOX 1420:43b8 on
     * shortfall. Linux mapping TBD (SDL_GetSystemRAM). Stub
     * CIV_ERR_UNSUPPORTED. No globals, no exit(), read-only. */
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

int ne_entry_count(const char *path)
{
    /* Orig NE entry table (MZ e_lfanew @ 0x3C + NE magic, entry table @
     * NE+0x04/0x06, bundle walk) (AH2). OS-free file-scan read-only port
     * (fopen "rb", LE reads, fseek/ftell bounds, no writes, never writes
     * unpacked/civ.exe or civfonts.fon). No globals, no exit(), C17. */
    FILE *fp;
    long fsize;
    uint8_t mz[64];
    uint32_t ne_off;
    uint8_t ne[64];
    uint16_t ent_rel;
    uint16_t ent_len;
    long ent_start;
    long ent_end;
    long pos;
    int total;

    if (!path)
        return -1;
    fp = fopen(path, "rb");
    if (!fp)
        return -1;
    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    fsize = ftell(fp);
    if (fsize < 0x40L) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0L, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    if (fread(mz, 1, sizeof(mz), fp) != sizeof(mz)) {
        fclose(fp);
        return -1;
    }
    if (mz[0] != 0x4Du || mz[1] != 0x5Au) {
        fclose(fp);
        return -1;
    }
    ne_off = (uint32_t)mz[0x3Cu] | ((uint32_t)mz[0x3Du] << 8) |
        ((uint32_t)mz[0x3Eu] << 16) | ((uint32_t)mz[0x3Fu] << 24);
    if ((long)ne_off + 64L > fsize) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, (long)ne_off, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    if (fread(ne, 1, sizeof(ne), fp) != sizeof(ne)) {
        fclose(fp);
        return -1;
    }
    if (ne[0] != 0x4Eu || ne[1] != 0x45u) {
        fclose(fp);
        return -1;
    }
    ent_rel = (uint16_t)((uint16_t)ne[0x04] | ((uint16_t)ne[0x05] << 8));
    ent_len = (uint16_t)((uint16_t)ne[0x06] | ((uint16_t)ne[0x07] << 8));
    ent_start = (long)ne_off + (long)ent_rel;
    ent_end = ent_start + (long)ent_len;
    if (ent_start > ent_end || ent_end > fsize) {
        fclose(fp);
        return -1;
    }
    total = 0;
    pos = ent_start;
    for (;;) {
        uint8_t hdr[2];
        uint8_t cnt;
        uint8_t seg;
        long esz;
        long need;

        if (pos + 2L > ent_end) {
            fclose(fp);
            return -1;
        }
        if (fseek(fp, pos, SEEK_SET) != 0) {
            fclose(fp);
            return -1;
        }
        if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
            fclose(fp);
            return -1;
        }
        cnt = hdr[0];
        seg = hdr[1];
        if (cnt == 0u)
            break;
        esz = (seg == 0xFFu) ? 6L : 3L;
        need = 2L + (long)cnt * esz;
        if (pos + need > ent_end) {
            fclose(fp);
            return -1;
        }
        if (total > 32767 - (int)cnt) {
            fclose(fp);
            return -1;
        }
        total += (int)cnt;
        pos += need;
    }
    fclose(fp);
    return total;
}

int ne_entry_addr(const char *path, int index,
                  uint16_t *seg_out, uint16_t *off_out)
{
    /* Orig NE entry table (same MZ e_lfanew @ 0x3C / NE magic / bundle
     * anchors as ne_entry_count) (AH2). OS-free file-scan read-only port
     * (same fopen "rb" + LE reads, no writes). 0-based index -> seg:off;
     * fixed seg = bundle seg, moveable 0xFF seg = byte 3. No globals,
     * no exit(), C17, read-only; outs untouched on error/OOR. */
    FILE *fp;
    long fsize;
    uint8_t mz[64];
    uint32_t ne_off;
    uint8_t ne[64];
    uint16_t ent_rel;
    uint16_t ent_len;
    long ent_start;
    long ent_end;
    long pos;
    int cur;

    if (!path || !seg_out || !off_out || index < 0)
        return -1;
    fp = fopen(path, "rb");
    if (!fp)
        return -1;
    if (fseek(fp, 0L, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    fsize = ftell(fp);
    if (fsize < 0x40L) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0L, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    if (fread(mz, 1, sizeof(mz), fp) != sizeof(mz)) {
        fclose(fp);
        return -1;
    }
    if (mz[0] != 0x4Du || mz[1] != 0x5Au) {
        fclose(fp);
        return -1;
    }
    ne_off = (uint32_t)mz[0x3Cu] | ((uint32_t)mz[0x3Du] << 8) |
        ((uint32_t)mz[0x3Eu] << 16) | ((uint32_t)mz[0x3Fu] << 24);
    if ((long)ne_off + 64L > fsize) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, (long)ne_off, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }
    if (fread(ne, 1, sizeof(ne), fp) != sizeof(ne)) {
        fclose(fp);
        return -1;
    }
    if (ne[0] != 0x4Eu || ne[1] != 0x45u) {
        fclose(fp);
        return -1;
    }
    ent_rel = (uint16_t)((uint16_t)ne[0x04] | ((uint16_t)ne[0x05] << 8));
    ent_len = (uint16_t)((uint16_t)ne[0x06] | ((uint16_t)ne[0x07] << 8));
    ent_start = (long)ne_off + (long)ent_rel;
    ent_end = ent_start + (long)ent_len;
    if (ent_start > ent_end || ent_end > fsize) {
        fclose(fp);
        return -1;
    }
    cur = 0;
    pos = ent_start;
    for (;;) {
        uint8_t hdr[2];
        uint8_t cnt;
        uint8_t seg;
        long esz;
        long need;

        if (pos + 2L > ent_end) {
            fclose(fp);
            return -1;
        }
        if (fseek(fp, pos, SEEK_SET) != 0) {
            fclose(fp);
            return -1;
        }
        if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
            fclose(fp);
            return -1;
        }
        cnt = hdr[0];
        seg = hdr[1];
        if (cnt == 0u)
            break;
        esz = (seg == 0xFFu) ? 6L : 3L;
        need = 2L + (long)cnt * esz;
        if (pos + need > ent_end) {
            fclose(fp);
            return -1;
        }
        if (index < cur + (int)cnt) {
            int ei = index - cur;
            long epos = pos + 2L + (long)ei * esz;
            uint8_t eb[6];

            if (fseek(fp, epos, SEEK_SET) != 0) {
                fclose(fp);
                return -1;
            }
            if (fread(eb, 1, (size_t)esz, fp) != (size_t)esz) {
                fclose(fp);
                return -1;
            }
            if (seg == 0xFFu) {
                *seg_out = (uint16_t)eb[3];
                *off_out = (uint16_t)((uint16_t)eb[4] |
                    ((uint16_t)eb[5] << 8));
            } else {
                *seg_out = (uint16_t)seg;
                *off_out = (uint16_t)((uint16_t)eb[1] |
                    ((uint16_t)eb[2] << 8));
            }
            fclose(fp);
            return 0;
        }
        cur += (int)cnt;
        pos += need;
    }
    fclose(fp);
    return -1;
}
