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
#include <vector>

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
// `refsFilledOut` accumulates count of type=5 (ref / CPtr<T>) columns that
// were successfully resolved across all rows in this table.
// r44: writes a single resolved ref pointer into a vector<CPtr<X>>[idx].
// `vecPtr` points at the std::vector<CPtr<CDBRecord>>-equivalent at the
// outer member offset. We resize-up if vec.size() <= elemIdx, then write the
// raw pointer at element[elemIdx].ptr (CPtrBase stores T* as the only field).
// Layout-compat note: std::vector<CPtr<X>> and std::vector<CPtr<Y>> have
// identical memory layout because (a) std::vector layout is independent of T
// (3 pointers in MSVC's STL), and (b) sizeof(CPtr<X>) == sizeof(void*) for
// every X (it only stores a T* ptr). resize() may use CPtr<T>'s default
// ctor (sets ptr=0) and dtor (calls Release on ptr) — for our purposes both
// are layout-compatible across T because CPtr<X>::ptr is at offset 0 and the
// AddRef/Release machinery shares CObjectBase::SRef regardless of X.
static void WriteVecRefSlot(void* vecPtr, int elemIdx, CDBRecord* pTarget)
{
    typedef std::vector<CPtr<CDBRecord> > VecT;
    VecT* v = reinterpret_cast<VecT*>(vecPtr);
    if ((int)v->size() <= elemIdx) {
        v->resize(elemIdx + 1);
    }
    // CPtr<T> derives from CPtrBase<T, SRef> which stores a single T* `ptr`
    // at offset 0. Write the raw pointer directly — bypasses refcount but
    // we already do this for scalar refs. Records leak intentionally (live
    // for the duration of the load).
    CPtr<CDBRecord>* slot = &(*v)[elemIdx];
    *reinterpret_cast<CDBRecord**>(slot) = pTarget;
}

// r46: typed scalar-vector writers. Each is a thin resize+assign over the
// raw std::vector<T>. Element types listed in the schema (vec_int/bool/...)
// drive which writer dbfill calls.
template <typename T>
static void WriteVecScalarSlot(void* vecPtr, int elemIdx, const T& val)
{
    std::vector<T>* v = reinterpret_cast<std::vector<T>*>(vecPtr);
    if ((int)v->size() <= elemIdx) {
        v->resize(elemIdx + 1);
    }
    (*v)[elemIdx] = val;
}

static int FillOneTable(CDBTableBase* table, const NDb::SColumnInfo* cols, FILE* log,
                        int* refsFilledOut, int* refsMissedOut,
                        int* vecRefsFilledOut, int* vecRefsMissedOut)
{
    CDBTableDataStorage* s = table->GetStorage();
    if (!s) return 0;

    STableColIdx idx;
    BuildColIdx(idx, s);

    int filled = 0;
    int refsFilled = 0;
    int refsMissed = 0;
    int vecRefsFilled = 0;
    int vecRefsMissed = 0;
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
                case 5: { // ref (CPtr<T>) — cross-table reference resolution
                    // refID stored as an int in the same m_buckets column slot
                    // as other ints (column metadata in m_column6 names it).
                    std::map<std::string, int>::const_iterator it = idx.intCols.find(cname);
                    if (it == idx.intCols.end()) { ++refsMissed; break; }
                    int j = it->second;
                    if (j >= (int)s->m_buckets[row].size()) { ++refsMissed; break; }
                    int refID = s->m_buckets[row][j];
                    if (refID <= 0) { break; }  // legitimate null ref
                    if (!c->refClass) { ++refsMissed; break; }
                    // r33: refClass may be an abstract base — try each of
                    // its concrete subclass tables until we find refID.
                    NDb::STableCandidates cands = NDb::GetTableIDsForClassName(c->refClass);
                    CDBRecord* pRefTarget = 0;
                    for (int k = 0; k < cands.count; ++k) {
                        CDBTableBase* pTargetTable = NDatabase::GetTable(cands.ids[k]);
                        if (!pTargetTable) continue;
                        pRefTarget = pTargetTable->GetDBRecord(refID);
                        if (pRefTarget) break;
                    }
                    if (!pRefTarget) { ++refsMissed; break; }
                    // CPtr<T> stores a single raw pointer field; write through
                    // as a CDBRecord*. We skip refcount bumps — records live
                    // for the duration of the load so leak/GC is moot.
                    *reinterpret_cast<CDBRecord**>(FieldAddr(rec, *c)) = pRefTarget;
                    ++refsFilled;
                    ++filled;
                    break;
                }
                case 6: { // r44: vec_ref — vector<CPtr<X>>[idx] = resolved ref
                    // The column-name maps to a single int slot (the refID
                    // for the i-th vector element). offset is the vector's
                    // offsetof; subOffset is the element index; refClass is
                    // the inner X class.
                    std::map<std::string, int>::const_iterator it = idx.intCols.find(cname);
                    if (it == idx.intCols.end()) { ++vecRefsMissed; break; }
                    int j = it->second;
                    if (j >= (int)s->m_buckets[row].size()) { ++vecRefsMissed; break; }
                    int refID = s->m_buckets[row][j];
                    if (refID <= 0) {
                        // Legitimate null/empty slot — still need to size the
                        // vector to at least subOffset+1 so .size() reflects
                        // the schema's intended capacity. Without this,
                        // pSide->defaultPersesSet.size() would stay at 0 if
                        // every entry happens to be null in this row.
                        void* vec = (char*)rec + c->offset;
                        WriteVecRefSlot(vec, c->subOffset, 0);
                        ++filled;
                        break;
                    }
                    if (!c->refClass) { ++vecRefsMissed; break; }
                    NDb::STableCandidates cands = NDb::GetTableIDsForClassName(c->refClass);
                    CDBRecord* pRefTarget = 0;
                    for (int k = 0; k < cands.count; ++k) {
                        CDBTableBase* pTargetTable = NDatabase::GetTable(cands.ids[k]);
                        if (!pTargetTable) continue;
                        pRefTarget = pTargetTable->GetDBRecord(refID);
                        if (pRefTarget) break;
                    }
                    if (!pRefTarget) { ++vecRefsMissed; break; }
                    void* vec = (char*)rec + c->offset;
                    WriteVecRefSlot(vec, c->subOffset, pRefTarget);
                    ++vecRefsFilled;
                    ++filled;
                    break;
                }
                case 7: { // r46: vec_int — vector<int>[idx] = stored int
                    std::map<std::string, int>::const_iterator it = idx.intCols.find(cname);
                    if (it == idx.intCols.end()) { ++vecRefsMissed; break; }
                    int j = it->second;
                    if (j >= (int)s->m_buckets[row].size()) { ++vecRefsMissed; break; }
                    void* vec = (char*)rec + c->offset;
                    WriteVecScalarSlot<int>(vec, c->subOffset, s->m_buckets[row][j]);
                    ++vecRefsFilled;
                    ++filled;
                    break;
                }
                case 8: { // r46: vec_bool — vector<bool> uses bit-packed STL;
                    // we treat each as a vector<unsigned char> for safety, but
                    // since std::vector<bool> isn't bit-compatible with that,
                    // we instead use the typed bool overload (which handles
                    // the proxy reference correctly via assignment to (*v)[i]).
                    std::map<std::string, int>::const_iterator it = idx.intCols.find(cname);
                    if (it == idx.intCols.end()) { ++vecRefsMissed; break; }
                    int j = it->second;
                    if (j >= (int)s->m_buckets[row].size()) { ++vecRefsMissed; break; }
                    void* vec = (char*)rec + c->offset;
                    std::vector<bool>* vb = reinterpret_cast<std::vector<bool>*>(vec);
                    if ((int)vb->size() <= c->subOffset) vb->resize(c->subOffset + 1);
                    (*vb)[c->subOffset] = (s->m_buckets[row][j] != 0);
                    ++vecRefsFilled;
                    ++filled;
                    break;
                }
                case 9: { // r46: vec_float — vector<float>[idx] = float bits
                    std::map<std::string, int>::const_iterator it = idx.floatCols.find(cname);
                    if (it == idx.floatCols.end()) { ++vecRefsMissed; break; }
                    int j = it->second;
                    if (row >= (int)s->m_chainNext.size() || j >= (int)s->m_chainNext[row].size()) { ++vecRefsMissed; break; }
                    int bits = s->m_chainNext[row][j];
                    float f = *reinterpret_cast<float*>(&bits);
                    void* vec = (char*)rec + c->offset;
                    WriteVecScalarSlot<float>(vec, c->subOffset, f);
                    ++vecRefsFilled;
                    ++filled;
                    break;
                }
                case 10: { // r46: vec_string — vector<std::string>[idx] (narrow)
                    std::map<std::string, int>::const_iterator it = idx.wstrCols.find(cname);
                    if (it == idx.wstrCols.end()) { ++vecRefsMissed; break; }
                    int j = it->second;
                    if (row >= (int)s->m_wstrings.size() || j >= (int)s->m_wstrings[row].size()) { ++vecRefsMissed; break; }
                    const std::wstring& w = s->m_wstrings[row][j];
                    std::string narrow;
                    narrow.reserve(w.size());
                    for (size_t k = 0; k < w.size(); ++k) narrow.push_back(static_cast<char>(w[k]));
                    void* vec = (char*)rec + c->offset;
                    WriteVecScalarSlot<std::string>(vec, c->subOffset, narrow);
                    ++vecRefsFilled;
                    ++filled;
                    break;
                }
                case 11: { // r46: vec_wstring — vector<std::wstring>[idx]
                    std::map<std::string, int>::const_iterator it = idx.wstrCols.find(cname);
                    if (it == idx.wstrCols.end()) { ++vecRefsMissed; break; }
                    int j = it->second;
                    if (row >= (int)s->m_wstrings.size() || j >= (int)s->m_wstrings[row].size()) { ++vecRefsMissed; break; }
                    void* vec = (char*)rec + c->offset;
                    WriteVecScalarSlot<std::wstring>(vec, c->subOffset, s->m_wstrings[row][j]);
                    ++vecRefsFilled;
                    ++filled;
                    break;
                }
                default:
                    break;
            }
        }
    }
    if (refsFilledOut) *refsFilledOut += refsFilled;
    if (refsMissedOut) *refsMissedOut += refsMissed;
    if (vecRefsFilledOut) *vecRefsFilledOut += vecRefsFilled;
    if (vecRefsMissedOut) *vecRefsMissedOut += vecRefsMissed;
    if (log) {
        fprintf(log, "  fill table: %d rows, %d field-writes (refs filled=%d missed=%d, vecRefs filled=%d missed=%d)\n",
                nRows, filled, refsFilled, refsMissed, vecRefsFilled, vecRefsMissed);
    }
    return filled;
}

} // anonymous namespace

extern "C" int PortFillAllRecordsFromStorage()
{
    int nSchemas = 0;
    const NDb::SSchemaEntry* schemas = NDb::GetAllSchemas(&nSchemas);

    // r30: write our own log so ado_stub's outer summary doesn't clobber it.
    FILE* log = 0;
    fopen_s(&log, "silent_storm_r30_dbfill.log", "w");
    if (log) {
        fprintf(log, "r30 dbfill: walking %d registered schemas\n", nSchemas);
    }

    int totalWritten = 0;
    int tablesHit = 0;
    int totalRefsFilled = 0;
    int totalRefsMissed = 0;
    int totalVecRefsFilled = 0;
    int totalVecRefsMissed = 0;
    for (int i = 0; i < nSchemas; ++i) {
        const NDb::SSchemaEntry& e = schemas[i];
        CDBTableBase* table = NDatabase::GetTable(e.typeID);
        if (!table) {
            if (log) fprintf(log, "schema[0x%08X]: no table registered, skip\n", e.typeID);
            continue;
        }
        if (log) fprintf(log, "schema[0x%08X]:\n", e.typeID);
        const NDb::SColumnInfo* cols = e.getter();
        int n = FillOneTable(table, cols, log, &totalRefsFilled, &totalRefsMissed,
                             &totalVecRefsFilled, &totalVecRefsMissed);
        if (n > 0) ++tablesHit;
        totalWritten += n;
    }

    if (log) {
        fprintf(log, "r30 dbfill: total %d field-writes across %d/%d tables\n",
                totalWritten, tablesHit, nSchemas);
        fprintf(log, "r30 dbfill: refs filled=%d, refs missed=%d\n",
                totalRefsFilled, totalRefsMissed);
        fprintf(log, "r44 dbfill: vecRefs filled=%d, vecRefs missed=%d\n",
                totalVecRefsFilled, totalVecRefsMissed);
        fclose(log);
    }
    return totalWritten;
}
