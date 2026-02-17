/*
 * DB6502 Emulator
 *
 * Based on Troy Schrapel's HBC-56 Emulator (MIT License)
 * https://github.com/visrealm/hbc-56/emulator
 *
 * Adapted for the DB6502 single board computer by Paul
 */

#ifndef _HBC56_EMU_H_
#define _HBC56_EMU_H_

#include "devices/device.h"
#include "config.h"

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void hbc56Reset();
int hbc56NumDevices();
HBC56Device *hbc56Device(size_t deviceNum);
HBC56Device *hbc56AddDevice(HBC56Device device);
void hbc56Interrupt(uint8_t irq, HBC56InterruptSignal signal);
int hbc56LoadRom(const uint8_t *romData, int romDataSize);
void hbc56LoadLabels(const char* labelFileContents);
void hbc56LoadSource(const char* rptFileContents);
void hbc56LoadLayout(const char* layoutFile);
const char *hbc56GetLayout();
void hbc56PasteText(const char* text);
void hbc56ToggleDebugger();
void hbc56DebugBreak();
void hbc56DebugRun();
void hbc56DebugStepInto();
void hbc56DebugStepOver();
void hbc56DebugStepOut();
double hbc56CpuRuntimeSeconds();
void hbc56DebugBreakOnInt();
uint8_t hbc56MemRead(uint16_t addr, bool dbg);
void hbc56MemWrite(uint16_t addr, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif
