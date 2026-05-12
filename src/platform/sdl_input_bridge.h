#pragma once
union SDL_Event;

namespace silent_storm::platform {

// Called from the event pump for each input-related SDL event.
// Translates SDL3 events into NInput::SMessage and pushes them to Nival's queue.
void forward_to_ninput(const SDL_Event& ev);

}  // namespace silent_storm::platform
