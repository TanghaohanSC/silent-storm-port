// silent-storm-port: shared header for CDBTableDataStorage (typeID 0xA1843130)
//
// Included by:
//   - upstream/Soft/Andy/Jan03/a5dll/ADOImport/BasicDB.h  (patched in r15 to
//     hold a CObj<CDBTableDataStorage> field instead of inline records hash_map)
//   - port/src/stubs/db_table_storage_stub.cpp  (the OBJECT registration +
//     class definition implementation TU)
//
// The class implementation (with OBJECT_BASIC_METHODS and REGISTER_SAVELOAD_CLASS)
// lives in db_table_storage_stub.cpp. This header is JUST the public-facing
// shape so BasicDB.h can declare a CObj<CDBTableDataStorage> member.

#ifndef SILENT_STORM_PORT_DB_TABLE_STORAGE_H
#define SILENT_STORM_PORT_DB_TABLE_STORAGE_H

#include <string>
#include <vector>

// basic2.h provides CObjectBase, CObj<T>, OBJECT_BASIC_METHODS — must be
// included by the consumer before this header.

// Wire field 5 entry: {string body, int id} — 16-byte struct on STLport,
// matching FUN_0044D410's two sub-fields (raw blob at +0x00 + int at +0x0C).
struct CDBTableDataStorageRecord
{
    std::string body;   // pre-serialized record bytes
    int         id;     // record nID

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

#endif
