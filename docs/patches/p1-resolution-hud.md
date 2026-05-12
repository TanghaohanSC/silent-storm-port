# Patch: Phase 1 T10+T11 — cfg-driven FOV + HUD integer scale

## What changed

### state_translator.h
- Added `int hud_scale_active = 1` field to `DeviceState`.  The draw path reads
  this to know whether a scaled ortho projection is active (reserved for future
  shader-side work).

### bgfx_init.h / bgfx_init.cpp
- Added `get_width()`, `get_height()` — expose the internal `g_width`/`g_height`
  globals already set by `init_hwnd` + `on_resize`.
- Added `get_hud_scale()` / `set_hud_scale(int)` — shared state between the facade
  (writer) and the input bridge (reader), stored as `g_hud_scale`.
- `ss_renderer_bootstrap` now calls `facade_init_with_config(cfg)` before
  `init_hwnd`, seeding the singleton with the loaded config.
- Added `#include "d3d9_facade.h"` so the bootstrap can call
  `facade_init_with_config`.

### d3d9_facade.h
- Added `#include "../config/config.h"`.
- Added `Config cfg_` private member.
- Added `explicit D3D9Facade(const Config& cfg)` constructor.
- Declared `facade_init_with_config(const Config&)` free function.

### d3d9_facade.cpp
- Added `#include <cmath>` + `#include <algorithm>` for `std::floor`, `std::min`,
  `std::max`, `std::tan`, `std::atan`.
- Added `D3D9Facade::D3D9Facade(const Config& cfg)` definition.
- Added `facade_init_with_config(const Config& cfg)` singleton factory that creates
  the facade with the config when called before Nival's `CreateDevice` hook.
- Rewrote `SetTransform(D3DTS_PROJECTION)` (T10+T11):
  - **Ortho path (T11)**: `is_ortho()` detects HUD/menu matrices. Computes
    `scale = compute_hud_scale(w, h, cfg_.display.hud_scale)` — uses config value
    if > 0, else `floor(min(w/1024, h/768))` clamped [1, 4]. Scales `_11`/`_22`
    and writes `hud_scale_active`. Calls `set_hud_scale()` so the input bridge sees it.
  - **Perspective path (T10)**: `is_perspective()` matches D3D LH matrices
    (`_44==0, _34==−1`). If `cfg_.display.fov_horizontal > 0`, extracts near/far
    from the incoming matrix (`zn = −m._43/m._33`, `zf = m._43/(m._33−1)`),
    converts horizontal FOV to vertical via `2*atan(tan(fov_h/2)/aspect)`, and
    rebuilds with `make_perspective_lh`.  Falls back to `zn=0.1, zf=10000` if the
    matrix values are degenerate.
  - **Passthrough**: unrecognised matrices stored as-is (preserves ortho with no
    scale when `hud_scale==0` and auto-scale would be 1x at 1024×768 resolution).

### sdl_input_bridge.cpp
- Added forward declaration `namespace silent_storm::renderer { int get_hud_scale(); }`
  (no header include to avoid circular library dependency).
- `SDL_EVENT_MOUSE_MOTION` handler now divides `xrel`/`yrel` by `get_hud_scale()`
  before pushing to NInput.  When scale == 1 (most cases) there is zero overhead —
  the branch is skipped entirely.

## Design decisions

- **Config access via member**: option (a) from the plan — `cfg_` stored on the
  facade.  Simplest and avoids a global `current_config()` accessor.
- **set_hud_scale in bgfx_init**: avoids a third header just for a single int.
  bgfx_init already owns the renderer globals.
- **No ss_platform → ss_renderer link edge**: the input bridge forward-declares
  `get_hud_scale` instead of including `bgfx_init.h`.  The exe links both libs;
  MSVC resolves the symbol from `ss_renderer.lib`.
- **Near/far extraction**: D3D LH perspective:
  `_33 = zf/(zf−zn)`, `_43 = −zn*zf/(zf−zn)` → `zn = −_43/_33`, `zf = _43/(_33−1)`.
  Degenerate guard: if `|_33| < 1e-6` or derived values are non-positive/inverted,
  falls back to `zn=0.1 zf=10000`.
