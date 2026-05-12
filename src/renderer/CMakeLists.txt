add_library(ss_renderer STATIC bgfx_init.cpp)
target_include_directories(ss_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ss_renderer
    PUBLIC  bgfx bx bimg
    PRIVATE silent_storm::config silent_storm::platform
)
target_apply_compiler_warnings(ss_renderer)
add_library(silent_storm::renderer ALIAS ss_renderer)
