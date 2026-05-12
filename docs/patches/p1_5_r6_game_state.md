# Phase 1.5 Round 6 — Past the intermission placeholder

**Date:** 2026-05-11
**Status:** in progress
**Goal:** push silent_storm.exe past the Nival "INTERMISSION / ESC - exit / Use console
to start game" placeholder into one of:

1. Real Nival main menu (Campaign / Load / Options / Quit)
2. A loaded mission with units on a map
3. The in-game console accepting commands

## Discovery

### How the intermission gets reached

`Game/Main.cpp` (WinMain) ends with:

```cpp
NMainLoop::Command( new CICInterMission( "start.cfg" ) );
```

`CICInterMission::Exec()` (`Main/iInterMission.cpp`) then:

1. Builds a `CInterMissionInterface`
2. Calls `Initialize(szConfig=start.cfg, wsMessage=L"")`
   - The empty message branch installs the placeholder text
     `INTERMISSION / ESC - exit / Use console to start game`
   - Then calls `NGlobal::LoadConfig( "start.cfg" )` to chain into whatever
     `start.cfg` says

So `start.cfg` *is* the chain link.  Its job is to invoke another command
that supersedes the intermission interface (via another `NMainLoop::Command`).

### Available console commands (grep `REGISTER_CMD` across a5dll)

State-changing:

| Command | Source | Action |
|---|---|---|
| `mainmenu` | iMainMenu.cpp:283 | `NMainLoop::Command( new CICMainMenu() )` |
| `map <variant> [...]` | iMission.cpp:2485 | start mission by map variant ID |
| `template <id> [...]` | iMission.cpp:2486 | start mission by template ID |
| `zone <persID...>` | iMission.cpp:2487 | start scenario zone |
| `chapter` | iChapterMap.cpp:283 | open chapter map |
| `global` | iGlobalMap.cpp:314 | open global strategic map |
| `load <slot>` | iMain.cpp:443 | load saved game |
| `save <slot>` | iMain.cpp:444 | save game |

Plumbing:

| Command | Source | Action |
|---|---|---|
| `exec <cfg>` | Commands.cpp:402 | LoadConfig recursively |
| `setvar var = val` | Commands.cpp:403 | set a global variable |
| `help` | Commands.cpp:401 | list commands |

NOT registered (despite appearing in `upstream/Versions/Current/start.cfg`):

- `sequence` — original was `sequence .\cfg\intro.seq` to chain Bink intros
- `play` — original was `play .\res\video\Nival.bik` (inside .seq files)

Those came from a sequence-playback subsystem that isn't in this code drop.
Phase 3's FFmpeg-based `play_video` is the port's replacement but isn't
wired into NGlobal::RegisterCmd yet.

### What start.cfg is supposed to contain

Bundled `upstream/Versions/Current/start.cfg`:

```
sequence .\cfg\intro.seq
//mainmenu
```

`intro.seq`:

```
play .\res\video\nVidia.bik
play .\res\video\JoWooD.bik
play .\res\video\Nival.bik
play .\res\video\Intro.bik
exec mainmenu
```

So the original boot flow is **intro Biks -> mainmenu**, and the
intermission placeholder is what we see when both `sequence` and `mainmenu`
fail silently (unknown command).

### Where the port currently sits

`upstream/Complete/` (the cwd we run silent_storm.exe in) does NOT contain a
`start.cfg`.  CICInterMission therefore opens nothing, the placeholder text
stays visible, and the boot loop just keeps ticking.

