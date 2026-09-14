import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, writeFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { pathToFileURL } from 'node:url';
import { UsdkError, Status } from '@usdk/contracts';
import { loadManifestModule } from '../src/index.mjs';

async function withTempModule(source, fn) {
  const dir = await mkdtemp(join(tmpdir(), 'usdk-loader-test-'));
  try {
    const file = join(dir, 'mod.mjs');
    await writeFile(file, source, 'utf8');
    await fn(pathToFileURL(dir + '/').href);
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
}

test('loadManifestModule: a well-formed module loads and passes validation', async () => {
  await withTempModule(
    `export const descriptor = { name: 'x', version: '1.0', kind: 'driver' };\nexport function create() { return { ok: true }; }\n`,
    async (baseUrl) => {
      const manifest = { name: 'x', version: '1.0', kind: 'driver', entry: './mod.mjs', dependencies: [] };
      const loaded = await loadManifestModule(manifest, baseUrl);
      assert.equal(loaded.descriptor.name, 'x');
      const inst = loaded.create();
      assert.equal(inst.ok, true);
    }
  );
});

test('loadManifestModule: a name mismatch between descriptor and manifest is rejected', async () => {
  await withTempModule(
    `export const descriptor = { name: 'wrong-name', version: '1.0', kind: 'driver' };\nexport function create() {}\n`,
    async (baseUrl) => {
      const manifest = { name: 'x', version: '1.0', kind: 'driver', entry: './mod.mjs', dependencies: [] };
      await assert.rejects(
        () => loadManifestModule(manifest, baseUrl),
        (e) => e instanceof UsdkError && e.status === Status.MODULE_LOAD_FAILED
      );
    }
  );
});

test('loadManifestModule: a version mismatch is rejected as an ABI version mismatch', async () => {
  await withTempModule(
    `export const descriptor = { name: 'x', version: '2.0', kind: 'driver' };\nexport function create() {}\n`,
    async (baseUrl) => {
      const manifest = { name: 'x', version: '1.0', kind: 'driver', entry: './mod.mjs', dependencies: [] };
      await assert.rejects(
        () => loadManifestModule(manifest, baseUrl),
        (e) => e instanceof UsdkError && e.status === Status.ABI_VERSION_MISMATCH
      );
    }
  );
});

test('loadManifestModule: a missing create() export is rejected', async () => {
  await withTempModule(
    `export const descriptor = { name: 'x', version: '1.0', kind: 'driver' };\n`,
    async (baseUrl) => {
      const manifest = { name: 'x', version: '1.0', kind: 'driver', entry: './mod.mjs', dependencies: [] };
      await assert.rejects(
        () => loadManifestModule(manifest, baseUrl),
        (e) => e instanceof UsdkError && e.status === Status.MODULE_LOAD_FAILED
      );
    }
  );
});

test('loadManifestModule: a module that fails to import (syntax error) is reported', async () => {
  await withTempModule(`this is not valid javascript {{{`, async (baseUrl) => {
    const manifest = { name: 'x', version: '1.0', kind: 'driver', entry: './mod.mjs', dependencies: [] };
    await assert.rejects(
      () => loadManifestModule(manifest, baseUrl),
      (e) => e instanceof UsdkError && e.status === Status.MODULE_LOAD_FAILED
    );
  });
});
