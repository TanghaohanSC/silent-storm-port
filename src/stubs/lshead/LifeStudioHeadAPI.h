#pragma once
// LifeStudioHeadAPI — real DLL binding (r43, 2026-05-12).
//
// Generation7 LifeStudio Head Animation SDK. The proprietary header was never
// distributed publicly, but the runtime DLL ships with the Steam release of
// Silent Storm at:
//   C:\Program Files (x86)\Steam\steamapps\common\Silent Storm\LifeStudioHeadAPI.dll
//
// The DLL exposes 6 exports (all C++-mangled, MSVC __stdcall static factories):
//   ?Create@IAnimator@LifeStudioHeadAPI@@SGPAU12@XZ
//   ?Create@IEyeTracking@LifeStudioHeadAPI@@SGPAU12@XZ
//   ?Create@IMMTree@LifeStudioHeadAPI@@SGPAU12@XZ
//   ?Create@IOptions@LifeStudioHeadAPI@@SGPAU12@PAXII@Z
//   ?Create@ISequencer@LifeStudioHeadAPI@@SGPAU12@XZ
//   ?Create@ITransformer@LifeStudioHeadAPI@@SGPAU12@XZ
//
// All non-static methods are virtual functions dispatched through the object's
// vtable. The vtable layout below mirrors the call order observed in
// upstream/Soft/Andy/Jan03/a5dll/Main/LSHead.cpp and LSConverter.cpp. Since the
// original SDK header is unavailable we declare the interfaces as pure-virtual
// abstract classes with members in the order the Jan03 source uses them; the
// Generation7 header convention follows this layout.
//
// The Main game (silent_storm.exe) consumes IAnimator, ISequencer, IMMTree.
// IEyeTracking / IOptions / ITransformer exports are present in the DLL but
// not used by Main; we still expose minimal interface skeletons for symmetry.

// IMPORTANT: the DLL was built with the SDK's interfaces declared as `struct`
// (mangled as 'U12@'). MSVC name mangling distinguishes class ('V') vs struct
// ('U'), so the declarations below MUST stay `struct` for the link references
// to match the exports (?Create@IAnimator@LifeStudioHeadAPI@@SGPAU12@XZ).

namespace LifeStudioHeadAPI {

struct IMMTree;  // forward decl

// IAnimator — per-mesh blend-shape animator. One per face mesh segment.
struct IAnimator {
    static __declspec(dllimport) IAnimator* __stdcall Create();
    virtual bool Load(const char* pData, int nSize) = 0;
    virtual void RegisterMacroMuscle(void* pRootMacroMuscle) = 0;
    virtual void ClearAllMacroMuscles() = 0;
    virtual void ComputePhysics() = 0;
    virtual void FillUnused(bool bFlag) = 0;
    virtual void Process(float* pOutVertices, int nStride) = 0;
    virtual void Destroy() = 0;
};

// ISequencer — animation sequence player. Drives a registered IAnimator.
struct ISequencer {
    static __declspec(dllimport) ISequencer* __stdcall Create();
    virtual bool Load(const char* pData, int nSize) = 0;
    virtual int  SequenceTime() = 0;
    virtual void RenderMacroMuscles(IAnimator* pAnimator, int nTime) = 0;
    virtual void RegisterMMTree(IMMTree* pTree) = 0;
    virtual void Destroy() = 0;
};

// IMMTree — shared MacroMuscle tree (loaded once from tree.mma).
struct IMMTree {
    static __declspec(dllimport) IMMTree* __stdcall Create();
    virtual bool Load(const char* pszPath) = 0;
    virtual void* RootMacroMuscle() = 0;
    virtual void Destroy() = 0;
};

// --- Unused-by-Main interfaces; kept for export surface completeness ---

struct IEyeTracking {
    static __declspec(dllimport) IEyeTracking* __stdcall Create();
    virtual void Destroy() = 0;
};

struct IOptions {
    static __declspec(dllimport) IOptions* __stdcall Create(void* pCtx, unsigned int a, unsigned int b);
    virtual void Destroy() = 0;
};

struct ITransformer {
    static __declspec(dllimport) ITransformer* __stdcall Create();
    virtual void Destroy() = 0;
};

} // namespace LifeStudioHeadAPI
