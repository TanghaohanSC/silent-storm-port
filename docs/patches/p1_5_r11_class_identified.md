---
name: p1_5_r11_class_identified
description: Phase 1.5 r11 — reverse-engineered missing typeID 0xA1843130 = CDBTableDataStorage via MapEdit.exe disasm + RTTI + vtable walk
---

# Phase 1.5 r11 — typeID `0xA1843130` identified

**Date:** 2026-05-12
**Status:** **class identified, partial wire layout recovered**
**Previous:** r10 documented the IDA/Ghidra blueprint
**Method:** `dumpbin /disasm:nobytes MapEdit.exe` + RTTI string lookup + vtable walk

## TL;DR

`0xA1843130` is **`CDBTableDataStorage`** — a custom hash-map-like backing
store for `CDBTableBase`. The Jan03 open-source drop has `CDBTableBase` keep
its records hash_map **inline**; the shipping binary refactored it to wrap the
records in a heap-allocated `CDBTableDataStorage` (registered as a serializable
polymorphic CObj). The class itself is NOT in the source — only the consumer
shape is.

## Disassembly trail (concrete file offsets in MapEdit.exe)

### Step 1 — find `0xA1843130` writes in disasm

```
00A2E2DE: mov dword ptr [esp+0Ch], 0A1843130h    ; key1 setup on stack
00A2E2F3: mov dword ptr [eax],     0A1843130h    ; write into find-result
```

Both inside one function at `0x00A2E2C0`. The function does TWO `call 0x004111C0` lookups, writes `0xA1843130` to the first result, and `0x0044B310` to the second.

### Step 2 — `0x004111C0` IS `HashMap::find`

Disasm shows:
```
mov esi, [eax]          ; hash key = first dword of arg
mov eax, [ecx+4]; mov edi, [ecx+8]; sub edi, eax; sar edi, 2
div eax, edi             ; hash % bucket_count
mov edx, [eax+edx*4]     ; fetch bucket head
```

So `0x00A2E2C0` is `RegisterClass(0xB341C4 → 0xA1843130, factory=0x0044B310)`. Two parallel registries — name→typeID and typeID→factory.

### Step 3 — companion key `0xB341C4` is the RTTI string

```
file 0x00731BC4 (.data VA 0x00B341C4): 68 E4 A8 00 00 00 00 00 2E 3F 41 56 43 44 42 54 ...
                                       ↑ vtable ptr            ↑ ".?AVCDBT...
                                                                "@" marker
```

ASCII: `.?AVCDBTableDataStorage@` — MSVC RTTI decorated class name. The `.?AV` prefix marks "class type_info".

### Step 4 — factory function at `0x0044B310`

```
push 0FFFFFFFFh; push 9D3B8Bh; ...      ; SEH preamble
push 88h                                  ; sizeof = 0x88 = 136 bytes
call 00958DC2                             ; operator new
mov ecx, eax; call 0044AE20               ; constructor
ret
```

So `sizeof(CDBTableDataStorage) == 136 bytes`, ctor at `0x0044AE20`.

### Step 5 — constructor at `0x0044AE20`

```
mov [esi],     0A57F7Ch         ; vtable address
xor eax, eax
mov [esi+04h], eax              ; \
mov [esi+08h], eax              ;  > zero-init [esi+4 .. esi+70]
... (27 dwords zeroed) ...
mov [esi+70h], eax              ; /
lea ecx, [esi+74h]              ; subobject at +0x74
mov [ecx+4], eax; mov [ecx+8], eax; mov [ecx+0Ch], eax; mov [ecx+10h], eax
push 0Ah; call 00478F10         ; init subobject (some std::list/vector reserve(10))
```

In-memory layout (136 bytes):
- `[+0x00]` vtable pointer (→ `0x00A57F7C`)
- `[+0x04..+0x08]` CObjectBase data (nObjData, nRefData)
- `[+0x0C..+0x70]` ~25 dwords of state — runtime indexes / cached structs
- `[+0x60]` element count (mutated by vtable method `inc [ecx+60h]`)
- `[+0x74..+0x84]` 16-byte subobject (load-factor / free-list helper)

### Step 6 — vtable at `0x00A57F7C` (16 slots)

| Slot | Addr | Identified role |
|---|---|---|
| 0 | `0x0044B630` | vector deleting destructor |
| 1 | `0x0044B370` | MakeCopy() — allocs sizeof=0x88 + copies |
| 2 | `0x0044AF20` | scalar deleting destructor |
| **3** | **`0x0044CA30`** | **`operator&(CStructureSaver&)` — the serialize method** |
| 4 | `0x00448EB0` | DestroyContents thunk |
| 5 | `0x00401000` | CRT base thunk |
| 6 | `0x004494D0` | (other CObjectBase virtual) |
| 7 | `0x0044AED0` | Refresh-ish (inc count at +0x60) |
| 8 | `0x0044AEF0` | IsFull / NeedsResize (load-factor check) |
| 9 | `0x0044A010` | hash-map entry getter (idx*3*4 stride) |
| 10 | `0x00449210` | hash-map find variant |
| 11..15 | various | mutation/iteration helpers |

### Step 7 — wire layout (the IMPORTANT part)

`vtable[3]` at `0x0044CA30` dispatches **8 conditional field reads/writes**:

| Wire field idx | Member offset | Sub-handler |
|---|---|---|
| 2 | `this+0x0C` (12 bytes) | `0x0044CB30` — std::vector-of-12-byte-entries serializer (uses `2AAAAAAB` divide-by-12) |
| 3 | `this+0x18` (12 bytes) | `0x0044CC90` |
| 4 | `this+0x24` (12 bytes) | `0x0044D810` |
| 5 | `this+0x30` (12 bytes) | `0x0044D360` |
| 6 | `this+0x3C` (12 bytes) | `0x0044CEC0` |
| 7 | `this+0x48` (12 bytes) | `0x0044CEC0` (shared) |
| 8 | `this+0x54` (12 bytes) | `0x0044CEC0` (shared) |

Eight `std::vector<T>` triples (begin/end/end_of_storage) at +0x0C, +0x18, +0x24, +0x30, +0x3C, +0x48, +0x54.

`0x0044CB30` deserialize loop reads field 1 = count, then fields 1..count = entries (each 12 bytes). 0x44CEC0 is used three times → shared sub-vector schema. So the class holds:

- 1× bucket vector (field 2) — the hash-map buckets
- 1× "main" record vector (field 3)
- 1× field-4 vector (handler 0x44D810 — special, maybe ID index)
- 1× field-5 vector
- 3× field-6/7/8 vectors with shared schema (probably secondary indexes by foreign key, name, etc.)

**Conclusion:** `CDBTableDataStorage` is a richly-indexed table. Not a simple hash_map wrapper. Each handler has its own deserialize routine and the 12-byte tuples likely encode `<int_key, CObj<CDBRecord>, extra>` triples.

## Why the open-source code can't read this wire format

Jan03 `CDBTableBase::operator&` does `f.Add(1, &records)` where `records` is `hash_map<int, CObj<CDBRecord>>`. The shipping wire for each CDBTableBase has field 1 = a polymorphic `CObj<CDBTableDataStorage>` reference (the wrapped storage object). The hash_map deserializer reads bogus bytes → either corrupt records or framework desync.

**BUT** the 155 `CDBTableDataStorage` instances live in the object pool independently — they're referenced by `pServer` pointers from the wire's metadata stream, not through `CDBTableBase`. Registering a stub class will allow them to instantiate and their bodies to skip cleanly via `FinishChunk()`, eliminating the 155 missing-class log lines.

## What this session delivered

- ✅ Identified `0xA1843130 = CDBTableDataStorage`
- ✅ Recovered size, vtable, ctor, factory, operator& addresses
- ✅ Mapped 8-field wire layout (offsets + handlers)
- ⏭️ DEFERRED: re-implementing the 5+ unique sub-handlers (0x44CB30, 0x44CC90, 0x44D810, 0x44D360, 0x44CEC0) needs IDA/Ghidra
- ⏭️ DEFERRED: patching `CDBTableBase::operator&` to delegate to a wrapped `CObj<CDBTableDataStorage>`

## Next-session blueprint (when picking this back up)

1. Open MapEdit.exe in IDA / Ghidra (no unpack)
2. Jump to `0x0044CB30`, `0x0044CC90`, `0x0044D810`, `0x0044D360`, `0x0044CEC0` — write decompiler output to text
3. Identify the 12-byte element types (probably `std::pair<int, CObj<X>>` for the bucket vector; index-tuples for the others)
4. Implement `class CDBTableDataStorage : public CObjectBase` in port with matching wire schema
5. Patch `CDBTableBase::operator&` to read `CObj<CDBTableDataStorage>` instead of inline hash_map; have `GetDBRecord(id)` delegate through the storage object

After all that: the 155 missing instances populate, the 328 cascade asserts at `BasicDB.h:102` resolve, `GetUIContainer(347)` / `GetDBCamera(26)` / `GetPers(54)` succeed, and the real Nival main menu renders.

## Empty-stub experiment — confirmed

Shipped `port/src/stubs/db_table_storage_stub.cpp` with `class CDBTableDataStorage : public CObjectBase` + empty `operator&` + `REGISTER_SAVELOAD_CLASS(0xA1843130, ...)`. Linked into silent_storm.exe with `/WHOLEARCHIVE:silent_storm_dbts_stub`. Result on running against upstream/Complete/game.db:

- ✅ **`silent_storm_missing_classes.log` not produced** — zero missing-class typeID errors (was 155 before this round)
- ✅ **exe runs stably** through the main loop (winmain trace shows `StepApp ret=1` steady-state)
- ✅ **No new crashes** from registering the class
- ⚠️ **328 `BasicDB.h:102` asserts unchanged** — empty `operator&` means records hash_map still empty downstream; UIContainer 347 / DBCamera 26 / Pers 54 still null; r8 fallback menu still draws
- ✅ **CStructureSaver framework's chunk auto-skip hypothesis confirmed** — body bytes for the 155 objects flow past `FinishChunk()` without consumption desync

So the missing-class log noise is eliminated. Real fix (auto-populating records from the wire) still needs the deferred IDA/Ghidra work above.

## Files touched this round

- `port/docs/patches/p1_5_r11_class_identified.md` (this file)
- `port/src/stubs/db_table_storage_stub.cpp` (new — empty stub, REGISTER_SAVELOAD_CLASS)
- `port/src/stubs/CMakeLists.txt` (+silent_storm_dbts_stub static lib target)
- `port/CMakeLists.txt` (jan03_Main + silent_storm link silent_storm_dbts_stub; /WHOLEARCHIVE:silent_storm_dbts_stub)
- `port/build/mapedit_disasm.txt` (79 MB dumpbin /disasm output — not committed, regen via `dumpbin /disasm:nobytes MapEdit.exe`)
