#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <d3d9.h>
#include <bgfx/bgfx.h>
#include "state_translator.h"

namespace silent_storm { namespace renderer {

class D3D9Facade : public IDirect3DDevice9 {
public:
    D3D9Facade();
    virtual ~D3D9Facade();

    // ----- IUnknown -----
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppv) override;
    ULONG   __stdcall AddRef() override;
    ULONG   __stdcall Release() override;

    // ----- IDirect3DDevice9 -----
    HRESULT __stdcall TestCooperativeLevel() override;
    UINT    __stdcall GetAvailableTextureMem() override;
    HRESULT __stdcall EvictManagedResources() override;
    HRESULT __stdcall GetDirect3D(IDirect3D9** ppD3D9) override;
    HRESULT __stdcall GetDeviceCaps(D3DCAPS9* pCaps) override;
    HRESULT __stdcall GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) override;
    HRESULT __stdcall GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters) override;
    HRESULT __stdcall SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) override;
    void    __stdcall SetCursorPosition(int X, int Y, DWORD Flags) override;
    BOOL    __stdcall ShowCursor(BOOL bShow) override;
    HRESULT __stdcall CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain) override;
    HRESULT __stdcall GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) override;
    UINT    __stdcall GetNumberOfSwapChains() override;
    HRESULT __stdcall Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) override;
    HRESULT __stdcall Present(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion) override;
    HRESULT __stdcall GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer) override;
    HRESULT __stdcall GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) override;
    HRESULT __stdcall SetDialogBoxMode(BOOL bEnableDialogs) override;
    void    __stdcall SetGammaRamp(UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp) override;
    void    __stdcall GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) override;
    HRESULT __stdcall CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) override;
    HRESULT __stdcall CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) override;
    HRESULT __stdcall CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) override;
    HRESULT __stdcall CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) override;
    HRESULT __stdcall CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) override;
    HRESULT __stdcall CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override;
    HRESULT __stdcall CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override;
    HRESULT __stdcall UpdateSurface(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, CONST POINT* pDestPoint) override;
    HRESULT __stdcall UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) override;
    HRESULT __stdcall GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) override;
    HRESULT __stdcall GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) override;
    HRESULT __stdcall StretchRect(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) override;
    HRESULT __stdcall ColorFill(IDirect3DSurface9* pSurface, CONST RECT* pRect, D3DCOLOR color) override;
    HRESULT __stdcall CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override;
    HRESULT __stdcall SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) override;
    HRESULT __stdcall GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) override;
    HRESULT __stdcall SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) override;
    HRESULT __stdcall GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) override;
    HRESULT __stdcall BeginScene() override;
    HRESULT __stdcall EndScene() override;
    HRESULT __stdcall Clear(DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override;
    HRESULT __stdcall SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) override;
    HRESULT __stdcall GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) override;
    HRESULT __stdcall MultiplyTransform(D3DTRANSFORMSTATETYPE, CONST D3DMATRIX*) override;
    HRESULT __stdcall SetViewport(CONST D3DVIEWPORT9* pViewport) override;
    HRESULT __stdcall GetViewport(D3DVIEWPORT9* pViewport) override;
    HRESULT __stdcall SetMaterial(CONST D3DMATERIAL9* pMaterial) override;
    HRESULT __stdcall GetMaterial(D3DMATERIAL9* pMaterial) override;
    HRESULT __stdcall SetLight(DWORD Index, CONST D3DLIGHT9*) override;
    HRESULT __stdcall GetLight(DWORD Index, D3DLIGHT9*) override;
    HRESULT __stdcall LightEnable(DWORD Index, BOOL Enable) override;
    HRESULT __stdcall GetLightEnable(DWORD Index, BOOL* pEnable) override;
    HRESULT __stdcall SetClipPlane(DWORD Index, CONST float* pPlane) override;
    HRESULT __stdcall GetClipPlane(DWORD Index, float* pPlane) override;
    HRESULT __stdcall SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) override;
    HRESULT __stdcall GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) override;
    HRESULT __stdcall CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) override;
    HRESULT __stdcall BeginStateBlock() override;
    HRESULT __stdcall EndStateBlock(IDirect3DStateBlock9** ppSB) override;
    HRESULT __stdcall SetClipStatus(CONST D3DCLIPSTATUS9* pClipStatus) override;
    HRESULT __stdcall GetClipStatus(D3DCLIPSTATUS9* pClipStatus) override;
    HRESULT __stdcall GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) override;
    HRESULT __stdcall SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) override;
    HRESULT __stdcall GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) override;
    HRESULT __stdcall SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override;
    HRESULT __stdcall GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) override;
    HRESULT __stdcall SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override;
    HRESULT __stdcall ValidateDevice(DWORD* pNumPasses) override;
    HRESULT __stdcall SetPaletteEntries(UINT PaletteNumber, CONST PALETTEENTRY* pEntries) override;
    HRESULT __stdcall GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) override;
    HRESULT __stdcall SetCurrentTexturePalette(UINT PaletteNumber) override;
    HRESULT __stdcall GetCurrentTexturePalette(UINT* PaletteNumber) override;
    HRESULT __stdcall SetScissorRect(CONST RECT* pRect) override;
    HRESULT __stdcall GetScissorRect(RECT* pRect) override;
    HRESULT __stdcall SetSoftwareVertexProcessing(BOOL bSoftware) override;
    BOOL    __stdcall GetSoftwareVertexProcessing() override;
    HRESULT __stdcall SetNPatchMode(float nSegments) override;
    float   __stdcall GetNPatchMode() override;
    HRESULT __stdcall DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override;
    HRESULT __stdcall DrawIndexedPrimitive(D3DPRIMITIVETYPE, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) override;
    HRESULT __stdcall DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) override;
    HRESULT __stdcall DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) override;
    HRESULT __stdcall ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) override;
    HRESULT __stdcall CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) override;
    HRESULT __stdcall SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) override;
    HRESULT __stdcall GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) override;
    HRESULT __stdcall SetFVF(DWORD FVF) override;
    HRESULT __stdcall GetFVF(DWORD* pFVF) override;
    HRESULT __stdcall CreateVertexShader(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader) override;
    HRESULT __stdcall SetVertexShader(IDirect3DVertexShader9* pShader) override;
    HRESULT __stdcall GetVertexShader(IDirect3DVertexShader9** ppShader) override;
    HRESULT __stdcall SetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) override;
    HRESULT __stdcall GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) override;
    HRESULT __stdcall SetVertexShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) override;
    HRESULT __stdcall GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) override;
    HRESULT __stdcall SetVertexShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount) override;
    HRESULT __stdcall GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) override;
    HRESULT __stdcall SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) override;
    HRESULT __stdcall GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* pOffsetInBytes, UINT* pStride) override;
    HRESULT __stdcall SetStreamSourceFreq(UINT StreamNumber, UINT Setting) override;
    HRESULT __stdcall GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting) override;
    HRESULT __stdcall SetIndices(IDirect3DIndexBuffer9* pIndexData) override;
    HRESULT __stdcall GetIndices(IDirect3DIndexBuffer9** ppIndexData) override;
    HRESULT __stdcall CreatePixelShader(CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader) override;
    HRESULT __stdcall SetPixelShader(IDirect3DPixelShader9* pShader) override;
    HRESULT __stdcall GetPixelShader(IDirect3DPixelShader9** ppShader) override;
    HRESULT __stdcall SetPixelShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) override;
    HRESULT __stdcall GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) override;
    HRESULT __stdcall SetPixelShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) override;
    HRESULT __stdcall GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) override;
    HRESULT __stdcall SetPixelShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount) override;
    HRESULT __stdcall GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) override;
    HRESULT __stdcall DrawRectPatch(UINT Handle, CONST float* pNumSegs, CONST D3DRECTPATCH_INFO* pRectPatchInfo) override;
    HRESULT __stdcall DrawTriPatch(UINT Handle, CONST float* pNumSegs, CONST D3DTRIPATCH_INFO* pTriPatchInfo) override;
    HRESULT __stdcall DeletePatch(UINT Handle) override;
    HRESULT __stdcall CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) override;

private:
    ULONG       ref_count_ = 1;
    DeviceState state_;

    // Saved viewport
    D3DVIEWPORT9 viewport_{};

    // Saved material
    D3DMATERIAL9 material_{};

    // Saved sampler state  [sampler][type]
    DWORD sampler_state_[16][14] = {};

    // Saved clip status
    D3DCLIPSTATUS9 clip_status_{};

    // Saved scissor rect
    RECT scissor_rect_{};

    // Saved FVF
    DWORD fvf_ = 0;

    // Saved current texture palette
    UINT current_palette_ = 0;

    // Saved stream source freq
    UINT stream_source_freq_[8] = {};

    // Saved SetVertexShaderConstant storage (128 * 4 floats = 2KB)
    float vs_const_f_[128][4] = {};
    int   vs_const_i_[16][4]  = {};
    BOOL  vs_const_b_[16]     = {};

    // Saved SetPixelShaderConstant storage
    float ps_const_f_[32][4] = {};
    int   ps_const_i_[16][4] = {};
    BOOL  ps_const_b_[16]    = {};

    // Phase 1 Task 9 — current draw stream state
    IDirect3DVertexBuffer9* stream_vbo_[8]   = {};
    UINT  stream_offset_[8]                  = {};
    UINT  stream_stride_[8]                  = {};
    IDirect3DIndexBuffer9*  ibo_             = nullptr;

    // Current bgfx view ID (0 by default). SetRenderTarget bumps this.
    uint16_t current_view_id_ = 0;

    bool scene_active_ = false;
};

// Single global facade instance — the "device" Nival's code receives.
IDirect3DDevice9* facade_instance();

}} // namespace silent_storm::renderer
