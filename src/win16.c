/* Win16 + DOS INT 21h -> SDL3 wrappers (SDL_IO + SDL_filesystem).
 * See win16.h for anchors/policy. No exit() here; arena state is private
 * to this file. SDL3 only. File tables are int-fd -> SDL_IOStream*
 * (dualism with sys.c dos table stays: separate arrays, fd from 5). */
#include "win16.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <SDL3/SDL.h>

/* --- Arena (16 MB at boot) --- */

static uint8_t *s_arena;
static size_t s_arena_used;

civ_error_t win16_mem_init(void)
{
    if (s_arena)
        return CIV_OK;
    s_arena = (uint8_t *)malloc(WIN16_ARENA_SIZE);
    if (!s_arena)
        return CIV_ERR_NOMEM;
    s_arena_used = 0;
    return CIV_OK;
}

void win16_mem_shutdown(void)
{
    free(s_arena);
    s_arena = NULL;
    s_arena_used = 0;
}

void *win16_arena_alloc(size_t n)
{
    size_t aligned;
    if (!s_arena || n == 0)
        return NULL;
    aligned = (s_arena_used + 7u) & ~(size_t)7u;
    if (aligned + n > WIN16_ARENA_SIZE)
        return NULL;
    s_arena_used = aligned + n;
    return s_arena + aligned;
}

size_t win16_arena_used(void)
{
    return s_arena_used;
}

size_t win16_arena_size(void)
{
    return WIN16_ARENA_SIZE;
}

void *win16_global_alloc(size_t n)
{
    /* WONTFIX-Win16: no selector/lock; plain malloc (Orig @ 1100:030f). */
    if (n == 0)
        return NULL;
    return malloc(n);
}

void win16_global_free(void *p)
{
    free(p);
}

long win16_get_free_space(void)
{
    /* GETFREESPACE @ 11e8:0000 -> SDL_GetSystemRAM (MB) to bytes. */
    int mb = SDL_GetSystemRAM();
    long bytes;
    if (mb <= 0)
        return 0L;
    bytes = (long)mb * 1024L * 1024L;
    if (bytes < 0L)
        return 2147483647L;
    return bytes;
}

int win16_get_free_resources(int which)
{
    /* GETFREESYSTEMRESOURCES @ 11e8:0000/0075: no GDI/USER heaps on Linux. */
    (void)which;
    return 100;
}

civ_error_t win16_check_free_memory(void)
{
    /* Boot floor @ 11e8:0000: 2.5 MB + 0x23/0x23 resources. */
    if (win16_get_free_space() < WIN16_FREE_BYTES_MIN)
        return CIV_ERR_NOMEM;
    if (win16_get_free_resources(1) < WIN16_FREE_RES_MIN)
        return CIV_ERR_NOMEM;
    if (win16_get_free_resources(0) < WIN16_FREE_RES_MIN)
        return CIV_ERR_NOMEM;
    return CIV_OK;
}

civ_error_t win16_check_runtime_memory(int already_warned, int warned_final)
{
    /* Per-turn @ 11e8:0075: warn-once at 1 MB/10, final at 300 KB/8.
     * Returns CIV_OK when fine, NOMEM when the caller should warn. */
    long free_bytes = win16_get_free_space();
    int r1 = win16_get_free_resources(1);
    int r0 = win16_get_free_resources(0);
    int low_warn = (free_bytes < WIN16_RUN_BYTES_WARN) || (r1 < WIN16_RUN_RES_WARN) ||
        (r0 < WIN16_RUN_RES_WARN);
    int low_final = (free_bytes < WIN16_RUN_BYTES_FINAL) ||
        (r1 < WIN16_RUN_RES_FINAL) || (r0 < WIN16_RUN_RES_FINAL);
    if (low_final && already_warned && !warned_final)
        return CIV_ERR_NOMEM;
    if (low_warn && !already_warned)
        return CIV_ERR_NOMEM;
    return CIV_OK;
}

/* --- DOS/file layer --- */

int win16_dos_get_drive(void)
{
    /* AH=19h @ 1000:1252: no drive letters -> 0 (so `0 < drive` guards skip). */
    return 0;
}

civ_error_t win16_dos_get_cwd(char *buf, size_t cap)
{
    /* AH=47h @ 1000:139c: getcwd (Orig returns path without drive prefix).
     * [PORT-SDL] Replaces: getcwd -> SDL_GetCurrentDirectory (malloc'd,
     * SDL_free after copy). POSIX getcwd stays only as fallback when SDL
     * returns NULL. SDL_GetError() is log-only, contract is civ_error_t. */
    char *cwd = NULL;
    const char *rel;
    size_t n;
    if (!buf || cap == 0)
        return CIV_ERR_NOENT;
    cwd = SDL_GetCurrentDirectory();
    if (cwd) {
        rel = (cwd[0] == '/') ? cwd + 1 : cwd;
        n = strlen(rel) + 1;
        if (n > cap) {
            SDL_free(cwd);
            return CIV_ERR_TRUNCATED;
        }
        memcpy(buf, rel, n);
        SDL_free(cwd);
        return CIV_OK;
    }
    /* POSIX-FALLBACK (no SDL twin): SDL_GetCurrentDirectory failed. */
    {
        char tmp[4096];
        if (!getcwd(tmp, sizeof(tmp))) /* POSIX-FALLBACK (no SDL twin) */
            return CIV_ERR_IO;
        rel = (tmp[0] == '/') ? tmp + 1 : tmp;
        n = strlen(rel) + 1;
        if (n > cap)
            return CIV_ERR_TRUNCATED;
        memcpy(buf, rel, n);
        return CIV_OK;
    }
}

civ_error_t win16_dos_chdir(const char *path)
{
    /* AH=3Bh @ 1000:116a: WONTFIX — cwd owned by launcher, no-op (Orig
     * faulted on NULL far ptr; we report NOENT instead). */
    if (!path)
        return CIV_ERR_NOENT;
    return CIV_OK;
}

#define WIN16_MAX_FILES 64

static SDL_IOStream *s_files[WIN16_MAX_FILES];

static int win16_fd_alloc(SDL_IOStream *fp)
{
    int i;
    if (!fp)
        return -1;
    for (i = 0; i < WIN16_MAX_FILES; i++) {
        if (!s_files[i]) {
            s_files[i] = fp;
            return i + 5; /* DOS fds 0..4 are std/aux/prn (see sys.h). */
        }
    }
    return -1;
}

static SDL_IOStream *win16_fd_get(int fd)
{
    int idx = fd - 5;
    if (idx < 0 || idx >= WIN16_MAX_FILES)
        return NULL;
    return s_files[idx];
}

int win16_lopen(const char *path, int mode)
{
    /* _LOPEN @ 1000:2ad9 (AH=3Dh): 0 = rb, 1 = wb, else r+b.
     * [PORT-SDL] Replaces: fopen -> SDL_IOFromFile (same modes, no
     * create/truncate beyond "wb"). Table int-fd shape unchanged (fd 5+). */
    const char *m = "rb";
    SDL_IOStream *fp;
    if (!path)
        return -1;
    if (mode == 1)
        m = "wb";
    else if (mode == 2)
        m = "r+b";
    fp = SDL_IOFromFile(path, m);
    if (!fp)
        return -1;
    {
        int fd = win16_fd_alloc(fp);
        if (fd < 0)
            (void)SDL_CloseIO(fp);
        return fd;
    }
}

int win16_lread(int fd, void *buf, unsigned len)
{
    /* _LREAD/_HREAD @ 1000:1750 (AH=3Fh): short counts preserved.
     * [PORT-SDL] Replaces: fread -> SDL_ReadIO (short preserved, 0 at
     * len==0, -1 on missing slot/fail). SDL_GetError() log-only. */
    SDL_IOStream *fp = win16_fd_get(fd);
    size_t n;
    if (!fp || (!buf && len > 0))
        return -1;
    if (len == 0)
        return 0;
    n = SDL_ReadIO(fp, buf, (size_t)len);
    if (n == (size_t)len)
        return (int)n;
    if (SDL_GetIOStatus(fp) == SDL_IO_STATUS_ERROR)
        return -1;
    return (int)n;
}

int win16_lwrite(int fd, const void *buf, unsigned len)
{
    /* _LWRITE/_HWRITE @ 1000:3fc4 (AH=40h).
     * [PORT-SDL] Replaces: fwrite -> SDL_WriteIO (same short/-1 shape). */
    SDL_IOStream *fp = win16_fd_get(fd);
    size_t n;
    if (!fp || (!buf && len > 0))
        return -1;
    if (len == 0)
        return 0;
    n = SDL_WriteIO(fp, buf, (size_t)len);
    if (n == (size_t)len)
        return (int)n;
    if (SDL_GetIOStatus(fp) == SDL_IO_STATUS_ERROR)
        return -1;
    return (int)n;
}

long win16_llseek(int fd, long off, int origin)
{
    /* _LLSEEK @ 1000:161c (AH=42h): 0/1/2 = SET/CUR/END.
     * [PORT-SDL] Replaces: fseek/ftell -> SDL_SeekIO (returns pos, -1L
     * on fail; OOR origin falls to SET like before). */
    SDL_IOStream *fp = win16_fd_get(fd);
    SDL_IOWhence whence = SDL_IO_SEEK_SET;
    Sint64 pos;
    if (!fp)
        return -1L;
    if (origin == 1)
        whence = SDL_IO_SEEK_CUR;
    else if (origin == 2)
        whence = SDL_IO_SEEK_END;
    pos = SDL_SeekIO(fp, (Sint64)off, whence);
    if (pos < 0)
        return -1L;
    return (long)pos;
}

int win16_lclose(int fd)
{
    /* _LCLOSE @ 1000:1fa6 (AH=3Eh).
     * [PORT-SDL] Replaces: fclose -> SDL_CloseIO + NULL slot. */
    int idx = fd - 5;
    bool ok;
    if (idx < 0 || idx >= WIN16_MAX_FILES || !s_files[idx])
        return -1;
    ok = SDL_CloseIO(s_files[idx]);
    s_files[idx] = NULL;
    return ok ? 0 : -1;
}

/* --- USER --- */

civ_error_t win16_register_classes(void)
{
    /* REGISTERCLASS x4 + Ctrl_RegisterCustomClasses @ 1098:06f8/1058:0b4d:
     * WONTFIX — SDL3 has no window classes. No-op CIV_OK. */
    return CIV_OK;
}

int win16_get_system_metrics(int which)
{
    /* GETSYSTEMMETRICS: 0 = CXSCREEN, 1 = CYSCREEN (only ones consumed at
     * boot); SDL3 windows are freely sized, report the CIV 640x480 base. */
    if (which == 1)
        return 480;
    return 640;
}

civ_error_t win16_message_box(const char *text, const char *caption)
{
    /* MESSAGEBOX (1420:43b8/4473/44ff): SDL3 simple dialog; NULL-safe. */
    if (!text)
        text = "";
    if (!caption)
        caption = "CivLin";
    if (!SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, caption, text, NULL))
        return CIV_ERR_IO;
    return CIV_OK;
}

static int s_timer_ms;

civ_error_t win16_set_timer(int ms)
{
    /* SETTIMER (5 ms @ 1008:00cd, 200 ms @ 1208:0e7b): store only; the real
     * SDL_AddTimer lands with the modal rewrite (see map_timer_proc). */
    if (ms <= 0)
        return CIV_ERR_RANGE;
    s_timer_ms = ms;
    return CIV_OK;
}

civ_error_t win16_kill_timer(void)
{
    s_timer_ms = 0;
    return CIV_OK;
}

int win16_timer_interval_ms(void)
{
    return s_timer_ms;
}

/* --- GDI --- */

civ_error_t win16_create_palette(void)
{
    /* CREATEPALETTE @ 1090:0488: WONTFIX — per-texture SDL palettes. */
    return CIV_OK;
}

civ_error_t win16_animate_palette(int first, int count)
{
    /* ANIMATEPALETTE @ 1090:0376: staged via palette_animate_range. */
    if (first < 0 || count <= 0 || first + count > 256)
        return CIV_ERR_RANGE;
    return CIV_OK;
}

civ_error_t win16_dispose_palette(void)
{
    return CIV_OK;
}

civ_error_t win16_delete_object(void)
{
    /* DELETEOBJECT (fonts/brushes/bitmaps): nothing owned here. */
    return CIV_OK;
}

/* --- Assets (HOST probes, read-only) --- */

static const char *const s_asset_dirs[] = { "civ1win", ".", "unpacked" };

typedef struct win16_asset_scan {
    const char *want;
    char hit[1024];
    int found;
    int truncated;
} win16_asset_scan_t;

static SDL_EnumerationResult SDLCALL win16_asset_cb(void *userdata,
                                                   const char *dirname,
                                                   const char *fname)
{
    win16_asset_scan_t *s = (win16_asset_scan_t *)userdata;
    int n;
    if (!s || s->found)
        return SDL_ENUM_SUCCESS;
    if (!fname || !s->want)
        return SDL_ENUM_CONTINUE;
    if (strcasecmp(fname, s->want) != 0)
        return SDL_ENUM_CONTINUE;
    n = snprintf(s->hit, sizeof(s->hit), "%s%s", dirname ? dirname : "",
        fname);
    if (n <= 0 || (size_t)n >= sizeof(s->hit))
        return SDL_ENUM_CONTINUE;
    s->found = 1;
    return SDL_ENUM_SUCCESS;
}

civ_error_t win16_find_asset(const char *name, char *out, size_t cap)
{
    /* Case-sensitive fast path first (exact civ1win names), then a
     * case-insensitive directory scan: on-disk case varies per file
     * (Civdata0.rsc vs CIVDATA1.RSC vs Civdata3.rsc) and ext4 is
     * case-sensitive, so exact/lower/upper guessing is not enough.
     * [PORT-SDL] Replaces: fopen probe -> SDL_GetPathInfo, opendir/
     * readdir scan -> SDL_EnumerateDirectory (case-insensitive match in
     * callback). Contract out/cap, TRUNCATED, NOENT unchanged. */
    size_t i;
    if (!name || !out || cap == 0)
        return CIV_ERR_NOENT;
    out[0] = '\0';
    for (i = 0; i < sizeof(s_asset_dirs) / sizeof(s_asset_dirs[0]); i++) {
        char cand[1024];
        SDL_PathInfo info;
        int n = snprintf(cand, sizeof(cand), "%s/%s", s_asset_dirs[i], name);
        if (n > 0 && (size_t)n < sizeof(cand)) {
            if (SDL_GetPathInfo(cand, &info) &&
                info.type == SDL_PATHTYPE_FILE) {
                if ((size_t)n + 1 > cap)
                    return CIV_ERR_TRUNCATED;
                memcpy(out, cand, (size_t)n + 1);
                return CIV_OK;
            }
        }
        /* Slow path: scan the directory case-insensitively via SDL. */
        {
            win16_asset_scan_t scan;
            scan.want = name;
            scan.hit[0] = '\0';
            scan.found = 0;
            scan.truncated = 0;
            (void)SDL_EnumerateDirectory(s_asset_dirs[i], win16_asset_cb,
                &scan);
            if (scan.found) {
                size_t hn = strlen(scan.hit) + 1;
                if (hn > cap)
                    return CIV_ERR_TRUNCATED;
                memcpy(out, scan.hit, hn);
                return CIV_OK;
            }
        }
    }
    return CIV_ERR_NOENT;
}

civ_error_t win16_probe_resources(void)
{
    /* CivData0..4.rsc presence probe (real load owns an rsc_mgr; that owner
     * lands with boot_load_resources; here: files must exist). */
    static const char *const names[] = {
        "CIVDATA0.RSC", "CIVDATA1.RSC", "CIVDATA2.RSC", "CIVDATA3.RSC",
        "CIVDATA4.RSC",
    };
    /* On-disk case varies (Civdata0.rsc vs CIVDATA1.RSC); win16_find_asset
     * already scans case-insensitively, so canonical names suffice. */
    size_t i;
    char hit[1024];
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        if (win16_find_asset(names[i], hit, sizeof(hit)) != CIV_OK)
            return CIV_ERR_NOENT;
    }
    return CIV_OK;
}

civ_error_t win16_probe_fonts(void)
{
    char hit[1024];
    if (win16_find_asset("civfonts.fon", hit, sizeof(hit)) == CIV_OK)
        return CIV_OK;
    if (win16_find_asset("CIVFONTS.FON", hit, sizeof(hit)) == CIV_OK)
        return CIV_OK;
    return CIV_ERR_NOENT;
}
