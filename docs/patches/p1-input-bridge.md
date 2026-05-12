# Phase 1 Task 4 — SDL3 → NInput Input Bridge Patches

## Date
2026-05-11

## Scope
- `upstream/Soft/Andy/Jan03/a5dll/Input/Input.cpp` — guarded DirectInput8 code
- `upstream/Soft/Andy/Jan03/a5dll/Input/Input.h` — added `PushMessageSDL` declaration

---

## Patch 1: `Input.cpp` — `SS_USE_SDL_INPUT` compile guard

### Motivation
Nival's original `Input.cpp` initializes DirectInput8 devices, polls them each
frame, and maps raw DI8 DIDEVICEOBJECTDATA entries to `NInput::SMessage` structs.
When the SDL3 input bridge is active, this entire subsystem must be disabled:
- The DI8 device handles it manages are invalid (SDL3 owns the window).
- Running both DI8 polling and SDL3 event forwarding simultaneously would
  double-post every input event.

### Changes
All DirectInput8-dependent code is wrapped in `#ifndef SS_USE_SDL_INPUT` blocks.
The SDL mode provides minimal stubs for the public API surface:

| Function | Non-SDL (DI8) behaviour | SDL stub behaviour |
|---|---|---|
| `InitInput` | Creates DI8 mouse + keyboard devices | Clears message queue, returns true |
| `DoneInput` | Releases DI8 device handles | Clears message queue, returns true |
| `PumpMessages` | Polls DI8 buffered data, fills message queue | No-op (bridge feeds queue directly) |
| `GetKeyForMessage` | Reverse-maps DI8 action IDs to VK codes | Returns false / 0 |
| `GetControlID` | Looks up action ID by string name | Returns -1 |
| `GetControlInfo` | Returns DI8 granularity for an action | Returns CT_UNKNOWN, 1.0f |
| Internal DI8 functions | `SetFocus`, `AddDeviceInfo`, `AddDeviceEnum`, etc. | Not compiled |

The `GetCharForKey` function (pure Win32 — no DI8 dependency) is unchanged.

### New function: `PushMessageSDL`
Added unconditionally (outside all `#ifdef` blocks):
```cpp
void PushMessageSDL(const SMessage& msg) {
    messages.push_back(msg);
}
```
This is the sole entry point through which the SDL3 bridge injects input events
into the existing NInput queue that game logic consumes via `GetMessage`.

---

## Patch 2: `Input.h` — `PushMessageSDL` declaration

Added to the `NInput` namespace:
```cpp
void PushMessageSDL(const SMessage& msg);
```
Exposed for completeness; `sdl_input_bridge.cpp` forward-declares locally to
avoid pulling Nival's PCH-dependent headers into a C++20 translation unit.

---

## Key design decisions

### Action ID encoding
Nival encodes action IDs as `INPUT_KEYID(deviceID, offset)`:
```c
#define INPUT_KEYID(vID, vOFFS) (((vID & 0xFF) << 24) | (vOFFS))
```
- Device 0 = mouse (registered first in `InitInput`)
- Device 1 = keyboard (registered second)
- Keyboard offset = `DIK_*` scan code
- Mouse axis offsets = `DIMOFS_X/Y/Z` (0, 4, 8 from `DIMOUSESTATE2`)
- Mouse button offsets = `DIMOFS_BUTTON0–4` (12–16)

The SDL bridge replicates this encoding so that `GetKeyForMessage` and the
game's key-binding lookups (which rely on `nAction` values) work correctly
when DI8 mode is restored for testing.

### No `dinput.h` in port code
`sdl_input_bridge.cpp` forward-declares `NInput::SMessage` and
`NInput::PushMessageSDL` locally rather than including `Input.h`. This keeps
DirectInput8 headers out of the modern C++20 port sources entirely.

### Mouse button mapping
| SDL button | DI8 name | nAction offset |
|---|---|---|
| SDL_BUTTON_LEFT | DIMOFS_BUTTON0 | 12 |
| SDL_BUTTON_RIGHT | DIMOFS_BUTTON1 | 13 |
| SDL_BUTTON_MIDDLE | DIMOFS_BUTTON2 | 14 |
| SDL_BUTTON_X1 | DIMOFS_BUTTON3 | 15 |
| SDL_BUTTON_X2 | DIMOFS_BUTTON4 | 16 |

### Mouse motion
SDL3 `xrel`/`yrel` deltas map directly to `CT_AXIS` messages on `DIMOFS_X`/`DIMOFS_Y`.
DI8 buffered mouse data is also relative (delta since last read), so the
semantics match.

### Mouse wheel
SDL3 `wheel.y` (positive = scroll up) maps to `DIMOFS_Z` axis, scaled by 120
(standard Windows scroll delta per notch).

---

## Files modified in upstream
- `upstream/Soft/Andy/Jan03/a5dll/Input/Input.cpp`
- `upstream/Soft/Andy/Jan03/a5dll/Input/Input.h`

## Files created in port
- `port/src/platform/sdl_to_dik.h`
- `port/src/platform/sdl_input_bridge.h`
- `port/src/platform/sdl_input_bridge.cpp`

## CMake changes
- `port/src/platform/CMakeLists.txt` — added `sdl_input_bridge.cpp`, linked `jan03::Input`
- `port/CMakeLists.txt` — added `target_compile_definitions(jan03_Input PRIVATE SS_USE_SDL_INPUT)`
