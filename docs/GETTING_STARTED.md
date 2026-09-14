# Getting started

## Prerequisites

Tested on **Windows via MSYS2 UCRT64** (GCC 16.2.0, CMake 4.4.3, Ninja) -
see `docs/VALIDATION.md` for exactly what was run. Linux/macOS
configurations are prepared (portable CMake/C11, no Windows-only API
outside the files already guarded by `#if defined(_WIN32)`) but **not
built or tested in this environment** - see
`docs/IMPLEMENTATION_STATUS.md`.

From a UCRT64 shell (`C:\msys64\ucrt64.exe`, not plain MSYS2/MINGW64):

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

## Build and test

Run `make` from a real MSYS2 UCRT64 shell - launch it as
`C:\msys64\ucrt64.exe` (or the "MSYS2 UCRT64" Start Menu shortcut), not
plain PowerShell/cmd, even if `C:\msys64\ucrt64\bin` happens to be on
that shell's `PATH` - see "If `make` warns about a toolchain/shell
mismatch" below for why that distinction matters.

```bash
make
make test
```

or directly with CMake:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

`make help` lists every target and overridable variable
(`BUILD_DIR`, `BUILD_TYPE`, `GENERATOR`, `INSTALL_PREFIX`, `JOBS`).

### If `make` warns about a toolchain/shell mismatch

`make`'s `configure` target runs `cmake/CheckBuildEnv.cmake` before
touching `BUILD_DIR`, and prints a `WARNING` if that directory already
has a cache configured with an MSYS2/MinGW compiler (ucrt64/mingw64/
clang64/msys64 in its path) while the *current* shell has no MSYS2
environment active (`MSYSTEM` unset). If you see that warning and then a
compile failure like:

```
-- Check for working C compiler: C:/msys64/ucrt64/bin/cc.exe - broken
CMake Error ... is not able to compile a simple test program.
```

this is that exact mismatch: **having `C:\msys64\ucrt64\bin` on `PATH`
is not the same as running inside an activated MSYS2 shell.** The
compiler driver (`cc.exe`) can still run standalone from an ordinary
PowerShell/cmd session (`cc --version` works), but the real compiler
back end it execs needs runtime DLLs that are only on `PATH` inside a
shell actually launched as MSYS2 UCRT64 - so configuring succeeds (CMake
just checks the cached path exists) but the first real compile fails,
with no useful message pointing at the cause. Two ways to fix it:

1. **Run `make` from a real MSYS2 UCRT64 shell** -
   `C:\msys64\ucrt64.exe`, or the "MSYS2 UCRT64" Start Menu shortcut -
   not plain PowerShell/cmd, even one with `ucrt64\bin` on `PATH`.
2. **Or build into a separate directory** for whatever toolchain your
   current shell actually has working, e.g. `make BUILD_DIR=build-msvc`
   if you have Visual Studio's `cl.exe` available instead.

## Run the demo

```bash
./build/bin/usdk --help
./build/bin/usdk doctor --json
./build/bin/usdk inspect usdk-perceive --json
./build/bin/usdk demo --scenario trilateral-consensus --json
```

`doctor` and `inspect` load role/driver modules **dynamically** through
`usdk-ffi` and the manifests in `manifests/` (copied next to the built
binaries) - this is what actually exercises ABI validation and
dependency resolution (`docs/ABI.md`). `demo` links the three
constituents **directly** and runs four scenarios end to end (unanimous
accept and commit, a rejected-for-insufficient-permission candidate, an
already-expired deadline, and a candidate whose recorded vote digest was
tampered with) - see the comment on `run_round()` in `src/cli/main.c` for
why the demo uses direct linking while `doctor`/`inspect` use dynamic
loading, and `docs/CONSENSUS_PROTOCOL.md` for what each scenario proves.

`examples/trilateral_integration/` is the same walkthrough with plain
text output, meant to be read start to finish; `examples/*_standalone/`
each exercise one constituent alone.

## Validate a manifest directory

```bash
./build/bin/usdk validate --config examples/config/validate-config.json --json
```

Run this from inside the directory that holds the built `usdk` binary
and its sibling `.dll`/`.so` files (the example config's
`"manifest_dir": "."` is relative to the current directory, not the
config file's own location) - e.g. `cd build/bin && ../../build/bin/usdk
validate --config ../../examples/config/validate-config.json --json`, or
copy the config next to the binaries.

## Adding a driver

A driver implements `usdk_driver_vtable_t` (`include/usdk/plugin.h`):
`create`/`destroy`/`generate`. See `src/driver_fixture/driver_fixture.c`
for the complete, minimal reference - a deterministic fixture with no
external dependencies. To use a new driver:

1. Build it as a `MODULE` library exporting `usdk_plugin_query_v1` with
   `role = USDK_ROLE_DRIVER` and `capability_flags = USDK_CAP_DRIVER`
   (see `src/driver_fixture/CMakeLists.txt`).
2. Add a `<name>.usdk-manifest.json` next to it (see `manifests/*.json`
   for the schema) if you want it discoverable via `usdk-ffi`'s
   dependency resolution.
3. Point `usdk-deliberate` at it: `usdk_deliberate_create`'s config JSON
   takes `{"driver_path": "<path to your built module>"}` - see
   `include/usdk/deliberate.h`.

A **real local-inference driver** (calling an actual model runtime) was
not added in this release - see `docs/IMPLEMENTATION_STATUS.md` for why,
and what such a driver would need to implement beyond the fixture (real
tokenization/inference, real latency, a real failure mode instead of
always succeeding deterministically).

## Adding a language binding

No `usdk-binding-<language>` is implemented in this release
(`docs/IMPLEMENTATION_STATUS.md`). The ABI is designed to make one
straightforward: every public struct uses only fixed-width types, opaque
handles with explicit lifecycle functions, and pointer+length buffers
(`docs/ABI.md`) - no raw pointers or padding need to survive a language
boundary. A binding would wrap `usdk_plugin_query_v1` (or, more simply,
call the three constituents' direct-link functions - `usdk_perceive_*`
etc. - the way `src/cli/main.c`'s `demo` command does) and the
`usdk-core` round API (`include/usdk/core.h`).

## Diagnosing a rejected agreement round

Every `usdk_vote_t` carries a machine-readable `reason_code` and a
human-readable `reason_detail` (`include/usdk/vote.h`) - `usdk demo
--json`'s output does not currently surface individual vote reasons
(only the round's final state per scenario), so to see *why* a
particular party rejected a candidate, call that role's own `_vote`
function directly (as `examples/*_standalone/main.c` do) and print
`vote.reason_code`/`vote.reason_detail`, or extend a caller's dispatch
loop (as `src/cli/main.c`'s `run_round()` does) to log each vote before
submitting it. `usdk_core_recover_incomplete_rounds`
(`include/usdk/core.h`) additionally reports rounds left open or with an
uncertain dispatch outcome across a restart - see
`docs/CONSENSUS_PROTOCOL.md` "Crash and restart behavior".
