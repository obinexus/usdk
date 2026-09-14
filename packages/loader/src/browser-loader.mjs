import { UsdkError, Status } from '@usdk/contracts';

/**
 * Loads a resolved manifest plan via dynamic `import()` - the browser-
 * local analogue of usdk-ffi's `usdk_ffi_load`/`usdk_ffi_load_resolved`
 * (src/ffi/ffi.c). This is a genuinely different mechanism from the
 * native ABI, not a renamed copy of it: there is no LoadLibraryExW/
 * dlopen here, no struct-size negotiation (there is no struct layout to
 * negotiate - only a plain JS object shape), and no risk of a module's
 * *load-time* code crashing the host process the way a native library's
 * init code can (docs/ABI.md "Trust model") - a browser module's
 * top-level code runs in the same JS realm and can still throw or loop
 * forever, but it cannot corrupt memory the way a native crash can.
 *
 * What IS carried over, deliberately, is the *shape* of the check: every
 * loaded module's declared identity (name/version/kind) is verified
 * against what its manifest claimed BEFORE `create()` is ever called -
 * exactly the "validate declared compatibility before invoking
 * operations" discipline docs/ABI.md requires for the native ABI,
 * applied here to catch a manifest/module drift instead of an ABI
 * version drift.
 *
 * Each loaded ES module must export:
 *   export const descriptor = { name, version, kind };
 *   export function create(config) { ... }   // returns an instance object
 */

/**
 * @param {import('@usdk/contracts').Manifest} manifest
 * @param {string} baseUrl - resolved against manifest.entry via `new URL(entry, baseUrl)`
 * @returns {Promise<{manifest: import('@usdk/contracts').Manifest, descriptor: object, create: Function}>}
 */
export async function loadManifestModule(manifest, baseUrl) {
  const url = new URL(manifest.entry, baseUrl).href;
  let mod;
  try {
    mod = await import(/* @vite-ignore */ url);
  } catch (e) {
    throw new UsdkError(Status.MODULE_LOAD_FAILED, `failed to import '${url}' for manifest '${manifest.name}': ${e && e.message}`);
  }

  const descriptor = mod.descriptor;
  if (!descriptor || typeof descriptor !== 'object') {
    throw new UsdkError(Status.MODULE_LOAD_FAILED, `module '${url}' does not export a 'descriptor' object`);
  }
  if (descriptor.name !== manifest.name) {
    throw new UsdkError(
      Status.MODULE_LOAD_FAILED,
      `module '${url}' declares descriptor.name '${descriptor.name}', but its manifest name is '${manifest.name}'`
    );
  }
  if (descriptor.version !== manifest.version) {
    throw new UsdkError(
      Status.ABI_VERSION_MISMATCH,
      `module '${url}' declares version '${descriptor.version}', but its manifest declares '${manifest.version}'`
    );
  }
  if (descriptor.kind !== manifest.kind) {
    throw new UsdkError(
      Status.MODULE_LOAD_FAILED,
      `module '${url}' declares kind '${descriptor.kind}', but its manifest declares '${manifest.kind}'`
    );
  }
  if (typeof mod.create !== 'function') {
    throw new UsdkError(Status.MODULE_LOAD_FAILED, `module '${url}' does not export a 'create' function`);
  }

  return { manifest, descriptor, create: mod.create };
}

/**
 * Loads every manifest in `plan`, in order (dependencies first, matching
 * ManifestRegistry#resolve's guarantee) - fails on the first module that
 * does not load or does not pass validation, per-module errors carrying
 * enough detail to identify which manifest failed.
 * @param {import('@usdk/contracts').Manifest[]} plan
 * @param {string} baseUrl
 */
export async function loadPlan(plan, baseUrl) {
  const loaded = [];
  for (const manifest of plan) {
    loaded.push(await loadManifestModule(manifest, baseUrl));
  }
  return loaded;
}
