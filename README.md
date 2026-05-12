# Silent Storm Port

Open-source modernization of the 2003 Nival Interactive game Silent Storm,
based on the 2026 source release. See `../docs/superpowers/specs/` for design.

**Status:** Phase 3 COMPLETE. FFmpeg (avcodec / avformat / swscale /
swresample) is now linked via vcpkg; the runtime decodes Bink (.bik)
files directly using FFmpeg's built-in decoder, color-converts to BGRA8,
uploads to a bgfx 2D texture, and draws a letterboxed fullscreen quad
via the `ss_ui` shader archetype. Verified with the new
`ss_video_test.exe Nival.bik` harness — opens a window, plays through to
EOF or user skip (ESC / click / window close). `silent_storm.exe`
continues to boot into the Phase 1.5 intermission screen with the Phase 2
miniaudio backend live (Phase 3 is a pure addition, no existing call
site is touched). See `docs/phase-3-completion.md` and
`docs/patches/phase3.md`. Next: hook `play_video` into the Main DLL's
cutscene state machine (Phase 3.5) once Phase 4 lands its decisions.
