# Integrated game evidence

Recorded 2026-10-30 KST. This supersedes the earlier lifecycle-only implementation status. The whole portfolio specification is still in progress.

## Implemented and observed

Dedicated authoritative server; lobby 1–4 humans with server FSM bots; manual start; two enemy kinds; movement, directional firing, core damage and three finite waves; deterministic victory and defeat tests. Running joins are denied, final departures leave vacancies, and the final human departure cleans match slots/enemies without terminating the shared transport service. Original external networking sources remain unmodified.

- Debug and Release builds succeeded with MSVC 19.41 and Windows SDK 10.0.19041.0. CTest `game.match_lifecycle` and `game.world_and_wire` passed in both configurations. Builds: `evidence/integration/build-debug.log`, `build-release.log`. C++23 selected-feature toolchain evidence remains in BUILD-001.
- Real server/two probes passed in both configurations. Release: `evidence/integration/release/`. Distinct slots, running snapshots and final server outcome 3 were observed.
- Two actual Release DX11 clients, each running the built-in 8-second smoke mode, exited 0. `graphics1.log`: slot 1, 72 snapshots, tick 176, core 100. `graphics2.log`: slot 0, 72 snapshots, tick 176, core 100. These are local execution observations, not performance measurements or byte-for-byte world equivalence proof.
- Actual GPU framebuffer readbacks `evidence/integration/gameplay1.bmp` and `gameplay2.bmp` were visually inspected: floor, central core, drones and enemies render; local drone color differs by assigned slot. A Windows capture path had returned white for a window despite rendering; framebuffer capture establishes rendered content but does not establish that capture-path issue is fixed.
- `tools/test-lifecycle.ps1` passed against Release. Four real UDP clients obtained unique slots; fifth lobby client and late in-progress client received no admitted snapshot. Explicit quit left a vacancy; unannounced socket destruction retained a human after 2 seconds and left a vacancy after an additional 4 seconds. Remaining humans continued. Final quits led to `SERVER_STOP tick=221 outcome=3`. Logs: `evidence/integration/lifecycle/`. This brackets the configured 5-second idle policy; it is not a precise timeout latency benchmark.
- World regression now checks victory remains victory after final departure.

## Reproduce

```powershell
cmake --build out/game --config Release
ctest --test-dir out/game -C Release --output-on-failure
./tools/integration.ps1 -Bin ./out/game/Release
./tools/test-lifecycle.ps1
```

Only one network test may own UDP 27015 at a time. Tests are bounded and clean up their owned processes. `tools/run-local.ps1 -Clients 2` launches interactive games and cleans its server after all client windows close. Direct server execution supports Ctrl+C.

## Authorship and independent review

Codex authored and verified the integrated implementation. Earlier Claude bridge implementation attempts returned no actual edits, so they are not credited with this code. Independent read-only review task `3c2356d3-3c33-43fe-aeac-485993b97b25` completed with findings. Its claimed pre-join quit-disconnect bug does not match the inspected current control handler: `if (!p.token) return` precedes start/quit handling. No patch was applied for that claim. Its GUI validation gap is addressed by the actual client executions and framebuffer captures above. The report does not independently prove runtime safety.

## Remaining gates

Separate-PC LAN; game-path loss/duplication/reordering injection; separate synthetic RUDP/RIO load benchmark with latency/CPU/queues; fixed-scene CPU/GPU frame timings and before/after optimization; video and publication/license/secret review. Current code does not address application sequence wrap, authenticated internet sessions, multiple hosted matches or automatic match restart. Upstream RIO shutdown/concurrency behavior has not been proven production-safe by these local tests.

The user handled the Windows firewall permission dialog; the agent did not click it or execute the prepared administrator firewall script. No remote publication has occurred. Source hashes in `evidence/integration/source-hashes.json` identify this local source state in the absence of a root repository commit.

## Follow-up measurements, 2026-10-30 KST

The previous remaining-gates paragraph describes the earlier checkpoint. Fault relay, 8/32/64-session real RUDP/RIO echo measurements, controlled DX11 instancing comparison, and full two-client victory now have raw evidence and reproducible scripts; see docs/measurements.md. Separate-PC LAN, deeper saturation/internal-RIO-queue measurements and publication remain open.

After instancing was enabled, two actual DX11 smoke clients again exited 0: slots 0/1, 72 snapshots each, ticks 175/179, core 100. One client's real GPU framebuffer was sampled approximately every 100ms and encoded with installed FFmpeg into evidence/demo/outpost.mp4: 71 frames, 7.1 seconds at 10 fps, 1084x722. Original client framebuffer was 1084x721; the encoder adds a single padding row to meet YUV420 dimensions. This footage shows the actual client scene with network-driven bots/enemies, not a mockup. It is resampled footage, not frame pacing evidence. Source frames and a mid-game frame were inspected; the encoder initially rejected odd height and was rerun with explicit padding.
