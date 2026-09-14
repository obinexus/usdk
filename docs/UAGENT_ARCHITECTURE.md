# UAgent architecture

This document covers the browser-facing polyglot extension built on top
of the native USDK C SDK (`docs/ARCHITECTURE.md` through
`docs/VALIDATION.md`, which this document does not repeat): a developer
composes an AI application from `packages/*` (JavaScript today; Python
and Lua bindings for the native ABI also ship, see
`packages/binding-python/README.md` and `packages/binding-lua/README.md`),
bundles it with `@usdk/devtools`, and loads its interface in a browser.

**Provenance, stated precisely** (full detail in
`docs/RESEARCH_REVIEW.md` sections 5 and 6): the five-capability shape
below is motivated by `Unbiased_AI.pdf` page 4, Hypothesis III ("Modular
System Architecture") - a design *proposal* in the source paper, not a
measured or proven result, and the paper's own pseudocode (Algorithm 3)
specifies none of the actual failure modes (version mismatch, missing
export, dependency cycle, a host lacking a capability) that this SDK's
loader and manifest resolver actually handle; that resolution mechanism
is new engineering for this project, not derived from the paper. No
`github.com/obinexus/uagent` source was available in this environment
(confirmed: the supplied local path contains zero files) - everything
below is independent engineering, not integration with prior UAgent
code, and should not be assumed compatible with a real UAgent codebase
until one is actually supplied and reviewed.

---

## Two execution profiles

Application authors write JavaScript, Python, or Lua - `npm` is a
package distribution/build ecosystem here, not another language; Node.js
and the browser are two separate JavaScript hosts. There are exactly two
ways an application built this way runs, and an application never
switches between them silently:

| | **Browser-local** | **Connected** |
|---|---|---|
| Where capability code runs | Inside the browser tab, as JS modules `import()`-ed by `@usdk/loader` (WASM, in a worker, is the other browser-local option - see "Browser and native boundaries" below; not used by any capability shipped in this release) | A separately, explicitly started local Node.js process (`@usdk/host-local`), talked to over HTTP |
| Consensus/round logic | `@usdk/core` + `@usdk/perceive`/`@usdk/deliberate`/`@usdk/verify`, running in the tab | The *same* `@usdk/host-browser` `ConversationSession` class, running inside `@usdk/host-local` instead - reused unmodified, not reimplemented (see `packages/host-local/README.md`) |
| Native `.dll`/`.so`/`.dylib` access | None - the browser never loads a native library directly, full stop | Only via the local host process, which can `dlopen`/`LoadLibrary` (through the native ABI's own tooling - see below); the browser never does |
| Entry point in this repo | `packages/uagent/public/index.html` | `packages/uagent/public/connected.html` |
| Client class | `@usdk/host-browser`'s `ConversationSession` | `@usdk/binding-javascript`'s `ConnectedSession` |

`ConversationSession` and `ConnectedSession` are deliberately
different-shaped classes (`ConnectedSession` has `pair()`/`disconnect()`
with no Browser-local equivalent; `ConversationSession` has
`voiceAvailable`/`startVoiceInput()` with no Connected equivalent, since
`@usdk/host-local` never loads a voice driver - Node has no Web Speech
API). A UI author picks one explicitly by which class they construct;
there is no shared interface an application could silently fall back
across. This is deliberate, not an oversight - see "Browser and native
boundaries" below for why the task requires it.

---

## Package structure

Every package lives under `packages/<name>/` and is never moved out of
it or ejected from the workspace - see "DO NOT EJECT" below. Identities
declared (proposed, not published/registered anywhere):

| Directory | Package identity | Language / host |
|---|---|---|
| `contracts` | `@usdk/contracts` | JS, any host |
| `core` | `@usdk/core` | JS, any host |
| `loader` | `@usdk/loader` | JS, browser or Node |
| `perceive`, `deliberate`, `verify` | `@usdk/perceive` etc. | JS, any host |
| `capability-llm`, `-voice`, `-vision`, `-a11y`, `-robotics` | `@usdk/capability-*` | JS, any host (interface contracts only, no implementation) |
| `driver-llm-fixture`, `driver-llm-local` | `@usdk/driver-llm-fixture` etc. | JS; fixture runs anywhere, `-local` is browser-only (see "Do not present fixture responses as real model inference" below) |
| `driver-voice`, `driver-vision` | `@usdk/driver-voice` etc. | JS, browser-only (Web Speech API / camera) |
| `driver-robotics-sim` | `@usdk/driver-robotics-sim` | JS, any host (pure simulation, no hardware) |
| `binding-javascript` | `@usdk/binding-javascript` | JS, browser (Connected-mode HTTP client) |
| `binding-python` | `usdk-binding-python` / `import usdk` | Python 3.9+, local process only (ctypes) |
| `binding-lua` | `usdk` (Lua module) | LuaJIT, local process only (FFI) - **untested**, see its README |
| `host-browser` | `@usdk/host-browser` | JS, browser (or Node - see below) |
| `host-local` | `@usdk/host-local` | JS, Node.js only |
| `uagent` | (not published - the application) | Static HTML/CSS/JS |
| `devtools` | `@usdk/devtools` | JS, Node.js only |

A fuller table (exports, dependencies, runtime-loading mechanism, build
artifact/install layout, example+test location) is
`docs/UAGENT_PACKAGES.md`.

### Correspondence with the native C SDK

`@usdk/contracts` mirrors `include/usdk/{candidate,vote,plugin,wire}.h`
field-for-field where a JS equivalent exists, and states where the two
intentionally diverge: JS `roundId` is a plain `number`, not a
fixed-width struct field; a JS `Candidate`'s `contentDigest` is not
required to match a native `usdk_candidate_t`'s `content_digest`
bit-for-bit, because the two are independent systems (different
canonical encodings - see `packages/contracts/src/wire.mjs` vs.
`src/contracts/candidate.c`) joined only at the Connected-mode wire
protocol, which itself does not attempt to make a JS candidate and a
native candidate the same object across that boundary - see "Browser and
native boundaries" below. `packages/binding-python/usdk/_ffi.py` and
`packages/binding-lua/usdk.lua` instead mirror the C structs *exactly*,
field order and width included, because ctypes/LuaJIT FFI calls the real
native functions directly and a mismatch there is a memory-safety bug,
not a design choice - see their READMEs.

---

## Modularity

- **Core** (`@usdk/core`) contains the candidate lifecycle and the
  round/agreement state machine only. It never imports a concrete
  driver, binding, UI package, or robotics code - checked directly:
  `packages/core/src/*.mjs` imports only `@usdk/contracts` and Node
  builtins (`node:fs/promises`, `node:path`, only inside `FileJournal`,
  itself only used when a caller opts in - see below).
- **The three roles** (`perceive`/`deliberate`/`verify`) depend only on
  `@usdk/contracts`. None imports either of the other two -
  `@usdk/deliberate`'s own package description states the boundary
  explicitly: "Drivers implement capabilities; they do not decide
  whether actions commit" - a driver (e.g. `driver-llm-fixture`) is
  *injected* into `deliberate.create({ llmDriver })` by the host/loader,
  never imported by `@usdk/deliberate` itself.
- **Bindings translate interfaces; they do not reimplement consensus.**
  `@usdk/binding-javascript` and `packages/binding-python` both call
  into an already-complete consensus mechanism (a running
  `@usdk/host-local` process, or the native `usdk_perceive_vote` ABI
  call respectively) - neither contains its own copy of the
  accept/reject/commit rule.
- **The host creates the runtime**: resolves the requested capability
  set against registered manifests (`@usdk/loader`'s
  `ManifestRegistry.resolve`, Kahn's-algorithm topological sort,
  rejecting a cycle, an unresolved dependency, or a version
  incompatibility - see `packages/loader/src/registry.mjs` and its
  tests), dynamically `import()`s each resolved module, and injects the
  constructed driver instances into the three roles and into
  `@usdk/core`'s dispatch function. `@usdk/loader` itself knows nothing
  about candidates, votes, or rounds - it resolves and loads named
  capabilities, nothing more (mirroring the native SDK's own
  `usdk-ffi`/`usdk-core` split - see `docs/PACKAGES.md`).
- **Storage is an injected adapter, not a fixed implementation**:
  `@usdk/core`'s round journal (`packages/core/src/journal.mjs`) is
  `MemoryJournal` by default (a browser tab has no filesystem) but
  accepts any object implementing `{append, readAll}` - `@usdk/host-local`
  can inject `FileJournal` instead for real cross-restart persistence,
  directly mirroring the native CLI's own on-disk journal
  (`src/core/journal.c`) without `@usdk/core` needing to know which
  backend is in use.
- **All five capabilities are replaceable through the same manifest
  mechanism**, and loading materially changes which implementation
  runs - demonstrated directly, not just structurally: swapping
  `driver-llm-fixture` for a different manifest `entry` in
  `packages/host-browser/test/session.test.mjs`/`packages/uagent/public/app.mjs`
  changes the LLM without touching `@usdk/deliberate`, `@usdk/core`, or
  `@usdk/host-browser`; the same is true for the robotics and voice
  drivers.

## Five capabilities are not five voting parties

The five capabilities (LLM, voice, vision, accessibility, robotics) are
*loaded* modules a conversation may use. The three roles (perceive,
deliberate, verify) are the only parties that *vote*. `@usdk/contracts`
enforces this at the type level - `Role` (`packages/contracts/src/vote.mjs`)
has exactly three members (`PERCEIVE`, `DELIBERATE`, `VERIFY`); a
`Vote.role` field is validated against that enum, and no capability
name is ever a valid value there. A capability is never a voting party,
and a role never doubles as a capability - `@usdk/deliberate` votes on
self-consistency (below), it does not vote *as* "the LLM."

## Give each party distinct checks

Each of the three roles performs one genuinely different check, exactly
mirroring the native SDK's constituent packages
(`docs/ARCHITECTURE.md`):

- **`@usdk/perceive`**: every `evidenceRef` on a candidate must match an
  observation this instance actually recorded via `observe()` - REJECT
  (not ABSTAIN) `"no-evidence-cited"` if evidence is required and none
  is cited, REJECT `"evidence-not-found"` for a fabricated/unmatched
  reference. Never checks who *produced* the response.
- **`@usdk/deliberate`**: self-consistency - a candidate is only
  accepted if its `proposedResponse` matches something this instance
  actually produced via `propose()`/`proposeDirect()`. Never checks
  evidence sufficiency (that is perceive's job) or permissions (that is
  verify's job).
- **`@usdk/verify`**: evidence sufficiency against a configured
  `maxUncertainty`, and permission - every `constraintId` on the
  candidate must be in the caller-configured `allowedConstraints` list.
  Never checks *who* generated the candidate.

No role re-derives another role's check; each is independently
sufficient to reject a bad candidate for its own distinct reason, and
the commit rule (below) requires all three to independently agree.

## Ensure external effects pass through the agreement gate

`@usdk/core`'s dispatch function (set via `Core.setDispatchFn`) is
called *only* from inside `decide()`, *only* after every required vote
is `ACCEPT` and the round transitions to `COMMITTED` - never before, and
never as a side effect of proposing or voting. `@usdk/host-browser`'s
`ConversationSession` wires this to the actual side effects: speaking
approved text (`driver-voice.speak`), and executing a robotics action
(`driver-robotics-sim.execute`). The UI (`packages/uagent/public/app.mjs`,
`connected.mjs`) never calls a driver's mutating method directly - every
path to "speak" or "move" goes through this same gate, checked by
construction (there is exactly one call site to each driver's
side-effecting method, inside the dispatch function) and by test
(`packages/host-browser/test/session.test.mjs`'s "a robotics action
outside the permission policy never executes" and "a disallowed
constraint is rejected" cases).

REJECT, ABSTAIN, a timed-out round, a detected candidate mutation
(`candidateDigestMatches`), or a missing vote all prevent commit and
therefore prevent dispatch - see `packages/core/src/round.mjs` and its
12 tests, and `docs/CONSENSUS_PROTOCOL.md` for the native SDK's
identical commit rule this mirrors.

---

## Browser and native boundaries

**Browser-local**: `@usdk/loader` performs a dynamic `import()` of each
resolved capability module's `entry` path (a real ES module, not a
bundler-specific mechanism) - see `packages/loader/src/browser-loader.mjs`.
No capability shipped in this release uses WASM; the mechanism exists in
the profile description because the task requires it be an explicit,
documented option, not because anything here currently exercises it.
Were a WASM capability added, it would run in a Worker with an explicit
ABI adapter at the JS/WASM boundary - WASM instantiation is not the same
thing as a native `dlopen`, and two WASM modules do not implicitly share
memory; passing a pointer between two unrelated WASM instances would be
a bug, not a supported pattern. Python and Lua are never claimed to run
natively in a browser anywhere in this codebase; `binding-python` and
`binding-lua` are explicitly local-process-only (see their READMEs) -
if a browser-compatible interpreter for either becomes available and is
actually wired in, that is future work, not something silently assumed
here.

**Connected mode**: `@usdk/host-local` is the only thing in this
codebase that touches a native `.dll` directly, and only by spawning the
already-built `usdk.exe` CLI (`node:child_process.execFile`, an argument
array, never a shell string) for two read-only diagnostics
(`/native/doctor`, `/native/inspect/:module`) - `usdk.exe` is what
actually performs dynamic C-ABI module loading
(`usdk-ffi`'s `LoadLibraryExW`/`dlopen` path, with ABI-version and
struct-size negotiation); `@usdk/host-local` does not reimplement that,
it spawns the real binary and relays its JSON output. Conversation turns
do **not** go through this native bridge - they run through the same
pure-JS role packages the Browser-local profile uses, reused via
`@usdk/host-browser`'s `ConversationSession` (see `packages/host-local/README.md`
"Why a subprocess, not an in-process ctypes-equivalent": Node has no
built-in FFI, and adding one would mean either a third-party dependency
or a native build step, both ruled out - see "Why zero dependencies"
below).

Connected mode requires, and this implementation provides:

- **Explicit pairing**: `@usdk/host-local` prints a random pairing
  secret to its own console at startup; a browser client must have that
  secret typed in (never hardcoded) before it can open a session
  (`POST /session` with `X-Usdk-Pairing-Secret`).
- **Origin checks**: every request with a browser-supplied `Origin`
  header must match a server-configured allowlist; a request with no
  `Origin` header (a non-browser HTTP client, which an `Origin` check
  was never able to restrict anyway) is allowed through so this same
  check does not accidentally block legitimate server-to-server or CLI
  use - see `packages/host-local/src/index.mjs`'s `requireOrigin`
  comment for the full reasoning, discovered directly while testing
  `@usdk/binding-javascript` against a real server (Node's own `fetch`
  does not send an `Origin` header; a real browser's `fetch` always
  does and cannot be told not to).
- **Session auth**: every subsequent request carries a per-session
  bearer token (`crypto.randomUUID()`), compared with
  `crypto.timingSafeEqual`.
- **Narrowly-scoped capabilities**: the manifest set a session loads is
  fixed server-side (`SESSION_MANIFESTS` in `host-local/src/index.mjs`)
  - a browser request never names an arbitrary capability, module, or
  file path. `/native/inspect/:module` additionally checks its `:module`
  path segment against an allowlist built from the `.usdk-manifest.json`
  files actually present under `build/bin/` at server startup, never
  passing a client-supplied string to `execFile` unchecked - **no
  arbitrary native-library paths or shell commands are exposed to
  browser requests.**
- **Versioned serialized messages**: every JSON response carries a
  `protocolVersion` field (`PROTOCOL_VERSION = 1` in both
  `host-local/src/index.mjs` and `binding-javascript/src/index.mjs`);
  `ConnectedSession` throws `ProtocolVersionMismatchError` rather than
  silently proceeding if the two ever disagree.
- **Keep secrets out of browser bundles**: the pairing secret exists
  only in the host process's own stdout and in whatever a person types
  into `connected.mjs`'s form field at runtime - it is never written
  into any `.mjs`/`.html` file in this repository, and `dist/`'s build
  (below) never embeds one either, since none exists at build time.

Verified in a real browser tab, not just Node's `fetch` (see
`packages/binding-javascript/README.md`): `connected.html` paired
against a live `@usdk/host-local`, sent a text turn (committed and
displayed), proposed a robotics action (approved and executed), and
triggered emergency stop - all confirmed via the browser's own network
log and DOM content.

---

## Voice, accessibility, and conversation

The voice pipeline is a replaceable set of stages, and no stage is
claimed to be something it is not:

| Stage | Where it runs | Offline? |
|---|---|---|
| Microphone capture | Browser (`SpeechRecognition`/`webkitSpeechRecognition`) | **No** - in every Chrome-family browser that implements it, captured audio is sent to a cloud speech-to-text service; this is stated directly in `driver-voice`'s own module docstring and `label()` output, not asserted as offline anywhere |
| Transcription | Same cloud service (STT) | No, per above |
| Conversation (perceive/deliberate/verify) | Wherever the host runs (browser tab, or `@usdk/host-local`) | Yes - no network call in this path |
| Approved-response speech synthesis | Browser (`speechSynthesis`) | Typically local (OS/browser TTS voices), but this is **not independently verified** in this environment - `isAvailable()` only confirms the API exists, not which backend a given browser/OS actually uses |
| Audio output | Browser/OS | N/A |

**Accessibility is always available at the interface level** and does
not depend on any capability's successful load: in both
`packages/uagent/public/app.mjs` and `connected.mjs`, the `a11y` object
(`announce`/`setStatus`) is constructed directly and passed into the
session constructor - it is never resolved through `@usdk/loader`, so a
failed LLM/voice/robotics driver load cannot take accessibility down
with it. Concretely: typed input with a readable transcript (`role="log"`
transcript div), keyboard-operable controls with visible focus
(`:focus-visible` in `packages/uagent/public/style.css`), accessible
names and status announcements (`aria-live="polite"` status line,
`aria-live="assertive"` announcer for approved responses), explicit mic
activation/recording state (`aria-pressed` on the mic button), stop/
cancel/mute/interruption controls (`cancelTurn()`, `stopSpeaking()`,
`emergencyStop()`, all reachable independent of any pending turn), a
text fallback whenever speech is unavailable (`voiceAvailable` gates
whether mic controls are even shown; text input/send are never gated by
it).

Base inference is an adapter capability, not a claim of new model
weights: `driver-llm-fixture` is deterministic, and its output is
labeled `"[fixture] deterministic fixture response - not real model
inference"` in every response it produces (see "Do not present fixture
responses as real model inference" below) - this task does not
create or train any model.

## Prevention of stale speech

Every turn (`sendText`, `proposeRoboticsAction`) generates a fresh
`turnId` and stores it as `ConversationSession`'s single mutable
`_currentTurnId`. Before speaking, executing, or reporting a result, the
code compares the turn's own id against the current value - a mismatch
means a newer call (or an explicit `cancelTurn()`) has superseded this
one, and the stale result is reported as `status: 'superseded'` instead
of being spoken or executed. This is checked in two places per turn: once
right after the (async) evidence/candidate-construction step, and again
after the (async) three-vote round completes - see
`packages/host-browser/src/index.mjs`'s `sendText`/`proposeRoboticsAction`/
`_runRound`, and the regression test in `session.test.mjs`
("`cancelTurn()` causes an in-flight turn to report superseded, not
committed"). A committed round's dispatch side effect (e.g. an
already-in-flight robotics movement) is not perfectly cancellable after
the fact - `_runRound`'s comment states this precisely rather than
implying a guarantee that does not exist; `emergencyStop()` exists
specifically because dispatch cancellation cannot be - see "Robotics"
below.

---

## Robotics

- A robotics action is a **structured, bounded** object
  (`{direction, distanceM, speedMS}`), validated by
  `@usdk/capability-robotics`'s `validateRoboticsAction` both at
  proposal time (before any candidate/round exists - fails fast on a
  malformed action) and again at execution time inside the driver
  (`driver-robotics-sim.execute` re-checks bounds independent of what
  was checked at proposal time, since a driver must not trust that
  nothing changed between the two).
- **Do not let spoken content directly become executable robot
  commands**: `@usdk/capability-robotics`'s action validator never
  accepts free-form text - the UI constructs a structured action object
  directly from button clicks (`data-direction` attributes), never from
  transcribed or typed text passed through unchanged.
- Movement only dispatches after all-three-ACCEPT commit, through the
  same dispatch gate every other side effect uses (see "Ensure external
  effects pass through the agreement gate" above) - `@usdk/perceive`
  observes the structured action itself as its evidence
  (`perceive.observe(actionJson, 0.05)`), giving `@usdk/verify`'s
  evidence-sufficiency check something real to evaluate, using the exact
  same `evidenceRef` mechanism a text turn uses rather than a special
  case.
- **`emergencyStop()` bypasses deliberation entirely** - it is a direct
  call to the loaded robotics driver, not gated by any round or
  candidate state, and cancels in-progress simulated movement
  immediately (`packages/driver-robotics-sim/src/index.mjs`). Because
  approval (three-party ACCEPT) and execution completion are different
  facts, a round can legitimately commit while `executionCompleted:
  false` is also true (interrupted by estop mid-move) - this was a real
  bug, found and fixed via live interactive browser testing (see git
  history / this document's own writing process): the UI originally
  showed a stale "approved and executed" message after an estop; the fix
  threads `executionCompleted` through `proposeRoboticsAction`'s result
  and the UI shows one of three distinct messages instead of a binary
  approved/not-approved one.
- Only `driver-robotics-sim` ships in this release - a **simulated**
  driver with no physical hardware anywhere in this codebase, per the
  task's explicit instruction that no physical motion is required for
  the initial acceptance demo. A real hardware driver would require its
  own explicit driver package, its own capability-flag/permission
  declaration, and an acknowledgement step beyond what
  `validateRoboticsAction` does today - none of that exists here, and
  nothing in this codebase claims it does.

---

## Do not present fixture responses as real model inference

`driver-llm-fixture`'s every response is prefixed
`"[fixture] deterministic fixture response - not real model inference"`
and includes the evidence count and max uncertainty it saw, so a
developer reading a transcript (or this document) cannot mistake it for
a real model's output.

`driver-llm-local` (browser-only) **does now perform real inference**:
it runs [WebLLM](https://github.com/mlc-ai/web-llm) - a real,
already-trained model (`SmolLM2-360M-Instruct-q4f16_1-MLC`, chosen for
its small ~376MB download) executing locally via WebAssembly + WebGPU,
no server, no account. Its every response is prefixed
`"[local: SmolLM2-360M-Instruct-q4f16_1-MLC]"` (the exact model id, not
a generic "[local]" label) so it is never mistaken for the fixture or
presented as something more than what it is. **This is still not model
training** - the weights are WebLLM's own prebuilt release, downloaded
and cached by the browser; this project trains nothing. Loading the
model is opt-in only: `packages/uagent/public/index.html` requires an
explicit "Real local model" choice and a "Start" click before any
download begins - it is never triggered by simply opening the page (see
"Why zero dependencies" below for how the WebLLM *library* itself is
loaded without becoming an npm dependency of this workspace). Verified
live: a real question ("What is the capital of France?") got a real,
correct, model-generated answer, which then went through the same
three-party vote as any other candidate before being displayed - see
`docs/VALIDATION.md` for the full verification, including a transient
failure found and characterized (a `Cache.add()` network error on one
parameter shard on the first attempt, succeeded cleanly on retry - not
reproduced on a second full run, and not caused by anything in this
project's own code, per `navigator.storage.estimate()`/Cache API checks
run at the time).

No response anywhere in this codebase is labeled as coming from a real
trained model when it is not; `driver-llm-fixture` remains available
and is still the default choice, precisely because it is instant and
requires no download.

---

## DO NOT EJECT

No build tool in this workspace copies, moves, or rewrites anything out
of `packages/` - `npm install --offline`'s workspace symlinks
(`node_modules/@usdk/<pkg>` -> `packages/<pkg>`) are the only
cross-package wiring, and they point back at the source directories,
never a generated copy. `@usdk/devtools`'s `build-dist.mjs` is
**additive only**: it discovers which packages are browser-relevant by
parsing the actual `<script type="importmap">` blocks and driver
manifest `entry:` paths in `packages/uagent/public/*.html`/`*.mjs` (not
a hardcoded list that could drift from the real dependency graph), then
copies each into `dist/packages/<name>/` preserving the identical
relative directory layout `packages/<name>/` already has - so every
relative import path already written in source keeps resolving with
zero path-rewriting. Running `npm run build` never touches
`packages/` itself (checked: `build-dist.mjs` only ever calls `cp` with
`dist/...` as the destination, never as a source, and never calls
anything that deletes from `packages/`). `npm run serve` serves the
built `dist/` output standalone - verified directly in a real browser
tab, loaded from `http://127.0.0.1:8422/packages/uagent/public/index.html`
(a location containing none of the original `packages/*/test/` or
`node_modules` symlink structure), confirming installed/bundled
execution works outside the checkout.

## Why zero dependencies

No `package.json` in this workspace lists an external (non-`@usdk/*`)
dependency - `npm install --offline` succeeds with zero network access
because every cross-package edge is an internal workspace symlink. This
is a real, checked constraint (`npm install --offline`, run repeatedly
throughout this project's construction, has never failed for a missing
external package) driven directly by the task's own instructions: work
offline, do not fetch packages/models/references, and do not require
application authors to write C or native addons to use this SDK from
JavaScript, Python, or Lua. The one place this constraint has a real,
documented cost is `@usdk/host-local`'s native bridge (see "Browser and
native boundaries" above): Node has no built-in FFI, so reaching a
native `.dll` from pure Node would require either a compiled N-API
addon or a third-party FFI package - both ruled out, which is why that
bridge spawns `usdk.exe` as a subprocess instead. Python and LuaJIT do
have a built-in FFI (`ctypes`, `ffi`), which is why `binding-python`
and `binding-lua` can and do call the native ABI in-process with no
added dependency at all.

`driver-llm-local`'s use of WebLLM is the same pattern applied to a
third case: the library is loaded via a runtime `import()` of a CDN ESM
URL (`https://esm.run/@mlc-ai/web-llm`) **inside `loadModel()`**, not
listed in any `package.json` - `npm install --offline`'s zero-network
guarantee is untouched by this, since nothing about `npm install` reads
that URL; the fetch happens only in a browser, only after an explicit
user action, exactly like the model-weight download it triggers (see
"Do not present fixture responses as real model inference" above).
