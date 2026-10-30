# Architecture — integrated prototype

Baseline: ycnet-rio-rudp f7c734298078bb8fcac2fb471355239ab08186ef, unmodified. Approved policies: LOCAL-003 and DEC-011 through DEC-020. Keep concrete ownership and ordinary structs; no ECS, abstract transport hierarchy, service locator or event framework.

## Ownership

- `src/game`: Match owns four slots and admission/lifecycle; World owns simulation, drones, enemies, waves and outcome. No Windows or transport dependencies.
- `apps/server.cpp`: one World, per-slot inputs and 16 transport peer records. The main thread pumps application callbacks, deferred disconnects, 30 Hz fixed simulation and 10 Hz snapshots. Original RIO workers perform transport I/O. Application tokens increase independently of reusable transport IDs.
- `src/net/protocol.hpp`: bounded byte arrays with explicit little-endian fields, version checks, finite floats and enum/count validation. No native World serialization.
- `src/net/client.hpp`: concrete socket/handler owner; callback payload is copied before returning. Main-thread poll drives transport, snapshots and 1-second heartbeat.
- `apps/client.cpp`: input and 100 ms remote interpolation, without prediction. `src/render`: concrete DX11 renderer owns COM resources, resize/depth targets and one cube mesh. Hardware device with WARP fallback.

Callbacks mark disconnects; handler destruction/disconnect occurs after receive pumping. Input older than 250 ms becomes idle. Five seconds without valid application activity finalizes departure; the library's separate transport progress timeout remains unchanged. Running departures remain empty; last-human departure clears match slots and enemies. Victory/defeat outcomes survive later departures. Match phase and game outcome are separate; outcome freezes simulation and prevents admission. Restart the server to host another match.

## Wire contract

Existing library transport framing and three-byte packet header are preserved. Application packet IDs: 10 control (16 bytes, reliable), 11 command (24 bytes, unreliable), 12 snapshot (900 bytes). Snapshot is replaceable unreliable state during play and reliable terminal state. Maximum full snapshot datagram is 924 bytes. Reliable sends retry through the original handler; terminal snapshot retries on backpressure until accepted.

Control carries version and join/start/heartbeat/quit. Command carries increasing sequence, move vector, aim vector and fire. Snapshot carries version, tick, recipient slot, phase, outcome, core HP, wave, four slot kinds/positions and up to 24 enemy IDs/positions/HP/kinds. Stale command sequences and older snapshot ticks are discarded. Long-session 32-bit application sequence wrap is not yet handled. No cryptographic authentication is provided by this prototype.

Bots use a small defend/approach/fire FSM. Server resolves movement, directional firing, enemy damage and finite waves. All rendering uses received state. The scene uses one instanced draw with a bounded 32-item constant array. The renderer retains the original per-object path for controlled comparison; optional GPU timestamps and CPU submission timing are enabled only for benchmarks.

## Evidence boundaries

Match and world/wire tests cover lifecycle, victory/defeat and terminal state. Socket lifecycle tests cover four humans, capacity, late join, quit and timeout. Two actual DX11 clients produced snapshots and GPU framebuffer captures. Socket fault injection, separate echo-session load measurements, controlled rendering comparison and a full-match result check now have evidence in docs/measurements.md. Separate-PC LAN and publication remain open. See the evidence record for actual logs and review disposition.

