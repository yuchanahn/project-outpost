#include "render/renderer.hpp"
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM w, LPARAM l) {
    return DefWindowProcW(window, message, w, l);
}
int main(int argc, char **argv) {
    try {
        bool instanced = argc > 1 && std::string(argv[1]) == "instanced";
        WNDCLASSW wc{};
        wc.lpfnWndProc = procedure;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"OutpostRenderBench";
        RegisterClassW(&wc);
        RECT rect{0, 0, 1280, 720};
        AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
        HWND window = CreateWindowW(wc.lpszClassName, L"Outpost fixed scene benchmark",
                                    WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, rect.right - rect.left,
                                    rect.bottom - rect.top, nullptr, nullptr, wc.hInstance, nullptr);
        if (!window)
            throw std::runtime_error("CreateWindow");
        outpost::Renderer renderer(window, instanced, true);
        outpost::World world;
        world.tick = 60;
        for (unsigned i = 0; i < 24; ++i)
            world.enemies.push_back(
                {i, {float(int(i % 6) - 3) * 4, float(int(i / 6) - 2) * 4}, 30, int(i % 2)});
        std::vector<double> cpu, gpu;
        unsigned draws = 0, invalid = 0;
        for (int frame = 0; frame < 620; ++frame) {
            MSG msg;
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            auto m = renderer.draw(world, 0);
            if (frame >= 120) {
                cpu.push_back(m.cpu_ms);
                if (m.gpu_valid)
                    gpu.push_back(m.gpu_ms);
                else
                    ++invalid;
                draws = m.draws;
            }
        }
        auto percentile = [](std::vector<double> values, double q) {
            std::sort(values.begin(), values.end());
            return values.empty() ? 0 : values[static_cast<size_t>((values.size() - 1) * q)];
        };
        std::cout << "RENDER mode=" << (instanced ? "instanced" : "baseline")
                  << " width=1280 height=720 samples=" << cpu.size() << " draws=" << draws
                  << " cpu_p50_ms=" << percentile(cpu, .5) << " cpu_p95_ms=" << percentile(cpu, .95)
                  << " gpu_p50_ms=" << percentile(gpu, .5) << " gpu_p95_ms=" << percentile(gpu, .95)
                  << " gpu_invalid=" << invalid << '\n';
        DestroyWindow(window);
        return invalid || cpu.size() != 500 ? 1 : 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
