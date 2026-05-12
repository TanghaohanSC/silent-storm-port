// ADOImport runtime stub for the silent-storm port.
//
// Upstream ADOImport/BasicDB.cpp uses `#import "msado15.dll"` to interop with
// Microsoft Access via ADO COM. The Import() / Refresh() paths are dev-time
// only — used by tools that regenerate game.db from the ADO source.
//
// At runtime, Game/Main.cpp calls NDatabase::Serialize() to LOAD game.db.
// That path goes through CDBTableBase::operator& (header-only) and only needs
// the static state (tables hash, RecordTypes factory) to be live.
//
// This stub provides the linker-required symbols from BasicDB.h with the
// minimum behavior to load game.db. Anything that would touch ADO at runtime
// asserts.
//
// Pulled from upstream/Soft/Andy/Jan03/a5dll/ADOImport/BasicDB.cpp — kept
// API-compatible, stripped of all COM/ADO.

// Match the upstream PCH preamble (StdAfx.h pulls these in this order).
#include <windows.h>
#include <cstdio>
#ifdef _DEBUG
#  define ASSERT(a) if(!(a)){__debugbreak();}
#else
#  define ASSERT(a) ((void)0)
#endif
// We're stubbing ADOImport which was a DLL — its BasicDB.h decls aren't
// dllimported here; redefine externA5 to plain extern for this TU.
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
#include "..\..\..\upstream\Soft\Andy\Jan03\a5dll\ADOImport\BasicDB.h"
// silent-storm-port r21: r20's std::list path was a dead end; r21 finds via
// Ghidra-RE of shipping CDBTableBase::op& (FUN_00449A10) that storage is on
// CDBTableBase itself (CObj<CDBTableDataStorage> m_storage). PromoteRecords
// walks NDatabase::tables, materializing each table's records cache from its
// m_storage.

namespace NDatabase
{
    CClassFactory<CDBRecord>& GetRecordTypes()
    {
        static CClassFactory<CDBRecord> recordTypes;
        return recordTypes;
    }
    typedef std::hash_map<int, CDBTableBase> CTablesHash;
    CTablesHash& GetTables()
    {
        static CTablesHash tables;
        return tables;
    }
    static std::string szDataSource;
}
// Linker storage for the externA5-declared global.
bool NDatabase::bIsDatabaseLoading = false;

void NDatabase::SetSource(const char* pszSource)
{
    szDataSource = pszSource ? pszSource : "";
}

void NDatabase::AddTable(int nTableID, const char* pszTableName,
                          RecordCreateFunc newf)
{
    CTablesHash& tables = GetTables();
    if (tables.find(nTableID) != tables.end())
    {
        ASSERT(0); // already registered
        return;
    }
    GetRecordTypes().RegisterTypeSafe(nTableID, newf);
    tables[nTableID]; // create empty CDBTableBase
    // r17 attempted: probe = newf(); probe->Import() to capture columns.
    // Crashed: CRndModel::Import() calls GetGeometry() which needs the
    // Geometry table already populated — pre-load Import() probing is
    // unsafe because Imports reference other tables. Rolled back; will
    // need a different approach (Ghidra-RE NDatabase::Serialize in
    // MapEdit.exe to find the real storage↔table linkage).
    FILE* fp = NULL; fopen_s(&fp, "silent_storm_r16_addtable.log", "a");
    if (fp) {
        fprintf(fp, "AddTable[%d]: nTableID=0x%08X name='%s'\n",
                (int)tables.size() - 1, nTableID, pszTableName ? pszTableName : "(null)");
        fclose(fp);
    }
}

CDBTableBase* NDatabase::GetTable(int nTableID)
{
    CTablesHash& tables = GetTables();
    CTablesHash::iterator i = tables.find(nTableID);
    if (i != tables.end())
        return &i->second;
    return 0;
}

void NDatabase::AddRelation(const char* /*pszTableName*/)
{
    // ADOImport-only — relations are loaded into game.db at build time.
    // At runtime they're already serialized.
}

void NDatabase::Import()
{
    // Dev-only path. Production loads game.db via Serialize().
    ASSERT(0);
}

void NDatabase::Refresh(int /*nTableID*/)
{
    ASSERT(0);
}

// r19: promote records from the storage map (shipping wire field 2).
// For each (nTableID, storage) pair, factory-create empty CDBRecord
// instances and InsertRecord them into the matching CDBTableBase's
// records hash_map. Records remain field-default (body decode is a
// separate future round) — sufficient to clear BasicDB.h:102 asserts.
static void PromoteRecordsFromStorage()
{
    NDatabase::CTablesHash& tables = NDatabase::GetTables();
    CClassFactory<CDBRecord>& factory = NDatabase::GetRecordTypes();
    int nMatched = 0, nPromoted = 0, nTotalTables = (int)tables.size();
    // r23: dump body byte stats to reverse-engineer record body format
    FILE* dump = NULL; fopen_s(&dump, "silent_storm_r23_body_dump.log", "w");
    int dumped = 0;
    for (NDatabase::CTablesHash::iterator it = tables.begin(); it != tables.end(); ++it) {
        int nTableID = it->first;
        CDBTableBase& table = it->second;
        CDBTableDataStorage* s = table.GetStorage();
        if (!s) continue;
        ++nMatched;
        // r23.2 diag: dump inner row containers (m_buckets/wstrings/chainNext)
        if (dump && dumped < 4) {
            int br = (int)s->m_buckets.size();
            int cr = (int)s->m_chainNext.size();
            int wr = (int)s->m_wstrings.size();
            fprintf(dump, "\n=== table[%d]: rows? buck=%d chain=%d wstr=%d ; cols m_records=%d c6=%d c7=%d c8=%d ===\n",
                    nTableID, br, cr, wr, (int)s->m_records.size(),
                    (int)s->m_column6.size(), (int)s->m_column7.size(), (int)s->m_column8.size());
            // First 3 buckets - what's inside?
            for (int k = 0; k < 3 && k < br; ++k) {
                const std::vector<int>& b = s->m_buckets[k];
                fprintf(dump, "  bucket[%d] size=%d:", k, (int)b.size());
                int dl = (int)b.size(); if (dl > 8) dl = 8;
                for (int j = 0; j < dl; ++j) fprintf(dump, " %d", b[j]);
                fprintf(dump, "\n");
            }
            // First 3 chainNext
            for (int k = 0; k < 3 && k < cr; ++k) {
                const std::vector<int>& c = s->m_chainNext[k];
                fprintf(dump, "  chain[%d] size=%d:", k, (int)c.size());
                int dl = (int)c.size(); if (dl > 8) dl = 8;
                for (int j = 0; j < dl; ++j) fprintf(dump, " %d", c[j]);
                fprintf(dump, "\n");
            }
            // First 3 wstrings rows
            for (int k = 0; k < 3 && k < wr; ++k) {
                const std::vector<std::wstring>& w = s->m_wstrings[k];
                fprintf(dump, "  wstr_row[%d] size=%d:", k, (int)w.size());
                int dl = (int)w.size(); if (dl > 3) dl = 3;
                for (int j = 0; j < dl; ++j) {
                    fprintf(dump, " '");
                    int wl = (int)w[j].size(); if (wl > 16) wl = 16;
                    for (int q = 0; q < wl; ++q)
                        fprintf(dump, "%c", w[j][q] >= 32 && w[j][q] < 127 ? (char)w[j][q] : '.');
                    fprintf(dump, "'");
                }
                fprintf(dump, "\n");
            }
            ++dumped;
        }
        // legacy diag block disabled
        if (false && dump && dumped < 8 && !s->m_records.empty()) {
            int n = (int)s->m_records.size();
            int maxIdx = 0, maxSz = (int)s->m_records[0].body.size();
            for (int j = 1; j < n; ++j) {
                int sz = (int)s->m_records[j].body.size();
                if (sz > maxSz) { maxSz = sz; maxIdx = j; }
            }
            fprintf(dump, "\n=== table[nTableID=%d]: %d records, c6=%d cols ===\n",
                    nTableID, n, (int)s->m_column6.size());
            // Show 1st 3 column names from c6/c7/c8
            for (int c = 0; c < 3 && c < (int)s->m_column6.size(); ++c)
                fprintf(dump, "  c6[%d]='%s'", c, s->m_column6[c].c_str());
            fprintf(dump, "\n");
            for (int c = 0; c < 3 && c < (int)s->m_column7.size(); ++c)
                fprintf(dump, "  c7[%d]='%s'", c, s->m_column7[c].c_str());
            fprintf(dump, "\n");
            for (int c = 0; c < 3 && c < (int)s->m_column8.size(); ++c)
                fprintf(dump, "  c8[%d]='%s'", c, s->m_column8[c].c_str());
            fprintf(dump, "\n");
            // Dump first 3 records + the largest one
            int indices[4] = {0, 1, 2, maxIdx};
            for (int k = 0; k < 4; ++k) {
                int idx = indices[k];
                if (idx >= n) continue;
                if (k > 0 && idx == indices[k-1]) continue;
                const std::string& body = s->m_records[idx].body;
                fprintf(dump, "  rec[%d] id=%d sz=%d hex=",
                        idx, s->m_records[idx].id, (int)body.size());
                int dl = (int)body.size(); if (dl > 64) dl = 64;
                for (int j = 0; j < dl; ++j)
                    fprintf(dump, "%02X", (unsigned char)body[j]);
                // Also try ASCII
                fprintf(dump, " ascii=");
                for (int j = 0; j < dl; ++j) {
                    char c = body[j];
                    fprintf(dump, "%c", (c >= 32 && c < 127) ? c : '.');
                }
                fprintf(dump, "\n");
            }
            ++dumped;
        }
        for (size_t i = 0; i < s->m_records.size(); ++i) {
            CDBRecord* p = factory.CreateObject(nTableID);
            if (p) {
                table.InsertRecord(s->m_records[i].id, p);
                ++nPromoted;
            }
        }
    }
    if (dump) fclose(dump);
    FILE* fp = NULL; fopen_s(&fp, "silent_storm_r21_promote.log", "w");
    if (fp) {
        fprintf(fp, "r21: promoted %d records from %d/%d tables with storage\n",
                nPromoted, nMatched, nTotalTables);
        fclose(fp);
    }
}

void NDatabase::Serialize(CDataStream& file, CStructureSaver::EMode mode)
{
    CTablesHash& tables = GetTables();
    NDatabase::bIsDatabaseLoading = true;
    {
        CStructureSaver f(file, mode);
        f.Add(1, &tables);   // each table's op& reads CObj<storage> via Ghidra-confirmed wire
        // r19/r20 attempts to also read field 2 — turned out shipping's field 1
        // already carries all per-table storage refs via CDBTableBase::m_storage.
        // The field 2 in NDatabase::Serialize (~25 extras) is for non-per-table
        // storages we don't need yet.
    }
    PromoteRecordsFromStorage();
    NDatabase::bIsDatabaseLoading = false;
}

// r17 attempt rolled back: ImportField probing requires fully-loaded tables.
void NDatabase::ImportField(const char*, int*)            { ASSERT(0); }
void NDatabase::ImportField(const char*, bool*)           { ASSERT(0); }
void NDatabase::ImportField(const char*, float*)          { ASSERT(0); }
void NDatabase::ImportField(const char*, std::string*)    { ASSERT(0); }
void NDatabase::ImportField(const char*, std::wstring*)   { ASSERT(0); }
void NDatabase::ImportField(const char*, CDBRecord**, CDBTableBase*) { ASSERT(0); }
void NDatabase::ImportRelation(CDBRecord*, CDBTableBase*,
                                std::vector<CPtr<CDBRecord> >*) { ASSERT(0); }

std::string NDatabase::GetDBConnectionStr(const std::string& szDBName)
{
    std::string szRet = "DRIVER=SQL Server;SERVER=";
    szRet += szDBName;
    szRet += ";UID=sa;PWD=simple;DATABASE=A5GAME;";
    return szRet;
}

// CDBTableBase: header declares PreCreate/Refresh/Import — these only fire
// during dev-time Import(), so stub to assert.
void CDBTableBase::PreCreate(int /*nTypeID*/)  { ASSERT(0); }
void CDBTableBase::Refresh(int /*nTypeID*/)    { ASSERT(0); }
void CDBTableBase::Import()                    { ASSERT(0); }

CDBRecord* CDBTableBase::GetDBRecord(int nID)
{
    CRecordHash::iterator i = records.find(nID);
    if (i == records.end())
        return 0;
    return i->second;
}
