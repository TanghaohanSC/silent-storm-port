// s3tc.h — Phase 0 stub for the S3 Texture Compression SDK.
// The real library (s3_truecolour_dxtc) is proprietary; we stub out the
// encode functions so Image/ImagePack.cpp can compile.  The DXT path is
// dead code for Phase 0 (no real game assets), so returning 0 / no-ops is
// safe for the link-test goal.
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// -----------------------------------------------------------------------
// Minimal DDSURFACEDESC / DDPIXELFORMAT stubs to avoid pulling in ddraw.h
// which conflicts with <windows.h> (HMONITOR redefinition).
// -----------------------------------------------------------------------
#ifndef DDSD_WIDTH
#define DDSD_WIDTH            0x00000004
#define DDSD_HEIGHT           0x00000002
#define DDSD_LINEARSIZE       0x00080000
#define DDSD_PIXELFORMAT      0x00001000
#define DDSD_LPSURFACE        0x00000800
#define DDPF_ALPHAPIXELS      0x00000001
#define DDPF_RGB              0x00000040

typedef struct _DDPIXELFORMAT {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwFourCC;
    DWORD dwRGBBitCount;
    DWORD dwRBitMask;
    DWORD dwGBitMask;
    DWORD dwBBitMask;
    DWORD dwRGBAlphaBitMask;
} DDPIXELFORMAT;

typedef struct _DDSURFACEDESC {
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwHeight;
    DWORD           dwWidth;
    LONG            lPitch;
    DWORD           dwBackBufferCount;
    DWORD           dwMipMapCount;
    DWORD           dwAlphaBitDepth;
    DWORD           dwReserved;
    LPVOID          lpSurface;
    DWORD           ddckCKDestOverlayLow;
    DWORD           ddckCKDestOverlayHigh;
    DWORD           ddckCKDestBltLow;
    DWORD           ddckCKDestBltHigh;
    DWORD           ddckCKSrcOverlayLow;
    DWORD           ddckCKSrcOverlayHigh;
    DWORD           ddckCKSrcBltLow;
    DWORD           ddckCKSrcBltHigh;
    DDPIXELFORMAT   ddpfPixelFormat;
    DWORD           ddsCaps;
    DWORD           dwTextureStage;
} DDSURFACEDESC;
#endif  // DDSD_WIDTH

// -----------------------------------------------------------------------
// Encode-type flags (bitfield, values match S3 SDK 1.x)
// -----------------------------------------------------------------------
#define S3TC_ENCODE_RGB_COLOR_KEY         0x0001
#define S3TC_ENCODE_RGB_ALPHA_COMPARE     0x0002
#define S3TC_ENCODE_RGB_FULL              0x0004
#define S3TC_ENCODE_ALPHA_EXPLICIT        0x0100
#define S3TC_ENCODE_ALPHA_INTERPOLATED    0x0200

// -----------------------------------------------------------------------
// Stub API — all no-ops; Phase 0 just needs the link to succeed
// -----------------------------------------------------------------------
inline int  S3TCgetEncodeSize(const DDSURFACEDESC* /*pIn*/, DWORD /*dwEncodeType*/) { return 0; }
inline void S3TCsetAlphaReference(DWORD /*ref*/) {}
inline int  S3TCencode(const DDSURFACEDESC* /*pIn*/,  int /*nMipLevels*/,
                        DDSURFACEDESC*      /*pOut*/,  void* /*pDst*/,
                        DWORD               /*dwEncodeType*/,
                        const float*        /*pfWeights*/) { return 0; }
