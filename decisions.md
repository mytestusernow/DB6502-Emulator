# DB6502 Emulator - Design Decisions

## Decision 1: Use HBC-56 as submodule, not fork
**Choice:** Git submodule referencing upstream visrealm/hbc-56
**Rationale:** Allows pulling upstream improvements. Only our config and main file differ. Device code compiled directly from submodule without modification.
**Trade-off:** Must maintain compatibility shim. Cannot modify shared device code directly.

## Decision 2: Keep hbc56* function names internally
**Choice:** Internal emulator functions keep `hbc56` prefix (e.g., `hbc56Interrupt`, `hbc56MemRead`)
**Rationale:** All shared device code from the submodule calls these names. Creating aliases or wrappers adds complexity. Only user-visible strings (window title, binary name) are renamed to DB6502.
**Trade-off:** Code reads "hbc56" but is actually DB6502. Acceptable since these are internal plumbing.

## Decision 3: Direct memory-mapped I/O (no I/O window)
**Choice:** I/O devices use full 16-bit addresses ($8200, $8400, etc.) instead of HBC-56's port-offset scheme ($7F00 + port)
**Rationale:** DB6502 maps peripherals directly into the $8000-$9FFF address space. The device read/write functions already use full addresses, so this is just a config change.
**Trade-off:** None significant. The `HBC56_IO_ADDRESS` macro is defined as identity function.

## Decision 4: ROM loaded last in device chain
**Choice:** ROM device is the last device added, after all I/O devices
**Rationale:** The memory bus iterates devices in order; first claim wins. I/O devices at $8200-$9FFF must shadow the ROM at those addresses. Adding ROM last ensures I/O takes priority.
**Trade-off:** None. This matches real hardware behavior where I/O chip-select lines take priority.
**Bug found:** Initially ROM was loaded during argument parsing (before devices), causing it to be first in chain and intercepting all I/O reads/writes. Fixed by deferring loadRom() to after device setup.

## Decision 5: New ACIA device instead of HBC-56 UART
**Choice:** Wrote new acia_device.c implementing WDC 65C51 with ImGui terminal
**Rationale:** HBC-56 uses a simple UART device tied to a physical COM port. DB6502 needs a 65C51 ACIA with proper register emulation (status, command, control registers) and an in-emulator terminal window for interactive use.
**Trade-off:** More code to maintain, but essential for the DB6502 use case.

## Decision 6: TMS9918A and AY-3-8910 at non-standard addresses
**Choice:** TMS9918A at $8200, AY-3-8910 at $8300
**Rationale:** User's desired I/O layout for DB6502. These chips aren't used by the current Woz Monitor/BASIC ROM but are mapped in the emulator ready for future ROM updates.
**Trade-off:** Current ROM won't exercise these devices. Requires new ROM code later.

## Decision 7: Clock speed (4 MHz)
**Choice:** HBC56_CLOCK_FREQ set to 4000000 (4 MHz)
**Rationale:** DB6502 hardware runs at 1 MHz, but 4 MHz provides snappier emulation and the timing fix (Decision 11) ensures full speed regardless. The BIOS TX delay loop runs faster but this is acceptable for emulation.
**Trade-off:** Software timing loops run 4x faster than real hardware. Acceptable for interactive use.

## Decision 8: SDL2 (not SDL3)
**Choice:** Keep SDL2 from HBC-56 submodule
**Rationale:** HBC-56 uses SDL2 with ImGui SDL2 backend. Migrating to SDL3 would require changing the submodule and all rendering code. Not worth the effort for initial version.
**Trade-off:** Using older SDL version. Can migrate later if needed.

## Decision 9: Disable non-ACIA IRQs
**Choice:** Set TMS9918A, VIA1, VIA2, and keyboard IRQ numbers to 0 in config.h
**Rationale:** The ROM's IRQ handler only acknowledges ACIA interrupts. TMS9918A VBlank (60Hz), VIA timer, and keyboard IRQs go unacknowledged, causing an interrupt storm.
**Trade-off:** Those devices can't generate interrupts until ROM code is written to handle them.

## Decision 10: CR/LF handling in ACIA terminal
**Choice:** CR ($0D) produces a newline. LF ($0A) immediately after CR is suppressed.
**Rationale:** The Woz Monitor sends CR followed by LF for line endings. Without special handling, this produced double newlines (one for CR, one for LF). The suppress-LF-after-CR approach handles both CR-only and CR+LF line endings correctly.
**Trade-off:** A bare LF (without preceding CR) still produces a newline, which is correct for Unix-style line endings.

## Decision 11: doTick() catch-up batching for full-speed emulation
**Choice:** doTick() calculates elapsed real time and runs multiple 100us/400-cycle batches to catch up, capped at 50ms
**Rationale:** Original HBC-56 doTick() ran one batch per call. With ImGui rendering taking ~40ms per frame, only ~25 doTick() calls per second occurred, giving ~10,000 cycles/sec instead of 4,000,000. The catch-up approach processes all missed batches in a burst after each render frame.
**Trade-off:** CPU work is bursty (idle during render, then burst of batches). 50ms cap prevents long freezes if the emulator stalls. This is imperceptible at 60 FPS.
**Bug found:** This was the root cause of the "MEMORY SIZE? hang" - BASIC wasn't hung, it was running 400x too slowly. The BIOS CHROUT TX delay loop (~1275 cycles) took ~130ms instead of ~0.3ms, making BASIC appear frozen.

## Decision 12: Paste flow control via BIOS zero-page buffer pointers
**Choice:** Read BIOS circular buffer fill level from zero page (READ_PTR at $0000, WRITE_PTR at $0001) and only inject characters when bufUsed < 192
**Rationale:** Ctrl+V paste must not overflow the BIOS INPUT_BUFFER ($0300, 256 bytes). Four approaches were tried:
1. Dump all chars to ACIA RX buffer at once → 256-byte RX buffer overflow, only last ~2 lines survived
2. Drip-feed one byte per tick batch → ACIA RX empties fast (IRQ grabs byte) but BIOS buffer still overflows since CHRIN consumes slower than injection
3. Check ACIA RX buffer empty → same issue, ACIA empties fast but BIOS buffer is the bottleneck
4. Check VIA1 PORTA bit 0 (hardware RTS) → hbc56MemRead(0x9001) didn't return useful output register value
5. Read zero-page buffer pointers directly → works, throttles to actual BIOS consumption rate
**Trade-off:** Tightly coupled to BIOS zero-page layout ($0000/$0001). If BIOS changes buffer pointer locations, paste breaks. Acceptable since we control the ROM.

## Bugs Found and Fixed

| Bug | Root Cause | Fix |
|-----|-----------|-----|
| cmake fails with SDL2 CMakeLists | cmake 4.x policy requirements vs old SDL2 CMakeLists | Added `CMAKE_POLICY_VERSION_MINIMUM=3.5` |
| acia_device.h: device.h not found | Include path didn't reach hbc-56 devices dir | Changed to `#include "devices/device.h"` |
| Terminal blank, no output | ROM loaded before I/O devices in device chain | Deferred loadRom() to after device setup |
| Double newlines in terminal | CR ignored, only LF produced newline; Woz sends CR+LF | CR produces newline, LF after CR suppressed |
| ACIA address check wraparound | `addr - baseAddr` underflows when addr < baseAddr (unsigned) | Added explicit bounds check |
| 400x CPU slowdown (appeared as BASIC hang) | doTick() ran one 400-cycle batch per call; ImGui render blocked ~40ms | Catch-up batching: run multiple batches per doTick() based on elapsed time |
| Paste only captures last ~2 lines | All paste chars dumped into 256-byte ACIA RX buffer at once | Queue chars, drip-feed with BIOS buffer flow control |
| Release build -Werror failure | vrEmu6502.c `-Werror=maybe-uninitialized` in Release mode | Added `-Wno-error=maybe-uninitialized` to CMAKE_C_FLAGS |
