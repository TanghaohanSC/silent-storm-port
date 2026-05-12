# Phase 1: Renderer Modernization — Detailed Design

**Date**: 2026-05-12
**Parent spec**: `2026-05-11-silent-storm-modernization-design.md` §3.3, §5
**Status**: brainstormed 2026-05-12, awaiting plan

---

## 1. Scope

Replace Jan03's Direct3D 9 + Win32 + DirectInput8 stack with SDL3 + bgfx, and
add modern resolution / FOV / HUD scaling.

Acceptance: `silent_storm.exe` started from `upstream/Complete/` displays
the main menu at **1920×1080, 2560×1440, 3840×2160** with no stretching
or HUD overflow. Main menu navigation (mouse + keyboard) works.

Combat scenes are out of scope — Phase 2 (audio) and later phases tackle
the rest of the game state machine.

## 2. Locked design decisions (2026-05-12 brainstorm)

| Topic | Decision | Why |
|---|---|---|
| Phase split | **Not split**, single Phase 1 spec covering 1A-1E | Cohesive renderer stack; partial substitutes (SDL3-only or bgfx-only) don't ship anything useful |
| Shader strategy | **Hand-write 5-8 generic bgfx shaders** | Nival's 30+ shader procs in `GfxShaders.txt` map to 5-8 archetypes. Translating the DSL (DX 1.1 vs-asm with macros) costs 4-6 weeks; hand-writing approximations costs 1-2 weeks and matches what Phase 1+ visual fidelity needs |
| D3D9 migration | **Hard cutover via `IDirect3DDevice9` facade** | Single backend after Phase 1. Game code keeps calling `IDirect3DDevice9*` methods; the facade routes to bgfx. Coexist-flag path doubles maintenance |
| Input migration | **Bundled into Phase 1** (SDL3 takes both window + input) | DirectInput8 stub in Phase 0 was acknowledged temporary; Phase 1 already replaces window, may as well |
| bgfx vendor method | **git submodule** at `port/third_party/bgfx/` | bgfx changes frequently, submodule pins exact commit; vcpkg lag isn't acceptable for this central dep |
| SDL3 vendor method | **vcpkg manifest** | mature, stable, no need for source-level control |
| Resolution selection | **`silent_storm.cfg` next to exe**, plus command-line override | No in-game menu in Phase 1; v1.5 may add launcher UI |
| HUD scale logic | **Auto by render resolution** with config override | 1080p→1×, 1440p→1×, 4K→2×. User can force scale via config |
| Game data cwd | exe must be run from `upstream/Complete/` (or copy Complete/ contents to a `data/` dir next to exe) | Phase 0 confirmed game.db loads from cwd |

## 3. Architecture

```
                ┌─────────────────────────┐
                │  Game.exe (Game/Main.cpp│  ← winmain + main loop
                │  + WinFrame.cpp via     │
                │     SDL3)               │
                └────────────┬────────────┘
                             │
                ┌────────────▼────────────┐
                │  Main DLL (OBJECT lib)  │  ← still uses IDirect3DDevice9*
                │  - 271 game-logic cpp   │  ← calls Set{Render,Texture,Transform}State
                │  - unchanged from P0    │
                └────────────┬────────────┘
                             │ IDirect3DDevice9 interface
              ┌──────────────▼───────────────┐
              │  D3D9-to-bgfx facade         │  ← NEW: port/src/renderer/
              │  - implements IDirect3DDevice9│
              │  - maps fixed-function state │
              │    to bgfx uniforms + program│
              │  - tracks active stream/index│
              │    buffers, picks shader for │
              │    the current state vector  │
              └─┬─────────────────────────┬──┘
                │                         │
        ┌───────▼──────┐         ┌────────▼──────────┐
        │  Renderer    │         │  ShaderArchive    │
        │  (bgfx C++)  │         │  - 5-8 .sc shaders│
        │  - init      │         │    compiled via   │
        │  - frame loop│         │    bgfx shaderc   │
        │  - resource  │         │  - selected by    │
        │    management│         │    state-vector   │
        └──────────────┘         └───────────────────┘

        ┌──────────────┐         ┌───────────────────┐
        │  Platform    │         │  Input            │
        │  (SDL3)      │         │  (SDL3 events)    │
        │  - window    │         │  - replaces       │
        │  - swapchain │         │    DirectInput8   │
        │    handle    │         │  - feeds Nival's  │
        │  - event pump│         │    NInput::Push... │
        └──────────────┘         └───────────────────┘
```

## 4. Shader archetypes (the 5-8)

Map Nival's 30+ procs to:

| Archetype | bgfx file | Nival procs covered |
|---|---|---|
| `ss_diffuse_unlit` | `vs_diffuse_unlit.sc` + `fs_diffuse_unlit.sc` | `Texture`, `TextureLM`, `PureGeometry`, `TextureTexU` |
| `ss_diffuse_lit` | `vs_diffuse_lit.sc` + `fs_diffuse_lit.sc` | `Glow`, `TextureLit`, lighting variants |
| `ss_skinned` | `vs_skinned.sc` + `fs_diffuse_lit.sc` | character skinning variants |
| `ss_ui` | `vs_ui.sc` + `fs_ui.sc` | HUD / menu blitting |
| `ss_particle` | `vs_particle.sc` + `fs_particle.sc` | particles, decals, `TransparentMap` |
| `ss_shadow` | `vs_shadow.sc` + `fs_shadow.sc` | shadow caster pass |
| `ss_terrain` | `vs_terrain.sc` + `fs_terrain.sc` | terrain (rendered to lightmap) |
| `ss_water` | `vs_water.sc` + `fs_water.sc` | water surface |

The facade picks the archetype from a **state vector**: `{has_texture, num_textures, lit, skinned, blend_mode, depth_test, alpha_test, fog}`. State vector → shader program lookup table.

## 5. State-machine translation (the hard part)

Fixed-function calls to translate:

| D3D9 call | bgfx equivalent |
|---|---|
| `SetRenderState(D3DRS_*)` | `bgfx::setState()` flag bits |
| `SetTextureStageState(N, D3DTSS_*)` | uniform for stage N or shader variant pick |
| `SetTexture(N, tex)` | `bgfx::setTexture(N, sampler, handle)` |
| `SetTransform(D3DTS_WORLD)` | uniform `u_world` |
| `SetTransform(D3DTS_VIEW)` / `PROJ` | `bgfx::setViewTransform()` |
| `SetStreamSource(0, vbo, offs, stride)` | `bgfx::setVertexBuffer()` |
| `SetIndices(ibo)` | `bgfx::setIndexBuffer()` |
| `SetVertexShader(shader)` | facade resolves to bgfx program by state |
| `SetPixelShader(shader)` | same |
| `DrawPrimitive` / `DrawIndexedPrimitive` | `bgfx::submit(viewId, program)` |
| `BeginScene` / `EndScene` | view setup at frame start, `bgfx::frame()` at end |
| `Present` | `bgfx::frame()` |
| `Clear` | `bgfx::setViewClear()` |

The facade maintains a dirty-state struct; before each `DrawPrimitive`, the
struct is flushed to bgfx (one `setState` + uniform updates).

## 6. Module layout (port/)

```
port/
├── src/
│   ├── platform/            # NEW
│   │   ├── sdl_window.{h,cpp}
│   │   ├── sdl_event_pump.{h,cpp}
│   │   └── sdl_input_bridge.{h,cpp}   # SDL events → NInput::Push*
│   ├── renderer/            # NEW
│   │   ├── bgfx_init.{h,cpp}
│   │   ├── d3d9_facade.{h,cpp}        # IDirect3DDevice9 impl
│   │   ├── d3d9_facade_resources.{h,cpp}
│   │   ├── state_translator.{h,cpp}   # state vector → bgfx state bits
│   │   └── shader_registry.{h,cpp}    # archetype lookup
│   ├── shaders/             # NEW (bgfx .sc sources)
│   │   ├── varying.def.sc
│   │   ├── vs_diffuse_unlit.sc
│   │   ├── fs_diffuse_unlit.sc
│   │   └── ... (16 .sc files for 8 archetypes)
│   └── stubs/               # Phase 0 stubs stay until removed
└── third_party/
    └── bgfx/                # NEW git submodule (bgfx + bimg + bx)
```

## 7. SDL3 platform migration

WinFrame.cpp currently has:
- `WinMain` setup
- `CreateWindowEx` + WNDCLASS registration
- `WndProc` message handler
- Direct call to `CreateDevice` for D3D9

Replace with:
- SDL3 `SDL_CreateWindow` (resizable, optionally fullscreen-desktop)
- SDL3 event loop (`SDL_PollEvent`)
- Native window handle → bgfx `bgfx::PlatformData::nwh`
- Window resize → bgfx `bgfx::reset(width, height)`

DirectInput8 → SDL3 input mapping:
- `IDirectInputDevice8::GetDeviceState(keyboard)` → poll `SDL_GetKeyboardState`
- `IDirectInputDevice8::GetDeviceData(mouse, ...)` → consume SDL `MOUSE*` events
- Joystick: SDL3 game controller API (if Nival used any — inventory says no joystick API references, but verify)

Bridge layer feeds Nival's `NInput::SMessage` queue exactly as the DI8 path did. Game logic in Main DLL doesn't change.

## 8. Modern resolution + FOV

Read at startup from `silent_storm.cfg` next to exe:

```ini
[display]
width = 0           # 0 = desktop, otherwise integer
height = 0
mode = fullscreen-desktop  # | fullscreen | windowed
vsync = on
fov_horizontal = auto       # | <degrees>; auto = 90° wide angle scaled
hud_scale = auto            # | 1 | 2 | 3
[renderer]
backend = vulkan            # | d3d11 | d3d12 | metal | opengl | auto
```

Phase 1 reads + applies. No launcher UI; user edits the file.

FOV fix: Nival's projection setup uses a fixed 4:3 horizontal FOV. We
intercept `SetTransform(D3DTS_PROJECTION)` in the facade and rebuild the
projection matrix with the cfg-specified FOV at the current aspect
ratio. The view matrix is untouched.

## 9. HUD scaling

HUD = elements drawn via Nival's UI path using ortho projection 1:1 to
screen pixels. Original assumes 1024×768. At higher resolutions, HUD
needs integer-multiplied scale to be visible.

Approach:
- Facade detects the ortho projection (`m._44 != 0` etc. or whenever Nival
  switches to ortho-projection mode)
- Apply scale factor: `scale = floor(min(width / 1024, height / 768))`
- Render HUD layer to scaled viewport; bgfx upscales to full resolution

Original UI button hit-test in Main DLL takes raw pixel coords. Need to
inverse-scale mouse coordinates before passing to UI (otherwise clicks
miss). Phase 1 patches `NWinFrame::OnMouseMove` to apply the inverse.

## 10. Risks

1. **Fixed-function state-vector explosion**: Nival may use uncommon state combinations not anticipated by the 8-archetype mapping. Mitigation: facade asserts on unknown state vector, logs and falls back to nearest archetype. Iterate on missing states.
2. **Performance**: extra uniform updates per draw call. 2003 game; even 100k uniform updates per frame is fine on modern GPU. Should be a non-issue.
3. **bgfx + Direct3D9 SDK header coexistence**: facade implements `IDirect3DDevice9`, so we include `<d3d9.h>` for the interface decl. bgfx internals also use D3D9 when running its d3d9 backend (we won't). Need to make sure bgfx's d3d9 backend isn't compiled in our build (compile-flag exclusion).
4. **Shader visual fidelity gap**: 5-8 archetypes vs Nival's 30 procs means some scenes will look subtly different. Phase 1.5 (Phase 2 of polish, not in scope here) can add more archetypes.
5. **SDL3 maturity**: SDL3 GA was Jan 2025. Mid-2026 should be solid, but corner cases may exist. Mitigation: minimal SDL3 usage surface; can fall back to SDL2 via small wrapper if blocking issue found.

## 11. Done = v1.5 admission ticket

When Phase 1 is done:
- `cmake --build` clean
- exe (renamed to `silent_storm.exe` or similar) starts via SDL3 window
- Main menu renders via bgfx (with our archetype shaders)
- Resolution honored from `silent_storm.cfg`
- HUD scales correctly at 1080p, 1440p, 4K
- Smoke test extended: exe stays alive past main menu render (3-5 seconds), captures a screenshot for visual diff
- Phase 1 completion doc + screenshots committed

After Phase 1: Phase 2 audio (FMOD → miniaudio) becomes the next priority. Combat / mission scenes will reveal more renderer state vectors needing handling — feed those back as iterative shader work.

## 12. Estimated effort

| Sub-piece | Effort |
|---|---|
| SDL3 window + event pump | 1 week |
| SDL3 input bridge (→ NInput::Push*) | 4-5 days |
| bgfx init + minimal frame | 3 days |
| IDirect3DDevice9 facade scaffolding (no draws yet) | 1 week |
| State translation (every D3DRS_* / D3DTSS_*) | 1-2 weeks |
| 8 shader archetypes (write + test) | 1-2 weeks |
| Resolution / FOV / HUD scale | 4-5 days |
| Polish + bug fixes | 1 week |
| **Total** | **6-8 weeks** (兼职 10-15 小时/周) |

---

## Next step

Run writing-plans skill against this design to produce a step-by-step
implementation plan. The plan will partition the 6-8 weeks of work into
~10-15 tasks dispatchable to subagents (some serial, some parallel).
