# Project Outpost

[Windows x64 preview](https://github.com/yuchanahn/project-outpost/releases/tag/v0.1.0-preview) · [MIT license](LICENSE) · [Measurements](docs/measurements.md)

Windows C++23 / DX11 cooperative drone defense prototype. A dedicated authoritative server runs four team slots, server FSM bots, two enemy types and three finite waves. Humans join in the lobby and press Enter to start. Wave completion wins; core HP zero loses. Running matches reject new joins; departed humans leave vacant slots, and the last human departure ends the match.

## Build and run

Requires Visual Studio 2022 Desktop C++ tools, Windows SDK, CMake, and the canonical dependency checkout at `external/ycnet-rio-rudp` (commit `f7c734298078bb8fcac2fb471355239ab08186ef`). Its source is unmodified. If missing, clone https://github.com/yuchanahn/ycnet-rio-rudp into that directory and check out the pinned commit.

```powershell
cmake -S . -B out/game -G 'Visual Studio 17 2022' -A x64
cmake --build out/game --config Release
ctest --test-dir out/game -C Release --output-on-failure
./tools/run-local.ps1 -Clients 2
```

Close all game windows to stop the launcher-owned server. Keep the launcher running while playing. For direct execution, run `./out/game/Release/outpost_server.exe` in one terminal and `./out/game/Release/outpost_client.exe 127.0.0.1` in another. Ctrl+C stops the direct server. A numeric argument to the server limits its runtime in seconds; default 0 runs indefinitely. Restart the server for a new match.

WASD moves, arrow keys aim, Space fires, Enter starts, Escape exits. Cyan is the local drone; blue drones are teammates; red/orange are enemies; the center is the core. The window title shows wave, core and outcome (0 playing, 1 victory, 2 defeat, 3 abandoned). Rendering uses simple procedural cubes and no downloaded assets.

For LAN, run the server on one PC and pass its private IPv4 address to clients on the same LAN. UDP 27015 must be reachable. [Firewall setup](docs/development-firewall.md). Separate-PC LAN validation remains pending.

## Verification

```powershell
./tools/integration.ps1 -Bin ./out/game/Release
./tools/test-lifecycle.ps1
```

The first runs a server and two probes. The second runs four socket clients and checks capacity, distinct slots, manual start, late-join rejection, explicit quit, application idle timeout and vacant slots; the server log must report final abandonment. Tests own and clean up only their launched processes. Do not run network scenarios concurrently on port 27015.

Debug/Release game tests, local socket integration, and two actual DX11 client processes have passed. [Evidence](project/tracks/GAME-001/evidence.md) includes commands and limitations. [Architecture](docs/architecture.md) describes ownership and wire format. The implementation uses MSVC /std:c++latest with selected feature checks; this is not a claim of complete final C++23 conformance.

Separate-PC LAN remains untested because no second PC is currently available. Source and the preview release are public under MIT. Deeper transport saturation and internal RIO queue measurement remain follow-ups. Current results and their limits are recorded below; the full specification is not marked complete.

## Measurements and demo

[Measured results and limitations](docs/measurements.md) include actual UDP fault injection, 8/32/64 synthetic transport sessions, a full two-client victory, and baseline/instanced DX11 CPU/GPU timing.

```powershell
./tools/test-faults.ps1 -Python 'path/to/python.exe'
./tools/test-result.ps1
./tools/measure.ps1 -Kind Load
./tools/measure.ps1 -Kind Render
```

Run network scripts sequentially. Render measurements open a fixed-size window and verify identical baseline/instanced framebuffer hashes. The normal client uses the measured instanced path. [Dependency and publication status](docs/dependencies.md).

[Actual two-client demo](evidence/demo/outpost.mp4): a 7.1-second framebuffer recording sampled near 10fps. It demonstrates the current prototype, not frame pacing or performance. Reproduction and timing caveats are in the evidence record.


LAN handoff: [second-PC checklist](docs/lan-check.md). `tools/package.ps1` produces a local ZIP under out/dist with the three game executables and per-file hashes. `tools/record-demo.ps1` reproduces the framebuffer video with the installed FFmpeg executable.

