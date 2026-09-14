import { test } from 'node:test';
import assert from 'node:assert/strict';
import { UsdkError, Status } from '@usdk/contracts';
import { ManifestRegistry } from '../src/index.mjs';

function m(name, version, deps = []) {
  return { name, version, kind: 'driver', entry: `./${name}.mjs`, dependencies: deps };
}

test('resolve: a real 3-node diamond (A depends on B and C; C also depends on B)', () => {
  const reg = new ManifestRegistry();
  reg.register(m('B', '1.0'));
  reg.register(m('C', '1.0', [{ capability: 'B', minVersion: '1.0' }]));
  reg.register(m('A', '1.0', [
    { capability: 'B', minVersion: '1.0' },
    { capability: 'C', minVersion: '1.0' },
  ]));
  const order = reg.resolve(['A']);
  const idx = (n) => order.findIndex((x) => x.name === n);
  assert.equal(order.length, 3);
  assert.ok(idx('B') < idx('C'));
  assert.ok(idx('B') < idx('A'));
  assert.ok(idx('C') < idx('A'));
});

test('resolve: a genuine cycle is detected, not silently accepted', () => {
  const reg = new ManifestRegistry();
  reg.register(m('X', '1.0', [{ capability: 'Y', minVersion: '1.0' }]));
  reg.register(m('Y', '1.0', [{ capability: 'X', minVersion: '1.0' }]));
  assert.throws(
    () => reg.resolve(['X']),
    (e) => e instanceof UsdkError && e.status === Status.DEPENDENCY_CYCLE
  );
});

test('resolve: a missing dependency is unresolved, not silently skipped', () => {
  const reg = new ManifestRegistry();
  reg.register(m('P', '1.0', [{ capability: 'does-not-exist', minVersion: '1.0' }]));
  assert.throws(
    () => reg.resolve(['P']),
    (e) => e instanceof UsdkError && e.status === Status.DEPENDENCY_UNRESOLVED
  );
});

test('resolve: a present-but-too-old dependency is rejected', () => {
  const reg = new ManifestRegistry();
  reg.register(m('OldDep', '0.5'));
  reg.register(m('Q', '1.0', [{ capability: 'OldDep', minVersion: '1.0' }]));
  assert.throws(
    () => reg.resolve(['Q']),
    (e) => e instanceof UsdkError && e.status === Status.DEPENDENCY_VERSION_INCOMPATIBLE
  );
});

test('resolve: an unregistered requested name fails', () => {
  const reg = new ManifestRegistry();
  assert.throws(
    () => reg.resolve(['nope']),
    (e) => e instanceof UsdkError && e.status === Status.DEPENDENCY_UNRESOLVED
  );
});
