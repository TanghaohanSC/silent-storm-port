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
        // silent-storm-port r16: dump storage layout for nTableID hunt
        if (f.IsReading()) {
            FILE* fp = NULL;
            fopen_s(&fp, "silent_storm_r16_storage_dump.log", "a");
            if (fp) {
                fprintf(fp, "storage @ %p: rec=%d buck=%d wstr=%d c6=%d c7=%d c8=%d",
                    (void*)this, (int)m_records.size(), (int)m_buckets.size(),
                    (int)m_wstrings.size(), (int)m_column6.size(),
                    (int)m_column7.size(), (int)m_column8.size());
                if (!m_records.empty()) {
                    fprintf(fp, " ids=[");
                    int n = (int)m_records.size(); if (n > 5) n = 5;
                    for (int i = 0; i < n; ++i) fprintf(fp, "%d,", m_records[i].id);
                    fprintf(fp, "] body0=%d", (int)m_records[0].body.size());
                }
                if (!m_column6.empty() && !m_column6[0].empty()) {
                    fprintf(fp, " c6[0]_b=");
                    int n = (int)m_column6[0].size(); if (n > 8) n = 8;
                    for (int i = 0; i < n; ++i)
                        fprintf(fp, "%02X", (unsigned char)m_column6[0][i]);
                }
                if (!m_column7.empty() && !m_column7[0].empty()) {
                    fprintf(fp, " c7[0]_b=");
                    int n = (int)m_column7[0].size(); if (n > 8) n = 8;
                    for (int i = 0; i < n; ++i)
                        fprintf(fp, "%02X", (unsigned char)m_column7[0][i]);
                }
                if (!m_column8.empty() && !m_column8[0].empty()) {
                    fprintf(fp, " c8[0]_b=");
                    int n = (int)m_column8[0].size(); if (n > 8) n = 8;
                    for (int i = 0; i < n; ++i)
                        fprintf(fp, "%02X", (unsigned char)m_column8[0][i]);
                }
                fprintf(fp, "\n");
                fclose(fp);
            }
        }
        return 0;
    }
};

#endif
