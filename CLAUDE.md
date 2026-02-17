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
- Emulator builds and runs successfully
- Woz Monitor `\` prompt displays in ACIA terminal
- BASIC can be entered via `A000 R` but "MEMORY SIZE?" prompt input is problematic
- **Known issue:** Slow execution (~30s for initial prompt at 4 MHz). Suspected interrupt storm from TMS9918A VBlank and VIA timer IRQs that the ROM's IRQ handler never acknowledges (it only handles ACIA). Fix: disable non-ACIA IRQs in config.h.
- Clock currently set to 4 MHz (was 1 MHz, bumped for testing)

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
- Non-ACIA IRQs should be disabled until ROM code handles them
