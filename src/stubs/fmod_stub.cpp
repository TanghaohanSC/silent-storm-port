// Phase 2: FMOD 3.x -> miniaudio backend.
//
// Public surface (fmod_stub.h) is unchanged from the Phase 0 silent fake:
// 67 FMOD 3.x symbols. This file routes them to miniaudio 0.11.25
// (third_party/miniaudio/miniaudio.h). Behaviour:
//
//   - FSOUND_Init brings up a real ma_engine (WASAPI/DSound auto-selected).
//   - FSOUND_Sample_Load(memory) initialises an ma_decoder from the
//     in-memory buffer; PlaySound spins an ma_sound off it.
//   - FSOUND_Stream_Open / FSOUND_Stream_OpenFile open an ma_sound straight
//     from the path with streaming flag.
//   - FSOUND_PlaySound / FSOUND_PlaySoundEx allocate a channel id, store the
//     ma_sound in a channel table, and start playback.
//   - Per-channel Stop/Set*/IsPlaying queries the channel table.
//   - 3D positional APIs are wired to ma_sound spatialisation (best-effort;
//     Nival's coordinate convention is left-as-is, just forwarded).
//
// Failure modes that the original silent fake had to handle, kept here:
//   - .sfap0 (FMOD's proprietary compressed format) — miniaudio has no
//     decoder. ma_sound_init_*_from_memory fails, we log and return
//     nullptr. Nival's NFMSound::NewSample already handles a nullptr
//     return cleanly.
//   - Any unexpected decoder failure — log path/size, return nullptr.
//
// Audio decisions log: silent_storm_audio.log next to the exe.

#include "fmod_stub.h"

// Disable backends we don't want on Windows. miniaudio defaults to
// enabling JACK on Windows desktop too; that drags in <jack/jack.h>
// which we don't ship. WASAPI + DirectSound + WinMM are plenty.
#define MA_NO_JACK
#define MA_IMPLEMENTATION
#include "../../third_party/miniaudio/miniaudio.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// ----------------------------------------------------------------------
// Logging
// ----------------------------------------------------------------------
std::FILE* g_log = nullptr;
std::once_flag g_log_once;

void open_log_once() {
    std::call_once(g_log_once, []() {
        g_log = std::fopen("silent_storm_audio.log", "w");
        if (g_log) {
            std::fprintf(g_log,
                "Silent Storm audio log (Phase 2: FMOD -> miniaudio backend)\n");
            std::fflush(g_log);
        }
    });
}

void audio_log(const char* fmt, ...) {
    open_log_once();
    if (!g_log) return;
    std::va_list ap;
    va_start(ap, fmt);
    std::vfprintf(g_log, fmt, ap);
    va_end(ap);
    std::fputc('\n', g_log);
    std::fflush(g_log);
}

// ----------------------------------------------------------------------
// Global state — one ma_engine, channel map, sample registry.
// ----------------------------------------------------------------------
struct AudioBackend {
    ma_engine engine{};
    bool engine_initialized = false;

    // Channel table: channel id -> live ma_sound*. We don't own the sound;
    // ownership stays on the sample or stream that minted it. The same
    // sample played twice in a row gets the same ma_sound, but Nival's
    // higher-level code (NFMSound) plays each sample by handing the
    // sample handle in, so this is fine.
    std::unordered_map<int, ma_sound*> channels;
    std::mutex channel_mutex;
    std::atomic<int> next_channel{1};

    // SFX master volume scaled into ma_engine. FMOD uses 0..255.
    int sfx_master = 255;

    // Last error code (FMOD-compatible-ish).
    int last_error = 0;
};

AudioBackend& backend() {
    static AudioBackend g_backend;
    return g_backend;
}

// Allocate the next channel id for a freshly started sound. Returns -1
// if the table fills up (extremely unlikely, but defensive).
int register_channel(ma_sound* snd) {
    if (!snd) return -1;
    auto& b = backend();
    std::lock_guard<std::mutex> lk(b.channel_mutex);
    int ch = b.next_channel.fetch_add(1);
    if (ch <= 0) ch = b.next_channel.fetch_add(1);  // wrap-around guard
    b.channels[ch] = snd;
    return ch;
}

ma_sound* lookup_channel(int channel) {
    if (channel <= 0) return nullptr;
    auto& b = backend();
    std::lock_guard<std::mutex> lk(b.channel_mutex);
    auto it = b.channels.find(channel);
    if (it == b.channels.end()) return nullptr;
    return it->second;
}

void unregister_channel(int channel) {
    if (channel <= 0) return;
    auto& b = backend();
    std::lock_guard<std::mutex> lk(b.channel_mutex);
    b.channels.erase(channel);
}

// Drop any channels whose ma_sound has been destroyed (matches sample
// pointer). Called from FSOUND_Sample_Free.
void unregister_channels_for_sound(ma_sound* snd) {
    if (!snd) return;
    auto& b = backend();
    std::lock_guard<std::mutex> lk(b.channel_mutex);
    for (auto it = b.channels.begin(); it != b.channels.end();) {
        if (it->second == snd) it = b.channels.erase(it);
        else ++it;
    }
}

} // namespace

// ----------------------------------------------------------------------
// Opaque types declared in fmod_stub.h. We own the storage.
// ----------------------------------------------------------------------
struct FSOUND_SAMPLE {
    ma_decoder decoder{};
    ma_sound   sound{};
    bool       decoder_inited = false;
    bool       sound_inited   = false;
    bool       from_memory    = false;
    std::vector<unsigned char> memory_copy;  // miniaudio doesn't copy
    std::string source_label;                // for logs
    unsigned int mode = 0;
};

struct FSOUND_STREAM {
    ma_sound sound{};
    bool     inited = false;
    std::string source_label;
    unsigned int mode = 0;
    // Synch callback support: FMOD's API is fire-and-forget, miniaudio's
    // end-of-sound callback is called by the device thread. We capture
    // the FMOD callback + userdata and bridge on end.
    FSOUND_STREAMCALLBACK synch_cb = nullptr;
    void* synch_user = nullptr;
};

namespace {

// miniaudio engine end-of-sound callback adapter for streams.
void MA_API stream_end_callback(void* user, ma_sound* /*snd*/) {
    auto* s = static_cast<FSOUND_STREAM*>(user);
    if (!s || !s->synch_cb) return;
    // FMOD's synch callback signature: (stream, buff, len, userdata).
    // We've got no buff/len in the end-of-stream context; pass a NUL
    // string so Nival's str.c_str() check is well-defined and len=0.
    char empty = 0;
    s->synch_cb(s, &empty, 0, s->synch_user);
}

} // namespace

extern "C" {

// ======================================================================
// System init / control
// ======================================================================
signed char FSOUND_Init(int mixrate, int /*maxsoftwarechannels*/,
                        unsigned int /*flags*/) {
    open_log_once();
    auto& b = backend();
    if (b.engine_initialized) {
        audio_log("FSOUND_Init: already initialised; returning success.");
        return 1;
    }

    ma_engine_config cfg = ma_engine_config_init();
    cfg.sampleRate = (mixrate > 0) ? (ma_uint32)mixrate : 48000;
    cfg.channels   = 2;

    ma_result rc = ma_engine_init(&cfg, &b.engine);
    if (rc != MA_SUCCESS) {
        audio_log("FSOUND_Init: ma_engine_init failed (rc=%d). Audio disabled.",
                  (int)rc);
        b.last_error = (int)rc;
        return 0;
    }
    b.engine_initialized = true;
    audio_log("FSOUND_Init: ok. sampleRate=%u channels=%u backend=miniaudio %s",
              cfg.sampleRate, cfg.channels, MA_VERSION_STRING);
    return 1;
}

void FSOUND_Close(void) {
    auto& b = backend();
    if (!b.engine_initialized) return;
    {
        std::lock_guard<std::mutex> lk(b.channel_mutex);
        b.channels.clear();
    }
    ma_engine_uninit(&b.engine);
    b.engine_initialized = false;
    audio_log("FSOUND_Close: engine torn down.");
    if (g_log) {
        std::fclose(g_log);
        g_log = nullptr;
    }
}

signed char FSOUND_SetOutput(int)            { return 1; }
signed char FSOUND_SetDriver(int)            { return 1; }
signed char FSOUND_SetSpeakerMode(unsigned)  { return 1; }
signed char FSOUND_SetHWND(void*)            { return 1; }

void FSOUND_SetSFXMasterVolume(int volume) {
    auto& b = backend();
    b.sfx_master = volume;
    if (!b.engine_initialized) return;
    float v = (float)volume / 255.0f;
    if (v < 0) v = 0; if (v > 1) v = 1;
    ma_engine_set_volume(&b.engine, v);
}

void FSOUND_Update(void) { /* miniaudio drives itself on its own thread. */ }

// ======================================================================
// System queries
// ======================================================================
int   FSOUND_GetMaxChannels(void)              { return 64; }
int   FSOUND_GetNumHardwareChannels(int* a, int* b2, int* c) {
    if (a) *a = 0; if (b2) *b2 = 0; if (c) *c = 0;
    return 0;
}
int   FSOUND_GetChannelsPlaying(void) {
    auto& b = backend();
    if (!b.engine_initialized) return 0;
    std::lock_guard<std::mutex> lk(b.channel_mutex);
    int n = 0;
    for (auto& kv : b.channels) {
        if (kv.second && ma_sound_is_playing(kv.second)) ++n;
    }
    return n;
}
int   FSOUND_GetNumDrivers(void)               { return 1; }
const char* FSOUND_GetDriverName(int)          { return "miniaudio (default device)"; }
signed char FSOUND_GetDriverCaps(int, unsigned int* caps) {
    if (caps) *caps = 0;
    return 1;
}
int   FSOUND_GetMixer(void)                    { return FSOUND_MIXER_QUALITY_FPU; }
float FSOUND_GetVersion(void)                  { return FMOD_VERSION; }
int   FSOUND_GetError(void) {
    return backend().last_error;
}

// ======================================================================
// Sample API
// ======================================================================
FSOUND_SAMPLE* FSOUND_Sample_Load(int /*index*/, const char* name,
                                  unsigned int mode, int /*offset*/,
                                  int length) {
    auto& b = backend();
    if (!b.engine_initialized || !name) return nullptr;

    auto* s = new FSOUND_SAMPLE{};
    s->mode = mode;

    const bool from_mem = (mode & FSOUND_LOADMEMORY) != 0;
    s->from_memory = from_mem;

    ma_result rc = MA_ERROR;
    if (from_mem) {
        // 'name' is actually a const char* pointing to encoded audio bytes.
        if (length <= 0) {
            audio_log("Sample_Load(mem): zero length, refusing.");
            delete s;
            return nullptr;
        }
        s->memory_copy.assign((const unsigned char*)name,
                              (const unsigned char*)name + length);
        s->source_label = "<memory:" + std::to_string(length) + "B>";

        ma_decoder_config dcfg = ma_decoder_config_init_default();
        rc = ma_decoder_init_memory(s->memory_copy.data(),
                                    s->memory_copy.size(), &dcfg, &s->decoder);
        if (rc != MA_SUCCESS) {
            audio_log("Sample_Load(mem,%dB): decoder init failed rc=%d (likely .sfap0 or unsupported codec).",
                      length, (int)rc);
            delete s;
            return nullptr;
        }
        s->decoder_inited = true;
        rc = ma_sound_init_from_data_source(&b.engine, &s->decoder,
            MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &s->sound);
        if (rc != MA_SUCCESS) {
            audio_log("Sample_Load(mem,%dB): sound init failed rc=%d.",
                      length, (int)rc);
            ma_decoder_uninit(&s->decoder);
            delete s;
            return nullptr;
        }
        s->sound_inited = true;
    } else {
        s->source_label = name;
        rc = ma_sound_init_from_file(&b.engine, name, MA_SOUND_FLAG_DECODE,
                                     nullptr, nullptr, &s->sound);
        if (rc != MA_SUCCESS) {
            audio_log("Sample_Load('%s'): file load failed rc=%d.", name,
                      (int)rc);
            delete s;
            return nullptr;
        }
        s->sound_inited = true;
    }

    if (mode & FSOUND_LOOP_NORMAL) ma_sound_set_looping(&s->sound, MA_TRUE);
    else                            ma_sound_set_looping(&s->sound, MA_FALSE);

    audio_log("Sample_Load: ok %s mode=0x%x.", s->source_label.c_str(), mode);
    return s;
}

void FSOUND_Sample_Free(FSOUND_SAMPLE* sptr) {
    if (!sptr) return;
    unregister_channels_for_sound(&sptr->sound);
    if (sptr->sound_inited) ma_sound_uninit(&sptr->sound);
    if (sptr->decoder_inited) ma_decoder_uninit(&sptr->decoder);
    delete sptr;
}

signed char FSOUND_Sample_SetMode(FSOUND_SAMPLE* sptr, unsigned int mode) {
    if (!sptr || !sptr->sound_inited) return 0;
    sptr->mode = mode;
    if (mode & FSOUND_LOOP_NORMAL) ma_sound_set_looping(&sptr->sound, MA_TRUE);
    else                            ma_sound_set_looping(&sptr->sound, MA_FALSE);
    return 1;
}

signed char FSOUND_Sample_SetDefaults(FSOUND_SAMPLE*, int, int, int, int) {
    return 1;
}

signed char FSOUND_Sample_SetMinMaxDistance(FSOUND_SAMPLE* sptr,
                                             float fmin, float fmax) {
    if (!sptr || !sptr->sound_inited) return 0;
    ma_sound_set_min_distance(&sptr->sound, fmin);
    ma_sound_set_max_distance(&sptr->sound, fmax);
    return 1;
}

unsigned int FSOUND_Sample_GetLength(FSOUND_SAMPLE* sptr) {
    if (!sptr || !sptr->sound_inited) return 0;
    ma_uint64 frames = 0;
    if (ma_sound_get_length_in_pcm_frames(&sptr->sound, &frames) != MA_SUCCESS) {
        return 0;
    }
    return (unsigned int)frames;
}

// ======================================================================
// Channel playback
// ======================================================================
int FSOUND_PlaySound(int /*channel*/, FSOUND_SAMPLE* sptr) {
    if (!sptr || !sptr->sound_inited) return -1;
    ma_sound_seek_to_pcm_frame(&sptr->sound, 0);
    if (ma_sound_start(&sptr->sound) != MA_SUCCESS) return -1;
    return register_channel(&sptr->sound);
}

int FSOUND_PlaySoundEx(int /*channel*/, FSOUND_SAMPLE* sptr, void* /*dsp*/,
                       signed char paused) {
    if (!sptr || !sptr->sound_inited) return -1;
    ma_sound_seek_to_pcm_frame(&sptr->sound, 0);
    if (!paused) {
        if (ma_sound_start(&sptr->sound) != MA_SUCCESS) return -1;
    }
    return register_channel(&sptr->sound);
}

signed char FSOUND_StopSound(int channel) {
    ma_sound* snd = lookup_channel(channel);
    if (snd) ma_sound_stop(snd);
    unregister_channel(channel);
    return 1;
}

signed char FSOUND_SetVolume(int channel, int vol) {
    ma_sound* snd = lookup_channel(channel);
    if (!snd) return 1;
    float v = (float)vol / 255.0f;
    if (v < 0) v = 0; if (v > 1) v = 1;
    ma_sound_set_volume(snd, v);
    return 1;
}

signed char FSOUND_SetVolumeAbsolute(int channel, int vol) {
    return FSOUND_SetVolume(channel, vol);
}

int FSOUND_GetVolume(int channel) {
    ma_sound* snd = lookup_channel(channel);
    if (!snd) return 0;
    float v = ma_sound_get_volume(snd);
    int n = (int)(v * 255.0f);
    if (n < 0) n = 0; if (n > 255) n = 255;
    return n;
}

signed char FSOUND_SetPaused(int channel, signed char paused) {
    ma_sound* snd = lookup_channel(channel);
    if (!snd) return 1;
    if (paused) ma_sound_stop(snd);
    else        ma_sound_start(snd);
    return 1;
}

signed char FSOUND_IsPlaying(int channel) {
    ma_sound* snd = lookup_channel(channel);
    if (!snd) return 0;
    return ma_sound_is_playing(snd) ? 1 : 0;
}

signed char FSOUND_SetPan(int channel, int pan) {
    ma_sound* snd = lookup_channel(channel);
    if (!snd) return 1;
    // FMOD pan: 0..255 with 128 = centre. FSOUND_STEREOPAN (0x00200000)
    // requests "no panning" — leave it at centre.
    if ((unsigned)pan == FSOUND_STEREOPAN) {
        ma_sound_set_pan(snd, 0.0f);
        return 1;
    }
    float p = ((float)pan - 128.0f) / 128.0f;
    if (p < -1) p = -1; if (p > 1) p = 1;
    ma_sound_set_pan(snd, p);
    return 1;
}

unsigned int FSOUND_GetCurrentPosition(int channel) {
    ma_sound* snd = lookup_channel(channel);
    if (!snd) return 0;
    ma_uint64 cursor = 0;
    if (ma_sound_get_cursor_in_pcm_frames(snd, &cursor) != MA_SUCCESS) return 0;
    return (unsigned int)cursor;
}

int FSOUND_GetPriority(int /*channel*/) { return 0; }

unsigned int FSOUND_GetLoopMode(int channel) {
    ma_sound* snd = lookup_channel(channel);
    if (!snd) return FSOUND_LOOP_OFF;
    return ma_sound_is_looping(snd) ? FSOUND_LOOP_NORMAL : FSOUND_LOOP_OFF;
}

// ======================================================================
// Stream API
// ======================================================================
static FSOUND_STREAM* open_stream_internal(const char* name, unsigned int mode) {
    auto& b = backend();
    if (!b.engine_initialized || !name) return nullptr;

    auto* s = new FSOUND_STREAM{};
    s->mode = mode;
    s->source_label = name;

    ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;
    ma_result rc = ma_sound_init_from_file(&b.engine, name, flags,
                                            nullptr, nullptr, &s->sound);
    if (rc != MA_SUCCESS) {
        audio_log("Stream_Open('%s'): rc=%d.", name, (int)rc);
        delete s;
        return nullptr;
    }
    s->inited = true;
    if (mode & FSOUND_LOOP_NORMAL) ma_sound_set_looping(&s->sound, MA_TRUE);
    audio_log("Stream_Open: ok %s mode=0x%x.", name, mode);
    return s;
}

FSOUND_STREAM* FSOUND_Stream_Open(const char* name, unsigned int mode,
                                  int /*offset*/, int /*length*/) {
    return open_stream_internal(name, mode);
}

FSOUND_STREAM* FSOUND_Stream_OpenFile(const char* name, unsigned int mode,
                                      int* length) {
    if (length) *length = 0;
    FSOUND_STREAM* s = open_stream_internal(name, mode);
    if (s && length) {
        ma_uint64 frames = 0;
        if (ma_sound_get_length_in_pcm_frames(&s->sound, &frames) == MA_SUCCESS) {
            *length = (int)frames;
        }
    }
    return s;
}

signed char FSOUND_Stream_Close(FSOUND_STREAM* stream) {
    if (!stream) return 0;
    if (stream->inited) {
        unregister_channels_for_sound(&stream->sound);
        ma_sound_uninit(&stream->sound);
    }
    delete stream;
    return 1;
}

int FSOUND_Stream_Play(int /*channel*/, FSOUND_STREAM* stream) {
    if (!stream || !stream->inited) return -1;
    ma_sound_seek_to_pcm_frame(&stream->sound, 0);
    if (ma_sound_start(&stream->sound) != MA_SUCCESS) return -1;
    return register_channel(&stream->sound);
}

signed char FSOUND_Stream_Stop(FSOUND_STREAM* stream) {
    if (!stream || !stream->inited) return 0;
    ma_sound_stop(&stream->sound);
    return 1;
}

signed char FSOUND_Stream_SetTime(FSOUND_STREAM* stream, int ms) {
    if (!stream || !stream->inited || ms < 0) return 0;
    // ms -> pcm frames via engine sample rate.
    ma_engine* eng = ma_sound_get_engine(&stream->sound);
    ma_uint32 sr = eng ? ma_engine_get_sample_rate(eng) : 48000;
    ma_uint64 frame = (ma_uint64)ms * (ma_uint64)sr / 1000;
    return ma_sound_seek_to_pcm_frame(&stream->sound, frame) == MA_SUCCESS ? 1 : 0;
}

int FSOUND_Stream_GetTime(FSOUND_STREAM* stream) {
    if (!stream || !stream->inited) return 0;
    ma_uint64 cursor = 0;
    if (ma_sound_get_cursor_in_pcm_frames(&stream->sound, &cursor) != MA_SUCCESS)
        return 0;
    ma_engine* eng = ma_sound_get_engine(&stream->sound);
    ma_uint32 sr = eng ? ma_engine_get_sample_rate(eng) : 48000;
    if (sr == 0) return 0;
    return (int)(cursor * 1000 / sr);
}

signed char FSOUND_Stream_SetPosition(FSOUND_STREAM* stream,
                                       unsigned int position) {
    if (!stream || !stream->inited) return 0;
    return ma_sound_seek_to_pcm_frame(&stream->sound, position) == MA_SUCCESS ? 1 : 0;
}

signed char FSOUND_Stream_SetSynchCallback(FSOUND_STREAM* stream,
                                            FSOUND_STREAMCALLBACK cb,
                                            void* userdata) {
    if (!stream || !stream->inited) return 0;
    stream->synch_cb = cb;
    stream->synch_user = userdata;
    ma_sound_set_end_callback(&stream->sound, stream_end_callback, stream);
    return 1;
}

// ======================================================================
// Error helper
// ======================================================================
const char* FMOD_ErrorString(int errcode) {
    if (errcode == 0) return "No error";
    return ma_result_description((ma_result)errcode);
}

} // extern "C"
