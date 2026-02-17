# DB6502 Emulator - Architecture

## Device-Based Architecture

The emulator follows the HBC-56 pattern where each hardware component is a `HBC56Device` struct with function pointers:

```
HBC56Device
├── name        - Device name (for UI display)
├── resetFn     - Called on hardware reset
├── destroyFn   - Cleanup on exit
├── tickFn      - Called every clock cycle batch
├── readFn      - Memory read (returns 1 if address claimed)
├── writeFn     - Memory write (returns 1 if address claimed)
├── renderFn    - Update display textures
├── audioFn     - Fill audio buffer
├── eventFn     - Handle SDL events
├── data        - Private device state
├── output      - SDL_Texture for display devices
└── visible     - ImGui window visibility
```

## Memory Access

Memory reads/writes iterate the device array in order. The first device whose read/write function returns 1 (claiming the address) wins. This means:

1. I/O devices are added BEFORE ROM
2. I/O devices at $8200-$9FFF claim their specific addresses
3. ROM at $8000-$FFFF serves everything else in that range
4. RAM at $0000-$7FFF handles the lower 32KB

**Critical:** ROM must be the last device added. If ROM is added before I/O devices, it will claim ACIA/VIA addresses and I/O will be dead. This was the root cause of the initial "blank terminal" bug.

## Device Order (as added to array)

1. **CPU** (65C02, 4 MHz currently) - drives the bus
2. **RAM** ($0000-$7FFF) - 32KB
3. **TMS9918A** ($8200-$8201) - video display processor
4. **AY-3-8910** ($8300-$8303) - sound
5. **ACIA** ($8400-$8403) - serial with terminal
6. **VIA2** ($8800-$880F) - general purpose I/O
7. **VIA1** ($9000-$900F) - keyboard interface, synced to CPU
8. **Keyboard** ($9000) - PS/2 on VIA1 port A
9. **ROM** ($8000-$FFFF) - loaded dynamically, added LAST

## ROM Loading

ROM loading is deferred until after all I/O devices are set up. During argument parsing, the ROM file path is saved. After all devices are added to the device chain, `loadRom()` is called. This ensures ROM is the last device and I/O takes priority.

## Interrupt Routing

| IRQ# | Source | Status | Notes |
|------|--------|--------|-------|
| 0 (disabled) | TMS9918A | Should be disabled | ROM doesn't acknowledge VDP IRQs |
| 2 | ACIA | Active | Receive data ready - ROM handles this |
| 0 (disabled) | VIA2 | Should be disabled | ROM doesn't acknowledge VIA2 IRQs |
| 0 (disabled) | VIA1/Keyboard | Should be disabled | ROM doesn't acknowledge VIA1 IRQs |

**Interrupt storm issue:** The ROM's IRQ handler (in bios.s) only reads the ACIA status register. It does NOT acknowledge TMS9918A, VIA1, or VIA2 interrupts. If those devices assert IRQ, the CPU re-enters the IRQ handler immediately after every RTI, consuming all CPU time. Solution: set non-ACIA IRQ numbers to 0 in config.h.

All IRQs are active-low, active if ANY source is raised.

## Timing

- CPU clock: 4 MHz (HBC56_CLOCK_FREQ = 4000000)
- Tick quantum: 100us batches of 400 cycles each
- doTick() uses **catch-up batching**: calculates elapsed real time since last call, runs multiple 400-cycle batches to match. Capped at 50ms (500 batches) to avoid long freezes after stalls.
- Render: ~60 FPS via ImGui/SDL2. Rendering blocks the main loop for ~17-40ms per frame, so catch-up batching is essential to maintain full CPU speed.
- Audio: 48 KHz, stereo float
- CPU_6502_MAX_TIMESTEP_STEPS = 4000 caps cycles per tick batch

**Why catch-up batching matters:** Without it, the original single-batch-per-call approach gave only ~10,000 cycles/sec (one 400-cycle batch per ~40ms render frame) instead of 4,000,000. This made the CPU appear frozen on any timing-sensitive code (e.g., BIOS CHROUT's 1275-cycle TX delay loop).

## ACIA Terminal

The ACIA device includes an ImGui terminal window:
- Green text on black background (VT100 style)
- 64KB output buffer with auto-scroll
- Keyboard input captured when terminal window is focused
- Enter sends CR ($0D), Backspace sends $08, ESC sends $1B
- Text input goes to ACIA receive circular buffer (256 bytes)
- CR handling: CR produces newline, LF after CR is suppressed (Woz Monitor sends CR+LF)
- Ctrl+V paste: characters queued in `aciaPasteQueue`, drip-fed one per tick batch with flow control (see below)

## Paste Flow Control

Ctrl+V paste uses a throttled injection system to avoid overflowing the BIOS INPUT_BUFFER:

1. **Queue:** `hbc56PasteText()` pushes characters into `aciaPasteQueue` (converting LF to CR)
2. **Drip-feed:** Each tick batch checks if the ACIA RX buffer is empty AND the BIOS circular buffer has room
3. **Flow control:** Reads BIOS zero-page pointers directly: READ_PTR ($0000) and WRITE_PTR ($0001). Only injects when `bufUsed = (wrPtr - rdPtr) < 192` (leaves headroom in the 256-byte buffer)
4. **Character injection:** Calls `aciaDeviceReceiveByte()` which triggers ACIA RX IRQ, BIOS IRQ handler moves byte to INPUT_BUFFER, BASIC's CHRIN reads from there

This approach is tightly coupled to the BIOS memory layout but works reliably for pasting multi-line BASIC programs

## File Structure

```
DB6502_Emulator/
├── CMakeLists.txt          -> Top-level, references submodule
├── src/
│   ├── CMakeLists.txt      -> Build config, lists all sources
│   ├── config.h            -> DB6502 address map (replaces HBC-56 config)
│   ├── db6502emu.h         -> API header (same function signatures as HBC-56)
│   ├── db6502emu.cpp       -> Main emulator + ImGui UI
│   ├── hbc56emu.h          -> Compat shim -> includes db6502emu.h
│   ├── audio.c/h           -> SDL2 audio subsystem
│   └── devices/
│       └── acia_device.c/h -> NEW: 65C51 ACIA + terminal
└── hbc-56/                 -> Git submodule
    └── emulator/
        ├── src/devices/    -> Shared: device.c, 6502, memory, TMS, AY, VIA, KB
        ├── src/debugger/   -> Shared: debugger.cpp
        ├── modules/        -> Hardware emulation libraries
        └── thirdparty/     -> SDL2, ImGui
```

## Compatibility Approach

The shared HBC-56 device code includes `"../hbc56emu.h"` for functions like `hbc56Interrupt()`, `hbc56MemRead()`, etc. Rather than modifying submodule source, we:

1. Keep internal function names as `hbc56*`
2. Provide `src/hbc56emu.h` that includes `db6502emu.h`
3. Set include paths so our src/ is searched first
4. Only rename user-visible strings (window title, binary name)
