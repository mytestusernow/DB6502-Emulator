/*
 * DB6502 Emulator - Audio
 *
 * Based on Troy Schrapel's HBC-56 Emulator (MIT License)
 * https://github.com/visrealm/hbc-56/emulator
 */

#ifndef _HBC56_AUDIO_H_
#define _HBC56_AUDIO_H_

#ifdef __cplusplus
extern "C" {
#endif

void hbc56Audio(int start);

int hbc56AudioChannels();

int hbc56AudioFreq();

#ifdef __cplusplus
}
#endif

#endif
