# Phase 1.5 — Runtime patches

Iterative runtime-blocker patches applied during Phase 1.5 debugging. These
extend the Nival upstream code (under `upstream/Soft/Andy/Jan03/a5dll/`) and
the port-side `port/src/renderer/` facade until `silent_storm.exe` reaches
the main menu render.

## Trace plumbing

Added fine-grained `fopen_s` traces to a new `silent_storm_smfc.log` (set
mode from config) covering:

- `Main/GInit.cpp`           — `SetModeFromConfig` entry/exit
- `Main/Gfx.cpp`             — `SetMode`, `ResetDevice`, `InitDXObjects`
- `Main/GfxBuffers.cpp`      — `InitBuffers` checkpoints + `CSysTexture` ctor

These traces are non-functional (just `fopen_s/fprintf/fclose`); safe to keep.

## Facade extensions

### `port/src/renderer/d3d9_facade_resources.{h,cpp}`

* Added **`FacadeCubeTexture`** — minimal `IDirect3DCubeTexture9` wrapper.
  Previously `D3D9Facade::CreateCubeTexture` returned `E_NOTIMPL`, causing
  `ASSERT(D3D_OK == hRes)` in `CCubeTB`'s constructor to trip when Nival's
  `InitBuffers` allocated the `gfx_cl` cube-lighting cache. Phase 1.5: we
  return CPU-backed scratch surfaces from `GetCubeMapSurface` / `LockRect`
  so callers see success; no bgfx upload yet (Phase 2 task).

### `port/src/renderer/d3d9_facade.cpp`

* `D3D9Facade::CreateCubeTexture` — now constructs a `FacadeCubeTexture`
  and returns `D3D_OK`.
