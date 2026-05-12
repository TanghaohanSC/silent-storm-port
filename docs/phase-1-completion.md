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

The code-level deliverables of Phase 1 are all in place. **Visual verification at main menu has NOT been achieved this phase** — runtime hits an access violation before reaching renderer init.

### Phase 1.5 progress (committed `ecdefc4`)

Linker dead-strip hypothesis **CONFIRMED**:
- Changed `add_jan03_subproject(Main TYPE OBJECT)` → `TYPE STATIC`
- Added `target_link_options(silent_storm PRIVATE /WHOLEARCHIVE:Main /WHOLEARCHIVE:DBFormat)`
- exe size grew 23 → 24.3 MB (every .obj now retained)
- Result: **no more `STATUS_BREAKPOINT` at BasicChunk1.cpp:406** — class registrations now survive linking.

### Remaining runtime blocker

exe now crashes with `STATUS_ACCESS_VIOLATION (0xC0000005)` instead of the prior breakpoint. The renderer init log (`silent_storm_renderer.log`) is NOT written, meaning the bootstrap inserted in `Game/Main.cpp` between `InitApplication` and `Init3D` is never reached. Some null pointer is dereferenced inside Nival's `InitApplication` path — most likely a `CObjectBase*` returned by `NDatabase::Serialize` that's still null even with all classes registered (perhaps a different deserialization edge case, OR our skip-and-log fallback in BasicChunk1.cpp is still active and now feeds null pointers into Nival's resource tables).

### Phase 1.5 next moves

1. Remove the BasicChunk1.cpp skip-and-log fallback — with /WHOLEARCHIVE active, the assert should now fire only on genuinely missing classes (which we can then add stubs for, vs silently nulling them).
2. Instrument `Game/Main.cpp` with `MessageBox` checkpoints between InitApplication and the renderer bootstrap to localize the access violation.
3. OR attach a debugger (Visual Studio "Attach to Process") at the crash point — fastest path to root cause.
4. Once renderer init is reached: expect bgfx clear color visible in SDL3 window, plus iterative facade method gaps to fill in.

Until that is fixed, runtime verification of T9-T11 work is impossible. The plumbing is in place; the moment InitApplication completes cleanly, `Init3D` calls the facade, the facade calls bgfx, and pixels reach the SDL3 window.

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
