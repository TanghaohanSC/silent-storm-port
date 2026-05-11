# Phase 0 Completion Report

**Date completed:** 2026-05-12
**Total port commits:** 16
**Total upstream commits added:** 2 (43d8c658d, 02e3aa22f on top of nival's 2026 release)

## Achieved

- Modern CMake (4.2.3) + Ninja + MSVC 19.50.35730 build via `cmake --preset msvc-debug && cmake --build --preset msvc-debug`
- All 271 Main DLL .cpp files compile under modern MSVC x86
- All 11 engine subprojects compile clean:
  - `zlib` (15 .c) / `libpng` (18 .c) / `Misc` (9 cpp) / `MemoryMngr` (6 cpp)
  - `FileIO` (4 cpp) / `DBFormat` (15 cpp) / `Image` (8 cpp)
  - `Input` (3 cpp) / `FModSound` (2 cpp) / `OpenDynamix` (23 cpp) / `Script` (20 cpp)
  - `MiscDll` (3 cpp)
- `silent_storm.exe` (20 MB) built from Game/Main.cpp's real `WinMain`
- All commercial / proprietary libraries replaced by stubs:
  - **FMOD 3.x** → `silent_storm_fmod_stub` (67 symbols, silent-fake)
  - **LifeStudioHeadAPI** → header-only stub (no-op face animation)
  - **S3TC / dxtlib** → header-only stub
  - **ADO COM (msado15.dll)** → `silent_storm_ado_stub`
- GitHub Actions CI workflow ready (`.github/workflows/windows.yml`)
- Boot smoke test script (`tests/smoke/test_boot.ps1`)
- Comprehensive patch documentation in `docs/upstream-patches.md` and
  per-subproject logs under `docs/patches/`

## Phase 0 acceptance (verified)

```
cmake --build --preset msvc-debug                          # exit 0
.\build\msvc-debug\bin\silent_storm.exe                    # MessageBox: "File game.db not found"
```

The "File game.db not found" MessageBox is thrown from `Game/Main.cpp:39`
when `f.OpenRead("game.db")` fails — i.e., the exe **launches, executes
WinMain, hits the first resource-load attempt, and exits cleanly**. This
is exactly what Phase 0 was supposed to prove: the build system + stubs +
modern toolchain produce a runnable binary. Game data is not in scope.

## Discoveries that contradicted the original spec

| Spec said | Reality (from inventory + build) |
|---|---|
| Bink Video is in use → Phase 3 replace | **Zero Bink API calls in Jan03 source**. Phase 3 reframed: ADD video playback from scratch (since `.bik` assets exist in game data but Jan03 lacks integration). |
| SQL Server 2000 + MySQL at runtime → Phase 3.5 SQLite migrate | **No runtime SQL.** `game.db` is custom binary via DBFormat. ADO only in dropped tools. Phase 3.5 DELETED. |
| Standard Lua 4 → migrate to Lua 5.4 (now 5.5) batch | **Custom-patched Lua 4** with `lua_startThread` cooperative threading. No `.lua` scripts (compiled bytecode in game.db). Migration deferred to v2. |
| D3D8 fixed-function | **D3D9** (Game.vcproj links d3d9.lib). Easier renderer Phase 1 target. |
| (not in spec) | **LifeStudioHeadAPI** proprietary face animation SDK linked into Game.exe. Headers absent. Permanent stub in v1. |

## Architectural decisions made during Phase 0

1. **x86 target** (not x64) — Jan03 has inline x86 asm in Misc/tools.h that modern MSVC x64 doesn't support. 32-bit toolchain matches original.
2. **stlport shim** instead of real stlport — vc7 vendored stlport at `upstream/Soft/SDK/stlport/` uses VS .NET 2003 relative include paths that hijack-then-break under modern MSVC. `port/third_party/stlport_shim/` provides empty `<stl/_config.h>` and `<hash_map>` → `std::unordered_map` alias; modern std:: backs everything.
3. **`legacy_compat.h` force-included** into every Jan03 C++ TU via `/FI` — provides TRACE/VERIFY/std::max/std::min in global scope.
4. **Main built as OBJECT lib** (not DLL) — original was a DLL with `.def` exports. OBJECT lib statically links into the exe; avoids the `.def` plumbing for Phase 0. Phase 1+ may convert to DLL if dynamic plugin loading becomes a goal.
5. **C++14 standard** pinned per Jan03 target via `CXX_STANDARD 14` + `/Zc:noexceptTypes-` to keep noexcept out of function type.

## Discovered (carries into Phase 1)

- DX9 fixed-function call density — to be quantified in Phase 1 renderer work
- CDynamicCast wrapper used 300+ times — Phase 1 should drop in favor of native `dynamic_cast<T*>`
- CEventRegister uses RTTI keys requiring typeid at ctor — Phase 1 should refactor event system
- PCH bloat: Main/StdAfx.h pulls all NDb data headers because MSVC eagerly instantiates `CDBPtr<T>::operator&`. Slow incremental builds when NDb changes
- `Misc/Time.h` shadows system `<time.h>` (case-insensitive on Windows) — Phase 1 should rename
- Operator new/delete now uses stdlib malloc/free (was Nival's FastDumbAlloc from skipped MemoryMngrDll). Performance may differ from original
- `Main/Interface.cpp::CMouseCaptureHandler` references nonexistent `IWindow`/`IInterface` types — wrapped in `#if 0`. Phase 1 should resolve or delete entirely

## Open issues

- Calling any `NDatabase::Import()` or `Refresh()` will assert (ADO stub deliberately limited to runtime Serialize path)
- No multiplayer-related .cpp files dropped — per inventory, no DirectPlay symbols exist in Jan03 source anyway

## Decisions made along the way that affect later phases

- stlport: shimmed (not real-vendored, not modern-std migration). Phase 1+ may complete the modern std:: migration as cleanup.
- DirectX SDK: modern Windows SDK sufficed for d3d9 / dinput8 / dxguid. No D3DX needed for Phase 0.
- Lua: vendored Lua 4 (Script subproject) kept unchanged. Phase 4 (now pushed to v2) migrates to Lua 5.5.
- No unit test infrastructure rolled in for Phase 0. v1 will add GoogleTest when meaningful tests can be written (Phase 2 onwards).
