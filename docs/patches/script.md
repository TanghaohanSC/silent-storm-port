# Script subproject — Phase 0 patch log

Subproject path: `upstream/Soft/Andy/Jan03/a5dll/Script/`

## Result

**No patches required.** The Script subproject (20 .cpp files containing a modified Lua 4 VM
with cooperative threading extensions) compiled cleanly on the first attempt.

```
cmake --build --preset msvc-debug --target jan03_Script
```

Exit code: **0**

## Build output summary

All 21 compilation units compiled without error:

- `StdAfx.cpp` (PCH)
- `lapi.cpp`, `lcode.cpp`, `ldebug.cpp`, `ldo.cpp`, `lfunc.cpp`, `lgc.cpp`
- `llex.cpp`, `lmem.cpp`, `lobject.cpp`, `lparser.cpp`, `lsaver.cpp`
- `lstate.cpp`, `lstring.cpp`, `ltable.cpp`, `ltm.cpp`, `lundump.cpp`
- `lvm.cpp`, `lzio.cpp`
- `Script.cpp` (Nival wrapper + thread API extensions)

Linked cleanly to `Script.lib`.

## Why no patches were needed

The existing `VendoredJan03.cmake` configuration already handles the compatibility
issues for C-era Lua 4 code compiled under modern MSVC:

- `/permissive` — allows legacy non-conformant C-style constructs
- `/W0` — silences all warnings (legacy code, no actionable fixes needed)
- `/Zc:noexceptTypes-` and `/Zc:ternary-` — legacy C++ behavior flags
- `_HAS_CXX17=0` / `_HAS_CXX20=0` — prevents `noexcept` function type conflicts
- `legacy_compat.h` force-include via `/FI` — STLport compatibility shim
- `CXX_STANDARD 14` pinned per-target — avoids C++17 breaking changes

The `legacy::stlport` link dependency provides the STLport headers that the
Nival codebase was originally built against.
