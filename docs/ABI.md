# The USDK dynamic C ABI

Baseline: C11 (`CMAKE_C_STANDARD 11`, `CMAKE_C_STANDARD_REQUIRED ON`,
`CMAKE_C_EXTENSIONS OFF`). Nothing in the supplied research required or
justified a different standard for this layer -
`docs/RESEARCH_REVIEW.md` section 1.3.10/2.2 reviews the closest existing
C ABI work found in the research (`polycall.h`) and it is itself
ordinary C, no different-standard features used.

## Entry point

```c
USDK_EXPORT usdk_status_t USDK_CALL
usdk_plugin_query_v1(const usdk_descriptor_t** out_descriptor,
                      const usdk_role_vtable_t** out_vtable);
```

Every USDK-loadable module (a role implementation or a driver) exports
exactly this symbol, matching `include/usdk/plugin.h`'s
`usdk_plugin_query_v1_fn` typedef. It returns a versioned function table
for the roles/capabilities that module declares support for -
`usdk_descriptor_t` carries the ABI version, `struct_size` of both output
structs, a `role` field, a `party_id` string (see
`docs/CONSENSUS_PROTOCOL.md` "Party identity"), and capability flags; the
caller (`usdk-ffi`) validates all of this before touching a single field
of `*out_vtable` - see "Validating compatibility before use" below.

**This entry point returns a function table for the declared, fixed set
of roles/capabilities `usdk_role_vtable_t` defines. USDK does not claim,
and this design does not support, discovering or safely calling an
arbitrary C function whose signature is not already known at compile
time** - every callable member of `usdk_role_vtable_t` has a fixed C
signature declared in `include/usdk/plugin.h`. Calling into a module
still requires the module's implementation to actually match that
signature; a module that lies about its capabilities in its descriptor
but does not implement the matching function can still crash the caller
(see "Trust model" below) - `usdk_plugin_query_v1` establishes a
*contract*, it does not enforce that the module honors it.

## Opaque handles and lifecycle

Every stateful object is an opaque pointer with paired explicit lifecycle
functions - no struct field is ever accessed directly by a caller across
the ABI boundary:

```c
typedef struct usdk_role_instance usdk_role_instance_t;  /* opaque */

usdk_status_t usdk_role_create(const usdk_role_vtable_t* vt,
                                const usdk_config_t* cfg,
                                usdk_role_instance_t** out);
void          usdk_role_destroy(usdk_role_instance_t* inst);
```

`usdk_module_t` (a loaded library, `usdk-ffi`), `usdk_round_t` (an
open consensus round, `usdk-core`), and every role instance follow this
same shape. Ownership rule, stated once and followed everywhere: **the
function that returns a handle owns creating it; the paired `_destroy`
is the only valid way to free it; a caller never calls `free()` directly
on anything returned across the ABI.**

## Fixed-width types, buffers, and ownership

All wire-adjacent and ABI struct fields use `<stdint.h>` fixed-width
types (`uint32_t`, `int64_t`, `uint8_t`), never `int`/`long`/`size_t`
directly. Variable-length data crosses the ABI as an explicit
pointer-plus-length pair, never a NUL-terminated string alone:

```c
typedef struct usdk_buffer {
    const uint8_t* data;
    uint32_t       len;
} usdk_buffer_t;

typedef struct usdk_owned_buffer {
    uint8_t* data;
    uint32_t len;
    uint32_t cap;      /* for the matching release function, not the caller to inspect */
} usdk_owned_buffer_t;
```

A function that returns an `usdk_owned_buffer_t` documents, in its own
header comment, the exact release function the caller must call
(`usdk_owned_buffer_release`); a function that only *borrows* a buffer
for the duration of one call documents that instead. No function returns
a plain `uint8_t*`/`char*` the caller is expected to guess how to free.

## Status codes and error reporting

```c
typedef enum usdk_status {
    USDK_OK = 0,
    USDK_ERR_INVALID_ARGUMENT,
    USDK_ERR_ABI_VERSION_MISMATCH,
    USDK_ERR_STRUCT_SIZE_MISMATCH,
    USDK_ERR_CAPABILITY_NOT_FOUND,
    USDK_ERR_MODULE_LOAD_FAILED,
    USDK_ERR_MODULE_ENTRY_NOT_FOUND,
    USDK_ERR_DEPENDENCY_CYCLE,
    USDK_ERR_DEPENDENCY_UNRESOLVED,
    USDK_ERR_DEPENDENCY_VERSION_INCOMPATIBLE,
    USDK_ERR_STALE_ROUND,
    USDK_ERR_DUPLICATE_VOTE,
    USDK_ERR_PARTY_CONFLICT,
    USDK_ERR_CANDIDATE_MUTATED,
    USDK_ERR_DEADLINE_EXCEEDED,
    USDK_ERR_OUT_OF_MEMORY,
    USDK_ERR_IO,
    USDK_ERR_NOT_IMPLEMENTED
} usdk_status_t;

const char* usdk_status_string(usdk_status_t status);
```

Every fallible function returns `usdk_status_t` directly (never a bare
`int`/`bool` with an out-of-band error state) and, where a richer message
is useful, writes it into a caller-supplied fixed buffer plus length
(never returns an allocated string a caller must remember to free) -
matching the pattern already proven in the author's prior OBINexus C
project (`obicall`, not part of the supplied research - see
`docs/RESEARCH_REVIEW.md` section 1.2 on the unrelated naming collision).

## ABI version and struct-size negotiation

```c
typedef struct usdk_descriptor {
    uint32_t struct_size;          /* sizeof(usdk_descriptor_t) as built - checked first */
    uint32_t abi_version_major;
    uint32_t abi_version_minor;
    uint32_t role;                 /* USDK_ROLE_PERCEIVE / _DELIBERATE / _VERIFY / _DRIVER */
    char     party_id[USDK_ID_LEN];
    uint32_t capability_flags;
} usdk_descriptor_t;
```

`usdk-ffi` checks, in this order, before ever calling a vtable function:
(1) `struct_size` is at least the caller's compiled `sizeof(usdk_descriptor_t)`
for the version of the header the caller was built against - a smaller
value means the module is older than the caller can safely read and is
rejected with `USDK_ERR_STRUCT_SIZE_MISMATCH`; (2) `abi_version_major`
matches the caller's exactly, and `abi_version_minor` is greater than or
equal to the caller's minimum - a mismatch is
`USDK_ERR_ABI_VERSION_MISMATCH`. **This exact two-step check exists
because of a concrete bug found in the supplied research**
(`docs/RESEARCH_REVIEW.md` section 1.3.10): two files in `obi-sdk`
declare what is meant to be the same C struct (`polycall_solution`) with
different fields, and nothing in that codebase would have caught the
mismatch at load time. USDK's struct-size check is exactly the mechanism
that would have caught it.

## Threading and callback rules

Every function in `usdk_role_vtable_t` and `usdk_core.h` is documented,
in its own header comment, as either **not thread-safe** (the default -
caller must not call it concurrently on the same handle from multiple
threads) or **thread-safe** (explicitly stated where true, e.g.
`usdk_status_string`, which is pure). This release provides no internal
locking inside a role instance or a round - `usdk-core`'s consensus round
is designed to be driven by one thread; nothing here is validated under
concurrent access, and nothing claims to be.

Callbacks (currently: the dispatch callback in `usdk-core`,
`usdk_dispatch_fn_t`) are called synchronously, on the calling thread,
within the bound of the round's deadline check happening *around* the
call, not *inside* it (`docs/CONSENSUS_PROTOCOL.md` "Crash and restart
behavior" explains why a hang inside the call itself cannot be
interrupted by this design). A module must not call back into
`usdk-core` or another role's vtable from inside a callback it was given
- reentrancy is not supported and is not tested.

## Cancellation, deadlines, and resource bounds

The only deadline mechanism in this release is the consensus round's
`deadline_ns` (`docs/CONSENSUS_PROTOCOL.md`) - a monotonic value checked
by `usdk_core_round_decide()`. There is no per-call cancellation token
threaded through every vtable function in this release; a long-running
role implementation is expected to bound its own work against the
deadline it can read from the candidate it was handed. This is a real
gap relative to a fully general ABI and is recorded as such in
`docs/IMPLEMENTATION_STATUS.md`, not silently assumed solved.

## Keeping modules loaded, and safe shutdown

`usdk-ffi` reference-counts a loaded module (`usdk_module_t`) against the
handles/instances created from it; `usdk_ffi_unload()` on a module with
outstanding instances returns `USDK_ERR_INVALID_ARGUMENT` rather than
unloading underneath live callers. Safe shutdown order, as implemented
by `usdk-core`'s teardown path: destroy every role instance
(`usdk_role_destroy`) - which drains any outstanding callback the
instance might still be expected to receive before returning - then
unload every module (`usdk_ffi_unload`), then destroy the `usdk-ffi`
context itself. Destroying a module out of this order while an instance
from it is still alive is a caller error this release detects (via the
reference count) and reports, not one it silently tolerates.

## Trust model

**Loading a native library can execute its initialization code (DLL
`DllMain`/ELF constructors) before `usdk_plugin_query_v1` is ever called,
and a module that lies about its capabilities can still crash the host
process when actually invoked.** ABI-compatibility validation
(`docs/ABI.md` "ABI version and struct-size negotiation") checks that a
module's *declared shape* matches what the caller expects; it is not
isolation, a sandbox, or a safety property against an actively malicious
module. `docs/ARCHITECTURE.md` "What this is not" and
`docs/CONSENSUS_PROTOCOL.md` "Crash and restart behavior" restate this
same point from the architecture and protocol angles respectively,
because it is easy to read "ABI validation" as a stronger guarantee than
it is.

## Serialized messages vs. in-process structures

`usdk_candidate_t`, `usdk_vote_t`, and every other in-process ABI struct
contain no function pointers and are used directly, in memory, only
within one process's address space; they are never written to disk or
sent anywhere as-is (compiler padding and pointer widths are not portable
across that boundary). Where a value genuinely needs to survive outside
the process - the round journal (`docs/CONSENSUS_PROTOCOL.md` "Commit
records") and the module manifest format (below) - it is defined
separately, in `include/usdk/wire.h`, as an explicit byte-oriented
encoding (fixed-width fields written in a stated order, no raw pointers,
no padding), the same discipline the author's prior OBINexus C project
already established for this exact problem (not derived from the
supplied research, which - per `docs/RESEARCH_REVIEW.md` section
1.3.10 - has no working example of this distinction being maintained: the
one compiled artifact reviewed there is a `.so` of unclear provenance,
not a documented wire format).

## Module manifests and dependency resolution

A loadable module ships a small JSON manifest (`usdk_manifest_t` after
parsing) declaring: `name`, `version`, `role_or_capability`,
`abi_version_major`/`minor`, `artifact_path`, and `dependencies` (a list
of other capability names, each with a minimum version). `usdk_ffi_resolve()`
builds the full dependency graph across every manifest reachable from a
requested root set, resolves it with Kahn's algorithm (a real topological
sort over the complete graph, not a shortest-path heuristic - the task
explicitly warns against exactly that substitution, and against it
because a shortest path can miss a mandatory dependency reachable only
through a longer edge), and fails with `USDK_ERR_DEPENDENCY_CYCLE` or
`USDK_ERR_DEPENDENCY_VERSION_INCOMPATIBLE` rather than silently loading a
partial or inconsistent set.

## Platform loading

`usdk-ffi`'s public surface (`include/usdk/ffi.h`) never exposes
`LoadLibraryExW`/`GetProcAddress` or `dlopen`/`dlsym` directly - both are
implemented behind one internal interface
(`src/ffi/platform_load.h`/`.c`, compiled per-platform) so every other
component, including `usdk-core` and `usdk-cli`, calls the same
`usdk_ffi_load`/`usdk_ffi_symbol`/`usdk_ffi_unload` regardless of OS.
Windows loads with `LoadLibraryExW(..., LOAD_LIBRARY_SEARCH_DEFAULT_DIRS)`
(searches the loading module's own directory, the application directory,
and system directories - never PATH or the current directory), matching
`CMakeLists.txt`'s single shared output directory for every built
artifact (`docs/ARCHITECTURE.md`); Unix platforms load with
`dlopen(..., RTLD_NOW | RTLD_LOCAL)`.

## C++ compatibility

Every public header is wrapped in `USDK_BEGIN_DECLS`/`USDK_END_DECLS`
(`include/usdk/platform.h`, `extern "C" { ... }` under `__cplusplus`), so
the entire public C ABI is includable and linkable from C++ without name
mangling.
