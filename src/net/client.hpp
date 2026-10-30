#pragma once
#include "net/protocol.hpp"
#include "ycnet/ycnet.hpp"
#include <chrono>
#include <iostream>
namespace outpost::net {
class Client {
    SOCKET socket_{INVALID_SOCKET};
    sockaddr_in address_{};
    std::unique_ptr<ycnet::rudp_handler> handler_;
    std::chrono::steady_clock::time_point heartbeat_{}, received_{};
    std::uint32_t sequence_{};
    bool start_pending_{};

  public:
    World world;
    int slot{-1};
    unsigned snapshots{};
    bool closed{};
    explicit Client(const char *host, unsigned short endpoint_port = port) {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data))
            throw std::runtime_error("WSAStartup");
        socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socket_ == INVALID_SOCKET) {
            WSACleanup();
            throw std::runtime_error("socket");
        }
        address_.sin_family = AF_INET;
        address_.sin_port = htons(endpoint_port);
        if (InetPtonA(AF_INET, host, &address_.sin_addr) != 1) {
            closesocket(socket_);
            WSACleanup();
            throw std::runtime_error("Use IPv4 address");
        }
        u_long enabled = 1;
        ioctlsocket(socket_, FIONBIO, &enabled);
        handler_ = std::make_unique<ycnet::rudp_handler>([](auto) {},
                                                         [this](char *data, size_t n) {
                                                             sendto(socket_, data, static_cast<int>(n), 0,
                                                                    reinterpret_cast<sockaddr *>(&address_),
                                                                    sizeof(address_));
                                                         });
        handler_->set_failure_callback([this](auto) { closed = true; });
        handler_->set_receive_callback([this](ycnet::packet::raw_packet packet) {
            if (packet.id != 12 || packet.size != sizeof(Snapshot) + ycnet::packet::header_size)
                return;
            Snapshot p;
            std::memcpy(p.bytes.data(), packet.body, p.bytes.size());
            try {
                World next;
                int assigned;
                decode(p, next, assigned);
                if (snapshots && next.tick < world.tick)
                    return;
                world = std::move(next);
                slot = assigned;
                ++snapshots;
                received_ = std::chrono::steady_clock::now();
            } catch (...) {
            }
        });
        auto join = control(Action::join);
        handler_->try_send(join);
        heartbeat_ = std::chrono::steady_clock::now();
        received_ = heartbeat_;
    }
    ~Client() {
        handler_.reset();
        closesocket(socket_);
        WSACleanup();
    }
    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;
    void start() {
        start_pending_ = true;
    }
    void quit() {
        auto p = control(Action::quit);
        handler_->try_send(p);
    }
    void poll() {
        for (int i = 0; i < 256; ++i) {
            char bytes[1024];
            sockaddr_in from{};
            int length = sizeof(from);
            int n = recvfrom(socket_, bytes, sizeof(bytes), 0, reinterpret_cast<sockaddr *>(&from), &length);
            if (n == SOCKET_ERROR)
                break;
            if (n > 0 && from.sin_addr.s_addr == address_.sin_addr.s_addr &&
                from.sin_port == address_.sin_port)
                handler_->on_receive(bytes, n);
        }
        handler_->process_retransmissions();
        auto now = std::chrono::steady_clock::now();
        if (now - heartbeat_ >= std::chrono::seconds(1)) {
            auto p = control(Action::heartbeat);
            if (handler_->try_send(p) == ycnet::send_result::accepted)
                heartbeat_ = now;
        }
        if (start_pending_ && slot >= 0) {
            auto p = control(Action::start);
            if (handler_->try_send(p) == ycnet::send_result::accepted)
                start_pending_ = false;
        }
        if (world.outcome == Outcome::none && now - received_ > std::chrono::seconds(6))
            closed = true;
    }
    void input(const Input &in) {
        auto p = command(in, ++sequence_);
        handler_->try_send(p);
    }
};
} // namespace outpost::net
