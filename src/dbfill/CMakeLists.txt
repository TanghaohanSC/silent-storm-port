# silent-storm-port r29: dbfill static lib.
#
# Wires the auto-generated SColumnInfo[] tables (port/src/stubs/db_record_schemas.h)
# into a runtime fill pass that ado_stub.cpp's PromoteRecordsFromStorage() calls
# at the end of NDatabase::Serialize().
#
# Lives in its own subproject because db_record_schemas.h transitively pulls in
# 14 DataXxx.h headers from upstream/DBFormat — those need the full Main/StdAfx.h
# preamble (basic2, tools, Geom, BasicFactory, ...) to compile cleanly. The ado
# stub's compile context is too minimal for that chain. We use Main/StdAfx.h as
# our PCH, matching how upstream's Main subproject builds its 271 .cpp files.

set(JAN03_ROOT "${CMAKE_SOURCE_DIR}/../upstream/Soft/Andy/Jan03/a5dll")
set(MAIN_DIR "${JAN03_ROOT}/Main")

add_library(silent_storm_dbfill STATIC dbfill.cpp)

# Match the Main subproject's compile context: same defines, same flags,
# same include search order. Without this, Main/StdAfx.h's #include chain
# (stl_user_config.h, ..\Misc\basic2.h, ..\DBFormat\DataXxx.h) won't resolve.
target_include_directories(silent_storm_dbfill PRIVATE
    ${MAIN_DIR}
    ${JAN03_ROOT}
    ${CMAKE_SOURCE_DIR}/src/stubs
)
target_precompile_headers(silent_storm_dbfill PRIVATE
    "${MAIN_DIR}/StdAfx.h"
)

set_target_properties(silent_storm_dbfill PROPERTIES
    CXX_STANDARD 14
    CXX_EXTENSIONS OFF
)

target_compile_definitions(silent_storm_dbfill PRIVATE
    WIN32 _WINDOWS _USRDLL
    _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS
    _SCL_SECURE_NO_WARNINGS
    _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
    _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
    _SILENCE_ALL_CXX20_DEPRECATION_WARNINGS
    _HAS_CXX17=0 _HAS_CXX20=0
    NOMINMAX WIN32_LEAN_AND_MEAN
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

if(MSVC)
    target_compile_options(silent_storm_dbfill PRIVATE
        /W0 /Zm300 /wd4828 /EHsc /MP /permissive
        /Zc:noexceptTypes- /Zc:ternary-
        /FI${CMAKE_SOURCE_DIR}/third_party/stlport_shim/legacy_compat.h
    )
endif()

# Link deps:
#   - legacy::stlport: matches the STLport hash_map/string used everywhere in Jan03.
#   - jan03::DBFormat: provides NDb::CDBXxx record class definitions referenced
#     by db_record_schemas.h (the offsetof()s require complete types).
#   - jan03::Misc / jan03::FileIO: basic2.h, BasicFactory.h, Geom.h, BasicChunk1.h.
#   - silent_storm_dbts_stub: CDBTableDataStorage definition (m_buckets etc.).
#   - silent_storm_ado_stub: NDatabase::GetTable() entry point.
target_link_libraries(silent_storm_dbfill PUBLIC
    legacy::stlport
    jan03::Misc
    jan03::FileIO
    jan03::DBFormat
    silent_storm_dbts_stub
    silent_storm_ado_stub
)
