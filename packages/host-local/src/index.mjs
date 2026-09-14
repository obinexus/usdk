import http from 'node:http';
import crypto from 'node:crypto';
import { execFile } from 'node:child_process';
import fs from 'node:fs';
import fs_promises from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { ConversationSession } from '@usdk/host-browser';
import { UsdkError, Status } from '@usdk/contracts';

/**
 * @usdk/host-local: the Connected execution profile's local runtime host.
 * See docs/UAGENT_ARCHITECTURE.md "Browser and native boundaries" -
 * Connected mode: "local host loads native modules, browser uses
 * versioned serialized messages, require explicit pairing/origin
 * checks/session auth/narrowly-scoped capabilities, keep secrets out of
 * browser bundles, no arbitrary native-library paths or shell commands
 * exposed to browser requests."
 *
 * The conversation/consensus logic itself (perceive/deliberate/verify
 * round, dispatch gating, turn cancellation) is @usdk/host-browser's
 * ConversationSession, reused unmodified - it is plain JS with no
 * browser-only API dependency (already proven by
 * packages/host-browser/test/session.test.mjs running it under Node).
 * What THIS package adds is: an HTTP+JSON transport, explicit pairing
 * and per-session bearer auth, an Origin allowlist, and a bridge to the
 * native usdk.exe CLI for the two read-only diagnostic endpoints
 * (/native/doctor, /native/inspect/:module) - the CLI is the thing
 * actually performing dynamic C-ABI module loading
 * (usdk-ffi's LoadLibraryExW/dlopen path), at the process boundary
 * rather than in-process, since Node has no built-in FFI to dlopen a
 * native library directly without a third-party dependency (which the
 * zero-dependency constraint rules out) - see README.md "Why a
 * subprocess, not in-process ctypes-equivalent".
 */

export const PROTOCOL_VERSION = 1;

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const REPO_ROOT = path.resolve(__dirname, '..', '..', '..');
const BUILD_BIN = path.join(REPO_ROOT, 'build', 'bin');
const USDK_EXE = path.join(BUILD_BIN, 'usdk.exe');

// Fixed, server-configured capability set - never client-supplied, so a
// browser request can never name an arbitrary manifest/module/path (see
// docs/UAGENT_ARCHITECTURE.md's "no arbitrary native-library paths...
// exposed to browser requests"). No voice driver: this process is plain
// Node, which has no Web Speech API - voice stays browser-local-only,
// documented rather than faked here.
const SESSION_MANIFESTS = [
  { name: 'driver-llm-fixture', version: '0.1', kind: 'llm', entry: '../../driver-llm-fixture/src/index.mjs', dependencies: [] },
  { name: 'driver-robotics-sim', version: '0.1', kind: 'robotics', entry: '../../driver-robotics-sim/src/index.mjs', dependencies: [] },
];
const MANIFEST_BASE_URL = import.meta.url;

function sessionA11y(record) {
  return {
    announce: (text) => record.announcements.push(text),
    setStatus: (text) => { record.status = text; },
    isAvailable: () => true,
  };
}

async function listInspectableModules() {
  let entries = [];
  try {
    entries = await fs_promises.readdir(BUILD_BIN);
  } catch {
    return [];
  }
  return entries
    .filter((f) => f.endsWith('.usdk-manifest.json'))
    .map((f) => f.slice(0, -'.usdk-manifest.json'.length));
}

function runUsdkExe(args) {
  return new Promise((resolve, reject) => {
    if (!fs.existsSync(USDK_EXE)) {
      reject(new Error(`native build not found: ${USDK_EXE} (run \`make\` at the repo root)`));
      return;
    }
    execFile(USDK_EXE, args, { timeout: 5000, cwd: BUILD_BIN }, (err, stdout, stderr) => {
      if (err && !stdout) { reject(new Error(stderr || err.message)); return; }
      resolve(stdout);
    });
  });
}

function readJsonBody(req) {
  return new Promise((resolve, reject) => {
    let data = '';
    req.on('data', (chunk) => {
      data += chunk;
      if (data.length > 1_000_000) { req.destroy(); reject(new Error('request body too large')); }
    });
    req.on('end', () => {
      if (!data) { resolve({}); return; }
      try { resolve(JSON.parse(data)); } catch (e) { reject(new Error('invalid JSON body')); }
    });
    req.on('error', reject);
  });
}

/**
 * @param {object} [opts]
 * @param {number} [opts.port] default 8421
 * @param {string[]} [opts.allowedOrigins] default ['http://127.0.0.1:8420', 'http://localhost:8420']
 * @param {string} [opts.pairingSecret] - if omitted, a random one is
 *   generated and printed to the console (never returned in any HTTP
 *   response body, never embedded in browser-bundled code - the person
 *   at the browser must copy it from THIS process's own stdout, the
 *   "explicitly-started local runtime host" pairing step).
 * @returns {Promise<{server: import('http').Server, url: string, pairingSecret: string, sessionCount: () => number, stop: () => Promise<void>}>}
 */
export async function startHostLocal(opts = {}) {
  const port = opts.port ?? 8421;
  const allowedOrigins = opts.allowedOrigins ?? ['http://127.0.0.1:8420', 'http://localhost:8420'];
  const pairingSecret = opts.pairingSecret ?? crypto.randomUUID();
  const inspectableModules = await listInspectableModules();

  /** @type {Map<string, {session: ConversationSession, token: string, announcements: string[], status: string}>} */
  const sessions = new Map();

  function corsHeaders(origin) {
    const headers = { 'Access-Control-Allow-Methods': 'GET, POST, DELETE, OPTIONS', 'Access-Control-Allow-Headers': 'Content-Type, Authorization, X-Usdk-Pairing-Secret' };
    if (origin && allowedOrigins.includes(origin)) headers['Access-Control-Allow-Origin'] = origin;
    return headers;
  }

  function send(res, origin, status, body) {
    const headers = { 'Content-Type': 'application/json', ...corsHeaders(origin) };
    res.writeHead(status, headers);
    res.end(JSON.stringify({ protocolVersion: PROTOCOL_VERSION, ...body }));
  }

  // A browser's fetch/XHR always sends an Origin header on a cross-
  // origin request, and cannot be told not to or told to lie about it -
  // that is what makes the allowlist check below meaningful defense
  // against a malicious web page silently calling this API using the
  // victim's browser. A non-browser HTTP client (curl, a Node script,
  // this package's own tests) sends no Origin header at all, and was
  // never something an Origin check could restrict anyway (it already
  // has arbitrary access to the local machine) - so a request with NO
  // Origin header is let through, while a request WITH one must match
  // the allowlist. This mirrors the standard CORS-plus-native-client
  // pattern; it is the pairing secret and bearer token, not this check
  // alone, that gate actual access.
  function requireOrigin(req, res) {
    const origin = req.headers.origin;
    if (origin && !allowedOrigins.includes(origin)) {
      send(res, origin, 403, { error: 'origin not allowed', allowedOrigins });
      return null;
    }
    return origin; // possibly undefined - see comment above; corsHeaders() only reflects a present, allowed origin
  }

  function requireBearer(req, res, origin, record) {
    const auth = req.headers.authorization ?? '';
    const token = auth.startsWith('Bearer ') ? auth.slice(7) : null;
    if (!token || !record) { send(res, origin, 401, { error: 'missing or unknown session' }); return false; }
    const tokenBuf = Buffer.from(token);
    const expectedBuf = Buffer.from(record.token);
    const matches = tokenBuf.length === expectedBuf.length && crypto.timingSafeEqual(tokenBuf, expectedBuf);
    if (!matches) { send(res, origin, 401, { error: 'invalid session token' }); return false; }
    return true;
  }

  const server = http.createServer(async (req, res) => {
    const origin = req.headers.origin;
    if (req.method === 'OPTIONS') { res.writeHead(204, corsHeaders(origin)); res.end(); return; }

    const url = new URL(req.url, `http://localhost:${port}`);

    try {
      if (url.pathname === '/health' && req.method === 'GET') {
        send(res, origin, 200, { ok: true });
        return;
      }

      if (url.pathname === '/session' && req.method === 'POST') {
        const checkedOrigin = requireOrigin(req, res);
        if (checkedOrigin === null) return;
        const providedSecret = req.headers['x-usdk-pairing-secret'];
        if (providedSecret !== pairingSecret) {
          send(res, checkedOrigin, 401, { error: 'missing or incorrect pairing secret - copy it from the host-local process console' });
          return;
        }
        const sessionId = crypto.randomUUID();
        const record = { announcements: [], status: 'idle', token: crypto.randomUUID() };
        record.session = new ConversationSession({
          sessionId,
          manifests: SESSION_MANIFESTS,
          baseUrl: MANIFEST_BASE_URL,
          llmManifestName: 'driver-llm-fixture',
          roboticsManifestName: 'driver-robotics-sim',
          a11y: sessionA11y(record),
          allowedConstraints: ['workspace:temp-only', 'robotics:bounded-move'],
        });
        const { loadedCapabilities } = await record.session.init();
        sessions.set(sessionId, record);
        send(res, checkedOrigin, 200, { sessionId, token: record.token, loadedCapabilities });
        return;
      }

      const sessionMatch = url.pathname.match(/^\/session\/([^/]+)(\/(turn|robotics|estop|cancel))?$/);
      if (sessionMatch) {
        const checkedOrigin = requireOrigin(req, res);
        if (checkedOrigin === null) return;
        const sessionId = sessionMatch[1];
        const record = sessions.get(sessionId);
        if (!requireBearer(req, res, checkedOrigin, record)) return;
        record.announcements = [];

        const sub = sessionMatch[3];
        if (!sub && req.method === 'DELETE') {
          sessions.delete(sessionId);
          send(res, checkedOrigin, 200, { ok: true });
          return;
        }
        if (sub === 'turn' && req.method === 'POST') {
          const body = await readJsonBody(req);
          const result = await record.session.sendText(String(body.text ?? ''));
          send(res, checkedOrigin, 200, { ...result, a11y: { status: record.status, announcements: record.announcements } });
          return;
        }
        if (sub === 'robotics' && req.method === 'POST') {
          const body = await readJsonBody(req);
          try {
            const result = await record.session.proposeRoboticsAction({
              direction: body.direction, distanceM: body.distanceM, speedMS: body.speedMS,
            });
            send(res, checkedOrigin, 200, { ...result, a11y: { status: record.status, announcements: record.announcements } });
          } catch (e) {
            send(res, checkedOrigin, 400, { error: e instanceof Error ? e.message : String(e) });
          }
          return;
        }
        if (sub === 'estop' && req.method === 'POST') {
          record.session.emergencyStop();
          send(res, checkedOrigin, 200, { ok: true });
          return;
        }
        if (sub === 'cancel' && req.method === 'POST') {
          record.session.cancelTurn();
          send(res, checkedOrigin, 200, { ok: true });
          return;
        }
      }

      if (url.pathname === '/native/doctor' && req.method === 'GET') {
        const checkedOrigin = requireOrigin(req, res);
        if (checkedOrigin === null) return;
        if (url.searchParams.get('pairing') !== pairingSecret) {
          send(res, checkedOrigin, 401, { error: 'missing or incorrect pairing secret' });
          return;
        }
        const stdout = await runUsdkExe(['doctor', '--json']);
        send(res, checkedOrigin, 200, { doctor: JSON.parse(stdout) });
        return;
      }

      const inspectMatch = url.pathname.match(/^\/native\/inspect\/([^/]+)$/);
      if (inspectMatch && req.method === 'GET') {
        const checkedOrigin = requireOrigin(req, res);
        if (checkedOrigin === null) return;
        if (url.searchParams.get('pairing') !== pairingSecret) {
          send(res, checkedOrigin, 401, { error: 'missing or incorrect pairing secret' });
          return;
        }
        const moduleName = inspectMatch[1];
        // Strict allowlist against manifests actually present in
        // build/bin - never pass a client-supplied string to execFile
        // beyond a value already known to name a real module (see this
        // file's module docstring: "no arbitrary native-library
        // paths... exposed to browser requests").
        if (!inspectableModules.includes(moduleName)) {
          send(res, checkedOrigin, 404, { error: `unknown module '${moduleName}'`, knownModules: inspectableModules });
          return;
        }
        const stdout = await runUsdkExe(['inspect', moduleName, '--json']);
        send(res, checkedOrigin, 200, { inspect: JSON.parse(stdout) });
        return;
      }

      send(res, origin, 404, { error: 'not found' });
    } catch (e) {
      const status = e instanceof UsdkError && e.status === Status.INVALID_ARGUMENT ? 400 : 500;
      send(res, origin, status, { error: e instanceof Error ? e.message : String(e) });
    }
  });

  await new Promise((resolve) => server.listen(port, '127.0.0.1', resolve));
  const boundPort = server.address().port;
  console.log(`@usdk/host-local listening on http://127.0.0.1:${boundPort}`);
  console.log(`Pairing secret (copy into the browser client to connect): ${pairingSecret}`);

  return {
    server,
    url: `http://127.0.0.1:${boundPort}`,
    pairingSecret,
    sessionCount: () => sessions.size,
    stop: () => new Promise((resolve) => server.close(() => resolve())),
  };
}
