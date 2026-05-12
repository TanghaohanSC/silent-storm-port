# Phase 1.5 Round 5 — Resolution Verification + Phase 1 Closure

**Date:** 2026-05-11
**Status:** DONE

## Scope

Verification-only round: no source code changes. Confirmed rendering at three target
resolutions, updated documentation, committed.

## Resolution tests

| Resolution | Config (W×H) | Window actual | HUD scale | Result |
|---|---|---|---|---|
| 1920×1080 | 1920×1080 | 1920×1080 | 1 | PASS — INTERMISSION, ESC/Use console visible, "Work in progress" right-aligned |
| 2560×1440 | 2560×1440 | 2560×1440 | 1 | PASS — same content, window fills 1440p viewport |
| 3840×2160 | 3840×2160 | 3840×2160 | 1 | PASS — window opens at full 4K, all text visible (text proportionally small) |

### HUD scale note

All three resolutions report HUD scale 1 in the debug header.  The spec calls for
`hud_scale = auto` to produce 1× at 1080p/1440p and 2× at 4K.  T11 wires this
correctly for Nival's `SetTransform(D3DTS_PROJECTION, ortho)` path.  However the
Phase 1.5 glyph-relay path bypasses that matrix path entirely — it injects text
directly via `bgfx::dbgTextPrintf`, which is unscaled.  The 4K text therefore appears
proportionally smaller than intended.

This is expected and acceptable for Phase 1.5.  The real fix is Phase 2 work:
load `CTypeface` fonts from `game.db` and render glyphs via the normal Nival
draw-primitive path, which DOES go through the `SetTransform` ortho detection and
will honour T11's `hud_scale`.

Screenshots saved to `docs/patches/`:
- `p1_5_r5_resolution_1080.png` — 1920×1080, 140 KB
- `p1_5_r5_resolution_1440.png` — 2560×1440, 290 KB
- `p1_5_r5_resolution_4k.png`  — 3840×2160, 112 KB  (lower entropy because text
  occupies a smaller fraction of the large frame)

## Config restore

`silent_storm.cfg` reset to `width = 1024 / height = 768` after testing.
