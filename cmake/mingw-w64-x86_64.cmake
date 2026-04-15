# cmake/mingw-w64-x86_64.cmake
# CMake toolchain file for cross-compiling from Linux to Windows x64
# using the MinGW-w64 toolchain (Arch: mingw-w64-gcc).
#
# Usage:
#   cmake -B build \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
#         ..

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Arch Linux installs mingw-w64 with this prefix.
# Adjust if your distro uses a different path (e.g. /usr on Ubuntu).
set(MINGW_PREFIX "/usr")
set(MINGW_TOOL_PREFIX "x86_64-w64-mingw32")

# Compilers
set(CMAKE_C_COMPILER   "${MINGW_PREFIX}/bin/${MINGW_TOOL_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER "${MINGW_PREFIX}/bin/${MINGW_TOOL_PREFIX}-g++")
set(CMAKE_RC_COMPILER  "${MINGW_PREFIX}/bin/${MINGW_TOOL_PREFIX}-windres")

# Target environment search paths
set(CMAKE_FIND_ROOT_PATH "${MINGW_PREFIX}/${MINGW_TOOL_PREFIX}")

# Search programs in the host, libraries/headers in the target
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Default to Release builds for cross-compilation.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
