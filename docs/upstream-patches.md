# Upstream patches log

This file tracks every modification we make to `upstream/Soft/` files
(originally read-only vendored upstream). Each entry includes: file,
line, reason, and date.

The goal is to keep upstream patches *minimal* — only what's needed to
get modern toolchains to accept the legacy code. Big rewrites belong in
`port/src/` (the modern code we author).

---

## 2026-05-11 — `MemoryMngr/DumbPow2Alloc.h` defer to std `<new>`

**File:** `upstream/Soft/Andy/Jan03/a5dll/MemoryMngr/DumbPow2Alloc.h`

**Change:** Replaced the entire body. Original was a "fake `<new>`":
- Guarded with `_NEW_`/`_INC_NEW` to suppress the real `<new>` include
- Redefined `std::bad_alloc`, `std::nothrow_t`, `std::new_handler`
- Declared `operator new` / `operator new[]` / placement new

New version: `#pragma once`, `#include <new>`, `#include "malloc.h"`. The
operator new/delete *replacement* mechanism (defined in `DumbPow2Alloc.cpp`)
still works under modern C++ — replaceable allocation functions are an ABI
feature, no header trickery needed.

**Reason:** Modern MSVC `vcruntime_new.h` uses different include guards
than the legacy `<new>` header, so the `_NEW_` guard trick didn't prevent
vcruntime from defining `std::nothrow_t`/`std::bad_alloc`/`operator new`.
Both definitions ended up active, producing C2011 (nothrow_t struct
redefined), C2084 (operator new has a body), C3615 (constexpr operator
new can't), C2504 (std::bad_alloc base undefined).

---

## 2026-05-11 — `Misc/tools.h` math `noexcept`

**Files:** `upstream/Soft/Andy/Jan03/a5dll/Misc/tools.h` lines 627, 695-698

**Change:** Commented out (kept as inline comments for traceability) Nival's
inline float overloads:
- `fabs(float)`, `cos(float)`, `sin(float)`, `acos(float)`, `asin(float)`

**Reason:** Modern MSVC `<cmath>` provides `float fabs(float) noexcept`
etc. **with inline bodies**. Nival's tools.h provides duplicate inline
definitions, triggering C2084 ("function already has a body"). Adding
`noexcept` to Nival's versions fixed the spec mismatch (C2382) but
created the body collision (C2084).

Commenting out is safe: every call site `fabs(myFloat)` resolves to
std's identical implementation. `cos(float)` callers similarly fall back
to std's float overload (which already specializes for float, no
double promotion). Behavior is byte-identical.

---

## 2026-05-12 — Main + Game subprojects

See `docs/patches/main-game.md` for the full per-file patch list. Summary:

- ~40 upstream files patched (Misc/, Main/, Game/, MemoryMngr/, ADOImport/, DBFormat/).
- 196 `if (CDynamicCast<T> v(arg))` sites refactored via script.
- 104 `else if (CDynamicCast<T> v(arg))` sites refactored via script.
- 30+ `.insert(.end())` → `.emplace(.end())` (modern STL).
- `Misc/Basic2.h` gains non-template global `operator==`/`!=` for CPtr/CObj/CMObj — fixes 50+ C2666 ambiguities.
- `Misc/EventsBase.h` caches typeid pointer at ctor to allow forward-decl TParam in dtor.
- `Main/StdAfx.h` pulls all NDb data headers into PCH (MSVC eagerly instantiates CDBPtr<T>::operator&).
- `MemoryMngr/DumbPow2Alloc.cpp` switches from MemoryMngrDll's FastDumbAlloc to stdlib malloc/free.
- `Main/Interface.cpp` excises dead `CMouseCaptureHandler` referencing nonexistent IWindow/IInterface.
- `port/third_party/stlport_shim/legacy_compat.h` includes `<../ucrt/time.h>` to bypass Main/Time.h shadow.

---

## 2026-05-12 — r31 main-menu null-guard suite (boot reaches steady state)

- `Main/iRenderWorld.cpp` null-guards in CRenderBaseInterface::OnGetFocus / ProcessEvent / Step. When MainMenuInterface skips parent::Initialize (DB cameras/world missing), pRender / pRenderSound / pCursor / pInterface / pCamera / pScene stay null. Each was being unconditionally dereferenced, triggering a `dynamic_cast` SEGV deep inside CastToUserObjectImpl (the EAX=0 vtable read seen in r30).
- `Main/iMain.cpp` finer trace points (StepApp.5 input handled / .6 LoadPrecached ok / .7 Step ok).
- Result: game reaches main-loop steady state — `22.N.c StepApp ret=1` for arbitrary N, no crash, fallback main menu painted via `ss_r8_render_fallback_menu`. This is the OpenMW-style "main menu boots and stays up" milestone.
