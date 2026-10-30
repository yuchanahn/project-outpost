# Measurements and remaining acceptance gates

2026-10-30 KST. Release build; Windows 11 / Intel i7-11800H. Full OS/CPU/display-driver inventory: evidence/os.json, cpu.json, gpu.json. Renderer reported **Intel UHD Graphics**, not the installed NVIDIA GPU. Dependency: unmodified ycnet-rio-rudp f7c734298078bb8fcac2fb471355239ab08186ef.

## Real socket impairment

`tools/test-faults.ps1 -Python <Python 3 executable>` starts the dedicated game server, two probes and a loopback relay. Seed 42; each direction independently uses 10% loss, 5% duplicate probability and uniform 0–80ms scheduling delay. Delay variability induces reorder. Seed fixes RNG choices but OS scheduling can alter which datagram gets each choice; exact counts are not guaranteed repeatable.

Passing run in evidence/faults/seed-42: 585 received datagrams, 69 dropped, 21 duplicates scheduled, 537 forwarded, 48 forwarding-order inversions, maximum scheduled relay queue 10. Two distinct admitted slots received 62/60 snapshots, tick 170/173, core 100, and server finalized abandonment. Different latest ticks are expected with lossy replaceable state. This scenario establishes continued operation/control and cleanup, not exact delivery latency or reliable-event deduplication under every fault schedule. Original transport reliability tests provide separate protocol evidence.

The earlier evidence/faults root logs are a **failed harness run**: both probes continued, but Windows UDP ICMP port-unreachable raised ConnectionResetError in the relay after a peer closed. The relay now ignores that expected teardown notification; passing seed-42 logs supersede the harness failure without deleting it.

## Transport load, separate from game slots

`tools/measure.ps1 -Kind Load` runs a separate RUDP/RIO echo server on UDP 27017 and synthetic Winsock/RUDP clients. 64-byte reliable payload, one outstanding request per session, five-second active send phase, one-second drain, explicit reliable quit. Two server RIO workers. Each table row is one run, not a confidence interval.

| Sessions | Sent / echoes | p50 / p95 / p99 RTT ms | Client / server CPU seconds | Cleaned sessions |
|---|---|---|---|---|
| 8 | 1106 / 1106 | 31.6362 / 47.5387 / 48.2617 | 0.03125 / 0.015625 | 8 |
| 32 | 4515 / 4515 | 31.4591 / 47.646 / 49.4643 | 0.078125 / 0.0625 | 32 |
| 64 | 9224 / 9224 | 31.0516 / 47.4408 / 48.5226 | 0.109375 / 0.109375 | 64 |

All rows: duplicate application echoes 0, client retransmissions 0, maximum per-client reliable pending count 1. Raw logs: evidence/load. CPU is GetProcessTimes total kernel+user across the process lifetime, not percent or per-core utilization. Server lifetime is ten seconds and includes idle time; client lifetime includes drain/quit. Windows scheduler/sleep granularity materially affects stop-and-wait RTT. About 221/903/1845 completed exchanges per active second were observed; these are scenario throughput, **not saturation capacity**. Internal RIO queue depth is not exposed by this library and is not claimed measured. The reported pending queue is the client reliability queue. Single-instance co-resident loopback experiments do not establish LAN capacity or maximum supported connections.

## DX11 controlled comparison

`tools/measure.ps1 -Kind Render` renders a frozen scene: floor, core, four drones and 24 enemies, 1280x720 client area. 120 warmup frames then 500 measured frames; VSync off. Both modes use the same cube mesh/shader lighting. Baseline updates constants and draws once per object. Instanced mode uploads 32 bounded per-instance constants once and draws once. Game client now uses instancing.

Latest paired run, evidence/render:

| Mode | Draw calls | CPU submit p50 / p95 ms | GPU p50 / p95 ms |
|---|---|---|---|
| Baseline | 30 | 0.0112 / 0.0144 | 0.135417 / 0.145677 |
| Instanced | 1 | 0.0046 / 0.0083 | 0.136458 / 0.1525 |

GPU timestamps bracket render commands and reject disjoint samples (0 invalid). CPU measures render command submission, excluding resize, query resolution, capture and Present. Queries are synchronously resolved each frame with a timeout; this intentionally serializes the benchmark, and results do not represent normal pipelined frame latency or FPS. Presentation, input and network simulation costs are excluded. This small scene demonstrates reduced CPU submission/draw count; there is no GPU speedup claim. Values fluctuate at this scale; no broad hardware claim is justified.

Baseline/instanced GPU framebuffer BMP SHA256 both `7A0F67AF4EBC5DA030C2B95AB1503D7A5AE3ED441F5254DB51B4033B907EC62F`; scene capture visually inspected. The script requires exact framebuffer equality. Capture images remain local and ignored by Git; regenerate them with the command above.

## Full match result

`tools/test-result.ps1` runs two automated defensive input clients through actual sockets until terminal state. Both received tick 1014, core 100, outcome 1 (victory), with 327/326 snapshots; server stopped at the same tick/outcome. This checks final tick/core/result agreement, not every entity byte. Source world tests separately reproduce defeat and terminal freezing.

## Outstanding acceptance

Separate-PC LAN requires a second Windows machine. Internal RIO queue instrumentation and a proven saturation capacity remain unmeasured; the later pipelined profile below extends offered load. MIT publication has now been explicitly approved; repository destination is yuchanahn/project-outpost. The pending hardware gate is separate-PC LAN. Current evidence supports a local working prototype and bounded measurements, not completion of every portfolio gate or production hardening.

Four-client idle socket defeat also passed: all four distinct slots received tick 796, core 0, outcome 2, with 260 snapshots each. Server preserved outcome 2 after all clients quit. Logs: evidence/defeat. Reproduce with `tools/test-result.ps1 -Outcome Defeat`; Victory is the default. All movement/firing in these scenarios is ordinary client input, not direct mutation of authoritative World state.

A local Release ZIP with server/client/probe executables, hashes and LAN instructions was produced under out/dist via tools/package.ps1. This is preparation for a second PC, not evidence of a successful second-PC run.

## Pipelined offered load

The benchmark was then extended to a bounded 1–32 in-flight window per client and a bounded 64-echo application queue per server peer. `tools/measure.ps1 -Kind Load -Window 8` exercised more offered load without replacing unreturned requests. Current server queue code preserves every accepted echo until try_send accepts it. The earlier window-1 results remain a historical run of the single-outstanding implementation.

| Sessions / window | Sent = echoes | p50 / p95 / p99 RTT ms | Retransmissions | Client / server CPU seconds |
|---|---|---|---|---|
| 8 / 8 | 7691 | 46.0622 / 47.5957 / 48.3463 | 14 | 0.234375 / 0.3125 |
| 32 / 8 | 30356 | 44.342 / 63.5938 / 77.8564 | 258 | 0.734375 / 0.9375 |
| 64 / 8 | 41310 | 59.6703 / 91.9121 / 138.081 | 2959 | 0.8125 / 1.40625 |

All sessions cleaned; duplicate application echoes 0; per-client reliability pending maximum 8 and per-server-peer application echo queue maximum 8. Raw logs: evidence/load/window-8. These queue measurements are explicitly outside the internal RIO queues. Increased loopback RTT/retransmissions under offered load are observed, not attributed to a proven root cause. About 8262 completed exchanges per active second at 64/window8 does not establish saturation capacity or a pure-RIO throughput figure. Timing includes RUDP, socket polling, scheduling, serialization and both processes sharing this PC.

Independent plan critique a9cd842c-f311-4b63-a96b-e086522526ca confirmed the distinctions between submission/GPU/presentation timing and RTT-limited load. Its suggestion to pipeline was implemented above. Adding presentation latency was deferred because it is not the current CPU/GPU comparison, and speculative frame-statistics fields are not used. The external transport's plain shared is_running flag remains an upstream concurrency concern; fresh processes avoid reuse but do not prove shutdown data-race safety. No external source repair was performed.


## Independent implementation review

Read-only Claude task f035accd-161a-4ae8-9f18-a8d013ee30de found no concrete crash/measurement-invalidating issue in the inspected render/fault/older stop-and-wait implementation. Review statements were checked against code: the fixed scene has **30** objects (not the report's 29), and the game uses instancing at construction time rather than a runtime user toggle. The later pipelined map/deque echo changes were verified by the actual 8/32/64 window-8 runs, not by treating that older review as coverage. Query recording commands are included in CPU submission timing; query resolution waits are excluded.


## Publication

Owner approved MIT publication on 2026-10-30 KST. Public source repository: https://github.com/yuchanahn/project-outpost. Separate-PC LAN is explicitly untested because no second PC is available. A preview release includes a Windows x64 executable ZIP with MIT notice, build/source identification, checksums, and the hardware validation checklist. No internet/NAT or production-safety claim is made.
