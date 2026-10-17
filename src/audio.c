/* Audio: ID table + normalization + SDL3 WAV playback.
 *
 * Orig anchors in audio.h (Sound_MciOpen @ 1150:001c, Sound_MciClose @
 * 1150:0066, Sound_PlayFile @ 1150:00a0, Sound_PlayId @ 1150:00c9,
 * Sound_MapIdToWav @ 1150:019e). Win16 SNDPLAYSOUND/MCI -> SDL3
 * AudioStream (SDL_OpenAudioDeviceStream + Put + Resume). Read-only vs
 * civ1win WAVs, never written. Case-insensitive lookup because the
 * original table uses UPPER names ("OPENING.wav" @ 1420:3114) while the
 * shipped files use Mixed case ("Opening.wav").
 */
#include "audio.h"

#include "boot.h"

#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>

#include <SDL3/SDL.h>

/* Single voice (orig played one SNDPLAYSOUND at a time; STOP halts it).
 * File-static: the stream/device owner for this module (cf. sys.c statics).
 * No exit(), read-only. */
static SDL_AudioStream *s_stream;

/* Resolve wav_name to a loadable path into out (cap bytes).
 * Tries: as-is, civ1win/<name>, case-insensitive scan of civ1win/.
 * Returns out on success, NULL when nothing readable is found. */
static char *audio_resolve_path(const char *wav_name, char *out, size_t cap)
{
    char probe[512];
    FILE *fp = NULL;
    DIR *dir = NULL;
    struct dirent *ent = NULL;

    if (!wav_name || !out || cap == 0)
        return NULL;
    out[0] = '\0';

    /* 1) As given (absolute or cwd-relative). */
    fp = fopen(wav_name, "rb");
    if (fp) {
        fclose(fp);
        snprintf(out, cap, "%s", wav_name);
        out[cap - 1] = '\0';
        return out;
    }

    /* 2) civ1win/<name> exact case. */
    snprintf(probe, sizeof(probe), "civ1win/%s", wav_name);
    fp = fopen(probe, "rb");
    if (fp) {
        fclose(fp);
        snprintf(out, cap, "%s", probe);
        out[cap - 1] = '\0';
        return out;
    }

    /* 3) Case-insensitive scan of civ1win/ (OPENING.wav vs Opening.wav). */
    dir = opendir("civ1win");
    if (!dir)
        return NULL;
    while ((ent = readdir(dir)) != NULL) {
        if (strcasecmp(ent->d_name, wav_name) == 0) {
            closedir(dir);
            snprintf(out, cap, "civ1win/%s", ent->d_name);
            out[cap - 1] = '\0';
            fp = fopen(out, "rb");
            if (fp) {
                fclose(fp);
                return out;
            }
            return NULL;
        }
    }
    closedir(dir);
    return NULL;
}

static void audio_stop_stream(void)
{
    if (s_stream) {
        SDL_DestroyAudioStream(s_stream);
        s_stream = NULL;
    }
}

civ_error_t sound_mci_open(void)
{
    /* Orig: FUN_1150_001c @ 1150:001c -> Sound_MciOpen. MCISENDCOMMAND
     * 0x803 (MCI_OPEN) for WAV; callee of audio_init. Linux: device is
     * opened lazily per file in sound_play_file (format comes from the
     * WAV header), so open is a no-op CIV_OK. No disk access. */
    return CIV_OK;
}

void sound_mci_close(void)
{
    /* Orig: FUN_1150_0066 @ 1150:0066 -> Sound_MciClose. MCISENDCOMMAND
     * 0x804 (MCI_CLOSE); callee of audio_shutdown. */
    audio_stop_stream();
}

civ_error_t sound_play_file(const char *wav_name)
{
    char path[512];
    SDL_AudioSpec spec;
    Uint8 *buf = NULL;
    Uint32 len = 0;

    /* Orig: FUN_1150_00a0 @ 1150:00a0 -> Sound_PlayFile. Raw SNDPLAYSOUND
     * playback of wav_name; callee of Sound_PlayId (audio_play_id) after
     * normalize + map + 0x80. Disk gate via Dos_DriveReady @ 1000:1184
     * (see boot.h; Linux: always ready). */
    if (!wav_name)
        return CIV_ERR_NOENT;
    if (!audio_resolve_path(wav_name, path, sizeof(path))) {
        SDL_Log("audio: wav not found: %s", wav_name);
        return CIV_ERR_NOENT;
    }
    if (!SDL_LoadWAV(path, &spec, &buf, &len)) {
        SDL_Log("audio: LoadWAV failed %s: %s", path, SDL_GetError());
        return CIV_ERR_IO;
    }
    audio_stop_stream();
    s_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                                         &spec, NULL, NULL);
    if (!s_stream) {
        SDL_Log("audio: OpenAudioDeviceStream failed: %s", SDL_GetError());
        SDL_free(buf);
        return CIV_ERR_IO;
    }
    if (!SDL_PutAudioStreamData(s_stream, buf, (int)len)) {
        SDL_Log("audio: PutAudioStreamData failed: %s", SDL_GetError());
        SDL_free(buf);
        audio_stop_stream();
        return CIV_ERR_IO;
    }
    SDL_free(buf);
    if (!SDL_ResumeAudioStreamDevice(s_stream)) {
        SDL_Log("audio: ResumeAudioStreamDevice failed: %s", SDL_GetError());
        audio_stop_stream();
        return CIV_ERR_IO;
    }
    SDL_Log("audio: playing %s (%u bytes, %d Hz x%d)", path, (unsigned)len,
            spec.freq, spec.channels);
    return CIV_OK;
}

civ_error_t sound_mci_play(const char *wav_name)
{
    /* Orig: FUN_1160_xxxx @ 1160:xxxx -> Sound_MciPlay (volume port).
     * Neighbour of the 1150 MCI cluster; MCISENDCOMMAND PLAY sibling to
     * MciOpen @ 1150:001c (0x803 OPEN) / MciClose @ 1150:0066 (0x804
     * CLOSE). Delegates to the ported sound_play_file
     * (SDL_OpenAudioDeviceStream path above); explicit volume variant
     * TBD (plays at WAV/device volume). */
    if (!wav_name)
        return CIV_ERR_NOENT;
    return sound_play_file(wav_name);
}

civ_error_t audio_init(void)
{
    /* WinMain audio step -> Sound_MciOpen @ 1150:001c. */
    return sound_mci_open();
}

civ_error_t audio_play(const char *wav_name)
{
    /* Wrapper over Sound_PlayFile @ 1150:00a0. */
    return sound_play_file(wav_name);
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
    case 0x83: return CIV_OPENING_WAV_NAME;
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

int audio_gap_is_silent(int file_id)
{
    /* AF2/AG3 doc: OS-free port of the Sound_MapIdToWav @ 1150:019e
     * table-default @ 1150:025b branch (jump table @ 1150:0266, 41 entries
     * 0x83..0xAB inclusive, 0xAB-0x83+1 = 41; names @ 1420:3080..315a).
     * Silent gaps (no file, playback skipped): 0x84, 0x93..0xA1, 0xA4,
     * 0xA9, plus any out-of-range file_id. No SDL (map check only), no
     * globals, no exit(), read-only. Parity: delegates to
     * sound_map_id_to_wav above so audio_play_id @ 1150:00c9 gate +
     * sound_map_id_to_wav stay untouched (doc only, no impl change). */
    return sound_map_id_to_wav(file_id) == NULL ? 1 : 0;
}

/* --- Chain 2026-10-16 (25): Sound 3/1 + ID map (goal 3). ---
 * See audio.h block. OS-free 1:1 LIVE (no SDL, no globals, no exit()). */

int sound_is_stop(int id)
{
    /* Orig Sound_PlayId @ 1150:00c9 head: if ((param_1 == 0) || (param_1 ==
     * 1)) SNDPLAYSOUND(0) STOP (drugi parametr na stosie ignorowany —
     * caller-side Anim slot/flag, see audio.h). */
    return (id == CIV_SOUND_STOP || id == CIV_SOUND_STOP1) ? 1 : 0;
}

int sound_file_id_for(int id)
{
    /* Orig same + normalize @ 1150:014f..016d (sound_normalize_id LIVE) +
     * AX = SI+0x80 -> Sound_MapIdToWav @ 1150:019e. */
    return sound_normalize_id(id) + 0x80;
}

int sound_is_opening(int id)
{
    /* Orig same: 3 -> 0x83 OPENING.wav; 4 normalizes to 3 (alias). Intro
     * (Main_RunStartupSequence @ 11e0:0000 loop#1 + title OPENING.wav @
     * 1420:3114) + Map_GenerateRandomOrLoad @ 1028:0000 start (PlayId(4)). */
    return (id == CIV_SOUND_OPENING || id == CIV_SOUND_OPENING_ALIAS) ? 1 : 0;
}

int audio_leader_count(void)
{
    /* AH3 count helper (see audio.h): leader slice of Sound_MapIdToWav @
     * 1150:019e (jump table @ 1150:0266, names @ 1420:3080..315a):
     * 14 entries for file IDs 0x85..0x92. No SDL, no globals, no exit(),
     * read-only. Play impls untouched; gaps stay silent via
     * audio_gap_is_silent above. */
    return CIV_AUDIO_LEADER_COUNT;
}

const char *audio_leader_names(int index)
{
    /* AH3 at helper: OS-free static const 14-entry port of the leader
     * slice (file_id 0x85 + index -> WAV name, exactly as in
     * sound_map_id_to_wav above: 0 LINC.wav @ 0x85 .. 13 FRED.wav @
     * 0x92; full-map context 0x83 OPENING + 0xA2 WINTUNE etc. lives in
     * sound_map_id_to_wav). No globals (function-local read-only view),
     * no exit(), read-only. Parity: delegates to nothing (table mirrors
     * the map; audio_gap_is_silent + audio_play_id @ 1150:00c9 stay
     * untouched, doc only). */
    static const char *const names[CIV_AUDIO_LEADER_COUNT] = {
        "LINC.wav",
        "MONT.wav",
        "RAMS.wav",
        "SHAK.wav",
        "NAPO.wav",
        "CEAS.wav",
        "STAL.wav",
        "ALEX.wav",
        "ELIZ.wav",
        "HAMA.wav",
        "MAO.wav",
        "GENG.wav",
        "GAND.wav",
        "FRED.wav"
    };

    if (index < 0 || index >= CIV_AUDIO_LEADER_COUNT)
        return NULL;
    return names[index];
}

civ_error_t audio_play_id(int id)
{
    const char *wav;
    civ_prefs_t prefs;

    /* Sound_PlayId @ 1150:00c9: STOP path = SNDPLAYSOUND(0) silence. */
    if (id == CIV_SOUND_STOP || id == CIV_SOUND_STOP1) {
        audio_stop_stream();
        return CIV_OK;
    }
    /* Two-half gate (orig skipped SNDPLAYSOUND when either half failed,
     * silent CIV_OK, not an error): (1) g_nOptionFlags @ 12d0:0064 bit
     * 0x10 (sounds on) via config_prefs_sounds_on (see boot.h
     * config_prefs_defaults; live civ.prf field map TBD so defaults
     * carry the decoded flags word); (2) DAT_1228:0104 disk-ready half
     * via dos_drive_ready @ 1000:1184 (see boot.h; Linux: always ready,
     * drive 0 = current). */
    prefs = config_prefs_defaults();
    if (!config_prefs_sounds_on(&prefs))
        return CIV_OK;
    if (!dos_drive_ready(0))
        return CIV_OK;
    wav = sound_map_id_to_wav(sound_normalize_id(id) + 0x80);
    if (!wav)
        return CIV_ERR_NOENT; /* silent gap in the table */
    return sound_play_file(wav);
}

int audio_is_playing(void)
{
    /* 1 while the device still has queued/available samples, 0 when
     * idle or when no stream is open. Used by the title loop to end the
     * intro when OPENING.wav finishes (fallback timeout stays in the
     * caller so dummy-audio builds still terminate). */
    if (!s_stream)
        return 0;
    if (SDL_GetAudioStreamQueued(s_stream) > 0)
        return 1;
    if (SDL_GetAudioStreamAvailable(s_stream) > 0)
        return 1;
    return 0;
}

void audio_shutdown(void)
{
    /* WinMain shutdown step -> Sound_MciClose @ 1150:0066. */
    sound_mci_close();
}

/* --- Batch 2026-10-17 fala 6 (Z): padding --- */

civ_error_t snd_nop000e(void)
{
    /* Orig Snd_Nop000e @ 1150:000e (empty; WONTFIX-Win16). */
    return CIV_OK;
}
