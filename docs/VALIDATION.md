# Validation

This documents what was actually built, run, and observed - not a
claim of what should work. All commands below were run for real in this
environment; see the exact output further down for each.

## Environment actually used

- Windows, MSYS2 **UCRT64**: `gcc.exe (Rev3, Built by MSYS2 project) 16.2.0`,
  CMake 4.4.3, Ninja. This is the only environment this project has been
  built or tested in.
- Linux, macOS: CMake configuration is portable C11 with no Linux/macOS-
  specific code path left untested by symmetry (the same `#if
  defined(_WIN32)` guards used throughout have exactly two branches, and
  the non-Windows branch uses only POSIX `dlopen`/`dlsym`/`dirent.h`/
  `readlink`/`clock_gettime`) - but **neither was actually built or run
  in this task**. Do not read "portable" as "tested."

## Build and full test suite

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Result: **16/16 tests passed**, twice in a row without a clean rebuild
(checked specifically to rule out state leaking between runs - see "A
real bug: stale journal state across test runs" below).

```
test_sha256, test_candidate, test_manifest, test_core_consensus,
test_perceive, test_verify, test_deliberate, test_ffi_abi_mismatch,
test_ffi_load_resolved, cli_doctor, cli_inspect_perceive,
cli_demo_trilateral_consensus, example_perceive_standalone,
example_verify_standalone, example_deliberate_standalone,
example_trilateral_integration
```

Total test time: ~0.35-6.5s depending on whether a rebuild was needed.

## What each test actually checks

- **`test_sha256`** - three independently-computed-and-verified known-
  answer vectors (via the local `sha256sum` utility, not retyped from
  memory - see the comment in the test), plus a multi-block message to
  exercise the padding/block-boundary path, not just single-block inputs.
- **`test_candidate`** - digest determinism (same content -> same digest,
  twice), digest sensitivity (different content -> different digest),
  mutation detection (`usdk_candidate_digest_matches` correctly returns
  false after a field is overwritten post-creation), and rejection of
  invalid arguments (null session id, an id longer than `USDK_ID_LEN`).
- **`test_manifest`** - manifest field parsing including a nested
  dependency array; a real 3-node diamond dependency graph resolved by
  Kahn's algorithm with the topological order checked explicitly (not
  just "did it succeed"); a genuine 2-node cycle rejected with
  `USDK_ERR_DEPENDENCY_CYCLE`; a missing dependency rejected with
  `USDK_ERR_DEPENDENCY_UNRESOLVED`; a present-but-too-old dependency
  rejected with `USDK_ERR_DEPENDENCY_VERSION_INCOMPATIBLE`.
- **`test_core_consensus`** - the full round state machine: unanimous
  accept commits and dispatches exactly once (checked with a call
  counter, including that redundantly calling `usdk_core_round_decide`
  again does not dispatch a second time); a single `REJECT` prevents
  commit; a single `ABSTAIN` prevents commit; a missing (never-cast) vote
  prevents commit; an already-expired deadline is rejected at
  `usdk_core_submit_vote` with `USDK_ERR_DEADLINE_EXCEEDED` and moves the
  round to `TIMED_OUT`; a duplicate vote from the same role is rejected
  with `USDK_ERR_DUPLICATE_VOTE` and the original vote is verified
  preserved (by committing with it, not just checking the reject code); a
  conflicting `party_id` for the same role is rejected with
  `USDK_ERR_PARTY_CONFLICT`; a replayed/stale `round_id` is rejected with
  `USDK_ERR_STALE_ROUND`; a vote whose recorded `candidate_digest_seen`
  does not match the round's candidate cannot count toward commit even
  with an `ACCEPT` verdict; cancelling a round blocks further votes and
  a second cancel is itself rejected; and `usdk_core_recover_incomplete_rounds`
  correctly reports a round left `OPEN` (never decided) after reopening
  the same `runtime_dir`, simulating a restart.
- **`test_perceive`/`test_verify`/`test_deliberate`** - each role's own
  distinct check (`docs/ARCHITECTURE.md`), independently, linked directly
  (no dynamic loading): perceive accepts real evidence and rejects
  fabricated evidence_refs and zero-evidence candidates; verify's
  constraint-allow-list check and uncertainty-threshold check are each
  independently exercised (not just "some rejection happened" - the
  specific `reason_code` is checked for each); deliberate accepts a
  candidate built from its own `usdk_deliberate_propose` output and
  rejects a foreign one, and a missing `driver_path` in its config is
  rejected rather than silently defaulted.
- **`test_ffi_abi_mismatch`** - a real, deliberately-incompatible fixture
  module (`tests/fixtures/module_bad_abi.c`, `abi_version_major` one past
  what this build supports) is rejected by `usdk_ffi_load` with
  `USDK_ERR_ABI_VERSION_MISMATCH`, and no module handle is left behind.
- **`test_ffi_load_resolved`** - loads the real, built
  `usdk-perceive`/`usdk-deliberate`/`usdk-verify` modules from the
  project's actual `manifests/` directory through the full
  `usdk_ffi_load_resolved` path, confirming the transitively-declared
  `usdk-driver-fixture` dependency is also resolved and loaded (4 modules
  from a 3-name request) and every loaded module's ABI version matches.
- **`cli_doctor`/`cli_inspect_perceive`/`cli_demo_trilateral_consensus`** -
  the built `usdk` CLI, invoked exactly as a user would, checked for exit
  code and (for `demo`) the JSON `"overall":"pass"` field, which is
  computed from the actual round states of all four demo scenarios
  matching their expected outcomes (`src/cli/main.c`'s `cmd_demo`), not
  merely "the command didn't crash."
- **`example_*`** - each of the four example programs
  (`docs/GETTING_STARTED.md`) built and run to completion (the standalone
  perceive/verify/deliberate examples call `usdk_*_vote` and print the
  verdict; a mismatch would show in the printed output even though these
  don't assert - they were manually read once, see below).

## Real bugs found and fixed by actually building and running this project

Listed because finding them is itself evidence the testing was real, not
aspirational - a build that never fails cannot have found these.

1. **CMake `add_custom_command(TARGET usdk_cli POST_BUILD ...)` called
   from the wrong directory scope.** The manifest-copy step was
   originally in the root `CMakeLists.txt`; CMake refused to configure
   at all (`TARGET 'usdk_cli' was not created in this directory`).
   Fixed by moving it into `src/cli/CMakeLists.txt`, where `usdk_cli` is
   actually defined.
2. **`test_manifest` failed to link**: `undefined reference to
   __imp_usdk_manifest_parse`. `src/ffi/manifest.c` was compiled directly
   into the `test_manifest` executable (to avoid a dynamic-loading
   dependency for a pure-logic test), but its functions are declared
   `USDK_EXPORT` in the public header, which defaults to
   `__declspec(dllimport)` unless `USDK_BUILDING_SHARED` (building the
   DLL) or `USDK_STATIC` (same-binary direct use) is defined - neither
   was, for this target specifically. Fixed by adding
   `target_compile_definitions(test_manifest PRIVATE USDK_STATIC)`.
3. **Every dynamically-loaded module failed to load**
   (`usdk doctor`/`inspect`/`demo` all reported "module load failed" or
   "failed to load usdk-perceive").
   Root cause: CMake's default MinGW `SHARED` library naming adds a
   `lib` prefix (`libusdk_perceive.dll`), but
   `manifests/usdk-perceive.usdk-manifest.json`'s `artifact_path` is the
   bare name `usdk_perceive`, and `usdk-ffi` resolves that literally
   (`docs/ABI.md`) - the file it looked for never existed under that
   name. Confirmed directly with `objdump -p`, which showed the export
   table and symbol names were correct all along; the failure was purely
   in path construction, not the ABI/export mechanism. Fixed by setting
   `PREFIX ""` on the three role library targets, matching the
   convention `usdk-driver-fixture` already used.
4. **`usdk_deliberate_create` still failed after fix 3, with
   `USDK_ERR_MODULE_LOAD_FAILED`, specifically only when the driver path
   was built from `own_exe_dir()` (the CLI/examples) - not when built
   from a CMake generator expression (`test_deliberate`, which passed).**
   Root cause: Windows paths contain backslashes, the JSON escape
   character. `src/cli/main.c`, `examples/deliberate_standalone/main.c`,
   and `examples/trilateral_integration/main.c` all hand-built a JSON
   config string embedding the driver path *without escaping it* -
   `src/ffi/json_min.c`'s minimal parser's unrecognized-escape fallback
   takes the character after an unknown `\X` literally, so
   `"C:\Users\...\usdk_driver_fixture.dll"` was silently corrupted into
   something like `"C:UsersProjectsusdkbuildbin/usdk_driver_fixture.dll"`
   on parse - a real path-mangling bug, not a load failure. Verified the
   raw `LoadLibraryExW` call itself was fine (tested directly via a
   PowerShell P/Invoke against the exact same path and flags, which
   succeeded) before finding this. Fixed by normalizing backslashes to
   forward slashes (which Windows accepts natively in paths) before
   embedding any path in a hand-built JSON string, in all three files.
5. **A real bug: stale journal state across test runs.**
   `test_core_consensus` used a fixed `runtime_dir` name; because
   `usdk-core`'s stale-round check reads the on-disk journal (not just
   in-process state, by design - `docs/CONSENSUS_PROTOCOL.md`), *re-running
   the already-passing test a second time without a clean rebuild* made
   every `usdk_core_open_round` call fail with `USDK_ERR_STALE_ROUND`,
   because the previous run's journal still recorded those exact
   `round_id`s as already opened. This is the consensus protocol working
   exactly as specified, applied correctly to a test that didn't account
   for it. Fixed by deriving a unique `runtime_dir` per run
   (`usdk_monotonic_ns()`-suffixed) - and the same latent flakiness was
   pre-emptively fixed the same way in `usdk demo`'s runtime directory
   (PID alone is not enough: PIDs get reused across separate process
   launches, and the demo always opens the same fixed round_ids every
   invocation).
6. **A real dependency-graph error caught before it shipped**:
   `usdk_monotonic_ns()`/`usdk_owned_buffer_release()` were originally
   declared in `usdk/core.h`, which would have made `usdk-perceive` and
   `usdk-verify` unable to timestamp their own votes without linking
   `usdk-core` - directly contradicting `docs/PACKAGES.md`'s documented
   dependency graph (neither role links `usdk-core`). Caught by re-reading
   the dependency table while writing the role implementations, before
   attempting a build - moved to `usdk/types.h` (`usdk-contracts`, which
   every role already depends on) instead.
7. **A real digest-encoding bug caught before it ran**: the wire encoder
   (`src/contracts/wire.c`) originally signaled a buffer-overflow failure
   by returning offset `0` from each `put_*` helper - but offset `0` is
   also the legitimate starting position, so a failure partway through
   encoding would have caused the *next* field to silently start
   overwriting from the beginning of the buffer instead of the encoding
   failing loudly. Caught by re-reading the function before running it
   (the bug was unreachable in practice, since the caller always
   pre-sizes the buffer exactly, but was fixed anyway rather than left as
   a latent trap) - fixed by threading an explicit `int* ok` flag through
   every helper instead of overloading the return value.

## Sanitizers: attempted, not available in this environment

```bash
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DUSDK_ENABLE_SANITIZERS=ON
cmake --build build-asan
```

Configuration succeeds; the **build fails at the link step**:
`ld.exe: cannot find -lasan` / `cannot find -lubsan`. Confirmed by
searching the entire UCRT64 toolchain installation for any `*asan*`/
`*ubsan*` file - none exist. This MSYS2 UCRT64 GCC 16.2.0 package does
not ship AddressSanitizer/UndefinedBehaviorSanitizer runtime libraries
(a known real limitation of MinGW-w64 GCC builds, distinct from Linux
GCC). `USDK_ENABLE_SANITIZERS` remains a supported CMake option
(`CMakeLists.txt`) for an environment where it works (e.g. Linux GCC/
Clang, or MSVC's `/fsanitize=address`, neither tested here) - it is
**not** silently claimed to have run successfully here. See
`docs/IMPLEMENTATION_STATUS.md`.

## Installed-location test

```bash
cmake --install build --prefix /tmp/usdk-install-test
```

Installs `bin/usdk.exe`, the four role/driver DLLs (correctly named,
matching fix 3 above), the four manifest JSON files, headers,
`UsdkConfig.cmake`/`UsdkTargets.cmake`, and docs, to a prefix entirely
outside the source checkout.

Then, from a separate, empty working directory
(`/tmp/usdk-isolated-run`) outside the checkout, with `PATH` restricted
to just the UCRT64/usr shells (nothing from the build tree on it), the
installed binary alone was run:

```bash
export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin"
/tmp/usdk-install-test/bin/usdk.exe --help      # usage to stdout, exit 0
/tmp/usdk-install-test/bin/usdk.exe doctor --json
/tmp/usdk-install-test/bin/usdk.exe demo --scenario trilateral-consensus --json
```

`doctor` returned `{"overall":"ok"}` (all four role/driver modules
discovered, loaded, and ABI-validated purely from the installed layout -
`usdk-ffi` resolving `manifest_dir` from the installed binary's own
directory, per `docs/ABI.md` "Platform loading"). `demo` returned the
same four correct scenario outcomes as the build-tree run and
`"overall":"pass"`. Runtime state was written under the current working
directory only (`./usdk-demo-run-<pid>-<timestamp>`), never inside the
install prefix.

## UAgent browser extension validation

See `docs/UAGENT_ARCHITECTURE.md` for what was built. This section
records what was actually run to check it, per this project's existing
standard of "write real code, then run it for real" (native validation
above did the same).

**Automated tests, run together, all passing at time of writing:**

```bash
npm install --offline                          # zero network access - internal workspace symlinks only
npm test                                        # node --test packages/*/test/*.test.mjs
```

- 99 JS tests (`node --test`) across `contracts`, `core`, `loader`,
  `perceive`, `deliberate`, `verify`, `capability-llm`,
  `capability-robotics`, `driver-llm-fixture`, `driver-llm-local`,
  `driver-voice` (incl. the `speak()` fallback-timeout regression, see
  below), `driver-vision`, `driver-robotics-sim`, `host-browser`,
  `host-local`, `binding-javascript`.
- 8 Python tests (`python -m unittest tests.test_perceive`, run from a
  real UCRT64 Python 3.14.7) against the actual
  `build/bin/usdk_perceive.dll` and `libusdk_contracts.dll` - not a
  mock; asserts the exact verdicts/reason codes
  `tests/unit/test_perceive.c` already establishes natively.
- All 16 native C tests (`ctest --test-dir build`) re-run after this
  extension's work to confirm no regression to the underlying SDK - all
  still passing.
- `packages/binding-lua/usdk.lua` has **no** automated test - no
  Lua/LuaJIT runtime exists anywhere in this environment (checked:
  `which lua luajit lua5.1 lua5.3 lua5.4` all empty); see its own README.

**Real, interactive browser verification (not simulated), both execution
profiles, using this environment's actual browser-automation tooling:**

- *Browser-local* (`packages/uagent/public/index.html`, served from
  source): loaded capabilities confirmed in the DOM; a text turn
  committed and displayed the fixture's labeled response; a robotics
  action was approved and executed; a robotics action triggered mid-move
  by clicking EMERGENCY STOP correctly showed "approved, but did not
  complete" instead of a stale "approved and executed" (the regression
  this session's own `session.test.mjs` 9th test now guards).
- *Connected* (`packages/uagent/public/connected.html`, talking to a
  real `@usdk/host-local` on a separate port): paired using the pairing
  secret printed to the host process's own console; a text turn
  committed over real HTTP with a real browser-supplied `Origin` header
  correctly validated against the server's allowlist; a robotics action
  was proposed, approved, and executed by the host process; emergency
  stop engaged. Network log and DOM content both inspected directly, not
  inferred.
- *Built `dist/` output* (`npm run build && npm run serve`, then loaded
  from `http://127.0.0.1:8422/...` - a location containing none of the
  `packages/*/test` or `node_modules` structure the checkout has): all
  three Browser-local capabilities loaded and a text turn committed
  correctly, confirming the build script's output is genuinely
  standalone-runnable, not just structurally plausible.

**A real bug found only through this live testing, not through static
reasoning**: the first live `dist/`-served text-turn attempt hung
indefinitely at "Thinking… (not yet approved)". Root cause: `driver-voice`'s
`speak()` awaited `speechSynthesis`'s `onend`/`onerror` callback with no
fallback, and in this browser pane's rendering environment neither
callback ever fired after `speechSynthesis.speak()` was called
(`speechSynthesis.speaking` read back `false` - the utterance was
dropped silently, not queued or erroring loudly) - a known class of
issue in headless/audio-device-less browser contexts. This blocked
`@usdk/core`'s post-commit dispatch from ever returning, which blocked
the entire turn from resolving, even though the three-party vote had
already committed. **Fixed** with a bounded fallback timeout
(`DEFAULT_SPEAK_FALLBACK_TIMEOUT_MS = 15000` in
`packages/driver-voice/src/index.mjs`) that calls `onDone()` regardless
if neither browser callback fires in time; covered by two new
regression tests (`driver-voice.test.mjs`) using a fake
`speechSynthesis` that never fires either callback, and re-verified live
in the same browser pane afterward (the turn now resolves, via the
fallback, instead of hanging forever).

## Not tested (stated plainly)

- Linux and macOS builds (see "Environment actually used" above).
- MSVC build (`docs/IMPLEMENTATION_STATUS.md` - CMake config is C11/
  portable but untried with `cl.exe`).
- Sanitizer-instrumented test runs (see above - toolchain limitation,
  not skipped by choice).
- Any test involving more than one OS process racing against the same
  `runtime_dir` concurrently (the consensus protocol's crash/restart
  handling was tested sequentially - open, kill the `usdk_core_t`,
  reopen - never with two processes actually running at once).
- A real local-inference driver (none exists in this release - see
  `docs/IMPLEMENTATION_STATUS.md`).
- `packages/binding-lua` against a real LuaJIT runtime - none exists in
  this environment (see its README).
- `packages/binding-python`/`binding-lua` binding `usdk-deliberate`,
  `usdk-verify`, or `usdk-core` - only `usdk-perceive` is bound in this
  release.
- A real microphone/`SpeechRecognition` transcript in a live browser -
  the browser-automation tooling used for the interactive verification
  above does not provide real microphone audio input; `startVoiceInput`'s
  wiring was exercised structurally (unit tests, `isAvailable()`
  gating) but not with a genuine spoken utterance.
- Cross-browser/cross-OS behavior of `speechSynthesis` - the fallback
  timeout above was added because it failed silently in exactly one
  browser-automation environment; whether other real browsers/OSes hit
  the same failure mode was not separately checked.
- Any TLS/HTTPS deployment of `@usdk/host-local` - it serves plain HTTP
  on `127.0.0.1` only; see its README's "Known limitations."
- Concurrent sessions against `@usdk/host-local` under real load (the
  test suite exercises one session per test, sequentially - no
  multi-session or high-concurrency test was run).
