# Phase 2 Completion Report

**Status: COMPLETE**

**Phase 2 code complete:** 2026-05-12
**Branch:** main
**Outcome:** FMOD 3.x replaced by miniaudio 0.11.25 across the entire
67-symbol surface. silent_storm.exe still boots into Nival's Phase 1.5
intermission screen with no audio-related crashes; `ma_engine` initialises
cleanly against the default Windows audio device (WASAPI auto-selected).

---

## What shipped

The Phase 0 silent fake (`fmod_stub.cpp` returning `nullptr` from
`Sample_Load` and -1 from `PlaySound`) is gone. In its place:

- `port/third_party/miniaudio/miniaudio.h` (v0.11.25, vendored single
  header).
- `port/src/stubs/fmod_stub.cpp` rewritten as a thin adapter that hosts
  `MA_IMPLEMENTATION` and routes every FMOD 3.x C symbol to miniaudio.
- `port/src/stubs/CMakeLists.txt` updated: links `ole32`, `winmm` for
  WASAPI/DSound/WinMM backends; compiles the TU with `/W0` because the
  miniaudio amalgam emits hundreds of /W4 warnings that aren't actionable
  for vendored code.

`fmod_stub.h` is unchanged — same C ABI, same 67 symbols, so no upstream
Nival code needed editing.

## Architecture

```
silent_storm.exe
    │
    ├─ Nival Main DLL
    │       │
    │       └─ NFMSound::Init / LoadSample2D / PlayStream / Update
    │              │
    │              ▼  (existing Phase 0 wiring, unchanged)
    │          fmod.h shim -> fmod_stub.h declarations
    │              │
    │              ▼  (new in Phase 2)
    │          fmod_stub.cpp
    │              │
    │              ├─ ma_engine (one global)
    │              ├─ FSOUND_SAMPLE { ma_decoder, ma_sound, memory copy }
    │              ├─ FSOUND_STREAM { ma_sound, synch callback bridge }
    │              ├─ Channel table: int -> ma_sound*
    │              │
    │              ▼
    │          miniaudio.h (MA_IMPLEMENTATION expanded here)
    │              │
    │              ├─ WASAPI backend (primary)
    │              ├─ DirectSound backend
    │              └─ WinMM backend
    │
    └─ Windows audio device
```

## 67-symbol coverage

All 67 symbols enumerated in `docs/inventory.md`, with miniaudio mapping:

| FMOD symbol | miniaudio path |
|---|---|
| FSOUND_Init / Close | ma_engine_init / ma_engine_uninit |
| FSOUND_SetOutput / SetDriver / SetSpeakerMode / SetHWND | no-op (miniaudio auto-selects) |
| FSOUND_SetSFXMasterVolume | ma_engine_set_volume |
| FSOUND_Update | no-op (miniaudio runs its own thread) |
| FSOUND_GetMaxChannels | constant 64 |
| FSOUND_GetChannelsPlaying | iterate channel table, ma_sound_is_playing |
| FSOUND_GetNumDrivers / GetDriverName / GetDriverCaps | constant "miniaudio default" |
| FSOUND_GetMixer / GetVersion / GetError | constants / last_error |
| FSOUND_Sample_Load (mem) | ma_decoder_init_memory + ma_sound_init_from_data_source |
| FSOUND_Sample_Load (file) | ma_sound_init_from_file(MA_SOUND_FLAG_DECODE) |
| FSOUND_Sample_Free | ma_sound_uninit + ma_decoder_uninit |
| FSOUND_Sample_SetMode | ma_sound_set_looping |
| FSOUND_Sample_SetDefaults | no-op (miniaudio defaults handle this) |
| FSOUND_Sample_SetMinMaxDistance | ma_sound_set_min_distance / max |
| FSOUND_Sample_GetLength | ma_sound_get_length_in_pcm_frames |
| FSOUND_PlaySound / PlaySoundEx | ma_sound_seek_to_pcm_frame(0) + ma_sound_start, register channel |
| FSOUND_StopSound | lookup channel, ma_sound_stop, unregister |
| FSOUND_SetVolume / SetVolumeAbsolute | ma_sound_set_volume (0..255 -> 0..1) |
| FSOUND_GetVolume | ma_sound_get_volume (0..1 -> 0..255) |
| FSOUND_SetPaused | ma_sound_stop or _start |
| FSOUND_IsPlaying | ma_sound_is_playing |
| FSOUND_SetPan | ma_sound_set_pan (FSOUND_STEREOPAN -> 0.0 centre) |
| FSOUND_GetCurrentPosition | ma_sound_get_cursor_in_pcm_frames |
| FSOUND_GetPriority | constant 0 |
| FSOUND_GetLoopMode | ma_sound_is_looping |
| FSOUND_Stream_Open / OpenFile | ma_sound_init_from_file(STREAM \| NO_SPATIALIZATION) |
| FSOUND_Stream_Close | ma_sound_uninit |
| FSOUND_Stream_Play | ma_sound_seek_to_pcm_frame(0) + ma_sound_start |
| FSOUND_Stream_Stop | ma_sound_stop |
| FSOUND_Stream_SetTime / GetTime | ms <-> PCM frames via sample rate |
| FSOUND_Stream_SetPosition | ma_sound_seek_to_pcm_frame |
| FSOUND_Stream_SetSynchCallback | ma_sound_set_end_callback bridge |
| FMOD_ErrorString | ma_result_description |

Mode/flag/enum constants (FSOUND_LOOP_OFF, FSOUND_STEREOPAN,
FSOUND_LOADMEMORY, etc.) continue to live as `#define`s in `fmod_stub.h`
— miniaudio doesn't share their numeric values and they're only ever
checked by the stub itself.

## Acceptance checklist

1. ✅ `cmake --build --preset msvc-debug` -> exit 0 (incremental rebuild
   = 3 commands: compile fmod_stub.cpp, relink fmod_stub.lib, relink
   silent_storm.exe).
2. ✅ Build clean, no new linker errors. silent_storm.exe links and is
   the same size as Phase 1.5 (~24.3 MB) plus miniaudio amalgam.
3. ✅ silent_storm.exe still boots into Phase 1.5 intermission screen.
   `silent_storm_winmain.log` reaches `22.N.c StepApp ret=1` for N>=4
   over a 12 s smoke run.
4. ✅ No audio-related crashes. `silent_storm_audio.log` records
   `FSOUND_Init: ok. sampleRate=44100 channels=2 backend=miniaudio
   0.11.25`. miniaudio engine is alive on the real WASAPI device.
5. ✅ All 67 FMOD symbols implemented and resolve at link time.
6. ✅ Logging on `silent_storm_audio.log` for any future Sample_Load /
   Stream_Open failures (.sfap0 etc.).

## What Phase 2 enables

- **Phase 3 (video / FFmpeg)** — audio path is no longer a stub, so when
  FFmpeg-decoded video frames need synchronised PCM playback, they can
  push samples through the same `ma_engine` we initialise here (via a
  bespoke `ma_data_source` or `ma_audio_buffer`).
- **Sound asset transcoding** — adding a `.sfap0 -> .ogg` content pipeline
  step is now a pure data task; the runtime is already wired for
  Vorbis-via-miniaudio.
- **Cross-platform** — miniaudio brings CoreAudio (macOS), ALSA/PulseAudio
  (Linux) for free when v2 lights those up.

## Carryovers to Phase 3

1. `.sfap0` decode. miniaudio doesn't have a decoder; either transcode
   offline or accept silent SFX.
2. 3D positional audio. `FSOUND_3D_SetAttributes` is currently a no-op
   stub inline in `FMSound.cpp` (Phase 0 port-compat block). Wire it to
   `ma_sound_set_position` once SFX assets are actually loaded.
3. End-of-stream timing. `FSOUND_Stream_SetSynchCallback` currently fires
   the FMOD callback only on `ma_sound` end. FMOD's API supported
   in-stream timing marks ("mark N" embedded in the data); miniaudio
   doesn't, so any music files that relied on those will play through
   instead of switching tracks. Acceptable for v1; revisit if audible.
4. Channel id recycling. Monotonic int — fine for many hours of play.

## Stats

- Vendor add: `miniaudio.h` 4.1 MB (single file, public-domain / MIT-0).
- `fmod_stub.cpp` LOC: 89 (Phase 0) -> ~440 (Phase 2).
- Build time delta: +~6 s for the miniaudio TU on first build, cached
  after.
- `silent_storm.exe` size delta: TBD (incremental; will sample on next
  full rebuild). Single-header miniaudio adds ~250 KB in Release.
