# BUILD-001 isolated probe evidence — 2026-10-29 KST

Status: isolated sample VERIFIED; production game BUILD-001 remains pending.
Plan revision 2 independently passed before source creation. Code by Codex.

Environment: Windows x64, VS 2022 17.11, MSVC 19.41.34120.0 (toolset 14.41.34120), SDK 10.0.19041.0, CMake/CTest 3.30.0-rc4.

Commands (PowerShell, workspace root):
```powershell
cmake -S harness/toolchain-smoke -B out/toolchain-smoke -G 'Visual Studio 17 2022' -A x64 '-DCMAKE_SYSTEM_VERSION=10.0.19041.0'
cmake --build out/toolchain-smoke --config Debug --verbose -- /v:diag
ctest --test-dir out/toolchain-smoke -C Debug --output-on-failure --no-tests=error -V
cmake -S harness/toolchain-smoke -B out/toolchain-smoke '-DBUILD001_NEGATIVE_TEST=ON' '-DCMAKE_SYSTEM_VERSION=10.0.19041.0'
ctest --test-dir out/toolchain-smoke -C Debug -N
ctest --test-dir out/toolchain-smoke -C Debug -R '^build001[.]intentional_failure$' --output-on-failure --no-tests=error
cmake -S harness/toolchain-smoke -B out/toolchain-smoke '-DBUILD001_NEGATIVE_TEST=OFF' '-DCMAKE_SYSTEM_VERSION=10.0.19041.0'
ctest --test-dir out/toolchain-smoke -C Debug --output-on-failure --no-tests=error -V
ctest --test-dir out/toolchain-smoke -C Debug -N
```

Actual results:
- Configure 0, build 0; positive suite 1/1 passed, exit 0.
- Negative test listed explicitly; executable returned 23 with INTENTIONAL_FAILURE marker. CTest reported 1 failed test and exit 8; no WILL_FAIL inversion.
- Negative option restored OFF; default suite 1/1 passed, exit 0; listing contains only build001.cpp23.
- `_MSC_VER=1941 _MSVC_LANG=202004 __cplusplus=202004 __cpp_lib_expected=202211 __cpp_lib_to_underlying=202102`.
- build.log CL command uses `/std:c++latest /permissive- /W4 /Zc:__cplusplus`; generated vcxproj LanguageStandard is stdcpplatest.

Limit: MSVC 14.41 reports draft 202004 even with latest mode. This verifies std::expected and std::to_underlying, not complete final C++23 conformance or a strict C++23-only compiler mode. CMake does require CXX_STANDARD 23 without downgrade. Review later features individually.

Logs: `evidence/build001/`: build.log, positive.log, configure-negative.log, negative-list.log, negative.log, configure-restored.log, restored.log, restored-list.log, exit-codes.json. Generated project: `out/toolchain-smoke/outpost_cpp23_probe.vcxproj`.
Initial configure invocation had an unquoted decimal SDK argument which PowerShell split; CMake still selected installed SDK 10.0.19041.0 automatically. Preserved in configure-initial.log; subsequent invocations quoted the argument and were warning-free. No installed tool or global setting changed.

No network, DX11, game correctness, performance, LAN, library compile, or Claude edit/worktree pilot was exercised by this probe. Workspace has no Git SHA yet; source hashes will identify this sample.
