add_library(ss_renderer STATIC
    bgfx_init.cpp
    state_translator.cpp
    d3d9_facade.cpp
    d3d9_facade_resources.cpp
    shader_registry.cpp
    video.cpp
)
target_include_directories(ss_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ss_renderer
    PUBLIC  bgfx bx bimg d3d9 dxguid
    PRIVATE silent_storm::config silent_storm::platform
)
target_apply_compiler_warnings(ss_renderer)
add_library(silent_storm::renderer ALIAS ss_renderer)

# Phase 3: FFmpeg video backend.  Brought in via vcpkg (`ffmpeg` port with
# avcodec/avformat/swscale/swresample features).  If the find_package fails
# (vcpkg never produced ffmpeg for this build), video.cpp falls back to a
# stub `play_video` that just logs "Phase 3.5 — FFmpeg not yet linked".
find_package(FFMPEG QUIET)
if (FFMPEG_FOUND)
    message(STATUS "Phase 3: FFmpeg found via find_package — linking video backend")
    target_compile_definitions(ss_renderer PRIVATE SS_HAVE_FFMPEG)
    target_include_directories(ss_renderer PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(ss_renderer PRIVATE ${FFMPEG_LIBRARIES})
    if (DEFINED FFMPEG_LIBRARY_DIRS)
        target_link_directories(ss_renderer PRIVATE ${FFMPEG_LIBRARY_DIRS})
    endif()
else()
    message(WARNING "Phase 3: FFmpeg not found — video.cpp falls back to stub")
endif()

# State translator unit test
add_executable(ss_state_translator_test
    ${CMAKE_SOURCE_DIR}/tests/renderer/test_state_translator.cpp
)
target_link_libraries(ss_state_translator_test PRIVATE ss_renderer)
target_apply_compiler_warnings(ss_state_translator_test)

# Phase 3: video player smoke-test exe.  Opens an SDL3 window, calls
# play_video(argv[1]), exits on completion / ESC / mouse click.
add_executable(ss_video_test WIN32
    ${CMAKE_SOURCE_DIR}/tests/video/test_play_video.cpp
)
target_link_libraries(ss_video_test PRIVATE
    silent_storm::renderer
    silent_storm::platform
    silent_storm::config
    SDL3::SDL3
    bgfx
    bx
    bimg
)
target_apply_compiler_warnings(ss_video_test)
# Test exe wants the shaders built so it can load ss_ui at runtime.
add_dependencies(ss_video_test ss_shaders)
