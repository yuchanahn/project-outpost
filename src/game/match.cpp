#include "game/match.hpp"
#include <algorithm>

namespace outpost {
int Match::human_count() const {
    return static_cast<int>(
        std::count_if(slots_.begin(), slots_.end(), [](const Slot &s) { return s.kind == SlotKind::human; }));
}

bool Match::join(std::uint64_t token) {
    if (phase_ != Phase::lobby || token == 0)
        return false;
    for (const auto &s : slots_)
        if (s.kind == SlotKind::human && s.token == token)
            return false;
    for (auto &s : slots_) {
        if (s.kind != SlotKind::human) {
            s = {SlotKind::human, token};
            return true;
        }
    }
    return false;
}

bool Match::start() {
    if (phase_ != Phase::lobby || human_count() == 0)
        return false;
    phase_ = Phase::running;
    return true;
}

bool Match::leave(std::uint64_t token) {
    if (phase_ == Phase::finished || token == 0)
        return false;
    for (auto &s : slots_) {
        if (s.kind != SlotKind::human || s.token != token)
            continue;
        s = {phase_ == Phase::lobby ? SlotKind::bot : SlotKind::empty, 0};
        if (phase_ == Phase::running && human_count() == 0) {
            phase_ = Phase::finished;
            slots_.fill({SlotKind::empty, 0});
        }
        return true;
    }
    return false;
}
} // namespace outpost
