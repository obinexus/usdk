import { test } from 'node:test';
import assert from 'node:assert/strict';
import { assertLlmCapability } from '@usdk/capability-llm';
import { UsdkError } from '@usdk/contracts';
import { create } from '../src/index.mjs';

test('driver-llm-local conforms to capability-llm', () => {
  assertLlmCapability(create());
});

test('driver-llm-local: honestly reports unavailable and never fabricates a response', async () => {
  const d = create();
  assert.equal(d.isAvailable(), false);
  await assert.rejects(() => d.generate('anything', []), UsdkError);
  assert.ok(!d.label().toLowerCase().includes('fixture')); // never mislabeled as the fixture
});
