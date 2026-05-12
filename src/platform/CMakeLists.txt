add_library(ss_platform STATIC
    sdl_window.cpp
    sdl_event_pump.cpp
    sdl_input_bridge.cpp
)
target_include_directories(ss_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ss_platform
    PUBLIC SDL3::SDL3
    PRIVATE
        silent_storm::config
        jan03::Input
)
target_apply_compiler_warnings(ss_platform)
add_library(silent_storm::platform ALIAS ss_platform)
