# usdk (Lua binding) - UNTESTED

LuaJIT FFI bindings for the native USDK C ABI, mirroring
[`packages/binding-python`](../binding-python/README.md) in scope
(usdk-perceive only, plus `usdk_candidate_create` from
usdk-contracts).

## This binding has not been run

No Lua or LuaJIT runtime exists anywhere in this development
environment. Checked directly, all with no result:

```bash
which lua luajit lua5.1 lua5.3 lua5.4
```

(a stray `C:\Program Files (x86)\Lua\5.1\` directory is on `PATH` but
contains no `lua.exe`/`lua51.exe`; the directory itself is a leftover,
not a usable install) and no system package manager (`pacman`, `choco`,
`winget` were not queried further once `pacman` itself was confirmed
absent) was used to install one, per this task's explicit instruction
not to download tooling to work around a missing dependency.

`usdk.lua` is therefore written from the same, already-verified struct
layout and ABI knowledge as `packages/binding-python/usdk/_ffi.py` -
every field name, order, and width was cross-checked against
`include/usdk/{types,candidate,vote,plugin,perceive}.h` while writing
that Python binding, and that binding's 8 tests pass for real against
`build/bin/usdk_perceive.dll` and `build/bin/libusdk_contracts.dll`
(see its README). The LuaJIT `ffi.cdef` block in `usdk.lua` declares
the identical layout. What has NOT happened is running this file
through an actual LuaJIT interpreter - no syntax check, no execution,
no ABI call. Treat it as a design-complete, unverified port, not a
tested artifact.

## If a LuaJIT runtime becomes available

```lua
local usdk = require("usdk")
local contracts = usdk.Contracts.new("build/bin/libusdk_contracts.dll")
local perceive = usdk.Perceive.new("build/bin/usdk_perceive.dll")

local evidence = perceive:observe("sensor reading: 22.4C", 0.05)
local candidate = contracts:make_candidate{
    session_id = "s1", round_id = 1, candidate_id = "c1",
    proposed_response = "It's 22.4C.",
    evidence_refs = { evidence },
    constraint_ids = { "workspace:temp-only" },
}
local vote = perceive:vote(candidate)
print(vote.verdict, vote.reason_code)  -- expected: accept  evidence-verified
perceive:close()
```

The first thing to run against a real interpreter, before trusting
this binding for anything else, should be a direct LuaJIT port of
`packages/binding-python/tests/test_perceive.py`'s cases (same DLLs,
same expected verdicts/reason codes) - that is the fastest way to
either confirm this file is correct or find the first real bug in it.
