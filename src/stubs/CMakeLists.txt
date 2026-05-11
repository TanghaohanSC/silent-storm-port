# Stub libraries replacing commercial / proprietary deps the Jan03 source
# references but whose runtime support isn't available.
#
# Per Task 2 inventory:
#   - Bink stub:       NOT needed (zero Bink API calls in Jan03 source)
#   - StarForce stub:  NOT needed (zero StarForce calls in Jan03 source)
#   - DirectPlay stub: NOT needed (zero DirectPlay calls in Jan03 source)
#   - DirectInput8:    link against system dinput8.lib (modern WinSDK has it)
#   - FMOD 3.x:        STUB (67 symbols, silent fake)
#   - LifeStudioHead:  STUB (header-only, permanent — face animation dropped)

# --- FMOD 3.x silent fake ---
add_library(silent_storm_fmod_stub STATIC fmod_stub.cpp)
target_include_directories(silent_storm_fmod_stub PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_apply_compiler_warnings(silent_storm_fmod_stub)

# --- LifeStudioHeadAPI permanent header-only stub ---
add_library(silent_storm_lshead_stub INTERFACE)
target_include_directories(silent_storm_lshead_stub SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/lshead
)

# Aggregate alias so the main exe can just link one target.
add_library(silent_storm_stubs INTERFACE)
target_link_libraries(silent_storm_stubs INTERFACE
    silent_storm_fmod_stub
    silent_storm_lshead_stub
)
