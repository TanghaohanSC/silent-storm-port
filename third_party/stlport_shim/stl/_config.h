#pragma once
// stlport shim — Jan03 source includes <stl/_config.h> via every StdAfx.h.
// Real stlport vc7 from upstream/Soft/SDK/stlport uses VS .NET 2003 relative
// include paths that break modern MSVC; we shim it instead and let modern
// std:: provide all the actual functionality.
//
// stlport's transitive includes pulled in typeinfo and similar standard
// headers automatically. The shim re-establishes that — anything Jan03
// code expects "for free" from including <stl/_config.h> goes here.

#include <typeinfo>      // Basic2.cpp etc reference type_info
#include <cstddef>       // size_t, ptrdiff_t

// Selected stlport-compat macros downstream code may reference unconditionally.
#ifndef _STLP_STD_NS
#define _STLP_STD_NS std
#endif
#ifndef _STLP_USE_NAMESPACES
#define _STLP_USE_NAMESPACES 1
#endif
#ifndef _STLP_BEGIN_NAMESPACE
#define _STLP_BEGIN_NAMESPACE namespace std {
#endif
#ifndef _STLP_END_NAMESPACE
#define _STLP_END_NAMESPACE }
#endif
