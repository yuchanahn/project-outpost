#pragma once
#include "game/match.hpp"
#include <array>
#include <cstdint>
#include <vector>
namespace outpost {
struct Vec2 {
    float x{}, z{};
};
struct Input {
    float x{}, z{}, aim_x{1}, aim_z{};
    bool fire{};
};
struct Drone {
    Vec2 position{};
    float cooldown{};
};
struct Enemy {
    std::uint32_t id{};
    Vec2 position{};
    float hp{};
    int kind{};
};
enum class Outcome { none, victory, defeat, abandoned };
struct World {
    Match match;
    std::array<Drone, 4> drones{};
    std::vector<Enemy> enemies;
    float core_hp{100};
    int wave{};
    std::uint32_t tick{}, next_enemy{1};
    float spawn_clock{};
    int spawned{};
    Outcome outcome{Outcome::none};
    World();
    bool start();
    bool leave(std::uint64_t token);
    void step(const std::array<Input, 4> &inputs, float dt);
};
} // namespace outpost
