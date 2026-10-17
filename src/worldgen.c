/* Worldgen pipeline detail (U slice, 1028 segment).
 * C17, no exit(), no globals (direction tables below are static const
 * read-only); tile I/O via map.h map_get/set_tile, RNG via sys.h
 * rng_range, pacing via sys.h sys_yield. */
#include "worldgen.h"

#include <string.h>

#include "map.h"
#include "sys.h"

/* 8-dir step table for rivers (Orig direction-table bytes TBD near
 * 1028:066a). Linux stub: 8-dir N-clockwise for the river walk.
 * Static const port (read-only). NOTE: paint_blob has its own 4-dir
 * N/E/S/W tables (12c8/12d0 odd words, see below), not this one. */
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
    /* Orig Map_GenerateRandomOrLoad @ 1028:0000 param==0 path: Sound_PlayId
     * (4)=OPENING.wav at start (audio.h, SDL tail TBD), then FillRect
     * scratch, SeedOneIsland loop (island_target/done), DrawRectBorder x2,
     * 6x-neighbour fixup, latitude/moisture, scatter TryPlaceIsolatedTile,
     * GenRivers, FillRect + LabelContinents (label + Score + Links +
     * borders). Ordered pipeline with Sys_Yield @ 1098:0157 per row +
     * Rng_Range @ 11c8:003d every pick + progress 1000:29ea. OS-free 1:1
     * port via map_fill_rect/draw_rect_border/get/set_tile,
     * map_try_place_isolated_tile @ 1028:05e7, map_wrap_x, rng_range,
     * island_target/done, seed_origin, scatter_count/is_resample/
     * tile_action, river_target/commit, score/label/link kernels,
     * sys_yield. Settings DAT_12d8_38f4/38f6/38f8/38fa TBD -> Linux
     * defaults land=1 (960 tiles), scatter=1 (1600 tries). Sound tail
     * TODO Orig Sound_PlayId(4) (audio module, not gated here). Latitude
     * exact band/6 switch + moisture budget/decay TBD -> minimal approx
     * (50% 1s kept for Score 1/2/0xb, rest random 3/6/7/0xd/0xe via
     * Rng_Range). Moisture sweep TBD. No Win16, no globals, no exit(). */
    int target;
    int count;
    int x;
    int y;
    int step;
    int scat_n;
    int cur_x;
    int cur_y;
    civ_error_t r;

    /* 1. Scratch clear (0,0,0x140,200). */
    map_fill_rect(0, 0, CIV_MAP_BUF_W, CIV_MAP_BUF_H, 0);
    /* 2. SeedOneIsland loop (no timeout per decompile: avg >0, always
     * ends; target-count !=0 && count <= target via island_done). */
    target = worldgen_island_target(1);
    count = 0;
    while (!worldgen_island_done(count, target)) {
        int added = 0;

        r = worldgen_seed_one_island(&added);
        if (r != CIV_OK)
            return r;
        count += added;
        sys_yield();
    }
    /* 3. Borders 0x0f (game 0x50 x 0x32, interior 1..0x4e/1..0x30). */
    map_draw_rect_border(0, 0, 0x4f, 0x31, 0x0f);
    map_draw_rect_border(1, 1, 0x4d, 0x2f, 0x0f);
    map_fill_rect(0, 0, CIV_MAP_W, 1, 0x0f);
    map_fill_rect(0, 0x31, CIV_MAP_W, 1, 0x0f);
    /* 4. 6x-neighbour diagonal fixup (0110/1001 over 2x2, 3 writes). */
    for (y = 1; y <= 0x30; y++) {
        for (x = 1; x <= 0x4d; x++) {
            int tl = map_get_tile(x, y);
            int tr = map_get_tile(x + 1, y);
            int bl = map_get_tile(x, y + 1);
            int br = map_get_tile(x + 1, y + 1);

            if (tl < 0 || tr < 0 || bl < 0 || br < 0)
                continue;
            if (tl == 0x0f || tr == 0x0f || bl == 0x0f || br == 0x0f)
                continue;
            if (tl == br && tr == bl && tl != tr) {
                map_set_tile(x + 1, y, (uint8_t)tl);
                map_set_tile(x, y + 1, (uint8_t)tl);
                map_set_tile(x + 1, y + 1, (uint8_t)tl);
            }
        }
        sys_yield();
    }
    /* 5. Latitude approx (exact band/6 desert/plains/forest/jungle/tundra
     * 3/6/7/0xc/0xd/0xe switch TBD). */
    for (y = 0; y < (int)CIV_MAP_H; y++) {
        for (x = 0; x < (int)CIV_MAP_W; x++) {
            int t = map_get_tile(x, y);

            if (t != 1)
                continue;
            if (rng_range(2) != 0)
                continue;
            {
                unsigned p = rng_range(5);

                if (p == 0)
                    map_set_tile(x, y, 3);
                else if (p == 1)
                    map_set_tile(x, y, 6);
                else if (p == 2)
                    map_set_tile(x, y, 7);
                else if (p == 3)
                    map_set_tile(x, y, 0x0d);
                else
                    map_set_tile(x, y, 0x0e);
            }
        }
        sys_yield();
    }
    /* 6. Scatter (DAT_12d8_38fa+1)*800, even resample + odd neighbour
     * step, 0xd -> TryPlaceIsolatedTile, 2/3/6/7/10/0xb/0xc/0xf/0xe commit
     * (no-op, tile already that value), default skip. */
    scat_n = worldgen_scatter_count(1);
    cur_x = 0;
    cur_y = 0;
    for (step = 0; step < scat_n; step++) {
        int t;
        int act;

        if (worldgen_scatter_is_resample((unsigned)step)) {
            cur_x = (int)rng_range((int)CIV_MAP_W);
            cur_y = (int)rng_range((int)CIV_MAP_H);
        } else {
            int dir = (int)rng_range(8);

            cur_x = map_wrap_x(cur_x + worldgen_dir_dx_at(dir));
            cur_y = cur_y + worldgen_dir_dy_at(dir);
            if (cur_y < 0 || cur_y >= (int)CIV_MAP_H)
                continue;
            if (cur_x < 0 || cur_x >= (int)CIV_MAP_W)
                continue;
        }
        t = map_get_tile(cur_x, cur_y);
        if (t < 0)
            continue;
        act = worldgen_scatter_tile_action(t);
        if (act == 2)
            (void)map_try_place_isolated_tile(cur_x, cur_y);
        else if (act == 1)
            (void)0;
        sys_yield();
    }
    /* 7. Rivers. */
    r = worldgen_gen_rivers();
    if (r != CIV_OK)
        return r;
    /* 8. Tail: FillRect(0xa0,0,0xa0,200,0) + Label (label + Score + Links
     * + borders, DAT_1248_0000=1 caller-side). */
    map_fill_rect(0xa0, 0, 0xa0, 200, 0);
    r = worldgen_label_continents();
    if (r != CIV_OK)
        return r;
    return CIV_OK;
}

civ_error_t worldgen_load_preset(const uint8_t *image, size_t len)
{
    /* Orig Map_GenerateRandomOrLoad @ 1028:0000 param!=0 path (civ.exe,
     * x86:LE:16:Protected Mode; ref unpacked/civ.exe read-only, NE Win3.x
     * 832512 B): Mem_DisposePtr + mem_alloc_lock 64000 + Art_LoadFrameData
     * (0x89) + GLOBALLOCK -10 adjust (0x600 head) + HMEMCPY 64000 via
     * WorldGen_CopyPreset @ 10b8:079d, then common tail Map_FillRect @
     * 1028:1d15 (0xa0,0,0xa0,200,0 clear) + Map_LabelContinents @ 1028:0a93
     * + DAT_1248_0000 = 1 (caller-side flag, no Linux global).
     * OS-free 1:1 port: caller supplies the 64000B image (RSC/file decode
     * stays caller-side: Art_LoadFrameData @ 10b8:079d via Art_LoadDiskFile
     * @ 10b8:0aa8 / Art_LoadResPaletted @ 10b8:11bc, see video.h art chain);
     * __far/HMEMCPY/GLOBALLOCK-0x600 selector plumbing maps to explicit
     * map_buffer() + worldgen_copy_preset memcpy (same bytes, no segment).
     * Tail via map_fill_rect + worldgen_label_continents (LIVE). NULL image
     * -> NOENT, len != 64000 -> RANGE. No Win16, no globals (buffer lives
     * in map.c .bss), no exit(). */
    uint8_t *dst;
    civ_error_t r;

    if (!image)
        return CIV_ERR_NOENT;
    if (len != CIV_MAP_PRESET_BYTES)
        return CIV_ERR_RANGE;
    if (map_buffer_len() < CIV_MAP_PRESET_BYTES)
        return CIV_ERR_RANGE;
    dst = map_buffer();
    if (!dst)
        return CIV_ERR_NOENT;
    r = worldgen_copy_preset(dst, image, len);
    if (r != CIV_OK)
        return r;
    map_fill_rect(0xa0, 0, 0xa0, 200, 0);
    r = worldgen_label_continents();
    if (r != CIV_OK)
        return r;
    return CIV_OK;
}

civ_error_t worldgen_load_earth(void)
{
    /* Orig WorldGen_LoadEarth @ 1108:0243 (civ.exe, x86:LE:16:Protected
     * Mode; ref unpacked/civ.exe read-only, NE Win3.x 832512 B: choice 3
     * Play on EARTH from MainMenu_WndProc @ 11e0:0221; dialog 0x8f with 4
     * rows x 3 cols + help col, IDs from 0x323, help texts Temperature @
     * 1420:2e28 / Climate @ 1420:2e34 / 0x2e23 / 0x2e3c via NewGame
     * customize @ 10e0:0000 + item rects via Dialog_GetItemRect @
     * 1100:03ff, frames via Art_LoadFrame @ 10b8:0e00). PARTIAL: OS-free
     * probes below are LIVE 1:1 (BAD_DATA on drift, no fake-OK); preset
     * image source + modal tail stay STUB.
     * OS-free LIVE: earth frame 0x89 via worldgen_earth_frame (caller-side
     * decode via Art_LoadFrameData @ 10b8:079d, see video.h art chain) +
     * 64000 B via worldgen_earth_preset_bytes + 320x200 via
     * worldgen_earth_frame_w/h (product == preset) + is_preset(1)/!is_preset
     * (0) gate + dialog ID->cell corners via worldgen_dialog_id_to_cell +
     * settings preset-1 shape via worldgen_custom_apply (help col 3 leaves
     * untouched). __far/HMEMCPY/GLOBALLOCK-0x600 plumbing maps to explicit
     * map_buffer() offsets in worldgen_load_preset/worldgen_copy_preset
     * (same bytes, no segment).
     * TODO: Orig preset image load (RSC entry vs file read --
     * Art_LoadFrame @ 10b8:0e00 frame 0x89 caller-side via
     * art_load_frame_data, takes no caller buffer) + modal dialog 0x8f
     * pump (Dlg_CreateTracked @ 1098:124f). Win16 resource/file load +
     * modal -> SDL TBD -> stub CIV_ERR_UNSUPPORTED (no fake-OK). */
    int row = -1;
    int col = -1;
    int settings[4] = { 1, 1, 1, 1 };
    civ_error_t r;

    if (worldgen_earth_frame() != (int)CIV_MAP_EARTH_FRAME)
        return CIV_ERR_BAD_DATA;
    if (worldgen_earth_frame() != 0x89)
        return CIV_ERR_BAD_DATA;
    if (worldgen_earth_preset_bytes() != CIV_MAP_PRESET_BYTES)
        return CIV_ERR_BAD_DATA;
    if (worldgen_earth_frame_w() != CIV_MAP_EARTH_W ||
        worldgen_earth_frame_h() != CIV_MAP_EARTH_H)
        return CIV_ERR_BAD_DATA;
    if (worldgen_earth_frame_w() * worldgen_earth_frame_h() !=
        (int)CIV_MAP_PRESET_BYTES)
        return CIV_ERR_BAD_DATA;
    if (!worldgen_is_preset(1) || worldgen_is_preset(0))
        return CIV_ERR_BAD_DATA;
    r = worldgen_dialog_id_to_cell(0x323, &row, &col);
    if (r != CIV_OK || row != 0 || col != 0)
        return CIV_ERR_BAD_DATA;
    r = worldgen_dialog_id_to_cell(0x323 + 15, &row, &col);
    if (r != CIV_OK || row != 3 || col != 3)
        return CIV_ERR_BAD_DATA;
    if (worldgen_dialog_id_to_cell(0x322, &row, &col) != CIV_ERR_RANGE)
        return CIV_ERR_BAD_DATA;
    r = worldgen_custom_apply(settings, 0, 0);
    if (r != CIV_OK || settings[0] != 0)
        return CIV_ERR_BAD_DATA;
    r = worldgen_custom_apply(settings, 1, 3);
    if (r != CIV_OK || settings[1] != 1)
        return CIV_ERR_BAD_DATA;
    if (worldgen_custom_apply(NULL, 0, 0) != CIV_ERR_NOENT)
        return CIV_ERR_BAD_DATA;
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
    /* Orig WorldGen_PrepareEarth @ 1108:01bc (civ.exe, x86:LE:16:Protected
     * Mode; ref unpacked/civ.exe read-only: pre-step w
     * Main_RunStartupSequence @ 11e0:0000 loop#2 przed NewGame_Wizard @
     * 10e8:0000: Cursor_Set(0x84) + Sound_PlayId @ 1150:00c9 (1) STOP +
     * dispatch g_nMenuChoice @ 12d8:44e2 przez worldgen_prepare_mode
     * (LIVE 1:1: 0 -> Map(0) random, 2 -> Map(1) earth-preset 0x89, 3 ->
     * Map(0) custom, inne -> -1 bez mapy) do Map_GenerateRandomOrLoad @
     * 1028:0000; ustawienia custom z WorldGen_LoadEarth @ 1108:0243 dialog
     * 0x8f + worldgen_dialog_id_to_cell LIVE). PARTIAL: OS-free dispatch
     * probes below are LIVE 1:1 (BAD_DATA on drift, no fake-OK);
     * Cursor/Sound/Map tails stay STUB. OS-free random tail LIVE
     * (worldgen_generate_random) + preset tail LIVE (worldgen_load_preset
     * + copy_preset/copy_rect 1:1); __far scratch maps to explicit
     * map_buffer() offsets (no segment).
     * TODO: Orig Cursor_Set(0x84) + Sound_PlayId(1) STOP + Map(0/1) call +
     * file/dialog tails -> SDL TBD -> stub CIV_ERR_UNSUPPORTED (no
     * fake-OK). P2 new-game closure: mode math done here, wizard/map own
     * the tails (see newgame.h/boot.h). */
    if (worldgen_prepare_mode(0) != 0)
        return CIV_ERR_BAD_DATA;
    if (worldgen_prepare_mode(2) != 1)
        return CIV_ERR_BAD_DATA;
    if (worldgen_prepare_mode(3) != 0)
        return CIV_ERR_BAD_DATA;
    if (worldgen_prepare_mode(1) != -1)
        return CIV_ERR_BAD_DATA;
    if (worldgen_prepare_mode(4) != -1)
        return CIV_ERR_BAD_DATA;
    if (worldgen_prepare_mode(5) != -1)
        return CIV_ERR_BAD_DATA;
    if (!worldgen_is_preset(worldgen_prepare_mode(2)))
        return CIV_ERR_BAD_DATA;
    if (worldgen_is_preset(worldgen_prepare_mode(0)))
        return CIV_ERR_BAD_DATA;
    if (worldgen_is_preset(worldgen_prepare_mode(1)) == 0)
        return CIV_ERR_BAD_DATA;
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
    /* Orig WorldGen_Customize entry driving NewGame_Wizard @ 10e8:0000 steps
     * 3/4 (civ.exe, x86:LE:16:Protected Mode; ref unpacked/civ.exe read-only,
     * NE Win3.x 832512 B: RANDOMRADIOPROC @ 1058:06ab, RANDOMSTATICPROC @
     * 1058:081a, RANDOMUSERPROC @ 1058:0921, REGIONPROC @ 1058:09f6 strings
     * present; classes via Ctrl_RegisterCustomClasses @ 1058:0b4d, owned by
     * the dialog module -- not redeclared here to keep parallel slices
     * disjoint, see dialog.h; custom modal variant @ 1220:16fd via
     * newgame_customize_world, see newgame.h). PARTIAL: OS-free dispatcher
     * gates below are LIVE 1:1 (BAD_DATA on drift, no fake-OK); Win16 modal
     * pump stays STUB. OS-free LIVE: custom path via worldgen_prepare_mode
     * (3 -> Map(0) random, 2 -> Map(1) earth-preset 0x89 contrast) +
     * preset gate via worldgen_is_preset (1 true / 0 false) + dialog
     * ID->cell corners via worldgen_dialog_id_to_cell (0x323 -> row 0 col
     * 0, 0x323+15 -> row 3 col 3, 0x322 -> RANGE) + settings write via
     * worldgen_custom_apply (choice col < 3 stores, help col 3 leaves
     * untouched, NULL -> NOENT, OOR row/col -> RANGE). __far/selector
     * plumbing maps to explicit out-params (no segment).
     * TODO: Orig modal host Dlg_CreateTracked @ 1098:124f (+
     * Dlg_DestroyTracked @ 1098:1612 teardown) + Dialog_MenuZ_Modal @
     * 1220:0156 / custom variant @ 1220:16fd + Ctrl_RegisterCustomClasses
     * @ 1058:0b4d fan-out + PEEKMESSAGE/ISDIALOGMESSAGE pump -> SDL TBD ->
     * stub CIV_ERR_UNSUPPORTED (no fake-OK). No globals, no exit(). */
    int row = -1;
    int col = -1;
    int settings[4] = { 1, 1, 1, 1 };
    civ_error_t r;

    if (worldgen_prepare_mode(3) != 0)
        return CIV_ERR_BAD_DATA;
    if (worldgen_prepare_mode(2) != 1)
        return CIV_ERR_BAD_DATA;
    if (!worldgen_is_preset(1) || worldgen_is_preset(0))
        return CIV_ERR_BAD_DATA;
    r = worldgen_dialog_id_to_cell(0x323, &row, &col);
    if (r != CIV_OK || row != 0 || col != 0)
        return CIV_ERR_BAD_DATA;
    r = worldgen_dialog_id_to_cell(0x323 + 15, &row, &col);
    if (r != CIV_OK || row != 3 || col != 3)
        return CIV_ERR_BAD_DATA;
    if (worldgen_dialog_id_to_cell(0x322, &row, &col) != CIV_ERR_RANGE)
        return CIV_ERR_BAD_DATA;
    if (worldgen_dialog_id_to_cell(0x323, NULL, &col) != CIV_ERR_NOENT)
        return CIV_ERR_BAD_DATA;
    r = worldgen_custom_apply(settings, 0, 0);
    if (r != CIV_OK || settings[0] != 0)
        return CIV_ERR_BAD_DATA;
    r = worldgen_custom_apply(settings, 1, 3);
    if (r != CIV_OK || settings[1] != 1)
        return CIV_ERR_BAD_DATA;
    if (worldgen_custom_apply(NULL, 0, 0) != CIV_ERR_NOENT)
        return CIV_ERR_BAD_DATA;
    if (worldgen_custom_apply(settings, 4, 0) != CIV_ERR_RANGE)
        return CIV_ERR_BAD_DATA;
    if (worldgen_custom_apply(settings, 0, 4) != CIV_ERR_RANGE)
        return CIV_ERR_BAD_DATA;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_customize_dialog(void)
{
    /* Orig WorldGen_CustomizeDialog host driving Ctrl_* @ 1058 (civ.exe,
     * x86:LE:16:Protected Mode; ref unpacked/civ.exe read-only, NE Win3.x
     * 832512 B: RANDOMRADIOPROC @ 1058:06ab, RANDOMSTATICPROC @ 1058:081a,
     * RANDOMUSERPROC @ 1058:0921, REGIONPROC @ 1058:09f6 strings present;
     * classes via Ctrl_RegisterCustomClasses @ 1058:0b4d, owned by the
     * dialog module -- not redeclared here to keep parallel slices
     * disjoint, see dialog.h). PARTIAL: OS-free ID->cell/settings gates
     * below are LIVE 1:1 (BAD_DATA on drift, no fake-OK); Win16 modal pump
     * stays STUB. OS-free LIVE: dialog ID->cell corners via
     * worldgen_dialog_id_to_cell (0x323 -> row 0 col 0, 0x323+15 -> row 3
     * col 3, 0x322 -> RANGE) + settings write via worldgen_custom_apply
     * (choice col < 3 stores, help col 3 leaves untouched, NULL -> NOENT,
     * OOR row/col -> RANGE). __far/selector plumbing maps to explicit
     * out-params (no segment).
     * TODO: Orig modal host Dlg_CreateTracked @ 1098:124f (+
     * Dlg_DestroyTracked @ 1098:1612 teardown) + Ctrl_RegisterCustomClasses
     * @ 1058:0b4d fan-out + PEEKMESSAGE/ISDIALOGMESSAGE pump -> SDL TBD ->
     * stub CIV_ERR_UNSUPPORTED (no fake-OK). No globals, no exit(). */
    int row = -1;
    int col = -1;
    int settings[4] = { 1, 1, 1, 1 };
    civ_error_t r;

    r = worldgen_dialog_id_to_cell(0x323, &row, &col);
    if (r != CIV_OK || row != 0 || col != 0)
        return CIV_ERR_BAD_DATA;
    r = worldgen_dialog_id_to_cell(0x323 + 15, &row, &col);
    if (r != CIV_OK || row != 3 || col != 3)
        return CIV_ERR_BAD_DATA;
    if (worldgen_dialog_id_to_cell(0x322, &row, &col) != CIV_ERR_RANGE)
        return CIV_ERR_BAD_DATA;
    if (worldgen_dialog_id_to_cell(0x323, NULL, &col) != CIV_ERR_NOENT)
        return CIV_ERR_BAD_DATA;
    r = worldgen_custom_apply(settings, 0, 0);
    if (r != CIV_OK || settings[0] != 0)
        return CIV_ERR_BAD_DATA;
    r = worldgen_custom_apply(settings, 1, 3);
    if (r != CIV_OK || settings[1] != 1)
        return CIV_ERR_BAD_DATA;
    if (worldgen_custom_apply(NULL, 0, 0) != CIV_ERR_NOENT)
        return CIV_ERR_BAD_DATA;
    if (worldgen_custom_apply(settings, 4, 0) != CIV_ERR_RANGE)
        return CIV_ERR_BAD_DATA;
    if (worldgen_custom_apply(settings, 0, 4) != CIV_ERR_RANGE)
        return CIV_ERR_BAD_DATA;
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_seed_one_island(int *count_out)
{
    /* Orig Map_GenSeedOneIsland @ 1028:0945 1:1 (decompiled):
     *   Map_FillRect(0xa0, 0, 0x50, 0x32, 0);
     *   x = Rng_Range(0x48); y = Rng_Range(0x22);
     *   Map_GenPaintBlob(x + 4, y + 8);            (single pass)
     *   for (x = 0; x < 0x50; x++) { Sys_Yield();
     *     for (y = 0; y < 0x32; y++)
     *       if (Map_GetTile(x + 0xa0, y) != 0) {
     *         Map_SetTile(x, y, Map_GetTile(x, y) + 1);
     *         DAT_1248_0002++;
     *       }
     *   }
     * Scratch lives at x+0xa0 inside the 0x140 x 200 tile buffer (see
     * map.h: game map 0x50 x 0x32 at origin, island copy at +0xa0).
     * Linux uses map_fill_rect / worldgen_paint_blob / map_get/set_tile
     * + rng_range / sys_yield directly (no segment). DAT_1248_0002 is
     * an explicit out-param (required; NULL -> NOENT). OOB reads return
     * the map_get_tile sentinel (-1 != 0, counted like orig nonzero).
     * No Win16, no exit(). */
    int x;
    int y;
    int count = 0;

    if (!count_out)
        return CIV_ERR_NOENT;
    map_fill_rect(0xa0, 0, 0x50u, 0x32u, 0);
    {
        int sx = (int)rng_range(0x48);
        int sy = (int)rng_range(0x22);

        worldgen_paint_blob(sx + 4, sy + 8);
    }
    for (x = 0; x < 0x50; x++) {
        sys_yield();
        for (y = 0; y < 0x32; y++) {
            if (map_get_tile(x + 0xa0, y) != 0) {
                /* Main-map (x,y) is always in-bounds here, so the
                 * get_tile sentinel never fires; (uint8_t) wrap matches
                 * the orig char +1. */
                map_set_tile(x, y, (uint8_t)(map_get_tile(x, y) + 1));
                count++;
            }
        }
    }
    *count_out = count;
    return CIV_OK;
}

civ_error_t worldgen_paint_blob(int x, int y)
{
    /* Orig Map_GenPaintBlob @ 1028:09ef 1:1 (disassembly):
     *   steps = Rng_Range(0x40)+1;
     *   loop { Map_SetTile(x+0xa0, y, 0xf); Map_SetTile(x+0xa1, y, 0xf);
     *     Map_SetTile(x+0xa0, y+1, 0xf);            (L-tromino, scratch)
     *     r = Rng_Range(4); i = r*2+1;               (odd word index)
     *     x += dx[i]; y += dy[i];                   (word tables below)
     *     if (--steps == 0) return;
     *     if (x <= 2 || x >= 0x4d) return;
     *     if (y <= 2 || y >= 0x2e) return; }
     * Direction words (read from unpacked/civ.exe): dx @ 12c8:0000 odd
     * words {0,+1,0,-1}, dy @ 12d0:0000 odd words {-1,0,+1,0} = 4-dir
     * N/E/S/W clockwise. Coords are scratch-local (paint adds +0xa0 to x
     * only); sole caller is worldgen_seed_one_island above. OOB tiles
     * fall out via map_set_tile. No Win16, no exit(). */
    static const int s_dx[4] = { 0, 1, 0, -1 };   /* 12c8 odd words */
    static const int s_dy[4] = { -1, 0, 1, 0 };   /* 12d0 odd words */
    unsigned steps = rng_range(0x40) + 1u;

    for (;;) {
        unsigned r;

        map_set_tile(x + 0xa0, y, 0x0fu);
        map_set_tile(x + 0xa1, y, 0x0fu);
        map_set_tile(x + 0xa0, y + 1, 0x0fu);
        r = rng_range(4);
        if (r > 3u)
            r = 3u;
        x += s_dx[r];
        y += s_dy[r];
        steps--;
        if (steps == 0)
            return CIV_OK;
        if (x <= 2 || x >= 0x4d)
            return CIV_OK;
        if (y <= 2 || y >= 0x2e)
            return CIV_OK;
    }
}

civ_error_t worldgen_label_continents(void)
{
    /* Orig Map_LabelContinents @ 1028:0a93: connected-components labeling
     * over 0x50 x 0x32 + rank sort (flood-merge labels, size counts @
     * 0x4334, top-0xF rank sort size desc via worldgen_continent_rank_cmp,
     * calls 1028:12c6/162c/0fb4 tail + border FillRects via Map_FillRect @
     * 1028:1d15 / Map_DrawRectBorder @ 1028:1ca7). OS-free 1:1 port via
     * map_get/set_tile, map_wrap_x @ 1128:177e (X wrap, Y never wraps),
     * worldgen_label_is_land_pass gate ((tile != 10) == (pass == 0)),
     * rank cmp, sys_yield per row. Continent ids live at tile(x,y+0x32)
     * (cf. map_get_continent_id @ 1110:1d99). No Win16, no globals (queue
     * + sizes are locals), no exit(). */
    static const int fdx[4] = { 0, 1, 0, -1 };
    static const int fdy[4] = { -1, 0, 1, 0 };
    int sizes[256];
    int remap[256];
    int order[256];
    int next_id;
    int pass;
    int y;
    int x;
    int i;
    int n;
    civ_error_t r;

    for (i = 0; i < 256; i++) {
        sizes[i] = 0;
        remap[i] = 0;
        order[i] = 0;
    }
    /* Clear continent plane (0,0x32,0x50,0x32). */
    map_fill_rect(0, 0x32, CIV_MAP_W, CIV_MAP_H, 0);
    next_id = 1;
    for (pass = 0; pass <= 1; pass++) {
        for (y = 0; y < (int)CIV_MAP_H; y++) {
            for (x = 0; x < (int)CIV_MAP_W; x++) {
                int t;
                int is_ocean;
                int cur;
                int qx[4000];
                int qy[4000];
                int qh;
                int qt;

                t = map_get_tile(x, y);
                if (t < 0)
                    continue;
                is_ocean = (t == 10) ? 1 : 0;
                if (!worldgen_label_is_land_pass(pass, is_ocean))
                    continue;
                cur = map_get_tile(x, y + 0x32);
                if (cur != 0)
                    continue;
                if (next_id >= 255)
                    continue;
                cur = next_id;
                next_id++;
                sizes[cur] = 1;
                map_set_tile(x, y + 0x32, (uint8_t)cur);
                qx[0] = x;
                qy[0] = y;
                qh = 0;
                qt = 1;
                while (qh < qt) {
                    int cx = qx[qh];
                    int cy = qy[qh];
                    int d;

                    qh++;
                    for (d = 0; d < 4; d++) {
                        int nx = map_wrap_x(cx + fdx[d]);
                        int ny = cy + fdy[d];
                        int nt;
                        int n_ocean;
                        int nl;

                        if (ny < 0 || ny >= (int)CIV_MAP_H)
                            continue;
                        if (nx < 0 || nx >= (int)CIV_MAP_W)
                            continue;
                        nl = map_get_tile(nx, ny + 0x32);
                        if (nl != 0)
                            continue;
                        nt = map_get_tile(nx, ny);
                        if (nt < 0)
                            continue;
                        n_ocean = (nt == 10) ? 1 : 0;
                        if (n_ocean != is_ocean)
                            continue;
                        if (!worldgen_label_is_land_pass(pass, n_ocean))
                            continue;
                        map_set_tile(nx, ny + 0x32, (uint8_t)cur);
                        sizes[cur]++;
                        if (qt < 4000) {
                            qx[qt] = nx;
                            qy[qt] = ny;
                            qt++;
                        }
                    }
                }
            }
            sys_yield();
        }
    }
    /* Rank sort size desc (larger first, tie 0 stable) via
     * worldgen_continent_rank_cmp, then remap old -> rank+1. */
    n = next_id - 1;
    if (n > 255)
        n = 255;
    for (i = 0; i < n; i++)
        order[i] = i + 1;
    for (i = 0; i < n; i++) {
        int j;

        for (j = i + 1; j < n; j++) {
            int a = order[i];
            int b = order[j];
            int c = worldgen_continent_rank_cmp(sizes[a], sizes[b]);

            if (c > 0) {
                order[i] = b;
                order[j] = a;
            }
        }
    }
    for (i = 0; i < n; i++)
        remap[order[i]] = i + 1;
    for (y = 0; y < (int)CIV_MAP_H; y++) {
        for (x = 0; x < (int)CIV_MAP_W; x++) {
            int old = map_get_tile(x, y + 0x32);

            if (old > 0 && old < 256 && remap[old] != 0)
                map_set_tile(x, y + 0x32, (uint8_t)remap[old]);
        }
    }
    /* Border FillRects + DrawRectBorder tails (Map_FillRect @ 1028:1d15
     * edges + Map_DrawRectBorder @ 1028:1ca7). Idempotent with the
     * generate_random borders. */
    map_fill_rect(0, 0, CIV_MAP_W, 1, 0x0f);
    map_fill_rect(0, 0x31, CIV_MAP_W, 1, 0x0f);
    map_draw_rect_border(0, 0, 0x4f, 0x31, 0x0f);
    map_draw_rect_border(1, 1, 0x4d, 0x2f, 0x0f);
    /* Tails: BuildContinentLinks @ 1028:12c6 + BuildOceanLinks @ 1028:162c
     * + ScoreCitySites @ 1028:0fb4 (sole caller 1028:0f0c path). */
    r = worldgen_build_continent_links(1);
    if (r != CIV_OK)
        return r;
    r = worldgen_build_ocean_links(1);
    if (r != CIV_OK)
        return r;
    r = worldgen_score_city_sites();
    if (r != CIV_OK)
        return r;
    return CIV_OK;
}

civ_error_t worldgen_gen_rivers(void)
{
    /* Orig Map_GenRivers @ 1028:066a: collect 0x0c tiles (8000B x2 bufs),
     * Rng_Range @ 11c8:003d start + dir, walk until adjacent 0x01/0x09/
     * 0x0d/0x01 or len cap; len > 4 commit via worldgen_river_commit_len
     * (banks 0x02 -> clear); save/restore island at +0xa0 via Map_CopyRect
     * @ 1028:1dfb (save @ 06c1, restore @ 0865, commit @ 0908); count =
     * river_target = DAT_12d8_38f4*2+DAT_12d8_38f8*2+6; abort after 0xff
     * tries / 0x32 fails. OS-free 1:1 port via map_get/set_tile,
     * map_wrap_x (X wrap), rng_range, copy_rect, river_commit_len/target,
     * dir dx/dy table, sys_yield per try. Settings 38f4/38f8 TBD -> Linux
     * defaults land=1/moist=1 (preset 1 => target 10). Walk drift
     * (local_c+iVar6*2+(local_10&1)*-2)&7 TBD -> simplified 8-dir
     * Rng_Range(8) walk (PARTIAL, exact metric TODO Orig). No Win16, no
     * globals (seeds + path are locals), no exit(). */
    int seed_x[4000];
    int seed_y[4000];
    int nseed;
    int target;
    int committed;
    int tries;
    int fails;
    int y;
    int x;

    nseed = 0;
    for (y = 0; y < (int)CIV_MAP_H; y++) {
        for (x = 0; x < (int)CIV_MAP_W; x++) {
            int t = map_get_tile(x, y);

            if (t == 0x0c && nseed < 4000) {
                seed_x[nseed] = x;
                seed_y[nseed] = y;
                nseed++;
            }
        }
    }
    if (nseed == 0)
        return CIV_OK;
    target = worldgen_river_target(1, 1);
    if (target < 0)
        target = 10;
    committed = 0;
    tries = 0;
    fails = 0;
    while (committed < target && tries < 0xff && fails < 0x32) {
        int si;
        int rx;
        int ry;
        int dir;
        int len;
        int path_x[32];
        int path_y[32];
        int k;
        int reached;
        civ_error_t sr;

        tries++;
        si = (int)rng_range(nseed);
        rx = seed_x[si];
        ry = seed_y[si];
        dir = (int)rng_range(8);
        /* Save island at +0xa0 (06c1). */
        sr = worldgen_copy_rect(0, 0, CIV_MAP_W, (int)CIV_MAP_H, 0xa0, 0);
        if (sr != CIV_OK) {
            fails++;
            sys_yield();
            continue;
        }
        len = 0;
        reached = 0;
        for (k = 0; k < 32; k++) {
            int d;
            int found;

            found = 0;
            for (d = 0; d < 8; d++) {
                int ax = map_wrap_x(rx + worldgen_dir_dx_at(d));
                int ay = ry + worldgen_dir_dy_at(d);
                int at;

                if (ay < 0 || ay >= (int)CIV_MAP_H)
                    continue;
                if (ax < 0 || ax >= (int)CIV_MAP_BUF_W)
                    continue;
                at = map_get_tile(ax, ay);
                if (at == 0x01 || at == 0x09 || at == 0x0d) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                reached = 1;
                break;
            }
            dir = (int)rng_range(8);
            rx = map_wrap_x(rx + worldgen_dir_dx_at(dir));
            ry = ry + worldgen_dir_dy_at(dir);
            if (ry < 0 || ry >= (int)CIV_MAP_H)
                break;
            if (rx < 0 || rx >= (int)CIV_MAP_W)
                break;
            if (len < 32) {
                path_x[len] = rx;
                path_y[len] = ry;
                len++;
            }
            map_set_tile(rx, ry, 0x0c);
        }
        (void)reached;
        if (worldgen_river_commit_len(len)) {
            int p;
            int d;

            /* Commit (0908): keep walk + clear banks 0x02. */
            for (p = 0; p < len; p++) {
                for (d = 0; d < 8; d++) {
                    int bx = map_wrap_x(path_x[p] + worldgen_dir_dx_at(d));
                    int by = path_y[p] + worldgen_dir_dy_at(d);

                    if (by < 0 || by >= (int)CIV_MAP_H)
                        continue;
                    if (bx < 0 || bx >= (int)CIV_MAP_W)
                        continue;
                    if (map_get_tile(bx, by) == 0x02)
                        map_set_tile(bx, by, 0);
                }
            }
            committed++;
        } else {
            /* Restore (0865). */
            (void)worldgen_copy_rect(0xa0, 0, CIV_MAP_W, (int)CIV_MAP_H,
                                     0, 0);
            fails++;
        }
        sys_yield();
    }
    return CIV_OK;
}

civ_error_t worldgen_spread_pollution(unsigned level)
{
    /* Orig Map_SpreadPollution @ 1028:1afd: fallout tiles from
     * Game_DoPollutionAndWonderRace. Level -> n tiles via Rng_Range @
     * 11c8:003d + Map_SetTile @ 11e8:0e19 loop over 0x50 x 0x32 (n grows
     * with level, random sites via Rng_Range, paint fallout tile via
     * Map_SetTile). Gate via worldgen_pollution_should_paint (ocean_neigh
     * < 7-level && ((x*0xb+y*0xd)&7) == (level&7), decompiler local_8 +
     * iVar6*0xb+iVar7*0xd&7). Paint is tile|1 + 130e/1306 vs 130b/1303 +
     * Map_ClearOccupancyBits(6) @ 1110:1b36 + Map_RevealCenter @ 1110:1461
     * tails. OS-free 1:1 port via rng_range, map_get/set_tile,
     * map_clear_occupancy_bits (LIVE), should_paint gate, sys_yield.
     * Exact n formula TBD -> Linux n = level*8 (grows with level, level 0
     * => 0 no-op). Ocean-neighbour count is 8-neighbour ocean(10) via
     * shared dx/dy. 130e/1306 vs 130b/1303 layer choice + RevealCenter SDL
     * view tail TBD -> PARTIAL (tile|1 + clear(6) LIVE, view documented).
     * Level 0 returns CIV_OK (provably no-op). No Win16, no globals. */
    unsigned n;
    unsigned i;

    if (level == 0)
        return CIV_OK;
    if (level > 7)
        level = 7;
    n = level * 8u;
    for (i = 0; i < n; i++) {
        int x = (int)rng_range((int)CIV_MAP_W);
        int y = (int)rng_range((int)CIV_MAP_H);
        int t;
        int d;
        int ocean_n;

        t = map_get_tile(x, y);
        if (t < 0)
            continue;
        ocean_n = 0;
        for (d = 0; d < 8; d++) {
            int nx = map_wrap_x(x + worldgen_dir_dx_at(d));
            int ny = y + worldgen_dir_dy_at(d);

            if (ny < 0 || ny >= (int)CIV_MAP_H)
                continue;
            if (nx < 0 || nx >= (int)CIV_MAP_W)
                continue;
            if (map_get_tile(nx, ny) == 10)
                ocean_n++;
        }
        if (!worldgen_pollution_should_paint(ocean_n, (int)level, x, y))
            continue;
        map_set_tile(x, y, (uint8_t)(t | 1));
        (void)map_clear_occupancy_bits(x, y, 6u);
        if ((i & 7u) == 0u)
            sys_yield();
    }
    return CIV_OK;
}

civ_error_t worldgen_score_city_sites(void)
{
    /* Orig FUN_1028_0fb4 @ 1028:0fb4 -> Map_ScoreCitySites: weights[0x18]
     * from 1420:17d6*3+17d8 (+17d7*2 except cols 2/0xb, moisture at
     * 12b8/12bc), scan x 2..0x4d y 2..0x2f for land 1/2/0xb, 0x15
     * neighbours (5x5 minus 4 corners: centre + 8 + outer 12), x2 idx<9
     * and idx==0, +2 water-adjacent 2/0xb, +0xc improved, dither -0x10,
     * clamp ((sum-0x78)/8,1,0xf) via worldgen_score_clamp + layer
     * score/2+8 via worldgen_score_layer, writes Map_SetTile(x+0x50,y,score)
     * + rank layer (y+0x32) + hist 0x218a. Caller Map_LabelContinents @
     * 1028:0f0c. OS-free 1:1 port via map_get/set_tile, map_wrap_x (X wrap),
     * map_check_tile_improved @ 1110:1c72 (seed DAT_13c0_0000 TBD -> 0),
     * score_weight/water_bonus/dither/accum/clamp/layer kernels, sys_yield
     * per row. Tables 1420:17d6/17d8/17d7 + 12b8/12bc caller-side TBD ->
     * Linux approx w = tile*3 via score_weight for land (ocean 10 folded
     * to 0, only water bonus matters), exact yield tables TBD. Rank-copy
     * (y+0x32 overwrite) + hist 0x218a tails TBD -> PARTIAL (score layer
     * LIVE, rank/hist documented). No Win16, no globals, no exit(). */
    static const int ndx[21] = {
        0, 0, 1, 1, 1, 0, -1, -1, -1,
        -2, -2, -2, -1, -1, 0, 0, 1, 1, 2, 2, 2
    };
    static const int ndy[21] = {
        0, -1, -1, 0, 1, 1, 1, 0, -1,
        -1, 0, 1, -2, 2, -2, 2, -2, 2, -1, 0, 1
    };
    int y;
    int x;

    for (y = 2; y <= 0x2f; y++) {
        for (x = 2; x <= 0x4d; x++) {
            int centre;
            int sum;
            int idx;
            int score;

            centre = map_get_tile(x, y);
            if (centre != 1 && centre != 2 && centre != 0x0b)
                continue;
            sum = 0;
            for (idx = 0; idx < 21; idx++) {
                int nx = map_wrap_x(x + ndx[idx]);
                int ny = y + ndy[idx];
                int t;
                int w;
                int bonus;

                if (ny < 0 || ny >= (int)CIV_MAP_BUF_H) {
                    w = 0;
                    bonus = 0;
                } else {
                    if (nx < 0 || nx >= (int)CIV_MAP_BUF_W)
                        t = -1;
                    else
                        t = map_get_tile(nx, ny);
                    if (t < 0) {
                        w = 0;
                        bonus = 0;
                    } else if (t == 10) {
                        w = 0;
                        bonus = worldgen_score_water_bonus(t, x, y);
                    } else {
                        w = worldgen_score_weight(t, 0, 0, t, 0, 0);
                        bonus = worldgen_score_water_bonus(t, x, y);
                    }
                }
                sum = worldgen_score_accum(sum, w, idx, bonus);
            }
            if (map_check_tile_improved(centre, x, y, 0))
                sum += 0x0c;
            sum += worldgen_score_dither(centre, x, y);
            score = worldgen_score_clamp(sum);
            (void)worldgen_score_layer(score);
            map_set_tile(x + 0x50, y, (uint8_t)score);
        }
        sys_yield();
    }
    return CIV_OK;
}

civ_error_t worldgen_build_continent_links(int yield)
{
    /* Orig FUN_1028_12c6 @ 1028:12c6 (yield) -> Map_BuildContinentLinks:
     * 0x14 x 0x0d blocks of 4x4, ocean(10)/land corner pair on one
     * continent id, gate Path_CheckContinentLink @ 1128:16d0 < 0x14, OR
     * dir bits at +4 (+ reverse). Progress 1000:29ea (Util_Progress over
     * 0x1248 bar) + Sys_Yield @ 1098:0157 per row when yield!=0. Caller
     * Map_LabelContinents @ 1028:0a93. OS-free 1:1 port via map_get_tile,
     * map_get_continent_id @ 1110:1d99 (tile(x,y+0x32)), map_wrap_x,
     * path_check_continent_link, continent_link_allowed, link_grid_w/h,
     * link_idx/bit/rev_bit, sys_yield. Link tables 0x104B at 1248:0004 /
     * 0108 have no Linux globals -> mapped into the free tile-buffer
     * window (80,50) 20x13 cont + (80,64) 20x13 ocean (x 80..99, y
     * 50..76, free vs main/score/scratch/continent/occupancy, see map.h),
     * persisted via map_get/set_tile (no new globals). Dir 1..4 cont
     * (1=N,2=E,3=S,4=W) from ocean->land delta; reverse via rev_bit.
     * Progress UI stays caller-side (util_progress available). No Win16,
     * no exit(). */
    const int base_x = (int)CIV_MAP_W;
    const int base_y = (int)CIV_MAP_H;
    int bx;
    int by;

    map_fill_rect(base_x, base_y, (unsigned)worldgen_link_grid_w(),
                  (unsigned)worldgen_link_grid_h(), 0);
    for (by = 0; by < worldgen_link_grid_h(); by++) {
        for (bx = 0; bx < worldgen_link_grid_w(); bx++) {
            int cx[4];
            int cy[4];
            int ct[4];
            int cc[4];
            int k;
            int ocean_i;
            int land_i;

            cx[0] = bx * 4 + 1;
            cy[0] = by * 4 + 1;
            cx[1] = bx * 4 + 2;
            cy[1] = by * 4 + 1;
            cx[2] = bx * 4 + 1;
            cy[2] = by * 4 + 2;
            cx[3] = bx * 4 + 2;
            cy[3] = by * 4 + 2;
            for (k = 0; k < 4; k++) {
                int wx = map_wrap_x(cx[k]);

                if (cx[k] < 0 || cx[k] >= (int)CIV_MAP_W ||
                    cy[k] < 0 || cy[k] >= (int)CIV_MAP_H) {
                    ct[k] = -1;
                    cc[k] = -1;
                } else {
                    ct[k] = map_get_tile(wx, cy[k]);
                    cc[k] = map_get_continent_id(wx, cy[k]);
                }
            }
            ocean_i = -1;
            land_i = -1;
            for (k = 0; k < 4; k++) {
                int t = ct[k];

                if (t == 10 && cc[k] >= 0) {
                    int m;

                    for (m = 0; m < 4; m++) {
                        int lt = ct[m];

                        if (m == k)
                            continue;
                        if (lt < 0 || lt == 10 || lt == 0 || lt == 0x0f)
                            continue;
                        if (cc[m] < 0)
                            continue;
                        if (cc[m] == cc[k]) {
                            ocean_i = k;
                            land_i = m;
                            break;
                        }
                    }
                    if (ocean_i >= 0)
                        break;
                }
            }
            if (ocean_i < 0 || land_i < 0)
                continue;
            {
                int cont = cc[ocean_i];
                int cost = path_check_continent_link(cx[ocean_i],
                                                     cy[ocean_i], cont);
                int dir;
                int bit;
                int rev;
                int idx;
                int dx;
                int dy;
                int old;

                if (!worldgen_continent_link_allowed(cost))
                    continue;
                dx = cx[land_i] - cx[ocean_i];
                dy = cy[land_i] - cy[ocean_i];
                if (dx > 0)
                    dir = 2;
                else if (dx < 0)
                    dir = 4;
                else if (dy > 0)
                    dir = 3;
                else if (dy < 0)
                    dir = 1;
                else
                    dir = 1;
                bit = worldgen_link_bit(dir);
                rev = worldgen_link_rev_bit(dir);
                idx = worldgen_link_idx(bx, by);
                if (idx < 0)
                    continue;
                old = map_get_tile(base_x + bx, base_y + by);
                if (old < 0)
                    old = 0;
                map_set_tile(base_x + bx, base_y + by,
                             (uint8_t)(old | bit | rev));
            }
        }
        if (yield != 0)
            sys_yield();
    }
    return CIV_OK;
}

civ_error_t worldgen_build_ocean_links(int yield)
{
    /* Orig FUN_1028_162c @ 1028:162c (yield) -> Map_BuildOceanLinks: same
     * 0x14 x 0x0d grid, 8-dir variant with 5-step ocean-run check straight
     * from g_offMapBuf/g_segMapBuf (needs 2+ clear rows each step via
     * worldgen_ocean_run_ok), bits at +0x108 (+ reverse) with (0xb,3)
     * corner exception, edge fixups (OR 0xe0/0x0e, AND masks). Progress
     * 1000:29ea (0x108,...) + Sys_Yield per row when yield!=0. Callers
     * Map_LabelContinents @ 1028:0f01 + save_deserialize @ 10f8:01e9.
     * OS-free 1:1 port via map_get_tile, map_wrap_x, link_grid_w/h, idx,
     * bit/rev_bit, ocean_run_ok, sys_yield. Tables mapped to free window
     * (80,64) 20x13 (see build_continent_links). Per-step gate: 2-wide
     * ocean pair (both 10) => clear_rows 2 else 0, must be >=2. Start
     * requires ocean at block origin. Edge ORs LIVE, AND masks TBD ->
     * PARTIAL (graph LIVE, masks documented). No Win16, no globals. */
    const int base_x = (int)CIV_MAP_W;
    const int base_y = (int)CIV_MAP_H + (int)CIV_LINK_GRID_H + 1;
    int bx;
    int by;

    map_fill_rect(base_x, base_y, (unsigned)worldgen_link_grid_w(),
                  (unsigned)worldgen_link_grid_h(), 0);
    for (by = 0; by < worldgen_link_grid_h(); by++) {
        for (bx = 0; bx < worldgen_link_grid_w(); bx++) {
            int sx = bx * 4 + 1;
            int sy = by * 4 + 1;
            int st;
            int dir;

            if (sx < 0 || sx >= (int)CIV_MAP_W || sy < 0 ||
                sy >= (int)CIV_MAP_H)
                continue;
            st = map_get_tile(map_wrap_x(sx), sy);
            if (st != 10)
                continue;
            for (dir = 1; dir <= 8; dir++) {
                int dx = worldgen_dir_dx_at(dir - 1);
                int dy = worldgen_dir_dy_at(dir - 1);
                int s;
                int ok;

                /* (0xb,3) corner exception. */
                if (st == 0x0b && dir == 3)
                    continue;
                ok = 1;
                for (s = 0; s < 5; s++) {
                    int px = map_wrap_x(sx + dx * s);
                    int py = sy + dy * s;
                    int qx = map_wrap_x(px + 1);
                    int t1;
                    int t2;
                    int clear_rows;

                    if (py < 0 || py >= (int)CIV_MAP_H) {
                        ok = 0;
                        break;
                    }
                    if (px < 0 || px >= (int)CIV_MAP_W || qx < 0 ||
                        qx >= (int)CIV_MAP_W) {
                        ok = 0;
                        break;
                    }
                    t1 = map_get_tile(px, py);
                    t2 = map_get_tile(qx, py);
                    clear_rows = (t1 == 10 && t2 == 10) ? 2 : 0;
                    if (!worldgen_ocean_run_ok(clear_rows)) {
                        ok = 0;
                        break;
                    }
                }
                if (!ok)
                    continue;
                {
                    int bit = worldgen_link_bit(dir);
                    int rev = worldgen_link_rev_bit(dir);
                    int idx = worldgen_link_idx(bx, by);
                    int old;

                    if (idx < 0 || bit == 0)
                        continue;
                    old = map_get_tile(base_x + bx, base_y + by);
                    if (old < 0)
                        old = 0;
                    map_set_tile(base_x + bx, base_y + by,
                                 (uint8_t)(old | bit | rev));
                }
            }
        }
        if (yield != 0)
            sys_yield();
    }
    /* Edge fixups: OR 0xe0/0x0e (AND masks TBD). */
    for (by = 0; by < worldgen_link_grid_h(); by++) {
        int old_l;
        int old_r;

        old_l = map_get_tile(base_x + 0, base_y + by);
        if (old_l < 0)
            old_l = 0;
        map_set_tile(base_x + 0, base_y + by, (uint8_t)(old_l | 0x0e));
        old_r = map_get_tile(base_x + worldgen_link_grid_w() - 1,
                             base_y + by);
        if (old_r < 0)
            old_r = 0;
        map_set_tile(base_x + worldgen_link_grid_w() - 1, base_y + by,
                     (uint8_t)(old_r | 0xe0));
    }
    return CIV_OK;
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
    /* Orig FUN_1128_16d0 @ 1128:16d0 -> Path_CheckContinentLink: gate
     * inside Map_BuildContinentLinks @ 1028:12c6 for ocean(10)/land corner
     * pairs sharing one continent id, returns cost linking only when
     * cost != -1 && cost < 0x14 (via worldgen_continent_link_allowed).
     * Full BFS is Path_FindCorridor @ 1128:10de (16x16 window +-8, 0x100B
     * cost bytes, 256 ring queue, road&8 + fast-unit + tile_cost, sea/land
     * + occupancy gates, budgets 0x14 links / 999 goto / 0x12 entry).
     * OS-free PARTIAL port via map_get_tile: OOB (-1) -> 0x14 (no link),
     * continent < 0 -> -1 (unreachable), else 0 (link, cost 0 < 0x14).
     * Exact corridor cost TBD (TODO Orig 1128:10de BFS), but gate shape
     * (|dx|<8|| |dy|<8 else 0xffff via path_corridor_gate_ok in map.c)
     * + allowed check are LIVE. No Win16, no globals. */
    int t;

    if (continent < 0)
        return -1;
    t = map_get_tile(x, y);
    if (t < 0)
        return 0x14;
    return 0;
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
    /* Orig WorldGen_ProbeCustomFont @ 1060:0000 (civ.exe, x86:LE:16:Protected
     * Mode; ref unpacked/civ.exe read-only, NE Win3.x 832512 B, file off
     * ~0x25000: 6a 00 x3 + 68 90 01 (weight 400) + 6a 00 x3 + 6a 01 + 6a 04
     * + 6a 00 x2 + 6a 12 (height 0x12) + 6a 00 x2 + 9a CREATEFONT + 8b f0
     * SELECTOBJECT + 56 DELETEOBJECT probe; sole caller
     * WorldGen_ShowVideoCapsDialog @ 1060:01c1 GETDEVICECAPS dump +
     * Dlg_CreateTracked @ 1098:124f 0x94). PARTIAL: OS-free height/weight
     * gates below are LIVE 1:1 (BAD_DATA on drift, no fake-OK); Win16 GDI
     * tail stays STUB. OS-free LIVE: height 0x12 via
     * worldgen_probe_font_height + weight 400 via
     * worldgen_probe_font_weight (double-checked literal + helper, either
     * drift -> BAD_DATA). __far/logfont selector plumbing maps to no
     * out-param (probe only, HFONT stays GDI-side).
     * TODO: Orig CREATEFONT(h 0x12, weight 400) + SELECTOBJECT into dialog
     * DC + DELETEOBJECT -> SDL_ttf TBD -> stub CIV_ERR_UNSUPPORTED (no
     * fake-OK). No globals, no exit(). */
    if (worldgen_probe_font_height() != 0x12)
        return CIV_ERR_BAD_DATA;
    if (worldgen_probe_font_height() != 18)
        return CIV_ERR_BAD_DATA;
    if (worldgen_probe_font_weight() != 400)
        return CIV_ERR_BAD_DATA;
    if (worldgen_probe_font_weight() != 0x190)
        return CIV_ERR_BAD_DATA;
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
    /* Orig WorldGen_ShowVideoCapsDialog @ 1060:01c1 (civ.exe,
     * x86:LE:16:Protected Mode; ref unpacked/civ.exe read-only, NE Win3.x
     * 832512 B, file off ~0x251c2: GETDC + GETDEVICECAPS x13 pushes 56 6a
     * 08/0a/0c/0e/18/16/26/24/22/28/2a/58/5a + f7 c7 00 01 test (0x26&0x100)
     * -> war. 56 6a 68 / 56 6a 6a raster ext + RELEASEDC + 6a 41 68 94 00
     * Dlg_CreateTracked @ 1098:124f (0x94,0x41) + FILLRECT + Text_SetColor @
     * 1098:0e0b + SETRECT/OFFSETRECT + DispatchFont @ 11e8:0b40 (5/6) +
     * WorldGen_ProbeCustomFont @ 1060:0000 (0x12/400) + rows via
     * WorldGen_DrawVideoCapsRow @ 1060:005b (label+value 2-col) /
     * WorldGen_DrawVideoCapsFlag @ 1060:00d1 (enabled/disabled) +
     * Sys_WaitForInput @ 1038:0c1d modal + Dlg_DestroyTracked @ 1098:1612;
     * no callers = debug-only, thresholds LIVE in boot.c). PARTIAL: OS-free
     * gates below are LIVE 1:1 (BAD_DATA on drift, no fake-OK); Win16
     * GDI/dialog pump stays STUB. OS-free LIVE: 13-query table via
     * worldgen_video_caps_query_count/at (OOR -> -1) + raster-ext gate via
     * worldgen_video_caps_needs_raster_ext (0x100 set -> 1, else 0) + font
     * h 0x12 / weight 400 via worldgen_probe_font_height/weight +
     * probe-custom-font gate via worldgen_probe_custom_font (expects
     * UNSUPPORTED while GDI unbridged; BAD_DATA there propagates). __far/DC
     * /selector plumbing maps to explicit locals (no segment).
     * TODO: Orig GETDC/GETDEVICECAPS dump + Dlg_CreateTracked @ 1098:124f
     * host + DrawRow @ 1060:005b / DrawFlag @ 1060:00d1 + WaitForInput modal
     * -> SDL TBD -> stub CIV_ERR_UNSUPPORTED (no fake-OK). No globals. */
    static const int want[CIV_VIDEO_CAPS_QUERY_N] = {
        8, 10, 0x0c, 0x0e, 0x16, 0x18, 0x22, 0x24, 0x26, 0x28, 0x2a, 0x58,
        0x5a
    };
    int i;
    civ_error_t r;

    if (worldgen_video_caps_query_count() != CIV_VIDEO_CAPS_QUERY_N)
        return CIV_ERR_BAD_DATA;
    if (worldgen_video_caps_query_count() != 13)
        return CIV_ERR_BAD_DATA;
    for (i = 0; i < CIV_VIDEO_CAPS_QUERY_N; i++) {
        if (worldgen_video_caps_query_at(i) != want[i])
            return CIV_ERR_BAD_DATA;
    }
    if (worldgen_video_caps_query_at(-1) != -1)
        return CIV_ERR_BAD_DATA;
    if (worldgen_video_caps_query_at(CIV_VIDEO_CAPS_QUERY_N) != -1)
        return CIV_ERR_BAD_DATA;
    if (!worldgen_video_caps_needs_raster_ext(0x100))
        return CIV_ERR_BAD_DATA;
    if (worldgen_video_caps_needs_raster_ext(0))
        return CIV_ERR_BAD_DATA;
    if (worldgen_video_caps_needs_raster_ext(0x26) != 0)
        return CIV_ERR_BAD_DATA;
    if (worldgen_probe_font_height() != 0x12)
        return CIV_ERR_BAD_DATA;
    if (worldgen_probe_font_weight() != 400)
        return CIV_ERR_BAD_DATA;
    r = worldgen_probe_custom_font();
    if (r == CIV_ERR_BAD_DATA)
        return CIV_ERR_BAD_DATA;
    if (r != CIV_ERR_UNSUPPORTED)
        return CIV_ERR_BAD_DATA;
    return CIV_ERR_UNSUPPORTED;
}

/* --- Batch 2026-10-17 fala 4 (T): link-graph bounds --- */

int map_is_link_grid_in_bounds(int x, int y)
{
    /* Orig Map_IsLinkGridInBounds @ 1028:1acf (link graph, not tiles). */
    if (x < 0 || y < 0)
        return 0;
    if (x >= 0x14 || y >= 0x0D)
        return 0;
    return 1;
}
