// video.h — Phase 3
//
// FFmpeg-backed fullscreen video playback for the Silent Storm port.
//
// Jan03's source has zero Bink API calls; the shipped 2003 binary used Bink
// but the open-source drop didn't include Nival's Bink integration glue.
// Phase 3 therefore *adds* a video subsystem rather than replacing one.
//
// FFmpeg ships a built-in Bink decoder, so `.bik` files can be decoded
// directly without an offline transcoding step. The runtime decodes one
// frame at a time, color-converts to BGRA8, uploads to a bgfx texture, and
// draws a fullscreen quad via the ss_ui shader archetype.
//
// API is deliberately minimal — Phase 3's acceptance is that the test exe
// (`ss_video_test.exe <path>`) opens a window, plays the file to the end (or
// until the user presses ESC / clicks), and returns cleanly.
#pragma once

namespace silent_storm::renderer {

// Blocking. Decodes `path` and submits one bgfx frame per decoded video
// frame; pumps SDL3 events between frames so the window stays responsive
// and the user can ESC out.
//
// Returns true if the video played to completion, false on error or user
// skip (ESC / window close / mouse click).
//
// Preconditions:
//   - bgfx::init has been called (see renderer::init_hwnd)
//   - shader registry has loaded the `ss_ui` archetype
//   - an SDL3 window is alive and event-pumpable from this thread
//
// The function does *not* take ownership of bgfx/SDL; it just borrows them
// while the video plays.
bool play_video(const char* path);

// Returns true if the runtime has been built with FFmpeg linked in. When
// false, `play_video` returns false immediately and logs a one-line note.
// Lets the integration point downgrade gracefully if vcpkg never produced
// an FFmpeg in time for a given build.
bool video_backend_available();

} // namespace silent_storm::renderer
