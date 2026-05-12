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
// silent-storm-port r20: Ghidra-RE of FUN_0044BBF0 (returns the field 2 global)
// shows a 40-byte heap-allocated struct with self-referencing next/prev at
// offsets 0 and 4 — a doubly-linked-list head sentinel. So shipping NDatabase
// holds storages in `std::list<CObj<CDBTableDataStorage>>` (ordered, no keys),
// not a hash_map. Wire dispatches through BasicChunk1.h's std::list template
// which writes/reads N CObj refs as sub-chunks 1..N.
typedef std::list<CObj<CDBTableDataStorage> > CStorageList;
static CStorageList& GetStorageList()
{
    static CStorageList s_storage;
    return s_storage;
}

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
static void PromoteRecordsFromStorageList()
{
    CStorageList& storageList = GetStorageList();
    int nTotal = (int)storageList.size();
    int nPromoted = 0;
    // r20: no key info in std::list — for now just count + log the sizes.
    // Mapping list-index → nTableID comes in the next round.
    FILE* fp = NULL; fopen_s(&fp, "silent_storm_r20_promote.log", "w");
    if (fp) {
        fprintf(fp, "r20: read %d storage entries from wire field 2 (std::list form)\n", nTotal);
        int i = 0;
        for (CStorageList::iterator it = storageList.begin(); it != storageList.end() && i < 20; ++it, ++i) {
            CDBTableDataStorage* s = it->GetPtr();
            if (s) fprintf(fp, "  storage[%d]: %d records, c6=%d cols\n",
                           i, (int)s->m_records.size(), (int)s->m_column6.size());
            else fprintf(fp, "  storage[%d]: NULL\n", i);
        }
        fclose(fp);
    }
    storageList.clear();
    (void)nPromoted;
}

void NDatabase::Serialize(CDataStream& file, CStructureSaver::EMode mode)
{
    CTablesHash& tables = GetTables();
    CStorageList& storageList = GetStorageList();
    NDatabase::bIsDatabaseLoading = true;
    {
        CStructureSaver f(file, mode);
        f.Add(1, &tables);          // field 1: tables hash_map (Jan03 compatible)
        f.Add(2, &storageList);     // field 2: std::list of storage refs (r20)
    }
    PromoteRecordsFromStorageList();
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
