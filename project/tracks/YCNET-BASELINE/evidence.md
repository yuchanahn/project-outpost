# Corrected networking baseline — 2026-10-29 KST

Canonical source supplied by user: https://github.com/yuchanahn/ycnet-rio-rudp
Local checkout: external/ycnet-rio-rudp
Commit: f7c734298078bb8fcac2fb471355239ab08186ef
Status: BASELINE_BUILD_AND_TESTS_VERIFIED. No source modifications; git status --short empty after execution.

## Correction

Previous G0/NET-CONTRACT-001 inspection targeted old local yc_framework/ycnet_core copies. It did NOT inspect this repository. The old 6-bit cyclic sequence helper defect and proposed modulo-64 patch do not apply here. Repair approval request withdrawn. Historical logs retained and marked superseded rather than presented as current baseline evidence.

Actual rudp.hpp:91 uses `return seq1 > seq2`; get_next_seq increments packet sequence, and sequence_slot independently takes modulo reliable_window_size. The protocol has 32-bit on-wire packet numbers and distinct 64-slot storage, with sequence exhaustion rather than old sequence reuse. Actual ycnet.hpp provides session handshake/binding, try_send backpressure, receive/failure callbacks and disconnect(id).

## Executed verification

VS 2022/MSVC 19.41, Windows SDK 10.0.19041.0, CMake 3.30.0-rc4. Commands from workspace root:
```powershell
git clone --depth 1 https://github.com/yuchanahn/ycnet-rio-rudp.git external/ycnet-rio-rudp
cmake -S external/ycnet-rio-rudp -B out/ycnet-rio-rudp -G 'Visual Studio 17 2022' -A x64 '-DCMAKE_SYSTEM_VERSION=10.0.19041.0' '-Dycnet_build_comparisons=OFF'
cmake --build out/ycnet-rio-rudp --config Release --parallel
ctest --test-dir out/ycnet-rio-rudp -C Release --output-on-failure --no-tests=error
```

Configure 0, build 0, CTest 0: **14/14 passed**, total CTest elapsed 11.25 seconds. Includes reliability policy, sequence reuse, ACK loss, send window, deterministic network loss, local RIO, multi-client RIO, capacity and timeout/id reuse tests. These are provided library tests executed locally, not implemented game acceptance tests. KCP comparison disabled; no 15/15 or full comparison claim.

Logs: evidence/ycnet-baseline/{configure.log,build.log,ctest.log,result.json}.

## Follow-on integration

Use ycnet INTERFACE target and include/ycnet public headers. Preserve this source unchanged and pin the commit in the production dependency contract. Reconcile LOCAL-003 application heartbeat/final-disconnect policy with actual library progress/handshake timeouts; do not mistake the default 10-second progress timeout for an idle-peer heartbeat. The old proposal's protocol-specific facts are superseded; approved game policy direction remains recorded.

No game adapter, DX11 client, server simulation, LAN second-PC run, or new performance result is claimed by this baseline check.
