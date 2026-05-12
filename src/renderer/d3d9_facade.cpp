// d3d9_facade.cpp — IDirect3DDevice9 facade scaffold
// Phase 1 Task 6: all ~90 method stubs.
// Draw calls, resource creation and state recording implemented as
// documented below; real bgfx submission wired in Task 9.

// WIN32_LEAN_AND_MEAN and NOMINMAX are already set by CMake command-line;
// d3d9_facade.h re-declares them for safety but they're already defined.
#include "d3d9_facade.h"
#include "bgfx_init.h"
#include <bgfx/bgfx.h>
#include <cstring>
#include <cstdint>

namespace silent_storm::renderer {

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
namespace {
D3D9Facade* g_instance = nullptr;
}

IDirect3DDevice9* facade_instance() {
    if (!g_instance)
        g_instance = new D3D9Facade();
    return g_instance;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
D3D9Facade::D3D9Facade()  = default;
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
// Reset — keeps the facade alive; Nival calls this on lost device
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::Reset(D3DPRESENT_PARAMETERS* pPP) {
    if (!pPP) return D3DERR_INVALIDCALL;
    // In Task 9 we'll call bgfx::reset() here. For now, accept the call.
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Present — forward to bgfx frame
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::Present(CONST RECT* /*pSourceRect*/, CONST RECT* /*pDestRect*/,
                                       HWND /*hDestWindowOverride*/, CONST RGNDATA* /*pDirtyRegion*/) {
    silent_storm::renderer::end_frame();
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Back buffer / raster status
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::GetBackBuffer(UINT /*iSwapChain*/, UINT /*iBackBuffer*/,
                                              D3DBACKBUFFER_TYPE /*Type*/, IDirect3DSurface9** ppBB) {
    if (ppBB) *ppBB = nullptr;
    return E_NOTIMPL;
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
// Resource creation — return E_NOTIMPL (wired in Task 9)
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::CreateTexture(UINT /*Width*/, UINT /*Height*/, UINT /*Levels*/,
                                              DWORD /*Usage*/, D3DFORMAT /*Format*/, D3DPOOL /*Pool*/,
                                              IDirect3DTexture9** ppTexture, HANDLE* /*pSharedHandle*/) {
    if (ppTexture) *ppTexture = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreateVolumeTexture(UINT /*Width*/, UINT /*Height*/, UINT /*Depth*/,
                                                    UINT /*Levels*/, DWORD /*Usage*/, D3DFORMAT /*Format*/,
                                                    D3DPOOL /*Pool*/, IDirect3DVolumeTexture9** ppVT,
                                                    HANDLE* /*pSharedHandle*/) {
    if (ppVT) *ppVT = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreateCubeTexture(UINT /*EdgeLength*/, UINT /*Levels*/, DWORD /*Usage*/,
                                                  D3DFORMAT /*Format*/, D3DPOOL /*Pool*/,
                                                  IDirect3DCubeTexture9** ppCT, HANDLE* /*pSharedHandle*/) {
    if (ppCT) *ppCT = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreateVertexBuffer(UINT /*Length*/, DWORD /*Usage*/, DWORD /*FVF*/,
                                                   D3DPOOL /*Pool*/, IDirect3DVertexBuffer9** ppVB,
                                                   HANDLE* /*pSharedHandle*/) {
    if (ppVB) *ppVB = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreateIndexBuffer(UINT /*Length*/, DWORD /*Usage*/, D3DFORMAT /*Format*/,
                                                  D3DPOOL /*Pool*/, IDirect3DIndexBuffer9** ppIB,
                                                  HANDLE* /*pSharedHandle*/) {
    if (ppIB) *ppIB = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreateRenderTarget(UINT /*Width*/, UINT /*Height*/, D3DFORMAT /*Format*/,
                                                   D3DMULTISAMPLE_TYPE /*MultiSample*/, DWORD /*MultisampleQuality*/,
                                                   BOOL /*Lockable*/, IDirect3DSurface9** ppSurface,
                                                   HANDLE* /*pSharedHandle*/) {
    if (ppSurface) *ppSurface = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreateDepthStencilSurface(UINT /*Width*/, UINT /*Height*/, D3DFORMAT /*Format*/,
                                                          D3DMULTISAMPLE_TYPE /*MultiSample*/, DWORD /*MultisampleQuality*/,
                                                          BOOL /*Discard*/, IDirect3DSurface9** ppSurface,
                                                          HANDLE* /*pSharedHandle*/) {
    if (ppSurface) *ppSurface = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* /*pVE*/,
                                                        IDirect3DVertexDeclaration9** ppDecl) {
    if (ppDecl) *ppDecl = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreateVertexShader(CONST DWORD* /*pFunction*/,
                                                   IDirect3DVertexShader9** ppShader) {
    if (ppShader) *ppShader = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreatePixelShader(CONST DWORD* /*pFunction*/,
                                                  IDirect3DPixelShader9** ppShader) {
    if (ppShader) *ppShader = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreateStateBlock(D3DSTATEBLOCKTYPE /*Type*/,
                                                IDirect3DStateBlock9** ppSB) {
    if (ppSB) *ppSB = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreateOffscreenPlainSurface(UINT /*Width*/, UINT /*Height*/,
                                                            D3DFORMAT /*Format*/, D3DPOOL /*Pool*/,
                                                            IDirect3DSurface9** ppSurface,
                                                            HANDLE* /*pSharedHandle*/) {
    if (ppSurface) *ppSurface = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::CreateQuery(D3DQUERYTYPE /*Type*/, IDirect3DQuery9** ppQuery) {
    if (ppQuery) *ppQuery = nullptr;
    return E_NOTIMPL;
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
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetRenderTarget(DWORD /*RenderTargetIndex*/, IDirect3DSurface9** ppRT) {
    if (ppRT) *ppRT = nullptr;
    return E_NOTIMPL;
}
HRESULT __stdcall D3D9Facade::SetDepthStencilSurface(IDirect3DSurface9* /*pNewZStencil*/) {
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetDepthStencilSurface(IDirect3DSurface9** ppZStencil) {
    if (ppZStencil) *ppZStencil = nullptr;
    return E_NOTIMPL;
}

// ---------------------------------------------------------------------------
// Scene
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::BeginScene() { return D3D_OK; }
HRESULT __stdcall D3D9Facade::EndScene()   { return D3D_OK; }

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

    bgfx::setViewClear(0, bgfx_flags, rgba, Z, static_cast<uint8_t>(Stencil));
    bgfx::touch(0);
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Transforms
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) {
    if (!pMatrix) return D3DERR_INVALIDCALL;
    DWORD s = static_cast<DWORD>(State);
    if (s == 256 /*D3DTS_WORLD*/) { state_.world      = *pMatrix; }
    else if (State == D3DTS_VIEW)  { state_.view       = *pMatrix; }
    else if (State == D3DTS_PROJECTION) { state_.projection = *pMatrix; }
    // bone matrices (D3DTS_WORLDMATRIX(1..n)) stored later in T9
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
// Draw calls — no-op for Phase 1; Task 9 wires bgfx::submit
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::DrawPrimitive(D3DPRIMITIVETYPE /*PrimitiveType*/,
                                             UINT /*StartVertex*/, UINT /*PrimitiveCount*/) {
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::DrawIndexedPrimitive(D3DPRIMITIVETYPE /*PrimitiveType*/,
                                                    INT /*BaseVertexIndex*/, UINT /*MinVertexIndex*/,
                                                    UINT /*NumVertices*/, UINT /*startIndex*/,
                                                    UINT /*primCount*/) {
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::DrawPrimitiveUP(D3DPRIMITIVETYPE /*PrimitiveType*/,
                                               UINT /*PrimitiveCount*/,
                                               CONST void* /*pVertexStreamZeroData*/,
                                               UINT /*VertexStreamZeroStride*/) {
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE /*PrimitiveType*/,
                                                      UINT /*MinVertexIndex*/, UINT /*NumVertices*/,
                                                      UINT /*PrimitiveCount*/,
                                                      CONST void* /*pIndexData*/,
                                                      D3DFORMAT /*IndexDataFormat*/,
                                                      CONST void* /*pVertexStreamZeroData*/,
                                                      UINT /*VertexStreamZeroStride*/) {
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
HRESULT __stdcall D3D9Facade::SetVertexDeclaration(IDirect3DVertexDeclaration9* /*pDecl*/) { return D3D_OK; }
HRESULT __stdcall D3D9Facade::GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) {
    if (ppDecl) *ppDecl = nullptr;
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
HRESULT __stdcall D3D9Facade::SetVertexShader(IDirect3DVertexShader9* /*pShader*/) { return D3D_OK; }
HRESULT __stdcall D3D9Facade::GetVertexShader(IDirect3DVertexShader9** ppShader) {
    if (ppShader) *ppShader = nullptr;
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::SetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {
    if (!pConstantData) return D3DERR_INVALIDCALL;
    for (UINT i = 0; i < Vector4fCount && (StartRegister + i) < 128; ++i)
        std::memcpy(vs_const_f_[StartRegister + i], pConstantData + i * 4, sizeof(float) * 4);
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
// Stream source + indices
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetStreamSource(UINT /*StreamNumber*/,
                                               IDirect3DVertexBuffer9* /*pStreamData*/,
                                               UINT /*OffsetInBytes*/, UINT /*Stride*/) {
    return D3D_OK;
}
HRESULT __stdcall D3D9Facade::GetStreamSource(UINT /*StreamNumber*/,
                                               IDirect3DVertexBuffer9** ppStreamData,
                                               UINT* pOffsetInBytes, UINT* pStride) {
    if (ppStreamData)   *ppStreamData   = nullptr;
    if (pOffsetInBytes) *pOffsetInBytes = 0;
    if (pStride)        *pStride        = 0;
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
HRESULT __stdcall D3D9Facade::SetIndices(IDirect3DIndexBuffer9* /*pIndexData*/) { return D3D_OK; }
HRESULT __stdcall D3D9Facade::GetIndices(IDirect3DIndexBuffer9** ppIndexData) {
    if (ppIndexData) *ppIndexData = nullptr;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// Pixel shader
// ---------------------------------------------------------------------------
HRESULT __stdcall D3D9Facade::SetPixelShader(IDirect3DPixelShader9* /*pShader*/) { return D3D_OK; }
HRESULT __stdcall D3D9Facade::GetPixelShader(IDirect3DPixelShader9** ppShader) {
    if (ppShader) *ppShader = nullptr;
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
