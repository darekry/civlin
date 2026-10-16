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

/* Orig: FUN_10f8_040a @ 10f8:040a RLLDecode (via gfx_decode_rle above) +
 * FUN_1198_xxxx @ 1198:xxxx raw gate (via gfx_probe_rsc_image above) ->
 * gfx_rle_expand_cvpc (CvPc RLE-expand-then-probe, OS-free, no SDL).
 * Confirmed: CvPc payload BB DF 06 45... is binary RLE, not raw INDEX8
 * (raw probe on the slice reports BAD_DATA/RANGE: w/h zero or bpp != 8,
 * so the strict 6 + 3*pal + w*h gate correctly skips it; expand first,
 * then re-probe). Expands src[0..src_len) via gfx_decode_rle (tag < 0x80
 * repeat next byte N times / tag >= 0x80 copy N-0x80 literals, save_rll
 * @ 10f8:05da/040a) into caller-owned dst[0..dst_cap), then re-probes
 * the expanded bytes via gfx_probe_rsc_image for w/h/bpp/pal (filled
 * even when the expanded bytes still fail the raw gate; caller logs the
 * reason via civ_strerror separately). Bounds: never reads past src_len,
 * never writes past dst_cap; dst is caller-owned (malloc/free
 * caller-side, e.g. rsc_dump 4 MiB scratch), src is borrowed (valid
 * while the container lives). Returns expanded byte count (>= 0, fits
 * int) or -1 on NULL src/dst, dst_cap == 0, decode failure (TRUNCATED
 * dst overrun / short tag tail), or expanded len > INT_MAX. No SDL, no
 * exit, no globals, read-only assets. */
int gfx_rle_expand_cvpc(const uint8_t *src, size_t src_len,
                        uint8_t *dst, size_t dst_cap,
                        uint32_t *w_out, uint32_t *h_out,
                        uint8_t *bpp_out, uint8_t *pal_out);

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
 * Ghidra dump steps (CivLin.gpr, Ghidra 12.1.2, program "civ.exe",
 * DAT_1420_9966 @ 1420:9966, no fake data):
 *   1. Open CivLin.gpr in Ghidra 12.1.2 (File -> Open Project ->
 *      CivLin.gpr, local-only, never committed), double-click "civ.exe"
 *      (unpacked/civ.exe import, NE Win3.x Borland C++ 1991).
 *   2. In Symbol Tree / Go To (G) open DAT_1420_9966 @ 1420:9966
 *      (segment 1420 offset 0x9966; Listing shows LOGPALETTE source).
 *   3. In Listing select exactly 0xec*3 = 0x2c4 bytes at 1420:9966
 *      (236 entries x R,G,B, no alpha; selection length reads 0x2c4).
 *   4. Right-click -> Copy Special -> C Array (or Window -> Bytes ->
 *      Export 0x2c4 bytes, then xxd -i); cross-check vs godpal.c source
 *      (order + count 0xec, LOGPALETTE 0x300 tail past 0xec is zero).
 *   5. Paste as static const uint8_t kGodpal[0xec][3] in gfx_indexed.c
 *      (read-only table, no globals beyond file-static const) + wire
 *      gfx_godpal_get to bounds-check (NULL -> NOENT, index OOR ->
 *      RANGE) then copy rgb[3] and return CIV_OK.
 * No globals, no exit(). */
#define GFX_GODPAL_COUNT 0xec

/* AD2 LOGPALETTE anchor (WinGDI LOGPALETTE behind Palette_Init @
 * 1090:0488 -> CREATEPALETTE -> DAT_1420_9966 @ 1420:9966, see
 * gfx_godpal_search below). On-disk LE bytes: palVersion 0x0300 LE =
 * 00 03, palNumEntries 0x00EC LE = EC 00, so the sliding-window magic
 * is 00 03 EC 00. Followed by 0xec PALETTEENTRY structs (4 B each:
 * R,G,B,flags) = 0x3B4 total with the 4-B header; godpal.c RGB source
 * is 0xec*3 = 0x2c4 (no flags, LOGPALETTE 0x300 tail past 0xec stays
 * zero). Search looks for the 4-B anchor only (RGB plausibility alone
 * is useless: every byte is < 256, always true); the 0x3B4/0x2c4 dump
 * + Ghidra DAT_1420_9966 order/count + "godpal.c" string-neighbour
 * check happen after the hit, before any static table is emitted. */
#define GFX_GODPAL_MAGIC_LEN 4
#define GFX_GODPAL_ENTRY_SIZE 4
#define GFX_GODPAL_LOGPAL_SIZE (4 + 0xec * 4)
#define GFX_GODPAL_RGB_SIZE (0xec * 3)

/* Returns GFX_GODPAL_COUNT (0xec). OS-free 1:1 size port. */
size_t gfx_godpal_count(void);

/* Fetches one godpal entry (index 0..0xeb) into rgb[3]. NULL rgb reports
 * NOENT; index outside 0..0xeb reports RANGE; otherwise UNSUPPORTED
 * until the godpal.c / DAT_1420_9966 @ 1420:9966 byte dump lands (no
 * fake bytes here). AD2 wiring (still stub until bytes pasted): run
 * gfx_godpal_search("unpacked/civ.exe", &off) (local-only, read-only);
 * on CIV_OK dump 0x2c4 RGB bytes at off+4 (or 0x3B4 LOGPALETTE and drop
 * the flag bytes) via dd/xxd, cross-check vs Ghidra DAT_1420_9966 @
 * 1420:9966 order/count + "godpal.c" string neighbour + LOGPALETTE
 * 0x300 zero tail past 0xec, paste as static const uint8_t
 * kGodpal[0xec][3] in gfx_indexed.c (read-only, file-static const),
 * then wire this to bounds-check + memcpy rgb[3] + CIV_OK. SDL3
 * consumer is palette_apply_to_port @ 1200:00a8/1090:0376 (see video.h)
 * + palette_animate_range staging. */
civ_error_t gfx_godpal_get(int index, uint8_t rgb[3]);

/* AC2 RLE variants + godpal search (OS-free, no SDL, no exit, no globals).
 * Context: CvPc head BB DF 06 45... expand FAILs under the current RLL
 * tag (<0x80 repeat N / >=0x80 literal N-0x80, save_rll @ 10f8:040a/05da
 * via gfx_decode_rle above). BB >= 0x80 as literal-59 needs a 59 B tail;
 * slice len from rsc_record_data is unknown (~500 B plausible) but FAIL
 * suggests dst overrun or tag mismatch, so the tag hypothesis may be
 * inverted and neighbouring codecs must be tried: EDI LZSS2 (installer
 * flags LSB-first, 1=literal, 0=match 2 B, 4 K ring 0x20, curpos
 * 4096-18, tools/unpack_edi.py, read via Read, not edited) vs RLL vs
 * RLL-inv vs raw-shifted. Rules: C17, SDL only for surface/texture
 * (never in these funcs), no exit(), no globals, borrowed src vs
 * caller-owned dst, read-only assets. gfx_decode_rle/raw/probe/head_hex
 * above are unchanged (parity with rsc_decode_bitmap @ 1198:xxxx). */

/* Try-fail reason (why a variant FAILs; shared by gfx_try_rll /
 * gfx_try_rll_inv / gfx_try_lzss_edi / gfx_try_lzss_1k / gfx_try_lzss_2k
 * below; *fail_out NULL = skip).
 * NONE = success (or NOENT arg failure, no decode attempted).
 * TRUNC_SRC = ran out of src: RLL repeat tag without value byte; LZSS
 *   flag byte / literal byte missing.
 * TAG_OVERRUN = tag's claimed run past src end: RLL literal n past
 *   src_len; LZSS match 2 B tail short (cpos+2 > src_len).
 * DST_OVERRUN = repeat/literal/match run past dst_cap.
 * AD2: human-readable form via gfx_try_fail_str below (OS-free, no SDL):
 *   NONE -> "NONE", TRUNC_SRC -> "TRUNC_SRC", TAG_OVERRUN ->
 *   "TAG_OVERRUN", DST_OVERRUN -> "DST_OVERRUN", other -> "UNKNOWN".
 *   gfx_try_rll and gfx_try_lzss_edi already report *fail_out; the
 *   string helper reuses #1 for all five variants (RLL/INV/LZSS/1K/2K). */
#define GFX_TRY_FAIL_NONE 0
#define GFX_TRY_FAIL_TRUNC_SRC 1
#define GFX_TRY_FAIL_TAG_OVERRUN 2
#define GFX_TRY_FAIL_DST_OVERRUN 3

/* Orig: none (AD2 reason strings, OS-free 1:1 over GFX_TRY_FAIL_* above,
 * no Win16, no SDL) -> gfx_try_fail_str. Maps the try-fail reason to a
 * static string ("NONE"/"TRUNC_SRC"/"TAG_OVERRUN"/"DST_OVERRUN", else
 * "UNKNOWN") for caller logging (rsc_dump head16 + expand_best log).
 * No SDL, no exit, no globals (string literals only), C17. */
const char *gfx_try_fail_str(int fail);

/* Expand-best method id (gfx_rle_expand_best below; *method_out NULL =
 * skip). NONE = no variant succeeded. Order on ties (AG1): RLL >
 * RLL_INV > LZSS_EDI > LZSS_1K > LZSS_2K > GIF > RAW_SHIFTED. AD2 log
 * names: RLL/INV/LZSS/GIF/RAW0/2/4/8 where GIF = GFX_EXPAND_GIF winner
 * (raw INDEX8 blob rebuilt from the GIF frame, len = 6+3*pal+w*h) and
 * RAW0/2/4/8 = RAW_SHIFTED with raw_off 0/2/4/8 (winning raw offset);
 * AG1 adds LZSS_1K/LZSS_2K (GFX_EXPAND_LZSS_1K/_2K winners, same rebuilt
 * raw-blob shape, raw_off_out = 0); gfx_expand_method_str maps the id to
 * "NONE"/"RLL"/"RLL_INV"/"LZSS_EDI"/"RAW_SHIFTED"/"GIF"/"LZSS_1K"/
 * "LZSS_2K" (else "UNKNOWN"), the log line appends "+<off>" for RAW
 * winners (e.g. "RAW_SHIFTED+2", short alias "RAW2") and "GIF+<off>"
 * carries the GIF magic offset (0 or 8) via raw_off_out.
 * Orig: none (dispatcher ids; AE1 adds GIF for the CvPc BB DF backlog;
 * AG1 adds 1K/2K for the CvPc BB DF 86k backlog: RLL/INV/LZSS-EDI/GIF/
 * RAW all miss, so smaller-window LZSS trials sit before GIF). */
#define GFX_EXPAND_NONE 0
#define GFX_EXPAND_RLL 1
#define GFX_EXPAND_RLL_INV 2
#define GFX_EXPAND_LZSS_EDI 3
#define GFX_EXPAND_RAW_SHIFTED 4
#define GFX_EXPAND_GIF 5
#define GFX_EXPAND_LZSS_1K 6
#define GFX_EXPAND_LZSS_2K 7
#define GFX_EXPAND_BITS_MSB 8
#define GFX_EXPAND_BITS_BIAS2 9
#define GFX_EXPAND_BITS_BIAS4 10
#define GFX_EXPAND_BIAS2 GFX_EXPAND_BITS_BIAS2
#define GFX_EXPAND_BIAS4 GFX_EXPAND_BITS_BIAS4
#define GFX_EXPAND_BIT_MSB GFX_EXPAND_BITS_MSB

/* Orig: none (AD2 method strings + AG1 1K/2K + AH1 BITS_MSB/BIAS2/BIAS4,
 * OS-free, no SDL) -> gfx_expand_method_str. Maps GFX_EXPAND_* to a static
 * string for the expand_best log line (see gfx_rle_expand_best_ex):
 * "NONE"/"RLL"/"RLL_INV"/"LZSS_EDI"/"RAW_SHIFTED"/"GIF"/"LZSS_1K"/
 * "LZSS_2K"/"BITS_MSB"/"BITS_BIAS2"/"BITS_BIAS4" (else "UNKNOWN").
 * BIAS2/BIAS4 aliases ("BIAS2"/"BIAS4" substrings) are covered: the
 * BITS_BIAS2/BITS_BIAS4 strings contain "BIAS2"/"BIAS4" so substring
 * checks for either form pass. No SDL, no exit, no globals, C17. */
const char *gfx_expand_method_str(int method);

/* EDI LZSS2 ring constants (OS-free 1:1 from tools/unpack_edi.py, read
 * via Read, not edited: 4 K ring prefilled 0x20, curpos starts at
 * 4096-18 = 4078, matchpos = ((x1 & 0xF0) << 4) | x0,
 * matchlen = (x1 & 0x0F) + 3). AG1 adds the smaller-window siblings:
 * 1K ring 1024 prefilled 0x20 curpos 1024-18 = 1006 (Borland/early
 * variant hypothesis for the CvPc BB DF 86k backlog), 2K ring 2048
 * prefilled 0x20 curpos 2048-18 = 2030; same flags-LSB-first + bias +3
 * code shape, matchpos masked to the window (see gfx_try_lzss_1k/_2k). */
#define GFX_LZSS_RING_SIZE 4096u
#define GFX_LZSS_RING_FILL 0x20u
#define GFX_LZSS_START_POS 4078u
#define GFX_LZSS_1K_RING_SIZE 1024u
#define GFX_LZSS_1K_START_POS 1006u
#define GFX_LZSS_2K_RING_SIZE 2048u
#define GFX_LZSS_2K_START_POS 2030u

/* Orig: FUN_10f8_040a @ 10f8:040a RLLDecode (same tag as gfx_decode_rle
 * / save_rll @ 10f8:05da/040a: tag < 0x80 repeat next byte N times,
 * tag >= 0x80 copy N-0x80 literals; tag 0 = no-op but still consumes
 * the value byte, kept 1:1) -> gfx_try_rll (current-tag try with
 * reason, OS-free, no SDL). Bounds: never reads past src_len, never
 * writes past dst_cap; src borrowed, dst caller-owned. Returns CIV_OK
 * with *out_len = expanded bytes and *fail_out = NONE; CIV_ERR_NOENT
 * on NULL src/dst (NULL out_len/fail_out = skip, *fail_out = NONE when
 * present); CIV_ERR_TRUNCATED with *fail_out = TRUNC_SRC (repeat value
 * missing) / TAG_OVERRUN (literal tail short) / DST_OVERRUN (run past
 * dst_cap). AD2: reason strings via gfx_try_fail_str(*fail_out)
 * ("NONE"/"TRUNC_SRC"/"TAG_OVERRUN"/"DST_OVERRUN"). No SDL, no exit,
 * no globals. */
civ_error_t gfx_try_rll(const uint8_t *src, size_t src_len,
                        uint8_t *dst, size_t dst_cap, size_t *out_len,
                        int *fail_out);

/* Orig: FUN_10f8_040a @ 10f8:040a RLLDecode inverted-tag hypothesis ->
 * gfx_try_rll_inv (OS-free, no SDL). Same bounds/shape as gfx_try_rll
 * but tags flipped: tag < 0x80 = copy tag literal bytes; tag >= 0x80 =
 * repeat next byte (tag-0x80) times (0x80 = no-op, still consumes the
 * value byte, mirror of the tag-0 rule). Tests the BB DF 06 45...
 * inversion: BB >= 0x80 as repeat-59 of DF needs 1 B tail (plausible
 * in a ~500 B slice) vs literal-59 needing 59 B. Same NOENT/TRUNCATED
 * + fail_out mapping as gfx_try_rll; AD2 reason strings reuse
 * gfx_try_fail_str (see gfx_try_rll). No SDL, no exit, no globals. */
civ_error_t gfx_try_rll_inv(const uint8_t *src, size_t src_len,
                            uint8_t *dst, size_t dst_cap, size_t *out_len,
                            int *fail_out);

/* Installer EDI LZSS2 variant (tools/unpack_edi.py, read via Read, not
 * edited) -> gfx_try_lzss_edi (OS-free 1:1 port, no Win16, no SDL).
 * Stream: flags LSB-first, 1 = literal (1 B), 0 = match (2 B:
 * matchpos = ((x1 & 0xF0) << 4) | x0, matchlen = (x1 & 0x0F) + 3);
 * 4 K ring prefilled 0x20, curpos 4096-18 = 4078 (stack buf, no
 * globals; 4096 B fits the caller frame). Bounds: never reads past
 * src_len (flag/literal/match tails), never writes past dst_cap
 * (literal + each match byte, overlap-safe forward copy). Returns
 * CIV_OK with *out_len; CIV_ERR_NOENT on NULL src/dst (NULL
 * out_len/fail_out = skip, *fail_out = NONE when present);
 * CIV_ERR_TRUNCATED with *fail_out = TRUNC_SRC (flag/literal byte
 * missing) / TAG_OVERRUN (match 2 B tail short) / DST_OVERRUN
 * (literal/match past dst_cap). AD2: same fail_out domain as
 * gfx_try_rll, same string helper gfx_try_fail_str(*fail_out) reused
 * (no second helper). No SDL, no exit, no globals. */
civ_error_t gfx_try_lzss_edi(const uint8_t *src, size_t src_len,
                             uint8_t *dst, size_t dst_cap, size_t *out_len,
                             int *fail_out);

/* AG1 smaller-window LZSS siblings (no Orig; Borland/early-variant
 * hypothesis for the CvPc BB DF 86k backlog: RLL/INV/LZSS-EDI/GIF/RAW
 * all miss, so trials beyond the EDI 4K ring are needed) ->
 * gfx_try_lzss_1k (OS-free 1:1 code shape over tools/unpack_edi.py,
 * read via Read, not edited, smaller ring only, no Win16, no SDL).
 * Stream: flags LSB-first, 1 = literal (1 B), 0 = match (2 B:
 * matchpos = (((x1 & 0xF0) << 4) | x0) & 1023, matchlen =
 * (x1 & 0x0F) + 3); 1K ring prefilled 0x20, curpos 1024-18 = 1006
 * (stack buf, no globals; 1024 B fits the caller frame). Same 2-byte
 * match packing as EDI (8 low bits in x0, top 4 in x1 bits 4..7, len
 * bias +3 in x1 bits 0..3); the smaller window is the 10-bit mask (top
 * 2 offset bits truncated, so streams using offsets >= 1024 decode
 * differently from EDI -- that divergence is the trial). Bounds: never
 * reads past src_len (flag/literal/match tails), never writes past
 * dst_cap (literal + each match byte, overlap-safe forward copy).
 * Returns CIV_OK with *out_len; CIV_ERR_NOENT on NULL src/dst (NULL
 * out_len/fail_out = skip, *fail_out = NONE when present);
 * CIV_ERR_TRUNCATED with *fail_out = TRUNC_SRC (flag/literal byte
 * missing) / TAG_OVERRUN (match 2 B tail short) / DST_OVERRUN
 * (literal/match past dst_cap). Same fail_out domain + string helper
 * gfx_try_fail_str as gfx_try_rll/gfx_try_lzss_edi. No SDL, no exit,
 * no globals, C17. */
civ_error_t gfx_try_lzss_1k(const uint8_t *src, size_t src_len,
                            uint8_t *dst, size_t dst_cap, size_t *out_len,
                            int *fail_out);

/* AG1 smaller-window LZSS sibling (no Orig; same backlog as
 * gfx_try_lzss_1k above) -> gfx_try_lzss_2k (OS-free, no Win16, no
 * SDL). Stream: flags LSB-first, 1 = literal (1 B), 0 = match (2 B:
 * matchpos = (((x1 & 0xF0) << 4) | x0) & 2047, matchlen =
 * (x1 & 0x0F) + 3); 2K ring prefilled 0x20, curpos 2048-18 = 2030
 * (stack buf, no globals; 2048 B fits the caller frame). Same 2-byte
 * match packing as EDI (len bias +3); the smaller window is the 11-bit
 * mask (top offset bit truncated vs EDI). Same NOENT/TRUNCATED +
 * fail_out mapping as gfx_try_lzss_1k/edi; strings via
 * gfx_try_fail_str. No SDL, no exit, no globals, C17. */
civ_error_t gfx_try_lzss_2k(const uint8_t *src, size_t src_len,
                            uint8_t *dst, size_t dst_cap, size_t *out_len,
                            int *fail_out);

/* AH1 Borland GR_Pic* bit-reader variants (no Orig; CvPc BB DF 86k slice
 * TAG_OVERRUN on RLL/INV/LZSS-EDI/1K/2K/GIF/RAW, so custom Borland
 * GR_Pic* bit orders are trialled; gr_pic.c LoadGifPicture @ 1420:2372 /
 * GR_PicRead @ 1420:2300 / PicDecompress @ 1420:23ce, see title.h).
 * All three share the EDI 4K ring (prefilled 0x20, curpos 4096-18 =
 * 4078, stack buf, no globals; 4096 B fits the caller frame) and the
 * 2-byte match packing (8 low offset bits in x0, top 4 in x1 bits 4..7,
 * len nibble in x1 bits 0..3), overlap-safe forward copy, raw LZSS code
 * stream framing (trailing incomplete flag/code at src end reports TRUNC,
 * dispatcher treats it as a miss). Difference vs EDI is the flag
 * bit order and/or the length bias:
 *   gfx_try_bits_msb: flags MSB-first (vs LSB-first EDI), 1 = literal
 *     (1 B), 0 = match (2 B: matchpos = ((x1 & 0xF0) << 4) | x0,
 *     matchlen = (x1 & 0x0F) + 3); ring 4K 0x20, bias +3. Bitbuf MSB
 *     (bit = (bitbuf >> 7) & 1, bitbuf <<= 1, 8 flags per byte).
 *   gfx_try_bits_bias2: same 4K 0x20 ring, flags MSB-first, matchlen =
 *     (x1 & 0x0F) + 2 (Borland threshold-1 variant vs +3 threshold-2).
 *   gfx_try_bits_bias4: same 4K 0x20 ring, flags MSB-first, matchlen =
 *     (x1 & 0x0F) + 4 (Borland threshold-3 variant).
 * Bounds: never reads past src_len (flag/literal/match tails), never
 * writes past dst_cap (literal + each match byte). Returns CIV_OK with
 * *out_len; CIV_ERR_NOENT on NULL src/dst (NULL out_len/fail_out = skip,
 * *fail_out = NONE when present); CIV_ERR_TRUNCATED with *fail_out =
 * TRUNC_SRC (flag/literal byte missing) / TAG_OVERRUN (match 2 B tail
 * short) / DST_OVERRUN (literal/match past dst_cap). Same fail_out
 * domain + string helper gfx_try_fail_str as gfx_try_rll/edi. No SDL,
 * no exit, no globals, C17. Orig: none (AH1 Borland hypothesis). */
civ_error_t gfx_try_bits_msb(const uint8_t *src, size_t src_len,
                             uint8_t *dst, size_t dst_cap, size_t *out_len,
                             int *fail_out);
civ_error_t gfx_try_bits_bias2(const uint8_t *src, size_t src_len,
                               uint8_t *dst, size_t dst_cap, size_t *out_len,
                               int *fail_out);
civ_error_t gfx_try_bits_bias4(const uint8_t *src, size_t src_len,
                               uint8_t *dst, size_t dst_cap, size_t *out_len,
                               int *fail_out);

/* Orig: FUN_1198_xxxx @ 1198:xxxx raw gate (via gfx_decode_raw above,
 * parity with rsc_decode_bitmap, read via Read, not edited) ->
 * gfx_try_raw_shifted (raw header at caller offset, OS-free, no SDL).
 * Payload layout identical to gfx_decode_raw (u16 w LE @ +off, u16 h LE
 * @ +off+2, u8 bpp @ +off+4 == 8, u8 pal @ +off+5 in 1..255, RGB +
 * indices; 6 + 3*pal + w*h <= size-off overflow-safe, every pixel <
 * pal). Offset skips prefixes: 0 = slice start, 2/4 = length/palette
 * prefix hypothesis, 8 = 8-byte type tag (e.g. CvD1CvMM/CvPc) with w/h
 * at +8. Borrowed view (pixels into data, palette copied tail-zeroed).
 * Returns NOENT on NULL data/out, TRUNCATED when offset > size (or the
 * shifted tail is short), else the raw BAD_DATA/RANGE/TRUNCATED gate.
 * No SDL, no exit, no globals. */
civ_error_t gfx_try_raw_shifted(const uint8_t *data, size_t size,
                                size_t offset, rsc_indexed_t *out);

/* --- AE1 GIF-LZW + PK (segment 1420 gr_pic.c siblings, OS-free) ---
 * Context: CvPc BB DF 86k slice TAG_OVERRUN on RLL/INV/LZSS-EDI.
 * Strong lead: GR_PicRead / LoadGifPicture @ 1420:2300/2372 +
 * PicDecompress @ 1420:23ce siblings (title.h gr_pic.c). Civ1 likely
 * GIF, not RLE. BB DF... could be GIF header after 8B tag? GIF magic
 * "GIF87a/89a" ASCII 47 49 46 38 37/39 61 -- BB DF != 47 49 ("GI"), so
 * no GIF at +0; GIF at +8 (skip 8B CvD tag e.g. "CvD1CvMM"/CvPc prefix)
 * may hit. PKZIP local header PK\x03\x04 = 50 4B 03 04 -- BB DF != 50
 * 4B ("PK"), so likely not PK (stub, no fake). LZW: dict 4096, clear/eoi
 * codes, code width min+1..12 (min 2..8 per GIF spec, max 12). Rules:
 * C17, no SDL in these funcs, no exit(), no globals, borrowed src vs
 * caller-owned dst, read-only assets. */
#define GFX_LZW_DICT_SIZE 4096u
#define GFX_LZW_MAX_BITS 12u
#define GFX_LZW_MIN_CODE_MIN 2u
#define GFX_LZW_MIN_CODE_MAX 8u
#define GFX_GIF_MAGIC_LEN 6u
#define GFX_GIF_RAW_HEAD_MAX (6u + 256u * 3u)

/* Orig: gr_pic.c LZW table sibling of PicDecompress @ 1420:23ce /
 * LoadGifPicture @ 1420:2372 (title.h) -> gfx_lzw_dict_t (AE1 OS-free).
 * prefix[i] = parent code (0xFFFF = root), suffix[i] = trailing byte;
 * clear_code = 1<<min, eoi_code = clear+1, next_code = first free slot,
 * code_width = current bits (min+1..12), min_code_size = GIF LZW min
 * (2..8). Stack-owned (12 KiB), never global. */
typedef struct gfx_lzw_dict {
    uint16_t prefix[4096];
    uint8_t suffix[4096];
    uint16_t clear_code;
    uint16_t eoi_code;
    uint16_t next_code;
    uint8_t code_width;
    uint8_t min_code_size;
} gfx_lzw_dict_t;

/* Orig: PicDecompress @ 1420:23ce table-init sibling (gr_pic.c via
 * LoadGifPicture @ 1420:2372 / GR_PicRead @ 1420:2300, see title.h) ->
 * gfx_lzw_init (AE1 OS-free 1:1 init, no Win16, no SDL). Sets
 * clear = 1<<min, eoi = clear+1, next = clear+2, width = min+1;
 * prefix[*] = 0xFFFF, suffix[i] = (uint8_t)i for i < clear (single-pixel
 * roots). Returns CIV_ERR_NOENT on NULL dict; CIV_ERR_RANGE when
 * min_code_size < 2 or > 8 (GIF spec range; max width stays 12 per
 * GFX_LZW_MAX_BITS, dict stays 4096); CIV_OK otherwise. No SDL, no
 * exit, no globals, C17. */
civ_error_t gfx_lzw_init(gfx_lzw_dict_t *dict, uint8_t min_code_size);

/* Orig: PicDecompress @ 1420:23ce LZW core sibling (gr_pic.c via
 * LoadGifPicture @ 1420:2372, see title.h) -> gfx_lzw_decode (AE1
 * OS-free port, no Win16, no SDL). LSB-first GIF code stream
 * (src[0..src_len) contiguous sub-block payload, min_code_size 2..8):
 * clear resets dict/width/next (via gfx_lzw_init shape), eoi ends the
 * stream (trailing bits ignored), else dict walk via prefix/suffix with
 * a 4096-B pixel stack (KwKwK code==next case handled), width grows
 * past (1<<width) up to 12, overlap-safe. Bounds: never reads past
 * src_len*8 bits (short tail -> TRUNCATED), never writes past dst_cap
 * (each output byte checked -> TRUNCATED, same DST_OVERRUN shape as
 * gfx_try_rll). Returns CIV_OK with *out_len = decoded bytes (NULL
 * out_len = skip); CIV_ERR_NOENT on NULL src/dst; CIV_ERR_RANGE on min
 * out of 2..8; CIV_ERR_TRUNCATED on empty stream, short code tail, or
 * dst overrun (*out_len stays 0); CIV_ERR_BAD_DATA on invalid code (>
 * next and not clear/eoi, or first code >= clear). Clean bitstream
 * exhaustion with op > 0 (missing eoi, tolerant for CvPc slices) is
 * CIV_OK. No SDL, no exit, no globals, C17. */
civ_error_t gfx_lzw_decode(const uint8_t *src, size_t src_len,
                           uint8_t min_code_size,
                           uint8_t *dst, size_t dst_cap, size_t *out_len);

/* Orig: GR_PicRead @ 1420:2300 / LoadGifPicture @ 1420:2372 frame-header
 * sibling (gr_pic.c, see title.h) -> gfx_gif_parse (AE1 OS-free port, no
 * Win16, no SDL). Parses GIF87a/89a at data[0..size): 6B magic
 * ("GIF87a"/"GIF89a"), 7B LSD (u16 w LE @ +6, u16 h LE @ +8, packed @
 * +10 with GCT flag 0x80 + N = bits 0..2 -> pal 2^(1+N)), GCT
 * pal*3 RGB when flagged, then 0x21 extensions (incl. GCE 0x21 0xF9,
 * skipped as label + sub-blocks to 0 terminator) until first 0x2C
 * image descriptor (10B: sep + left/top LE16 + iw/ih LE16 + packed with
 * LCT flag 0x80 + interlace 0x40); LCT pal*3 when flagged (frame pal
 * wins over GCT); interlaced (0x40) -> UNSUPPORTED (no fake
 * de-interlace); no image before trailer/end -> BAD_DATA. Then 1B LZW
 * min (must be 2..8, else RANGE) + sub-blocks (len byte N + N bytes to
 * 0 terminator; empty -> TRUNCATED). Full mode (px_buf != NULL):
 * LZW-decodes the sub-block chain (malloc'd flat copy + gfx_lzw_decode
 * with gfx_lzw_init reset, freed before return) into
 * px_buf[0..px_cap) requiring iw*ih <= px_cap overflow-safe, sets
 * *out as borrowed-from-px_buf view (pixels = px_buf, valid while the
 * caller buffer lives; palette copied, tail zeroed, bpp = 8). Header
 * mode (px_buf == NULL, px_cap ignored): validates the full structure
 * incl. sub-block bounds + LZW min but skips the decode, fills w/h/bpp
 * /pal/palette with pixels = NULL (sizing probe for gfx_try_gif).
 * Palette: GCT/LCT entries 2..256; 256-entry tables -> UNSUPPORTED
 * (rsc_indexed_t pal_count is u8 max 255, 0 = no palette, so 256 is
 * unrepresentable without a fake truncation). w/h come from the image
 * descriptor (frame dims; LSD canvas ignored except for GCT skip).
 * Returns CIV_ERR_NOENT on NULL data/out; CIV_ERR_TRUNCATED on short
 * header/LSD/GCT/descriptor/LCT/LZW-min/sub-block tail or px_cap short
 * for iw*ih; CIV_ERR_BAD_DATA on bad magic/packed mismatch/no image/no
 * palette; CIV_ERR_RANGE on w/h zero or min out of 2..8 or pixel
 * overflow; CIV_ERR_UNSUPPORTED on interlace or 256-entry pal (no
 * fake). No SDL, no exit, no globals, C17. */
civ_error_t gfx_gif_parse(const uint8_t *data, size_t size,
                          uint8_t *px_buf, size_t px_cap,
                          rsc_indexed_t *out);

/* Orig: LoadGifPicture @ 1420:2372 slice dispatcher sibling (gr_pic.c,
 * see title.h) -> gfx_try_gif (AE1 OS-free dispatcher, no SDL).
 * Tries GIF magic ("GIF87a"/"GIF89a", 47 49 46...) at offsets {0, 8}:
 * 0 = slice start, 8 = skip 8B CvD tag (e.g. "CvD1CvMM" 8B family tag;
 * CvPc BB DF 86k slice head BB DF != 47 49 at +0, so +0 misses and +8
 * is the live hypothesis). Per offset: header-mode gfx_gif_parse for
 * w/h/pal sizing -> raw_len = 6+3*pal+w*h overflow-safe fits dst_cap ->
 * full gfx_gif_parse decoding pixels directly into dst+6+3*pal ->
 * header+palette filled in dst[0..] (u16 w LE, u16 h LE, bpp 8, pal) ->
 * re-probed via gfx_probe_rsc_image for w/h/bpp/pal outs. Dst holds the
 * rebuilt raw INDEX8 blob (same 6+3*pal+w*h shape as RLL/LZSS winners,
 * so gfx_rle_expand_best_ex longest-wins + refill + re-probe stay
 * uniform). Src borrowed, dst caller-owned. Returns the raw blob len
 * (>= 0, fits int) or -1 on NULL src/dst, dst_cap == 0, no GIF magic at
 * 0/8, parse/decode failure, raw_len > dst_cap, or len > INT_MAX (no
 * fake bytes ever left behind on miss). w/h/bpp/pal outs optional
 * (NULL = skip; filled from the winning re-probe, mirroring
 * gfx_rle_expand_cvpc). No SDL, no exit, no globals, C17. */
int gfx_try_gif(const uint8_t *src, size_t src_len,
                uint8_t *dst, size_t dst_cap,
                uint32_t *w_out, uint32_t *h_out,
                uint8_t *bpp_out, uint8_t *pal_out);

/* Orig: none (PKZIP local-header hypothesis for the CvPc BB DF backlog;
 * gr_pic.c siblings above are GIF, not ZIP, so this stays a stub) ->
 * gfx_try_pk (AE1 OS-free stub, no SDL). PK magic reasoning: local
 * header PK\x03\x04 = 50 4B 03 04, empty archive PK\x05\x06, spanned
 * PK\x07\x08 -- slice head BB DF != 50 4B ("PK") at +0 and no PK magic
 * is expected at +8 either (8B CvD tag family, not ZIP), so likely not
 * PK; no inflate is attempted here (no fake output, dst never touched,
 * outs never filled). Same int-dispatcher shape as gfx_try_gif for a
 * future longest-wins slot: always returns -1 (NULL src/dst and
 * dst_cap == 0 also -1). No SDL, no exit, no globals, C17. */
int gfx_try_pk(const uint8_t *src, size_t src_len,
               uint8_t *dst, size_t dst_cap,
               uint32_t *w_out, uint32_t *h_out,
               uint8_t *bpp_out, uint8_t *pal_out);

/* AG1 Huffman/LZW-beyond-GIF hypothesis (no Orig; CvPc BB DF 86k slice
 * misses RLL/INV/LZSS-EDI/LZSS-1K/LZSS-2K/GIF/RAW) ->
 * gfx_try_huff_stub (OS-free stub, no SDL, no fake). Why a stub: a
 * Huffman (or raw LZW/Huffman hybrid outside GIF) decoder needs its
 * code-length table from the payload head (counts per bit-length +
 * symbol order, or an explicit tree dump) before any bit can be
 * decoded; BB DF 06 45... has no proven table at +0/+2/+4/+8 yet (no
 * plausible 256-symbol length vector or tree header carved), so any
 * decode attempt now would emit fake bytes. This stub therefore never
 * touches dst, never fills outs, and always returns -1 (NULL src/dst
 * and dst_cap == 0 also -1), same int-dispatcher shape as gfx_try_gif /
 * gfx_try_pk for a future longest-wins slot. TODO (needs payload-head
 * carving first): log head hex via gfx_probe_head_hex, look for a
 * length-vector/tree candidate (e.g. count bytes summing to a sane
 * alphabet, min/max length 1..16), then wire a real table-driven
 * decoder here. No SDL, no exit, no globals, C17. */
int gfx_try_huff_stub(const uint8_t *src, size_t src_len,
                      uint8_t *dst, size_t dst_cap,
                      uint32_t *w_out, uint32_t *h_out,
                      uint8_t *bpp_out, uint8_t *pal_out);

/* Orig: godpal.c 0xec RGB entries feeding FUN_1090_0488 @ 1090:0488
 * Palette_Init (LOGPALETTE 0x300 -> CREATEPALETTE -> DAT_1420_9966 @
 * 1420:9966, see gfx_godpal_get above) -> gfx_godpal_search (AD2
 * implemented, read-only file scan, no writes, no SDL, no fake table).
 * Magic reasoning (WinGDI LOGPALETTE): WORD palVersion 0x0300 LE =
 * bytes 00 03, WORD palNumEntries 0x00EC LE = bytes EC 00, so the LE
 * anchor is 00 03 EC 00 (GFX_GODPAL_MAGIC_LEN = 4), followed by 0xec
 * PALETTEENTRY structs (R,G,B,flags, 4 B each = 0x3B4 with header;
 * godpal.c RGB source is 0xec*3 = 0x2c4, flags dropped, LOGPALETTE
 * 0x300 tail past 0xec stays zero). RGB-triple plausibility alone is
 * useless (every byte is < 256, always true), so the anchor is
 * required; the 0x3B4/0x2c4 dump + Ghidra DAT_1420_9966 @ 1420:9966
 * order/count + "godpal.c" string-neighbour check happen after the hit,
 * before any static const kGodpal table is emitted (never here).
 * Scan (unpacked/civ.exe local-only, never committed, never written;
 * caller passes the path, e.g. "unpacked/civ.exe"): fopen(path,"rb") +
 * 4 KiB sliding window with 3-B overlap (magic straddling the chunk
 * edge is still caught), fread chunks, bounds-checked, no writes.
 * Returns CIV_OK with *offset_out = file offset of the 00 03 EC 00
 * anchor on hit; CIV_ERR_NOENT with *offset_out = -1 (when present) on
 * clean miss (scanned to EOF, no anchor); CIV_ERR_NOENT with
 * *offset_out untouched on NULL/empty path (NULL offset_out = skip,
 * existence check only); CIV_ERR_IO with *offset_out untouched on
 * fopen/fread failure. No SDL, no exit, no globals, C17. */
civ_error_t gfx_godpal_search(const char *path, long *offset_out);

/* Orig: none (dispatcher over the AC2 variants above + AE1 GIF + AG1
 * 1K/2K + AH1 BITS_MSB/BIAS2/BIAS4; re-probe via gfx_probe_rsc_image @
 * 1198:xxxx gate) -> gfx_rle_expand_best (OS-free, no SDL). Tries RLL
 * (gfx_try_rll) then RLL_INV then LZSS_EDI then LZSS_1K then LZSS_2K
 * then BITS_MSB then BITS_BIAS2 then BITS_BIAS4 then GIF (gfx_try_gif
 * offsets {0,8}, rebuilt raw blob len 6+3*pal+w*h) then raw-shifted
 * offsets {0,2,4,8} (gfx_probe_rsc_image pass = candidate, len =
 * size-off when it fits dst_cap), picks the longest expanded/candidate
 * len (ties keep the earlier method, order RLL > INV > LZSS_EDI >
 * LZSS_1K > LZSS_2K > BITS_MSB > BITS_BIAS2 > BITS_BIAS4 > GIF > RAW),
 * refills dst with the winning bytes (re-runs the winning RLL/LZSS/BITS/
 * GIF decode; memcpys the winning raw slice; RAW leaves no RLE bytes
 * behind), then re-probes the winning bytes via gfx_probe_rsc_image for
 * w/h/bpp/pal (filled even on gate failure for caller logging, mirror
 * of gfx_rle_expand_cvpc). PK (gfx_try_pk) and HUFF (gfx_try_huff_stub)
 * are NOT trialled here (stubs, always -1: BB DF != 50 4B; Huffman
 * needs its code-length table from the payload head first). Src
 * borrowed, dst caller-owned. Returns the winning byte count (>= 0,
 * fits int) with *method_out = GFX_EXPAND_* and *raw_off_out = winning
 * offset (0 for RLL/INV/LZSS-EDI/1K/2K/BITS, GIF magic offset 0/8 for
 * GIF, slice offset for RAW; NULL outs = skip); -1 on NULL src/dst,
 * dst_cap == 0, no variant succeeds, or len > INT_MAX. AG1 log names
 * are RLL/INV/LZSS/LZSS1K/LZSS2K/GIF/RAW0/2/4/8 plus AH1
 * BITS_MSB/BITS_BIAS2/BITS_BIAS4 (see gfx_expand_method_str +
 * gfx_rle_expand_best_ex log line); per-method fail reasons flow through
 * the _ex variant below (GIF as ok/miss, 1K/2K/BITS as TRY_FAIL log
 * tokens). No SDL, no exit, no globals, read-only assets. */
int gfx_rle_expand_best(const uint8_t *src, size_t src_len,
                        uint8_t *dst, size_t dst_cap,
                        int *method_out, size_t *raw_off_out,
                        uint32_t *w_out, uint32_t *h_out,
                        uint8_t *bpp_out, uint8_t *pal_out);

/* Orig: none (AD2 extended dispatcher + AE1 GIF slot + AG1 1K/2K
 * slots + AH1 BITS_MSB/BIAS2/BIAS4 slots, OS-free, no SDL) ->
 * gfx_rle_expand_best_ex (best method + per-method fail log). Same trial
 * order, tie-break, refill, and re-probe as gfx_rle_expand_best above:
 * RLL -> RLL_INV -> LZSS_EDI -> LZSS_1K -> LZSS_2K -> BITS_MSB ->
 * BITS_BIAS2 -> BITS_BIAS4 -> GIF (gfx_try_gif {0,8}, rebuilt raw blob)
 * -> RAW {0,2,4,8} (borrowed src, caller-owned dst, no side buffer:
 * trials share the caller dst, winner refilled at the end; order RLL >
 * INV > LZSS_EDI > LZSS_1K > LZSS_2K > BITS_MSB > BITS_BIAS2 > BITS_BIAS4
 * > GIF > RAW, longest wins).
 * Additionally reports:
 *   *fail_rll_out/_inv/_lzss = GFX_TRY_FAIL_* for the RLL/INV/LZSS_EDI
 *     trials (NONE on that trial's success; NONE when no decode was
 *     attempted; NULL = skip; strings via gfx_try_fail_str). AH1 keeps
 *     this signature (no new fail_out params): the 1K/2K/BITS fail reasons
 *     live in the log line only as LZSS1K:<fail>/LZSS2K:<fail>/
 *     BITS_MSB:<fail>/BITS_BIAS2:<fail>/BITS_BIAS4:<fail> (same TRY_FAIL
 *     domain, strings via gfx_try_fail_str; BITS_BIAS2/BITS_BIAS4 contain
 *     the BIAS2/BIAS4 substrings).
 *   GIF trial status lives in the log line only as GIF:ok/GIF:miss
 *     (gfx_try_gif int dispatcher: ok = rebuilt raw blob candidate,
 *     miss = no GIF magic at 0/8 or parse/decode failure; PK stub and
 *     HUFF stub are never trialled: BB DF != 50 4B; Huffman needs its
 *     code-length table from the payload head first; always -1, no log
 *     token).
 *   RAW0/2/4/8 per-offset probe results (gfx_probe_rsc_image gate:
 *     CIV_OK = candidate, else BAD_DATA/RANGE/TRUNCATED gate reason;
 *     skipped offsets with off > src_len or cand > dst_cap are logged
 *     as "skip") live in the log line only via civ_strerror (RAW fails
 *     are probe-gate codes, not TRY_FAIL, so no fail_out for them).
 *   log_buf/log_cap = caller-provided one-line summary (NULL or cap 0
 *     = skip; always NUL-terminated when cap > 0, snprintf-truncated):
 *     "best=RLL len=123 RLL:NONE INV:TAG_OVERRUN LZSS:TRUNC_SRC
 *      LZSS1K:TAG_OVERRUN LZSS2K:NONE BITS_MSB:NONE BITS_BIAS2:TAG_OVERRUN
 *      BITS_BIAS4:DST_OVERRUN GIF:miss RAW0:bad data RAW2:skip ..."
 *     where best is RLL/RLL_INV/LZSS_EDI/LZSS_1K/LZSS_2K/BITS_MSB/
 *     BITS_BIAS2/BITS_BIAS4/GIF (GFX_EXPAND_GIF; raw_off_out carries the
 *     0/8 magic offset) or RAW0/RAW2/RAW4/RAW8 (RAW_SHIFTED + winning
 *     raw_off; NONE when no variant succeeds) and len is the winning byte
 *     count (-1 when none). Signature unchanged by AE1/AG1/AH1 (no new
 *     fail_out; GIF + 1K/2K + BITS status is log-only). Returns the same
 *     winning count or -1 as gfx_rle_expand_best (len > INT_MAX = -1).
 *     All out params optional (NULL = skip). No SDL, no exit, no globals,
 *     C17, read-only assets. */
int gfx_rle_expand_best_ex(const uint8_t *src, size_t src_len,
                            uint8_t *dst, size_t dst_cap,
                            int *method_out, size_t *raw_off_out,
                            uint32_t *w_out, uint32_t *h_out,
                            uint8_t *bpp_out, uint8_t *pal_out,
                            int *fail_rll_out, int *fail_inv_out,
                            int *fail_lzss_out,
                            char *log_buf, size_t log_cap);

/* --- AF1 BMP (MPLOGO.BMP 380x97x1, OS-free) ---
 * Context: MPLOGO.BMP has NO filename string in the binary — NE resource
 * or RSC frame, NOT a file load (see title.h). The Linux port loads the
 * file explicitly by path (unpacked/MPLOGO.BMP or civ1win/MPLOGO.BMP,
 * read-only, never writes) via gfx_bmp_load_indexed below. Confirmed
 * file(1): PC bitmap, Windows 3.x format, 380 x 97 x 1, image size 4656,
 * cbSize 4718, bits offset 62. Byte dump (unpacked/MPLOGO.BMP, read-only):
 * file magic 42 4D "BM", fsize 4718, reserved 0,0, pixel off 62; DIB size
 * 40 (BITMAPINFOHEADER), w 380 (0x17C LE), h 97 (0x61 LE, positive =
 * bottom-up), planes 1, bpp 1, compression 0 (BI_RGB), image size 4656,
 * xppm=yppm=2834, clrused=0/clrimp=0 (derive 2 entries); palette 2xRGBQUAD
 * at +54: entry0 00 00 00 00 (black, B,G,R,reserved) entry1 FF FF FF 00
 * (white); pixels at +62, row stride 48 B (((380*1+31)/32)*4), 48*97=4656
 * matches the image-size field. Bit order MSB-first (bit 7 = leftmost),
 * rows bottom-up, each row padded to 4 B. Rules: C17, no SDL in these
 * funcs (SDL only for surface/texture above), no exit(), no globals,
 * borrowed src vs caller-owned dst, read-only assets (fopen "rb" only in
 * gfx_bmp_load_indexed, never writes). Parity: gfx_bmp_to_indexed fills
 * the same rsc_indexed_t shape as rsc_decode_bitmap @ 1198:xxxx (w/h,
 * bpp 8, pal_count 2, palette copied tail-zeroed, pixels into caller
 * buffer); rsc_loader is NOT edited (mirror gate only). */

/* Orig: none (Linux-port explicit-path helper; MPLOGO.BMP has NO filename
 * string in civ.exe — NE resource or RSC frame, NOT a file load) ->
 * gfx_bmp_t (AF1 OS-free BMP view, no Win16, no SDL). width/height are
 * absolute (height strips the top-down sign); bpp is the source bpp
 * (AF1 supports 1); pal_count is the decoded palette entries (2 for
 * 1-bpp); pal holds RGB (reserved byte dropped, tail zeroed); pixels is
 * the borrowed pixel-array start (data+pixel_offset, valid while the
 * file buffer lives); row_stride is the padded bytes per row
 * (((w*bpp+31)/32)*4); pixel_offset is the file-header off field;
 * top_down is 1 when the DIB height was negative, else 0 (MPLOGO.BMP
 * h=+97 so bottom-up, top_down=0). */
typedef struct gfx_bmp {
    uint32_t width;
    uint32_t height;
    uint16_t bpp;
    uint16_t pal_count;
    uint8_t pal[256][3];
    const uint8_t *pixels;
    size_t row_stride;
    size_t pixel_offset;
    int top_down;
} gfx_bmp_t;

/* Orig: none (same Linux-port note as gfx_bmp_t) -> gfx_bmp_parse (AF1
 * OS-free 1:1 port, no Win16, no SDL). Validates BITMAPFILEHEADER 14 B
 * (magic 42 4D "BM" @ +0, fsize u32 LE @ +2, reserved @ +6/+8 ignored,
 * pixel off u32 LE @ +10) + BITMAPINFOHEADER 40 B (dib size u32 LE @ +14
 * must be 40, w i32 LE @ +18, h i32 LE @ +22 signed, planes u16 LE @ +26
 * must be 1, bpp u16 LE @ +28, compression u32 LE @ +30 must be 0 BI_RGB,
 * image size/xppm/yppm/clrused/clrimp @ +34..+50 bounds only) + palette
 * (2xRGBQUAD B,G,R,reserved at +14+dib_size, 8 B, before pixel off) +
 * pixels (stride*((w*bpp+31)/32)*4, total stride*abs(h) from pixel off to
 * size). LE16/32 reads are manual bytes (no Win16). Borrowed view
 * (pixels into data, palette copied tail-zeroed). Returns CIV_ERR_NOENT
 * on NULL data/out; CIV_ERR_TRUNCATED when size < 14, short DIB/palette/
 * pixel tail, or fsize field > size; CIV_ERR_BAD_DATA on bad magic, dib
 * size != 40 handled as UNSUPPORTED (see below), planes != 1, clrused
 * not 0/2 for 1-bpp, pixel off out of range; CIV_ERR_RANGE on w <= 0 or
 * h == 0 or stride/total overflow; CIV_ERR_UNSUPPORTED on dib size !=
 * 40, bpp != 1, or compression != 0 (no fake decode). No SDL, no exit,
 * no globals, C17, read-only. */
civ_error_t gfx_bmp_parse(const uint8_t *data, size_t size, gfx_bmp_t *out);

/* Orig: none (same Linux-port note) -> gfx_bmp_to_indexed (AF1 OS-free
 * 1:1 port, no Win16, no SDL). Unpacks a parsed 1-bpp BMP (e.g. MPLOGO
 * 380x97) into caller-owned px_buf[0..px_cap) as w*h 8-bit indices
 * (MSB-first: bit 7 = leftmost pixel; bottom-up rows flipped unless
 * top_down; per-row 4 B padding skipped via row_stride). Palette: copies
 * the BMP entries (B,G,R -> R,G,B; MPLOGO.BMP confirmed black 00 00 00 /
 * white FF FF FF via byte dump) when pal_count >= 2, else defaults to
 * black/white (no fake third color). Fills *out as the rsc_decode_bitmap
 * @ 1198:xxxx shape (width/height, bpp 8, pal_count 2, palette copied
 * tail-zeroed, pixels = px_buf valid while the caller buffer lives).
 * Returns CIV_ERR_NOENT on NULL bmp/px_buf/out; CIV_ERR_UNSUPPORTED when
 * bmp bpp != 1; CIV_ERR_RANGE on w/h zero; CIV_ERR_TRUNCATED when
 * px_cap < w*h or the borrowed pixel tail is short. No SDL, no exit, no
 * globals, C17. */
civ_error_t gfx_bmp_to_indexed(const gfx_bmp_t *bmp, uint8_t *px_buf,
                                size_t px_cap, rsc_indexed_t *out);

/* Orig: none (same Linux-port note) -> gfx_bmp_load_indexed (AF1 file
 * loader, read-only, no SDL). fopen(path,"rb") + fseek/ftell size +
 * malloc + fread the whole file (MPLOGO.BMP 4718 B), gfx_bmp_parse, then
 * malloc w*h indices + gfx_bmp_to_indexed; frees the file buffer before
 * return so only *px_buf_out survives (caller frees with free(),
 * *img_out->pixels aliases it, valid until freed; palette copied).
 * Never writes assets (rb only). Returns CIV_ERR_NOENT on NULL/empty
 * path or NULL outs (*px_buf_out = NULL when present); CIV_ERR_IO on
 * fopen/fseek/ftell/fread failure; CIV_ERR_NOMEM on malloc failure;
 * otherwise the parse/convert code passthrough (TRUNCATED/BAD_DATA/
 * RANGE/UNSUPPORTED). No SDL, no exit, no globals, C17. */
civ_error_t gfx_bmp_load_indexed(const char *path, rsc_indexed_t *img_out,
                                   uint8_t **px_buf_out);

/* --- AJ1 font glyph surface + CvPc custom codec (FNT + GR_Pic*) ---
 * Context (read-only, no asset writes): FNT faces via
 * Font_EnumFamiliesInit @ 1008:0e99 (civfonts.fon, see boot.h font_fnt_bits /
 * font_render_glyph @1008:0e99 dfBitsOffset, read via Read, not edited) plus
 * the CvPc BB DF 86k backlog (slice len 86857, head16 BB DF 06 45..., best
 * NONE: all RLL/LZSS/GIF/RAW/BITS miss, every RLL/LZSS/BITS trial reports
 * TAG_OVERRUN, GIF miss, RAW probe miss). Rules: C17, SDL only for the
 * surface func below (never in the custom stub / decoders), no exit(), no
 * globals, borrowed src/bits vs caller-owned dst/surface, read-only assets.
 * Parity: gfx_decode_rle/raw/probe/lzw/gif/bits below are unchanged (no
 * break, no reorder of gfx_rle_expand_best_ex). */

/* Orig: Font_EnumFamiliesInit @ 1008:0e99 FNT glyph-bits side via
 * font_render_glyph @1008:0e99 dfBitsOffset (see boot.h font_fnt_bits /
 * font_render_glyph, read via Read, not edited) -> font_glyph_to_surface
 * (AJ1 OS-free bit expand + SDL bridge port, malloc + bit expand, no
 * Win16). Input is FNT 1-bpp bits via font_render_glyph @1008:0e99
 * dfBitsOffset: w/h + bits MSB-first stride=(w+7)/8 (tight 1-bpp rows,
 * bit 7 = leftmost, same repack as font_render_glyph dst). Output is an
 * SDL surface 8-bit + 2-entry palette black/white via the
 * gfx_indexed_to_surface path (indices 0 = black 00 00 00, 1 = white
 * FF FF FF; palette travels on the surface for later animation). Bits are
 * borrowed (valid while the FNT image / font_render_glyph dst lives, never
 * freed here); the returned surface is caller-owned (caller frees with
 * SDL_DestroySurface, NULL on failure). Returns NULL on NULL bits, w <= 0,
 * h <= 0, w*h overflow, or malloc/SDL failure (no exit, no globals).
 * Parity: bit order matches font_fnt_bits (AH2, MSB-first, see boot.h) and
 * gfx_bmp_to_indexed MSB-first unpack; gfx_decode_rle/raw/probe/lzw/gif/
 * bits parity kept (no break). C17, SDL only for this surface func, no
 * exit(), no globals. */
SDL_Surface *font_glyph_to_surface(const uint8_t *bits, int w, int h);

/* Orig: FUN_1198_xxxx @ 1198:xxxx Rsc_DecodeBitmap CvPc custom codec doc
 * (AJ1 doc-only, no decode, no fake decode). Observation (read-only, no
 * asset writes): CvPc BB DF 86k slice (slice len 86857, head16 BB DF 06
 * 45...) reports best NONE under gfx_rle_expand_best_ex: all
 * RLL/LZSS/GIF/RAW/BITS miss (RLL/RLL_INV/LZSS_EDI/LZSS_1K/LZSS_2K/BITS_MSB/
 * BITS_BIAS2/BITS_BIAS4 all TAG_OVERRUN, GIF miss at offsets {0,8}, RAW
 * probe miss at offsets {0,2,4,8}). Conclusion: document as custom Borland
 * GR_Pic* beyond LZSS (gr_pic.c LoadGifPicture @ 1420:2372 / GR_PicRead @
 * 1420:2300 / PicDecompress @ 1420:23ce family, see title.h; not EDI LZSS2,
 * not GIF LZW, not RLL), not a gate bug. TODO needs Ghidra decompile
 * FUN_1198_xxxx body via analyzeHeadless (open CivLin.gpr program civ.exe,
 * decompile FUN_1198_xxxx, compare the BB DF head handling vs the tried
 * codecs) before any real decoder is wired here; no fake decode emitted.
 * Keep gfx_rle_expand_best_ex order (RLL > RLL_INV > LZSS_EDI > LZSS_1K >
 * LZSS_2K > BITS_MSB > BITS_BIAS2 > BITS_BIAS4 > GIF > RAW; PK/HUFF/custom
 * stubs untrialled) so the NONE log line stays comparable. No SDL, no
 * exit, no globals, C17. */

/* Orig: none (AJ1 placeholder for the future custom codec above) ->
 * gfx_try_custom_stub (OS-free stub, no SDL). Placeholder for future
 * custom codec (custom Borland GR_Pic* beyond LZSS, see CvPc custom codec
 * doc above): always -1, same int-dispatcher shape as gfx_try_gif /
 * gfx_try_pk / gfx_try_huff_stub for a future longest-wins slot in
 * gfx_rle_expand_best_ex (currently untrialled: keep
 * gfx_rle_expand_best_ex order, PK/HUFF/custom never trialled). TODO
 * (needs FUN_1198_xxxx body via analyzeHeadless first): CvPc BB DF 86k
 * slice (slice len 86857, head16 BB DF 06 45..., best NONE, TAG_OVERRUN all
 * on RLL/INV/LZSS-EDI/1K/2K/BITS) shows no proven table/header at +0/+2/
 * +4/+8 yet, so any decode now would emit fake bytes; this stub therefore
 * never touches dst, never fills w/h/bpp/pal outs, and always returns -1
 * (NULL src/dst and dst_cap == 0 also -1, no fake). No SDL, no exit, no
 * globals, C17. */
int gfx_try_custom_stub(const uint8_t *src, size_t src_len,
                        uint8_t *dst, size_t dst_cap,
                        uint32_t *w_out, uint32_t *h_out,
                        uint8_t *bpp_out, uint8_t *pal_out);

/* --- Chain P4 render+text (2026-10-16): godpal 0xec range/flag ---
 * Anchors (civ.exe, x86:LE:16:Protected Mode), batch-decompiled this pass:
 *   Palette_AnimateRange @ 1090:0376 (if (0xec < first+count) count =
 *     0xec - first; then RGB triples + flag byte per entry, then
 *     ANIMATEPALETTE over 0xec; callers MainPort_ApplyPalette @ 1200:00a8
 *     opcode 1 + Anim_AdvanceFrames @ 10b0:0461 timed 1/0xe + Push @
 *     1090:015e / Pop @ 1090:02d2 / Zero @ 1090:0670 / Set @ 1090:06fd).
 *   Palette_Init @ 1090:0488 (LOGPALETTE 0x300 + 0xec entries zeroed +
 *     CREATEPALETTE -> DAT_1420_9966 @ 1420:9966 HPALETTE handle word).
 *   ZeroGodPalette @ 1090:0670 (flags = 0 over [first, first+count)) vs
 *     Set @ 1090:06fd (flags = DAT_1420_17a6 == 0 ? 1 : 0 over the range).
 *   DAT_1420_9966 @ 1420:9966 (HPALETTE BSS handle, zeros at load: Ghidra
 *     read 16 B zeros, not a static RGB table; unpacked/civ.exe
 *     (832512 B NE Win3.x) has no static LOGPALETTE anchor 00 03 EC 00 —
 *     the 0xec RGB payload is built at runtime via AnimateRange from
 *     art/anim sources, so gfx_godpal_get stays UNSUPPORTED with no fake
 *     bytes; range/flag math below is LIVE).
 * OS-free 1:1 LIVE below (no Win16, no SDL, no globals); the RGB table
 * itself stays STUB until a runtime capture (not a static dump) lands. */

/* Orig Palette_AnimateRange @ 1090:0376 0xec cap -> gfx_godpal_range_len.
 * 1:1: first < 0 || count <= 0 || first >= 0xec -> 0; else
 * min(count, 0xec - first). No globals. */
int gfx_godpal_range_len(int first, int count);

/* Orig same + Zero/Set @ 1090:0670/06fd flag byte ->
 * gfx_godpal_flag_for_mono. 1:1 DAT_1420_17a6 == 0 (color/VGA) ? 1 : 0
 * (Zero always clears; Set follows mono). No globals. */
int gfx_godpal_flag_for_mono(int mono);

#endif /* CIVLIN_GFX_INDEXED_H */
