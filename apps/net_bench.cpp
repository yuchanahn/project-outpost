#include "net/protocol.hpp"
#include "ycnet/ycnet.hpp"
#include <algorithm>
#include <chrono>
#include <deque>
#include <iostream>
#include <map>
#include <thread>
using Clock = std::chrono::steady_clock;
struct Echo {
    static constexpr unsigned short packet_id = 20;
    static constexpr bool is_reliable = true;
    std::array<unsigned char, 64> bytes{};
};
struct Quit {
    static constexpr unsigned short packet_id = 21;
    static constexpr bool is_reliable = true;
    std::array<unsigned char, 4> bytes{};
};
constexpr unsigned short port = 27017;
double cpu_seconds() {
    FILETIME created, exit, kernel, user;
    GetProcessTimes(GetCurrentProcess(), &created, &exit, &kernel, &user);
    auto value = [](FILETIME t) {
        return (static_cast<std::uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime;
    };
    return (value(kernel) + value(user)) / 1e7;
}
int main(int argc, char **argv) {
    try {
        const bool serving = argc > 1 && std::string(argv[1]) == "server";
        const auto begin = Clock::now();
        const double cpu = cpu_seconds();
        if (serving) {
            unsigned connected = 0, disconnected = 0, delivered = 0;
            size_t queue_max = 0;
            bool failed = false;
            std::array<bool, 128> closing{};
            std::array<std::deque<Echo>, 128> pending;
            ycnet::rudp_server<128, 2, port> server([&](auto msg) {
                std::cerr << msg << '\n';
                failed = true;
            });
            server.set_connect_callback([&](auto, size_t id) {
                ++connected;
                closing[id] = false;
                pending[id].clear();
            });
            server.set_failure_callback([&](auto, size_t id, auto) { closing[id] = true; });
            packet_events[20] = [&](void *body, packet_size_type size, size_t id) {
                if (size != sizeof(Echo) + ycnet::packet::header_size)
                    return;
                Echo e;
                std::memcpy(e.bytes.data(), body, e.bytes.size());
                if (pending[id].size() >= 64) {
                    failed = true;
                    closing[id] = true;
                    return;
                }
                pending[id].push_back(e);
                queue_max = std::max(queue_max, pending[id].size());
                ++delivered;
            };
            packet_events[21] = [&](void *, packet_size_type size, size_t id) {
                if (size == sizeof(Quit) + ycnet::packet::header_size)
                    closing[id] = true;
            };
            server.start_io();
            if (failed)
                return 1;
            while (Clock::now() - begin < std::chrono::seconds(10)) {
                server.process_received_packets();
                for (size_t id = 0; id < 128; ++id) {
                    if (closing[id]) {
                        server.disconnect(id);
                        pending[id].clear();
                        closing[id] = false;
                        ++disconnected;
                    }
                    while (!pending[id].empty() &&
                           server.try_send(pending[id].front(), id) == ycnet::send_result::accepted)
                        pending[id].pop_front();
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            bool stopped = static_cast<bool>(server.stop());
            std::cout << "SERVER clients=" << connected << " cleaned=" << disconnected
                      << " delivered=" << delivered << " app_queue_max=" << queue_max
                      << " cpu_s=" << cpu_seconds() - cpu << '\n';
            return !failed && stopped && connected == disconnected && delivered > 0 ? 0 : 1;
        }
        int count = argc > 1 ? std::stoi(argv[1]) : 8;
        int window = argc > 2 ? std::stoi(argv[2]) : 1;
        if (window < 1 || window > 32)
            throw std::runtime_error("window 1..32");
        if (count < 1 || count > 128)
            throw std::runtime_error("client count 1..128");
        WSADATA wsa{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa))
            return 1;
        struct Peer {
            SOCKET socket = INVALID_SOCKET;
            std::unique_ptr<ycnet::rudp_handler> handler;
            unsigned sequence{}, received{};
            std::map<unsigned, Clock::time_point> outstanding;
        };
        std::vector<Peer> peers(count);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = inet_addr("127.0.0.1");
        std::vector<double> latency;
        unsigned duplicates = 0;
        size_t max_pending = 0;
        for (auto &p : peers) {
            p.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            u_long nonblocking = 1;
            ioctlsocket(p.socket, FIONBIO, &nonblocking);
            auto *peer = &p;
            p.handler = std::make_unique<ycnet::rudp_handler>(
                [](auto) {},
                [&, peer](char *data, size_t n) {
                    sendto(peer->socket, data, static_cast<int>(n), 0, reinterpret_cast<sockaddr *>(&address),
                           sizeof(address));
                });
            p.handler->set_receive_callback([&, peer](ycnet::packet::raw_packet raw) {
                if (raw.id != 20 || raw.size != sizeof(Echo) + ycnet::packet::header_size)
                    return;
                outpost::net::Reader r{{reinterpret_cast<unsigned char *>(raw.body), 64}};
                auto sent = peer->outstanding.find(r.u32());
                if (sent == peer->outstanding.end()) {
                    ++duplicates;
                    return;
                }
                latency.push_back(
                    std::chrono::duration<double, std::milli>(Clock::now() - sent->second).count());
                peer->outstanding.erase(sent);
                ++peer->received;
            });
        }
        auto pump = [&] {
            for (auto &p : peers) {
                for (int i = 0; i < 256; ++i) {
                    char bytes[1024];
                    int n = recv(p.socket, bytes, sizeof(bytes), 0);
                    if (n <= 0)
                        break;
                    p.handler->on_receive(bytes, n);
                }
                p.handler->process_retransmissions();
                max_pending = std::max(max_pending, p.handler->metrics().pending_reliable);
            }
        };
        const auto run = Clock::now();
        while (Clock::now() - run < std::chrono::seconds(5)) {
            pump();
            for (auto &p : peers)
                while (p.outstanding.size() < static_cast<size_t>(window)) {
                    Echo e;
                    outpost::net::Writer w{e.bytes};
                    w.u32(p.sequence + 1);
                    if (p.handler->try_send(e) == ycnet::send_result::accepted) {
                        ++p.sequence;
                        p.outstanding.emplace(p.sequence, Clock::now());
                        max_pending = std::max(max_pending, p.handler->metrics().pending_reliable);
                    } else
                        break;
                }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        auto drain = Clock::now();
        while (Clock::now() - drain < std::chrono::seconds(1)) {
            pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        Quit quit;
        for (auto &p : peers)
            p.handler->try_send(quit);
        drain = Clock::now();
        while (Clock::now() - drain < std::chrono::milliseconds(500)) {
            pump();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        unsigned sent = 0, received = 0;
        std::uint64_t retries = 0;
        bool all = true;
        for (auto &p : peers) {
            sent += p.sequence;
            received += p.received;
            all &= p.received > 0;
            retries += p.handler->metrics().retransmissions;
            p.handler.reset();
            closesocket(p.socket);
        }
        WSACleanup();
        std::sort(latency.begin(), latency.end());
        auto percentile = [&](double q) {
            return latency.empty() ? 0 : latency[static_cast<size_t>((latency.size() - 1) * q)];
        };
        std::cout << "CLIENT clients=" << count << " window=" << window << " sent=" << sent
                  << " echoes=" << received << " duplicates=" << duplicates << " p50_ms=" << percentile(.5)
                  << " p95_ms=" << percentile(.95) << " p99_ms=" << percentile(.99)
                  << " pending_max=" << max_pending << " retries=" << retries
                  << " cpu_s=" << cpu_seconds() - cpu << " active_s=5 payload_bytes=64\n";
        return all && sent == received && duplicates == 0 ? 0 : 1;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
