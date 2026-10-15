/* Audio: SNDPLAYSOUND / MCISENDCOMMAND -> SDL3 audio.
 *
 * Ghidra anchors (civ.exe, segment 1150), renamed 2026-10-15:
 *   Sound_MciOpen    @ 1150:001c (MCISENDCOMMAND 0x803)
 *   Sound_MciClose   @ 1150:0066 (MCISENDCOMMAND 0x804)
 *   Sound_PlayFile   @ 1150:00a0 (SNDPLAYSOUND(filename))
 *   Sound_PlayId     @ 1150:00c9 (SI==0/1 -> STOP; else gate on
 *                       g_nOptionFlags bit 0x10 + DAT_1228:0104, normalize,
 *                       AX=SI+0x80 -> Sound_MapIdToWav -> Sound_PlayFile)
 *   Sound_MapIdToWav @ 1150:019e (jump table @ 1150:0266, 41 entries for
 *                       IDs 0x83..0xAB; names @ 1420:3080..315a, dump-confirmed)
 *
 * String literal anchor (packet F): "OPENING.wav" @ 1420:3114
 * (CIV_OPENING_WAV_NAME, via the 1150:0266 jump-table names @
 * 1420:3080..315a). Other packet-F literals live with their owners:
 * civfonts.fon + CivData0..4.rsc (boot.h / rsc_loader.h), civ.prf
 * (boot.h), civhelp.hlp (dialog.h), famehall.civ (halloffame.h);
 * MPLOGO.BMP has no filename string (NE resource / RSC frame, see
 * title.h). All asset reads are read-only.
 *
 * ID normalization in Sound_PlayId (disassembly @ 1150:014f..016d):
 *   0x13..0x20 -> -= 0x0e; 4 -> 3; 0x29 -> 0x28.
 * Mapped IDs: 0x83 OPENING, 0x85..0x92 leaders LINC/MONT/RAMS/SHAK/NAPO/
 *   CEAS/STAL/ALEX/ELIZ/HAMA/MAO/GENG/GAND/FRED, 0xA2 WINTUNE, 0xA3 LOSE2,
 *   0xA5 S_BEEP, 0xA6 THEY_DIE, 0xA7 WE_DIE, 0xA8 S_LAND, 0xAA AIRNUKE,
 *   0xAB Cannon. Gaps (0x84, 0x93..0xA1, 0xA4, 0xA9 -> table default
 *   @ 1150:025b) are silent: no file, playback skipped.
 * Assets: unpacked WAV files (see civ1win/ read-only reference).
 */
#ifndef CIVLIN_AUDIO_H
#define CIVLIN_AUDIO_H

#include "civ_common.h"

/* Packet F string literal: "OPENING.wav" @ 1420:3114 (read-only). */
#define CIV_OPENING_WAV_NAME "OPENING.wav"

/* Sound IDs (SI values for Sound_PlayId; file IDs are SI+0x80). */
typedef enum civ_sound_id {
    CIV_SOUND_STOP = 0,      /* SNDPLAYSOUND(0): silence */
    CIV_SOUND_STOP1 = 1,     /* same */
    CIV_SOUND_OPENING = 3,   /* -> 0x83 OPENING.wav (4 normalizes to 3) */
    CIV_SOUND_VICTORY = 0x22 /* -> 0xA2 WINTUNE.wav (HoF ceremony) */
} civ_sound_id_t;

civ_error_t audio_init(void);

/* Packet K: MCI open/close + raw file playback (stubs; SDL audio TBD).
 *
 * Orig: FUN_1150_001c @ 1150:001c -> Sound_MciOpen. MCISENDCOMMAND 0x803
 *   (MCI_OPEN) for WAV; callee of audio_init. Win16 MCI -> SDL3 audio
 *   device TBD; stub CIV_ERR_UNSUPPORTED.
 * Orig: FUN_1150_0066 @ 1150:0066 -> Sound_MciClose. MCISENDCOMMAND 0x804
 *   (MCI_CLOSE); callee of audio_shutdown. Win16 MCI -> SDL3 teardown
 *   TBD; stub (void).
 * Orig: FUN_1150_00a0 @ 1150:00a0 -> Sound_PlayFile. Raw SNDPLAYSOUND
 *   playback of a WAV name; callee of Sound_PlayId (audio_play_id) after
 *   normalize + map + 0x80, disk gate via Dos_DriveReady @ 1000:1184
 *   (see boot.h; Linux: always ready). Read-only vs unpacked WAVs;
 *   SDL3 WAV playback TBD; stub CIV_ERR_UNSUPPORTED (NULL -> NOENT). */
civ_error_t sound_mci_open(void);
void sound_mci_close(void);
civ_error_t sound_play_file(const char *wav_name);

/* Orig: FUN_1160_xxxx @ 1160:xxxx -> Sound_MciPlay. Neighbour of the 1150
 * MCI cluster; MCISENDCOMMAND PLAY sibling to Sound_MciOpen @ 1150:001c
 * (0x803 OPEN) / Sound_MciClose @ 1150:0066 (0x804 CLOSE) — STOP is the
 * close/0 path. Ported volume path: delegates to the ported
 * sound_play_file (SDL_OpenAudioDeviceStream WAV playback); explicit
 * volume variant TBD (plays at WAV/device volume). NULL -> NOENT. */
civ_error_t sound_mci_play(const char *wav_name);

/* Raw filename playback (Sound_PlayFile @ 1150:00a0). Thin wrapper over
 * sound_play_file; kept for callers that already use audio_play. */
civ_error_t audio_play(const char *wav_name);

/* 1 while a WAV is still queued/available on the stream, 0 when idle or
 * closed. Used by Title_PlayAnimLoop @ 1030:0000 to end the intro when
 * OPENING.wav finishes (caller keeps a timeout fallback for dummy audio). */
int audio_is_playing(void);

/* ID playback (Sound_PlayId @ 1150:00c9). Honors the STOP/silent rules
 * plus the two-half gate: (1) g_nOptionFlags @ 12d0:0064 bit 0x10
 * sounds-on via config_prefs_sounds_on (see boot.h; defaults carry the
 * decoded flags word until the civ.prf field map lands), and (2)
 * DAT_1228:0104 disk-ready half via dos_drive_ready (see boot.h
 * Dos_DriveReady @ 1000:1184; Linux: always ready, drive 0 = current).
 * Both halves must pass; gated-off stays silent with CIV_OK (orig
 * skipped SNDPLAYSOUND, not an error). UNSUPPORTED only when a mapped
 * file exists but SDL playback is missing. */
civ_error_t audio_play_id(int id);

/* Pure mappers (implemented; verified against jump table + mem dump). */
int sound_normalize_id(int id);
const char *sound_map_id_to_wav(int file_id);

/* AF2/AG3: WAV silent-gap check (OS-free port of the Sound_MapIdToWav @
 * 1150:019e table-default @ 1150:025b branch; no SDL, no globals, no
 * exit(), read-only; doc only, play impls untouched). Jump table @
 * 1150:0266 holds 41 entries for file IDs 0x83..0xAB inclusive
 * (0xAB-0x83+1 = 41; names @ 1420:3080..315a, dump-confirmed); gaps hit
 * the default @ 1150:025b = silent (no file, playback skipped):
 *   silent: 0x84, 0x93..0xA1, 0xA4, 0xA9 (+ out-of-range file IDs).
 * Returns 1 when file_id is silent (sound_map_id_to_wav would return
 * NULL), 0 when a WAV name is mapped. Parity: implemented via
 * sound_map_id_to_wav so audio_play_id @ 1150:00c9 + sound_map_id_to_wav
 * above stay untouched. */
int audio_gap_is_silent(int file_id);

/* AH3: leader WAV name table (2026-10-15). OS-free static const port of
 * the Sound_MapIdToWav @ 1150:019e jump table @ 1150:0266 leader slice
 * (names @ 1420:3080..315a, dump-confirmed; full map context: 0x83
 * OPENING, 0x85..0x92 leaders below, 0xA2 WINTUNE, 0xA3 LOSE2, 0xA5
 * S_BEEP, 0xA6 THEY_DIE, 0xA7 WE_DIE, 0xA8 S_LAND, 0xAA AIRNUKE, 0xAB
 * Cannon). 14-entry table, index 0..13 == file_id 0x85..0x92:
 *   0 LINC, 1 MONT, 2 RAMS, 3 SHAK, 4 NAPO, 5 CEAS, 6 STAL, 7 ALEX,
 *   8 ELIZ, 9 HAMA, 10 MAO, 11 GENG, 12 GAND, 13 FRED (all "*.wav" exactly
 *   as in sound_map_id_to_wav above). Gaps (0x84, 0x93..0xA1, 0xA4, 0xA9
 *   + out-of-range) stay silent already via audio_gap_is_silent above;
 *   play impls (sound_play_file/audio_play_id) stay untouched. No SDL, no
 *   globals, no exit(), read-only. */
#define CIV_AUDIO_LEADER_COUNT 14

/* Count helper: returns CIV_AUDIO_LEADER_COUNT (14). */
int audio_leader_count(void);

/* At helper: leader WAV name for index 0..CIV_AUDIO_LEADER_COUNT-1
 * (file_id 0x85 + index), NULL when index is out of range. No globals
 * (function-local static const view), no exit(), read-only. */
const char *audio_leader_names(int index);

void audio_shutdown(void);

#endif /* CIVLIN_AUDIO_H */
