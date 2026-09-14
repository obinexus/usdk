# usdk (Python binding)

ctypes bindings for the native USDK C ABI. Stdlib only - `ctypes`,
nothing else. No `pip install` step is required to use this package
from a checkout; `pyproject.toml` exists so it can be installed
editable (`pip install -e .`) if a consuming project wants `import usdk`
from outside this directory.

## Scope of this release

Only **usdk-perceive** is bound: create/destroy an instance, `observe()`
evidence, and `vote()` on a candidate (`usdk/__init__.py`'s `Perceive`
class), plus a `Contracts` class that wraps `usdk_candidate_create` from
`libusdk_contracts.dll` so a `Candidate`'s `content_digest` is computed
by the real native canonical-encoding + SHA-256, not a Python
reimplementation of it.

**usdk-deliberate and usdk-verify are not bound.** The native libraries
already exist and are loadable (`build/bin/usdk_deliberate.dll`,
`build/bin/usdk_verify.dll`) - the same struct layouts
(`Candidate`/`Vote`/`EvidenceRef`/`Constraint`) and the same
`ctypes.CDLL` + `argtypes`/`restype` pattern in `usdk/_ffi.py` apply to
them directly (`usdk_deliberate_create/destroy/propose/vote` and
`usdk_verify_create/destroy/vote` - see `include/usdk/deliberate.h` and
`include/usdk/verify.h`). They were left out of this release to keep
the reviewed surface area small and fully tested rather than shipping
three untested role bindings at once. Extending this file with a
`Deliberate`/`Verify` class each is the direct next step, not a
redesign.

**usdk-core (the round/journal orchestrator) is also not bound.** A
Python application that wants the full three-role trilateral-agreement
flow currently must drive `Perceive`/`Deliberate`/`Verify` votes itself
and apply the commit rule (`unanimous ACCEPT` -> commit) in Python; it
does not yet get `@usdk/core`'s round-lifecycle/journal machinery for
free the way the JavaScript binding does via `@usdk/core`. See
`docs/UAGENT_ARCHITECTURE.md` for where this sits relative to the
JS-side packages.

## Usage

```python
from usdk import Perceive, Contracts

contracts = Contracts("build/bin/libusdk_contracts.dll")

with Perceive("build/bin/usdk_perceive.dll") as perceive:
    evidence = perceive.observe(b"sensor reading: 22.4C", uncertainty=0.05)
    candidate = contracts.make_candidate(
        session_id="s1", round_id=1, candidate_id="c1",
        proposed_response=b"It's 22.4C.",
        evidence_refs=[evidence],
        constraint_ids=["workspace:temp-only"],
    )
    vote = perceive.vote(candidate)
    print(vote.verdict, vote.reason_code)  # "accept" "evidence-verified"
```

By default `usdk-perceive` requires at least one `evidence_ref` on a
candidate (`no-evidence-cited` REJECT otherwise) and REJECTs (not
ABSTAINs) any evidence reference it did not itself record via
`observe()` (`evidence-not-found`) - see `include/usdk/perceive.h`.
Pass `config_json=b'{"require_evidence":false}'` to `Perceive(...)` to
permit zero-evidence candidates instead.

## Running the tests

Requires a built native SDK (`make` at the repo root, or
`cmake --build build`) so `build/bin/usdk_perceive.dll` and
`build/bin/libusdk_contracts.dll` exist; the test module skips itself
with a clear reason if they don't.

```bash
python -m unittest tests.test_perceive -v
```

`tests/test_perceive.py` exercises the binding against the real DLLs
(not a mock) and mirrors `tests/unit/test_perceive.c`'s cases so the
Python surface is checked against the same behavior the native C test
suite already establishes.

## Struct layout and ABI safety

`usdk/_ffi.py` is the single place struct field order and
`argtypes`/`restype` are declared, matching
`include/usdk/{types,candidate,vote,plugin,perceive}.h` field-for-field
(verified against those headers directly while writing this binding -
`struct_size`, field order, and fixed-width types all match). If the C
ABI changes, this file is what needs updating; nothing else in this
package should redeclare a struct layout independently, since the
research review for this SDK (`docs/RESEARCH_REVIEW.md`) found that
kind of duplication is exactly how a native/binding struct mismatch
bug happens.
