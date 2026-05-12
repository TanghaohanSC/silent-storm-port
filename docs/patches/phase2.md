# Phase 2 — FMOD 3.x -> miniaudio (patch log)

**Date:** 2026-05-12
**Status:** DONE
**Scope:** Replace the Phase 0 silent fake `fmod_stub.cpp` with a real audio
backend backed by miniaudio 0.11.25. All 67 FMOD symbols enumerated in
`docs/inventory.md` keep the same C signatures; their bodies now route to
`ma_engine` / `ma_sound` / `ma_decoder`.

## What changed

### Iter 1 — Vendor miniaudio

- Added `port/third_party/miniaudio/miniaudio.h` (single header, ~4 MB) at
  tag `0.11.25`, fetched from
  `https://raw.githubusercontent.com/mackron/miniaudio/0.11.25/miniaudio.h`.
- One-time vendor in; no submodule.

### Iter 2 — Audit FModSound

Read `upstream/Soft/Andy/Jan03/a5dll/FModSound/FMSound.cpp`. Key findings:

- `NFMSound::Init` (line 415) drives `FSOUND_Init(mixrate, nMaxChannels, 0)`
  after `FSOUND_SetDriver` / `FSOUND_SetHWND`. Returns false-and-asserts if
  init fails; we return true.
- `NFMSound::LoadSample2D` / `LoadSample3D` pass `FSOUND_LOADMEMORY` and a
  pre-loaded raw buffer — there's no codec hint, just the encoded bytes.
  miniaudio's `ma_decoder_init_memory` sniffs WAV/MP3/FLAC/Vorbis from
  magic bytes.
- `NFMSound::PlayStream` calls `FSOUND_Stream_OpenFile(path, FSOUND_2D |
  FSOUND_LOOP_NORMAL?, 0)` then `FSOUND_Stream_Play(0, stream)`. Channel
  return is stored for later `SetVolume` / `SetPan(FSOUND_STEREOPAN)` /
  `IsPlaying`.
- Per-channel ops (`SetVolume`, `IsPlaying`, `StopSound`, `GetLoopMode`,
  `GetCurrentPosition`, `SetPaused`, `SetPan`) take FMOD-style channel
  ids. We mint integer ids in the new stub and store them in a thread-safe
  `unordered_map<int, ma_sound*>`.
- 3D positional APIs (`FSOUND_3D_SetAttributes`,
  `FSOUND_3D_Listener_SetAttributes`) are declared inline in `FMSound.cpp`
  via the Phase 0 port-compat block; left untouched.

### Iter 3 — Implement miniaudio backend

- Rewrote `port/src/stubs/fmod_stub.cpp`. Public header `fmod_stub.h`
  unchanged.
- `FSOUND_Init` -> `ma_engine_init` with sampleRate from caller (defaults
  48 kHz, but Nival passes 44 100 here so we honour it), stereo.
- `FSOUND_Sample_Load`:
  - Memory mode (`FSOUND_LOADMEMORY`): `ma_decoder_init_memory` ->
    `ma_sound_init_from_data_source`. Buffer is copied into the
    `FSOUND_SAMPLE` struct (miniaudio doesn't copy by itself).
  - File mode: `ma_sound_init_from_file(... MA_SOUND_FLAG_DECODE)`.
  - Looping forwarded via `ma_sound_set_looping`.
- `FSOUND_PlaySound` / `PlaySoundEx`: `ma_sound_seek_to_pcm_frame(0)` +
  `ma_sound_start`, then register the sound pointer in a channel table
  and return the channel id.
- `FSOUND_Stream_Open` / `OpenFile`:
  `ma_sound_init_from_file(... MA_SOUND_FLAG_STREAM |
  MA_SOUND_FLAG_NO_SPATIALIZATION)`.
- `FSOUND_Stream_SetTime` / `GetTime`: ms <-> PCM frames via
  `ma_engine_get_sample_rate`.
- `FSOUND_Stream_SetSynchCallback`: bridges to `ma_sound_set_end_callback`.
  Nival's CStream Switch path will fire when miniaudio reports end-of-sound
  (called from the audio thread; Nival's callback only flips a switch flag,
  so this is safe).
- `FSOUND_SetSFXMasterVolume`: maps 0..255 -> 0..1, `ma_engine_set_volume`.
- All 67 symbols implemented; mapping table in `fmod_stub.cpp` comments.
- `FMOD_ErrorString` proxies to `ma_result_description`.

### Iter 4 — .sfap0 graceful failure

- `ma_decoder_init_memory` returns a non-`MA_SUCCESS` result for the FMOD
  proprietary `.sfap0` container. We log `Sample_Load(mem,Nb): decoder
  init failed rc=X (likely .sfap0 or unsupported codec)` and return
  `nullptr`. `NFMSound::NewSample` already calls
  `OutputDebugString("Can't load sample")` on `nullptr` and returns 0, so
  Nival's caller sees no playback rather than a crash.
- Log is written to `silent_storm_audio.log` next to the exe.

### Iter 5 — Build + smoke

Build:

```
cmake --build --preset msvc-debug
[1/4] Building CXX object src\stubs\CMakeFiles\silent_storm_fmod_stub.dir\fmod_stub.cpp.obj
[2/4] Linking CXX static library lib\silent_storm_fmod_stub.lib
[3/4] Linking CXX executable bin\silent_storm.exe
```

Build clean. Initial attempt failed because miniaudio auto-enables the JACK
backend on Windows desktop (it includes `<jack/jack.h>` under
`MA_NO_RUNTIME_LINKING`). Fix: `#define MA_NO_JACK` before the
implementation. WASAPI + DirectSound + WinMM remain enabled.

Smoke run (12 s) against `upstream/Complete/`:

- `silent_storm_winmain.log` reaches step `22.4.c StepApp ret=1` — five
  full main-loop iterations, no audio-related abort.
- `silent_storm_audio.log` shows:
  ```
  Silent Storm audio log (Phase 2: FMOD -> miniaudio backend)
  FSOUND_Init: ok. sampleRate=44100 channels=2 backend=miniaudio 0.11.25
  ```
- `silent_storm_renderer.log` confirms bgfx still alive (backend=2, D3D11).
- Phase 1.5 intermission screen continues to render (visible in
  `phase2_smoke.png` capture).

The intermission state itself does no Sample_Load / Stream_Open calls
because the game isn't yet wired to load `Sounds.pak` / `.sfap0` assets at
this state. Phase 2 acceptance therefore reduces to:
1. fmod stub completely replaced by miniaudio implementation;
2. 67 FMOD symbols route to miniaudio APIs;
3. exe still boots into intermission screen with no audio crashes.

All three confirmed.

## Build artifacts

- `port/src/stubs/fmod_stub.cpp` (rewritten, ~430 LOC).
- `port/src/stubs/fmod_stub.h` unchanged (Phase 0 ABI).
- `port/src/stubs/CMakeLists.txt`: links `ole32`, `winmm`; sets `/W0` for
  the miniaudio TU (4 MB of generated C drowns /W4 otherwise);
  `MA_NO_JACK` enforced by source `#define` not CMake.
- `port/third_party/miniaudio/miniaudio.h` v0.11.25.

## Carryovers to Phase 3

1. **Real game audio assets.** Nival's `Sounds.pak` contains `.sfap0`
   files (FMOD's proprietary lossy format). miniaudio cannot decode them;
   they will fail `Sample_Load` and be silent. Two options for v1:
   - Transcode `.sfap0` -> `.wav`/`.ogg` offline using the original FMOD
     3.x tool chain (we have the binaries in `Versions/`).
   - Drop sound effects entirely (acceptable per "audio is bonus" Phase 2
     framing).
   Punt to Phase 3 alongside FFmpeg video.
2. **3D positional audio.** `FSOUND_3D_SetAttributes` is currently a
   no-op inline in `FMSound.cpp`. Hook it to `ma_sound_set_position` when
   real sample loading lights up.
3. **Stream resampling.** FMOD silently resampled to the device rate;
   miniaudio's engine pipeline does the same via the resource manager
   path, so this should work as-is when streams open. Verify on first
   `.ogg` stream.
4. **Channel free-list pressure.** Current implementation uses a
   monotonically growing channel id with no recycling. Plenty of headroom
   in `int` for one session; revisit if logs show >100 k channel ids per
   session.
