import { test } from 'node:test';
import assert from 'node:assert/strict';
import { UsdkError } from '@usdk/contracts';
import { checkLlmCapability, assertLlmCapability } from '../src/index.mjs';

test('checkLlmCapability flags a missing method', () => {
  assert.deepEqual(checkLlmCapability({ generate: async () => '' }), ['isAvailable()', 'label()']);
});

test('assertLlmCapability passes a conforming instance through', () => {
  const conforming = { generate: async () => '', isAvailable: () => true, label: () => 'x' };
  assert.equal(assertLlmCapability(conforming), conforming);
});

test('assertLlmCapability throws on a non-conforming instance', () => {
  assert.throws(() => assertLlmCapability({}), UsdkError);
});
