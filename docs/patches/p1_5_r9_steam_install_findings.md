# Phase 1.5 r9 — Steam install findings

**Date:** 2026-05-12
**Status:** structural wall confirmed across all available game.db variants
**Goal:** see if Steam English Gold install provides the missing pieces

## What user reported

User had `C:\Program Files (x86)\Steam\steamapps\common\Silent Storm\` (English Gold install) and suggested it would have the files we need plus reverse-engineerable binaries.

## What's there

```
C:\Program Files (x86)\Steam\steamapps\common\Silent Storm\
├── game.exe                  8.9 MB  hash 7883a81632fc707e3aa4a72072650a63
├── MapEdit.exe               8.5 MB  Nival's level editor
├── game.db                   34.6 MB hash a6e92938daeb627abf4e1c3c37db1285
├── start.cfg                 38 B    "sequence .\cfg\intro.seq\n//mainmenu\n"
├── tree.mma                  143 KB  LifeStudio anim tree
├── binkw32.dll, fmod.dll, LifeStudioHeadAPI.dll, MFC71.dll, TriangLib.dll, ...
├── cfg/                      autoexec, config, input, intro.seq, loose.seq, win.seq
├── save/default/             sample save dir
├── source/                   Models + Textures (3D ASSETS only — NOT source code)
├── tools/                    DataImport.exe FontGen.exe PkgBuilder.exe TexConv.exe
├── Docs/                     EditorManual GameManual Mod SilentStormComplete_Manual.pdf
└── Versions: AlwaysCritical, APForInventoryUsage, HeadshotShouldKill, ImprovedBackstab
            (mod variant dirs with their own game.db)
```

## Hash comparison

| File | Steam | upstream |
|---|---|---|
| `game.db` | `a6e92938...` (34.6 MB) | `cdb4fe01...` (34.6 MB) |
| `Game.exe` | `7883a816...` | EnglishGold/Game.exe = `8ecc81b6...` |

Both Game.exe hashes differ — Steam build is a later patch.

## Test: swap Steam game.db into upstream/Complete

Backed up upstream game.db, copied Steam version in, ran silent_storm.exe (with our /WHOLEARCHIVE + criticalsBan deferred + ASSERT log-and-continue from earlier rounds).

**Result: identical 0xA1843130 missing-class failure mode.**

- 155 typeID `0xA1843130` records skipped at deserialize (same count as before)
- 328 `BasicDB.h:102 assertion pRes || nID < 1 failed` — downstream DB lookups fail because of the 155 skips
- `r8_render_fallback_menu` still triggers (UIContainer 347 still missing)
- exe alive, main loop ticking, no unhandled exception

**`0xA1843130` is a class that exists in the shipped Game.exe but was NOT included in the Jan03 open-source drop.** This is the structural wall confirmed across:
- upstream/Complete/game.db (original)
- upstream/Versions/Current/*/game.db (9 mod variants)
- Steam English Gold/game.db
- Steam mod variants

## Game.exe reverse-engineering attempt

`strings game.exe` returns empty — both Steam and upstream EnglishGold Game.exe are **packed** (UPX / Themida / similar). Symbol names are compressed; we can't trivially find the class registration.

## What "fixing this properly" looks like

A new sub-project (not in current Phase 1.5 scope):

1. **Detect packer** (Detect It Easy / DiE on Steam Game.exe)
2. **Unpack** to plain PE (UPX -d or whichever)
3. **IDA / Ghidra disassembly**:
   - Find the array of `REGISTER_SAVELOAD_CLASS` registrations (look for 0xA1843130 immediate value)
   - Locate the class name string adjacent
   - Map to a vtable
   - Inspect Serialize method bytecode → infer member layout
4. **Re-implement the class** in `port/src/` with same typeID + same serialize layout
5. **155 instances now deserialize** → DB lookups succeed → main menu camera + UIContainer load → real Nival UI renders

Estimated effort: 2-3 separate sessions (reverse engineering + implementation + integration).

## Where this leaves Phase 1.5

Game state machine works end-to-end through `mainmenu` transition (r7 fix). Fallback dbg-text main menu visible (r8). Real Nival UI requires the missing class.

Phase 1.5 acceptance: **substantially complete** — bgfx renderer, SDL3, fonts, markup, state machine all running. Real UI render gated on db schema mismatch that the open-source drop can't resolve without reverse-engineering the shipped binary.

## Files touched this round

None committed in this round — just diagnostic swap + revert. game.db restored to original.
