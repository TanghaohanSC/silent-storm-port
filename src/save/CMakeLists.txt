# Silent Storm port — Phase 7: flatbuffers-backed save system.
#
# The .fbs schema lives in this directory; the generated header
# `save_v1_generated.h` is committed alongside the schema (vendored)
# so the engine can build without invoking `flatc` at every configure
# step.  When the schema changes, regenerate with:
#
#   $flatc = "build/msvc-debug/vcpkg_installed/x64-windows/tools/flatbuffers/flatc.exe"
#   & $flatc --cpp --scoped-enums -o src/save src/save/save_v1.fbs
#
# (Host-arch tools live under the x64-windows triplet because vcpkg
# only builds the flatc executable for the host; the runtime headers
# come from the target triplet x86-windows.)

find_package(flatbuffers CONFIG REQUIRED)

add_library(ss_save STATIC save.cpp)
target_include_directories(ss_save PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ss_save PUBLIC flatbuffers::flatbuffers)
target_apply_compiler_warnings(ss_save)
add_library(silent_storm::save ALIAS ss_save)

# --- Phase 7 smoke test ----------------------------------------------------
add_executable(ss_save_test
    ${CMAKE_SOURCE_DIR}/tests/save/test_save_roundtrip.cpp
)
target_link_libraries(ss_save_test PRIVATE silent_storm::save)
target_apply_compiler_warnings(ss_save_test)
