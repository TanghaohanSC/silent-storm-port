---
name: p1_5_r15_promote_attempt_failed
description: Phase 1.5 r15 — failed attempt to wire CDBTableDataStorage back to CDBTableBase via CObj reference; storage objects are orphans in obj pool
---

# Phase 1.5 r15 — Promote attempt failed (storage objects are orphans)

**Date:** 2026-05-12
**Status:** **attempt failed but documented; r14 progress unchanged**
**Result:** `silent_storm_r15_promote.log` shows `promoted 0 records from 0 storage objects`

## What r15 tried

1. Patched `BasicDB.h`:
   - Added forward decl `void PortReadCDBTableStorage(CStructureSaver&, CDBTableBase*)`
   - Replaced `CDBTableBase::operator&`'s `f.Add(1, &records)` with `PortReadCDBTableStorage(f, this)`
   - Added inline `CDBTableBase::InsertRecord(int nID, CObjectBase*)` setter (friend-of-CDBRecord access to private `nID`)

2. Implemented in `ado_stub.cpp`:
   - `PortReadCDBTableStorage(f, pTable)`: reads `CObj<CDBTableDataStorage>` via `f.Add(1, &storage)`, records the pair `(pTable → storage)` in a static map
   - `PromoteRecordsFromStorage()`: scans the map, for each storage iterates `m_records[i]`, factories an empty `CDBRecord` of the table's typeID, calls `pTable->InsertRecord(id, p)`
   - Hooked at the end of `NDatabase::Serialize`

3. Split `CDBTableDataStorage` class to `port/src/stubs/db_table_storage.h` (declaration) and `db_table_storage_stub.cpp` (REGISTER_SAVELOAD_CLASS)

## Why it failed

Build succeeded clean. Runtime test:
```
silent_storm_r15_promote.log: r15 promoted 0 records from 0 storage objects
silent_storm_assert.log:       335 lines  (same as r14)
silent_storm_r8_mainmenu.log:  still triggered
```

`s_tableToStorage` map ended up empty — `f.Add(1, &storage)` inside `PortReadCDBTableStorage` produced `storage.GetPtr() == null` for every CDBTableBase entry.

**The wire format does NOT put a CObj<CDBTableDataStorage> reference inside CDBTableBase's chunk.** The 155 storage instances are loaded from the obj pool (via CStructureSaver's `Start()` machinery that creates every typeID-tagged object), but **nothing in the wire holds a CObj reference to them** — they're orphans in the obj pool with refcount=0, kept alive only because nobody calls `Release()`.

This explains why r14's storage data showed up correctly (CStructureSaver creates objects + calls their op& regardless of reference graph), but r15's approach to find storage *from* table failed.

## What this means for the wire model

Shipping's data flow is one of:

**Hypothesis A** — `NDatabase::tables` itself has a different wire type. Could be `hash_map<int, CObj<CDBTableDataStorage>>` (replacing the table-by-value with a storage-pointer-by-reference). In that case, `NDatabase::Serialize` should read storage objects directly into a table-id-keyed map, and there is no CDBTableBase wrapper on the wire at all.

**Hypothesis B** — Storage objects self-register on load. Some global structure (`NDatabase` internal cache, or a `static` member somewhere) holds a `vector<CObj<CDBTableDataStorage>>` populated by an `operator&` side-effect or a vtable hook. CDBTableBase queries this global by table-id when needed.

**Hypothesis C** — The mapping is encoded inside storage's own data — e.g., `m_column8` first byte contains nTableID, or `m_records[0].id` is actually nTableID. Storage knows which table it belongs to without an external reference.

## Where r16 should investigate

1. Open `MapEdit.exe` in Ghidra → cross-reference XRef the typeID `0xA1843130`. Find all functions that read this typeID. Beyond the registration site (FUN_0044CA30) — look for places that pop typeIDs from CStructureSaver and call CreateObject. There must be SOMETHING that holds a CObj ref to keep these alive in shipping.

2. Look at the shipping NDatabase::Serialize equivalent. The Jan03 source has `f.Add(1, &tables)` where tables is `hash_map<int, CDBTableBase>`. Shipping may have replaced `tables` with a different type — could find via string xref for "Serialize" or by chasing the call site in Main.cpp's WinMain.

3. Inspect actual byte content of a sample `m_records[0].body` and check if there's a leading `nTableID` field. Pure data-side experiment, doable from r14's loaded state by adding a `printf` of body[0..7] for a few records.

## Files in this state (commit r15)

- `port/src/stubs/db_table_storage.h` (new, kept — same shape as r14 inline class)
- `port/src/stubs/db_table_storage_stub.cpp` (downgraded: just the REGISTER macro now)
- `port/src/stubs/ado_stub.cpp` (added: PortReadCDBTableStorage, PromoteRecordsFromStorage, s_tableToStorage map)
- `upstream/Soft/Andy/Jan03/a5dll/ADOImport/BasicDB.h` (patched: forward decl, op& delegation, inline InsertRecord)

All code compiles and runs; promote is a no-op (0 records) until the storage-to-table linkage is figured out.

## Net effect of r15 vs r14

Identical runtime behavior — 0 missing-class log lines, 335 BasicDB.h:102 cascade asserts (records still empty), r8 fallback menu still drawn. The work is real plumbing that's ready to receive data once the linkage hypothesis is verified.
