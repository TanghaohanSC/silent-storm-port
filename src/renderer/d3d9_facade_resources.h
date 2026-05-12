// d3d9_facade_resources.h
//
// Phase 1 Task 9 — minimal IDirect3D*9 wrapper classes that route data
// uploads through bgfx.
//
// Lifetime model: every wrapper starts with ref_count = 1 (the freshly
// returned-from-Create reference). AddRef/Release follow COM convention.
//
// Upload model: Lock/Unlock returns a CPU-side staging buffer; on Unlock the
// data is pushed into bgfx via updateTexture2D / updateDynamicVertexBuffer /
// createDynamicIndexBuffer (which is then kept around for subsequent draws).
#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <d3d9.h>
#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>

namespace silent_storm::renderer {

// ---------------------------------------------------------------------------
// FacadeTexture — wraps a bgfx::TextureHandle
// ---------------------------------------------------------------------------
class FacadeTexture final : public IDirect3DTexture9 {
public:
    FacadeTexture(UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool);
    ~FacadeTexture();

    // ----- IUnknown -----
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppv) override;
    ULONG   __stdcall AddRef() override;
    ULONG   __stdcall Release() override;

    // ----- IDirect3DResource9 -----
    HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) override;
    HRESULT __stdcall SetPrivateData(REFGUID, const void*, DWORD, DWORD) override { return D3D_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID, void*, DWORD*) override { return D3DERR_NOTFOUND; }
    HRESULT __stdcall FreePrivateData(REFGUID) override { return D3D_OK; }
    DWORD   __stdcall SetPriority(DWORD p) override { priority_ = p; return p; }
    DWORD   __stdcall GetPriority() override { return priority_; }
    void    __stdcall PreLoad() override {}
    D3DRESOURCETYPE __stdcall GetType() override { return D3DRTYPE_TEXTURE; }

    // ----- IDirect3DBaseTexture9 -----
    DWORD   __stdcall SetLOD(DWORD lod) override { lod_ = lod; return lod; }
    DWORD   __stdcall GetLOD() override { return lod_; }
    DWORD   __stdcall GetLevelCount() override { return levels_; }
    HRESULT __stdcall SetAutoGenFilterType(D3DTEXTUREFILTERTYPE f) override { autogen_filter_ = f; return D3D_OK; }
    D3DTEXTUREFILTERTYPE __stdcall GetAutoGenFilterType() override { return autogen_filter_; }
    void    __stdcall GenerateMipSubLevels() override {}

    // ----- IDirect3DTexture9 -----
    HRESULT __stdcall GetLevelDesc(UINT level, D3DSURFACE_DESC* pDesc) override;
    HRESULT __stdcall GetSurfaceLevel(UINT level, IDirect3DSurface9** ppSurfaceLevel) override;
    HRESULT __stdcall LockRect(UINT level, D3DLOCKED_RECT* pLockedRect,
                                const RECT* pRect, DWORD flags) override;
    HRESULT __stdcall UnlockRect(UINT level) override;
    HRESULT __stdcall AddDirtyRect(const RECT* pDirtyRect) override { return D3D_OK; }

    bgfx::TextureHandle handle{BGFX_INVALID_HANDLE};
    UINT      width = 0, height = 0, levels_ = 1;
    DWORD     usage_ = 0;
    D3DFORMAT format = D3DFMT_UNKNOWN;
    D3DPOOL   pool_  = D3DPOOL_DEFAULT;

private:
    ULONG     ref_count_ = 1;
    DWORD     priority_  = 0;
    DWORD     lod_       = 0;
    D3DTEXTUREFILTERTYPE autogen_filter_ = D3DTEXF_LINEAR;
    // CPU staging buffer per mip level. Sized lazily on first Lock.
    std::vector<std::vector<uint8_t>> level_staging_;
    bool      locked_ = false;
    UINT      locked_level_ = 0;
    uint32_t  pitch_bytes_ = 0;
};

// ---------------------------------------------------------------------------
// FacadeVertexBuffer
// ---------------------------------------------------------------------------
class FacadeVertexBuffer final : public IDirect3DVertexBuffer9 {
public:
    FacadeVertexBuffer(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool);
    ~FacadeVertexBuffer();

    HRESULT __stdcall QueryInterface(REFIID riid, void** ppv) override;
    ULONG   __stdcall AddRef() override;
    ULONG   __stdcall Release() override;
    HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) override;
    HRESULT __stdcall SetPrivateData(REFGUID, const void*, DWORD, DWORD) override { return D3D_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID, void*, DWORD*) override { return D3DERR_NOTFOUND; }
    HRESULT __stdcall FreePrivateData(REFGUID) override { return D3D_OK; }
    DWORD   __stdcall SetPriority(DWORD p) override { priority_ = p; return p; }
    DWORD   __stdcall GetPriority() override { return priority_; }
    void    __stdcall PreLoad() override {}
    D3DRESOURCETYPE __stdcall GetType() override { return D3DRTYPE_VERTEXBUFFER; }
    HRESULT __stdcall Lock(UINT offset, UINT size, void** ppbData, DWORD flags) override;
    HRESULT __stdcall Unlock() override;
    HRESULT __stdcall GetDesc(D3DVERTEXBUFFER_DESC* pDesc) override;

    bgfx::DynamicVertexBufferHandle handle{BGFX_INVALID_HANDLE};
    UINT      length = 0;
    DWORD     fvf    = 0;
    DWORD     usage_ = 0;
    D3DPOOL   pool_  = D3DPOOL_DEFAULT;

private:
    ULONG     ref_count_ = 1;
    DWORD     priority_  = 0;
    std::vector<uint8_t> cpu_buf_;
    bool      locked_ = false;
    UINT      lock_offset_ = 0;
    UINT      lock_size_   = 0;
};

// ---------------------------------------------------------------------------
// FacadeIndexBuffer
// ---------------------------------------------------------------------------
class FacadeIndexBuffer final : public IDirect3DIndexBuffer9 {
public:
    FacadeIndexBuffer(UINT length, DWORD usage, D3DFORMAT fmt, D3DPOOL pool);
    ~FacadeIndexBuffer();

    HRESULT __stdcall QueryInterface(REFIID riid, void** ppv) override;
    ULONG   __stdcall AddRef() override;
    ULONG   __stdcall Release() override;
    HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) override;
    HRESULT __stdcall SetPrivateData(REFGUID, const void*, DWORD, DWORD) override { return D3D_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID, void*, DWORD*) override { return D3DERR_NOTFOUND; }
    HRESULT __stdcall FreePrivateData(REFGUID) override { return D3D_OK; }
    DWORD   __stdcall SetPriority(DWORD p) override { priority_ = p; return p; }
    DWORD   __stdcall GetPriority() override { return priority_; }
    void    __stdcall PreLoad() override {}
    D3DRESOURCETYPE __stdcall GetType() override { return D3DRTYPE_INDEXBUFFER; }
    HRESULT __stdcall Lock(UINT offset, UINT size, void** ppbData, DWORD flags) override;
    HRESULT __stdcall Unlock() override;
    HRESULT __stdcall GetDesc(D3DINDEXBUFFER_DESC* pDesc) override;

    bgfx::DynamicIndexBufferHandle handle{BGFX_INVALID_HANDLE};
    UINT      length = 0;
    D3DFORMAT format = D3DFMT_INDEX16;
    DWORD     usage_ = 0;
    D3DPOOL   pool_  = D3DPOOL_DEFAULT;

private:
    ULONG     ref_count_ = 1;
    DWORD     priority_  = 0;
    std::vector<uint8_t> cpu_buf_;
    bool      locked_ = false;
    UINT      lock_offset_ = 0;
    UINT      lock_size_   = 0;
};

// ---------------------------------------------------------------------------
// FacadeCubeTexture — minimal IDirect3DCubeTexture9 wrapper.  Cube textures are
// allocated by Nival for the cube-lighting cache (gfx_cl) and as render-target
// cubemaps. Phase 1.5: we return CPU-backed scratch surfaces from
// GetCubeMapSurface / LockRect so callers' ASSERT(D3D_OK) succeeds; no bgfx
// upload happens yet — Phase 2 task.
// ---------------------------------------------------------------------------
class FacadeCubeTexture final : public IDirect3DCubeTexture9 {
public:
    FacadeCubeTexture(UINT edge_length, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool);
    ~FacadeCubeTexture();

    // ----- IUnknown -----
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppv) override;
    ULONG   __stdcall AddRef() override;
    ULONG   __stdcall Release() override;

    // ----- IDirect3DResource9 -----
    HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) override;
    HRESULT __stdcall SetPrivateData(REFGUID, const void*, DWORD, DWORD) override { return D3D_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID, void*, DWORD*) override { return D3DERR_NOTFOUND; }
    HRESULT __stdcall FreePrivateData(REFGUID) override { return D3D_OK; }
    DWORD   __stdcall SetPriority(DWORD p) override { priority_ = p; return p; }
    DWORD   __stdcall GetPriority() override { return priority_; }
    void    __stdcall PreLoad() override {}
    D3DRESOURCETYPE __stdcall GetType() override { return D3DRTYPE_CUBETEXTURE; }

    // ----- IDirect3DBaseTexture9 -----
    DWORD   __stdcall SetLOD(DWORD lod) override { lod_ = lod; return lod; }
    DWORD   __stdcall GetLOD() override { return lod_; }
    DWORD   __stdcall GetLevelCount() override { return levels_; }
    HRESULT __stdcall SetAutoGenFilterType(D3DTEXTUREFILTERTYPE f) override { autogen_filter_ = f; return D3D_OK; }
    D3DTEXTUREFILTERTYPE __stdcall GetAutoGenFilterType() override { return autogen_filter_; }
    void    __stdcall GenerateMipSubLevels() override {}

    // ----- IDirect3DCubeTexture9 -----
    HRESULT __stdcall GetLevelDesc(UINT level, D3DSURFACE_DESC* pDesc) override;
    HRESULT __stdcall GetCubeMapSurface(D3DCUBEMAP_FACES FaceType, UINT Level, IDirect3DSurface9** ppCubeMapSurface) override;
    HRESULT __stdcall LockRect(D3DCUBEMAP_FACES FaceType, UINT Level, D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD flags) override;
    HRESULT __stdcall UnlockRect(D3DCUBEMAP_FACES FaceType, UINT Level) override;
    HRESULT __stdcall AddDirtyRect(D3DCUBEMAP_FACES FaceType, const RECT* pDirtyRect) override { return D3D_OK; }

    UINT      edge_length = 0;
    UINT      levels_     = 1;
    DWORD     usage_      = 0;
    D3DFORMAT format      = D3DFMT_UNKNOWN;
    D3DPOOL   pool_       = D3DPOOL_DEFAULT;

private:
    ULONG     ref_count_  = 1;
    DWORD     priority_   = 0;
    DWORD     lod_        = 0;
    D3DTEXTUREFILTERTYPE autogen_filter_ = D3DTEXTUREFILTERTYPE::D3DTEXF_LINEAR;
    // 6 faces × levels staging
    std::vector<std::vector<uint8_t>> face_staging_;
    bool      locked_ = false;
    uint32_t  pitch_bytes_ = 0;
    UINT      locked_face_  = 0;
    UINT      locked_level_ = 0;
};

// ---------------------------------------------------------------------------
// FacadeSurface — a passive wrapper used for back buffer / RT / depth /
// offscreen plain surfaces.  Doesn't own a bgfx handle in v1; mostly returns
// D3D_OK so callers see "the call succeeded" rather than E_NOTIMPL.
// ---------------------------------------------------------------------------
class FacadeSurface final : public IDirect3DSurface9 {
public:
    FacadeSurface(UINT w, UINT h, D3DFORMAT fmt);
    ~FacadeSurface();

    HRESULT __stdcall QueryInterface(REFIID riid, void** ppv) override;
    ULONG   __stdcall AddRef() override;
    ULONG   __stdcall Release() override;
    HRESULT __stdcall GetDevice(IDirect3DDevice9** ppDevice) override;
    HRESULT __stdcall SetPrivateData(REFGUID, const void*, DWORD, DWORD) override { return D3D_OK; }
    HRESULT __stdcall GetPrivateData(REFGUID, void*, DWORD*) override { return D3DERR_NOTFOUND; }
    HRESULT __stdcall FreePrivateData(REFGUID) override { return D3D_OK; }
    DWORD   __stdcall SetPriority(DWORD p) override { priority_ = p; return p; }
    DWORD   __stdcall GetPriority() override { return priority_; }
    void    __stdcall PreLoad() override {}
    D3DRESOURCETYPE __stdcall GetType() override { return D3DRTYPE_SURFACE; }
    HRESULT __stdcall GetContainer(REFIID, void** ppContainer) override;
    HRESULT __stdcall GetDesc(D3DSURFACE_DESC* pDesc) override;
    HRESULT __stdcall LockRect(D3DLOCKED_RECT* pLockedRect, const RECT* pRect, DWORD flags) override;
    HRESULT __stdcall UnlockRect() override;
    HRESULT __stdcall GetDC(HDC* phdc) override { *phdc = nullptr; return E_NOTIMPL; }
    HRESULT __stdcall ReleaseDC(HDC) override { return E_NOTIMPL; }

    UINT      width = 0, height = 0;
    D3DFORMAT format = D3DFMT_UNKNOWN;

private:
    ULONG     ref_count_ = 1;
    DWORD     priority_  = 0;
    std::vector<uint8_t> cpu_buf_;
    bool      locked_ = false;
};

} // namespace silent_storm::renderer
