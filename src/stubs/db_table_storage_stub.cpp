// silent-storm-port: minimal stub for `CDBTableDataStorage` (typeID 0xA1843130)
//
// The shipping Game.exe wraps each CDBTableBase's records hash_map in a
// heap-allocated CDBTableDataStorage CObj. The Jan03 open-source drop omits
// this class entirely, so 155 instances in game.db fail their factory lookup
// and skip (which then cascades into 328 BasicDB.h:102 asserts downstream).
//
// **What this stub does**: register typeID 0xA1843130 with a class that
//   inherits CObjectBase and has an empty operator&. CStructureSaver's
//   chunk-delimited wire format means the framework's FinishChunk() will
//   advance past the (unread) body bytes — so the 155 instances will at
//   least *instantiate* cleanly. They won't be functional (the wire data
//   never lands in CDBTableBase::records), but the missing-class log noise
//   disappears.
//
// **What this stub does NOT do**: read the 8 wire fields at offsets 0x0C
//   through 0x54 that the real CDBTableDataStorage::operator& reads. Doing
//   so requires reverse-engineering five sub-handler functions in MapEdit.exe
//   (0x44CB30, 0x44CC90, 0x44D810, 0x44D360, 0x44CEC0) — see
//   `port/docs/patches/p1_5_r11_class_identified.md` for the full RE blueprint.
//
// Once the wire format is fully recovered, CDBTableBase::operator& will need
// to be patched to delegate to a wrapped CObj<CDBTableDataStorage> instead of
// using its inline records hash_map.

#include <windows.h>
#include <cstdio>
#ifdef _DEBUG
#  define ASSERT(a) if(!(a)){__debugbreak();}
#else
#  define ASSERT(a) ((void)0)
#endif
// Mirror ado_stub.cpp: this TU is not inside the ADOImport DLL, so externA5
// (used by BasicChunk1.h to declare pSSClasses) needs a plain extern fallback.
#undef externA5
#define externA5 extern

#include <algorithm>
#include <list>
#include <string>
#include <vector>
#include <typeinfo>
#include <hash_map>
using namespace std;

#include "..\..\..\upstream\Soft\Andy\Jan03\a5dll\Misc\basic2.h"
#include "..\..\..\upstream\Soft\Andy\Jan03\a5dll\Misc\BasicFactory.h"
#include "..\..\..\upstream\Soft\Andy\Jan03\a5dll\FileIO\BasicChunk1.h"

class CDBTableDataStorage : public CObjectBase
{
    OBJECT_BASIC_METHODS(CDBTableDataStorage)
public:
    int operator&(CStructureSaver& /*f*/) { return 0; }
};

REGISTER_SAVELOAD_CLASS(0xA1843130, CDBTableDataStorage)
