// Silent Storm port — save / load public API.
//
// Phase 7: thin C++ wrapper around the `save_v1.fbs` flatbuffers schema.
// This file is engine-agnostic — it knows nothing about Nival's object
// graph.  It defines a flat POD `GameState` struct that game code packs
// (on save) or consumes (on load), and the only I/O the wrapper performs
// is "read this file" / "write this file".
//
// Schema versioning lives in `save_v1.fbs` (root table has a
// `schema_version` field; the file is tagged with a 4-byte
// `file_identifier`, "SSV1").  v2 fields must be appended to the END of
// existing tables; flatbuffers' wire format then keeps v1 readers
// compatible with v2 files and vice versa.

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace silent_storm::save {

// --- Plain-old-data mirror of the .fbs root --------------------------------

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline bool operator==(const Vec3& a, const Vec3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

struct UnitState {
    uint32_t id = 0;
    std::string name;
    uint32_t archetype_id = 0;
    Vec3 position{};
    float rotation = 0.0f;
    int32_t hp = 0;
    int32_t hp_max = 0;
    int32_t ap = 0;
    int32_t ap_max = 0;
    uint8_t side = 0;
    std::vector<uint32_t> inventory_item_ids;
    std::vector<uint16_t> perks;
    uint32_t state_flags = 0;
};

inline bool operator==(const UnitState& a, const UnitState& b) {
    return a.id == b.id
        && a.name == b.name
        && a.archetype_id == b.archetype_id
        && a.position == b.position
        && a.rotation == b.rotation
        && a.hp == b.hp
        && a.hp_max == b.hp_max
        && a.ap == b.ap
        && a.ap_max == b.ap_max
        && a.side == b.side
        && a.inventory_item_ids == b.inventory_item_ids
        && a.perks == b.perks
        && a.state_flags == b.state_flags;
}

struct MissionState {
    std::string id;
    std::string map_path;
    uint32_t turn_counter = 0;
    uint8_t active_side = 0;
    std::vector<uint32_t> objectives_complete;
    std::vector<uint32_t> objectives_failed;
};

inline bool operator==(const MissionState& a, const MissionState& b) {
    return a.id == b.id
        && a.map_path == b.map_path
        && a.turn_counter == b.turn_counter
        && a.active_side == b.active_side
        && a.objectives_complete == b.objectives_complete
        && a.objectives_failed == b.objectives_failed;
}

struct KeyValue {
    std::string key;
    std::string value;
};

inline bool operator==(const KeyValue& a, const KeyValue& b) {
    return a.key == b.key && a.value == b.value;
}

struct GameState {
    uint32_t schema_version = 1;
    uint64_t timestamp_unix = 0;
    std::string slot_name;
    MissionState mission;
    std::vector<UnitState> units;
    uint64_t rng_seed = 0;
    std::vector<KeyValue> extras;
};

inline bool operator==(const GameState& a, const GameState& b) {
    return a.schema_version == b.schema_version
        && a.timestamp_unix == b.timestamp_unix
        && a.slot_name == b.slot_name
        && a.mission == b.mission
        && a.units == b.units
        && a.rng_seed == b.rng_seed
        && a.extras == b.extras;
}

// --- Public API ------------------------------------------------------------
//
// Both functions return `true` on success.  Failure modes:
//   - save_to_file: filesystem write error / serializer threw
//   - load_from_file: file missing, file_identifier wrong, Verify() failed,
//     buffer truncated.  `out_state` is left untouched on failure.
//
// `path` is interpreted as a host-OS path (UTF-8 on all platforms).
//
// On save, `timestamp_unix` is auto-stamped to the current wall clock if
// the caller leaves it at 0; otherwise the caller's value is kept (this
// keeps the round-trip test deterministic).

bool save_to_file(const std::string& path, const GameState& state);
bool load_from_file(const std::string& path, GameState& out_state);

// Same payloads, but operate against an in-memory byte buffer.  Useful
// for unit tests and for plumbing the save data through Nival's existing
// CFileStream-backed slot writer without ever touching the filesystem.

std::vector<uint8_t> serialize(const GameState& state);
bool deserialize(const uint8_t* data, size_t size, GameState& out_state);

} // namespace silent_storm::save
