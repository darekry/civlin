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
 *   FUN_1108_0243 @ 1108:0243 (Play on EARTH map load; from MainMenu dispatch;
 *     still FUN_*, next RE frontier with 1108:01bc neighbour)
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

/* Preset load (param!=0 path: 64000B image + 1028:1d15/0a93 tail). */
civ_error_t worldgen_load_preset(const uint8_t *image, size_t len);

/* Play on EARTH (FUN_1108_0243). */
civ_error_t worldgen_load_earth(void);

/* Custom-world editor (wizard steps 3/4 + 1058 controls). */
civ_error_t worldgen_customize(void);

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

#endif /* CIVLIN_WORLDGEN_H */
