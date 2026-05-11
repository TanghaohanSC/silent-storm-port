#pragma once
// stlport shim — Jan03 source includes <stl/_config.h> via every StdAfx.h.
// Real stlport's _config.h paths into "../include/stddef.h" (VS .NET 2003
// layout) which doesn't exist on modern MSVC. We replace with an empty
// stub; modern MSVC's std:: provides everything the original code expected
// from stlport.

// Selected stlport macros that downstream code may reference unconditionally:
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
