cmake_minimum_required(VERSION 3.22)

# SDL2 bundled source has old cmake_minimum_required; allow it
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "")

include(CheckSymbolExists)

project(Db6502Emu C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/)
file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

include(CTest)

# HBC-56 hardware emulation libraries
add_subdirectory(hbc-56/emulator/modules)

# HBC-56 third-party dependencies (SDL2, ImGui)
add_subdirectory(hbc-56/emulator/thirdparty)

# DB6502 emulator source
add_subdirectory(src)

check_symbol_exists(fopen_s "stdio.h" HAVE_FOPEN_S)
if(HAVE_FOPEN_S)
    target_compile_definitions(Db6502Emu PRIVATE -DHAVE_FOPEN_S)
endif()

# Copy imgui.ini to output directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/src/imgui.ini DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
