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

# --- stlport shim ---
# Original Jan03 code includes <stl/_config.h> via every StdAfx.h. Real stlport
# vc7 is at upstream/Soft/SDK/stlport but uses VS .NET 2003 relative include
# paths (e.g. stlport/stddef.h does "../include/stddef.h") that don't resolve
# under modern MSVC. Even pure C files break because stlport's stddef.h hijacks
# the include path before MSVC's stddef.h.
#
# Solution: provide a TINY shim (port/third_party/stlport_shim/stl/_config.h)
# that satisfies the include directive but leaves all std:: types coming from
# modern MSVC. The original Jan03 source expects stlport-as-drop-in-std, which
# modern MSVC's std:: implementation already satisfies.
#
# If specific stlport-only headers are referenced (slist, rope, hash_map),
# add forward-shims here on demand.

set(STLPORT_SHIM_DIR "${CMAKE_SOURCE_DIR}/third_party/stlport_shim")
if(NOT EXISTS "${STLPORT_SHIM_DIR}/stl/_config.h")
    message(FATAL_ERROR "stlport shim missing at ${STLPORT_SHIM_DIR}/stl/_config.h")
endif()

add_library(legacy_stlport INTERFACE)
target_include_directories(legacy_stlport SYSTEM INTERFACE
    ${STLPORT_SHIM_DIR}
)
add_library(legacy::stlport ALIAS legacy_stlport)

message(STATUS "Legacy: stlport shim active at ${STLPORT_SHIM_DIR} (modern std:: backs it)")
