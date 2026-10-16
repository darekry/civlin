/* Worldgen pipeline detail (U slice, 1028 segment).
 * C17, no exit(), no globals (direction tables below are static const
 * read-only); tile I/O via map.h map_get/set_tile, RNG via sys.h
 * rng_range, pacing via sys.h sys_yield. */
#include "worldgen.h"

#include <string.h>

#include "map.h"
#include "sys.h"

/* 8-dir step table for paint_blob + rivers (Orig direction-table bytes TBD
 * near 1028:09ef/066a). Linux stub: 8-dir N-clockwise, shared by
 * worldgen_paint_blob (dir = rng_range(8)) and the river walk. Static
 * const port (read-only). */
const int worldgen_dir_dx[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
const int worldgen_dir_dy[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };

int worldgen_dir_dx_at(int dir)
{
    /* Bounds-checked accessor for the static const dx table above. */
    if (dir < 0 || dir > 7)
        return 0;
    return worldgen_dir_dx[dir];
}

int worldgen_dir_dy_at(int dir)
{
    /* Bounds-checked accessor for the static const dy table above. */
    if (dir < 0 || dir > 7)
        return 0;
    return worldgen_dir_dy[dir];
}

civ_error_t worldgen_generate_random(void)
{
    /* TODO: Map_GenerateRandomOrLoad @ 1028:0000 param==0 path, ordered
     * pipeline: Sound_PlayId(4)=OPENING at start, then seed_one_island ->
     * paint_blob loop (Rng_Range @ 11c8:003d sized scatter) ->
     * label_continents (@ 1028:0a93) -> gen_rivers (@ 1028:066a) ->
     * score_city_sites (@ 1028:0fb4) -> build_continent/ocean_links (@
     * 1028:12c6/162c), with Sys_Yield @ 1098:0157 per row + Rng_Range for
     * every random pick and progress 1000:29ea. Callers outward: NewGame
     * random path; scratch/Win16 tail TBD -> stub. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_load_preset(const uint8_t *image, size_t len)
{
    (void)image;
    (void)len;
    /* TODO: Map_GenerateRandomOrLoad param!=0: WorldGen_CopyPreset @ 10b8:079d
     * (HMEMCPY 64000B, see worldgen_copy_preset) + tail Map_FillRect @
     * 1028:1d15 / Map_LabelContinents @ 1028:0a93. Map buffer owned via
     * map.h (parallel agent), so the copy target is not wired yet. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_load_earth(void)
{
    /* TODO: Orig WorldGen_LoadEarth @ 1108:0243 (choice 3 Play on EARTH
     * from MainMenu_WndProc @ 11e0:0221; dialog 0x8f with 4 rows x 3
     * cols + help col, IDs from 0x323, help texts Temperature @
     * 1420:2e28 / Climate @ 1420:2e34 / 0x2e23 / 0x2e3c via NewGame
     * customize @ 10e0:0000, item rects via Dialog_GetItemRect @
     * 1100:03ff, frames via Art_LoadFrame @ 10b8:0e00). Preset 64000B via
     * WorldGen_CopyPreset @ 10b8:079d + tail 1028:1d15/0a93. Preset image
     * source TBD (RSC entry vs file read); takes no caller buffer. Win16
     * resource/file load -> stub. The OS-free ID->cell math lives in
     * worldgen_dialog_id_to_cell / worldgen_custom_apply below. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_dialog_id_to_cell(int dialog_id, int *row_out,
                                       int *col_out)
{
    /* Orig WorldGen_LoadEarth @ 1108:0243 ID math: row = (id - 0x323) / 4,
     * col = (id - 0x323) % 4 (4 rows: landmass/temperature/climate/age
     * hypothesis; 3 choice cols + col 3 = help via 10e0:0000). Caller
     * passes DAT_1328_0004 (dialog result ID); 0x65/0x66 are OK/Cancel
     * (handled by the dialog pump, not here). NOENT on NULL outs, RANGE
     * when id < 0x323 or row outside 0..3. No Win16, no globals. */
    int rel = 0;

    if (!row_out || !col_out)
        return CIV_ERR_NOENT;
    if (dialog_id < 0x323)
        return CIV_ERR_RANGE;
    rel = dialog_id - 0x323;
    *row_out = rel / 4;
    *col_out = rel % 4;
    if (*row_out < 0 || *row_out > 3 || *col_out < 0 || *col_out > 3)
        return CIV_ERR_RANGE;
    return CIV_OK;
}

civ_error_t worldgen_custom_apply(int *settings, int row, int col)
{
    /* Orig WorldGen_LoadEarth @ 1108:0243 choice write: when col < 3 the
     * dialog stores col into settings[row] (int array at
     * s_found_the_city_of_1420_38f1 + 3, 4 entries x 2 B, preset to 1);
     * col == 3 is the help column (Temperature/Climate/0x2e23/0x2e3c via
     * NewGame customize @ 10e0:0000 + Dialog_GetItemRect @ 1100:03ff) and
     * leaves settings untouched. NOENT on NULL settings, RANGE on
     * row/col outside 0..3, else CIV_OK. No Win16, no globals. */
    if (!settings)
        return CIV_ERR_NOENT;
    if (row < 0 || row > 3 || col < 0 || col > 3)
        return CIV_ERR_RANGE;
    if (col < 3)
        settings[row] = col;
    return CIV_OK;
}

civ_error_t worldgen_prepare_earth(void)
{
    /* TODO: Orig WorldGen_PrepareEarth @ 1108:01bc (pre-step w
     * Main_RunStartupSequence @ 11e0:0000 loop#2: Cursor_Set(0x84) +
     * Sound_PlayId @ 1150:00c9 (1) STOP + dispatch g_nMenuChoice @
     * 12d8:44e2 przez worldgen_prepare_mode ponizej do
     * Map_GenerateRandomOrLoad @ 1028:0000 (0 = random, 1 = earth-preset
     * 0x89; ustawienia custom z WorldGen_LoadEarth @ 1108:0243 dialog 0x8f
     * + worldgen_dialog_id_to_cell LIVE). Cursor/Sound/Map tails Win16 ->
     * SDL TBD. */
    return CIV_ERR_UNSUPPORTED;
}

int worldgen_prepare_mode(int menu_choice)
{
    /* Orig WorldGen_PrepareEarth @ 1108:01bc dispatch: 0 -> Map(0), 2 ->
     * Map(1), 3 -> Map(0), else brak mapy (-1). */
    if (menu_choice == 0)
        return 0;
    if (menu_choice == 2)
        return 1;
    if (menu_choice == 3)
        return 0;
    return -1;
}

/* --- Chain 2026-10-16 (24): PrepareEarth modes + Map params (goals 4/5). ---
 * See worldgen.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()).
 * Cursor_Set(0x84)/Sound_PlayId(1) STOP/Map_GenerateRandomOrLoad @ 1028:0000
 * tails stay in worldgen_prepare_earth STUB above. */

int worldgen_is_preset(int map_param)
{
    /* Orig Map_GenerateRandomOrLoad @ 1028:0000: if (DAT_1308_0022 == 0)
     * random-tile passes else Art_LoadFrameData(0x89) + HMEMCPY 64000. */
    return (map_param != 0) ? 1 : 0;
}

int worldgen_earth_frame(void)
{
    /* Orig same preset path: Art_LoadFrameData(0x89,0,local_52) (frame 0x89 =
     * EARTH bitmap 320x200; RSC via Art_LoadResPaletted @ 10b8:11bc lub disk
     * via Art_LoadDiskFile @ 10b8:0aa8, see video.h art chain). */
    return (int)CIV_MAP_EARTH_FRAME;
}

unsigned worldgen_earth_preset_bytes(void)
{
    /* Orig same: HMEMCPY(...,64000,...) via WorldGen_CopyPreset @ 10b8:079d
     * (worldgen_copy_preset LIVE memcpyugged above). */
    return CIV_MAP_PRESET_BYTES;
}

civ_error_t worldgen_copy_preset(uint8_t *dst, const uint8_t *src, size_t len)
{
    /* Orig FUN_10b8_079d @ 10b8:079d (WorldGen_CopyPreset): HMEMCPY 64000B
     * preset, callee of Map_GenerateRandomOrLoad @ 1028:0000 param!=0 path.
     * OS-free -> ported 1:1. Segment-neighbour Art_MaybeConvertArtwork @
     * 10b8:0000 (1420:20a9 prompt; boot_maybe_convert_artwork, see boot.h)
     * gates artwork conversion before this copy. */
    if (!dst || !src)
        return CIV_ERR_NOENT;
    if (len != CIV_MAP_PRESET_BYTES)
        return CIV_ERR_RANGE;
    memcpy(dst, src, len);
    return CIV_OK;
}

civ_error_t worldgen_customize(void)
{
    /* TODO: wizard steps 3/4 + Ctrl_* @ 1058:xxxx. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_customize_dialog(void)
{
    /* TODO: Orig FUN_1060_xxxx @ 1060:xxxx (WorldGen_CustomizeDialog):
     * Customize-World editor host. Needs the modal Dlg_CreateTracked @
     * 1098:124f host (+ Dlg_DestroyTracked @ 1098:1612 teardown) driving
     * the Ctrl_* @ 1058 controls (RANDOMRADIOPROC @ 1058:06ab,
     * RANDOMSTATICPROC @ 1058:081a, RANDOMUSERPROC @ 1058:0921,
     * REGIONPROC @ 1058:09f6; cf. dialog module, not redeclared here to
     * keep parallel slices disjoint). Win16 USER/GDI modal -> SDL TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_seed_one_island(void)
{
    /* TODO: Map_GenSeedOneIsland @ 1028:0945: scratch Map_FillRect clear
     * + Map_GenPaintBlob scatter + island-copy math (copy scratch +0xa0
     * -> main with +1 land increment, counting via 1248:0002). Caller:
     * worldgen_generate_random param==0 loop. Scratch segment TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_paint_blob(int x, int y)
{
    /* Orig FUN_1028_09ef @ 1028:09ef -> Map_GenPaintBlob: steps =
     * Rng_Range(0x40)+1 random walk painting 0x0f, stepping by the 8-dir
     * dx/dy table (dir = Rng_Range(8)). Direction-table bytes TBD; Linux
     * uses the static const 8-dir stub above. OS-free 1:1 port via
     * map_set_tile + rng_range; OOB steps fall out via map_set_tile. */
    unsigned steps = rng_range(0x40) + 1u;
    unsigned i;

    for (i = 0; i < steps; i++) {
        int dir;

        map_set_tile(x, y, 0x0fu);
        dir = (int)rng_range(8);
        x += worldgen_dir_dx_at(dir);
        y += worldgen_dir_dy_at(dir);
    }
    return CIV_OK;
}

civ_error_t worldgen_label_continents(void)
{
    /* TODO: Map_LabelContinents @ 1028:0a93: connected-components
     * labeling over 0x50 x 0x32 + rank sort (flood-merge labels, size
     * counts @ 0x4334, top-0xF rank sort with sort comparator size desc
     * -- larger first, tiebreak TBD; calls 1028:12c6/162c/0fb4 tail +
     * border FillRects via Map_FillRect @ 1028:1d15 /
     * Map_DrawRectBorder @ 1028:1ca7). Caller: generate_random tail. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_gen_rivers(void)
{
    /* TODO: Map_GenRivers @ 1028:066a: river random-walk (collect 0x0c
     * tiles, directional walk on the 8-dir dx/dy table via Rng_Range,
     * len > 4 required to commit) + save/restore island at +0xa0 via
     * copy_rect @ 1028:06c1 (save) / 0865 (restore) / 0908 (commit).
     * Walk metric TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_spread_pollution(unsigned level)
{
    (void)level;
    /* TODO: Map_SpreadPollution @ 1028:1afd: fallout tiles from
     * Game_DoPollutionAndWonderRace. Intended OS-free loop is level -> n
     * tiles via Rng_Range + Map_SetTile over 0x50 x 0x32, but the exact n
     * formula + ocean-neighbour count vs 7 - level gate + paint
     * 0x130e/0x1306 or 0x130b/0x1303 + 1110:1b36 tail are still TBD from
     * decompile, so this stays a stub (CIV_ERR_UNSUPPORTED). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_score_city_sites(void)
{
    /* TODO: FUN_1028_0fb4 @ 1028:0fb4: weights[0x18] from
     * 1420:17d6*3+17d8 (+17d7*2 except cols 2/0xb, moisture penalties at
     * 12b8/12bc), x 2..0x4d y 2..0x2f scan for land 1/2/0xb, 0x15
     * neighbours sum (x2 idx<9 and idx==0, +2 water-adjacent 2/0xb, +0xc
     * river), dither -0x10 on (x*7+y*0xb&2), clamp ((sum-0x78)/8,1,0xf),
     * Map_SetTile(x+0x50,y,score) + rank layer (y+0x32) + hist 0x218a.
     * Caller 1028:0f0c. Tables TBD. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_build_continent_links(int yield)
{
    (void)yield;
    /* TODO: FUN_1028_12c6 @ 1028:12c6 (yield): 0x14 x 0x0d blocks of 4x4,
     * ocean (10) vs land corner pair on one continent id, gate
     * Path_CheckContinentLink @ 1128:16d0 < 0x14, OR dir bits at +4 (+
     * reverse). Progress 1000:29ea (Util_Progress over 0x1248) +
     * Sys_Yield @ 1098:0157 per row when yield!=0. Caller labeling. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_build_ocean_links(int yield)
{
    (void)yield;
    /* TODO: FUN_1028_162c @ 1028:162c (yield): 8-dir + 5-step ocean-run
     * check, bits at +0x108 (+ reverse), (0xb,3) corner exception, edge
     * OR 0xe0/0x0e + AND masks. Progress 1000:29ea (0x108,...) +
     * Sys_Yield per row when yield!=0. Callers 1028:0f01 + 10f8:01e9. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_copy_rect(int src_x, int src_y, unsigned w_bytes,
                                int h, int dst_x, int dst_y)
{
    /* Orig FUN_1028_1dfb @ 1028:1dfb -> block copy (src_x, src_y,
     * w_bytes, h, dst_x, dst_y) with dword fast path (w>>2, tail bytes)
     * and 0x50/0x140 stride wrap. OS-free memcpy loops port 1:1: row copy
     * via map_get/set_tile (OOB skipped, overlap-safe per-row snapshot);
     * dword path is equivalent to the byte loop on Linux. Callers
     * 1028:06c1/0865/0908 (save/restore island at +0xa0, commit). */
    int r;

    if (h <= 0)
        return CIV_OK;
    if (w_bytes == 0)
        return CIV_OK;
    if (w_bytes > 0x140u)
        return CIV_ERR_RANGE;
    if (h > 200)
        return CIV_ERR_RANGE;
    for (r = 0; r < h; r++) {
        /* Row snapshot keeps overlapping copies memmove-correct. */
        int row[0x140];
        unsigned c;

        for (c = 0; c < w_bytes; c++) {
            row[c] = map_get_tile(src_x + (int)c, src_y + r);
        }
        for (c = 0; c < w_bytes; c++) {
            if (row[c] < 0)
                continue;
            map_set_tile(dst_x + (int)c, dst_y + r, (uint8_t)row[c]);
        }
    }
    return CIV_OK;
}

int path_check_continent_link(int x, int y, int continent)
{
    (void)x;
    (void)y;
    (void)continent;
    /* TODO: Orig FUN_1128_16d0 @ 1128:16d0 (continent-link cost for the
     * 1028:12c6 ocean/land corner pairs; links only when < 0x14).
     * Pure tile-buffer walk; metric decode pending, so report no link. */
    return 0x14;
}

/* --- Chain 2026-10-16 (25): Map tile-passes (goal 2) + Earth frame (goal 5). ---
 * See worldgen.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit()).
 * Scratch/Win16 tails stay in worldgen_generate_random STUB above. */

int worldgen_island_target(int land_param)
{
    /* Orig Map_GenerateRandomOrLoad @ 1028:0000: iVar3 =
     * (DAT_12d8_38f4*8+0x10)*0x28 (DAT_12d8_38f4 = landmass setting,
     * preset 1 -> (8+0x10)*0x28 = 960). */
    if (land_param < 0)
        return -1;
    return (land_param * 8 + 0x10) * 0x28;
}

int worldgen_island_done(int count, int target)
{
    /* Orig same loop: do { Map_GenSeedOneIsland @ 1028:0945; ...; iVar3 =
     * target; } while (iVar3 - DAT_1248_0002 != 0 && DAT_1248_0002 <= iVar3).
     * Done when count == target (exact fill) or count > target (overshoot). */
    if (count == target)
        return 1;
    if (count > target)
        return 1;
    return 0;
}

int worldgen_blob_in_bounds(int x, int y)
{
    /* Orig Map_GenPaintBlob @ 1028:09ef: if ((local_6 == 0) || (param_1 < 3)
     * || (0x4c < param_1) return; if (param_2 < 3) return; } while (param_2
     * < 0x2e). Step-count gate lives caller-side (steps = Rng_Range(0x40)+1
     * in worldgen_paint_blob above); here the pure rect gate. */
    if (x < 3 || x > 0x4c)
        return 0;
    if (y < 3 || y >= 0x2e)
        return 0;
    return 1;
}

int worldgen_river_commit_len(int len)
{
    /* Orig Map_GenRivers @ 1028:066a: if ((bVar1 || cVar2 == '\t') && (4 <
     * (int)local_10)) commit (local_a++, banks 0x02 -> clear); else restore
     * via Map_CopyRect @ 1028:1dfb (save @ 06c1, restore @ 0865). */
    return (len > 4) ? 1 : 0;
}

int worldgen_river_target(int land_param, int moist_param)
{
    /* Orig same: ... || (DAT_12d8_38f4*2+DAT_12d8_38f8*2+6 <= local_a) break
     * (DAT_12d8_38f4 = landmass, DAT_12d8_38f8 = moisture; abort tez po 0xff
     * probach / 0x32 fails caller-side). */
    if (land_param < 0 || moist_param < 0)
        return -1;
    return land_param * 2 + moist_param * 2 + 6;
}

int worldgen_pollution_should_paint(int ocean_neighbours, int level, int x, int y)
{
    /* Orig Map_SpreadPollution @ 1028:1afd: if (local_8 < 7-param_1) { if
     * ((iVar6*0xb+iVar7*0xd&7) == param_1) paint (tile|1 + 130e/1306); }
     * else paint (130b/1303 + Map_ClearOccupancyBits(6)); Map_RevealCenter
     * + FUN_1138_0000 tails STUB. Level mask &7 matches the decompiler
     * (param_1 u16, low 3 bits used by &7). */
    int need;

    need = ((x * 0xb + y * 0xd) & 7);
    if (ocean_neighbours < 7 - level && need == (level & 7))
        return 1;
    return 0;
}

int worldgen_score_clamp(int sum)
{
    /* Orig Map_ScoreCitySites @ 1028:0fb4 (was FUN_1028_0fb4, renamed this
     * pass): iVar5 = Util_Clamp @ 11c8:0063 ((local_12-0x78)/8,1,0xf). */
    int v = (sum - 0x78) / 8;

    if (v < 1)
        v = 1;
    if (v > 0xf)
        v = 0xf;
    return v;
}

int worldgen_score_layer(int score)
{
    /* Orig same: local_c = iVar5/2+8; Map_SetTile(x+0x50,y,score) + rank
     * layer (y+0x32) + hist 0x218a tails caller-side. */
    return score / 2 + 8;
}

int worldgen_earth_frame_w(void)
{
    /* Orig preset path: Art_LoadFrameData(0x89) bitmap 320x200 (CIV_MAP_W/H
     * 0x50x0x32 tiles to 320x200 px; 64000B = 320*200 via CIV_MAP_PRESET_BYTES). */
    return CIV_MAP_EARTH_W;
}

int worldgen_earth_frame_h(void)
{
    /* Orig same. */
    return CIV_MAP_EARTH_H;
}

int worldgen_continent_rank_cmp(int size_a, int size_b)
{
    /* Orig Map_LabelContinents @ 1028:0a93 rank sort: larger continents rank
     * first (size desc; exact tiebreak TBD — tu stabilny 0). qsort shape. */
    if (size_a < size_b)
        return 1;
    if (size_a > size_b)
        return -1;
    return 0;
}

/* --- Chain 2026-10-16 (26): ScoreCitySites weights + link-graph kernels
 * (goals 1/2). See worldgen.h block. OS-free 1:1 LIVE (no Win16, no
 * globals, no exit()). Caller Map_LabelContinents @ 1028:0a93 tail;
 * tables 1420:17d6/17d8/17d7 + 12b8/12bc stay caller-side. */

int worldgen_score_weight(int v17d6, int v17d8, int v17d7, int col,
                          int moist_a, int moist_b)
{
    /* Orig Map_ScoreCitySites @ 1028:0fb4: local_42[i] = (char)17d6*3 +
     * (char)17d8 (+ (char)17d7*2 except cols 2/0xb) + moisture at
     * 12b8/12bc. Params are signed bytes/words already. */
    int w = v17d6 * 3 + v17d8;

    if (col != 2 && col != 0xb)
        w += v17d7 * 2;
    if (moist_b < 0)
        w += (-1 - moist_b);
    else if (moist_a < 0)
        w += (-1 - moist_a) * 2;
    return w;
}

int worldgen_score_water_bonus(int tile_class, int x, int y)
{
    /* Orig same: if ((class == 2 || class == 0xb) && ((x*7+y*0xb & 2) == 0))
     * local_c += 2. */
    if ((tile_class == 2 || tile_class == 0xb) &&
        (((x * 7 + y * 0xb) & 2) == 0))
        return 2;
    return 0;
}

int worldgen_score_dither(int center_class, int x, int y)
{
    /* Orig same: if (center != 1 && ((x*7+y*0xb & 2) != 0)) sum += -0x10. */
    if (center_class != 1 && (((x * 7 + y * 0xb) & 2) != 0))
        return -0x10;
    return 0;
}

int worldgen_score_accum(int acc, int weight, int idx, int bonus)
{
    /* Orig same 0x15-neighbour step: c = bonus + weight; idx<9 -> *2;
     * idx==0 -> *2 again (idx 0 counts x4). */
    int c = weight + bonus;

    if (idx < 9)
        c *= 2;
    if (idx == 0)
        c *= 2;
    return acc + c;
}

int worldgen_link_grid_w(void)
{
    /* Orig Map_BuildContinentLinks @ 1028:12c6 / Map_BuildOceanLinks @
     * 1028:162c: local_6 0..0x13 (0x14 cols), local_8 0..0x0c (0x0d rows),
     * 0x104 B at 1248:0004 / 1248:0108. */
    return CIV_LINK_GRID_W;
}

int worldgen_link_grid_h(void)
{
    /* Orig same. Ocean inner loop runs 0..0x0b + edge fixups, same grid. */
    return CIV_LINK_GRID_H;
}

int worldgen_link_idx(int bx, int by)
{
    /* Orig same cell: (byte*)(bx*0xd + by + 4) continent, (+0x108) ocean. */
    if (bx < 0 || bx > 0x13)
        return -1;
    if (by < 0 || by > 0x0c)
        return -1;
    return bx * 0x0d + by;
}

int worldgen_link_bit(int dir)
{
    /* Orig same OR: 1 << ((dir-1) & 0x1f), dir 1..4 continent / 1..8 ocean. */
    if (dir < 1 || dir > 8)
        return 0;
    return 1 << (dir - 1);
}

int worldgen_link_rev_bit(int dir)
{
    /* Orig same reverse OR: 1 << ((dir+3) & 7). */
    if (dir < 1 || dir > 8)
        return 0;
    return 1 << ((dir + 3) & 7);
}

int worldgen_continent_link_allowed(int cost)
{
    /* Orig Map_BuildContinentLinks @ 1028:12c6 gate: cost =
     * Path_CheckContinentLink @ 1128:16d0(...); link when cost != -1 &&
     * cost < 0x14 (path_check_continent_link STUB returns 0x14 = no link). */
    if (cost == -1)
        return 0;
    return (cost < CIV_LINK_COST_MAX) ? 1 : 0;
}

int worldgen_ocean_run_ok(int clear_rows)
{
    /* Orig Map_BuildOceanLinks @ 1028:162c 5-step check: each step needs
     * local_18 >= 2 (starts at 4, -1 per failed 2-wide row pair). */
    return (clear_rows >= 2) ? 1 : 0;
}

int worldgen_label_is_land_pass(int pass, int tile_is_ocean)
{
    /* Orig Map_LabelContinents @ 1028:0a93: if ((tile != 10) == (pass == 0))
     * label this pass (pass 0 land, pass 1 sea; tile==10 ocean). */
    int is_land = tile_is_ocean ? 0 : 1;
    int want_land = (pass == 0) ? 1 : 0;

    return (is_land == want_land) ? 1 : 0;
}

int worldgen_probe_font_height(void)
{
    /* Orig WorldGen_ProbeCustomFont @ 1060:0000: CREATEFONT(...,0x12,...)
     * (h = 0x12 = 18, caller WorldGen_ShowVideoCapsDialog @ 1060:01c1). */
    return 0x12;
}

int worldgen_probe_font_weight(void)
{
    /* Orig same: CREATEFONT weight 400 (normal, FW_NORMAL). */
    return 400;
}

civ_error_t worldgen_probe_custom_font(void)
{
    /* TODO: Orig WorldGen_ProbeCustomFont @ 1060:0000 GDI tail:
     * CREATEFONT(h 0x12, weight 400 above) + SELECTOBJECT + DELETEOBJECT
     * probe (caller WorldGen_ShowVideoCapsDialog @ 1060:01c1 GETDEVICECAPS
     * dump + Dlg_CreateTracked @ 1098:124f 0x94). Win16 GDI -> SDL_ttf TBD. */
    return CIV_ERR_UNSUPPORTED;
}

/* --- Chain 2026-10-16 (27): Scatter + SeedOneIsland-loop + VideoCaps.
 * See worldgen.h block. OS-free 1:1 LIVE (no Win16, no globals, no exit());
 * GDI/dialog tails STUB. */

int worldgen_scatter_count(int scatter_param)
{
    /* Orig Map_GenerateRandomOrLoad @ 1028:0000 scatter: iVar4 =
     * (DAT_12d8_38fa + 1) * 800; loop while iVar4 - local_6 != 0 &&
     * local_6 <= iVar4. */
    if (scatter_param < 0)
        return -1;
    return (scatter_param + 1) * CIV_SCATTER_STEP_MUL;
}

int worldgen_scatter_is_resample(unsigned step)
{
    /* Orig same: if ((local_6 & 1) == 0) Rng_Range x/y else neighbour step
     * via shared dir table (*(local_10*2) both axes). */
    return ((step & 1u) == 0u) ? 1 : 0;
}

int worldgen_scatter_tile_action(int tile)
{
    /* Orig same switch on Map_GetTile @ 11e8:0dcf: 0xd ->
     * Map_TryPlaceIsolatedTile @ 1028:05e7 (4 diagonale != 1 -> SetTile 1);
     * 2/3/6/7/10/0xb/0xc/0xf/0xe -> Map_SetTile commit; default skip. */
    if (tile == CIV_SCATTER_TRY_TILE)
        return 2;
    if (tile == 2 || tile == 3 || tile == 6 || tile == 7 || tile == 10 ||
        tile == 0x0b || tile == 0x0c || tile == 0x0f || tile == 0x0e)
        return 1;
    return 0;
}

int worldgen_seed_origin_x(int raw)
{
    /* Orig Map_GenSeedOneIsland @ 1028:0945: iVar2 = Rng_Range(0x48);
     * Map_GenPaintBlob(iVar2 + 4, ...). */
    return raw + 4;
}

int worldgen_seed_origin_y(int raw)
{
    /* Orig same: iVar3 = Rng_Range(0x22); PaintBlob(..., iVar3 + 8). */
    return raw + 8;
}

int worldgen_video_caps_query_count(void)
{
    /* Orig WorldGen_ShowVideoCapsDialog @ 1060:01c1 GETDEVICECAPS set:
     * 8,10,0xc,0xe,0x16,0x18,0x22,0x24,0x26,0x28,0x2a,0x58,0x5a (13). */
    return CIV_VIDEO_CAPS_QUERY_N;
}

int worldgen_video_caps_query_at(int index)
{
    /* Orig same order as the decompile locals local_20..local_3a. */
    static const int queries[CIV_VIDEO_CAPS_QUERY_N] = {
        8, 10, 0x0c, 0x0e, 0x16, 0x18, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x58,
        0x5a
    };

    if (index < 0 || index >= CIV_VIDEO_CAPS_QUERY_N)
        return -1;
    return queries[index];
}

int worldgen_video_caps_needs_raster_ext(int caps26)
{
    /* Orig same: if ((uVar3 & 0x100) != 0) query 0x68/0x6a (raster ext). */
    return ((caps26 & 0x100) != 0) ? 1 : 0;
}

civ_error_t worldgen_show_video_caps_dialog(void)
{
    /* TODO: Orig WorldGen_ShowVideoCapsDialog @ 1060:01c1 GDI tail: GETDC +
     * GETDEVICECAPS x13 (+ war. 0x68/0x6a) + RELEASEDC +
     * Dlg_CreateTracked @ 1098:124f (0x94,0x41) + FILLRECT + Text_SetColor @
     * 1098:0e0b + SETRECT/OFFSETRECT + DispatchFont @ 11e8:0b40 (5/6) +
     * WorldGen_ProbeCustomFont @ 1060:0000 (0x12) + rows via
     * WorldGen_DrawVideoCapsRow @ 1060:005b / WorldGen_DrawVideoCapsFlag @
     * 1060:00d1 + Sys_WaitForInput @ 1038:0c1d modal + Dlg_DestroyTracked @
     * 1098:1612. No callers (debug-only); thresholds LIVE in boot.c. */
    return CIV_ERR_UNSUPPORTED;
}
