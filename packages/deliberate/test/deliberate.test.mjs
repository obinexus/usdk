import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createCandidate, Verdict, UsdkError } from '@usdk/contracts';
import { create } from '../src/index.mjs';

function fixtureDriver() {
  return { async generate(prompt) { return `[fixture] response to: ${prompt}`; } };
}

test('deliberate: requires a valid llmDriver at creation', () => {
  assert.throws(() => create({}), UsdkError);
  assert.throws(() => create(), UsdkError);
});

test('deliberate: accepts a candidate built from its own propose() output', async () => {
  const d = create({ llmDriver: fixtureDriver() });
  const response = await d.propose('summarize the evidence', []);
  const candidate = await createCandidate({
    policyVersion: 1, sessionId: 's', roundId: 1, candidateId: 'c1',
    proposedResponse: response, deadlineMs: Date.now() + 5000,
  });
  const vote = await d.vote(candidate);
  assert.equal(vote.verdict, Verdict.ACCEPT);
  assert.equal(vote.reasonCode, 'self-consistent');
});

test('deliberate: proposeDirect() records host-authored content (e.g. a robotics action) as self-consistent', async () => {
  const d = create({ llmDriver: fixtureDriver() });
  const actionJson = JSON.stringify({ direction: 'forward', distanceM: 0.3, speedMS: 0.1 });
  await d.proposeDirect(actionJson);
  const candidate = await createCandidate({
    policyVersion: 1, sessionId: 's', roundId: 1, candidateId: 'c-robot',
    proposedResponse: actionJson, deadlineMs: Date.now() + 5000,
  });
  const vote = await d.vote(candidate);
  assert.equal(vote.verdict, Verdict.ACCEPT);
});

test('deliberate: rejects a candidate it never generated - not a rubber stamp', async () => {
  const d = create({ llmDriver: fixtureDriver() });
  await d.propose('something', []);
  const candidate = await createCandidate({
    policyVersion: 1, sessionId: 's', roundId: 1, candidateId: 'c2',
    proposedResponse: 'an externally-authored response', deadlineMs: Date.now() + 5000,
  });
  const vote = await d.vote(candidate);
  assert.equal(vote.verdict, Verdict.REJECT);
  assert.equal(vote.reasonCode, 'not-self-generated');
});
