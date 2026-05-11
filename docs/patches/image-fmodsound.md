# Phase 0 Patches: Image and FModSound subprojects

## Image (`upstream/Soft/Andy/Jan03/a5dll/Image/`)

`jan03_Image` compiled and linked without any source changes on the first attempt.
The pre-existing s3tc_shim and CMakeLists configuration were already sufficient.

No patches applied to Image sources.

---

## FModSound (`upstream/Soft/Andy/Jan03/a5dll/FModSound/FMSound.cpp`)

All fixes are in a single compat block added at the top of `FMSound.cpp` plus
three in-line corrections to legacy API call sites.

### Patch 1 — Missing FMOD flag constants

`FSOUND_2D`, `FSOUND_HW3D`, and `FSOUND_CAPS_EAX2` were used in the source but
absent from `fmod_stub.h`.  Added as `#define` guards in FMSound.cpp:

```c
#define FSOUND_2D             0x00000008
#define FSOUND_HW3D           0x00001000
#define FSOUND_CAPS_EAX2      0x00000010
```

Values match the FMOD 3.x public header historical constants.

### Patch 2 — Missing mixer-type constants

`FSOUND_MIXER_MMXP5`, `FSOUND_MIXER_MMXP6`, `FSOUND_MIXER_QUALITY_MMXP5`,
`FSOUND_MIXER_QUALITY_MMXP6` were used in a `#ifdef _DEBUG` switch block but
not in `fmod_stub.h`.  These constants appear only in the debug-logging path;
all map to distinct int values so the switch compiles:

```c
#define FSOUND_MIXER_MMXP5          2
#define FSOUND_MIXER_MMXP6          3
#define FSOUND_MIXER_QUALITY_MMXP5  4
#define FSOUND_MIXER_QUALITY_MMXP6  5
```

### Patch 3 — `FSOUND_Sample_Load` 4-param overload

The stub declared `FSOUND_Sample_Load(index, name, mode, offset, length)` (5
params) but FMSound.cpp calls it with only 4 params (no explicit `offset`).
Added a thin inline overload that forwards with `offset = 0`:

```cpp
inline FSOUND_SAMPLE* FSOUND_Sample_Load(int index, const char* name,
    unsigned int mode, int length)
{
    return FSOUND_Sample_Load(index, name, mode, 0, length);
}
```

### Patch 4 — `FSOUND_3D_SetAttributes` and `FSOUND_3D_Listener_SetAttributes` stubs

Both 3D positioning functions were called but not declared in `fmod_stub.h`.
Added no-op inline stubs in FMSound.cpp (Phase 0 goal: silent fake at runtime):

```cpp
inline signed char FSOUND_3D_SetAttributes(int channel,
    const float* pos, const float* vel) { return 1; }
inline signed char FSOUND_3D_Listener_SetAttributes(
    const float* pos, const float* vel,
    float fx, float fy, float fz,
    float tx, float ty, float tz) { return 1; }
```

### Patch 5 — `SynchCallback` / `FSOUND_STREAMCALLBACK` signature alignment

The stub declared `FSOUND_STREAMCALLBACK` as
`int (*)(FSOUND_STREAM*, void*, int, void*)` (4th param `void*`).
The original `SynchCallback` used `int param` as 4th param and its two call
sites passed `(int)this`.

Fixed in three places:
1. Forward declaration: `void *param` instead of `int param`.
2. Definition: same change.
3. Both `FSOUND_Stream_SetSynchCallback` call sites: cast callback to
   `(FSOUND_STREAMCALLBACK)` and cast `this` to `(void*)`.
