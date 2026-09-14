import { test } from 'node:test';
import assert from 'node:assert/strict';
import { assertLlmCapability } from '@usdk/capability-llm';
import { UsdkError } from '@usdk/contracts';
import { create, loadModel, isModelLoaded } from '../src/index.mjs';

// Node has no `navigator.gpu` (real WebGPU) - this exercises the real,
// honest degradation path, not a browser simulation. Real model loading
// and inference can only be verified in a real browser with a working
// WebGPU adapter - see packages/uagent/public/index.html's "Real local
// model" section and docs/VALIDATION.md for that live verification.

test('driver-llm-local conforms to capability-llm', () => {
  assertLlmCapability(create());
});

test('driver-llm-local: honestly reports unavailable and never fabricates a response', async () => {
  const d = create();
  assert.equal(d.isAvailable(), false);
  await assert.rejects(() => d.generate('anything', []), UsdkError);
  assert.ok(!d.label().toLowerCase().includes('fixture')); // never mislabeled as the fixture
});

test('driver-llm-local: loadModel() rejects honestly with no WebGPU, rather than attempting a download', async () => {
  await assert.rejects(() => loadModel(), UsdkError);
  assert.equal(isModelLoaded(), false);
});

test('driver-llm-local: create() exposes loadModel/isModelLoaded beyond the base capability-llm contract', () => {
  const d = create();
  assert.equal(typeof d.loadModel, 'function');
  assert.equal(typeof d.isModelLoaded, 'function');
  assert.equal(d.isModelLoaded(), false);
});
