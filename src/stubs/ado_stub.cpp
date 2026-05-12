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
#include <map>
#include <set>
// silent-storm-port r17: schema-based storage<->table linkage.
// At AddTable time we run Import() against a probe instance, capturing the
// ImportField column names. At storage load time the storage::operator& tail
// looks up its c6 column set in s_tableSchemas to find the matching nTableID.
namespace {
    static int s_currentProbeTableID = -1;  // thread-local-ish (single-threaded boot)
    static std::map<int, std::set<std::string> > s_tableSchemas;       // nTableID -> {col names}
    static std::map<std::set<std::string>, int> s_schemaToTable;       // {col names} -> nTableID
}

// Called by db_table_storage.h's operator& after fields load.
// Returns nTableID for the storage, or -1 if no match.
int PortFindStorageTable( const std::vector<std::string>& c6 )
{
    std::set<std::string> cols(c6.begin(), c6.end());
    std::map<std::set<std::string>, int>::iterator it = s_schemaToTable.find(cols);
    if (it != s_schemaToTable.end()) return it->second;
    return -1;
}

// silent-storm-port r15: temporary backref (now unused — kept so BasicDB.h's
// forward declaration still resolves at link time).
void PortReadCDBTableStorage( CStructureSaver& f, CDBTableBase* pTable )
{
    // r15 approach: tried to read CObj<CDBTableDataStorage> from wire field 1.
    // r16 revealed the wire holds no such ref — storage objects are obj-pool
    // orphans. r17 routes through schema matching instead; this helper is now
    // a no-op that just consumes whatever's in the field 1 chunk (which is
    // empty/null on the wire for this path).
    CObj<CDBTableDataStorage> _unused;
    f.Add( 1, &_unused );
    (void)pTable;
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

// r17: storage→nTableID associations captured during storage load.
static std::map<CDBTableDataStorage*, int> s_loadedStorages;

void PortRegisterLoadedStorage(int nTableID, CDBTableDataStorage* s)
{
    if (s) s_loadedStorages[s] = nTableID;
}

// silent-storm-port r17: promote records from schema-matched storages.
// For each (storage, nTableID) pair, factory-create empty CDBRecord
// instances (typeID = nTableID), set nID = stored record id, register
// into CDBTableBase::records. Records remain field-default (no body
// decode yet) — sufficient to clear BasicDB.h:102 GetRecord asserts.
static void PromoteRecordsFromStorage()
{
    NDatabase::CTablesHash& tables = NDatabase::GetTables();
    CClassFactory<CDBRecord>& factory = NDatabase::GetRecordTypes();
    int nPromoted = 0;
    int nUnmatched = 0;
    int nStorages = (int)s_loadedStorages.size();
    for (std::map<CDBTableDataStorage*, int>::iterator
         it = s_loadedStorages.begin(); it != s_loadedStorages.end(); ++it) {
        CDBTableDataStorage* pStorage = it->first;
        int nTableID = it->second;
        if (nTableID < 0) { ++nUnmatched; continue; }
        NDatabase::CTablesHash::iterator tk = tables.find(nTableID);
        if (tk == tables.end()) { ++nUnmatched; continue; }
        CDBTableBase* pTable = &tk->second;
        for (size_t i = 0; i < pStorage->m_records.size(); ++i) {
            CDBRecord* p = factory.CreateObject(nTableID);
            if (p) {
                pTable->InsertRecord(pStorage->m_records[i].id, p);
                ++nPromoted;
            }
        }
    }
    s_loadedStorages.clear();
    FILE* fp = NULL; fopen_s(&fp, "silent_storm_r17_promote.log", "w");
    if (fp) {
        fprintf(fp, "r17 promoted %d records from %d matched storages (%d unmatched)\n",
                nPromoted, nStorages - nUnmatched, nUnmatched);
        fclose(fp);
    }
}

void NDatabase::Serialize(CDataStream& file, CStructureSaver::EMode mode)
{
    CTablesHash& tables = GetTables();
    NDatabase::bIsDatabaseLoading = true;
    {
        CStructureSaver f(file, mode);
        f.Add(1, &tables);
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
