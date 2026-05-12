# Phase 3 Completion Report

**Status: COMPLETE**

**Phase 3 code complete:** 2026-05-11
**Branch:** main
**Outcome:** A working FFmpeg-backed video playback subsystem now ships
with the port.  FFmpeg (avcodec + avformat + swscale + swresample) is
pulled in through vcpkg; the runtime decodes `.bik` files directly using
FFmpeg's built-in Bink decoder — no offline transcoding step is required.
Frames are color-converted to BGRA8 by `libswscale`, uploaded into a bgfx
2D texture, and drawn as a letterboxed fullscreen quad via the existing
`ss_ui` shader archetype.  The test exe `ss_video_test.exe` plays
`Nival.bik` from boot to EOF (or to ESC/click) and exits cleanly.
`silent_storm.exe` continues to boot into the Phase 1.5 intermission
screen — Phase 3 is a pure *addition* and does not touch any pre-existing
code path.

---

## What shipped

- `port/vcpkg.json` adds `ffmpeg` with the minimal four features
  (`avcodec`, `avformat`, `swscale`, `swresample`) needed for video-only
  playback. Audio resamplers come along for free because `avformat`
  pulls `swresample` regardless. Bink decode is part of stock `avcodec`.
- `port/src/renderer/video.{h,cpp}` — public `play_video(const char*)`
  blocking API. Returns `true` when the video reaches EOF, `false` on
  user skip or error. `video_backend_available()` predicate reports
  whether FFmpeg was successfully linked into this build (the TU
  contains a `#if defined(SS_HAVE_FFMPEG)` stub fallback that logs
  "Phase 3.5 — FFmpeg not yet linked" and returns false, so a build
  without vcpkg's FFmpeg still links and runs).
- `port/src/renderer/CMakeLists.txt` adds `video.cpp` to `ss_renderer`,
  performs `find_package(FFMPEG QUIET)` and conditionally defines
  `SS_HAVE_FFMPEG`. A new `ss_video_test` WIN32 executable target is
  declared with `add_dependencies(ss_video_test ss_shaders)` so the
  `ss_ui` shader binary is on disk before the test launches.
- `port/tests/video/test_play_video.cpp` — the acceptance harness.
  Creates a 1280x720 SDL3 window, initialises bgfx against its HWND,
  loads shader archetypes, calls `play_video(argv[1] ?? Nival.bik)`,
  then runs the shutdown sequence.
- `port/tools/bik2webm/bik2webm.ps1` — optional offline transcoder.
  Unused by the runtime (Bink decodes natively) but kept for downstream
  users who want smaller VP9 files.

The top-level `port/CMakeLists.txt` is **not** modified, in accordance
with Phase 3's scope guard. Every Phase 3 build wire-up lives in
`src/renderer/CMakeLists.txt`.

## Architecture

```
ss_video_test.exe                                    silent_storm.exe
        |                                                   |
        |  WinMain -> create SDL3 window                    |  (Phase 1.5
        |  -> init bgfx -> load_all_archetypes              |   boot path,
        |  -> play_video("Nival.bik")                       |   unchanged)
        |                                                   |
        v                                                   v
silent_storm::renderer::play_video                   <not exercised>
        |
        +-- avformat_open_input("Nival.bik")
        |   avformat_find_stream_info
        |   first AVMEDIA_TYPE_VIDEO stream  ->  codec_id = AV_CODEC_ID_BINKVIDEO
        |   pixfmt yuv420p, 640x352, ~16 fps
        |
        +-- av_image_alloc(BGRA, w, h)   (sws_scale staging buf)
        |   sws_getContext(yuv420p -> BGRA, BILINEAR)
        |
        +-- bgfx::createTexture2D(w, h, BGRA8, point-clamp)
        |
        +-- frame loop:
        |     av_read_frame   -> AVPacket
        |     avcodec_send_packet
        |     drain { avcodec_receive_frame -> AVFrame
        |             sws_scale(yuv -> BGRA)
        |             bgfx::updateTexture2D(BGRA pixels)
        |             submit fullscreen quad via ss_ui   (letterbox-fit)
        |             bgfx::frame()
        |             SDL events: ESC/click -> early exit }
        |     EOF -> send flush packet, drain once more, break
        |
        +-- cleanup (sws_freeContext, av_*_free_context, bgfx::destroy(tex))
```

## Acceptance checklist

Per the Phase 3 plan in
`docs/superpowers/specs/2026-05-11-silent-storm-modernization-design.md`
§3.5:

1. **Build clean (FFmpeg linked).** `cmake --build --preset msvc-debug`
   exits 0; `find_package(FFMPEG)` succeeds against vcpkg's
   `x86-windows` triplet; `silent_storm_renderer.log` shows
   `SS_HAVE_FFMPEG` is defined and `video_backend_available()` returns
   true.
2. **`ss_video_test.exe Nival.bik` plays.** The exe opens a 1280x720
   borderless window, decodes Bink, draws frames through `ss_ui`, and
   exits 0 on EOF.  `silent_storm_video.log` records the codec name,
   stream dimensions, and the final frame count.
3. **`silent_storm.exe` still boots.** Phase 1.5 intermission screen
   continues to render — `silent_storm_winmain.log` still reaches
   `22.N.c StepApp ret=1` for N>=4 over a 12s smoke run.
4. **No regressions.** Existing Phase 1 / 1.5 / 2 outputs unchanged.

## Bink decoder verification

The same `.bik` files that the 2003 retail game shipped — exercised here
via the production drop in
`upstream/Versions/Current/res/Video/`:

| File          | Codec         | Pixfmt   | Dimensions | Frame rate |
|---------------|---------------|----------|------------|------------|
| `Nival.bik`   | bink (Bink1)  | yuv420p  | 640x352    | ~16 fps    |
| `JoWood.bik`  | bink (Bink1)  | yuv420p  | 640x352    | ~16 fps    |
| `Intro.bik`   | bink (Bink1)  | yuv420p  | 640x352    | ~16 fps    |
| `Credits.bik` | bink (Bink1)  | yuv420p  | 640x352    | ~16 fps    |
| `Win.bik`     | bink (Bink1)  | yuv420p  | 640x352    | ~16 fps    |
| `Fail.bik`    | bink (Bink1)  | yuv420p  | 640x352    | ~16 fps    |
| `Loading.bik` | bink (Bink1)  | yuv420p  | 256x256    | ~16 fps    |
| `nVidia.bik`  | bink (Bink1)  | yuv420p  | 640x352    | ~16 fps    |

FFmpeg's built-in `binkvideo` decoder handles all 8 cleanly. No
artefacts visible at 1280x720 letterboxing.

## What Phase 3 enables

- **Cutscene-driven game flow.** When Phase 4+ wires `play_video` into
  the actual game state machine (Main DLL's `iMainMenu` /
  `iMissionMovieUI` etc.), all 8 production videos play through the
  same code path with zero asset transcoding.
- **Offline transcoding when needed.** `tools/bik2webm/bik2webm.ps1` is
  ready for anyone who wants .webm/VP9 (smaller, more portable).  The
  runtime is built to accept any container / codec FFmpeg supports, so
  switching is one renamed-file change with no code change.
- **Future audio sync.** miniaudio (Phase 2) is already live; the FFmpeg
  audio stream can be pulled in via a parallel `SwrContext` -> int16
  PCM -> `ma_audio_buffer` -> `ma_engine` path.  Not implemented in
  Phase 3 (the spec marks audio sync as out-of-scope), but the
  infrastructure on both sides is ready.

## Carryovers to Phase 3.5 / Phase 4

1. **Audio track + lipsync.** Not wired in Phase 3.  See above for the
   plumbing sketch.
2. **Game-state integration.** No existing call site invokes
   `play_video`.  When we want the engine to play the Nival splash at
   boot, we add a `#ifdef SS_USE_BGFX_FACADE` hook in Main's
   `iMainMenu.cpp` (or wherever the original 2003 build called Bink).
3. **PTS-accurate pacing.** Current pacing is `sleep_for(1000/fps ms)`.
   Drift is invisible on a 16-fps source but would matter for higher
   frame-rate content; trivial fix once we ingest audio (sync to audio
   clock) or `av_gettime_relative()`-based wall-clock targets.
4. **Fullscreen on resize.** The letterbox math recomputes per frame
   from `renderer::get_width/get_height()`, so window resizing is
   already handled.  Tested by dragging the window mid-playback.

## Stats

- Vendor add: FFmpeg via vcpkg (`avcodec`/`avformat`/`swscale`/`swresample`
  features).  First-time vcpkg build ~15-25 min and ~700 MB of build
  artefacts; binary cached for subsequent configures.
- `video.cpp` LOC: ~330.
- `test_play_video.cpp` LOC: ~85.
- New static-lib code in `ss_renderer.lib`: ~10 KB.
- `ss_video_test.exe` size: ~140 KB (links FFmpeg DLLs at runtime;
  `avcodec-*.dll` / `avformat-*.dll` / `swscale-*.dll` come from the
  vcpkg install dir and the build copies them next to the exe via the
  CMake `RUNTIME_OUTPUT_DIRECTORY` convention).
