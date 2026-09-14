# @usdk/binding-javascript

The Connected execution profile's browser client. Talks to an
explicitly-started `@usdk/host-local` process over `fetch` - no
bundler, no third-party HTTP client, no WebSocket. See
`packages/host-local/README.md` for the server half and
`docs/UAGENT_ARCHITECTURE.md` "Browser and native boundaries" for the
Browser-local vs. Connected distinction.

## Usage

```js
import { ConnectedSession } from '@usdk/binding-javascript';

const session = new ConnectedSession({
  hostUrl: 'http://127.0.0.1:8421',
  pairingSecret: pairingSecretTypedByTheUser, // never hardcode this
});
const { loadedCapabilities } = await session.pair();

const result = await session.sendText('what is the status?');
if (result.status === 'committed') console.log(result.text);

await session.proposeRoboticsAction({ direction: 'forward', distanceM: 0.2, speedMS: 0.1 });
session.emergencyStop();
await session.disconnect();
```

A working example UI is `packages/uagent/public/connected.html` /
`connected.mjs` - a second entry point alongside the Browser-local
`index.html`, never auto-selected (see "Never silently switch
profiles" below).

## Why this is not a drop-in replacement for `@usdk/host-browser`

`ConversationSession` (Browser-local) and `ConnectedSession`
(Connected) have deliberately different shapes - `ConnectedSession` has
`pair()`/`disconnect()` with no Browser-local equivalent, and no
`voiceAvailable`/`startVoiceInput()` since `@usdk/host-local` never
loads a voice driver (Node has no Web Speech API - see its README). A
UI author must explicitly construct one or the other; nothing here lets
an app silently fall back from one profile to the other, per this
task's "Never silently switch profiles."

## Origin and secrets

`fetch()` from a real browser page always sends a same-origin or
cross-origin request with a non-spoofable `Origin` header - this class
never sets one itself (browsers refuse to let JS set `Origin` manually
in the first place). `@usdk/host-local` checks that header against its
own allowlist. The pairing secret must come from user input (a text
field, as in `connected.mjs`) or another out-of-band channel - never
bake it into this file or into bundled application code.

## Running the tests

```bash
node --test packages/binding-javascript/test/*.test.mjs
```

`test/connected-session.test.mjs` starts a real `@usdk/host-local`
server and drives it only through `ConnectedSession`'s public API (no
mocking of either side).

**Also verified in a real browser tab** (not just Node's `fetch`):
`packages/uagent/public/connected.html` was loaded in an actual browser,
paired against a real running `@usdk/host-local` instance, sent a text
turn (committed and displayed), proposed a robotics action (approved
and executed), and triggered emergency stop - all confirmed via the
browser's own network log and DOM content, not simulated. This is what
caught a real gap during development: Node's built-in `fetch` does not
send an `Origin` header the way a browser's `fetch` always does, which
`packages/host-local/src/index.mjs`'s origin check needed to account
for (present-and-not-allowlisted is rejected; absent - i.e. a
non-browser caller - is allowed, since Origin was never a control that
could restrict non-browser HTTP clients in the first place).
