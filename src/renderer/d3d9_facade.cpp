// d3d9_facade.cpp — IDirect3DDevice9 facade
// Phase 1 Task 6: 90 method stubs / state recording.
// Phase 1 Task 9: resource wrappers + draw→bgfx::submit pipeline.

// WIN32_LEAN_AND_MEAN and NOMINMAX are already set by CMake command-line;
// d3d9_facade.h re-declares them for safety but they're already defined.
#include "d3d9_facade.h"
#include "bgfx_init.h"
#include "d3d9_facade_resources.h"
#include "shader_registry.h"
#include <bgfx/bgfx.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cmath>    // std::floor, std::min, std::max, std::tan
#include <algorithm>

extern "C" void ss_dbg_rect_push(int x1, int y1, int x2, int y2, unsigned abgr);

namespace silent_storm::renderer {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
namespace {
D3D9Facade* g_instance = nullptr;

// ----- Phase 1.5 r2 — draw-call trace (declared file-scope so BeginScene /
// Present in the IDirect3DDevice9 method block can also reference them).
FILE* draw_log() {
    static FILE* f = nullptr;
    if (!f) fopen_s(&f, "silent_storm_draw.log", "w");
    return f;
}
uint64_t g_total_draw_calls = 0;
uint64_t g_total_submits    = 0;
bool     g_archetype_seen[16] = {};

int archetype_index(const char* name) {
    static const char* names[] = {
        "ss_diffuse_unlit", "ss_diffuse_lit", "ss_skinned", "ss_ui",
        "ss_particle", "ss_shadow", "ss_terrain", "ss_water",
    };
    if (!name) return -1;
    for (int i = 0; i < 8; ++i) {
        if (std::strcmp(names[i], name) == 0) return i;
    }
    return -1;
}

void trace_draw_entry(const char* fn) {
    ++g_total_draw_calls;
    if (g_total_draw_calls <= 16 || (g_total_draw_calls % 256) == 0) {
        if (FILE* f = draw_log()) {
            fprintf(f, "[draw#%llu] %s\n",
                    (unsigned long long)g_total_draw_calls, fn);
            fflush(f);
        }
    }
}

void trace_submit(const char* arch, bgfx::ProgramHandle prog) {
    ++g_total_submits;
    int idx = archetype_index(arch);
    if (idx >= 0 && !g_archetype_seen[idx]) {
        g_archetype_seen[idx] = true;
        if (FILE* f = draw_log()) {
            fprintf(f, "[submit#%llu] FIRST %s prog.idx=%u valid=%d\n",
                    (unsigned long long)g_total_submits,
                    arch, prog.idx, (int)bgfx::isValid(prog));
            fflush(f);
        }
    }
    if (g_total_submits <= 8) {
        if (FILE* f = draw_log()) {
            fprintf(f, "[submit#%llu] %s prog.idx=%u valid=%d\n",
                    (unsigned long long)g_total_submits,
                    arch ? arch : "(null)", prog.idx, (int)bgfx::isValid(prog));
            fflush(f);
        }
    }
}

void trace_invalid_prog(const char* arch) {
    static int once = 0;
    if (once < 8) {
        ++once;
        if (FILE* f = draw_log()) {
            fprintf(f, "[discard] no program for archetype '%s' — bgfx::discard()\n",
                    arch ? arch : "(null)");
            fflush(f);
        }
    }
}

// r61: detailed reason counters for early returns in draw paths
struct DrawReasonCounts {
    uint64_t no_vb        = 0;  // stream_vbo_[0] == nullptr
    uint64_t no_ib        = 0;  // ibo_ == nullptr
    uint64_t no_stride    = 0;  // stream_stride_[0] == 0
    uint64_t no_tvb_space = 0;  // bgfx avail < verts
    uint64_t no_tib_space = 0;
    uint64_t empty_cpu_vb = 0;  // vb cpu_size == 0
    uint64_t submitted    = 0;
    uint64_t real_data    = 0;  // submissions with real VB data copy
} g_reasons;

void trace_reason(const char* tag, uint64_t* counter) {
    ++*counter;
    if (*counter <= 4 || (*counter & (*counter - 1)) == 0) {
        if (FILE* f = draw_log()) {
            fprintf(f, "[reason] %s now=%llu\n", tag, (unsigned long long)*counter);
            fflush(f);
        }
    }
}

void trace_dip_args(UINT NumVertices, UINT startIndex, UINT primCount,
                    UINT stride, DWORD fvf, UINT vb_cpu_size, UINT ib_cpu_size) {
    static int once = 0;
    if (once < 24) {
        ++once;
        if (FILE* f = draw_log()) {
            fprintf(f, "[dip_args#%d] NumV=%u sIdx=%u primCount=%u stride=%u fvf=%x vb_cpu=%u ib_cpu=%u\n",
                    once, NumVertices, startIndex, primCount, stride,
                    (unsigned)fvf, vb_cpu_size, ib_cpu_size);
            fflush(f);
        }
    }
}
} // namespace

IDirect3DDevice9* facade_instance() {
    if (!g_instance)
        g_instance = new D3D9Facade();
    return g_instance;
}

// T10/T11: called from ss_renderer_bootstrap after the config is loaded, so
// the facade can apply FOV + HUD scale from the very first frame.
void facade_init_with_config(const Config& cfg) {
    if (!g_instance)
        g_instance = new D3D9Facade(cfg);
    // If an instance already exists (e.g. Nival called CreateDevice first)
    // we can't safely re-create it — just patch the config in place via a
    // public setter.  For Phase 1 the normal path is bootstrap-before-device.
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
D3D9Facade::D3D9Facade()  = default;
D3D9Facade::D3D9Facade(const Config& cfg) : cfg_(cfg) {}
D3D9Facade::~D3D9Facade() = default;

// ---------------------------------------------------------------------------
// IUnknown
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3DDevice9) {
        *ppv = static_cast<IDirect3DDevice9*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

ULONG __stdcall D3D9Facade::AddRef() {
    return ++ref_count_;
}

ULONG __stdcall D3D9Facade::Release() {
    ULONG r = --ref_count_;
    if (r == 0) delete this;
    return r;
}

// ---------------------------------------------------------------------------
// IDirect3DDevice9 — device-level queries (return minimal sensible values)
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::TestCooperativeLevel() {
    return D3D_OK;
}

UINT __stdcall D3D9Facade::GetAvailableTextureMem() {
    return 512u * 1024u * 1024u; // claim 512 MB
}

HRESULT __stdcall D3D9Facade::EvictManagedResources() {
    return D3D_OK;
}

HRESULT __stdcall D3D9Facade::GetDirect3D(IDirect3D9** ppD3D9) {
    if (!ppD3D9) return D3DERR_INVALIDCALL;
    *ppD3D9 = nullptr;
    return E_NOTIMPL;
}

HRESULT __stdcall D3D9Facade::GetDeviceCaps(D3DCAPS9* pCaps) {
    if (!pCaps) return D3DERR_INVALIDCALL;
    std::memset(pCaps, 0, sizeof(D3DCAPS9));
    // Fill in the minimum caps Nival's code queries at startup.
    pCaps->DeviceType              = D3DDEVTYPE_HAL;
    pCaps->AdapterOrdinal          = D3DADAPTER_DEFAULT;
    pCaps->Caps2                   = D3DCAPS2_DYNAMICTEXTURES | D3DCAPS2_CANAUTOGENMIPMAP;
    pCaps->PresentationIntervals   = D3DPRESENT_INTERVAL_DEFAULT | D3DPRESENT_INTERVAL_ONE;
    pCaps->MaxTextureWidth         = 4096;
    pCaps->MaxTextureHeight        = 4096;
    pCaps->MaxVolumeExtent         = 256;
    pCaps->MaxTextureAspectRatio   = 0; // 0 = unlimited
    pCaps->MaxAnisotropy           = 16;
    pCaps->MaxPrimitiveCount       = 0x555555;
    pCaps->MaxVertexIndex          = 0xFFFFFF;
    pCaps->MaxStreams               = 16;
    pCaps->MaxStreamStride         = 256;
    pCaps->VertexShaderVersion     = D3DVS_VERSION(3, 0);
    pCaps->MaxVertexShaderConst    = 256;
    pCaps->PixelShaderVersion      = D3DPS_VERSION(3, 0);
    pCaps->PixelShader1xMaxValue   = 8.0f;
    pCaps->MaxVertexW              = 1.0e10f;
    pCaps->NumSimultaneousRTs      = 4;
    pCaps->MaxTextureBlendStages   = 8;
    pCaps->MaxSimultaneousTextures = 8;
    pCaps->VertexProcessingCaps    = D3DVTXPCAPS_TEXGEN | D3DVTXPCAPS_MATERIALSOURCE7 |
                                     D3DVTXPCAPS_DIRECTIONALLIGHTS | D3DVTXPCAPS_POSITIONALLIGHTS |
                                     D3DVTXPCAPS_LOCALVIEWER;
    pCaps->MaxActiveLights         = 8;
    pCaps->MaxUserClipPlanes       = 6;
    pCaps->MaxVertexBlendMatrices  = 4;
    pCaps->MaxVertexBlendMatrixIndex = 255;
    pCaps->RasterCaps              = D3DPRASTERCAPS_ANISOTROPY | D3DPRASTERCAPS_DEPTHBIAS |
                                     D3DPRASTERCAPS_FOGRANGE | D3DPRASTERCAPS_FOGTABLE |
                                     D3DPRASTERCAPS_FOGVERTEX | D3DPRASTERCAPS_MIPMAPLODBIAS |
                                     D3DPRASTERCAPS_MULTISAMPLE_TOGGLE | D3DPRASTERCAPS_SCISSORTEST |
                                     D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS | D3DPRASTERCAPS_WFOG |
                                     D3DPRASTERCAPS_WBUFFER | D3DPRASTERCAPS_ZFOG | D3DPRASTERCAPS_ZTEST;
    pCaps->ZCmpCaps = pCaps->AlphaCmpCaps =
        D3DPCMPCAPS_ALWAYS | D3DPCMPCAPS_EQUAL | D3DPCMPCAPS_GREATER |
        D3DPCMPCAPS_GREATEREQUAL | D3DPCMPCAPS_LESS | D3DPCMPCAPS_LESSEQUAL |
        D3DPCMPCAPS_NEVER | D3DPCMPCAPS_NOTEQUAL;
    pCaps->SrcBlendCaps = pCaps->DestBlendCaps =
        D3DPBLENDCAPS_BLENDFACTOR | D3DPBLENDCAPS_BOTHINVSRCALPHA | D3DPBLENDCAPS_BOTHSRCALPHA |
        D3DPBLENDCAPS_DESTALPHA | D3DPBLENDCAPS_DESTCOLOR | D3DPBLENDCAPS_INVDESTALPHA |
        D3DPBLENDCAPS_INVDESTCOLOR | D3DPBLENDCAPS_INVSRCALPHA | D3DPBLENDCAPS_INVSRCCOLOR |
        D3DPBLENDCAPS_ONE | D3DPBLENDCAPS_SRCALPHA | D3DPBLENDCAPS_SRCALPHASAT |
        D3DPBLENDCAPS_SRCCOLOR | D3DPBLENDCAPS_ZERO;
    pCaps->ShadeCaps = D3DPSHADECAPS_ALPHAGOURAUDBLEND | D3DPSHADECAPS_COLORGOURAUDRGB |
                       D3DPSHADECAPS_FOGGOURAUD | D3DPSHADECAPS_SPECULARGOURAUDRGB;
    pCaps->TextureCaps = D3DPTEXTURECAPS_ALPHA | D3DPTEXTURECAPS_ALPHAPALETTE |
                         D3DPTEXTURECAPS_CUBEMAP | D3DPTEXTURECAPS_MIPMAP |
                         D3DPTEXTURECAPS_MIPCUBEMAP | D3DPTEXTURECAPS_MIPVOLUMEMAP |
                         D3DPTEXTURECAPS_NONPOW2CONDITIONAL | D3DPTEXTURECAPS_PERSPECTIVE |
                         D3DPTEXTURECAPS_PROJECTED | D3DPTEXTURECAPS_TEXREPEATNOTSCALEDBYSIZE |
                         D3DPTEXTURECAPS_VOLUMEMAP;
    pCaps->TextureFilterCaps = pCaps->CubeTextureFilterCaps = pCaps->VolumeTextureFilterCaps =
        D3DPTFILTERCAPS_MAGFANISOTROPIC | D3DPTFILTERCAPS_MAGFLINEAR | D3DPTFILTERCAPS_MAGFPOINT |
        D3DPTFILTERCAPS_MINFANISOTROPIC | D3DPTFILTERCAPS_MINFLINEAR | D3DPTFILTERCAPS_MINFPOINT |
        D3DPTFILTERCAPS_MIPFLINEAR | D3DPTFILTERCAPS_MIPFPOINT;
    pCaps->TextureAddressCaps = pCaps->VolumeTextureAddressCaps =
        D3DPTADDRESSCAPS_BORDER | D3DPTADDRESSCAPS_CLAMP | D3DPTADDRESSCAPS_INDEPENDENTUV |
        D3DPTADDRESSCAPS_MIRROR | D3DPTADDRESSCAPS_MIRRORONCE | D3DPTADDRESSCAPS_WRAP;
    pCaps->LineCaps = D3DLINECAPS_ALPHACMP | D3DLINECAPS_BLEND | D3DLINECAPS_FOG |
                      D3DLINECAPS_TEXTURE | D3DLINECAPS_ZTEST;
    pCaps->MaxNpatchTessellationLevel = 1.0f;
    pCaps->DeclTypes = D3DDTCAPS_UBYTE4 | D3DDTCAPS_UBYTE4N | D3DDTCAPS_SHORT2N |
                       D3DDTCAPS_SHORT4N | D3DDTCAPS_USHORT2N | D3DDTCAPS_USHORT4N |
                       D3DDTCAPS_UDEC3 | D3DDTCAPS_DEC3N | D3DDTCAPS_FLOAT16_2 |
                       D3DDTCAPS_FLOAT16_4;
    return D3D_OK;
}

HRESULT __stdcall D3D9Facade::GetDisplayMode(UINT /*iSwapChain*/, D3DDISPLAYMODE* pMode) {
    if (!pMode) return D3DERR_INVALIDCALL;
    std::memset(pMode, 0, sizeof(*pMode));
    pMode->Width  = 1920;
    pMode->Height = 1080;
    pMode->RefreshRate = 60;
    pMode->Format = D3DFMT_X8R8G8B8;
    return D3D_OK;
}

HRESULT __stdcall D3D9Facade::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters) {
    if (!pParameters) return D3DERR_INVALIDCALL;
    std::memset(pParameters, 0, sizeof(*pParameters));
    pParameters->AdapterOrdinal = D3DADAPTER_DEFAULT;
    pParameters->DeviceType     = D3DDEVTYPE_HAL;
    pParameters->BehaviorFlags  = D3DCREATE_HARDWARE_VERTEXPROCESSING;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Cursor
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetCursorProperties(UINT /*XHotSpot*/, UINT /*YHotSpot*/, IDirect3DSurface9* /*pCursorBitmap*/) {
    return D3D_OK;
}
void __stdcall D3D9Facade::SetCursorPosition(int /*X*/, int /*Y*/, DWORD /*Flags*/) {}
BOOL __stdcall D3D9Facade::ShowCursor(BOOL /*bShow*/) { return FALSE; }

// ---------------------------------------------------------------------------
// Swap chains
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* /*pPP*/, IDirect3DSwapChain9** ppSC) {
    if (ppSC) *ppSC = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::GetSwapChain(UINT /*iSwapChain*/, IDirect3DSwapChain9** ppSC) {
    if (ppSC) *ppSC = nullptr;
    return E_NOTIMPL;
}
UINT __stdcall D3D9Facade::GetNumberOfSwapChains() { return 1; }

// ---------------------------------------------------------------------------
// Reset — keeps the facade alive; Nival calls this on lost device.
// Phase 1.5 r2: actually call bgfx::reset() so the back-buffer grows from the
// tiny 100x100 we bootstrapped at, to Nival's chosen mode (e.g. 1024x768).
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::Reset(D3DPRESENT_PARAMETERS* pPP) {
    if (!pPP) return D3DERR_INVALIDCALL;
    if (pPP->BackBufferWidth > 0 && pPP->BackBufferHeight > 0) {
        FILE* _f = nullptr; fopen_s(&_f, "silent_storm_smfc.log", "a");
        if (_f) {
            fprintf(_f, "  D3D9Facade::Reset -> bgfx::reset(%u,%u)\n",
                    pPP->BackBufferWidth, pPP->BackBufferHeight);
            fclose(_f);
        }
        silent_storm::renderer::on_resize(
            static_cast<int>(pPP->BackBufferWidth),
            static_cast<int>(pPP->BackBufferHeight));
    }
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Present — flush bgfx frame
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::Present(CONST RECT* /*pSourceRect*/, CONST RECT* /*pDestRect*/,
                                       HWND /*hDestWindowOverride*/, CONST RGNDATA* /*pDirtyRegion*/) {
    if (!scene_active_) {
        // BeginScene was never called — touch the view so bgfx has something
        // to submit (otherwise frame() emits a "no submit" warning).
        bgfx::touch(current_view_id_);
    }
    static int once = 0;
    static uint64_t last_dump = 0;
    if (once < 3 || (g_total_draw_calls - last_dump) > 5000) {
        ++once;
        last_dump = g_total_draw_calls;
        if (FILE* f = draw_log()) {
            fprintf(f, "[present#%d] draws=%llu submits=%llu real=%llu  "
                       "no_vb=%llu no_ib=%llu no_stride=%llu empty_cpu=%llu "
                       "no_tvb=%llu no_tib=%llu submitted=%llu\n",
                    once,
                    (unsigned long long)g_total_draw_calls,
                    (unsigned long long)g_total_submits,
                    (unsigned long long)g_reasons.real_data,
                    (unsigned long long)g_reasons.no_vb,
                    (unsigned long long)g_reasons.no_ib,
                    (unsigned long long)g_reasons.no_stride,
                    (unsigned long long)g_reasons.empty_cpu_vb,
                    (unsigned long long)g_reasons.no_tvb_space,
                    (unsigned long long)g_reasons.no_tib_space,
                    (unsigned long long)g_reasons.submitted);
            fflush(f);
        }
    }

    // r71: push a fake-terrain rect to the existing dbg-rect queue. end_frame
    // already flushes that queue via ss_ui — proven path that lands pixels.
    // Sky band (upper third) light blue
    ss_dbg_rect_push(0, 90, 1024, 280, 0xff80a0ffu);
    // Distant terrain (middle band) medium green
    ss_dbg_rect_push(0, 280, 1024, 420, 0xff408050u);
    // Near terrain (lower half) darker olive green
    ss_dbg_rect_push(0, 420, 1024, 768, 0xff205030u);

    // Phase 1.5 r2 iter 5: submit a debug "I am alive" colored triangle once
    // per frame so we can validate the actual shader/vertex pipeline end-to-end.
    // Phase 1.5 r70: extend to a fake-terrain ground plane filling the lower
    // half of the framebuffer in NDC. Two triangles, green-grey gradient. Acts
    // as the visible terrain proxy while upstream pScene->Draw still SEHs in
    // the partial-world Init path. ss_ui shader is fine — accepts position +
    // ABGR color, no texture.
    {
        static bgfx::VertexLayout s_layout;
        static bool s_layout_init = false;
        if (!s_layout_init) {
            s_layout
                .begin()
                .add(bgfx::Attrib::Position,  3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
                .end();
            s_layout_init = true;
        }

        struct DbgVert { float x, y, z; float u, v; uint32_t abgr; };
        // Six verts (two tris) covering lower 2/3 of NDC framebuffer in
        // screen-space. NDC y is up-positive in bgfx. Color: olive-green
        // gradient (front lighter, back darker) suggesting a ground plane.
        // r71: full-screen magenta to validate view 250 actually rasterizes.
        // UVs 0,0 -> sample white 1x1 fallback -> color = vertex color.
        DbgVert verts[6] = {
            // First tri (BL, BR, TR)
            { -1.0f, -1.0f, 0.0f, 0.0f,0.0f, 0xffff00ffu },
            {  1.0f, -1.0f, 0.0f, 0.0f,0.0f, 0xffff00ffu },
            {  1.0f,  1.0f, 0.0f, 0.0f,0.0f, 0xffff00ffu },
            // Second tri (BL, TR, TL)
            { -1.0f, -1.0f, 0.0f, 0.0f,0.0f, 0xffff00ffu },
            {  1.0f,  1.0f, 0.0f, 0.0f,0.0f, 0xffff00ffu },
            { -1.0f,  1.0f, 0.0f, 0.0f,0.0f, 0xffff00ffu },
        };
        if (bgfx::getAvailTransientVertexBuffer(6, s_layout) >= 6) {
            bgfx::TransientVertexBuffer tvb;
            bgfx::allocTransientVertexBuffer(&tvb, 6, s_layout);
            std::memcpy(tvb.data, verts, sizeof(verts));

            // Identity transforms — vertices are already in clip-space.
            // r71: submit on view 250 with explicit FB=backbuffer, no clear.
            const bgfx::ViewId fg_view = 250;
            static bool s_fg_view_inited = false;
            if (!s_fg_view_inited) {
                bgfx::setViewFrameBuffer(fg_view, BGFX_INVALID_HANDLE);
                bgfx::setViewClear(fg_view, BGFX_CLEAR_NONE, 0, 1.0f, 0);
                bgfx::setViewMode(fg_view, bgfx::ViewMode::Sequential);
                s_fg_view_inited = true;
            }
            // setViewRect every frame (window may have resized).
            bgfx::setViewRect(fg_view, 0, 0, bgfx::BackbufferRatio::Equal);
            float ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
            bgfx::setViewTransform(fg_view, ident, ident);
            bgfx::setTransform(ident);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
                BGFX_STATE_DEPTH_TEST_ALWAYS);
            bgfx::setVertexBuffer(0, &tvb);
            // r70: ss_ui fs samples s_diffuse and multiplies by vertex color —
            // without a valid texture bound, sample returns 0 -> black. Bind
            // a literal 1x1 white texture and sampler uniform created here.
            static bgfx::TextureHandle s_fg_tex = BGFX_INVALID_HANDLE;
            static bgfx::UniformHandle s_fg_smp = BGFX_INVALID_HANDLE;
            if (!bgfx::isValid(s_fg_tex)) {
                const uint32_t white_px = 0xffffffffu;
                const bgfx::Memory* mem = bgfx::copy(&white_px, sizeof(white_px));
                s_fg_tex = bgfx::createTexture2D(1, 1, false, 1,
                    bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_NONE, mem);
                s_fg_smp = bgfx::createUniform("s_diffuse", bgfx::UniformType::Sampler);
            }
            bgfx::setTexture(0, s_fg_smp, s_fg_tex);

            bgfx::ProgramHandle prog = get_program("ss_ui");
            if (bgfx::isValid(prog)) {
                bgfx::submit(fg_view, prog);
                static int once_tri = 0;
                if (once_tri < 3) {
                    ++once_tri;
                    if (FILE* f = draw_log()) {
                        fprintf(f, "[fake_ground#%d] submitted via ss_ui view=%u\n",
                                once_tri, (unsigned)fg_view);
                        fflush(f);
                    }
                }
            } else {
                bgfx::discard();
            }
        }
    }

    silent_storm::renderer::end_frame();
    scene_active_ = false;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Back buffer / raster status
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::GetBackBuffer(UINT /*iSwapChain*/, UINT /*iBackBuffer*/,
                                              D3DBACKBUFFER_TYPE /*Type*/, IDirect3DSurface9** ppBB) {
    if (!ppBB) return D3DERR_INVALIDCALL;
    // Phase 1: return a sentinel surface sized to the bgfx framebuffer.  Caller
    // typically uses this for GetDesc / StretchRect and doesn't actually touch
    // its pixel data via Lock.
    *ppBB = new FacadeSurface(1920, 1080, D3DFMT_X8R8G8B8);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetRasterStatus(UINT /*iSwapChain*/, D3DRASTER_STATUS* pRS) {
    if (!pRS) return D3DERR_INVALIDCALL;
    pRS->InVBlank = FALSE;
    pRS->ScanLine = 0;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetDialogBoxMode(BOOL /*bEnableDialogs*/) { return D3D_OK; }
void    __stdcall D3D9Facade::SetGammaRamp(UINT /*iSwapChain*/, DWORD /*Flags*/, CONST D3DGAMMARAMP* /*pRamp*/) {}
void    __stdcall D3D9Facade::GetGammaRamp(UINT /*iSwapChain*/, D3DGAMMARAMP* pRamp) {
    if (pRamp) std::memset(pRamp, 0, sizeof(*pRamp));
}

// ---------------------------------------------------------------------------
// Resource creation — return real bgfx-backed wrapper objects (Task 9)
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::CreateTexture(UINT Width, UINT Height, UINT Levels,
                                              DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                              IDirect3DTexture9** ppTexture, HANDLE* /*pSharedHandle*/) {
    if (!ppTexture) return D3DERR_INVALIDCALL;
    *ppTexture = new FacadeTexture(Width, Height, Levels, Usage, Format, Pool);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::CreateVolumeTexture(UINT /*Width*/, UINT /*Height*/, UINT /*Depth*/,
                                                    UINT /*Levels*/, DWORD /*Usage*/, D3DFORMAT /*Format*/,
                                                    D3DPOOL /*Pool*/, IDirect3DVolumeTexture9** ppVT,
                                                    HANDLE* /*pSharedHandle*/) {
    if (ppVT) *ppVT = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage,
                                                  D3DFORMAT Format, D3DPOOL Pool,
                                                  IDirect3DCubeTexture9** ppCT, HANDLE* /*pSharedHandle*/) {
    if (!ppCT) return D3DERR_INVALIDCALL;
    *ppCT = new FacadeCubeTexture(EdgeLength, Levels, Usage, Format, Pool);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF,
                                                   D3DPOOL Pool, IDirect3DVertexBuffer9** ppVB,
                                                   HANDLE* /*pSharedHandle*/) {
    if (!ppVB) return D3DERR_INVALIDCALL;
    *ppVB = new FacadeVertexBuffer(Length, Usage, FVF, Pool);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format,
                                                  D3DPOOL Pool, IDirect3DIndexBuffer9** ppIB,
                                                  HANDLE* /*pSharedHandle*/) {
    if (!ppIB) return D3DERR_INVALIDCALL;
    *ppIB = new FacadeIndexBuffer(Length, Usage, Format, Pool);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format,
                                                   D3DMULTISAMPLE_TYPE /*MultiSample*/, DWORD /*MultisampleQuality*/,
                                                   BOOL /*Lockable*/, IDirect3DSurface9** ppSurface,
                                                   HANDLE* /*pSharedHandle*/) {
    if (!ppSurface) return D3DERR_INVALIDCALL;
    *ppSurface = new FacadeSurface(Width, Height, Format);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format,
                                                          D3DMULTISAMPLE_TYPE /*MultiSample*/, DWORD /*MultisampleQuality*/,
                                                          BOOL /*Discard*/, IDirect3DSurface9** ppSurface,
                                                          HANDLE* /*pSharedHandle*/) {
    if (!ppSurface) return D3DERR_INVALIDCALL;
    *ppSurface = new FacadeSurface(Width, Height, Format);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVE,
                                                        IDirect3DVertexDeclaration9** ppDecl) {
    if (!ppDecl) return D3DERR_INVALIDCALL;
    *ppDecl = new FacadeVertexDeclaration(pVE);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::CreateVertexShader(CONST DWORD* pFunction,
                                                   IDirect3DVertexShader9** ppShader) {
    if (!ppShader) return D3DERR_INVALIDCALL;
    *ppShader = new FacadeVertexShader(pFunction);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::CreatePixelShader(CONST DWORD* pFunction,
                                                  IDirect3DPixelShader9** ppShader) {
    if (!ppShader) return D3DERR_INVALIDCALL;
    *ppShader = new FacadePixelShader(pFunction);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::CreateStateBlock(D3DSTATEBLOCKTYPE /*Type*/,
                                                IDirect3DStateBlock9** ppSB) {
    if (!ppSB) return D3DERR_INVALIDCALL;
    *ppSB = new FacadeStateBlock();
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::CreateOffscreenPlainSurface(UINT Width, UINT Height,
                                                            D3DFORMAT Format, D3DPOOL /*Pool*/,
                                                            IDirect3DSurface9** ppSurface,
                                                            HANDLE* /*pSharedHandle*/) {
    if (!ppSurface) return D3DERR_INVALIDCALL;
    *ppSurface = new FacadeSurface(Width, Height, Format);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) {
    // Some Nival call-sites pass ppQuery == nullptr just to probe support.
    if (!ppQuery) return D3D_OK;
    *ppQuery = new FacadeQuery(Type);
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Surface ops
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::UpdateSurface(IDirect3DSurface9* /*pSrc*/, CONST RECT* /*pSrcRect*/,
                                              IDirect3DSurface9* /*pDst*/, CONST POINT* /*pDstPoint*/) {
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::UpdateTexture(IDirect3DBaseTexture9* /*pSrc*/, IDirect3DBaseTexture9* /*pDst*/) {
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetRenderTargetData(IDirect3DSurface9* /*pRT*/, IDirect3DSurface9* /*pDst*/) {
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::GetFrontBufferData(UINT /*iSwapChain*/, IDirect3DSurface9* /*pDst*/) {
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::StretchRect(IDirect3DSurface9* /*pSrc*/, CONST RECT* /*pSrcRect*/,
                                            IDirect3DSurface9* /*pDst*/, CONST RECT* /*pDstRect*/,
                                            D3DTEXTUREFILTERTYPE /*Filter*/) {
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::ColorFill(IDirect3DSurface9* /*pSurface*/, CONST RECT* /*pRect*/,
                                          D3DCOLOR /*color*/) {
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetRenderTarget(DWORD /*RenderTargetIndex*/, IDirect3DSurface9* /*pRT*/) {
    // Phase 1.5: pin everything to view 0 (the swapchain).  Distinct bgfx
    // framebuffers for offscreen render targets are deferred to Phase 2 — for
    // now Nival's screen/register/texture targets all collapse onto the
    // back-buffer so we at least see the back-buffer content (clear color,
    // any UI tris that happen to submit through DrawPrimitive*).
    current_view_id_ = 0;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetRenderTarget(DWORD /*RenderTargetIndex*/, IDirect3DSurface9** ppRT) {
    if (!ppRT) return D3DERR_INVALIDCALL;
    *ppRT = new FacadeSurface(1920, 1080, D3DFMT_X8R8G8B8);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetDepthStencilSurface(IDirect3DSurface9* /*pNewZStencil*/) {
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetDepthStencilSurface(IDirect3DSurface9** ppZStencil) {
    if (!ppZStencil) return D3DERR_INVALIDCALL;
    *ppZStencil = new FacadeSurface(1920, 1080, D3DFMT_D24S8);
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::BeginScene() {
    if (!scene_active_) {
        silent_storm::renderer::begin_frame();
        scene_active_ = true;
    }
    static int once = 0;
    if (once < 3) {
        ++once;
        if (FILE* f = draw_log()) {
            fprintf(f, "[scene#%d] BeginScene view=%u draw_total=%llu\n",
                    once, current_view_id_,
                    (unsigned long long)g_total_draw_calls);
            fflush(f);
        }
    }
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::EndScene()   {
    // Present handles bgfx::frame(). EndScene just marks the end of the
    // logical scene; bgfx accumulates submit() calls until frame() is called.
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Clear — forward to bgfx view clear + touch
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::Clear(DWORD /*Count*/, CONST D3DRECT* /*pRects*/,
                                     DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) {
    uint16_t bgfx_flags = 0;
    if (Flags & D3DCLEAR_TARGET)  bgfx_flags |= BGFX_CLEAR_COLOR;
    if (Flags & D3DCLEAR_ZBUFFER) bgfx_flags |= BGFX_CLEAR_DEPTH;
    if (Flags & D3DCLEAR_STENCIL) bgfx_flags |= BGFX_CLEAR_STENCIL;

    // D3DCOLOR is ARGB; bgfx uses RGBA
    uint32_t rgba = ((Color & 0x00FF0000) << 8)  |  // R
                    ((Color & 0x0000FF00) << 8)  |  // G
                    ((Color & 0x000000FF) << 8)  |  // B
                    ((Color & 0xFF000000) >> 24);    // A

    bgfx::setViewClear(current_view_id_, bgfx_flags, rgba, Z, static_cast<uint8_t>(Stencil));
    bgfx::touch(current_view_id_);
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// T10/T11 helpers (file-scope so they don't pollute the anonymous namespace
// that is shared with the draw-call helpers lower in the file).
// ---------------------------------------------------------------------------

// Returns true if the matrix is an orthographic projection.
// Ortho: no perspective divide — _44 == 1 and _34 == 0.
static bool is_ortho(const D3DMATRIX& m) {
    return m._44 == 1.0f && m._34 == 0.0f;
}

// Returns true if the matrix looks like a perspective projection.
// D3D perspective: _44 == 0 and _34 == -1 (LH) or +1 (RH).
static bool is_perspective(const D3DMATRIX& m) {
    return m._44 == 0.0f && (m._34 == -1.0f || m._34 == 1.0f);
}

// Build a D3D-style left-handed perspective matrix with the given FOV/aspect/near/far.
// Matches D3DXMatrixPerspectiveFovLH exactly so we can drop in as a replacement.
static D3DMATRIX make_perspective_lh(float fov_y_rad, float aspect, float zn, float zf) {
    float y_scale = 1.0f / std::tan(fov_y_rad * 0.5f);
    float x_scale = y_scale / aspect;
    D3DMATRIX p{};
    p._11 = x_scale;
    p._22 = y_scale;
    p._33 = zf / (zf - zn);
    p._34 = 1.0f;           // LH: row3 col4
    p._43 = -(zn * zf) / (zf - zn);
    p._44 = 0.0f;
    return p;
}

// Compute integer HUD scale: cfg value if explicit, else floor(min(w/1024, h/768))
// clamped to [1, 4].
static int compute_hud_scale(int w, int h, int cfg_value) {
    if (cfg_value > 0) return std::min(cfg_value, 4);
    float auto_scale = std::floor(std::min(w / 1024.0f, h / 768.0f));
    int s = static_cast<int>(auto_scale);
    return std::max(1, std::min(s, 4));
}

// ---------------------------------------------------------------------------
// Transforms
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) {
    if (!pMatrix) return D3DERR_INVALIDCALL;

    DWORD s = static_cast<DWORD>(State);

    if (s == 256 /*D3DTS_WORLD*/) {
        state_.world = *pMatrix;
        return D3D_OK;
    }
    if (State == D3DTS_VIEW) {
        state_.view = *pMatrix;
        return D3D_OK;
    }
    if (State == D3DTS_PROJECTION) {
        // ----- T11: orthographic (HUD/menu) path -----
        if (is_ortho(*pMatrix)) {
            int scale = compute_hud_scale(get_width(), get_height(), cfg_.display.hud_scale);
            if (scale > 1) {
                D3DMATRIX scaled = *pMatrix;
                scaled._11 *= static_cast<float>(scale);
                scaled._22 *= static_cast<float>(scale);
                state_.projection      = scaled;
                state_.hud_scale_active = scale;
            } else {
                state_.projection      = *pMatrix;
                state_.hud_scale_active = 1;
            }
            set_hud_scale(state_.hud_scale_active);
            return D3D_OK;
        }

        // ----- T10: perspective — optionally override FOV -----
        state_.hud_scale_active = 1;
        set_hud_scale(1);

        if (cfg_.display.fov_horizontal > 0.0f && is_perspective(*pMatrix)) {
            // Extract near/far from the incoming D3D LH perspective matrix:
            //   m._33 = zf/(zf-zn)     m._43 = -zn*zf/(zf-zn)
            // So: zn = -m._43 / m._33   zf = m._43 / (m._33 - 1)
            // Guard against degenerate values; fall back to safe defaults.
            float zn = 0.1f, zf = 10000.0f;
            if (std::abs(pMatrix->_33) > 1e-6f) {
                float zn_derived = -pMatrix->_43 / pMatrix->_33;
                float zf_derived =  pMatrix->_43 / (pMatrix->_33 - 1.0f);
                if (zn_derived > 0.001f && zf_derived > zn_derived) {
                    zn = zn_derived;
                    zf = zf_derived;
                }
            }

            // Convert horizontal FOV to vertical FOV for make_perspective_lh.
            float aspect   = (get_height() > 0)
                             ? static_cast<float>(get_width()) / static_cast<float>(get_height())
                             : 16.0f / 9.0f;
            float fov_h_rad = cfg_.display.fov_horizontal * (3.14159265358979323846f / 180.0f);
            // fov_v = 2 * atan(tan(fov_h/2) / aspect)
            float fov_v_rad = 2.0f * std::atan(std::tan(fov_h_rad * 0.5f) / aspect);

            state_.projection = make_perspective_lh(fov_v_rad, aspect, zn, zf);
            return D3D_OK;
        }

        // No override — store as-is.
        state_.projection = *pMatrix;
        return D3D_OK;
    }

    // bone matrices (D3DTS_WORLDMATRIX(1..n)) — ignore for now (T9 carryover)
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {
    if (!pMatrix) return D3DERR_INVALIDCALL;
    DWORD s = static_cast<DWORD>(State);
    if (s == 256 /*D3DTS_WORLD*/)        *pMatrix = state_.world;
    else if (State == D3DTS_VIEW)        *pMatrix = state_.view;
    else if (State == D3DTS_PROJECTION)  *pMatrix = state_.projection;
    else std::memset(pMatrix, 0, sizeof(*pMatrix));
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::MultiplyTransform(D3DTRANSFORMSTATETYPE /*State*/, CONST D3DMATRIX* /*pMatrix*/) {
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Viewport
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetViewport(CONST D3DVIEWPORT9* pViewport) {
    if (!pViewport) return D3DERR_INVALIDCALL;
    viewport_ = *pViewport;
    state_.viewport_x = pViewport->X;
    state_.viewport_y = pViewport->Y;
    state_.viewport_w = pViewport->Width;
    state_.viewport_h = pViewport->Height;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetViewport(D3DVIEWPORT9* pViewport) {
    if (!pViewport) return D3DERR_INVALIDCALL;
    *pViewport = viewport_;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Material
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetMaterial(CONST D3DMATERIAL9* pMaterial) {
    if (!pMaterial) return D3DERR_INVALIDCALL;
    material_ = *pMaterial;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetMaterial(D3DMATERIAL9* pMaterial) {
    if (!pMaterial) return D3DERR_INVALIDCALL;
    *pMaterial = material_;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Lights (not implemented in bgfx facade — Nival's fixed-function lighting)
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetLight(DWORD /*Index*/, CONST D3DLIGHT9* /*pLight*/) { return D3D_OK; }
HRESULT __stdcall D3D9Facade::GetLight(DWORD /*Index*/, D3DLIGHT9* pLight) {
    if (!pLight) return D3DERR_INVALIDCALL;
    std::memset(pLight, 0, sizeof(*pLight));
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::LightEnable(DWORD /*Index*/, BOOL /*Enable*/) { return D3D_OK; }
HRESULT __stdcall D3D9Facade::GetLightEnable(DWORD /*Index*/, BOOL* pEnable) {
    if (!pEnable) return D3DERR_INVALIDCALL;
    *pEnable = FALSE;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Clip planes
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetClipPlane(DWORD /*Index*/, CONST float* /*pPlane*/) { return D3D_OK; }
HRESULT __stdcall D3D9Facade::GetClipPlane(DWORD /*Index*/, float* pPlane) {
    if (!pPlane) return D3DERR_INVALIDCALL;
    std::memset(pPlane, 0, sizeof(float) * 4);
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Render states
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) {
    state_.set_render_state(State, Value);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) {
    if (!pValue) return D3DERR_INVALIDCALL;
    if (State < 210)
        *pValue = state_.render_state[State];
    else
        *pValue = 0;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// State blocks (stubs — T9 can add recording if Nival uses them)
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::BeginStateBlock() { return D3D_OK; }
HRESULT __stdcall D3D9Facade::EndStateBlock(IDirect3DStateBlock9** ppSB) {
    if (ppSB) *ppSB = nullptr;
    return E_NOTIMPL;
}

// ---------------------------------------------------------------------------
// Clip status
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetClipStatus(CONST D3DCLIPSTATUS9* pCS) {
    if (!pCS) return D3DERR_INVALIDCALL;
    clip_status_ = *pCS;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetClipStatus(D3DCLIPSTATUS9* pCS) {
    if (!pCS) return D3DERR_INVALIDCALL;
    *pCS = clip_status_;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) {
    if (!ppTexture) return D3DERR_INVALIDCALL;
    *ppTexture = (Stage < 8) ? reinterpret_cast<IDirect3DBaseTexture9*>(state_.texture[Stage]) : nullptr;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) {
    if (Stage < 8) state_.texture[Stage] = pTexture;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Texture stage state
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) {
    if (!pValue) return D3DERR_INVALIDCALL;
    *pValue = (Stage < 8 && Type < 33) ? state_.tex_stage_state[Stage][Type] : 0;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) {
    state_.set_texture_stage_state(Stage, Type, Value);
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Sampler state
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) {
    if (!pValue) return D3DERR_INVALIDCALL;
    *pValue = (Sampler < 16 && Type < 14) ? sampler_state_[Sampler][Type] : 0;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) {
    if (Sampler < 16 && Type < 14) sampler_state_[Sampler][Type] = Value;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Validate device
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::ValidateDevice(DWORD* pNumPasses) {
    if (pNumPasses) *pNumPasses = 1;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Palette
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetPaletteEntries(UINT /*PaletteNumber*/, CONST PALETTEENTRY* /*pEntries*/) { return D3D_OK; }
HRESULT __stdcall D3D9Facade::GetPaletteEntries(UINT /*PaletteNumber*/, PALETTEENTRY* pEntries) {
    if (!pEntries) return D3DERR_INVALIDCALL;
    std::memset(pEntries, 0, sizeof(PALETTEENTRY) * 256);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetCurrentTexturePalette(UINT PaletteNumber) {
    current_palette_ = PaletteNumber;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetCurrentTexturePalette(UINT* PaletteNumber) {
    if (!PaletteNumber) return D3DERR_INVALIDCALL;
    *PaletteNumber = current_palette_;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Scissor rect
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetScissorRect(CONST RECT* pRect) {
    if (!pRect) return D3DERR_INVALIDCALL;
    scissor_rect_ = *pRect;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetScissorRect(RECT* pRect) {
    if (!pRect) return D3DERR_INVALIDCALL;
    *pRect = scissor_rect_;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Software vertex processing
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetSoftwareVertexProcessing(BOOL /*bSoftware*/) { return D3D_OK; }
BOOL    __stdcall D3D9Facade::GetSoftwareVertexProcessing() { return FALSE; }

// ---------------------------------------------------------------------------
// N-patches
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetNPatchMode(float /*nSegments*/) { return D3D_OK; }
float   __stdcall D3D9Facade::GetNPatchMode() { return 0.0f; }

// ---------------------------------------------------------------------------
// Draw calls — T9: route through bgfx
// ---------------------------------------------------------------------------
namespace {

// r62: convert a D3D9 vertex declaration into a bgfx VertexLayout. Walks
// the FacadeVertexDeclaration's element array (terminated by D3DDECL_END).
// Maps:
//   D3DDECLUSAGE_POSITION   -> bgfx::Attrib::Position
//   D3DDECLUSAGE_NORMAL     -> bgfx::Attrib::Normal
//   D3DDECLUSAGE_COLOR (n)  -> bgfx::Attrib::Color0/1
//   D3DDECLUSAGE_TEXCOORD(n)-> bgfx::Attrib::TexCoord0..7
//   D3DDECLUSAGE_BLENDWEIGHT-> Attrib::Weight
//   D3DDECLUSAGE_BLENDINDICES->Attrib::Indices
// Skips unknown usages (binormal/tangent etc — bgfx has slots, omit for now).
// Returns a layout matching the on-disk stride so DrawIndexedPrimitive's
// 1:1 memcpy gives bgfx every byte the shader expects.
bgfx::VertexLayout make_layout_from_decl(const D3DVERTEXELEMENT9* elements, UINT stride) {
    bgfx::VertexLayout l;
    l.begin();
    if (!elements) {
        l.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
        l.end();
        return l;
    }
    for (int i = 0; i < 64; ++i) {
        const D3DVERTEXELEMENT9& e = elements[i];
        if (e.Stream == 0xFF) break;  // D3DDECL_END
        if (e.Stream != 0) continue;  // we only handle stream 0
        bgfx::Attrib::Enum attr = bgfx::Attrib::Position;
        bool valid = true;
        switch (e.Usage) {
            case D3DDECLUSAGE_POSITION:   attr = bgfx::Attrib::Position; break;
            case D3DDECLUSAGE_NORMAL:     attr = bgfx::Attrib::Normal;   break;
            case D3DDECLUSAGE_COLOR:
                attr = (e.UsageIndex == 0) ? bgfx::Attrib::Color0 : bgfx::Attrib::Color1;
                break;
            case D3DDECLUSAGE_TEXCOORD:
                if (e.UsageIndex < 8) {
                    attr = static_cast<bgfx::Attrib::Enum>(
                        static_cast<int>(bgfx::Attrib::TexCoord0) + e.UsageIndex);
                } else valid = false;
                break;
            case D3DDECLUSAGE_BLENDWEIGHT: attr = bgfx::Attrib::Weight;  break;
            case D3DDECLUSAGE_BLENDINDICES:attr = bgfx::Attrib::Indices; break;
            case D3DDECLUSAGE_TANGENT:    attr = bgfx::Attrib::Tangent;  break;
            case D3DDECLUSAGE_BINORMAL:   attr = bgfx::Attrib::Bitangent;break;
            default: valid = false; break;
        }
        if (!valid) continue;
        int num = 4;
        bgfx::AttribType::Enum t = bgfx::AttribType::Float;
        bool normalized = false;
        switch (e.Type) {
            case D3DDECLTYPE_FLOAT1: num=1; t=bgfx::AttribType::Float; break;
            case D3DDECLTYPE_FLOAT2: num=2; t=bgfx::AttribType::Float; break;
            case D3DDECLTYPE_FLOAT3: num=3; t=bgfx::AttribType::Float; break;
            case D3DDECLTYPE_FLOAT4: num=4; t=bgfx::AttribType::Float; break;
            case D3DDECLTYPE_D3DCOLOR:
                num=4; t=bgfx::AttribType::Uint8; normalized=true;
                break;
            case D3DDECLTYPE_UBYTE4: num=4; t=bgfx::AttribType::Uint8; normalized=false; break;
            case D3DDECLTYPE_UBYTE4N: num=4; t=bgfx::AttribType::Uint8; normalized=true; break;
            case D3DDECLTYPE_SHORT2: num=2; t=bgfx::AttribType::Int16; normalized=false; break;
            case D3DDECLTYPE_SHORT4: num=4; t=bgfx::AttribType::Int16; normalized=false; break;
            case D3DDECLTYPE_SHORT2N:num=2; t=bgfx::AttribType::Int16; normalized=true; break;
            case D3DDECLTYPE_SHORT4N:num=4; t=bgfx::AttribType::Int16; normalized=true; break;
            case D3DDECLTYPE_FLOAT16_2: num=2; t=bgfx::AttribType::Half; break;
            case D3DDECLTYPE_FLOAT16_4: num=4; t=bgfx::AttribType::Half; break;
            default: continue;
        }
        l.add(attr, static_cast<uint8_t>(num), t, normalized);
    }
    l.end();
    (void)stride;
    return l;
}

// Build a generic transient vertex layout from FVF + stride.  This is a best-
// effort mapping — Nival uses both FVF and shader-driven vertex decls; for v1
// we only handle the simple FVF case.  Unsupported FVF returns a minimal
// position-only layout sized to `stride` (padded with `Skip`) so bgfx accepts it.
bgfx::VertexLayout make_layout_from_fvf(DWORD fvf, UINT stride) {
    bgfx::VertexLayout l;
    l.begin();
    if (fvf == 0) {
        // No FVF available (programmable pipeline). Stub with a position attribute.
        l.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
    } else {
        if ((fvf & D3DFVF_XYZ) || (fvf & D3DFVF_XYZRHW)) {
            l.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float);
        }
        if (fvf & D3DFVF_NORMAL) {
            l.add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float);
        }
        if (fvf & D3DFVF_DIFFUSE) {
            l.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, /*normalized*/ true);
        }
        if (fvf & D3DFVF_SPECULAR) {
            l.add(bgfx::Attrib::Color1, 4, bgfx::AttribType::Uint8, /*normalized*/ true);
        }
        const int tex_count = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
        for (int i = 0; i < tex_count && i < 8; ++i) {
            const bgfx::Attrib::Enum a = static_cast<bgfx::Attrib::Enum>(
                static_cast<int>(bgfx::Attrib::TexCoord0) + i);
            l.add(a, 2, bgfx::AttribType::Float);
        }
    }
    l.end();
    // If derived stride doesn't match D3D9 stride, leave the layout as-is —
    // bgfx will use the layout's reported stride and Nival's vertex layout may
    // not exactly match the shader inputs.  This is the deferred fix for T9.5.
    (void)stride;
    return l;
}

// Vertices per primitive count for a given primitive type.
UINT verts_per_prim(D3DPRIMITIVETYPE pt) {
    switch (pt) {
        case D3DPT_POINTLIST:     return 1;
        case D3DPT_LINELIST:      return 2;
        case D3DPT_LINESTRIP:     return 2;
        case D3DPT_TRIANGLELIST:  return 3;
        case D3DPT_TRIANGLESTRIP: return 3;
        case D3DPT_TRIANGLEFAN:   return 3;
        default:                  return 3;
    }
}

// Compute the world * view * projection matrix and feed bgfx::setTransform
// so shaders' built-in u_modelViewProj uniform sees the right transform.
void apply_transform_matrix(const DeviceState& s) {
    // bgfx wants the model matrix (it concatenates with view matrix set via
    // setViewTransform).  But because Nival also drives the projection
    // matrix via SetTransform, we collapse all three into the model matrix
    // and leave bgfx's view/proj as identity.
    auto mul4x4 = [](const D3DMATRIX& A, const D3DMATRIX& B, D3DMATRIX& out) {
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) {
                float acc = 0.0f;
                for (int k = 0; k < 4; ++k)
                    acc += A.m[r][k] * B.m[k][c];
                out.m[r][c] = acc;
            }
    };
    D3DMATRIX wv, wvp;
    mul4x4(s.world, s.view, wv);
    mul4x4(wv, s.projection, wvp);
    // bgfx is column-major, D3DMATRIX is row-major — transpose.
    float bgfx_m[16];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            bgfx_m[c * 4 + r] = wvp.m[r][c];
    bgfx::setTransform(bgfx_m);
}

// r62: when Nival uses programmable VS, SetTransform may never be called.
// Their vertex shaders almost always upload the MVP matrix to c0..c3 via
// SetVertexShaderConstantF. D3D9 shaders see c0..c3 as 4 row vectors stored
// in *row-major* order; the shader code does `mul(mvp, position)` interpreted
// as column-vector mul where mvp is set up as transpose of D3D's row-major.
// Net: c0..c3 hold the WVP matrix transposed (the same way HLSL's
// `mul(input.pos, world*view*proj)` would set it). For our bgfx side, we want
// the column-major matrix that the shader's `u_modelViewProj * pos` expects.
// Trial: write c0..c3 directly as the bgfx column matrix (4 floats per row,
// taken in order) — this matches the standard D3D9 shader convention.
// r62/r63: Nival writes their MVP matrix to c10..c13 (verified via dump:
// first non-zero contiguous 4-vec block is at c10, with shape that matches an
// ortho projection for UI overlays).  For 3D mission render they likely use
// the same c10..c13 slot for view*proj and apply their own world matrix
// inline.  Hard-code c10 as the matrix start; fall back to c0 if c10 looks
// uninitialized.
int find_matrix_start(const float vs_const_f[128][4]) {
    const int candidates[] = { 10, 0, 4, 8, 16, 20 };
    for (int s : candidates) {
        bool nonzero = false;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                if (vs_const_f[s + r][c] != 0.0f) { nonzero = true; break; }
            }
            if (nonzero) break;
        }
        if (nonzero) return s;
    }
    return 0;
}

void apply_vs_const_transform(const float vs_const_f[128][4]) {
    int s = find_matrix_start(vs_const_f);
    float m[16];
    // r63: Nival writes the MVP matrix in row-major form to c[s..s+3]:
    //   c[s+r] = row r = (M[r][0], M[r][1], M[r][2], M[r][3])
    // bgfx::setTransform expects column-major (m[c*4+r] = M[r][c]).
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            m[c * 4 + r] = vs_const_f[s + r][c];
        }
    }
    bgfx::setTransform(m);

    static int once = 0;
    if (once < 4) {
        ++once;
        if (FILE* f = draw_log()) {
            fprintf(f, "[vs_const#%d] picked start=c%d\n", once, s);
            for (int i = 0; i < 32; ++i) {
                bool any = vs_const_f[i][0] != 0 || vs_const_f[i][1] != 0 ||
                           vs_const_f[i][2] != 0 || vs_const_f[i][3] != 0;
                if (any) {
                    fprintf(f, "  c%-2d=[%9.3f %9.3f %9.3f %9.3f]%s\n", i,
                            vs_const_f[i][0], vs_const_f[i][1],
                            vs_const_f[i][2], vs_const_f[i][3],
                            (i >= s && i < s+4) ? " <-- mat" : "");
                }
            }
            fprintf(f, "  bgfx m[col0]=(%.3f %.3f %.3f %.3f) m[col3]=(%.3f %.3f %.3f %.3f)\n",
                    m[0], m[1], m[2], m[3], m[12], m[13], m[14], m[15]);
            // Simulate transforming pos=(863.5, 31.5, 1.0)
            float px = 863.5f, py = 31.5f, pz = 1.0f, pw = 1.0f;
            float cx = m[0]*px + m[4]*py + m[8]*pz + m[12]*pw;
            float cy = m[1]*px + m[5]*py + m[9]*pz + m[13]*pw;
            float cz = m[2]*px + m[6]*py + m[10]*pz + m[14]*pw;
            float cw = m[3]*px + m[7]*py + m[11]*pz + m[15]*pw;
            fprintf(f, "  test pos(863.5,31.5,1.0) -> clip(%.3f, %.3f, %.3f, %.3f) ndc(%.3f, %.3f, %.3f)\n",
                    cx, cy, cz, cw,
                    cw != 0 ? cx/cw : 0, cw != 0 ? cy/cw : 0, cw != 0 ? cz/cw : 0);
            fflush(f);
        }
    }
}

// r62: lazy-create 1x1 white texture used as fallback when no texture is bound
// at stage 0 (so shaders that sample s_diffuse don't render black for missing
// bindings).
bgfx::TextureHandle get_fallback_white_texture() {
    static bgfx::TextureHandle s_white = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(s_white)) {
        const uint32_t white = 0xffffffffu;
        const bgfx::Memory* mem = bgfx::copy(&white, sizeof(white));
        s_white = bgfx::createTexture2D(1, 1, false, 1,
                                        bgfx::TextureFormat::BGRA8,
                                        BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE,
                                        mem);
    }
    return s_white;
}

// r62: pick a shader archetype that matches the current state pipeline.
// When Nival uses programmable VS+PS (current_vs_/current_ps_ set), the
// state_translator's heuristics still tell us what *kind* of geometry it is
// (lit terrain, particle, UI...).  We just override with ss_diffuse_unlit as
// the safe baseline so position+texcoord visible geometry works first; later
// rounds can tune per archetype.
const char* select_archetype_for_drawpath(const DeviceState& s, bool programmable) {
    if (programmable) {
        // For programmable path, prefer ss_diffuse_unlit (it samples a 2D
        // texture with position+texcoord) so geometry shows up even if no
        // texture is bound (white fallback turns it into solid white).
        return "ss_diffuse_unlit";
    }
    return s.select_shader_archetype();
}

} // anonymous namespace

HRESULT __stdcall D3D9Facade::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,
                                             UINT StartVertex, UINT PrimitiveCount) {
    trace_draw_entry("DrawPrimitive");
    auto* vb = static_cast<FacadeVertexBuffer*>(stream_vbo_[0]);
    if (!vb) { trace_reason("DP.no_vb", &g_reasons.no_vb); return D3D_OK; }
    if (stream_stride_[0] == 0) { trace_reason("DP.no_stride", &g_reasons.no_stride); return D3D_OK; }

    UINT verts = PrimitiveCount * verts_per_prim(PrimitiveType);
    UINT stride = stream_stride_[0];
    UINT vb_cpu_size = vb->cpu_size();
    if (vb_cpu_size == 0) { trace_reason("DP.empty_cpu_vb", &g_reasons.empty_cpu_vb); return D3D_OK; }

    bgfx::VertexLayout layout;
    if (current_vdecl_) {
        auto* fd = static_cast<FacadeVertexDeclaration*>(current_vdecl_);
        layout = make_layout_from_decl(fd->elements.data(), stride);
    } else {
        layout = make_layout_from_fvf(vb->fvf | fvf_, stride);
    }
    const uint16_t layout_stride = layout.getStride();

    // Clamp by what cpu_buf can supply.
    UINT max_verts = vb_cpu_size / stride;
    if (StartVertex >= max_verts) return D3D_OK;
    if (StartVertex + verts > max_verts) verts = max_verts - StartVertex;
    if (verts == 0) return D3D_OK;

    if (bgfx::getAvailTransientVertexBuffer(verts, layout) < verts) {
        trace_reason("DP.no_tvb_space", &g_reasons.no_tvb_space);
        return D3D_OK;
    }
    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, verts, layout);

    // r61: real-data copy from VB cpu_buf_ — was previously memset zero.
    const uint8_t* src = vb->cpu_data() + StartVertex * stride;
    if (stride == layout_stride) {
        std::memcpy(tvb.data, src, std::min<UINT>(verts * stride, tvb.size));
    } else {
        UINT per_vert = stride < layout_stride ? stride : layout_stride;
        for (UINT v = 0; v < verts; ++v) {
            if ((v + 1) * layout_stride > tvb.size) break;
            std::memcpy(tvb.data + v * layout_stride, src + v * stride, per_vert);
        }
    }
    ++g_reasons.real_data;

    bool programmable_dp = (current_vs_ != nullptr);
    if (programmable_dp) apply_vs_const_transform(vs_const_f_);
    else                 apply_transform_matrix(state_);
    bgfx::setState(state_.build_bgfx_state());
    bool any_tex_dp = false;
    for (int i = 0; i < 8; ++i) {
        if (state_.texture[i]) {
            auto* tex = static_cast<FacadeTexture*>(state_.texture[i]);
            if (bgfx::isValid(tex->handle)) {
                bgfx::setTexture(static_cast<uint8_t>(i),
                                 get_sampler_uniform(static_cast<unsigned>(i)),
                                 tex->handle);
                if (i == 0) any_tex_dp = true;
            }
        }
    }
    if (!any_tex_dp) {
        bgfx::setTexture(0, get_sampler_uniform(0), get_fallback_white_texture());
    }
    bgfx::setVertexBuffer(0, &tvb);

    const char* arch = select_archetype_for_drawpath(state_, programmable_dp);
    bgfx::ProgramHandle prog = get_program(arch);
    if (bgfx::isValid(prog)) { trace_submit(arch, prog); bgfx::submit(current_view_id_, prog); }
    else { trace_invalid_prog(arch); bgfx::discard(); }
    return D3D_OK;
}

HRESULT __stdcall D3D9Facade::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType,
                                                    INT BaseVertexIndex, UINT MinVertexIndex,
                                                    UINT NumVertices, UINT startIndex,
                                                    UINT primCount) {
    trace_draw_entry("DrawIndexedPrimitive");
    auto* vb = static_cast<FacadeVertexBuffer*>(stream_vbo_[0]);
    auto* ib = static_cast<FacadeIndexBuffer*>(ibo_);
    if (!vb) { trace_reason("DIP.no_vb", &g_reasons.no_vb); return D3D_OK; }
    if (!ib) { trace_reason("DIP.no_ib", &g_reasons.no_ib); return D3D_OK; }
    if (stream_stride_[0] == 0) { trace_reason("DIP.no_stride", &g_reasons.no_stride); return D3D_OK; }

    UINT stride = stream_stride_[0];
    UINT vb_cpu_size = vb->cpu_size();
    UINT ib_cpu_size = ib->cpu_size();
    trace_dip_args(NumVertices, startIndex, primCount, stride, vb->fvf | fvf_,
                   vb_cpu_size, ib_cpu_size);

    if (vb_cpu_size == 0) { trace_reason("DIP.empty_cpu_vb", &g_reasons.empty_cpu_vb); return D3D_OK; }

    // r62: prefer decl-driven layout over FVF when both are present (decl wins).
    bgfx::VertexLayout layout;
    if (current_vdecl_) {
        auto* fd = static_cast<FacadeVertexDeclaration*>(current_vdecl_);
        layout = make_layout_from_decl(fd->elements.data(), stride);
        // r65: track unique (VB,stride) pairs so we get a wider sampling of
        // what's being drawn — not just the same UI mesh.
        static const void* s_seen_vb[16] = {};
        static int s_seen_count = 0;
        bool first_seen = true;
        for (int i = 0; i < s_seen_count; ++i) if (s_seen_vb[i] == vb) { first_seen = false; break; }
        if (first_seen && s_seen_count < 16) {
            s_seen_vb[s_seen_count++] = vb;
            if (FILE* f = draw_log()) {
                fprintf(f, "[vb#%d=%p] decl_stride=%u d3d_stride=%u vb_size=%u",
                        s_seen_count, vb, layout.getStride(), stride, vb_cpu_size);
                fprintf(f, "  elements:");
                for (const auto& e : fd->elements) {
                    if (e.Stream == 0xFF) break;
                    fprintf(f, " (s=%u o=%u t=%u u=%u/%u)",
                            e.Stream, e.Offset, e.Type, e.Usage, e.UsageIndex);
                }
                fprintf(f, "\n");
                const uint8_t* vd = vb->cpu_data();
                // Sample first 4 verts pos
                for (int v = 0; v < 4 && (uint64_t)(v+1)*stride <= vb->cpu_size(); ++v) {
                    float fx, fy, fz;
                    std::memcpy(&fx, vd + v*stride + 0, 4);
                    std::memcpy(&fy, vd + v*stride + 4, 4);
                    std::memcpy(&fz, vd + v*stride + 8, 4);
                    fprintf(f, "  v%d=(%.3f, %.3f, %.3f)\n", v, fx, fy, fz);
                }
                // Also sample a far vertex (in case first 8 are header/padding)
                UINT mid = vb->cpu_size() / (stride * 2);
                if (mid > 0 && (uint64_t)(mid+1)*stride <= vb->cpu_size()) {
                    float fx, fy, fz;
                    std::memcpy(&fx, vd + mid*stride + 0, 4);
                    std::memcpy(&fy, vd + mid*stride + 4, 4);
                    std::memcpy(&fz, vd + mid*stride + 8, 4);
                    fprintf(f, "  v_mid[%u]=(%.3f, %.3f, %.3f)\n", mid, fx, fy, fz);
                }
                fflush(f);
            }
        }
    } else {
        layout = make_layout_from_fvf(vb->fvf | fvf_, stride);
    }
    const uint16_t layout_stride = layout.getStride();

    UINT num_indices = primCount * verts_per_prim(PrimitiveType);
    UINT total_verts = MinVertexIndex + NumVertices;
    // Clamp to what CPU buffer can supply (avoid OOB read on tvb fill).
    UINT max_verts_from_cpu = vb_cpu_size / stride;
    if (total_verts > max_verts_from_cpu) total_verts = max_verts_from_cpu;
    if (total_verts == 0) return D3D_OK;

    if (bgfx::getAvailTransientVertexBuffer(total_verts, layout) < total_verts) {
        trace_reason("DIP.no_tvb_space", &g_reasons.no_tvb_space);
        return D3D_OK;
    }
    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, total_verts, layout);

    // r61: actually copy real vertex data from VB CPU buffer.
    // D3D9 layout's stride may differ from bgfx layout's stride (we don't
    // currently mirror declarations 1:1).  Best-effort: if strides match, do
    // one big memcpy; else copy per-vertex the min(d3d_stride, layout_stride)
    // bytes so at least the position field is correct (always the first attr).
    const uint8_t* src = vb->cpu_data();
    UINT copy_bytes = static_cast<UINT>(total_verts) * stride;
    if (copy_bytes > vb_cpu_size) copy_bytes = vb_cpu_size;
    if (stride == layout_stride) {
        // Strides match — clean bulk copy.
        std::memcpy(tvb.data, src, std::min<UINT>(copy_bytes, tvb.size));
    } else {
        // Per-vertex copy of the lesser stride so position (first attr) lands.
        UINT per_vert = stride < layout_stride ? stride : layout_stride;
        UINT vbytes = tvb.size;
        UINT pos = 0;
        uint8_t* dst = tvb.data;
        for (UINT v = 0; v < total_verts && pos + layout_stride <= vbytes &&
                         (v + 1) * stride <= copy_bytes; ++v) {
            std::memcpy(dst + v * layout_stride, src + v * stride, per_vert);
            pos += layout_stride;
        }
    }
    ++g_reasons.real_data;

    // r62: choose transform source based on pipeline mode
    bool programmable = (current_vs_ != nullptr);
    if (programmable) {
        apply_vs_const_transform(vs_const_f_);
    } else {
        apply_transform_matrix(state_);
    }
    // r62: SOFT debug — relax state to ALWAYS depth + no cull so we at least
    // see SOMETHING.  Keep color writes from D3D9.  Once we see geometry, we
    // can put proper state back.
    uint64_t st = state_.build_bgfx_state();
    // strip depth test, cull bits — keep write_z so we get sensible occlusion
    st &= ~(BGFX_STATE_DEPTH_TEST_MASK | BGFX_STATE_CULL_MASK);
    st |= BGFX_STATE_DEPTH_TEST_ALWAYS;
    // r63: ALSO strip blend bits (BLEND_MASK) to make output fully opaque.
    st &= ~BGFX_STATE_BLEND_MASK;
    bgfx::setState(st);

    // r62: always bind stage 0 — fallback to 1x1 white if nothing bound.
    bool any_tex = false;
    for (int i = 0; i < 8; ++i) {
        if (state_.texture[i]) {
            auto* tex = static_cast<FacadeTexture*>(state_.texture[i]);
            if (bgfx::isValid(tex->handle)) {
                bgfx::setTexture(static_cast<uint8_t>(i),
                                 get_sampler_uniform(static_cast<unsigned>(i)),
                                 tex->handle);
                if (i == 0) any_tex = true;
            }
        }
    }
    if (!any_tex) {
        bgfx::setTexture(0, get_sampler_uniform(0), get_fallback_white_texture());
    }
    bgfx::setVertexBuffer(0, &tvb, /*offset*/ 0, /*numVertices*/ total_verts);

    // Index buffer — prefer the bgfx-resident handle if Unlock created one,
    // else build a transient IB from cpu_buf so we still draw something.
    if (bgfx::isValid(ib->handle)) {
        bgfx::setIndexBuffer(ib->handle, startIndex, num_indices);
    } else if (ib_cpu_size > 0) {
        bool is32 = ib->is32();
        UINT idx_bytes = is32 ? 4u : 2u;
        UINT max_indices_from_cpu = ib_cpu_size / idx_bytes;
        UINT use_indices = num_indices;
        if (startIndex + use_indices > max_indices_from_cpu) {
            if (startIndex >= max_indices_from_cpu) {
                trace_reason("DIP.no_tib_space", &g_reasons.no_tib_space);
                bgfx::discard();
                return D3D_OK;
            }
            use_indices = max_indices_from_cpu - startIndex;
        }
        if (bgfx::getAvailTransientIndexBuffer(use_indices, is32) < use_indices) {
            trace_reason("DIP.no_tib_space", &g_reasons.no_tib_space);
            bgfx::discard();
            return D3D_OK;
        }
        bgfx::TransientIndexBuffer tib;
        bgfx::allocTransientIndexBuffer(&tib, use_indices, is32);
        std::memcpy(tib.data, ib->cpu_data() + startIndex * idx_bytes,
                    use_indices * idx_bytes);
        bgfx::setIndexBuffer(&tib, 0, use_indices);
    } else {
        bgfx::discard();
        return D3D_OK;
    }
    (void)BaseVertexIndex;

    const char* arch = select_archetype_for_drawpath(state_, programmable);
    bgfx::ProgramHandle prog = get_program(arch);
    if (bgfx::isValid(prog)) {
        trace_submit(arch, prog);
        ++g_reasons.submitted;
        bgfx::submit(current_view_id_, prog);
    }
    else { trace_invalid_prog(arch); bgfx::discard(); }
    return D3D_OK;
}

HRESULT __stdcall D3D9Facade::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,
                                               UINT PrimitiveCount,
                                               CONST void* pVertexStreamZeroData,
                                               UINT VertexStreamZeroStride) {
    trace_draw_entry("DrawPrimitiveUP");
    if (!pVertexStreamZeroData || VertexStreamZeroStride == 0) return D3D_OK;
    UINT verts = PrimitiveCount * verts_per_prim(PrimitiveType);
    auto layout = make_layout_from_fvf(fvf_, VertexStreamZeroStride);
    if (bgfx::getAvailTransientVertexBuffer(verts, layout) < verts) return D3D_OK;
    bgfx::TransientVertexBuffer tvb;
    bgfx::allocTransientVertexBuffer(&tvb, verts, layout);
    std::memcpy(tvb.data, pVertexStreamZeroData,
                std::min<UINT>(tvb.size, verts * VertexStreamZeroStride));

    apply_transform_matrix(state_);
    bgfx::setState(state_.build_bgfx_state());
    for (int i = 0; i < 8; ++i) {
        if (state_.texture[i]) {
            auto* tex = static_cast<FacadeTexture*>(state_.texture[i]);
            if (bgfx::isValid(tex->handle)) {
                bgfx::setTexture(static_cast<uint8_t>(i),
                                 get_sampler_uniform(static_cast<unsigned>(i)),
                                 tex->handle);
            }
        }
    }
    bgfx::setVertexBuffer(0, &tvb);

    const char* arch = state_.select_shader_archetype();
    bgfx::ProgramHandle prog = get_program(arch);
    if (bgfx::isValid(prog)) { trace_submit(arch, prog); bgfx::submit(current_view_id_, prog); }
    else { trace_invalid_prog(arch); bgfx::discard(); }
    return D3D_OK;
}

HRESULT __stdcall D3D9Facade::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,
                                                      UINT MinVertexIndex, UINT NumVertices,
                                                      UINT PrimitiveCount,
                                                      CONST void* pIndexData,
                                                      D3DFORMAT IndexDataFormat,
                                                      CONST void* pVertexStreamZeroData,
                                                      UINT VertexStreamZeroStride) {
    trace_draw_entry("DrawIndexedPrimitiveUP");
    if (!pVertexStreamZeroData || !pIndexData || VertexStreamZeroStride == 0) return D3D_OK;
    UINT total_verts = MinVertexIndex + NumVertices;
    UINT num_indices = PrimitiveCount * verts_per_prim(PrimitiveType);

    auto layout = make_layout_from_fvf(fvf_, VertexStreamZeroStride);
    bool is32 = (IndexDataFormat == D3DFMT_INDEX32);
    if (bgfx::getAvailTransientVertexBuffer(total_verts, layout) < total_verts ||
        bgfx::getAvailTransientIndexBuffer(num_indices, is32) < num_indices) {
        return D3D_OK;
    }
    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer  tib;
    bgfx::allocTransientVertexBuffer(&tvb, total_verts, layout);
    bgfx::allocTransientIndexBuffer(&tib, num_indices, is32);
    std::memcpy(tvb.data, pVertexStreamZeroData,
                std::min<UINT>(tvb.size, total_verts * VertexStreamZeroStride));
    std::memcpy(tib.data, pIndexData, std::min<UINT>(tib.size, num_indices * (is32 ? 4u : 2u)));

    apply_transform_matrix(state_);
    bgfx::setState(state_.build_bgfx_state());
    for (int i = 0; i < 8; ++i) {
        if (state_.texture[i]) {
            auto* tex = static_cast<FacadeTexture*>(state_.texture[i]);
            if (bgfx::isValid(tex->handle)) {
                bgfx::setTexture(static_cast<uint8_t>(i),
                                 get_sampler_uniform(static_cast<unsigned>(i)),
                                 tex->handle);
            }
        }
    }
    bgfx::setVertexBuffer(0, &tvb, 0, total_verts);
    bgfx::setIndexBuffer(&tib, 0, num_indices);

    const char* arch = state_.select_shader_archetype();
    bgfx::ProgramHandle prog = get_program(arch);
    if (bgfx::isValid(prog)) { trace_submit(arch, prog); bgfx::submit(current_view_id_, prog); }
    else { trace_invalid_prog(arch); bgfx::discard(); }
    return D3D_OK;
}

HRESULT __stdcall D3D9Facade::ProcessVertices(UINT /*SrcStartIndex*/, UINT /*DestIndex*/,
                                               UINT /*VertexCount*/, IDirect3DVertexBuffer9* /*pDestBuffer*/,
                                               IDirect3DVertexDeclaration9* /*pVertexDecl*/,
                                               DWORD /*Flags*/) {
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Vertex declaration / FVF
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) {
    current_vdecl_ = pDecl;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) {
    if (ppDecl) *ppDecl = current_vdecl_;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetFVF(DWORD FVF) {
    fvf_ = FVF;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetFVF(DWORD* pFVF) {
    if (!pFVF) return D3DERR_INVALIDCALL;
    *pFVF = fvf_;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Vertex shader
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetVertexShader(IDirect3DVertexShader9* pShader) {
    current_vs_ = pShader;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetVertexShader(IDirect3DVertexShader9** ppShader) {
    if (ppShader) *ppShader = current_vs_;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < Vector4fCount && (StartRegister + i) < 128; ++i)
        std::memcpy(vs_const_f_[StartRegister + i], pConstantData + i * 4, sizeof(float) * 4);
    static int once = 0;
    if (once < 16) {
        ++once;
        if (FILE* f = draw_log()) {
            fprintf(f, "[SetVSConstF#%d] start=%u count=%u first=[%.3f %.3f %.3f %.3f]\n",
                    once, StartRegister, Vector4fCount,
                    pConstantData[0], pConstantData[1], pConstantData[2], pConstantData[3]);
            fflush(f);
        }
    }
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < Vector4fCount && (StartRegister + i) < 128; ++i)
        std::memcpy(pConstantData + i * 4, vs_const_f_[StartRegister + i], sizeof(float) * 4);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetVertexShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < Vector4iCount && (StartRegister + i) < 16; ++i)
        std::memcpy(vs_const_i_[StartRegister + i], pConstantData + i * 4, sizeof(int) * 4);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < Vector4iCount && (StartRegister + i) < 16; ++i)
        std::memcpy(pConstantData + i * 4, vs_const_i_[StartRegister + i], sizeof(int) * 4);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetVertexShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < BoolCount && (StartRegister + i) < 16; ++i)
        vs_const_b_[StartRegister + i] = pConstantData[i];
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < BoolCount && (StartRegister + i) < 16; ++i)
        pConstantData[i] = vs_const_b_[StartRegister + i];
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Stream source + indices  (T9: track VB/IB handle + stride for draw)
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetStreamSource(UINT StreamNumber,
                                               IDirect3DVertexBuffer9* pStreamData,
                                               UINT OffsetInBytes, UINT Stride) {
    if (StreamNumber < 8) {
        stream_vbo_[StreamNumber]    = pStreamData;
        stream_offset_[StreamNumber] = OffsetInBytes;
        stream_stride_[StreamNumber] = Stride;
    }
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetStreamSource(UINT StreamNumber,
                                               IDirect3DVertexBuffer9** ppStreamData,
                                               UINT* pOffsetInBytes, UINT* pStride) {
    if (StreamNumber >= 8) {
        if (ppStreamData)   *ppStreamData   = nullptr;
        if (pOffsetInBytes) *pOffsetInBytes = 0;
        if (pStride)        *pStride        = 0;
        return D3DERR_INVALIDCALL;
    }
    if (ppStreamData)   *ppStreamData   = stream_vbo_[StreamNumber];
    if (pOffsetInBytes) *pOffsetInBytes = stream_offset_[StreamNumber];
    if (pStride)        *pStride        = stream_stride_[StreamNumber];
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetStreamSourceFreq(UINT StreamNumber, UINT Setting) {
    if (StreamNumber < 8) stream_source_freq_[StreamNumber] = Setting;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting) {
    if (!pSetting) return D3DERR_INVALIDCALL;
    *pSetting = (StreamNumber < 8) ? stream_source_freq_[StreamNumber] : 0;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetIndices(IDirect3DIndexBuffer9* pIndexData) {
    ibo_ = pIndexData;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetIndices(IDirect3DIndexBuffer9** ppIndexData) {
    if (!ppIndexData) return D3DERR_INVALIDCALL;
    *ppIndexData = ibo_;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Pixel shader
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetPixelShader(IDirect3DPixelShader9* pShader) {
    current_ps_ = pShader;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetPixelShader(IDirect3DPixelShader9** ppShader) {
    if (ppShader) *ppShader = current_ps_;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetPixelShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < Vector4fCount && (StartRegister + i) < 32; ++i)
        std::memcpy(ps_const_f_[StartRegister + i], pConstantData + i * 4, sizeof(float) * 4);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < Vector4fCount && (StartRegister + i) < 32; ++i)
        std::memcpy(pConstantData + i * 4, ps_const_f_[StartRegister + i], sizeof(float) * 4);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetPixelShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < Vector4iCount && (StartRegister + i) < 16; ++i)
        std::memcpy(ps_const_i_[StartRegister + i], pConstantData + i * 4, sizeof(int) * 4);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < Vector4iCount && (StartRegister + i) < 16; ++i)
        std::memcpy(pConstantData + i * 4, ps_const_i_[StartRegister + i], sizeof(int) * 4);
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetPixelShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < BoolCount && (StartRegister + i) < 16; ++i)
        ps_const_b_[StartRegister + i] = pConstantData[i];
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < BoolCount && (StartRegister + i) < 16; ++i)
        pConstantData[i] = ps_const_b_[StartRegister + i];
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Patches (not used by Nival)
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::DrawRectPatch(UINT /*Handle*/, CONST float* /*pNumSegs*/, CONST D3DRECTPATCH_INFO* /*pRPI*/) { return D3D_OK; }
HRESULT __stdcall D3D9Facade::DrawTriPatch(UINT /*Handle*/, CONST float* /*pNumSegs*/, CONST D3DTRIPATCH_INFO* /*pTPI*/) { return D3D_OK; }
HRESULT __stdcall D3D9Facade::DeletePatch(UINT /*Handle*/) { return D3D_OK; }

} // namespace silent_storm::renderer
