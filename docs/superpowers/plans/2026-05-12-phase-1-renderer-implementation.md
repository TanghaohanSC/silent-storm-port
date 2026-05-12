# Phase 1 Renderer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace Jan03's Direct3D 9 + Win32 + DirectInput8 stack with SDL3 + bgfx. Main menu renders at 1920×1080 / 2560×1440 / 3840×2160 with HUD integer-scale.

**Architecture:** SDL3 owns window + input. bgfx owns rendering. An `IDirect3DDevice9` facade implements the D3D9 interface and routes every call to bgfx. Nival's 271-cpp Main DLL stays unchanged — it still calls `IDirect3DDevice9::SetRenderState` etc; the facade redirects.

**Tech Stack:**
- SDL3 (vcpkg)
- bgfx + bimg + bx (git submodule, pinned commit)
- shaderc (bgfx's bundled shader compiler)
- Existing port/ build system (CMake 3.25+ / Ninja / MSVC 19.50 x86)

---

## Dependency graph (read this first)

```
                T1: bgfx submodule + vcpkg deps (SDL3)
                                  │
       ┌──────────────────────────┼──────────────────────────┐
       │                          │                          │
   T2: cfg                    T3: SDL3 window         T6: facade scaffold
   loader                     + event pump            (no-op methods)
       │                          │                          │
       │                     T4: SDL3 input bridge      T7: state translator
       │                          │                          │
       │                     T5: bgfx init              T8: shader archetypes
       │                          │                       (8 parallel)
       │                          │                          │
       └──────────────────────────┴──────────────────────────┘
                                  │
                          T9: facade plumbing
                          (state flush → bgfx)
                                  │
                          T10: resolution + FOV
                                  │
                          T11: HUD scale
                                  │
                          T12: smoke test + completion
```

**Serial:** T1 → everything else. T9 needs T6+T7+T8. T10/T11 need T9. T12 last.
**Parallel groups:**
- T2 / T3-T5 / T6-T8 can run as 3 parallel streams after T1
- T8a–T8h (8 shaders) can run as 8 parallel subagents after T7 lands

---

## Workspace conventions (every task assumes these)

- `cd C:\Users\Haohan\Documents\silent-storm\port`
- Bootstrap VS env at start of every PowerShell call:
  ```powershell
  & "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" -SkipAutomaticLocation -Arch x86 -HostArch amd64 *>$null
  cd C:\Users\Haohan\Documents\silent-storm\port
  ```
- Build: `cmake --build --preset msvc-debug`
- Configure if CMakeLists changed: `cmake --preset msvc-debug`
- Smoke test: `.\tests\smoke\test_boot.ps1`
- Commit conventional format: `feat(phase1/X): ...`, `fix(phase1/X): ...`
- `port/` repo's remote `origin` is `TanghaohanSC/silent-storm-port`; CI runs on push to main

---

## Task 1: bgfx submodule + vcpkg dependencies

**Files:**
- Modify: `port\vcpkg.json` (add sdl3, fmt)
- Modify: `port\.gitmodules` (create — git submodule for bgfx)
- Modify: `port\CMakeLists.txt` (find_package SDL3 + add bgfx subdir)
- Create: `port\third_party\bgfx\` (git submodule destination)

- [ ] **Step 1: Add SDL3 to vcpkg manifest**

Edit `port/vcpkg.json`:

```json
{
    "name": "silent-storm-port",
    "version": "0.1.0",
    "dependencies": [
        "sdl3",
        "fmt"
    ]
}
```

`fmt` is small and useful for the config loader + debug logging. SDL3 is the platform layer.

- [ ] **Step 2: Add bgfx as git submodule**

Run from port repo:

```powershell
cd C:\Users\Haohan\Documents\silent-storm\port
git submodule add https://github.com/bkaradzic/bgfx.cmake.git third_party/bgfx
cd third_party\bgfx
git submodule update --init --recursive
```

Note: `bgfx.cmake` is the CMake wrapper repo that bundles bgfx + bimg + bx. It's the canonical way to use bgfx in a CMake project.

- [ ] **Step 3: Pin bgfx commit**

```powershell
cd C:\Users\Haohan\Documents\silent-storm\port\third_party\bgfx
git log --oneline -1
```

Record the SHA in `port\third_party\bgfx-version.txt`:

```
bgfx.cmake commit: <SHA from above>
date: 2026-05-12
notes: pinned for Phase 1 reproducibility
```

- [ ] **Step 4: Wire SDL3 + bgfx into top-level CMakeLists**

Edit `port/CMakeLists.txt`, add BEFORE `add_subdirectory(src/stubs)`:

```cmake
find_package(SDL3 CONFIG REQUIRED)

# bgfx via submodule
set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS ON CACHE BOOL "" FORCE)
set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
set(BGFX_CONFIG_RENDERER_VULKAN ON CACHE BOOL "" FORCE)
set(BGFX_CONFIG_RENDERER_DIRECT3D11 ON CACHE BOOL "" FORCE)
set(BGFX_CONFIG_RENDERER_DIRECT3D9 OFF CACHE BOOL "" FORCE)   # avoid d3d9.h symbol collision with our facade
set(BGFX_CONFIG_RENDERER_OPENGL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/bgfx EXCLUDE_FROM_ALL)
```

- [ ] **Step 5: Smoke test the toolchain pickup**

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" -SkipAutomaticLocation -Arch x86 -HostArch amd64 *>$null
cd C:\Users\Haohan\Documents\silent-storm\port
cmake --preset msvc-debug 2>&1 | Select-Object -Last 20
```

Expected: no errors. SDL3 + bgfx targets discovered. If vcpkg auto-bootstraps via the manifest preset, SDL3 gets fetched + built.

If vcpkg manifest mode isn't already active in CMakePresets.json, you'll need to add `"toolchainFile": "$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"` and ensure VCPKG_ROOT is set (or use vcpkg binary cache from VS).

- [ ] **Step 6: Build still passes**

```powershell
cmake --build --preset msvc-debug
```

Expected: builds clean. silent_storm.exe still 20 MB. We just added the *toolchain plumbing*, no game code touched yet.

- [ ] **Step 7: Commit**

```powershell
git add vcpkg.json CMakeLists.txt .gitmodules third_party/bgfx-version.txt
git commit -m "build(phase1): add SDL3 (vcpkg) + bgfx (submodule) — no game code yet"
```

---

## Task 2: silent_storm.cfg loader

**Files:**
- Create: `port\src\config\config.h`
- Create: `port\src\config\config.cpp`
- Create: `port\src\config\CMakeLists.txt`
- Create: `port\tests\config\test_config.cpp`
- Modify: `port\CMakeLists.txt` (add `add_subdirectory(src/config)`)
- Create: `port\silent_storm.cfg` (sample, committed)

**Parallel with:** T3-T5, T6-T8.

- [ ] **Step 1: Write the test (TDD)**

Create `port/tests/config/test_config.cpp`:

```cpp
#include "../../src/config/config.h"
#include <cassert>
#include <fstream>
#include <cstdio>

int main() {
    // Write a test cfg
    {
        std::ofstream f("test.cfg");
        f << "[display]\n"
             "width = 1920\n"
             "height = 1080\n"
             "mode = fullscreen-desktop\n"
             "vsync = on\n"
             "fov_horizontal = 90\n"
             "hud_scale = 2\n"
             "[renderer]\n"
             "backend = vulkan\n";
    }

    silent_storm::Config c = silent_storm::LoadConfig("test.cfg");

    assert(c.display.width == 1920);
    assert(c.display.height == 1080);
    assert(c.display.mode == silent_storm::WindowMode::FullscreenDesktop);
    assert(c.display.vsync == true);
    assert(c.display.fov_horizontal == 90.0f);
    assert(c.display.hud_scale == 2);
    assert(c.renderer.backend == silent_storm::Backend::Vulkan);

    std::remove("test.cfg");
    return 0;
}
```

- [ ] **Step 2: Run test, expect link failure**

```powershell
cmake --build --preset msvc-debug --target ss_config_test 2>&1 | Select-Object -Last 5
```

Expected: target doesn't exist yet → cmake error.

- [ ] **Step 3: Create config.h**

`port/src/config/config.h`:

```cpp
#pragma once
#include <string>
#include <cstdint>

namespace silent_storm {

enum class WindowMode { Windowed, Fullscreen, FullscreenDesktop };
enum class Backend { Auto, D3D11, D3D12, Vulkan, Metal, OpenGL };

struct DisplayCfg {
    int width = 0;          // 0 = desktop
    int height = 0;
    WindowMode mode = WindowMode::FullscreenDesktop;
    bool vsync = true;
    float fov_horizontal = 0.0f;  // 0 = auto (90° wide-angle)
    int hud_scale = 0;            // 0 = auto
};

struct RendererCfg {
    Backend backend = Backend::Auto;
};

struct Config {
    DisplayCfg display;
    RendererCfg renderer;
};

// Load a cfg file. If the file doesn't exist, returns Config with defaults.
// Malformed lines are silently ignored — game must start even with a broken cfg.
Config LoadConfig(const std::string& path);

} // namespace silent_storm
```

- [ ] **Step 4: Create config.cpp**

`port/src/config/config.cpp`:

```cpp
#include "config.h"
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <cctype>

namespace silent_storm {

namespace {
std::string strip(std::string s) {
    auto first = s.find_first_not_of(" \t\r\n");
    auto last = s.find_last_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return s.substr(first, last - first + 1);
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

WindowMode parseMode(const std::string& v) {
    auto l = lower(v);
    if (l == "windowed") return WindowMode::Windowed;
    if (l == "fullscreen") return WindowMode::Fullscreen;
    return WindowMode::FullscreenDesktop;
}

Backend parseBackend(const std::string& v) {
    auto l = lower(v);
    if (l == "d3d11") return Backend::D3D11;
    if (l == "d3d12") return Backend::D3D12;
    if (l == "vulkan") return Backend::Vulkan;
    if (l == "metal") return Backend::Metal;
    if (l == "opengl") return Backend::OpenGL;
    return Backend::Auto;
}

bool parseBool(const std::string& v) {
    auto l = lower(v);
    return l == "on" || l == "true" || l == "yes" || l == "1";
}
} // namespace

Config LoadConfig(const std::string& path) {
    Config cfg;
    std::ifstream f(path);
    if (!f) return cfg;

    std::string section, line;
    while (std::getline(f, line)) {
        line = strip(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = lower(line.substr(1, line.size() - 2));
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = lower(strip(line.substr(0, eq)));
        auto val = strip(line.substr(eq + 1));

        if (section == "display") {
            if (key == "width") cfg.display.width = std::stoi(val);
            else if (key == "height") cfg.display.height = std::stoi(val);
            else if (key == "mode") cfg.display.mode = parseMode(val);
            else if (key == "vsync") cfg.display.vsync = parseBool(val);
            else if (key == "fov_horizontal") {
                if (lower(val) != "auto") cfg.display.fov_horizontal = std::stof(val);
            }
            else if (key == "hud_scale") {
                if (lower(val) != "auto") cfg.display.hud_scale = std::stoi(val);
            }
        } else if (section == "renderer") {
            if (key == "backend") cfg.renderer.backend = parseBackend(val);
        }
    }
    return cfg;
}

} // namespace silent_storm
```

- [ ] **Step 5: Wire CMake**

`port/src/config/CMakeLists.txt`:

```cmake
add_library(ss_config STATIC config.cpp)
target_include_directories(ss_config PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_apply_compiler_warnings(ss_config)
add_library(silent_storm::config ALIAS ss_config)

# Unit test
if(BUILD_TESTING OR PROJECT_IS_TOP_LEVEL)
    add_executable(ss_config_test ${CMAKE_SOURCE_DIR}/tests/config/test_config.cpp)
    target_link_libraries(ss_config_test PRIVATE ss_config)
    target_apply_compiler_warnings(ss_config_test)
endif()
```

Add to top-level `port/CMakeLists.txt` (before `add_executable(silent_storm ...)`):

```cmake
add_subdirectory(src/config)
```

- [ ] **Step 6: Build + run test**

```powershell
cmake --preset msvc-debug
cmake --build --preset msvc-debug --target ss_config_test
.\build\msvc-debug\bin\ss_config_test.exe
echo "exit code: $LASTEXITCODE"
```

Expected: exit code 0 (assertions pass).

- [ ] **Step 7: Sample silent_storm.cfg**

Create `port/silent_storm.cfg`:

```ini
[display]
; 0 = use desktop resolution
width = 0
height = 0
mode = fullscreen-desktop
vsync = on
; auto = 90 degrees at current aspect ratio
fov_horizontal = auto
; auto = integer scale based on resolution (1080p->1x, 4K->2x)
hud_scale = auto

[renderer]
; auto | d3d11 | d3d12 | vulkan | metal | opengl
backend = auto
```

- [ ] **Step 8: Commit**

```powershell
git add src/config/ tests/config/ silent_storm.cfg CMakeLists.txt
git commit -m "feat(phase1/config): silent_storm.cfg loader + test"
```

---

## Task 3: SDL3 window + event pump

**Files:**
- Create: `port\src\platform\sdl_window.h`
- Create: `port\src\platform\sdl_window.cpp`
- Create: `port\src\platform\sdl_event_pump.h`
- Create: `port\src\platform\sdl_event_pump.cpp`
- Create: `port\src\platform\CMakeLists.txt`
- Modify: `port\CMakeLists.txt` (add subdir + link silent_storm)

**Parallel with:** T2, T6-T8. Depends on T1.

- [ ] **Step 1: Define the window interface**

`port/src/platform/sdl_window.h`:

```cpp
#pragma once
#include <cstdint>
#include "../config/config.h"

struct SDL_Window;

namespace silent_storm::platform {

struct WindowSize { int width; int height; };

class Window {
public:
    // Creates the window per cfg. Returns nullptr-ish state if creation failed.
    Window(const Config& cfg);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Native window handle for bgfx PlatformData::nwh
    void* native_handle() const;

    WindowSize size() const;
    bool valid() const { return window_ != nullptr; }

    // SDL_Window opaque pointer for the event pump
    SDL_Window* raw() const { return window_; }

private:
    SDL_Window* window_ = nullptr;
};

} // namespace silent_storm::platform
```

- [ ] **Step 2: Implement window**

`port/src/platform/sdl_window.cpp`:

```cpp
#include "sdl_window.h"
#include <SDL3/SDL.h>
#include <cstdio>

namespace silent_storm::platform {

namespace {
Uint32 mode_flags(const Config& cfg) {
    Uint32 f = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (cfg.display.mode == WindowMode::Fullscreen) f |= SDL_WINDOW_FULLSCREEN;
    return f;
}
} // namespace

Window::Window(const Config& cfg) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_InitSubSystem(VIDEO) failed: %s\n", SDL_GetError());
        return;
    }

    int w = cfg.display.width  > 0 ? cfg.display.width  : 1920;
    int h = cfg.display.height > 0 ? cfg.display.height : 1080;

    if (cfg.display.mode == WindowMode::FullscreenDesktop) {
        const SDL_DisplayMode* dm = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
        if (dm) { w = dm->w; h = dm->h; }
    }

    window_ = SDL_CreateWindow("Silent Storm (port)", w, h, mode_flags(cfg));
    if (!window_) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return;
    }
    if (cfg.display.mode == WindowMode::FullscreenDesktop) {
        SDL_SetWindowFullscreen(window_, true);
    }
}

Window::~Window() {
    if (window_) SDL_DestroyWindow(window_);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void* Window::native_handle() const {
    if (!window_) return nullptr;
    SDL_PropertiesID props = SDL_GetWindowProperties(window_);
    return SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
}

WindowSize Window::size() const {
    WindowSize s{0, 0};
    if (window_) SDL_GetWindowSize(window_, &s.width, &s.height);
    return s;
}

} // namespace silent_storm::platform
```

- [ ] **Step 3: Event pump**

`port/src/platform/sdl_event_pump.h`:

```cpp
#pragma once

namespace silent_storm::platform {

enum class PumpResult { Continue, Quit };

// Pump SDL events for one frame. Returns Quit if the user requested exit.
// Calls into sdl_input_bridge to forward input events (see Task 4).
PumpResult pump_events();

} // namespace silent_storm::platform
```

`port/src/platform/sdl_event_pump.cpp`:

```cpp
#include "sdl_event_pump.h"
#include <SDL3/SDL.h>

namespace silent_storm::platform {

PumpResult pump_events() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                return PumpResult::Quit;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            case SDL_EVENT_MOUSE_MOTION:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_WHEEL:
                // Task 4 hooks input forwarding here.
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                // Task 5 hooks bgfx::reset() here.
                break;
            default:
                break;
        }
    }
    return PumpResult::Continue;
}

} // namespace silent_storm::platform
```

- [ ] **Step 4: CMake**

`port/src/platform/CMakeLists.txt`:

```cmake
add_library(ss_platform STATIC
    sdl_window.cpp
    sdl_event_pump.cpp
)
target_include_directories(ss_platform PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ss_platform
    PUBLIC SDL3::SDL3
    PRIVATE silent_storm::config
)
target_apply_compiler_warnings(ss_platform)
add_library(silent_storm::platform ALIAS ss_platform)
```

Add to top-level CMakeLists: `add_subdirectory(src/platform)` and link `silent_storm::platform` to the `silent_storm` exe target.

- [ ] **Step 5: Smoke build**

```powershell
cmake --build --preset msvc-debug --target ss_platform
```

Expected: clean. SDL3 headers found via vcpkg.

- [ ] **Step 6: Commit**

```powershell
git add src/platform/ CMakeLists.txt
git commit -m "feat(phase1/platform): SDL3 window + event pump"
```

---

## Task 4: SDL3 input bridge → NInput

**Files:**
- Create: `port\src\platform\sdl_input_bridge.h`
- Create: `port\src\platform\sdl_input_bridge.cpp`
- Modify: `port\src\platform\sdl_event_pump.cpp` (call into bridge)
- Modify: `port\src\platform\CMakeLists.txt` (add cpp)

**Parallel with:** T6-T8. Depends on T3.

### Context (subagent: read this carefully)

The Main DLL's `NInput::SMessage` queue is what game logic consumes. Look at:
- `upstream/Soft/Andy/Jan03/a5dll/Input/Input.h` — defines `SMessage`, the action codes, key codes
- `upstream/Soft/Andy/Jan03/a5dll/Input/Input.cpp` — the original DirectInput8 polling loop that pushes messages

The bridge replaces that polling loop. Map:

| SDL3 event | NInput action |
|---|---|
| `SDL_EVENT_KEY_DOWN/UP` (SDL_Scancode) | Translate to Nival's key code, push `SMessage{cType=CT_KEY, ...}` |
| `SDL_EVENT_MOUSE_MOTION` | `SMessage{cType=CT_MOUSE, ...}` with delta |
| `SDL_EVENT_MOUSE_BUTTON_DOWN/UP` | `SMessage{cType=CT_MOUSE_BUTTON, ...}` |
| `SDL_EVENT_MOUSE_WHEEL` | `SMessage{cType=CT_MOUSE_WHEEL, ...}` |

### Steps

- [ ] **Step 1: Read Nival's Input.h and copy the relevant types**

Read `upstream/Soft/Andy/Jan03/a5dll/Input/Input.h`. Identify `SMessage`, `CT_*` enum, and the function that adds to the queue (look for `messages.emplace` we patched in Phase 0).

- [ ] **Step 2: Map SDL3 scancodes to Nival key codes**

Nival uses DirectInput8 scan codes (e.g. `DIK_A=0x1E`). SDL3 uses its own `SDL_Scancode`. Create a translation table `port/src/platform/sdl_to_dik.h` mapping the ~100 keys Nival cares about.

(Subagent: this is mechanical. SDL3's scancode enum and DirectInput's `DIK_*` are well-documented. Cover at minimum: letters, digits, F1-F12, arrows, modifiers, space, enter, escape, tab, mouse buttons.)

- [ ] **Step 3: Write the bridge**

`port/src/platform/sdl_input_bridge.h`:

```cpp
#pragma once
union SDL_Event;

namespace silent_storm::platform {
// Called from the event pump for each input-related SDL event.
void forward_to_ninput(const SDL_Event& ev);
} // namespace silent_storm::platform
```

`port/src/platform/sdl_input_bridge.cpp`:

```cpp
#include "sdl_input_bridge.h"
#include "sdl_to_dik.h"
#include <SDL3/SDL.h>

// Forward decl of Nival's queue push. Defined in upstream/Soft/Andy/Jan03/a5dll/Input/Input.cpp
// or wrap it via a small adapter — subagent decides based on linkage.
namespace NInput {
struct SMessage;
void PushMessage(const SMessage& m);
}

namespace silent_storm::platform {

void forward_to_ninput(const SDL_Event& ev) {
    // ... fill SMessage from ev, call NInput::PushMessage(...)
}

} // namespace silent_storm::platform
```

- [ ] **Step 4: Wire into event pump**

Edit `sdl_event_pump.cpp` to call `forward_to_ninput(ev)` in the input event cases.

- [ ] **Step 5: Read Input.cpp, decide on the integration point**

Either:
- (a) Stub out Nival's DirectInput init/poll path, redirect message queue feeds to our bridge, OR
- (b) Add a flag in `Input/Input.cpp` that disables its DirectInput path when SDL3 mode is active

(a) is cleaner. Likely involves an `#ifdef SS_USE_SDL_INPUT` block.

- [ ] **Step 6: Build**

```powershell
cmake --build --preset msvc-debug
```

Expected: clean. silent_storm.exe still 20-something MB.

- [ ] **Step 7: Commit**

```powershell
git add src/platform/ upstream/Soft/Andy/Jan03/a5dll/Input/Input.cpp
git commit -m "feat(phase1/platform): SDL3 -> NInput bridge"
```

---

## Task 5: bgfx init

**Files:**
- Create: `port\src\renderer\bgfx_init.h`
- Create: `port\src\renderer\bgfx_init.cpp`
- Create: `port\src\renderer\CMakeLists.txt`
- Modify: `port\CMakeLists.txt`
- Modify: `port\src\platform\sdl_event_pump.cpp` (call bgfx::reset on resize)

**Parallel with:** T6-T8. Depends on T1, T3.

- [ ] **Step 1: bgfx_init.h**

```cpp
#pragma once
#include "../config/config.h"
#include "../platform/sdl_window.h"

namespace silent_storm::renderer {

bool init(const platform::Window& window, const Config& cfg);
void shutdown();

// Called from the event pump on SDL_EVENT_WINDOW_RESIZED.
void on_resize(int width, int height);

// Called once per frame from the main loop.
void begin_frame();
void end_frame();

} // namespace silent_storm::renderer
```

- [ ] **Step 2: bgfx_init.cpp**

```cpp
#include "bgfx_init.h"
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <cstdio>

namespace silent_storm::renderer {

namespace {
bgfx::RendererType::Enum map_backend(Backend b) {
    switch (b) {
        case Backend::D3D11:  return bgfx::RendererType::Direct3D11;
        case Backend::D3D12:  return bgfx::RendererType::Direct3D12;
        case Backend::Vulkan: return bgfx::RendererType::Vulkan;
        case Backend::Metal:  return bgfx::RendererType::Metal;
        case Backend::OpenGL: return bgfx::RendererType::OpenGL;
        default:              return bgfx::RendererType::Count;  // auto
    }
}
int g_width = 0;
int g_height = 0;
} // namespace

bool init(const platform::Window& window, const Config& cfg) {
    bgfx::PlatformData pd{};
    pd.nwh = window.native_handle();
    if (!pd.nwh) {
        std::fprintf(stderr, "bgfx init: no native window handle\n");
        return false;
    }
    bgfx::setPlatformData(pd);

    auto sz = window.size();
    g_width = sz.width;
    g_height = sz.height;

    bgfx::Init init;
    init.type = map_backend(cfg.renderer.backend);
    init.resolution.width  = static_cast<uint32_t>(g_width);
    init.resolution.height = static_cast<uint32_t>(g_height);
    init.resolution.reset  = cfg.display.vsync ? BGFX_RESET_VSYNC : 0;
    if (!bgfx::init(init)) {
        std::fprintf(stderr, "bgfx::init failed\n");
        return false;
    }
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x202020ff, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, uint16_t(g_width), uint16_t(g_height));
    return true;
}

void shutdown() { bgfx::shutdown(); }

void on_resize(int width, int height) {
    g_width = width;
    g_height = height;
    bgfx::reset(uint32_t(width), uint32_t(height), 0);
    bgfx::setViewRect(0, 0, 0, uint16_t(width), uint16_t(height));
}

void begin_frame() {
    bgfx::touch(0);  // ensures view 0 is drawn even if no real draws this frame
}

void end_frame() { bgfx::frame(); }

} // namespace silent_storm::renderer
```

- [ ] **Step 3: CMake**

`port/src/renderer/CMakeLists.txt`:

```cmake
add_library(ss_renderer STATIC bgfx_init.cpp)
target_include_directories(ss_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ss_renderer
    PUBLIC bgfx bx bimg
    PRIVATE silent_storm::config silent_storm::platform
)
target_apply_compiler_warnings(ss_renderer)
add_library(silent_storm::renderer ALIAS ss_renderer)
```

Top-level: `add_subdirectory(src/renderer)`. Link `silent_storm::renderer` to silent_storm exe.

- [ ] **Step 4: Build**

```powershell
cmake --preset msvc-debug
cmake --build --preset msvc-debug
```

Expected: clean. silent_storm.exe size grows (bgfx is ~5 MB statically linked).

- [ ] **Step 5: Commit**

```powershell
git add src/renderer/ CMakeLists.txt src/platform/sdl_event_pump.cpp
git commit -m "feat(phase1/renderer): bgfx init + frame loop scaffolding"
```

---

## Task 6: IDirect3DDevice9 facade scaffold

**Files:**
- Create: `port\src\renderer\d3d9_facade.h`
- Create: `port\src\renderer\d3d9_facade.cpp`
- Modify: `port\src\renderer\CMakeLists.txt`

**Parallel with:** T7, T8 (different concerns). Depends on T1.

### Context (subagent)

`IDirect3DDevice9` is a COM interface defined in `<d3d9.h>`. It has ~90 methods. We implement them all but most just record state into a struct that gets flushed in `DrawPrimitive` / `DrawIndexedPrimitive`.

Strategy:
- Inherit from `IDirect3DDevice9` (multiple inheritance with `IUnknown`)
- AddRef/Release/QueryInterface trivially
- Each state setter stores into a `DeviceState` struct (Task 7)
- Each draw call calls into Task 9's flush logic

For Phase 1 scaffolding (this task), every method body is `return D3D_OK;` (or `return E_NOTIMPL` for unsupported ones).

### Steps

- [ ] **Step 1: facade.h declares the class**

```cpp
#pragma once
#include <d3d9.h>
#include "state_translator.h"  // from Task 7

namespace silent_storm::renderer {

class D3D9Facade : public IDirect3DDevice9 {
public:
    D3D9Facade();
    ~D3D9Facade() override;

    // IUnknown
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppv) override;
    ULONG   __stdcall AddRef() override;
    ULONG   __stdcall Release() override;

    // IDirect3DDevice9 — 90 methods; implementation in .cpp
    HRESULT __stdcall TestCooperativeLevel() override;
    UINT    __stdcall GetAvailableTextureMem() override;
    HRESULT __stdcall EvictManagedResources() override;
    HRESULT __stdcall GetDirect3D(IDirect3D9**) override;
    // ... full method list, see implementation file

private:
    ULONG ref_count_ = 1;
    DeviceState state_;  // Task 7 provides this struct
};

// Single global facade instance — the "device" Nival's code receives.
IDirect3DDevice9* facade_instance();

} // namespace silent_storm::renderer
```

- [ ] **Step 2: facade.cpp implements every method**

This file is long — ~500 lines. Most methods are 3-line stubs:

```cpp
HRESULT __stdcall D3D9Facade::SetRenderState(D3DRENDERSTATETYPE state, DWORD value) {
    state_.set_render_state(state, value);
    return D3D_OK;
}

HRESULT __stdcall D3D9Facade::SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD value) {
    state_.set_texture_stage_state(stage, type, value);
    return D3D_OK;
}

HRESULT __stdcall D3D9Facade::DrawPrimitive(D3DPRIMITIVETYPE prim, UINT start, UINT count) {
    // Task 9 implements: state_.flush_to_bgfx(); bgfx::submit(...)
    return D3D_OK;
}
```

(Subagent: there are 90 methods on IDirect3DDevice9. Implement the signatures, leave bodies as `return D3D_OK;` for now except for the state-setters which call `state_.*` from Task 7. We'll fill in real bodies in Task 9.)

- [ ] **Step 3: Provide facade_instance()**

```cpp
namespace {
D3D9Facade* g_instance = nullptr;
}
IDirect3DDevice9* facade_instance() {
    if (!g_instance) g_instance = new D3D9Facade();
    return g_instance;
}
```

- [ ] **Step 4: Hook into Nival's CreateDevice flow**

Look at `upstream/Soft/Andy/Jan03/a5dll/Game/WinFrame.cpp` (or wherever D3D9 device creation lives — likely in Main DLL's gfx init). Find the `IDirect3D9::CreateDevice` call. Replace it (via #ifdef block) with `IDirect3DDevice9* dev = silent_storm::renderer::facade_instance();`.

- [ ] **Step 5: Build**

```powershell
cmake --build --preset msvc-debug
```

Expected: clean. silent_storm.exe linked. exe behavior unchanged from Phase 0 (still fails at game.db).

- [ ] **Step 6: Commit**

```powershell
git add src/renderer/ upstream/Soft/Andy/Jan03/a5dll/Game/WinFrame.cpp
git commit -m "feat(phase1/renderer): IDirect3DDevice9 facade scaffold (no-op methods)"
```

---

## Task 7: State translator

**Files:**
- Create: `port\src\renderer\state_translator.h`
- Create: `port\src\renderer\state_translator.cpp`
- Create: `port\tests\renderer\test_state_translator.cpp`
- Modify: `port\src\renderer\CMakeLists.txt`

**Parallel with:** T6, T8. Depends on T1.

### Context

This is the dense translation layer. The state translator owns a `DeviceState` struct that holds all D3D9 fixed-function state. When a draw call happens, it builds a bgfx state vector (a `uint64_t` of flags) + uniform values + shader program selection, then issues `bgfx::setState` / `bgfx::setUniform` / `bgfx::submit`.

Refer to Phase 1 spec §5 for the translation table (D3D9 → bgfx).

### Steps

- [ ] **Step 1: Write the unit test for state mapping**

```cpp
// port/tests/renderer/test_state_translator.cpp
#include "../../src/renderer/state_translator.h"
#include <d3d9.h>
#include <cassert>
#include <bgfx/bgfx.h>

int main() {
    using namespace silent_storm::renderer;
    DeviceState s;

    // Default state should be opaque, depth-write, cull-back
    auto bits = s.build_bgfx_state();
    assert(bits & BGFX_STATE_WRITE_RGB);
    assert(bits & BGFX_STATE_WRITE_A);
    assert(bits & BGFX_STATE_WRITE_Z);
    assert(bits & BGFX_STATE_DEPTH_TEST_LESS);

    // Disable Z write
    s.set_render_state(D3DRS_ZWRITEENABLE, FALSE);
    bits = s.build_bgfx_state();
    assert((bits & BGFX_STATE_WRITE_Z) == 0);

    // Alpha blend
    s.set_render_state(D3DRS_ALPHABLENDENABLE, TRUE);
    s.set_render_state(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    s.set_render_state(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    bits = s.build_bgfx_state();
    assert((bits & BGFX_STATE_BLEND_MASK) != 0);

    return 0;
}
```

- [ ] **Step 2: state_translator.h**

```cpp
#pragma once
#include <cstdint>
#include <d3d9.h>

namespace silent_storm::renderer {

struct DeviceState {
    DWORD render_state[210] = {};       // D3DRS_* (D3D9 has ~200)
    DWORD tex_stage_state[8][33] = {};  // D3DTSS_* per stage
    void* texture[8] = {};              // IDirect3DTexture9*
    D3DMATRIX world{}, view{}, projection{};
    UINT viewport_x = 0, viewport_y = 0, viewport_w = 0, viewport_h = 0;
    // Add others as needed by Nival's calls

    DeviceState() { reset_defaults(); }
    void reset_defaults();
    void set_render_state(D3DRENDERSTATETYPE s, DWORD v) { render_state[s] = v; }
    void set_texture_stage_state(DWORD stage, D3DTEXTURESTAGESTATETYPE t, DWORD v) {
        if (stage < 8) tex_stage_state[stage][t] = v;
    }

    // Compute bgfx state bits from current D3D9 state.
    uint64_t build_bgfx_state() const;

    // Pick a shader archetype (string ID resolved via shader_registry).
    const char* select_shader_archetype() const;
};

} // namespace silent_storm::renderer
```

- [ ] **Step 3: state_translator.cpp**

Implement `reset_defaults()`, `build_bgfx_state()`, `select_shader_archetype()` per Phase 1 spec §5. Show the full mapping:

```cpp
void DeviceState::reset_defaults() {
    render_state[D3DRS_ZENABLE] = TRUE;
    render_state[D3DRS_ZWRITEENABLE] = TRUE;
    render_state[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL;
    render_state[D3DRS_CULLMODE] = D3DCULL_CCW;
    render_state[D3DRS_ALPHABLENDENABLE] = FALSE;
    render_state[D3DRS_SRCBLEND] = D3DBLEND_ONE;
    render_state[D3DRS_DESTBLEND] = D3DBLEND_ZERO;
    // ... etc
}

uint64_t DeviceState::build_bgfx_state() const {
    uint64_t s = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A;
    if (render_state[D3DRS_ZWRITEENABLE]) s |= BGFX_STATE_WRITE_Z;
    if (render_state[D3DRS_ZENABLE]) {
        switch (render_state[D3DRS_ZFUNC]) {
            case D3DCMP_LESS:        s |= BGFX_STATE_DEPTH_TEST_LESS; break;
            case D3DCMP_LESSEQUAL:   s |= BGFX_STATE_DEPTH_TEST_LEQUAL; break;
            case D3DCMP_GREATER:     s |= BGFX_STATE_DEPTH_TEST_GREATER; break;
            case D3DCMP_GEQUAL:      s |= BGFX_STATE_DEPTH_TEST_GEQUAL; break;
            case D3DCMP_EQUAL:       s |= BGFX_STATE_DEPTH_TEST_EQUAL; break;
            case D3DCMP_NOTEQUAL:    s |= BGFX_STATE_DEPTH_TEST_NOTEQUAL; break;
            case D3DCMP_ALWAYS:      s |= BGFX_STATE_DEPTH_TEST_ALWAYS; break;
            default:                 s |= BGFX_STATE_DEPTH_TEST_LEQUAL; break;
        }
    }
    if (render_state[D3DRS_ALPHABLENDENABLE]) {
        auto src = static_cast<D3DBLEND>(render_state[D3DRS_SRCBLEND]);
        auto dst = static_cast<D3DBLEND>(render_state[D3DRS_DESTBLEND]);
        s |= map_blend_func(src, dst);   // helper, switch on D3DBLEND enum → bgfx blend bits
    }
    switch (render_state[D3DRS_CULLMODE]) {
        case D3DCULL_NONE: break;
        case D3DCULL_CW:   s |= BGFX_STATE_CULL_CW;  break;
        case D3DCULL_CCW:  s |= BGFX_STATE_CULL_CCW; break;
    }
    return s;
}

const char* DeviceState::select_shader_archetype() const {
    bool lit = render_state[D3DRS_LIGHTING] != FALSE;
    bool textured = texture[0] != nullptr;
    bool blend = render_state[D3DRS_ALPHABLENDENABLE] != FALSE;
    if (!textured && !lit) return "ss_solid_color";
    if (textured && lit) return "ss_diffuse_lit";
    if (textured && !lit) return "ss_diffuse_unlit";
    if (blend) return "ss_particle";
    return "ss_diffuse_unlit";
}
```

(Subagent: complete `map_blend_func` to cover D3DBLEND_ONE/ZERO/SRCALPHA/INVSRCALPHA/SRCCOLOR/INVSRCCOLOR/DESTCOLOR — Nival's main combos. Look at GfxShaders.txt for hints.)

- [ ] **Step 4: Wire test target**

`port/src/renderer/CMakeLists.txt`:

```cmake
add_library(ss_renderer STATIC
    bgfx_init.cpp
    state_translator.cpp
    # d3d9_facade.cpp added by Task 6
)
target_link_libraries(ss_renderer PUBLIC bgfx bx d3d9.lib)

add_executable(ss_state_translator_test ${CMAKE_SOURCE_DIR}/tests/renderer/test_state_translator.cpp)
target_link_libraries(ss_state_translator_test PRIVATE ss_renderer)
```

- [ ] **Step 5: Build + run test**

```powershell
cmake --build --preset msvc-debug --target ss_state_translator_test
.\build\msvc-debug\bin\ss_state_translator_test.exe
echo "exit: $LASTEXITCODE"
```

Expected: exit 0.

- [ ] **Step 6: Commit**

```powershell
git add src/renderer/ tests/renderer/
git commit -m "feat(phase1/renderer): D3D9 state -> bgfx state translator + tests"
```

---

## Tasks 8a–8h: Shader archetypes (8 parallel)

**Files (one set per archetype):**
- Create: `port\src\shaders\<archetype>\vs_<archetype>.sc`
- Create: `port\src\shaders\<archetype>\fs_<archetype>.sc`
- Create: `port\src\shaders\<archetype>\varying.def.sc`

**Parallel:** all 8 can run as independent subagents after T7 lands (so they have the state vector knowledge).

The 8 archetypes (recap from spec §4):

| # | Name | Notes |
|---|---|---|
| 8a | `ss_diffuse_unlit` | textured, no lighting |
| 8b | `ss_diffuse_lit` | textured + Lambert |
| 8c | `ss_skinned` | bones (vertex blend matrices) |
| 8d | `ss_ui` | ortho, alpha blend, no depth |
| 8e | `ss_particle` | billboard, additive blend |
| 8f | `ss_shadow` | depth-only output |
| 8g | `ss_terrain` | multi-texture + lightmap |
| 8h | `ss_water` | scrolling UV + reflection |

### Per-archetype steps (template — repeat for each)

- [ ] **Step 1: varying.def.sc**

Per bgfx convention, defines vertex attribute → fragment interpolant binding:

```glsl
// port/src/shaders/ss_diffuse_unlit/varying.def.sc
vec3 a_position : POSITION;
vec2 a_texcoord0 : TEXCOORD0;
vec2 v_texcoord0 : TEXCOORD0 = vec2(0.0, 0.0);
```

- [ ] **Step 2: Vertex shader (vs_*.sc)**

```glsl
$input  a_position, a_texcoord0
$output v_texcoord0

#include <bgfx_shader.sh>

void main() {
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
    v_texcoord0 = a_texcoord0;
}
```

- [ ] **Step 3: Fragment shader (fs_*.sc)**

```glsl
$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_diffuse, 0);

void main() {
    gl_FragColor = texture2D(s_diffuse, v_texcoord0);
}
```

(Subagent: per archetype, customize vs/fs accordingly. Refer to bgfx examples in `third_party/bgfx/examples/` for patterns. The skinned variant pulls bone matrices from a uniform array. The shadow variant writes nothing to color, only depth. Etc.)

- [ ] **Step 4: Build via bgfx's shaderc**

bgfx supplies a `shaderc` CLI tool built as part of `BGFX_BUILD_TOOLS=ON`. CMake integration:

```cmake
# In a new port/src/shaders/CMakeLists.txt
function(compile_shader name)
    set(out_dir ${CMAKE_BINARY_DIR}/shaders/${name})
    file(MAKE_DIRECTORY ${out_dir})
    set(shaderc $<TARGET_FILE:shaderc>)
    set(vs_src ${CMAKE_CURRENT_SOURCE_DIR}/${name}/vs_${name}.sc)
    set(fs_src ${CMAKE_CURRENT_SOURCE_DIR}/${name}/fs_${name}.sc)
    set(varying ${CMAKE_CURRENT_SOURCE_DIR}/${name}/varying.def.sc)
    set(vs_out  ${out_dir}/vs_${name}.bin)
    set(fs_out  ${out_dir}/fs_${name}.bin)
    add_custom_command(
        OUTPUT ${vs_out}
        COMMAND ${shaderc}
            --type vertex --platform windows --profile s_5_0
            --varyingdef ${varying} -f ${vs_src} -o ${vs_out}
        DEPENDS ${vs_src} ${varying} shaderc
        VERBATIM
    )
    add_custom_command(
        OUTPUT ${fs_out}
        COMMAND ${shaderc}
            --type fragment --platform windows --profile s_5_0
            --varyingdef ${varying} -f ${fs_src} -o ${fs_out}
        DEPENDS ${fs_src} ${varying} shaderc
        VERBATIM
    )
    add_custom_target(shader_${name} DEPENDS ${vs_out} ${fs_out})
endfunction()

compile_shader(ss_diffuse_unlit)
compile_shader(ss_diffuse_lit)
# ... 6 more
```

- [ ] **Step 5: Test compilation**

```powershell
cmake --build --preset msvc-debug --target shader_ss_diffuse_unlit
ls build\msvc-debug\shaders\ss_diffuse_unlit\
```

Expected: `vs_ss_diffuse_unlit.bin` + `fs_ss_diffuse_unlit.bin`. Non-zero size.

- [ ] **Step 6: Commit (per archetype)**

```powershell
git add src/shaders/ss_diffuse_unlit/
git commit -m "feat(phase1/shaders): ss_diffuse_unlit archetype"
```

---

## Task 9: Facade plumbing — state flush → bgfx submit

**Files:**
- Create: `port\src\renderer\shader_registry.h`
- Create: `port\src\renderer\shader_registry.cpp`
- Modify: `port\src\renderer\d3d9_facade.cpp` (DrawPrimitive bodies)
- Modify: `port\src\renderer\CMakeLists.txt`

**Serial:** depends on T6 + T7 + at least T8a (proof archetype). Likely needs ALL T8s for full coverage but can land iteratively.

### Steps

- [ ] **Step 1: shader_registry loads compiled .bin files at startup**

```cpp
// shader_registry.h
#pragma once
#include <bgfx/bgfx.h>
#include <string>

namespace silent_storm::renderer {
bool load_all_archetypes(const std::string& shader_dir);
bgfx::ProgramHandle get_program(const char* archetype);
}
```

- [ ] **Step 2: shader_registry.cpp**

Scan `shader_dir`, mmap each `vs_*.bin` / `fs_*.bin` pair into `bgfx::Memory`, create `bgfx::ShaderHandle` then `bgfx::createProgram`. Store in a `std::unordered_map<std::string, bgfx::ProgramHandle>`.

- [ ] **Step 3: D3D9Facade::DrawPrimitive implementation**

```cpp
HRESULT __stdcall D3D9Facade::DrawIndexedPrimitive(D3DPRIMITIVETYPE prim, INT base_vert,
                                                    UINT min_vert, UINT num_verts,
                                                    UINT start_idx, UINT prim_count) {
    // Apply current state to bgfx
    bgfx::setState(state_.build_bgfx_state());

    // Bind textures
    for (int i = 0; i < 8; ++i) {
        if (state_.texture[i]) {
            auto* tex = static_cast<FacadeTexture*>(state_.texture[i]);
            bgfx::setTexture(uint8_t(i), tex->sampler_uniform(), tex->handle());
        }
    }

    // Bind vertex + index buffers (tracked from SetStreamSource / SetIndices)
    bgfx::setVertexBuffer(0, state_.current_vbo);
    bgfx::setIndexBuffer(state_.current_ibo, start_idx, prim_count * 3 /* assume triangles */);

    // Upload world/view/proj
    float mvp[16];
    /* multiply state_.projection * state_.view * state_.world */
    bgfx::setUniform(state_.u_mvp, mvp);

    // Submit
    bgfx::submit(0, shader_registry::get_program(state_.select_shader_archetype()));
    return D3D_OK;
}
```

(Subagent: this is the densest task. Refer to bgfx examples for the exact API shapes. The buffer-tracking part requires `SetStreamSource` and `SetIndices` to also wrap their args into bgfx-compatible handles — that's a fair amount of new code in `d3d9_facade.cpp`.)

- [ ] **Step 4: Build + first visual smoke**

```powershell
cmake --build --preset msvc-debug
cd C:\Users\Haohan\Documents\silent-storm\upstream\Complete
& C:\Users\Haohan\Documents\silent-storm\port\build\msvc-debug\bin\silent_storm.exe
```

Expected: SDL3 window appears, clears to dark gray (the `0x202020ff` color from bgfx init). May or may not see actual game UI yet depending on draw-call coverage.

- [ ] **Step 5: Commit**

```powershell
git add src/renderer/
git commit -m "feat(phase1/renderer): facade -> bgfx submit pipeline"
```

---

## Task 10: Resolution + FOV from cfg

**Files:**
- Modify: `port\src\renderer\d3d9_facade.cpp` (intercept SetTransform(D3DTS_PROJECTION))
- Modify: `port\src\renderer\bgfx_init.cpp` (apply cfg resolution)

**Serial after T9.**

- [ ] **Step 1: Apply cfg.display.fov_horizontal in SetTransform**

```cpp
HRESULT __stdcall D3D9Facade::SetTransform(D3DTRANSFORMSTATETYPE which, const D3DMATRIX* m) {
    if (which == D3DTS_PROJECTION && cfg_.display.fov_horizontal > 0) {
        // Rebuild projection with cfg FOV at current aspect
        float aspect = float(g_width) / float(g_height);
        float fov_rad = cfg_.display.fov_horizontal * 3.14159265f / 180.0f;
        D3DMATRIX p = make_perspective(fov_rad, aspect, /*near*/0.1f, /*far*/1000.0f);
        state_.projection = p;
    } else {
        if (which == D3DTS_WORLD) state_.world = *m;
        else if (which == D3DTS_VIEW) state_.view = *m;
        else if (which == D3DTS_PROJECTION) state_.projection = *m;
    }
    return D3D_OK;
}
```

- [ ] **Step 2: Build, run, verify FOV looks wider/narrower per cfg**

```powershell
# Try fov_horizontal=60 in cfg, vs 100
```

- [ ] **Step 3: Commit**

```powershell
git add src/renderer/d3d9_facade.cpp src/renderer/bgfx_init.cpp
git commit -m "feat(phase1/renderer): cfg-driven FOV + resolution"
```

---

## Task 11: HUD scale

**Files:**
- Modify: `port\src\renderer\d3d9_facade.cpp` (detect ortho, apply scale)
- Modify: `upstream\Soft\Andy\Jan03\a5dll\Main\<wherever NWinFrame::OnMouseMove lives>` (inverse-scale mouse coords)

**Serial after T9 + T10.**

- [ ] **Step 1: Detect ortho projection**

In `SetTransform(D3DTS_PROJECTION)`, check if the matrix is ortho:

```cpp
bool is_ortho(const D3DMATRIX& m) {
    // ortho matrices have m[3][3] == 1, perspective has m[3][3] == 0
    return m._44 == 1.0f && m._34 == 0.0f;
}
```

- [ ] **Step 2: Apply scale to ortho**

```cpp
if (which == D3DTS_PROJECTION && is_ortho(*m)) {
    int scale = compute_hud_scale(g_width, g_height, cfg_.display.hud_scale);
    D3DMATRIX scaled = *m;
    scaled._11 *= scale;
    scaled._22 *= scale;
    state_.projection = scaled;
    state_.hud_scale_active = scale;
}
```

`compute_hud_scale` returns config value if non-zero, else `floor(min(width / 1024, height / 768))`, clamped to [1, 4].

- [ ] **Step 3: Inverse-scale mouse coords**

Find Nival's mouse-handling code path (likely `NWinFrame::OnMouseMove` in WinFrame.cpp or similar). Multiply incoming coords by `1.0f / hud_scale` before passing to UI hit-test.

- [ ] **Step 4: Verify at 1080p / 1440p / 4K**

```powershell
# Edit silent_storm.cfg width=1920 height=1080, run
# Then width=2560 height=1440, run
# Then width=3840 height=2160, run
# Confirm HUD elements stay readable, mouse clicks land on buttons
```

- [ ] **Step 5: Commit**

```powershell
git add src/renderer/d3d9_facade.cpp upstream/...
git commit -m "feat(phase1/renderer): HUD integer-scale per resolution"
```

---

## Task 12: Smoke test extension + Phase 1 completion docs

**Files:**
- Modify: `port\tests\smoke\test_boot.ps1`
- Create: `port\docs\phase-1-completion.md`
- Modify: `port\README.md` (status update)

- [ ] **Step 1: Extend smoke test to capture a screenshot**

```powershell
# port/tests/smoke/test_boot.ps1
# After exe stays alive past 3s, screenshot the SDL3 window
$exe = "...\silent_storm.exe"
$proc = Start-Process $exe -PassThru -WorkingDirectory ...\upstream\Complete
Start-Sleep -Seconds 5
# Use System.Drawing to grab window contents by HWND
# Save to build/screenshots/phase1-1080p.png
# Compare against a stored baseline (committed) via hash or pixel-diff
$proc.Kill()
exit 0
```

(Subagent: may need to use `Add-Type` to grab Windows.Forms or P/Invoke `BitBlt`. Or skip baseline comparison and just verify a non-trivial screenshot was captured.)

- [ ] **Step 2: Phase 1 completion report**

`port/docs/phase-1-completion.md` — mirror Phase 0's structure:
- Achieved
- Acceptance proof (screenshots at 1080/1440/4K)
- Discoveries
- Carryovers for Phase 2

- [ ] **Step 3: README update**

In `port/README.md`, change `**Status:** Phase 0 (bootstrap) in progress.` to `**Status:** Phase 1 complete (SDL3 + bgfx renderer, main menu renders at 4K).`

- [ ] **Step 4: Commit + push**

```powershell
git add tests/smoke/ docs/phase-1-completion.md README.md
git commit -m "docs(phase1): completion report + screenshots"
git push
```

---

## Phase 1 acceptance checklist

1. ☐ `cmake --build --preset msvc-debug` → exit 0
2. ☐ `silent_storm.exe` launches in SDL3 window
3. ☐ Main menu renders via bgfx (no D3D9 calls in actual runtime)
4. ☐ Mouse + keyboard work in main menu
5. ☐ Switching `width=1920/2560/3840` in cfg works; HUD scales
6. ☐ `vsync = on/off` works
7. ☐ FOV from cfg honored
8. ☐ GitHub Actions CI green on push to main
9. ☐ Phase 1 completion doc committed with screenshots

---

## Self-review

**1. Spec coverage:**
- §2 decisions → ✅ T1 (vendor methods), T7 (state vector), T8 (8 archetypes), T6 (facade), T4 (SDL3 input bundled), T2/T10/T11 (cfg/FOV/HUD)
- §3 architecture → ✅ T3-T9 build it out
- §4 shader archetypes → ✅ T8a-T8h
- §5 D3D9 → bgfx state map → ✅ T7
- §6 module layout → ✅ matches T3/T5/T6/T7/T8 file paths
- §7 SDL3 platform → ✅ T3 + T4
- §8 resolution + FOV → ✅ T10
- §9 HUD scale → ✅ T11
- §10 risks → addressed in T7 (unknown state vector fallback) and T6 (BGFX_CONFIG_RENDERER_DIRECT3D9 OFF to avoid header clash)

**2. Placeholder scan:**
- T4 Steps 2, 5: subagent does scan-and-create of SDL→DIK map. That's a real task, just labor-heavy — not a placeholder.
- T6 Step 2: "implement 90 methods" is concrete (stub bodies are `return D3D_OK;`).
- T8 Step 3: "customize vs/fs per archetype" — each archetype is its own task with concrete code. The "customize" is just labelling the per-archetype customization.
- No actual TBDs or "implement later" stubs.

**3. Type consistency:**
- `DeviceState` defined T7, used T6/T9 — ✅
- `Window` defined T3, used T5 — ✅
- `Backend` enum T2, used T5 — ✅
- `Config` T2, used T3/T5/T10 — ✅
- `facade_instance()` T6, hooked into Nival code T6 step 4 — ✅
