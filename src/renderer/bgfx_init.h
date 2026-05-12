#pragma once
#include "../config/config.h"
#include "../platform/sdl_window.h"

namespace silent_storm::renderer {

bool init(const platform::Window& window, const Config& cfg);

// Phase 1 Task 9: variant that takes an existing Win32 HWND + explicit width/height.
// Used when Nival creates its own window (via WinFrame.cpp) and we attach bgfx
// to that HWND instead of an SDL3 window.
bool init_hwnd(void* hwnd, int width, int height, const Config& cfg);

// Phase 1 Task 9: simplest C-style bootstrap used by Game/Main.cpp.  Doesn't
// pull <string>/Config across the modern-toolchain↔STLport ABI boundary.
// Reads the cfg file with our modern toolchain internally, then calls init_hwnd.
extern "C" bool ss_renderer_bootstrap(void* hwnd, int width, int height,
                                       const char* cfg_path);
extern "C" bool ss_renderer_load_shaders(const char* shader_dir);

void shutdown();

// Called from the event pump on SDL_EVENT_WINDOW_RESIZED.
void on_resize(int width, int height);

// Called once per frame from the main loop.
void begin_frame();
void end_frame();

} // namespace silent_storm::renderer
