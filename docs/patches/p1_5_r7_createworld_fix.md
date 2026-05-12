# Phase 1.5 Round 7 — Past intermission into main-menu state

**Date:** 2026-05-11
**Status:** done — `mainmenu` command no longer crashes; `CICMainMenu` runs to
completion and the app enters Nival's main-menu state instead of staying in
the intermission placeholder.

## TL;DR

`start.cfg` is now `mainmenu`. After r7 patches it boots cleanly through to a
running main-menu interface (no buttons / camera scene yet because the shipped
`Complete/game.db` is missing the DBCamera/UIContainer assets the source-drop
expects, but no crash — main loop ticks indefinitely).

| Stage | r6 status | r7 status |
|---|---|---|
| `CICInterMission::Exec` | OK | OK |
| `mainmenu` console cmd | not registered | registered (no change) |
| `CICMainMenu::Exec` | crash inside | runs to completion |
| `CRenderBaseInterface::Initialize` | crash in `CreateGlobalPlayer` | runs |
| `CMainMenuInterface::Initialize` | crashed before reachable | runs |
| Window | blank intermission text | blank main-menu state (no assets) |

## Crash chain — what was actually NULL

Two consecutive crashes, both root-caused to **missing records in the shipped
game.db** (the `Complete/game.db` we have is the Hammer&Sickle Russian
release database; it does not contain a few hard-coded IDs that the Jan03
source drop's main-menu path expects):

### Crash #1 — `NDb::GetPers(54)` → NULL → CUnit ctor null deref

Call site: `RPGGlobal.cpp:225` (inside `CreateGlobalPlayer()`)

```cpp
pPlayer->AddMerc( CreateMerc( NDb::GetPers(PC_SOLDIER), 0, true ) );
```

`PC_SOLDIER=54`, `PC_GRENADER=53`, `PC_SNIPER=14` — none of these CRPGPers
IDs exist in `Complete/game.db`. `CreateMerc(NULL,...)` chains to
`CUnit::CUnit(NULL,...)` (`RPGUnit.cpp:87`), which on line 92-93 does

```cpp
if ( !IsValid( pHead ) )
    pHead = pPers->pHead;   // deref NULL pPers, member offset 0x50
```

Crash signature: `EIP` inside `??$Add<vector<SRawMaterialApply>>@CStructureSaver`
(misleading — `+0x3A` is just a chained inline epilogue), `EAX=ECX=0x00000050`
= field offset of `CRPGPers::pHead` reached via `this=NULL`.

### Crash #2 — `NDb::GetDBCamera(26)` → NULL

Call site: `iMainMenu.cpp:191` (after fix #1 unblocked the path)

```cpp
CPtr<NDb::CDBCamera> pDBCamera = NDb::GetDBCamera( N_MAINMENU_CAMERA );
ICamera::SCameraPos sCameraPos( pDBCamera->vAnchor, ... );   // NULL deref
```

`N_MAINMENU_CAMERA = 26`. Database lacks that camera record. (Same data-side
root cause as crash #1.) Also `NDb::GetUIContainer(347)` for the main-menu
UI template is missing in the same DB — would have been crash #3 if the
guard didn't cover it pre-emptively.

## Patches

### 1. `RPGGlobal.cpp` — guard CreateGlobalPlayer against missing pers

```cpp
NDb::CRPGPers *pSoldier  = NDb::GetPers( PC_SOLDIER  );
NDb::CRPGPers *pGrenader = NDb::GetPers( PC_GRENADER );
NDb::CRPGPers *pSniper   = NDb::GetPers( PC_SNIPER   );
if ( pSoldier  ) pPlayer->AddMerc( CreateMerc( pSoldier,  0, true ) );
if ( pGrenader ) pPlayer->AddMerc( CreateMerc( pGrenader, 0, true ) );
if ( pSniper   ) pPlayer->AddMerc( CreateMerc( pSniper,   0, true ) );
```

The CGlobalPlayer ends up with an empty `mercs` vector. `CWorld::AddPlayer`
called downstream with `bLeanAndMean=true` exits early after
`SetCommander(...)` so it never iterates mercs — fine for the main-menu
scene which has no units anyway.

### 2. `iMainMenu.cpp` — guard CMainMenuInterface::Initialize

Pre-checks before the bare-pointer derefs of `pDBCamera->...` and the
`LoadTemplate(GetUIContainer(347))` call. When the records are missing
the main menu still constructs (no camera placement / no UI buttons),
which is enough for the state to be reachable. The next round can drop
hand-built DBCamera/UIContainer fixtures into `Complete/` to make the
menu actually visible.

### 3. `Complete/start.cfg` — wired to mainmenu

Was empty in r6 (intermission only). Now contains `mainmenu`. The
`CICInterMission::Initialize` config-chain still runs first, so the
intermission text is briefly installed, then immediately superseded by
`CICMainMenu` which `ResetStack()`s and pushes its own interface.

## Final state (screenshot `p1_5_r7_mainmenu.png`)

- Top HUD strip ("Silent Storm port — Phase 1.5 r4 — bgfx alive, frame N")
  is r4's diagnostic overlay; ticks happily through 28k+ bgfx frames per
  10-second run.
- The "Work in progress" placeholder text top-right is from Nival's font
  pipeline when a glyph is unresolved.
- No more "INTERMISSION / ESC - exit / Use console to start game" banner —
  that interface has been swapped out by `CICMainMenu`.
- `silent_storm_crash.log` is **empty** (no crashes; main loop ticks
  StepApp=1 each iteration).

## What's still missing (out of scope for r7)

- DBCamera 26 / UIContainer 347 / CRPGPers 14/53/54 — all data-side. Will
  need either:
  - hand-built fixtures dropped into Complete/ (small data files
    overriding game.db records), OR
  - using the actual Jan03-era game.db (we don't have it), OR
  - mapping the Hammer&Sickle DB's renumbered IDs back to Jan03 ones
- `missing typeID 0xA1843130` log spam in `silent_storm_missing_classes.log`
  — a legacy SAVELOAD class hash not registered in the port. Harmless
  beyond the noise.

## Iterations used: 4

1. Add trace harness around CICMainMenu / CMainMenuInterface::Initialize /
   CRenderBaseInterface::Initialize → crash at `AddPlayer` step.
2. Drill down into `CreateGlobalPlayer` → `GetPers(54)=NULL` confirmed.
3. Apply null-guard to CreateGlobalPlayer → next crash at `GetDBCamera(26)`.
4. Apply null-guard to CMainMenuInterface::Initialize for DBCamera + UI
   container → main menu state reached, runs stably.

Then a 5th cleanup pass to strip the verbose `ss_rbi_trace` / `ss_mm_trace`
scaffolding now that the path is understood.

## Files touched

- `upstream/Complete/start.cfg` — now `mainmenu`
- `upstream/Soft/Andy/Jan03/a5dll/Main/RPGGlobal.cpp` — CreateGlobalPlayer
  null-guards (lines ~212-230)
- `upstream/Soft/Andy/Jan03/a5dll/Main/iMainMenu.cpp` — Initialize null-guards
- `port/docs/patches/p1_5_r7_createworld_fix.md` — this doc
- `port/docs/patches/p1_5_r7_mainmenu.png` — screenshot
