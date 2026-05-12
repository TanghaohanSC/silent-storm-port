---
name: p1_5_r12_wire_handlers_partial
description: Phase 1.5 r12 — Ghidra-assisted decompilation of CDBTableDataStorage wire handlers; structure clearer but full materialization is multi-day work
---

# Phase 1.5 r12 — Wire-handler decompilation (partial)

**Date:** 2026-05-12
**Status:** **structure understood, full materialization deferred**
**Previous:** r11 identified the class + shipped empty stub
**Tool:** Ghidra 12.0.4 + JDK 21.0.11 LTS (installed this session)

## What we got from Ghidra

`CDBTableDataStorage::operator&` (FUN_0044CA30) decompiled cleanly:

```c
if (saver.IsField(2, 1)) { FUN_0044CB30(this + 0x0C); saver.FinishField(); }
if (saver.IsField(3, 1)) { FUN_0044CC90(this + 0x18); saver.FinishField(); }
if (saver.IsField(4, 1)) { FUN_0044D810(this + 0x24); saver.FinishField(); }
if (saver.IsField(5, 1)) { FUN_0044D360(this + 0x30); saver.FinishField(); }
if (saver.IsField(6, 1)) { FUN_0044CEC0(this + 0x3C); saver.FinishField(); }
if (saver.IsField(7, 1)) { FUN_0044CEC0(this + 0x48); saver.FinishField(); }
if (saver.IsField(8, 1)) { FUN_0044CEC0(this + 0x54); saver.FinishField(); }
return 0;
```

Three of the five sub-handlers decompiled too:

| Handler | Field(s) | Pattern |
|---|---|---|
| `FUN_0044CB30` | 2 | `vector<vector<int>>` — outer vector of 12-byte triples (begin/end/eos), each is a `vector<int>` serialized as count + raw-int blob via `FUN_0044CC10` |
| `FUN_0044CC90` | 3 | Same `vector<vector<int>>` shape but per-entry serializer `FUN_0044CD20` (different destructor `FUN_00440F10` vs `operator_delete[]`) |
| `FUN_0044CEC0` | 6, 7, 8 | `vector<12-byte-entry>` where each entry is serialized by **CStructureSaver framework method** `FUN_0091A760` |

`FUN_0091A760` decompile shows it's a **raw-byte blob serializer**:

```c
if (writing) saver.WriteBlob(*entry, entry[1] - *entry);
else { src = saver.chunk_data + entry.offset; FUN_00409620(src, src + size); }
```

So each 12-byte entry of fields 6/7/8 is itself a `std::vector<byte>` (begin/end/eos), and the writer just dumps the raw byte range to disk. On read, the reader registers a reference to a range inside the chunk data stream (deferred / lazy resolution).

Not yet decompiled: `FUN_0044D810` (field 4) and `FUN_0044D360` (field 5).

## Revised structure of `CDBTableDataStorage`

```
[+0x00] CObjectBase vptr
[+0x04..+0x08] nObjData, nRefData (CObjectBase bookkeeping)

[+0x0C] vector<vector<int>>        field 2 — hash bucket heads (chain index)
[+0x18] vector<vector<int>>        field 3 — bucket chain next-pointers
[+0x24] ???                         field 4 — needs RE
[+0x30] ???                         field 5 — needs RE
[+0x3C] vector<vector<byte>>       field 6 — packed byte blob array #1
[+0x48] vector<vector<byte>>       field 7 — packed byte blob array #2
[+0x54] vector<vector<byte>>       field 8 — packed byte blob array #3

[+0x60] element count (mutated by vtable[7])
[+0x74..+0x84] inner subobject (load-factor / free-list helper)
```

## What this is, conceptually

`CDBTableDataStorage` is **not** a thin `hash_map<int, CObj<CDBRecord>>` wrapper. It's a **packed columnar storage with lazy deserialization**:

- Records aren't stored as live `CObj<CDBRecord>` pointers
- They're **pre-serialized into raw byte blobs** at table-load time, stored as three parallel `vector<vector<byte>>` arrays (fields 6/7/8 — probably key column / string column / body column, but column identity needs more work)
- Hash buckets + chains (fields 2/3) index into those blob arrays
- Fields 4/5 are likely metadata: maybe record type IDs, hash values, or version counters
- When game code calls `GetRecord(id)`, the storage hashes id → bucket → chain → blob index → on first access deserializes the blob to a real `CDBRecord` and caches the pointer

This explains why the open-source Jan03 `CDBTableBase` (which has `hash_map<int, CObj<CDBRecord>> records` inline) couldn't read the shipping wire — the wire format is structurally different (columnar+lazy vs hash_map).

## Why the port can't ship a full impl this session

A real implementation needs:

1. ✅ Recover field 4/5 semantics (RE `FUN_0044D810` + `FUN_0044D360`) — 1-2 hours
2. ⏭️ Decode the "deferred reference" mechanism in `FUN_0091A760` reader — figure out how the blob byte-ranges get registered with `CStructureSaver`, and **how the framework later resolves them to materialized records**. This is the chunk-stream-reference protocol — needs tracing through the framework's chunk handlers, possibly another `0x91Axxx` set of functions. — 4-8 hours
3. ⏭️ Implement port-side `CDBTableDataStorage` with matching wire layout (read all 8 fields, materialize records back into a hash_map) — 1-2 days
4. ⏭️ Patch `CDBTableBase::operator&` to delegate to a wrapped `CObj<CDBTableDataStorage>` instead of inline hash_map — half a day
5. ⏭️ Verify 155 records actually populate + `GetUIContainer(347)` / `GetDBCamera(26)` / `GetPers(54)` succeed — half a day

Total estimate: 3-5 dedicated sessions of Ghidra + port-side work.

## What the user can ship NOW (i.e. r12 itself)

Just the **knowledge artifact** — this document — plus the Ghidra installation. No code changes:

- Empty stub from r11 still silences 155 missing-class log entries
- 328 `BasicDB.h:102` cascade asserts remain (fallback dbg-text menu still drawn)
- Game state: still parked at intermission → mainmenu state machine entry, no real UI

## Reproducibility — for the next session

1. Open Ghidra → `C:\Users\Haohan\Documents\silent-storm\re\silent-storm-re` project
2. Load MapEdit (analysis already cached)
3. Jump to:
   - `0044D810` (field 4 handler) — first
   - `0044D360` (field 5 handler) — second
   - `0091A760` reader (the deferred-reference path) — most important for understanding the materialization protocol; look at what `FUN_00409620` does
4. Cross-reference `pSSClasses->CreateObject(0xA1843130)` callers — find where the framework actually USES the deserialized CDBTableDataStorage to populate `records`. That's where the materialization protocol lives.

## What's installed this session

- **JDK 21.0.11 LTS Temurin** at `C:\Program Files\Eclipse Adoptium\jdk-21.0.11.10-hotspot\` (system-level JAVA_HOME + PATH)
- **Ghidra 12.0.4 PUBLIC** at `C:\Tools\ghidra_12.0.4_PUBLIC\` (with `ghidraRun.bat` patched to force-set JAVA_HOME on launch — protects against stale env vars in cmd windows opened pre-JDK-install)
- Desktop shortcut: `Ghidra.lnk`
- RE project directory: `C:\Users\Haohan\Documents\silent-storm\re\silent-storm-re\`
- Quickstart guide: `port\docs\ghidra_quickstart.md`
