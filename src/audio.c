/* Audio: ID table + normalization implemented; SDL playback is a stub. */
#include "audio.h"

#include <stddef.h>

civ_error_t audio_init(void)
{
    /* TODO: Sound_MciOpen @ 1150:001c equiv + SDL3 audio device. */
    return CIV_ERR_UNSUPPORTED;
}

civ_error_t audio_play(const char *wav_name)
{
    (void)wav_name;
    /* TODO: Sound_PlayFile @ 1150:00a0 -> SDL3 (load WAV from unpacked/). */
    return CIV_ERR_UNSUPPORTED;
}

int sound_normalize_id(int id)
{
    /* Sound_PlayId @ 1150:014f..016d. */
    if (id >= 0x13 && id <= 0x20)
        return id - 0x0e;
    if (id == 4)
        return 3;
    if (id == 0x29)
        return 0x28;
    return id;
}

const char *sound_map_id_to_wav(int file_id)
{
    /* Sound_MapIdToWav @ 1150:019e; names @ 1420:3080..315a. Gaps fall to
     * the table default @ 1150:025b = silent (NULL). */
    switch (file_id) {
    case 0x83: return "OPENING.wav";
    case 0x85: return "LINC.wav";
    case 0x86: return "MONT.wav";
    case 0x87: return "RAMS.wav";
    case 0x88: return "SHAK.wav";
    case 0x89: return "NAPO.wav";
    case 0x8a: return "CEAS.wav";
    case 0x8b: return "STAL.wav";
    case 0x8c: return "ALEX.wav";
    case 0x8d: return "ELIZ.wav";
    case 0x8e: return "HAMA.wav";
    case 0x8f: return "MAO.wav";
    case 0x90: return "GENG.wav";
    case 0x91: return "GAND.wav";
    case 0x92: return "FRED.wav";
    case 0xa2: return "WINTUNE.wav";
    case 0xa3: return "LOSE2.wav";
    case 0xa5: return "S_BEEP.wav";
    case 0xa6: return "THEY_DIE.wav";
    case 0xa7: return "WE_DIE.wav";
    case 0xa8: return "S_LAND.wav";
    case 0xaa: return "AIRNUKE.wav";
    case 0xab: return "Cannon.wav";
    default: break;
    }
    return NULL;
}

civ_error_t audio_play_id(int id)
{
    const char *wav;

    /* Sound_PlayId @ 1150:011e..0132: STOP path. */
    if (id == CIV_SOUND_STOP || id == CIV_SOUND_STOP1) {
        /* TODO: SNDPLAYSOUND(0) -> SDL3 stop. */
        return CIV_ERR_UNSUPPORTED;
    }
    /* TODO: gate on g_nOptionFlags (12d0:0064) bit 0x10 + DAT_1228:0104. */
    wav = sound_map_id_to_wav(sound_normalize_id(id) + 0x80);
    if (!wav)
        return CIV_ERR_NOENT; /* silent gap in the table */
    return audio_play(wav);
}

void audio_shutdown(void)
{
    /* TODO: Sound_MciClose @ 1150:0066 equiv. */
}
