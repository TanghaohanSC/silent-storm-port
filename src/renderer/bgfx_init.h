#pragma once
#include "../config/config.h"
#include "../platform/sdl_window.h"

namespace silent_storm::renderer {

bool init(const platform::Window& window, const Config& cfg);
void shutdown();

// Called from the event pump on SDL_EVENT_WINDOW_RESIZED.
void on_resize(int width, int height);

// Called once per frame from the main loop.
void begin_frame();
void end_frame();

} // namespace silent_storm::renderer
