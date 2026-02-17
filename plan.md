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
- [ ] Fix interrupt storm (disable non-ACIA IRQs)
- [ ] Verify BASIC works end-to-end (A000 R → MEMORY SIZE? → OK prompt)

## Phase 2: Refinements
- [ ] Restore 1 MHz clock after speed fix
- [ ] ACIA terminal improvements (scrollback, cursor blink, colors)
- [ ] VIA1/VIA2 debugger views
- [ ] File dialog default to eater.bin location
- [ ] Save/restore emulator state

## Phase 3: Graphics & Sound
- [ ] ROM code to initialize TMS9918A (new BIOS routines)
- [ ] ROM code to use AY-3-8910 for sound
- [ ] BASIC extensions for graphics/sound commands

## Phase 4: Advanced Features
- [ ] Serial file transfer (load BASIC programs via ACIA)
- [ ] Breakpoint conditions
- [ ] Memory watch windows
- [ ] Performance profiling view

## Source of HBC-56 Emulator
- Repository: https://github.com/visrealm/hbc-56
- License: MIT
- Author: Troy Schrapel
- Used as git submodule for hardware emulation libraries
