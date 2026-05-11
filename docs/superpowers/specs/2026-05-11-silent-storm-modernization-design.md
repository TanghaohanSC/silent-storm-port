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
| Lua | 升级到 5.4，批量迁移原版脚本 |
| 多人联机 | **丢弃**（DirectPlay 死，不重写网络层） |
| 原版存档兼容 | **丢弃**，自定义新存档格式 |
| 关卡编辑器 / MOD 工具 | **丢弃**，只跑游戏本体 |
| StarForce DRM | **丢弃**，全部清除 |
| 渲染：原生 Vulkan vs 抽象层 | 走 bgfx 抽象层（Vulkan/D3D11/Metal/GL 多后端） |
| UI scale up 方案 | (a) 整数倍放大原 bitmap，HUD 重定位；UI 重写推 v2 |
| 运行时 DB（**2026-05-11 补**）| 原版用 SQL Server 2000 + MySQL；**迁移到 SQLite**。原 .MDF/.MYD/ibdata 一次性导出为 .sqlite，SQL 调用点重写到 sqlite3 C API |
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

### 3.5 视频
- **FFmpeg (libavcodec + libavformat)** 运行时解码
- 解码输出 RGB → bgfx 纹理 → 全屏 quad
- **离线转码**：原 `.bik` 文件用脚本一次性转 WebM (VP9)，运行时只播 WebM
- 过场动画估计 10-20 个，转码 + 校对一周搞定

### 3.6 脚本
- **Lua 5.4 + sol3 绑定**
- `Soft/` 里原 Lua 4 C API 调用（`lua_dostring`、`lua_pushuserdata` 等）全部用 sol3 重写
- `Data/` 里的 `.lua` 文件用转换器批量迁移：
  - `nil` 关键字处理（Lua 4 用 nil 但语义略不同）
  - `tag method` → `metatable`
  - `getn(t)` → `#t`
  - `function name(...) ... end` 中 `arg` 隐式变量 → `...` 展开
  - `string.find` 行为差异
- 转换后手工修边界 case，估计 100-300 个脚本文件

### 3.7 序列化与存档
- 原 `Soft/Serialize7` 弃用
- 新存档格式：**flatbuffers**（schema 可演化，新版本兼容老存档）
- 资源文件序列化保持原格式（不动 `Complete/` 下的 .pak/.dat），只新写存档

### 3.7.5 运行时数据库（SQLite 迁移）
- 原版 `Data/A5GAME_Data.MDF`（SQL Server 2000）+ `Data/ibdata1` / `Data/masterserver/statistics.MYD`（MySQL）替换为 **SQLite 3**
- 一次性导出脚本（`tools/db2sqlite/`）：
  - 用旧 `SQL Server 2000 Desktop Engine`（仓库里 `Tools/MSDE/SQL2KDeskSP3.exe`）短暂启起来 → 通过 ODBC dump 表结构 + 数据 → 写入 `data.sqlite`
  - 用 MySQL 5.x 临时跑起来读 `ibdata1` 表 → 同样 dump → 同一个 `data.sqlite`
  - 输出单个 `.sqlite` 文件给游戏运行时
- 游戏运行时：链 **sqlite3 amalgamation**（单文件 C 库），调用点把原 ODBC/MySQL API 重写到 `sqlite3_exec` / `sqlite3_prepare_v2`
- SQL 方言差异处理：MS T-SQL/MySQL → SQLite 的不兼容点（`TOP N` → `LIMIT N`、`GETDATE()` → `datetime('now')`、`IDENTITY` → `AUTOINCREMENT` 等）由迁移脚本+少量手工修
- **注意范围**：仅迁移**游戏运行时数据库**。`masterserver/` 的 MySQL 是多人联机用的，按 §2 决策"多人联机丢弃"——masterserver 整块不迁移，不进 v1

### 3.8 第三方库依赖清单（v1）

| 库 | 用途 | License | 接入方式 |
|---|---|---|---|
| SDL3 | 平台层 | Zlib | vcpkg |
| bgfx + bimg + bx | 渲染 | BSD-2 | git submodule（bgfx 不在 vcpkg 主线） |
| miniaudio | 音频 | MIT-0 / Unlicense | 单头文件 vendor in |
| FFmpeg | 视频解码 | LGPL 2.1 | vcpkg（动态链接以满足 LGPL） |
| Lua 5.4 | 脚本 | MIT | vcpkg |
| sol3 | C++↔Lua 绑定 | MIT | 单头文件 vendor in |
| flatbuffers | 存档 | Apache 2.0 | vcpkg |
| stb_image | 截图保存 | MIT / Unlicense | 单头文件 vendor in |

License 兼容性：原项目是「禁商用、允许社区/教育/研究」的特殊 license。所有上述依赖在非商用场景下都兼容。LGPL FFmpeg 必须动态链接，要在 README 注明。

---

## 4. 子系统边界与模块图

```
┌─────────────────────────────────────────────────┐
│  Game Layer (原 Soft/Andy, Bandures, Monster)    │
│  - 任务/AI/战斗/物品系统                          │
│  - 已重命名目录结构，但内部逻辑保持              │
└──────────┬───────────────────┬──────────────────┘
           │                   │
   ┌───────▼────────┐   ┌──────▼────────┐
   │ Script (Lua5)  │   │ Engine API    │
   │ - sol3 binding │   │ - 渲染/音频/  │
   └────────────────┘   │   视频抽象层  │
                        └──┬─┬─┬─────┬──┘
                           │ │ │     │
              ┌────────────┘ │ │     └────────┐
              │              │ │              │
        ┌─────▼────┐  ┌──────▼─┐  ┌──▼────┐  ┌▼────────┐
        │ Renderer │  │ Audio  │  │ Video │  │ Platform│
        │ (bgfx)   │  │(minia.)│  │(ffmpg)│  │ (SDL3)  │
        └──────────┘  └────────┘  └───────┘  └─────────┘
```

每个子系统都有清晰的 C++ 接口（头文件少于 200 行），game layer 不允许直接 include bgfx/SDL/miniaudio/ffmpeg 的头。

---

## 5. 阶段路线（v1）

每个 phase 必须有可观察的验收点，未通过不进下一 phase。

| Phase | 目标 | 验收 | 估时 |
|---|---|---|---|
| **0** | CMake + 现代 MSVC 能编 EnglishGold 源码 | `cmake --build` 成功；Game.exe 启动到第一个 Bink/FMOD/SQL 调用前崩溃 | 1-2 周 |
| **1** | SDL3 接管平台层；bgfx 接管渲染；DX8→bgfx 翻译层；任意分辨率 + FOV 修正 | 主菜单在 1920×1080、2560×1440、3840×2160 都正常显示，无拉伸 | 6-8 周 |
| **1.5** | UI 整数倍 scale + HUD 重定位 | 4K 屏上 UI 元素可读，HUD 不溢出 | 2 周 |
| **2** | miniaudio 替换 FMOD | 主菜单 BGM + 至少 5 个按钮音效正常 | 2-3 周 |
| **3** | FFmpeg 替换 Bink + 原 `.bik` 离线转 `.webm` | 开场 CG + 至少一个任务 brief CG 能播 | 1-2 周 |
| **3.5** | SQLite 迁移：dump 工具 + 运行时改 SQL 调用 | 游戏能从 `data.sqlite` 加载装备/角色/关卡数据 | 3-4 周 |
| **4** | Lua 4 → 5.4 迁移 + sol3 绑定（如有 Lua 脚本） | 能从主菜单加载第一个任务到可操作状态（角色能移动、能开火） | 4-6 周 |
| **7** | flatbuffers 存档系统 | 任务中存档、退到主菜单、读档、状态完全恢复 | 2-3 周 |
| **8** | bug 修复滚动 | 按社区 issue 优先级处理 | 持续 |

**总估时**：6-8 个月（兼职 10-15 小时/周；DB 迁移加了 3-4 周）。

v2（推后）：Phase 5 Linux 移植、Phase 6 macOS 移植、UI 重写（RmlUi 或类似）、mod 工具复活。

---

## 6. 仓库与工作目录布局

**本地工作目录**：`C:\Users\Haohan\Documents\silent-storm\`

```
silent-storm/
├── docs/superpowers/specs/      # 设计文档
├── upstream/                     # git clone 自 GitHub 的原仓库
│   ├── Soft/                     # 原代码
│   ├── Complete/                 # 原资源
│   └── ...
├── port/                         # 我们的现代化代码（新仓库）
│   ├── CMakeLists.txt
│   ├── cmake/
│   ├── src/
│   │   ├── platform/             # SDL3 wrapper
│   │   ├── renderer/             # bgfx + DX8 翻译层
│   │   ├── audio/                # miniaudio wrapper
│   │   ├── video/                # ffmpeg wrapper
│   │   ├── script/               # sol3 + Lua 5.4
│   │   ├── save/                 # flatbuffers
│   │   └── game/                 # 从 upstream/Soft 移植过来的游戏逻辑
│   ├── tools/
│   │   ├── lua4to5/              # 脚本转换器
│   │   └── bik2webm/             # 视频转码脚本
│   ├── third_party/              # vendored 头文件库（miniaudio/sol3/stb）
│   └── tests/
└── scripts/                      # 一次性辅助脚本
```

`port/` 后期会独立成 GitHub 仓库（保持原 fork 干净，方便对照）。

---

## 7. 风险与未决

### 已识别风险
1. **DX8 fixed function 翻译层是最大不确定性**。如果原代码用了某些罕见的 render state 组合，bgfx 抽象可能映射不上，得 case-by-case 写 shader 分支。Phase 1 实际可能比估时长 50%。
2. **Lua 4 → 5.4 自动转换器漏 case**。某些 Lua 4 习惯写法（比如 `tag method`、`%括号匹配模式`）很难自动转，得手工修。Phase 4 风险偏大。
3. **原代码可能有未文档化的 Windows-specific 假设**（路径分隔符、字符编码、字节序）。Phase 0 走通可能比想象的难。
4. **资源文件加密/校验**。原 `Complete/` 里的 .pak 可能有 StarForce 相关校验，删 DRM 时不能误伤资源加载逻辑。

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
