#include "sdl_window.h"
#include <SDL3/SDL.h>
#include <cstdio>

namespace silent_storm::platform {

namespace {
SDL_WindowFlags mode_flags(const silent_storm::Config& cfg) {
    SDL_WindowFlags f = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (cfg.display.mode == silent_storm::WindowMode::Fullscreen) {
        f |= SDL_WINDOW_FULLSCREEN;
    }
    return f;
}
} // namespace

Window::Window(const silent_storm::Config& cfg) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_InitSubSystem(VIDEO) failed: %s\n", SDL_GetError());
        return;
    }

    int w = cfg.display.width  > 0 ? cfg.display.width  : 1920;
    int h = cfg.display.height > 0 ? cfg.display.height : 1080;

    if (cfg.display.mode == silent_storm::WindowMode::FullscreenDesktop) {
        const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
        if (dm) { w = dm->w; h = dm->h; }
    }

    window_ = SDL_CreateWindow("Silent Storm (port)", w, h, mode_flags(cfg));
    if (!window_) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return;
    }
    if (cfg.display.mode == silent_storm::WindowMode::FullscreenDesktop) {
        SDL_SetWindowFullscreen(window_, true);
    }
}

Window::~Window() {
    if (window_) SDL_DestroyWindow(window_);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void* Window::native_handle() const {
    if (!window_) return nullptr;
    SDL_PropertiesID props = SDL_GetWindowProperties(window_);
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
}

WindowSize Window::size() const {
    WindowSize s{0, 0};
    if (window_) SDL_GetWindowSize(window_, &s.width, &s.height);
    return s;
}

} // namespace silent_storm::platform
