# Phase 1 Patch: IDirect3DDevice9 Facade Hook

**File patched:** `upstream/Soft/Andy/Jan03/a5dll/Main/Gfx.cpp`

**Guard macro:** `SS_USE_BGFX_FACADE`  
**Definition site:** `port/CMakeLists.txt` via `target_compile_definitions(jan03_Main PRIVATE SS_USE_BGFX_FACADE)`

## What changed

Added to top of `Gfx.cpp` (after existing includes):

```cpp
#ifdef SS_USE_BGFX_FACADE
// Forward declaration of the facade singleton
namespace silent_storm { namespace renderer {
    IDirect3DDevice9* facade_instance();
}} // namespace silent_storm::renderer
#endif
```

In `ResetDevice()`, replaced the two `pD3D->CreateDevice(...)` calls with:

```cpp
#ifdef SS_USE_BGFX_FACADE
    {
        IDirect3DDevice9** ppDev = pDevice.GetAddr();
        *ppDev = silent_storm::renderer::facade_instance();
        (*ppDev)->AddRef();   // com_ptr will Release() on destruction
        hr = D3D_OK;
        bHardwareVP = true;
    }
#else
    // ... original D3D9 CreateDevice calls ...
#endif
```

## Why

Nival's `pDevice` is a `com_ptr<IDirect3DDevice9>`. When `SS_USE_BGFX_FACADE` is defined, 
we short-circuit device creation and inject the `D3D9Facade` singleton instead. 
The facade returns `D3D_OK` on all state calls, no-ops on draw calls (Phase 1 scope), 
and returns `E_NOTIMPL` on resource creation (Task 9 wires real bgfx resources).

The `AddRef()` call balances the `Release()` that `com_ptr`'s destructor will issue.

## Impact

- `#else` branch is 100% unchanged — disabling `SS_USE_BGFX_FACADE` restores original D3D9 behavior.
- No other upstream files touched.
