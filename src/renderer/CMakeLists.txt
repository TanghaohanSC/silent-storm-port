add_library(ss_renderer STATIC
    bgfx_init.cpp
    state_translator.cpp
)
target_include_directories(ss_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ss_renderer
    PUBLIC  bgfx bx bimg d3d9
    PRIVATE silent_storm::config silent_storm::platform
)
target_apply_compiler_warnings(ss_renderer)
add_library(silent_storm::renderer ALIAS ss_renderer)

# State translator unit test
add_executable(ss_state_translator_test
    ${CMAKE_SOURCE_DIR}/tests/renderer/test_state_translator.cpp
)
target_link_libraries(ss_state_translator_test PRIVATE ss_renderer)
target_apply_compiler_warnings(ss_state_translator_test)
