// silent-storm-port: REGISTER_SAVELOAD_CLASS for CDBTableDataStorage.
// The class definition itself lives in db_table_storage.h so that BasicDB.h
// (patched in r15) can declare a CObj<CDBTableDataStorage> member.
//
// See port/docs/patches/p1_5_r13_wire_format_complete.md for the byte-level
// reverse-engineering of the 8-field wire format.

#include <windows.h>
#include <cstdio>
#ifdef _DEBUG
#  define ASSERT(a) if(!(a)){__debugbreak();}
#else
#  define ASSERT(a) ((void)0)
#endif
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
#include "db_table_storage.h"

REGISTER_SAVELOAD_CLASS(0xA1843130, CDBTableDataStorage)
