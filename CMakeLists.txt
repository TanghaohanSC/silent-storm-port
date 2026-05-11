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

# Engine subprojects — order matters for #include "../Misc/..." chains
# but the helper exposes each subproject's dir as PUBLIC include, so as
# long as targets are declared before being linked, order is flexible.
add_jan03_subproject(Misc)
add_jan03_subproject(MemoryMngr)
add_jan03_subproject(MemoryMngrDll)
add_jan03_subproject(MiscDll)
add_jan03_subproject(FileIO)
add_jan03_subproject(DBFormat)
add_jan03_subproject(Image)
add_jan03_subproject(Input)
add_jan03_subproject(FModSound)
add_jan03_subproject(OpenDynamix)
add_jan03_subproject(Script)

# Cross-subproject deps (best-guess from #include chains; refine as
# linker errors appear).
target_link_libraries(jan03_MemoryMngrDll PUBLIC jan03::MemoryMngr)
target_link_libraries(jan03_MiscDll PUBLIC jan03::Misc)
target_link_libraries(jan03_FileIO PUBLIC jan03::Misc jan03::MemoryMngr)
target_link_libraries(jan03_DBFormat PUBLIC jan03::Misc jan03::FileIO)
target_link_libraries(jan03_Image PUBLIC jan03::Misc jan03::zlib jan03::libpng)
target_link_libraries(jan03_FModSound PUBLIC jan03::Misc silent_storm_fmod_stub)
target_link_libraries(jan03_Input PUBLIC jan03::Misc)
target_link_libraries(jan03_Script PUBLIC jan03::Misc jan03::MemoryMngr)
target_link_libraries(jan03_OpenDynamix PUBLIC jan03::Misc)

add_executable(silent_storm WIN32 src/main_stub.cpp)
target_apply_compiler_warnings(silent_storm)
target_link_libraries(silent_storm PRIVATE
    silent_storm_stubs
    jan03::zlib
    jan03::libpng
    jan03::Misc
    jan03::MemoryMngr
    jan03::MemoryMngrDll
    jan03::MiscDll
    jan03::FileIO
    jan03::DBFormat
    jan03::Image
    jan03::Input
    jan03::FModSound
    jan03::OpenDynamix
    jan03::Script
)
