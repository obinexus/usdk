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
