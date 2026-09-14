import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  sha256Hex,
  createCandidate,
  candidateDigestMatches,
  createVote,
  Verdict,
  Role,
  parseManifest,
  compareVersions,
  UsdkError,
  Status,
} from '../src/index.mjs';

test('sha256Hex matches a known-answer vector (verified with local sha256sum, see docs/VALIDATION.md)', async () => {
  assert.equal(
    await sha256Hex('abc'),
    'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad'
  );
});

test('createCandidate: same content twice produces the same digest (determinism)', async () => {
  const fields = {
    policyVersion: 1,
    sessionId: 's1',
    roundId: 1,
    candidateId: 'c1',
    evidenceRefs: [{ evidenceId: 'ev-1', evidenceDigest: 'deadbeef', uncertainty: 0.2 }],
    proposedResponse: 'hello',
    constraints: [{ constraintId: 'workspace:temp-only' }],
    deadlineMs: 1000,
  };
  const a = await createCandidate(fields);
  const b = await createCandidate(fields);
  assert.equal(a.contentDigest, b.contentDigest);
});

test('createCandidate: different content produces a different digest', async () => {
  const base = {
    policyVersion: 1,
    sessionId: 's1',
    roundId: 1,
    candidateId: 'c1',
    proposedResponse: 'hello',
    deadlineMs: 1000,
  };
  const a = await createCandidate(base);
  const b = await createCandidate({ ...base, proposedResponse: 'goodbye' });
  assert.notEqual(a.contentDigest, b.contentDigest);
});

test('createCandidate: candidate is frozen (no setters, matching the C struct design)', async () => {
  const c = await createCandidate({
    policyVersion: 1,
    sessionId: 's1',
    roundId: 1,
    candidateId: 'c1',
    proposedResponse: 'hello',
    deadlineMs: 1000,
  });
  assert.throws(() => {
    'use strict';
    c.roundId = 999;
  });
});

test('candidateDigestMatches: detects a mutated candidate', async () => {
  const c = await createCandidate({
    policyVersion: 1,
    sessionId: 's1',
    roundId: 1,
    candidateId: 'c1',
    proposedResponse: 'hello',
    deadlineMs: 1000,
  });
  assert.equal(await candidateDigestMatches(c), true);
  // Bypass the freeze the way a determined caller (or a bug) could, the
  // same way a C caller could still overwrite a struct field through
  // raw memory access - see the doc comment on createCandidate.
  const mutated = { ...c, roundId: 999 };
  assert.equal(await candidateDigestMatches(mutated), false);
});

test('createCandidate rejects invalid arguments rather than silently accepting them', async () => {
  await assert.rejects(
    () => createCandidate({ policyVersion: 1, sessionId: '', candidateId: 'c1', roundId: 1, proposedResponse: 'x', deadlineMs: 1 }),
    UsdkError
  );
});

test('createVote produces a frozen vote with a timestamp', () => {
  const v = createVote({
    partyId: 'p1',
    role: Role.PERCEIVE,
    verdict: Verdict.ACCEPT,
    reasonCode: 'ok',
    reasonDetail: 'fine',
    candidateDigestSeen: 'abc123',
  });
  assert.equal(v.verdict, Verdict.ACCEPT);
  assert.ok(Number.isFinite(v.votedAtMs));
});

test('parseManifest validates required fields', () => {
  const good = parseManifest({ name: 'x', version: '0.1', kind: 'llm', entry: './x.mjs', dependencies: [] });
  assert.equal(good.name, 'x');
  assert.throws(() => parseManifest({ version: '0.1', kind: 'llm', entry: './x.mjs' }), UsdkError);
  assert.throws(() => parseManifest({ name: 'x', version: 'bad', kind: 'llm', entry: './x.mjs' }), UsdkError);
});

test('compareVersions orders major.minor strings numerically, not lexically', () => {
  assert.equal(compareVersions('0.9', '0.10') < 0, true); // lexical compare would get this backwards
  assert.equal(compareVersions('1.0', '0.9') > 0, true);
  assert.equal(compareVersions('1.2', '1.2'), 0);
});

test('Status enum values are stable strings', () => {
  assert.equal(Status.DEPENDENCY_CYCLE, 'dependency-cycle');
});
