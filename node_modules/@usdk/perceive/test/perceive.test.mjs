import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createCandidate, Verdict } from '@usdk/contracts';
import { create } from '../src/index.mjs';

test('perceive: accepts a candidate citing real observed evidence', async () => {
  const p = create();
  const ref = await p.observe('door sensor: closed', 0.05);
  const candidate = await createCandidate({
    policyVersion: 1, sessionId: 's', roundId: 1, candidateId: 'c1',
    evidenceRefs: [ref], proposedResponse: 'the door is closed', deadlineMs: Date.now() + 5000,
  });
  const vote = await p.vote(candidate);
  assert.equal(vote.verdict, Verdict.ACCEPT);
});

test('perceive: rejects a candidate citing fabricated evidence', async () => {
  const p = create();
  await p.observe('real evidence', 0.1);
  const fake = { evidenceId: 'ev-does-not-exist', evidenceDigest: 'x', uncertainty: 0 };
  const candidate = await createCandidate({
    policyVersion: 1, sessionId: 's', roundId: 1, candidateId: 'c2',
    evidenceRefs: [fake], proposedResponse: 'the vault is unlocked', deadlineMs: Date.now() + 5000,
  });
  const vote = await p.vote(candidate);
  assert.equal(vote.verdict, Verdict.REJECT);
  assert.equal(vote.reasonCode, 'evidence-not-found');
});

test('perceive: rejects zero-evidence candidates by default', async () => {
  const p = create();
  const candidate = await createCandidate({
    policyVersion: 1, sessionId: 's', roundId: 1, candidateId: 'c3',
    proposedResponse: 'r', deadlineMs: Date.now() + 5000,
  });
  const vote = await p.vote(candidate);
  assert.equal(vote.verdict, Verdict.REJECT);
  assert.equal(vote.reasonCode, 'no-evidence-cited');
});

test('perceive: requireEvidence:false permits zero evidence_refs', async () => {
  const p = create({ requireEvidence: false });
  const candidate = await createCandidate({
    policyVersion: 1, sessionId: 's', roundId: 1, candidateId: 'c4',
    proposedResponse: 'r', deadlineMs: Date.now() + 5000,
  });
  const vote = await p.vote(candidate);
  assert.equal(vote.verdict, Verdict.ACCEPT);
});

test('perceive: uncertainty is clamped to [0,1]', async () => {
  const p = create();
  const ref = await p.observe('x', 5.0);
  assert.equal(ref.uncertainty, 1);
  const ref2 = await p.observe('y', -3.0);
  assert.equal(ref2.uncertainty, 0);
});
