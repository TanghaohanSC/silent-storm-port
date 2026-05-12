# Phase 1.5 Round 2 — Runtime patches

Continues from `p1_5_runtime.md` (Round 1). Round 2 goal: make Nival's main
menu actually visible — i.e. move past "the exe runs in the loop but the
window stays a uniform gray".

## Iteration 1 — Blocker A: bgfx back-buffer sized to window

**Symptom**: `bgfx::init` was called from `ss_renderer_bootstrap` with the
window's then-current client size of 100x100 (the window had not been
sized yet at Nival's `NWinFrame::InitApplication` time). After `SetMode`
ran, the Win32 window grew to 1024x768 but bgfx kept rendering its tiny
viewport in the top-left corner.

**Diagnosis**: `D3D9Facade::Reset(D3DPRESENT_PARAMETERS*)` was a no-op
stub. Worse, `ResetDevice()` on the bgfx facade path only *injected* the
facade and never called `pDevice->Reset(&pp)`, so even a proper Reset
implementation would not have been triggered.

**Fix**:

1. `port/src/renderer/d3d9_facade.cpp` — `Reset` now reads
   `pPP->BackBufferWidth/Height` and forwards to
   `silent_storm::renderer::on_resize(w,h)` which calls
   `bgfx::reset(w,h,...)` + `bgfx::setViewRect(0, …)`.
2. `upstream/.../Main/Gfx.cpp::ResetDevice()` — after injecting the
   facade, explicitly call `(*ppDev)->Reset(&pp)` so the back-buffer
   resizes to the chosen mode.
3. `port/src/platform/sdl_event_pump.cpp` —
   `SDL_EVENT_WINDOW_RESIZED` + `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`
   forward to `on_resize` (defensive — Nival has its own message pump,
   but the SDL pump is in the loop for the next refactor).
4. `upstream/.../Game/Main.cpp::WinMain` — call
   `user32!SetProcessDPIAware` at process start so Win32 client sizes
   reported to bgfx match physical pixels (otherwise on HiDPI display
   Windows lies to the DPI-unaware process and bgfx's D3D11 swapchain
   ends up stretched to a quarter of the visible window).

**Sub-fix — view 0 pin**: `D3D9Facade::SetRenderTarget` previously
bumped `current_view_id_` on every non-null SetRenderTarget call. But
Nival calls `SetRenderTarget(0, pScreenColor)` to set the *screen* RT
during `CRenderContext::Use → ApplyRenderTarget → SetRT`. Bumping the
view id meant Nival's `Clear` color ended up on view 1, 2, 3… (none of
which are presented to the swapchain), while view 0 stayed at bgfx's
init clear color `0x202020`. Phase 1.5 r2: pin everything to view 0 —
distinct framebuffers for offscreen targets are Phase 2 work.

**Verification**: `ss_client.png` after iteration 1 — uniform `0x808080`
(Nival's `ClearScreen(0.5,0.5,0.5)`) covering the entire 1024x768 client
area. See `p1_5_r2_iter1_clear_color_fills_window.png`.

