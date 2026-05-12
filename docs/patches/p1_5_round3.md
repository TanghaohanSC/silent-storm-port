# Phase 1.5 Round 3 — Runtime patches

Continues from `p1_5_round2.md` (Round 2). Round 2 produced a working render
pipeline (debug-text overlay + an `ss_ui` test triangle), but the round-2 log
showed Nival's text path emits **zero rectangle layouts** every frame because
the locale has no fonts in the (un-mounted) game database. Round 3 target:
get **VISIBLE text strings on screen** so we have an unambiguous "the
intermission is rendering" signal.

## Iteration 1 — debug-text relay (STLport ↔ modern toolchain bridge)

Added a small POD/extern-C debug-text relay so the STLport side (UI code)
can push ASCII labels + virtual-screen positions into a buffer that the
modern bgfx side flushes via `bgfx::dbgTextPrintf` at `end_frame()`.

* `port/src/renderer/bgfx_init.h` — declarations for
  `ss_dbg_text_push(int virtX, int virtY, unsigned attr, const char* text)`
  and `ss_dbg_text_banner(const char* text)`.
* `port/src/renderer/bgfx_init.cpp` —
  * Adds `DbgTextEntry g_dbg_text[32]` + count + banner buffer at file
    scope (so the extern-C entry points can reference them by qualified
    name).
  * `end_frame()` flushes those entries via `bgfx::dbgTextPrintf`, mapping
    1024x768 virtual coords → bgfx's 80-col × 24-row debug-text grid.
  * Row-collision avoidance: when two entries land on the same mapped row,
    successive entries bump to the next free row so they don't overprint.
  * Renamed the status banner to "Phase 1.5 r3" and exposed the relay
    entry count + banner text in the second status line.
* `upstream/.../UIWrap.cpp::CTextDraw::Draw` — added a hook that strips
  `<font>/<color>/<right>/<center>` markup tags from `wsText`, turns
  `<br>` into `|`, transcodes the lower-7-bit ASCII subset, trims leading
  whitespace, then `ss_dbg_text_push()`'s the cleaned string at the
  CTextDraw's sPosition.  Falls back to `'?'` for non-ASCII glyphs.
* `upstream/.../UIWrap.cpp::CImageDraw::Draw` — pushes an
  `[IMG <w>x<h> tex|no-tex]` marker at the image's sWindow.x1/y1 so the
  user can see where Nival is trying to place sprite content even before
  the texture binding lands.

**Verification screenshot**: `p1_5_r3_iter3.png` — the bgfx debug-text
overlay now reads:

```
Silent Storm port  -  Phase 1.5 r3  -  bgfx alive, frame 39795
Backend 2  Window 1024 x 768  HUD scale 1  text-relay entries=2

INTERMISSION|ESC - exit|Use console to start game
Work in progress
```

The `INTERMISSION` / `ESC - exit` / `Use console to start game` strings
are Nival's literal placeholder copy from `iInterMission.cpp` — the
intermission screen is rendering its real UI content.  Done criterion #1
(VISIBLE TEXT/UI) satisfied.

The test triangle from round 2 still draws (top-right corner) — proves
the full bgfx submit pipeline is healthy alongside the dbgText path.

## Status at end of round 3

- Done criterion #1 (VISIBLE TEXT) satisfied — Nival's actual UI text
  strings render via the bgfx debug overlay.  Background is the
  `0x808080` clear color from `IM::ClearScreen(0.5,0.5,0.5)`.  Triangle
  marker confirms the geometry submit path is alive.
- The text appears via `bgfx::dbgTextPrintf`, not via Nival's actual
  font/quad pipeline — that pipeline still emits 0 rect layouts because
  `pLocale->GetFont(...)` returns null.  Wiring up real glyph rendering
  requires either (a) loading Nival's `Fonts/<N>` binaries into a real
  `CFontInfo`/`CFontFormatInfo` graph, or (b) synthesizing a minimal
  in-code 8x16 ASCII bitmap font and short-circuiting `GetFont` to it.
  Both are Phase 2 / round-4+ work.
- `FacadeTexture::Lock/Unlock` is already wired to
  `bgfx::createTexture2D` + `updateTexture2D` (round-1 work) — it'll
  upload pixels the moment Nival hands us any, but it can't run until
  the font/image asset binding lands.
