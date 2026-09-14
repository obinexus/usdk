/**
 * Pluggable append-only round journal - the JS analogue of
 * src/core/journal.c/journal.h. See docs/CONSENSUS_PROTOCOL.md "Commit
 * records and side-effect dispatch" and "Crash and restart behavior" for
 * what this exists to make detectable (a lost dispatch acknowledgement
 * after a commit) and docs/UAGENT_ARCHITECTURE.md for why the storage
 * backend is an injected adapter here rather than one fixed
 * implementation: a browser tab has no filesystem, so MemoryJournal is
 * the browser-local default; a Node.js host (packages/host-local) can
 * inject FileJournal instead for real cross-restart persistence,
 * exactly mirroring the native CLI's on-disk journal.
 */

export const JournalRecordType = Object.freeze({
  OPEN: 'open',
  COMMITTED: 'committed',
  REJECTED: 'rejected',
  TIMED_OUT: 'timed-out',
  CANCELLED: 'cancelled',
  DISPATCHED: 'dispatched',
  DISPATCH_UNKNOWN: 'dispatch-unknown',
});

/**
 * @typedef {Object} JournalRecord
 * @property {string} type - one of JournalRecordType
 * @property {string} sessionId
 * @property {number} roundId
 * @property {string} candidateId
 */

/** In-memory journal - the default everywhere (browser tab, quick Node
 * scripts, tests). State does not survive the process/tab closing - see
 * FileJournal for the persistent alternative. */
export class MemoryJournal {
  constructor() {
    /** @type {JournalRecord[]} */
    this._records = [];
  }
  /** @param {JournalRecord} record */
  async append(record) {
    this._records.push({ ...record });
  }
  /** @returns {Promise<JournalRecord[]>} oldest first */
  async readAll() {
    return this._records.map((r) => ({ ...r }));
  }
}

/**
 * Node.js-only, file-backed journal - one JSON object per line, append
 * mode, directly analogous to src/core/journal.c's text format (a
 * different concrete encoding, same "one record per line, human-
 * readable" design choice). Reopening the SAME `path` after the process
 * restarts sees every record a prior process wrote - this is what makes
 * restart/recovery testing meaningful (docs/VALIDATION.md).
 */
export class FileJournal {
  /** @param {string} path */
  constructor(path) {
    this._path = path;
  }
  /** @param {JournalRecord} record */
  async append(record) {
    const { appendFile, mkdir } = await import('node:fs/promises');
    const { dirname } = await import('node:path');
    await mkdir(dirname(this._path), { recursive: true });
    await appendFile(this._path, JSON.stringify(record) + '\n', 'utf8');
  }
  /** @returns {Promise<JournalRecord[]>} oldest first */
  async readAll() {
    const { readFile } = await import('node:fs/promises');
    let text;
    try {
      text = await readFile(this._path, 'utf8');
    } catch (e) {
      if (e && e.code === 'ENOENT') return [];
      throw e;
    }
    const records = [];
    for (const line of text.split('\n')) {
      if (!line.trim()) continue;
      try {
        records.push(JSON.parse(line));
      } catch {
        // A truncated final line (e.g. process killed mid-write) is
        // skipped, not fatal - matching the native journal's own
        // tolerance for a torn trailing record.
      }
    }
    return records;
  }
}
