// Silent Storm port — save / load implementation (Phase 7).
//
// Packs `silent_storm::save::GameState` to a flatbuffers `Save` table
// using the generated builder helpers in `save_v1_generated.h`, then
// writes the buffer to disk with a size prefix.  Load is the reverse:
// read the file in full, verify the buffer with the generated
// `Verify*Buffer` helper, then unpack each table back into the POD
// mirror types.
//
// The generated reader types live in `silent_storm::save::fb`
// (child namespace) so they don't collide with our identically-named
// POD mirrors in `silent_storm::save`.  In this TU `fbs::` is an
// alias for that generated namespace.
//
// Why size-prefixed?  Lets future tooling concat multiple save records
// (autosave history, replays) into one container without ambiguity.
// The on-disk file is just `<uint32-le size>` followed by the raw
// flatbuffer; readers can therefore stream multiple records out of one
// stream if we ever want to.

#include "save.h"
#include "save_v1_generated.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

namespace silent_storm::save {

namespace fbs = silent_storm::save::fb;

namespace {

// -- pack helpers -----------------------------------------------------------

flatbuffers::Offset<fbs::Vec3>
PackVec3(flatbuffers::FlatBufferBuilder& b, const Vec3& v) {
    fbs::Vec3Builder vb(b);
    vb.add_x(v.x);
    vb.add_y(v.y);
    vb.add_z(v.z);
    return vb.Finish();
}

flatbuffers::Offset<fbs::Unit>
PackUnit(flatbuffers::FlatBufferBuilder& b, const UnitState& u) {
    auto name_off = u.name.empty() ? flatbuffers::Offset<flatbuffers::String>(0)
                                   : b.CreateString(u.name);
    auto pos_off = PackVec3(b, u.position);
    auto inv_off = u.inventory_item_ids.empty()
                       ? flatbuffers::Offset<flatbuffers::Vector<uint32_t>>(0)
                       : b.CreateVector(u.inventory_item_ids);
    auto perks_off = u.perks.empty()
                         ? flatbuffers::Offset<flatbuffers::Vector<uint16_t>>(0)
                         : b.CreateVector(u.perks);

    fbs::UnitBuilder ub(b);
    ub.add_id(u.id);
    ub.add_name(name_off);
    ub.add_archetype_id(u.archetype_id);
    ub.add_position(pos_off);
    ub.add_rotation(u.rotation);
    ub.add_hp(u.hp);
    ub.add_hp_max(u.hp_max);
    ub.add_ap(u.ap);
    ub.add_ap_max(u.ap_max);
    ub.add_side(u.side);
    ub.add_inventory_item_ids(inv_off);
    ub.add_perks(perks_off);
    ub.add_state_flags(u.state_flags);
    return ub.Finish();
}

flatbuffers::Offset<fbs::Mission>
PackMission(flatbuffers::FlatBufferBuilder& b, const MissionState& m) {
    auto id_off = m.id.empty() ? flatbuffers::Offset<flatbuffers::String>(0)
                               : b.CreateString(m.id);
    auto map_off = m.map_path.empty()
                       ? flatbuffers::Offset<flatbuffers::String>(0)
                       : b.CreateString(m.map_path);
    auto oc_off = m.objectives_complete.empty()
                      ? flatbuffers::Offset<flatbuffers::Vector<uint32_t>>(0)
                      : b.CreateVector(m.objectives_complete);
    auto of_off = m.objectives_failed.empty()
                      ? flatbuffers::Offset<flatbuffers::Vector<uint32_t>>(0)
                      : b.CreateVector(m.objectives_failed);

    fbs::MissionBuilder mb(b);
    mb.add_id(id_off);
    mb.add_map_path(map_off);
    mb.add_turn_counter(m.turn_counter);
    mb.add_active_side(m.active_side);
    mb.add_objectives_complete(oc_off);
    mb.add_objectives_failed(of_off);
    return mb.Finish();
}

flatbuffers::Offset<fbs::KeyValue>
PackKV(flatbuffers::FlatBufferBuilder& b, const KeyValue& kv) {
    auto k = kv.key.empty() ? flatbuffers::Offset<flatbuffers::String>(0)
                            : b.CreateString(kv.key);
    auto v = kv.value.empty() ? flatbuffers::Offset<flatbuffers::String>(0)
                              : b.CreateString(kv.value);
    fbs::KeyValueBuilder kb(b);
    kb.add_key(k);
    kb.add_value(v);
    return kb.Finish();
}

// -- unpack helpers ---------------------------------------------------------

Vec3 UnpackVec3(const fbs::Vec3* v) {
    Vec3 out;
    if (v) {
        out.x = v->x();
        out.y = v->y();
        out.z = v->z();
    }
    return out;
}

UnitState UnpackUnit(const fbs::Unit* u) {
    UnitState out;
    if (!u) return out;
    out.id = u->id();
    if (auto s = u->name()) out.name = s->str();
    out.archetype_id = u->archetype_id();
    out.position = UnpackVec3(u->position());
    out.rotation = u->rotation();
    out.hp = u->hp();
    out.hp_max = u->hp_max();
    out.ap = u->ap();
    out.ap_max = u->ap_max();
    out.side = u->side();
    if (auto inv = u->inventory_item_ids()) {
        out.inventory_item_ids.assign(inv->begin(), inv->end());
    }
    if (auto perks = u->perks()) {
        out.perks.assign(perks->begin(), perks->end());
    }
    out.state_flags = u->state_flags();
    return out;
}

MissionState UnpackMission(const fbs::Mission* m) {
    MissionState out;
    if (!m) return out;
    if (auto s = m->id()) out.id = s->str();
    if (auto s = m->map_path()) out.map_path = s->str();
    out.turn_counter = m->turn_counter();
    out.active_side = m->active_side();
    if (auto v = m->objectives_complete()) {
        out.objectives_complete.assign(v->begin(), v->end());
    }
    if (auto v = m->objectives_failed()) {
        out.objectives_failed.assign(v->begin(), v->end());
    }
    return out;
}

uint64_t now_unix() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
}

} // namespace

// ---------------------------------------------------------------------------

std::vector<uint8_t> serialize(const GameState& state) {
    flatbuffers::FlatBufferBuilder b(1024);

    auto slot_off = state.slot_name.empty()
                        ? flatbuffers::Offset<flatbuffers::String>(0)
                        : b.CreateString(state.slot_name);
    auto mission_off = PackMission(b, state.mission);

    std::vector<flatbuffers::Offset<fbs::Unit>> unit_offsets;
    unit_offsets.reserve(state.units.size());
    for (const auto& u : state.units) {
        unit_offsets.push_back(PackUnit(b, u));
    }
    auto units_off =
        unit_offsets.empty()
            ? flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<fbs::Unit>>>(0)
            : b.CreateVector(unit_offsets);

    std::vector<flatbuffers::Offset<fbs::KeyValue>> kv_offsets;
    kv_offsets.reserve(state.extras.size());
    for (const auto& kv : state.extras) {
        kv_offsets.push_back(PackKV(b, kv));
    }
    auto extras_off =
        kv_offsets.empty()
            ? flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<fbs::KeyValue>>>(0)
            : b.CreateVector(kv_offsets);

    fbs::SaveBuilder sb(b);
    sb.add_schema_version(state.schema_version ? state.schema_version : 1);
    sb.add_timestamp_unix(state.timestamp_unix ? state.timestamp_unix : now_unix());
    sb.add_slot_name(slot_off);
    sb.add_mission(mission_off);
    sb.add_units(units_off);
    sb.add_rng_seed(state.rng_seed);
    sb.add_extras(extras_off);
    auto root = sb.Finish();

    // FinishSizePrefixed embeds a 4-byte little-endian size at the front
    // and the "SSV1" file identifier right after.  Buffer is then ready
    // to stream / concatenate / hand to GetSizePrefixedSave.
    b.FinishSizePrefixed(root, fbs::SaveIdentifier());

    const uint8_t* p = b.GetBufferPointer();
    return std::vector<uint8_t>(p, p + b.GetSize());
}

bool deserialize(const uint8_t* data, size_t size, GameState& out_state) {
    if (!data || size < sizeof(uint32_t) + 4) return false;

    // Reject anything that isn't a size-prefixed buffer carrying our id.
    flatbuffers::Verifier verifier(data, size);
    if (!verifier.VerifySizePrefixedBuffer<fbs::Save>(fbs::SaveIdentifier())) {
        return false;
    }

    auto root = fbs::GetSizePrefixedSave(data);
    if (!root) return false;

    GameState out;
    out.schema_version = root->schema_version();
    out.timestamp_unix = root->timestamp_unix();
    if (auto s = root->slot_name()) out.slot_name = s->str();
    out.mission = UnpackMission(root->mission());
    if (auto v = root->units()) {
        out.units.reserve(v->size());
        for (auto u : *v) {
            out.units.push_back(UnpackUnit(u));
        }
    }
    out.rng_seed = root->rng_seed();
    if (auto v = root->extras()) {
        out.extras.reserve(v->size());
        for (auto kv : *v) {
            KeyValue out_kv;
            if (kv) {
                if (auto k = kv->key()) out_kv.key = k->str();
                if (auto vv = kv->value()) out_kv.value = vv->str();
            }
            out.extras.push_back(std::move(out_kv));
        }
    }

    out_state = std::move(out);
    return true;
}

bool save_to_file(const std::string& path, const GameState& state) {
    auto buf = serialize(state);
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(buf.data()),
            static_cast<std::streamsize>(buf.size()));
    return static_cast<bool>(f);
}

bool load_from_file(const std::string& path, GameState& out_state) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const auto sz = static_cast<std::streamsize>(f.tellg());
    if (sz <= 0) return false;
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    if (!f.read(reinterpret_cast<char*>(buf.data()), sz)) return false;
    return deserialize(buf.data(), buf.size(), out_state);
}

} // namespace silent_storm::save
