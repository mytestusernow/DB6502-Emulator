# DB6502 Emulator - TODO

## Critical (Blocking)
- [ ] Disable non-ACIA IRQs in config.h (TMS9918, VIA1, VIA2, KB set to 0) to fix interrupt storm causing ~30s startup delay
- [ ] Verify BASIC works after IRQ fix (A000 R -> MEMORY SIZE? -> enter -> OK prompt)
- [ ] Fix keyboard input not getting through to BASIC "MEMORY SIZE?" prompt

## Completed
- [x] Project scaffold, git init, HBC-56 submodule added
- [x] config.h with DB6502 memory map
- [x] db6502emu.cpp adapted from HBC-56
- [x] ACIA device with ImGui terminal (acia_device.c/h)
- [x] CMake build system working
- [x] hbc56emu.h compatibility shim
- [x] Fix: SDL2 CMakeLists cmake policy version (CMAKE_POLICY_VERSION_MINIMUM=3.5)
- [x] Fix: acia_device.h include paths (device.h -> devices/device.h)
- [x] Fix: acia_device.c include paths (use include-path-relative)
- [x] Fix: CR handling in terminal (CR produces newline, LF after CR suppressed)
- [x] Fix: ROM loaded before I/O devices - deferred loadRom() to after device setup
- [x] Fix: ACIA address range check unsigned wraparound
- [x] Bumped clock to 4 MHz for speed testing
- [x] Woz Monitor `\` prompt displays correctly
- [x] BASIC "MEMORY SIZE?" prompt appears (but input not working)

## Short Term
- [ ] Restore 1 MHz clock after interrupt storm fix
- [ ] ACIA: verify IRQ behavior matches real 65C51
- [ ] Terminal: add cursor blink
- [ ] Terminal: handle screen clear (Ctrl+L or escape sequence)
- [ ] VIA2 debugger view (currently only VIA1 shown)

## Medium Term
- [ ] TMS9918A: write test ROM to verify VDP operation
- [ ] AY-3-8910: write test ROM to verify PSG operation
- [ ] Add memory-mapped I/O view in debugger
- [ ] Support label files (.lbl) from cc65 builds
- [ ] Serial file transfer: ability to "send" a file to ACIA
- [ ] Save/load emulator state snapshots

## Long Term
- [ ] BASIC extensions for TMS9918A graphics
- [ ] BASIC extensions for AY-3-8910 sound
- [ ] SD card emulation (if hardware supports it)
- [ ] Network: telnet into ACIA from external terminal
- [ ] Automated testing: load ROM, send commands, verify output
