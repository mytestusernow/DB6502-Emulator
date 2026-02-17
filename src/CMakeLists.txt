# DB6502 Emulator - source build configuration

# Device source files from HBC-56 submodule (compiled directly)
set(HBC56_DEVICES_DIR ${CMAKE_SOURCE_DIR}/hbc-56/emulator/src/devices)
set(HBC56_DEBUGGER_DIR ${CMAKE_SOURCE_DIR}/hbc-56/emulator/src/debugger)

set(HBC56_DEVICE_SOURCES
    ${HBC56_DEVICES_DIR}/device.c
    ${HBC56_DEVICES_DIR}/device.h
    ${HBC56_DEVICES_DIR}/6502_device.c
    ${HBC56_DEVICES_DIR}/6502_device.h
    ${HBC56_DEVICES_DIR}/memory_device.c
    ${HBC56_DEVICES_DIR}/memory_device.h
    ${HBC56_DEVICES_DIR}/tms9918_device.c
    ${HBC56_DEVICES_DIR}/tms9918_device.h
    ${HBC56_DEVICES_DIR}/ay38910_device.c
    ${HBC56_DEVICES_DIR}/ay38910_device.h
    ${HBC56_DEVICES_DIR}/via_device.c
    ${HBC56_DEVICES_DIR}/via_device.h
    ${HBC56_DEVICES_DIR}/keyboard_device.c
    ${HBC56_DEVICES_DIR}/keyboard_device.h
)

set(HBC56_DEBUGGER_SOURCES
    ${HBC56_DEBUGGER_DIR}/debugger.cpp
    ${HBC56_DEBUGGER_DIR}/debugger.h
)

# DB6502-specific sources
set(DB6502_SOURCES
    db6502emu.cpp
    db6502emu.h
    hbc56emu.h
    audio.c
    audio.h
    config.h
    devices/acia_device.c
    devices/acia_device.h
)

add_executable(Db6502Emu ${DB6502_SOURCES} ${HBC56_DEVICE_SOURCES} ${HBC56_DEBUGGER_SOURCES})

add_definitions(-DVR_6502_EMU_STATIC)

# Include paths:
# - Our src/ directory (for config.h, hbc56emu.h, db6502emu.h)
# - HBC-56 src/ directory (for shared headers that use relative paths)
target_include_directories(Db6502Emu PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/hbc-56/emulator/src
)

target_link_libraries(Db6502Emu vrEmu6502 vrEmu6522 vrEmuTms9918 vrEmuTms9918Util vrEmuLcd emu2149 SDL2 SDL2main imgui)
