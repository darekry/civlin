/* Hall of Fame stubs. Persistence format of famehall.civ TBD from
 * HoF_Load @ 1080:0023 disassembly (save_file_open/read/close flow). */
#include "halloffame.h"

#include <string.h>

civ_error_t hof_load(civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    int i;

    if (!slots)
        return CIV_ERR_NOENT;
    for (i = 0; i < CIV_HOF_SLOTS; i++)
        memset(&slots[i], 0, sizeof(slots[i]));
    /* TODO: HoF_Load @ 1080:0023 (famehall.civ via save_file_*). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t hof_save(const civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    if (!slots)
        return CIV_ERR_NOENT;
    /* TODO: HoF_Save @ 1080:0092. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t hof_show(const civ_hof_entry_t slots[CIV_HOF_SLOTS])
{
    if (!slots)
        return CIV_ERR_NOENT;
    /* TODO: HoF_Show @ 1080:0119 (dialog 0x94 + per-slot text). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t hof_try_insert(civ_hof_entry_t slots[CIV_HOF_SLOTS],
                           const civ_hof_entry_t *result, int *out_index)
{
    if (!slots || !result || !out_index)
        return CIV_ERR_NOENT;
    *out_index = -1;
    /* TODO: HoF_TryInsert @ 1080:0595 ordering + HoF_Show. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t hof_victory_ceremony(int level, int score)
{
    (void)level;
    (void)score;
    /* TODO: HoF_VictoryCeremony @ 1080:06ff (WINTUNE + pyramid frames). */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t city_show_top_five(void)
{
    /* TODO: City_ShowTopFive @ 1080:099b. */
    return CIV_ERR_UNSUPPORTED;
}
