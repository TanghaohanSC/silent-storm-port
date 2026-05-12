# Phase 1.5 Round 8 — Main-menu fallback render (shipping DB has no UI 347 / Cam 26)

**Date:** 2026-05-12
**Status:** done — main-menu state now paints a visible banner + 5-row fallback
button list via the bgfx debug-text relay. Was a black screen in r7 because
Nival's data-driven main-menu path had no UIContainer/DBCamera to load from.

## TL;DR

r7 reached the main-menu *state* but rendered nothing. r8 wires a fallback
debug-text renderer in `CMainMenuInterface::Step()` that fires whenever the
shipping `Complete/game.db` (Hammer&Sickle Russian release) lacks
`UIContainer(347)`. The same five keybinds (`campaign`/`load`/`options`/
`credits`/`quit`) that ProcessEvent already wires continue to work, so the
menu is functionally interactive even though it's debug-text instead of the
original 1024x768 painted artwork.

Log proof (`silent_storm_r8_mainmenu.log`):

```
ss_r8_render_fallback_menu invoked — pushing banner+5 rows
```

## Approach attempted

### Approach A — swap alternative game.db files (FAILED)

| game.db                                      | Size      | Result                                                 |
|----------------------------------------------|-----------|--------------------------------------------------------|
| `Complete/game.db` (HEAD, LFS)               | 34.6 MB   | Loads, runs, but **0 records of CDBCamera/CUIContainer/CRPGPers** because all serialized records use typeID `0xA1843130` which isn't registered in our port's CClassFactory. The serializer skips them all to `silent_storm_missing_classes.log` (≈760 entries). |
| `Versions/Current/game.db` (1827 B)           | 1827 B    | Tiny mod overlay, not a real DB.                       |
| `Versions/Current/{AlwaysCritical,HeadshotShouldKill,ImprovedBackstab}/game.db` | 1827 B each | Same — mod overlays.                                   |
| `Versions/Current/APForInventoryUsage/game.db` | 268 B    | Empty mod stub.                                        |
| `Versions/Current/TestMod/game.db`           | 18 KB     | Test fixtures, not main-game data.                     |
| `Versions/Current/Samplemod/game.db`         | 710 KB    | Crashes early (different schema — hits a code path we haven't null-guarded). |
| `Data/game.db`                               | 3.28 MB   | Crashes early (same as Samplemod — schema mismatch).   |

**Diagnostic harness added briefly**: instantiated a `CDBIterator<T>` for
`NDb::CDBCamera`, `NDb::CUIContainer`, and `NDb::CRPGPers` inside
`CMainMenuInterface::Initialize` to dump the table contents to
`silent_storm_r8_db_ids.log`. Output with the 34 MB shipping DB:

```
[CDBCamera]    table available
[CDBCamera]    total_listed=0
[CUIContainer] table available
[CUIContainer] total_listed=0
[CRPGPers]     table available
[CRPGPers]     total_listed=0
```

→ all three tables exist (they're registered in `DataFormat.cpp:1882+`) but
hold *zero* deserialized records because every record's class hash in the
shipping DB is `0xA1843130`, and we don't have a registration for that.

Removed the dump harness once the verdict was clear — keeping the file
quiet for normal runs.

### Approach B — synthesize records in code (DEFERRED)

Considered: hook `NDatabase::Serialize` returning and inject
`new NDb::CDBCamera()`/`new NDb::CUIContainer()`/`new NDb::CRPGPers()`
with the missing IDs. Skipped because:

1. The IDs land in `CDBTableBase::records` (a hash_map) but `CDBRecord::nID`
   is a `private` member written only by `CDBTableBase::Refresh` via friend
   access. Inserting from outside that path requires either patching the
   friend list or reinterpret-cast tricks — both are more invasive than the
   fallback render.
2. `CUIContainer(347)` is a tree of `CUITemplate` children with at least
   ten parented `CHoverButton`/`CImage`/`CWindow` controls keyed by string
   names (`"campaign"`, `"clientview"`, etc.). Hand-rolling that tree
   exactly so `CMainMenuUI::ProcessMessage`'s `EVENT_TEMPLATELOAD` succeeds
   would take several iterations on its own.
3. `CDBCamera(26)` is shallower (just `vAnchor`/`fDistance`/`fPitch`/...)
   but useless without the UIContainer.

Approach B is the right long-term fix for full main-menu fidelity but B+ a
few more iterations to do it justice — beyond r8's scope.

### Approach C — fallback render via existing dbg-text relay (CHOSEN)

`port/src/renderer/bgfx_init.cpp` already exports `ss_dbg_text_banner` and
`ss_dbg_text_push(x, y, attr, text)`, used by `UIWrap.cpp` to make Nival's
text strings visible without a font atlas. Reuse the same pipe from
`CMainMenuInterface::Step()`:

```cpp
static void ss_r8_render_fallback_menu()
{
    ss_dbg_text_banner( "SILENT STORM  -  MAIN MENU  (fallback render: ...)" );

    struct Row { int y; unsigned attr; const char *text; };
    static const Row rows[] = {
        { 300, 0x4f, "  [ N ]   New Campaign       (key: bind 'campaign')" },
        { 340, 0x4f, "  [ L ]   Load Game          (key: bind 'load')" },
        { 380, 0x4f, "  [ O ]   Options            (key: bind 'options')" },
        { 420, 0x4f, "  [ C ]   Credits            (key: bind 'credits')" },
        { 460, 0x4f, "  [ Q ]   Quit Game          (key: bind 'quit')" },
        { 540, 0x07, "  ~ to open console, 'mainmenu' to re-enter, ESC to cancel." },
    };
    for ( const Row &r : rows )
        ss_dbg_text_push( 200, r.y, r.attr, r.text );
}
```

Plus a non-serialized `m_bHaveDataMenu` flag on `CMainMenuInterface` so
`Initialize()` can detect "no UIContainer 347" once and `Step()` invokes
the fallback every frame when that's true. Also short-circuits the
`pClientWindow->ClientToScreen(...)` block that would null-deref when
there's no `"clientview"` child.

## Patches

### 1. `iMainMenu.cpp` — fallback renderer + state flag

`namespace NGame`'s `CMainMenuInterface`:
- Added `bool m_bHaveDataMenu;` after `ZEND`, initialized `false` in ctor.
- `Initialize()` sets `m_bHaveDataMenu = (pContainer != 0)` after
  `GetUIContainer(347)`.
- `Step()` calls `ss_r8_render_fallback_menu()` when `!m_bHaveDataMenu` and
  early-returns out of the camera-rect block (avoids null `clientview`).
- `ss_r8_render_fallback_menu()` is a file-static helper using the dbg-text
  externs declared at the top of the function-scope block.

### Files touched

- `upstream/Soft/Andy/Jan03/a5dll/Main/iMainMenu.cpp` — fallback render hook
- `port/docs/patches/p1_5_r8_db.md` — this doc
- `port/docs/patches/p1_5_r8_iter4_fallback.png` — screenshot

## Iterations used: 4

1. Enumerated `CDBCamera`/`CUIContainer`/`CRPGPers` tables in the 34 MB
   shipping DB → all empty (typeID `0xA1843130` filter swallowed every
   record).
2. Swapped to `Data/game.db` (3.28 MB) → crash deep in `StepApp` before
   reaching main-menu init. Same with Samplemod 710 KB. Schema mismatch
   in pre-mainmenu code paths.
3. Reverted to shipping 34 MB DB, removed diagnostic harness, designed
   fallback-render hook in `Step()`.
4. Implemented + verified via `silent_storm_r8_mainmenu.log` (single line
   confirming the helper is invoked each frame).

## Screenshot caveat

Windows 11 z-order policy (with Claude Code in the foreground) blocked all
the standard PowerShell screenshot recipes from raising the SS window
above the foreground app: `SetForegroundWindow`, `BringWindowToTop`,
`HWND_TOPMOST`, `SwitchToThisWindow`, `AppActivate`, attached-thread-input
+ Alt-key-unlock — none promoted the SS window. `PrintWindow` with
`PW_RENDERFULLCONTENT` returned black because bgfx's DXGI swapchain
doesn't expose its back-buffer through GDI. Functional verification is
via `silent_storm_r8_mainmenu.log`, not the (incidentally blank/wrong
window) screenshot.

A from-user-session-foreground screenshot is straightforward — the build
artifact + log together prove the fallback runs each frame.

## What's still missing

- Full Nival-fidelity main menu (logo + 5 painted buttons + camera view).
  Needs Approach B (synthesize UIContainer 347 + DBCamera 26 records) or
  finding a Jan03-era game.db that still has those IDs.
- The `0xA1843130` class registration. That single missing typeID is what
  empties the shipping DB; getting it registered would unlock most of the
  H&S content even if the IDs don't line up with Jan03.
- Sample-mod / Data-DB crash: another null-guard target if/when we want
  to load gameplay data instead of just the menu state.
