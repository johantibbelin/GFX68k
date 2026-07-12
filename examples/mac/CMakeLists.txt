# Builds the Mac 68k demo with Retro68 (https://github.com/autc04/Retro68).
#
# Not verified against a real compiler in this environment (no Mac 68k
# cross toolchain available here); written directly against Retro68's
# documented CMake usage.
#
# Usage, from a Retro68 build tree with the toolchain installed to
# $RETRO68/toolchain:
#   mkdir build && cd build
#   cmake -DCMAKE_TOOLCHAIN_FILE=$RETRO68/toolchain/m68k-apple-macos/cmake/retro68.toolchain.cmake ..
#   make

cmake_minimum_required(VERSION 3.10)
project(gfx68k_mac_demo C)

set(GFX68K_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

include_directories(
    ${GFX68K_ROOT}/include
    ${GFX68K_ROOT}/src/platform/mac
)

add_application(demo
    demo.c
    ${GFX68K_ROOT}/src/common/gfx_primitives.c
    ${GFX68K_ROOT}/src/common/font/gfx_font.c
    ${GFX68K_ROOT}/src/common/font/font_default.c
    ${GFX68K_ROOT}/src/platform/mac/gfx_mac.c
)
