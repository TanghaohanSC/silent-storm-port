# Source Inventory (Phase 0)

Generated: 2026-05-11
Source baseline: upstream/Soft/Andy/Jan03/a5dll/

## Jan03 subprojects

| Subproject    | cpp/c | .h  | vcproj |
|---------------|-------|-----|--------|
| Main          | 271   | 321 | 1      |
| MapEdit       | 179   | 186 | 1      |
| OpenDynamix   | 23    | 29  | 1      |
| Script        | 20    | 26  | 1      |
| libpng        | 18    | 2   | 1      |
| zlib          | 15    | 11  | 1      |
| DBFormat      | 15    | 24  | 1      |
| Misc          | 9     | 17  | 1      |
| Image         | 8     | 11  | 1      |
| A5ExportModel | 8     | 12  | 1      |
| AIPathTest    | 6     | 9   | 0      |
| MemoryMngrDll | 6     | 5   | 1      |
| MemoryMngr    | 6     | 5   | 1      |
| ShaderCompiler| 4     | 5   | 1      |
| FileIO        | 4     | 6   | 1      |
| AItest        | 4     | 8   | 1      |
| Sound         | 3     | 5   | 1      |
| MiscDll       | 3     | 5   | 1      |
| FontGen       | 3     | 4   | 1      |
| Game          | 3     | 5   | 1      |
| Input         | 3     | 6   | 1      |
| GfxTest       | 3     | 4   | 1      |
| DataImport    | 3     | 3   | 1      |
| TexMipStrip   | 2     | 3   | 1      |
| ADOImport     | 2     | 4   | 1      |
| PkgBuilder    | 2     | 3   | 1      |
| ADOFake       | 2     | 3   | 1      |
| FModSound     | 2     | 4   | 1      |
| TexConv       | 2     | 6   | 1      |
| LSConverter   | 2     | 3   | 1      |
| Scintilla     | 1     | 31  | 1      |

**Total (Jan03/a5dll): 639 .cpp/.c + 776 .h across 31 subprojects (30 have vcproj; AIPathTest has none)**

### Subproject classification

- **Main DLL (game engine core)**: `Main` — 271 cpp + 321 h; this is the primary game logic DLL (`Main.vcproj`), not the exe entry point. Covers AI, world simulation, script bindings, save/load, UI, voxel destruction.
- **Main exe entry point**: `Game` — 3 cpp + 5 h; `Game/Main.cpp` contains `WinMain` and the main loop. Tiny wrapper that initialises subsystems and drives `NMainLoop::StepApp`.
- **Engine libs (port in v1)**:
  - `OpenDynamix` — physics (23 cpp, likely ODE or custom rigid-body)
  - `Script` — embedded Lua 4.x VM (modified; 20 cpp)
  - `FileIO` — custom streaming / package file I/O (4 cpp)
  - `DBFormat` — game.db binary format serialisation (15 cpp)
  - `Image` — texture loading (8 cpp)
  - `Misc` / `MiscDll` — string utilities, miscellaneous helpers
  - `MemoryMngr` / `MemoryMngrDll` — custom allocator
  - `Input` — DirectInput8 wrapper (3 cpp)
  - `FModSound` — FMOD 3.x sound wrapper (2 cpp)
  - `Sound` — MFC-based sound test app (3 cpp)
  - `GfxTest` — standalone DX8/DX9 graphics testbed (3 cpp); has its own `WinMain`
- **Tools (dropping in v1)**:
  - `MapEdit` — MFC-based level editor (179 cpp, MFC + DirectX + LifeStudioHeadAPI)
  - `AItest` — AI path-finding test tool (4 cpp; console `main`)
  - `AIPathTest` — AI path test variant (6 cpp; no vcproj)
  - `ShaderCompiler` — standalone HLSL compiler utility (4 cpp)
  - `FontGen` — bitmap font generator (3 cpp)
  - `TexMipStrip` / `TexConv` — texture conversion utilities
  - `PkgBuilder` — game data package builder (2 cpp)
  - `LSConverter` — LifeStudio Head model converter (2 cpp)
  - `DataImport` / `ADOImport` — content pipeline tools that import from Access/OLEDB into game.db
  - `A5ExportModel` — Maya 4.0 exporter plug-in (8 cpp; requires `C:\AW\Maya4.0\include`)
- **Vendored 3rd-party**:
  - `zlib` — zlib 1.x (15 cpp)
  - `libpng` — libpng (18 cpp)
  - `Scintilla` — Scintilla text editor component used by MapEdit (1 cpp, 31 h)
  - `ADOFake` — stub/fake ADO COM layer (2 cpp)

---

## Bink symbols referenced (Phase 3 will replace)

**No Bink API calls found in Jan03/a5dll source.** The grep `\bBink[A-Z][a-zA-Z]*\b` over all .cpp/.c/.h returned zero matches.

`binkw32.lib` is also absent from all vcproj `AdditionalDependencies` entries. The spec assumption about Bink Video is **incorrect for this codebase** — video playback is either absent or handled by a different mechanism not present in the open-source drop. No Phase 3 Bink stub is needed.

---

## FMOD symbols referenced (Phase 2 will replace)

| Symbol | Call sites |
|--------|------------|
| FSOUND_SAMPLE | 9 |
| FSOUND_StopSound | 7 |
| FSOUND_SetSpeakerMode | 6 |
| FSOUND_IsPlaying | 5 |
| FSOUND_Stream_SetTime | 4 |
| FSOUND_SetVolume | 4 |
| FSOUND_LOOP_OFF | 4 |
| FSOUND_SetPaused | 3 |
| FSOUND_OUTPUT_DSOUND | 3 |
| FSOUND_LOOP_NORMAL | 3 |
| FSOUND_STREAM | 3 |
| FSOUND_Sample_SetMode | 3 |
| FSOUND_Stream_Play | 3 |
| FSOUND_FREE | 3 |
| FSOUND_GetCurrentPosition | 2 |
| FSOUND_Sample_Load | 2 |
| FSOUND_SetOutput | 2 |
| FSOUND_SetPan | 2 |
| FSOUND_UNMANAGED | 2 |
| FSOUND_PlaySoundEx | 2 |
| FSOUND_Stream_Stop | 2 |
| FSOUND_Stream_SetSynchCallback | 2 |
| FSOUND_Stream_SetPosition | 2 |
| FSOUND_MIXER_QUALITY_FPU | 2 |
| FSOUND_Stream_Close | 2 |
| FSOUND_MIXER_BLENDMODE | 2 |
| FSOUND_LOADMEMORY | 2 |
| FSOUND_STEREOPAN | 2 |
| FSOUND_GetLoopMode | 2 |
| FSOUND_Stream_GetTime | 1 |
| FSOUND_SetVolumeAbsolute | 1 |
| FSOUND_SetDriver | 1 |
| FSOUND_SPEAKERMODE_HEADPHONES | 1 |
| FSOUND_SetSFXMasterVolume | 1 |
| FSOUND_SPEAKERMODE_MONO | 1 |
| FSOUND_SPEAKERMODE_QUAD | 1 |
| FSOUND_SPEAKERMODE_STEREO | 1 |
| FSOUND_SPEAKERMODE_SURROUND | 1 |
| FSOUND_SetHWND | 1 |
| FSOUND_Stream_OpenFile | 1 |
| FSOUND_SPEAKERMODE_DOLBYDIGITAL | 1 |
| FMOD_ErrorString | 1 |
| FSOUND_Sample_SetDefaults | 1 |
| FMOD_VERSION | 1 |
| FSOUND_CAPS_EAX | 1 |
| FSOUND_CAPS_GEOMETRY_OCCLUSIONS | 1 |
| FSOUND_CAPS_GEOMETRY_REFLECTIONS | 1 |
| FSOUND_CAPS_HARDWARE | 1 |
| FSOUND_Close | 1 |
| FSOUND_GetChannelsPlaying | 1 |
| FSOUND_GetDriverCaps | 1 |
| FSOUND_GetDriverName | 1 |
| FSOUND_GetError | 1 |
| FSOUND_GetMaxChannels | 1 |
| FSOUND_Sample_SetMinMaxDistance | 1 |
| FSOUND_GetMixer | 1 |
| FSOUND_GetNumHardwareChannels | 1 |
| FSOUND_GetPriority | 1 |
| FSOUND_GetVersion | 1 |
| FSOUND_GetVolume | 1 |
| FSOUND_Init | 1 |
| FSOUND_OUTPUT_NOSOUND | 1 |
| FSOUND_OUTPUT_WINMM | 1 |
| FSOUND_OUTPUTTYPES | 1 |
| FSOUND_PlaySound | 1 |
| FSOUND_Sample_Free | 1 |
| FSOUND_Sample_GetLength | 1 |
| FSOUND_GetNumDrivers | 1 |
| FSOUND_Update | 1 |

**Total unique symbols: 67.** 60 of 60 call-site occurrences are in `FModSound/FMSound.cpp`; 1 occurrence (of `FSOUND_GetVersion`, called as `FSOUND_GetVersion() < FMOD_VERSION`) is in `Sound/SoundDlg.cpp` (a test harness, not shipped). No FMOD calls in `Main`, `Game`, or any other production subproject — callers go through the `NFMSound` namespace abstraction layer defined in `FModSound/FMsound.h`.

FMOD usage isolation: **FModSound.vcproj only** (production code). Sound.vcproj references the wrapper header, not FMOD directly.

FModSound.vcproj path: `upstream/Soft/Andy/Jan03/a5dll/FModSound/FModSound.vcproj`

FMOD API version in use: **FMOD 3.x (pre-FMOD Ex)** — `FSOUND_*` / `FMUSIC_*` C API (not the FMOD Studio/Ex C++ API). Phase 2 replacement target: miniaudio or FMOD 5 (FMOD Ex compatible wrapper).

---

## DirectPlay / DirectInput / StarForce

- **DirectPlay**: none — no `DirectPlay` or `IDirectPlay` symbols found anywhere in Jan03/a5dll source.
- **DirectInput**: present in `Input/Input.cpp` (DirectInput8 only — `IDirectInput8`, `IDirectInputDevice8`, `DirectInput8Create`). Also referenced by string in error messages in `Game/Main.cpp`, `GfxTest/Main.cpp`, and `MapEdit/GameView.cpp`. All DI8 usage is isolated to the `Input` subproject.
  - Files: `Input/Input.cpp` (all actual API calls), `Game/Main.cpp` (error string only), `GfxTest/Main.cpp` (error string only), `MapEdit/GameView.cpp` (error string only)
- **StarForce / DRM**: none — no `protect.h`, `starforce`, `sfini`, or `sfcheck` identifiers in source. The pattern `sfini` false-matched `IsFinished` — confirmed no StarForce SDK present.
- **binkw32.lib in vcproj LinkAdditionalDependencies**: none — `binkw32` absent from all vcproj files.
- **fmodvc.lib in vcproj LinkAdditionalDependencies**: present in `FModSound/FModSound.vcproj` (primary), `Sound/Sound.vcproj`, `Game/Game.vcproj`, `MapEdit/MapEdit.vcproj` (tools).

### Unexpected dependency: LifeStudioHeadAPI

`lifeStudioHeadAPI.lib` appears in `Game/Game.vcproj` and `MapEdit/MapEdit.vcproj` link dependencies. The `LSConverter` tool (a content pipeline converter) uses the LifeStudio Head API SDK for character animation import. `Main/LSHead.h` includes `<LifeStudioHeadAPI.h>`, suggesting the runtime also references character animation data from this SDK. This is an **undocumented 3rd-party dependency** not mentioned in the spec — the SDK is proprietary and its headers are not in the repo. Phase 4 / animation system work will need to account for this.

---

## SQL / DB API usage (Phase 3.5 will migrate to SQLite)

### ODBC API (SQLConnect, SQLExecDirect, etc.)

No ODBC C API function calls (`SQLConnect`, `SQLExecDirect`, `SQLAllocHandle`, `SQLBindCol`, `SQLFetch`) found in .cpp/.c/.h source files. The `odbc32.lib` / `odbccp32.lib` appears in vcproj link dependencies for many subprojects (Game, GfxTest, PkgBuilder, DataImport, FontGen, ShaderCompiler) but no direct ODBC C API call sites exist in source — these are likely transitive link requirements from ADO/OLE DB.

- ODBC API call sites: **0**

### MySQL C API

No `mysql_query`, `mysql_real_connect`, `mysql_init`, or `mysql_fetch` calls found anywhere in source.

- MySQL C API call sites: **0**

### ADO COM (_ConnectionPtr, _RecordsetPtr, _CommandPtr)

ADO is used in tool subprojects only (content pipeline), not in the shipped game binary:

| File | _ConnectionPtr | _RecordsetPtr | _CommandPtr |
|------|---------------|---------------|-------------|
| ADOImport/BasicDB.cpp | 2 | 1 | 0 |
| DataImport/BasicDB.cpp | 2 | 1 | 0 |
| MapEdit/BasicDB.cpp | 2 | 1 | 0 |

- ADO COM call sites: **9** total (all in content-pipeline tools, not runtime game code)

### DB file path references

| Pattern | Location |
|---------|----------|
| `game.db` | `DataImport/DataImport.cpp:19` (output path `w:\Complete\game.db`), `Game/Main.cpp:33,38,39` (runtime load) |
| `.mdb` | `ADOImport/BasicDB.cpp:686,689`, `DataImport/BasicDB.cpp:686,689`, `MapEdit/BasicDB.cpp:686,689`, `MapEdit/db.cpp:20` (`W:\Data\game.mdb`) |
| `.mdf` | none |
| `A5GAME_Data` | none |

The runtime game reads a single binary `game.db` file (custom serialised format via `NDatabase::Serialize`). The `.mdb` references are all in content pipeline / import tools that read from a Microsoft Access database to generate game.db. **No SQL Server or MySQL is used at runtime.** The "SQL Server / MySQL at runtime" spec assumption is incorrect — only game.db (custom binary format, see `DBFormat` subproject) is used at runtime.

- Estimated migration scope: **small** — runtime DB interaction is a single file load via `NDatabase::Serialize`. The DBFormat subproject (15 cpp) defines the serialisation. No ODBC/MySQL at runtime to migrate; only the content pipeline tools (DataImport, ADOImport) use ADO against Access files.

---

## stlport usage

- Files with `#include <stl/_config.h>` (stlport configuration header): **28 files** — one `StdAfx.h` per subproject (every subproject uses stlport).
- vcproj injecting stlport via `AdditionalIncludeDirectories`: stlport is **not** listed in `AdditionalIncludeDirectories` in any vcproj. The include path for stlport must be set at the solution/IDE level or via a system-level VS .NET 2003 configuration, not per-project. Only `A5ExportModel.vcproj` has an `AdditionalIncludeDirectories` entry (pointing to Maya includes).
- **Recommendation**: **vendor in legacy stlport** (or port to modern `std::` as a separate task). With 28 files each pulling in `<stl/_config.h>` as the first include in every precompiled header, stlport is pervasive. The stlport vendored copy is at `upstream/Soft/SDK/stlport/`. For the initial port, carry the vendored stlport and configure CMake to inject it; plan a follow-up modernisation pass to replace with C++17/20 `std::`.

---

## Main entry

- File: `Game/Main.cpp`
- Signature: `int APIENTRY WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )`
- Main loop: **in this file** — `for(;;)` loop at line ~138, drives `NMainLoop::StepApp(bActive, bActive)` each tick, pumps `NWinFrame::PumpMessages()` and `NInput::PumpMessages(bActive)`. No separate game loop class.
- Subsystem init sequence: game.db load → `NWinFrame::InitApplication` → `NGfx::Init3D` (Direct3D8 noted; vcproj links d3d9.lib) → `NSound::InitSound` → `NInput::InitInput` → config load → main loop → shutdown.

Other entry points in tree:

| File | Entry |
|------|-------|
| `GfxTest/Main.cpp:538` | `WinMain` — standalone graphics testbed (tool, drop) |
| `DataImport/DataImport.cpp:9` | `main` — content pipeline import tool (drop) |
| `AItest/AItest.cpp:302` | `main` — AI test tool (drop) |
| `TexMipStrip/TexMipStrip.cpp:80` | `main` — texture utility (drop) |
| `FontGen/FontGen.cpp:378` | `main` — font generator (drop) |
| `PkgBuilder/PkgBuilder.cpp:6` | `main` — package builder (drop) |
| `TexConv/main.cpp:262` | `main` — texture converter (drop) |
| `LSConverter/LSConverter.cpp:586` | `main` — LifeStudio converter (drop) |
| `ShaderCompiler/ShaderCompiler.cpp:798` | `main` — shader compiler (drop) |
| `zlib/minigzip.c:270` | `main` — vendored zlib test (ignore) |
| `libpng/pngtest.c:1216` | `main` — vendored libpng test (ignore) |

---

## Lua

- `lua_*` / `luaL_*` references in source: **215 occurrences across 18 files**
- Lua source is **embedded in the `Script` subproject** — this is a modified Lua 4.x VM (files: `lapi.cpp`, `ldebug.cpp`, `ldo.cpp`, `lmem.cpp`, `lsaver.cpp`, `lstate.cpp`, `lobject.cpp`, `ltm.cpp`, `lua.h`, `lvm.cpp`, `Script.cpp`, `Script.h`). The VM has custom extensions (`lua_startThread`, `lua_newThread`, `lua_executeThreads` — cooperative multi-threading on top of Lua).
- Script binding usage in `Main`: `Main/scriptPosition.cpp`, `Main/scriptCommon.cpp`, `Main/scriptUnit.cpp` use Lua C API directly to bind game objects. `MapEdit/TextEditor.cpp` uses it for the in-editor script editor.
- `.lua` files in Data/: **0**
- `.lua` files in Complete/: **0**
- `.lua` files in Soft/Andy/Jan03/: **0**
- **Conclusion**: Lua is deeply integrated in source (custom-patched Lua 4.x VM, not 5.x). Scripts are compiled into binary bytecode and stored in game.db — no plain .lua files shipped. Phase 4 Lua work must account for: (a) the modified VM with thread extensions, (b) migration path to Lua 5.4 will require re-binding all `lua_*` call sites. Consider carrying the patched Lua 4 source as a vendored subproject initially.

---

## Bandures, Monster, Serialize7

### Bandures (8.9 MB, 0 cpp)

Contains a single file: `NVStatInstall_12.60.exe` (9.3 MB). This is an NVIDIA statistics/driver installer — a binary blob unrelated to game source. **Not relevant to the port.**

### Monster (1.2 MB, 35 cpp + 46 h across 3 subdirs)

An **earlier prototype / standalone demo** of the game engine predating the Jan03 codebase. Three subdirectories:
- `game/` — a standalone game prototype (35 cpp, includes `wmain.cpp`/`wmain.h` for a Windows message loop, `gfx.cpp` for DX8 rendering, `imission.cpp` for mission logic, `RPGMerc.cpp`/`RPGMission.cpp` for RPG character mechanics, `dg.cpp` for scene graph). Has a VS 6.0 `.dsp`/`.dsw` (not VS .NET 2003). The `readme.txt` is a task list covering export formats, interface, input, model loading, AI, and building destruction — essentially an early design document for what became Silent Storm.
- `A5ExportModel/` — earlier version of the Maya 4.0 exporter (also in Jan03/a5dll); VS 6.0 project.
- `temp/` — scratch project stub (2 files).

Monster is a historical snapshot only. No code to port; interesting as context for architecture decisions.

### Serialize7 (0.1 MB, 0 cpp)

Contains only Visual Studio project metadata: `Serialize7.sln`, `Serialize7.vbproj`, `Serialize7.vbproj.user`, `Serialize7.suo`, `Serialize7.vsmacros`. This is a **VB .NET macro project** — likely a VS automation script or code-generation tool. No C++ source. **Not relevant to the port.**

---

## Summary notes for downstream tasks

- **Task 4 (stub libraries)**: Bink stub not needed. FMOD 3.x stub needs 67 symbols, all callable through `NFMSound` abstraction — replacement surface is small. DirectInput8 stub needs IDirectInput8 / IDirectInputDevice8 interfaces (all in `Input/Input.cpp`).
- **Task 6 (CMake structure)**: Main DLL is the largest build target (271 cpp). Game.exe is a thin 3-cpp wrapper. Suggested CMake targets: `Main` (DLL), `Game` (EXE), `Script` (lib, carry Lua 4 modified), `FileIO` (lib), `DBFormat` (lib), `Image` (lib), `Misc` (lib), `Input` (lib), `FModSound` (lib, Phase 2 replace), `OpenDynamix` (lib), `MemoryMngr` (lib), vendored `zlib`/`libpng`/`Scintilla`.
- **LifeStudioHeadAPI**: undocumented 3rd-party dependency linked into Game.exe and MapEdit. Headers not in repo. Must stub or source an alternative for character animation import before Game target can link.
- **stlport**: pervasive (28/31 subprojects). Must be configured as a CMake include path injection to avoid changing every StdAfx.h.
