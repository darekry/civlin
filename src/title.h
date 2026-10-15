/* Title / intro movie -> SDL3.
 *
 * Ghidra anchors (civ.exe):
 *   Sound_PlayId      @ 1150:00c9  (SI=3/4 -> 0x83 OPENING.wav; see audio.h)
 *   Sound_MapIdToWav  @ 1150:019e  (jump table @ 1150:0266)
 *   Sound_MciOpen     @ 1150:001c / Sound_MciClose @ 1150:0066 (MCI 0x803/0x804)
 *   GR_PicRead / LoadGifPicture / PicDecompress : gr_pic.c (1420:2300/2372/23ce)
 *     — candidate frame player for the intro sequence (NOT yet confirmed
 *     which caller drives the pre-menu animation).
 *   Map_GenerateRandomOrLoad @ 1028:0000 plays Sound_PlayId(4)=OPENING at
 *     map-gen start — related but distinct from the pre-menu intro.
 *
 * Asset notes: OPENING.wav confirmed at 1420:3114. MPLOGO.BMP has NO filename
 * string in the binary — likely an NE resource or an RSC frame, NOT a file
 * load. Dumping NE resources of unpacked/civ.exe is the next step before
 * implementing frame playback here.
 */
#ifndef CIVLIN_TITLE_H
#define CIVLIN_TITLE_H

#include "civ_common.h"

/* Plays the pre-menu intro (OPENING.wav + frames). Skippable on input in
 * the original. UNSUPPORTED until the frame source (NE resource vs RSC) is
 * confirmed. */
civ_error_t title_play_intro(void);

/* 1 == user asked to skip (key/mouse), 0 == keep playing. Pure SDL3 event
 * poll helper used by the intro and the victory ceremony. */
int title_poll_skip(void);

#endif /* CIVLIN_TITLE_H */
