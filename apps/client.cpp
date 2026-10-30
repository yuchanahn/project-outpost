#include "net/client.hpp"
#include "render/renderer.hpp"
#include <Windows.h>
#include <deque>
#include <thread>
using namespace outpost;
bool start_requested{};
LRESULT CALLBACK window_proc(HWND w, UINT m, WPARAM p, LPARAM l) {
    if (m == WM_KEYDOWN && p == VK_RETURN)
        start_requested = true;
    if (m == WM_KEYDOWN && p == VK_ESCAPE) {
        PostQuitMessage(0);
        return 0;
    }
    if (m == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, p, l);
}
int main(int argc, char **argv) {
    try {
        net::Client client(argc > 1 ? argv[1] : "127.0.0.1");
        const bool smoke = argc > 2 && std::string(argv[2]) == "smoke";
        const auto smoke_begin = std::chrono::steady_clock::now();
        bool smoke_started = false;
        WNDCLASSW wc{};
        wc.lpfnWndProc = window_proc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"Outpost";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassW(&wc);
        HWND window =
            CreateWindowW(wc.lpszClassName, L"Outpost", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                          CW_USEDEFAULT, 1100, 760, nullptr, nullptr, wc.hInstance, nullptr);
        if (!window)
            throw std::runtime_error("CreateWindow");
        Renderer renderer(window, true);
        auto next_input = std::chrono::steady_clock::now(), title_at = next_input;
        MSG msg{};
        bool running = true;
        unsigned last_snapshot{};
        using Clock = std::chrono::steady_clock;
        std::deque<std::pair<Clock::time_point, World>> frames;
        while (running) {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT)
                    running = false;
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            if (!running)
                break;
            client.poll();
            if (start_requested) {
                client.start();
                start_requested = false;
            }
            auto now = Clock::now();
            if (smoke && !smoke_started && client.snapshots && now - smoke_begin > std::chrono::seconds(2)) {
                client.start();
                smoke_started = true;
            }
            if (smoke && now - smoke_begin > std::chrono::seconds(8))
                running = false;
            if (last_snapshot != client.snapshots) {
                frames.emplace_back(now, client.world);
                last_snapshot = client.snapshots;
                while (frames.size() > 8)
                    frames.pop_front();
            }
            if (now >= next_input) {
                Input in{};
                if (GetForegroundWindow() == window) {
                    in.x = float(bool(GetAsyncKeyState('D') & 0x8000)) -
                           float(bool(GetAsyncKeyState('A') & 0x8000));
                    in.z = float(bool(GetAsyncKeyState('S') & 0x8000)) -
                           float(bool(GetAsyncKeyState('W') & 0x8000));
                    // Arrow keys aim in world axes; Space fires. Mouse is not interpreted as world
                    // coordinates.
                    in.aim_x = float(bool(GetAsyncKeyState(VK_RIGHT) & 0x8000)) -
                               float(bool(GetAsyncKeyState(VK_LEFT) & 0x8000));
                    in.aim_z = float(bool(GetAsyncKeyState(VK_DOWN) & 0x8000)) -
                               float(bool(GetAsyncKeyState(VK_UP) & 0x8000));
                    if (in.aim_x == 0 && in.aim_z == 0)
                        in.aim_z = -1;
                    in.fire = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
                    if (GetAsyncKeyState(VK_RETURN) & 0x8000)
                        client.start();
                    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000)
                        running = false;
                }
                client.input(in);
                next_input = now + std::chrono::milliseconds(33);
            }
            World view = client.world;
            auto target = now - std::chrono::milliseconds(100);
            while (frames.size() > 2 && frames[1].first <= target)
                frames.pop_front();
            if (frames.size() >= 2) {
                const auto &a = frames[0];
                const auto &b = frames[1];
                float t = std::clamp(std::chrono::duration<float>(target - a.first).count() /
                                         std::chrono::duration<float>(b.first - a.first).count(),
                                     0.f, 1.f);
                view = b.second;
                for (int i = 0; i < 4; ++i) {
                    view.drones[i].position.x =
                        a.second.drones[i].position.x +
                        (b.second.drones[i].position.x - a.second.drones[i].position.x) * t;
                    view.drones[i].position.z =
                        a.second.drones[i].position.z +
                        (b.second.drones[i].position.z - a.second.drones[i].position.z) * t;
                }
                for (auto &e : view.enemies)
                    for (const auto &old : a.second.enemies)
                        if (e.id == old.id) {
                            e.position.x = old.position.x + (e.position.x - old.position.x) * t;
                            e.position.z = old.position.z + (e.position.z - old.position.z) * t;
                            break;
                        }
            }
            renderer.draw(view, client.slot);
            if (now >= title_at) {
                std::string title =
                    "Outpost | WASD move / arrows aim / Space fire / Enter start | slot " +
                    std::to_string(client.slot + 1) + " | wave " + std::to_string(client.world.wave) +
                    " | core " + std::to_string(int(client.world.core_hp)) + " | outcome " +
                    std::to_string(int(client.world.outcome)) + (client.closed ? " | DISCONNECTED" : "");
                SetWindowTextA(window, title.c_str());
                title_at = now + std::chrono::milliseconds(200);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        client.quit();
        std::cout << "CLIENT slot=" << client.slot << " snapshots=" << client.snapshots
                  << " tick=" << client.world.tick << " core=" << client.world.core_hp << std::endl;
        for (int i = 0; i < 10; ++i) {
            client.poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        DestroyWindow(window);
        return smoke && (client.snapshots < 10 || client.world.tick == 0) ? 1 : 0;
    } catch (const std::exception &e) {
        MessageBoxA(nullptr, e.what(), "Outpost", MB_ICONERROR);
        return 1;
    }
}
