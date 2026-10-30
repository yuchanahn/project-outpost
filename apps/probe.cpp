#include "net/client.hpp"
#include <thread>
int main(int argc, char **argv) {
    try {
        outpost::net::Client client(argc > 1 ? argv[1] : "127.0.0.1",
                                    argc > 3 ? static_cast<unsigned short>(std::stoi(argv[3]))
                                             : outpost::net::port);
        const bool defend = argc > 2 && std::string(argv[2]) == "defend";
        const bool defeat = argc > 2 && std::string(argv[2]) == "defeat";
        const bool terminal = defend || defeat;
        const bool start = terminal || (argc > 2 && std::string(argv[2]) == "start");
        auto begin = std::chrono::steady_clock::now();
        bool requested = false;
        unsigned running{};
        while (std::chrono::steady_clock::now() - begin < std::chrono::seconds(terminal ? 90 : 8) &&
               !client.closed) {
            client.poll();
            if (start && !requested && client.snapshots &&
                std::chrono::steady_clock::now() - begin > std::chrono::seconds(2)) {
                client.start();
                requested = true;
            }
            outpost::Input input{};
            if (defend && client.slot >= 0 && !client.world.enemies.empty()) {
                auto d = client.world.drones[client.slot].position;
                auto e = client.world.enemies.front().position;
                float x = e.x - d.x, z = e.z - d.z, n = std::sqrt(x * x + z * z);
                if (n > .01f)
                    input = {n > 8 ? x / n : 0, n > 8 ? z / n : 0, x / n, z / n, true};
            }
            client.input(input);
            if (client.world.match.phase() == outpost::Phase::running)
                ++running;
            if (terminal && client.world.outcome != outpost::Outcome::none)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
        std::cout << "PROBE slot=" << client.slot << " snapshots=" << client.snapshots
                  << " tick=" << client.world.tick << " running=" << running
                  << " core=" << client.world.core_hp << " outcome=" << int(client.world.outcome)
                  << std::endl;
        client.quit();
        for (int i = 0; i < 10; ++i) {
            client.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return client.snapshots > 10 && running > 0 &&
                       (!terminal || client.world.outcome ==
                                         (defeat ? outpost::Outcome::defeat : outpost::Outcome::victory))
                   ? 0
                   : 1;
    } catch (const std::exception &e) {
        std::cerr << e.what();
        return 2;
    }
}
