# Silent Storm 现代化与开源社区版 — 设计稿

**日期**：2026-05-11
**仓库**：https://github.com/TanghaohanSC/Silent-Storm
**状态**：v1 设计已拍板，待写实现 plan

---

## 1. 项目目标

把 2003 年 Nival Interactive 的 Silent Storm 源码（2026 年根据禁商用 license 开源）改造成可在现代 Windows 上运行、可被社区长期维护、可扩展到 Linux/macOS 的开源版本，参照 OpenMW 之于 Morrowind 的模式。

**v1 范围**：Windows 单机战役可玩，支持现代分辨率（4K / 超宽屏），修复影响通关的原版 bug。
**v2 范围**：Linux/macOS 移植、UI 重写、mod 工具。

---

## 2. 已锁定的范围决策

| 主题 | 决策 |
|---|---|
| 平台 | v1：Windows 11 only；v2：+ Linux + macOS |
| ~~Lua（已被下方"修正"行覆盖）~~ | ~~升级到 5.4，批量迁移原版脚本~~ |
| 多人联机 | **丢弃**（DirectPlay 死，不重写网络层） |
| 原版存档兼容 | **丢弃**，自定义新存档格式 |
| 关卡编辑器 / MOD 工具 | **丢弃**，只跑游戏本体 |
| StarForce DRM | **丢弃**，全部清除 |
| 渲染：原生 Vulkan vs 抽象层 | 走 bgfx 抽象层（Vulkan/D3D11/Metal/GL 多后端） |
| UI scale up 方案 | (a) 整数倍放大原 bitmap，HUD 重定位；UI 重写推 v2 |
| 运行时 DB（**2026-05-11 修正后再修正**）| inventory 表明 Jan03 **运行时不用 SQL**，game.db 是自定义二进制格式（`DBFormat` 子项目 15 cpp 写的序列化）。SQL/MySQL/ADO 只出现在内容流水线工具（DataImport/ADOImport，按 §2 全部丢弃）。**Phase 3.5 SQLite 迁移整个删除**；运行时 DB = 直接读 game.db |
| Lua（**2026-05-11 修正**）| inventory 表明 Jan03 内置**改造版 Lua 4**（含 `lua_startThread`/`lua_executeThreads` 协作线程扩展，215 调用点 18 文件），**没有 .lua 脚本文件**（脚本编进 game.db 字节码）。Phase 4 策略 = 把改造版 Lua 4 当 vendored 子项目保留 → 让游戏先跑通；移到 Lua 5.4 留 v2 |
| 源码基线（**2026-05-11 修正**）| `Soft/Andy/` 下只有 **Jan03** 是完整 VS .NET 2003 solution（`a5dll/A5.sln`，~30 子项目，639 .cpp + 776 .h）。EnglishGold/RussianGold/RussianPatch1 只有 Game.exe 二进制无源码；Oct02 是旧 snapshot（不到一半代码）；其余子目录基本空。**baseline = `Soft/Andy/Jan03/a5dll/`**，主入口 `Game/Main.cpp` |

---

## 3. 技术栈

### 3.1 构建系统
- **CMake 3.25+ + Ninja**，整个 `Soft/` 重写 CMakeLists
- 工程文件 `.sln/.vcproj` 全部弃用
- 三平台 toolchain：MSVC 17.10+ (Windows v1)、Clang 17+ (Linux v2)、Apple Clang (macOS v2)
- 包管理：vcpkg（与 MSVC 配合好；Linux/macOS 后续视情况切 Conan 或 system pkg）

### 3.2 平台层
- **SDL3**：窗口、输入、事件、音频设备
- 把原代码里所有 `WinMain` / `CreateWindow` / `WndProc` / `DirectInput` 调用全部迁到 SDL3
- v1 阶段只在 Windows 编译，但代码不写 `#ifdef _WIN32` 的硬编码 path

### 3.3 渲染层
- **bgfx + bimg + bx**（同一个组织的三件套）
- 后端：v1 用 D3D11 + Vulkan 双后端（用户可切），v2 加 Metal + OpenGL
- shader 工具：bgfx 自带 `shaderc`，从 GLSL 编到所有目标
- **核心工作 = DX8 fixed-function 翻译层**：
  - 把原代码的 `SetTextureStageState`、`SetRenderState`、`SetTransform` 等 fixed function 调用映射成 bgfx 的 uniform + shader program
  - 设计 5-8 个通用 shader：`diffuse_unlit`、`diffuse_lit`、`diffuse_lit_skinned`、`ui_textured`、`particle`、`shadow_caster`、`terrain`、`water`
  - 原 `IDirect3DDevice8` 方法签名保持兼容，内部转发到翻译层
- 任意分辨率支持：render target 用系统分辨率，FOV/aspect math 修复 4:3 假设
- 鼠标→世界坐标反投影矩阵按当前分辨率计算

### 3.4 音频
- **miniaudio**（单头文件 C 库）
- 内置 wav/mp3/vorbis/flac 解码，三平台后端自动选
- 3D 空间音效用 miniaudio 内置 API
- FMOD 调用点重新封装一层 `Audio::` 抽象接口，原 `FSOUND_*` 全部走新接口

### 3.5 视频（**2026-05-11 重新理解**）
**Jan03 源码完全没有 Bink API 调用**（inventory 已验证），但 game data 有 6 个 .bik（Credits/Fail/Intro/JoWood/Nival/Win）。Jan03 是 Bink 集成之前的 dev snapshot，shipped 二进制里的 Bink 代码不在开源 drop 里。

所以 Phase 3 的工作**不是替换**而是**新增**：
- **FFmpeg (libavcodec + libavformat)** 运行时解码 → bgfx 纹理 → 全屏 quad
- `.bik` → `.webm` (VP9) 一次性转码
- 找到游戏触发过场动画的位点（`Main/iMainMenu.cpp` 等），加播放调用
- 因为没原始 Bink 接口可参考，集成形式由我们设计：极简 `Video::Play(path)` 阻塞接口

### 3.6 脚本（**2026-05-11 重写**）
inventory 表明 Jan03 内置一个**改造版 Lua 4**（`Script/` 子项目：lapi/ldebug/ldo/lmem/lsaver/lstate/lobject/ltm/lua.h/lvm.cpp/Script.cpp/Script.h），加了 `lua_startThread`/`lua_newThread`/`lua_executeThreads` 协作线程扩展。Main 子项目 215 个 lua_* 调用在 18 个文件里（`scriptPosition.cpp`/`scriptCommon.cpp`/`scriptUnit.cpp` 等）。

**没有 .lua 脚本文件**：脚本是预编译字节码进 game.db，运行时由改造版 Lua 4 VM 加载。

v1 策略：**保留 vendored 改造版 Lua 4 不动**，让游戏先跑通。理由：
- 没 .lua 源文件可以批量迁移
- 215 调用点 + 协作线程扩展，移到 Lua 5.4 等于重写半个 VM
- 现代化收益小（没社区会写新 Lua 4 mod）

迁移到 Lua 5.4 推 **v2**。

### 3.7 序列化与存档
- 原 `Soft/Serialize7` 弃用
- 新存档格式：**flatbuffers**（schema 可演化，新版本兼容老存档）
- 资源文件序列化保持原格式（不动 `Complete/` 下的 .pak/.dat），只新写存档

### ~~3.7.5 运行时数据库（SQLite 迁移）~~ — **2026-05-11 整节删除**

inventory 已验证：Jan03 运行时不调用 SQL。`game.db` 是 DBFormat 子项目自定义二进制序列化，runtime 一次性 load + 解析。SQL/MySQL/ADO 的引用全在我们丢弃的内容流水线工具里（DataImport/ADOImport/MapEdit）。

新事实：v1 **不需要任何 DB 迁移**。`DBFormat` 子项目按现状 vendor 编译进去。

### 3.8 第三方库依赖清单（v1，**2026-05-11 更新**）

#### 新增的现代依赖
| 库 | 用途 | License | 接入方式 |
|---|---|---|---|
| SDL3 | 平台层（替代 DirectInput8/WinAPI） | Zlib | vcpkg |
| bgfx + bimg + bx | 渲染（替代 D3D8） | BSD-2 | git submodule |
| miniaudio | 音频（替代 FMOD 3.x） | MIT-0 / Unlicense | 单头文件 vendor in |
| FFmpeg | 视频（新增，原 Jan03 没视频） | LGPL 2.1 | vcpkg（动态链接） |
| flatbuffers | 存档 | Apache 2.0 | vcpkg |
| stb_image | 截图保存 | MIT / Unlicense | 单头文件 vendor in |

#### Vendored 原版子项目（按现状编进 port）
| Vendored | 用途 | 出处 |
|---|---|---|
| 改造版 Lua 4 | 脚本运行时 | `upstream/Soft/Andy/Jan03/a5dll/Script/` |
| stlport (vc7) | STL 替代 | `upstream/Soft/SDK/stlport/` |
| zlib (旧版) | 压缩 | `upstream/Soft/Andy/Jan03/a5dll/zlib/` |
| libpng (旧版) | PNG 解码 | `upstream/Soft/Andy/Jan03/a5dll/libpng/` |
| DBFormat | game.db 二进制读写 | `upstream/Soft/Andy/Jan03/a5dll/DBFormat/` |
| MemoryMngr | 自定义内存分配器 | `upstream/Soft/Andy/Jan03/a5dll/MemoryMngr/` |
| OpenDynamix | 物理 | `upstream/Soft/Andy/Jan03/a5dll/OpenDynamix/` |

#### Stub（Phase 0 占位，后续 Phase 替换或永久 stub）
| 原库 | Stub 行为 | 后续 Phase |
|---|---|---|
| ~~Bink~~ | **不需要 stub**（源代码没调用） | Phase 3 新增视频播放 |
| FMOD 3.x | silent fake，67 个符号（inventory 已列） | Phase 2 → miniaudio |
| DirectInput8 | 永久 stub（SDL3 接管输入） | Phase 1 删除 |
| LifeStudioHeadAPI | **新发现**：proprietary SDK，头文件不在仓库；stub 返回空值，角色面部动画功能砍掉 | v1 永久 stub，v2 找替代或砍 |
| ~~StarForce~~ | **不需要 stub**（源代码没调用） | — |
| ~~DirectPlay~~ | **不需要 stub**（源代码没调用） | — |

License 兼容性：原项目是禁商用、允许社区/教育/研究 license。所有上述依赖在非商用场景下都兼容。LGPL FFmpeg 必须动态链接，要在 README 注明。

---

## 4. 子系统边界与模块图（**2026-05-11 按 inventory 重画**）

```
                  ┌──────────────────────────────┐
                  │  Game.exe (Game/Main.cpp)    │  ← 主入口、message loop
                  │  thin wrapper, 3 .cpp        │
                  └──────────────┬───────────────┘
                                 │ links
                  ┌──────────────▼───────────────┐
                  │  Main.dll  (Main/, 271 cpp)  │  ← 游戏逻辑核心
                  │  - AI / 战斗 / 任务 / UI     │
                  │  - script bindings           │
                  │  - LSHead.h includes         │
                  └─┬────┬────┬────┬────┬────┬───┘
                    │    │    │    │    │    │
              ┌─────┘    │    │    │    │    └──────┐
              │     ┌────┘    │    │    └────┐      │
   ┌──────────▼──┐ ┌▼──────┐ ┌▼────┐ ┌▼─────▼┐ ┌──▼──────────┐
   │ Script (lib)│ │FileIO │ │Image│ │ Misc  │ │MemoryMngrDll│
   │ Lua 4 mod   │ │       │ │     │ │MiscDll│ │             │
   └─────────────┘ └───────┘ └─────┘ └───────┘ └─────────────┘
   ┌─────────────┐ ┌───────┐ ┌────────────────┐ ┌────────────┐
   │ DBFormat    │ │Input  │ │ FModSound      │ │OpenDynamix │
   │ game.db RW  │ │ DI8→  │ │ Phase 2 替换   │ │ 物理       │
   │             │ │ SDL3  │ │                │ │            │
   └─────────────┘ └───────┘ └────────────────┘ └────────────┘

   Vendored 3rd-party (按现状编进):
   ┌─────────────┐ ┌───────────┐ ┌────────────┐
   │ zlib 1.x    │ │ libpng    │ │ stlport vc7│
   └─────────────┘ └───────────┘ └────────────┘

   v1 抽象层（封装第三方现代库）:
   ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐
   │ Platform │  │ Renderer │  │  Audio   │  │  Video   │
   │  (SDL3)  │  │  (bgfx)  │  │(miniaud) │  │ (ffmpeg) │
   └──────────┘  └──────────┘  └──────────┘  └──────────┘

   Stubs（Phase 0 占位）:
   ┌──────────────────┐  ┌──────────────────────┐
   │ DirectInput8 stub│  │ LifeStudioHeadAPI    │
   │ → SDL3 接管输入  │  │ stub（永久砍掉功能） │
   └──────────────────┘  └──────────────────────┘

   Tools subprojects（**v1 全部 drop**，不进 CMake 编译）:
   MapEdit, AItest, AIPathTest, ShaderCompiler, FontGen,
   TexMipStrip, TexConv, PkgBuilder, LSConverter,
   DataImport, ADOImport, A5ExportModel, GfxTest, Sound
```

每个新写的子系统（Platform/Renderer/Audio/Video）都有清晰 C++ 接口（头 < 200 行），Main DLL 通过这些接口而不是直接 include bgfx/SDL/miniaudio/ffmpeg。

---

## 5. 阶段路线（v1）

每个 phase 必须有可观察的验收点，未通过不进下一 phase。

| Phase | 目标 | 验收 | 估时 |
|---|---|---|---|
| **0** | CMake + 现代 MSVC 能编 Jan03 源码 | `cmake --build` 成功；Game.exe 启动到第一个缺资源/不支持调用前崩溃 | 1-2 周 |
| **1** | SDL3 接管平台层（替 DirectInput8）；bgfx 接管渲染（DX8→bgfx 翻译层）；任意分辨率 + FOV 修正 | 主菜单在 1920×1080、2560×1440、3840×2160 都正常显示，无拉伸 | 6-8 周 |
| **1.5** | UI 整数倍 scale + HUD 重定位 | 4K 屏上 UI 元素可读，HUD 不溢出 | 2 周 |
| **2** | miniaudio 替换 FMOD（67 个符号） | 主菜单 BGM + 至少 5 个按钮音效正常 | 2-3 周 |
| **3** | FFmpeg **新增**视频播放（原代码没视频） + `.bik` 离线转 `.webm` | 开场 CG 能播 | 1-2 周 |
| ~~**3.5**~~ | ~~SQLite 迁移~~ | **删除**：运行时不用 SQL | — |
| **4** | ~~Lua 4 → 5.4 迁移~~（推 v2）→ Phase 4 改为 **LifeStudioHeadAPI 替代或永久 stub 决策**，以及 stlport → modern std 一次性迁移评估 | 决策文档 + 关键替代验证 | 2-3 周 |
| **7** | flatbuffers 存档系统 | 任务中存档、退到主菜单、读档、状态完全恢复 | 2-3 周 |
| **8** | bug 修复滚动 | 按社区 issue 优先级处理 | 持续 |

**总估时**：5-7 个月（兼职 10-15 小时/周；删了 3.5 + 简化了 4，节省 5-7 周）。

v2（推后）：Phase 5 Linux 移植、Phase 6 macOS 移植、UI 重写（RmlUi 或类似）、mod 工具复活。

---

## 6. 仓库与工作目录布局（**2026-05-11 修正：docs 移进 port/**）

**本地工作目录**：`C:\Users\Haohan\Documents\silent-storm\`

```
silent-storm/                    # 非 git repo，只是容器目录
├── upstream/                    # git clone 自 GitHub 的原仓库
│   ├── Soft/Andy/Jan03/a5dll/  # 唯一现实源代码基线
│   ├── Complete/                # 3.6 GB 游戏资源（88K files）
│   ├── Versions/Current/        # binary release + .bik / .sfap0 资源
│   └── ...
└── port/                        # 我们的现代化代码（独立 git repo → GH: TanghaohanSC/silent-storm-port）
    ├── CMakeLists.txt
    ├── CMakePresets.json
    ├── vcpkg.json
    ├── cmake/
    │   ├── CompilerWarnings.cmake
    │   └── VendoredJan03.cmake      # 把 Jan03 子项目挂成 CMake targets
    ├── src/
    │   ├── stubs/                   # 旧 API stub
    │   │   ├── fmod_stub.{h,cpp}    # 67 个 FMOD 3.x 符号
    │   │   ├── dinput8_stub.{h,cpp} # DirectInput8（Phase 1 删）
    │   │   └── lifestudio_stub.{h,cpp} # LifeStudioHeadAPI 永久 stub
    │   ├── platform/                # SDL3 wrapper（Phase 1）
    │   ├── renderer/                # bgfx + DX8 翻译层（Phase 1）
    │   ├── audio/                   # miniaudio wrapper（Phase 2）
    │   └── video/                   # ffmpeg wrapper（Phase 3）
    ├── tools/
    │   └── bik2webm/                # 一次性视频转码
    ├── third_party/                 # vendored 头文件库（miniaudio/stb）
    ├── docs/
    │   ├── inventory.md             # Task 2 产出
    │   └── superpowers/
    │       ├── specs/...
    │       └── plans/...
    └── tests/
        └── smoke/                   # boot smoke test
```

`port/` 是独立 git repo，最终 push 到 `github.com/TanghaohanSC/silent-storm-port`。原 fork 保持只读对照。

---

## 7. 风险与未决

### 已识别风险
1. **DX8 fixed function 翻译层是最大不确定性**。如果原代码用了某些罕见的 render state 组合，bgfx 抽象可能映射不上，得 case-by-case 写 shader 分支。Phase 1 实际可能比估时长 50%。
2. ~~Lua 4 → 5.4 自动转换器漏 case~~ — 取消，按 §3.6 决策保留 vendored Lua 4 不迁移。
3. **原代码可能有未文档化的 Windows-specific 假设**（路径分隔符、字符编码、字节序）。Phase 0 走通可能比想象的难。
4. **资源文件加密/校验**。原 `Complete/` 里的 .pak 可能有 StarForce 相关校验，删 DRM 时不能误伤资源加载逻辑。
5. **LifeStudioHeadAPI (2026-05-11 新发现)**: proprietary SDK 头文件不在仓库，所有引用 `<LifeStudioHeadAPI.h>` 的 .cpp 编不过。stub 让链接过但角色面部动画功能丢失。如果 Phase 1 发现角色脸部播放是关键体验，得找替代或重写。

### 未决问题

**阻塞 Phase 0 implementation plan，必须先答**：
- 原版游戏数据（`Complete/` 目录）是否完整可用？需要 clone 仓库下来确认大小和完整性。如果 Complete/ 没在 git 里，得另找渠道。
- 是否需要单元测试基础设施（GoogleTest）从 Phase 0 就铺好？v1 没有测试是一个独立风险。

**不阻塞，但影响协作节奏**：
- 用户的 C++ 经验和每周可投入时间。影响我是「多解释少自动化」还是「多自动化少干预」。

---

## 10. 实现 plan 拆分

**这份 spec 是整个 v1 的总设计，不直接对应一份 implementation plan**。每个 Phase 各自写一份 plan：

- `plans/phase-0-cmake-bootstrap.md`
- `plans/phase-1-renderer-bgfx.md`
- `plans/phase-1.5-ui-scaleup.md`
- `plans/phase-2-audio-miniaudio.md`
- `plans/phase-3-video-ffmpeg.md`
- `plans/phase-4-lua54-migration.md`
- `plans/phase-7-flatbuffers-save.md`

下一步只写 **Phase 0 的 implementation plan**。Phase 0 完成后再写 Phase 1 的 plan，依次类推。这样每次只规划一个可控范围，前一个 phase 的真实发现可以反馈到下一个 plan。

---

## 8. 不在 v1 范围

明确写出来避免范围漂移：

- ❌ Linux / macOS 支持
- ❌ 多人联机
- ❌ 原版存档读取
- ❌ 关卡编辑器
- ❌ MOD 工具
- ❌ UI 重写（RmlUi/HTML 风格矢量 UI）
- ❌ 控制器/手柄支持
- ❌ Steam Workshop 集成
- ❌ 中文/多语言界面（保留原版语言资源）
- ❌ 高清贴图/模型 mod 支持
- ❌ AI/平衡性调整

以上要么 v2 处理，要么交给社区。

---

## 9. 验收：v1 完成意味着什么

- Windows 11 上 `cmake --build && ./port/silent_storm.exe` 能跑起来
- 在 1920×1080 和 4K 分辨率下分别完整通关第一章战役（约 5-8 个任务）
- 主菜单 / 任务 brief / 战斗 / 存读档 / 退出 全流程无崩溃
- 所有原版商业库引用清零（Bink/FMOD/StarForce）
- GitHub Actions CI：Windows MSVC 编译通过 + 启动 smoke test
- README 写明依赖、构建方法、license 边界
