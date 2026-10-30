#pragma once
#include "game/world.hpp"
#include <memory>
struct HWND__;
namespace outpost {
class Renderer {
  public:
    struct Metrics {
        double cpu_ms{}, gpu_ms{};
        unsigned draws{};
        bool gpu_valid{};
    };
    explicit Renderer(HWND__ *window, bool instanced = false, bool measure = false);
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Metrics draw(const World &world, int local_slot);

  private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace outpost
