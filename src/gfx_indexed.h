/* CivLin indexed+palette -> SDL3 bridge (in-memory only).
 *
 * Palette fidelity is the whole point of this file: source pixels stay
 * indexed and the palette travels with them, so later palette animation
 * (Win16 SELECTPALETTE/ANIMATEPALETTE equivalents) remains possible.
 * Expansion to 32-bit RGBA happens only in RAM at load time; nothing is
 * ever written back to civ1win/ or unpacked/ (or anywhere on disk).
 *
 * Two paths:
 *   1. gfx_indexed_to_surface(): 8-bit SDL_Surface + SDL_Palette. Keeps
 *      indices intact on the surface; re-palettizing later = animation.
 *   2. gfx_indexed_to_texture(): convenience for direct rendering; expands
 *      through the palette into an RGBA texture in VRAM/RAM, indices kept
 *      in the source rsc_indexed_t for future re-uploads.
 */
#ifndef CIVLIN_GFX_INDEXED_H
#define CIVLIN_GFX_INDEXED_H

#include <SDL3/SDL.h>

#include "civ_common.h"
#include "rsc_loader.h"

/* X1 producer gate: rsc_get_indexed @ 1190:0a29 (ResMgr image payload
 * via rsc_record_data slice + rsc_decode_bitmap @ 1198:xxxx strict gate:
 * w/h > 0, bpp == 8, pal 1..255, every pixel index < pal_count) feeds
 * this bridge. Consumer enforces the same 8-bit + palette gate here
 * before touching SDL (8-bit only, w/h > 0, pixels != NULL, pal_count
 * 0..255 with 0 = no palette attached); palette path installs via
 * SDL_SetPaletteColors (SDL_CreateSurfacePalette + SetPaletteColors).
 * No behavior break: malformed producer output is rejected on both
 * sides, valid 8-bit + palette views flow unchanged to INDEX8.
 * Z2 probe below (gfx_probe_rsc_image, OS-free, no SDL) mirrors the
 * producer strict gate so rsc_dump can report *why* a slice was skipped
 * without touching SDL. Synthetic ok path (no assets, see rsc_dump.c +
 * video_present @ 10d0:0854 doc in video.h): 4x2 surface (R/G/B/white)
 * through gfx_indexed_to_surface + 64x64 palette gradient through
 * gfx_indexed_to_texture + SDL_RenderTexture/SDL_RenderPresent both pass
 * (incl. SDL_VIDEODRIVER=dummy); real RSC frames stay gated until the
 * carving lands. */
SDL_Surface *gfx_indexed_to_surface(const rsc_indexed_t *img);
SDL_Texture *gfx_indexed_to_texture(SDL_Renderer *ren, const rsc_indexed_t *img);

/* Z2 image-probe gate (OS-free, no SDL, no exit, no globals).
 * Mirrors rsc_decode_bitmap @ 1198:xxxx strict gate (read via Read tool,
 * not edited): u16 w LE @ +0, u16 h LE @ +2, u8 bpp @ +4 (must be 8),
 * u8 pal_count @ +5 (1..255), pal_count*3 RGB @ +6, then w*h indices with
 * every pixel < pal_count; bounds 6 + 3*pal + w*h <= size overflow-safe.
 * Returns CIV_OK when the slice would pass the strict gate; CIV_ERR_NOENT
 * on NULL data; CIV_ERR_TRUNCATED when size < 6 or the palette/pixel
 * tail is short; CIV_ERR_BAD_DATA on bpp != 8 or pal_count == 0;
 * CIV_ERR_RANGE on w == 0 || h == 0 or any pixel >= pal_count (RSC codes
 * are RANGE/BAD_MAGIC/TRUNCATED respectively; CIV mapping kept here).
 * w_out/h_out/bpp_out/pal_out are optional (NULL = skip).
 * Why dir 454/14/31/143/1 yields images_strict=0: none of the
 * rsc_record_data slices in the 5 CivData files presents a raw 8-bit
 * header at record start (w/h zero, or bpp != 8, or pal 0, or pixel OOR
 * at the probe) — the payload first bytes look like a compressed/RLE or
 * type-tagged frame (length/palette-offset prefix), not raw INDEX8, so
 * the strict gate correctly skips them (not a gate bug). Next carving
 * uses gfx_probe_head_hex (first-bytes hex into caller buffer, no asset
 * writes) to log head[0..15] per record, then Ghidra compare vs the
 * 1198 decoder. Orig: FUN_1198_xxxx @ 1198:xxxx + FUN_1190_0a29 @
 * 1190:0a29. */
civ_error_t gfx_probe_rsc_image(const uint8_t *data, size_t size,
                                uint32_t *w_out, uint32_t *h_out,
                                uint8_t *bpp_out, uint8_t *pal_out);

/* Z2 payload head sniffer (OS-free, no SDL, no exit, no globals).
 * Read-only carving helper for the images_strict=0 backlog: formats the
 * first n bytes of a record slice (clamped to size, capped at 32) as
 * upper-case hex "XX ..." into dst (cap bytes incl. NUL, truncated
 * safely via snprintf). Returns the would-be length (like
 * util_format_message @ 1000:2928 sibling), -1 on NULL dst/data or
 * cap == 0. Never touches civ1win//unpacked/ (caller passes a borrowed
 * slice), never writes assets. rsc_dump get_indexed[0] OOR plan: when
 * images_strict=0 the index-0 slice fails the probe above (not an 8-bit
 * image, skipped -> RANGE/OOR on the indexed view), so log
 * gfx_probe_head_hex(head, 16) per file + gfx_probe_rsc_image reason
 * instead of failing validation; use the hex to spot RLE/compression
 * magic before editing any decoder. Orig slice: FUN_1190_0a29 @
 * 1190:0a29 via rsc_record_data. */
int gfx_probe_head_hex(char *dst, size_t cap,
                       const uint8_t *data, size_t size, size_t n);

/* AA2 RLE tag constants (OS-free, no SDL).
 * Mirror of CIV_SAVE_RLL_LITERAL_FLAG/REPEAT_MAX in save.h @ 10f8:05da /
 * 10f8:040a (read via Read, not included: save.h pulls win16_compat.h,
 * gfx stays SDL + civ_common + rsc_loader only). Tag < 0x80 = repeat
 * next byte N times; tag >= 0x80 = copy N-0x80 literal bytes; 0x7f chunk
 * cap on encode. */
#define GFX_RLE_LITERAL_FLAG 0x80u
#define GFX_RLE_REPEAT_MAX 0x7fu

/* Orig: FUN_10f8_040a @ 10f8:040a RLLDecode (save_unpack core) + FUN_10f8_05da
 * @ 10f8:05da RLLEncode (save_pack core) -> gfx_decode_rle (CvPc payload
 * RLE expander; OS-free 1:1 port of save_rll_decode in save.c, read via
 * Read for reuse, not included).
 * Tag scheme (same as save_rll_*): tag < 0x80 = repeat next byte tag times
 * (tag 0 = no-op, kept 1:1); tag >= 0x80 = copy tag-0x80 literal bytes.
 * Vs raw gate: CvPc payloads in CivData1/2/3 (cnt 13/30/148) present
 * RLE/compressed frames at record start, NOT raw INDEX8 (probe shows w/h
 * zero or bpp != 8), so the raw gate below (gfx_probe_rsc_image /
 * rsc_decode_bitmap @ 1198:xxxx, 6 + 3*pal + w*h) correctly skips them;
 * expand via this RLE first, then re-probe the expanded bytes with the raw
 * gate (this function never calls the raw gate itself). Bounds: src is
 * borrowed (valid while container lives), dst is caller-owned; never reads
 * past src_len, never writes past dst_cap. Returns CIV_OK with *out_len =
 * expanded bytes; CIV_ERR_NOENT on NULL src/dst (NULL out_len = skip, kept
 * 1:1); CIV_ERR_TRUNCATED on short tag/value/literal tail or dst overrun
 * (matches save_rll_decode TRUNCATED shape). No SDL, no exit, no globals. */
civ_error_t gfx_decode_rle(const uint8_t *src, size_t src_len,
                           uint8_t *dst, size_t dst_cap, size_t *out_len);

/* Orig: FUN_1198_xxxx @ 1198:xxxx Rsc_DecodeBitmap -> gfx_decode_raw
 * (raw INDEX8 decoder, OS-free 1:1 port; no Win16, no SDL, no exit, no
 * globals). Payload layout (all LE, same as rsc_decode_bitmap in
 * rsc_loader.c, read via Read, not edited, + gfx_probe_rsc_image gate
 * above): u16 w LE @ +0, u16 h LE @ +2, u8 bpp @ +4 (must be 8), u8
 * pal_count @ +5 (1..255), pal_count*3 RGB @ +6, then w*h 8-bit indices;
 * bounds 6 + 3*pal + w*h <= size overflow-safe. Parity: identical field
 * order/bounds/pixel check to rsc_decode_bitmap; error mapping matches
 * gfx_probe_rsc_image (CIV_ERR_BAD_DATA on bpp != 8 or pal == 0, CIV
 * maps RSC BAD_MAGIC there; RANGE on w/h zero or pixel >= pal; TRUNCATED
 * on short header/palette/pixel tail; NOENT on NULL data/out). On success
 * fills *out as a borrowed view (pixels points into data, valid while the
 * container lives; palette copied, tail zeroed). RLE payloads must be
 * expanded via gfx_decode_rle first; this decodes raw bytes only. */
civ_error_t gfx_decode_raw(const uint8_t *data, size_t size,
                           rsc_indexed_t *out);

/* Orig: godpal.c 0xec RGB entries feeding FUN_1090_0488 @ 1090:0488
 * Palette_Init (LOGPALETTE 0x300 -> CREATEPALETTE -> DAT_1420_9966 @
 * 1420:9966, SELECTPALETTE/REALIZEPALETTE at each port present;
 * ANIMATEPALETTE via Palette_AnimateRange @ 1090:0376, see video.h).
 * OS-free table port: 0xec entries x 3 bytes (R,G,B, no alpha;
 * LOGPALETTE tail past 0xec stays zero/black). Byte payload not yet
 * dumped from godpal.c / DAT_1420_9966, so only the size is live (TODO
 * dump); entry fetch stays CIV_ERR_UNSUPPORTED until the dump lands (no
 * fake bytes emitted here). unpacked/civ.exe is present locally
 * (unpacked/ local-only, never committed, holds civ.exe + 5 RSC) and
 * contains the DAT_1420_9966 bytes, but the 0x2c4 dump has not been
 * extracted yet -> TODO dump, do not hardcode fake bytes here.
 * Ghidra dump steps (no fake data):
 *   1. Open unpacked/civ.exe in Ghidra 12.1.2 (CivLin.gpr, program "civ.exe").
 *   2. Go to DAT_1420_9966 @ 1420:9966 (Memory view / Symbol Tree).
 *   3. Select 0xec*3 = 0x2c4 bytes (236 entries x R,G,B, no alpha).
 *   4. Copy as C array / export bytes; cross-check vs godpal.c source
 *      (order + count 0xec, LOGPALETTE 0x300 tail past 0xec is zero).
 *   5. Paste as static const uint8_t kGodpal[0xec][3] in gfx_indexed.c
 *      (read-only table, no globals beyond file-static const) + wire
 *      gfx_godpal_get to bounds-check (NULL -> NOENT, index OOR ->
 *      RANGE) then copy rgb[3] and return CIV_OK.
 * No globals, no exit(). */
#define GFX_GODPAL_COUNT 0xec

/* Returns GFX_GODPAL_COUNT (0xec). OS-free 1:1 size port. */
size_t gfx_godpal_count(void);

/* Fetches one godpal entry (index 0..0xeb) into rgb[3]. NULL rgb reports
 * NOENT; index outside 0..0xeb reports RANGE; otherwise UNSUPPORTED
 * until the godpal.c / DAT_1420_9966 @ 1420:9966 byte dump lands (steps
 * above: 0xec*3 RGB dump, cross-check vs godpal.c, static const table,
 * then copy; no fake bytes here). SDL3 consumer is palette_apply_to_port
 * @ 1200:00a8/1090:0376 (see video.h) + palette_animate_range staging. */
civ_error_t gfx_godpal_get(int index, uint8_t rgb[3]);

#endif /* CIVLIN_GFX_INDEXED_H */
