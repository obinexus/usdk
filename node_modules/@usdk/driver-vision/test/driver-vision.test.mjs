import { test } from 'node:test';
import assert from 'node:assert/strict';
import { assertVisionCapability } from '@usdk/capability-vision';
import { UsdkError } from '@usdk/contracts';
import { create } from '../src/index.mjs';

test('driver-vision conforms to capability-vision', () => {
  assertVisionCapability(create());
});

test('driver-vision: honestly reports unavailable rather than fabricating a description', async () => {
  const v = create();
  assert.equal(v.isAvailable(), false);
  await assert.rejects(() => v.describe({}), UsdkError);
});
