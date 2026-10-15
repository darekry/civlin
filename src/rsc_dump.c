/* CivLin RSC smoke test (local-only, never committed output).
 *
 * Usage: rsc_dump [file.rsc ...]
 *   Default (no args): the 5 known files under unpacked/.
 *
 * Does, in order:
 *   1. rsc_open() each file; print size, record count/types, image count.
 *      record_data()/get_indexed() are EXPECTED to return UNSUPPORTED in
 *      v1 (framing not yet decoded) — that is reported, not a failure.
 *   2. Synthetic palette-path test (no assets): 4x2 indexed image with a
 *      4-entry palette through gfx_indexed_to_surface(), verified by
 *      reading format/dimensions/pixels/palette back.
 *   3. One frame on screen: synthetic 64x64 palette gradient via
 *      gfx_indexed_to_texture() + SDL_RenderTexture, ~60 frames or until
 *      QUIT/key. Works under SDL_VIDEODRIVER=dummy (no crash = pass).
 *
 * Exit 0 iff all files parsed AND the synthetic tests pass. Never writes
 * to civ1win/ or unpacked/ (or anywhere on disk).
 */
#include <SDL3/SDL.h>

#include <stdio.h>
#include <string.h>

#include "gfx_indexed.h"
#include "rsc_loader.h"

static const char *kDefaultFiles[] = {
    "unpacked/civdata0.rsc",
    "unpacked/CIVDATA1.RSC",
    "unpacked/CIVDATA2.RSC",
    "unpacked/Civdata3.rsc",
    "unpacked/CIVDATA4.RSC",
};

static int dump_one(const char *path)
{
    rsc_file_t *f = NULL;
    rsc_error_t rc = rsc_open(path, &f);
    if (rc != RSC_OK) {
        printf("%s: open failed: %s\n", path, rsc_strerror(rc));
        return 1;
    }
    printf("%s: size=%zu records=%zu images=%zu\n", path, rsc_size(f),
           rsc_record_count(f), rsc_image_count(f));
    for (size_t i = 0; i < rsc_record_count(f); i++) {
        rsc_record_t rec;
        if (rsc_record_info(f, i, &rec) == RSC_OK)
            printf("  rec[%zu] type=%s entry=0x%x unknown_a=0x%08x unknown_b=0x%08x\n",
                   i, rec.type, rec.entry_offset, rec.unknown_a,
                   rec.unknown_b);
    }
    {
        const uint8_t *data = NULL;
        size_t size = 0;
        rsc_error_t dr = rsc_record_data(f, 0, &data, &size);
        printf("  record_data: %s (expected in v1)\n", rsc_strerror(dr));
    }
    {
        rsc_indexed_t img;
        rsc_error_t gr = rsc_get_indexed(f, 0, &img);
        printf("  get_indexed: %s (expected in v1)\n", rsc_strerror(gr));
    }
    rsc_close(f);
    return 0;
}

/* 4x2 synthetic indexed image: indices 0..3, palette R/G/B/white. */
static int test_surface_bridge(void)
{
    static const uint8_t kPix[8] = {0, 1, 2, 3, 3, 2, 1, 0};
    rsc_indexed_t img;
    SDL_Surface *surf = NULL;
    int failures = 0;

    memset(&img, 0, sizeof(img));
    img.width = 4;
    img.height = 2;
    img.bpp = 8;
    img.pal_count = 4;
    img.pal[0][0] = 255;
    img.pal[1][1] = 255;
    img.pal[2][2] = 255;
    img.pal[3][0] = img.pal[3][1] = img.pal[3][2] = 255;
    img.pixels = kPix;

    surf = gfx_indexed_to_surface(&img);
    if (!surf) {
        printf("synthetic surface: FAILED (%s)\n", SDL_GetError());
        return 1;
    }
    if (surf->format != SDL_PIXELFORMAT_INDEX8 || surf->w != 4 || surf->h != 2) {
        printf("synthetic surface: FAILED (format/w/h mismatch)\n");
        failures++;
    }
    {
        const uint8_t *px = (const uint8_t *)surf->pixels;
        if (memcmp(px, kPix, sizeof(kPix)) != 0) {
            printf("synthetic surface: FAILED (pixel mismatch)\n");
            failures++;
        }
    }
    {
        SDL_Palette *pal = SDL_GetSurfacePalette(surf);
        if (!pal || pal->ncolors < 4 || pal->colors[0].r != 255 ||
            pal->colors[1].g != 255 || pal->colors[2].b != 255 ||
            pal->colors[3].r != 255) {
            printf("synthetic surface: FAILED (palette mismatch)\n");
            failures++;
        }
    }
    SDL_DestroySurface(surf);
    if (!failures)
        printf("synthetic surface: ok (4x2 indexed + 4-entry palette)\n");
    return failures ? 1 : 0;
}

/* One frame: 64x64 palette gradient through the texture path. */
static int present_frame(SDL_Renderer *ren)
{
    static uint8_t kPix[64 * 64];
    rsc_indexed_t img;
    SDL_Texture *tex = NULL;

    memset(&img, 0, sizeof(img));
    img.width = 64;
    img.height = 64;
    img.bpp = 8;
    img.pal_count = 0; /* filled below */
    for (int i = 0; i < 64; i++) {
        img.pal[i][0] = (uint8_t)(i * 4);
        img.pal[i][1] = (uint8_t)(255 - i * 4);
        img.pal[i][2] = 128;
    }
    img.pal_count = 64;
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++)
            kPix[y * 64 + x] = (uint8_t)((x + y) % 64);
    img.pixels = kPix;

    tex = gfx_indexed_to_texture(ren, &img);
    if (!tex) {
        printf("synthetic texture: FAILED (%s)\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_RenderTexture(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
    SDL_DestroyTexture(tex);
    printf("synthetic texture: ok (64x64 palette gradient, 1 frame)\n");
    return 0;
}

int main(int argc, char *argv[])
{
    int rc = 0;
    int frames = 0;

    const char **files = kDefaultFiles;
    int nfiles = (int)(sizeof(kDefaultFiles) / sizeof(kDefaultFiles[0]));
    if (argc > 1) {
        files = (const char **)(argv + 1);
        nfiles = argc - 1;
    }

    for (int i = 0; i < nfiles; i++)
        rc |= dump_one(files[i]);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    rc |= test_surface_bridge();
    {
        SDL_Window *win = SDL_CreateWindow("CivLin - rsc smoke", 320, 200, 0);
        SDL_Renderer *ren = NULL;
        if (!win) {
            printf("CreateWindow failed: %s\n", SDL_GetError());
            SDL_Quit();
            return 1;
        }
        ren = SDL_CreateRenderer(win, NULL);
        if (!ren) {
            printf("CreateRenderer failed: %s\n", SDL_GetError());
            SDL_DestroyWindow(win);
            SDL_Quit();
            return 1;
        }
        rc |= present_frame(ren);
        /* Keep the frame up briefly; quit early on input. */
        while (frames < 60) {
            SDL_Event ev;
            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_EVENT_QUIT ||
                    ev.type == SDL_EVENT_KEY_DOWN)
                    frames = 60;
            }
            SDL_Delay(16);
            frames++;
        }
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
    }
    SDL_Quit();
    printf(rc == 0 ? "rsc_dump: PASS\n" : "rsc_dump: FAIL\n");
    return rc;
}
