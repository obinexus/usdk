# Implementation status

Status values: **Implemented** (code exists and is exercised by at least
one automated test that actually ran - see `docs/VALIDATION.md`),
**Partial** (code exists but narrower than the full requirement),
**Deferred** (explicitly out of scope for this release, stated here with
why), **Not tested** (code exists, believed correct, no automated test in
this environment).

## 1. Research review

**Implemented.** `docs/RESEARCH_REVIEW.md` - all three archives reviewed,
each claim classified (Definition / Proposal / Empirical result /
Mathematical claim with reviewed proof / Unsupported claim), with
boundary-case checks performed directly on two proof claims (section
3.3's AEGIS-PROOF-1.2 non-negativity re-derivation, and the "95.4%
consensus" threshold's Gaussian-to-cosine-similarity gap).

## 2. The three constituent packages

**Implemented**, with the comparison-to-research and revision-
justification the task requires - see `docs/ARCHITECTURE.md`
"Comparison with the archive research." `usdk-perceive`,
`usdk-deliberate`, `usdk-verify` each: have their own public header and
lifecycle (`include/usdk/{perceive,deliberate,verify}.h`); build as
independent CMake targets; are unit-tested independently
(`tests/unit/test_{perceive,deliberate,verify}.c`) and exercised via
fixture-shaped hand-built candidates rather than against each other's
real code; do not link each other (`docs/PACKAGES.md`'s dependency
table, enforced by CMake's own DAG, not just documented); are reachable
through `usdk_plugin_query_v1` (each exports it - `src/{perceive,
deliberate,verify}/*.c`); document inputs/outputs/failure modes in their
own header doc comments and in `docs/ARCHITECTURE.md`'s table.

## 3. Modular dependency boundaries

**Implemented**: `usdk-contracts`, `usdk-core`, `usdk-ffi` all exist and
match their documented responsibilities (`docs/PACKAGES.md`). One
driver, `usdk-driver-fixture`, is implemented (**Deferred**: additional
backends - see section 6 below). `usdk-cli` is implemented (section 7).
`usdk-binding-<language>` is **Deferred**: not implemented in this
release - the ABI is designed to make one straightforward
(`docs/ABI.md`), but building an actual binding (Python/Node/etc.) was
out of scope for the time available; nothing in the core, contracts, or
role libraries assumes a binding exists.

The dependency graph is exactly as documented in `docs/PACKAGES.md`,
including the `usdk-cli` exception explained there (a support package
linking all three constituents directly is permitted; the constituents
not linking each other is the actual requirement, and is enforced by
there being no such edge anywhere in the CMake target graph).

## 4. The trilateral agreement protocol

**Implemented** and tested (`docs/VALIDATION.md`'s `test_core_consensus`
entry): candidate structure with every required field; unanimous
ACCEPT/REJECT/ABSTAIN vote rule; party identity via loaded-module-handle
(documented as sufficient only for the in-process trust model, not a
general authentication mechanism - `docs/CONSENSUS_PROTOCOL.md` "Party
identity"); duplicate-vote and party-conflict handling; stale-round/
replay rejection via a persistent on-disk journal (survives process
restart, not just in-memory state); candidate-mutation detection via
per-vote digest comparison; bounded retries as a caller-side convention
(not enforced by `usdk-core` itself - stated as a deliberate scope
choice in `docs/CONSENSUS_PROTOCOL.md` "Bounded retries and
cancellation"); a two-phase journal write (`COMMITTED` then
`DISPATCHED`/`DISPATCH_UNKNOWN`) making a lost-acknowledgement dispatch
outcome detectable via `usdk_core_recover_incomplete_rounds` (tested);
an explicit idempotency-key contract for dispatch functions (documented,
and followed by the one dispatch function actually implemented -
`dispatch_write_file` in `src/cli/main.c` - not mechanically enforced for
an arbitrary caller-supplied function, which is stated plainly rather
than claimed).

**Partial**, stated honestly in `docs/CONSENSUS_PROTOCOL.md` itself:
crash/restart handling was tested only *sequentially* (kill the
in-process `usdk_core_t`, reopen the same `runtime_dir`), never with two
real OS processes racing against the same journal file concurrently - no
file locking exists on the journal, so concurrent-process correctness is
unverified, not merely untested-but-assumed-fine.

**Not claimed** (per the task's explicit instructions, restated in
`docs/CONSENSUS_PROTOCOL.md` "Tradeoffs and non-goals"): Byzantine fault
tolerance; that unanimous agreement proves the underlying response is
true, safe, or correct; that dispatch is exactly-once (it is at-least-
once plus a caller-honored idempotency key).

## 5. The dynamic C ABI

**Implemented** and tested: C11 baseline; opaque handles with explicit
create/destroy; fixed-width types throughout every public struct
(`include/usdk/types.h`); ABI version + `struct_size` negotiation,
checked in that order, before any vtable field is read
(`src/ffi/ffi.c`/`usdk_ffi_load`) - tested against a real deliberately-
incompatible module (`test_ffi_abi_mismatch`); explicit `usdk_status_t`
codes, never a bare `int`/`bool`; documented ownership (`usdk_buffer_t`
borrowed vs. `usdk_owned_buffer_t` owned, paired with
`usdk_owned_buffer_release`); `usdk_plugin_query_v1` entry point,
returning a role- or driver-shaped vtable selected by the caller based on
the descriptor's declared role (`docs/ABI.md` "Entry point"); platform
loading isolated behind one internal interface
(`src/ffi/platform_load.h`, `LoadLibraryExW`/`dlopen` implementations
never called anywhere else); full transitive manifest dependency
resolution via a real topological sort with cycle detection (Kahn's
algorithm, tested against a genuine diamond graph and a genuine cycle -
`test_manifest`), not a shortest-path substitute; a separate,
byte-explicit wire encoding for the one thing that needs to survive
outside a single process's memory (the candidate digest input -
`include/usdk/wire.h`) kept deliberately distinct from the in-process
ABI structs; C++ compatibility guards on every public header
(`USDK_BEGIN_DECLS`/`END_DECLS`, untested by an actual C++ translation
unit in this task, but mechanically simple enough that this is a
Not-tested gap, not a Partial one).

**Explicitly not claimed**, per the task's own instruction and restated
in `docs/ABI.md` "Entry point"/"Trust model": that arbitrary C function
signatures can be discovered or called safely without prior signature
metadata (every callable member of `usdk_role_vtable_t`/
`usdk_driver_vtable_t` has a fixed, compile-time-known signature); that
ABI validation is isolation against an actively malicious module (loading
a native library can run its own init code before any check runs, and a
module that lies about its capabilities can still crash the host when
actually invoked - stated plainly, not glossed over).

**Not tested / Partial**: per-call cancellation tokens do not exist in
this release (only the consensus round's own deadline does) -
acknowledged directly in `docs/ABI.md` "Cancellation, deadlines, and
resource bounds" as a real gap, not silently assumed solved. Callback-
draining/reentrancy rules are documented (`docs/ABI.md` "Threading and
callback rules") but not exercised by a test that actually attempts
reentrant or concurrent calls.

## 6. Vertical slice

**Implemented** and tested (`docs/VALIDATION.md`): input -> structured
evidence (`usdk_perceive_observe`) -> candidate response
(`usdk_deliberate_propose`, via the fixture driver) -> three party
verdicts -> committed output or explained refusal, exactly as specified.
All four required cases are implemented and pass: unanimous accept and
commit (with a real dispatched side effect - a file written into a
temporary runtime directory, keyed by `candidate_id` for idempotency);
one candidate rejected for insufficient permission (a constraint outside
`usdk-verify`'s configured allow-list); one deadline/unavailable-party
case (an already-expired deadline, chosen deliberately over simulating an
actual hung call - `src/cli/main.c`'s comment on why); one
altered-candidate case that cannot commit (a vote's recorded digest
tampered before submission, exercising the same mutation-detection path
a real in-memory alteration would).

**The one driver is an offline fixture** (`usdk-driver-fixture`),
deterministic, and every response it produces is prefixed with a literal
`[usdk-driver-fixture]` marker (checked by `test_deliberate`) so it can
never be mistaken for real model inference, per the task's explicit
instruction. **No real local-inference driver was added** - this
environment has no confirmed-available local model runtime, and the task
explicitly makes a real driver conditional on one already being present;
inventing one would have meant either fabricating "real" inference
results or spending remaining time on infrastructure (downloading/
serving a model) rather than the SDK itself. This is a genuine scope gap,
not a hidden one.

## 7. Developer tools

**Implemented**: `usdk --help`/`-h`/`help` (usage to stdout, exit 0);
`usdk doctor --json`; `usdk inspect <module> --json`; `usdk validate
--config <path> --json`; `usdk demo --scenario trilateral-consensus
--json`. Stable exit codes (0 success, 1 runtime failure, 2 usage error),
JSON on stdout when `--json` is passed, diagnostics on stderr - tested
via the `cli_*` CTest entries (`docs/VALIDATION.md`).

A minimal C consumer: **Implemented** - `examples/trilateral_integration/main.c`
is exactly this (the complete integration example), and each
`examples/*_standalone/main.c` is a minimal consumer of one constituent.
Guides for adding a driver, adding a binding (scope/why-not), and
diagnosing a rejected round: **Implemented** - `docs/GETTING_STARTED.md`.

## 8. Build and install

**Implemented and tested** on UCRT64 (`docs/VALIDATION.md`): target-based
CMake, `Makefile` wrapper (`make`/`make test`/`make install`/`make
clean`/`make help`), a single shared output directory per build so
Windows DLL search rules resolve sibling modules correctly
(`CMakeLists.txt`'s comment on `CMAKE_RUNTIME_OUTPUT_DIRECTORY`),
headers/libraries/CLI/manifests/examples/docs/license all installed
(`cmake --install`), `UsdkConfig.cmake`/`UsdkTargets.cmake` exported for
downstream `find_package(Usdk)` use, and `OBICALL_EMBED_SOURCE_DIR_FALLBACK`-
equivalent care taken to avoid compiled-in developer paths: **checked
directly** - none of `src/*/*.c` embeds `PROJECT_SOURCE_DIR` or any other
build-machine-specific absolute path into a shipped binary (unlike a
similar issue found and fixed in a sibling OBINexus project's packaging
work, not part of this task's research, applied here proactively by not
introducing the pattern in the first place). Runtime state
(`runtime_dir`, the round journal) is always a caller-chosen, separate,
writable directory - never the install prefix.

**Linux and macOS**: CMake configuration prepared (`CMakeLists.txt` is
portable C11; every platform-specific file is `#if defined(_WIN32)`/
`#else` with the `#else` branch using only POSIX APIs) but **not built or
tested** - claiming otherwise would violate the task's own instruction
not to claim untested platforms pass.

**MSVC**: not attempted in this task (this project's CMake does not
special-case MSVC, but `dirent.h`-free directory scanning
(`src/ffi/manifest.c`'s `#if defined(_WIN32)` branch uses
`FindFirstFileA`, not `dirent.h`) was written with MSVC compatibility in
mind - untested, not claimed).

## 9. Verify behavior

See `docs/VALIDATION.md` for the full, itemized list of what
`test_core_consensus`, `test_manifest`, `test_ffi_*`, and each role's
unit test actually check - it maps directly onto every bullet in the
task's section 9 (independent constituent builds; ABI mismatch and
missing-capability rejection; complete manifest dependency resolution;
lifecycle/ownership; unanimous acceptance; rejection and abstention;
missing party and deadline expiry; duplicate/stale/replayed votes;
candidate mutation after voting; installed example execution - covered
by the `cmake --install` pass in `docs/VALIDATION.md`).

**Not implemented / Partial**, stated plainly: "prevention of duplicate
action dispatch" is implemented as an idempotency-key *contract* the one
real dispatch function honors (`docs/VALIDATION.md`), not a mechanism
`usdk-core` enforces for an arbitrary caller-supplied one - there is no
test proving a second, independent dispatch call with the same key is
actually blocked at the `usdk-core` layer, because `usdk-core` does not
attempt to block it (that responsibility is documented as the dispatch
function's, per `docs/CONSENSUS_PROTOCOL.md`). Sanitizer runs: attempted,
unavailable in this toolchain (`docs/VALIDATION.md`) - not fabricated.

## 10. Deliverables

All required files exist: `docs/RESEARCH_REVIEW.md`,
`docs/ARCHITECTURE.md`, `docs/PACKAGES.md`, `docs/CONSENSUS_PROTOCOL.md`,
`docs/ABI.md`, `docs/GETTING_STARTED.md`, `docs/VALIDATION.md`, this
file, public C headers under `include/usdk/`, `CMakeLists.txt` +
`Makefile`, tests under `tests/`, the one driver under
`src/driver_fixture/`, and examples under `examples/`.

## Summary: what the supplied research supports vs. what remains unresolved

See `docs/RESEARCH_REVIEW.md` section 4 for the full synthesis. In short:
the research supports the `perceive -> deliberate -> verify` pipeline
shape (an independently-arrived-at match in `obiai-main`'s own highest-
quality code path); it supplies **no** existing three-party *voting*
protocol to adapt (the closest analogues are a single-active-authority
escalation cascade and a threshold-quorum mechanism with a fake signature
check); it supplies a strong, repeated, three-way-corroborated negative
lesson (every "AuraSeal" cryptographic-validation claim across all three
archives is fake or unimplemented) that directly shaped this project's
own explicit digest-is-not-authentication design. Numeric claims this
project does not import as proven: the "95.4%" consensus threshold, the
AEGIS-PROOF-1.2 traversal-cost non-negativity theorem (checked directly
and found insufficiently justified in every copy reviewed), and every
unsourced percentage claim ("85% improvement" etc.) found repeated
verbatim across multiple documents in multiple archives with no dataset,
citation, or method in any of them.
