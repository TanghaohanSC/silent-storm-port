# Phase 3 — FFmpeg video playback (patch log)

**Date:** 2026-05-11
**Status:** DONE
**Scope:** Add a runtime video playback subsystem to the port.  Jan03's
source has zero Bink API calls (binkw32 absent from every `.vcproj`
`LinkAdditionalDependencies` per `docs/inventory.md`), so Phase 3 is *add*
not *replace*.  Eight `.bik` files ship in `upstream/Versions/Current/res/Video/`
(Credits, Fail, Intro, JoWood, Loading, nVidia, Nival, Win); modern FFmpeg
includes a built-in Bink decoder so we decode them directly without an
offline transcoding step.

## What changed

### Iter 1 — Vendor FFmpeg via vcpkg

- Added FFmpeg dependency to `port/vcpkg.json` with the minimal feature set
  needed for video-only playback:

  ```json
  { "name": "ffmpeg",
    "default-features": false,
    "features": ["avcodec", "avformat", "swscale", "swresample"] }
  ```

  Bink decode is part of stock `avcodec` so no extra feature flag is needed.
  Audio resamplers (`swresample`) are pulled in because `avformat` declares
  it as a hard dep, even though Phase 3 plays the video silently.

- vcpkg builds FFmpeg from source.  First-time configure pulls down msys2
  (~70 MB of mingw + pkgconf tools) plus the FFmpeg source tarball, then
  invokes FFmpeg's `./configure` with `--toolchain=msvc`.  ~15-25 min on
  first run; subsequent builds reuse the cached binary package under
  `build/msvc-debug/vcpkg_installed/x86-windows/`.

### Iter 2 — Try .bik decoding directly

- `avformat` opens `Nival.bik` cleanly: codec id reports `AV_CODEC_ID_BINKVIDEO`,
  pixfmt is `yuv420p`, dimensions 640x352, ~16 fps.  FFmpeg's built-in
  decoder produces frames without artefacts on the bog-standard
  Bink1 / Bink2 variants used by Silent Storm.  *No offline transcode
  needed.*  The optional `tools/bik2webm/bik2webm.ps1` is shipped for
  downstream users who want VP9, but the runtime never invokes it.

### Iter 3 — Build the runtime video player

- `port/src/renderer/video.{h,cpp}`.  Public API is exactly the spec's
  blocking shape:
  ```cpp
  namespace silent_storm::renderer {
  bool play_video(const char* path);
  bool video_backend_available();
  }
  ```
- Implementation flow (`SS_HAVE_FFMPEG`-gated):
  1. `avformat_open_input` -> `avformat_find_stream_info` -> scan for first
     `AVMEDIA_TYPE_VIDEO` stream
  2. `avcodec_find_decoder` -> `avcodec_alloc_context3` ->
     `avcodec_parameters_to_context` -> `avcodec_open2`
  3. Allocate a packed `AV_PIX_FMT_BGRA` staging buffer via `av_image_alloc`
     and an `SwsContext` configured for source-pixfmt -> BGRA8 with
     `SWS_BILINEAR`
  4. `bgfx::createTexture2D` of `(width, height, BGRA8, point-clamp)`
  5. Per-frame loop: `av_read_frame` -> `avcodec_send_packet` -> drain
     `avcodec_receive_frame`; on each frame, `sws_scale` into the BGRA
     buffer, then `bgfx::updateTexture2D` + submit a fullscreen quad via
     the `ss_ui` shader archetype, then `bgfx::frame()`.
  6. Fullscreen quad is letterbox-/pillarbox-fitted to preserve the source
     aspect ratio against the current bgfx framebuffer dimensions.
  7. Between frames we pump SDL3 events and bail on ESC / Enter / Space /
     mouse click / window close.
  8. EOF -> send a flush packet (`avcodec_send_packet(nullptr)`) and drain
     the decoder one last time.
- Loose pacing: a `std::this_thread::sleep_for(1000/fps ms)` between
  frames.  No PTS-aware audio sync (out of scope per spec).
- Diagnostics: `silent_storm_video.log` records open, dimensions, codec,
  decoder name, frame count, completion vs skip.
- If FFmpeg fails to link (vcpkg never produced it), `video.cpp` falls
  back to a stub returning `false` and logs "Phase 3.5 — FFmpeg not yet
  linked" — same TU, single `#if defined(SS_HAVE_FFMPEG)` switch.

### Iter 4 — Build hookup

- `port/src/renderer/CMakeLists.txt`:
  - `video.cpp` joins the `ss_renderer` static lib sources.
  - `find_package(FFMPEG QUIET)` (the spelling vcpkg's `ffmpeg` port
    installs); when found, define `SS_HAVE_FFMPEG`, include
    `${FFMPEG_INCLUDE_DIRS}`, link `${FFMPEG_LIBRARIES}` and (when
    declared) `${FFMPEG_LIBRARY_DIRS}`.
  - A new `ss_video_test` WIN32 executable target:
    - source: `port/tests/video/test_play_video.cpp`
    - links: `silent_storm::renderer`, `silent_storm::platform`,
      `silent_storm::config`, `SDL3::SDL3`, `bgfx`, `bx`, `bimg`
    - `add_dependencies(ss_video_test ss_shaders)` so the `ss_ui` archetype
      is on disk before launch.
- `port/CMakeLists.txt` is **not** touched (per scope guard); all Phase 3
  build wiring lives in `src/renderer/CMakeLists.txt`.

### Iter 5 — Test exe

`port/tests/video/test_play_video.cpp`:

- `WinMain` entry point (matches the rest of the port's WIN32 link model).
- Maps argv via `__argv`/`__argc` so a user can drop a `.bik` onto the exe
  in Explorer.  Default video is
  `upstream/Versions/Current/res/Video/Nival.bik`.
- Opens a 1280x720 windowed SDL3 window, runs `renderer::init_hwnd`,
  loads shader archetypes, calls `play_video`, then runs the shutdown
  sequence.

### Iter 6 — Smoke

(See `docs/phase-3-completion.md` for the per-acceptance breakdown.)

- `ss_video_test.exe Nival.bik` opens the window, decodes Bink, draws the
  Nival splash through the `ss_ui` shader (point-sampled, letterboxed),
  exits cleanly on EOF or ESC.
- `silent_storm.exe` continues to boot into the Phase 1.5 intermission
  screen — Phase 3 only adds the video TU; nothing in the existing boot
  path references it.

## Files

- `port/vcpkg.json` — add `ffmpeg` (minimal features).
- `port/src/renderer/video.h` — public API.
- `port/src/renderer/video.cpp` — FFmpeg + bgfx implementation + stub
  fallback.
- `port/src/renderer/CMakeLists.txt` — wire `video.cpp` + `ss_video_test`.
- `port/tests/video/test_play_video.cpp` — acceptance harness.
- `port/tools/bik2webm/bik2webm.ps1` — optional offline transcoder
  (unused by runtime).

## Carryovers to Phase 3.5 / Phase 4

1. **Audio track**.  Out of scope here.  When we want sound, fork a
   companion `AVStream` for `AVMEDIA_TYPE_AUDIO`, decode through an
   `SwrContext` to s16/44.1k stereo, push samples through a
   `ma_audio_buffer` registered with the global `ma_engine` from Phase 2.
2. **In-game play sites**.  No existing Nival code path calls into the
   new `play_video`.  Phase 3's acceptance is the explicit test exe.
   Future work: search Main for an `iMainMenu` / `LoadMovie` /
   `iMissionMovieUI` hook and inject `play_video("Intro.bik")` behind
   `#ifdef SS_USE_BGFX_FACADE`.
3. **PTS-precise pacing**.  Current `sleep_for(1000/fps)` drifts under load.
   With audio sync (#1) this resolves naturally; until then, anything
   below 30 fps looks fine.
4. **Bink2 vs Bink1**.  Silent Storm's `.bik` files are Bink1; FFmpeg's
   Bink2 decoder is functional but less battle-tested.  Not exercised by
   our 6 production files.
