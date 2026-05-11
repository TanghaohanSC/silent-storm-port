#include "fmod_stub.h"
#include <cstdio>

// Strategy: init/control APIs return success so the game's audio init
// path completes; sample/stream loads return nullptr so callers see "no
// sound" but never get a usable handle to dereference. Playback APIs
// return -1 or 0 (FSOUND_FREE / error) consistently.
//
// Log every distinct call once via __FUNCTION__ identity (no dedup needed
// for Phase 0 — verbosity is helpful to see the call pattern).

namespace {
void log_call(const char* fn) {
    std::fprintf(stderr, "[fmod_stub] %s (silent fake)\n", fn);
}
} // namespace

#define STUB_LOG() log_call(__FUNCTION__)

extern "C" {

// --- System init / control ---
signed char FSOUND_Init(int, int, unsigned int)       { STUB_LOG(); return 1; }
void        FSOUND_Close(void)                         { STUB_LOG(); }
signed char FSOUND_SetOutput(int)                      { STUB_LOG(); return 1; }
signed char FSOUND_SetDriver(int)                      { STUB_LOG(); return 1; }
signed char FSOUND_SetSpeakerMode(unsigned int)        { STUB_LOG(); return 1; }
signed char FSOUND_SetHWND(void*)                      { STUB_LOG(); return 1; }
void        FSOUND_SetSFXMasterVolume(int)             { STUB_LOG(); }
void        FSOUND_Update(void)                        { /* hot path — silent */ }

// --- System queries ---
int   FSOUND_GetMaxChannels(void)                      { STUB_LOG(); return 32; }
int   FSOUND_GetNumHardwareChannels(int* a, int* b, int* c) {
    STUB_LOG();
    if (a) *a = 0;
    if (b) *b = 0;
    if (c) *c = 0;
    return 0;
}
int   FSOUND_GetChannelsPlaying(void)                  { return 0; }
int   FSOUND_GetNumDrivers(void)                       { STUB_LOG(); return 1; }
const char* FSOUND_GetDriverName(int)                  { STUB_LOG(); return "fmod_stub: no audio"; }
signed char FSOUND_GetDriverCaps(int, unsigned int* caps) {
    STUB_LOG();
    if (caps) *caps = 0;
    return 1;
}
int   FSOUND_GetMixer(void)                            { STUB_LOG(); return FSOUND_MIXER_QUALITY_FPU; }
float FSOUND_GetVersion(void)                          { STUB_LOG(); return FMOD_VERSION; }
int   FSOUND_GetError(void)                            { return 0; }

// --- Sample API ---
FSOUND_SAMPLE* FSOUND_Sample_Load(int, const char*, unsigned int, int, int) {
    STUB_LOG();
    return nullptr;
}
void           FSOUND_Sample_Free(FSOUND_SAMPLE*)                        { STUB_LOG(); }
signed char    FSOUND_Sample_SetMode(FSOUND_SAMPLE*, unsigned int)       { STUB_LOG(); return 1; }
signed char    FSOUND_Sample_SetDefaults(FSOUND_SAMPLE*, int, int, int, int) { STUB_LOG(); return 1; }
signed char    FSOUND_Sample_SetMinMaxDistance(FSOUND_SAMPLE*, float, float) { STUB_LOG(); return 1; }
unsigned int   FSOUND_Sample_GetLength(FSOUND_SAMPLE*)                   { STUB_LOG(); return 0; }

// --- Channel playback ---
int            FSOUND_PlaySound(int, FSOUND_SAMPLE*)                     { STUB_LOG(); return -1; }
int            FSOUND_PlaySoundEx(int, FSOUND_SAMPLE*, void*, signed char) { STUB_LOG(); return -1; }
signed char    FSOUND_StopSound(int)                                     { return 1; }
signed char    FSOUND_SetVolume(int, int)                                { return 1; }
signed char    FSOUND_SetVolumeAbsolute(int, int)                        { return 1; }
int            FSOUND_GetVolume(int)                                     { return 0; }
signed char    FSOUND_SetPaused(int, signed char)                        { return 1; }
signed char    FSOUND_IsPlaying(int)                                     { return 0; }
signed char    FSOUND_SetPan(int, int)                                   { return 1; }
unsigned int   FSOUND_GetCurrentPosition(int)                            { return 0; }
int            FSOUND_GetPriority(int)                                   { return 0; }
unsigned int   FSOUND_GetLoopMode(int)                                   { return 0; }

// --- Stream API ---
FSOUND_STREAM* FSOUND_Stream_Open(const char*, unsigned int, int, int) {
    STUB_LOG();
    return nullptr;
}
FSOUND_STREAM* FSOUND_Stream_OpenFile(const char*, unsigned int, int* length) {
    STUB_LOG();
    if (length) *length = 0;
    return nullptr;
}
signed char    FSOUND_Stream_Close(FSOUND_STREAM*)                       { STUB_LOG(); return 1; }
int            FSOUND_Stream_Play(int, FSOUND_STREAM*)                   { STUB_LOG(); return -1; }
signed char    FSOUND_Stream_Stop(FSOUND_STREAM*)                        { return 1; }
signed char    FSOUND_Stream_SetTime(FSOUND_STREAM*, int)                { return 1; }
int            FSOUND_Stream_GetTime(FSOUND_STREAM*)                     { return 0; }
signed char    FSOUND_Stream_SetPosition(FSOUND_STREAM*, unsigned int)   { return 1; }
signed char    FSOUND_Stream_SetSynchCallback(FSOUND_STREAM*, FSOUND_STREAMCALLBACK, void*) {
    STUB_LOG();
    return 1;
}

// --- Error helper ---
const char* FMOD_ErrorString(int) { return "fmod_stub: no real FMOD"; }

} // extern "C"
