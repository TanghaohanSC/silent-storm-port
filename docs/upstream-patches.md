# Upstream patches log

This file tracks every modification we make to `upstream/Soft/` files
(originally read-only vendored upstream). Each entry includes: file,
line, reason, and date.

The goal is to keep upstream patches *minimal* — only what's needed to
get modern toolchains to accept the legacy code. Big rewrites belong in
`port/src/` (the modern code we author).

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
