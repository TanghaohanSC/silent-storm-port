---
name: p1_5_r16_schema_in_storage
description: Phase 1.5 r16 — diagnostic dump reveals CDBTableDataStorage columns 6/7/8 contain ASCII schema info (column names); schema-based table linking is the path forward
---

# Phase 1.5 r16 — Schema info lives inside storage columns

**Date:** 2026-05-12
**Previous:** r15 promote failed because wire has no CObj reference from CDBTableBase to its storage
**Method:** instrumented `CDBTableDataStorage::operator&` to dump first bytes of c6/c7/c8 columns per storage

## Key finding

**`m_column6`, `m_column7`, `m_column8` are NOT raw byte blobs — they're `vector<std::string>` of ASCII column names (the table's schema).**

Sample dump (155 storage entries total):
```
storage @ 02667770: rec=42 buck=105 wstr=105 c6=37 c7=2 c8=3
                    c6[0]="ID"  c7[0]="Silencer"  c8[0]="UserName"
storage @ 02667040: rec=46 buck=728 wstr=728 c6=27 c7=16 c8=3
                    c6[0]="ID"  c7[0]="CameraAn..."  c8[0]="UserName"
storage @ 02667490: rec=58 buck=2045 wstr=2045 c6=47 c7=4 c8=7
                    c6[0]="ID"  c7[0]="FPS"  c8[0]="SrcName"
```

All storages have `c6[0] = "ID"` (primary key name). c7/c8 are table-specific.

`m_records[i].body` is **2 bytes**, way too small for full record data → records' actual data lives in the schema-keyed wstring + column columns, not in body.

## Implication for r15's failed approach

r15 tried to link storage → table via a `CObj<CDBTableDataStorage>` reference in CDBTableBase's wire chunk. That reference doesn't exist in the wire — confirmed.

But **the storage carries its own schema as text**, which can be reverse-matched against each CDBTable<T>'s expected schema (defined by its `T::Import()` member calling `ImportField("col1", ...)`, `ImportField("col2", ...)`, etc.).

## r17 plan: schema-based table linkage

```
Step 1 — capture per-table expected columns
  • Patch NDatabase::ImportField stubs (currently `ASSERT(0)`) to record
    column names into a thread_local registry keyed by "currently probing"
    table ID.
  • In NDatabase::AddTable, after registering factory, construct one probe
    instance via newf(), set s_currentTableID = nTableID, call probe->Import(),
    clear the thread-local, delete probe.
  • Result: s_tableSchemas[nTableID] = { "col1", "col2", ... }

Step 2 — match storage to table via schema intersection
  • On each CDBTableDataStorage load (in operator& tail), collect its full c6
    column list (currently we only see c6[0]).
  • Linear-scan s_tableSchemas for a table where storage's c6 ⊇ table's expected
    columns (or some scoring function).
  • Record (storage → nTableID) in s_storageToTable map.

Step 3 — promote records
  • Same as r15's PromoteRecordsFromStorage, but iterate s_storageToTable
    (now non-empty).
  • For each (storage, nTableID): for each (id, body) in storage.m_records,
    factory(nTableID) → CDBRecord*, set nID, table.InsertRecord(id, p).
  • body is only 2 bytes per record — actually decoding the field values
    is a separate step (and may need to read body + cross-reference into
    the wstring column for human-readable fields).
```

## Diagnostic code shipped this round

- `port/src/stubs/db_table_storage.h`: `operator&` writes to `silent_storm_r16_storage_dump.log` on each storage load (155 lines, one per storage)
- `port/src/stubs/ado_stub.cpp`: `NDatabase::AddTable` writes to `silent_storm_r16_addtable.log` (130 lines, one per registered table)

Diagnostic logging can be removed in r17 or kept gated behind a `#ifdef SS_R16_DUMP`.

## State of runtime

| Round | missing log | assert.log | menu | storage data |
|---|---|---|---|---|
| baseline | 155 lines | 1340 | dbg fallback | not loaded |
| r11 | 0 ✓ | 1340 | dbg fallback | not loaded (empty stub) |
| r14 | 0 ✓ | **335** ↓75% | dbg fallback | **fully loaded** ✓ |
| r15 | 0 ✓ | 335 | dbg fallback | loaded but orphaned |
| r16 | 0 ✓ | 335 | dbg fallback | loaded, schema visible |

r16 doesn't change runtime — it's an instrumentation round that uncovered the schema-in-storage finding. r17 wires the schema match through and should drop asserts toward 0.

## Tables registered (first 8 of 130, from addtable.log)

```
[0] 0x00000001 'Models'        [4] 0x00000005 'TemplVariants'
[1] 0x00000002 'Animations'    [5] 0x00000006 'Rects'
[2] 0x00000003 'Textures'      [6] 0x00000007 'FinalElements'
[3] 0x00000004 'Templates'     [7] 0x00000008 'BRDFs'
```

Wait — 130 tables registered but only 155 storage objects loaded. 25 storage objects don't correspond to a table? Or some tables have multiple storages? Or storage count > table count because something else uses 0xA1843130 (sub-tables / partitions)? Worth investigating in r17.
