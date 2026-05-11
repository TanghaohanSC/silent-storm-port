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
            /W0                    # legacy code, no actionable warnings
            /Zm300                 # PCH memory ceiling, matches original /Zm300
            /wd4828                # invalid CP-1251 chars in source comments
            /EHsc                  # exception model
            /MP                    # parallel per-target compilation
            /permissive            # legacy non-conformant code
            /Zc:noexceptTypes-     # noexcept NOT part of function type (pre-C++17)
            /Zc:ternary-           # legacy ternary operator behavior
        )
    endif()
    target_compile_definitions(${target} PRIVATE
        WIN32
        _WINDOWS
        _USRDLL
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_WARNINGS
        _SCL_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
        _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS  # legacy uses <hash_map>
        _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS    # legacy uses removed-in-C++17 stuff
        _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS
        # Force MSVC <math.h>/<cmath> to NOT introduce noexcept-qualified
        # float overloads at global scope. Without this, Nival's tools.h
        # `inline float fabs(float) {...}` collides with std's
        # `float fabs(float) noexcept` and gets C2382 (different exception spec).
        _HAS_CXX17=0
        _HAS_CXX20=0
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
        # Legacy code predates C++17, where `noexcept` became part of the
        # function type. Nival's tools.h redeclares fabs/cos/sin/etc as
        # non-noexcept overloads, which under C++17+ conflicts with std's
        # noexcept-qualified versions. Pin legacy targets to C++14.
        CXX_STANDARD 14
        CXX_EXTENSIONS OFF
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
