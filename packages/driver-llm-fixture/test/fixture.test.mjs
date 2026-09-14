import { test } from 'node:test';
import assert from 'node:assert/strict';
import { assertLlmCapability } from '@usdk/capability-llm';
import { create, descriptor } from '../src/index.mjs';

test('driver-llm-fixture conforms to capability-llm', () => {
  assertLlmCapability(create());
});

test('driver-llm-fixture: responses are clearly labeled, never presented as real inference', async () => {
  const d = create();
  const response = await d.generate('hello', []);
  assert.ok(response.startsWith('[fixture]'));
  assert.ok(d.label().includes('not real model inference'));
});

test('driver-llm-fixture: deterministic - same input produces the same output', async () => {
  const d = create();
  const a = await d.generate('summarize', [{ evidenceId: 'e', evidenceDigest: 'x', uncertainty: 0.3 }]);
  const b = await d.generate('summarize', [{ evidenceId: 'e', evidenceDigest: 'x', uncertainty: 0.3 }]);
  assert.equal(a, b);
});

test('descriptor matches the loader module contract', () => {
  assert.equal(descriptor.kind, 'llm');
  assert.equal(descriptor.name, 'driver-llm-fixture');
});
