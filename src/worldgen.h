/* World generation (random / earth / custom) -> SDL3.
 *
 * Ghidra anchors (civ.exe), renamed 2026-10-15 (Orig FUN_* -> New):
 *   Map_GenerateRandomOrLoad @ 1028:0000 (param==0: random gen via tile
 *     passes below; plays Sound_PlayId(4) = OPENING.wav at start.
 *     param!=0: HMEMCPY 64000B preset via 10b8:079d)
 *   Map_GenSeedOneIsland @ 1028:0945 (scratch FillRect + Map_GenPaintBlob
 *     + copy +0xa0 -> main with +1, counting 1248:0002)
 *   Map_GenPaintBlob @ 1028:09ef (random walk painting 0x0f; direction
 *     table TBD)
 *   Map_LabelContinents @ 1028:0a93 (connected-components labeling over
 *     0x50 x 0x32 + rank sort; calls 1028:12c6/162c/0fb4, border FillRects)
 *   Map_TryPlaceIsolatedTile @ 1028:05e7 (implemented in map.c)
 *   Map_GenRivers @ 1028:066a (river random-walk gen)
 *   Map_SpreadPollution @ 1028:1afd (fallout tiles; from
 *     Game_DoPollutionAndWonderRace)
 *   Map_GetTile @ 11e8:0dcf / Map_SetTile @ 11e8:0e19 (see map.h)
 *   Map_FillRect @ 1028:1d15 / Map_DrawRectBorder @ 1028:1ca7 (see map.h)
 *   Rng_Range @ 11c8:003d / Sys_Yield @ 1098:0157 (see sys.h)
 *   WorldGen_LoadEarth @ 1108:0243 (Orig FUN_1108_0243; choice 3 Play on
 *     EARTH from MainMenu_WndProc @ 11e0:0221; preset 64000B via 10b8:079d
 *     + tail 1028:1d15/0a93)
 *   WorldGen_PrepareEarth @ 1108:01bc (Orig neighbour of 1108:0243; exact
 *     role TBD from decompile near 1108:0243)
 *   WorldGen_CopyPreset @ 10b8:079d (Orig FUN_10b8_079d; HMEMCPY 64000B
 *     preset, callee of Map_GenerateRandomOrLoad param!=0; segment-neighbour
 *     Art_MaybeConvertArtwork @ 10b8:0000 with the 1420:20a9 prompt gates
 *     artwork before this copy -- stub lives in boot.h/boot.c, see link
 *     on worldgen_copy_preset below)
 *   Custom editor: NewGame_Wizard steps 3/4 + Ctrl_* @ 1058:xxxx
 *     (RANDOMRADIOPROC @ 1058:06ab, RANDOMSTATICPROC @ 1058:081a,
 *      RANDOMUSERPROC @ 1058:0921, REGIONPROC @ 1058:09f6)
 *   Map windows: WdwMap_Open @ 1208:0af1 etc. (see map.h)
 *
 * Map dims observed in NewGame_Wizard finalize: 0x50 x 0x32 tiles.
 */
#ifndef CIVLIN_WORLDGEN_H
#define CIVLIN_WORLDGEN_H

#include <stddef.h>
#include <stdint.h>

#include "civ_common.h"

#define CIV_MAP_W 0x50u
#define CIV_MAP_H 0x32u
#define CIV_MAP_PRESET_BYTES 64000u

/* Random-map pipeline (Map_GenerateRandomOrLoad @ 1028:0000 param==0 path).
 * Ordered pipeline (param==0): Sound_PlayId(4)=OPENING at start, then
 * seed_one_island -> paint_blob loop (Rng_Range sized) -> label_continents
 * -> gen_rivers -> score_city_sites -> build_continent/ocean_links, with
 * Sys_Yield per row + Rng_Range for every random choice. Callers outward:
 * NewGame path via MainMenu choice; param!=0 takes the preset path below.
 * Win16/scratch-buffer tail TBD -> stub CIV_ERR_UNSUPPORTED. */
civ_error_t worldgen_generate_random(void);

/* Preset load (param!=0 path: 64000B image + 1028:1d15/0a93 tail).
 * First step is WorldGen_CopyPreset @ 10b8:079d below (LIVE memcpy);
 * tail is Map_FillRect(0xa0,0,0xa0,200,0) + Map_LabelContinents @
 * 1028:0a93 (both LIVE). Caller supplies the image (Art frame 0x89
 * decode stays caller-side, see video.h); __far/HMEMCPY/GLOBALLOCK-0x600
 * maps to map_buffer() offsets. NULL -> NOENT, len != 64000 -> RANGE. */
civ_error_t worldgen_load_preset(const uint8_t *image, size_t len);

/* Orig: FUN_1108_0243 @ 1108:0243 -> worldgen_load_earth (WorldGen_LoadEarth).
 * Play on EARTH (MainMenu choice 3 via MainMenu_WndProc @ 11e0:0221).
 * Preset path: 64000B image via WorldGen_CopyPreset @ 10b8:079d, then tail
 * Map_FillRect @ 1028:1d15 / Map_LabelContinents @ 1028:0a93.
 * Params: orig takes no caller buffer (preset source TBD -- RSC entry or
 * file image via Art_LoadFrame @ 10b8:0e00 frame 0x89 caller-side, NOT a
 * caller-supplied pointer); takes void here so mainmenu_dispatch can call
 * it directly. PARTIAL: OS-free frame/size/dialog probes LIVE 1:1 in the
 * body (BAD_DATA on drift, no fake-OK); Win16 file/resource load + modal
 * dialog 0x8f tail -> CIV_ERR_UNSUPPORTED. */
civ_error_t worldgen_load_earth(void);

/* OS-free customize-dialog cell math behind WorldGen_LoadEarth @
 * 1108:0243 (4 rows x 3 choice cols + help col; dialog IDs from 0x323:
 * row = (id - 0x323) / 4, col = (id - 0x323) % 4; col < 3 picks the
 * option, col == 3 opens the help text via NewGame customize @ 10e0:0000
 * with Temperature @ 1420:2e28 / Climate @ 1420:2e34 / 0x2e23 / 0x2e3c).
 * worldgen_dialog_id_to_cell maps the dialog control ID to (row, col);
 * worldgen_custom_apply writes settings[row] = col for choice cols
 * (help col leaves settings untouched, still CIV_OK). NOENT on NULL
 * outs/settings, RANGE on id below 0x323, row/col outside 0..3, else
 * CIV_OK. No Win16, no globals, no exit(). */
civ_error_t worldgen_dialog_id_to_cell(int dialog_id, int *row_out,
                                       int *col_out);
civ_error_t worldgen_custom_apply(int *settings, int row, int col);

/* Orig: FUN_1108_01bc @ 1108:01bc -> worldgen_prepare_earth
 * (WorldGen_PrepareEarth). Pre-step w Main_RunStartupSequence @ 11e0:0000
 * (loop#2 przed NewGame_Wizard @ 10e8:0000): Cursor_Set(0x84) +
 * Sound_PlayId(1) STOP + dispatch g_nMenuChoice @ 12d8:44e2 (patrz
 * mainmenu.h/boot.h: 0 = Start New, 1 = Load, 2 = EARTH, 3 = Customize):
 * 0 -> Map_GenerateRandomOrLoad(0) random (+ preset 4x1 talii), 2 ->
 * Map_GenerateRandomOrLoad(1) earth-preset (frame 0x89 via
 * Art_LoadFrameData @ 10b8:079d + HMEMCPY 64000), 3 -> Map(0) custom/random
 * (ustawienia z WorldGen_LoadEarth @ 1108:0243 dialog 0x8f), inne (1/4/5)
 * -> return 1 bez mapy. PARTIAL: OS-free dispatch probes LIVE 1:1 via
 * worldgen_prepare_mode below (BAD_DATA on drift, no fake-OK);
 * Cursor/Sound/Map tails -> CIV_ERR_UNSUPPORTED. */
civ_error_t worldgen_prepare_earth(void);

/* Orig WorldGen_PrepareEarth @ 1108:01bc dispatch -> worldgen_prepare_mode.
 * 1:1 g_nMenuChoice @ 12d8:44e2 do Map_GenerateRandomOrLoad @ 1028:0000
 * param: 0 -> 0 (random), 2 -> 1 (earth-preset 0x89), 3 -> 0 (custom
 * ustawienia + random), inne -> -1 (bez mapy, orig zwraca 1 wczesniej).
 * No Win16, no globals. */
int worldgen_prepare_mode(int menu_choice);

/* --- Chain 2026-10-16 (24): PrepareEarth modes + Map params (goals 4/5) ---
 * Orig WorldGen_PrepareEarth @ 1108:01bc (civ.exe, x86:LE:16:Protected
 * Mode), decompiled this pass (pre-step w Main_RunStartupSequence @
 * 11e0:0000 loop#2 przed NewGame_Wizard @ 10e8:0000; Cursor_Set(0x84) +
 * Sound_PlayId @ 1150:00c9 (1) STOP + dispatch g_nMenuChoice @ 12d8:44e2,
 * patrz mainmenu.h/boot.h: 0 = Start New, 1 = Load, 2 = EARTH, 3 =
 * Customize; menu 4 = HoF, 5 = quit NIE docieraja tu (MainMenu_WndProc @
 * 11e0:0221 petla while choice<0||>3 + dispatch 1/3/4/5; 1/4/5 w PrepareEarth
 * wpadaja w "inne -> return 1 bez mapy"):
 *   0 -> settings preset 1 (4x s_38f1+3+i*2 = 1) + Map_GenerateRandomOrLoad(0)
 *     random (WorldGen_LoadEarth @ 1108:0243 dialog 0x8f NIE sunie tu; tryb
 *     custom z menu 3 tez laduje Map(0), ale ustawienia bierze z dialogu
 *     0x8f/0x323 4 rows x 3 cols + help col, patrz worldgen_dialog_id_to_cell
 *     LIVE + help texts Temperature @ 1420:2e28 / Climate @ 1420:2e34 /
 *     0x2e23 / 0x2e3c via FUN_10e0_0000).
 *   2 -> same settings preset 1 + Map_GenerateRandomOrLoad(1) earth-preset
 *     (frame 0x89 via Art_LoadFrameData @ 10b8:079d + HMEMCPY 64000).
 *   3 -> Map_GenerateRandomOrLoad(0) custom/random (bez presetu 1? orig
 *     spada do wspolnego uVar2 = 0 label bez petli presetu — settings
 *     zostaja z WorldGen_LoadEarth @ 1108:0243 dialogu 0x8f wywolanego wczesniej
 *     z MainMenu_WndProc @ 11e0:0221 choice 3; por. worldgen_load_earth STUB).
 *   inne (1/4/5) -> return 1 bez mapy.
 * Co robi Map(0) vs Map(1) (Map_GenerateRandomOrLoad @ 1028:0000, param u16
 *   w DAT_1308_0022; jedyny caller z wartoscia to PrepareEarth @ 1108:0211):
 *   0 = random gen (tile passes: FillRect 0x140x200 scratch + GenSeedOneIsland
 *     @ 1028:0945 loop + DrawRectBorder x2 + 6x-neighbour fixup + latitude/
 *     moisture passes + TryPlaceIsolatedTile @ 1028:05e7 scatter + GenRivers
 *     @ 1028:066a; Sound_PlayId(4) = OPENING.wav na starcie; seedy = Rng_Range
 *     @ 11c8:003d / Rng_NextRaw @ 1000:1729 w kazdym losowym wyborze, seed
 *     Rng_Init @ 11c8:0000 = Sys_GetTickScaled & 0x7fff; granice mapy
 *     0x50 (80) x 0x32 (50) tiles, interior 1..0x4e/1..0x30 + border FillRects;
 *     worldgen_generate_random STUB).
 *   1 (dowolne != 0) = earth-preset: Mem_DisposePtr + mem_alloc_lock 64000 +
 *     Art_LoadFrameData(0x89) + GLOBALLOCK -10 adjust (0x600 bajtow head) +
 *     HMEMCPY 64000 (WorldGen_CopyPreset @ 10b8:079d LIVE memcpy) ; potem
 *     wspolny ogon Map_FillRect(0xa0,0,0xa0,200,0) + Map_LabelContinents @
 *     1028:0a93 + DAT_1248_0000 = 1 (worldgen_load_preset STUB).
 * Skad preset Ziemi (RSC? kod? EARTH frame 0x89?): Art frame 0x89 (NIE plik
 *   RSC ani hardcoded tablica w kodzie; frame ladowany via Art_LoadFrameData
 *   @ 10b8:079d — frames ida z RSC via Art_LoadResPaletted @ 10b8:11bc lub z
 *   dysku via Art_LoadDiskFile @ 10b8:0aa8, patrz video.h art chain; bitmapa
 *   320x200 = 64000 B, por. CIV_MAP_PRESET_BYTES).
 * Menu g_nMenuChoice (0=new,1=load,2=EARTH,3=customize,4=HoF,5=quit): wartosci
 *   z Dialog_MenuZ_Modal @ 1220:0156 w MainMenu_WndProc @ 11e0:0221 nad tekstem
 *   1420:435b; dispatch 1 = Save_LoadDialog @ 1130:000e, 3 = WorldGen_LoadEarth
 *   (dialog 0x8f, zwraca != 0 -> re-prompt -1), 4 = HoF_Load + HoF_Show,
 *   5 = WinMain_Shutdown (patrz mainmenu.h). 0/2/3 wchodza w Startup loop#2
 *   (main_startup_should_run_wizard w boot.h).
 * g_year = 0xF060 (co to za wartosc?): u16 0xF060 = int16 -4000 = rok startowy
 *   4000 BC (ujemne = BC, dodatnie = AD; patrz newgame_start_year_* w
 *   newgame.h + game_append_year_text @ 1088:1dd0 w game.h).
 * OS-free mode/frame/size math ponizej LIVE 1:1 (no Win16, no globals);
 * Cursor/Sound/Map/file tails zostaja STUB. No exit(), C17. */
#define CIV_MAP_EARTH_FRAME 0x89u

/* Orig Map_GenerateRandomOrLoad @ 1028:0000 param gate -> worldgen_is_preset.
 * 1:1 param != 0 (orig if (DAT_1308_0022 == 0) random else preset; jedyne
 * wartosci z PrepareEarth to 0/1). */
int worldgen_is_preset(int map_param);

/* Orig earth-preset frame (Art_LoadFrameData(0x89) w Map @ 1028:0000 preset
 * path) -> worldgen_earth_frame. 1:1 0x89. */
int worldgen_earth_frame(void);

/* Orig preset size (HMEMCPY 64000 w Map preset path, WorldGen_CopyPreset @
 * 10b8:079d) -> worldgen_earth_preset_bytes. 1:1 CIV_MAP_PRESET_BYTES. */
unsigned worldgen_earth_preset_bytes(void);

/* Orig: FUN_10b8_079d @ 10b8:079d -> worldgen_copy_preset (WorldGen_CopyPreset).
 * HMEMCPY 64000B preset block; callee of Map_GenerateRandomOrLoad @ 1028:0000
 * param!=0 path. Segment-neighbour Art_MaybeConvertArtwork @ 10b8:0000
 * (1420:20a9 prompt; stub boot_maybe_convert_artwork in boot.h/boot.c,
 * owned by the parallel agent -- not redeclared here) gates artwork
 * conversion before this copy. OS-free memcpy wrapper -> ported 1:1. */
civ_error_t worldgen_copy_preset(uint8_t *dst, const uint8_t *src, size_t len);

/* Custom-world editor (wizard steps 3/4 + 1058 controls). */
civ_error_t worldgen_customize(void);

/* Orig: FUN_1060_xxxx @ 1060:xxxx -> worldgen_customize_dialog
 * (WorldGen_CustomizeDialog). Customize-World editor host using the Ctrl_*
 * @ 1058 controls (RANDOMRADIOPROC @ 1058:06ab, RANDOMSTATICPROC @ 1058:081a,
 * RANDOMUSERPROC @ 1058:0921, REGIONPROC @ 1058:09f6; classes registered by
 * Ctrl_RegisterCustomClasses @ 1058:0b4d). Stub: needs the modal
 * Dlg_CreateTracked @ 1098:124f host (+ Dlg_DestroyTracked @ 1098:1612
 * teardown) with the 1058 control fan-out. Control/dialog symbols are
 * referenced by Orig address only here (owned by the dialog module) to keep
 * this parallel slice disjoint -- dialog.h is intentionally not edited. */
civ_error_t worldgen_customize_dialog(void);

/* Gen passes (tile buffer via map.h map_get/set_tile + map_fill_rect,
 * RNG via sys.h rng_range, pacing via sys.h sys_yield; C17, no exit(),
 * no globals). Direction table below is static const (see worldgen.c). */

/* Orig: FUN_1028_0945 @ 1028:0945 -> worldgen_seed_one_island.
 * Scratch Map_FillRect(0xa0,0,0x50,0x32,0) clear + single
 * Map_GenPaintBlob(Rng_Range(0x48)+4, Rng_Range(0x22)+8) scatter +
 * island-copy math: scratch +0xa0 -> main with +1 (land increment),
 * Sys_Yield per x row. DAT_1248_0002 counter is an explicit required
 * out-param (NULL -> NOENT). Caller: worldgen_generate_random param==0
 * loop. LIVE 1:1 on the map buffer (no segment, no Win16, no exit()). */
civ_error_t worldgen_seed_one_island(int *count_out);

/* Orig: FUN_1028_09ef @ 1028:09ef -> worldgen_paint_blob.
 * Scratch-local (x,y) random walk, steps = Rng_Range(0x40)+1: each step
 * writes an L-tromino 0x0f at (x+0xa0,y)/(x+0xa1,y)/(x+0xa0,y+1), then
 * steps by the dedicated 4-dir tables (dx @ 12c8 odd words {0,+1,0,-1},
 * dy @ 12d0 odd words {-1,0,+1,0} = N/E/S/W; dir = Rng_Range(4)).
 * Stops when steps run out or (x,y) leaves (2,0x4d)x(2,0x2e).
 * OS-free 1:1 port (see worldgen.c). */
civ_error_t worldgen_paint_blob(int x, int y);

/* 8-dir step table for rivers (random-walk geometry).
 * Orig direction-table bytes TBD from decompile near 1028:066a; Linux
 * stub is 8-dir N-clockwise: N(0,-1) NE(1,-1) E(1,0) SE(1,1) S(0,1)
 * SW(-1,1) W(-1,0) NW(-1,-1). OS-free static const port (read-only,
 * no globals mutated, no exit()). Index 0..7 via Rng_Range(8).
 * NOTE: paint_blob above does NOT use this table (own 4-dir N/E/S/W). */
extern const int worldgen_dir_dx[8];
extern const int worldgen_dir_dy[8];
/* Accessors (bounds-checked: OOB dir -> 0 step). */
int worldgen_dir_dx_at(int dir);
int worldgen_dir_dy_at(int dir);

/* Orig: FUN_1028_0a93 @ 1028:0a93 -> worldgen_label_continents.
 * Connected-components labeling over 0x50 x 0x32 + rank sort: flood-merge
 * labels, size counts @ 0x4334, top-0xF rank sort (sort comparator: size
 * desc -- larger continents rank first, exact tiebreak TBD), calls
 * 1028:12c6/162c/0fb4 tail + border FillRects (Map_FillRect @ 1028:1d15
 * edges + Map_DrawRectBorder @ 1028:1ca7). Caller: generate_random tail.
 * Win16 label segments TBD -> stub CIV_ERR_UNSUPPORTED. */
civ_error_t worldgen_label_continents(void);

/* Orig: FUN_1028_066a @ 1028:066a -> worldgen_gen_rivers.
 * River random-walk: collect 0x0c tiles, directional walk with 8-dir table
 * (shared dx/dy above, Rng_Range picks), len > 4 required to commit;
 * save/restore island at +0xa0 via copy_rect @ 1028:06c1/0865/0908
 * (save @ 06c1, restore @ 0865, commit @ 0908). Caller: generate_random.
 * Walk decode pending -> stub CIV_ERR_UNSUPPORTED. */
civ_error_t worldgen_gen_rivers(void);

/* Orig: FUN_1028_1afd @ 1028:1afd -> worldgen_spread_pollution.
 * Fallout tiles from Game_DoPollutionAndWonderRace: level -> n tiles via
 * Rng_Range + Map_SetTile loop (n grows with level; random sites via
 * Rng_Range over 0x50 x 0x32, paint fallout tile via Map_SetTile).
 * Exact n formula + ocean-neighbour gate + 1110:1b36 tail TBD, so this
 * stays a stub CIV_ERR_UNSUPPORTED until the 1afd walk decodes. */
civ_error_t worldgen_spread_pollution(unsigned level);

/* --- Map_LabelContinents / Map_GenRivers tails (frontier 2026-10-15) ---
 * All four operate on the 64 KB tile buffer (see map.h) with
 * Rng_Range/Sys_Yield (see sys.h). Tile codes: 10 = ocean,
 * 1/2/0xb = scored land, 0xc = river seed, 0xf = border, 0 = cleared. */

/* Orig: FUN_1028_0fb4 @ 1028:0fb4 -> worldgen_score_city_sites.
 * Fertility pass: builds weights[0x18] from 1420:17d6*3+17d8 (+17d7*2
 * except cols 2/0xb, moisture penalties at 12b8/12bc), scans x 2..0x4d
 * y 2..0x2f for land 1/2/0xb, sums 0x15 neighbours (x2 for idx<9 and
 * idx==0, +2 for water-adjacent 2/0xb, +0xc river), dithers -0x10 on
 * (x*7+y*0xb&2), clamps ((sum-0x78)/8,1,0xf), writes Map_SetTile(x+0x50,
 * y,score) + rank layer (y+0x32) + hist 0x218a. Caller
 * Map_LabelContinents @ 1028:0f0c. Table addresses TBD -> stub
 * CIV_ERR_UNSUPPORTED. */
civ_error_t worldgen_score_city_sites(void);

/* Orig: FUN_1028_12c6 @ 1028:12c6 (yield) -> worldgen_build_continent_links.
 * 0x14 x 0x0d blocks of 4x4: finds an ocean (10) corner vs land corner
 * pair sharing the same continent id, checks FUN_1128_16d0 < 0x14, ORs
 * direction bits at +4 (+ reverse). Progress 1000:29ea (Util_Progress
 * over 0x1248 bar range) + Sys_Yield per row when yield!=0 (Sys_Yield @
 * 1098:0157 pumps Anim_PumpIfActive @ 10b0:0000). Caller
 * Map_LabelContinents. Segment walk TBD -> stub CIV_ERR_UNSUPPORTED. */
civ_error_t worldgen_build_continent_links(int yield);

/* Orig: FUN_1028_162c @ 1028:162c (yield) -> worldgen_build_ocean_links.
 * Same grid, 8-dir variant with a 5-step ocean-run check straight from
 * g_offMapBuf/g_segMapBuf (needs 2+ clear rows each step); sets bits at
 * +0x108 (+ reverse) with the (0xb,3) corner exception; edge fixups
 * (OR 0xe0/0x0e, AND masks). Progress 1000:29ea (0x108,...) + Sys_Yield
 * per row when yield!=0. Callers: Map_LabelContinents @ 1028:0f01,
 * save_deserialize @ 10f8:01e9. Segment walk TBD -> stub. */
civ_error_t worldgen_build_ocean_links(int yield);

/* Orig: FUN_1028_1dfb @ 1028:1dfb -> worldgen_copy_rect.
 * Block copy (src_x, src_y, w_bytes, h, dst_x, dst_y), dword fast path
 * (w>>2, remainder byte loop) with 0x50/0x140 stride wrap (game width
 * 0x50 inside 0x140 buffer stride). 3 callers in Map_GenRivers
 * (@ 1028:06c1/0865/0908: save/restore island at +0xa0, commit rivers).
 * OS-free memcpy loops port 1:1: row copy via map_get/set_tile (or
 * memmove on buffer) with bounds-check (OOB cells skipped, overlap-safe
 * per-row snapshot). NOTE: 1028:1dbd is not an entry -- it is the
 * byte-path loop inside Map_FillRect @ 1028:1d15 (see map.h). */
civ_error_t worldgen_copy_rect(int src_x, int src_y, unsigned w_bytes,
                                int h, int dst_x, int dst_y);

/* Orig: FUN_1128_16d0 @ 1128:16d0 -> Path_CheckContinentLink.
 * Gate inside worldgen_build_continent_links @ 1028:12c6: for an
 * ocean(10)/land corner pair sharing one continent id, returns a cost
 * that links only when < 0x14 (else the direction bits at +4 stay
 * clear). Params: tile position plus continent id under test. OS-free
 * grid walk over the tile buffer (no Win16); exact distance metric TBD,
 * see Ghidra. Stub returns 0x14 (no link). */
int path_check_continent_link(int x, int y, int continent);

/* --- Chain 2026-10-16 (25): Map tile-passes (task goal 2) ---
 * Orig Map_GenerateRandomOrLoad @ 1028:0000 (civ.exe, x86:LE:16:Protected
 * Mode), decompiled this pass (param u16 in DAT_1308_0022; jedyny caller z
 * wartoscia to WorldGen_PrepareEarth @ 1108:0211: 0 = random, 1 = preset):
 *   Cursor_Set + Sound_PlayId(4) = OPENING.wav (4 normalizes to 3 -> 0x83,
 *   see audio.h) + DAT_1248_0002 = 0 + DAT_1248_0000 = 0; param == 0 random
 *   else Art_LoadFrameData(0x89) + HMEMCPY 64000 (WorldGen_CopyPreset @
 *   10b8:079d LIVE memcpy); ogon Map_FillRect(0xa0,0,0xa0,200,0) +
 *   Map_LabelContinents @ 1028:0a93 + DAT_1248_0000 = 1.
 * Jakie passy (param == 0, w kolejnosci dekompilatu):
 *   1. Map_FillRect(0,0,0x140,200,0) scratch clear (0x140 stride wrap).
 *   2. Map_GenSeedOneIsland @ 1028:0945 loop: FillRect(0xa0 scratch) +
 *      Rng_Range(0x48)/Rng_Range(0x22) origin + Map_GenPaintBlob @ 1028:09ef
 *      (steps = Rng_Range(0x40)+1, paints 0x0f 2x2-ish via Map_SetTile at
 *      +0xa0, 4-dir step table iVar1 = Rng_Range(4)*2+1, bounds 3..0x4c /
 *      3..0x2d) + copy scratch +0xa0 -> main with +1 (land increment,
 *      counting DAT_1248_0002); cel = worldgen_island_target(land_param)
 *      = (land*8+0x10)*0x28 (land = DAT_12d8_38f4, settings preset 1 ->
 *      (8+0x10)*0x28 = 960 tiles); petla while target-count != 0 &&
 *      count <= target.
 *   3. Map_DrawRectBorder(0,0,0x4f,0x31) + (1,1,0x4d,0x2f) bordery 0x0f
 *      (granice mapy 0x50 x 0x32, interior 1..0x4e / 1..0x30 + border
 *      FillRects w LabelContinents ogonie: (0,0,0x50,1)/(0,0x31,0x50,1) 0x0f
 *      + 0x14 scatter 1x1 0x07 na gorze/dole).
 *   4. 6x-neighbour fixup (diagonal 0110/1001: 3 SetTile over the 2x2).
 *   5. Latitude/moisture pass: land 1 -> Rng_Range gate, u = 0x1d-(rng+y),
 *      band = ((u^sign)-sign)+(1-DAT_12d8_38f6), switch band/6 (desert/
 *      plains/forest/jungle/tundra kody 3/6/7/0xc/0xd/0xe — exact map TBD,
 *      switch szkielety w dekompilacie).
 *   6. Moisture sweep x2 (L->R i R->L): local_e budget vs (0xc-band)+
 *      DAT_12d8_38f8*4 / band/2+DAT_12d8_38f8, Rng_Range decay, ocean 10 ->
 *      SetTile commit, 0xd -3.
 *   7. Scatter (DAT_12d8_38fa+1)*800 prob: even = Rng_Range x/y, odd =
 *      neighbour step; 0xd -> Map_TryPlaceIsolatedTile @ 1028:05e7 (LIVE w
 *      map.c: 4 diagonale != 1 -> SetTile 1), 2/3/6/7/10/0xb/0xc/0xf/0xe ->
 *      SetTile commit.
 *   8. Map_GenRivers @ 1028:066a (renamed; byl Map_GenRivers juz): zbiera
 *      0x0c tiles (8000B x2 bufory), Rng_Range start + Rng_Range dir, walk
 *      az adjacent 0x01 / 0x09 / 0x0d / 0x01 lub len cap; len > 4 commit
 *      (worldgen_river_commit_len LIVE), banks 0x02 -> clear; save/restore
 *      via Map_CopyRect @ 1028:1dfb (was FUN_1028_1dfb, renamed this pass;
 *      save @ 06c1, restore @ 0865, commit @ 0908); count = river_target =
 *      DAT_12d8_38f4*2+DAT_12d8_38f8*2+6; abort po 0xff probach / 0x32 fails.
 *   9. Ogon: Map_FillRect(0xa0,0,0xa0,200,0) + Map_LabelContinents @
 *      1028:0a93 (renamed; connected-components 2 passy land/sea +
 *      flood-merge + size @ 0x4334 + top-0xF rank sort size desc +
 *      Map_ScoreCitySites @ 1028:0fb4 (was FUN_1028_0fb4, renamed this pass:
 *      weights[0x18] z 1420:17d6*3+17d8 (+17d7*2 oprocz col 2/0xb, moisture
 *      12b8/12bc), scan x 2..0x4d y 2..0x2f land 1/2/0xb, 0x15 sasiadow,
 *      dither -0x10 na (x*7+y*0xb&2), clamp ((sum-0x78)/8,1,0xf) via
 *      worldgen_score_clamp LIVE + warstwa score/2+8 via worldgen_score_layer
 *      + rank (y+0x32) + hist 0x218a) + Map_BuildContinentLinks @ 1028:12c6
 *      (was FUN_1028_12c6, renamed this pass; yield) +
 *      Map_BuildOceanLinks @ 1028:162c (was FUN_1028_162c, renamed this
 *      pass; yield) + border FillRects).
 *   Start positions? huts? resource? NIE w tym łańcuchu: start positions =
 *   NewGame_Wizard finalize (FUN_10e8_12b5 per-civ + Tech_HasTech @ 1170:3181
 *   + Rng_Range(0x32) drift, see newgame.h); huts/resource = tile codes
 *   0x02/0x0b scored land + river 0x0c w powyzszych passach (hut draw to
 *   Map_DrawCityLabels/city layer, nie gen). Seedy: Rng_Range @ 11c8:003d /
 *   Rng_NextRaw @ 1000:1729 w kazdym losowym wyborze (rng_range w sys.h
 *   LIVE); seed Rng_Init @ 11c8:0000 = Sys_GetTickScaled & 0x7fff.
 *   Zwiazek z Map_GenerateRandom STUB w worldgen.c: ten STUB to caly powyzszy
 *   pipeline param == 0 (worldgen_generate_random); ponizsze helpery to jego
 *   OS-free kawalki LIVE.
 * Art frame 0x89 (preset Ziemi 64000B HMEMCPY — wymiary? paleta? czy zwykly
 *   RSC dump?): bitmapa 320x200 = 64000 B (CIV_MAP_PRESET_BYTES; w =
 *   worldgen_earth_frame_w LIVE 320, h = worldgen_earth_frame_h LIVE 200);
 *   frame ladowany via Art_LoadFrameData @ 10b8:079d (NIE plik RSC ani
 *   hardcoded tablica: probuje Art_LoadDiskFile @ 10b8:0aa8 z dysku, else
 *   Art_LoadResPaletted @ 10b8:11bc z RSC z paleta; 0x600-bajtowy head adjust
 *   GLOBALLOCK -10 przed HMEMCPY; paleta via GfxPort_ApplyPalette; wymiary z
 *   frame header local_34/local_32; por. video.h art chain). FillRect+
 *   LabelContinents ogon Map(1): Map_FillRect(0xa0,0,0xa0,200,0) clear +
 *   Map_LabelContinents via Map_GetContinentId @ 11e8:0dcf/0e19 (LIVE w map.c:
 *   rank layer y+0x32; labelowanie kontynentow = flood-merge + rank sort).
 * OS-free reguly ponizej LIVE 1:1 (no Win16, no globals, read-only);
 * scratch/Win16 tails zostaja STUB. No exit(), C17. */
#define CIV_MAP_EARTH_W 320
#define CIV_MAP_EARTH_H 200

/* Orig Map(0) island target ((DAT_12d8_38f4*8+0x10)*0x28) ->
 * worldgen_island_target. 1:1 (land*8+0x10)*0x28. land < 0 -> -1. */
int worldgen_island_target(int land_param);

/* Orig same loop gate (target-count != 0 && count <= target) ->
 * worldgen_island_done. 1:1 nonzero (1) gdy done (count == target ||
 * count > target), 0 gdy kontynuowac. */
int worldgen_island_done(int count, int target);

/* Orig Map_GenPaintBlob @ 1028:09ef bounds -> worldgen_blob_in_bounds.
 * 1:1 3 <= x <= 0x4c && 3 <= y && y < 0x2e (orig: param_1 < 3 || 0x4c <
 * param_1 -> exit; param_2 < 3 -> exit; while param_2 < 0x2e). */
int worldgen_blob_in_bounds(int x, int y);

/* Orig Map_GenRivers @ 1028:066a commit gate (4 < len) ->
 * worldgen_river_commit_len. 1:1 len > 4. */
int worldgen_river_commit_len(int len);

/* Orig same river count (DAT_12d8_38f4*2+DAT_12d8_38f8*2+6) ->
 * worldgen_river_target. 1:1 land*2+moist*2+6. < 0 -> -1. */
int worldgen_river_target(int land_param, int moist_param);

/* Orig Map_SpreadPollution @ 1028:1afd gate ->
 * worldgen_pollution_should_paint. 1:1 ocean_neighbours < 7-level &&
 * ((x*0xb+y*0xd)&7) == (level&7) (decompiler: local_8 < 7-param_1 +
 * (iVar6*0xb+iVar7*0xd&7) == param_1; pb = tile|1 + 130e/1306 vs
 * 130b/1303 + Map_ClearOccupancyBits(6) + Map_RevealCenter tails STUB).
 * Zwraca 1 gdy malowac, 0 gdy nie. */
int worldgen_pollution_should_paint(int ocean_neighbours, int level, int x, int y);

/* Orig Map_ScoreCitySites @ 1028:0fb4 clamp (Util_Clamp((sum-0x78)/8,1,0xf))
 * -> worldgen_score_clamp. 1:1 clamp((sum-0x78)/8,1,0xf). */
int worldgen_score_clamp(int sum);

/* Orig same score layer (iVar5/2+8) -> worldgen_score_layer. 1:1
 * score/2+8 (C trunc). score 1..0xf -> 8..15. */
int worldgen_score_layer(int score);

/* Orig earth-preset wymiary (320x200 = 64000) -> helpers. 1:1. */
int worldgen_earth_frame_w(void);
int worldgen_earth_frame_h(void);

/* Orig Map_LabelContinents @ 1028:0a93 rank sort (size desc, wieksze first)
 * -> worldgen_continent_rank_cmp. 1:1 (b > a) - (b < a) (qsort comparator
 * shape; tiebreak TBD w dekompilacie — tu stabilny 0). */
int worldgen_continent_rank_cmp(int size_a, int size_b);

/* --- Chain 2026-10-16 (26): ScoreCitySites weights + link-graph kernels
 * (task goals 1/2) ---
 * Orig (civ.exe, x86:LE:16:Protected Mode), decompiled this pass:
 *   Map_ScoreCitySites @ 1028:0fb4 (sole caller Map_LabelContinents @
 *     1028:0a93 tail after BuildContinentLinks/BuildOceanLinks; callees
 *     Map_GetTile @ 11e8:0dcf / Map_SetTile @ 11e8:0e19 / Sys_Yield @
 *     1098:0157 / Util_Clamp @ 11c8:0063 / Map_CheckTileImproved @
 *     1110:1c72): weights[0x18] from 1420:17d6*3+17d8 (+17d7*2 except cols
 *     2/0xb, moisture penalties at 12b8/12bc), scan x 2..0x4d y 2..0x2f
 *     for land class 1/2/0xb (via *(tile*2) class table), 0x15 neighbours
 *     (x2 for idx<9, x2 again for idx==0, +2 water-adjacent 2/0xb when
 *     (x*7+y*0xb&2)==0, +0xc when improved), dither -0x10 when center
 *     class != 1 and (x*7+y*0xb&2)!=0, clamp ((sum-0x78)/8,1,0xf) via
 *     worldgen_score_clamp + layer score/2+8 via worldgen_score_layer,
 *     writes Map_SetTile(x+0x50,y,score) + rank layer (y+0x32) + hist
 *     0x218a. Kryteria: zasoby (wagi 17d6/17d8/17d7 = yield tables),
 *     woda (+2 adjacent ocean/river), defensywa (improved +0xc, dither
 *     noise), odleglosc BRAK (lokalne 0x15 okno, nie globalny dystans).
 *     Zwiazek z City_FindNearest @ 11c8:0124 (LIVE city_find_nearest w
 *     game.h: callerzy City_DestroyCity @ 1170:0241, FUN_10e0_005b,
 *     FUN_10e8_12b5 wizard per-civ starts, FUN_1160_25fa, FUN_11c0_2677,
 *     Game_SpawnUprisings, Unit_AIPlayerTurn, Unit_ChooseOrder,
 *     Unit_SpawnUnit): scoring buduje warstwe score/rank na mapie,
 *     FindNearest szuka najblizszego miasta po WrapStepDist (Map(0)
 *     random? starty wizarda = FUN_10e8_12b5 per-civ + Tech_HasTech @
 *     1170:3181 + Rng_Range(0x32) drift, nie ScoreCitySites; AI city
 *     placement czyta warstwe score + FindNearest do odleglosci).
 *   Map_BuildContinentLinks @ 1028:12c6 (sole caller LabelContinents;
 *     callees Map_GetTile/Sys_Yield/Util_MemSet/Path_CheckContinentLink @
 *     1128:16d0): 0x14 x 0x0d blocks of 4x4, ocean(10)/land corner pair on
 *     one continent id, gate cost < 0x14, OR dir bits at +4 (+ reverse):
 *     link graph = ladowy graf kontynentow (mosty ladowe 4-dir), czytany
 *     via Map_GetContinentId @ 1110:1d99 (callers City_ChooseProduction @
 *     1170:05ca, Unit_* AI, Trade_DeliverCaravanGoods @ 1038:0d09,
 *     Map_GetOceanOrContVal @ 1110:1dbd, Pic_PlayShared, Replay_RefreshMapTile,
 *     Game_SpawnUprisings) dla ruchu/handlu/AI (continent gates).
 *   Map_BuildOceanLinks @ 1028:162c (callers LabelContinents +
 *     save_deserialize @ 10f8:01e9; same + direct g_segMapBuf reads):
 *     same grid 8-dir + 5-step ocean-run (2+ clear rows/step), bits at
 *     +0x108 (+ reverse), (0xb,3) corner exception, edge OR/AND fixups:
 *     morski graf (wyspy/mosty morskie), czytany jw. dla sea ruchu.
 *   Map_CopyRect @ 1028:1dfb (callers Map_GenRivers @ 1028:066a
 *     save@06c1/restore@0865/commit@0908 island at +0xa0): dword fast path
 *     + 0x50/0x140 stride wrap (LIVE worldgen_copy_rect w worldgen.c).
 *   Map master Map_GenerateRandomOrLoad @ 1028:0000 order: FillRect
 *     scratch -> SeedOneIsland loop (island_target) -> DrawRectBorder x2
 *     -> 6x-neighbour fixup -> latitude/moisture -> scatter
 *     TryPlaceIsolatedTile -> GenRivers -> FillRect + LabelContinents
 *     (label 2-pass land/sea + rank sort + Score + Links + border FillRects).
 * OS-free kernels below LIVE 1:1 (no Win16, no globals, read-only);
 * scratch/Win16 tails stay STUB. No exit(), C17. */
#define CIV_SCORE_WEIGHT_N 0x18
#define CIV_LINK_GRID_W 0x14
#define CIV_LINK_GRID_H 0x0d
#define CIV_LINK_COST_MAX 0x14

/* Orig Score weights local_42[i] -> worldgen_score_weight. 1:1
 * w = v17d6*3+v17d8 (+ v17d7*2 except col 2/0xb) + moisture: moist_b<0 ?
 * (-1-moist_b) : (moist_a<0 ? (-1-moist_a)*2 : 0). Params are the signed
 * bytes/words at 1420:17d6/17d8/17d7 + 12b8/12bc caller-side. No globals. */
int worldgen_score_weight(int v17d6, int v17d8, int v17d7, int col,
                          int moist_a, int moist_b);

/* Orig water-adjacent +2 (tile 2/0xb, (x*7+y*0xb&2)==0) ->
 * worldgen_score_water_bonus. 1:1 2 else 0. No globals. */
int worldgen_score_water_bonus(int tile_class, int x, int y);

/* Orig center dither -0x10 (center != 1, (x*7+y*0xb&2)!=0) ->
 * worldgen_score_dither. 1:1 -0x10 else 0. No globals. */
int worldgen_score_dither(int center_class, int x, int y);

/* Orig neighbour accum (c = w+bonus; idx<9 ? *2; idx==0 ? *2 again) ->
 * worldgen_score_accum. 1:1 acc + stepped c. No globals. */
int worldgen_score_accum(int acc, int weight, int idx, int bonus);

/* Orig link grids (0x14 x 0x0d, 0x104 B at 1248:0004 / 1248:0108) ->
 * grid sizes. 1:1. No globals. */
int worldgen_link_grid_w(void);
int worldgen_link_grid_h(void);

/* Orig link cell index (bx*0xd+by, +4 / +0x108 base caller-side) ->
 * worldgen_link_idx. 1:1 bx*0x0d+by, -1 on OOR 0..0x13/0..0x0c. */
int worldgen_link_idx(int bx, int by);

/* Orig dir-bit OR (1 << ((dir-1)&0x1f), dir 1..8) -> worldgen_link_bit.
 * 1:1 1<<(dir-1), 0 on dir OOR. No globals. */
int worldgen_link_bit(int dir);

/* Orig reverse-bit OR (1 << ((dir+3)&7)) -> worldgen_link_rev_bit.
 * 1:1, 0 on dir OOR 1..8. No globals. */
int worldgen_link_rev_bit(int dir);

/* Orig continent gate (cost != -1 && cost < 0x14) ->
 * worldgen_continent_link_allowed. 1:1 1 = link, 0 = no link. */
int worldgen_continent_link_allowed(int cost);

/* Orig ocean 5-step per-step gate (2+ clear rows of 4) ->
 * worldgen_ocean_run_ok. 1:1 clear_rows >= 2. No globals. */
int worldgen_ocean_run_ok(int clear_rows);

/* Orig Label pass gate ((tile != 10) == (pass == 0)) ->
 * worldgen_label_is_land_pass. 1:1 nonzero when tile belongs to this
 * pass (pass 0 = land, pass 1 = sea). tile_is_ocean = (tile == 10). */
int worldgen_label_is_land_pass(int pass, int tile_is_ocean);

/* Orig WorldGen_ProbeCustomFont @ 1060:0000 GDI probe
 * (CREATEFONT h=0x12 weight 400 + SELECTOBJECT + DELETEOBJECT, caller
 * WorldGen_ShowVideoCapsDialog @ 1060:01c1 GETDEVICECAPS dump + Dlg 0x94)
 * -> height/weight helpers. 1:1. No Win16, no globals. */
int worldgen_probe_font_height(void);
int worldgen_probe_font_weight(void);

/* Orig same GDI tail -> worldgen_probe_custom_font. CREATEFONT/
 * SELECTOBJECT/DELETEOBJECT -> SDL_ttf TBD; stub CIV_ERR_UNSUPPORTED.
 * No globals, no exit(). */
civ_error_t worldgen_probe_custom_font(void);

/* --- Chain 2026-10-16 (27): Scatter + SeedOneIsland-loop + VideoCaps +
 * link-gate answers (task goals 1-5; civ.exe, x86:LE:16:Protected Mode) ---
 * Orig (decompiled this pass):
 *   Map_GenerateRandomOrLoad @ 1028:0000 scatter (miedzy latitude/moisture a
 *     Map_GenRivers @ 1028:066a): for (local_6 = 0; (38fa+1)*800 - local_6
 *     != 0 && local_6 <= (38fa+1)*800; local_6++): Sys_Yield per iter; even
 *     (local_6&1)==0: iVar3 = Rng_Range @ 11c8:003d (x), local_8 = Rng_Range
 *     (y); odd: iVar4 = Rng_Range; step = *(iVar4+1 *2) shared dir table
 *     (4-dir? orig *(local_10*2) both axes, same shape as PaintBlob drift);
 *     tile = Map_GetTile @ 11e8:0dcf; switch: 0xd -> Map_TryPlaceIsolatedTile
 *     @ 1028:05e7 (4 diagonale != 1 -> SetTile 1, LIVE w map.c); 2/3/6/7/10/
 *     0xb/0xc/0xf/0xe -> Map_SetTile commit; default -> skip (no write).
 *     Ile prob: (scatter_param+1)*800 (presety 0..?; 1 -> 1600). Odrzucenia:
 *     default-tiles (0/1/4/5/8/9/0xa/inne) skipuja SetTile; 0xd z sasiadem 1
 *     zwraca 0 bez malowania. Izolacja = 4 DIAGONALE != 1 (ortogonale
 *     ignorowane; NIE woda wokol — woda to tile 10, tu tylko gate != 1).
 *     Zwiazek z Map_GenPaintBlob @ 1028:09ef (steps = Rng_Range(0x40)+1,
 *     paints 0x0f 2x2-ish via +0xa0, 4-dir step Rng_Range(4)*2+1, bounds
 *     3..0x4c/3..0x2e via worldgen_blob_in_bounds LIVE): PaintBlob robi
 *     wyspy w SeedOneIsland-loop, scatter robi teksture/uzupelnienia PO
 *     latitude/moisture (inne zrodlo losowosci, inny cel: Blob = masa ladu
 *     do island_target, scatter = rozrzut typow terenu).
 *   Map_GenSeedOneIsland @ 1028:0945: FillRect(0xa0 scratch) + Rng_Range(0x48)
 *     /Rng_Range(0x22) origin + Map_GenPaintBlob(origin+4,+8) single (loop
 *     local_6 < 1 = raz) + copy scratch +0xa0 -> main with +1 (land incr,
 *     counting DAT_1248_0002). Petla wokol (Map @ 1028:0000): do {
 *     SeedOneIsland; Sys_Yield; target = (38f4*8+0x10)*0x28 } while
 *     (target-count != 0 && count <= target) = worldgen_island_target/done
 *     LIVE (960 przy preset 1). Warunek konca: count == target (exact) lub
 *     count > target (overshoot); BRAK timeoutu (nie jak rivers 0xff/0x32)
 *     ani RNG w warunku (Rng tylko w SeedOneIsland wnetrzu). Timeout TBD?
 *     nie — dekompilat nie ma licznika prob, petla zawsze sie konczy bo
 *     SeedOneIsland doklada srednio > 0 (PaintBlob steps >= 1).
 *   Map_GenPaintBlob callerzy: TYLKO Map_GenSeedOneIsland @ 1028:098c (NE
 *     xref scan; rivers @ 1028:066a i pollution @ 1028:1afd maja WLASNE
 *     walki: rivers drift (local_c+iVar6*2+(local_10&1)*-2)&7 + len>4 commit,
 *     pollution 8-neighbour ocean count + (x*0xb+y*0xd&7)==level gate —
 *     nie wolaja PaintBlob).
 *   Map_ScoreCitySites @ 1028:0fb4 jedyny caller Map_LabelContinents @
 *     1028:0f0c (NE xref scan): Score NIE wplywa na rank-sort ani border —
 *     rank sort + rank layer (y+0x32) + border FillRects ida PRZED/OBOK Score
 *     w LabelContinents; Score dopisuje warstwy score (x+0x50) + rank-copy
 *     (y+0x32 overwrite) + hist 0x218a dla AI city placement (czytane via
 *     City_FindNearest @ 11c8:0124 dystans + score warstwa), nie dla grafu.
 *   Path_CheckContinentLink @ 1128:16d0 gate < 0x14: limit = BUDZET korytarza
 *     Path_FindCorridor @ 1128:10de (param_3 = 0x14 = 20 krokow/kosztu;
 *     BFS cost w DAT_1420_aea8, start 68f1/68f2, cel 1338:0002/0000, okno
 *     +-8 tiles, sea/land match + occupancy gates). Link laduje gdy cost !=
 *     -1 (osiagalny) && cost < 0x14 (<= 19). 0x14 to rozmiar/budzet grafu,
 *     nie liczba kontynentow (te do 0x40 w rank sort). LIVE w
 *     worldgen_continent_link_allowed (cost != -1 && < 0x14).
 *   WorldGen_ShowVideoCapsDialog @ 1060:01c1: GETDC + GETDEVICECAPS x13
 *     (8,10,0xc,0xe,0x16,0x18,0x22,0x24,0x26,0x28,0x2a,0x58,0x5a + war. 0x68/
 *     0x6a gdy 0x26&0x100) + RELEASEDC + Dlg_CreateTracked(0x94,0x41) +
 *     FILLRECT + Text_SetColor + SETRECT + DispatchFont(5/6) + ProbeCustomFont
 *     (0x12) + IntToDec/StrCopy/Append rows via WorldGen_DrawVideoCapsRow @
 *     1060:005b (was FUN_1060_005b, renamed this pass: label+value 2-col) +
 *     WorldGen_DrawVideoCapsFlag @ 1060:00d1 (was FUN_1060_00d1, renamed:
 *     flag row enabled/disabled) + Sys_WaitForInput @ 1038:0c1d modal +
 *     DestroyTracked. Po co: DEBUG/requirements dump (caps + raster flags
 *     0x26 + clip caps 0x22), nie wybor trybu video (brak SET trybu, tylko
 *     odczyt + pokaz). Kiedy: BRAK callerow w NE xref scan (martwy/debug;
 *     Sys_WaitForInput lista 21 callerow wymienia FUN_1060_01c1 jako modal,
 *     ale zaden kod go nie wola — wywolanie reczne/F-key TBD, nie boot ani
 *     blad). Zwiazek z boot_video_caps (checklist 1420:0af4 dla
 *     Video_CheckDriverCaps @ 1008:059a, LIVE metryki count/describe = 0/NULL
 *     do dumpu) i Sys_CheckFreeMemory @ 11e8:0000 (GETFREESPACE < 2500000 /
 *     resources < 0x23 -> MESSAGEBOX 43b8 + exit; STUB) / Sys_CheckRuntimeMemory
 *     @ 11e8:0075 (caller Game_TurnLoopTick @ 1088:0000: <1000000/<10 once +
 *     <300000/<8 once; STUB): caps-dialog to ODCZYT uurzedzenia, memory to
 *     PROGI ilościowe (logika progow LIVE w boot.c, dialog STUB tutaj).
 * OS-free reguly ponizej LIVE 1:1 (no Win16, no globals, read-only);
 * GDI/dialog tails STUB. No exit(), C17. */
#define CIV_SCATTER_STEP_MUL 800
#define CIV_SCATTER_TRY_TILE 0x0d
#define CIV_VIDEO_CAPS_QUERY_N 13

/* Orig scatter count ((38fa+1)*800) -> worldgen_scatter_count. 1:1
 * (scatter_param+1)*800. -1 gdy scatter_param < 0. */
int worldgen_scatter_count(int scatter_param);

/* Orig scatter parity ((local_6&1)==0 resample) -> worldgen_scatter_is_resample.
 * 1:1 1 gdy step parzysty (Rng_Range x/y), 0 gdy nieparzysty (neighbour step). */
int worldgen_scatter_is_resample(unsigned step);

/* Orig scatter tile dispatch -> worldgen_scatter_tile_action. 1:1: 0xd -> 2
 * (Map_TryPlaceIsolatedTile @ 1028:05e7), 2/3/6/7/10/0xb/0xc/0xf/0xe -> 1
 * (Map_SetTile commit), inne -> 0 (skip, brak write). */
int worldgen_scatter_tile_action(int tile);

/* Orig SeedOneIsland origin (+4/+8) -> helpers. 1:1 raw+4 / raw+8
 * (raw to Rng_Range(0x48)/Rng_Range(0x22) 0..71/0..33; +4/+8 trzyma w
 * blob bounds 3..0x4c/3..0x2e). */
int worldgen_seed_origin_x(int raw);
int worldgen_seed_origin_y(int raw);

/* Orig VideoCaps GETDEVICECAPS query set -> count + index. 1:1 13 bazowych
 * (8,10,0xc,0xe,0x16,0x18,0x22,0x24,0x26,0x28,0x2a,0x58,0x5a) + war. 0x68/0x6a
 * gdy raster caps 0x26&0x100 (caller-side, STUB). OOR -> -1. */
int worldgen_video_caps_query_count(void);
int worldgen_video_caps_query_at(int index);

/* Orig VideoCaps raster gate (0x26&0x100 -> 0x68/0x6a) ->
 * worldgen_video_caps_needs_raster_ext. 1:1 nonzero (1) gdy (caps26&0x100). */
int worldgen_video_caps_needs_raster_ext(int caps26);

/* Orig WorldGen_ShowVideoCapsDialog @ 1060:01c1 GDI tail (GETDC/GETDEVICECAPS
 * dump + Dlg_CreateTracked 0x94 + ProbeCustomFont + WaitForInput) -> STUB.
 * Progi pamieci LIVE w boot.h (boot_free_memory_is_low + boot_runtime_*).
 * No globals, no exit(). */
civ_error_t worldgen_show_video_caps_dialog(void);

#endif /* CIVLIN_WORLDGEN_H */
