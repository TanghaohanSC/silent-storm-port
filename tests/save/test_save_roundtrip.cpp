// Phase 7 smoke test — save / load round-trip.
//
// Builds a non-trivial GameState (multiple units, all field types
// populated, including empty optional vectors), serialises it,
// deserialises a fresh copy, and asserts equality.  Also exercises
// the on-disk path via save_to_file / load_from_file.
//
// Exit code 0 = success, non-zero = failure.  Used by CI / by hand
// during Phase 7 validation.

#include "../../src/save/save.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace silent_storm::save;

namespace {

GameState build_fixture() {
    GameState s;
    s.schema_version = 1;
    s.timestamp_unix = 1747000000ull;  // fixed -> deterministic equality
    s.slot_name = "quicksave";
    s.rng_seed = 0xDEADBEEFCAFEBABEull;

    s.mission.id = "m01_intro";
    s.mission.map_path = "maps/m01.map";
    s.mission.turn_counter = 7;
    s.mission.active_side = 0;
    s.mission.objectives_complete = {1u, 3u};
    s.mission.objectives_failed = {};

    UnitState u0;
    u0.id = 100;
    u0.name = "Hauptmann Hans";
    u0.archetype_id = 42;
    u0.position = {12.5f, 0.0f, -7.25f};
    u0.rotation = 1.5707963f;
    u0.hp = 80;
    u0.hp_max = 100;
    u0.ap = 6;
    u0.ap_max = 8;
    u0.side = 1;
    u0.inventory_item_ids = {201u, 202u, 999u};
    u0.perks = {1u, 4u, 7u};
    u0.state_flags = 0x1A;
    s.units.push_back(u0);

    UnitState u1;
    u1.id = 101;
    u1.name = "";                       // empty string is a valid field
    u1.archetype_id = 7;
    u1.position = {0.0f, 0.0f, 0.0f};
    u1.rotation = 0.0f;
    u1.hp = 50;
    u1.hp_max = 50;
    u1.ap = 4;
    u1.ap_max = 4;
    u1.side = 0;
    u1.inventory_item_ids = {};          // empty vector also valid
    u1.perks = {};
    u1.state_flags = 0;
    s.units.push_back(u1);

    s.extras = {
        {"future_v2_blob", "deadbeef"},
        {"replay_seed", "12345"},
    };
    return s;
}

#define CHECK(expr) do {                                                       \
    if (!(expr)) {                                                             \
        std::fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);   \
        std::exit(1);                                                          \
    }                                                                          \
} while (0)

void test_in_memory_roundtrip() {
    GameState orig = build_fixture();
    auto buf = serialize(orig);
    CHECK(!buf.empty());
    CHECK(buf.size() >= 8);  // size prefix + identifier at least

    // size prefix is little-endian uint32 of (total - 4)
    uint32_t prefix = 0;
    std::memcpy(&prefix, buf.data(), sizeof(prefix));
    CHECK(prefix == buf.size() - 4);

    // bytes 4..8 should be the "SSV1" file identifier (offset to root
    // sits at bytes 4..8 of the unprefixed buffer; the 4-byte ident is
    // placed by Finish*SizePrefixed at offset 4..8 of the prefixed
    // buffer).  flatbuffers' verifier already proves this — the manual
    // check below is just a paranoia floor.
    // No assert here; the verifier in deserialize() is authoritative.

    GameState round;
    CHECK(deserialize(buf.data(), buf.size(), round));
    CHECK(round == orig);
    std::printf("  in-memory round-trip OK  (%zu bytes)\n", buf.size());
}

void test_file_roundtrip() {
    GameState orig = build_fixture();
    const std::string path = "test_phase7.sav";

    CHECK(save_to_file(path, orig));

    GameState loaded;
    CHECK(load_from_file(path, loaded));
    CHECK(loaded == orig);
    std::printf("  on-disk round-trip OK  (%s)\n", path.c_str());

    // Best-effort cleanup; don't fail the test if the OS keeps the file.
    std::remove(path.c_str());
}

void test_rejects_garbage() {
    // Empty buffer
    GameState dummy;
    CHECK(!deserialize(nullptr, 0, dummy));

    // Random non-flatbuffer bytes
    const uint8_t garbage[] = {0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA,
                               0x99, 0x88, 0x77, 0x66, 0x55, 0x44};
    CHECK(!deserialize(garbage, sizeof(garbage), dummy));

    // Truncated buffer from a real save
    GameState orig = build_fixture();
    auto buf = serialize(orig);
    CHECK(!deserialize(buf.data(), buf.size() / 2, dummy));

    // load_from_file on a missing path
    GameState d2;
    CHECK(!load_from_file("does-not-exist-zzzz.sav", d2));

    std::printf("  garbage rejection OK\n");
}

void test_empty_state() {
    // All-default GameState (no units, no extras, no mission strings).
    // The schema's defaulted fields must survive round-trip.
    GameState empty;
    empty.timestamp_unix = 0;  // serialize() will auto-stamp -> non-deterministic
    auto buf = serialize(empty);
    CHECK(!buf.empty());

    GameState back;
    CHECK(deserialize(buf.data(), buf.size(), back));
    CHECK(back.schema_version == 1);
    CHECK(back.units.empty());
    CHECK(back.extras.empty());
    CHECK(back.mission.id.empty());
    // timestamp was auto-stamped, just confirm it's non-zero.
    CHECK(back.timestamp_unix != 0);

    std::printf("  empty-state round-trip OK\n");
}

} // namespace

int main() {
    std::printf("Phase 7 save round-trip test\n");
    test_in_memory_roundtrip();
    test_file_roundtrip();
    test_rejects_garbage();
    test_empty_state();
    std::printf("ALL PASS\n");
    return 0;
}
