#pragma once
// LifeStudioHeadAPI permanent stub — replaces the proprietary RAD/Generation7
// LifeStudio Head animation SDK that Jan03 sources reference but whose headers
// are absent from the open-source drop.
//
// Surface derived from usage in:
//   - upstream/Soft/Andy/Jan03/a5dll/Main/LSHead.h
//   - upstream/Soft/Andy/Jan03/a5dll/Main/LSHead.cpp
//   - upstream/Soft/Andy/Jan03/a5dll/Main/LSController.h
//   - upstream/Soft/Andy/Jan03/a5dll/Main/LSController.cpp
//
// Spec decision (2026-05-11): face animation feature is permanently dropped
// in v1. Every method here is a no-op or returns sentinel values that let
// callers proceed without dereferencing null. CHeadAnimator::Recalc treats
// SequenceTime()==0 as "no sequence playing" and skips the render path,
// which is exactly what we want.

namespace LifeStudioHeadAPI {

class IMMTree; // forward decl, defined fully below

class IAnimator {
public:
    static IAnimator* Create()                    { return new IAnimator(); }
    void Destroy()                                { delete this; }
    void Load(const char*, int)                   {}
    void RegisterMacroMuscle(void* /*root*/)      {}
    void ClearAllMacroMuscles()                   {}
    void ComputePhysics()                         {}
    void FillUnused(bool)                         {}
    void Process(float* /*out*/, int /*stride*/)  {}
protected:
    IAnimator() = default;
    ~IAnimator() = default;
};

class ISequencer {
public:
    static ISequencer* Create()                              { return new ISequencer(); }
    void Destroy()                                           { delete this; }
    void Load(const char*, int)                              {}
    int SequenceTime()                                       { return 0; }
    void RenderMacroMuscles(IAnimator*, int /*time*/)        {}
    void RegisterMMTree(IMMTree*)                            {}
protected:
    ISequencer() = default;
    ~ISequencer() = default;
};

class IMMTree {
public:
    static IMMTree* Create()                      { return new IMMTree(); }
    void Destroy()                                { delete this; }
    void Load(const char* /*path*/)               {}
    void* RootMacroMuscle()                       { return nullptr; }
protected:
    IMMTree() = default;
    ~IMMTree() = default;
};

} // namespace LifeStudioHeadAPI
