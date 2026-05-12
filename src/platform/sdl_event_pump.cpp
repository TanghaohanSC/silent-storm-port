#include "sdl_event_pump.h"
#include "sdl_input_bridge.h"
#include "../renderer/bgfx_init.h"
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
                forward_to_ninput(ev);
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                int w = ev.window.data1;
                int h = ev.window.data2;
                if (w > 0 && h > 0) {
                    silent_storm::renderer::on_resize(w, h);
                }
                break;
            }
            default:
                break;
        }
    }
    return PumpResult::Continue;
}

} // namespace silent_storm::platform
