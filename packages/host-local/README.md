# @usdk/host-local

The Connected execution profile's local runtime host. See
`docs/UAGENT_ARCHITECTURE.md` "Browser and native boundaries" for the
Browser-local vs. Connected distinction; this package is the Connected
profile's server half - `@usdk/binding-javascript` is its browser client.

## Start it

```bash
node -e "import('@usdk/host-local').then(m => m.startHostLocal())"
```

or from code:

```js
import { startHostLocal } from '@usdk/host-local';
const { url, pairingSecret, stop } = await startHostLocal({ port: 8421 });
```

Prints the pairing secret to its own console - **this is the explicit
pairing step**: a person connecting a browser client must copy that
secret from this process's terminal into the browser UI. It is never
returned in any HTTP response body and never embedded in any
browser-bundled code.

## What it actually does

- Runs `@usdk/host-browser`'s `ConversationSession` - the exact same
  conversation/consensus/turn-cancellation logic the Browser-local
  profile uses - inside this Node process instead of inside a browser
  tab. It is reused unmodified because it is plain JS with no
  browser-only API dependency (already proven by
  `packages/host-browser/test/session.test.mjs` running it under
  `node --test`).
- Exposes it over a small JSON HTTP API (`/session`, `/session/:id/turn`,
  `/session/:id/robotics`, `/session/:id/estop`, `/session/:id/cancel`),
  gated by an Origin allowlist, the pairing secret above (required once,
  to create a session), and a per-session bearer token (required on
  every subsequent call to that session).
- Bridges two read-only diagnostics - `/native/doctor` and
  `/native/inspect/:module` - to the real `build/bin/usdk.exe` CLI via
  `node:child_process.execFile` (argument array, no shell). `usdk.exe`
  is what actually performs dynamic C-ABI module loading
  (`usdk-ffi`'s `LoadLibraryExW`/`dlopen` path, with ABI-version and
  struct-size negotiation) - this package does not reimplement that, it
  spawns the real binary and relays its JSON output. `:module` is
  checked against an allowlist built from the `.usdk-manifest.json`
  files actually present in `build/bin/` at startup, never passed
  through from the request unchecked.

## Why a subprocess, not an in-process ctypes-equivalent

Python (`packages/binding-python`) and LuaJIT (`packages/binding-lua`)
both have a built-in FFI (`ctypes`, `ffi`) that can `dlopen`/`LoadLibrary`
a native `.dll` directly from process memory. **Node.js has no built-in
equivalent** - reaching a native library from pure Node requires either a
compiled N-API/node-gyp addon (a native build step per platform, and a
third-party dependency to wrap it) or a third-party FFI package
(`ffi-napi`/`koffi`), both ruled out by this SDK's zero-external-
dependency constraint (see the root `package.json` and
`docs/UAGENT_ARCHITECTURE.md` "Why zero dependencies"). Spawning the
already-built `usdk.exe` is therefore the honest way for a Node host to
exercise real native dynamic loading without either adding a dependency
or fabricating a capability Node does not have.

**Conversation turns (`/session/:id/turn`, `/session/:id/robotics`) do
NOT go through this native bridge.** They run through the same pure-JS
`@usdk/perceive`/`@usdk/deliberate`/`@usdk/verify`/`@usdk/core`
implementation the Browser-local profile uses - not the native C role
libraries. Only `/native/doctor` and `/native/inspect` touch the actual
`.dll` files, and only as a read-only capability check, not as part of
the consensus round.

## Running the tests

```bash
node --test packages/host-local/test/*.test.mjs
```

`test/server.test.mjs` starts a real server on an OS-assigned port and
drives it with `fetch` - including two tests that require a *correct*
pairing secret and assert on the *real* JSON `usdk.exe doctor`/
`usdk.exe inspect usdk-perceive` return (not a mock), so a regression in
either the HTTP layer or the native bridge is caught.

## Known limitations

- No TLS - `http://127.0.0.1` only, matching the "local host" framing;
  do not expose this port beyond localhost/a trusted LAN without adding
  TLS and a stronger auth scheme first.
- No SSE/streaming endpoint - every request is a single request/response;
  fine for the fixture driver's near-instant latency, but a slower real
  LLM driver would need a streaming or polling addition later.
- Voice is not available in this profile (Node has no Web Speech API) -
  `SESSION_MANIFESTS` in `src/index.mjs` deliberately omits a voice
  driver rather than pretending one is loaded.
