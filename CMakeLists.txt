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

add_executable(silent_storm WIN32 src/main_stub.cpp)
target_apply_compiler_warnings(silent_storm)
target_link_libraries(silent_storm PRIVATE
    silent_storm_stubs
    jan03::zlib
)
