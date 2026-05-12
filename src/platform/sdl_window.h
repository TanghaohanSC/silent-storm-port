#pragma once
#include <cstdint>
#include "../config/config.h"

struct SDL_Window;

namespace silent_storm::platform {

struct WindowSize { int width; int height; };

class Window {
public:
    // Creates the window per cfg. Returns nullptr-ish state if creation failed.
    explicit Window(const silent_storm::Config& cfg);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Native window handle for bgfx PlatformData::nwh
    void* native_handle() const;

    WindowSize size() const;
    bool valid() const { return window_ != nullptr; }

    // SDL_Window opaque pointer for the event pump
    SDL_Window* raw() const { return window_; }

private:
    SDL_Window* window_ = nullptr;
};

} // namespace silent_storm::platform
