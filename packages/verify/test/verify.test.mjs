import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createCandidate, Verdict } from '@usdk/contracts';
import { create } from '../src/index.mjs';

const ev = { evidenceId: 'ev-1', evidenceDigest: 'x', uncertainty: 0.1 };

test('verify: permitted constraint + acceptable uncertainty -> ACCEPT', async () => {
  const v = create({ allowedConstraints: ['workspace:temp-only'], maxUncertainty: 0.4 });
  const candidate = await createCandidate({
    policyVersion: 1, sessionId: 's', roundId: 1, candidateId: 'c1',
    evidenceRefs: [ev], proposedResponse: 'r',
    constraints: [{ constraintId: 'workspace:temp-only' }], deadlineMs: Date.now() + 5000,
  });
  const vote = await v.vote(candidate);
  assert.equal(vote.verdict, Verdict.ACCEPT);
});

test('verify: disallowed constraint -> REJECT constraint-not-permitted', async () => {
  const v = create({ allowedConstraints: ['workspace:temp-only'], maxUncertainty: 0.4 });
  const candidate = await createCandidate({
    policyVersion: 1, sessionId: 's', roundId: 1, candidateId: 'c2',
    evidenceRefs: [ev], proposedResponse: 'r',
    constraints: [{ constraintId: 'action:delete-everything' }], deadlineMs: Date.now() + 5000,
  });
  const vote = await v.vote(candidate);
  assert.equal(vote.verdict, Verdict.REJECT);
  assert.equal(vote.reasonCode, 'constraint-not-permitted');
});

test('verify: uncertainty above policy -> REJECT uncertainty-exceeds-policy', async () => {
  const v = create({ allowedConstraints: ['workspace:temp-only'], maxUncertainty: 0.05 });
  const candidate = await createCandidate({
    policyVersion: 1, sessionId: 's', roundId: 1, candidateId: 'c3',
    evidenceRefs: [ev], proposedResponse: 'r',
    constraints: [{ constraintId: 'workspace:temp-only' }], deadlineMs: Date.now() + 5000,
  });
  const vote = await v.vote(candidate);
  assert.equal(vote.verdict, Verdict.REJECT);
  assert.equal(vote.reasonCode, 'uncertainty-exceeds-policy');
});

test('verify: zero evidence -> REJECT insufficient-evidence', async () => {
  const v = create({ allowedConstraints: ['workspace:temp-only'] });
  const candidate = await createCandidate({
    policyVersion: 1, sessionId: 's', roundId: 1, candidateId: 'c4',
    proposedResponse: 'r', constraints: [{ constraintId: 'workspace:temp-only' }],
    deadlineMs: Date.now() + 5000,
  });
  const vote = await v.vote(candidate);
  assert.equal(vote.verdict, Verdict.REJECT);
  assert.equal(vote.reasonCode, 'insufficient-evidence');
});
