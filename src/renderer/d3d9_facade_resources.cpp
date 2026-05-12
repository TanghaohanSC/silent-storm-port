// d3d9_facade_resources.cpp — Phase 1 Task 9
#include "d3d9_facade_resources.h"
#include <bgfx/bgfx.h>
#include <cstring>

namespace silent_storm::renderer {

namespace {

// Crude D3DFORMAT → bgfx::TextureFormat mapping for the formats Nival uses.
bgfx::TextureFormat::Enum map_format(D3DFORMAT f) {
    switch (f) {
        case D3DFMT_A8R8G8B8: return bgfx::TextureFormat::BGRA8;
        case D3DFMT_X8R8G8B8: return bgfx::TextureFormat::BGRA8;
        case D3DFMT_R5G6B5:   return bgfx::TextureFormat::R5G6B5;
        case D3DFMT_A1R5G5B5: return bgfx::TextureFormat::BGR5A1;
        case D3DFMT_A4R4G4B4: return bgfx::TextureFormat::BGRA4;
        case D3DFMT_DXT1:     return bgfx::TextureFormat::BC1;
        case D3DFMT_DXT3:     return bgfx::TextureFormat::BC2;
        case D3DFMT_DXT5:     return bgfx::TextureFormat::BC3;
        case D3DFMT_A8:       return bgfx::TextureFormat::A8;
        case D3DFMT_L8:       return bgfx::TextureFormat::R8;
        case D3DFMT_D16:      return bgfx::TextureFormat::D16;
        case D3DFMT_D24S8:    return bgfx::TextureFormat::D24S8;
        case D3DFMT_D32:      return bgfx::TextureFormat::D32;
        default:              return bgfx::TextureFormat::BGRA8;
    }
}

// Bytes per pixel for the formats above (approximate).
uint32_t bytes_per_pixel(D3DFORMAT f) {
    switch (f) {
        case D3DFMT_A8R8G8B8:
        case D3DFMT_X8R8G8B8: return 4;
        case D3DFMT_R5G6B5:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A4R4G4B4: return 2;
        case D3DFMT_A8:
        case D3DFMT_L8:       return 1;
        // Compressed: caller of this should special-case; we return 1 to avoid /0
        case D3DFMT_DXT1:
        case D3DFMT_DXT3:
        case D3DFMT_DXT5:     return 1;
        default:              return 4;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// FacadeTexture
// ---------------------------------------------------------------------------
FacadeTexture::FacadeTexture(UINT w, UINT h, UINT levels, DWORD usage, D3DFORMAT fmt, D3DPOOL pool)
    : width(w), height(h), levels_(levels ? levels : 1), usage_(usage), format(fmt), pool_(pool)
{
    level_staging_.resize(levels_);
}

FacadeTexture::~FacadeTexture() {
    if (bgfx::isValid(handle)) bgfx::destroy(handle);
}

HRESULT __stdcall FacadeTexture::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3DResource9 ||
        riid == IID_IDirect3DBaseTexture9 || riid == IID_IDirect3DTexture9) {
        *ppv = static_cast<IDirect3DTexture9*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

ULONG __stdcall FacadeTexture::AddRef()  { return ++ref_count_; }
ULONG __stdcall FacadeTexture::Release() {
    ULONG r = --ref_count_;
    if (r == 0) delete this;
    return r;
}
HRESULT __stdcall FacadeTexture::GetDevice(IDirect3DDevice9** ppDevice) {
    if (ppDevice) *ppDevice = nullptr;
    return D3DERR_INVALIDCALL;
}

HRESULT __stdcall FacadeTexture::GetLevelDesc(UINT level, D3DSURFACE_DESC* pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    std::memset(pDesc, 0, sizeof(*pDesc));
    pDesc->Format = format;
    pDesc->Type   = D3DRTYPE_SURFACE;
    pDesc->Usage  = usage_;
    pDesc->Pool   = pool_;
    pDesc->Width  = width  >> level;
    pDesc->Height = height >> level;
    if (pDesc->Width  == 0) pDesc->Width  = 1;
    if (pDesc->Height == 0) pDesc->Height = 1;
    return D3D_OK;
}

HRESULT __stdcall FacadeTexture::GetSurfaceLevel(UINT level, IDirect3DSurface9** ppSurfaceLevel) {
    if (!ppSurfaceLevel) return D3DERR_INVALIDCALL;
    // Return a minimal surface wrapper.  The returned surface is owned by the caller;
    // we don't track it as a child here (a future cleanup could associate it with the
    // texture and use it for Lock/Unlock proxy).
    UINT w = width  >> level;  if (w == 0) w = 1;
    UINT h = height >> level;  if (h == 0) h = 1;
    *ppSurfaceLevel = new FacadeSurface(w, h, format);
    return D3D_OK;
}

HRESULT __stdcall FacadeTexture::LockRect(UINT level, D3DLOCKED_RECT* pLockedRect,
                                          const RECT* /*pRect*/, DWORD /*flags*/) {
    if (!pLockedRect || level >= levels_) return D3DERR_INVALIDCALL;
    UINT lw = width  >> level; if (lw == 0) lw = 1;
    UINT lh = height >> level; if (lh == 0) lh = 1;
    uint32_t bpp = bytes_per_pixel(format);
    pitch_bytes_ = lw * bpp;
    auto& buf = level_staging_[level];
    if (buf.empty()) buf.resize(pitch_bytes_ * lh);
    pLockedRect->Pitch = static_cast<INT>(pitch_bytes_);
    pLockedRect->pBits = buf.data();
    locked_ = true;
    locked_level_ = level;
    return D3D_OK;
}

HRESULT __stdcall FacadeTexture::UnlockRect(UINT level) {
    if (!locked_ || level != locked_level_) return D3DERR_INVALIDCALL;
    locked_ = false;

    if (level >= levels_) return D3DERR_INVALIDCALL;
    auto& buf = level_staging_[level];
    if (buf.empty()) return D3D_OK;

    // Create the bgfx texture lazily on first unlock.
    if (!bgfx::isValid(handle)) {
        uint64_t bgfx_flags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE;
        handle = bgfx::createTexture2D(
            static_cast<uint16_t>(width),
            static_cast<uint16_t>(height),
            /*hasMips*/ levels_ > 1,
            /*numLayers*/ 1,
            map_format(format),
            bgfx_flags);
    }
    if (!bgfx::isValid(handle)) return E_FAIL;

    UINT lw = width  >> level; if (lw == 0) lw = 1;
    UINT lh = height >> level; if (lh == 0) lh = 1;
    const bgfx::Memory* mem = bgfx::copy(buf.data(), static_cast<uint32_t>(buf.size()));
    bgfx::updateTexture2D(
        handle,
        /*layer*/ 0,
        static_cast<uint8_t>(level),
        /*x*/0, /*y*/0,
        static_cast<uint16_t>(lw),
        static_cast<uint16_t>(lh),
        mem,
        static_cast<uint16_t>(pitch_bytes_));
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// FacadeVertexBuffer
// ---------------------------------------------------------------------------
FacadeVertexBuffer::FacadeVertexBuffer(UINT len, DWORD usage, DWORD f, D3DPOOL pool)
    : length(len), fvf(f), usage_(usage), pool_(pool)
{
    cpu_buf_.resize(len);
}

FacadeVertexBuffer::~FacadeVertexBuffer() {
    if (bgfx::isValid(handle)) bgfx::destroy(handle);
}

HRESULT __stdcall FacadeVertexBuffer::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3DResource9 ||
        riid == IID_IDirect3DVertexBuffer9) {
        *ppv = static_cast<IDirect3DVertexBuffer9*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}
ULONG __stdcall FacadeVertexBuffer::AddRef() { return ++ref_count_; }
ULONG __stdcall FacadeVertexBuffer::Release() {
    ULONG r = --ref_count_;
    if (r == 0) delete this;
    return r;
}
HRESULT __stdcall FacadeVertexBuffer::GetDevice(IDirect3DDevice9** ppDevice) {
    if (ppDevice) *ppDevice = nullptr;
    return D3DERR_INVALIDCALL;
}

HRESULT __stdcall FacadeVertexBuffer::Lock(UINT offset, UINT size, void** ppbData, DWORD /*flags*/) {
    if (!ppbData) return D3DERR_INVALIDCALL;
    if (size == 0) size = length - offset;
    if (offset + size > length) return D3DERR_INVALIDCALL;
    *ppbData = cpu_buf_.data() + offset;
    lock_offset_ = offset;
    lock_size_   = size;
    locked_      = true;
    return D3D_OK;
}

HRESULT __stdcall FacadeVertexBuffer::Unlock() {
    if (!locked_) return D3D_OK;
    locked_ = false;
    // Hold off creating a bgfx buffer until we know its layout (set lazily at
    // first draw via SetStreamSource->stride).  For now we just keep the CPU
    // copy; the facade reads from cpu_buf_ when binding.
    return D3D_OK;
}

HRESULT __stdcall FacadeVertexBuffer::GetDesc(D3DVERTEXBUFFER_DESC* pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    std::memset(pDesc, 0, sizeof(*pDesc));
    pDesc->Format = D3DFMT_VERTEXDATA;
    pDesc->Type   = D3DRTYPE_VERTEXBUFFER;
    pDesc->Usage  = usage_;
    pDesc->Pool   = pool_;
    pDesc->Size   = length;
    pDesc->FVF    = fvf;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// FacadeIndexBuffer
// ---------------------------------------------------------------------------
FacadeIndexBuffer::FacadeIndexBuffer(UINT len, DWORD usage, D3DFORMAT fmt, D3DPOOL pool)
    : length(len), format(fmt), usage_(usage), pool_(pool)
{
    cpu_buf_.resize(len);
}

FacadeIndexBuffer::~FacadeIndexBuffer() {
    if (bgfx::isValid(handle)) bgfx::destroy(handle);
}

HRESULT __stdcall FacadeIndexBuffer::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3DResource9 ||
        riid == IID_IDirect3DIndexBuffer9) {
        *ppv = static_cast<IDirect3DIndexBuffer9*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}
ULONG __stdcall FacadeIndexBuffer::AddRef() { return ++ref_count_; }
ULONG __stdcall FacadeIndexBuffer::Release() {
    ULONG r = --ref_count_;
    if (r == 0) delete this;
    return r;
}
HRESULT __stdcall FacadeIndexBuffer::GetDevice(IDirect3DDevice9** ppDevice) {
    if (ppDevice) *ppDevice = nullptr;
    return D3DERR_INVALIDCALL;
}

HRESULT __stdcall FacadeIndexBuffer::Lock(UINT offset, UINT size, void** ppbData, DWORD /*flags*/) {
    if (!ppbData) return D3DERR_INVALIDCALL;
    if (size == 0) size = length - offset;
    if (offset + size > length) return D3DERR_INVALIDCALL;
    *ppbData = cpu_buf_.data() + offset;
    lock_offset_ = offset;
    lock_size_   = size;
    locked_      = true;
    return D3D_OK;
}

HRESULT __stdcall FacadeIndexBuffer::Unlock() {
    if (!locked_) return D3D_OK;
    locked_ = false;

    // Upload to a bgfx dynamic index buffer.  Create lazily on first unlock.
    if (!bgfx::isValid(handle)) {
        uint16_t flags = (format == D3DFMT_INDEX32) ? BGFX_BUFFER_INDEX32 : 0;
        const bgfx::Memory* mem = bgfx::copy(cpu_buf_.data(), static_cast<uint32_t>(cpu_buf_.size()));
        handle = bgfx::createDynamicIndexBuffer(mem, flags);
    } else {
        const bgfx::Memory* mem = bgfx::copy(cpu_buf_.data() + lock_offset_,
                                             static_cast<uint32_t>(lock_size_));
        bgfx::update(handle, lock_offset_ / ((format == D3DFMT_INDEX32) ? 4 : 2), mem);
    }
    return D3D_OK;
}

HRESULT __stdcall FacadeIndexBuffer::GetDesc(D3DINDEXBUFFER_DESC* pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    std::memset(pDesc, 0, sizeof(*pDesc));
    pDesc->Format = format;
    pDesc->Type   = D3DRTYPE_INDEXBUFFER;
    pDesc->Usage  = usage_;
    pDesc->Pool   = pool_;
    pDesc->Size   = length;
    return D3D_OK;
}

// ---------------------------------------------------------------------------
// FacadeSurface
// ---------------------------------------------------------------------------
FacadeSurface::FacadeSurface(UINT w, UINT h, D3DFORMAT fmt)
    : width(w), height(h), format(fmt) {}
FacadeSurface::~FacadeSurface() = default;

HRESULT __stdcall FacadeSurface::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    if (riid == IID_IUnknown || riid == IID_IDirect3DResource9 ||
        riid == IID_IDirect3DSurface9) {
        *ppv = static_cast<IDirect3DSurface9*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}
ULONG __stdcall FacadeSurface::AddRef() { return ++ref_count_; }
ULONG __stdcall FacadeSurface::Release() {
    ULONG r = --ref_count_;
    if (r == 0) delete this;
    return r;
}
HRESULT __stdcall FacadeSurface::GetDevice(IDirect3DDevice9** ppDevice) {
    if (ppDevice) *ppDevice = nullptr;
    return D3DERR_INVALIDCALL;
}
HRESULT __stdcall FacadeSurface::GetContainer(REFIID, void** ppContainer) {
    if (ppContainer) *ppContainer = nullptr;
    return E_NOINTERFACE;
}
HRESULT __stdcall FacadeSurface::GetDesc(D3DSURFACE_DESC* pDesc) {
    if (!pDesc) return D3DERR_INVALIDCALL;
    std::memset(pDesc, 0, sizeof(*pDesc));
    pDesc->Format            = format;
    pDesc->Type              = D3DRTYPE_SURFACE;
    pDesc->Usage             = 0;
    pDesc->Pool              = D3DPOOL_DEFAULT;
    pDesc->Width             = width;
    pDesc->Height            = height;
    pDesc->MultiSampleType   = D3DMULTISAMPLE_NONE;
    pDesc->MultiSampleQuality = 0;
    return D3D_OK;
}
HRESULT __stdcall FacadeSurface::LockRect(D3DLOCKED_RECT* pLockedRect, const RECT* /*pRect*/, DWORD /*flags*/) {
    if (!pLockedRect) return D3DERR_INVALIDCALL;
    uint32_t bpp = bytes_per_pixel(format);
    uint32_t pitch = width * bpp;
    if (cpu_buf_.empty()) cpu_buf_.resize(pitch * height);
    pLockedRect->Pitch = static_cast<INT>(pitch);
    pLockedRect->pBits = cpu_buf_.data();
    locked_ = true;
    return D3D_OK;
}
HRESULT __stdcall FacadeSurface::UnlockRect() {
    locked_ = false;
    return D3D_OK;
}

} // namespace silent_storm::renderer
