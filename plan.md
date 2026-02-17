# DB6502 Emulator - Implementation Plan

## Phase 1: Core Emulator
- [x] Project scaffold with git repo and HBC-56 submodule
- [x] Config.h with DB6502 memory map
- [x] Main emulator adapted from HBC-56 (db6502emu.cpp)
- [x] 65C51 ACIA device with ImGui serial terminal
- [x] CMake build system referencing HBC-56 submodule libraries
- [x] Compatibility shim (hbc56emu.h) for shared device code
- [x] Build verification and bug fixes
- [x] Test with eater.bin ROM - Woz Monitor `\` prompt displays
- [x] Fix interrupt storm (disable non-ACIA IRQs in config.h)
- [x] Fix doTick() timing (catch-up batching for full 4 MHz speed)
- [x] Verify BASIC works end-to-end (A000 R → MEMORY SIZE? → OK prompt)
- [x] Ctrl+V paste with flow control (reads BIOS buffer pointers)
- [x] v0.1 GitHub release with portable Linux x86_64 binary

## Phase 2: Refinements
- [ ] ACIA terminal improvements (cursor blink, screen clear)
- [ ] VIA2 debugger view (currently only VIA1 shown)
- [ ] ACIA IRQ behavior verification against real 65C51
- [ ] Paste: respect VIA1 PORTA bit 0 (hardware RTS) instead of zero-page hack
- [ ] Serial file transfer (send file to ACIA)
- [ ] Save/restore emulator state

## Phase 3: Graphics & Sound
- [ ] ROM code to initialize TMS9918A (new BIOS routines)
- [ ] ROM code to use AY-3-8910 for sound
- [ ] BASIC extensions for graphics/sound commands

## Phase 4: Advanced Features
- [ ] Support label files (.lbl) from cc65 builds
- [ ] Memory-mapped I/O view in debugger
- [ ] Breakpoint conditions
- [ ] Memory watch windows
- [ ] Automated testing: load ROM, send commands, verify output

## Source of HBC-56 Emulator
- Repository: https://github.com/visrealm/hbc-56
- License: MIT
- Author: Troy Schrapel
- Used as git submodule for hardware emulation libraries
