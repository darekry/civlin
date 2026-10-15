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
 * close/0 path, volume variant TBD. Win16 MCI -> SDL3 audio TBD; stub
 * CIV_ERR_UNSUPPORTED (NULL -> NOENT). */
civ_error_t sound_mci_play(const char *wav_name);

/* Raw filename playback (Sound_PlayFile @ 1150:00a0). Thin wrapper over
 * sound_play_file; kept for callers that already use audio_play. */
civ_error_t audio_play(const char *wav_name);

/* ID playback (Sound_PlayId @ 1150:00c9). Honors the STOP/silent rules;
 * UNSUPPORTED only when a mapped file exists but SDL playback is missing. */
civ_error_t audio_play_id(int id);

/* Pure mappers (implemented; verified against jump table + mem dump). */
int sound_normalize_id(int id);
const char *sound_map_id_to_wav(int file_id);

void audio_shutdown(void);

#endif /* CIVLIN_AUDIO_H */
