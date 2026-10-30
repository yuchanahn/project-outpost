#include "game/match.hpp"
#include <algorithm>
#include <iostream>

using namespace outpost;
int failures{};
void check(bool ok, const char *name) {
    if (!ok) {
        std::cerr << "FAIL: " << name << '\n';
        ++failures;
    }
}
int count(const Match &m, SlotKind kind) {
    return static_cast<int>(
        std::count_if(m.slots().begin(), m.slots().end(), [kind](const Slot &s) { return s.kind == kind; }));
}
int main() {
    Match lobby;
    check(!lobby.start() && count(lobby, SlotKind::bot) == 4, "empty-human lobby cannot start");
    check(!lobby.join(0) && !lobby.leave(0) && !lobby.leave(99), "invalid and unknown tokens");
    check(lobby.join(1) && !lobby.join(1), "duplicate join rejected");
    check(lobby.leave(1) && !lobby.leave(1), "lobby removal idempotent");
    check(lobby.phase() == Phase::lobby && count(lobby, SlotKind::bot) == 4 && !lobby.start(),
          "lobby remains reusable");
    check(lobby.join(1), "lobby rejoin allowed");
    for (int humans = 1; humans <= 4; ++humans) {
        Match match;
        for (int i = 1; i <= humans; ++i)
            check(match.join(i), "human admitted");
        check(match.human_count() == humans && count(match, SlotKind::bot) == 4 - humans,
              "four slots occupied");
        if (humans == 4)
            check(!match.join(5), "fifth human rejected");
        check(match.start() && !match.start(), "start once");
        check(!match.join(99) && !match.leave(99), "running admission and unknown leave rejected");
        for (int i = 1; i <= humans; ++i) {
            check(match.leave(i) && !match.leave(i) && !match.join(i), "leave once, no rejoin");
            if (i < humans) {
                check(match.phase() == Phase::running && count(match, SlotKind::empty) == i &&
                          count(match, SlotKind::bot) == 4 - humans,
                      "vacancies stay empty, existing bots remain");
            }
        }
        check(match.phase() == Phase::finished && count(match, SlotKind::empty) == 4,
              "last human ends match and clears bots");
        check(!match.start() && !match.join(123), "finished match cannot restart");
        for (const auto &slot : match.slots())
            check(slot.token == 0, "finished tokens cleared");
        check(lobby.phase() == Phase::lobby && lobby.human_count() == 1, "other match unaffected");
    }
    if (!failures)
        std::cout << "Match lifecycle checks passed\n";
    return failures ? 1 : 0;
}
