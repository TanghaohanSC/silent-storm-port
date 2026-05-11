#pragma once
// FMOD 3.x legacy stub — replaces fmodvc.lib symbol surface used by
// Jan03/a5dll/FModSound and Jan03/a5dll/Game.
//
// Inventory (Task 2) found 67 unique FMOD symbols, all reachable through
// the NFMSound wrapper namespace in FModSound/FMSound.cpp. Phase 2 replaces
// this stub with miniaudio. Phase 0 goal: link succeeds, silent fake at runtime.
//
// Init/state queries return success/sensible defaults so the game proceeds.
// Sample/Stream/Play functions return nullptr/-1 so callers see "no sound"
// but don't crash.

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// --- Opaque types ---
typedef struct FSOUND_SAMPLE FSOUND_SAMPLE;
typedef struct FSOUND_STREAM FSOUND_STREAM;
typedef int (*FSOUND_STREAMCALLBACK)(FSOUND_STREAM* stream, void* buff,
                                      int len, void* userdata);

// --- Mode/flag constants used by callers (inventory listed these) ---
#define FSOUND_LOOP_OFF        0x00000001
#define FSOUND_LOOP_NORMAL     0x00000002
#define FSOUND_STEREOPAN       0x00200000
#define FSOUND_UNMANAGED       0x04000000
#define FSOUND_LOADMEMORY      0x00008000
#define FSOUND_FREE            -1
#define FMOD_VERSION           3.74f

// --- Output / speaker modes ---
#define FSOUND_OUTPUT_NOSOUND  0
#define FSOUND_OUTPUT_WINMM    1
#define FSOUND_OUTPUT_DSOUND   2

typedef int FSOUND_OUTPUTTYPES;

#define FSOUND_SPEAKERMODE_STEREO         0
#define FSOUND_SPEAKERMODE_HEADPHONES     1
#define FSOUND_SPEAKERMODE_MONO           2
#define FSOUND_SPEAKERMODE_QUAD           3
#define FSOUND_SPEAKERMODE_SURROUND       4
#define FSOUND_SPEAKERMODE_DOLBYDIGITAL   5

// --- Mixer / hardware caps ---
#define FSOUND_MIXER_QUALITY_FPU          0
#define FSOUND_MIXER_BLENDMODE            1

#define FSOUND_CAPS_HARDWARE              0x00000001
#define FSOUND_CAPS_EAX                   0x00000002
#define FSOUND_CAPS_GEOMETRY_OCCLUSIONS   0x00000004
#define FSOUND_CAPS_GEOMETRY_REFLECTIONS  0x00000008

// --- System init / control ---
signed char FSOUND_Init(int mixrate, int maxsoftwarechannels, unsigned int flags);
void        FSOUND_Close(void);
signed char FSOUND_SetOutput(int outputtype);
signed char FSOUND_SetDriver(int driver);
signed char FSOUND_SetSpeakerMode(unsigned int speakermode);
signed char FSOUND_SetHWND(void* hwnd);
void        FSOUND_SetSFXMasterVolume(int volume);
void        FSOUND_Update(void);

// --- System queries ---
int         FSOUND_GetMaxChannels(void);
int         FSOUND_GetNumHardwareChannels(int* num2d, int* num3d, int* total);
int         FSOUND_GetChannelsPlaying(void);
int         FSOUND_GetNumDrivers(void);
const char* FSOUND_GetDriverName(int id);
signed char FSOUND_GetDriverCaps(int id, unsigned int* caps);
int         FSOUND_GetMixer(void);
float       FSOUND_GetVersion(void);
int         FSOUND_GetError(void);

// --- Sample API ---
FSOUND_SAMPLE* FSOUND_Sample_Load(int index, const char* name,
                                  unsigned int mode, int offset, int length);
void           FSOUND_Sample_Free(FSOUND_SAMPLE* sptr);
signed char    FSOUND_Sample_SetMode(FSOUND_SAMPLE* sptr, unsigned int mode);
signed char    FSOUND_Sample_SetDefaults(FSOUND_SAMPLE* sptr,
                                          int deffreq, int defvol,
                                          int defpan, int defpri);
signed char    FSOUND_Sample_SetMinMaxDistance(FSOUND_SAMPLE* sptr,
                                                float min, float max);
unsigned int   FSOUND_Sample_GetLength(FSOUND_SAMPLE* sptr);

// --- Channel playback ---
int            FSOUND_PlaySound(int channel, FSOUND_SAMPLE* sptr);
int            FSOUND_PlaySoundEx(int channel, FSOUND_SAMPLE* sptr,
                                   void* dsp, signed char paused);
signed char    FSOUND_StopSound(int channel);
signed char    FSOUND_SetVolume(int channel, int vol);
signed char    FSOUND_SetVolumeAbsolute(int channel, int vol);
int            FSOUND_GetVolume(int channel);
signed char    FSOUND_SetPaused(int channel, signed char paused);
signed char    FSOUND_IsPlaying(int channel);
signed char    FSOUND_SetPan(int channel, int pan);
unsigned int   FSOUND_GetCurrentPosition(int channel);
int            FSOUND_GetPriority(int channel);
unsigned int   FSOUND_GetLoopMode(int channel);

// --- Stream API ---
FSOUND_STREAM* FSOUND_Stream_Open(const char* name, unsigned int mode,
                                   int offset, int length);
FSOUND_STREAM* FSOUND_Stream_OpenFile(const char* name, unsigned int mode,
                                       int* length);
signed char    FSOUND_Stream_Close(FSOUND_STREAM* stream);
int            FSOUND_Stream_Play(int channel, FSOUND_STREAM* stream);
signed char    FSOUND_Stream_Stop(FSOUND_STREAM* stream);
signed char    FSOUND_Stream_SetTime(FSOUND_STREAM* stream, int ms);
int            FSOUND_Stream_GetTime(FSOUND_STREAM* stream);
signed char    FSOUND_Stream_SetPosition(FSOUND_STREAM* stream,
                                          unsigned int position);
signed char    FSOUND_Stream_SetSynchCallback(FSOUND_STREAM* stream,
                                               FSOUND_STREAMCALLBACK cb,
                                               void* userdata);

// --- Error helper ---
const char* FMOD_ErrorString(int errcode);

#ifdef __cplusplus
}
#endif
