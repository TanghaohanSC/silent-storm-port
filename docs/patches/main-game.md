# Main + Game subprojects port patches

This file documents the upstream and port-side changes made to compile the
Main DLL (270 .cpp) and Game.exe (3 .cpp) under modern MSVC (VC++ 18,
C++14 mode, x86 target). The build target is `silent_storm.exe`.

## Strategy

- **Main**: built as a static OBJECT library, linked into `silent_storm.exe`
  alongside all other engine libs. Skipped `Main.def` exports — we link
  everything statically rather than building a DLL.
- **Game**: 3 .cpp files (Main.cpp, WinFrame.cpp, StdAfx.cpp) wired directly
  into the `silent_storm` exe target. WinMain replaces port/src/main_stub.cpp
  (deleted).
- **ADOImport**: stubbed out as `port/src/stubs/ado_stub.cpp` — original
  uses `#import "msado15.dll"` (Microsoft Access COM) which is dev-time only.
  Stub provides runtime NDatabase symbols (Serialize, AddTable, GetTable,
  bIsDatabaseLoading, etc.) so game.db can be loaded.
- **MemoryMngrDll / MiscDll**: MiscDll added; MemoryMngrDll skipped.
  Replaced `FastDumbAlloc`/`FastDumbFree` from MemoryMngrDll with stdlib
  `malloc`/`free` in MemoryMngr/DumbPow2Alloc.cpp.

## Bulk refactors (scripted)

| Pattern | Count | Fix |
|---|---|---|
| `if (CDynamicCast<T> v(arg))` | 196 sites, 45 files | `CDynamicCast<T> v((arg)); if (v) ...` |
| `else if (CDynamicCast<T> v(arg))` | 104 sites, 17 files | `else if (T* v = (T*)(CDynamicCast<T>(arg)))` |
| `container.insert(container.end())` | 16 sites | `.emplace(.end())` |
| `ptr->insert(ptr->end())` | 6 sites | `->emplace(->end())` |
| `chained.insert(chained.end())` | 12 sites | `.emplace(.end())` |
| Misc `else if` paren fixes | ~14 sites | bulk regex repair |

## Per-file patches (upstream)

### Misc/Basic2.h
Added global `operator==(const CPtr<T>&, T*)`, `operator==(T*, const CPtr<T>&)`,
and `!=` variants for CPtr/CObj/CMObj. Modern MSVC's overload resolution
found `member op==(const T*)`, `member op==(const CPtr&)` and builtin
`op==(T*,T*)` (via `operator T*()` conversion) all "similarly convertible"
(C2666). The non-template global wins unambiguously.

### Misc/EventsBase.h
`CEventRegister<T,TParam>` dtor was `UnregisterEventHandler(this, typeid(TParam))`.
Modern MSVC instantiates the dtor's typeid(TParam) wherever a CEventRegister
field's enclosing object's dtor is called — requiring TParam complete at every
TU. Patched to cache `const type_info* pEventID = &typeid(TParam)` at ctor
time and use it in dtor. The ctor still needs TParam complete (called only
where ctor is instantiated, which already has the full type).

### Misc/tools.h
- `IsInSet(c, e)` rewritten from `return find(...)!=...` to explicit
  loop — std::find triggers C2666 on `*iter == elem` when iter is
  container-of-CPtr<T> and elem is T*.
- Removed earlier `silent_storm_port_detail::_eq` helper (moved global op==
  into Basic2.h, simpler).

### Misc/BasicFactory.h
Annotation only — `GetTypeID<T>` requires T complete. No code change; the
existing template is fine.

### ADOImport/BasicDB.h
- `CDBPtr<T>::operator&` body left as is; the underlying `NDatabase::GetTable<T>`
  call requires T complete for typeid. Worked around by pulling all NDb data
  headers into Main's StdAfx.h PCH (see below).
- `CDBTable<T>::GetRecord` dropped the `typeid(*pRes) == typeid(T)` assert
  (was VS2003-lax, modern MSVC requires complete T).

### Main/StdAfx.h
Pulled `<DBFormat/Data*.h>` into Main's PCH globally so every Main TU sees
all NDb:: data types as complete. This breaks the chain that would otherwise
force CDBPtr<T>::operator& to be parsed with forward-decl T.

### Main/Cache.h
Added `typename` to:
- `typename CTracker::pointer p`
- `typename Alloc::pointer pointer`
- `typename TElement::ESplitType t` (3 sites)

### Main/Pool.h
- `typename list<SBlock>::iterator i`
- `data.insert(data.end())` → `data.emplace(data.end())`

### Main/Interpolate.h
`typename TInterp::RET` qualifier.

### Main/BuildingGrid.h
`const ZSHIFT = 16` → `const int ZSHIFT = 16` (implicit int).

### Main/BuildingSchema.cpp
`const nz = ...` → `const float nz = ...`.

### Main/GAnimParticles.h
`CATrailPath::STrailPoint` moved from private to public (referenced by
wBullet.cpp).

### Main/aiPathTable.h
`CPathPlaceTable::SMove` moved from private to public (referenced by
derived CMultiMovesTable).

### Main/aiPassCalcer.h
`CPassCalcer::STile` moved from private to public (referenced by free
function `MarkCandidatesToDisplace`).

### Main/aiPosition.h
`struct SMove` with anonymous union of SPathPlace had implicitly-deleted
ctor/copy/assign. Added explicit `SMove(){}`, copy ctor and assignment
that copy via the first union alternative.

### Main/wMain.h
- `SWorldDeploySpot` moved to public (used by free function `GetDeployWithNumber`).
- Restored default access with `private:` after.

### Main/wDecal.h / wDumbUnit.cpp
`class CShowBloodUpdated;` (forward decl) → `class CShowBloodUpdated { ... };`
in both files. CEventRegister's ctor needs the type complete; ThrowEvent in
wDumbUnit.cpp similarly. Same class definition in both TUs is ODR-allowed.

### Main/wAck.cpp, wDecal.cpp/h, wMain.cpp, wUnitServer.cpp
Member function pointers must be qualified: `OnEvent` → `&Class::OnEvent`.

### Main/wAck.cpp
Imports the system `time.h` (header line) — caused by an editor/linter pass
during the build iterations; not strictly necessary.

### Main/wInterface.h, wUnitServer.h
Replaced `find(container_of_CPtr, T*)` with explicit `for` loops. The
container's element type is CPtr<T>, and `*iter == T*` is ambiguous in C++17.

### Main/wBuilding.cpp
`extern FixSmallPieceID(...)` → `extern int FixSmallPieceID(...)` (implicit
int return).

### Main/wDialog.cpp
`min(int, size_t)` → `min((size_t)int, size_t)`.

### Main/wUnitAttack.cpp
- Untangled stacked `else if (CDynamicCast<T>(arg))` chain in `GetActionType`
  — rewrote as series of independent CDynamicCast blocks.
- Wrapped duplicate-`pTarget` blocks in `{}` scopes.

### Main/wUnitStates.cpp
Wrapped sequential `CDynamicCast<T> p((cmd))` decls in `{}` scopes (modern
C++ disallows redefinition in same scope, original VS2003 allowed it).

### Main/wUnitAttackExec.cpp
`pCmd->GetTarget().(CUnitServer*)pUnit` → `(CUnitServer*)pCmd->GetTarget().pUnit.GetPtr()`
(malformed cast in original — must have been an extension typo).

### Main/aiAttackAction.cpp
- Two `(NRPG::IGrenadeItem*)pItem` casts where pItem is CDynamicCast<IWeaponItem>
  and the function takes `IWeaponItem*` — corrected to `(NRPG::IWeaponItem*)pItem`.

### Main/Interface.cpp
The `CMouseCaptureHandler` class references `IWindow` and `IInterface` types
that don't exist anywhere in the codebase — dead code. Excised wholesale
inside `#if 0 ... #endif`. Also commented its `REGISTER_SAVELOAD_CLASS` line.

### Main/UIInterface.cpp
Commented out duplicate `REGISTER_SAVELOAD_CLASS` lines for `CInterface`
(0xB2841122 — already registered in Interface.cpp) and `CMouseCaptureHandler`
(excised in Interface.cpp).

### Main/InventoryUnit.cpp
Added `{}` braces around `case UIT_WEAPON_HEAVY:` since a CDynamicCast
declaration's init is jumped over by subsequent case labels.

### Main/iGameStates.cpp
- Bulk CDynamicCast cleanup (else if chains). Some had `((arg)` paren
  imbalances that I fixed manually.
- Reverted broken `pTempObject.GetPtr()` after my refactor (pTempObject is
  T*, not CPtr).

### Main/iInventoryPanel.cpp
Renamed second `pUnloadItem` to `pUnloadItem2` (duplicate decl in same scope).

### Main/iMissionMovieUI.cpp
Renamed second `pTurn` (used for `CUICmdUnit`) to `pUnit` (was duplicate).

### Main/iMissionUI.cpp
Added explicit `(float)` casts to `min(float, int)` / `max(float, int)` calls.

### Main/iSaveManager.cpp
Replaced `localtime(&sStat.st_mtime)` + `wcsftime` with `FileTimeToSystemTime`
+ `swprintf_s`. Modern MSVC's `<ctime>` doesn't reliably expose `::localtime`
when `_HAS_CXX17=0`, and Main/ has its own `Time.h` shadowing the system one.

### Main/rpgPerk.cpp
Stubbed `CPerksTree::Draw` (dev-time graphviz dot dump). Removed
`<fstream>` include — it transitively pulls `<ctime>` which fails under our
compile flags (see iSaveManager note).

### Main/scriptPtr.h
Two more CDynamicCast pattern refactors (template context).

### Main/scFlowChart.cpp
- Explicit `1.0f`/`fCurrent` types for min/max.
- Member fn pointer qualification: `CompareSize` → `&CScenarioFlowChartPathFinder::CompareSize`.

### Main/GfxRender.cpp
`SRenderParam<SFBTransform> transformMode( SFBTransform() )` triggered
most-vexing-parse (parsed as fn decl). Changed `()` to `{}`.

### Main/GfxBuffers.cpp
- `frames.insert(frames.begin())` → `frames.emplace(frames.begin())`
- `frames.push_front()` (no arg) → `frames.emplace_front()`

### Main/GRenderLight.cpp
`inline IsPointLightSupported(...)` → `inline bool IsPointLightSupported(...)`.

### Main/RectPacker.cpp
`operator()(int a, int b)` → `bool operator()(int a, int b)` (implicit int).

### Main/RodJunction.cpp
`const iRnd = ...` → `const int iRnd = ...`.

### Main/RPGUnit.cpp
Added braces around `else { CDynamicCast<...>...; if (...)... }` (single-stmt
else+for-body issue from refactor).

### Main/GCombiner.cpp
- `pCombiner == _pCombiner` (CPtr==T*) → `pCombiner.GetPtr() == _pCombiner`
- `T::TRes` → `typename T::TRes`

### Main/GDecal.cpp
CPtr==T* fixed with explicit `.GetPtr()` calls.

### Main/MapBuild.cpp, MapBuildTerrain.cpp, PolyUtils.cpp, GTransparent.cpp,
### Main/iLogPanel.cpp, iMissionDlgUI.cpp, iPopupMenu.cpp, iStorePanel.cpp,
### Main/RPGUnitMission.cpp, RectPacker.cpp, UIInterface.cpp,
### Main/wTerrain.cpp, RPGItemSet.cpp, iTeamMngMenu.cpp, iUnitPanel.cpp,
### Main/wMain.cpp, MakeBuildingInternal.cpp
Various `.insert(.end())` → `.emplace(.end())` (modern std::vector/list
insert requires an arg).

### Main/wMain.cpp
- Member fn pointer qualifications (`registerOnNewPlayerFastTurnOrTime(this, &CWorld::OnNewPlayerFastTurnOrTime)`).
- Brace for-loop bodies around CDynamicCast lifts.

### MemoryMngr/DumbPow2Alloc.cpp
Replaced `FastDumbAlloc`/`FastDumbFree` (MemoryMngrDll, skipped) with
`malloc`/`free`. operator new/delete still override at link time.

### Game/WinFrame.cpp
`msgList.insert(msgList.end())` → `msgList.emplace(msgList.end())`.

## Port-owned files

### port/src/stubs/ado_stub.cpp (new)
Drop-in replacement for ADOImport's runtime symbols. Provides `NDatabase::*`
free functions and `CDBTableBase::*` member functions stubbed to just track
the tables hash (for Serialize). All Import()/Refresh() paths assert at
runtime — they were dev-tooling only.

### port/src/stubs/CMakeLists.txt
Added `silent_storm_ado_stub` target.

### port/src/main_stub.cpp (deleted)
Replaced by Game/Main.cpp.

### port/CMakeLists.txt
- Added `add_jan03_subproject(Main TYPE OBJECT)` with full dep wiring.
- Added `add_jan03_subproject(MiscDll)`.
- Added Game source files directly to `silent_storm` exe target (StdAfx.cpp,
  Main.cpp, WinFrame.cpp).
- Replaced `silent_storm` source (was `src/main_stub.cpp`) with the Game/Main.cpp
  WinMain entry.
- Linked silent_storm against legacy::stlport, all jan03::* libs, stubs, and
  Windows system libs (dxguid, d3d9, dinput8, winmm, odbc32, odbccp32).

### port/third_party/stlport_shim/legacy_compat.h
Added `extern "C" { #include <../ucrt/time.h> }` — Jan03's `Main/Time.h`
case-insensitively shadows the system `<time.h>`. Using the explicit UCRT
relative path bypasses the shadow so global namespace gets clock_t, etc.,
which `<ctime>`'s `using ::clock_t` later requires.

## Runtime behavior

```
$ silent_storm.exe
```

Launches a window titled "Error" — comes from `MessageBox(0, ..., "Error", MB_OK)`
in Game/Main.cpp, line 39 (likely "File game.db not found" — the game data
files aren't present in the build tree). Phase 0 acceptance: ✓ link clean,
exe launches, fails BEYOND any link issue.

## Phase 1+ TODO

1. **ADO Import**: actually run the ADO importer once on an external machine
   with msado15.dll, or replace game.db gen with sqlite/json from
   `upstream/Soft/Andy/.../A5GAME.mdb`.
2. **CMouseCaptureHandler**: dead-code resurrection — the class was
   half-written referencing nonexistent IWindow/IInterface. If a feature was
   intended here, rewrite.
3. **CDynamicCast refactor**: 300+ sites use `if (CDynamicCast<T> v(arg))` —
   a thin wrapper class. Phase 1 should drop CDynamicCast in favor of
   `dynamic_cast<T*>(arg)` directly.
4. **CEventRegister typeid**: my cached-pointer fix works but is fragile.
   Phase 1 should refactor the event system away from RTTI keys.
5. **PCH bloat**: Main's StdAfx.h now includes all NDb data headers globally
   — fast PCH build, but breaks incremental compilation when any NDb header
   changes. Phase 1 should narrow CDBPtr<T>'s instantiation requirements.
