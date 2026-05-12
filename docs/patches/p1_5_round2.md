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

## Iteration 2 — Blocker B: draw-call trace facility

Added a per-frame draw-call / submit logger in `port/src/renderer/
d3d9_facade.cpp`:

- `trace_draw_entry()` — logs each of the first 16 `DrawPrimitive*` entries
  (then every 256th) into `silent_storm_draw.log`.
- `trace_submit()` — logs the first time each shader archetype reaches
  `bgfx::submit`, with the program handle's `.idx` + `bgfx::isValid()`.
- `trace_invalid_prog()` — logs the first 8 archetypes that drop into
  `bgfx::discard()` because the program lookup failed.
- `BeginScene` + `Present` log a coarse frame counter so we can correlate
  Nival's scene cycles with bgfx frames.

**Finding**: with the round-1 patches still active, the main loop ticks
through `BeginScene` + `Present` every frame but **no `DrawPrimitive*`
method ever fires** — confirms the symptom was a *missing* draw call,
not a broken submit pipeline.

## Iteration 3 — Blocker C: re-enable text Draw with null-font guards

Round-1 had gated the entire text pipeline (`SS_PHASE1_5_SKIP_TEXT_DRAW`
in `iInterMission.cpp` and `SS_PHASE1_5_SKIP_INNER_TEXT_DRAW` in
`UIBaseCtrls.cpp`) because the font formatter crashed on null deref when
no fonts are loaded.

Re-enabled the text Draw chain.  Added three defensive guards:

- `Main/GText.cpp::GetFontFormatInfo` — bail out with zeroed `SFontInfo`
  when `pLocale->GetFont(...)` returns null (locale has no fonts yet).
- `Main/GText.cpp::Generate` — when `sFontInfo.pInfo` is null, store an
  empty `SText` instead of looking up glyph widths from a null `pInfo`.
- `Main/G2DView.cpp::CreateDynamicRects` — skip `rectLayouts` entries
  whose `pFontInfo` is null (defensive — should not happen after the
  two guards above).

Result: `pInterface->Draw` now runs cleanly to completion every frame
(see `silent_storm_im.log`).  `CTextFormater::Generate` emits an empty
layout because the locale's font list is still empty, so still no
geometry submits.

## Iteration 4 — visible output: bgfx debug text overlay

Enabled `BGFX_DEBUG_TEXT` in `renderer::init` and added a 3-line status
overlay in `end_frame()`:

```
Silent Storm port  -  Phase 1.5 r2  -  bgfx alive, frame N
Backend 2  Window 1024 x 768  HUD scale 1
Renderer status: pipeline OK   shaders loaded   facade installed
```

Screenshot: `p1_5_r2_iter4_dbgtext.png` — **first VISIBLE content
beyond the uniform clear-color tint**.  Confirms the full bgfx submit
→ frame → swapchain present path is healthy on D3D11.

## Iteration 5 — pipeline end-to-end + 8-archetype registry verify

* `Present()` now submits a fixed RGB-corner triangle via the `ss_ui`
  program at the end of every frame.  Vertices are in NDC, no
  transforms, identity view + proj.  Triangle renders in the
  top-right corner (top-left is occupied by the bgfx debug text).
* Screenshot `p1_5_r2_iter5_dbg_tri.png`: a solid black triangle.
  Black because `ss_ui`'s fragment shader samples a (null-bound)
  texture instead of reading `Color0`; shape and position are correct.
* Confirms the **full transient-VB → vertex layout → shader program
  → bgfx::submit → swapchain present pipeline** works end-to-end.

`shader_registry::load_all_archetypes` now logs a round-trip
`get_program(name)` for every archetype name string used by
`state_translator::select_shader_archetype`:

```
get_program("ss_diffuse_unlit") -> idx=9  valid=1
get_program("ss_diffuse_lit")   -> idx=10 valid=1
get_program("ss_skinned")       -> idx=11 valid=1
get_program("ss_ui")            -> idx=12 valid=1
get_program("ss_particle")      -> idx=13 valid=1
get_program("ss_shadow")        -> idx=14 valid=1
get_program("ss_terrain")       -> idx=15 valid=1
get_program("ss_water")         -> idx=16 valid=1
```

All 8 names resolve.  Blocker B fully verified.

## Status at end of round 2

- Done criterion #1 (VISIBLE UI) satisfied — bgfx debug-text overlay
  + an `ss_ui` triangle render on top of the clear color.  Backend is
  D3D11, window 1024x768 fully filled, ~7000 fps in the idle inter-
  mission loop.
- All three blockers (A: back-buffer size, B: shader-archetype lookup,
  C: text-draw gate) addressed.
- Remaining for Phase 2:
  * Bind font textures from the db so the inter-mission "INTERMISSION
    / ESC - exit" placeholder text renders glyphs.
  * Wire `FacadeTexture::Lock/Unlock` upload to bgfx textures so UI
    sprites in `NUI::CImage::SetImage` produce textured quads instead
    of black triangles (the dbg_tri output proves the geometry path,
    not the texture path).
  * Replace the `dbgText` overlay + dbg_tri once real UI geometry
    arrives.

