// silent-storm-port r29: static-schema-based record body fill.
//
// r28 generated `port/src/stubs/db_record_schemas.h` — for each of 39 known
// record types, a static SColumnInfo[] (column-name + type + member offset).
// r29 wires that into a runtime fill pass: after PromoteRecordsFromStorage()
// has materialized empty record instances under their IDs, this TU walks
// the schema registry, looks up each table by typeID via NDatabase::GetTable,
// iterates the per-table CDBTableDataStorage row arrays, and writes field
// values directly into the record memory using offsets from offsetof().
//
// Why a separate TU: db_record_schemas.h includes 14 DataXxx.h headers which
// transitively pull in Misc/Geom.h, basic2.h, tools.h, etc. — the full chain
// the Jan03 codebase expects through its PCH. ado_stub.cpp's compile context
// rebuilds only a minimal prefix of that (no Geom.h, no tools.h), so the
// schemas header doesn't parse cleanly there. Here we use Main/StdAfx.h as
// the PCH, matching the Main subproject's compile context exactly.

#include "..\..\..\upstream\Soft\Andy\Jan03\a5dll\Main\StdAfx.h"  // PCH; pulls all DataXxx + BasicDB.h via Specific.h

#include "..\stubs\db_table_storage.h"
#include "..\stubs\db_record_schemas.h"

#include <cstdio>
#include <map>
#include <string>

extern "C" int PortFillAllRecordsFromStorage();

namespace {

// Build (col_name -> index) maps once per table — m_columnN vectors don't
// change after load, so we can cache.
struct STableColIdx
{
    std::map<std::string, int> intCols;   // m_column6
    std::map<std::string, int> floatCols; // m_column7
    std::map<std::string, int> wstrCols;  // m_column8
    bool built;
    STableColIdx() : built(false) {}
};

static void BuildColIdx(STableColIdx& idx, const CDBTableDataStorage* s)
{
    if (idx.built) return;
    for (size_t i = 0; i < s->m_column6.size(); ++i)
        idx.intCols[s->m_column6[i]] = (int)i;
    for (size_t i = 0; i < s->m_column7.size(); ++i)
        idx.floatCols[s->m_column7[i]] = (int)i;
    for (size_t i = 0; i < s->m_column8.size(); ++i)
        idx.wstrCols[s->m_column8[i]] = (int)i;
    idx.built = true;
}

static inline void* FieldAddr(CDBRecord* rec, const NDb::SColumnInfo& col)
{
    char* base = reinterpret_cast<char*>(rec);
    return base + col.offset + col.subOffset;
}

// Read row data for `recordID` from storage. Storage rows are addressed by
// the row index where m_buckets[row][0] (the "ID" column at intCols index 0)
// equals recordID. We discover rows on the fly per table.
static int FillOneTable(CDBTableBase* table, const NDb::SColumnInfo* cols, FILE* log)
{
    CDBTableDataStorage* s = table->GetStorage();
    if (!s) return 0;

    STableColIdx idx;
    BuildColIdx(idx, s);

    int filled = 0;
    int nRows = (int)s->m_buckets.size();
    for (int row = 0; row < nRows; ++row) {
        if (s->m_buckets[row].empty()) continue;
        int recordID = s->m_buckets[row][0];  // first int column is "ID"
        if (recordID <= 0) continue;
        CDBRecord* rec = table->GetDBRecord(recordID);
        if (!rec) continue;

        for (const NDb::SColumnInfo* c = cols; c->name != 0; ++c) {
            std::string cname(c->name);
            switch (c->type) {
                case 0: { // int
                    std::map<std::string, int>::const_iterator it = idx.intCols.find(cname);
                    if (it == idx.intCols.end()) break;
                    int j = it->second;
                    if (j < (int)s->m_buckets[row].size()) {
                        *reinterpret_cast<int*>(FieldAddr(rec, *c)) = s->m_buckets[row][j];
                        ++filled;
                    }
                    break;
                }
                case 1: { // bool
                    std::map<std::string, int>::const_iterator it = idx.intCols.find(cname);
                    if (it == idx.intCols.end()) break;
                    int j = it->second;
                    if (j < (int)s->m_buckets[row].size()) {
                        *reinterpret_cast<bool*>(FieldAddr(rec, *c)) = (s->m_buckets[row][j] != 0);
                        ++filled;
                    }
                    break;
                }
                case 2: { // float (stored as int bits in m_chainNext)
                    std::map<std::string, int>::const_iterator it = idx.floatCols.find(cname);
                    if (it == idx.floatCols.end()) break;
                    int j = it->second;
                    if (row < (int)s->m_chainNext.size() && j < (int)s->m_chainNext[row].size()) {
                        int bits = s->m_chainNext[row][j];
                        *reinterpret_cast<float*>(FieldAddr(rec, *c)) = *reinterpret_cast<float*>(&bits);
                        ++filled;
                    }
                    break;
                }
                case 3: { // std::string (narrow-converted from wstring storage)
                    std::map<std::string, int>::const_iterator it = idx.wstrCols.find(cname);
                    if (it == idx.wstrCols.end()) break;
                    int j = it->second;
                    if (row < (int)s->m_wstrings.size() && j < (int)s->m_wstrings[row].size()) {
                        const std::wstring& w = s->m_wstrings[row][j];
                        std::string narrow;
                        narrow.reserve(w.size());
                        for (size_t k = 0; k < w.size(); ++k)
                            narrow.push_back(static_cast<char>(w[k]));
                        *reinterpret_cast<std::string*>(FieldAddr(rec, *c)) = narrow;
                        ++filled;
                    }
                    break;
                }
                case 4: { // std::wstring
                    std::map<std::string, int>::const_iterator it = idx.wstrCols.find(cname);
                    if (it == idx.wstrCols.end()) break;
                    int j = it->second;
                    if (row < (int)s->m_wstrings.size() && j < (int)s->m_wstrings[row].size()) {
                        *reinterpret_cast<std::wstring*>(FieldAddr(rec, *c)) = s->m_wstrings[row][j];
                        ++filled;
                    }
                    break;
                }
                case 5: // ref (CPtr<T>) — deferred to a later round
                default:
                    break;
            }
        }
    }
    if (log) {
        fprintf(log, "  fill table: %d rows, %d field-writes\n", nRows, filled);
    }
    return filled;
}

} // anonymous namespace

extern "C" int PortFillAllRecordsFromStorage()
{
    int nSchemas = 0;
    const NDb::SSchemaEntry* schemas = NDb::GetAllSchemas(&nSchemas);

    FILE* log = 0;
    fopen_s(&log, "silent_storm_r29_promote.log", "w");
    if (log) {
        fprintf(log, "r29 dbfill: walking %d registered schemas\n", nSchemas);
    }

    int totalWritten = 0;
    int tablesHit = 0;
    for (int i = 0; i < nSchemas; ++i) {
        const NDb::SSchemaEntry& e = schemas[i];
        CDBTableBase* table = NDatabase::GetTable(e.typeID);
        if (!table) {
            if (log) fprintf(log, "schema[0x%08X]: no table registered, skip\n", e.typeID);
            continue;
        }
        if (log) fprintf(log, "schema[0x%08X]:\n", e.typeID);
        const NDb::SColumnInfo* cols = e.getter();
        int n = FillOneTable(table, cols, log);
        if (n > 0) ++tablesHit;
        totalWritten += n;
    }

    if (log) {
        fprintf(log, "r29 dbfill: total %d field-writes across %d/%d tables\n",
                totalWritten, tablesHit, nSchemas);
        fclose(log);
    }
    return totalWritten;
}
