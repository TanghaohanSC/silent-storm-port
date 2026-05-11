cmake_minimum_required(VERSION 3.25)

project(silent_storm
    VERSION 0.1.0
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(LegacyDeps)        # legacy::stlport
include(VendoredJan03)     # add_jan03_subproject()

add_subdirectory(src/stubs)

# Phase 0 / Task 6: add Jan03 subprojects one batch at a time.
# Start with the leaf vendored libs (zlib, libpng) — no PCH, pure C,
# no dependencies on other subprojects.
add_jan03_subproject(zlib LANGUAGES C)
add_jan03_subproject(libpng LANGUAGES C)
target_link_libraries(jan03_libpng PUBLIC jan03::zlib)
# Nival's Misc/Tools.h defines `externA5` as `extern __declspec(dllimport)`
# for runtime A5.dll imports. They sprinkled `externA5` through their
# vendored libpng's png.h instead of plain `extern`. For static-linked
# libpng, plain `extern` is correct.
target_compile_definitions(jan03_libpng PRIVATE externA5=extern)

# First C++ subproject test (PCH + stlport shim verification).
add_jan03_subproject(Misc)

add_executable(silent_storm WIN32 src/main_stub.cpp)
target_apply_compiler_warnings(silent_storm)
target_link_libraries(silent_storm PRIVATE
    silent_storm_stubs
    jan03::zlib
    jan03::libpng
    jan03::Misc
)
