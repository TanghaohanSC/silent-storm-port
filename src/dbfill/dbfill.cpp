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
// r49: post-fill back-population of CRndPtr<T>::variants and inverse vector
// relations (CTemplVariant::rects, ::pFinalElements, ::pUnits, etc.). The
// upstream Import() implementations did this via PushItem(&parent->vec,this)
// after ImportField filled the back-ref ptr. Our dbfill bypasses Import(),
// so we replay the back-population here in one sweep.
extern "C" int PortBackPopulateVariants();

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

namespace {
// r49: helper — push p into pVec only if not already present. Matches the
// upstream PushItem(...) semantics from DBFormat/DataMap.cpp:29.
template <typename T>
static bool PushItemUnique(std::vector< CPtr<T> >* pVec, T* p)
{
    if (!pVec || !p) return false;
    for (size_t i = 0; i < pVec->size(); ++i) {
        if (pVec->operator[](i).GetPtr() == p) return false;
    }
    pVec->push_back(CPtr<T>(p));
    return true;
}

// r49: iterate every record in a table, and for each `variant` whose back-ref
// pointer (member-of-pointer-to-parent) is non-null, push it into the parent's
// vector and (optionally) add an equal-weight sector to the parent's roulette.
//
// Implemented as a non-template helper per (Variant,Parent,member) triple,
// done inline below where types are known.
} // anonymous namespace

extern "C" int PortBackPopulateVariants()
{
    FILE* log = 0;
    fopen_s(&log, "silent_storm_r49_backpop.log", "w");
    if (log) {
        fprintf(log, "r49 backpop: starting reverse-population of CRndPtr vectors\n");
    }

    int nTemplVar = 0;
    int nRect = 0;
    int nFinalEl = 0;
    int nUnit = 0;
    int nMaterial = 0;
    int nRndModel = 0;
    int nEffect = 0;
    int nAmbLight = 0;
    int nSound = 0;
    int nRndObj = 0;
    int nRndCP = 0;
    int nExplosion = 0;
    int nTerrSpot = 0;
    int nWaypoint = 0;

    // === CTemplVariant -> CTemplate::variants =========================
    if ( NDb::CTemplate * /*unused*/ probe = (NDb::CTemplate*)0 )
    {
        (void)probe; // silence
    }
    {
        CDBTable<NDb::CTemplVariant>* pT = NDatabase::GetTable<NDb::CTemplVariant>();
        if (pT) {
            CDBIterator<NDb::CTemplVariant> it(*pT);
            while (it.MoveNext()) {
                NDb::CTemplVariant* pV = it.Get();
                if (!pV) continue;
                NDb::CTemplate* pParent = pV->pTemplate.GetPtr();
                if (!pParent) continue;
                if (PushItemUnique<NDb::CTemplVariant>(&pParent->variants, pV)) {
                    pParent->roulette.AddSector(1.0f);
                    ++nTemplVar;
                }
            }
        }
    }

    // === CRectangle -> CTemplVariant::rects ===========================
    {
        CDBTable<NDb::CRectangle>* pT = NDatabase::GetTable<NDb::CRectangle>();
        if (pT) {
            CDBIterator<NDb::CRectangle> it(*pT);
            while (it.MoveNext()) {
                NDb::CRectangle* pR = it.Get();
                if (!pR) continue;
                NDb::CTemplVariant* pParent = pR->pVariant.GetPtr();
                if (!pParent) continue;
                if (PushItemUnique<NDb::CRectangle>(&pParent->rects, pR)) ++nRect;
            }
        }
    }

    // === CFinalElement -> CTemplVariant::pFinalElements ==============
    {
        CDBTable<NDb::CFinalElement>* pT = NDatabase::GetTable<NDb::CFinalElement>();
        if (pT) {
            CDBIterator<NDb::CFinalElement> it(*pT);
            while (it.MoveNext()) {
                NDb::CFinalElement* pE = it.Get();
                if (!pE) continue;
                NDb::CTemplVariant* pParent = pE->pVariant.GetPtr();
                if (!pParent) continue;
                if (PushItemUnique<NDb::CFinalElement>(&pParent->pFinalElements, pE)) ++nFinalEl;
            }
        }
    }

    // === CUnit -> CTemplVariant::pUnits ==============================
    {
        CDBTable<NDb::CUnit>* pT = NDatabase::GetTable<NDb::CUnit>();
        if (pT) {
            CDBIterator<NDb::CUnit> it(*pT);
            while (it.MoveNext()) {
                NDb::CUnit* pU = it.Get();
                if (!pU) continue;
                NDb::CTemplVariant* pParent = pU->pVariant.GetPtr();
                if (!pParent) continue;
                if (PushItemUnique<NDb::CUnit>(&pParent->pUnits, pU)) ++nUnit;
            }
        }
    }

    // === CMaterial -> CTMaterial::variants ===========================
    {
        CDBTable<NDb::CMaterial>* pT = NDatabase::GetTable<NDb::CMaterial>();
        if (pT) {
            CDBIterator<NDb::CMaterial> it(*pT);
            while (it.MoveNext()) {
                NDb::CMaterial* pM = it.Get();
                if (!pM) continue;
                NDb::CTMaterial* pParent = pM->pTemplate.GetPtr();
                if (!pParent) continue;
                if (PushItemUnique<NDb::CMaterial>(&pParent->variants, pM)) {
                    pParent->roulette.AddSector(1.0f);
                    ++nMaterial;
                }
            }
        }
    }

    // === CRndModel -> CTRndModel::variants ===========================
    // r49 NOTE: CRndModel is defined in DataFormat.cpp's NDb namespace —
    // forward-decl-only in the public header. Cannot dereference its members
    // from this TU. Skipped; if needed for terrain rendering, move the class
    // definition into DataFormat.h or stage a second TU compiled inside the
    // DBFormat directory.

    // === CEffect -> CTEffect::variants ===============================
    {
        CDBTable<NDb::CEffect>* pT = NDatabase::GetTable<NDb::CEffect>();
        if (pT) {
            CDBIterator<NDb::CEffect> it(*pT);
            while (it.MoveNext()) {
                NDb::CEffect* pE = it.Get();
                if (!pE) continue;
                NDb::CTEffect* pParent = pE->pTemplate.GetPtr();
                if (!pParent) continue;
                if (PushItemUnique<NDb::CEffect>(&pParent->variants, pE)) {
                    pParent->roulette.AddSector(1.0f);
                    ++nEffect;
                }
            }
        }
    }

    // === CAmbientLight -> CTAmbientLight::variants ===================
    {
        CDBTable<NDb::CAmbientLight>* pT = NDatabase::GetTable<NDb::CAmbientLight>();
        if (pT) {
            CDBIterator<NDb::CAmbientLight> it(*pT);
            while (it.MoveNext()) {
                NDb::CAmbientLight* pL = it.Get();
                if (!pL) continue;
                NDb::CTAmbientLight* pParent = pL->pTemplate.GetPtr();
                if (!pParent) continue;
                if (PushItemUnique<NDb::CAmbientLight>(&pParent->variants, pL)) {
                    pParent->roulette.AddSector(1.0f);
                    ++nAmbLight;
                }
            }
        }
    }

    // === CSoundVariant -> CTSound::variants ==========================
    {
        CDBTable<NDb::CSoundVariant>* pT = NDatabase::GetTable<NDb::CSoundVariant>();
        if (pT) {
            CDBIterator<NDb::CSoundVariant> it(*pT);
            while (it.MoveNext()) {
                NDb::CSoundVariant* pS = it.Get();
                if (!pS) continue;
                NDb::CTSound* pParent = pS->pTemplate.GetPtr();
                if (!pParent) continue;
                if (PushItemUnique<NDb::CSoundVariant>(&pParent->variants, pS)) {
                    pParent->roulette.AddSector(1.0f);
                    ++nSound;
                }
            }
        }
    }

    // === CRndObject -> CTRndObject::variants =========================
    {
        CDBTable<NDb::CRndObject>* pT = NDatabase::GetTable<NDb::CRndObject>();
        if (pT) {
            CDBIterator<NDb::CRndObject> it(*pT);
            while (it.MoveNext()) {
                NDb::CRndObject* pO = it.Get();
                if (!pO) continue;
                NDb::CTRndObject* pParent = pO->pTemplate.GetPtr();
                if (!pParent) continue;
                if (PushItemUnique<NDb::CRndObject>(&pParent->variants, pO)) {
                    pParent->roulette.AddSector(1.0f);
                    ++nRndObj;
                }
            }
        }
    }

    // === CRndConstructionPart -> CTConstructionPart::variants ========
    // r49 NOTE: same as CRndModel — defined inline in DataFormat.cpp's NDb
    // namespace; not visible here. Skipped.

    // === CExplosion -> CTemplVariant::explosions =====================
    {
        CDBTable<NDb::CExplosion>* pT = NDatabase::GetTable<NDb::CExplosion>();
        if (pT) {
            CDBIterator<NDb::CExplosion> it(*pT);
            while (it.MoveNext()) {
                NDb::CExplosion* pE = it.Get();
                if (!pE) continue;
                NDb::CTemplVariant* pParent = pE->pVariant.GetPtr();
                if (!pParent) continue;
                if (PushItemUnique<NDb::CExplosion>(&pParent->explosions, pE)) ++nExplosion;
            }
        }
    }

    // === CRndTerrainSpot -> CTemplVariant::terrainSpots ==============
    {
        CDBTable<NDb::CRndTerrainSpot>* pT = NDatabase::GetTable<NDb::CRndTerrainSpot>();
        if (pT) {
            CDBIterator<NDb::CRndTerrainSpot> it(*pT);
            while (it.MoveNext()) {
                NDb::CRndTerrainSpot* pS = it.Get();
                if (!pS) continue;
                NDb::CTemplVariant* pParent = pS->pVar.GetPtr();
                if (!pParent) continue;
                if (PushItemUnique<NDb::CRndTerrainSpot>(&pParent->terrainSpots, pS)) ++nTerrSpot;
            }
        }
    }

    // === CWaypoint -> CTemplVariant::waypoints =======================
    {
        CDBTable<NDb::CWaypoint>* pT = NDatabase::GetTable<NDb::CWaypoint>();
        if (pT) {
            CDBIterator<NDb::CWaypoint> it(*pT);
            while (it.MoveNext()) {
                NDb::CWaypoint* pW = it.Get();
                if (!pW) continue;
                NDb::CTemplVariant* pParent = pW->pVar.GetPtr();
                if (!pParent) continue;
                if (PushItemUnique<NDb::CWaypoint>(&pParent->waypoints, pW)) ++nWaypoint;
            }
        }
    }

    int nTotal = nTemplVar + nRect + nFinalEl + nUnit + nMaterial + nRndModel + nEffect +
                 nAmbLight + nSound + nRndObj + nRndCP + nExplosion + nTerrSpot + nWaypoint;

    if (log) {
        fprintf(log, "r49 backpop:  CTemplVariant->CTemplate::variants            = %d\n", nTemplVar);
        fprintf(log, "r49 backpop:  CRectangle->CTemplVariant::rects              = %d\n", nRect);
        fprintf(log, "r49 backpop:  CFinalElement->CTemplVariant::pFinalElements  = %d\n", nFinalEl);
        fprintf(log, "r49 backpop:  CUnit->CTemplVariant::pUnits                  = %d\n", nUnit);
        fprintf(log, "r49 backpop:  CMaterial->CTMaterial::variants               = %d\n", nMaterial);
        fprintf(log, "r49 backpop:  CRndModel->CTRndModel::variants               = %d\n", nRndModel);
        fprintf(log, "r49 backpop:  CEffect->CTEffect::variants                   = %d\n", nEffect);
        fprintf(log, "r49 backpop:  CAmbientLight->CTAmbientLight::variants       = %d\n", nAmbLight);
        fprintf(log, "r49 backpop:  CSoundVariant->CTSound::variants              = %d\n", nSound);
        fprintf(log, "r49 backpop:  CRndObject->CTRndObject::variants             = %d\n", nRndObj);
        fprintf(log, "r49 backpop:  CRndConstructionPart->CTConstructionPart::v   = %d\n", nRndCP);
        fprintf(log, "r49 backpop:  CExplosion->CTemplVariant::explosions         = %d\n", nExplosion);
        fprintf(log, "r49 backpop:  CRndTerrainSpot->CTemplVariant::terrainSpots  = %d\n", nTerrSpot);
        fprintf(log, "r49 backpop:  CWaypoint->CTemplVariant::waypoints           = %d\n", nWaypoint);
        fprintf(log, "r49 backpop:  TOTAL                                         = %d\n", nTotal);
        fclose(log);
    }
    return nTotal;
}
