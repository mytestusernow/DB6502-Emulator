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

## Decision 7: Clock speed (currently 4 MHz, target 1 MHz)
**Choice:** HBC56_CLOCK_FREQ temporarily set to 4000000 (4 MHz), target is 1000000 (1 MHz)
**Rationale:** DB6502 hardware runs at 1 MHz. Was bumped to 4 MHz during debugging to reduce startup time. Should be restored to 1 MHz after fixing the interrupt storm issue.
**Trade-off:** At 4 MHz, timing-sensitive code (software delays, baud rate) runs too fast. Must restore to 1 MHz for accurate emulation.

## Decision 8: SDL2 (not SDL3)
**Choice:** Keep SDL2 from HBC-56 submodule
**Rationale:** HBC-56 uses SDL2 with ImGui SDL2 backend. Migrating to SDL3 would require changing the submodule and all rendering code. Not worth the effort for initial version.
**Trade-off:** Using older SDL version. Can migrate later if needed.

## Decision 9: Disable non-ACIA IRQs (pending)
**Choice:** Set TMS9918A, VIA1, VIA2, and keyboard IRQ numbers to 0 in config.h
**Rationale:** The ROM's IRQ handler only acknowledges ACIA interrupts. TMS9918A VBlank (60Hz), VIA timer, and keyboard IRQs go unacknowledged, causing the CPU to immediately re-enter the IRQ handler after every RTI. This consumes nearly all CPU time, resulting in ~30s for the initial Woz Monitor prompt.
**Trade-off:** Those devices can't generate interrupts until ROM code is written to handle them. This is fine since the current ROM doesn't use them.

## Decision 10: CR/LF handling in ACIA terminal
**Choice:** CR ($0D) produces a newline. LF ($0A) immediately after CR is suppressed.
**Rationale:** The Woz Monitor sends CR followed by LF for line endings. Without special handling, this produced double newlines (one for CR, one for LF). The suppress-LF-after-CR approach handles both CR-only and CR+LF line endings correctly.
**Trade-off:** A bare LF (without preceding CR) still produces a newline, which is correct for Unix-style line endings.

## Bugs Found and Fixed

| Bug | Root Cause | Fix |
|-----|-----------|-----|
| cmake fails with SDL2 CMakeLists | cmake 4.x policy requirements vs old SDL2 CMakeLists | Added `CMAKE_POLICY_VERSION_MINIMUM=3.5` |
| acia_device.h: device.h not found | Include path didn't reach hbc-56 devices dir | Changed to `#include "devices/device.h"` |
| Terminal blank, no output | ROM loaded before I/O devices in device chain | Deferred loadRom() to after device setup |
| Double newlines in terminal | CR ignored, only LF produced newline; Woz sends CR+LF | CR produces newline, LF after CR suppressed |
| ACIA address check wraparound | `addr - baseAddr` underflows when addr < baseAddr (unsigned) | Added explicit bounds check |
| Very slow execution | Suspected interrupt storm from unacknowledged non-ACIA IRQs | Pending: disable non-ACIA IRQs |
