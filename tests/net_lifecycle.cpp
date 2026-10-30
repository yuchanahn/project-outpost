#include "net/client.hpp"
#include <thread>
using namespace outpost;
using Clock = std::chrono::steady_clock;
int main() {
    try {
        std::array<std::unique_ptr<net::Client>, 4> clients;
        for (auto &c : clients)
            c = std::make_unique<net::Client>("127.0.0.1");
        auto pump = [&](auto duration, net::Client *extra = nullptr) {
            auto until = Clock::now() + duration;
            while (Clock::now() < until) {
                for (auto &c : clients)
                    if (c)
                        c->poll();
                if (extra)
                    extra->poll();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        };
        auto require = [](bool ok, const char *message) {
            if (!ok)
                throw std::runtime_error(message);
        };
        pump(std::chrono::seconds(1));
        unsigned slots = 0;
        for (auto &c : clients) {
            require(c->snapshots && c->slot >= 0 && c->slot < 4, "four clients admitted");
            slots |= 1u << c->slot;
        }
        require(slots == 15, "unique slots");
        {
            net::Client fifth("127.0.0.1");
            pump(std::chrono::milliseconds(500), &fifth);
            require(fifth.snapshots == 0, "fifth lobby client rejected");
        }
        clients[0]->start();
        pump(std::chrono::milliseconds(500));
        require(clients[0]->world.match.phase() == Phase::running, "manual start");
        int departed = clients[1]->slot;
        clients[1]->quit();
        pump(std::chrono::milliseconds(300));
        clients[1].reset();
        require(clients[0]->world.match.slots()[departed].kind == SlotKind::empty, "quit leaves vacancy");
        {
            net::Client late("127.0.0.1");
            pump(std::chrono::milliseconds(500), &late);
            require(late.snapshots == 0, "in-progress join rejected");
        }
        int lost = clients[2]->slot;
        clients[2].reset(); // Deliberately omit quit to exercise application idle timeout.
        pump(std::chrono::seconds(2));
        require(clients[0]->world.match.slots()[lost].kind == SlotKind::human, "no premature departure");
        pump(std::chrono::seconds(4));
        require(clients[0]->world.match.slots()[lost].kind == SlotKind::empty, "idle timeout leaves vacancy");
        require(clients[0]->world.match.phase() == Phase::running, "remaining humans continue");
        for (auto &c : clients)
            if (c)
                c->quit();
        pump(std::chrono::milliseconds(200));
        std::cout
            << "Lifecycle PASS: capacity, unique slots, manual start, late join, quit, timeout, vacancies\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
