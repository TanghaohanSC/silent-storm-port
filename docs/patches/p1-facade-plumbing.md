# Phase 1 Task 9 Patch: bgfx Bootstrap in Game/Main.cpp

**File patched:** `upstream/Soft/Andy/Jan03/a5dll/Game/Main.cpp`

**Guard macro:** `SS_USE_BGFX_FACADE` (already defined for `jan03_Main`; T9 also adds it to the `silent_storm` exe target in `port/CMakeLists.txt`).

## What changed

### Top-of-file forward declarations (after existing `#include` block)

```cpp
#ifdef SS_USE_BGFX_FACADE
// Phase 1 Task 9: C-style bootstrap so we avoid the STLport <-> MSVC STL ABI
// mismatch on std::string between Game/Main.cpp and the renderer lib.
extern "C" bool ss_renderer_bootstrap(void* hwnd, int width, int height,
                                       const char* cfg_path);
extern "C" bool ss_renderer_load_shaders(const char* shader_dir);
#endif
```

### Inside `WinMain`, between `NWinFrame::InitApplication` and `NGfx::Init3D`

```cpp
#ifdef SS_USE_BGFX_FACADE
    // Phase 1 Task 9: initialize bgfx on Nival's HWND, then load shaders.
    // This must happen before NGfx::Init3D — NGfx::Init3D ultimately creates
    // the IDirect3DDevice9 facade, which assumes bgfx is already alive.
    {
        RECT rc;
        GetClientRect(NWinFrame::GetWnd(), &rc);
        int w = rc.right  - rc.left;
        int h = rc.bottom - rc.top;
        if (w <= 0 || h <= 0) { w = 1280; h = 720; }
        if (!ss_renderer_bootstrap(NWinFrame::GetWnd(), w, h, "silent_storm.cfg"))
        {
            MessageBox(0, "bgfx init failed", "Error", MB_OK);
            return 0;
        }
        ss_renderer_load_shaders("shaders");
    }
#endif
```

## Why C-style `extern "C"` instead of the in-namespace `init_hwnd`

`Game/Main.cpp` is compiled against **STLport** via `Game/StdAfx.h` (PCH plus `legacy_compat.h` forced include).  The modern renderer library (`silent_storm::renderer`) uses the modern MSVC `std::` headers.  Passing `std::string` or `silent_storm::Config` between the two compilation units would mix ABIs and result in UB or linker errors.

The renderer therefore exposes a pair of `extern "C"` thunks (`ss_renderer_bootstrap` / `ss_renderer_load_shaders`) defined in `port/src/renderer/bgfx_init.cpp`:

```cpp
extern "C" bool ss_renderer_bootstrap(void* hwnd, int width, int height,
                                       const char* cfg_path) {
    silent_storm::Config cfg = silent_storm::LoadConfig(cfg_path ? cfg_path : "silent_storm.cfg");
    return silent_storm::renderer::init_hwnd(hwnd, width, height, cfg);
}
extern "C" bool ss_renderer_load_shaders(const char* shader_dir) {
    return silent_storm::renderer::load_all_archetypes(shader_dir);
}
```

These cross the boundary using POD types only (`void*`, `int`, `const char*`, `bool`).

## CMake side-effect (port/CMakeLists.txt)

Two small additions:

1. `add_dependencies(ss_renderer ss_shaders)` so the 8 compiled shader `.bin` files exist when the renderer-using exe links / runs.
2. `SS_USE_BGFX_FACADE` is added to `silent_storm`'s `target_compile_definitions` (it was previously only on `jan03_Main`).  This activates the bootstrap block in `Game/Main.cpp`.

## Impact

- `#else` branches in both Game/Main.cpp and Gfx.cpp are 100% unchanged — disabling `SS_USE_BGFX_FACADE` restores the original D3D9 init flow.
- No other upstream files touched.

## Open issue surfaced during testing (NOT a regression caused by T9)

Running `silent_storm.exe` from `upstream/Complete/` triggers an `ASSERT(pObject)` failure in `FileIO/BasicChunk1.cpp(406)` during `NDatabase::Serialize` (game.db read).  This is preexisting Phase 0 / Phase 1.5 work: a missing `CObjectBase` factory registration for one of the database type IDs.  Because the assertion fires before `NWinFrame::InitApplication`, the bgfx bootstrap never runs and we can't yet observe a rendered frame.  Resolving the DB-factory registration is tracked as a separate Phase 1.5 task; T9 ships the rendering pipeline so it can be exercised the moment the DB issue is unblocked.
