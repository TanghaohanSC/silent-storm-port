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

# --- FMOD 3.x -> miniaudio (Phase 2) ---
# Single .cpp defines MA_IMPLEMENTATION and emits the entire miniaudio
# 0.11.25 implementation. Routes the 67 FSOUND_* symbols to ma_engine /
# ma_sound. On Windows, miniaudio auto-selects WASAPI (primary) /
# DirectSound / WinMM backends; we link the supporting system libs.
add_library(silent_storm_fmod_stub STATIC fmod_stub.cpp)
target_include_directories(silent_storm_fmod_stub PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    # Provides <fmod.h> shim that forwards to fmod_stub.h (Nival code
    # does #include <fmod.h>; the real fmod.h is proprietary).
    ${CMAKE_CURRENT_SOURCE_DIR}/fmod_shim_headers
)
target_include_directories(silent_storm_fmod_stub PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/miniaudio
)
if(WIN32)
    # WASAPI/DSound need ole32 (CoTaskMemAlloc, IIDs); WinMM is the
    # legacy fallback. miniaudio also pulls in user32/advapi32 which
    # are already in the default lib set.
    target_link_libraries(silent_storm_fmod_stub PUBLIC ole32 winmm)
    # miniaudio.h is ~700KB of dense C; under our /W4 policy it would
    # drown the log. Compile this TU with /W0 — the public surface
    # (our stub wrappers) is small and well-typed.
    target_compile_options(silent_storm_fmod_stub PRIVATE /W0)
    target_compile_definitions(silent_storm_fmod_stub PRIVATE
        _CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN
    )
endif()

# --- ADOImport runtime stub: NDatabase symbols without ADO COM ---
# Upstream ADOImport links against msado15.dll (Microsoft Access ADO). Only
# the Import()/Refresh() paths use it, and those are dev-time only. This stub
# provides the runtime NDatabase symbols so the Main DLL links and game.db
# loads via Serialize().
add_library(silent_storm_ado_stub STATIC ado_stub.cpp)
target_link_libraries(silent_storm_ado_stub PUBLIC legacy::stlport jan03::Misc jan03::FileIO jan03::DBFormat)
set_target_properties(silent_storm_ado_stub PROPERTIES
    CXX_STANDARD 14
    CXX_EXTENSIONS OFF
)
target_compile_definitions(silent_storm_ado_stub PRIVATE
    _CRT_SECURE_NO_WARNINGS _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
    _HAS_CXX17=0
)
target_compile_options(silent_storm_ado_stub PRIVATE /W0 /permissive /Zc:noexceptTypes-
    /FI${CMAKE_SOURCE_DIR}/third_party/stlport_shim/legacy_compat.h)

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
