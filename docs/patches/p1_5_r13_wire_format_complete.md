---
name: p1_5_r13_wire_format_complete
description: Phase 1.5 r13 — CDBTableDataStorage wire format fully reverse-engineered via Ghidra; ready to write port-side implementation
---

# Phase 1.5 r13 — `CDBTableDataStorage` wire format FULLY recovered

**Date:** 2026-05-12
**Status:** **RE complete — ready to implement**
**Previous:** r12 had 3/5 handlers + partial structure hypothesis (wrong about field 5)
**Tool:** Ghidra 12.0.4 + JDK 21.0.11 LTS

## Final wire format

```cpp
class CDBTableDataStorage : public CObjectBase {
    // [+0x00..0x0B] CObjectBase vptr + bookkeeping

    std::vector<std::vector<int>>             buckets;        // [+0x0C] field 2
    std::vector<std::vector<int>>             chainNext;      // [+0x18] field 3
    std::vector<std::vector<std::wstring>>    wstrings;       // [+0x24] field 4
    std::vector<RecordEntry>                  records;        // [+0x30] field 5  ⭐
    std::vector<std::vector<byte>>            column6;        // [+0x3C] field 6
    std::vector<std::vector<byte>>            column7;        // [+0x48] field 7
    std::vector<std::vector<byte>>            column8;        // [+0x54] field 8

    // [+0x60] element count
    // [+0x74..+0x84] internal subobject (load-factor / free-list helper)

    int operator&(CStructureSaver& f);  // dispatches fields 2..8
};

struct RecordEntry {            // 16 bytes
    std::vector<byte> body;     // 12 bytes — pre-serialized record bytes
    int               id;       // 4 bytes  — record nID (primary key)
};
```

## How it works conceptually

`CDBTableDataStorage` is a **packed columnar table with lazy deserialization**:

1. **Load time** (game.db deserialization):
   - Records aren't stored as live `CObj<CDBRecord>` pointers
   - Each record's `Serialize(CStructureSaver)` output is captured as a byte blob and stored in `records[i].body`
   - The record's `nID` is stored in `records[i].id`
   - Hash buckets (`buckets` / `chainNext`) index from `recordID` to `records[]` slot
   - `wstrings` / `column6/7/8` hold parallel column data (localized strings, type info, foreign-key relations, etc.)

2. **Runtime** (`GetRecord(int nID)`):
   - `hash(nID) % buckets.size()` → bucket
   - Walk `chainNext` chain through bucket entries until match
   - Found: `RecordEntry& entry = records[idx]`
   - First-access: re-deserialize `entry.body` bytes through `CStructureSaver` → produces a live `CDBRecord*` of the correct type
   - Cache the live pointer in `CDBTableBase::records` (the normal `hash_map<int, CObj<CDBRecord>>`)
   - Subsequent calls hit the cache

This explains everything we observed:
- Why 155 `0xA1843130` instances appear in `game.db` (one per registered DB table type)
- Why the Jan03 source's `CDBTableBase` (hash_map inline) can't read the wire (it's wrapped in `CDBTableDataStorage` now)
- Why all the 12-byte sub-handlers (`FUN_0044CB30/CC90/CEC0`) and the 16-byte one (`FUN_0044D810`) share the same vector-of-vector pattern — they're all columnar storage

## Handler/address cross-reference (final)

| Wire field | Offset | Handler | Inner serializer | Type |
|---|---|---|---|---|
| 2 | +0x0C | FUN_0044CB30 | FUN_0044CC10 (vec\<int\> blob) | `vec<vec<int>>` |
| 3 | +0x18 | FUN_0044CC90 | FUN_0044CD20 (vec\<int\> blob) | `vec<vec<int>>` |
| 4 | +0x24 | FUN_0044D810 | FUN_0044D8E0 → FUN_0091A7B0 (2-byte aligned blob) | `vec<vec<wstring>>` |
| 5 | +0x30 | FUN_0044D360 | FUN_0044D3E0 → FUN_0044D410 (16-byte struct) | `vec<{vec<byte>, int}>` |
| 6 | +0x3C | FUN_0044CEC0 | FUN_0091A760 (raw byte blob) | `vec<vec<byte>>` |
| 7 | +0x48 | FUN_0044CEC0 | FUN_0091A760 (raw byte blob) | `vec<vec<byte>>` |
| 8 | +0x54 | FUN_0044CEC0 | FUN_0091A760 (raw byte blob) | `vec<vec<byte>>` |

CStructureSaver framework primitives (in `0x91Axxx` segment, shared across all classes):

| VA | Role |
|---|---|
| `0x0091A370` | `Field(idx, ptr, size, type)` — generic typed read/write of N bytes |
| `0x0091A500` | `ReadInt(field)` — read int field |
| `0x0091A520` | `FinishField()` — close a field sub-chunk |
| `0x0091A6C0` | `IsField(idx, subIdx)` → bool — try to enter a field sub-chunk |
| `0x0091A760` | raw byte blob ref (`vec<byte>` content) — used by field 6/7/8 and field 5 body |
| `0x0091A7B0` | 2-byte aligned blob ref (`vec<wchar_t>` content) — used by field 4 wstrings |

## Implementation plan (Phase 1.5 r14)

```
Task 1 — wire-compatible class (port/src/stubs/db_table_storage_full.cpp)
  • Define class CDBTableDataStorage : public CObjectBase
  • Embed the 7 fields with proper STL types
  • operator&(saver) calls f.Add(2..8, &field)
  • Inherit serialize semantics from STL — std::vector + std::wstring are already
    auto-serialized by the framework's f.Add overloads (verify by checking Misc/basic2.h
    f.Add<T> templates).

Task 2 — patch CDBTableBase to delegate
  • Add CObj<CDBTableDataStorage> m_storage member
  • Modify operator&(f): f.Add(1, &m_storage)  // replaces inline hash_map<...> records
  • Modify GetDBRecord(int nID):
      1. Hash nID, find in m_storage.buckets + chainNext
      2. If found, get RecordEntry from m_storage.records[idx]
      3. If records-cache miss for nID, deserialize entry.body to a CDBRecord
         and cache. Otherwise return cached.

Task 3 — verify
  • Rebuild, run, check silent_storm_missing_classes.log absent (should still be)
  • Check silent_storm_assert.log line count drops from 1340 → much smaller
    (records should now populate, BasicDB.h:102 asserts should resolve)
  • Check r8_mainmenu.log NO LONGER firing — real Nival mainmenu UI render

Estimated effort: 1-2 dedicated sessions, mostly because of:
  • Verifying the hash function (MSVC stdext::hash_map<int> uses (val * 2654435761) typically)
  • Getting the lazy deserialization re-entry working without infinite loop
  • Handling the column 6/7/8 content (might be needed for full record reconstruction
    or might be just lookup-table data — needs experimentation)
```

## Open questions for impl

1. **What hash function does `CDBTableDataStorage` use?** MSVC stdext::hash uses a multiply-by-Knuth-constant scheme. The bucket indexing is `(*hash) / bucket_count`, so bucket index = `hash(nID) % bucket_count`. Need to confirm by inspecting `FUN_0044A010` or similar bucket-lookup methods in the vtable.

2. **What's in column 6/7/8?** Could be:
   - Foreign key relations (CObj-style references to other tables, stored as server-pointer arrays)
   - Type ID arrays (one typeID per record for polymorphic table types)
   - Index column data (sorted indexes by name, etc.)

   Worth a quick look at `0x0044C800` neighborhood (the import/AddTable functions) to see how these get populated at load time.

3. **Does the body blob in field 5 need re-serialization to start with a special header?** When we read it back through CStructureSaver we need to set up the chunk frame correctly. Probably need to wrap the bytes in a CStructureSaver context with the right "this is a CDBRecord subclass with typeID X" header.

## Files touched this round

- `port/docs/patches/p1_5_r13_wire_format_complete.md` (this file)
- The empty stub at `port/src/stubs/db_table_storage_stub.cpp` from r11 STAYS — it's still correct as a partial fix. The full impl will replace it in r14.

## Status of structural wall

Was: "unknown class blocks game.db deserialize → 155 missing → 328 cascade asserts → fallback menu"

Now: "fully understood lazy-deserialization scheme. Port needs to either:
  (a) implement matching storage + lazy materialization, OR
  (b) at load time, eagerly pre-deserialize all 155 storage objects' bodies into the legacy hash_map.
  Both produce working records. (a) is more compatible; (b) is faster to ship."
