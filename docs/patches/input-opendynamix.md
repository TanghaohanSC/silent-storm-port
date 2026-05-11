# Input + OpenDynamix Patches Log

## Phase 0 Task 6 — May 2026

### Scope
- `upstream/Soft/Andy/Jan03/a5dll/Input/`
- `upstream/Soft/Andy/Jan03/a5dll/OpenDynamix/`

### Result: No patches required

Both targets compiled and linked clean on the first build attempt with the
existing CMake setup. No source modifications were necessary.

**Build verification:**
- `cmake --build --preset msvc-debug --target jan03_Input` → exit 0
  - Compiled: StdAfx.cpp, Input.cpp, Bind.cpp → `lib/Input.lib`
- `cmake --build --preset msvc-debug --target jan03_OpenDynamix` → exit 0
  - Compiled all 23 translation units (array, error, fastdot, fastldlt,
    fastlsolve, fastltsolve, geom, joint, lcp, mass, mat, matrix, memory,
    misc, obstack, ode, odemath, rotation, space, step, testing, timer,
    stdafx) → `lib/OpenDynamix.lib`

### Notes
- Input uses DirectInput8 (system `dinput8.lib`); the CMake target links it
  via `jan03::Misc` chain — no stub required, no manual linkage change needed.
- OpenDynamix (23 cpp files, physics library) compiled cleanly under `/Arch:IA32`
  (x86) mode — no inline asm issues encountered, no template instantiation
  failures, no intrinsic problems.
- The `/Arch x86` build flag handles any x86-specific code paths correctly.
