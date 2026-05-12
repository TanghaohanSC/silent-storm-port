#include "sdl_event_pump.h"
#include <SDL3/SDL.h>

namespace silent_storm::platform {

PumpResult pump_events() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                return PumpResult::Quit;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_WHEEL:
                // Task 4 hooks input forwarding here.
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                // Task 5 hooks bgfx::reset() here.
                break;
            default:
                break;
        }
    }
    return PumpResult::Continue;
}

} // namespace silent_storm::platform
