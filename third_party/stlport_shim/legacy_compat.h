#pragma once
// Force-included into every Jan03 subproject. Provides shims for:
//   - MFC debug TRACE macros (Jan03 uses TRACE1..TRACE3 without including <afx.h>)
//   - other pre-modern artifacts surfaced during Phase 0 build

// silent-storm-port: Jan03's Main/ has its own `Time.h` which case-insensitively
// shadows the system `<time.h>`. <ctime>'s `using ::clock_t;` chain fails
// because Main/Time.h doesn't declare clock_t. Pull the C time header via
// its full relative path under the UCRT SDK to bypass the shadow.
extern "C" {
#  include <../ucrt/time.h>
}


// Some legacy translation units use `max`/`min` without including <algorithm>
// (assume MFC's windef.h provides them as macros — NOMINMAX disables those).
// Provide std::max/min in global scope unconditionally for legacy code.
#include <algorithm>
using std::max;
using std::min;

// MFC VERIFY — like ASSERT, but always evaluates its argument even in release.
#ifndef VERIFY
#  ifdef _DEBUG
#    define VERIFY(expr) do { if(!(expr)) __debugbreak(); } while(0)
#  else
#    define VERIFY(expr) ((void)(expr))
#  endif
#endif

// MFC TRACE macros — Jan03 referenced these from non-MFC subprojects
// (debug-only printf-style). Make them no-ops in release, OutputDebugString
// in debug.
#ifndef TRACE
#  ifdef _DEBUG
#    include <windows.h>
#    include <cstdio>
#    define TRACE(...)        do { char _ssbuf[1024]; sprintf(_ssbuf, __VA_ARGS__); OutputDebugStringA(_ssbuf); } while(0)
#    define TRACE0(s)         OutputDebugStringA(s)
#    define TRACE1(s, a)      TRACE(s, a)
#    define TRACE2(s, a, b)   TRACE(s, a, b)
#    define TRACE3(s, a, b, c) TRACE(s, a, b, c)
#  else
#    define TRACE(...)        ((void)0)
#    define TRACE0(s)         ((void)0)
#    define TRACE1(s, a)      ((void)0)
#    define TRACE2(s, a, b)   ((void)0)
#    define TRACE3(s, a, b, c) ((void)0)
#  endif
#endif
