---
name: p1_5_r17_probe_unsafe
description: Phase 1.5 r17 — attempted schema probe via Import() per-table at AddTable crashes (Import() refs other not-yet-loaded tables); rolled back
---

# Phase 1.5 r17 — Probe Import() unsafe; rolled back

**Date:** 2026-05-12
**Status:** **attempt failed, fully rolled back to r16 steady state**

## What r17 tried

Per r16 plan: instrument `NDatabase::ImportField` to record column names into `s_tableSchemas[currentTableID]`, then at each `NDatabase::AddTable` call run `probe = newf(); probe->Import()` to capture that table's schema. Storage load matches its c6 column set against `s_tableSchemas` to find the table.

## Why it failed

First `AddTable("Models")` runs `CRndModel::Import()` whose body is:

```cpp
void CRndModel::Import() {
    NDatabase::ImportField("Material0", &pMaterials[0]);
    // ... more ImportField calls ...
    string szFlags;
    NDatabase::ImportField("Flags", &szFlags);
    UnpackVariantFlags(szFlags, &flags);

    if (IsValid(pTemplate)) {
        pTemplate->variants.push_back(this);
        // ...
    } else {
        ASSERT(0);
        ErrOut("Bad RndModel: ", GetRecordID());
    }

    if (!IsValid(pGeometry))
        pGeometry = GetGeometry(N_SPHERE_GEOMETRY_ID);  // ← needs Geometry table loaded
}
```

`GetGeometry(id)` calls `NDatabase::GetTable<CGeometry>()` → which iterates the `tables` hash_map for the Geometry typeID. Geometry table isn't registered yet (we're inside the very first AddTable call). Null-deref → access violation (0xC0000005).

Conclusion: **`Import()` cannot be called during table registration**. It's a runtime-only path that assumes the entire DB is already loaded.

## Schema discovery — alternate paths

The schema info is still in the storage (r16 finding). To extract per-table schemas without running `Import()`:

**Path A — static parse of upstream source.** For each `CDBxxx`'s Import() implementation, grep the `ImportField("col", ...)` string literals. Build a hard-coded `nTableID → {columns}` table. Tedious (130 tables) but safe.

**Path B — Ghidra-RE `NDatabase::Serialize` in MapEdit.exe.** The shipping wire format must encode storage↔table mapping somehow (otherwise the game wouldn't work). Look at how MapEdit's NDatabase::Serialize reads tables; the type signature of `NDatabase::tables` may have changed from `hash_map<int, CDBTableBase>` to `hash_map<int, CObj<CDBTableDataStorage>>`. This is the cleanest path.

**Path C — defer schema match to runtime.** Some `GetRecord(nID)` callers know which table they're querying. If the call site sets a thread-local "current table" before calling, we can promote-on-first-access. But this requires patching every call site.

## Recommendation for r18

Path B (Ghidra on MapEdit.exe). Open MapEdit, search for the `NDatabase::Serialize` function, decompile, look at:
1. The shape of the `tables` variable being serialized — is it `hash_map<int, CDBTableBase>` or `hash_map<int, CObj<CDBTableDataStorage>>`?
2. The op& for whatever wraps it.

If hypothesis A from r15 doc is correct (tables = `hash_map<int, CObj<CDBTableDataStorage>>`), the fix is:
- Replace `NDatabase::Serialize` in ado_stub.cpp to read that type
- Build a `nTableID → storage*` map directly from wire
- PromoteRecordsFromStorage as in r17 (but with correct linkage)

## What's committed in r17

- Rolled all r17 logic out (Import probe removed, ImportField back to ASSERT, db_table_storage.h's tail-hook removed)
- This doc capturing the lesson
- Runtime state: assert.log = 335 lines, identical to r14/r16. No regression.
