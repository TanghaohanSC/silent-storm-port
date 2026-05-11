# Vendored dependencies inherited from the 2003 codebase.
#
# Per Task 2 inventory:
#   - stlport vc7: 28 of 31 Jan03 subprojects pull <stl/_config.h> in
#     their precompiled header. Migration to modern std:: deferred to v2;
#     v1 vendors stlport via the upstream copy already in
#     upstream/Soft/SDK/stlport/.
#
# UPSTREAM_ROOT is set by the parent CMakeLists when this module is included.

if(NOT DEFINED UPSTREAM_ROOT)
    set(UPSTREAM_ROOT "${CMAKE_SOURCE_DIR}/../upstream")
endif()

# --- stlport (vc7 build vendored in upstream/Soft/SDK/stlport) ---
set(STLPORT_INCLUDE_DIR "${UPSTREAM_ROOT}/Soft/SDK/stlport/stlport")

if(NOT EXISTS "${STLPORT_INCLUDE_DIR}/stl/_config.h")
    message(FATAL_ERROR
        "stlport headers not found at ${STLPORT_INCLUDE_DIR}.\n"
        "Expected upstream/Soft/SDK/stlport/stlport/stl/_config.h "
        "(verify upstream/ is cloned and stlport is checked out).")
endif()

add_library(legacy_stlport INTERFACE)
target_include_directories(legacy_stlport SYSTEM INTERFACE
    ${STLPORT_INCLUDE_DIR}
)
# stlport headers conflict with system <new>, <iostream> etc. if both visible.
# Inject a project-wide preprocessor symbol that stlport headers test for.
target_compile_definitions(legacy_stlport INTERFACE
    _STLP_USE_STATIC_LIB
    _STLP_USE_NEWALLOC
)
add_library(legacy::stlport ALIAS legacy_stlport)

message(STATUS "Legacy: stlport vc7 vendored from ${STLPORT_INCLUDE_DIR}")
