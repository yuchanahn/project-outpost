#pragma once
#include "game/world.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <span>
#include <stdexcept>
namespace outpost::net {
constexpr unsigned short port = 27015;
template <int Id, int N> struct Packet {
    static constexpr int packet_id = Id;
    std::array<unsigned char, N> bytes{};
};
struct Control : Packet<10, 16> {
    static constexpr bool is_reliable = true;
};
using Command = Packet<11, 24>;
using Snapshot = Packet<12, 900>;
struct FinalSnapshot : Snapshot {
    static constexpr bool is_reliable = true;
};
static_assert(sizeof(Control) == 16 && sizeof(Command) == 24 && sizeof(Snapshot) == 900);
enum class Action : unsigned char { join = 1, start = 2, heartbeat = 3, quit = 4 };
struct Writer {
    std::span<unsigned char> bytes;
    size_t at{};
    void u32(std::uint32_t v) {
        for (int i = 0; i < 4; ++i) {
            if (at >= bytes.size())
                throw std::runtime_error("wire overflow");
            bytes[at++] = static_cast<unsigned char>(v >> (8 * i));
        }
    }
    void f32(float v) {
        u32(std::bit_cast<std::uint32_t>(v));
    }
};
struct Reader {
    std::span<const unsigned char> bytes;
    size_t at{};
    std::uint32_t u32() {
        std::uint32_t v{};
        for (int i = 0; i < 4; ++i) {
            if (at >= bytes.size())
                throw std::runtime_error("short packet");
            v |= std::uint32_t(bytes[at++]) << (8 * i);
        }
        return v;
    }
    float f32() {
        float v = std::bit_cast<float>(u32());
        if (!std::isfinite(v))
            throw std::runtime_error("nonfinite packet");
        return v;
    }
};
inline Control control(Action action) {
    Control p;
    Writer w{p.bytes};
    w.u32(1);
    w.u32(static_cast<unsigned>(action));
    return p;
}
inline Command command(const Input &in, std::uint32_t seq) {
    Command p;
    Writer w{p.bytes};
    w.u32(seq);
    w.f32(in.x);
    w.f32(in.z);
    w.f32(in.aim_x);
    w.f32(in.aim_z);
    w.u32(in.fire);
    return p;
}
inline Snapshot snapshot(const World &world, int slot) {
    Snapshot p;
    Writer w{p.bytes};
    w.u32(1);
    w.u32(world.tick);
    w.u32(slot < 0 ? 4 : slot);
    w.u32(static_cast<unsigned>(world.match.phase()));
    w.u32(static_cast<unsigned>(world.outcome));
    w.f32(world.core_hp);
    w.u32(world.wave);
    for (int i = 0; i < 4; ++i) {
        w.u32(static_cast<unsigned>(world.match.slots()[i].kind));
        w.f32(world.drones[i].position.x);
        w.f32(world.drones[i].position.z);
    }
    const auto n = (std::min)(world.enemies.size(), size_t{24});
    w.u32(static_cast<unsigned>(n));
    for (size_t i = 0; i < n; ++i) {
        const auto &e = world.enemies[i];
        w.u32(e.id);
        w.f32(e.position.x);
        w.f32(e.position.z);
        w.f32(e.hp);
        w.u32(e.kind);
    }
    return p;
}
inline void decode(const Snapshot &p, World &world, int &slot) {
    Reader r{p.bytes};
    if (r.u32() != 1)
        throw std::runtime_error("version");
    World next;
    next.tick = r.u32();
    auto s = r.u32(), phase = r.u32(), outcome = r.u32();
    if (s > 4 || phase > 2 || outcome > 3)
        throw std::runtime_error("state");
    next.outcome = static_cast<Outcome>(outcome);
    next.core_hp = r.f32();
    next.wave = static_cast<int>(r.u32());
    std::array<Slot, 4> slots;
    for (int i = 0; i < 4; ++i) {
        auto kind = r.u32();
        if (kind > 2)
            throw std::runtime_error("slot");
        slots[i] = {static_cast<SlotKind>(kind), kind == 1 ? std::uint64_t(i + 1) : 0};
        next.drones[i].position = {r.f32(), r.f32()};
    }
    next.match = Match::snapshot(static_cast<Phase>(phase), slots);
    auto n = r.u32();
    if (n > 24)
        throw std::runtime_error("enemy count");
    for (unsigned i = 0; i < n; ++i) {
        Enemy e;
        e.id = r.u32();
        e.position = {r.f32(), r.f32()};
        e.hp = r.f32();
        e.kind = static_cast<int>(r.u32());
        if (e.kind > 1)
            throw std::runtime_error("kind");
        next.enemies.push_back(e);
    }
    world = std::move(next);
    slot = s == 4 ? -1 : static_cast<int>(s);
}
} // namespace outpost::net
