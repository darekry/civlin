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
    /* TODO: Orig FUN_1108_0243 @ 1108:0243 (WorldGen_LoadEarth, choice 3 Play
     * on EARTH from MainMenu_WndProc @ 11e0:0221). Preset 64000B via
     * WorldGen_CopyPreset @ 10b8:079d + tail 1028:1d15/0a93. Preset image
     * source TBD (RSC entry vs file read); takes no caller buffer. Win16
     * resource/file load -> stub. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_prepare_earth(void)
{
    /* TODO: Orig neighbour @ 1108:01bc (WorldGen_PrepareEarth), adjacent to
     * FUN_1108_0243. Exact role TBD from decompile near 1108:0243
     * (presumed setup/prompt around the EARTH load). Win16 -> stub. */
    return CIV_ERR_UNSUPPORTED;
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
