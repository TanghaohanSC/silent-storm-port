#include "bgfx_init.h"
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <cstdio>

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
    bgfx::PlatformData pd{};
    pd.nwh          = window.native_handle();
    pd.ndt          = nullptr;   // Windows: no display handle (X11 only)
    pd.backBuffer   = nullptr;   // let bgfx manage
    pd.backBufferDS = nullptr;   // let bgfx manage
    if (!pd.nwh) {
        std::fprintf(stderr, "bgfx init: no native window handle\n");
        return false;
    }
    bgfx::setPlatformData(pd);

    auto sz  = window.size();
    g_width  = sz.width;
    g_height = sz.height;

    bgfx::Init bInit;
    bInit.type                    = map_backend(cfg.renderer.backend);
    bInit.resolution.width        = static_cast<uint32_t>(g_width);
    bInit.resolution.height       = static_cast<uint32_t>(g_height);
    bInit.resolution.reset        = cfg.display.vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
    bInit.platformData            = pd;
    if (!bgfx::init(bInit)) {
        std::fprintf(stderr, "bgfx::init failed\n");
        return false;
    }

    bgfx::setViewClear(0,
        BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
        0x202020ff,   // dark gray background
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
