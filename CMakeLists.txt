cmake_minimum_required(VERSION 3.20)
project(NppTreeSitter VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# --------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------
option(NPP_TREESITTER_BUILD_TESTS "Build unit tests" OFF)

set(PLUGIN_NAME "NppTreeSitter")

# --------------------------------------------------------------------------
# Dependencies — tree-sitter core
#
# You can either:
#   1. Use FetchContent (default) to pull tree-sitter from GitHub, or
#   2. Set TREESITTER_ROOT to a local checkout.
# --------------------------------------------------------------------------
include(FetchContent)

if(NOT DEFINED TREESITTER_ROOT)
    FetchContent_Declare(
        tree_sitter
        GIT_REPOSITORY https://github.com/tree-sitter/tree-sitter.git
        GIT_TAG        v0.25.6
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(tree_sitter)
    set(TS_LIB_DIR "${tree_sitter_SOURCE_DIR}/lib")
else()
    set(TS_LIB_DIR "${TREESITTER_ROOT}/lib")
endif()

# Build tree-sitter as a static library.
#
# tree-sitter ships a convenience "lib.c" that #includes all the
# implementation files.  We compile ONLY lib.c to avoid duplicate-symbol
# errors from globbing src/*.c (which would pull in the same files twice).
add_library(tree_sitter_core STATIC "${TS_LIB_DIR}/src/lib.c")
target_include_directories(tree_sitter_core
    PUBLIC  "${TS_LIB_DIR}/include"
    PRIVATE "${TS_LIB_DIR}/src"
)

# --------------------------------------------------------------------------
# Scintilla / Lexilla / Notepad++ headers (vendored minimal stubs)
# --------------------------------------------------------------------------
set(SCINTILLA_INCLUDE "${CMAKE_SOURCE_DIR}/include/scintilla")
set(NPP_INCLUDE       "${CMAKE_SOURCE_DIR}/include/npp")

# --------------------------------------------------------------------------
# Plugin DLL
# --------------------------------------------------------------------------
set(PLUGIN_SOURCES
    src/plugin_main.cpp
    src/ts_lexer_bridge.cpp
    src/ts_query_runner.cpp
    src/style_map.cpp
    src/grammar_registry.cpp
    src/plugin_exports.cpp
)

add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCES})

target_include_directories(${PLUGIN_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${SCINTILLA_INCLUDE}"
    "${NPP_INCLUDE}"
    "${TS_LIB_DIR}/include"
)

target_link_libraries(${PLUGIN_NAME} PRIVATE tree_sitter_core)

# --------------------------------------------------------------------------
# Export control
#
# The .def file controls which symbols are exported from the DLL.
# MSVC and MinGW handle .def files differently in CMake.
# --------------------------------------------------------------------------
set(DEF_FILE "${CMAKE_SOURCE_DIR}/src/plugin_exports.def")

if(MSVC)
    # MSVC: pass .def via linker flag.
    target_link_options(${PLUGIN_NAME} PRIVATE "/DEF:${DEF_FILE}")
else()
    # MinGW (GCC): pass .def as a source file — CMake/GNU ld handles it.
    target_sources(${PLUGIN_NAME} PRIVATE "${DEF_FILE}")
endif()

# --------------------------------------------------------------------------
# Compiler-specific flags
# --------------------------------------------------------------------------
if(MINGW)
    # MinGW needs to link against Windows system libraries.
    target_link_libraries(${PLUGIN_NAME} PRIVATE
        shlwapi     # path helpers
        comctl32    # InitCommonControlsEx (SysLink control)
        shell32     # ShellExecuteW
    )

    # Produce a clean DLL with no lib prefix.
    # Enable position-independent code (default for shared libs, but be explicit).
    target_compile_options(${PLUGIN_NAME} PRIVATE -fPIC)

    # Static-link libgcc/libstdc++ so the DLL has no runtime dependencies
    # beyond the Windows system DLLs.
    target_link_options(${PLUGIN_NAME} PRIVATE
        -static-libgcc
        -static-libstdc++
        -static
        # Ensure the DLL doesn't export every symbol — only those in the .def.
        -Wl,--exclude-all-symbols
    )
endif()

set_target_properties(${PLUGIN_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME ${PLUGIN_NAME}
    SUFFIX ".dll"
)

# --------------------------------------------------------------------------
# Install
# --------------------------------------------------------------------------
install(TARGETS ${PLUGIN_NAME} RUNTIME DESTINATION "plugins/${PLUGIN_NAME}")
install(DIRECTORY config/ DESTINATION "plugins/${PLUGIN_NAME}/config")
