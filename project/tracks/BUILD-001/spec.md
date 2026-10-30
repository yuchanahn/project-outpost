# BUILD-001 — isolated toolchain probe (revision 1)

Source: [Notion v2.0](../../source/notion-v2.0.md), DEC-017, DEC-020.
Outcome: verify the installed Windows/MSVC/CMake environment before game implementation.
Scope: `harness/toolchain-smoke/`, generated `out/toolchain-smoke/`, and evidence/docs only. No game, network, global settings, installation, remote fetch, or existing library changes.

Acceptance:
- Configure with Visual Studio 17 2022, x64, installed SDK; reject non-MSVC.
- Require CXX_STANDARD 23, CXX_STANDARD_REQUIRED YES, CXX_EXTENSIONS NO. Use /Zc:__cplusplus, /permissive- and /W4.
- Compile and run a C++23 library probe using std::expected and std::to_underlying, checking feature macros and printing compiler/language values. Passing demonstrates these features only, not full C++23 conformance. Record CMake's actual /std switch; /std:c++latest may enable later extensions.
- CTest normal test succeeds; intentionally failing test is opt-in and unmasked (no WILL_FAIL). Its CTest invocation must return nonzero. Default suite must remain green.
- Preserve exact commands, compiler/SDK identity, logs, exit codes and limitations. Stop on failure, report diagnostics; do not install or downgrade the required standard.
- Independent plan critic before probe source creation; review actual source and evidence afterward.

This probe is an isolated sample, not the production build or completion of BUILD-001 for the game.
