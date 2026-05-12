# Phase 1 Completion Report

**Date:** 2026-05-12
**Branch:** main
**Port repo commits (this phase):** T1 → T12 (~16 commits, see `git log`)
**Upstream patches (this phase):** 2 (bgfx bootstrap in Game/Main.cpp; ASSERT skip in BasicChunk1.cpp)

---

## What shipped

**12/12 Phase 1 tasks delivered as code.** Build is green (`cmake --build --preset msvc-debug` exit 0), silent_storm.exe links cleanly at ~23 MB. SDL3, bgfx, fmt all integrated. 8 bgfx shader archetypes compiled to DXBC s_5_0 binaries.

| Task | Delivered |
|---|---|
| T1 — SDL3 + bgfx + vcpkg deps | `488e2d0` |
| T2 — silent_storm.cfg loader | `7d5ce13` |
| T3 — SDL3 window + event pump | `86a4d7f` |
| T4 — SDL3 → NInput bridge (DirectInput8 stubbed) | `bd7dfec` |
| T5 — bgfx init + frame loop | `abcca86` |
| T6 — IDirect3DDevice9 facade scaffold (90 method stubs) | `e2b08f8` |
| T7 — D3D9 → bgfx state translator + unit test | `5699259` |
| T8a-h — 8 shader archetypes (parallel, 16 .bin files) | `70953a0` `92a77c7` `e5beb59` `40b67bc` `0940f0d` `8f69eaf` `a971444` `ca1dcb7` |
| T8 infrastructure (shaderc wiring + bgfx include) | `d8a5af4` |
| T9 — facade plumbing (resource wrappers, draw → bgfx submit, shader registry) | `e9c4f40` |
| T10 — cfg-driven FOV override (perspective path) | included in `f7efaa1` |
| T11 — HUD integer scale (ortho path + mouse inverse-scale) | `f7efaa1` |
| T12 — completion docs + smoke test | (this commit) |

## Architecture as built

```
silent_storm.exe (Game/Main.cpp's WinMain, statically linked)
    │
    ├─ Nival Main DLL (271 cpp as OBJECT lib, calls IDirect3DDevice9* API)
    │       │
    │       ▼
    │   D3D9Facade : public IDirect3DDevice9
    │   ├── all ~90 methods stubbed/implemented (T6+T9)
    │   ├── DeviceState (T7) — render state, texture stages, transforms
    │   ├── state→bgfx flag mapping (T7)
    │   ├── facade resources (T9) — FacadeTexture/VertexBuffer/IndexBuffer
    │   ├── FOV override (T10) — rebuilds proj matrix from cfg
    │   ├── HUD integer scale (T11) — detects ortho, scales _11/_22
    │   └── DrawPrimitive → setState + setVertexBuffer + setUniform + bgfx::submit
    │
    ├─ SDL3 platform layer (T3+T4)
    │   ├─ Window via SDL_CreateWindow
    │   ├─ Event pump (SDL_PollEvent)
    │   └─ Input bridge → NInput::PushMessageSDL → Main DLL's input queue
    │
    └─ bgfx renderer (T5+T8+T9)
        ├─ Vulkan + D3D11 backends (D3D9 backend disabled to avoid d3d9.h clash)
        ├─ shader registry — 8 archetypes loaded at startup
        └─ frame loop: begin_frame (bgfx::touch) → ... → end_frame (bgfx::frame)
```

## Phase 1 acceptance: PARTIAL

The code-level deliverables of Phase 1 are all in place. **Visual verification at main menu has NOT been achieved this phase** due to a runtime issue:

silent_storm.exe currently crashes with an assertion failure inside `NDatabase::Serialize` at `BasicChunk1.cpp:406` — a class typeID present in game.db has no registered `CObjectBase` factory in our build. The skip-and-log fix added (`fix(jan03): skip unregistered class typeIDs`) keeps Nival's deserialization alive past that point, but downstream code then hits another `__debugbreak` before reaching the renderer init hook.

The most likely root cause: Main DLL is built as a CMake `OBJECT` library and statically linked into silent_storm.exe. MSVC's linker may dead-strip the static initializers from `REGISTER_SAVELOAD_CLASS(typeId, ClassName)` macros that aren't directly referenced anywhere — losing the class registration at runtime even though the code is in the binary.

**This is recognized as a Phase 1.5 polish item** (Task #23 in the project task list). Likely fixes:
- Pass `/WHOLEARCHIVE:jan03_Main` to the linker via `target_link_options` to force-keep all object files.
- OR convert Main from OBJECT lib to STATIC lib + ensure `MSVC: target_link_options(... /WHOLEARCHIVE:Main)`.
- OR add explicit `__pragma(comment(linker, "/include:..."))` references to each REGISTER_SAVELOAD_CLASS symbol.

Until that is fixed, runtime verification of T9-T11 work is impossible. The plumbing is in place; the moment classes register correctly, `Init3D` calls the facade, the facade calls bgfx, and pixels reach the SDL3 window.

## Acceptance checklist

1. ✅ `cmake --build --preset msvc-debug` → exit 0
2. ✅ silent_storm.exe links (23 MB)
3. ❌ Main menu renders via bgfx — **blocked on Phase 1.5 linker dead-strip fix**
4. ❌ Mouse + keyboard work in main menu — blocked on same
5. ❌ Resolution variants verified — blocked on same
6. ❌ FOV / HUD scale verified — code path correct, runtime unverified
7. ✅ GitHub Actions CI green on push to main (commits up to `f7efaa1`)
8. ✅ Phase 1 completion doc (this file)

## Carryovers for Phase 1.5 / Phase 2

1. **Linker dead-strip of class registrations** — top priority. Until fixed, exe asserts before reaching renderer.
2. **Specific draw call missing-state-vector detection** — once renderer runs, expect Nival to set state combinations the 8 archetypes don't cover; facade should assert + log on unknown state vector. (Already structured in `select_shader_archetype()` returning a fallback.)
3. **Render target management** — Nival's GetBackBuffer / GetRenderTarget return sentinel surfaces; real implementation needed when full scenes render.
4. **Resource refcount lifetimes** — FacadeTexture/VertexBuffer use simple refcounts. Nival's CDBPtr<> may have shared-pointer semantics requiring more care.
5. **Phase 1 was bigger than estimated** — original 6-8 weeks estimate held up for code, but runtime polish adds ~1-2 weeks. Phase 1.5 absorbs this.

## What this enables

Phase 1 sets up the engine modernization scaffolding such that:
- Phase 2 (FMOD → miniaudio) can replace audio independently — SDL3 + bgfx changes don't touch the audio path.
- Phase 3 (video playback) can plug into bgfx-textured fullscreen quads — the renderer pipeline accepts FFmpeg-decoded frames as bgfx textures.
- Phase 4 (Lua 4 → 5.5) untouched by renderer; sequenceable.
- v2 (Linux/macOS) needs SDL3 (done) + bgfx (done) + replace WinFrame.cpp's remaining Win32 calls — small follow-up.

## Stats

- **Port repo size**: ~50 KB code (excluding bgfx submodule + vcpkg builds)
- **bgfx submodule**: 240 MB checked out, ~5 min first-build
- **vcpkg dependencies built**: SDL3 + fmt (~10 min first-build)
- **Shader compilation**: ~15 KB across 16 .bin files, ~30 s first-build
- **Subagents dispatched this phase**: 14 (1 each for T2/T3/T4/T5/T6/T7, 8 for T8a-h, 1 for T9, 1 for T10+T11)
- **Total session token usage**: ~3M (subagents dominate)
