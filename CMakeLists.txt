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

# --- Phase 1 modern deps ---
find_package(SDL3 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)

# bgfx via git submodule. Disable D3D9 backend to avoid d3d9.h symbol
# collision with our IDirect3DDevice9 facade (Phase 1 Task 6+).
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
set(BGFX_CONFIG_RENDERER_VULKAN ON CACHE BOOL "" FORCE)
set(BGFX_CONFIG_RENDERER_DIRECT3D11 ON CACHE BOOL "" FORCE)
set(BGFX_CONFIG_RENDERER_DIRECT3D12 OFF CACHE BOOL "" FORCE)
set(BGFX_CONFIG_RENDERER_DIRECT3D9 OFF CACHE BOOL "" FORCE)
set(BGFX_CONFIG_RENDERER_OPENGL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/bgfx EXCLUDE_FROM_ALL)

add_subdirectory(src/stubs)
add_subdirectory(src/config)
add_subdirectory(src/platform)
add_subdirectory(src/renderer)
add_subdirectory(src/shaders)    # Phase 1 T8: bgfx shader archetypes
add_subdirectory(src/save)       # Phase 7: flatbuffers save game system
# Make sure compiled .bin shaders exist before the renderer-using exe links.
add_dependencies(ss_renderer ss_shaders)

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
add_jan03_subproject(FileIO)
add_jan03_subproject(DBFormat)
add_jan03_subproject(Image)
add_jan03_subproject(Input)
add_jan03_subproject(FModSound)
add_jan03_subproject(OpenDynamix)
add_jan03_subproject(Script)
add_jan03_subproject(MiscDll)
# Skipped per Phase 0 scope:
# - MemoryMngrDll: MFC-style debug DLL (TRACE1 macros etc.); revisit if linker needs
# - ADOImport: uses `#import "msado15.dll"` for Microsoft Access ADO COM
#   interop — only used at dev time to (re)generate game.db. We stub the
#   runtime-needed NDatabase symbols in src/stubs/ado_stub.cpp.

target_link_libraries(jan03_FileIO PUBLIC jan03::Misc jan03::MemoryMngr)
target_link_libraries(jan03_DBFormat PUBLIC jan03::Misc jan03::FileIO)
# r54: DataFormat.cpp now back-pops file-local CRndModel/CRndConstructionPart
# variants directly from CDBTableDataStorage rows — needs the storage header.
target_include_directories(jan03_DBFormat PRIVATE ${CMAKE_SOURCE_DIR}/src/stubs)
target_link_libraries(jan03_Image PUBLIC jan03::Misc jan03::zlib jan03::libpng)
target_include_directories(jan03_Image PRIVATE ${CMAKE_SOURCE_DIR}/src/stubs/s3tc_shim)  # proprietary S3TC SDK stub
target_link_libraries(jan03_FModSound PUBLIC jan03::Misc silent_storm_fmod_stub)
target_link_libraries(jan03_Input PUBLIC jan03::Misc)
target_compile_definitions(jan03_Input PRIVATE SS_USE_SDL_INPUT)
target_link_libraries(jan03_Script PUBLIC jan03::Misc jan03::MemoryMngr)
target_link_libraries(jan03_OpenDynamix PUBLIC jan03::Misc)
target_link_libraries(jan03_MiscDll PUBLIC jan03::Misc)

# Main: the big game logic library. 271 .cpp files of pre-modern C++.
# Built as STATIC (was OBJECT pre-Phase 1.5) so we can use /WHOLEARCHIVE
# on it. Many REGISTER_SAVELOAD_CLASS static initializers are otherwise
# dead-stripped by the linker since no .cpp directly references them —
# game.db then tries to deserialize unregistered class typeIDs and asserts.
add_jan03_subproject(Main TYPE STATIC)
target_link_libraries(jan03_Main PUBLIC
    jan03::Misc
    jan03::MemoryMngr
    jan03::FileIO
    jan03::DBFormat
    jan03::Image
    jan03::Input
    jan03::FModSound
    jan03::OpenDynamix
    jan03::Script
    jan03::MiscDll
    silent_storm_ado_stub     # NDatabase runtime symbols (replaces ADOImport)
    silent_storm_dbts_stub    # r11: CDBTableDataStorage (typeID 0xA1843130)
    silent_storm_stubs        # LifeStudioHeadAPI shim
    silent_storm_fmod_stub
)
target_include_directories(jan03_Main PRIVATE
    ${CMAKE_SOURCE_DIR}/src/stubs/lshead
    ${CMAKE_SOURCE_DIR}/src/stubs/s3tc_shim
)
target_compile_definitions(jan03_Main PRIVATE SS_USE_BGFX_FACADE)

# r29: dbfill — schema-based static record field fill. Depends on jan03_DBFormat
# (NDb record class defs) + silent_storm_dbts_stub (CDBTableDataStorage def) +
# silent_storm_ado_stub (NDatabase::GetTable). Added here, after those targets
# exist. Exposes one C-style entry point that ado_stub's PromoteRecordsFromStorage
# calls at the tail of NDatabase::Serialize().
add_subdirectory(src/dbfill)

# --- Game.exe: WinMain + main loop. Links statically against everything. ---
set(JAN03_GAME_DIR "${CMAKE_SOURCE_DIR}/../upstream/Soft/Andy/Jan03/a5dll/Game")
add_executable(silent_storm WIN32
    ${JAN03_GAME_DIR}/Main.cpp
    ${JAN03_GAME_DIR}/WinFrame.cpp
    ${JAN03_GAME_DIR}/StdAfx.cpp
)
# Game has its own StdAfx.h PCH
target_precompile_headers(silent_storm PRIVATE ${JAN03_GAME_DIR}/StdAfx.h)
target_include_directories(silent_storm PRIVATE
    ${JAN03_GAME_DIR}
    ${CMAKE_SOURCE_DIR}/../upstream/Soft/Andy/Jan03/a5dll
    ${CMAKE_SOURCE_DIR}/../upstream/Soft/Andy/Jan03/a5dll/Main
)
set_target_properties(silent_storm PROPERTIES
    CXX_STANDARD 14
    CXX_EXTENSIONS OFF
)
target_compile_definitions(silent_storm PRIVATE
    WIN32 _WINDOWS _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS
    _HAS_CXX17=0 _HAS_CXX20=0
    _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
    _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    NOMINMAX WIN32_LEAN_AND_MEAN
    SS_USE_BGFX_FACADE     # phase 1 T9 — enable bgfx bootstrap in Main.cpp
    SS_USE_SDL_INPUT       # phase 1.5 r6 — pump Win32 msgs into NInput in WndProc
)
if(MSVC)
    target_compile_options(silent_storm PRIVATE
        /W0 /Zm300 /wd4828 /EHsc /MP /permissive
        /Zc:noexceptTypes- /Zc:ternary-
        /FI${CMAKE_SOURCE_DIR}/third_party/stlport_shim/legacy_compat.h
    )
    # Phase 1.5: prevent linker dead-strip of REGISTER_SAVELOAD_CLASS
    # static initializers in Main. Without this, game.db deserialization
    # asserts on missing class typeIDs. Bisecting — try just Main first.
    target_link_options(silent_storm PRIVATE
        /WHOLEARCHIVE:Main
        /WHOLEARCHIVE:DBFormat
        /WHOLEARCHIVE:silent_storm_dbts_stub
        /MAP:$<TARGET_FILE_DIR:silent_storm>/silent_storm.map
    )
endif()
target_link_libraries(silent_storm PRIVATE
    legacy::stlport
    jan03::Main
    silent_storm::renderer
    silent_storm::platform
    silent_storm_stubs
    silent_storm_fmod_stub
    silent_storm_ado_stub
    silent_storm_dbts_stub    # r11: CDBTableDataStorage class registration
    silent_storm_dbfill       # r29: schema-based record body field fill
    jan03::zlib
    jan03::libpng
    jan03::Misc
    jan03::MemoryMngr
    jan03::FileIO
    jan03::DBFormat
    jan03::Image
    jan03::Input
    jan03::FModSound
    jan03::OpenDynamix
    jan03::Script
    jan03::MiscDll
    # System libs (DirectX, etc.) — modern WinSDK ships these.
    dxguid d3d9 dinput8 winmm odbc32 odbccp32
)

# r43: stage Steam's LifeStudioHeadAPI.dll + its private dependencies next to
# silent_storm.exe so the import lib (port/src/stubs/lshead/lshead.lib) can
# resolve at runtime. SILENT_STORM_LSHEAD_DLL is the Steam path set in
# src/stubs/CMakeLists.txt. LifeStudioHeadAPI.dll DT_NEEDS IMorphFile.dll +
# TriangLib.dll (TriangLib also needs IMorphFile). All three ship with Steam.
if(EXISTS "${SILENT_STORM_LSHEAD_DLL}")
    get_filename_component(_lshead_steam_dir "${SILENT_STORM_LSHEAD_DLL}" DIRECTORY)
    foreach(_dll LifeStudioHeadAPI.dll IMorphFile.dll TriangLib.dll)
        if(EXISTS "${_lshead_steam_dir}/${_dll}")
            add_custom_command(TARGET silent_storm POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_lshead_steam_dir}/${_dll}"
                    "$<TARGET_FILE_DIR:silent_storm>/${_dll}"
                COMMENT "r43: staging ${_dll} from Steam install -> bin/"
                VERBATIM
            )
        else()
            message(WARNING "r43: ${_dll} not found in Steam dir; face animation may fail at runtime")
        endif()
    endforeach()
endif()
