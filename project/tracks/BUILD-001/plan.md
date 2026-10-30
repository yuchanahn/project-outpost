# BUILD-001 plan — revision 2

State: PLAN_CRITIC. Product/transport implementation remains gated.

1. Codex records source requirements and local environment evidence.
2. Independent Claude Plan Critic reads this spec/plan with read-only access; address blocking findings before creating source.
3. Codex creates the isolated CMake/CTest sample, with one real C++23 probe executable supporting --fail; ordinary test and opt-in negative test invoke the same binary.
4. Configure/build/positive CTest sequentially, then configure negative test on and run only it, capturing its nonzero exit code; restore negative test off afterward.
5. Record results in evidence.md with paths to logs. Reviewer inspects source and evidence; Codex verifies claims from tool results.

Concrete mechanisms added after critic findings:
- CMake `option(BUILD001_NEGATIVE_TEST "Enable intentional failure" OFF)`; only inside `if(BUILD001_NEGATIVE_TEST)` register `build001.intentional_failure` with `--fail`. Always register `build001.cpp23`. Use `ctest -R '^build001[.]intentional_failure$' -C Debug --output-on-failure --no-tests=error`; first require `ctest -N` to list that test, and require the output to show the executable's INTENTIONAL_FAILURE marker. Thus missing tests or unrelated CTest errors cannot count as the expected failure.
- Guard `if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "MSVC") message(FATAL_ERROR ...) endif()` after project(). Configure explicitly `-G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.19041.0`.
- Capture `cmake --build ... --config Debug --verbose -- /v:diag` to build.log. Preserve generated vcxproj LanguageStandard and CL command lines (/std:) as switch evidence; do not rely on compile_commands.json for the VS generator.
- Capture each native exit code immediately. Abort on unexpected configure/build/positive-test failure. Restore negative option OFF, then rerun ordinary suite and list tests to verify restoration. Store logs per invocation.
- Print feature macros plus _MSC_VER, _MSVC_LANG, __cplusplus in the probe. No changes outside scoped sample/output/docs/evidence.

Dependencies: installed VS/MSVC/SDK and CMake (found). No networking dependency or source ownership assumption. No performance measurements claimed.

Follow-on DAG: confirmed RUDP/RIO source + read-only contract analysis -> G2 transport/game contracts + critic + material approvals -> production CMake/CTest harness -> server + two DX11 clients -> local integration -> separate-PC LAN -> fault/load/rendering evidence. Each downstream node needs a separate concrete contract before implementation.
