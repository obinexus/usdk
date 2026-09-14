# UAgent package table

Companion to `docs/UAGENT_ARCHITECTURE.md`. One row per `packages/*`
directory added for the browser/UAgent extension (the original native
C SDK's own support packages - `usdk-contracts`, `usdk-core`, etc. as
C libraries - are covered separately in `docs/PACKAGES.md`; this table
is the JS/Python/Lua layer). "Runtime loading" says how a capability
actually reaches a running conversation: **static** (imported directly
by name in source), **dynamic `import()`** (resolved by `@usdk/loader`
from a manifest at runtime - the mechanism the task requires at least
one path genuinely exercise), **DI** (constructed elsewhere, passed in
as a constructor/factory argument - never imported by the consumer),
or **N/A** (not loaded into a conversation at all - a binding or tool).

| Package | Purpose / key exports | Language / host | Mandatory deps | Runtime loading | Install / build artifact | Example + test |
|---|---|---|---|---|---|---|
| `contracts` | Shared types, `createCandidate`, `candidateDigestMatches`, `Status`, `UsdkError`, `Role`, capability check helpers. Mirrors `include/usdk/{candidate,vote,plugin,wire}.h` where a JS equivalent exists. | JS, any host | none | static | `packages/contracts/src/*.mjs`, no build step (native ESM) | `test/contracts.test.mjs` (10 tests) |
| `core` | `Core` (round state machine: open -> collect votes -> decide -> commit/reject), `MemoryJournal`/`FileJournal`. | JS, any host | `contracts` | static | same as above | `test/round.test.mjs` (12 tests, incl. a real `mkdtemp` restart-simulation test) |
| `loader` | `ManifestRegistry` (Kahn's-algorithm dependency resolution, cycle/version rejection), `loadPlan` (dynamic `import()` of resolved modules). | JS, browser or Node | `contracts` | N/A (this package *is* the loading mechanism) | same | `test/registry.test.mjs`, `test/browser-loader.test.mjs` (10 tests) |
| `perceive` | `create()` -> `{observe, vote}`. Evidence-match/no-evidence-cited checks. | JS, any host | `contracts` | static (imported by `host-browser`) | same | `test/perceive.test.mjs` |
| `deliberate` | `create({llmDriver})` -> `{propose, proposeDirect, vote}`. Self-consistency check. | JS, any host | `contracts` | static; `llmDriver` itself is DI | same | `test/deliberate.test.mjs` |
| `verify` | `create({allowedConstraints, maxUncertainty})` -> `{vote}`. Evidence-sufficiency + permission check. | JS, any host | `contracts` | static | same | `test/verify.test.mjs` |
| `capability-llm` | Interface contract only: `checkLlmCapability`/`assertLlmCapability`. No implementation. | JS, any host | `contracts` | N/A (contract, not loaded) | same | `test/capability-llm.test.mjs` |
| `capability-voice` | Contract: `startListening`/`speak`/`stopSpeaking`/`isAvailable`/`label`. | JS, any host | `contracts` | N/A | same | none dedicated (exercised via `driver-voice`'s tests) |
| `capability-vision` | Contract: `describe`/`isAvailable`/`label`. | JS, any host | `contracts` | N/A | same | none dedicated (exercised via `driver-vision`'s tests) |
| `capability-a11y` | Contract: `announce`/`setStatus`/`isAvailable` (always true by construction). | JS, any host | `contracts` | N/A - wired directly by the UI, never through the loader, so it cannot depend on another capability's load (see `UAGENT_ARCHITECTURE.md`) | same | none dedicated |
| `capability-robotics` | Contract + `validateRoboticsAction`, `ROBOTICS_LIMITS`. | JS, any host | `contracts` | N/A (validation helper, used by both proposal and execution sides) | same | `test/robotics.test.mjs` (5 tests) |
| `driver-llm-fixture` | Deterministic, labeled fixture LLM driver. | JS, any host | `contracts`, `capability-llm` | dynamic `import()` (manifest `entry`) | same | `test/fixture.test.mjs` |
| `driver-llm-local` | Second, distinctly-named LLM driver slot for a real local-inference backend if wired in later - does not claim real inference in this release. Browser-only. | JS, browser | `contracts`, `capability-llm` | dynamic `import()` | same | `test/driver-llm-local.test.mjs` |
| `driver-voice` | Web Speech API driver (`SpeechRecognition`+`speechSynthesis`), with a bounded fallback timeout on `speak()` (see `UAGENT_ARCHITECTURE.md` "Voice, accessibility, and conversation" - a real hang found and fixed via live browser testing). Browser-only; degrades honestly (`isAvailable()` false) elsewhere. | JS, browser | `contracts`, `capability-voice` | dynamic `import()` | same | `test/driver-voice.test.mjs` (7 tests) |
| `driver-vision` | Camera/vision driver contract implementation. Browser-only. | JS, browser | `contracts`, `capability-vision` | dynamic `import()` | same | `test/driver-vision.test.mjs` |
| `driver-robotics-sim` | Simulated bounded-movement driver; `execute()` re-validates bounds independently of proposal-time validation; `emergencyStop()` bypasses deliberation and cancels in-flight movement. No physical hardware. | JS, any host | `contracts`, `capability-robotics` | dynamic `import()` | same | `test/robotics-sim.test.mjs` |
| `host-browser` | `ConversationSession` - the Browser-local profile's orchestrator: resolves manifests, loads capabilities, constructs the three roles + `Core`, runs the full turn lifecycle, gates every side effect through post-commit dispatch. Reused unmodified by `host-local`. | JS, browser (also runs correctly under Node - proven by its own test suite) | `contracts`, `core`, `loader`, `perceive`, `deliberate`, `verify`, `capability-robotics` | orchestrates dynamic `import()` via `loader` | same | `test/session.test.mjs` (9 tests, incl. the estop/`executionCompleted` regression) |
| `host-local` | Connected profile's server: HTTP+JSON wrapper around `host-browser`'s `ConversationSession`, pairing secret, origin allowlist, per-session bearer auth, `/native/doctor` + `/native/inspect/:module` bridge to real `usdk.exe`. | JS, Node.js only | `contracts`, `host-browser` | reuses `host-browser`'s dynamic loading; `/native/*` spawns `usdk.exe` (real dynamic C-ABI loading, out of process) | same; started with `node -e "import('@usdk/host-local').then(m=>m.startHostLocal())"` | `test/server.test.mjs` (12 tests against a real listening server, incl. two that assert on real `usdk.exe` JSON output) |
| `binding-javascript` | `ConnectedSession` - the Connected profile's browser client. `fetch`-only, zero deps. | JS, browser | none | N/A (a client, not a loaded capability) | same | `test/connected-session.test.mjs` (7 tests against a real `host-local` server) + verified live in a real browser tab (`packages/uagent/public/connected.html`) |
| `binding-python` | ctypes bindings for the native C ABI - `usdk-perceive` role only this release, plus `usdk_candidate_create`/`usdk_monotonic_ns` from `usdk-contracts`. | Python 3.9+, local process only | none (stdlib `ctypes` only) | N/A (calls the native ABI directly, in-process) | `pip install -e .` optional; usable directly from a checkout | `tests/test_perceive.py` (8 tests against the real `build/bin/usdk_perceive.dll` + `libusdk_contracts.dll`) |
| `binding-lua` | LuaJIT FFI bindings, same scope as `binding-python`. **Untested** - no Lua/LuaJIT runtime exists anywhere in this environment (see its README for the exact commands run to confirm this). | LuaJIT, local process only | none | N/A | `usdk.lua`, `require`-able directly | none runnable here |
| `uagent` | The application: `index.html`/`app.mjs` (Browser-local) and `connected.html`/`connected.mjs` (Connected), plus shared `style.css`. Not a library - the actual conversation UI. | Static HTML/CSS/JS | `host-browser`, `driver-llm-fixture`, `driver-voice`, `driver-robotics-sim` (Browser-local); `binding-javascript` (Connected) | both entry points exercise dynamic `import()` for their driver manifests | served directly in dev (`npm run dev`) or from `dist/` (`npm run build && npm run serve`) | verified live in a real browser tab, both profiles, both from source and from a built `dist/` (see `UAGENT_ARCHITECTURE.md`) |
| `devtools` | `build-dist.mjs` (discovers the browser-relevant package set from `uagent/public/*` itself, copies into `dist/`), `serve-dev.mjs`/`serve-dist.mjs` (zero-dep static servers), `cli.mjs` (`dev`/`build`/`serve-dist` dispatcher). | JS, Node.js only | none | N/A (build tooling) | `bin: usdk-devtools` | no automated test (the build/serve pipeline itself was run and its *output* verified live in a browser - see `UAGENT_ARCHITECTURE.md` "DO NOT EJECT") |

## Packages with no automated test, and why

- `capability-voice`, `capability-vision`, `capability-a11y`: pure
  interface contracts (a `check*`/`assert*` function pair each,
  ~15 lines); exercised indirectly through the driver packages that
  implement them (`driver-voice`, `driver-vision`) rather than tested in
  isolation, since there is no behavior of their own beyond a shape
  check.
- `devtools`: no `node:test` file, but its actual output (`dist/`) was
  built and served for real, then loaded in a real browser tab on a
  different port than the dev server, with a full conversation turn and
  a robotics action both completing successfully - see
  `UAGENT_ARCHITECTURE.md` "DO NOT EJECT" for the specifics. Adding a
  `node:test` around `buildDist()` asserting the expected files exist
  under `dist/` would be a reasonable next addition; it was not written
  in this pass because the live-browser verification already exercises
  the thing that actually matters (does the built output run).
- `uagent`: an application, not a library - "tested" here means the
  live-browser interaction sequences described throughout
  `UAGENT_ARCHITECTURE.md`, not a `node:test` file (there is no
  meaningful way to unit-test a static HTML page's DOM wiring without a
  browser).
- `binding-lua`: see its own README - no Lua/LuaJIT runtime exists in
  this environment to run a test against.
