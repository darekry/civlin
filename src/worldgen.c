/* Worldgen stubs. */
#include "worldgen.h"

civ_error_t worldgen_generate_random(void)
{
    /* TODO: Map_GenerateRandomOrLoad @ 1028:0000 param==0 (Sound_PlayId(4)
     * = OPENING, 11e8:0dcf/0e19 passes, 1028:0945/1d15/066a). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_load_preset(const uint8_t *image, size_t len)
{
    (void)image;
    (void)len;
    /* TODO: Map_GenerateRandomOrLoad param!=0 (HMEMCPY 64000B + tail). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_load_earth(void)
{
    /* TODO: FUN_1108_0243 earth-map load. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t worldgen_customize(void)
{
    /* TODO: wizard steps 3/4 + Ctrl_* @ 1058:xxxx. */
    return CIV_ERR_UNSUPPORTED;
}
