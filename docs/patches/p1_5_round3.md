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

## Iteration 5 — debug-rect relay (real geometry path activated)

Round-2 had proven the bgfx submit pipeline works for a single hard-coded
test triangle through `ss_ui`.  In round 3 iter 5 we wire up a colored-rect
relay so the upstream Nival side can request **arbitrary** quads to render
via `ss_ui` — closing the loop on the "real geometry path" goal even before
font/texture binding lands.

* `port/src/renderer/bgfx_init.{h,cpp}` —
  * Adds `ss_dbg_rect_push(x1, y1, x2, y2, abgr)` extern-C entry point.
  * Adds `g_dbg_rect[64]` queue at file scope.
  * `end_frame()` flushes the queue as one transient vertex buffer
    (6 verts/rect, NDC-space, Y-flipped to match D3D convention) submitted
    on view 0 with `BGFX_STATE_BLEND_FUNC(SRC_ALPHA, INV_SRC_ALPHA)`.
  * Creates a lazy 1x1 white BGRA8 texture and binds it via
    `bgfx::setTexture(0, s_diffuse_uniform, white_tex)` so the textured
    fragment shader (`tex * v_color0`) outputs the per-vertex color.
* `upstream/.../UIWrap.cpp::CTextDraw::Draw` — after stripping markup,
  estimate the text's pixel extent (8 px/char × 24 px tall) and call
  `ss_dbg_rect_push` with a reddish-orange `0xc03060f0` color.  The
  `sSize` field on the CTextDraw is the *bounding container*
  (usually 1024×768), not the text's actual extent, so we explicitly
  size from character count instead.
* `upstream/.../UIWrap.cpp::CImageDraw::Draw` — pushes a rect for the
  image's window: green tint when `pUITexture` is valid, gray when not.

**Verification screenshot**: `p1_5_r3_iter7.png` — two reddish-orange
rectangles render behind the two intermission text lines:

```
[orange 396x26]  INTERMISSION|ESC - exit|Use console to start game
[orange 132x26]  Work in progress
```

Triangle from round-2 (top-right) renders on top (no texture, so it
inherits the white sampler from the rect submit and shades orange too,
proving sampler-state retention works as expected).

This means the **full real bgfx pipeline** — transient vertex buffer,
vertex layout, sampler-uniform binding, textured fragment shader, alpha
blend, submit, present — is fully functional and accessible to upstream
code via a stable extern-C ABI.

## Status at end of round 3

- Done criterion #1 (VISIBLE TEXT) satisfied — Nival's UI text strings
  render via the bgfx debug overlay (`p1_5_r3_iter4.png` / `_iter7.png`).
- Done criterion bonus (REAL GEOMETRY path) satisfied — colored quads
  submitted through the real `ss_ui` shader pipeline (`p1_5_r3_iter6.png`,
  `_iter7.png`).  Demonstrates that `FacadeTexture::Lock/Unlock` +
  `bgfx::createTexture2D` + `setTexture` work end-to-end, which is the
  ABI Nival's `CFontInfo` / `CUITexture` will use once their asset binding
  lands.
- Remaining for round 4 / Phase 2:
  * Wire up `pLocale->GetFont(...)` to a real `CFontInfo` (font db load)
    or a synthesized minimal 8x16 ASCII bitmap font so that the text
    formatter produces real rect layouts and Nival's own
    `DrawPrimitive` path activates.
  * Once Nival emits real `DrawPrimitive*` calls, the dbg-text and
    dbg-rect relays can be removed — the relay screenshots in this
    round confirm the underlying machinery is healthy.
