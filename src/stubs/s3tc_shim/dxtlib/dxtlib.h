#pragma once
// S3TC / DXT (texture compression) SDK shim.
//
// Nival's Image subproject uses the proprietary S3TC compression library
// (formerly from S3, then NVidia DXT tools). The headers are not in the
// open-source drop. Phase 0 stub: accept inputs, no-op the compression,
// return success. Phase 1 will route texture loading through bgfx/bimg
// which has its own DXT codec — this stub becomes dead code at that point.
//
// API surface deduced from Nival's Image/*.cpp usage patterns. If Phase 0
// build still misses symbols, extend here.

#ifdef __cplusplus
extern "C" {
#endif

// Common DXT format identifiers (return values + arguments)
#define S3TC_DXT1   1
#define S3TC_DXT3   3
#define S3TC_DXT5   5

// Compress / decompress entry points — silent no-op stubs.
inline int dxtCompressDXT1(const unsigned char* /*rgba*/, int /*w*/, int /*h*/, unsigned char* /*out*/) { return 0; }
inline int dxtCompressDXT3(const unsigned char* /*rgba*/, int /*w*/, int /*h*/, unsigned char* /*out*/) { return 0; }
inline int dxtCompressDXT5(const unsigned char* /*rgba*/, int /*w*/, int /*h*/, unsigned char* /*out*/) { return 0; }

#ifdef __cplusplus
}
#endif
