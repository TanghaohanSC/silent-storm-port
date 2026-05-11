# Phase 0: CMake Bootstrap — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 2003 年原版 Silent Storm 代码用现代 MSVC + CMake 编出可执行文件，启动后能跑到第一个 Bink 视频或 FMOD 音频调用处崩溃。在此过程中所有商业库（Bink/FMOD/StarForce/DirectPlay）都用 stub 替代，构成后续 Phase 的脚手架。

**源代码基线（2026-05-11 修正）**：`upstream/Soft/Andy/Jan03/a5dll/`（VS .NET 2003 solution，~30 子项目）。`upstream/Soft/Andy/{EnglishGold,RussianGold,RussianPatch1,Oct01,April01,Aug02,KRI}` 是 binary release 无源码；Oct02 是旧 snapshot。主入口 `Game/Main.cpp`。

**Architecture:** 把原 `Soft/` 当 "vendored legacy source"，原地不动；新写一个 `port/` 工程承载 CMake、stub、入口。stub 库提供原商业库的最小符号表，让链接器满意但运行时立刻 abort。stlport 优先尝试用现代 `std::` 替换，失败时再 vendor 老版本。

**Tech Stack:** CMake 3.25+、Ninja、MSVC 17.10+（VS 2022 17.10）、Windows SDK 10.0.22621+、vcpkg manifest 模式、GitHub Actions

---

## 上下文：本地工作目录

所有路径以 `C:\Users\Haohan\Documents\silent-storm\` 为根，下文称 `$ROOT`。
- `$ROOT\upstream\` — 从 GitHub clone 来的原仓库（只读对照）
- `$ROOT\port\` — 我们的现代化工程（独立 git 仓库）
- `$ROOT\docs\superpowers\` — 设计文档（已存在）

## 上下文：discovery 性质说明

Phase 0 前 3 个 Task 是**探查工作**，会产生 inventory 报告。第 6 Task 之后的「compile loop」具体步骤数取决于探查结果，所以设了一个 **Task 6.5 = 中途 replan 触发点**。Plan 写到那里会暂停，根据 inventory 结果追加具体的 compile-fix tasks 到本文件下半部分，再继续执行。

---

## Task 1：Workspace 初始化与 upstream clone

**Files:**
- 创建：`$ROOT\port\.gitignore`
- 创建：`$ROOT\port\README.md`（占位）
- 创建：`$ROOT\.gitignore`（顶层，忽略 build artifacts）

- [ ] **Step 1：clone 原仓库到 upstream/**

```powershell
cd C:\Users\Haohan\Documents\silent-storm
git clone https://github.com/TanghaohanSC/Silent-Storm.git upstream
```

期待输出：`Cloning into 'upstream'...` 接着 receiving objects 进度条，最后 `Resolving deltas: ... done.`。
**如果失败**（网络/权限）→ 立即停下，告诉用户，不继续。

- [ ] **Step 2：验证 Complete/ 资源是否在仓库里**

```powershell
Get-ChildItem -Path "C:\Users\Haohan\Documents\silent-storm\upstream\Complete" -Recurse -File | Measure-Object -Property Length -Sum | Format-List Count,Sum
```

期待：返回 Count 和 Sum（字节数）。如果 Count = 0 或 Complete/ 不存在，说明资源不在 git 里——立即停下问用户怎么搞到资源（原版游戏 ISO？fan archive？）。资源是 Phase 0 末尾 boot test 的前置条件。

- [ ] **Step 3：初始化 port/ 为独立 git 仓库**

```powershell
cd C:\Users\Haohan\Documents\silent-storm
New-Item -ItemType Directory -Path port -Force | Out-Null
cd port
git init -b main
```

期待：`Initialized empty Git repository in .../port/.git/`。

- [ ] **Step 4：写顶层 .gitignore**

文件 `$ROOT\port\.gitignore` 内容：

```gitignore
# CMake
build/
out/
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
Makefile
*.cmake.in

# Visual Studio
.vs/
*.user
*.suo
*.vcxproj.filters
x64/
Debug/
Release/

# vcpkg
vcpkg_installed/

# Binary outputs
*.exe
*.dll
*.lib
*.pdb
*.ilk
*.exp

# IDE
.idea/
.vscode/settings.json

# OS
Thumbs.db
.DS_Store
```

- [ ] **Step 5：占位 README**

文件 `$ROOT\port\README.md`：

```markdown
# Silent Storm Port

Open-source modernization of the 2003 Nival Interactive game Silent Storm,
based on the 2026 source release. See `../docs/superpowers/specs/` for design.

**Status:** Phase 0 (bootstrap) in progress.
```

- [ ] **Step 6：首个 commit**

```powershell
cd C:\Users\Haohan\Documents\silent-storm\port
git add .gitignore README.md
git commit -m "Initial port workspace"
```

期待：`[main (root-commit) ...] Initial port workspace`，2 file changed。

---

## Task 2：源码 inventory

**Files:**
- 创建：`$ROOT\port\docs\inventory.md`（探查报告）

- [ ] **Step 1：统计源文件数量与体积（按目录）**

```powershell
$upstream = "C:\Users\Haohan\Documents\silent-storm\upstream\Soft"
Get-ChildItem -Path $upstream -Recurse -Include *.cpp,*.c,*.h,*.hpp,*.inl |
  Group-Object { Split-Path (Resolve-Path -Path $_.FullName -Relative -RelativeBasePath $upstream) -Parent } |
  Sort-Object Count -Descending |
  Select-Object Count, Name |
  Format-Table -AutoSize
```

记录输出，留着填 inventory.md。

- [ ] **Step 2：搜索 Bink 调用点**

用 Grep 工具搜：
- pattern：`\bBink[A-Z][a-zA-Z]*\b`
- path：`C:\Users\Haohan\Documents\silent-storm\upstream\Soft`
- output_mode：`content`
- glob：`*.cpp,*.c,*.h`
- head_limit：50

记录所有出现的 Bink 函数名（去重），它们就是 stub 要提供的符号。

- [ ] **Step 3：搜索 FMOD 调用点**

用 Grep 搜两个 pattern（FMOD 历史上分 FMOD Music + FMOD Sound）：
- `\bFSOUND_[A-Z][a-zA-Z]*\b`
- `\bFMUSIC_[A-Z][a-zA-Z]*\b`

加 `\bFSOUND_SAMPLE\b`、`\bFMUSIC_MODULE\b` 等类型名搜一遍。

记录所有符号到 inventory.md。

- [ ] **Step 4：搜索 DirectPlay / DirectInput / StarForce 痕迹**

Grep pattern（一次一个）：
- `DirectPlay|IDirectPlay`
- `DirectInput|IDirectInput`
- `protect\.h|starforce|SF_|sfini`（StarForce SDK 历史符号）

记录到 inventory.md，并标记为「Phase 0 stub，Phase 1+ 替换或丢弃」。

- [ ] **Step 5：识别 stlport 依赖范围**

```powershell
$soft = "C:\Users\Haohan\Documents\silent-storm\upstream\Soft"
Get-ChildItem -Path "$soft\SDK\stlport" -Recurse -File | Measure-Object -Property Length -Sum | Format-List Count,Sum
```

然后 Grep 在 Soft/ 里找 `#include\s+<(stl/|stlport/)` 出现位置。如果出现非常普遍（>50 个文件）→ Task 4 走「保留 stlport」路线；如果 <20 个 → 走「迁移到 modern std」路线。

- [ ] **Step 6：识别入口点和 main 模块**

Grep `WinMain|wWinMain|int main`，定位主入口在哪个文件。这决定了 Task 7 链接 target 的入口。

- [ ] **Step 7：写 inventory.md**

文件 `$ROOT\port\docs\inventory.md`，把 Step 1-6 的所有发现整理成结构化报告：

```markdown
# Source Inventory (Phase 0)

Generated: <today's date>
Source root: upstream/Soft/

## File count by directory
| Directory | .cpp | .c | .h | total |
|---|---|---|---|---|
| Andy | ... | | | |
| Bandures | ... | | | |
...

## Bink symbols referenced
- BinkOpen
- BinkClose
- ... (full list)

## FMOD symbols referenced
- FSOUND_Init
- ... (full list)

## DirectPlay/DirectInput/StarForce
- IDirectInput8: <count> call sites
- StarForce: <none|symbols>

## stlport usage
- stlport headers in SDK/stlport: <size>
- #include <stl/...> call sites: <count>
- Decision: <migrate to std | keep stlport>

## Main entry
- File: upstream/Soft/<path>/<file>.cpp
- Signature: WinMain | wWinMain
```

- [ ] **Step 8：commit inventory**

```powershell
cd C:\Users\Haohan\Documents\silent-storm\port
git add docs\inventory.md
git commit -m "docs: source inventory for Phase 0"
```

---

## Task 3：最小 CMake 骨架（空 exe 能 build）

**Files:**
- 创建：`$ROOT\port\CMakeLists.txt`
- 创建：`$ROOT\port\src\main_stub.cpp`
- 创建：`$ROOT\port\cmake\CompilerWarnings.cmake`
- 创建：`$ROOT\port\vcpkg.json`
- 创建：`$ROOT\port\CMakePresets.json`

- [ ] **Step 1：顶层 CMakeLists.txt**

文件 `$ROOT\port\CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.25)

project(silent_storm
    VERSION 0.1.0
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CompilerWarnings)

# 最小 exe，下一 task 才挂原代码
add_executable(silent_storm WIN32 src/main_stub.cpp)
target_apply_compiler_warnings(silent_storm)
```

- [ ] **Step 2：CompilerWarnings.cmake**

文件 `$ROOT\port\cmake\CompilerWarnings.cmake`：

```cmake
function(target_apply_compiler_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /Zc:__cplusplus
            /Zc:preprocessor
            /utf-8
            /wd4100  # unreferenced formal parameter (legacy code noise)
            /wd4189  # local variable initialized but not referenced
            /wd4244  # conversion possible loss of data (legacy numeric mixing)
            /wd4267  # size_t -> int conversion
            /wd4996  # deprecated functions (sprintf etc.) — handle later
        )
        target_compile_definitions(${target} PRIVATE
            _CRT_SECURE_NO_WARNINGS
            NOMINMAX
            WIN32_LEAN_AND_MEAN
        )
    endif()
endfunction()
```

- [ ] **Step 3：最小 main_stub.cpp**

文件 `$ROOT\port\src\main_stub.cpp`：

```cpp
#include <windows.h>
#include <cstdio>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    MessageBoxA(nullptr,
        "Silent Storm port: Phase 0 stub. Not implemented.",
        "silent_storm",
        MB_OK | MB_ICONINFORMATION);
    return 0;
}
```

- [ ] **Step 4：vcpkg.json（空 manifest）**

文件 `$ROOT\port\vcpkg.json`：

```json
{
    "name": "silent-storm-port",
    "version": "0.1.0",
    "dependencies": []
}
```

Phase 0 不引外部依赖，留着后续 Phase 添加。

- [ ] **Step 5：CMakePresets.json**

文件 `$ROOT\port\CMakePresets.json`：

```json
{
    "version": 6,
    "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
    "configurePresets": [
        {
            "name": "msvc-debug",
            "displayName": "MSVC x64 Debug (Ninja)",
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/msvc-debug",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "Debug",
                "CMAKE_C_COMPILER": "cl",
                "CMAKE_CXX_COMPILER": "cl"
            },
            "environment": {
                "VSCMD_SKIP_SENDTELEMETRY": "1"
            }
        },
        {
            "name": "msvc-release",
            "inherits": "msvc-debug",
            "displayName": "MSVC x64 Release (Ninja)",
            "binaryDir": "${sourceDir}/build/msvc-release",
            "cacheVariables": {
                "CMAKE_BUILD_TYPE": "RelWithDebInfo"
            }
        }
    ],
    "buildPresets": [
        { "name": "msvc-debug", "configurePreset": "msvc-debug" },
        { "name": "msvc-release", "configurePreset": "msvc-release" }
    ]
}
```

- [ ] **Step 6：从 VS 2022 Developer PowerShell 配置 + 构建**

打开 "x64 Native Tools Command Prompt for VS 2022"（或对应 PowerShell 版），在其中：

```powershell
cd C:\Users\Haohan\Documents\silent-storm\port
cmake --preset msvc-debug
cmake --build --preset msvc-debug
```

期待最后两行：
```
[2/2] Linking CXX executable bin\silent_storm.exe
```

如果失败，先确认是不是没在 Developer 终端里运行（普通 PowerShell 不会有 `cl.exe` 在 PATH）。

- [ ] **Step 7：跑一下 exe 确认能弹消息框**

```powershell
.\build\msvc-debug\bin\silent_storm.exe
```

期待：弹出消息框 "Silent Storm port: Phase 0 stub. Not implemented."，点 OK 后退出。

- [ ] **Step 8：commit**

```powershell
git add CMakeLists.txt CMakePresets.json vcpkg.json cmake\ src\
git commit -m "build: minimal CMake skeleton with empty WinMain"
```

---

## Task 4：Stub 库定义（Bink / FMOD / StarForce）

**Files:**
- 创建：`$ROOT\port\src\stubs\bink_stub.h`
- 创建：`$ROOT\port\src\stubs\bink_stub.cpp`
- 创建：`$ROOT\port\src\stubs\fmod_stub.h`
- 创建：`$ROOT\port\src\stubs\fmod_stub.cpp`
- 创建：`$ROOT\port\src\stubs\starforce_stub.h`
- 创建：`$ROOT\port\src\stubs\starforce_stub.cpp`
- 创建：`$ROOT\port\src\stubs\CMakeLists.txt`
- 修改：`$ROOT\port\CMakeLists.txt`（add_subdirectory）

> **依赖 Task 2 inventory 的符号列表**。如果 inventory 报告 Bink 符号 = N 个，本 task 的 bink_stub 必须实现这 N 个；FMOD 同理。下面给的是 Silent Storm 同代游戏典型用到的符号集，**实施前对照 inventory 增删**。

- [ ] **Step 1：bink_stub.h — 最小 API 表面**

文件 `$ROOT\port\src\stubs\bink_stub.h`：

```cpp
#pragma once
// Bink Video stub — replaces RAD Game Tools binkw32.lib API surface.
// All entry points log + abort the process. Phase 3 replaces with FFmpeg.

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BINK BINK;
typedef BINK* HBINK;

HBINK __stdcall BinkOpen(const char* name, uint32_t flags);
void  __stdcall BinkClose(HBINK bnk);
int   __stdcall BinkDoFrame(HBINK bnk);
int   __stdcall BinkCopyToBuffer(HBINK bnk, void* dest, int dest_pitch,
                                  uint32_t dest_height, uint32_t dest_x,
                                  uint32_t dest_y, uint32_t flags);
void  __stdcall BinkNextFrame(HBINK bnk);
int   __stdcall BinkWait(HBINK bnk);
void  __stdcall BinkGoto(HBINK bnk, uint32_t frame, int flags);
int   __stdcall BinkPause(HBINK bnk, int pause);
void  __stdcall BinkSetVolume(HBINK bnk, int32_t volume);
int   __stdcall BinkSetSoundOnOff(HBINK bnk, int on);
void  __stdcall BinkSetSoundSystem(void* open_func, uint32_t param);

#ifdef __cplusplus
}
#endif
```

> **按 inventory 调整**：如果 Bink 还引用了 `BinkOpenWithOptions`、`BinkSetIOSize` 等额外符号，加进来。inventory 报告里不在这列表上的都补上；本列表里没被 inventory 引用的删掉（防止误导后续工作）。

- [ ] **Step 2：bink_stub.cpp**

文件 `$ROOT\port\src\stubs\bink_stub.cpp`：

```cpp
#include "bink_stub.h"
#include <cstdio>
#include <cstdlib>

namespace {
[[noreturn]] void abort_stub(const char* fn) {
    std::fprintf(stderr,
        "[bink_stub] %s called -- Bink replacement not implemented yet.\n"
        "This is Phase 0; video playback comes in Phase 3.\n", fn);
    std::abort();
}
}

extern "C" {

HBINK __stdcall BinkOpen(const char*, uint32_t)              { abort_stub("BinkOpen"); }
void  __stdcall BinkClose(HBINK)                              { abort_stub("BinkClose"); }
int   __stdcall BinkDoFrame(HBINK)                            { abort_stub("BinkDoFrame"); }
int   __stdcall BinkCopyToBuffer(HBINK, void*, int, uint32_t, uint32_t, uint32_t, uint32_t)
                                                              { abort_stub("BinkCopyToBuffer"); }
void  __stdcall BinkNextFrame(HBINK)                          { abort_stub("BinkNextFrame"); }
int   __stdcall BinkWait(HBINK)                               { abort_stub("BinkWait"); }
void  __stdcall BinkGoto(HBINK, uint32_t, int)                { abort_stub("BinkGoto"); }
int   __stdcall BinkPause(HBINK, int)                         { abort_stub("BinkPause"); }
void  __stdcall BinkSetVolume(HBINK, int32_t)                 { abort_stub("BinkSetVolume"); }
int   __stdcall BinkSetSoundOnOff(HBINK, int)                 { abort_stub("BinkSetSoundOnOff"); }
void  __stdcall BinkSetSoundSystem(void*, uint32_t)           { abort_stub("BinkSetSoundSystem"); }

}
```

- [ ] **Step 3：fmod_stub.h**

文件 `$ROOT\port\src\stubs\fmod_stub.h`。FMOD 旧版 C API 类型名按 inventory 调，下面是常见集合：

```cpp
#pragma once
// FMOD legacy stub — replaces fmodvc.lib (FMOD 3.x C API).
// Phase 2 replaces with miniaudio.

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FSOUND_SAMPLE  FSOUND_SAMPLE;
typedef struct FSOUND_STREAM  FSOUND_STREAM;
typedef struct FMUSIC_MODULE  FMUSIC_MODULE;

// Init / shutdown
signed char __stdcall FSOUND_Init(int mixrate, int maxsoftwarechannels, unsigned int flags);
void        __stdcall FSOUND_Close(void);
signed char __stdcall FSOUND_SetOutput(int outputtype);
signed char __stdcall FSOUND_SetDriver(int driver);

// Sample
FSOUND_SAMPLE* __stdcall FSOUND_Sample_Load(int index, const char* name,
                                            unsigned int mode, int offset, int length);
void           __stdcall FSOUND_Sample_Free(FSOUND_SAMPLE* sptr);
int            __stdcall FSOUND_PlaySound(int channel, FSOUND_SAMPLE* sptr);
signed char    __stdcall FSOUND_StopSound(int channel);
signed char    __stdcall FSOUND_SetVolume(int channel, int vol);
signed char    __stdcall FSOUND_SetPaused(int channel, signed char paused);

// Stream
FSOUND_STREAM* __stdcall FSOUND_Stream_Open(const char* name, unsigned int mode,
                                            int offset, int length);
signed char    __stdcall FSOUND_Stream_Close(FSOUND_STREAM* stream);
int            __stdcall FSOUND_Stream_Play(int channel, FSOUND_STREAM* stream);
signed char    __stdcall FSOUND_Stream_Stop(FSOUND_STREAM* stream);

// Music (mod files)
FMUSIC_MODULE* __stdcall FMUSIC_LoadSong(const char* name);
signed char    __stdcall FMUSIC_FreeSong(FMUSIC_MODULE* mod);
signed char    __stdcall FMUSIC_PlaySong(FMUSIC_MODULE* mod);
signed char    __stdcall FMUSIC_StopSong(FMUSIC_MODULE* mod);
signed char    __stdcall FMUSIC_SetMasterVolume(FMUSIC_MODULE* mod, int volume);

#ifdef __cplusplus
}
#endif
```

> 实施前对照 inventory：删掉源代码没用到的符号，加上 inventory 列出但这里没有的。

- [ ] **Step 4：fmod_stub.cpp**

文件 `$ROOT\port\src\stubs\fmod_stub.cpp`。FMOD 不同于 Bink，init 和「无声播放」要返回成功而不是 abort，否则游戏一启动就死。abort 只用在不能 fake 的接口上：

```cpp
#include "fmod_stub.h"
#include <cstdio>

namespace {
void log_call(const char* fn) {
    // Phase 0: log every call to see the call pattern. No dedup.
    // Phase 2 (miniaudio) replaces this; logging volume here is fine.
    std::fprintf(stderr, "[fmod_stub] %s (silent fake)\n", fn);
}
}

extern "C" {

signed char __stdcall FSOUND_Init(int, int, unsigned int)       { log_call("FSOUND_Init"); return 1; }
void        __stdcall FSOUND_Close(void)                         { log_call("FSOUND_Close"); }
signed char __stdcall FSOUND_SetOutput(int)                      { log_call("FSOUND_SetOutput"); return 1; }
signed char __stdcall FSOUND_SetDriver(int)                      { log_call("FSOUND_SetDriver"); return 1; }

FSOUND_SAMPLE* __stdcall FSOUND_Sample_Load(int, const char*, unsigned int, int, int)
                                                                  { log_call("FSOUND_Sample_Load"); return nullptr; }
void           __stdcall FSOUND_Sample_Free(FSOUND_SAMPLE*)      { log_call("FSOUND_Sample_Free"); }
int            __stdcall FSOUND_PlaySound(int, FSOUND_SAMPLE*)   { log_call("FSOUND_PlaySound"); return -1; }
signed char    __stdcall FSOUND_StopSound(int)                   { log_call("FSOUND_StopSound"); return 1; }
signed char    __stdcall FSOUND_SetVolume(int, int)              { log_call("FSOUND_SetVolume"); return 1; }
signed char    __stdcall FSOUND_SetPaused(int, signed char)      { log_call("FSOUND_SetPaused"); return 1; }

FSOUND_STREAM* __stdcall FSOUND_Stream_Open(const char*, unsigned int, int, int)
                                                                  { log_call("FSOUND_Stream_Open"); return nullptr; }
signed char    __stdcall FSOUND_Stream_Close(FSOUND_STREAM*)     { log_call("FSOUND_Stream_Close"); return 1; }
int            __stdcall FSOUND_Stream_Play(int, FSOUND_STREAM*) { log_call("FSOUND_Stream_Play"); return -1; }
signed char    __stdcall FSOUND_Stream_Stop(FSOUND_STREAM*)      { log_call("FSOUND_Stream_Stop"); return 1; }

FMUSIC_MODULE* __stdcall FMUSIC_LoadSong(const char*)            { log_call("FMUSIC_LoadSong"); return nullptr; }
signed char    __stdcall FMUSIC_FreeSong(FMUSIC_MODULE*)         { log_call("FMUSIC_FreeSong"); return 1; }
signed char    __stdcall FMUSIC_PlaySong(FMUSIC_MODULE*)         { log_call("FMUSIC_PlaySong"); return 1; }
signed char    __stdcall FMUSIC_StopSong(FMUSIC_MODULE*)         { log_call("FMUSIC_StopSong"); return 1; }
signed char    __stdcall FMUSIC_SetMasterVolume(FMUSIC_MODULE*, int) { log_call("FMUSIC_SetMasterVolume"); return 1; }

}
```

> Bink 选 abort 因为视频窗口卡住等不出来，用户立刻能看见崩。FMOD 选 silent fake 因为「无声」是合法状态，游戏能继续往下跑暴露更多 bug。

- [ ] **Step 5：starforce_stub.h / .cpp**

文件 `$ROOT\port\src\stubs\starforce_stub.h`：

```cpp
#pragma once
// StarForce DRM stub — always returns "valid disc / no protection violation".
// Phase 0 plan §8: StarForce code paths to be deleted entirely in a later sweep.

#ifdef __cplusplus
extern "C" {
#endif

int  __stdcall sfini(void);
int  __stdcall sfcheck(void);
void __stdcall sfclose(void);

#ifdef __cplusplus
}
#endif
```

文件 `$ROOT\port\src\stubs\starforce_stub.cpp`：

```cpp
#include "starforce_stub.h"

extern "C" {
int  __stdcall sfini(void)   { return 0; }   // 0 = success
int  __stdcall sfcheck(void) { return 0; }
void __stdcall sfclose(void) {}
}
```

> **inventory 决定符号名**。StarForce 不同版本符号不一样（sf_ini / sfi_init / DiscCheck 等），按 inventory Step 4 找到的真名重写。如果 inventory 没找到 StarForce 引用，删掉整个 starforce_stub，省得养死代码。

- [ ] **Step 6：stubs CMakeLists.txt**

文件 `$ROOT\port\src\stubs\CMakeLists.txt`：

```cmake
add_library(silent_storm_stubs STATIC
    bink_stub.cpp
    fmod_stub.cpp
    starforce_stub.cpp  # 如 inventory 无 StarForce 则删本行 + 相应 .cpp/.h
)

target_include_directories(silent_storm_stubs PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_apply_compiler_warnings(silent_storm_stubs)
```

- [ ] **Step 7：顶层 CMakeLists.txt 引入 stubs**

修改 `$ROOT\port\CMakeLists.txt`，在 `add_executable` 之前加：

```cmake
add_subdirectory(src/stubs)
```

在 `add_executable` 之后加：

```cmake
target_link_libraries(silent_storm PRIVATE silent_storm_stubs)
```

- [ ] **Step 8：build 验证**

```powershell
cd C:\Users\Haohan\Documents\silent-storm\port
cmake --build --preset msvc-debug
```

期待：编译 4 个 .cpp（3 个 stub + main_stub）成功，链接 silent_storm.exe 成功。

- [ ] **Step 9：commit**

```powershell
git add src\stubs\ CMakeLists.txt
git commit -m "stubs: Bink, FMOD, StarForce no-op libraries"
```

---

## Task 5：stlport 策略决策与 SDK 路径暴露

**Files:**
- 创建：`$ROOT\port\cmake\LegacyDeps.cmake`
- 修改：`$ROOT\port\CMakeLists.txt`

- [ ] **Step 1：根据 Task 2 Step 5 的发现决定路线**

如果 inventory 报告 `#include <stl/...>` 出现 < 20 次 → 走「迁移到 modern std」：
- 写一个 sed 脚本把这些 include 改成标准头（`<stl/algorithm>` → `<algorithm>` 等）
- 这部分作为 Task 6 的 compile-fix 子任务做，先跳到 Step 3

如果 ≥ 20 次 → 走「vendor 老 stlport」：
- 把 `upstream/Soft/SDK/stlport` 整个 copy 到 `port/third_party/stlport/`
- 在 LegacyDeps.cmake 里定义 `INTERFACE` 库 `legacy::stlport`，include path 指向 vendored copy
- 实施 Step 2

- [ ] **Step 2：（仅 vendor 路线）LegacyDeps.cmake**

文件 `$ROOT\port\cmake\LegacyDeps.cmake`：

```cmake
# Vendored legacy STL replacement from the 2003 codebase.
# Modern std:: is preferred; this exists only if migration would touch too many files.

add_library(legacy_stlport INTERFACE)
target_include_directories(legacy_stlport INTERFACE
    ${CMAKE_SOURCE_DIR}/third_party/stlport/stlport
)
add_library(legacy::stlport ALIAS legacy_stlport)
```

并在顶层 CMakeLists.txt `include(CompilerWarnings)` 之后加 `include(LegacyDeps)`。

- [ ] **Step 3：暴露 DirectX 8.1 SDK header（如果 inventory 显示原代码大量用）**

DirectX 8 SDK header 现在不容易拿，但 d3d8.h / dinput8.h 现代 Windows SDK 还在。先试用现代 SDK 编译，如果碰到 `D3DX*` 系列（D3DX 已废弃）大量缺失，再考虑 vendor 老 D3DX。

这一步在本 task 是探测性的——只验证 `#include <d3d8.h>` 在 modern SDK 下能找到。

```powershell
cd C:\Users\Haohan\Documents\silent-storm\port
$probe = @"
#include <d3d8.h>
#include <dinput.h>
int main() { return 0; }
"@
$probe | Out-File -Encoding ascii build\probe.cpp
cl /nologo /EHsc /c build\probe.cpp /Fobuild\probe.obj
```

期待：编译通过。如果 `d3d8.h` 缺失说明 Windows SDK 太旧或没装 DirectX 组件——记录到 inventory.md，后续要解决。

- [ ] **Step 4：commit**

```powershell
git add cmake\LegacyDeps.cmake CMakeLists.txt third_party\
git commit -m "build: stlport vendor strategy + DX8 header probe"
```

---

## Task 6：Original source 接入（首批 + 编译错误循环）

**Files:**
- 创建：`$ROOT\port\src\game\CMakeLists.txt`
- 修改：`$ROOT\port\CMakeLists.txt`

这一 task 是 Phase 0 最大的不确定块。结构是：写一个 CMakeLists 把 `upstream/Soft/` 下所有 `.cpp/.c` 都吸进来，配 include path，然后**迭代编译 → 修错 → 再编译**，直到全部编过为止。每个 compile error 是一个小子任务。

- [ ] **Step 1：game/CMakeLists.txt 吸入全部源**

文件 `$ROOT\port\src\game\CMakeLists.txt`：

```cmake
# Wrap the original 2003 source tree as a static library.
# upstream/Soft is treated as read-only vendored code.

set(SOFT_ROOT ${CMAKE_SOURCE_DIR}/../upstream/Soft)

file(GLOB_RECURSE GAME_SOURCES
    ${SOFT_ROOT}/Andy/*.cpp
    ${SOFT_ROOT}/Andy/*.c
    ${SOFT_ROOT}/Bandures/*.cpp
    ${SOFT_ROOT}/Bandures/*.c
    ${SOFT_ROOT}/Monster/*.cpp
    ${SOFT_ROOT}/Monster/*.c
    ${SOFT_ROOT}/Serialize7/*.cpp
    ${SOFT_ROOT}/Serialize7/*.c
)

# 排除已知不要的（按 inventory 调整）
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*/MoveMe/.*")

add_library(silent_storm_game STATIC ${GAME_SOURCES})

target_include_directories(silent_storm_game PUBLIC
    ${SOFT_ROOT}
    ${SOFT_ROOT}/Andy
    ${SOFT_ROOT}/Bandures
    ${SOFT_ROOT}/Monster
    ${SOFT_ROOT}/Serialize7
)

# 链 stub 库提供的 Bink/FMOD/StarForce 符号
target_link_libraries(silent_storm_game PUBLIC silent_storm_stubs)

# Legacy code: 关闭所有警告即可（不是我们写的，不去清噪音）
if(MSVC)
    target_compile_options(silent_storm_game PRIVATE /W0 /wd4828)
endif()

target_compile_definitions(silent_storm_game PUBLIC
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _USE_MATH_DEFINES
)
```

> 上面的目录列表按 Task 2 inventory 实际目录调。如果有 Soft/Common 之类，加进来。

- [ ] **Step 2：顶层 CMakeLists 接入 game 库**

修改 `$ROOT\port\CMakeLists.txt`：

```cmake
add_subdirectory(src/stubs)
add_subdirectory(src/game)    # 新增

add_executable(silent_storm WIN32 src/main_stub.cpp)
target_apply_compiler_warnings(silent_storm)
target_link_libraries(silent_storm PRIVATE silent_storm_stubs silent_storm_game)
```

- [ ] **Step 3：第一次全量构建（预计大量失败）**

```powershell
cd C:\Users\Haohan\Documents\silent-storm\port
cmake --preset msvc-debug 2>&1 | Tee-Object -FilePath build\configure.log
cmake --build --preset msvc-debug 2>&1 | Tee-Object -FilePath build\build.log
```

把 `build\build.log` 里所有错误按类别聚合（写到 inventory.md 的「Phase 0 build errors」小节）：
- C2065 undeclared identifier
- C2039 not a member of
- C4838 conversion requires narrowing
- C2440 cannot convert
- LNK2019 unresolved external
- 等等

---

## Task 6.5：REPLAN CHECKPOINT — 中途停下补 plan

> **触发条件**：Task 6 Step 3 完成后，build.log 里有 build error 聚合数据。
>
> **停下做什么**：
> 1. 把 Task 6 Step 3 的错误分布给用户看（Top 10 错误类型 + 出现次数）
> 2. 决策：哪些错误适合一次 sed 批量修（比如 `<stl/x>` → `<x>`）、哪些必须手工修（比如 union/enum 命名冲突）
> 3. 在本 plan 文件**追加** Task 7, 8, 9...每个 task 处理一类错误
> 4. 重新跑 plan 执行从 Task 7 开始
>
> **不能跳过这一步**。如果跳过直接乱修 build error，会无尽消耗时间没有进度感。

---

## Task 7-N：[由 Task 6.5 replan 决定]

placeholder — 等 Task 6.5 后追加。典型形态会是：

- Task 7：批量替换 `<stl/...>` 到现代 `<...>`（sed 脚本 + 验证）
- Task 8：消解 `min`/`max` 宏污染（加 `#define NOMINMAX`，全局替换 `min`/`max` → `(std::min)` / `(std::max)`）
- Task 9：修 `nullptr` / 0 / NULL 不兼容点
- Task 10：修 `for` 循环变量作用域（VS6 风格 → C++ 标准）
- Task 11：修缺失的 `#include <algorithm>` 等隐式依赖
- ...
- Task K：链接错误处理（缺符号 → 补 stub）
- Task K+1：最后一次全量 build 通过

---

## Task M（倒数第 3）：Boot smoke test

**Files:**
- 创建：`$ROOT\port\tests\smoke\test_boot.ps1`

- [ ] **Step 1：找到 main 入口并把 main_stub.cpp 换掉**

把 `src/main_stub.cpp` 里的 stub WinMain 删掉，让原代码里 inventory Task 2 Step 6 找到的真 WinMain 链接进来。

- [ ] **Step 2：rebuild + 第一次真启动**

```powershell
cd C:\Users\Haohan\Documents\silent-storm\port
cmake --build --preset msvc-debug
.\build\msvc-debug\bin\silent_storm.exe
```

期待两种之一：
- 控制台/stderr 出现 `[bink_stub] BinkOpen called -- ...` 然后 abort 弹窗 → **成功**
- 弹窗 `[fmod_stub] FSOUND_Init (silent fake)` 多条，然后跑到主菜单前别的崩 → 也是**进展**，记录到 inventory

如果直接闪退没任何 stub 日志 → 失败，进调试。

- [ ] **Step 3：smoke test 脚本**

文件 `$ROOT\port\tests\smoke\test_boot.ps1`：

```powershell
# Phase 0 smoke test: exe launches and reaches a stub call (Bink or FMOD).
$exe = "$PSScriptRoot\..\..\build\msvc-debug\bin\silent_storm.exe"
if (-not (Test-Path $exe)) { Write-Error "exe not built: $exe"; exit 2 }

$proc = Start-Process -FilePath $exe -PassThru -RedirectStandardError stderr.log -NoNewWindow
Start-Sleep -Seconds 10
if (-not $proc.HasExited) { $proc.Kill() | Out-Null }

$err = Get-Content stderr.log -Raw -ErrorAction SilentlyContinue
if ($err -match "\[bink_stub\]|\[fmod_stub\]") {
    Write-Host "PASS: reached a stub call." -ForegroundColor Green
    Remove-Item stderr.log
    exit 0
} else {
    Write-Host "FAIL: no stub call reached. stderr:" -ForegroundColor Red
    Write-Host $err
    exit 1
}
```

- [ ] **Step 4：commit**

```powershell
git add src\main_stub.cpp tests\
git commit -m "test: Phase 0 boot smoke test"
```

---

## Task M+1：GitHub Actions CI

**Files:**
- 创建：`$ROOT\port\.github\workflows\windows.yml`

- [ ] **Step 1：CI workflow**

文件 `$ROOT\port\.github\workflows\windows.yml`：

```yaml
name: windows-build

on:
  push:
    branches: [main]
  pull_request:

jobs:
  build:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v4
        with:
          path: port
          submodules: false

      - uses: actions/checkout@v4
        with:
          repository: TanghaohanSC/Silent-Storm
          path: upstream

      - name: Set up MSVC
        uses: ilammy/msvc-dev-cmd@v1
        with:
          arch: x64

      - name: Configure
        working-directory: port
        run: cmake --preset msvc-debug

      - name: Build
        working-directory: port
        run: cmake --build --preset msvc-debug

      - name: Verify exe exists
        working-directory: port
        run: |
          if (-not (Test-Path build\msvc-debug\bin\silent_storm.exe)) {
            Write-Error "exe missing"
            exit 1
          }
```

> **不在 CI 里跑 smoke test**：runner 上没有原版 `Complete/` 资源，exe 跑起来立刻 fail 在找资源那一步。资源依赖 + smoke test 留给本地开发。

- [ ] **Step 2：push 仓库到 GitHub 触发 CI**

> 阻塞动作：需要用户先创建 GitHub 仓库（建议名 `silent-storm-port`）并给 push 权限。**Task M+1 Step 2 要等用户确认仓库地址才能继续**。

```powershell
cd C:\Users\Haohan\Documents\silent-storm\port
git remote add origin <user-provided-url>
git push -u origin main
```

期待：Actions 标签页里看见 windows-build 跑绿。

- [ ] **Step 3：CI passing badge 加进 README**

修改 `$ROOT\port\README.md`，在标题下加：

```markdown
![windows-build](https://github.com/<owner>/<repo>/actions/workflows/windows.yml/badge.svg)
```

- [ ] **Step 4：commit + push**

```powershell
git add .github\workflows\windows.yml README.md
git commit -m "ci: Windows MSVC build on GitHub Actions"
git push
```

---

## Task M+2：Phase 0 收尾文档

**Files:**
- 修改：`$ROOT\port\README.md`
- 创建：`$ROOT\port\docs\phase-0-completion.md`

- [ ] **Step 1：完善 README（构建方法 + 当前限制）**

`$ROOT\port\README.md` 替换为：

```markdown
# Silent Storm Port

![windows-build](https://github.com/<owner>/<repo>/actions/workflows/windows.yml/badge.svg)

Open-source modernization of Nival Interactive's *Silent Storm* (2003),
based on the 2026 source release.

## Status

Phase 0 (CMake bootstrap) complete. The game compiles with modern MSVC and
launches, but immediately aborts at the first Bink video call — this is
expected. Phase 1 (renderer) is next.

See `docs/superpowers/specs/` for the v1 design and `docs/superpowers/plans/`
for per-phase implementation plans.

## Build (Windows)

Requires:
- Visual Studio 2022 17.10+ with C++ Desktop workload
- CMake 3.25+
- Ninja (ships with VS 2022)
- The upstream Silent-Storm source at `../upstream/`

From an x64 Native Tools terminal:

```powershell
cd port
cmake --preset msvc-debug
cmake --build --preset msvc-debug
```

To run, you also need the original game data in `../upstream/Complete/`.

## License

The original source is licensed under Nival's non-commercial release license
(community/education/research only). Our modifications carry the same terms.
See `LICENSE` in the upstream repo.
```

- [ ] **Step 2：Phase 0 completion 报告**

文件 `$ROOT\port\docs\phase-0-completion.md`：

```markdown
# Phase 0 Completion Report

**Date completed:** <fill>
**Hours spent:** <fill>

## Achieved
- Modern CMake build (3.25+ / MSVC 17.10+) produces silent_storm.exe
- All original Soft/ source compiles
- Bink / FMOD / StarForce replaced by stubs
- GitHub Actions CI green on windows-2022
- Exe launches and reaches Bink call before aborting (smoke test passes)

## Discovered (carries into Phase 1)
- <e.g. DX8 fixed-function call density per module>
- <e.g. unexpected dependencies on D3DX>
- <e.g. main loop structure>

## Decisions made along the way that affect later phases
- stlport: <migrated to std | vendored>
- DirectX SDK: <modern Windows SDK sufficed | needed D3DX vendor>
- <any other architectural surprise>

## Open issues
- <link to issues filed in repo>
```

- [ ] **Step 3：commit**

```powershell
git add README.md docs\phase-0-completion.md
git commit -m "docs: Phase 0 completion report"
git push
```

---

## Phase 0 验收

满足全部以下条件 = Phase 0 done：

1. `cmake --preset msvc-debug && cmake --build --preset msvc-debug` 在干净 clone 上一次通过
2. `silent_storm.exe` 启动后能跑到 `[bink_stub]` 或 `[fmod_stub]` 日志
3. GitHub Actions windows-build workflow 在 main 分支绿
4. `docs/inventory.md` 完整、 `docs/phase-0-completion.md` 已填
5. 无任何商业库（binkw32.lib / fmodvc.lib / starforce / DRM）出现在链接命令里

验收通过 → invoke `superpowers:brainstorming` for Phase 1（渲染层），然后 writing-plans。

---

## Self-review notes

- ✅ Spec 第 5 节 Phase 0 完全覆盖：CMake、stub、boot 验收
- ⚠️ Task 7-N 是 placeholder，但有显式 replan checkpoint（Task 6.5）说明何时填——这是 discovery-heavy 任务的必然结构，不是 plan failure
- ✅ 每个 Task 都有具体 file path、可执行命令、期待输出
- ⚠️ Bink/FMOD stub 的符号表是「典型推测」+「inventory 调整」组合，每个 stub task 都明确写了「按 inventory 增删」
- ✅ git commit 节奏密集（每 task 一个 commit）
- ⚠️ Task M+1 Step 2 阻塞在用户提供 GitHub 仓库地址，已显式标注阻塞动作
- ✅ Phase 0 不引入运行时第三方依赖（vcpkg manifest 是空的）——把外部依赖延后到 Phase 1，降低 Phase 0 失败面
