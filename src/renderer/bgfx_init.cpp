#include "bgfx_init.h"
#include "shader_registry.h"
#include "d3d9_facade.h"
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
int g_width     = 0;
int g_height    = 0;
int g_hud_scale = 1;  // T11: updated by D3D9Facade::SetTransform when ortho detected
} // namespace

// ----- Phase 1.5 r3 — debug text relay -----
// Lives at silent_storm::renderer namespace scope (not anonymous) so the
// extern "C" entry points further down in this file can reference them by
// fully-qualified name.
struct DbgTextEntry {
    int x, y;
    uint8_t attr;
    char text[96];
};
constexpr int kDbgTextCap = 32;
DbgTextEntry g_dbg_text[kDbgTextCap];
int g_dbg_text_count = 0;
char g_dbg_text_banner[160] = {};

int get_width()     { return g_width; }
int get_height()    { return g_height; }
int get_hud_scale() { return g_hud_scale; }

// Called by the facade's SetTransform to record the computed HUD scale so the
// input bridge can read it without depending on D3D9Facade directly.
void set_hud_scale(int scale) { g_hud_scale = scale; }

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

    // Phase 1.5 r2 iter 4: enable bgfx debug text overlay so we have a
    // visible "we are alive" signal independent of Nival's draw path.
    bgfx::setDebug(BGFX_DEBUG_TEXT);

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
    // Phase 1.5 r3: paint a status bar AND flush the upstream debug-text
    // relay (CTextDraw → ss_dbg_text_push) via bgfx::dbgTextPrintf.  This
    // makes Nival's text strings VISIBLE on screen even before the font
    // atlas binding lands.
    static uint64_t s_frame = 0;
    ++s_frame;
    bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(2, 0, 0x0f,
        "Silent Storm port  -  Phase 1.5 r3  -  bgfx alive, frame %llu",
        (unsigned long long)s_frame);
    bgfx::dbgTextPrintf(2, 1, 0x1f,
        "Backend %d  Window %d x %d  HUD scale %d  text-relay entries=%d",
        (int)bgfx::getRendererType(), g_width, g_height, g_hud_scale,
        g_dbg_text_count);
    if (g_dbg_text_banner[0])
        bgfx::dbgTextPrintf(2, 2, 0x2f, "%s", g_dbg_text_banner);

    // Flush queued upstream debug-text entries.  Coords come in 1024x768
    // virtual space; bgfx dbg text uses ~80 cols × ~24 rows.
    // Track which (row,col) cells have been written this frame so successive
    // entries with the same virtual position don't overprint each other.
    uint8_t cell_used[24] = {};
    for (int i = 0; i < g_dbg_text_count; ++i) {
        const auto& e = g_dbg_text[i];
        int col = (e.x * 80) / 1024;
        int row = 3 + (e.y * 19) / 768;   // rows 3..21 are usable
        if (col < 0)  col = 0;
        if (col > 78) col = 78;
        if (row < 3)  row = 3;
        if (row > 21) row = 21;
        // Bump down until we find an unused row (don't overwrite previous lines).
        while (row < 22 && cell_used[row]) ++row;
        if (row > 21) row = 22;
        cell_used[row] = 1;
        bgfx::dbgTextPrintf(col, row, e.attr ? e.attr : 0x0f, "%s", e.text);
    }
    g_dbg_text_count = 0;

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
    // T10/T11: seed the facade singleton with the loaded config so that
    // FOV + HUD scale kick in from the very first SetTransform call.
    silent_storm::renderer::facade_init_with_config(cfg);
    return silent_storm::renderer::init_hwnd(hwnd, width, height, cfg);
}

extern "C" bool ss_renderer_load_shaders(const char* shader_dir) {
    return silent_storm::renderer::load_all_archetypes(shader_dir);
}

// ---------------------------------------------------------------------------
// Phase 1.5 r3 — debug text relay C entry points (called from STLport side)
// ---------------------------------------------------------------------------
extern "C" void ss_dbg_text_push(int virtX, int virtY, unsigned attr, const char* text) {
    if (!text) return;
    int idx = silent_storm::renderer::g_dbg_text_count;
    if (idx >= silent_storm::renderer::kDbgTextCap) return;
    auto& e = silent_storm::renderer::g_dbg_text[idx];
    e.x    = virtX;
    e.y    = virtY;
    e.attr = static_cast<uint8_t>(attr & 0xff);
    int n = 0;
    while (n < (int)sizeof(e.text) - 1 && text[n]) { e.text[n] = text[n]; ++n; }
    e.text[n] = '\0';
    ++silent_storm::renderer::g_dbg_text_count;
}

extern "C" void ss_dbg_text_banner(const char* text) {
    if (!text) { silent_storm::renderer::g_dbg_text_banner[0] = '\0'; return; }
    int n = 0;
    while (n < (int)sizeof(silent_storm::renderer::g_dbg_text_banner) - 1 && text[n]) {
        silent_storm::renderer::g_dbg_text_banner[n] = text[n];
        ++n;
    }
    silent_storm::renderer::g_dbg_text_banner[n] = '\0';
}

