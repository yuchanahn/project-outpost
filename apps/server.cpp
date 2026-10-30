#include "net/protocol.hpp"
#include "ycnet/ycnet.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
using namespace outpost;
using Clock = std::chrono::steady_clock;
std::atomic_bool stopped{};
BOOL WINAPI stop_signal(DWORD) {
    stopped = true;
    return TRUE;
}
int main(int argc, char **argv) {
    const int duration = argc > 1 ? std::stoi(argv[1]) : 0;
    World world;
    std::array<Input, 4> inputs{};
    struct Peer {
        std::uint64_t token{};
        Clock::time_point seen{}, input_at{};
        std::uint32_t seq{};
        bool closing{}, result_sent{};
    };
    std::array<Peer, 16> peers{};
    std::uint64_t next_token = 1;
    auto slot_for = [&](std::uint64_t token) {
        for (int i = 0; i < 4; ++i)
            if (world.match.slots()[i].kind == SlotKind::human && world.match.slots()[i].token == token)
                return i;
        return -1;
    };
    bool startup_error = false;
    ycnet::rudp_server<16, 1, net::port> server([&](auto message) {
        startup_error = true;
        std::cerr << message << '\n';
    });
    server.set_connect_callback(
        [&](auto, size_t id) { peers[id] = {0, Clock::now(), Clock::now(), 0, false}; });
    server.set_failure_callback([&](auto, size_t id, auto) { peers[id].closing = true; });
    packet_events[10] = [&](void *body, packet_size_type size, size_t id) {
        if (size != sizeof(net::Control) + ycnet::packet::header_size)
            return;
        try {
            net::Reader r{{static_cast<unsigned char *>(body), sizeof(net::Control)}};
            if (r.u32() != 1)
                return;
            auto a = r.u32();
            if (a < 1 || a > 4)
                return;
            auto &p = peers[id];
            p.seen = Clock::now();
            if (a == 1 && !p.token) {
                auto token = next_token++;
                if (world.outcome == Outcome::none && world.match.join(token))
                    p.token = token;
                else
                    p.closing = true;
            }
            if (!p.token)
                return;
            if (a == 2)
                world.start();
            if (a == 4)
                p.closing = true;
        } catch (...) {
        }
    };
    packet_events[11] = [&](void *body, packet_size_type size, size_t id) {
        if (size != sizeof(net::Command) + ycnet::packet::header_size)
            return;
        auto &p = peers[id];
        int slot = slot_for(p.token);
        if (slot < 0)
            return;
        try {
            net::Reader r{{static_cast<unsigned char *>(body), sizeof(net::Command)}};
            auto seq = r.u32();
            Input in{r.f32(), r.f32(), r.f32(), r.f32(), r.u32() != 0};
            if (seq <= p.seq)
                return;
            if (std::abs(in.x) > 1 || std::abs(in.z) > 1 || std::abs(in.aim_x) > 1 || std::abs(in.aim_z) > 1)
                return;
            p.seq = seq;
            p.seen = p.input_at = Clock::now();
            inputs[slot] = in;
        } catch (...) {
        }
    };
    server.start_io();
    if (startup_error)
        return 1;
    SetConsoleCtrlHandler(stop_signal, TRUE);
    const auto begin = Clock::now();
    auto next_tick = begin, next_snapshot = begin;
    std::cout << "SERVER_READY port=" << net::port << std::endl;
    while (!stopped && (duration <= 0 || Clock::now() - begin < std::chrono::seconds(duration))) {
        server.process_received_packets();
        auto now = Clock::now();
        for (size_t id = 0; id < peers.size(); ++id) {
            auto &p = peers[id];
            if (p.token && now - p.seen > std::chrono::seconds(5))
                p.closing = true;
            if (p.closing) {
                int slot = slot_for(p.token);
                world.leave(p.token);
                if (slot >= 0)
                    inputs[slot] = {};
                p.token = 0;
                server.disconnect(id);
                p = {};
            }
            if (p.token && now - p.input_at > std::chrono::milliseconds(250)) {
                int slot = slot_for(p.token);
                if (slot >= 0)
                    inputs[slot] = {};
            }
        }
        int catchup = 0;
        while (now >= next_tick && catchup++ < 5) {
            world.step(inputs, 1.f / 30);
            next_tick += std::chrono::microseconds(33333);
        }
        if (catchup > 5)
            next_tick = now;
        if (now >= next_snapshot) {
            for (size_t id = 0; id < peers.size(); ++id)
                if (peers[id].token) {
                    auto p = net::snapshot(world, slot_for(peers[id].token));
                    if (world.outcome == Outcome::none)
                        server.try_send(p, id);
                    else if (!peers[id].result_sent) {
                        net::FinalSnapshot final;
                        final.bytes = p.bytes;
                        peers[id].result_sent = server.try_send(final, id) == ycnet::send_result::accepted;
                    }
                }
            next_snapshot = now + std::chrono::milliseconds(100);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto result = server.stop();
    std::cout << "SERVER_STOP tick=" << world.tick << " outcome=" << static_cast<int>(world.outcome)
              << std::endl;
    return result ? 0 : 1;
}
