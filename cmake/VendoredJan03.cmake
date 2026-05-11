# Wrap the upstream Jan03 codebase as CMake targets.
#
# Usage:
#   include(LegacyDeps)        # exposes legacy::stlport
#   include(VendoredJan03)     # this file
#   add_jan03_subproject(zlib LANGUAGES C)
#   add_jan03_subproject(libpng LANGUAGES C)
#   add_jan03_subproject(Misc)
#   # ... etc, then wire up dependencies between them.
#
# Notes:
#  - Each subproject lives at JAN03_ROOT/<name>/ with its own StdAfx.h PCH
#    pattern. The function configures /Yc /Yu via target_precompile_headers
#    when a StdAfx.h is present.
#  - Legacy code: warnings are silenced project-wide (/W0). Per-subproject
#    bugs are not ours to fix; downstream Phase 1+ work will replace these
#    files wholesale, not clean them in place.
#  - Most subprojects compile to a STATIC library by default. Main and Game
#    are special and handled outside this helper.

if(NOT DEFINED UPSTREAM_ROOT)
    set(UPSTREAM_ROOT "${CMAKE_SOURCE_DIR}/../upstream")
endif()
set(JAN03_ROOT "${UPSTREAM_ROOT}/Soft/Andy/Jan03/a5dll")

if(NOT EXISTS "${JAN03_ROOT}/A5.sln")
    message(FATAL_ERROR
        "Jan03 source not found at ${JAN03_ROOT}.\n"
        "Verify upstream/ is cloned and Soft/Andy/Jan03/a5dll/A5.sln exists.")
endif()

# Common defines + flags applied to every Jan03 subproject.
function(_jan03_apply_legacy_flags target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W0           # legacy code, no warnings expected to be actionable by us
            /Zm300        # PCH memory ceiling, matches original /Zm300
            /wd4828       # invalid character in source (CP-1251 / cyrillic comments)
            /EHsc         # exception model — original used /EHsc
            /MP           # parallel per-target compilation
        )
        # The original code is pre-C++17 and assumes implicit
        # `for(int i=0; ...; ++i) { ... } for(int i=0; ...; ...)` (no scope warning).
        # Force MSVC into permissive mode for legacy.
        target_compile_options(${target} PRIVATE /permissive)
    endif()
    target_compile_definitions(${target} PRIVATE
        WIN32
        _WINDOWS
        _USRDLL
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
    )
    target_link_libraries(${target} PRIVATE legacy::stlport)
    # Subproject's own dir as public include so siblings can do
    # #include "../Misc/Foo.h" the way Jan03 code does.
    target_include_directories(${target} PUBLIC
        ${JAN03_ROOT}
    )
endfunction()

# add_jan03_subproject(<name> [LANGUAGES C|CXX|C CXX] [TYPE STATIC|OBJECT])
function(add_jan03_subproject NAME)
    cmake_parse_arguments(SP "" "TYPE" "LANGUAGES" ${ARGN})
    if(NOT SP_TYPE)
        set(SP_TYPE STATIC)
    endif()
    if(NOT SP_LANGUAGES)
        set(SP_LANGUAGES CXX C)
    endif()

    set(SP_DIR "${JAN03_ROOT}/${NAME}")
    if(NOT EXISTS "${SP_DIR}")
        message(FATAL_ERROR "Jan03 subproject '${NAME}' not found at ${SP_DIR}")
    endif()

    # Gather sources from this subproject only (non-recursive — flat layout).
    set(_globs "")
    if("CXX" IN_LIST SP_LANGUAGES)
        list(APPEND _globs "${SP_DIR}/*.cpp")
    endif()
    if("C" IN_LIST SP_LANGUAGES)
        list(APPEND _globs "${SP_DIR}/*.c")
    endif()
    file(GLOB SP_SOURCES CONFIGURE_DEPENDS ${_globs})

    if(NOT SP_SOURCES)
        message(FATAL_ERROR "Jan03 subproject '${NAME}' has no .cpp/.c sources under ${SP_DIR}")
    endif()

    set(TARGET_NAME "jan03_${NAME}")
    add_library(${TARGET_NAME} ${SP_TYPE} ${SP_SOURCES})
    set_target_properties(${TARGET_NAME} PROPERTIES
        FOLDER "Jan03"
        OUTPUT_NAME ${NAME}
    )
    target_include_directories(${TARGET_NAME} PUBLIC ${SP_DIR})

    _jan03_apply_legacy_flags(${TARGET_NAME})

    # PCH wiring: if StdAfx.h exists in this subproject, configure it.
    # CMake handles /Yc on StdAfx.cpp + /Yu on the rest automatically.
    if(EXISTS "${SP_DIR}/StdAfx.h")
        target_precompile_headers(${TARGET_NAME} PRIVATE
            "${SP_DIR}/StdAfx.h"
        )
    endif()

    # Convenience alias so callers can `target_link_libraries(... jan03::Misc)`
    add_library(jan03::${NAME} ALIAS ${TARGET_NAME})

    message(STATUS "Jan03: added ${TARGET_NAME} (${SP_TYPE}, ${SP_LANGUAGES}, $<COMMA>${SP_SOURCES}$<COMMA> files)")
endfunction()
