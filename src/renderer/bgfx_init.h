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

// T10/T11: current framebuffer dimensions (updated by init_hwnd + on_resize).
int get_width();
int get_height();

// T11: the HUD integer scale that was applied during the last SetTransform(ortho).
// Returns 1 when no scale is active.  Used by the input bridge to inverse-scale
// mouse coordinates before passing them to Nival's hit-test.
int get_hud_scale();

// Internal: called by D3D9Facade::SetTransform to record the active HUD scale.
void set_hud_scale(int scale);

// Called once per frame from the main loop.
void begin_frame();
void end_frame();

} // namespace silent_storm::renderer

// ---------------------------------------------------------------------------
// Phase 1.5 r3 — debug text relay.  Lets upstream (Nival's CTextDraw /
// CImageDraw path, which is compiled with STLport) push a short ASCII label
// + on-screen position into a small ring buffer.  end_frame() flushes the
// buffer into bgfx::dbgTextPrintf so we get VISIBLE text overlays in lieu
// of a real font atlas binding.
//
// POD-only / extern "C" so it can be called across the modern-toolchain ↔
// STLport ABI boundary.  Coordinates are in 1024x768 virtual screen space
// (the same space Nival's UI uses) — end_frame() rescales to the current
// dbg-text grid (80 cols × 24 rows, roughly).
// ---------------------------------------------------------------------------
extern "C" {
// Push one label.  abgr is 0xAABBGGRR; text is null-terminated ASCII.  If
// the buffer is full the call is silently dropped.  Cleared automatically
// at end_frame() so callers re-submit every frame.
void ss_dbg_text_push(int virtX, int virtY, unsigned attr, const char* text);
// Set the "frame banner" message shown on row 0.  text == NULL clears it.
void ss_dbg_text_banner(const char* text);
// Queue a colored quad for this frame in 1024x768 virtual coords.  abgr is
// little-endian 0xAABBGGRR.  Cleared automatically at end_frame().  Used by
// CTextDraw/CImageDraw to indicate the *real* bounding rect on screen even
// before font/texture geometry is wired up — flushed via ss_ui shader on
// view 0 alongside the dbg_tri.
void ss_dbg_rect_push(int virtX1, int virtY1, int virtX2, int virtY2, unsigned abgr);

// Phase 1.5 r4 — REAL bitmap glyph relay.  Pushes one ASCII string to be
// rendered as a row of textured quads sampled from an 8x16 Consolas atlas
// uploaded into a 128x128 BGRA8 bgfx texture.  Each character produces one
// quad (6 vertices) emitted on view 0 via ss_ui with alpha blend.  abgr is
// the per-vertex color modulating the glyph coverage.  scale_x/y are
// integer multipliers (1, 2, ...) applied to the 8x16 cell size — pass 1
// for native size.  Coords are 1024x768 virtual; clipped silently to the
// available glyph-quad pool.  Cleared automatically at end_frame().
void ss_dbg_glyph_push(int virtX, int virtY, unsigned abgr, int scale_x, int scale_y, const char* text);
} // extern "C"
