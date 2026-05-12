#pragma once

namespace silent_storm::platform {

enum class PumpResult { Continue, Quit };

// Pump SDL events for one frame. Returns Quit if the user requested exit.
// Calls into sdl_input_bridge to forward input events (see Task 4).
PumpResult pump_events();

} // namespace silent_storm::platform
