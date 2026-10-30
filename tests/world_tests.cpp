#include "game/world.hpp"
#include "net/protocol.hpp"
#include <cmath>
#include <iostream>
#include <limits>
using namespace outpost;
int main() {
    int failures = 0;
    auto check = [&](bool ok, const char *message) {
        if (!ok) {
            ++failures;
            std::cerr << message << '\n';
        }
    };
    World win;
    win.match.join(1);
    win.start();
    std::array<Input, 4> input{};
    for (int i = 0; i < 30000 && win.outcome == Outcome::none; ++i) {
        if (!win.enemies.empty()) {
            auto d = win.drones[0].position, e = win.enemies.front().position;
            float x = e.x - d.x, z = e.z - d.z, n = std::sqrt(x * x + z * z);
            if (n > 0.01f)
                input[0] = {n > 8 ? x / n : 0, n > 8 ? z / n : 0, x / n, z / n, true};
        }
        win.step(input, 1.f / 30);
    }
    check(win.outcome == Outcome::victory, "automated defense must win finite waves");
    check(win.wave == 3, "three waves");
    auto tick = win.tick;
    win.step(input, 1.f / 30);
    check(win.tick == tick, "terminal state frozen");
    win.leave(1);
    check(win.outcome == Outcome::victory, "departure preserves final victory");
    World lose;
    for (int i = 1; i <= 4; ++i)
        lose.match.join(i);
    lose.start();
    input = {};
    for (int i = 0; i < 30000 && lose.outcome == Outcome::none; ++i)
        lose.step(input, 1.f / 30);
    check(lose.outcome == Outcome::defeat && lose.core_hp == 0, "idle human team must lose");
    World leave;
    leave.match.join(1);
    leave.start();
    leave.step(input, 1.f / 30);
    check(leave.leave(1) && leave.outcome == Outcome::abandoned && leave.enemies.empty(),
          "abandon cleans enemies");
    World invalid;
    invalid.match.join(1);
    invalid.start();
    invalid.step(input, std::numeric_limits<float>::quiet_NaN());
    check(invalid.tick == 0, "reject nonfinite dt");
    auto p = net::snapshot(win, 0);
    World copy;
    int slot = -1;
    net::decode(p, copy, slot);
    check(slot == 0 && copy.tick == win.tick && copy.outcome == win.outcome && copy.core_hp == win.core_hp,
          "snapshot round trip");
    p.bytes[0] = 99;
    bool rejected = false;
    try {
        net::decode(p, copy, slot);
    } catch (...) {
        rejected = true;
    }
    check(rejected, "reject protocol version");
    std::cout << "world tests failures=" << failures << " win_tick=" << win.tick << " lose_tick=" << lose.tick
              << '\n';
    return failures ? 1 : 0;
}
