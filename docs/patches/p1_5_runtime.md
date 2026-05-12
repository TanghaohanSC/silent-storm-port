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

* Added **`FacadeVertexShader` / `FacadePixelShader` / `FacadeVertexDeclaration`**
  — owning wrappers over the raw shader bytecode / vertex element list. Used
  by `InitStateBlocks` (called from `InitEffects` → `InitRender`) which would
  otherwise see `E_NOTIMPL` for the create calls and trip its `ASSERT(D3D_OK
  == hr)` chain. Phase 2 will bind these to bgfx::Program handles; for now
  they're inert.

* Added **`FacadeStateBlock`** — empty Capture/Apply object so
  `CreateStateBlock` returns success.

* Added **`FacadeQuery`** — wraps `D3DQUERYTYPE_VCACHE` and similar query
  objects. `Issue`/`GetData` are no-ops returning `S_OK`. Previously
  `CreateQuery` returned `E_NOTIMPL`, which Nival's `ResetDevice`
  short-circuited via its `if (SUCCEEDED(hr))` guard — but other call-sites
  weren't as forgiving.

### `port/src/renderer/d3d9_facade.cpp`

* `D3D9Facade::CreateCubeTexture` — now constructs a `FacadeCubeTexture`
  and returns `D3D_OK`.
* `D3D9Facade::CreateVertexShader/CreatePixelShader/CreateVertexDeclaration`
  — replaced `E_NOTIMPL` stubs with real wrapper constructors.
* `D3D9Facade::CreateStateBlock` — real `FacadeStateBlock`.
* `D3D9Facade::CreateQuery` — real `FacadeQuery`; null `ppQuery` returns
  `D3D_OK` (probe-only call sites).

## Upstream patches

### `upstream/Soft/Andy/Jan03/a5dll/*/StdAfx.h`

Patched every linked-subproject `StdAfx.h` (`Main`, `Game`, `Misc`,
`MiscDll`, `FileIO`, `FModSound`, `Image`, `Input`, `Script`, `DBFormat`,
`MemoryMngr`, `MemoryMngrDll`) so the `ASSERT` macro logs to
`silent_storm_assert.log` and returns instead of popping a modal
`MessageBox` + `__debugbreak()`. This is gated on
`SS_ASSERT_LOG_AND_CONTINUE` which we hard-define inside the `_DEBUG` arm.
Phase 1.5 only — restore the popup before shipping.
