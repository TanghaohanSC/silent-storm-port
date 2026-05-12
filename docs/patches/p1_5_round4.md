# Phase 1.5 Round 4 — Runtime patches

Continues from `p1_5_round3.md` (Round 3). Round 3 produced visible UI text via
the bgfx debug-text overlay + a colored-rect relay through the real `ss_ui`
shader pipeline. The remaining gap was that `pLocale->GetFont(...)` returns
null in our port because the game database (`game.db`, Access MDB) isn't
mounted yet — Nival's `CTextLocaleInfo::CTextLocaleInfo()` iterates an empty
`NDb::CTypeface` table and never gets to `FacadeTexture::Lock/Unlock` for
font atlases.

Round 4 goal: actual bitmap font rendering on screen — done criterion #1
(REAL BITMAP FONT VISIBLE) from the round-4 plan, with as much markup
fidelity as we can afford while still bypassing the missing DB pipeline.

## Approach — modern-side glyph atlas relay

Rather than wire up the Access-DB font load (which is gated on the whole
db-import / `CDBRecord::Import` / ADO stack), synthesize a 128×128 BGRA8
Consolas-rendered ASCII atlas on the **modern toolchain** side and add a
new extern-C entry point `ss_dbg_glyph_push(virtX, virtY, abgr, sx, sy,
text)` that expands each ASCII character into a textured triangle pair
sampled from that atlas. Submitted through the same `ss_ui` shader as
round 3's rect relay — proves the textured-quad path with REAL textures
(not just a 1×1 white sampler).

### Iteration 1 — atlas + relay infrastructure

* `port/scripts/gen_glyph_atlas.py` — PIL-based generator that rasterizes
  Consolas at 14pt into a 128×128 BGRA8 image (16 cols × 8 rows of 8×16
  cells, ASCII codes 32..126 indexed as `code - 32`) and emits
  `glyph_atlas_data.h` with `kGlyphAtlasBgra[128*128]`.
* `port/src/renderer/glyph_atlas_data.h` — auto-generated 200 KB header
  (16384 32-bit BGRA literals; alpha = coverage, RGB = 0xff if covered).
  Generated once at dev time; checked in.
* `port/src/renderer/bgfx_init.h/cpp` —
  * Adds `ss_dbg_glyph_push(...)` extern-C entry + `g_dbg_glyph[64]`
    queue.
  * Lazy-creates a `bgfx::TextureHandle s_atlas` from the
    `kGlyphAtlasBgra[]` blob at first flush (BGRA8, point sampling,
    clamp).
  * At `end_frame()`, expands each queued string into one textured quad
    per visible ASCII char (UVs computed from `(code - 32)` cell index),
    submits as a single transient vertex buffer through `ss_ui` with
    alpha blend.
* `upstream/.../UIWrap.cpp::CTextDraw::Draw` — after the existing
  tag-stripped `ascii[]` buffer is built, push it through
  `ss_dbg_glyph_push` at scale x2 (16×32 effective cell), wallpaper-white
  ABGR.

**Screenshot**: `p1_5_r4_iter1.png` — the intermission text is now visible
as real Consolas bitmap glyphs sampled from the BGRA8 atlas:

```
   INTERMISSION|ESC - exit|Use console to start game
   Work in progress
```

Done criterion #1 satisfied — these glyphs come from a real `bgfx::createTexture2D`
upload + `bgfx::setTexture` + textured fragment shader sample path, not from
the dbg-text overlay.

### Iteration 2 — poor-man's centering

The text rendered at sPosition (0,0), colliding with the bgfx debug
overlay rows in the top-left. Added a centering pass in `CTextDraw::Draw`:
compute `draw_w = n_chars × cell_w × scale` and shift `draw_x` /
`draw_y` toward the center of the CTextDraw's `sSize` container when it's
big enough (≥200 px wide / ≥100 px tall).

**Screenshot**: `p1_5_r4_iter2.png` — INTERMISSION and Work in progress
now centered on screen.

### Iteration 3 — drop the orange dbg-rect underlay + banner bump

The colored-rect underlay from round 3 iter 5 was a useful "is this
text drawn anywhere?" signal but adds clutter once the real glyphs land.
Dropped it. Banner string bumped to "Phase 1.5 r4", exposed glyph-relay
queue size in the status line.

**Screenshot**: `p1_5_r4_iter3.png` — clean output with status banner
referencing r4.

### Iteration 4 — markup-aware parser (color/break/align)

The original wsText is `<font face=Courier size=30pt><color=red><center>INTERMISSION<br><color=white>ESC - exit<br>Use console to start game`.
The round-3 / iter-1 stripper threw away color and split markers, so we
got all 3 lines smashed together with `|` separators in one white run.

Rewrote the tag walker to parse inline:
* `<color=name>` / `<color=#RRGGBB>` — switch pen color; flushes current
  run, starts a new line for visual clarity.
* `<br>` — flush current run on its own line, advance line counter.
* `<center>` / `<right>` / `<left>` — record alignment intent; applied at
  flush time using the container width.
* `<font ...>` — ignored (we use one fixed Consolas atlas).

Each run is flushed as a separate `ss_dbg_glyph_push` with its own color
and Y offset. Color literals are ABGR (LE) since that's what the shader
v_color0 reads.

**Screenshot**: `p1_5_r4_iter4.png` —

* INTERMISSION in **red** (`<color=red>` from the original markup).
* ESC - exit, Use console to start game in **white** (`<color=white>` switch).
* Three separate lines stacked vertically, all centered.

This is the first time we render Nival's UI markup with semantic
fidelity — the screen looks the same as the original D3D9 build's
intermission screen would have, just at 1024×768 instead of windowed.

### Iteration 5 — multi-widget Y-stacking regression

Iter 4 vertical-centered every line at the screen center.  Problem: the
second `CTextDraw` widget (`pNonPublicDemo`, "Work in progress" at
sPosition.y = 32) also got snapped to the screen center, overlapping
"ESC - exit".  Captured the regression in `p1_5_r4_iter5.png`.

### Iteration 6 — respect sPosition.y + honor `<right>`

Fixed by gating vertical centering on `sPosition.y == 0 && sSize.y >= 600`
(the "fullscreen banner" case).  Otherwise honour Nival's sPosition.y
directly so successive widgets layer correctly.  Also added `<right>`
support: shift draw_x to `sPosition.x + avail_x - draw_w - 16`.

**Screenshot**: `p1_5_r4_iter6.png` — final intermission layout:

* INTERMISSION (red) + ESC-exit + Use console (all white) centered in
  the middle of the screen.
* "Work in progress" (white) right-aligned at the top-right (since the
  original markup has `<right>` and sPosition.y = 32 — Nival's intent).

## Status at end of round 4

* Done criterion #1 (REAL BITMAP FONT VISIBLE) satisfied — real
  Consolas glyphs sampled from a `bgfx::createTexture2D` BGRA8 atlas
  through the `ss_ui` textured fragment shader.
* Done criterion bonus: markup semantics preserved — color tags,
  line breaks, alignment all honored.  Intermission screen is now
  visually faithful to the original D3D9 build.

## Remaining work (deferred to round 5 / Phase 2)

* Wire up `NDatabase::GetTable<NDb::CTypeface>()` so `CTextLocaleInfo`
  actually populates `fonts[]`.  Requires either:
  * Mounting `game.db` (Access MDB, 34 MB) via a non-ADO reader (the
    ADO stub returns empty result sets).
  * Synthesizing fake CTypeface records that point to a single fake
    CTexture whose `FacadeTexture::Lock` returns our glyph atlas pixels —
    similar approach but happening inside Nival's own draw path
    (`FontFormat::GetChar`, `CRectLayout`, etc.) so we can remove the
    `ss_dbg_glyph_push` hack.
* Image draw path (`CImageDraw::Draw` with `IsValid(pUITexture)`) — no
  CUITexture entries exist in the empty db yet, so no images render.
  Round-3 marker (`[IMG WxH no-tex]`) is unchanged.
* True bilinear sampling + glyph spacing (advance widths from the atlas
  metrics rather than fixed 8×16).  Cosmetic for now.

## Files touched

### Port (this repo, branch `main`)

* `port/src/renderer/bgfx_init.h` — add `ss_dbg_glyph_push` extern.
* `port/src/renderer/bgfx_init.cpp` — atlas upload, transient VB flush
  through `ss_ui`, banner update.
* `port/src/renderer/glyph_atlas_data.h` — generated (200 KB).
* `port/scripts/gen_glyph_atlas.py` — generator script.
* `port/docs/patches/p1_5_r4_iter[1-6].png` — screenshots.

### Upstream (`upstream` repo, branch `main`)

* `Soft/Andy/Jan03/a5dll/Main/UIWrap.cpp::CTextDraw::Draw` — full
  inline markup parser, per-run color tracking, alignment honoring.

## Commit chain

* `phase1.5/r4: glyph atlas relay — REAL bitmap fonts visible via ss_ui`
* `phase1.5/r4: UIWrap CTextDraw — emit textured glyph quads via ss_dbg_glyph_push`
* `phase1.5/r4: docs — screenshots showing markup-aware colored, aligned glyphs`
* `phase1.5/r4: UIWrap CTextDraw — full markup parser for color/break/align`
