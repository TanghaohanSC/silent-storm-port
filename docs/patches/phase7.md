# Phase 7 — flatbuffers save game system (patch log)

**Date:** 2026-05-11
**Status:** Infrastructure DONE; mid-mission integration deferred until the
parallel renderer track lifts `silent_storm.exe` out of the Phase 1.5
intermission placeholder.
**Scope:** Add a Nival-independent save / load subsystem using a
flatbuffers schema as the on-disk format.  The wrapper exposes a flat
POD `GameState` struct to game code; once Nival's mid-mission state can
actually be reached from `silent_storm.exe`, Phase 7.5 will pack that
state into `GameState` and call our wrapper.

## What changed

### Iter 1 — Vendor flatbuffers via vcpkg

- Added `flatbuffers` to `port/vcpkg.json`'s dependency list.  vcpkg
  drops the runtime library and headers under
  `build/msvc-debug/vcpkg_installed/x86-windows/`, and the host-arch
  `flatc.exe` compiler under
  `build/msvc-debug/vcpkg_installed/x64-windows/tools/flatbuffers/`.
- `flatbuffers-config.cmake` exposes the `flatbuffers::flatbuffers`
  imported target; the new `ss_save` library links against it.

### Iter 2 — Author the schema

`port/src/save/save_v1.fbs` defines the v1 wire format:

- `Vec3 { x, y, z: float }` — leaf.
- `KeyValue { key, value: string }` — leaf, used by `Save::extras` to
  smuggle data we don't yet have first-class fields for.
- `Unit` — id / name / archetype id / position / rotation / hp+max /
  ap+max / side / inventory item ids / perks / state flags.
- `Mission` — id / map_path / turn_counter / active_side /
  objectives_complete / objectives_failed.
- `Save` (root) — `schema_version` (defaulted to 1), `timestamp_unix`,
  `slot_name`, `mission`, `units`, `rng_seed`, `extras`.

Top of file declares `file_identifier "SSV1"` and `file_extension "sav"`
so the buffer carries its own four-byte tag — the load path uses
`Verifier::VerifySizePrefixedBuffer<Save>(SaveIdentifier())` to refuse
anything that isn't ours before unpacking.

`namespace silent_storm.save.fb` (child namespace) is used by the
schema so the generated reader types — also named `Vec3`, `KeyValue`,
`Unit`, `Mission`, `Save` — don't collide with the POD mirror types
declared in `silent_storm::save` (`Vec3`, `KeyValue`, `UnitState`,
`MissionState`, `GameState`).  In the implementation TU `fbs::`
aliases the generated namespace.

Forward / backward compatibility is achieved the flatbuffers-standard
way: future schema versions must only **append** fields to the END of
existing tables (or add new tables), and each field has a sensible
default.  v1 readers reading v2 files transparently ignore the trailing
fields; v2 readers reading v1 files see the defaults.

### Iter 3 — Generate the C++ header

```powershell
$flatc = "build/msvc-debug/vcpkg_installed/x64-windows/tools/flatbuffers/flatc.exe"
& $flatc --cpp --scoped-enums -o src/save src/save/save_v1.fbs
```

The output `save_v1_generated.h` is committed alongside the schema as a
vendored artifact.  Tradeoff: every schema change is a manual
regenerate step, but the build doesn't depend on `flatc` being
discoverable at configure time (vcpkg only ships it under the host
triplet) and CI doesn't pay the codegen tax on every clean build.  A
banner at the top of `save_v1_generated.h` warns that the file is
machine-generated.

### Iter 4 — C++ wrapper

`port/src/save/save.{h,cpp}` exposes:

```cpp
namespace silent_storm::save {
    struct Vec3, UnitState, MissionState, KeyValue, GameState;
    bool save_to_file(const std::string& path, const GameState& s);
    bool load_from_file(const std::string& path, GameState& out);
    std::vector<uint8_t> serialize(const GameState& s);
    bool deserialize(const uint8_t* data, size_t size, GameState& out);
}
```

`GameState` mirrors the .fbs root table 1:1 with STL types so game code
can treat it as a plain struct.  `==` is defined on every leaf type
which makes round-trip equality assertions trivial.

`serialize()` writes the buffer in **size-prefixed** form
(`FlatBufferBuilder::FinishSizePrefixed`).  This bakes a 4-byte
little-endian total-size header before the root offset, leaving room
for a future container that concatenates several saves (autosave ring,
replay snapshots) into one file.  `deserialize()` reciprocates with
`GetSizePrefixedSave` after `VerifySizePrefixedBuffer`.

If the caller leaves `timestamp_unix == 0`, `serialize` auto-stamps the
current wall clock; if it's non-zero the caller's value is kept (used
by the deterministic round-trip test).

### Iter 5 — Nival integration

**Deferred.**  `iSaveManager.cpp` writes saves through `CStructureSaver`
on top of `CFileStream` and consumes the entire object graph
(`REGISTER_SAVELOAD_CLASS` chain rooted at the world / mission /
inventory subsystems).  Replacing it cleanly requires the parallel
renderer thread to first surface a runnable mid-mission scene from
`silent_storm.exe`; until that work lands there's no way to populate
`GameState` from a real game tick, and the only thing a "wire in now"
patch would do is force-stub every Nival save site at boot.

The scope guard in the Phase 7 plan explicitly authorises this defer.
Concrete next step (Phase 7.5):

1. In `iSaveManager::SaveSlot`, gate the existing Nival path under
   `#ifndef SS_USE_FLATBUFFERS_SAVE`, add an `#else` branch that
   builds a `silent_storm::save::GameState` by walking the in-memory
   world and calls `save_to_file(GetSlotFilePath(name, "game.sav"), s)`.
2. Mirror in `iSaveManager::LoadSlot`.
3. Add `target_compile_definitions(jan03_Main PRIVATE
   SS_USE_FLATBUFFERS_SAVE)` once the gate is built.

### Iter 6 — Smoke test

`port/tests/save/test_save_roundtrip.cpp` (target `ss_save_test`):

- `test_in_memory_roundtrip` — full-fidelity fixture (2 units, all
  optional vectors populated, both empty and non-empty strings).  Calls
  `serialize`, then `deserialize`, asserts `==`.  Verifies the
  size-prefix header by re-reading the first 4 bytes.
- `test_file_roundtrip` — `save_to_file` -> `load_from_file` against
  the same fixture, then `std::remove`.
- `test_rejects_garbage` — nullptr/0-size, random bytes, truncated real
  buffer, missing-file `load_from_file`.  All must return false without
  trashing `out_state`.
- `test_empty_state` — default-constructed `GameState`, confirms
  defaults survive round-trip (`schema_version` lands back at 1).

Exits 0 on success.

### Iter 7 — Build wiring

- New `port/src/save/CMakeLists.txt` declares the `ss_save` STATIC
  library and the `ss_save_test` exe.
- `port/CMakeLists.txt` adds a single line —
  `add_subdirectory(src/save)` — alongside the other phase subdirs.
- No existing target is modified.

## Files

### New
- `port/src/save/save_v1.fbs` — wire schema.
- `port/src/save/save_v1_generated.h` — vendored flatc output.
- `port/src/save/save.h` — public API + POD mirror types.
- `port/src/save/save.cpp` — pack / unpack + file I/O.
- `port/src/save/CMakeLists.txt` — `ss_save` lib + `ss_save_test` exe.
- `port/tests/save/test_save_roundtrip.cpp` — Phase 7 acceptance.

### Modified
- `port/vcpkg.json` — add `flatbuffers`.
- `port/CMakeLists.txt` — `add_subdirectory(src/save)`.

## Carryovers to Phase 7.5 / Phase 8

1. **Nival call-site wiring.**  See Iter 5 plan above.  Needs the
   mid-mission boot path from the renderer thread.
2. **Screenshot in save header.**  Nival's `SSaveFileHeader` carries a
   320x200 BGRA8 thumbnail.  Phase 7.5 can stash the framebuffer
   readback into `extras` as base64 (or add a dedicated `Vector<uint8>`
   field — schema-additive so v1 readers still cope).
3. **Autosave ring.**  The size-prefixed wire format means we can
   concat N saves into one file; an autosave subsystem can keep the
   last 3 turns in a single `autosave.sav` without parser changes.
4. **Slot directory layout.**  Current `save_to_file` takes a literal
   path.  Nival's `CSaveManager::GetSlotFilePath` builds a per-profile
   per-slot directory tree; Phase 7.5 will plug those paths in directly
   — no API change needed on this side.
