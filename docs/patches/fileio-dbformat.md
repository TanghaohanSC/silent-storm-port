# FileIO + DBFormat Patches Log

## Phase 0 Task 6 — May 2026

### Scope
- `upstream/Soft/Andy/Jan03/a5dll/FileIO/`
- `upstream/Soft/Andy/Jan03/a5dll/DBFormat/`

### Result: Both targets compile and link cleanly (exit 0)

**Build verification:**
- `cmake --build --preset msvc-debug --target jan03_FileIO --clean-first` → exit 0
  - Compiled: StdAfx.cpp, BasicChunk1.cpp, FilesPackage.cpp, Streams.cpp → `lib/FileIO.lib`
- `cmake --build --preset msvc-debug --target jan03_DBFormat --clean-first` → exit 0
  - Compiled: DataConst.cpp, DataCamera.cpp, DataDifficulty.cpp, DataAI.cpp,
    DataAck.cpp, DataScript.cpp, DataInterface.cpp, DataPerk.cpp,
    DataRpgConstants.cpp, DataScenario.cpp, DataRPGTmp.cpp, DataRPG.cpp,
    DataMap.cpp, DataFormat.cpp → `lib/DBFormat.lib`

---

## Patches Applied

### 1. FileIO/BasicChunk1.cpp — `push_back()` → `emplace_back()` (2 sites)

**File:** `upstream/Soft/Andy/Jan03/a5dll/FileIO/BasicChunk1.cpp`

**Line 335** (`CStructureSaver::StartChunk`):
```cpp
// Before:
chunks.push_back();
// After:
chunks.emplace_back();  // silent-storm-port: was chunks.push_back() — modern std::list requires arg or use emplace_back
```

**Line 388** (`CStructureSaver::Start`):
```cpp
// Before:
chunks.push_back();
// After:
chunks.emplace_back();  // silent-storm-port: modern std::list — was push_back()
```

**Reason:** Modern MSVC `std::list::push_back()` requires an argument (the value
to push). Nival's code called `push_back()` with no arguments intending
default-construction of the element — a non-conforming extension of the VC6-era
STLport. `emplace_back()` (C++11) default-constructs in-place, which is the
correct modern equivalent.

---

### 2. DBFormat/DataAI.cpp — Added `#include "DataSound.h"`

**File:** `upstream/Soft/Andy/Jan03/a5dll/DBFormat/DataAI.cpp`

**Added at line 3:**
```cpp
#include "DataSound.h"  // NDb::CSound full definition needed for GetTable<CSound>()
```

**Reason:** `DataAI.h` forward-declares `NDb::CSound`; the `.cpp` file calls
`NDatabase::GetTable<CSound>()`, which needs the full class definition to
instantiate the template. Without the full definition the compiler emits
C2027 (use of undefined type).

---

### 3. DBFormat/DataAck.cpp — Added `#include "DataFormat.h"` and `#include "DataSound.h"`

**File:** `upstream/Soft/Andy/Jan03/a5dll/DBFormat/DataAck.cpp`

**Added at lines 3–4:**
```cpp
#include "DataFormat.h"   // NDb::CString, NDb::CSequence full definitions
#include "DataSound.h"    // NDb::CSound full definition
```

**Reason:** `DataAck.h` uses forward declarations for `NDb::CSound`,
`NDb::CString`, and `NDb::CSequence`. The `.cpp` body dereferences pointers
to these types, requiring full definitions. Without them the compiler emits
C2027 for pointer-to-incomplete-type dereferences.

---

### 4. DBFormat/DataInterface.cpp — Added `#include "DataFormat.h"` and `#include "DataSound.h"`

**File:** `upstream/Soft/Andy/Jan03/a5dll/DBFormat/DataInterface.cpp`

**Added at lines 2–4:**
```cpp
#include "DataFormat.h"
#include "DataInterface.h"
#include "DataSound.h"    // NDb::CSound full definition needed for GetTable<CSound>()
```

**Reason:** Same pattern: `DataInterface.cpp` calls into templates over `NDb::CSound`
that need the full definition at the template instantiation point. Also `DataFormat.h`
is needed for `NDb::CString` and `NDb::CTexture` full types used in method bodies.

---

### 5. DBFormat/DataPerk.cpp — Added `#include "DataInterface.h"`

**File:** `upstream/Soft/Andy/Jan03/a5dll/DBFormat/DataPerk.cpp`

**Added at line 5:**
```cpp
#include "DataInterface.h"  // NDb::CUITexture full definition needed for GetTable<CUITexture>()
```

**Reason:** `CDBPerk::Import()` assigns to a `CPtr<NDb::CUITexture>` field (`pIcon`,
`pIconDisabled`). Without the full `CUITexture` definition the compiler cannot
instantiate `CPtr<CUITexture>` assignment or `GetTable<CUITexture>()`.

---

### 6. DBFormat/DataRPG.cpp — Added `#include "DataInterface.h"` and `#include "DataAck.h"`

**File:** `upstream/Soft/Andy/Jan03/a5dll/DBFormat/DataRPG.cpp`

**Added at lines 11–12:**
```cpp
#include "DataInterface.h"  // NDb::CUITexture, NDb::CUIContainer full definitions
#include "DataAck.h"        // NDb::CDBDialogPers full definition
```

**Reason:** `DataRPG.cpp` contains methods on `CRPGPers` and related classes that
reference `NDb::CUITexture`, `NDb::CUIContainer`, and `NDb::CDBDialogPers` by
value or call `GetTable<T>()` on them. All three are forward-declared in `DataRPG.h`
but require full definitions in the `.cpp`.

---

### 7. DBFormat/DataMap.cpp — Added multiple full-definition includes

**File:** `upstream/Soft/Andy/Jan03/a5dll/DBFormat/DataMap.cpp`

**Added at lines 13–17:**
```cpp
#include "DataLight.h"      // NDb::CTAmbientLight full definition
#include "DataObject.h"     // NDb::CPlacableObject, NDb::CRndContainerModel full definitions
#include "DataInterface.h"  // NDb::CUITexture full definition
#include "DataAI.h"         // NDb::CUnitGroup full definition
#include "DataRPG.h"        // NDb::CUnit, NDb::CExplosion, etc.
```

**Reason:** `DataMap.cpp` is the largest translation unit (~800 lines) and
dereferences the widest variety of DB types via `GetTable<T>()` calls and direct
field access on `CPtr<T>` values. `DataMap.h` only has forward declarations for
all of these types. Each added `#include` resolves one or more C2027
(use-of-undefined-type) or C2079 (uses-undefined-class) errors.

---

### 8. DBFormat/DataFormat.cpp — Already included all needed headers

**File:** `upstream/Soft/Andy/Jan03/a5dll/DBFormat/DataFormat.cpp`

`DataFormat.cpp` already included `DataRPG.h`, `DataInterface.h`, `DataAI.h`,
`DataAck.h`, `DataObject.h`, `DataSound.h`, `DataLight.h`, `DataPerk.h` etc.
at the top level — no additional `#include` patches were required for this file.
The file's timestamp was updated as part of the same batch fix session but
the includes it needed were already present.

---

## Summary

| File | Patch type | Sites |
|------|-----------|-------|
| `FileIO/BasicChunk1.cpp` | `push_back()` → `emplace_back()` | 2 |
| `DBFormat/DataAI.cpp` | Added `#include "DataSound.h"` | 1 |
| `DBFormat/DataAck.cpp` | Added `#include "DataFormat.h"` + `#include "DataSound.h"` | 2 |
| `DBFormat/DataInterface.cpp` | Added `#include "DataSound.h"` | 1 |
| `DBFormat/DataPerk.cpp` | Added `#include "DataInterface.h"` | 1 |
| `DBFormat/DataRPG.cpp` | Added `#include "DataInterface.h"` + `#include "DataAck.h"` | 2 |
| `DBFormat/DataMap.cpp` | Added 5 full-definition includes | 5 |

**Total upstream patches: 14 patch sites across 7 files**

No changes to shared files (`Misc/`, `port/cmake/`, `port/src/stubs/`) were needed
for these two subprojects.
