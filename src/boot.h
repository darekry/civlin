/* Boot sequence (WinMain equivalent) -> SDL3.
 *
 * Ghidra anchors (civ.exe, NE Win3.x), renamed 2026-10-15 (Orig -> New):
 *   WinMain                  @ 1008:0000  (entry; main while loop)
 *   WinMain_Shutdown         @ 1008:0329  (KILLTIMER/FREEPROCINSTANCE/
 *                              DESTROYWINDOW/REMOVEFONTRESOURCE)
 *   Sys_GetCurrentDrive      @ 1000:11bb  (drive -> g_nCurrentDrive;
 *                              see sys.h, boot_setup_paths)
 *   Sys_BuildCurrentDirPath  @ 1000:2826  (curdir -> g_szBasePath;
 *                              see sys.h, boot_setup_paths)
 *   Sys_CheckFreeMemory      @ 11e8:0000  (GETFREESPACE + 1420:43b8 msg)
 *   App_RegisterWindowClasses@ 1098:06f8  (REGISTERCLASS x4, calls
 *                              Ctrl_RegisterCustomClasses, GETSYSTEMMETRICS)
 *   Font_EnumFamiliesInit    @ 1008:0e99  (ENUMFONTFAMILIES +
 *                              ADDFONTRESOURCE civfonts.fon @ 1420:00a4/00fe)
 *   Video_CheckDriverCaps    @ 1008:059a  (GETDEVICECAPS; 1420:0af4 checklist)
 *   Config_LoadPrefs         @ 1008:0b4a  (_LOPEN/_LREAD prefs file civ.prf)
 *   Config_SavePrefs         @ 1008:0bbf  (guarded by Dos_DriveReady @
 *                              1000:1184 + Dos_SetPath @ 1000:116a)
 *   Dos_DriveReady           @ 1000:1184  (drive-ready guard; Linux: always 1)
 *   Dos_SetPath              @ 1000:116a  (chdir guard; Linux: no-op CIV_OK)
 *   Palette_Init             @ 1090:0488  (CREATEPALETTE, godpal.c)
 *   ResMgr_LoadRsc           @ 1190:0000  (OpenResFile : resmgr.c; _LOPEN/
 *                              _HREAD 0xE360; called 5x with 1420:00b1,
 *                              00be, 00cb, 00d8, 00e5 = CivData0..4.rsc)
 *   Art_MaybeConvertArtwork  @ 10b8:0000  (1420:20a9 prompt)
 *   Game_InitGlobalTables    @ 11e8:0337  (key remap, map buffer, Rng_Init,
 *                              string tables; see sys.h, boot_init_tables)
 *   Init_UnitFontTables      @ 10e8:2d46  (FIRSTMOVE/PILLAGE/font map)
 *   Main_RunStartupSequence  @ 11e0:0000  (calls NewGame_Wizard)
 *   GfxPort_Create           @ 10d0:0000  (GR_PortOpen : gr_port.c)
 *   GfxPort_SetActive        @ 10d0:0956
 *   MainPort_GetOrCreate     @ 1098:114a
 *   WdwMap_Open              @ 1208:0af1  (see map.h, boot_open_windows)
 *   WdwSmMap_Open            @ 1210:025b  (see map.h, boot_open_windows)
 *   WdwStatus_Open           @ 1218:0386  (see map.h, boot_open_windows)
 *   Menu_InitMainBar         @ 1048:0000  (CHECKMENUITEM/MODIFYMENU)
 *   Cursor_Set               @ 1098:096c
 *   Game_TurnLoopTick        @ 1088:0000  (while body; see gameloop in game.h)
 *
 * String literals (packet F anchors; read-only reference, never written):
 *   1420:00a4/00fe "civfonts.fon" -> CIV_FONT_FILE (font init, below)
 *   1420:00b1/00be/00cb/00d8/00e5 "CivData0..4.rsc" -> rsc_loader
 *     (ResMgr_LoadRsc @ 1190:0000; DO NOT delete any *.rsc files)
 *   "civ.prf" -> CIV_PREFS_FILE (Config_LoadPrefs/SavePrefs, below)
 *   "civhelp.hlp" -> dialog_open_help (see dialog.h; WINHELP bridge TBD)
 *   "OPENING.wav" @ 1420:3114 -> audio (see audio.h; table @ 1150:0266)
 *   "famehall.civ" @ 1420:14e8 -> halloffame (see halloffame.h; ref only)
 *   MPLOGO.BMP: NO filename string in the binary -> NE resource or RSC
 *     frame (see title.h; ref only, NOT a file load)
 *
 * WinMain order reproduced by boot_run(): drive/path setup -> memory check ->
 * window classes -> timer/accel/font -> 5x RSC -> video caps -> artwork ->
 * prefs -> palette -> global tables -> unit/font tables -> ports/map windows
 * -> menu bar -> cursor -> event/turn loop (caller-owned, see game.h).
 */
#ifndef CIVLIN_BOOT_H
#define CIVLIN_BOOT_H

#include <SDL3/SDL.h>

#include "civ_common.h"

/* WinMain @ 1008:0000 init order (caller-owned loop tail).
 *
 * Fala H rule (2026-10-16): CIV_OK = observable effect achieved or step
 * provably vacuous on SDL, else CIV_ERR_UNSUPPORTED (no fake-OK); first
 * non-OK short-circuits, main.c aborts like orig MESSAGEBOX+exit.
 *
 * drive/path (live) -> memory (live check) -> classes
 * (App_RegisterWindowClasses @ 1098:06f8, HONEST STOP: brushes/metrics/
 * custom classes TBD, U2) -> timer/accel/font (font probe live; 5/16 ms
 * timers documented, SDL bridge TBD) -> 5x RSC probe gate (presence live,
 * full image owner TBD) -> artwork (honest no-op: vacuous on SDL) ->
 * prefs (defaults live, consumed by audio_play_id; civ.prf read needs a
 * prefs owner, TBD) -> palette (honest no-op: no global palette object on
 * SDL, per-surface install is the architecture) -> tables (rng + arena
 * live) -> ports/map windows (HONEST STOP: video_init live incl.
 * driver-caps check, then map_init/ports UNSUPPORTED, U4) -> menu bar
 * (HONEST STOP: dialog_init UNSUPPORTED, U2) -> cursor (void) ->
 * event/turn loop caller-owned (while(!g_nQuitFlag) + Game_TurnLoopTick @
 * 1088:0000 via game_tick, see game.h; main.c owns the loop and aborts
 * when boot fails; SDL window/renderer are caller-created and borrowed,
 * never owned here). No globals, no exit(), read-only. */
civ_error_t boot_run(SDL_Window *win, SDL_Renderer *ren);

/* Single steps, in WinMain order (each UNSUPPORTED until implemented,
 * except boot_setup_paths which is live via sys.h). */
civ_error_t boot_setup_paths(void);
/* Boot memory step: delegates to Sys_CheckFreeMemory @ 11e8:0000 (see
 * sys.h sys_check_free_memory). Stub CIV_ERR_UNSUPPORTED until the
 * SDL_GetSystemRAM mapping lands. */
civ_error_t boot_check_memory(void);
/* Boot window-class step: delegates to App_RegisterWindowClasses @
 * 1098:06f8 (below). Stub CIV_ERR_UNSUPPORTED until the SDL3 window
 * mapping lands. */
civ_error_t boot_register_classes(void);
/* Orig: FUN_1098_06f8 @ 1098:06f8 -> App_RegisterWindowClasses.
 * Registers the 4 WinMain window classes via REGISTERCLASS, then the
 * custom controls via Ctrl_RegisterCustomClasses @ 1058:0b4d (see
 * dialog.h) and caches GETSYSTEMMETRICS. Win16 USER -> SDL3 TBD; stub
 * CIV_ERR_UNSUPPORTED. Owns the registration; boot_register_classes
 * delegates to it. No globals, no exit(), read-only. */
civ_error_t app_register_window_classes(void);
civ_error_t boot_load_fonts(void);
civ_error_t boot_load_resources(void);
/* (No boot_check_video step: the arg-less order slot carried no check; the
 * live Video_CheckDriverCaps @ 1008:059a verdict runs in video_init where
 * the renderer exists, see video.h. Removed fala H, no fake-OK slot.) */
civ_error_t boot_maybe_convert_artwork(void);
civ_error_t boot_load_prefs(void);
civ_error_t boot_init_palette(void);
/* Packet I: global tables (mixed OS-free port + Win16 stub).
 *
 * Orig: FUN_11e8_0337 @ 11e8:0337 -> Game_InitGlobalTables. Called once
 *   from WinMain after Palette_Init @ 1090:0488, before
 *   Main_RunStartupSequence @ 11e0:0000. Orig shape (decompiled
 *   2026-10-16): 16 B key-remap DAT_12d8_45be..45cd =
 *   0,1,2,3,4,5,6,7,f8,f9,fa,fb,fc,fd,fe,ff (same 16 B rewritten by
 *   Init_UnitFontTables @ 10e8:2d46) + Gfx_AllocIconBuffer(0x20,0x20)
 *   + mem_alloc_lock(64000) -> 1420:b394/b396 + Rng_Init @ 11c8:0000
 *   + Sys_GetDate + Rng_Seed + string tables via Tribe_GetNameList @
 *   11f8:0080 (see newgame.h). Linux port: remap image LIVE via
 *   boot_key_remap_value below + rng_init() + malloc(64000) probe are
 *   live in boot.c; Win16 GLOBALALLOC lock selector semantics
 *   (-> 1420:b394/b396 far ptr) have no Linux equivalent -> overall
 *   CIV_ERR_UNSUPPORTED until the map-buffer owner (see map.h
 *   CIV_MAP_BUF_LEN) takes the persistent allocation. No globals here;
 *   no exit(). */
civ_error_t boot_init_tables(void);
/* Orig key-remap image DAT_12d8_45be..45cd (see above). OS-free 1:1
 * table lookup, -1 on OOR. No Win16, no globals, no exit(). */
int boot_key_remap_value(int idx);
/* Orig: FUN_10e8_2d46 @ 10e8:2d46 -> Init_UnitFontTables. Sibling of
 *   NewGame_Wizard @ 10e8:0000; loads FIRSTMOVE/PILLAGE/font-map tables
 *   (exact table bytes/offsets TBD from segment dump; pure memcpy when
 *   decoded). Win16 resource/selector load TBD -> stub
 *   CIV_ERR_UNSUPPORTED. */
civ_error_t boot_init_unit_font_tables(void);
/* Orig: FUN_1098_096c @ 1098:096c -> Cursor_Set. (cursor_id) void, called
 *   after Menu_InitMainBar @ 1048:0000. LOADCURSOR + SETCURSOR Win16
 *   cursor install -> SDL3 system-cursor bridge TBD (see video.h for the
 *   future SDL_CreateSystemCursor owner; impl stays in boot.c per packet
 *   I scope, video.* untouched). Stub: records nothing, returns void. */
void cursor_set(int cursor_id);
/* Orig ports/map windows (partial port; SDL window/renderer caller-owned).
 *
 * GfxPort_Create @ 10d0:0000 (GR_PortOpen) + MainPort_GetOrCreate @
 *   1098:114a + WdwMap_Open @ 1208:0af1 + WdwSmMap_Open @ 1210:025b +
 *   WdwStatus_Open @ 1218:0386 (see video.h gfx_port_create /
 *   mainport_get_or_create + map.h map_open_main/small/status). SDL3:
 *   SDL_Window/SDL_Renderer are created by the caller (main.c via
 *   SDL_CreateWindow/SDL_CreateRenderer) and borrowed here, never
 *   Destroyed/owned; video_init + map_init are live, the Win16
 *   CREATEWINDOW/scroll-range/SETTIMER tails stay stubbed so overall
 *   CIV_ERR_UNSUPPORTED until the window handles land. No globals, no
 *   exit(), read-only. */
civ_error_t boot_open_windows(SDL_Window *win, SDL_Renderer *ren);
/* Orig menu/cursor init (partial port).
 *
 * Menu_InitMainBar @ 1048:0000 (CHECKMENUITEM/MODIFYMENU, see
 *   menu_init_mainbar below) + Cursor_Set @ 1098:096c (void stub
 *   cursor_set() in boot.c; SDL_CreateSystemCursor bridge TBD) +
 *   dialog_init (DLGPROC layer, see dialog.h). menu_init_mainbar +
 *   dialog tails stay stubbed so overall CIV_ERR_UNSUPPORTED; cursor step
 *   is void (installs nothing, keeps WinMain-order fidelity). No globals,
 *   no exit(), read-only. */
civ_error_t boot_init_menu(void);

/* Packet G: main menu bar (stub).
 *
 * Orig: FUN_1048_0000 @ 1048:0000 -> Menu_InitMainBar. Builds/updates the
 *   WinMain menu bar via CHECKMENUITEM + MODIFYMENU (menu strings such as
 *   "&Save Game\tCtrl+S" @ 14b0:0010, "&Auto Save" @ 14b0:0092 live in
 *   segment 14b0; "Save First" @ 14d0:0028, see dialog.h). Win16 USER menu
 *   handles -> SDL3 menu TBD; stub CIV_ERR_UNSUPPORTED. Called from
 *   boot_init_menu ahead of Cursor_Set @ 1098:096c. */
civ_error_t menu_init_mainbar(void);

void boot_shutdown(void);

/* Packet F: font / prefs / DOS guards (anchors above). Win16-backed entries
 * are stubs (CIV_ERR_UNSUPPORTED); DOS guards are OS-free 1:1 ports. */

/* Startup asset names (packet F string literals; read-only, never written). */
#define CIV_FONT_FILE "civfonts.fon" /* 1420:00a4/00fe via ADDFONTRESOURCE */
#define CIV_PREFS_FILE "civ.prf"     /* Config_LoadPrefs @ 1008:0b4a */

/* Orig: FUN_1008_0e99 @ 1008:0e99 -> font_enum_families_init
 * (Font_EnumFamiliesInit). ENUMFONTFAMILIES enumeration callback +
 * ADDFONTRESOURCE(CIV_FONT_FILE "civfonts.fon" @ 1420:00a4/00fe).
 * civfonts.fon is a Win16 NE DLL/font container (244224 B on disk):
 * MZ+NE header + font-directory resources; exact FON header bytes +
 * font-table offsets TBD from the unpacked dump (do not hardcode).
 * SDL_ttf bridge plan (not implemented): read-only parse of the NE
 * font resources -> extract a TTF/bitmap face -> TTF_OpenFont +
 * ENUMFONTFAMILIES-equivalent family list; full TTF raster out of
 * scope here. Win16 GDI font-table install -> SDL_ttf TBD; stub
 * CIV_ERR_UNSUPPORTED. Read-only: never writes civfonts.fon (see
 * video_shutdown_display for the REMOVEFONTRESOURCE tail). */
civ_error_t font_enum_families_init(void);

/* AF2: FON header (civfonts.fon 244224 B NE DLL/font container, ADDFONTRESOURCE
 * @ 1420:00a4/00fe via Font_EnumFamiliesInit @ 1008:0e99; ENUMFONTFAMILIES
 * family list bridge TBD, see video_shutdown_display in video.h for the
 * REMOVEFONTRESOURCE tail). Unpacked dump-confirmed 2026-10-15 (read-only,
 * never written; packed civ1win/CIVFONTS.FON is EDILZSS2 68797 B, unpacked/
 * civfonts.fon is the 244224 B NE image parsed here):
 *   MZ "MZ" (0x4D 0x5A) at +0x00, e_lfanew u32 LE at +0x3C = 0x250,
 *   NE "NE" at 0x250 (linker 0x0A05, 1 segment, align shift 9),
 *   resource table at NE+0x48: RT_FONTDIR 0x8007 x1 + RT_FONT 0x8008 x21
 *   (fontdir at file 0x600, nfonts 0x15 = 21; resident names "FONTRES" +
 *   "FONTRES 100,100,100 : Civilization Raster Fonts").
 * OS-free header-only port (no SDL, no TTF raster, no globals, no exit()):
 * validates MZ + NE + resource-table bounds on borrowed bytes (no copy,
 * caller keeps ownership) and reports counts/offsets; face bitmap decode
 * stays a TODO (see below). */
#define CIV_FON_MAGIC_MZ0 0x4Du /* 'M' */
#define CIV_FON_MAGIC_MZ1 0x5Au /* 'Z' */
#define CIV_FON_MAGIC_NE0 0x4Eu /* 'N' */
#define CIV_FON_MAGIC_NE1 0x45u /* 'E' */
#define CIV_FON_RES_FONTDIR 0x8007u
#define CIV_FON_RES_FONT 0x8008u
#define CIV_FON_MIN_SIZE 0x40u /* MZ + e_lfanew */
#define CIV_FON_NE_MIN_SIZE 64u

typedef struct font_fon_info {
    uint32_t ne_offset;    /* e_lfanew image (0x250 on disk) */
    uint16_t align_shift;  /* NE resource align shift (9 on disk) */
    uint16_t fontdir_count;/* RT_FONTDIR entries (1 on disk) */
    uint16_t face_count;   /* RT_FONT entries = ENUMFONTFAMILIES faces (21) */
} font_fon_info_t;

/* Orig: Font_EnumFamiliesInit @ 1008:0e99 FON header side ->
 * font_fon_parse. OS-free 1:1 header check on borrowed bytes (never writes
 * civfonts.fon): MZ + e_lfanew + NE + resource-table walk to the
 * RT_FONTDIR/RT_FONT counts. NULL data/out -> NOENT; too short for
 * MZ/e_lfanew/NE/res header -> TRUNCATED; bad MZ/NE -> BAD_DATA.
 * TODO: face bitmap decode (FONTDIR entry + per-face FNT headers, e.g.
 * dfPixHeight/face names CIVBABYLON..CIVTIMES36 seen at file 0x600..) ->
 * TTF/bitmap bridge TBD (no raster here). */
civ_error_t font_fon_parse(const uint8_t *data, size_t len,
                           font_fon_info_t *out);

/* AF2: FON face list (count + names via ENUMFONTFAMILIES @ 1008:0e99).
 * OS-free stubs over font_fon_info_t (no SDL, no globals, no exit(),
 * read-only): count reports parsed RT_FONT faces (0 on NULL info);
 * list maps index -> family name, NULL until the fontdir-string order is
 * wired to the ENUMFONTFAMILIES enumeration order (expected 21 faces:
 * CIVBABYLON/CIVZULU/CIVEGYPT/CIVENGLISH/CIVGREEK/CIVRUSSIAN/CIVGERMAN/
 * CIVCHINESE/CIVFRENCH/CIVROMAN/CIVINDIAN/CIVAMERICAN/CIVAZTEC/CIVMONGOL/
 * CIVTIMES10/12/14/18/24/30/36 seen in the 0x600 fontdir dump; do not
 * hardcode here, ENUMFONTFAMILIES order TBD from Ghidra). */
int font_face_count(const font_fon_info_t *info);
const char *font_face_list(const font_fon_info_t *info, int index);

/* AG2: FNT raster + menu strings (FON + menu strings only; video/dialog
 * untouched). OS-free 1:1 ports on borrowed bytes / read-only tables; no
 * SDL, no TTF raster, no globals mutated, no exit(). */
#define CIV_FNT_VERSION_2 0x0200u
#define CIV_FNT_VERSION_3 0x0300u
#define CIV_FNT_MIN_SIZE 117u /* packed header through dfBitsOffset */
#define CIV_FNT_COPYRIGHT_LEN 60u

typedef struct font_fnt_info {
    uint16_t dfVersion;      /* 0x0200 / 0x0300 */
    uint32_t dfSize;         /* file-size image */
    char dfCopyright[60];    /* raw 60 B, not necessarily NUL-terminated */
    uint16_t dfType;
    uint16_t dfPoints;
    uint16_t dfVertRes;
    uint16_t dfHorizRes;
    uint16_t dfAscent;
    uint16_t dfInternalLeading;
    uint16_t dfExternalLeading;
    uint8_t dfItalic;
    uint8_t dfUnderline;
    uint8_t dfStrikeOut;
    uint16_t dfWeight;
    uint8_t dfCharSet;
    uint16_t dfPixWidth;
    uint16_t dfPixHeight;
    uint8_t dfPitchAndFamily; /* dfFamilyFace pitch+family byte */
    uint16_t dfAvgWidth;
    uint16_t dfMaxWidth;
    uint8_t dfFirstChar;
    uint8_t dfLastChar;
    uint8_t dfDefaultChar;
    uint8_t dfBreakChar;
    uint16_t dfWidthBytes;
    uint32_t dfDevice;      /* file-offset image */
    uint32_t dfFace;        /* file-offset image of facename string */
    uint32_t dfBitsPointer;
    uint32_t dfBitsOffset;  /* bitmap bits file offset */
    uint8_t dfReserved;     /* 0 when image holds only 117 B */
} font_fnt_info_t;

/* Orig Font_EnumFamiliesInit @ 1008:0e99 FNT header side -> font_fnt_parse.
 * OS-free header-only 1:1 on borrowed bytes (caller-owned, no copy, never
 * writes civfonts.fon): NULL data/out -> NOENT; len < CIV_FNT_MIN_SIZE ->
 * TRUNCATED; dfVersion not 0x0200/0x0300, dfFirstChar > dfLastChar, or
 * dfFace/dfBitsOffset pointing outside len -> BAD_DATA; dfSize claiming
 * more than len -> TRUNCATED. TODO: face bitmap decode behind dfBitsOffset
 * + dfCharTable (width/offset tables) -> SDL_ttf bridge TBD (see
 * font_face_bitmap stub below, no raster here). */
civ_error_t font_fnt_parse(const uint8_t *data, size_t len,
                            font_fnt_info_t *out);

/* Orig Font_EnumFamiliesInit @ 1008:0e99 FNT glyph side -> font_face_bitmap.
 * LIVE thin delegate to font_fnt_bits (bounds + v2 dfCharTable width/file
 * offset + sentinel glyph length; v3 6-B entries stay UNSUPPORTED inside
 * font_fnt_char_entry). Per-row locating lives in font_fnt_row_ptr below
 * (row_addr = bits + row*stride). */
civ_error_t font_face_bitmap(const uint8_t *data, size_t len,
                             const font_fnt_info_t *info, int c,
                             int *w_out, int *h_out,
                             const uint8_t **bits_out);

/* Orig Font_EnumFamiliesInit @ 1008:0e99 FNT dfCharTable side ->
 * font_fnt_char_entry (AH2). OS-free 1:1 header/entry-only port on borrowed
 * bytes (caller-owned FNT resource image starting at dfVersion, e.g.
 * unpacked/civfonts.fon faces at file 0x1400/0x3000/0x30e00; never writes
 * civfonts.fon): entry idx = c - dfFirstChar at 0x76 + idx*4 (0x75 when
 * len == 117, no dfReserved), each 4 B = u16 width + u16 offset-from-resource
 * -start (v2 0x0200, dump-confirmed 182/224 +1 sentinel entries, e.g. off
 * 0x352 == dfBitsOffset for c == dfFirstChar, stride 30 B = 2*15 for max 13,
 * 190 B = 5*38 for max 37). NULL data/info/outs -> NOENT;
 * len < CIV_FNT_MIN_SIZE -> TRUNCATED; dfFirst > dfLast or bad version or
 * table overrun or offset outside len -> BAD_DATA; c outside
 * [dfFirstChar, dfLastChar] -> RANGE. Width/offset via outs, height is
 * info->dfPixHeight (same for all glyphs in the face). TODO: v3 0x0300
 * 6-B entries (no v3 face in civfonts.fon, all 21 faces are 0x0200) ->
 * UNSUPPORTED. No SDL, no globals, no exit(), read-only. */
civ_error_t font_fnt_char_entry(const uint8_t *data, size_t len,
                                const font_fnt_info_t *info, int c,
                                int *width_out, uint32_t *offset_out);

/* Orig Font_EnumFamiliesInit @ 1008:0e99 FNT glyph-bits side ->
 * font_fnt_bits (AH2). OS-free 1:1 port on borrowed bytes (same image as
 * font_fnt_char_entry, no copy, never writes civfonts.fon): locates the
 * glyph via dfCharTable width/offset + sentinel next-offset for the byte
 * length (glyph_len = next_off - off, e.g. 30 B for 15-px faces, 190 B for
 * TIMES36), then returns borrowed bits = data + off (valid while the image
 * lives) + width bits per row (only first `width` bits valid) + height rows
 * = dfPixHeight, MSB-first, row stride = (dfMaxWidth+7)/8 (2 B for max 13,
 * 5 B for max 37; dfWidthBytes stays 2 on disk, so stride comes from max,
 * not from dfWidthBytes). Same bounds as char_entry plus pixHeight == 0,
 * off < table-end / dfBitsOffset, or off+len outside len -> BAD_DATA;
 * c OOR -> RANGE. TODO: v3 6-B entries -> UNSUPPORTED. No SDL, no globals,
 * no exit(), read-only. */
civ_error_t font_fnt_bits(const uint8_t *data, size_t len,
                          const font_fnt_info_t *info, int c,
                          const uint8_t **bits_out,
                          int *width_out, int *height_out);

/* AI-A: FNT glyph render + text metrics (Font_EnumFamiliesInit @ 1008:0e99
 * family; video/dialog/game/map untouched, see scope in task).
 *
 * Orig Font_EnumFamiliesInit @ 1008:0e99 FNT glyph-bits side (dfBitsOffset
 * + dfCharTable) -> font_render_glyph. OS-free 1:1 bit-copy port on
 * borrowed bytes (same image as font_fnt_bits/font_fnt_char_entry, no
 * copy, never writes civfonts.fon): locates the glyph via font_fnt_bits
 * (width = per-char entry width, height = dfPixHeight; fixed-pitch faces
 * report width == dfPixWidth), then repacks the first `width` bits of each
 * row MSB-first into the caller-owned 1-bpp `dst` with tight stride
 * (w+7)/8 (source stride is (dfMaxWidth+7)/8, see font_fnt_bits). Bounds:
 * NULL data/info/dst/w_out/h_out -> NOENT; len < CIV_FNT_MIN_SIZE ->
 * TRUNCATED; c OOR -> RANGE; v3/corrupt table -> UNSUPPORTED/BAD_DATA
 * (propagated from font_fnt_bits); dst_cap < stride*h -> TRUNCATED
 * (outs + dst untouched). No SDL, no globals, no exit(), read-only
 * (writes only the caller-owned dst). */
civ_error_t font_render_glyph(const uint8_t *data, size_t len,
                              const font_fnt_info_t *info, int c,
                              uint8_t *dst, size_t dst_cap,
                              int *w_out, int *h_out);

/* Orig status stride @ 1098:0dda (status_font_height, tmHeight line stride)
 * + Text_DrawStatusRow @ 11f8:0278 row side -> font_text_width. OS-free 1:1
 * port (sum of per-char widths via font_fnt_char_entry, no SDL): walks the
 * NUL-terminated `s`, adds each char entry width, skips OOR/undecodable
 * chars (0 for that char). NULL data/info/s -> 0; short/corrupt image ->
 * 0 when the table is unreadable, else partial sum of decodable prefix.
 * Tracking (inter-char extra pixels) is 0 here (GetTextExtent shape without
 * tracking; TODO: wire when the 1098:0dda/11f8:0278 tracking byte lands).
 * No globals, no exit(), read-only. */
int font_text_width(const uint8_t *data, size_t len,
                    const font_fnt_info_t *info, const char *s);

/* Orig status stride @ 1098:0dda (GETTEXTMETRICS tmHeight) + FNT metrics
 * (dfPixHeight + dfExternalLeading) -> font_text_height. OS-free 1:1 port
 * (no SDL): returns (int)info->dfPixHeight (ascent+descent image; external
 * leading stays separate per TEXTMETRIC, TODO when the 1098:0dda caller
 * needs tmHeight+tmExternalLeading stride). NULL info -> -1 (same contract
 * as status_font_height stub in game.h, see game.c). No globals, no
 * exit(), read-only. */
int font_text_height(const font_fnt_info_t *info);

/* --- Chain 2026-10-16 (12): FNT stride + glyph span (goal 2) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode):
 *   Font_EnumFamiliesInit @ 1008:0e99 (GETDC + GETDEVICECAPS x4 +
 *     MAKEPROCINSTANCE 0xe12 + ENUMFONTFAMILIES + FREEPROCINSTANCE +
 *     RELEASEDC; sole caller WinMain @ 1008:0000; neighbours
 *     Text_FontPreApply @ 1098:0186 + Text_FontPostApply @ 1098:0194 are
 *     the dispatch-tail hooks, see dialog.h; 1008:0d4c/0d5a/0df0/0e02 are
 *     empty neighbouring stubs with no xrefs, left unnamed).
 *   NE + FONTDIR/RT_FONT header LIVE via font_fon_parse (MZ + e_lfanew
 *     0x250 + NE + align 9 + fontdir x1 + faces x21) + font_face_count.
 *   Raster FNT PARTIAL: header LIVE via font_fnt_parse (v2 0x0200 / v3
 *     0x0300 through dfBitsOffset) + per-char entry LIVE for v2 via
 *     font_fnt_char_entry (width + file offset) + borrowed bits LIVE via
 *     font_fnt_bits (MSB-first, stride (max+7)/8, height pixHeight) +
 *     tight repack LIVE via font_render_glyph + width/height metrics LIVE
 *     via font_text_width/height; face-bitmap lookup PARTIAL via
 *     font_face_bitmap (bounds only, needs the table below).
 *   Co dokładnie brakuje do pełnego ładowania civfonts.fon (FNT raster
 *     PARTIAL -> co?): metryki są LIVE (dfPixHeight/dfWidth/char widths),
 *     bitmapy glifów są połowicznie LIVE (font_fnt_bits/render_glyph dają
 *     surowe/te same bity; font_face_bitmap nie lokuje wiersza bo nie
 *     dekoduje dfCharTable do w/h + bits — TODO tam) + brak: (a) kolejność
 *     ENUMFONTFAMILIES vs fontdir strings (font_face_list STUB, 21 nazw
 *     CIVBABYLON..CIVTIMES36 wisi w 0x600), (b) mostek TTF/SDL_ttf
 *     (ADDFONTRESOURCE/ENUMFONTFAMILIES + Text_SelectFont @ 1100:0a33 /
 *     DispatchFontHandler @ 11e8:0b40 slot install, STUB), (c) v3 6-B
 *     wpisy dfCharTable (wszystkie 21 twarzy to v2, więc STUB zamiast
 *     złego parsowania). Bez (a)-(c) pełny viewer SDL_ttf jest STUB.
 * OS-free stride/span poniżej jest LIVE 1:1 (no Win16, no globals). */

/* Orig FNT row stride (dfMaxWidth side in font_fnt_bits/render_glyph) ->
 * font_fnt_stride. 1:1 (max_width+7)/8 (2 B dla max 13, 5 B dla max 37;
 * dfWidthBytes na dysku stale 2, więc stride idzie z max, nie z pola).
 * -1 gdy max_width < 0. No globals. */
int font_fnt_stride(int max_width);

/* Orig FNT glyph span (sentinel next_off - off in font_fnt_bits) ->
 * font_fnt_glyph_len. 1:1 next_off - off (np. 30 B dla twarzy 15-px,
 * 190 B dla TIMES36). -1 gdy next_off <= off. No globals. */
int font_fnt_glyph_len(int off, int next_off);

/* --- Chain 2026-10-16 (13): ENUM order + row locating + TTF bridge projekt
 * (goals 1-3; civ.exe, x86:LE:16:Protected Mode, C17, no exit, no globals) ---
 * Anchors, renamed this pass (Ghidra plates carry the shape):
 *   Font_EnumFamiliesCallback @ 1008:0e12 (was ENUMFUNC: MAKEPROCINSTANCE
 *     target from Font_EnumFamiliesInit @ 1008:0e99; if (dwType & 1):
 *     facename match via Util_CompareBounded @ 1000:3744 (was FUN_1000_3744;
 *     +0x12, 0x11b, mode 3), on match
 *     CREATEFONTINDIRECT into DAT_1420_5484[DAT_12d8_0022++] with +0xf
 *     mark; always Font_EnumFamiliesCallbackTail @ 1008:09d3; returns 1).
 *   Font_EnumFamiliesCallbackTail @ 1008:09d3 (was FUN_1008_09d3: empty
 *     far stub, sole caller is the callback above; purpose TBD, see
 *     chain-14 + font_enum_families_callback_tail NOOP).
 *   ADDFONTRESOURCE side: no CALLF xref in the disasm (import
 *     EXTERNAL:0000008c present, REMOVEFONTRESOURCE(0xfe) confirmed in
 *     WinMain_Shutdown @ 1008:0329 against "civfonts.fon" @ 1420:00fe);
 *     the install is the CREATEFONTINDIRECT slot fill above at boot, the
 *     remove is the shutdown tail. Slot array DAT_1420_5484 holds 0x1e
 *     entries (Font_FreeSlots @ 1008:062f, was FUN_1008_062f, frees 0x1e);
 *     counter DAT_12d8_0022.
 * Kiedy install (boot? pierwszy Text_SelectFont?): install = raz przy
 *   bocie w Font_EnumFamiliesInit (sole caller WinMain @ 1008:0000,
 *   przed 5x ResMgr_LoadRsc @ 1190:0000); select = przy każdym rysowaniu
 *   przez Text_SelectFont @ 1100:0a33 -> Text_DispatchFontHandler @
 *   11e8:0b40 (slot DAT_12d8_0024 + SI height, ogon 11e8:0cb0), pierwszy
 *   raz np. w Unit_ShowInfoPopup @ 11f0:468c (DispatchFontHandler(1)).
 *   PreApply @ 1098:0186 / PostApply @ 1098:0194 to puste far stub-y
 *   (sama gadanina DS + RETF), wołane WYŁĄCZNIE z ogona 11e8:0cb0 pod
 *   strażą (Pre gdy DAT_1420_b384 != 0; Post gdy b384 != 0 z 0x45cb;
 *   zapisy SI gdy DAT_1420_9960/17cc non-null) — brak callerów wprost.
 * Kolejność ENUM vs fontdir strings: kanoniczna jest kolejność RT_FONT
 *   w NE resource table (21 wpisów, fontdir w pliku 0x600, nazwy
 *   CIVBABYLON..CIVTIMES36); ENUMFONTFAMILIES GDI może zwracać w innej
 *   kolejności + callback filtruje (dwType&1, match FUN_1000_3744), więc
 *   font_face_list zostaje STUB (NULL) aż kolejność ENUM-vs-fontdir
 *   zostanie potwierdzona dumpem (nie zgadywać indeksów).
 * Decyzja mostka TTF (projekt w komentarzu, BEZ implementacji TTF):
 *   odrzucone: fontconfig/systemowe fonty (twarze to customowe rastry
 *   z civfonts.fon, nie systemowe rodziny) oraz konwersja FNT->TTF
 *   (utrata wierności metryk dfPixWidth/dfWeight/dfItalic). Wybrane:
 *   (a) raster FNT jako kanoniczny (font_fnt_* LIVE 1:1 + row locating
 *   poniżej), (b) ENUM = parse RT_FONT order + match jak callback,
 *   slot-install = font_install_slot STUB, (c) viewer SDL_ttf dopiero
 *   gdy raster SDL (font_glyph_to_surface, gfx_indexed.c) udowodni
 *   spójność metryk z GETTEXTEXTENT (patrz dialog.h chain-13). Bez
 *   bundled TTF, bez fontconfig, bez SDL_ttf w tym łańcuchu.
 * OS-free row/slot poniżej: row LIVE 1:1, install STUB z LIVE walidacją
 * zakresu (no Win16, no globals, read-only poza out). */

/* Orig FNT row locating (dfCharTable offset + source pitch) ->
 * font_fnt_row_ptr. 1:1 row_addr = glyph_base + row * stride
 * (glyph_base via font_fnt_bits, stride = (dfMaxWidth+7)/8; wiersze
 * 0..h-1 z góry na dół, bit x w row[x/8] MSB-first). NULL data/info/
 * row_out -> NOENT (row_out zeroed); len < MIN -> TRUNCATED; v3/corrupt
 * -> UNSUPPORTED/BAD_DATA (z font_fnt_bits); row OOR -> RANGE. No
 * globals, no exit(), read-only (borrowed view, caller owns image). */
civ_error_t font_fnt_row_ptr(const uint8_t *data, size_t len,
                             const font_fnt_info_t *info, int c, int row,
                             const uint8_t **row_out);

/* Orig Font_EnumFamiliesCallback @ 1008:0e12 slot-install side ->
 * font_install_slot. STUB: slot 0..0x1d (array DAT_1420_5484, 0x1e per
 * Font_FreeSlots @ 1008:062f, was FUN_1008_062f) LIVE-validated, < 0 /
 * >= 0x1e -> RANGE; GDI CREATEFONTINDIRECT/ADDFONTRESOURCE install ->
 * CIV_ERR_UNSUPPORTED (mostek wg projektu powyżej). No globals, no exit().
 * Slotów realnie 0x1e = 30 (pętla free w Font_FreeSlots); text_font_table
 * @ 11e8:0d1d używa 25 id -> 20 różnych slotów 0x00..0x14 (aliasy
 * 2->0x0f, 3->0x11, 7->0x10, 207->0x10, 213->0x10, patrz dialog.h
 * text_font_table), więc przepełnienie nie występuje w praktyce (21 twarzy
 * < 30); orig callback nie sprawdza przepełnienia (DAT_12d8_0022++ bez
 * guarda) — port dokłada RANGE w font_slot_claim poniżej. */
civ_error_t font_install_slot(int slot);

/* --- Chain 2026-10-16 (14): fontdir strings + FNT face name + slot alloc +
 * callback-tail NOOP (goals 1-3+5; civ.exe, x86:LE:16:Protected Mode, C17,
 * no exit, no globals, read-only) ---
 * Gdzie leżą stringi fontdir w pliku (offset? format Pascal/C?):
 *   unpacked/civfonts.fon (244224 B, NE; packed civ1win/CIVFONTS.FON to
 *   EDILZSS2 68797 B — read-only, nigdy nie pisane): e_lfanew 0x250, NE,
 *   align shift 9, RT_FONTDIR 0x8007 x1 (file 0x600, len 6*512 = 3072 B) +
 *   RT_FONT 0x8008 x21 (file 0x1400/0x3000/0x4c00/0x6800/0x8400/0xaa00/
 *   0xc600/0xe200/0x10000/0x12600/0x14200/0x15e00/0x17a00/0x19600/0x1b200/
 *   0x1cc00/0x1f200/0x21400/0x24a00/0x29000/0x30e00). Fontdir w pliku 0x600:
 *   u16 nfonts = 21 (0x15), potem 21 wpisów każdy ~0x7d-0x80 B: device
 *   string "Civilization XXX (c) 1993" NUL + metryki + face string
 *   "CIVXXX" NUL. Face stringi (C, NUL-terminated, NIE Pascal):
 *   0x676 CIVBABYLON, 0x6f5 CIVZULU, 0x771 CIVEGYPT, 0x7ee CIVENGLISH,
 *   0x86d CIVGREEK, 0x8ea CIVRUSSIAN, 0x969 CIVGERMAN, 0x9e7 CIVCHINESE,
 *   0xa66 CIVFRENCH, 0xae4 CIVROMAN, 0xb61 CIVINDIAN, 0xbdf CIVAMERICAN,
 *   0xc5f CIVAZTEC, 0xcdc CIVMONGOL, 0xd5a CIVTIMES10, 0xdd9 CIVTIMES12,
 *   0xe58 CIVTIMES14, 0xed7 CIVTIMES18, 0xf56 CIVTIMES24, 0xfd5 CIVTIMES30,
 *   0x1054 CIVTIMES36. Te same nazwy siedzą w zasobach FNT na
 *   base+dfFace (resource-relative, C NUL, np. 0x1400+0x18c4 = 0x2cc4
 *   CIVBABYLON) — port LIVE poniżej czyta stronę FNT (font_fnt_face_name),
 *   bo fontdir-entry stride nie jest stały i nie ma go co zgadywać.
 * Jak mapować twarz->slot (font_slot_for_id LIVE?):
 *   TAK — text_font_slot_for_id / text_font_height_for_id / text_font_table
 *   / text_dispatch_font w dialog.h/dialog.c są LIVE (tabela @ 11e8:0d1d:
 *   25 id 1..8, 200..215, 333 -> slot DAT_12d8_0024 + SI height; patrz
 *   chain-11/12 tam). Slot = rodzina/rozmiar (0x0f h12, 0x13 h30, 0x12 h24,
 *   0x11 h18, 0x10 h14 body, 0x00..0x0d h14 serie 200.., 0x14 h36 tytuł),
 *   nie bold/italic (te w dfItalic/dfWeight FNT). ENUMFONTFAMILIES GDI może
 *   zwracać w innej kolejności niż RT_FONT + callback filtruje (dwType&1,
 *   match Util_CompareBounded @ 1000:3744, bound 0x11b, mode 3), więc
 *   font_face_list zostaje STUB (NULL) aż kolejność ENUM-vs-fontdir
 *   zostanie potwierdzona dumpem (nie zgadywać indeksów).
 * Wpisy v3 6-B (czy v3 w ogóle występuje w civfonts.fon?):
 *   NIE — przeskanowano wszystkie 21 twarzy na pliku unpacked/civfonts.fon
 *   (read-only, struct <H dfVersion na base +0x00 dla każdego z 21 base
 *   powyżej): 21/21 to 0x0200 (v2). Brak ani jednego 0x0300, więc gałąź v3
 *   w font_fnt_char_entry zostaje STUB (UNSUPPORTED) z powodem zamiast
 *   złego parsowania 6-B wpisów bez próbki (width u16 + offset u32?;
 *   dokładny layout 6-B TBD gdyby v3 się pojawił).
 * Sloty TTF (ile? kiedy przepełnienie? związek z DAT_1420_5484 i tabelą?):
 *   0x1e = 30 slotów (Font_FreeSlots @ 1008:062f kasuje 0x1e). Install =
 *   raz przy bocie w Font_EnumFamiliesInit @ 1008:0e99 (sole caller WinMain
 *   @ 1008:0000) via Font_EnumFamiliesCallback @ 1008:0e12
 *   (DAT_1420_5484[DAT_12d8_0022++] bez guarda — przepełnienie gdyby
 *   > 30 hitów ENUM; 21 twarzy więc bezpiecznie). Select = przy każdym
 *   rysowaniu via Text_SelectFont @ 1100:0a33 -> Text_DispatchFontHandler
 *   @ 11e8:0b40 (slot DAT_12d8_0024 + SI height, ogon 11e8:0cb0 z
 *   Text_FontPreApply @ 1098:0186 / PostApply @ 1098:0194 — oba puste far
 *   stub-y jak Tail, NOOP). Port alokacji LIVE poniżej (font_slot_claim:
 *   RANGE przy >= 0x1e); sam install GDI (CREATEFONTINDIRECT/
 *   ADDFONTRESOURCE) zostaje STUB (font_install_slot) — mostek to raster
 *   FNT jako kanoniczny (bez fontconfig, bez FNT->TTF, bez SDL_ttf tutaj).
 * Tail @ 1008:09d3 (pusty far stub, purpose TBD — po co?):
 *   16 B (09d3-09e2: MOV AX,DS/NOP/INC BP/PUSH BP/MOV BP,SP/PUSH DS/
 *   MOV DS,AX/JMP 09df/POP DS/POP BP/DEC BP/RETF, bez ciała; dekompilator
 *   dopowiada fikcyjny return — ignorować). Wołany ZAWSZE na końcu
 *   callbacku (return ignorowany, enumeracja leci dalej via return 1);
 *   brak innych xrefs; rodzeństwo 1008:0d4c/0d5a/0df0/0e02 to puste
 *   bezimienne stub-y tego samego kształtu. Najpewniej placeholder
 *   linkera / thunk pod przyszły hook (ten sam codegen co Pre/PostApply
 *   @ 1098:0186/0194, wołane tylko z ogona dispatch 11e8:0cb0 pod strażą
 *   DAT_1420_b384). Port: NOOP font_enum_families_callback_tail (jak
 *   cursor_set / text_font_pre/post_apply).
 * Spójność font_render_glyph vs font_glyph_to_surface (tight vs surowe):
 *   render_glyph daje TIGHT repack (stride (w+7)/8, MSB-first, tylko
 *   pierwsze `width` bitów ważne); font_glyph_to_surface (gfx_indexed.c,
 *   LIVE) bierze DOKŁADNIE ten tight obraz (stride (w+7)/8) i expanduje
 *   każdy bit do indeksu 8-bit (0 = black, 1 = white) + 2-entry paleta —
 *   NIE bierze surowych font_fnt_bits (source stride (dfMaxWidth+7)/8).
 *   Row locating font_fnt_row_ptr operuje na surowym stride. Spójne co do
 *   kontraktu (tight jest wejściem surface); metryki GETTEXTEXTENT vs
 *   font_text_width zgodne co do kształtu (suma advance bez trackingu),
 *   równość wartości bez dowodu z dumpu (hinting GDI vs surowe bity). */
#define CIV_FONT_SLOT_MAX 0x1e /* DAT_1420_5484 entries (Font_FreeSlots) */

/* Orig Font_EnumFamiliesInit @ 1008:0e99 FNT face-name side ->
 * font_fnt_face_name. OS-free 1:1 na pożyczonych bajtach (obraz zasobu FNT
 * od dfVersion, np. unpacked/civfonts.fon 0x1400.., nigdy nie pisze FON):
 * nazwa to C NUL-string pod base+dfFace (resource-relative, np.
 * 0x1400+0x18c4 CIVBABYLON .. 0x30e00+0xaaf8 CIVTIMES36; te same co fontdir
 * 0x676..0x1054 powyżej). NULL data/info/name_out -> NOENT (name_out
 * zerowane); len < MIN -> TRUNCATED; dfFace == 0 / >= len -> BAD_DATA
 * (brak nazwy); brak NUL w [dfFace, len) -> BAD_DATA. Sukces: *name_out =
 * data + dfFace (pożyczony widok, ważny póki obraz żyje) + CIV_OK. No SDL,
 * no globals, no exit(), read-only. */
civ_error_t font_fnt_face_name(const uint8_t *data, size_t len,
                               const font_fnt_info_t *info,
                               const char **name_out);

/* Orig Font_EnumFamiliesCallback @ 1008:0e12 alloc half -> font_slot_claim.
 * 1:1 DAT_1420_5484[DAT_12d8_0022++] z guardem (orig guarda nie ma):
 * *count to DAT_12d8_0022 (0..0x1d), *slot_out to przyznany slot. NULL
 * outs -> NOENT (zerowane); *count < 0 / >= CIV_FONT_SLOT_MAX (0x1e) ->
 * RANGE (przepełnienie: orig by nadpisał za tablicą; tu twardy stop).
 * Sukces: *slot_out = *count, (*count)++, CIV_OK. No Win16 (GDI install w
 * font_install_slot STUB), no globals (licznik explicite), no exit(). */
civ_error_t font_slot_claim(int *count, int *slot_out);

/* Orig: FUN_1008_062f @ 1008:062f -> Font_FreeSlots.
 * Frees 0x1e HFONT handles in DAT_1420_5484 via DELETEOBJECT (loop SI
 * 0..0x1d, BX = SI*2, ES = 0x1420, skip zeros: CMP [BX+0x5484],0).
 * Counterpart to Font_EnumFamiliesCallback @ 1008:0e12 slot fill
 * (DAT_1420_5484[DAT_12d8_0022++] + CREATEFONTINDIRECT, install raz przy
 * bocie w Font_EnumFamiliesInit @ 1008:0e99, sole caller WinMain @
 * 1008:0000; select przy każdym rysowaniu via Text_SelectFont @
 * 1100:0a33 -> Text_DispatchFontHandler @ 11e8:0b40, slot DAT_12d8_0024
 * + SI height, ogon 11e8:0cb0 z Text_FontPreApply @ 1098:0186 /
 * PostApply @ 1098:0194 NOOP). Sole caller: WinMain_Shutdown @ 1008:0329
 * tail (Font_FreeSlots, potem REMOVEFONTRESOURCE(CIV_FONT_FILE @
 * 1420:00fe), SENDMESSAGE + Sys_ExitFallback; sąsiedzi w shutdown:
 * Gfx_FreeAllIconBuffers @ 1008:075b (was FUN_1008_075b: icon-buffer
 * loops via Gfx_FreeIconBuffer) + Gfx_FreeDriverObjects @ 1008:066d (was
 * FUN_1008_066d: DELETEOBJECT/GLOBALFREE driver handles), oba sole-caller
 * shutdown, renamed this pass). Slot array size proof: 0x1e = 30;
 * text_font_table @ 11e8:0d1d uses 20 distinct slots 0x00..0x14 (25 id,
 * aliases 2->0x0f, 3->0x11, 7->0x10, 207->0x10, 213->0x10, see dialog.h),
 * so no overflow in practice (21 faces < 30; orig callback has no guard,
 * claim guards with RANGE). Linux: GDI DELETEOBJECT has no equivalent
 * (handle table lives in GDI, like CREATEFONTINDIRECT in font_install_slot
 * STUB) -> the slot-state half below is LIVE 1:1 on explicit
 * caller-owned array+count (no Win16, no globals, read-only except the
 * explicit outs); the GDI free stays caller-side TODO. */
civ_error_t font_free_slots(int *slots, int n, int *count);

/* Orig Font_EnumFamiliesCallbackTail @ 1008:09d3 ->
 * font_enum_families_callback_tail. Pusty far stub (16 B DS-gadaniny +
 * RETF, bez ciała; dekompilator dopowiada return — ignorować). Wołany
 * zawsze z ogona callbacku; brak innych xrefs. NOOP void (jak cursor_set /
 * text_font_pre/post_apply); straże/return 1 zostają caller-side. */
void font_enum_families_callback_tail(void);

/* Orig Menu_InitMainBar @ 1048:0000 string side -> menu_strings.
 * OS-free static const port (const char * array, no Win16 USER handles,
 * read-only, never written): [0] 14b0:0010 "&Save Game\tCtrl+S",
 * [1] 14b0:0092 "&Auto Save", [2] 14d0:0028 "Save First",
 * [3] 1420:435b menu text (mirrors CIV_MAINMENU_TEXT, see mainmenu.h;
 * canonical owner stays there, this entry is the menu-bar table copy).
 * Count + bounded accessor below (OOB -> NULL, same contract as
 * tribe_get_name_list / boot_video_caps_describe / font_face_list). */
extern const char *const menu_strings[];
int menu_strings_count(void);
const char *menu_string_at(int index);

/* Orig: FUN_1008_0b4a @ 1008:0b4a -> config_load_prefs (Config_LoadPrefs).
 * _LOPEN (OF_READ) + _LREAD of CIV_PREFS_FILE from the base path, then
 * option-struct fill with built-in defaults when missing/short. Prefs
 * field map not yet decoded; stub. Read-only: never creates civ.prf. */
civ_error_t config_load_prefs(void);

/* Orig: FUN_1008_0bbf @ 1008:0bbf -> config_save_prefs (Config_SavePrefs).
 * Guarded by dos_drive_ready @ 1000:1184 (must be ready) + dos_set_path @
 * 1000:116a (cwd in place) before the _LCREAT/_LWRITE of CIV_PREFS_FILE.
 * Win16 file-handle path -> stdio bridge TBD; stub. */
civ_error_t config_save_prefs(void);

/* Orig: FUN_1000_1184 @ 1000:1184 -> dos_drive_ready (Dos_DriveReady).
 * Drive-ready guard checked before the Config_SavePrefs @ 1008:0bbf and
 * Sound_PlayId @ 1150:00c9 disk paths (see sys.h `0 < drive` note).
 * OS-free Linux equivalent: no removable-drive concept -> always 1
 * (ready). Implemented 1:1. */
int dos_drive_ready(int drive);

/* Orig: FUN_1000_116a @ 1000:116a -> dos_set_path (Dos_SetPath). Chdir
 * guard paired with dos_drive_ready before Config_SavePrefs writes
 * civ.prf. Linux equivalent: no drive letters, cwd owned by the launcher
 * -> no-op returning CIV_OK (NULL path reports NOENT instead of crashing
 * like the original far-pointer fault). Implemented 1:1. */
civ_error_t dos_set_path(int drive, const char *path);

/* Packet S2: shutdown/startup (WinMain_Shutdown order, artwork prompt,
 * startup dispatcher, prefs defaults, video caps checklist, timer).
 *
 * Orig anchors (Orig FUN_seg_off @ seg:off kept on each symbol):
 *   WinMain_Shutdown        @ 1008:0329 (lives as boot_shutdown() above;
 *     KILLTIMER / FREEPROCINSTANCE / DESTROYWINDOW / REMOVEFONTRESOURCE
 *     order documented in boot.c; timer detail below)
 *   TIMERPROC               @ 1008:0d68 (200 ms SETTIMER installed by
 *     WdwMap_Open @ 1208:0af1, pumped from the WinMain @ 1008:0000
 *     message loop, torn down by KILLTIMER first in WinMain_Shutdown;
 *     impl map_timer_proc, see map.h; SDL timer bridge TBD, no new
 *     symbol here)
 *   Art_MaybeConvertArtwork @ 10b8:0000 (1420:20a9 prompt; gates
 *     WorldGen_CopyPreset @ 10b8:079d, see worldgen.h)
 *   Main_RunStartupSequence @ 11e0:0000 (dispatcher for MainMenu_WndProc
 *     @ 11e0:0221; NewGame_Wizard @ 10e8:0000 via newgame_wizard_run,
 *     see newgame.h / mainmenu.h)
 *   Config_LoadPrefs        @ 1008:0b4a / Config_SavePrefs @ 1008:0bbf
 *     (CIV_PREFS_FILE "civ.prf"; defaults below; file I/O tail stays
 *     stubbed, read-only: never creates civ.prf)
 *   Video_CheckDriverCaps   @ 1008:059a (GETDEVICECAPS checklist @
 *     1420:0af4; SDL mapping via video_check_driver_caps, see video.h)
 * No globals, no exit(), read-only. Win16 tails stub CIV_ERR_UNSUPPORTED;
 * OS-free parts (prefs defaults, caps index shape) are ported 1:1. */

/* Orig: FUN_10b8_0000 @ 10b8:0000 -> art_maybe_convert_artwork
 * (Art_MaybeConvertArtwork). LIVE no-op on Linux (returns CIV_OK).
 * Orig shape: DAT_1420_17a8 gate + _LOPEN probe + 1420:20a9 prompt
 * ("CONVERT CIVILIZATION ARTWORK" branch), then the convert loop over
 * frame ranges (200..0xca, 0x44c..0x514, 0x80..0x90, 500..0x201,
 * 400..0x196, 0x3f5..0x3f7, top-five, 0xd05..0xd13, gov, tech) via
 * Art_PreviewFrame @ 10b8:076a + Art_PumpConvertCancel @ 10b8:06a0 +
 * Art_DrawProgressFill @ 10b8:062d (art_progress_fill_x, LIVE) +
 * Art_CheckDiskSpace @ 10b8:04df 9 MB gate, each frame through
 * Art_LoadFrameData @ 10b8:079d (disk via Art_LoadDiskFile @ 10b8:0aa8
 * + save_unpack when DAT_1420_17a8 != 0, else RSC via
 * Art_LoadResPaletted @ 10b8:11bc + palette expand to 0xec +
 * Art_DecodeLZW @ 10b8:158c GIF-style LZW) + Art_SaveConvertedFrame @
 * 10b8:0c1c (Art_ShouldConvert @ 10b8:0ded = 1 always; save_pack +
 * _LCREAT/_HWRITE disk cache) + Art_PortDataToBitmap @ 10b8:0e36.
 * What PortDataToBitmap converts: device-independent port data to a
 * device-dependent bitmap — even width = direct GfxPort_AllocBitmap @
 * 10d0:0cfa + BITBLT; odd width = pad to even (GLOBALALLOC + HMEMCPY
 * per row when DAT_1420_17a6 == 1, else CREATECOMPATIBLEDC +
 * SETBITMAPBITS + BITBLT 0xcc0020 per scanline); then SELECTOBJECT +
 * SETRECT clip. Palette is the 0xec LOGPALETTE from Palette_Init @
 * 1090:0488 (DAT_1420_9964 -> CREATEPALETTE -> DAT_1420_9966, see
 * video.h/gfx_indexed.h gfx_godpal_get); dimensions ride in the
 * port-data struct (+0xe/+0x10 w, +0xc/+0xe h region). On Linux this
 * whole driver conversion is unnecessary: SDL3 INDEX8 textures take
 * arbitrary widths (no WORD-align pad), palette applies via
 * SDL_SetPaletteColors (see palette_apply_to_port), LZW decodes via
 * gfx_lzw_decode/pic_decompress_chunk — so a trivial CIV_OK no-op is
 * faithful (same contract as the STATUS.md boot path note). Called
 * from the boot_maybe_convert_artwork step above. No globals, no
 * exit(), read-only. */
civ_error_t art_maybe_convert_artwork(void);

/* Orig: FUN_11e0_0000 @ 11e0:0000 -> main_run_startup_sequence
 * (Main_RunStartupSequence). PARTIAL dispatcher for the MainMenu_WndProc @
 * 11e0:0221 choices over CIV_MAINMENU_TEXT @ 1420:435b (see mainmenu.h
 * mainmenu_run/mainmenu_dispatch): new-game paths -> NewGame_Wizard @
 * 10e8:0000 via newgame_wizard_run (see newgame.h), Load -> save prompt
 * @ 1130:000e, EARTH -> WorldGen_LoadEarth @ 1108:0243 (see worldgen.h),
 * Hall -> HoF, Quit -> WinMain_Shutdown @ 1008:0329 (boot_shutdown
 * above). Drives Anim_OpenSequence @ 10b0:008a + the Anim_AdvanceFrames @
 * 10b0:0461 loop over the 0x94 procedural host (preset 0x200x0x140, see
 * main_startup_intro_proc_w/h + procedural_ready below); OS-free
 * ID/count/tick halves are LIVE 1:1 (ref unpacked/civ.exe @ 11e0:0000:
 * tracked preset 0x200x0x140 + 0x18 ctx + ctl 0x65-0x6e via dlg_create_*
 * @ 1098:124f, close pop(1) = 0 via dlg_tracked_pop_count @ 1098:1612,
 * rebase 1100 - 1000 = 100 via anim_rebase_clock @ 10b0:005b; fala 23
 * MenuZ tail: dialog_menuz_art_mode(-1/0/3) = 0/1/2 +
 * dialog_menuz_is_cancel triple + dlg_center_tracked_window @ 1098:0b33
 * centered 0x200x0x140 at (64,80)).
 * Startup bridge (see boot.c): Dlg_CreateTracked @ 1098:124f (0x94,0x41) /
 * Dlg_DestroyTracked @ 1098:1612 host + GfxPort_Create @ 10d0:0000 x2 pair
 * geometry (main_startup_port_pair_ok: anim_seq_port_count/off +
 * gfx_startup_port_size_ok/pitch 0x200x0x140, see video.h) +
 * Art_LoadFrameData @ 10b8:079d gate (art_frame_id_ok +
 * art_load_frame probe, see video.h) + SDL modal/event drain
 * (main_startup_pump_events: SDL_PollEvent for Dialog_MenuZ_Modal @
 * 1220:0156 + loop-done via main_startup_modal_is_done); device tails
 * STUB -> overall PARTIAL CIV_ERR_UNSUPPORTED, never fake CIV_OK.
 * No globals, no exit(), read-only. */
civ_error_t main_run_startup_sequence(void);

/* Config prefs defaults (civ.prf via Config_LoadPrefs @ 1008:0b4a /
 * Config_SavePrefs @ 1008:0bbf above). g_nOptionFlags @ 12d0:0064 bit
 * 0x10 = sounds on (Sound_PlayId @ 1150:00c9 gate together with
 * DAT_1228:0104, see audio.h audio_play_id + game.h); remaining civ.prf
 * field map TBD from the dump, so the struct carries only the decoded
 * flags word. OS-free 1:1; file I/O tail stays stubbed (config_load_prefs
 * / config_save_prefs above remain CIV_ERR_UNSUPPORTED, read-only). */
#define CIV_PREFS_FLAG_SOUNDS_ON 0x10u /* g_nOptionFlags bit: sounds on */

typedef struct civ_prefs {
    uint16_t option_flags; /* g_nOptionFlags image @ 12d0:0064 */
} civ_prefs_t;

/* Built-in defaults 1:1 (sounds on; other fields TBD). By value, no I/O. */
civ_prefs_t config_prefs_defaults(void);

/* OS-free predicate for the sounds-on shape (NULL -> 0). The DAT_1228:0104
 * second half of the Sound_PlayId gate stays on the file/drive side
 * (dos_drive_ready + config_load_prefs above, TBD). */
int config_prefs_sounds_on(const civ_prefs_t *prefs);

/* Video caps checklist @ 1420:0af4 for Video_CheckDriverCaps @ 1008:059a
 * (GETDEVICECAPS x4, see video.h video_check_driver_caps; decompiled
 * 2026-10-17, ref unpacked/civ.exe read-only NE Win3.x 832512 B:
 * disasm 1008:059a-062e pushes 0x0c/0x0e/0x26 (+ cond 0x18) + TEST
 * AX,0x100 mono gate (DAT_1420_17a6) + w*h convert gate (DAT_1420_17a8)
 * + GETDC @ 1608:00f4 / RELEASEDC @ 1608:00f8 pair; checklist label
 * strings @ 1420:0af4 ("Display / Video Driver Checklist" + "  x  " +
 * "Screen Dimensions (pixels):   " + "Color planes:   " + "Bits per
 * pixel: " + "Color table entries:  " + ... + "RASTER CAPABILITIES:" +
 * flag pairs, through "* Marked options should be supported.") are the
 * WorldGen_ShowVideoCapsDialog @ 1060:01c1 debug-dialog labels (sister,
 * see worldgen.h/worldgen.c PARTIAL there), not fetched here).
 * OS-free query table below is LIVE 1:1 (4 entries: 0x0c width / 0x0e
 * height / 0x26 raster caps / 0x18 depth, same order as the pushes;
 * mono gate LIVE via video_driver_is_mono + convert gate LIVE via
 * video_driver_needs_convert, see video.h): count reports entries,
 * describe maps index to its checklist meaning (NULL on OOR, same
 * contract as tribe_get_name_list, see newgame.h). SDL renderer-caps
 * mapping lives in video_check_driver_caps (see video.h), called from
 * video_init where the renderer exists. Win16 GETDC/RELEASEDC fetch
 * tail stays PARTIAL with TODO Orig in boot.c (no fake-OK). No globals,
 * no exit(), read-only. */
#define CIV_BOOT_VIDEO_CAPS_N 4
int boot_video_caps_count(void);
const char *boot_video_caps_describe(int index);

/* --- Chain 2026-10-16 (16): shutdown order + tail (goals 1/5) ---
 * Orig WinMain_Shutdown @ 1008:0329 (decompiled this pass, sole-caller
 * order verified in disasm 1008:0329-0599): GETKEYBOARDSTATE/SETKEYBOARDSTATE
 * save + WINHELP close (if 12b0:0004) + KILLTIMER 12d8:460e + KILLTIMER
 * 12d8:28fa + FREEPROCINSTANCE 1420:5148 + Ctrl_FreeGdiObjects @ 1058:0d71
 * (1420:514a) + Config_SavePrefs @ 1008:0bbf (was FUN_1008_0bbf: _LCREAT +
 * option-struct write; if 12b0:0004) + GfxPort_Close @ 10d0:0172 x5
 * (12d8:4598/455a/451c/44bc + 1420:9314) + Palette_Dispose @ 1090:0596 (was
 * FUN_1090_0596: GLOBALFREE 1420:9964 + DELETEOBJECT 1420:9966) +
 * Mem_DisposePtr @ 1100:060b (map buf b394/b396) + ResMgr_Dispose @
 * 1190:0c92 + Map_DisposeTileCache @ 1110:0127 (was FUN_1110_0127: 0xfa
 * DELETEOBJECT loop + 1330:0004/0002) + Gfx_FreeAllIconBuffers @ 1008:075b
 * (19 grup / 821 slotow, see video.h registry) + Gfx_FreeDriverObjects @
 * 1008:066d (DELETEOBJECT 1420:547e/547c/179e/17a0/17a2 + GLOBALFREE
 * 485a/48fa/4964) + GLOBALFREE 1420:b388 + GLOBALFREE 1420:0130 +
 * Debug_GetEmptyString @ 1148:0020 (was FUN_1148_0020: trace base) +
 * File_CloseAll @ 1000:1fc6 (was FUN_1000_1fc6: fd 5..4e4c via File_CloseEntry
 * @ 1000:2098, was FUN_1000_2098) + GETSTOCKOBJECT(0xd)+SELECTOBJECT x3
 * (12b0:0006/001e/0036 restore) + DESTROYWINDOW (12b0:0004) + Font_FreeSlots
 * @ 1008:062f (0x1e slots DAT_1420_5484) + REMOVEFONTRESOURCE("civfonts.fon"
 * @ 1420:00fe, 13 B + NUL; para do ADDFONTRESOURCE w Font_EnumFamiliesInit @
 * 1008:0e99; install = CREATEFONTINDIRECT slot fill w callbacku @ 1008:0e12
 * raz przy bocie, remove = ten ogon) + SENDMESSAGE(main,0,0,0x1d) (PUSH
 * -1/0x1d/0/0: do glownego okna, shutdown broadcast, nie broadcast-all) +
 * Sys_ExitFallback @ 1000:12d6 (Sys_DoExit(code,0,0) wrapper; exit code =
 * DAT_1420_54ea, ostatni key/wParam z Sys_WaitForInput @ 1038:0c1d petli).
 * Kto wola Shutdown: WinMain @ 1008:0000 tail + Main_DispatchQuit @
 * 1098:062f (was FUN_1098_062f: WM_QUIT 0x12 + g_nQuitFlag == 1 paths) +
 * MainMenu_WndProc @ 11e0:0221 (Quit) + Game_ProcessGameEnd @ 1088:0b6d
 * (via Hof_ShowEndDialog @ 1180:0000 family) + Art paths @ 10b8:0000/04df +
 * Main_PumpMessage @ 1098:02d1 quit (g_nQuitFlag == 1). Spojnosc z
 * boot_shutdown w boot.c: void org (timers/windows via video/map,
 * dialogs, audio) zachowuje te sama kolejnosc faz (KILLTIMER->ports->
 * palette/buffers->font-remove); szczegoly Win16 (KILLTIMER handles,
 * FREEPROCINSTANCE, DESTROYWINDOW, DELETEOBJECT/GLOBALFREE, SENDMESSAGE,
 * exit code) nie maja odpowiednika i zostaja caller-side STUB — sama
 * kolejnosc faz ponizej jest LIVE 1:1 jako tabela (no Win16, no globals).
 * No exit() (Sys_ExitFallback/Sys_DoExit @ 1000:127f/12d6 stay STUB). */
int boot_shutdown_step_count(void);
const char *boot_shutdown_step_name(int index);
int boot_shutdown_exit_code(int last_key);

/* --- Chain 2026-10-16 (17): prefs file format + quit dispatch (goals 1/5) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Config_SavePrefs @ 1008:0bbf (was FUN_1008_0bbf: drive guard 0 <
 *     g_nCurrentDrive @ 1228:0002 via Dos_DriveReady @ 1000:1184 +
 *     Dos_SetPath @ 1000:116a, then _LCREAT "civ.prf" @ 1420:0113 (8 B
 *     "civ.prf\\0", second copy @ 1420:010b for _LOPEN) + option-word
 *     store g_nOptionFlags @ 12d0:0064 -> DAT_1420_17ca + 3x GLOBALLOCK
 *     copies of 8 B from driver handles +0x14 (DAT_1420_48fa ->
 *     DAT_1420_17ba, DAT_1420_485a -> DAT_1420_17b2, DAT_1420_4964 ->
 *     DAT_1420_17c2, each via Util_MemCopyFar @ 1000:137f CX=8) +
 *     _LLSEEK 0,0 + _LWRITE 0x1a from DAT_1420_17b2 + _LCLOSE).
 *   Config_LoadPrefs @ 1008:0b4a (was FUN_1008_0b4a: defaults
 *     DAT_1420_17ca -> g_nOptionFlags, then _LOPEN "civ.prf" @ 1420:010b
 *     + _LREAD 0x1a into DAT_1420_17b2 + _LCLOSE + flags 17ca -> 12d0:64;
 *     missing file keeps the defaults).
 * Format pliku prefs (civ.prf, 0x1a = 26 B od DAT_1420_17b2):
 *   [0x00..0x07] driver chunk A (8 B z handle DAT_1420_485a+0x14),
 *   [0x08..0x0f] driver chunk B (8 B z DAT_1420_48fa+0x14),
 *   [0x10..0x17] driver chunk C (8 B z DAT_1420_4964+0x14),
 *   [0x18..0x19] option_flags word (DAT_1420_17ca = g_nOptionFlags @
 *   12d0:0064, bit 0x10 = sounds on, zob. config_prefs_defaults).
 *   Chunki A/B/C to nieprzezroczyste bajty obiektow drivera (te same
 *   handle GLOBALFREE w Gfx_FreeDriverObjects @ 1008:066d + WdwSmMap_Open
 *   @ 1210:025b + Menu_CheckCityNameId @ 1008:0dc9 xrefs; MM code? nie:
 *   to GDI/driver state, nie dzwiek — dzwiek to tylko bit 0x10 flag, por.
 *   Sound_PlayId @ 1150:00c9 gate z DAT_1228:0104 w audio.h; video?
 *   czesciowo: te same handle czyta tez WdwMap_Open @ 1208:0af1, wiec
 *   sciezki plikowe w chunkach TBD z dumpu, nigdy nie zgadywane).
 *   Zwiazek z config_prefs_defaults/sounds_on (LIVE): defaults daja samo
 *   slowo flag (sounds on), pack/unpack ponizej daja caly 26-B obraz.
 * Port: layout pack/unpack + drive-guard LIVE 1:1 na jawnych buforach
 * (no Win16, no globals, read-only oprocz out); _LCREAT/_LWRITE/_LOPEN/
 * _LREAD/GLOBALLOCK mostek w config_load/save_prefs zostaje STUB.
 *   Main_DispatchQuit @ 1098:062f (was FUN_1098_062f: PEEKMESSAGE +
 *     TRANSLATEACCELERATOR (12b0:0004) + WM_QUIT 0x12 -> WinMain_Shutdown
 *     @ 1008:0329 + TRANSLATEMESSAGE/DISPATCHMESSAGE + Menu_Dispatch-
 *     Command @ 1048:01cd (DAT_1420_54c6/54c8, 0x44-entry table) +
 *     g_nQuitFlag @ 12d8:24ee == 1 -> WinMain_Shutdown; sole caller
 *     Main_DispatchQuitEntry @ 1098:061a).
 * Sciezki quit: menu Quit (MainMenu_WndProc @ 11e0:0221) + X okna / ESC
 * (WM_QUIT 0x12) + Game_ProcessGameEnd @ 1088:0b6d (HoF family) + Art
 * sciezki @ 10b8:0000/04df + Main_PumpMessage @ 1098:02d1 (g_nQuitFlag).
 * Zwiazek z Main_DrainMessages @ 11e8:02fb (drain loop az Main_PumpMessage
 * @ 1098:02d1 == 0, potem 12b0:004c select) i boot_shutdown (faza
 * save-prefs w tabeli: Config_SavePrefs tylko gdy 12b0:0004).
 * Port: predykaty flagi/dispatch LIVE 1:1 (no Win16, no globals); pompa
 * MSG (PEEKMESSAGE/TRANSLATE/DISPATCH) zostaje STUB w main_pump_message
 * (zob. sys.h). No exit(). */

/* Orig civ.prf image @ 1420:17b2 (Config_SavePrefs @ 1008:0bbf _LWRITE). */
#define CIV_PREFS_LEN 0x1a
#define CIV_PREFS_CHUNK_LEN 8
#define CIV_PREFS_NCHUNKS 3
#define CIV_PREFS_FLAGS_OFF 24

/* Orig Config_SavePrefs @ 1008:0bbf image side -> config_prefs_pack.
 * 1:1 sklada 26-B obraz: c0 -> [0..8), c1 -> [8..16), c2 -> [16..24),
 * flags LE -> [24..26). NULL c0/c1/c2/out -> NOENT (out zerowane gdy
 * cap > 0); cap < CIV_PREFS_LEN -> TRUNCATED (out nietkniete). No Win16,
 * no globals, no exit(), read-only oprocz out. */
civ_error_t config_prefs_pack(const uint8_t *c0, const uint8_t *c1,
                              const uint8_t *c2, uint16_t option_flags,
                              uint8_t *out, size_t cap);

/* Orig Config_LoadPrefs @ 1008:0b4a image side -> config_prefs_unpack.
 * 1:1 rozbiera 26-B obraz (img musi miec >= CIV_PREFS_LEN). NULL img ->
 * NOENT (outs zerowane); len < CIV_PREFS_LEN -> TRUNCATED (outs
 * nietkniete); NULL c0/c1/c2/flags_out -> NOENT (reszta nietknieta).
 * No Win16, no globals, no exit(), read-only oprocz outs. */
civ_error_t config_prefs_unpack(const uint8_t *img, size_t len,
                                uint8_t *c0, uint8_t *c1, uint8_t *c2,
                                uint16_t *flags_out);

/* Orig Config_SavePrefs @ 1008:0bbf guard (0 < g_nCurrentDrive @
 * 1228:0002) -> config_prefs_needs_path_setup. 1:1: 1 gdy drive > 0
 * (Dos_DriveReady @ 1000:1184 + Dos_SetPath @ 1000:116a), 0 wpp (na
 * Linuksie drive zawsze 0, zob. sys.h). No globals. */
int config_prefs_needs_path_setup(int drive);

/* Orig Main_DispatchQuit @ 1098:062f message side. */
#define CIV_WM_QUIT 0x12

/* Orig same (DAT_1420_54e8 == 0x12) -> main_quit_msg_is_quit.
 * 1:1: 1 gdy msg == CIV_WM_QUIT, 0 wpp. No globals. */
int main_quit_msg_is_quit(int msg);

/* Orig same (g_nQuitFlag @ 12d8:24ee == 1) -> main_quit_flag_requests_
 * shutdown. 1:1: 1 gdy flag == 1 (wartosci jak w game.h: 0 run, 1 quit,
 * -1 defeat, 2 ?menu), 0 wpp. No globals. */
int main_quit_flag_requests_shutdown(int quit_flag);

/* Orig same obie sciezki -> main_dispatch_quit_requests_shutdown. 1:1:
 * shutdown gdy (msg == 0x12) || (has_cmd != 0 && quit_flag == 1) (druga
 * galaz to Menu_DispatchCommand @ 1048:01cd + g_nQuitFlag == 1). No
 * globals. */
int main_dispatch_quit_requests_shutdown(int msg, int has_cmd,
                                         int quit_flag);

/* Orig Main_DrainMessages @ 11e8:02fb loop (Main_PumpMessage @ 1098:02d1
 * az 0) -> main_drain_should_continue. 1:1: pumped != 0. No globals. */
int main_drain_should_continue(int pumped);

/* --- Chain 2026-10-16 (19): pump-quit/drain discriminators (goals 1+5) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass (zob.
 * sys.h hierarchy block):
 *   Main_PumpQuitLoop @ 1008:0f77 fazy: DAT_1420_1d4c = 0; while (== 0)
 *     Main_DispatchQuitEntry @ 1098:061a; while (!= 0) Entry. Faza 0 =
 *     init/quit-przed (flaga czyszczona), faza 1 = po (flaga ustawiona
 *     przez Sys_DrainInput @ 11f0:4ea0 / Sys_PumpMessages @ 1108:0000
 *     sciezki); tick gry (game_tick -> game_turn_tick @ 1088:0000) nie
 *     bierze udzialu.
 *   Main_PumpMessage @ 1098:02d1 kody: WM_CHAR 0x102 -> Enqueue direct;
 *     WM_KEYDOWN 0x100 -> KeyToResChar map; ACCEL przed Dispatch.
 *   Main_DrainMessages @ 11e8:02fb ogon: 12b0:004c == 0 -> 1420:11f2+0xe,
 *     inaczej +0xf (offset w zwracanym far ptr).
 *   Main_DispatchQuitEntry @ 1098:061a: czysta trampolina (MOV AX,DS +
 *     PUSH CS near CALL Main_DispatchQuit @ 1098:062f); nie wymaga
 *     symbolu — logika kryta przez main_dispatch_quit_requests_shutdown
 *     (WM_QUIT 0x12 + has_cmd && quit_flag == 1); tu tylko doc, brak
 *     nowego symbolu celowo.
 * Port LIVE 1:1 na jawnych parametrach (no Win16, no globals). No exit(). */

/* Orig Main_PumpQuitLoop @ 1008:0f77 fazy -> main_pump_quit_should_continue.
 * 1:1: phase_first != 0 ? (flag_1d4c == 0) : (flag_1d4c != 0). */
int main_pump_quit_should_continue(int phase_first, int flag_1d4c);

/* Orig Main_PumpMessage @ 1098:02d1 gałęzie -> is_char / is_keydown.
 * 1:1: msg == 0x102 (WM_CHAR) / msg == 0x100 (WM_KEYDOWN). */
int main_pump_msg_is_char(int msg);
int main_pump_msg_is_keydown(int msg);

/* Orig Main_DrainMessages @ 11e8:02fb ogon -> main_drain_tail_offset.
 * 1:1: flag_004c == 0 -> 0x0e, inaczej 0x0f. */
int main_drain_tail_offset(int flag_004c);

/* --- Chain 2026-10-16 (20): timed pump flag (goal 3) ---
 * Orig Main_PumpUntilFlagClear @ 11e8:02a4 (civ.exe, x86:LE:16:Protected
 * Mode), decompiled this pass: DAT_12d8_24f0 = param; while (flag != 0)
 * PEEKMESSAGE + TRANSLATE + DISPATCH. Flaga to countdown timera: stawia ja
 * caller (wartosc = ticki), czysci TIMERPROC @ 1008:0d68 (was TIMERPROC:
 * if (flag != 0) flag--; decompiled this pass). Callerzy: Civ_ProcessUnitMoves
 * @ 11f0:0000, Diplomat_BribeUnit @ 11b8:07c2, Ship_ShowView @ 1070:0327
 * (param_2 << 1 timed preview), Replay_PlayLog @ 1180:02d3 /
 * Replay_ShowCivDestroyed @ 1180:17e9. PEEKMESSAGE-pompa Win16 -> STUB
 * (main_pump_message w sys.h); predykat + tick ponizej LIVE 1:1 na jawnych
 * parametrach (no Win16, no globals, no exit()). */

/* Orig Main_PumpUntilFlagClear @ 11e8:02a4 loop-guard (while flag != 0) ->
 * main_pump_until_flag_should_continue. 1:1 flag != 0. */
int main_pump_until_flag_should_continue(int flag);

/* Orig TIMERPROC @ 1008:0d68 body (if (flag != 0) flag--) ->
 * main_pump_timer_tick. 1:1 tick o 1 w dol, floor 0. */
int main_pump_timer_tick(int flag);

/* --- Chain 2026-10-16 (21): timer intervals (goal 3) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   WinMain @ 1008:0000: SETTIMER(hwnd 0, id 0, 5, TIMERPROC @ 1008:0d68 via
 *     MAKEPROCINSTANCE -> DAT_1420_5148/514a; 1008:00cd-00db) — 5 ms proc
 *     timer; tick = main_pump_timer_tick powyzej (DAT_12d8_24f0--).
 *   WdwMap_Open @ 1208:0af1: SETTIMER(hwnd main, id 1, 0xC8, NULL;
 *     1208:0e7b) — 200 ms WM_TIMER do map_wndproc_main (patrz map.h
 *     map_timer_interval_ms; NULL-proc = wiadomosc, nie callback).
 *   KILLTIMER w WinMain_Shutdown @ 1008:0329: 12d8:460e + 12d8:28fa
 *     (1008:0391/03bb; trzeci KILLTIMER to Dialog_MenuZ_Modal @ 1220:0cfd).
 *   Co jeszcze tyka: NIC poza flaga 12d8:24f0 — TIMERPROC robi wylacznie
 *     if (flag) flag--; animacje (Anim_AdvanceFrames @ 10b0:0461) ida przez
 *     Sys_GetTickScaled poll w pump-loop, nie przez timer; autosave/muzyka/
 *     blink ida przez 200 ms WM_TIMER + Sys_PumpMessages/Main_PumpQuitLoop
 *     (patrz sys.h), nie przez TIMERPROC. Boot: WinMain stawia 5 ms timer
 *     przed font/RSC, WdwMap doklada 200 ms, shutdown sciaga KILLTIMER
 *     najpierw (faza kill-timers w boot_shutdown_step_name).
 * Decyzja SDL_AddTimer: DOKUMENTACJA, BEZ implementacji — Win16 timery sa
 * HWND-bound (message pump w main.c while(!g_nQuitFlag)), SDL_AddTimer to
 * osobny watek bez message loop; mostek (SDL_AddTimer + SDL_PushEvent +
 * main.c pump) TBD caller-side jako STUB map_timer_proc. Ponizej tylko
 * LIVE interwaly + tick (no Win16, no globals, no exit()). */

/* Orig WinMain @ 1008:0000 SETTIMER-interval dla TIMERPROC @ 1008:0d68 ->
 * boot_timer_proc_interval_ms. 1:1 5 (ms). */
int boot_timer_proc_interval_ms(void);

/* --- Chain 2026-10-16 (U1): pozostale SETTIMER orig (fala uruchomieniowa).
 * Anchors (civ.exe, x86:LE:16:Protected Mode), decompiled 2026-10-16 via
 * xrefs to the SETTIMER @ 1608:008c thunk (4 sites total; 5 ms + 200 ms
 * sa w chain-21 powyzej, map_timer_interval_ms w map.h). Oba NULL-proc =
 * WM_TIMER do okna, nie callback. LIVE interwaly (no Win16, no globals). */

/* Orig App_RegisterWindowClasses @ 1098:06f8 tail: SETTIMER(0,0,0x10,NULL)
 * (1098:0948-0952; id -> DAT_12d8_460e, KILLTIMER w WinMain_Shutdown @
 * 1008:0329 obok 12d8:28fa) -> app_timer_interval_ms. 1:1 16 (ms). */
int app_timer_interval_ms(void);

/* Orig Dialog_MenuZ_Modal @ 1220:0156: SETTIMER(hwnd,2,0x64,NULL)
 * (1220:0202-0210; id -> [BP-0x46], fail = 'Can't initialize timer in
 * MenuZ' @ 1420:4a41 via FUN_1098_28e4, KILLTIMER(hwnd,id) @ 1220:0cfd) ->
 * menuz_timer_interval_ms. 1:1 100 (ms). Modalny tick menu (petla 0aa0:
 * Main_DrainMessages @ 11e8:02fb + PEEKMESSAGE @ 1608:0134). */
int menuz_timer_interval_ms(void);

/* --- Chain 2026-10-16 (22): Main_RunStartupSequence + Pic pump caller
 * (goal 5) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   ADRES-KOREKTA: zadanie mówi Main_RunStartupSequence @ 10e8:0000, ale
 *     10e8:0000 to NewGame_Wizard @ 10e8:0000 (wizard 0..5: difficulty
 *     1420:2a5a / competition 2aa9+2ac3 / tribe 2ad2+Custom / custom-world
 *     1220:16fd + finalize: clear units/cities/map 0x50x0x32, g_year 0xF060,
 *     leader Sound_PlayId, Map_GenerateRandomOrLoad; sole caller chain
 *     poniżej). Prawdziwy Main_RunStartupSequence @ 11e0:0000 (renamed;
 *     dispatcher MainMenu_WndProc @ 11e0:0221 choices over CIV_MAINMENU_TEXT
 *     @ 1420:435b; callees: Dlg_CreateTracked @ 1098:124f (0x94,0x41) +
 *     Gfx_StoreDC @ 1098:1104 + Palette_AnimateRange @ 1090:0376 +
 *     Anim_OpenSequence @ 10b0:008a + Sound_PlayId @ 1150:00c9 (3,0) +
 *     Anim_AdvanceFrames @ 10b0:0461 loop#1 + Anim_CloseSequence @
 *     10b0:0287 + Sound_PlayId (1,0) + [g_nMenuChoice 0/2/3 path:]
 *     Sys_DrainInput @ 11f0:4ea0 + Anim_OpenSequence (0x91e) +
 *     Anim frame-set 1000 via FUN_10b0_005b + WorldGen_PrepareEarth @
 *     1108:01bc + Anim_AdvanceFrames loop#2 (until DAT_13e0_001e) +
 *     Anim_CloseSequence + Sound_PlayId (1,0) + Dlg_DestroyTracked @
 *     1098:1612 + NewGame_Wizard @ 10e8:0000; else DestroyTracked only).
 * Związek z Anim: StartupSequence to GŁÓWNY driver Anim (2x open/advance/
 *   close loops; Anim_PumpIfActive @ 10b0:0000 to per-tick pump tego samego
 *   silnika przez Sys_Yield @ 1098:0157). Związek z intro: loop#1 to intro
 *   (Sound_PlayId 3 = intro jingle, potem 1 = STOP; dialog 0x94 tło;
 *   OPENING.wav @ 1420:3114 + MPLOGO.BMP (NE resource, brak filename
 *   string, patrz title.h) idą caller-side, nie w opkodach 1..0xe — patrz
 *   dialog.h anim chain). Związek z Pic: Pic_PlayShared @ 1030:0000 NIE
 *   jest wołany ze StartupSequence (sole Pic pump-caller to sam
 *   Pic_PlayShared dla Pic_PumpMenuCommand @ 1030:9aed, patrz game.h).
 * main_run_startup_sequence powyżej zostaje PARTIAL (OS-free liczniki/id
 *   poniżej LIVE + MenuZ/tracked/menu-text gałęzie probowane read-only w
 *   ciele; SDL modal/event bridge TBD, nigdy fake CIV_OK); liczniki/id
 *   dźwięków poniżej są LIVE 1:1 na jawnych stałych
 *   (no Win16, no globals, read-only). No exit(), C17. */

/* Orig Main_RunStartupSequence @ 11e0:0000 loop count ->
 * main_startup_anim_loops. 1:1 2 (loop#1 intro + loop#2 earth-prepare). */
int main_startup_anim_loops(void);

/* Orig same Sound_PlayId(3,0) before loop#1 -> main_startup_intro_sound.
 * 1:1 3 (intro jingle; STOP = 1 wraca via main_startup_stop_sound). */
int main_startup_intro_sound(void);

/* Orig same Sound_PlayId(1,0) after each loop -> main_startup_stop_sound.
 * 1:1 1 (STOP). */
int main_startup_stop_sound(void);

/* --- Chain 2026-10-16 (23): Startup wizard gate + order (goals 3/5) ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Main_RunStartupSequence @ 11e0:0000 (full order: Dlg_CreateTracked 0x94
 *     + Gfx_StoreDC @ 1098:1104 + Palette_AnimateRange @ 1090:0376 +
 *     Anim_OpenSequence @ 10b0:008a + DAT_13e0_001e = 0 + Sound_PlayId @
 *     1150:00c9 (3,0) + FILLRECT + Cursor_Set @ 1098:096c + loop#1
 *     Anim_AdvanceFrames @ 10b0:0461 + Anim_CloseSequence @ 10b0:0287 +
 *     Sound (1,0) + [g_nMenuChoice @ 12d8:44e2 w 0/2/3:] Sys_DrainInput @
 *     11f0:4ea0 + Anim_OpenSequence (0x91e) + Anim_RebaseClock @ 10b0:005b
 *     (1000) + WorldGen_PrepareEarth @ 1108:01bc + loop#2 Advance (until
 *     DAT_13e0_001e) + Close + Sound (1,0) + Dlg_DestroyTracked @ 1098:1612
 *     + NewGame_Wizard @ 10e8:0000; else DestroyTracked only).
 *   Menu choices (MainMenu_WndProc @ 11e0:0221 over CIV_MAINMENU_TEXT @
 *     1420:435b, g_nMenuChoice @ 12d8:44e2): 0 = Start New (random), 1 =
 *     Load Saved (Save_LoadDialog @ 1130:000e; skips the wizard), 2 = Play
 *     on EARTH (preset), 3 = Customize World (LoadEarth customizer @
 *     1108:0243), 4 = Hall of Fame, 5 = Quit. Startup wizard gate = 0/2/3.
 *   Wizard tail (NewGame_Wizard @ 10e8:0000 steps 0..5: difficulty 2a5a /
 *     competition 2aa9+2ac3 / tribe 2ad2+Custom / custom-world 1220:16fd x2
 *     / finalize: clear units/cities/map 0x50x0x32, g_year 0xF060 (-4000),
 *     leader Sound_PlayId, Map_GenerateRandomOrLoad @ 1028:0000; zwiad tylko,
 *     pelny port wizarda poza tym lancuchem) + Menu_PromptNation @ 1070:1f58
 *     (NIE startowy wybor — spaceship-preview w grze; patrz newgame.h) +
 *     tribe_get_name_list STUB (Res blob 0x94).
 *   Boot/title link: loop#1 to intro (title_play_intro_sdl w title.c gra
 *     MPLOGO.BMP + OPENING.wav best-effort; boot_run/main best-effort).
 * main_run_startup_sequence powyzej zostaje STUB (SDL modal/event bridge
 * TBD); gate ponizej jest LIVE 1:1 na jawnym wyborze (no Win16, no globals,
 * read-only). No exit(), C17. */

/* Orig Main_RunStartupSequence @ 11e0:0000 wizard gate
 * (g_nMenuChoice == 0/2/3) -> main_startup_should_run_wizard. 1:1 1 gdy
 * choice w {0, 2, 3} (nowa gra / EARTH / customize -> NewGame_Wizard),
 * 0 wpp (1 = load, 4/5/HoF/quit/out-of-range -> sam DestroyTracked). */
int main_startup_should_run_wizard(int menu_choice);

/* Orig Main_RunStartupSequence @ 11e0:0000 loop#2 break (decompiled
 * 2026-10-16: do { if (DAT_13e0_001e != 0) break; AdvanceFrames } while;
 * flag stawiany przez MainMenu_WndProc @ 11e0:0221 idle/timer branch) ->
 * main_startup_should_break. 1:1 nonzero (1) gdy flag != 0. */
int main_startup_should_break(int flag_1e);

/* --- Chain 2026-10-16 (27): memory thresholds + VideoCaps reading (goals 3/5) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Sys_CheckFreeMemory @ 11e8:0000 (boot; GETFREESPACE -> 1420:b390 +
 *     GETFREESYSTEMRESOURCES(1/0/2) -> b38e/b38a/b38c; if (free < 2500000 ||
 *     r1 < 0x23 || r0 < 0x23): MESSAGEBOX 1420:43b8 + Sys_ExitFallback(1);
 *     sole caller WinMain @ 1008:0000 memory step via boot_check_memory).
 *   Sys_CheckRuntimeMemory @ 11e8:0075 (caller Game_TurnLoopTick @ 1088:0000
 *     per-civ: GETFREESPACE + resources(1/0); if ((free < 1000000 || r1 < 10
 *     || r0 < 10) && 43b4 == 0): MESSAGEBOX 1420:4473 once (43b4 = 1); else
 *     if ((free < 300000 || r1 < 8 || r0 < 8) && 43b4 != 0 && 43b6 == 0):
 *     MESSAGEBOX 1420:44ff once (43b6 = 1)).
 *   WorldGen_ShowVideoCapsDialog @ 1060:01c1: GETDC + GETDEVICECAPS x13
 *     (8,10,0xc,0xe,0x16,0x18,0x22,0x24,0x26,0x28,0x2a,0x58,0x5a + war. 0x68/
 *     0x6a gdy 0x26&0x100) + Dlg_CreateTracked(0x94,0x41) +
 *     WorldGen_ProbeCustomFont(0x12) (h 0x12 weight 400) + rows via
 *     WorldGen_DrawVideoCapsRow @ 1060:005b / WorldGen_DrawVideoCapsFlag @
 *     1060:00d1 (renamed this pass) + Sys_WaitForInput modal. Po co: debug/
 *     requirements dump (odczyt caps, brak SET trybu); kiedy: BRAK callerow
 *     (martwy/debug, nie boot/F-key/blad — zaden kod go nie wola).
 *     Zwiazek z boot_video_caps (checklist 1420:0af4 dla Video_CheckDriverCaps
 *     @ 1008:059a; count/describe = 0/NULL do dumpu) i memory STUBami
 *     (sys_check_free/runtime_memory): caps to ODCZYT urzadzenia, memory to
 *     PROGI ilosciowe. Dialog GDI -> STUB (worldgen_show_video_caps_dialog
 *     w worldgen.h); logika progow ponizej LIVE 1:1 (no Win16, no globals).
 * No exit(), C17. */
#define CIV_BOOT_FREE_BYTES_MIN 2500000L
#define CIV_BOOT_FREE_RES_MIN 0x23
#define CIV_RUN_FREE_BYTES_WARN 1000000L
#define CIV_RUN_FREE_RES_WARN 10
#define CIV_RUN_FREE_BYTES_FINAL 300000L
#define CIV_RUN_FREE_RES_FINAL 8

/* Orig Sys_CheckFreeMemory @ 11e8:0000 gate -> boot_free_memory_is_low.
 * 1:1 1 gdy free_bytes < 2500000 || res1 < 0x23 || res0 < 0x23. */
int boot_free_memory_is_low(long free_bytes, int res1, int res0);

/* Orig Sys_CheckRuntimeMemory @ 11e8:0075 first gate (+ 43b4 == 0) ->
 * boot_runtime_needs_first_warning. 1:1 (free < 1000000 || r1 < 10 ||
 * r0 < 10) && already_warned == 0. */
int boot_runtime_needs_first_warning(long free_bytes, int res1, int res0,
                                     int already_warned);

/* Orig same second gate (43b4 != 0 && 43b6 == 0) ->
 * boot_runtime_needs_final_warning. 1:1 (free < 300000 || r1 < 8 ||
 * r0 < 8) && warned_once != 0 && warned_final == 0. */
int boot_runtime_needs_final_warning(long free_bytes, int res1, int res0,
                                      int warned_once, int warned_final);

/* --- Chain 2026-10-16 (28): startup IDs + rebase ticks (goal 5) ---
 * Orig Main_RunStartupSequence @ 11e0:0000 (civ.exe, x86:LE:16:Protected
 * Mode), decompiled this pass (ref unpacked/civ.exe):
 *   Dlg_CreateTracked(0x94,0x41) loop#1 intro host (dialog 0x94 + flags
 *   0x41: center + show via dlg_create_should_center/show, preset size via
 *   dlg_id_needs_preset_size, ctls via dlg_ctl_is_tracked_init; see
 *   dialog.h) + Anim_OpenSequence @ 10b0:008a + Sound_PlayId(3,0) intro +
 *   loop#1 Anim_AdvanceFrames @ 10b0:0461 while(c == 0) + Close + Sound(1,0)
 *   + [choice 0/2/3:] Sys_DrainInput @ 11f0:4ea0 + Anim_OpenSequence seq
 *   0x91e (loop#2 earth-prepare id, NOT a dialog id) + Anim_RebaseClock @
 *   10b0:005b (1000 ticks: skips the intro-overlay/map-gen time) +
 *   WorldGen_PrepareEarth @ 1108:01bc + loop#2 Advance (until DAT_13e0_001e
 *   via main_startup_should_break) + Close + Sound(1,0) +
 *   Dlg_DestroyTracked @ 1098:1612 + NewGame_Wizard @ 10e8:0000; else
 *   DestroyTracked only. main_run_startup_sequence stays STUB (SDL
 *   modal/event bridge TBD); IDs/ticks below are LIVE 1:1 constants on
 *   explicit params (no Win16, no globals, read-only). No exit(), C17. */

/* Orig same Dlg_CreateTracked(0x94,0x41) param_1 -> main_startup_intro_dialog_id.
 * 1:1 0x94 (intro/PIC host dialog; earth-custom is 0x8f, wizard 0x90). */
int main_startup_intro_dialog_id(void);

/* Orig same param_2 -> main_startup_intro_dialog_flags. 1:1 0x41. */
int main_startup_intro_dialog_flags(void);

/* Orig same loop#2 Anim_OpenSequence seq id -> main_startup_earth_seq_id.
 * 1:1 0x91e (sequence id, NOT a dialog id; 0x94 above is the dialog). */
int main_startup_earth_seq_id(void);

/* Orig same loop#2 Anim_RebaseClock param -> main_startup_rebase_ticks.
 * 1:1 1000 (menu idle uses 100000 / 0x48c via MainMenu_WndProc @ 11e0:0221,
 * owned by mainmenu, not here). */
int main_startup_rebase_ticks(void);

/* --- Chain 2026-10-16 (P1): intro host procedural widgets (goal P1) ---
 * Orig Main_RunStartupSequence @ 11e0:0000 loop#1 intro host:
 * Dlg_CreateTracked @ 1098:124f (0x94,0x41) + Dialog_MenuZ_Modal @ 1220:0156
 * (civ.exe, x86:LE:16:Protected Mode; ref unpacked/civ.exe strings
 * "RanDoMRadio"/"RanDoMStatic" + "Can't initialize timer in MenuZ" @
 * 1420:4a41, file offset 0xbba41). Procedural host below is LIVE 1:1 on
 * explicit params (no Win16, no globals, read-only): preset size + dialog-id
 * gate place the centered 0x200x0x140 host without any bitmap, so the intro
 * frame is drivable (main_run_startup_sequence PARTIAL dispatcher) before
 * frames land. Faithful look waits for the frame decode via rsc_get_indexed
 * @ 1190:0a29 (+ payload via rsc_load_to_mem @ 1190:0fda, see rsc_loader.h).
 * Pump/host tails stay STUB in main_run_startup_sequence with TODO Orig
 * 1220:0156 / 1098:124f. No exit(), C17. */

/* Orig Dlg_CreateTracked @ 1098:124f preset branch for the 0x94 intro host
 * (DAT_12d8_002e = 0x200, see dlg_preset_w in dialog.h) ->
 * main_startup_intro_proc_w. 1:1 0x200 (procedural host width). No globals. */
int main_startup_intro_proc_w(void);

/* Orig Dlg_CreateTracked @ 1098:124f preset branch (DAT_12d8_002c = 0x140,
 * see dlg_preset_h in dialog.h) -> main_startup_intro_proc_h. 1:1 0x140
 * (procedural host height). No globals. */
int main_startup_intro_proc_h(void);

/* Orig same dialog-id gate (param_1 == 0x94 intro/PIC host, see
 * dlg_id_needs_preset_size) -> main_startup_procedural_ready. 1:1 1 when
 * dlg_id == 0x94 (procedural fallback covers the intro host), 0 otherwise
 * (0x8f earth-custom / 0x90 wizard frame need their own art). No globals. */
int main_startup_procedural_ready(int dlg_id);

/* Orig frame source gate (faithful bitmaps via rsc_get_indexed @ 1190:0a29)
 * -> main_startup_needs_rsc_frame. 1:1 1 (procedural host is drawn until
 * the RSC frame decode lands; never 0 while the faithful look is pending).
 * No globals. */
int main_startup_needs_rsc_frame(void);

/* Orig Dlg_CreateTracked @ 1098:124f palette tail (0xec entries via
 * Palette_CopyRange @ 1098:201d, exercised through anim_pal_copy @
 * 10b0:0461 case 2; host entry Dlg_CreateTracked(0x94,0x41) in
 * Main_RunStartupSequence @ 11e0:0000, ref unpacked/civ.exe strings
 * "RanDoMRadio" @ 0xb7ac6 / "RanDoMStatic" @ 0xb7ad2) ->
 * main_startup_host_palette_ok. Twin of mainmenu_host_palette_ok (see
 * mainmenu.h; same Orig, owned here per the startup loop#1 scope). 1:1 1
 * when the tracked palette count is 0xec and a 2-entry anim_pal_copy
 * self-test round-trips; 0 otherwise. OS-free, no Win16 (CREATEDIALOG +
 * MAKEPROCINSTANCE + GETDC/SELECTPALETTE/REALIZEPALETTE tails stay STUB in
 * dlg_create_tracked @ 1098:124f with TODO Orig there), no globals, no
 * exit(), read-only except stack locals. */
int main_startup_host_palette_ok(void);

/* --- Chain 2026-10-16 (P2): startup SDL modal/event bridge (goal P2) ---
 * Orig Main_RunStartupSequence @ 11e0:0000 loop shape (civ.exe,
 * x86:LE:16:Protected Mode; ref unpacked/civ.exe): loop#1 intro =
 * while (Anim_AdvanceFrames @ 10b0:0461 == 0) pump (dialog 0x94 host via
 * Dlg_CreateTracked @ 1098:124f (0x94,0x41) + GfxPort_Create @ 10d0:0000 x2
 * pair at ctx offs +9/+0x62 via Anim_OpenSequence @ 10b0:008a); loop#2
 * earth-prepare = do { if (DAT_13e0_001e != 0) break; AdvanceFrames }
 * while (c == 0) (flag via MainMenu_WndProc @ 11e0:0221 idle/timer, see
 * main_startup_should_break). Modal pump shape comes from
 * Dialog_MenuZ_Modal @ 1220:0156 (PEEKMESSAGE/ISDIALOGMESSAGE drain with
 * 100 ms SETTIMER @ 1220:0202 via menuz_timer_interval_ms, cancel -1 via
 * dialog_menuz_cancel_code; dismiss = button/ESC/close, noise = motion).
 * SDL mapping (no Win16, no globals, read-only except the explicit break
 * out): SDL_PollEvent drain replaces PEEKMESSAGE, dismiss events set the
 * break flag (loop#2 DAT_13e0_001e image), the GfxPort x2 pair stays a
 * geometry gate (0x200x0x140 preset, renderer-borrowed textures TBD in
 * gfx_port_create, see video.h). Helpers below are LIVE 1:1 on explicit
 * params; the device tails (CREATEDIALOG/palette/GfxPort textures/RSC
 * decode) stay STUB in main_run_startup_sequence above with TODO Orig
 * 11e0:0000, never fake CIV_OK. No exit(), C17. */

/* Orig GfxPort_Create @ 10d0:0000 x2 pair backing for the 0x94 intro host
 * (Anim_OpenSequence @ 10b0:008a ctx offs +9/+0x62 + Dlg_CreateTracked @
 * 1098:124f preset 0x200x0x140) -> main_startup_port_pair_ok. 1:1 1 when
 * anim_seq_port_count() == 2 && anim_seq_port_off(0) == 9 &&
 * anim_seq_port_off(1) == 0x62 && gfx_startup_port_size_ok(0x200,0x140)
 * == 1 && gfx_port_bitmap_pitch(0x200) == 0x200 &&
 * art_port_padded_width(0x200) == 0x200 && gfx_remap_stride(0x200) ==
 * 0x200 (even pitch ((w+1)/2*2); INDEX8 TARGET textures need a borrowed
 * renderer, see gfx_port_create, so geometry only here). 0 otherwise
 * (any geometry drift = BAD host, caller maps to BAD_DATA). No SDL
 * init, no globals. */
int main_startup_port_pair_ok(void);

/* Orig same loop-done shape (loop#1 c != 0 done + loop#2 DAT_13e0_001e
 * break) -> main_startup_modal_is_done. 1:1 1 when adv_done != 0 (loop#1
 * Anim_AdvanceFrames finished, nonzero = done) or break_flag != 0
 * (loop#2 DAT_13e0_001e / SDL dismiss, see main_startup_pump_events),
 * 0 while busy (adv 0 + no break = keep pumping). No globals. */
int main_startup_modal_is_done(int adv_done, int break_flag);

/* Orig Dialog_MenuZ_Modal @ 1220:0156 dismiss shape (button/ESC/close vs
 * motion/window noise) -> main_startup_event_is_dismiss. 1:1 1 for the
 * SDL dismiss family (SDL_EVENT_QUIT close + SDL_EVENT_KEY_DOWN key incl.
 * ESC + SDL_EVENT_MOUSE_BUTTON_DOWN click = MenuZ button/close), 0 for
 * everything else (motion/window/user-noise = keep pumping). Takes the
 * SDL event type explicitly (Uint32 image, no SDL init needed for the
 * predicate). No globals. */
int main_startup_event_is_dismiss(uint32_t sdl_type);

/* Orig same modal pump (PEEKMESSAGE/ISDIALOGMESSAGE drain per tick) ->
 * main_startup_pump_events. SDL3 drain bridge LIVE (no Win16, no
 * globals, read-only except the explicit out): SDL_PollEvent loop, any
 * dismiss event (see main_startup_event_is_dismiss) sets *break_flag = 1
 * (sticky: already-1 stays 1; DAT_13e0_001e image for loop#2); noise
 * events are dropped like ISDIALOGMESSAGE-non-dialog msgs. NULL
 * break_flag -> NOENT (untouched); else CIV_OK after draining (0 events
 * is still OK, caller keeps pumping while modal_is_done == 0 and paces
 * ticks caller-side via main_startup_tick_is_due below). Safe
 * before SDL_Init (SDL_PollEvent reports none). TODO: Orig
 * FUN_11e0_0000 @ 11e0:0000 (CREATEDIALOG + MAKEPROCINSTANCE +
 * GETDC/SELECTPALETTE/REALIZEPALETTE + 0xec palette + tracked-stack tails
 * live in dlg_create_tracked @ 1098:124f, STUB there; GfxPort textures /
 * RSC decode tails STUB in main_run_startup_sequence above; never fake
 * CIV_OK). */
civ_error_t main_startup_pump_events(int *break_flag);

/* Orig Dialog_MenuZ_Modal @ 1220:0156 SETTIMER(hwnd,2,0x64,NULL) tick (@
 * 1220:0202, KILLTIMER @ 1220:0cfd; fail = 'Can''t initialize timer in
 * MenuZ' @ 1420:4a41, ref unpacked/civ.exe file offset 0xbba41, windows.c;
 * shared with the MainMenu_WndProc @ 11e0:0221 modal, same 100 ms via
 * menuz_timer_interval_ms) paced caller-side via Sys_GetTickScaled @
 * 1100:02e1 (GETTICKCOUNT * 60 / 1000, see sys_tick_scaled_from in sys.h)
 * -> main_startup_tick_is_due. 1:1 due when (now_scaled - last_scaled) >=
 * sys_tick_scaled_from(100 ms) (100 ms = 6 scaled ticks; unsigned
 * subtraction, so a 32-bit wrap still measures the elapsed span). Pure
 * predicate on explicit scaled ticks (no SDL, no Win16, no globals, no
 * exit(), read-only); the caller drains via main_startup_pump_events
 * above once per due tick (loop#2 DAT_13e0_001e image). Twin of
 * mainmenu_tick_is_due (see mainmenu.h; same Orig, owned here per the
 * startup loop#2 scope). */
int main_startup_tick_is_due(unsigned long last_scaled, unsigned long now_scaled);

/* --- Chain 2026-10-17 (P0): startup anim ctx + seq-resolve + menu-text
 * (goal P0 anim_open_sequence @ 10b0:008a) ---
 * Orig Main_RunStartupSequence @ 11e0:0000 loop#1 intro host (civ.exe,
 * x86:LE:16:Protected Mode; ref unpacked/civ.exe read-only NE Win3.x
 * 832512 B): Dlg_CreateTracked @ 1098:124f (0x94,0x41) host +
 * Anim_OpenSequence @ 10b0:008a (mem_alloc_lock 0xa6 ctx + GfxPort_Create
 * x2 offs +9/+0x62 + DAT_1420_1f40 = ctx, see dialog.h anim_seq_ctx_size
 * / anim_seq_port_count_off) + Dialog_MenuZ_Modal @ 1220:0156 pump shape
 * (100 ms SETTIMER @ 1220:0202, cancel -1) over CIV_MAINMENU_TEXT @
 * 1420:435b (file offset 0xbb35b, 6 items, verified
 * "_Start a New Game_..._Quit"). OS-free 1:1 on explicit params (no
 * Win16, no globals, read-only); device tails (CREATEDIALOG / GfxPort
 * textures / RSC decode via rsc_get_indexed @ 1190:0a29) stay STUB in
 * main_run_startup_sequence above with TODO Orig there, never fake CIV_OK.
 * No exit(), C17. */

/* Orig Anim_OpenSequence @ 10b0:008a ctx half (0xa6) + Dlg_CreateTracked
 * @ 1098:124f ctx half (0x18) -> main_startup_anim_ctx_ok. 1:1 1 when
 * anim_seq_ctx_size() == 0xa6 && dlg_tracked_ctx_size() == 0x18 (== also
 * CIV_DLG_TRACKED_CTX_SIZE); 0 otherwise. Proves the intro-host backing
 * pair before the GfxPort texture bridge. No globals. */
int main_startup_anim_ctx_ok(void);

/* Orig Anim_OpenSequence @ 10b0:008a count half (param_3 == 0 -> count
 * recs until opcode 0 via anim_seq_count_frames, stride 0xf words; else
 * param_3; see anim_seq_resolve_count in dialog.h) intro path ->
 * main_startup_intro_seq_resolve. 1:1 anim_seq_resolve_count(0, counted)
 * (intro open uses the counted path; loop#2 earth-prepare uses explicit
 * 0x91e via main_startup_earth_seq_id, not here). counted < 0 -> -1
 * (propagated range shape, no scan here). No globals. */
int main_startup_intro_seq_resolve(int counted);

/* Orig Main_RunStartupSequence @ 11e0:0000 menu-text shape (Util_StrCopy
 * @ 1000:36b2 over CIV_MAINMENU_TEXT @ 1420:435b, ref unpacked/civ.exe
 * file offset 0xbb35b, 6 items; same '_' split as Dialog_SplitMenuText @
 * 1220:0000) -> main_startup_menu_text_ok. 1:1 1 when
 * dialog_split_menu_count(CIV_MAINMENU_TEXT) == 6 &&
 * mainmenu_item_count() == 6 && both agree; 0 otherwise. Shared P0->P1
 * gate (startup dispatcher reuses the MainMenu_WndProc @ 11e0:0221 text).
 * No Win16, no globals. */
int main_startup_menu_text_ok(void);

/* --- Chain 2026-10-17 (boot-text): Init_UnitFontTables @ 10e8:2d46 OS-free
 * + Text_Draw* @ 10d8:* OS-free (goal; boot-owned only, dialog/video/map
 * untouched) ---
 * Orig (civ.exe NE Win3.x, ref unpacked/civ.exe read-only 832512 B, MZ at
 * +0 e_lfanew 0x60 -> NE; strings FIRSTMOVE/PILLAGE @ dump + "End of
 * Turn"/"Press Return" for 10d8:00e0; no bytes copied):
 *   Init_UnitFontTables @ 10e8:2d46 (sibling NewGame_Wizard @ 10e8:0000;
 *     Util_MemCopyFar 0x2a02 6 B + 0x2a12 64 B + GLOBALLOCK DAT_1420_b388
 *     palette, then 8x FIRSTMOVE idx 0..7 + 8x PILLAGE idx 2..9 best-match
 *     over 0x40 entries via Gfx_CalcColorDistSq @ 10e8:2cc1 keeping the
 *     min, then the same 16 B key-remap as Game_InitGlobalTables @
 *     11e8:0337 (see boot_key_remap_value), then 16x font-map best-match
 *     when DAT_1420_17a8 == 0 else fixed 16 B (ff,fb,f9,1,5,4,fc,6,fa,2,3,
 *     fb,7,f8,f8,0), then 8x28 icon recolor via Gfx_ReplaceIconColor @
 *     11a8:022f + Sys_Yield @ 1098:0157; fixed/alt byte images LIVE via
 *     newgame_unit_font_fixed/alt_at, see newgame.h; distance LIVE via
 *     gfx_calc_color_dist_sq, see video.h).
 *   Text_DrawShadowedCentered @ 10d8:0495 (DispatchFont @ 11e8:0b40 +
 *     MeasureWidth @ 10d8:05b8 + x-w/2 + DrawShadowed @ 10d8:02df).
 *   Text_MeasureWidth @ 10d8:05b8 (Util_StrCopy + PackPascal @ 1100:0000 +
 *     MeasurePascal @ 1100:00a1 + (w*10)/0x10 scale, see text_measure_scaled
 *     in dialog.h).
 *   Text_DrawPascalAtOrigin @ 1100:017b (SETBKMODE(1) + SETTEXTALIGN(1) +
 *     PascalToCStr + TEXTOUT at 0,0 + restore).
 * Split (AGENTS.md triage): OS-free counts/branch/best-pick + FNT measure
 * below are LIVE 1:1 on explicit params (no far, no GLOBALLOCK, no GDI,
 * no globals, no exit(), read-only except explicit outs); far-ptr +
 * GLOBALLOCK + SELECTOBJECT/MOVETO/TEXTOUT/SETBKMODE device tails stay
 * STUB/PARTIAL with TODO Orig (no fake-OK). Names text_draw_shadowed_
 * centered / text_measure_width / text_draw_pascal_at_origin are free
 * (dialog owns the sibling text_draw_shadowed/at/clipped/direct/centered/
 * header_centered + text_measure_scaled/text_resolve_color/text_shadow_pos;
 * map owns text_draw_status_row/with_font) so no duplicate symbols. */

/* Orig Init_UnitFontTables @ 10e8:2d46 FIRSTMOVE loop count -> boot_unit_font_firstmove_count. 1:1 8. */
int boot_unit_font_firstmove_count(void);
/* Orig same PILLAGE loop count -> boot_unit_font_pillage_count. 1:1 8. */
int boot_unit_font_pillage_count(void);
/* Orig same PILLAGE first index (idx 2..9) -> boot_unit_font_pillage_first. 1:1 2. */
int boot_unit_font_pillage_first(void);
/* Orig same palette scan extent (0x40 entries via GLOBALLOCK b388) -> boot_unit_font_palette_entries. 1:1 0x40. */
int boot_unit_font_palette_entries(void);
/* Orig same font-map loop count (16x when 17a8 == 0) -> boot_unit_font_map_count. 1:1 16. */
int boot_unit_font_map_count(void);
/* Orig same icon-recolor group count (8x28 via Gfx_ReplaceIconColor @ 11a8:022f) -> boot_unit_font_icon_count. 1:1 8. */
int boot_unit_font_icon_count(void);
/* Orig same per-icon recolor length -> boot_unit_font_icon_len. 1:1 28. */
int boot_unit_font_icon_len(void);
/* Orig same font-map branch (DAT_1420_17a8 != 0 takes the fixed 16 B) -> boot_unit_font_needs_alt. 1:1 convert_17a8 != 0. */
int boot_unit_font_needs_alt(int convert_17a8);
/* Orig same best-match core (Gfx_CalcColorDistSq @ 10e8:2cc1 min over 0x40) -> boot_unit_font_best_pick. 1:1 index of min dist (first wins ties, like orig keeping < only); pal is 64x3 borrowed bytes, target is 3 B. NULL either -> -1; n <= 0 -> -1. No Win16. */
int boot_unit_font_best_pick(const uint8_t *pal, int n, const uint8_t target[3]);
/* Orig Init_UnitFontTables @ 10e8:2d46 GLOBALLOCK DAT_1420_b388 size side -> boot_unit_font_palette_bytes. 1:1 0x40 entries x 3 B (RGB triples behind the lock; ref unpacked/civ.exe read-only NE Win3.x 832512 B, strings FIRSTMOVE @ 0xbb5fc / PILLAGE @ 0xbb606). No Win16 handle here, just the byte span. */
int boot_unit_font_palette_bytes(void);
/* Orig same GLOBALLOCK DAT_1420_b388 success shape -> boot_unit_font_palette_lock_ok. 1:1 lock-ok = pal != NULL && len >= 0x40*3 (the 0x40 RGB triples the 8+8+16 best-match loops scan via Gfx_CalcColorDistSq @ 10e8:2cc1); 0 otherwise (NULL/short = failed lock). No Win16, no globals. */
int boot_unit_font_palette_lock_ok(const uint8_t *pal, size_t len);
/* Orig Gfx_ReplaceIconColor @ 11a8:022f scan half (caller Init_UnitFontTables @ 10e8:2d46 8x28 + Sys_Yield @ 1098:0157) -> boot_unit_font_replace_color. 1:1 w*h byte scan old -> new on an explicit caller buffer (icon bytes are palette indices, no BITBLT/GLOBALLOCK handle here): NULL px -> NOENT; n == 0 -> CIV_OK (empty scan); else replaces every old_c with new_c (including old == new no-op) -> CIV_OK. No Win16, no globals, read-only except px. GDI handle/selector tail stays STUB in boot_init_unit_font_tables with TODO Orig there. */
civ_error_t boot_unit_font_replace_color(uint8_t *px, size_t n, uint8_t old_c, uint8_t new_c);

/* Orig Text_DrawShadowedCentered @ 10d8:0495 dispatcher -> text_draw_shadowed_centered. PARTIAL: OS-free centered_x (x-w/2 via text_centered_x) + shadow_pos (via text_shadow_pos) + measure scale (via text_measure_scaled) probes LIVE; SELECTOBJECT/DispatchFont/TEXTOUT GDI tail STUB -> CIV_ERR_UNSUPPORTED with TODO Orig. NULL text -> NOENT. */
civ_error_t text_draw_shadowed_centered(const char *text, int x, int y, int color, int shadow);
/* Orig Text_MeasureWidth @ 10d8:05b8 FNT side -> text_measure_width. OS-free 1:1 on borrowed FNT bytes (same image as font_fnt_char_entry, never writes civfonts.fon): sum of per-char entry widths via font_text_width, then (w*10)/0x10 scale like Text_MeasureWidth. NULL data/info/s -> 0; short/corrupt -> partial prefix (same contract as font_text_width). No SDL, no globals. */
int text_measure_width(const uint8_t *data, size_t len, const font_fnt_info_t *info, const char *s);
/* Orig Text_DrawPascalAtOrigin @ 1100:017b -> text_draw_pascal_at_origin. STUB: pascal is buf[0]-is-len Pascal (see text_pack_pascal in dialog.h); SETBKMODE/SETTEXTALIGN/TEXTOUT tail -> CIV_ERR_UNSUPPORTED with TODO Orig. NULL -> NOENT. */
civ_error_t text_draw_pascal_at_origin(const char *pascal, int color);

#endif /* CIVLIN_BOOT_H */
