/* World generation (random / earth / custom) -> SDL3.
 *
 * Ghidra anchors (civ.exe):
 *   Map_GenerateRandomOrLoad @ 1028:0000 (param==0: random gen via
 *     11e8:0dcf/0e19 + 1028:0945/1d15/066a passes; plays Sound_PlayId(4) =
 *     OPENING.wav at start. param!=0: HMEMCPY 64000B preset via 10b8:079d)
 *   FUN_1108_0243 @ 1108:0243 (Play on EARTH map load; from MainMenu dispatch)
 *   Custom editor: NewGame_Wizard steps 3/4 + Ctrl_* @ 1058:xxxx
 *     (RANDOMRADIOPROC @ 1058:06ab, RANDOMSTATICPROC @ 1058:081a,
 *      RANDOMUSERPROC @ 1058:0921, REGIONPROC @ 1058:09f6)
 *   Map windows: WdwMapOpen : wdwmap.c @ 1208:0af1 etc. (see map.h)
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

#endif /* CIVLIN_WORLDGEN_H */
