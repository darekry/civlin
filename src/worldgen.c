/* Worldgen stubs. */
#include "worldgen.h"

#include <string.h>

civ_error_t worldgen_generate_random(void)
{
    /* TODO: Map_GenerateRandomOrLoad @ 1028:0000 param==0 (Sound_PlayId(4)
     * = OPENING, Map_GenSeedOneIsland loop, latitude/climate passes,
     * Map_GenRivers, Map_LabelContinents tail). */
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
    /* TODO: Map_GenSeedOneIsland @ 1028:0945 (scratch Map_FillRect +
     * Map_GenPaintBlob + copy pass). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_paint_blob(int x, int y)
{
    (void)x;
    (void)y;
    /* TODO: Map_GenPaintBlob @ 1028:09ef (Rng_Range(0x40)+1 step random
     * walk painting 0x0f; step table TBD). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_label_continents(void)
{
    /* TODO: Map_LabelContinents @ 1028:0a93 (flood-merge labels, size
     * counts @ 0x4334, top-0xF rank sort, 1028:12c6/162c/0fb4 tail). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_gen_rivers(void)
{
    /* TODO: Map_GenRivers @ 1028:066a (collect 0x0c tiles, directional
     * random walk, len > 4 required). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_spread_pollution(unsigned level)
{
    (void)level;
    /* TODO: Map_SpreadPollution @ 1028:1afd (ocean-neighbour count vs
     * 7 - level, paint 0x130e/0x1306 or 0x130b/0x1303 + 1110:1b36). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_score_city_sites(void)
{
    /* TODO: FUN_1028_0fb4 @ 1028:0fb4 (weights + 0x4c x 0x2e scan,
     * Map_SetTile x+0x50 score + histogram; see worldgen.h). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_build_continent_links(int yield)
{
    (void)yield;
    /* TODO: FUN_1028_12c6 @ 1028:12c6 (4x4 ocean/land corner match +
     * 1128:16d0 gate; see worldgen.h). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_build_ocean_links(int yield)
{
    (void)yield;
    /* TODO: FUN_1028_162c @ 1028:162c (8-dir + 5-step ocean-run check;
     * see worldgen.h). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_copy_rect(int src_x, int src_y, unsigned w_bytes,
                                int h, int dst_x, int dst_y)
{
    (void)src_x;
    (void)src_y;
    (void)w_bytes;
    (void)h;
    (void)dst_x;
    (void)dst_y;
    /* TODO: FUN_1028_1dfb @ 1028:1dfb (dword memcpy with stride wrap). */
    return CIV_ERR_UNSUPPORTED;
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
