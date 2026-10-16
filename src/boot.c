/* Boot sequence. Order mirrors WinMain @ 1008:0000; see boot.h for anchors.
 * No exit() here; no globals. Each step delegates to its owning module. */
#include <stdint.h>
#include <stdlib.h>

#include "boot.h"

#include "audio.h"
#include "dialog.h"
#include "game.h"
#include "map.h"
#include "newgame.h"
#include "sys.h"
#include "video.h"
#include "win16.h"

civ_error_t boot_setup_paths(void)
{
    /* WinMain prologue: Sys_GetCurrentDrive @ 1000:11bb +
     * Sys_BuildCurrentDirPath @ 1000:2826 (see sys.h). */
    return sys_init_paths();
}

civ_error_t boot_check_memory(void)
{
    /* Boot step delegating to Orig FUN_11e8_0000 @ 11e8:0000 ->
     * Sys_CheckFreeMemory (see sys.h sys_check_free_memory; GETFREESPACE
     * + 1420:43b8 msg, SDL_GetSystemRAM TBD). */
    return sys_check_free_memory();
}

civ_error_t app_register_window_classes(void)
{
    /* Orig: FUN_1098_06f8 @ 1098:06f8 -> App_RegisterWindowClasses.
     * REGISTERCLASS x4 + Ctrl_RegisterCustomClasses @ 1058:0b4d (see
     * dialog.h) + GETSYSTEMMETRICS. WONTFIX-Win16: SDL3 has no window
     * classes; delegate win16_register_classes (no-op CIV_OK). No globals,
     * no exit(), read-only. */
    return win16_register_classes();
}

civ_error_t boot_register_classes(void)
{
    /* Boot step delegating to Orig FUN_1098_06f8 @ 1098:06f8 ->
     * App_RegisterWindowClasses (above). */
    return app_register_window_classes();
}

civ_error_t boot_load_fonts(void)
{
    /* WinMain font step: Font_EnumFamiliesInit @ 1008:0e99 +
     * ADDFONTRESOURCE(CIV_FONT_FILE) (see boot.h). */
    return font_enum_families_init();
}

civ_error_t boot_load_resources(void)
{
    /* 5x ResMgr_LoadRsc @ 1190:0000 (CivData0..4.rsc via rsc_loader).
     * Linux: probe via win16_probe_resources (read-only search civ1win/./
     * unpacked, case variants); the owning rsc_mgr load lands with the
     * RSC consumer (no globals here). Missing files -> NOENT (honest stop,
     * not a fake OK). */
    return win16_probe_resources();
}

civ_error_t boot_check_video(void)
{
    /* Video_CheckDriverCaps @ 1008:059a (see video.h
     * video_check_driver_caps). WONTFIX-Win16 checklist: GETDEVICECAPS has
     * no Linux equivalent; the live renderer check runs where the renderer
     * exists (boot_open_windows via video_init). This order step is CIV_OK. */
    return CIV_OK;
}

civ_error_t boot_maybe_convert_artwork(void)
{
    /* WinMain artwork step delegating to Orig FUN_10b8_0000 @ 10b8:0000
     * -> Art_MaybeConvertArtwork (below; 1420:20a9 prompt, gates
     * WorldGen_CopyPreset @ 10b8:079d, see worldgen.h). */
    return art_maybe_convert_artwork();
}

civ_error_t boot_load_prefs(void)
{
    /* WinMain prefs step: Config_LoadPrefs @ 1008:0b4a (see boot.h).
     * Linux: built-in defaults (config_prefs_defaults, sounds on); civ.prf
     * file I/O stays in config_load_prefs STUB (read-only, never creates).
     * Boot proceeds on defaults -> CIV_OK. */
    (void)config_prefs_defaults();
    return CIV_OK;
}

civ_error_t boot_init_palette(void)
{
    /* WinMain palette step: Palette_Init @ 1090:0488 (see video.h). */
    return palette_init();
}

civ_error_t boot_init_tables(void)
{
    /* Orig: FUN_11e8_0337 @ 11e8:0337 -> Game_InitGlobalTables. Called
     * once from WinMain after Palette_Init @ 1090:0488, before
     * Main_RunStartupSequence @ 11e0:0000. Orig shape: key-remap loop +
     * mem_alloc_lock(64000) -> 1420:b394/b396 + Rng_Init @ 11c8:0000 +
     * string tables via Tribe_GetNameList @ 11f8:0080 (see newgame.h).
     * Linux: WONTFIX-Win16 lock selector; the 64 KB map buffer is carved
     * from the 16 MB startup arena (win16_mem_init + arena_alloc, owner
     * is map.c). No globals here; read-only; no exit(). */
    uint8_t key_remap[256];
    unsigned i;
    civ_error_t err;

    /* 1) Key-remap loop (OS-free shape 1:1; exact remap pairs TBD from
     * the 11e8:0337 dump, so identity skeleton for now). */
    for (i = 0; i < sizeof(key_remap); i++)
        key_remap[i] = (uint8_t)i;
    (void)key_remap; /* consumed once the dump gives the remap pairs */

    /* 2) Rng_Init @ 11c8:0000 (live via sys.h). */
    rng_init();

    /* 3) Arena: proves + reserves the host backing for the 1420:b394/b396
     * map buffer (see map.h CIV_MAP_BUF_LEN). Persistent carve happens in
     * map_init; here only the arena readiness gate. */
    err = win16_mem_init();
    if (err != CIV_OK)
        return err;
    if (!win16_arena_alloc(0u + WIN16_MAP_BUF_LEN))
        return CIV_ERR_NOMEM;

    /* 4) String-table anchor (see newgame.h): exercises the
     * Tribe_GetNameList @ 11f8:0080 dependency without requiring the
     * seg 1420 dump yet (still NULL stub). */
    (void)tribe_get_name_list(0);

    /* 5) Sibling tables (see below): Init_UnitFontTables @ 10e8:2d46. */
    (void)boot_init_unit_font_tables();

    return CIV_OK;
}

civ_error_t boot_init_unit_font_tables(void)
{
    /* Orig: FUN_10e8_2d46 @ 10e8:2d46 -> Init_UnitFontTables. Sibling of
     * NewGame_Wizard @ 10e8:0000; loads FIRSTMOVE/PILLAGE/font-map
     * tables. Segment dump has no decoded table bytes yet, so no pure
     * tables/memcpy port is possible -> stub CIV_ERR_UNSUPPORTED.
     * Read-only; no globals; no exit(). */
    return CIV_ERR_UNSUPPORTED;
}

void cursor_set(int cursor_id)
{
    /* Orig: FUN_1098_096c @ 1098:096c -> Cursor_Set. (cursor_id) void,
     * called after Menu_InitMainBar @ 1048:0000. LOADCURSOR + SETCURSOR
     * Win16 cursor install -> SDL3 system-cursor bridge TBD (future
     * SDL_CreateSystemCursor owner documented in boot.h/video.h; impl
     * stays here per packet I scope, video.* untouched). Stub: takes
     * the id for WinMain-order fidelity, installs nothing. */
    (void)cursor_id;
}

civ_error_t boot_open_windows(SDL_Window *win, SDL_Renderer *ren)
{
    civ_error_t err;

    /* Orig: GfxPort_Create @ 10d0:0000 (GR_PortOpen) +
     * MainPort_GetOrCreate @ 1098:114a + WdwMap_Open @ 1208:0af1 +
     * WdwSmMap_Open @ 1210:025b + WdwStatus_Open @ 1218:0386 (see video.h
     * gfx_port_create/mainport_get_or_create + map.h map_open_main/small/
     * status). SDL3: SDL_Window/SDL_Renderer are caller-owned (created by
     * main.c, borrowed here, never Destroyed). Partial port: video_init +
     * map_init are live; Win16 CREATEWINDOW/scroll-range/SETTIMER tails
     * stay stubbed -> overall CIV_ERR_UNSUPPORTED. No globals, no exit(),
     * read-only. */
    err = video_init(win, ren);
    if (err != CIV_OK)
        return err;
    err = map_init();
    if (err != CIV_OK)
        return err;
    /* WONTFIX-Win16 tails: GfxPort_Create @ 10d0:0000 + MainPort_GetOrCreate
     * @ 1098:114a + WdwMap_Open @ 1208:0af1 + WdwSmMap_Open @ 1210:025b +
     * WdwStatus_Open @ 1218:0386 CREATEWINDOW/scroll-range/SETTIMER (see
     * map.h). SDL3: window/renderer caller-owned; ports are textures. */
    return CIV_OK;
}

civ_error_t menu_init_mainbar(void)
{
    /* Orig: FUN_1048_0000 @ 1048:0000 -> Menu_InitMainBar.
     * CHECKMENUITEM + MODIFYMENU main-bar build/update ("&Save
     * Game\tCtrl+S" @ 14b0:0010, "&Auto Save" @ 14b0:0092, "Save First"
     * @ 14d0:0028; see dialog.h). WONTFIX-Win16: no USER menu handles on
     * SDL3 (commands via SDL UI later); strings stay LIVE in menu_strings.
     * No-op CIV_OK. */
    return CIV_OK;
}

civ_error_t boot_init_menu(void)
{
    civ_error_t err;

    /* WinMain menu step: Menu_InitMainBar @ 1048:0000 (menu_init_mainbar
     * below; CHECKMENUITEM/MODIFYMENU) + Cursor_Set @ 1098:096c
     * (cursor_set() void stub in this file; SDL_CreateSystemCursor bridge
     * TBD, installs nothing) + dialog_init (DLGPROC layer, see dialog.h).
     * Partial port: cursor step is live (void, order fidelity); menu +
     * dialog tails stay stubbed -> overall CIV_ERR_UNSUPPORTED. No
     * globals, no exit(), read-only. */
    err = menu_init_mainbar();
    if (err != CIV_OK)
        return err;
    err = dialog_init();
    if (err != CIV_OK)
        return err;
    /* Cursor_Set @ 1098:096c order fidelity (void, always live). */
    cursor_set(0);
    return CIV_OK;
}

/* --- Packet F: font / prefs / DOS guards (see boot.h anchors) --- */

int dos_drive_ready(int drive)
{
    /* Orig: FUN_1000_1184 @ 1000:1184 -> Dos_DriveReady. OS-free Linux
     * equivalent: no removable-drive concept -> always ready (1). */
    (void)drive;
    return 1;
}

civ_error_t dos_set_path(int drive, const char *path)
{
    /* Orig: FUN_1000_116a @ 1000:116a -> Dos_SetPath. Linux: no drive
     * letters, cwd owned by the launcher -> no-op. */
    (void)drive;
    if (!path)
        return CIV_ERR_NOENT;
    return CIV_OK;
}

civ_error_t font_enum_families_init(void)
{
    /* Orig FUN_1008_0e99 @ 1008:0e99 -> Font_EnumFamiliesInit.
     * ENUMFONTFAMILIES enumeration callback + ADDFONTRESOURCE(CIV_FONT_FILE
     * "civfonts.fon" @ 1420:00a4/00fe). WONTFIX-Win16 GDI install: faces are
     * parsed on demand via font_fon_parse/font_fnt_* (bundled FNT raster,
     * no fontconfig/TTF); presence probe below is read-only, never writes
     * civ1win/ (REMOVEFONTRESOURCE tail in video_shutdown_display). */
    return win16_probe_fonts();
}

/* AF2: FON header port (see boot.h font_fon_info_t). OS-free, no SDL. */
static uint16_t font_fon_u16le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t font_fon_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

civ_error_t font_fon_parse(const uint8_t *data, size_t len,
                           font_fon_info_t *out)
{
    /* Orig Font_EnumFamiliesInit @ 1008:0e99 FON header side (ADDFONTRESOURCE
     * CIV_FONT_FILE @ 1420:00a4/00fe). OS-free 1:1 header check on borrowed
     * bytes (caller-owned, no copy, never writes civfonts.fon): MZ at +0,
     * e_lfanew u32 LE at +0x3C -> NE sig -> resource-table walk counting
     * RT_FONTDIR (0x8007) / RT_FONT (0x8008). Dump-confirmed (unpacked/
     * civfonts.fon 244224 B, read-only): e_lfanew 0x250, align shift 9,
     * fontdir x1 (file 0x600, nfonts 0x15), faces x21. No SDL (parse only),
     * no globals, no exit(). TODO: per-face FNT decode (df metrics +
     * bitmap bits behind the FONTDIR entry; face names CIVBABYLON..
     * CIVTIMES36 at file 0x600.. + resident "FONTRES") -> SDL_ttf bridge
     * TBD (no TTF raster here, see font_face_list stub below). */
    uint32_t ne_off;
    uint16_t res_off;
    size_t rt;
    size_t p;
    uint16_t fontdir_count = 0;
    uint16_t face_count = 0;

    if (!data || !out)
        return CIV_ERR_NOENT;
    if (len < CIV_FON_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    if (data[0] != CIV_FON_MAGIC_MZ0 || data[1] != CIV_FON_MAGIC_MZ1)
        return CIV_ERR_BAD_DATA;
    ne_off = font_fon_u32le(data + 0x3Cu);
    if ((size_t)ne_off + CIV_FON_NE_MIN_SIZE > len)
        return CIV_ERR_TRUNCATED;
    if (data[ne_off] != CIV_FON_MAGIC_NE0 ||
        data[ne_off + 1u] != CIV_FON_MAGIC_NE1)
        return CIV_ERR_BAD_DATA;
    res_off = font_fon_u16le(data + ne_off + 0x24u);
    rt = (size_t)ne_off + (size_t)res_off;
    if (rt + 2u > len)
        return CIV_ERR_TRUNCATED;
    out->ne_offset = ne_off;
    out->align_shift = font_fon_u16le(data + rt);
    p = rt + 2u;
    for (;;) {
        uint16_t rtype;
        uint16_t rcount;

        if (p + 8u > len)
            return CIV_ERR_TRUNCATED;
        rtype = font_fon_u16le(data + p);
        if (rtype == 0u)
            break;
        rcount = font_fon_u16le(data + p + 2u);
        if (rtype == CIV_FON_RES_FONTDIR)
            fontdir_count = rcount;
        else if (rtype == CIV_FON_RES_FONT)
            face_count = rcount;
        p += 8u;
        if (p + (size_t)rcount * 12u > len)
            return CIV_ERR_TRUNCATED;
        p += (size_t)rcount * 12u;
    }
    out->fontdir_count = fontdir_count;
    out->face_count = face_count;
    return CIV_OK;
}

int font_face_count(const font_fon_info_t *info)
{
    /* Orig ENUMFONTFAMILIES @ 1008:0e99 face-count side. OS-free: parsed
     * RT_FONT count (21 on disk); 0 on NULL. Names via font_face_list
     * below (order TBD, still NULL stub). No SDL, no globals, no exit(). */
    if (!info)
        return 0;
    return (int)info->face_count;
}

const char *font_face_list(const font_fon_info_t *info, int index)
{
    /* Orig ENUMFONTFAMILIES @ 1008:0e99 family-name side. OS-free stub:
     * index -> family name once the fontdir-string order (CIVBABYLON..
     * CIVTIMES36 at file 0x600 + resident "FONTRES", 21 faces) is wired
     * to the enumeration order; NULL until then (same contract as
     * tribe_get_name_list / boot_video_caps_describe). NULL info or
     * out-of-range -> NULL. No SDL, no globals, no exit(), read-only. */
    if (!info || index < 0 || index >= (int)info->face_count)
        return NULL;
    /* TODO: wire fontdir face strings in ENUMFONTFAMILIES order. */
    return NULL;
}

/* AG2: FNT raster + menu strings (see boot.h). OS-free, no SDL. Reuses the
 * font_fon_u16le/u32le helpers above (packed LE reads, no alignment). */
civ_error_t font_fnt_parse(const uint8_t *data, size_t len,
                            font_fnt_info_t *out)
{
    /* Orig Font_EnumFamiliesInit @ 1008:0e99 FNT header side. Packed
     * Windows 2.x/3.x FNT header through dfBitsOffset (117 B); dfReserved
     * parsed only when the image holds it. Borrowed bytes, never writes
     * civfonts.fon. TODO: bitmap rows behind dfBitsOffset + dfCharTable. */
    uint16_t version;
    uint32_t size;
    size_t p;
    unsigned i;

    if (!data || !out)
        return CIV_ERR_NOENT;
    if (len < CIV_FNT_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    version = font_fon_u16le(data + 0x00u);
    if (version != CIV_FNT_VERSION_2 && version != CIV_FNT_VERSION_3)
        return CIV_ERR_BAD_DATA;
    size = font_fon_u32le(data + 0x02u);
    if (size > (uint32_t)len)
        return CIV_ERR_TRUNCATED;
    if (size != 0u && size < CIV_FNT_MIN_SIZE)
        return CIV_ERR_BAD_DATA;
    out->dfVersion = version;
    out->dfSize = size;
    for (i = 0; i < CIV_FNT_COPYRIGHT_LEN; i++)
        out->dfCopyright[i] = (char)data[0x06u + i];
    out->dfType = font_fon_u16le(data + 0x42u);
    out->dfPoints = font_fon_u16le(data + 0x44u);
    out->dfVertRes = font_fon_u16le(data + 0x46u);
    out->dfHorizRes = font_fon_u16le(data + 0x48u);
    out->dfAscent = font_fon_u16le(data + 0x4Au);
    out->dfInternalLeading = font_fon_u16le(data + 0x4Cu);
    out->dfExternalLeading = font_fon_u16le(data + 0x4Eu);
    out->dfItalic = data[0x50u];
    out->dfUnderline = data[0x51u];
    out->dfStrikeOut = data[0x52u];
    out->dfWeight = font_fon_u16le(data + 0x53u);
    out->dfCharSet = data[0x55u];
    out->dfPixWidth = font_fon_u16le(data + 0x56u);
    out->dfPixHeight = font_fon_u16le(data + 0x58u);
    out->dfPitchAndFamily = data[0x5Au];
    out->dfAvgWidth = font_fon_u16le(data + 0x5Bu);
    out->dfMaxWidth = font_fon_u16le(data + 0x5Du);
    out->dfFirstChar = data[0x5Fu];
    out->dfLastChar = data[0x60u];
    out->dfDefaultChar = data[0x61u];
    out->dfBreakChar = data[0x62u];
    out->dfWidthBytes = font_fon_u16le(data + 0x63u);
    out->dfDevice = font_fon_u32le(data + 0x65u);
    out->dfFace = font_fon_u32le(data + 0x69u);
    out->dfBitsPointer = font_fon_u32le(data + 0x6Du);
    out->dfBitsOffset = font_fon_u32le(data + 0x71u);
    out->dfReserved = (len >= 0x76u) ? data[0x75u] : 0u;
    if (out->dfFirstChar > out->dfLastChar)
        return CIV_ERR_BAD_DATA;
    if (out->dfFace != 0u && (size_t)out->dfFace >= len)
        return CIV_ERR_BAD_DATA;
    if (out->dfBitsOffset != 0u && (size_t)out->dfBitsOffset >= len)
        return CIV_ERR_BAD_DATA;
    p = (size_t)(out->dfLastChar - out->dfFirstChar + 1u);
    (void)p; /* char-table extent validated by font_face_bitmap bounds */
    return CIV_OK;
}

civ_error_t font_face_bitmap(const uint8_t *data, size_t len,
                             const font_fnt_info_t *info, int c,
                             int *w_out, int *h_out,
                             const uint8_t **bits_out)
{
    /* Orig Font_EnumFamiliesInit @ 1008:0e99 FNT glyph side. LIVE thin
     * delegate to font_fnt_bits below (bounds + v2 dfCharTable width/file
     * offset + sentinel length; v3 6-B entries stay UNSUPPORTED inside
     * font_fnt_char_entry). Per-row locating lives in font_fnt_row_ptr
     * (row_addr = bits + row*stride). No SDL, no globals, no exit(). */
    if (!data || !info || !w_out || !h_out || !bits_out)
        return CIV_ERR_NOENT;
    if (len < CIV_FNT_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    return font_fnt_bits(data, len, info, c, bits_out, w_out, h_out);
}

/* Orig Font_EnumFamiliesInit @ 1008:0e99 FNT row locating (goal 2).
 * Row address inside one glyph: row_addr = glyph_base + row * stride,
 * where glyph_base = data + char_entry.offset (dfCharTable file offset),
 * stride = (dfMaxWidth+7)/8 (source pitch, NOT dfWidthBytes which stays 2
 * on disk; 2 B for max 13, 5 B for max 37), rows 0..dfPixHeight-1
 * top-to-bottom (row 0 = top, row h-1 = bottom, no flipping), bit x of a
 * row lives in row[x/8] MSB-first (bit 7 = leftmost, only the first
 * `width` bits valid). OS-free 1:1 on borrowed bytes. No SDL, no globals,
 * no exit(), read-only. */
civ_error_t font_fnt_row_ptr(const uint8_t *data, size_t len,
                             const font_fnt_info_t *info, int c, int row,
                             const uint8_t **row_out)
{
    const uint8_t *bits = NULL;
    int w_tmp = 0;
    int h_tmp = 0;
    unsigned stride;
    civ_error_t err;

    if (row_out)
        *row_out = NULL;
    if (!data || !info || !row_out)
        return CIV_ERR_NOENT;
    if (len < CIV_FNT_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    err = font_fnt_bits(data, len, info, c, &bits, &w_tmp, &h_tmp);
    if (err != CIV_OK)
        return err;
    if (!bits || h_tmp <= 0)
        return CIV_ERR_BAD_DATA;
    if (row < 0 || row >= h_tmp)
        return CIV_ERR_RANGE;
    stride = ((unsigned)info->dfMaxWidth + 7u) / 8u;
    if (stride == 0u)
        return CIV_ERR_BAD_DATA;
    *row_out = bits + (size_t)row * (size_t)stride;
    return CIV_OK;
}

/* Orig Font_EnumFamiliesCallback @ 1008:0e12 slot-install side (goal 3).
 * The callback installs one GDI face per ENUMFONTFAMILIES hit via
 * CREATEFONTINDIRECT into DAT_1420_5484[DAT_12d8_0022++] (slot array freed
 * in Font_FreeSlots @ 1008:062f, was FUN_1008_062f, over 0x1e entries;
 * install happens once at boot inside Font_EnumFamiliesInit @ 1008:0e99,
 * selection happens later per-draw via Text_SelectFont @ 1100:0a33 /
 * Text_DispatchFontHandler @ 11e8:0b40, table @ 11e8:0d1d 25 id ->
 * 20 slotów 0x00..0x14, patrz boot.h chain-14 + dialog.h text_font_table).
 * STUB: slot-range half is LIVE (0..0x1d valid, else RANGE), the
 * CREATEFONTINDIRECT/ADDFONTRESOURCE GDI install has no Linux equivalent
 * yet (bridge projekt: see boot.h chain-13/14, bundled-FNT raster first,
 * no fontconfig, no TTF here) -> CIV_ERR_UNSUPPORTED. No globals,
 * no exit(). */
civ_error_t font_install_slot(int slot)
{
    if (slot < 0 || slot >= CIV_FONT_SLOT_MAX)
        return CIV_ERR_RANGE;
    /* TODO: CREATEFONTINDIRECT install into the face-slot table
     * (DAT_1420_5484 image) + ADDFONTRESOURCE(CIV_FONT_FILE) bridge. */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (14): fontdir/FNT face name + slot alloc + tail ---
 * See boot.h chain-14 (fontdir file 0x600 C strings, FNT base+dfFace,
 * 21/21 v2, 0x1e slots, Tail empty stub). OS-free, no SDL. */

civ_error_t font_fnt_face_name(const uint8_t *data, size_t len,
                               const font_fnt_info_t *info,
                               const char **name_out)
{
    /* Orig Font_EnumFamiliesInit @ 1008:0e99 FNT face-name side (dfFace
     * C string at base+dfFace, resource-relative; e.g. unpacked/civfonts.fon
     * 0x1400+0x18c4 CIVBABYLON .. 0x30e00+0xaaf8 CIVTIMES36 — te same co
     * fontdir 0x676..0x1054; format C NUL, nie Pascal). Borrowed view. */
    size_t off;
    size_t pos;

    if (name_out)
        *name_out = NULL;
    if (!data || !info || !name_out)
        return CIV_ERR_NOENT;
    if (len < CIV_FNT_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    if (info->dfFace == 0u)
        return CIV_ERR_BAD_DATA;
    off = (size_t)info->dfFace;
    if (off >= len)
        return CIV_ERR_BAD_DATA;
    pos = off;
    while (pos < len && data[pos] != 0u)
        pos++;
    if (pos >= len)
        return CIV_ERR_BAD_DATA;
    *name_out = (const char *)(data + off);
    return CIV_OK;
}

civ_error_t font_slot_claim(int *count, int *slot_out)
{
    /* Orig Font_EnumFamiliesCallback @ 1008:0e12 alloc half
     * (DAT_1420_5484[DAT_12d8_0022++] bez guarda w orig; 21 twarzy < 0x1e
     * więc bezpiecznie w praktyce; tu twardy RANGE przy >= 0x1e żeby nie
     * nadpisać za tablicą gdyby ENUM zwrócił > 30 hitów). */
    if (slot_out)
        *slot_out = -1;
    if (!count || !slot_out)
        return CIV_ERR_NOENT;
    if (*count < 0 || *count >= CIV_FONT_SLOT_MAX)
        return CIV_ERR_RANGE;
    *slot_out = *count;
    (*count)++;
    return CIV_OK;
}

civ_error_t font_free_slots(int *slots, int n, int *count)
{
    /* Orig FUN_1008_062f @ 1008:062f -> Font_FreeSlots (see boot.h):
     * loop SI 0..0x1d over DAT_1420_5484, DELETEOBJECT non-zero handles.
     * OS-free slot-state half LIVE 1:1 on explicit caller-owned array
     * (slots[0..n), n slots; orig n is fixed 0x1e) + count image
     * (DAT_12d8_0022, reset to 0): zeroes every entry (zeros stay zero,
     * mirroring the CMP-skip), resets *count. GDI DELETEOBJECT per
     * handle stays caller-side (no Linux equivalent, like install in
     * font_install_slot STUB). NULL slots/count -> NOENT; n < 0 or n >
     * CIV_FONT_SLOT_MAX (0x1e) -> RANGE (orig fixed 0x1e; larger would
     * overrun DAT_1420_5484). No globals, no exit(). */
    int i;

    if (!slots || !count)
        return CIV_ERR_NOENT;
    if (n < 0 || n > CIV_FONT_SLOT_MAX)
        return CIV_ERR_RANGE;
    for (i = 0; i < n; i++)
        slots[i] = 0;
    *count = 0;
    return CIV_OK;
}

void font_enum_families_callback_tail(void)
{
    /* Orig Font_EnumFamiliesCallbackTail @ 1008:09d3: pusty far stub
     * (16 B: MOV AX,DS/NOP/INC BP/PUSH BP/MOV BP,SP/PUSH DS/MOV DS,AX/
     * JMP 09df/POP DS/POP BP/DEC BP/RETF, bez ciała; dekompilator dopowiada
     * fikcyjny return — ignorować). Wołany zawsze z ogona callbacku
     * @ 1008:0e12 (return ignorowany); brak innych xrefs; rodzeństwo
     * 1008:0d4c/0d5a/0df0/0e02 to puste bezimienne stub-y tego samego
     * kształtu (placeholder linkera / thunk pod hook, jak
     * Text_FontPreApply @ 1098:0186 / PostApply @ 1098:0194 wołane tylko
     * z ogona dispatch 11e8:0cb0). NOOP. */
}

civ_error_t font_fnt_char_entry(const uint8_t *data, size_t len,
                                const font_fnt_info_t *info, int c,
                                int *width_out, uint32_t *offset_out)
{
    /* Orig Font_EnumFamiliesInit @ 1008:0e99 FNT dfCharTable side (AH2).
     * OS-free header/entry-only 1:1 on borrowed bytes (caller-owned FNT
     * image, never writes civfonts.fon). v2 4-B entries (u16 width + u16
     * offset); v3 6-B STUB z powodem poniżej. No SDL, no globals, no
     * exit(), read-only. */
    size_t ct_off;
    unsigned nchars;
    unsigned idx;
    size_t epos;
    uint16_t w;
    uint16_t off;

    if (!data || !info || !width_out || !offset_out)
        return CIV_ERR_NOENT;
    if (len < CIV_FNT_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    if (info->dfVersion != CIV_FNT_VERSION_2 &&
        info->dfVersion != CIV_FNT_VERSION_3)
        return CIV_ERR_BAD_DATA;
    if (info->dfVersion == CIV_FNT_VERSION_3)
        /* v3 6-B entries (width u16 + offset u32? dokładny layout TBD):
         * STUB z powodem — przeskanowano unpacked/civfonts.fon read-only
         * (21 base 0x1400..0x30e00, dfVersion na base+0x00): 21/21 to
         * 0x0200 (v2), zero 0x0300 w pliku, więc brak próbki do walidacji
         * parsowania 6-B (nie zgadywać, nie mis-parsować). */
        return CIV_ERR_UNSUPPORTED;
    if (info->dfFirstChar > info->dfLastChar)
        return CIV_ERR_BAD_DATA;
    if (c < (int)info->dfFirstChar || c > (int)info->dfLastChar)
        return CIV_ERR_RANGE;
    nchars = (unsigned)info->dfLastChar - (unsigned)info->dfFirstChar + 1u;
    /* Header 118 B with dfReserved (0x76) when len allows it, else 117 B
     * (0x75); all disk faces use 118 (bitsOff 0x352 == 0x76 + 183*4). */
    ct_off = (len >= 0x76u) ? 0x76u : 0x75u;
    if (len < ct_off)
        return CIV_ERR_TRUNCATED;
    /* Table has nchars+1 entries (sentinel for last glyph length). */
    if (nchars + 1u > (len - ct_off) / 4u)
        return CIV_ERR_TRUNCATED;
    idx = (unsigned)(c - (int)info->dfFirstChar);
    epos = ct_off + (size_t)idx * 4u;
    if (epos + 4u > len)
        return CIV_ERR_TRUNCATED;
    w = font_fon_u16le(data + epos);
    off = font_fon_u16le(data + epos + 2u);
    if ((size_t)off >= len)
        return CIV_ERR_BAD_DATA;
    *width_out = (int)w;
    *offset_out = (uint32_t)off;
    return CIV_OK;
}

civ_error_t font_fnt_bits(const uint8_t *data, size_t len,
                          const font_fnt_info_t *info, int c,
                          const uint8_t **bits_out,
                          int *width_out, int *height_out)
{
    /* Orig Font_EnumFamiliesInit @ 1008:0e99 FNT glyph-bits side (AH2).
     * OS-free 1:1 on borrowed bytes via font_fnt_char_entry + sentinel
     * next-offset for length; MSB-first, stride (max+7)/8, height pixHeight.
     * No SDL, no globals, no exit(), read-only. */
    size_t ct_off;
    unsigned nchars;
    unsigned idx;
    size_t table_end;
    size_t epos_next;
    int w_tmp;
    uint32_t off_tmp;
    uint16_t next_off;
    civ_error_t err;

    if (!data || !info || !bits_out || !width_out || !height_out)
        return CIV_ERR_NOENT;
    if (len < CIV_FNT_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    err = font_fnt_char_entry(data, len, info, c, &w_tmp, &off_tmp);
    if (err != CIV_OK)
        return err;
    if (info->dfPixHeight == 0u)
        return CIV_ERR_BAD_DATA;
    nchars = (unsigned)info->dfLastChar - (unsigned)info->dfFirstChar + 1u;
    ct_off = (len >= 0x76u) ? 0x76u : 0x75u;
    table_end = ct_off + (size_t)(nchars + 1u) * 4u;
    if (table_end > len)
        return CIV_ERR_TRUNCATED;
    if ((size_t)off_tmp < table_end)
        return CIV_ERR_BAD_DATA;
    if (info->dfBitsOffset != 0u && off_tmp < info->dfBitsOffset)
        return CIV_ERR_BAD_DATA;
    idx = (unsigned)(c - (int)info->dfFirstChar);
    epos_next = ct_off + (size_t)(idx + 1u) * 4u;
    if (epos_next + 4u > len)
        return CIV_ERR_TRUNCATED;
    next_off = font_fon_u16le(data + epos_next + 2u);
    if ((size_t)next_off <= (size_t)off_tmp)
        return CIV_ERR_BAD_DATA;
    if ((size_t)next_off > len)
        return CIV_ERR_BAD_DATA;
    *bits_out = data + (size_t)off_tmp;
    *width_out = w_tmp;
    *height_out = (int)info->dfPixHeight;
    return CIV_OK;
}

/* AI-A: FNT glyph render + text metrics (see boot.h). OS-free, no SDL. */
civ_error_t font_render_glyph(const uint8_t *data, size_t len,
                              const font_fnt_info_t *info, int c,
                              uint8_t *dst, size_t dst_cap,
                              int *w_out, int *h_out)
{
    /* Orig Font_EnumFamiliesInit @ 1008:0e99 FNT glyph-bits side
     * (dfBitsOffset + dfCharTable). OS-free bit-copy 1:1 via font_fnt_bits:
     * borrowed MSB-first rows with source stride (dfMaxWidth+7)/8 repacked
     * tightly into caller-owned dst with stride (w+7)/8. No SDL, no
     * globals, no exit(), read-only (writes only caller-owned dst). */
    const uint8_t *src_bits = NULL;
    int w_tmp = 0;
    int h_tmp = 0;
    civ_error_t err;
    unsigned src_stride;
    unsigned dst_stride;
    size_t need;
    int y;
    int x;
    size_t i;

    if (!data || !info || !dst || !w_out || !h_out)
        return CIV_ERR_NOENT;
    if (len < CIV_FNT_MIN_SIZE)
        return CIV_ERR_TRUNCATED;
    err = font_fnt_bits(data, len, info, c, &src_bits, &w_tmp, &h_tmp);
    if (err != CIV_OK)
        return err;
    if (!src_bits)
        return CIV_ERR_BAD_DATA;
    if (w_tmp < 0 || h_tmp <= 0)
        return CIV_ERR_BAD_DATA;
    if (w_tmp == 0) {
        *w_out = 0;
        *h_out = h_tmp;
        return CIV_OK;
    }
    src_stride = ((unsigned)info->dfMaxWidth + 7u) / 8u;
    if (src_stride == 0u)
        return CIV_ERR_BAD_DATA;
    if ((unsigned)w_tmp > src_stride * 8u)
        return CIV_ERR_BAD_DATA;
    dst_stride = ((unsigned)w_tmp + 7u) / 8u;
    need = (size_t)dst_stride * (size_t)h_tmp;
    if (dst_cap < need)
        return CIV_ERR_TRUNCATED;
    for (i = 0; i < need; i++)
        dst[i] = 0u;
    for (y = 0; y < h_tmp; y++) {
        for (x = 0; x < w_tmp; x++) {
            unsigned sx = (unsigned)x;
            unsigned src_idx = (unsigned)y * src_stride + sx / 8u;
            unsigned src_shift = 7u - (sx % 8u);
            unsigned bit;
            unsigned dst_idx;
            unsigned dst_shift;

            bit = (unsigned)((src_bits[src_idx] >> src_shift) & 1u);
            if (bit == 0u)
                continue;
            dst_idx = (unsigned)y * dst_stride + sx / 8u;
            dst_shift = 7u - (sx % 8u);
            dst[dst_idx] = (uint8_t)(dst[dst_idx] | (uint8_t)(1u << dst_shift));
        }
    }
    *w_out = w_tmp;
    *h_out = h_tmp;
    return CIV_OK;
}

int font_text_width(const uint8_t *data, size_t len,
                    const font_fnt_info_t *info, const char *s)
{
    /* Orig status stride @ 1098:0dda + Text_DrawStatusRow @ 11f8:0278 width
     * side. OS-free sum of per-char entry widths (tracking 0, see boot.h).
     * NULL -> 0. No SDL, no globals, no exit(), read-only. */
    int total = 0;
    size_t i;

    if (!data || !info || !s)
        return 0;
    if (len < CIV_FNT_MIN_SIZE)
        return 0;
    for (i = 0; s[i] != '\0'; i++) {
        int c = (unsigned char)s[i];
        int w = 0;
        uint32_t off = 0u;
        civ_error_t err;

        err = font_fnt_char_entry(data, len, info, c, &w, &off);
        if (err != CIV_OK)
            continue;
        if (w < 0)
            continue;
        total += w;
    }
    return total;
}

int font_text_height(const font_fnt_info_t *info)
{
    /* Orig status stride @ 1098:0dda (tmHeight) via dfPixHeight (see
     * boot.h). OS-free, no SDL, no globals, no exit(), read-only. */
    if (!info)
        return -1;
    return (int)info->dfPixHeight;
}

int font_fnt_stride(int max_width)
{
    /* Orig FNT row stride in font_fnt_bits/font_render_glyph @ 1008:0e99
     * (source stride (dfMaxWidth+7)/8, MSB-first rows). */
    if (max_width < 0)
        return -1;
    return (max_width + 7) / 8;
}

int font_fnt_glyph_len(int off, int next_off)
{
    /* Orig FNT glyph span in font_fnt_bits @ 1008:0e99 (sentinel
     * next_off - off; BAD_DATA when next <= off). */
    if (next_off <= off)
        return -1;
    return next_off - off;
}

/* Orig Menu_InitMainBar @ 1048:0000 string side. OS-free static const port
 * (read-only, no Win16, never written); entry [3] mirrors
 * CIV_MAINMENU_TEXT @ 1420:435b (canonical owner see mainmenu.h). */
const char *const menu_strings[] = {
    "&Save Game\tCtrl+S", /* 14b0:0010 */
    "&Auto Save",           /* 14b0:0092 */
    "Save First",           /* 14d0:0028 */
    "_Start a New Game_Load a Saved Game_Play on EARTH_Customize World_View Hall of Fame_Quit", /* 1420:435b */
};

int menu_strings_count(void)
{
    /* OS-free count for the table above (same shape as font_face_count). */
    return (int)(sizeof(menu_strings) / sizeof(menu_strings[0]));
}

const char *menu_string_at(int index)
{
    /* Bounded accessor (NULL on OOB, same contract as font_face_list /
     * tribe_get_name_list / boot_video_caps_describe). */
    if (index < 0 || index >= menu_strings_count())
        return NULL;
    return menu_strings[index];
}

civ_error_t config_load_prefs(void)
{
    /* TODO: Orig FUN_1008_0b4a @ 1008:0b4a -> Config_LoadPrefs. _LOPEN
     * (OF_READ) "civ.prf" @ 1420:010b + _LREAD 0x1a do DAT_1420_17b2 +
     * _LCLOSE + flags 17ca -> g_nOptionFlags @ 12d0:0064 (brak pliku =
     * defaults). Format 26 B LIVE w config_prefs_unpack (chunki
     * 485a/48fa/4964 + flags); Win16 file-handle mostek TBD.
     * Read-only: never creates civ.prf. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t config_save_prefs(void)
{
    /* TODO: Orig FUN_1008_0bbf @ 1008:0bbf -> Config_SavePrefs. Guard
     * config_prefs_needs_path_setup (drive > 0 -> Dos_DriveReady @
     * 1000:1184 + Dos_SetPath @ 1000:116a) przed _LCREAT "civ.prf" @
     * 1420:0113 + _LLSEEK 0,0 + _LWRITE 0x1a z 17b2 + _LCLOSE. Obraz 26 B
     * LIVE w config_prefs_pack; Win16 file-handle mostek TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t boot_run(SDL_Window *win, SDL_Renderer *ren)
{
    /* Orig WinMain @ 1008:0000 order, SDL window/renderer caller-owned:
     * drive/path (boot_setup_paths live via sys_init_paths) -> memory
     * (boot_check_memory stub via Sys_CheckFreeMemory @ 11e8:0000) ->
     * classes (boot_register_classes stub via App_RegisterWindowClasses @
     * 1098:06f8) -> timer/accel/font (boot_load_fonts stub via
     * Font_EnumFamiliesInit @ 1008:0e99; 200 ms SETTIMER for TIMERPROC @
     * 1008:0d68 installed later by WdwMap_Open @ 1208:0af1) -> 5x RSC
     * (boot_load_resources stub via ResMgr_LoadRsc @ 1190:0000 CivData0..4)
     * -> video caps (boot_check_video stub via Video_CheckDriverCaps @
     * 1008:059a) -> artwork (boot_maybe_convert_artwork stub via
     * Art_MaybeConvertArtwork @ 10b8:0000) -> prefs (boot_load_prefs stub
     * via Config_LoadPrefs @ 1008:0b4a) -> palette (boot_init_palette stub
     * via Palette_Init @ 1090:0488) -> tables (boot_init_tables partial:
     * remap + rng_init + malloc probe live, Win16 lock stub) -> ports/map
     * windows (boot_open_windows partial: video_init + map_init live) ->
     * menu bar (boot_init_menu partial: cursor live, menu/dialog stub) ->
     * cursor (Cursor_Set @ 1098:096c void) -> event/turn loop caller-owned
     * (while(!g_nQuitFlag) + TIMERPROC body via game_tick ->
     * game_turn_tick @ 1088:0000, see game.h; main.c owns it). Wiring:
     * CIV_OK where OS-free (setup_paths live, tables partial but overall
     * UNSUPPORTED), otherwise CIV_ERR_UNSUPPORTED stub; first non-OK
     * short-circuits. No globals, no exit(), read-only. */
    static civ_error_t (*const steps[])(void) = {
        boot_setup_paths,
        boot_check_memory,
        boot_register_classes,
        boot_load_fonts,
        boot_load_resources,
        boot_check_video,
        boot_maybe_convert_artwork,
        boot_load_prefs,
        boot_init_palette,
        boot_init_tables,
    };
    size_t i;
    civ_error_t err;

    for (i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        err = steps[i]();
        if (err != CIV_OK)
            return err;
    }
    err = boot_open_windows(win, ren);
    if (err != CIV_OK)
        return err;
    err = boot_init_menu();
    if (err != CIV_OK)
        return err;
    err = audio_init();
    if (err != CIV_OK)
        return err;
    return CIV_OK;
}

void boot_shutdown(void)
{
    /* Orig: FUN_1008_0329 @ 1008:0329 -> WinMain_Shutdown. Win16 teardown
     * order reproduced against the SDL3 owners: KILLTIMER for TIMERPROC
     * @ 1008:0d68 (200 ms SETTIMER installed by WdwMap_Open @ 1208:0af1,
     * pumped from the WinMain @ 1008:0000 message loop, impl map_timer_proc
     * -> game_tick, see map.h/game.h; SDL timer bridge TBD) first, then
     * FREEPROCINSTANCE for the DLGPROC @ 1098:16e2 / CIVDIALOGPROC @
     * 1098:1838 MAKEPROCINSTANCE targets (see dialog.h dlg_proc /
     * civ_dialog_proc via Dlg_CreateTracked @ 1098:124f), DESTROYWINDOW
     * for the main/map/status windows (see map.h), and
     * REMOVEFONTRESOURCE(CIV_FONT_FILE "civfonts.fon" @ 1420:00a4/00fe,
     * added by Font_EnumFamiliesInit @ 1008:0e99). SDL3 tails below in
     * the same order (timers/windows via video/map, dialogs, audio MCI
     * close). Void stub: installs/removes nothing yet (Win16 handles
     * TBD); no globals, no exit(), read-only. */
    video_shutdown();
    map_shutdown();
    dialog_shutdown();
    audio_shutdown();
}

/* --- Packet S2: shutdown/startup (see boot.h anchors) --- */

civ_error_t art_maybe_convert_artwork(void)
{
    /* Orig: FUN_10b8_0000 @ 10b8:0000 -> Art_MaybeConvertArtwork.
     * LIVE no-op on Linux (see boot.h): the Win16 driver conversion
     * (Art_PortDataToBitmap @ 10b8:0e36 even-BITBLT vs odd-pad via
     * DAT_1420_17a6 / CREATECOMPATIBLEDC + SETBITMAPBITS + BITBLT per
     * row; 0xec palette via Palette_Init @ 1090:0488; LZW via
     * Art_DecodeLZW @ 10b8:158c; disk cache via Art_SaveConvertedFrame
     * @ 10b8:0c1c with Art_ShouldConvert @ 10b8:0ded = 1) has no Linux
     * equivalent — SDL3 INDEX8 textures take any width, palette via
     * SDL_SetPaletteColors, LZW via gfx_lzw_decode. Trivial CIV_OK. */
    return CIV_OK;
}

civ_error_t main_run_startup_sequence(void)
{
    /* Orig: FUN_11e0_0000 @ 11e0:0000 -> Main_RunStartupSequence (full order
     * 2026-10-16, P0c verified: Dlg_CreateTracked @ 1098:124f (0x94,0x41 =
     * CIV_DLG_INTRO_ID/CIV_DLG_INTRO_FLAGS, see dialog.h: center + show via
     * dlg_create_should_center/show, preset size via dlg_id_needs_preset_size,
     * ctl 0x65-0x6e via dlg_ctl_is_tracked_init, push via dlg_tracked_push_off)
     * + Gfx_StoreDC @ 1098:1104 (Gfx_SetCurrentDC @ 1100:09a9; SDL owner
     * gfx_store_dc, see video.h) + Palette_AnimateRange @ 1090:0376 +
     * Anim_OpenSequence @ 10b0:008a + DAT_13e0_001e = 0 + Sound_PlayId @
     * 1150:00c9 (3,0) + FILLRECT + Cursor_Set @ 1098:096c + loop#1
     * Anim_AdvanceFrames @ 10b0:0461 + Anim_CloseSequence @ 10b0:0287 +
     * Sound (1,0) + [main_startup_should_run_wizard above, g_nMenuChoice @
     * 12d8:44e2:] Sys_DrainInput @ 11f0:4ea0 + Anim_OpenSequence (0x91e) +
     * Anim_RebaseClock @ 10b0:005b (1000) + WorldGen_PrepareEarth @
     * 1108:01bc + loop#2 Advance + Close + Sound (1,0) +
     * Dlg_DestroyTracked @ 1098:1612 (pop via dlg_tracked_pop_count) +
     * NewGame_Wizard @ 10e8:0000; else DestroyTracked only. Dispatcher for
     * MainMenu_WndProc @ 11e0:0221 choices over CIV_MAINMENU_TEXT @
     * 1420:435b (see mainmenu.h). SDL modal/event bridge TBD -> stub
     * CIV_ERR_UNSUPPORTED. No globals, no exit(), read-only. */
    return CIV_ERR_UNSUPPORTED;
}

civ_prefs_t config_prefs_defaults(void)
{
    /* OS-free defaults 1:1 for civ.prf (Config_LoadPrefs @ 1008:0b4a /
     * Config_SavePrefs @ 1008:0bbf fill with these when the file is
     * missing/short): sounds on (g_nOptionFlags @ 12d0:0064 bit 0x10,
     * Sound_PlayId @ 1150:00c9 gate with DAT_1228:0104, see audio.h).
     * Remaining civ.prf fields TBD from the dump. By value, no I/O, no
     * globals, no exit(), read-only. */
    civ_prefs_t prefs;

    prefs.option_flags = CIV_PREFS_FLAG_SOUNDS_ON;
    return prefs;
}

int config_prefs_sounds_on(const civ_prefs_t *prefs)
{
    /* OS-free predicate for the sounds-on shape (Sound_PlayId gate head;
     * DAT_1228:0104 second half stays on the file/drive side, TBD). */
    if (!prefs)
        return 0;
    return (prefs->option_flags & CIV_PREFS_FLAG_SOUNDS_ON) ? 1 : 0;
}

int boot_video_caps_count(void)
{
    /* Video caps checklist @ 1420:0af4 entry count for
     * Video_CheckDriverCaps @ 1008:059a (GETDEVICECAPS; SDL mapping via
     * video_check_driver_caps, see video.h). Exact entry bytes not yet
     * decoded from the dump -> 0 until the dump lands. OS-free, no
     * globals, no exit(), read-only. */
    return 0;
}

const char *boot_video_caps_describe(int index)
{
    /* Checklist meaning by entry index (NULL when out of range or not
     * yet dumped, same contract as tribe_get_name_list, see newgame.h).
     * No entries decoded yet -> always NULL. */
    (void)index;
    return NULL;
}

/* --- Chain 2026-10-16 (16): shutdown order table @ 1008:0329 (goals 1/5).
 * See boot.h block. OS-free phase order LIVE as a read-only table (same
 * order as the disasm; Win16 handles/exit stay caller-side STUB). */

int boot_shutdown_step_count(void)
{
    /* 18 faz w kolejnosci WinMain_Shutdown @ 1008:0329 (see boot.h). */
    return 18;
}

const char *boot_shutdown_step_name(int index)
{
    /* Fazy j.w.; NULL on OOB (same contract as font_face_list /
     * tribe_get_name_list / boot_video_caps_describe). */
    static const char *const steps[18] = {
        "kill-timers",        /* KILLTIMER 460e + 28fa */
        "free-proc-instance", /* FREEPROCINSTANCE + Ctrl_FreeGdiObjects */
        "save-prefs",         /* Config_SavePrefs @ 1008:0bbf */
        "close-ports",        /* GfxPort_Close x5 @ 10d0:0172 */
        "dispose-palette",    /* Palette_Dispose @ 1090:0596 */
        "dispose-map-buf",    /* Mem_DisposePtr b394/b396 */
        "dispose-resmgr",     /* ResMgr_Dispose @ 1190:0c92 */
        "dispose-tile-cache", /* Map_DisposeTileCache @ 1110:0127 */
        "free-icon-buffers",  /* Gfx_FreeAllIconBuffers @ 1008:075b */
        "free-driver-objects",/* Gfx_FreeDriverObjects @ 1008:066d */
        "free-global-bufs",   /* GLOBALFREE b388 + 0130 */
        "trace-base",         /* Debug_GetEmptyString @ 1148:0020 */
        "close-files",        /* File_CloseAll @ 1000:1fc6 */
        "restore-stock",      /* GETSTOCKOBJECT+SELECTOBJECT x3 */
        "destroy-window",     /* DESTROYWINDOW */
        "free-font-slots",    /* Font_FreeSlots @ 1008:062f */
        "remove-font-message",/* REMOVEFONTRESOURCE + SENDMESSAGE */
        "exit-fallback",      /* Sys_ExitFallback @ 1000:12d6 */
    };

    if (index < 0 || index >= 18)
        return NULL;
    return steps[index];
}

int boot_shutdown_exit_code(int last_key)
{
    /* Orig tail: Sys_ExitFallback(DAT_1420_54ea) @ 1000:12d6 (Sys_DoExit
     * wrapper @ 1000:127f); exit code = ostatni key/wParam (54ea). */
    return last_key;
}

/* --- Chain 2026-10-16 (17): prefs format + quit dispatch (goals 1/5) ---
 * See boot.h block. OS-free 1:1 (no Win16, no globals, no exit()). */

civ_error_t config_prefs_pack(const uint8_t *c0, const uint8_t *c1,
                              const uint8_t *c2, uint16_t option_flags,
                              uint8_t *out, size_t cap)
{
    /* Orig Config_SavePrefs @ 1008:0bbf image side: 3x 8 B GLOBALLOCK
     * copies (48fa -> 17ba, 485a -> 17b2, 4964 -> 17c2) + flags word ->
     * 17ca, potem _LWRITE 0x1a z 17b2. Kolejnosc w pliku: [c(485a)]
     * [c(48fa)] [c(4964)] [flags] (17b2, 17ba, 17c2, 17ca). */
    size_t i;

    if (!c0 || !c1 || !c2 || !out) {
        if (out && cap > 0)
            out[0] = 0;
        return CIV_ERR_NOENT;
    }
    if (cap < (size_t)CIV_PREFS_LEN)
        return CIV_ERR_TRUNCATED;
    for (i = 0; i < (size_t)CIV_PREFS_CHUNK_LEN; i++) {
        out[i] = c0[i];
        out[8 + i] = c1[i];
        out[16 + i] = c2[i];
    }
    out[CIV_PREFS_FLAGS_OFF] = (uint8_t)(option_flags & 0xffu);
    out[CIV_PREFS_FLAGS_OFF + 1] = (uint8_t)((option_flags >> 8) & 0xffu);
    return CIV_OK;
}

civ_error_t config_prefs_unpack(const uint8_t *img, size_t len,
                                uint8_t *c0, uint8_t *c1, uint8_t *c2,
                                uint16_t *flags_out)
{
    /* Orig Config_LoadPrefs @ 1008:0b4a image side: _LREAD 0x1a do 17b2,
     * potem flags 17ca -> g_nOptionFlags @ 12d0:0064 (brak pliku = zostaja
     * defaults z config_prefs_defaults). */
    size_t i;

    if (!img) {
        if (c0)
            c0[0] = 0;
        if (c1)
            c1[0] = 0;
        if (c2)
            c2[0] = 0;
        if (flags_out)
            *flags_out = 0;
        return CIV_ERR_NOENT;
    }
    if (len < (size_t)CIV_PREFS_LEN)
        return CIV_ERR_TRUNCATED;
    if (!c0 || !c1 || !c2 || !flags_out)
        return CIV_ERR_NOENT;
    for (i = 0; i < (size_t)CIV_PREFS_CHUNK_LEN; i++) {
        c0[i] = img[i];
        c1[i] = img[8 + i];
        c2[i] = img[16 + i];
    }
    *flags_out = (uint16_t)((uint16_t)img[CIV_PREFS_FLAGS_OFF] |
                            ((uint16_t)img[CIV_PREFS_FLAGS_OFF + 1] << 8));
    return CIV_OK;
}

int config_prefs_needs_path_setup(int drive)
{
    /* Orig Config_SavePrefs @ 1008:0bbf head: if (0 < g_nCurrentDrive @
     * 1228:0002) { Dos_DriveReady + Dos_SetPath } (na Linuksie drive = 0,
     * wiec galaz DOS nigdy nie wchodzi, zob. sys.h). */
    return (drive > 0) ? 1 : 0;
}

int main_quit_msg_is_quit(int msg)
{
    /* Orig Main_DispatchQuit @ 1098:062f: DAT_1420_54e8 == 0x12 (WM_QUIT)
     * -> WinMain_Shutdown @ 1008:0329. */
    return (msg == CIV_WM_QUIT) ? 1 : 0;
}

int main_quit_flag_requests_shutdown(int quit_flag)
{
    /* Orig same: g_nQuitFlag @ 12d8:24ee == 1 (0 run, 1 quit/end,
     * -1 defeat, 2 ?menu, zob. game.h). */
    return (quit_flag == 1) ? 1 : 0;
}

int main_dispatch_quit_requests_shutdown(int msg, int has_cmd,
                                         int quit_flag)
{
    /* Orig same obie sciezki: WM_QUIT 0x12 direct + (Menu_DispatchCommand
     * @ 1048:01cd wynik != 0 z DAT_1420_54c6/54c8, 0x44-entry table, potem
     * g_nQuitFlag == 1) -> WinMain_Shutdown. */
    if (msg == CIV_WM_QUIT)
        return 1;
    if (has_cmd != 0 && quit_flag == 1)
        return 1;
    return 0;
}

int main_drain_should_continue(int pumped)
{
    /* Orig Main_DrainMessages @ 11e8:02fb: do { Main_PumpMessage @
     * 1098:02d1 } while (!= 0) (potem 12b0:004c select). */
    return (pumped != 0) ? 1 : 0;
}

/* --- Chain 2026-10-16 (19): pump-quit/drain discriminators (goals 1+5) ---
 * See boot.h block. OS-free 1:1 (no Win16, no globals, no exit()). */

int main_pump_quit_should_continue(int phase_first, int flag_1d4c)
{
    /* Orig Main_PumpQuitLoop @ 1008:0f77: DAT_1420_1d4c = 0; while (== 0)
     * Entry; while (!= 0) Entry. */
    if (phase_first != 0)
        return (flag_1d4c == 0) ? 1 : 0;
    return (flag_1d4c != 0) ? 1 : 0;
}

int main_pump_msg_is_char(int msg)
{
    /* Orig Main_PumpMessage @ 1098:02d1: *(param+2) == 0x102 (WM_CHAR) ->
     * Main_EnqueueResString @ 1008:0a8d direct. */
    return (msg == 0x102) ? 1 : 0;
}

int main_pump_msg_is_keydown(int msg)
{
    /* Orig same: *(param+2) == 0x100 (WM_KEYDOWN) -> Main_KeyToResChar @
     * 1208:0000 map (main_key_to_res_char LIVE) then Enqueue gdy != 0. */
    return (msg == 0x100) ? 1 : 0;
}

int main_drain_tail_offset(int flag_004c)
{
    /* Orig Main_DrainMessages @ 11e8:02fb tail: DAT_12b0_004c == 0 ->
     * ptr 1420:11f2+0xe, else +0xf. */
    return (flag_004c == 0) ? 0x0e : 0x0f;
}

/* --- Chain 2026-10-16 (20): timed pump flag (goal 3). See boot.h.
 * OS-free 1:1 (no Win16, no globals, no exit()). */

int main_pump_until_flag_should_continue(int flag)
{
    /* Orig Main_PumpUntilFlagClear @ 11e8:02a4: DAT_12d8_24f0 = param;
     * while (DAT_12d8_24f0 != 0) pump. */
    return (flag != 0) ? 1 : 0;
}

int main_pump_timer_tick(int flag)
{
    /* Orig TIMERPROC @ 1008:0d68: if (DAT_12d8_24f0 != 0)
     * DAT_12d8_24f0--. */
    if (flag != 0)
        return flag - 1;
    return flag;
}

int boot_timer_proc_interval_ms(void)
{
    /* Orig WinMain @ 1008:0000 SETTIMER(hwnd 0, id 0, 5, TIMERPROC @
     * 1008:0d68) interval (1008:00cd-00db); KILLTIMER 12d8:460e+28fa w
     * WinMain_Shutdown @ 1008:0329. SDL_AddTimer: dokumentacja, bez
     * implementacji (patrz boot.h decyzja: HWND-bound pump vs watek SDL;
     * mostek TBD caller-side w map_timer_proc STUB). */
    return 5;
}

/* --- Chain 2026-10-16 (22): Main_RunStartupSequence + Pic pump caller
 * (goal 5). See boot.h. OS-free 1:1 LIVE (no Win16, no globals). */

int main_startup_anim_loops(void)
{
    /* Orig Main_RunStartupSequence @ 11e0:0000: loop#1 (intro, dialog 0x94
     * + Anim_OpenSequence @ 10b0:008a + Sound 3 + Anim_AdvanceFrames @
     * 10b0:0461 + Close + Sound 1) + loop#2 (0x91e + WorldGen_PrepareEarth
     * @ 1108:01bc + Advance + Close + Sound 1) przed NewGame_Wizard @
     * 10e8:0000. (Nie mylić z 10e8:0000 = wizard, nie startup.) */
    return 2;
}

int main_startup_intro_sound(void)
{
    /* Orig same: Sound_PlayId(3,0) przed loop#1 (intro jingle; OPENING.wav
     * @ 1420:3114 + MPLOGO.BMP caller-side, nie w opkodach). */
    return 3;
}

int main_startup_stop_sound(void)
{
    /* Orig same: Sound_PlayId(1,0) po każdej pętli (STOP). */
    return 1;
}

int main_startup_should_run_wizard(int menu_choice)
{
    /* Orig Main_RunStartupSequence @ 11e0:0000 gate: ((choice == 0) ||
     * (choice == 2) || (choice == 3)) -> drain + open 0x91e + rebase +
     * WorldGen_PrepareEarth @ 1108:01bc + loop#2 + NewGame_Wizard @
     * 10e8:0000; else sam Dlg_DestroyTracked @ 1098:1612. */
    if (menu_choice == 0 || menu_choice == 2 || menu_choice == 3)
        return 1;
    return 0;
}

/* --- Chain 2026-10-16 (27): memory thresholds (goals 3/5). See boot.h.
 * OS-free 1:1 LIVE (no Win16, no globals, no exit()); MESSAGEBOX/exit tails
 * stay in sys_check_free/runtime_memory STUBs. */

int boot_free_memory_is_low(long free_bytes, int res1, int res0)
{
    /* Orig Sys_CheckFreeMemory @ 11e8:0000: if ((b390 < 2500000) ||
     * (b38e < 0x23) || (b38a < 0x23)) warn+exit. */
    if (free_bytes < CIV_BOOT_FREE_BYTES_MIN)
        return 1;
    if (res1 < CIV_BOOT_FREE_RES_MIN)
        return 1;
    if (res0 < CIV_BOOT_FREE_RES_MIN)
        return 1;
    return 0;
}

int boot_runtime_needs_first_warning(long free_bytes, int res1, int res0,
                                     int already_warned)
{
    /* Orig Sys_CheckRuntimeMemory @ 11e8:0075 first branch (caller
     * Game_TurnLoopTick @ 1088:0000): ((free < 1000000 || r1 < 10 || r0 < 10)
     * && 43b4 == 0) -> MESSAGEBOX 4473 once. */
    int low = 0;

    if (free_bytes < CIV_RUN_FREE_BYTES_WARN)
        low = 1;
    if (res1 < CIV_RUN_FREE_RES_WARN)
        low = 1;
    if (res0 < CIV_RUN_FREE_RES_WARN)
        low = 1;
    if (low == 0)
        return 0;
    return (already_warned == 0) ? 1 : 0;
}

int boot_runtime_needs_final_warning(long free_bytes, int res1, int res0,
                                      int warned_once, int warned_final)
{
    /* Orig same second branch: ((free < 300000 || r1 < 8 || r0 < 8) &&
     * 43b4 != 0 && 43b6 == 0) -> MESSAGEBOX 44ff once. */
    int low = 0;

    if (free_bytes < CIV_RUN_FREE_BYTES_FINAL)
        low = 1;
    if (res1 < CIV_RUN_FREE_RES_FINAL)
        low = 1;
    if (res0 < CIV_RUN_FREE_RES_FINAL)
        low = 1;
    if (low == 0)
        return 0;
    if (warned_once == 0)
        return 0;
    return (warned_final == 0) ? 1 : 0;
}
