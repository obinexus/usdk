#!/usr/bin/env node
/**
 * Builds dist/ - the distributable artifact for the browser-facing
 * capabilities of this SDK - WITHOUT ejecting anything: dist/ is an
 * additive copy, packages/ is never moved, deleted, or rewritten, and
 * nothing here changes any package's source. See
 * docs/UAGENT_ARCHITECTURE.md "DO NOT EJECT."
 *
 * "Which packages are browser-relevant" is not a hardcoded list here -
 * it is discovered mechanically from the two things that actually
 * determine what a browser loads:
 *   1. Every `<script type="importmap">` block in packages/uagent/public/*.html
 *      (statically `import`-ed packages - @usdk/host-browser,
 *      @usdk/binding-javascript, @usdk/contracts, etc.)
 *   2. Every driver manifest `entry:` path referenced in
 *      packages/uagent/public/*.mjs (dynamically `import()`-ed by
 *      @usdk/loader at runtime - the driver packages, which are
 *      deliberately NOT in the importmap since nothing statically
 *      imports them)
 * The union of both is copied into dist/packages/<name>/ preserving the
 * exact same relative directory layout, so every relative import path
 * already written in the source HTML/JS keeps resolving correctly with
 * zero path rewriting - "reproducible build configuration" in the
 * simplest sense: dist/ has the identical shape as the relevant slice
 * of packages/, so there is no separate rewriting step that could drift
 * out of sync with the source.
 */
import { readFile, writeFile, mkdir, cp } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { join, dirname, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const REPO_ROOT = resolve(fileURLToPath(import.meta.url), '..', '..', '..', '..');
const PACKAGES_DIR = join(REPO_ROOT, 'packages');
const UAGENT_PUBLIC = join(PACKAGES_DIR, 'uagent', 'public');
const DIST_DIR = join(REPO_ROOT, 'dist');

function packageDirFromSpecifier(spec) {
  // "@usdk/host-browser" -> "host-browser" (the packages/ directory name
  // matches the specifier's suffix for every package in this workspace -
  // checked directly against every package.json "name" field).
  return spec.startsWith('@usdk/') ? spec.slice('@usdk/'.length) : null;
}

function packageDirFromEntryPath(entryPath) {
  // "../../driver-llm-fixture/src/index.mjs" -> "driver-llm-fixture"
  const m = entryPath.match(/\.\.\/\.\.\/([^/]+)\//);
  return m ? m[1] : null;
}

async function discoverBrowserPackageDirs() {
  const dirs = new Set();
  const publicFiles = await readdirRecursive(UAGENT_PUBLIC);

  for (const file of publicFiles.filter((f) => f.endsWith('.html'))) {
    const text = await readFile(file, 'utf8');
    const importmapMatch = text.match(/<script type="importmap">([\s\S]*?)<\/script>/);
    if (!importmapMatch) continue;
    const importmap = JSON.parse(importmapMatch[1]);
    for (const spec of Object.keys(importmap.imports ?? {})) {
      const dir = packageDirFromSpecifier(spec);
      if (dir) dirs.add(dir);
    }
  }

  for (const file of publicFiles.filter((f) => f.endsWith('.mjs'))) {
    const text = await readFile(file, 'utf8');
    const entryMatches = text.matchAll(/entry:\s*'([^']+)'/g);
    for (const [, entryPath] of entryMatches) {
      const dir = packageDirFromEntryPath(entryPath);
      if (dir) dirs.add(dir);
    }
  }

  return [...dirs].sort();
}

async function readdirRecursive(dir) {
  const { readdir } = await import('node:fs/promises');
  const entries = await readdir(dir, { withFileTypes: true });
  const files = [];
  for (const entry of entries) {
    const full = join(dir, entry.name);
    if (entry.isDirectory()) files.push(...(await readdirRecursive(full)));
    else files.push(full);
  }
  return files;
}

export async function buildDist() {
  const packageDirs = await discoverBrowserPackageDirs();
  const manifest = { builtAt: new Date().toISOString(), packages: [] };

  await mkdir(DIST_DIR, { recursive: true });

  for (const dir of packageDirs) {
    const srcPkgDir = join(PACKAGES_DIR, dir);
    if (!existsSync(srcPkgDir)) {
      throw new Error(`build-dist: importmap/entry referenced package directory '${dir}' does not exist under packages/`);
    }
    const pkgJsonPath = join(srcPkgDir, 'package.json');
    const pkgJson = existsSync(pkgJsonPath) ? JSON.parse(await readFile(pkgJsonPath, 'utf8')) : null;

    const destPkgDir = join(DIST_DIR, 'packages', dir);
    await mkdir(destPkgDir, { recursive: true });
    if (pkgJson) await cp(pkgJsonPath, join(destPkgDir, 'package.json'));
    const srcDir = join(srcPkgDir, 'src');
    if (existsSync(srcDir)) await cp(srcDir, join(destPkgDir, 'src'), { recursive: true });

    manifest.packages.push({
      dir,
      name: pkgJson?.name ?? null,
      version: pkgJson?.version ?? null,
      dependencies: pkgJson?.dependencies ? Object.keys(pkgJson.dependencies) : [],
    });
  }

  const destPublicDir = join(DIST_DIR, 'packages', 'uagent', 'public');
  await mkdir(dirname(destPublicDir), { recursive: true });
  await cp(UAGENT_PUBLIC, destPublicDir, { recursive: true });

  await writeFile(join(DIST_DIR, 'BUILD_MANIFEST.json'), JSON.stringify(manifest, null, 2) + '\n');

  return { distDir: DIST_DIR, packageDirs, entryPoints: ['packages/uagent/public/index.html', 'packages/uagent/public/connected.html'] };
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const result = await buildDist();
  console.log(`Built dist/ with ${result.packageDirs.length} packages: ${result.packageDirs.join(', ')}`);
  console.log(`Entry points: ${result.entryPoints.join(', ')}`);
}
