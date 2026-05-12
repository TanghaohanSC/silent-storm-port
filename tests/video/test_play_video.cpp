// tests/video/test_play_video.cpp — Phase 3 acceptance harness
//
// Usage:
//     ss_video_test.exe <path_to_video>
//
// Defaults to upstream/Versions/Current/res/Video/Nival.bik if no argument
// is given.  Creates a borderless 1280x720 SDL3 window, initialises bgfx
// against its HWND, loads the ss_ui shader archetype, then calls
// silent_storm::renderer::play_video() — which blocks until the video ends
// or the user hits ESC / clicks / closes the window.
//
// WIN32 entry point (WinMain) is used because the renderer lib pulls in
// d3d9.lib + dxguid.lib, and matching silent_storm.exe's link model keeps
// the toolchain happy.  We also map argv via __argv / __argc so the user
// can drop a .bik onto the exe in Explorer.
#include "../../src/renderer/bgfx_init.h"
#include "../../src/renderer/shader_registry.h"
#include "../../src/renderer/video.h"
#include "../../src/config/config.h"
#include "../../src/platform/sdl_window.h"

#include <bgfx/bgfx.h>

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <windows.h>

namespace {

FILE* open_log() {
    static FILE* f = nullptr;
    if (!f) fopen_s(&f, "ss_video_test.log", "w");
    return f;
}
void log_line(const char* fmt, ...) {
    FILE* f = open_log();
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fflush(f);
}

int run(const char* video_path) {
    log_line("[video_test] starting, video=%s", video_path);

    silent_storm::Config cfg;
    cfg.display.width  = 1280;
    cfg.display.height = 720;
    cfg.display.mode   = silent_storm::WindowMode::Windowed;
    cfg.display.vsync  = true;
    cfg.renderer.backend = silent_storm::Backend::Auto;

    silent_storm::platform::Window window(cfg);
    if (!window.valid()) {
        log_line("[video_test] window creation failed");
        return 2;
    }

    void* hwnd = window.native_handle();
    auto sz = window.size();
    if (!silent_storm::renderer::init_hwnd(hwnd, sz.width, sz.height, cfg)) {
        log_line("[video_test] bgfx init failed");
        return 3;
    }

    if (!silent_storm::renderer::load_all_archetypes(nullptr)) {
        log_line("[video_test] shader archetypes failed to load (continuing)");
    }

    bool backend_ok = silent_storm::renderer::video_backend_available();
    log_line("[video_test] video backend available: %d", backend_ok ? 1 : 0);

    if (!silent_storm::renderer::play_video(video_path)) {
        log_line("[video_test] play_video returned false (skip/error)");
    }

    silent_storm::renderer::shutdown_registry();
    silent_storm::renderer::shutdown();

    log_line("[video_test] clean exit");
    return 0;
}

} // namespace

// Use WinMain for a windowed-subsystem binary; pull argv from __argv globals.
int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    const char* default_path =
        "C:\\Users\\Haohan\\Documents\\silent-storm\\upstream\\Versions\\Current\\res\\Video\\Nival.bik";
    const char* path = default_path;
    if (__argc >= 2 && __argv && __argv[1] && __argv[1][0] != '\0') {
        path = __argv[1];
    }
    return run(path);
}
