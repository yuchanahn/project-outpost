#include "game/world.hpp"
#include <algorithm>
#include <cmath>
namespace outpost {
namespace {
float length(Vec2 v) {
    return std::sqrt(v.x * v.x + v.z * v.z);
}
Vec2 toward(Vec2 a, Vec2 b) {
    Vec2 d{b.x - a.x, b.z - a.z};
    float n = length(d);
    return n > 0.001f ? Vec2{d.x / n, d.z / n} : Vec2{};
}
} // namespace
World::World() {
    for (int i = 0; i < 4; ++i)
        drones[i].position = {float((i % 2) * 6 - 3), float((i / 2) * 6 - 3)};
}
bool World::start() {
    return outcome == Outcome::none && match.start();
}
bool World::leave(std::uint64_t token) {
    bool changed = match.leave(token);
    if (changed && match.phase() == Phase::finished) {
        enemies.clear();
        if (outcome == Outcome::none)
            outcome = Outcome::abandoned;
    }
    return changed;
}
void World::step(const std::array<Input, 4> &inputs, float dt) {
    if (match.phase() != Phase::running || outcome != Outcome::none || !std::isfinite(dt) || dt <= 0 ||
        dt > 0.1f)
        return;
    ++tick;
    if (wave == 0) {
        wave = 1;
        spawn_clock = 0;
    }
    spawn_clock -= dt;
    const int total = 4 + wave * 2;
    if (spawned < total && spawn_clock <= 0 && enemies.size() < 24) {
        const auto id = next_enemy++;
        float angle = float(id) * 2.399963f;
        int kind = int(id % 2);
        enemies.push_back({id, {std::cos(angle) * 18, std::sin(angle) * 18}, kind ? 55.f : 30.f, kind});
        ++spawned;
        spawn_clock = 1.2f;
    }
    for (int i = 0; i < 4; ++i) {
        const auto kind = match.slots()[i].kind;
        if (kind == SlotKind::empty)
            continue;
        auto &drone = drones[i];
        Input in = inputs[i];
        if (kind == SlotKind::bot) {
            enum class BotState { defend, approach, fire };
            BotState state = BotState::defend;
            Enemy *target = nullptr;
            float nearest = 1e9f;
            for (auto &e : enemies) {
                float distance = length({e.position.x - drone.position.x, e.position.z - drone.position.z});
                if (e.hp > 0 && distance < nearest) {
                    nearest = distance;
                    target = &e;
                }
            }
            in = {};
            if (target) {
                auto aim = toward(drone.position, target->position);
                in.aim_x = aim.x;
                in.aim_z = aim.z;
                state = nearest > 9 ? BotState::approach : BotState::fire;
                if (state == BotState::approach) {
                    in.x = aim.x;
                    in.z = aim.z;
                }
                in.fire = true;
            } else if (length(drone.position) > 5) {
                auto move = toward(drone.position, {});
                in.x = move.x;
                in.z = move.z;
            }
        }
        if (!std::isfinite(in.x) || !std::isfinite(in.z) || !std::isfinite(in.aim_x) ||
            !std::isfinite(in.aim_z))
            in = {};
        Vec2 move{std::clamp(in.x, -1.f, 1.f), std::clamp(in.z, -1.f, 1.f)};
        float n = length(move);
        if (n > 1) {
            move.x /= n;
            move.z /= n;
        }
        drone.position.x = std::clamp(drone.position.x + move.x * 7 * dt, -18.f, 18.f);
        drone.position.z = std::clamp(drone.position.z + move.z * 7 * dt, -18.f, 18.f);
        drone.cooldown -= dt;
        if (in.fire && drone.cooldown <= 0) {
            auto aim = toward({}, {in.aim_x, in.aim_z});
            Enemy *hit = nullptr;
            float best = 12;
            for (auto &e : enemies) {
                Vec2 d{e.position.x - drone.position.x, e.position.z - drone.position.z};
                float dist = length(d);
                if (e.hp > 0 && dist < best && dist > 0.001f && (d.x * aim.x + d.z * aim.z) / dist > 0.94f) {
                    hit = &e;
                    best = dist;
                }
            }
            if (hit)
                hit->hp -= 20;
            drone.cooldown = 0.28f;
        }
    }
    for (auto &e : enemies) {
        if (e.hp <= 0)
            continue;
        auto move = toward(e.position, {});
        float speed = e.kind ? 1.4f : 2.5f;
        e.position.x += move.x * speed * dt;
        e.position.z += move.z * speed * dt;
        if (length(e.position) < 1.4f) {
            core_hp -= e.kind ? 20.f : 12.f;
            e.hp = 0;
        }
    }
    std::erase_if(enemies, [](const Enemy &e) { return e.hp <= 0; });
    if (core_hp <= 0) {
        core_hp = 0;
        outcome = Outcome::defeat;
        enemies.clear();
        return;
    }
    if (spawned == total && enemies.empty()) {
        if (wave == 3) {
            outcome = Outcome::victory;
            return;
        }
        ++wave;
        spawned = 0;
        spawn_clock = 2;
    }
}
} // namespace outpost
