#include "bgfx_init.h"
#include "shader_registry.h"
#include "../config/config.h"
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <cstdarg>
#include <cstdio>
#include <string>

namespace {
FILE* open_log() {
    static FILE* f = nullptr;
    if (!f) { fopen_s(&f, "silent_storm_renderer.log", "w"); }
    return f;
}
void log_line(const char* fmt, ...) {
    FILE* f = open_log();
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fflush(f);
}
} // namespace


namespace silent_storm::renderer {

namespace {
bgfx::RendererType::Enum map_backend(Backend b) {
    switch (b) {
        case Backend::D3D11:  return bgfx::RendererType::Direct3D11;
        case Backend::D3D12:  return bgfx::RendererType::Direct3D12;
        case Backend::Vulkan: return bgfx::RendererType::Vulkan;
        case Backend::Metal:  return bgfx::RendererType::Metal;
        case Backend::OpenGL: return bgfx::RendererType::OpenGL;
        default:              return bgfx::RendererType::Count;  // auto
    }
}
int g_width  = 0;
int g_height = 0;
} // namespace

bool init(const platform::Window& window, const Config& cfg) {
    auto sz = window.size();
    return init_hwnd(window.native_handle(), sz.width, sz.height, cfg);
}

bool init_hwnd(void* hwnd, int width, int height, const Config& cfg) {
    log_line("renderer::init_hwnd hwnd=%p w=%d h=%d", hwnd, width, height);
    bgfx::PlatformData pd{};
    pd.nwh          = hwnd;
    pd.ndt          = nullptr;
    pd.backBuffer   = nullptr;
    pd.backBufferDS = nullptr;
    if (!pd.nwh) {
        log_line("bgfx init: no native window handle");
        return false;
    }
    bgfx::setPlatformData(pd);

    g_width  = width  > 0 ? width  : 1280;
    g_height = height > 0 ? height : 720;

    bgfx::Init bInit;
    bInit.type                    = map_backend(cfg.renderer.backend);
    bInit.resolution.width        = static_cast<uint32_t>(g_width);
    bInit.resolution.height       = static_cast<uint32_t>(g_height);
    bInit.resolution.reset        = cfg.display.vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
    bInit.platformData            = pd;
    if (!bgfx::init(bInit)) {
        log_line("bgfx::init failed");
        return false;
    }
    log_line("bgfx::init ok, backend=%d", (int)bgfx::getRendererType());

    bgfx::setViewClear(0,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
        0x202020ff,
        1.0f,
        0);
    bgfx::setViewRect(0, 0, 0,
        static_cast<uint16_t>(g_width),
        static_cast<uint16_t>(g_height));
    return true;
}

void shutdown() {
    bgfx::shutdown();
}

void on_resize(int width, int height) {
    g_width  = width;
    g_height = height;
    bgfx::reset(static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
                BGFX_RESET_NONE);
    bgfx::setViewRect(0, 0, 0,
        static_cast<uint16_t>(width),
        static_cast<uint16_t>(height));
}

void begin_frame() {
    // Ensure view 0 is submitted even if no draw calls are issued this frame.
    bgfx::touch(0);
}

void end_frame() {
    bgfx::frame();
}

} // namespace silent_storm::renderer

// ---------------------------------------------------------------------------
// C-style entry points for Game/Main.cpp (STLport-compiled).  Keep these
// `extern "C"` and POD-only so they're safe to call across the ABI boundary.
// ---------------------------------------------------------------------------
extern "C" bool ss_renderer_bootstrap(void* hwnd, int width, int height,
                                       const char* cfg_path) {
    silent_storm::Config cfg = silent_storm::LoadConfig(cfg_path ? cfg_path : "silent_storm.cfg");
    return silent_storm::renderer::init_hwnd(hwnd, width, height, cfg);
}

extern "C" bool ss_renderer_load_shaders(const char* shader_dir) {
    return silent_storm::renderer::load_all_archetypes(shader_dir);
}

