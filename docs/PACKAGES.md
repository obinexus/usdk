# Packages

## Support packages

| Package | Responsibility | Why this boundary |
|---|---|---|
| `usdk-contracts` | Minimal public types, ABI struct declarations, protocol/party identifiers, status codes, and the wire-format helpers for serializing a candidate for hashing. Header-only plus a tiny implementation (digest helper, wire encode). | Everything else depends on this; it depends on nothing else in USDK. If two components disagree about a struct layout, it is because one of them did not build against the current `usdk-contracts`, not because the layout is ambiguous - there is exactly one definition. |
| `usdk-core` | The candidate lifecycle, the round state machine (open -> collect votes -> decide -> commit/reject), commit records, and idempotency-key bookkeeping for dispatched actions. Backend-independent: no interpreter headers, no device APIs, no dynamic-loader implementation. | This is the only place the consensus protocol's state machine is implemented. It orchestrates calls through role handles it is given - it does not itself decide how those handles were loaded. |
| `usdk-ffi` | Dynamic module discovery (`LoadLibraryExW`/`dlopen` behind one interface), ABI negotiation via `usdk_plugin_query_v1`, module manifest parsing, full transitive dependency resolution with cycle detection, and library lifecycle (load, keep-alive while calls/objects/callbacks are outstanding, safe unload). | Platform-loading code is deliberately isolated here so `usdk-core` and the three role libraries never call `LoadLibraryExW`/`dlopen` directly. `usdk-core` depends on `usdk-ffi` to *load* role modules; `usdk-ffi` does not depend on `usdk-core` or know what a "role" or "candidate" is - it resolves and loads named capabilities, nothing more. |
| `usdk-driver-<backend>` | Optional model/device/storage/execution adapters implementing a declared interface (e.g. `usdk-driver-fixture`, a deterministic candidate generator used by `usdk-deliberate` and by every test in this repository that needs reproducible output). | Drivers are loaded the same way role modules are - through `usdk-ffi` - so `usdk-deliberate`'s core logic never links a specific backend. Replacing the fixture driver with a real local-inference driver requires no change to `usdk-deliberate`, `usdk-core`, or the protocol. |
| `usdk-binding-<language>` | Optional language-native wrappers around the public C ABI. | Not implemented in this release - see `docs/IMPLEMENTATION_STATUS.md`. The C ABI is designed to make one straightforward (opaque handles, explicit lifecycle, no raw pointers over any serialized boundary), but no binding is shipped. |
| `usdk-cli` | Developer commands: `--help`/`-h`/`help`, `doctor`, `inspect <module>`, `validate --config`, `demo --scenario trilateral-consensus`. | Depends on `usdk-core` and `usdk-ffi` to do real work; contains no protocol logic of its own beyond argument parsing and JSON/text formatting. |

## The three constituent packages

`usdk-perceive`, `usdk-deliberate`, `usdk-verify` - see
`docs/ARCHITECTURE.md` for responsibilities and the required distinct
check each performs. Each depends only on `usdk-contracts` (for the
shared ABI types) and, where it needs one, `usdk-ffi` (to load a driver -
currently only `usdk-deliberate` does, to load `usdk-driver-fixture`).
None depends on `usdk-core` or on either other role.

## Dependency graph

```
                          usdk-contracts
                         /   |   |   |   \
                        /    |   |   |    \
              usdk-perceive  |   |   |  usdk-verify
                        \    |   |   |    /
                     usdk-deliberate    /
                              \        /
                            usdk-ffi
                               |
                           usdk-core
                               |
                       usdk-cli, examples,
                       tests/integration
```

Read top-to-bottom as "depends on, and is built after." Exact CMake
target edges (`target_link_libraries`), as actually declared:

| Target | Links against |
|---|---|
| `usdk_contracts` | (nothing in this project) |
| `usdk_perceive` | `usdk_contracts` |
| `usdk_deliberate` | `usdk_contracts`, `usdk_ffi` (to load a driver at runtime - not link one at build time) |
| `usdk_verify` | `usdk_contracts` |
| `usdk_ffi` | `usdk_contracts` |
| `usdk_core` | `usdk_contracts`, `usdk_ffi` |
| `usdk_driver_fixture` | `usdk_contracts` (built as a standalone loadable module, not linked into anything at build time) |
| `usdk_cli` | `usdk_core`, `usdk_ffi`, `usdk_contracts`, `usdk_perceive`, `usdk_deliberate`, `usdk_verify` |

`usdk_cli` links all three constituents directly, which is permitted:
the architecture forbids the three constituents from linking **each
other** (the actual "no interdependence" requirement below), not a
support package linking them. `usdk-cli`'s `demo` command uses each
role's full public API this way (including operations with no generic-
ABI vtable equivalent, like `usdk_perceive_observe`); its `doctor`/
`inspect`/`validate` commands separately exercise the dynamic-loading
path through `usdk-ffi` and manifest resolution instead, since checking
whether an arbitrary named module can be discovered and loaded is
exactly what those commands are for. See `src/cli/main.c`'s comment on
`run_round()` for the full reasoning.

**"No interdependence" means, precisely, as implemented here**:

- No circular dependencies - the table above is already a DAG; CMake
  itself would refuse a cycle in `target_link_libraries`, so this is
  enforced by the build, not merely documented.
- No mandatory cross-linking between the three constituents -
  `usdk_perceive`, `usdk_deliberate`, and `usdk_verify` share no edge in
  the table above, checked by inspection of `src/*/CMakeLists.txt`.
- No `usdk_core` dependency on a concrete binding or driver -
  `usdk_core` links only `usdk_contracts`/`usdk_ffi`; it discovers role
  and driver modules by name at runtime through `usdk_ffi`, and never
  names `usdk_perceive.dll`/`usdk_driver_fixture.dll` (or any other
  concrete module) in its own source. The module name it loads comes
  from the caller-supplied configuration (`usdk validate --config`,
  `usdk demo --scenario ...`), not a compiled-in constant.
- Shared contracts (`usdk_contracts`) and explicit one-way dependencies
  (`usdk_deliberate -> usdk_ffi`, everything `-> usdk_contracts`) are
  exactly the permitted exception the task describes, and are the only
  edges in this graph.

**The core does not depend on Python, a model vendor, a GPU framework, or
any specific driver.** Checked directly: `src/core/CMakeLists.txt` names
only `usdk_contracts` and `usdk_ffi` in `target_link_libraries`; nothing
under `src/core/` `#include`s a Python header, a vendor SDK header, or
`src/driver_fixture/*`. No target in this repository links Python at all
- `usdk-binding-python` is not implemented (see `docs/IMPLEMENTATION_STATUS.md`).
