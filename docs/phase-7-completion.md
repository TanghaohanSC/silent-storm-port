# Phase 7 Completion Report

**Status: INFRASTRUCTURE COMPLETE**

**Phase 7 code complete:** 2026-05-11
**Branch:** main
**Outcome:** A working flatbuffers-backed save / load subsystem now
ships with the port. `flatbuffers` is pulled in through vcpkg; the
schema `save_v1.fbs` (root table `Save`, file identifier `"SSV1"`) is
the on-disk wire format. The C++ wrapper exposes a flat
`silent_storm::save::GameState` POD that mirrors the schema 1:1, plus
four public entry points: `serialize`, `deserialize`, `save_to_file`,
`load_from_file`. A new test exe `ss_save_test.exe` exercises the
round-trip end to end (in-memory, on-disk, garbage rejection, empty
state). `silent_storm.exe` continues to boot into the Phase 1.5
intermission screen — Phase 7 is a pure *addition* and does not touch
any pre-existing call site.

The Phase 7 acceptance from the spec ("任务中存档、退到主菜单、读档、状态完全恢复")
is gated on the parallel renderer track surfacing a runnable
mid-mission scene from `silent_storm.exe`. Until that scene exists,
there is no production state to pack into `GameState`; the round-trip
test in `ss_save_test` covers the only piece of Phase 7 that can be
verified today. Patch log `docs/patches/phase7.md` ends with a
concrete plan for the Phase 7.5 follow-up that wires our wrapper into
`iSaveManager::SaveSlot` / `LoadSlot` under `SS_USE_FLATBUFFERS_SAVE`.

---

## What shipped

- `port/vcpkg.json` adds `flatbuffers`. vcpkg drops the headers under
  the target triplet and `flatc.exe` under the host triplet
  (`x64-windows/tools/flatbuffers/flatc.exe`).
- `port/src/save/save_v1.fbs` — wire schema. Five tables:
  `Vec3`, `KeyValue`, `Unit`, `Mission`, `Save`. Root is `Save`,
  carrying `schema_version` (defaulted to 1), `timestamp_unix`,
  `slot_name`, `mission`, `units`, `rng_seed`, and an `extras` bag of
  string key/value pairs for fields v2 hasn't claimed yet. Top of file
  declares `file_identifier "SSV1"` so the buffer self-tags.
- `port/src/save/save_v1_generated.h` — `flatc --cpp --scoped-enums`
  output, committed alongside the schema. ~665 lines of builder/getter
  helpers in `namespace silent_storm::save`.
- `port/src/save/save.h` / `save.cpp` — public API + POD mirror types
  + pack/unpack helpers. Buffer is written in **size-prefixed** form
  via `FinishSizePrefixed`, which puts a 4-byte LE total-size header
  in front of the root offset (lets us concat saves into one file
  later for autosave rings).
- `port/src/save/CMakeLists.txt` declares the `ss_save` STATIC library
  (linked against `flatbuffers::flatbuffers` via
  `find_package(flatbuffers CONFIG REQUIRED)`) and the `ss_save_test`
  executable.
- `port/tests/save/test_save_roundtrip.cpp` — Phase 7 acceptance
  harness. Four sub-tests: in-memory round-trip, on-disk round-trip,
  garbage rejection (nullptr, random bytes, truncated buffer, missing
  file), empty-state round-trip. Exits 0 on success.
- `port/CMakeLists.txt` gets one new line:
  `add_subdirectory(src/save)`. No other top-level target is
  modified.

---

## How to verify

```powershell
cd C:\Users\Haohan\Documents\silent-storm\port
cmake --preset msvc-debug
cmake --build --preset msvc-debug --target ss_save_test
build\msvc-debug\bin\Debug\ss_save_test.exe
```

Expected output:

```
Phase 7 save round-trip test
  in-memory round-trip OK  (XXX bytes)
  on-disk round-trip OK  (test_phase7.sav)
  garbage rejection OK
  empty-state round-trip OK
ALL PASS
```

Exit code: 0.

---

## Schema design notes

### Why flatbuffers
1. Schema evolution is free as long as new fields are appended to the
   END of existing tables. v2 readers transparently see v1 files
   (defaults), v1 readers transparently see v2 files (trailing fields
   ignored). This is exactly the property the spec calls out in §3.7
   ("schema 可演化，新版本兼容老存档").
2. Zero-copy reads: `GetSizePrefixedSave(buffer)` returns a typed
   pointer into the buffer directly. No allocation, no parsing pass.
3. Trustable load path: `Verifier::VerifySizePrefixedBuffer` walks the
   buffer before unpacking and refuses anything malformed,
   short-circuiting on bad offsets / overflowing vectors before we
   ever touch a string. Critical because save files are user input.
4. Apache-2.0 (per inventory, compatible with our LGPL-and-friends
   dependency mix).

### Why a POD mirror type
The generated `silent_storm::save::Save` is a read-only view into a
flatbuffer; the matching `SaveT` builder helper is hideously
inefficient for incremental field updates. Game code wants a regular
struct it can mutate by hand and pass to save/load. The wrapper does
the SaveT-style packing internally so callers never see flatbuffers
types.

### Why size-prefixed
Modern saves should embed their own length. It lets a future autosave
subsystem concatenate snapshots into one file (`autosave.sav` keeps
the last N turns), it lets tooling skip a corrupt save and continue,
and it costs 4 bytes per save.

### Why vendored generated header (vs codegen step)
- `flatc.exe` only ships under the host triplet
  (`x64-windows/tools/flatbuffers/`) in our vcpkg, not the target
  triplet (`x86-windows/`). Discovering it at configure time would
  mean a manual `find_program` plus a `CMAKE_CROSSCOMPILING_EMULATOR`
  dance.
- Clean builds don't pay codegen tax.
- Schema changes are rare (this is a v1 frozen format). The cost of
  remembering to re-run `flatc` on a schema edit is acceptable.
- A banner at the top of `save_v1_generated.h` warns that the file is
  machine-generated.

---

## What's deferred to Phase 7.5

Wiring our wrapper into Nival's `iSaveManager::SaveSlot` /
`LoadSlot` flow. The Nival save path is structurally
`CStructureSaver` walking the whole object graph via
`REGISTER_SAVELOAD_CLASS`; replacing it requires the parallel
renderer track to first put us into a runnable mid-mission scene from
`silent_storm.exe`. The patch log lists the four-step plan once that
scene exists; it's a ~50-LOC change behind
`#ifdef SS_USE_FLATBUFFERS_SAVE` in two functions.

---

## Files added / modified

### Added
- `port/src/save/save_v1.fbs`
- `port/src/save/save_v1_generated.h` (vendored from flatc output)
- `port/src/save/save.h`
- `port/src/save/save.cpp`
- `port/src/save/CMakeLists.txt`
- `port/tests/save/test_save_roundtrip.cpp`
- `port/docs/patches/phase7.md`
- `port/docs/phase-7-completion.md`

### Modified
- `port/vcpkg.json` — `+ "flatbuffers"`
- `port/CMakeLists.txt` — `+ add_subdirectory(src/save)`
- `port/README.md` — status bumped to Phase 7
