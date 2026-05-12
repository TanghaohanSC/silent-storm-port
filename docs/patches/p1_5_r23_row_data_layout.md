---
name: p1_5_r23_row_data_layout
description: Phase 1.5 r23 — row data layout in CDBTableDataStorage decoded. m_buckets/chainNext/wstrings are per-row column-value arrays grouped by type
---

# Phase 1.5 r23 — Row data structure fully decoded

**Date:** 2026-05-12
**Status:** **structure understood, materialization roadmap clear**
**Previous:** r22 promoted 1633 record IDs from 155/155 storages but with empty fields

## The breakthrough

r23's diagnostic dump revealed the actual row-data layout. Previously named fields meant something completely different:

| Field (r13 name) | r23 corrected meaning | size pattern |
|---|---|---|
| `m_buckets` | `vector<vector<int>>` per-row int column values | rows × c6_count |
| `m_chainNext` | `vector<vector<int>>` per-row float (as bits) values | rows × c7_count |
| `m_wstrings` | `vector<vector<wstring>>` per-row wstring values | rows × c8_count |
| `m_records` | column **descriptors** `{name, type_code}` | columns total |
| `m_column6` | int column **names** | c6_count |
| `m_column7` | float column **names** | c7_count |
| `m_column8` | wstring column **names** | c8_count |

Sample (Weapons table, nTableID=-536870910, 105 rows × 42 columns):
```
m_records[0]:    body="ID" id=0        # column descriptor: ID is primary key
m_records[1]:    body="UserName" id=3  # column: UserName, type 3 = wstring
m_records[2]:    body="InnerClip" id=0 # column: InnerClip, type 0 = int

m_column6 = ["ID", "InnerClip", "InnerClipAmmoQuantity", ...]  # 37 int col names
m_column7 = ["Silencer", "TrailSpeed"]                          # 2 float col names
m_column8 = ["UserName", "AnimationName", "WeaponRPGLogics"]   # 3 wstring col names

m_buckets[0]    = [1, 1, -1, 2, 253, 8, 91, 105, ...]          # row 0: 37 int values
m_chainNext[0]  = [1065353216 (=0x3F800000=1.0f),
                   1109393408 (=0x42220000=40.5f)]              # row 0: 2 float values
m_wstrings[0]   = ["Colt M1911", "", ""]                        # row 0: 3 wstring values

m_buckets[1]    = [2, 1, ...]                                    # row 1
m_wstrings[1]   = ["Luger (British)", "", ""]
m_wstrings[2]   = ["Luger P-08 (Para)", "", ""]
```

Primary key (record ID) is always `m_buckets[row][0]` (first int column = "ID").

## Materialization plan (r24)

To populate `CDBTableBase::records[nID] = filled_record`:

```cpp
for each (nTableID, table) in NDatabase::tables:
    s = table.GetStorage();
    if (!s) continue;
    
    nRows = s->m_buckets.size();
    for row in 0..nRows:
        // 1. Create record instance
        CDBRecord* rec = factory.CreateObject(nTableID);
        
        // 2. Set primary key (always first int column)
        int nID = s->m_buckets[row][0];
        rec->nID = nID;
        
        // 3. For each column, find the matching field on rec by NAME
        //    and write the value. This requires CDBRecord's Import() to
        //    have been used to record (col_name → field_addr) mapping.
        for j in 0..c6_count:
            col_name = s->m_column6[j];
            field_addr = field_map[nTableID][col_name];
            if field_addr: *(int*)field_addr = s->m_buckets[row][j];
        for j in 0..c7_count:
            col_name = s->m_column7[j];
            field_addr = field_map[nTableID][col_name];
            if field_addr: *(float*)field_addr = bitcast<float>(s->m_chainNext[row][j]);
        for j in 0..c8_count:
            col_name = s->m_column8[j];
            field_addr = field_map[nTableID][col_name];
            if field_addr: *(wstring*)field_addr = s->m_wstrings[row][j];
        
        // 4. Register the now-filled record into the table
        table.InsertRecord(nID, rec);
```

The `field_map` is the (col_name → field_addr) per typeID, captured by patching `NDatabase::ImportField` to record names + addresses. We then need to:

1. Either run `Import()` per-record at row-fill time (handles refs cleanly — *Import is fine when DB is mostly loaded*)
2. Or extract the field map once via a probe approach (failed in r17 because Import calls cross-table refs)

The crash from r17 was because Import() did `GetGeometry(N_SPHERE)` which deref'd a not-yet-loaded table. By the time we run Import for materialization (after the basic ID-promote in r22), all tables exist and most have records — so Import should succeed without crashing.

## Why this approach should work

- `Import()` already exists in every CDBxxx::Import() method (one per table type)
- It calls `ImportField("col_name", &this->member)` for each column — exactly what we need
- Just need ImportField stubs to **record the (name, addr) pairs** instead of asserting
- Then we have a per-record field map keyed by column name

## Why r22's IDs-only promote isn't enough

Current state (post-r22): records exist with correct `nID` but all other fields are zero/null. Main loop iteration tries to read e.g. `pRes = m_pTextures` (some CDBTexture ptr) on a Models record and gets null because Material0 field wasn't filled → assert fail or null deref later.

After r24 materializes the actual field values, Models record's `m_pTextures` becomes valid → resource lookup chains work → real Nival UI renders past the menu.

## r23 dump diagnostic preserved in the source as reference

`silent_storm_r23_body_dump.log` shows the exact mapping with real game data. Useful for future debugging.
