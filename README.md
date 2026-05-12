# Silent Storm Port

Open-source modernization of the 2003 Nival Interactive game Silent Storm,
based on the 2026 source release. See `../docs/superpowers/specs/` for design.

**Status:** Phase 2 COMPLETE. FMOD 3.x replaced by miniaudio 0.11.25 — all 67 `FSOUND_*` symbols route to `ma_engine` / `ma_sound` / `ma_decoder`. silent_storm.exe boots into Nival's intermission screen rendered via bgfx D3D11 with the audio backend live on WASAPI; verified at 1920×1080, 2560×1440, 3840×2160. Next: Phase 3 video (FFmpeg). See `docs/phase-2-completion.md`.
