# Silent Storm Port

Open-source modernization of the 2003 Nival Interactive game Silent Storm,
based on the 2026 source release. See `../docs/superpowers/specs/` for design.

**Status:** Phase 7 infrastructure COMPLETE (Phase 3 and earlier all
green). `flatbuffers` is now linked via vcpkg; `port/src/save/save_v1.fbs`
defines the v1 wire schema (root `Save`, file identifier `"SSV1"`); the
C++ wrapper `silent_storm::save::{save,load}_to_file` packs a flat
`GameState` POD through the generated builder and writes a
size-prefixed buffer to disk. Verified with the new `ss_save_test.exe`
harness — in-memory round-trip, on-disk round-trip, garbage rejection,
empty-state round-trip all pass. `silent_storm.exe` continues to boot
into the Phase 1.5 intermission screen with the Phase 2 miniaudio
backend live and the Phase 3 video player available as a sibling exe
(Phase 7, like Phase 3, is a pure addition — no existing call site is
touched). See `docs/phase-7-completion.md` and `docs/patches/phase7.md`.
Next: wire `save_to_file` / `load_from_file` into `iSaveManager::SaveSlot`
/ `LoadSlot` once the parallel renderer track surfaces a runnable
mid-mission scene from `silent_storm.exe` (Phase 7.5).
