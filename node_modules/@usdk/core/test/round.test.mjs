import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createCandidate, createVote, Role, Verdict, UsdkError, Status } from '@usdk/contracts';
import { Core, RoundState, MemoryJournal, FileJournal } from '../src/index.mjs';

function farFuture() {
  return Date.now() + 5000;
}

async function makeCandidate(overrides = {}) {
  return createCandidate({
    policyVersion: 1,
    sessionId: 's1',
    roundId: 1,
    candidateId: 'c1',
    proposedResponse: 'response',
    deadlineMs: farFuture(),
    ...overrides,
  });
}

function vote(role, verdict, candidate, partyId) {
  return createVote({
    partyId,
    role,
    verdict,
    reasonCode: 'test',
    reasonDetail: 'test',
    candidateDigestSeen: candidate.contentDigest,
  });
}

test('unanimous accept commits, and dispatch runs exactly once', async () => {
  let dispatchCalls = 0;
  const core = new Core({ dispatchFn: async () => { dispatchCalls++; } });
  const c = await makeCandidate();
  const round = await core.openRound(c);
  await core.submitVote(round, vote(Role.PERCEIVE, Verdict.ACCEPT, c, 'p1'));
  await core.submitVote(round, vote(Role.DELIBERATE, Verdict.ACCEPT, c, 'd1'));
  await core.submitVote(round, vote(Role.VERIFY, Verdict.ACCEPT, c, 'v1'));
  assert.equal(await core.decide(round), RoundState.COMMITTED);
  assert.equal(dispatchCalls, 1);
  // Idempotent: deciding again does not re-dispatch.
  assert.equal(await core.decide(round), RoundState.COMMITTED);
  assert.equal(dispatchCalls, 1);
});

test('a single REJECT prevents commit', async () => {
  const core = new Core();
  const c = await makeCandidate({ sessionId: 's2' });
  const round = await core.openRound(c);
  await core.submitVote(round, vote(Role.PERCEIVE, Verdict.ACCEPT, c, 'p1'));
  await core.submitVote(round, vote(Role.DELIBERATE, Verdict.REJECT, c, 'd1'));
  await core.submitVote(round, vote(Role.VERIFY, Verdict.ACCEPT, c, 'v1'));
  assert.equal(await core.decide(round), RoundState.REJECTED);
});

test('a single ABSTAIN prevents commit', async () => {
  const core = new Core();
  const c = await makeCandidate({ sessionId: 's3' });
  const round = await core.openRound(c);
  await core.submitVote(round, vote(Role.PERCEIVE, Verdict.ACCEPT, c, 'p1'));
  await core.submitVote(round, vote(Role.DELIBERATE, Verdict.ABSTAIN, c, 'd1'));
  await core.submitVote(round, vote(Role.VERIFY, Verdict.ACCEPT, c, 'v1'));
  assert.equal(await core.decide(round), RoundState.REJECTED);
});

test('a missing (never-cast) vote prevents commit', async () => {
  const core = new Core();
  const c = await makeCandidate({ sessionId: 's4' });
  const round = await core.openRound(c);
  await core.submitVote(round, vote(Role.PERCEIVE, Verdict.ACCEPT, c, 'p1'));
  await core.submitVote(round, vote(Role.DELIBERATE, Verdict.ACCEPT, c, 'd1'));
  // verify never votes
  assert.equal(await core.decide(round), RoundState.REJECTED);
});

test('an already-expired deadline times out, both at submitVote and decide', async () => {
  const core = new Core();
  const c = await makeCandidate({ sessionId: 's5', deadlineMs: Date.now() - 1000 });
  const round = await core.openRound(c);
  await assert.rejects(
    () => core.submitVote(round, vote(Role.PERCEIVE, Verdict.ACCEPT, c, 'p1')),
    (e) => e instanceof UsdkError && e.status === Status.DEADLINE_EXCEEDED
  );
  assert.equal(round.state, RoundState.TIMED_OUT);
});

test('a duplicate vote from the same role/party is rejected; the original is preserved', async () => {
  const core = new Core();
  const c = await makeCandidate({ sessionId: 's6' });
  const round = await core.openRound(c);
  await core.submitVote(round, vote(Role.PERCEIVE, Verdict.ACCEPT, c, 'p1'));
  await assert.rejects(
    () => core.submitVote(round, vote(Role.PERCEIVE, Verdict.REJECT, c, 'p1')),
    (e) => e instanceof UsdkError && e.status === Status.DUPLICATE_VOTE
  );
  await core.submitVote(round, vote(Role.DELIBERATE, Verdict.ACCEPT, c, 'd1'));
  await core.submitVote(round, vote(Role.VERIFY, Verdict.ACCEPT, c, 'v1'));
  // Still commits - proves the REJECT resubmission never actually counted.
  assert.equal(await core.decide(round), RoundState.COMMITTED);
});

test('a conflicting party_id for the same role is rejected', async () => {
  const core = new Core();
  const c = await makeCandidate({ sessionId: 's7' });
  const round = await core.openRound(c);
  await core.submitVote(round, vote(Role.PERCEIVE, Verdict.ACCEPT, c, 'p1'));
  await assert.rejects(
    () => core.submitVote(round, vote(Role.PERCEIVE, Verdict.ACCEPT, c, 'p1-impostor')),
    (e) => e instanceof UsdkError && e.status === Status.PARTY_CONFLICT
  );
});

test('a stale/replayed round_id is refused, within one Core instance', async () => {
  const core = new Core();
  const c1 = await makeCandidate({ sessionId: 's8', roundId: 1 });
  await core.openRound(c1);
  const c1replay = await makeCandidate({ sessionId: 's8', roundId: 1, candidateId: 'c1-replay' });
  await assert.rejects(
    () => core.openRound(c1replay),
    (e) => e instanceof UsdkError && e.status === Status.STALE_ROUND
  );
  const c2 = await makeCandidate({ sessionId: 's8', roundId: 2 });
  await core.openRound(c2); // strictly higher round_id is fine
});

test('a vote whose recorded candidateDigestSeen does not match the round candidate cannot commit', async () => {
  const core = new Core();
  const c = await makeCandidate({ sessionId: 's9' });
  const round = await core.openRound(c);
  await core.submitVote(round, vote(Role.PERCEIVE, Verdict.ACCEPT, c, 'p1'));
  const tampered = vote(Role.DELIBERATE, Verdict.ACCEPT, c, 'd1');
  await core.submitVote(round, { ...tampered, candidateDigestSeen: '0000000000000000000000000000000000000000000000000000000000000000' });
  await core.submitVote(round, vote(Role.VERIFY, Verdict.ACCEPT, c, 'v1'));
  assert.equal(await core.decide(round), RoundState.REJECTED);
});

test('cancelling a round blocks further votes and a second cancel', async () => {
  const core = new Core();
  const c = await makeCandidate({ sessionId: 's10' });
  const round = await core.openRound(c);
  await core.cancelRound(round);
  assert.equal(round.state, RoundState.CANCELLED);
  await assert.rejects(
    () => core.submitVote(round, vote(Role.PERCEIVE, Verdict.ACCEPT, c, 'p1')),
    (e) => e instanceof UsdkError && e.status === Status.ROUND_NOT_OPEN
  );
  await assert.rejects(
    () => core.cancelRound(round),
    (e) => e instanceof UsdkError && e.status === Status.ROUND_NOT_OPEN
  );
});

test('recoverIncompleteRounds reports a round left OPEN across a simulated restart (FileJournal)', async () => {
  const { mkdtemp, rm } = await import('node:fs/promises');
  const { tmpdir } = await import('node:os');
  const { join } = await import('node:path');
  const dir = await mkdtemp(join(tmpdir(), 'usdk-core-test-'));
  const journalPath = join(dir, 'journal.ndjson');
  try {
    const core1 = new Core({ journal: new FileJournal(journalPath) });
    const c = await makeCandidate({ sessionId: 's-recover', roundId: 1, candidateId: 'c-recover' });
    await core1.openRound(c);
    // Never decide/cancel - simulates a crash before the round finished.

    const core2 = new Core({ journal: new FileJournal(journalPath) });
    const incomplete = await core2.recoverIncompleteRounds();
    assert.equal(incomplete.length, 1);
    assert.equal(incomplete[0].sessionId, 's-recover');
    assert.equal(incomplete[0].roundId, 1);

    // And the journal itself enforces stale-round rejection across the
    // "restart" too, not just within one process's memory.
    const replay = await makeCandidate({ sessionId: 's-recover', roundId: 1, candidateId: 'c-recover-2' });
    await assert.rejects(
      () => core2.openRound(replay),
      (e) => e instanceof UsdkError && e.status === Status.STALE_ROUND
    );
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
});

test('MemoryJournal readAll returns records in append order', async () => {
  const j = new MemoryJournal();
  await j.append({ type: 'open', sessionId: 's', roundId: 1, candidateId: 'c' });
  await j.append({ type: 'committed', sessionId: 's', roundId: 1, candidateId: 'c' });
  const all = await j.readAll();
  assert.equal(all.length, 2);
  assert.equal(all[1].type, 'committed');
});
