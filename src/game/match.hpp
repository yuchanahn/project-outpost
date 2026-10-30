#pragma once
#include <array>
#include <cstdint>

namespace outpost {
enum class Phase { lobby, running, finished };
enum class SlotKind { empty, human, bot };
struct Slot {
    SlotKind kind{SlotKind::bot};
    std::uint64_t token{};
};

class Match {
  public:
    bool join(std::uint64_t token);
    bool start();
    bool leave(std::uint64_t token);
    int human_count() const;
    Phase phase() const {
        return phase_;
    }
    const std::array<Slot, 4> &slots() const {
        return slots_;
    }
    // Client presentation only; authoritative admission uses join/start/leave.
    static Match snapshot(Phase phase, const std::array<Slot, 4> &slots) {
        Match result;
        result.phase_ = phase;
        result.slots_ = slots;
        return result;
    }

  private:
    Phase phase_{Phase::lobby};
    std::array<Slot, 4> slots_{};
};
} // namespace outpost
