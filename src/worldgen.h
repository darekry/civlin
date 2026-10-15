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

/* Random-map pipeline (Map_GenerateRandomOrLoad param==0 path). */
civ_error_t worldgen_generate_random(void);

/* Preset load (param!=0 path: 64000B image + 1028:1d15/0a93 tail).
 * First step is WorldGen_CopyPreset @ 10b8:079d below. */
civ_error_t worldgen_load_preset(const uint8_t *image, size_t len);

/* Orig: FUN_1108_0243 @ 1108:0243 -> worldgen_load_earth (WorldGen_LoadEarth).
 * Play on EARTH (MainMenu choice 3 via MainMenu_WndProc @ 11e0:0221).
 * Preset path: 64000B image via WorldGen_CopyPreset @ 10b8:079d, then tail
 * Map_FillRect @ 1028:1d15 / Map_LabelContinents @ 1028:0a93.
 * Params: orig takes no caller buffer (preset source TBD -- RSC entry or
 * file image, NOT a caller-supplied pointer); takes void here so
 * mainmenu_dispatch can call it directly. Win16 file/resource load ->
 * stub CIV_ERR_UNSUPPORTED. */
civ_error_t worldgen_load_earth(void);

/* Orig: FUN_1108_01bc @ 1108:01bc -> worldgen_prepare_earth
 * (WorldGen_PrepareEarth). Adjacent routine immediately before
 * WorldGen_LoadEarth @ 1108:0243 in the same 1108 segment; exact role TBD
 * from decompile near 1108:0243 (presumed pre/post step around the EARTH
 * load -- setup, prompt, or buffer init). Win16 -> stub. */
civ_error_t worldgen_prepare_earth(void);

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

/* Gen passes (stubs; tile buffer via map.h, RNG via sys.h). */
civ_error_t worldgen_seed_one_island(void);
civ_error_t worldgen_paint_blob(int x, int y);
civ_error_t worldgen_label_continents(void);
civ_error_t worldgen_gen_rivers(void);
civ_error_t worldgen_spread_pollution(unsigned level);

/* --- Map_LabelContinents / Map_GenRivers tails (frontier 2026-10-15) ---
 * All four operate on the 64 KB tile buffer (see map.h) with
 * Rng_Range/Sys_Yield (see sys.h). Tile codes: 10 = ocean,
 * 1/2/0xb = scored land, 0xc = river seed, 0xf = border, 0 = cleared. */

/* Orig: FUN_1028_0fb4 @ 1028:0fb4 -> worldgen_score_city_sites.
 * Fertility pass: builds weights[0x18] from 1420:17d6*3+17d8
 * (+17d7*2 except cols 2/0xb, moisture penalties at 12b8/12bc), scans
 * x 2..0x4d y 2..0x2f for land 1/2/0xb, sums 0x15 neighbours (x2 for
 * idx<9 and idx==0, +2 for water-adjacent 2/0xb, +0xc river), dithers
 * -0x10 on (x*7+y*0xb&2), clamps ((sum-0x78)/8,1,0xf), writes
 * Map_SetTile(x+0x50,y,score) + rank layer (y+0x32) + histogram at
 * 0x218a. Caller Map_LabelContinents @ 1028:0f0c. */
civ_error_t worldgen_score_city_sites(void);

/* Orig: FUN_1028_12c6 @ 1028:12c6 (yield) -> worldgen_build_continent_links.
 * 0x14 x 0x0d blocks of 4x4: finds an ocean (10) corner vs land corner
 * pair sharing the same continent id, checks FUN_1128_16d0 < 0x14, ORs
 * direction bits at +4 (+ reverse). Progress 1000:29ea(4,0x1248...),
 * Sys_Yield per row when yield!=0. Caller Map_LabelContinents. */
civ_error_t worldgen_build_continent_links(int yield);

/* Orig: FUN_1028_162c @ 1028:162c (yield) -> worldgen_build_ocean_links.
 * Same grid, 8-dir variant with a 5-step ocean-run check straight from
 * g_offMapBuf/g_segMapBuf (needs 2+ clear rows each step); sets bits at
 * +0x108 (+ reverse) with the (0xb,3) corner exception; edge fixups
 * (OR 0xe0/0x0e, AND masks). Progress 1000:29ea(0x108,...). Callers:
 * Map_LabelContinents @ 1028:0f01, save_deserialize @ 10f8:01e9. */
civ_error_t worldgen_build_ocean_links(int yield);

/* Orig: FUN_1028_1dfb @ 1028:1dfb -> worldgen_copy_rect.
 * Block copy (src_x, src_y, w_bytes, h, dst_x, dst_y), dword fast path
 * (w>>2) with 0x50/0x140 stride wrap. 3 callers in Map_GenRivers
 * (@ 1028:06c1/0865/0908: save/restore island at +0xa0, commit rivers).
 * NOTE: 1028:1dbd is not an entry -- it is the byte-path loop inside
 * Map_FillRect @ 1028:1d15 (see map.h). */
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

#endif /* CIVLIN_WORLDGEN_H */
