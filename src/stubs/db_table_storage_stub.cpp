// silent-storm-port: full impl of `CDBTableDataStorage` (typeID 0xA1843130)
//
// r11 shipped an empty-operator& stub (silenced 155 missing-class log lines but
// records stayed empty — 328 BasicDB.h:102 cascade asserts remained).
//
// r14 (this file) upgrades to the full 7-field wire format reverse-engineered
// in r12/r13 via Ghidra. See `port/docs/patches/p1_5_r13_wire_format_complete.md`
// for the byte-level breakdown.
//
// Wire layout:
//   field 2 — vector<vector<int>>          hash bucket head chain index
//   field 3 — vector<vector<int>>          bucket chain next pointers
//   field 4 — vector<vector<wstring>>      wide-string columns
//   field 5 — vector<{string body, int id}>  records: (packed bytes, primary key)
//   field 6 — vector<string>               metadata blob column #1
//   field 7 — vector<string>               metadata blob column #2
//   field 8 — vector<string>               metadata blob column #3
//
// CStructureSaver auto-dispatches all of these via template specializations in
// BasicChunk1.h — no hand-rolled wire code needed; STL types alone produce the
// correct chunk format.
//
// IMPORTANT: This impl loads the data into the storage object, but does NOT
// yet wire it back into CDBTableBase::records. Records remain in their packed
// byte-blob form in m_records[]. A follow-up round will patch CDBTableBase to
// hold a CObj<CDBTableDataStorage> and implement lazy materialization in
// GetDBRecord(int).

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

// Field 5 entry — std::string body + int id (16 bytes on the wire shape,
// matching FUN_0044D410's two sub-fields: blob at +0 and int at +0xC).
struct CDBTableDataStorageRecord
{
    std::string body;   // pre-serialized record bytes (packed-byte form)
    int         id;     // primary key (record nID)

    CDBTableDataStorageRecord() : id(0) {}

    int operator&(CStructureSaver& f)
    {
        f.Add(2, &body);
        f.Add(3, &id);
        return 0;
    }
};

class CDBTableDataStorage : public CObjectBase
{
    OBJECT_BASIC_METHODS(CDBTableDataStorage)
public:
    std::vector<std::vector<int> >                  m_buckets;     // field 2
    std::vector<std::vector<int> >                  m_chainNext;   // field 3
    std::vector<std::vector<std::wstring> >         m_wstrings;    // field 4
    std::vector<CDBTableDataStorageRecord>          m_records;     // field 5
    std::vector<std::string>                        m_column6;     // field 6
    std::vector<std::string>                        m_column7;     // field 7
    std::vector<std::string>                        m_column8;     // field 8

    int operator&(CStructureSaver& f)
    {
        f.Add(2, &m_buckets);
        f.Add(3, &m_chainNext);
        f.Add(4, &m_wstrings);
        f.Add(5, &m_records);
        f.Add(6, &m_column6);
        f.Add(7, &m_column7);
        f.Add(8, &m_column8);
        return 0;
    }
};

REGISTER_SAVELOAD_CLASS(0xA1843130, CDBTableDataStorage)
