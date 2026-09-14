/**
 * @usdk/binding-javascript: the Connected execution profile's browser
 * client. Talks to an explicitly-started @usdk/host-local process over
 * plain `fetch` (a built-in browser + Node API - no bundler-specific
 * import, no third-party HTTP client). See
 * docs/UAGENT_ARCHITECTURE.md "Browser and native boundaries" for the
 * Connected profile's requirements this class implements: versioned
 * serialized messages, explicit pairing, session auth, narrowly-scoped
 * capabilities.
 *
 * Deliberately NOT a drop-in replacement for @usdk/host-browser's
 * ConversationSession - the two have similar but distinct methods
 * (`connect()` here has no Browser-local equivalent; there is no
 * `voiceAvailable` here since @usdk/host-local never loads a voice
 * driver - see its README) so that a UI author must explicitly choose
 * which class to construct rather than being able to swap the import
 * and have everything keep compiling. That explicitness is the point -
 * see the task's "Never silently switch profiles."
 */

export const PROTOCOL_VERSION = 1;

export class ProtocolVersionMismatchError extends Error {
  constructor(expected, received) {
    super(`@usdk/host-local speaks protocol version ${received}, this client expects ${expected}`);
    this.name = 'ProtocolVersionMismatchError';
  }
}

export class ConnectedSession {
  /**
   * @param {object} opts
   * @param {string} opts.hostUrl e.g. "http://127.0.0.1:8421" - the
   *   already-running @usdk/host-local instance's URL.
   * @param {string} opts.pairingSecret - typed in by the person at the
   *   browser, copied from the host-local process's own console output.
   *   Never hardcode this in bundled source - see this module's
   *   docstring and docs/UAGENT_ARCHITECTURE.md "keep secrets out of
   *   browser bundles."
   */
  constructor(opts) {
    this._hostUrl = opts.hostUrl.replace(/\/$/, '');
    this._pairingSecret = opts.pairingSecret;
    this._sessionId = null;
    this._token = null;
  }

  get connected() {
    return this._sessionId !== null;
  }

  async _request(path, { method = 'GET', body, auth = true } = {}) {
    const headers = {};
    if (body !== undefined) headers['Content-Type'] = 'application/json';
    if (auth) {
      if (!this._token) throw new Error('ConnectedSession: connect() must be awaited before use');
      headers['Authorization'] = `Bearer ${this._token}`;
    }
    const res = await fetch(`${this._hostUrl}${path}`, {
      method,
      headers,
      body: body !== undefined ? JSON.stringify(body) : undefined,
    });
    const json = await res.json().catch(() => ({}));
    if (json.protocolVersion !== undefined && json.protocolVersion !== PROTOCOL_VERSION) {
      throw new ProtocolVersionMismatchError(PROTOCOL_VERSION, json.protocolVersion);
    }
    if (!res.ok) {
      throw new Error(json.error ?? `@usdk/host-local request failed: ${res.status}`);
    }
    return json;
  }

  /** Pairs with the host using the secret from its console and opens a
   * new session with the host's fixed, narrowly-scoped capability set
   * (the browser never requests a capability list - see
   * @usdk/host-local's README "Fixed, server-configured capability
   * set"). Must be awaited before any other method. Built as a direct
   * fetch call, not through _request(), because this is the one call
   * that authenticates with the pairing-secret header instead of a
   * bearer token (there is no session yet to hold one). */
  async pair() {
    const res = await fetch(`${this._hostUrl}/session`, {
      method: 'POST',
      headers: { 'X-Usdk-Pairing-Secret': this._pairingSecret },
    });
    const json = await res.json().catch(() => ({}));
    if (json.protocolVersion !== undefined && json.protocolVersion !== PROTOCOL_VERSION) {
      throw new ProtocolVersionMismatchError(PROTOCOL_VERSION, json.protocolVersion);
    }
    if (!res.ok) throw new Error(json.error ?? `pairing failed: ${res.status}`);
    this._sessionId = json.sessionId;
    this._token = json.token;
    return { loadedCapabilities: json.loadedCapabilities };
  }

  async sendText(text) {
    const result = await this._request(`/session/${this._sessionId}/turn`, { method: 'POST', body: { text } });
    return result;
  }

  async proposeRoboticsAction(action) {
    return this._request(`/session/${this._sessionId}/robotics`, { method: 'POST', body: action });
  }

  async emergencyStop() {
    return this._request(`/session/${this._sessionId}/estop`, { method: 'POST', body: {} });
  }

  async cancelTurn() {
    return this._request(`/session/${this._sessionId}/cancel`, { method: 'POST', body: {} });
  }

  async disconnect() {
    if (!this._sessionId) return;
    await this._request(`/session/${this._sessionId}`, { method: 'DELETE' });
    this._sessionId = null;
    this._token = null;
  }
}
