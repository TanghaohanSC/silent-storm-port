// video.cpp — Phase 3
//
// Minimal FFmpeg-backed video playback for the Silent Storm port.
//
// FFmpeg dependency is brought in through vcpkg (`ffmpeg` port, the
// `avcodec`/`avformat`/`swscale`/`swresample` features).  We talk to FFmpeg
// via its C API (libavcodec / libavformat / libswscale) and *not* through
// the C++ wrappers — keeps build/headers simple.
//
// Per-frame loop:
//   1. av_read_frame to get a compressed packet from the input file
//   2. avcodec_send_packet to feed the decoder
//   3. avcodec_receive_frame to pull a decoded YUV/whatever AVFrame
//   4. sws_scale to convert to packed BGRA8 (bgfx's BGRA8 layout)
//   5. bgfx::updateTexture2D to push the pixels to the GPU
//   6. submit a fullscreen quad through the ss_ui shader archetype
//   7. bgfx::frame + pump SDL3 events (ESC / click / close = early-out)
//
// Audio is intentionally out of scope here (per Phase 3 spec: "Audio sync —
// out of scope"); the video plays silently.  Loose frame pacing — we sleep
// just enough to keep ~stream timebase, but no PTS-precise sync.
#include "video.h"

#include "bgfx_init.h"
#include "shader_registry.h"

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>

#include <SDL3/SDL.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>

#if defined(SS_HAVE_FFMPEG)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libavutil/rational.h>
}
#endif

namespace {

FILE* open_log() {
    static FILE* f = nullptr;
    if (!f) fopen_s(&f, "silent_storm_video.log", "w");
    return f;
}

void log_line(const char* fmt, ...) {
    FILE* f = open_log();
    if (!f) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fflush(f);
}

} // namespace

namespace silent_storm::renderer {

#if !defined(SS_HAVE_FFMPEG)

bool video_backend_available() { return false; }

bool play_video(const char* path) {
    log_line("[video] play_video(\"%s\"): Phase 3.5 — FFmpeg not yet linked",
             path ? path : "<null>");
    return false;
}

#else // SS_HAVE_FFMPEG

bool video_backend_available() { return true; }

namespace {

// Drain SDL events; return false if the user wants to stop the video.
bool poll_continue() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                return false;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.key == SDLK_ESCAPE ||
                    ev.key.key == SDLK_RETURN ||
                    ev.key.key == SDLK_SPACE) {
                    return false;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                return false;
            case SDL_EVENT_WINDOW_RESIZED: {
                int w = ev.window.data1;
                int h = ev.window.data2;
                if (w > 0 && h > 0) on_resize(w, h);
                break;
            }
            default:
                break;
        }
    }
    return true;
}

struct VideoState {
    AVFormatContext* fmt_ctx     = nullptr;
    AVCodecContext*  codec_ctx   = nullptr;
    SwsContext*      sws         = nullptr;
    AVFrame*         frame       = nullptr;  // decoded (codec native pixfmt)
    AVPacket*        pkt         = nullptr;
    int              video_stream = -1;
    int              width        = 0;
    int              height       = 0;
    uint8_t*         bgra_buf     = nullptr; // staging buffer for sws_scale dst
    int              bgra_stride  = 0;
    bgfx::TextureHandle tex       = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout layout;
    bool             layout_init  = false;
    AVRational       time_base    = {0, 1};
    double           frame_rate   = 0.0;

    ~VideoState() { cleanup(); }

    void cleanup() {
        if (bgfx::isValid(tex)) {
            bgfx::destroy(tex);
            tex = BGFX_INVALID_HANDLE;
        }
        if (sws) {
            sws_freeContext(sws);
            sws = nullptr;
        }
        if (frame) av_frame_free(&frame);
        if (pkt)   av_packet_free(&pkt);
        if (codec_ctx) avcodec_free_context(&codec_ctx);
        if (fmt_ctx) {
            avformat_close_input(&fmt_ctx);
        }
        if (bgra_buf) {
            av_freep(&bgra_buf);
        }
    }
};

bool open_input(VideoState& st, const char* path) {
    st.fmt_ctx = nullptr;
    int rc = avformat_open_input(&st.fmt_ctx, path, nullptr, nullptr);
    if (rc < 0) {
        char err[256] = {};
        av_strerror(rc, err, sizeof(err));
        log_line("[video] avformat_open_input(\"%s\") failed: %s", path, err);
        return false;
    }
    rc = avformat_find_stream_info(st.fmt_ctx, nullptr);
    if (rc < 0) {
        log_line("[video] avformat_find_stream_info failed");
        return false;
    }
    st.video_stream = -1;
    for (unsigned i = 0; i < st.fmt_ctx->nb_streams; ++i) {
        if (st.fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            st.video_stream = (int)i;
            break;
        }
    }
    if (st.video_stream < 0) {
        log_line("[video] no video stream found in %s", path);
        return false;
    }
    AVStream* s = st.fmt_ctx->streams[st.video_stream];
    const AVCodec* codec = avcodec_find_decoder(s->codecpar->codec_id);
    if (!codec) {
        log_line("[video] no decoder for codec_id=%d (%s)",
                 (int)s->codecpar->codec_id,
                 avcodec_get_name(s->codecpar->codec_id));
        return false;
    }
    log_line("[video] decoder found: %s (codec_id=%d)",
             codec->name, (int)s->codecpar->codec_id);

    st.codec_ctx = avcodec_alloc_context3(codec);
    if (!st.codec_ctx) {
        log_line("[video] avcodec_alloc_context3 failed");
        return false;
    }
    if (avcodec_parameters_to_context(st.codec_ctx, s->codecpar) < 0) {
        log_line("[video] avcodec_parameters_to_context failed");
        return false;
    }
    if (avcodec_open2(st.codec_ctx, codec, nullptr) < 0) {
        log_line("[video] avcodec_open2 failed");
        return false;
    }

    st.width  = st.codec_ctx->width;
    st.height = st.codec_ctx->height;
    st.time_base = s->time_base;
    if (s->avg_frame_rate.num > 0 && s->avg_frame_rate.den > 0) {
        st.frame_rate = (double)s->avg_frame_rate.num / (double)s->avg_frame_rate.den;
    } else if (s->r_frame_rate.num > 0 && s->r_frame_rate.den > 0) {
        st.frame_rate = (double)s->r_frame_rate.num / (double)s->r_frame_rate.den;
    } else {
        st.frame_rate = 30.0;
    }
    log_line("[video] %dx%d  pixfmt=%s  ~%.2f fps",
             st.width, st.height,
             av_get_pix_fmt_name(st.codec_ctx->pix_fmt),
             st.frame_rate);

    st.frame = av_frame_alloc();
    st.pkt   = av_packet_alloc();
    if (!st.frame || !st.pkt) {
        log_line("[video] av_frame_alloc / av_packet_alloc failed");
        return false;
    }

    // BGRA staging buffer (bgfx::TextureFormat::BGRA8 layout).
    int dst_linesize[4] = {};
    uint8_t* dst_data[4] = {};
    int bufsize = av_image_alloc(dst_data, dst_linesize,
                                 st.width, st.height,
                                 AV_PIX_FMT_BGRA, 1);
    if (bufsize < 0) {
        log_line("[video] av_image_alloc failed");
        return false;
    }
    st.bgra_buf    = dst_data[0];
    st.bgra_stride = dst_linesize[0];

    st.sws = sws_getContext(st.width, st.height, st.codec_ctx->pix_fmt,
                            st.width, st.height, AV_PIX_FMT_BGRA,
                            SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!st.sws) {
        log_line("[video] sws_getContext failed");
        return false;
    }

    return true;
}

bool create_texture(VideoState& st) {
    st.tex = bgfx::createTexture2D(
        static_cast<uint16_t>(st.width),
        static_cast<uint16_t>(st.height),
        false, 1,
        bgfx::TextureFormat::BGRA8,
        BGFX_TEXTURE_NONE |
            BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
            BGFX_SAMPLER_U_CLAMP   | BGFX_SAMPLER_V_CLAMP);
    if (!bgfx::isValid(st.tex)) {
        log_line("[video] bgfx::createTexture2D failed");
        return false;
    }
    return true;
}

void init_layout(VideoState& st) {
    if (st.layout_init) return;
    st.layout
        .begin()
        .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
        .end();
    st.layout_init = true;
}

// Submit one fullscreen quad textured with `st.tex`, letterboxed to keep
// the source aspect ratio against the current bgfx framebuffer size.
void submit_quad(VideoState& st) {
    init_layout(st);

    const int fb_w = get_width();
    const int fb_h = get_height();
    if (fb_w <= 0 || fb_h <= 0) return;

    // Compute letterbox / pillarbox in NDC.
    const float src_aspect = (float)st.width / (float)st.height;
    const float dst_aspect = (float)fb_w / (float)fb_h;
    float nx0 = -1.0f, ny0 = -1.0f, nx1 = 1.0f, ny1 = 1.0f;
    if (src_aspect > dst_aspect) {
        // Source is wider: full width, pillarboxed height.
        float h_scale = dst_aspect / src_aspect;
        ny0 = -h_scale; ny1 = h_scale;
    } else {
        float w_scale = src_aspect / dst_aspect;
        nx0 = -w_scale; nx1 = w_scale;
    }

    // bgfx textures use top-left origin in our path (D3D-style Y); the
    // ss_ui shader samples v_texcoord0 directly so flipping V here flips
    // the image right side up on D3D11/Vulkan.
    struct V { float x, y, z; float u, v; uint32_t abgr; };
    constexpr int kVerts = 6;
    if (bgfx::getAvailTransientVertexBuffer(kVerts, st.layout) < kVerts) return;

    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, kVerts, st.layout);
    V* dst = reinterpret_cast<V*>(tvb.data);
    const uint32_t white = 0xffffffffu;
    dst[0] = { nx0, ny1, 0.0f, 0.0f, 0.0f, white };
    dst[1] = { nx1, ny1, 0.0f, 1.0f, 0.0f, white };
    dst[2] = { nx1, ny0, 0.0f, 1.0f, 1.0f, white };
    dst[3] = { nx0, ny1, 0.0f, 0.0f, 0.0f, white };
    dst[4] = { nx1, ny0, 0.0f, 1.0f, 1.0f, white };
    dst[5] = { nx0, ny0, 0.0f, 0.0f, 1.0f, white };

    float ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    bgfx::setViewTransform(0, ident, ident);
    bgfx::setTransform(ident);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setVertexBuffer(0, &tvb, 0, kVerts);
    bgfx::setTexture(0, get_sampler_uniform(0), st.tex);

    bgfx::ProgramHandle prog = get_program("ss_ui");
    if (bgfx::isValid(prog)) bgfx::submit(0, prog);
    else                     bgfx::discard();
}

// Receive zero or more decoded frames after a packet has been sent.
// `flush` selects whether avcodec_receive_frame is allowed to return EOF.
// Returns 1 = at least one frame uploaded + drawn, 0 = nothing yet (try
// more input), -1 = EOF, < -1 = hard error.
int drain_decoder(VideoState& st, bool& kept_going) {
    int got_any = 0;
    for (;;) {
        int rc = avcodec_receive_frame(st.codec_ctx, st.frame);
        if (rc == AVERROR(EAGAIN)) return got_any;          // need more input
        if (rc == AVERROR_EOF)    return -1;
        if (rc < 0) {
            char err[256] = {};
            av_strerror(rc, err, sizeof(err));
            log_line("[video] avcodec_receive_frame: %s", err);
            return -2;
        }

        // Convert to BGRA into the staging buffer.
        uint8_t* dst_data[4]    = { st.bgra_buf, nullptr, nullptr, nullptr };
        int      dst_linesize[4]= { st.bgra_stride, 0, 0, 0 };
        sws_scale(st.sws,
                  st.frame->data, st.frame->linesize, 0, st.height,
                  dst_data, dst_linesize);

        const uint32_t bytes = static_cast<uint32_t>(st.bgra_stride) *
                               static_cast<uint32_t>(st.height);
        const bgfx::Memory* mem = bgfx::copy(st.bgra_buf, bytes);
        bgfx::updateTexture2D(st.tex, 0, 0, 0, 0,
                              static_cast<uint16_t>(st.width),
                              static_cast<uint16_t>(st.height),
                              mem,
                              static_cast<uint16_t>(st.bgra_stride));

        bgfx::touch(0);
        submit_quad(st);
        bgfx::frame();
        ++got_any;

        // Loose pacing: nominal 1/fps sleep.  We do this *between* receive
        // calls so b-frame bursts don't stutter — naive but matches "no
        // audio sync" Phase-3 scope.
        if (st.frame_rate > 0.0) {
            int ms = (int)(1000.0 / st.frame_rate);
            if (ms > 0 && ms < 100) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
        }

        if (!poll_continue()) {
            kept_going = false;
            return -1;
        }
    }
}

} // namespace

bool play_video(const char* path) {
    if (!path || !*path) {
        log_line("[video] play_video: empty path");
        return false;
    }
    log_line("[video] play_video(\"%s\")", path);

    VideoState st;
    if (!open_input(st, path))      return false;
    if (!create_texture(st))         return false;

    // Initial clear so we don't show garbage on slow first decode.
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
    bgfx::touch(0);
    bgfx::frame();

    bool kept_going = true;
    int frames = 0;
    for (;;) {
        if (!poll_continue()) { kept_going = false; break; }

        int rc = av_read_frame(st.fmt_ctx, st.pkt);
        if (rc == AVERROR_EOF) {
            // Flush decoder.
            avcodec_send_packet(st.codec_ctx, nullptr);
            int drained = drain_decoder(st, kept_going);
            if (drained > 0) frames += drained;
            break;
        }
        if (rc < 0) {
            char err[256] = {};
            av_strerror(rc, err, sizeof(err));
            log_line("[video] av_read_frame: %s", err);
            break;
        }

        if (st.pkt->stream_index == st.video_stream) {
            rc = avcodec_send_packet(st.codec_ctx, st.pkt);
            if (rc < 0 && rc != AVERROR(EAGAIN)) {
                char err[256] = {};
                av_strerror(rc, err, sizeof(err));
                log_line("[video] avcodec_send_packet: %s", err);
                av_packet_unref(st.pkt);
                break;
            }
            int drained = drain_decoder(st, kept_going);
            if (drained > 0) frames += drained;
            if (!kept_going) {
                av_packet_unref(st.pkt);
                break;
            }
        }
        av_packet_unref(st.pkt);
    }

    log_line("[video] play_video done: frames=%d completed=%d",
             frames, kept_going ? 1 : 0);
    return kept_going;
}

#endif // SS_HAVE_FFMPEG

} // namespace silent_storm::renderer
