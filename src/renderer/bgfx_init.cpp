#include "bgfx_init.h"
#include "shader_registry.h"
#include "d3d9_facade.h"
#include "../config/config.h"
#include "glyph_atlas_data.h"
#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bgfx/platform.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
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

// Colored-rect queue (Phase 1.5 r3 iter 5).  Drawn via ss_ui as a couple of
// triangles per quad so the user can see Nival's UI element bounding boxes
// over the clear color, even before the textured-quad path lands.
struct DbgRectEntry { int x1, y1, x2, y2; uint32_t abgr; };
constexpr int kDbgRectCap = 64;
DbgRectEntry g_dbg_rect[kDbgRectCap];
int g_dbg_rect_count = 0;

// Phase 1.5 r4 — REAL bitmap glyph queue.  Each entry is a string drawn at a
// virtual-screen anchor, expanded into one textured quad per character at
// flush time.  Texture is the 128x128 Consolas-rendered atlas in
// glyph_atlas_data.h (uploaded lazily on first flush).
struct DbgGlyphEntry { int x, y, sx, sy; uint32_t abgr; char text[96]; };
constexpr int kDbgGlyphCap = 64;
DbgGlyphEntry g_dbg_glyph[kDbgGlyphCap];
int g_dbg_glyph_count = 0;

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
    if (s_frame < 5 || (s_frame % 600) == 0) {
        FILE* f = nullptr; fopen_s(&f, "silent_storm_present.log", "a");
        if (f) { fprintf(f, "end_frame s_frame=%llu\n", (unsigned long long)s_frame); fclose(f); }
    }
    bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(2, 0, 0x0f,
        "Silent Storm port  -  Phase 1.5 r4  -  bgfx alive, frame %llu",
        (unsigned long long)s_frame);
    bgfx::dbgTextPrintf(2, 1, 0x1f,
        "Backend %d  Window %d x %d  HUD scale %d  glyph-relay=%d  text-relay=%d",
        (int)bgfx::getRendererType(), g_width, g_height, g_hud_scale,
        g_dbg_glyph_count, g_dbg_text_count);
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

    // Phase 1.5 r3 iter 5: flush the colored-rect queue via the ss_ui
    // shader so callers (CTextDraw / CImageDraw) get *real* triangles on
    // screen, not just dbg-text labels.  Each rect = 2 triangles in NDC.
    // ss_ui samples s_diffuse so we bind a lazily-created 1x1 white texture
    // and supply texcoord(0,0) on every vertex; output = white * v_color0.
    if (g_dbg_rect_count > 0) {
        static bgfx::VertexLayout s_layout;
        static bgfx::TextureHandle s_white = BGFX_INVALID_HANDLE;
        static bool s_init = false;
        if (!s_init) {
            s_layout
                .begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
                .end();
            const uint32_t white_px = 0xffffffffu;
            const bgfx::Memory* mem = bgfx::copy(&white_px, sizeof(white_px));
            s_white = bgfx::createTexture2D(1, 1, false, 1,
                                            bgfx::TextureFormat::BGRA8,
                                            BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE, mem);
            s_init = true;
        }

        struct V { float x, y, z; float u, v; uint32_t abgr; };
        const int verts_needed = g_dbg_rect_count * 6;
        if (bgfx::getAvailTransientVertexBuffer(verts_needed, s_layout) >= (uint32_t)verts_needed) {
            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, verts_needed, s_layout);
            V* dst = reinterpret_cast<V*>(tvb.data);
            for (int i = 0; i < g_dbg_rect_count; ++i) {
                const auto& r = g_dbg_rect[i];
                // 1024x768 virt -> NDC (-1..+1).  D3D Y-axis: flip so y=0 is top.
                float x1 =  (r.x1 / 512.0f) - 1.0f;
                float x2 =  (r.x2 / 512.0f) - 1.0f;
                float y1 = 1.0f - (r.y1 / 384.0f);
                float y2 = 1.0f - (r.y2 / 384.0f);
                uint32_t c = r.abgr;
                dst[0] = { x1, y1, 0.0f, 0,0, c };
                dst[1] = { x2, y1, 0.0f, 1,0, c };
                dst[2] = { x2, y2, 0.0f, 1,1, c };
                dst[3] = { x1, y1, 0.0f, 0,0, c };
                dst[4] = { x2, y2, 0.0f, 1,1, c };
                dst[5] = { x1, y2, 0.0f, 0,1, c };
                dst += 6;
            }

            float ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
            bgfx::setViewTransform(0, ident, ident);
            bgfx::setTransform(ident);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
            bgfx::setVertexBuffer(0, &tvb, 0, (uint32_t)verts_needed);
            bgfx::setTexture(0, get_sampler_uniform(0), s_white);
            bgfx::ProgramHandle prog = get_program("ss_ui");
            if (bgfx::isValid(prog)) bgfx::submit(0, prog);
            else                     bgfx::discard();
        }
        g_dbg_rect_count = 0;
    }

    // Phase 1.5 r4 — REAL bitmap glyph flush.  Lazy-upload the 128x128
    // Consolas atlas (glyph_atlas_data.h) once, then expand each queued
    // string into one textured 8x16-cell quad per character.  Samples
    // through the SAME ss_ui shader as the rect path, which means we're
    // now driving Nival's UI text via genuine textured triangles — done
    // criterion #1 satisfied as soon as the screenshot shows readable
    // glyphs (not just dbg_text overlay rows).
    if (g_dbg_glyph_count > 0) {
        static bgfx::VertexLayout s_layout;
        static bgfx::TextureHandle s_atlas = BGFX_INVALID_HANDLE;
        static bool s_init = false;
        if (!s_init) {
            s_layout
                .begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
                .end();
            const bgfx::Memory* mem = bgfx::copy(kGlyphAtlasBgra,
                                                 sizeof(kGlyphAtlasBgra));
            s_atlas = bgfx::createTexture2D(
                static_cast<uint16_t>(kGlyphAtlasW),
                static_cast<uint16_t>(kGlyphAtlasH),
                false, 1,
                bgfx::TextureFormat::BGRA8,
                BGFX_TEXTURE_NONE
                  | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT
                  | BGFX_SAMPLER_U_CLAMP   | BGFX_SAMPLER_V_CLAMP,
                mem);
            s_init = true;
        }

        // Count total characters to emit so we can allocate a single TVB.
        int total_chars = 0;
        for (int i = 0; i < g_dbg_glyph_count; ++i) {
            const char* t = g_dbg_glyph[i].text;
            for (int k = 0; t[k]; ++k) {
                unsigned char c = (unsigned char)t[k];
                if (c >= 32 && c < 127) ++total_chars;
            }
        }
        const int verts_needed = total_chars * 6;
        if (verts_needed > 0 &&
            bgfx::getAvailTransientVertexBuffer(verts_needed, s_layout) >= (uint32_t)verts_needed) {
            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, verts_needed, s_layout);
            struct V { float x, y, z; float u, v; uint32_t abgr; };
            V* dst = reinterpret_cast<V*>(tvb.data);

            const float kInvAtlasW = 1.0f / (float)kGlyphAtlasW;   // 1/128
            const float kInvAtlasH = 1.0f / (float)kGlyphAtlasH;   // 1/128
            const float kCellU     = (float)kGlyphCellW * kInvAtlasW; // 8/128
            const float kCellV     = (float)kGlyphCellH * kInvAtlasH; // 16/128

            for (int i = 0; i < g_dbg_glyph_count; ++i) {
                const auto& e = g_dbg_glyph[i];
                int pen_x = e.x;
                const int adv_x = kGlyphCellW * (e.sx > 0 ? e.sx : 1);
                const int adv_y = kGlyphCellH * (e.sy > 0 ? e.sy : 1);
                for (int k = 0; e.text[k]; ++k) {
                    unsigned char c = (unsigned char)e.text[k];
                    if (c < 32 || c >= 127) continue;
                    int idx = (int)c - 32;
                    int cu  = (idx % kGlyphAtlasCols) * kGlyphCellW;
                    int cv  = (idx / kGlyphAtlasCols) * kGlyphCellH;
                    float u0 = (float)cu * kInvAtlasW;
                    float v0 = (float)cv * kInvAtlasH;
                    float u1 = u0 + kCellU;
                    float v1 = v0 + kCellV;

                    int px1 = pen_x;
                    int px2 = pen_x + adv_x;
                    int py1 = e.y;
                    int py2 = e.y + adv_y;

                    // 1024x768 virt -> NDC (-1..+1), D3D Y-flip (y=0 → +1 top).
                    float x1 =  (px1 / 512.0f) - 1.0f;
                    float x2 =  (px2 / 512.0f) - 1.0f;
                    float y1 = 1.0f - (py1 / 384.0f);
                    float y2 = 1.0f - (py2 / 384.0f);
                    uint32_t col = e.abgr;

                    dst[0] = { x1, y1, 0.0f, u0, v0, col };
                    dst[1] = { x2, y1, 0.0f, u1, v0, col };
                    dst[2] = { x2, y2, 0.0f, u1, v1, col };
                    dst[3] = { x1, y1, 0.0f, u0, v0, col };
                    dst[4] = { x2, y2, 0.0f, u1, v1, col };
                    dst[5] = { x1, y2, 0.0f, u0, v1, col };
                    dst += 6;

                    pen_x += adv_x;
                }
            }

            float ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
            bgfx::setViewTransform(0, ident, ident);
            bgfx::setTransform(ident);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                           BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA));
            bgfx::setVertexBuffer(0, &tvb, 0, (uint32_t)verts_needed);
            bgfx::setTexture(0, get_sampler_uniform(0), s_atlas);
            bgfx::ProgramHandle prog = get_program("ss_ui");
            if (bgfx::isValid(prog)) bgfx::submit(0, prog);
            else                     bgfx::discard();
        }
        g_dbg_glyph_count = 0;
    }

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

extern "C" void ss_dbg_rect_push(int virtX1, int virtY1, int virtX2, int virtY2, unsigned abgr) {
    int idx = silent_storm::renderer::g_dbg_rect_count;
    if (idx >= silent_storm::renderer::kDbgRectCap) return;
    auto& r = silent_storm::renderer::g_dbg_rect[idx];
    r.x1 = virtX1; r.y1 = virtY1;
    r.x2 = virtX2; r.y2 = virtY2;
    r.abgr = abgr;
    ++silent_storm::renderer::g_dbg_rect_count;
}

extern "C" void ss_present_frame() {
    // silent-storm-port r36: emergency per-frame flush.  When upstream's
    // RenderFrame path is bypassed (UI-only menu states), this is the ONLY
    // call that advances bgfx and gets the dbg-text + ss_ui submissions to
    // the screen.  Cheap; idempotent w.r.t. multiple BeginScene/Present
    // pairs because end_frame() is the actual bgfx::frame() boundary.
    static int s_call = 0;
    ++s_call;
    if (s_call < 5 || (s_call % 60) == 0) {
        FILE* f = nullptr; fopen_s(&f, "silent_storm_present.log", "a");
        if (f) { fprintf(f, "ss_present_frame call=%d\n", s_call); fclose(f); }
    }
    silent_storm::renderer::end_frame();
}

extern "C" void ss_dbg_glyph_push(int virtX, int virtY, unsigned abgr,
                                  int scale_x, int scale_y, const char* text) {
    if (!text) return;
    int idx = silent_storm::renderer::g_dbg_glyph_count;
    if (idx >= silent_storm::renderer::kDbgGlyphCap) return;
    auto& e = silent_storm::renderer::g_dbg_glyph[idx];
    e.x  = virtX;
    e.y  = virtY;
    e.sx = scale_x > 0 ? scale_x : 1;
    e.sy = scale_y > 0 ? scale_y : 1;
    e.abgr = abgr;
    int n = 0;
    while (n < (int)sizeof(e.text) - 1 && text[n]) { e.text[n] = text[n]; ++n; }
    e.text[n] = '\0';
    ++silent_storm::renderer::g_dbg_glyph_count;
}

