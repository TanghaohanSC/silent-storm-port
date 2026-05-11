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
#include "..\..\..\upstream\Soft\Andy\Jan03\a5dll\ADOImport\BasicDB.h"

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

void NDatabase::Serialize(CDataStream& file, CStructureSaver::EMode mode)
{
    CTablesHash& tables = GetTables();
    NDatabase::bIsDatabaseLoading = true;
    {
        CStructureSaver f(file, mode);
        f.Add(1, &tables);
    }
    NDatabase::bIsDatabaseLoading = false;
}

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
