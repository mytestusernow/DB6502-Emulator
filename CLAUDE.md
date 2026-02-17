# DB6502 Emulator

## Overview
Standalone emulator for the DB6502 single board computer, based on Troy Schrapel's HBC-56 emulator (MIT license). Emulates a 65C02 CPU with 65C51 ACIA serial, two 65C22 VIAs, TMS9918A graphics, and AY-3-8910 sound.

## Repository Structure
- `src/` - DB6502-specific emulator source code
  - `config.h` - DB6502 memory map and hardware configuration
  - `db6502emu.h/cpp` - Main emulator (adapted from HBC-56)
  - `hbc56emu.h` - Compatibility shim for shared HBC-56 device code
  - `audio.c/h` - Audio subsystem
  - `devices/acia_device.c/h` - 65C51 ACIA with ImGui terminal
- `hbc-56/` - Git submodule: HBC-56 emulator (provides hardware libs and shared device code)
- `build/` - CMake build output

## Hardware Emulated (DB6502 Memory Map)
| Range | Device |
|-------|--------|
| $0000-$7FFF | 32KB RAM |
| $8200-$8201 | TMS9918A VDP |
| $8300-$8303 | AY-3-8910 PSG |
| $8400-$8403 | 65C51 ACIA (serial terminal) |
| $8800-$880F | VIA2 (65C22) |
| $9000-$900F | VIA1 (65C22) + PS/2 keyboard |
| $8000-$FFFF | 32KB ROM (eater.bin, I/O takes priority) |

## Build
Requires: cmake, gcc/g++, SDL2 development headers, X11 development headers.

```sh
cd /home/paul/AI_Terminal/DB6502_Emulator
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

Note: cmake 4.x requires `CMAKE_POLICY_VERSION_MINIMUM=3.5` (already set in CMakeLists.txt) for the bundled SDL2 CMakeLists compatibility.

## Run
```sh
./build/bin/Db6502Emu --rom /path/to/eater.bin
```

The eater.bin ROM is at: `/home/paul/AI_Terminal/DB6502_Basic/eater.bin`

Options:
- `--rom <file>` - Load ROM file (32KB binary)
- `--brk` - Start paused in debugger

## Architecture
The emulator uses the HBC-56 device-based architecture where each hardware component is an `HBC56Device` struct with function pointers for read/write/tick/reset/render. Memory access iterates devices in order - first device to claim an address wins. I/O devices are added before ROM so they take priority in the overlapping $8000-$9FFF range.

Internal function names kept as `hbc56*` for compatibility with shared device code from the submodule. Only user-visible strings (window title, binary name) are renamed.

## Current Status
- Emulator builds and runs at full 4 MHz speed
- Woz Monitor boots and auto-enters BASIC via `A000 R`
- BASIC fully functional: MEMORY SIZE?, TERMINAL WIDTH?, 31487 BYTES FREE, OK prompt
- Ctrl+V paste works with flow control (reads BIOS buffer fill level)
- Debugger, serial terminal, TMS9918A display all operational
- v0.1 release published on GitHub with portable Linux x86_64 binary
- Non-ACIA IRQs disabled in config.h (ROM only handles ACIA interrupts)

## Git Configuration
- User: Paul
- Email: github.mhspb@slmails.com
- GitHub username: mytestusernow

## Key Design Decisions
- HBC-56 submodule used for hardware emulation libraries and shared device code
- Device source files compiled directly from submodule (not copied)
- `hbc56emu.h` compatibility shim avoids modifying any submodule code
- ACIA device is new (HBC-56 uses UART, DB6502 uses 65C51)
- ROM loaded last in device chain so I/O devices shadow it
- Non-ACIA IRQs disabled in config.h (ROM only handles ACIA)
- doTick() uses catch-up batching to maintain 4 MHz despite slow rendering
- Paste flow control reads BIOS zero-page buffer pointers ($0000/$0001) directly

## GitHub
- Repository: https://github.com/mytestusernow/DB6502-Emulator
- Release: v0.1 - Linux x86_64 portable binary (Db6502Emu-linux-x86_64.tar.gz)
